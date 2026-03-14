/**
 * @file SITM001A.cpp
 * @brief Score IR temporal system — implementation
 *
 * Component: SITM001A
 * Domain: SI (Score IR) | Category: TM (Temporal)
 */

#include "SITM001A.h"

#include <cmath>

namespace Sunny::Core {

// =============================================================================
// Helpers
// =============================================================================

namespace {

/**
 * @brief Look up the active TimeSignature at a given bar
 *
 * Returns the last entry whose bar <= target_bar.
 */
const TimeSignatureEntry* find_time_signature(
    const TimeSignatureMap& time_map,
    std::uint32_t target_bar
) {
    const TimeSignatureEntry* active = nullptr;
    for (const auto& entry : time_map) {
        if (entry.bar <= target_bar) {
            active = &entry;
        } else {
            break;
        }
    }
    return active;
}

/**
 * @brief Look up the active TempoEvent at or before a given AbsoluteBeat
 *
 * Returns index into tempo_map of the last entry whose AbsoluteBeat
 * position <= target.
 */
struct TempoSegment {
    std::size_t index;
    Beat segment_start;  // AbsoluteBeat of this tempo event
};

/**
 * @brief Compute AbsoluteBeat for each TempoMap entry position
 *
 * Returns an error if any tempo event position fails conversion rather
 * than silently relocating it to the score origin. A failed conversion
 * indicates a structural inconsistency between the tempo map and the
 * time signature map that callers must handle.
 */
Result<std::vector<Beat>> compute_tempo_beats(
    const TempoMap& tempo_map,
    const TimeSignatureMap& time_map
) {
    std::vector<Beat> beats;
    beats.reserve(tempo_map.size());
    for (const auto& event : tempo_map) {
        auto result = score_time_to_absolute_beat(event.position, time_map);
        if (!result) return std::unexpected(result.error());
        beats.push_back(*result);
    }
    return beats;
}

}  // anonymous namespace

// =============================================================================
// AbsoluteBeat (SS-IR §5.2)
// =============================================================================

Result<Beat> score_time_to_absolute_beat(
    ScoreTime time,
    const TimeSignatureMap& time_map
) {
    if (time_map.empty()) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }
    if (time.bar < 1) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    Beat cumulative = Beat::zero();

    // Sum measure durations from bar 1 to bar (time.bar - 1)
    for (std::uint32_t bar = 1; bar < time.bar; ++bar) {
        const TimeSignatureEntry* entry = find_time_signature(time_map, bar);
        if (!entry) {
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        }
        auto sum = checked_add(cumulative, entry->time_signature.measure_duration());
        if (!sum) return std::unexpected(sum.error());
        cumulative = *sum;
    }

    // Add the beat offset within the target bar
    auto final_sum = checked_add(cumulative, time.beat);
    if (!final_sum) return std::unexpected(final_sum.error());

    return *final_sum;
}

Result<ScoreTime> absolute_beat_to_score_time(
    Beat absolute,
    const TimeSignatureMap& time_map,
    std::uint32_t total_bars
) {
    if (time_map.empty() || total_bars == 0) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }
    if (absolute < Beat::zero()) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    Beat remaining = absolute;

    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        const TimeSignatureEntry* entry = find_time_signature(time_map, bar);
        if (!entry) {
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        }
        Beat measure_dur = entry->time_signature.measure_duration();

        if (remaining < measure_dur) {
            return ScoreTime{bar, remaining};
        }
        auto diff = checked_sub(remaining, measure_dur);
        if (!diff) return std::unexpected(diff.error());
        remaining = *diff;
    }

    // Past end of score
    if (remaining == Beat::zero()) {
        return ScoreTime{total_bars + 1, Beat::zero()};
    }
    return std::unexpected(ErrorCode::InvalidTimeSignature);
}

// =============================================================================
// RealTime (SS-IR §5.3)
// =============================================================================

Result<double> absolute_beat_to_real_time(
    Beat absolute,
    const TempoMap& tempo_map,
    const TimeSignatureMap& time_map
) {
    if (tempo_map.empty()) {
        return std::unexpected(ErrorCode::InvalidTempo);
    }

    // Compute AbsoluteBeat for each tempo event
    auto tempo_beats_result = compute_tempo_beats(tempo_map, time_map);
    if (!tempo_beats_result) return std::unexpected(tempo_beats_result.error());
    auto tempo_beats = std::move(*tempo_beats_result);

    double total_seconds = 0.0;
    Beat current_pos = Beat::zero();

    for (std::size_t i = 0; i < tempo_map.size(); ++i) {
        const auto& event = tempo_map[i];
        Beat event_beat = tempo_beats[i];

        // Determine the end of this tempo segment
        Beat segment_end;
        if (i + 1 < tempo_map.size()) {
            segment_end = tempo_beats[i + 1];
            if (absolute < segment_end) {
                segment_end = absolute;
            }
        } else {
            segment_end = absolute;
        }

        if (current_pos >= segment_end) continue;

        // Skip if segment starts before current position
        Beat seg_start = (event_beat > current_pos) ? event_beat : current_pos;
        if (seg_start >= segment_end) continue;

        Beat span = segment_end - seg_start;
        // Convert from whole-note units to quarter-note units (BPM reference)
        double span_quarters = span.to_float() * 4.0;

        double bpm_start = effective_quarter_bpm(event);

        // Check if this segment has a linear transition
        if (event.transition_type == TempoTransitionType::Linear &&
            event.linear_duration > Beat::zero() && i + 1 < tempo_map.size()) {

            double bpm_end = effective_quarter_bpm(tempo_map[i + 1]);
            Beat trans_end_beat = event_beat + event.linear_duration;

            if (seg_start < trans_end_beat) {
                // Portion within the linear transition
                Beat trans_span_end = (segment_end < trans_end_beat)
                    ? segment_end : trans_end_beat;
                Beat trans_span = trans_span_end - seg_start;
                double trans_quarters = trans_span.to_float() * 4.0;

                // Interpolation factor at seg_start and trans_span_end
                double total_trans = event.linear_duration.to_float();
                double offset_start = (seg_start - event_beat).to_float();
                double t0 = offset_start / total_trans;
                double t1 = (offset_start + trans_span.to_float()) / total_trans;

                double b0 = bpm_start + t0 * (bpm_end - bpm_start);
                double b1 = bpm_start + t1 * (bpm_end - bpm_start);

                if (std::abs(b1 - b0) < 1e-10) {
                    // Constant tempo within this portion
                    total_seconds += 60.0 * trans_quarters / b0;
                } else {
                    // Closed-form: Δt = 60·d·ln(B₂/B₁) / (B₂ − B₁)
                    total_seconds += 60.0 * trans_quarters * std::log(b1 / b0)
                                     / (b1 - b0);
                }

                // Remaining constant portion after transition
                if (segment_end > trans_end_beat) {
                    Beat const_span = segment_end - trans_end_beat;
                    total_seconds += 60.0 * const_span.to_float() * 4.0 / bpm_end;
                }
            } else {
                // Past the transition — use the end BPM
                double bpm_post_trans = effective_quarter_bpm(tempo_map[i + 1]);
                total_seconds += 60.0 * span_quarters / bpm_post_trans;
            }
        } else {
            // Constant tempo (Immediate or MetricModulation)
            total_seconds += 60.0 * span_quarters / bpm_start;
        }

        current_pos = segment_end;
        if (current_pos >= absolute) break;
    }

    return total_seconds;
}

Result<double> score_time_to_real_time(
    ScoreTime time,
    const TempoMap& tempo_map,
    const TimeSignatureMap& time_map
) {
    auto abs_beat = score_time_to_absolute_beat(time, time_map);
    if (!abs_beat) return std::unexpected(abs_beat.error());

    return absolute_beat_to_real_time(*abs_beat, tempo_map, time_map);
}

// =============================================================================
// TickTime (SS-IR §5.4)
// =============================================================================

Result<std::int64_t> score_time_to_tick(
    ScoreTime time,
    const TimeSignatureMap& time_map,
    int ppq
) {
    auto abs_beat = score_time_to_absolute_beat(time, time_map);
    if (!abs_beat) return std::unexpected(abs_beat.error());

    return absolute_beat_to_tick(*abs_beat, ppq);
}

}  // namespace Sunny::Core
