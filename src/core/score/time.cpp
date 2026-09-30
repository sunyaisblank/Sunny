/**
 * @file time.cpp
 * @brief Score IR temporal system — implementation
 *
 */

#include <cmath>
#include <sunny/core/score/time.hpp>

namespace sunny::core {

// =============================================================================
// Helpers
// =============================================================================

namespace {

/**
 * @brief Look up the active TimeSignature at a given bar
 *
 * Returns the last entry whose bar <= target_bar.
 */
const TimeSignatureEntry* find_time_signature(const TimeSignatureMap& time_map,
                                              std::uint32_t target_bar) {
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
    Beat segment_start; // AbsoluteBeat of this tempo event
};

/**
 * @brief Compute AbsoluteBeat for each TempoMap entry position
 *
 * Returns an error if any tempo event position fails conversion rather
 * than silently relocating it to the score origin. A failed conversion
 * indicates a structural inconsistency between the tempo map and the
 * time signature map that callers must handle.
 */
Result<std::vector<Beat>> compute_tempo_beats(const TempoMap& tempo_map,
                                              const TimeSignatureMap& time_map) {
    std::vector<Beat> beats;
    beats.reserve(tempo_map.size());
    for (const auto& event : tempo_map) {
        auto result = score_time_to_absolute_beat(event.position, time_map);
        if (!result) return std::unexpected(result.error());
        beats.push_back(*result);
    }
    return beats;
}

Result<Beat> exact_effective_quarter_rate(const TempoEvent& event) {
    if (static_cast<std::uint8_t>(event.beat_unit) > static_cast<std::uint8_t>(BeatUnit::Sixteenth))
        return std::unexpected(ErrorCode::InvalidTempo);
    auto scaled = checked_mul(Beat{event.bpm.numerator(), event.bpm.denominator()},
                              beat_unit_duration(event.beat_unit));
    if (!scaled) return std::unexpected(scaled.error());
    return checked_mul(*scaled, Beat{4, 1});
}

} // anonymous namespace

// =============================================================================
// AbsoluteBeat (SS-IR §5.2)
// =============================================================================

Result<Beat> score_time_to_absolute_beat(ScoreTime time, const TimeSignatureMap& time_map) {
    if (time_map.empty()) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }
    if (time.bar < 1 || time.beat < Beat::zero()) {
        return std::unexpected(ErrorCode::InvalidTimeSignature);
    }

    const TimeSignatureEntry* target_signature = find_time_signature(time_map, time.bar);
    if (!target_signature) return std::unexpected(ErrorCode::InvalidTimeSignature);
    auto target_duration = checked_measure_duration(target_signature->time_signature);
    if (!target_duration || time.beat >= *target_duration)
        return std::unexpected(ErrorCode::InvalidTimeSignature);

    Beat cumulative = Beat::zero();

    // Sum measure durations from bar 1 to bar (time.bar - 1)
    for (std::uint32_t bar = 1; bar < time.bar; ++bar) {
        const TimeSignatureEntry* entry = find_time_signature(time_map, bar);
        if (!entry) {
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        }
        auto measure_duration = checked_measure_duration(entry->time_signature);
        if (!measure_duration) return std::unexpected(measure_duration.error());
        auto sum = checked_add(cumulative, *measure_duration);
        if (!sum) return std::unexpected(sum.error());
        cumulative = *sum;
    }

    // Add the beat offset within the target bar
    auto final_sum = checked_add(cumulative, time.beat);
    if (!final_sum) return std::unexpected(final_sum.error());

    return *final_sum;
}

Result<ScoreTime> absolute_beat_to_score_time(Beat absolute,
                                              const TimeSignatureMap& time_map,
                                              std::uint32_t total_bars) {
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
        auto measure_duration = checked_measure_duration(entry->time_signature);
        if (!measure_duration) return std::unexpected(measure_duration.error());
        Beat measure_dur = *measure_duration;

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

Result<double> absolute_beat_to_real_time(Beat absolute,
                                          const TempoMap& tempo_map,
                                          const TimeSignatureMap& time_map) {
    if (tempo_map.empty() || absolute < Beat::zero()) {
        return std::unexpected(ErrorCode::InvalidTempo);
    }

    // Compute AbsoluteBeat for each tempo event
    auto tempo_beats_result = compute_tempo_beats(tempo_map, time_map);
    if (!tempo_beats_result) return std::unexpected(tempo_beats_result.error());
    auto tempo_beats = std::move(*tempo_beats_result);

    if (tempo_beats.front() != Beat::zero() ||
        tempo_map.front().transition_type != TempoTransitionType::Immediate ||
        tempo_map.front().linear_duration != Beat::zero())
        return std::unexpected(ErrorCode::InvalidTempo);

    const auto valid_beat_unit = [](BeatUnit unit) {
        return static_cast<std::uint8_t>(unit) <= static_cast<std::uint8_t>(BeatUnit::Sixteenth);
    };
    const auto valid_transition = [](TempoTransitionType transition) {
        return static_cast<std::uint8_t>(transition) <=
               static_cast<std::uint8_t>(TempoTransitionType::MetricModulation);
    };

    for (std::size_t i = 0; i < tempo_map.size(); ++i) {
        const auto& event = tempo_map[i];
        if (!valid_beat_unit(event.beat_unit) || !valid_beat_unit(event.old_unit) ||
            !valid_beat_unit(event.new_unit) || !valid_transition(event.transition_type))
            return std::unexpected(ErrorCode::InvalidTempo);
        if (i > 0 && tempo_beats[i] <= tempo_beats[i - 1])
            return std::unexpected(ErrorCode::InvalidTempo);

        if (event.transition_type == TempoTransitionType::Linear) {
            if (i == 0 || event.linear_duration <= Beat::zero())
                return std::unexpected(ErrorCode::InvalidTempo);
            auto interval = checked_sub(tempo_beats[i], tempo_beats[i - 1]);
            if (!interval || *interval != event.linear_duration)
                return std::unexpected(ErrorCode::InvalidTempo);
        } else if (event.linear_duration != Beat::zero()) {
            return std::unexpected(ErrorCode::InvalidTempo);
        }

        if (event.transition_type == TempoTransitionType::MetricModulation) {
            if (i == 0 || event.beat_unit != event.new_unit)
                return std::unexpected(ErrorCode::InvalidTempo);
            auto previous_rate = exact_effective_quarter_rate(tempo_map[i - 1]);
            auto current_rate = exact_effective_quarter_rate(event);
            auto unit_ratio =
                checked_div(beat_unit_duration(event.new_unit), beat_unit_duration(event.old_unit));
            if (!previous_rate || !current_rate || !unit_ratio)
                return std::unexpected(ErrorCode::InvalidTempo);
            auto expected = checked_mul(*previous_rate, *unit_ratio);
            if (!expected || *expected != *current_rate)
                return std::unexpected(ErrorCode::InvalidTempo);
        }
    }

    double total_seconds = 0.0;

    for (std::size_t i = 0; i < tempo_map.size(); ++i) {
        const auto& event = tempo_map[i];
        const Beat event_beat = tempo_beats[i];
        if (event_beat >= absolute) break;

        // Determine the end of this tempo segment
        const Beat segment_end = i + 1 < tempo_map.size() && tempo_beats[i + 1] < absolute
                                     ? tempo_beats[i + 1]
                                     : absolute;
        if (event_beat >= segment_end) continue;

        auto span_result = checked_sub(segment_end, event_beat);
        if (!span_result) return std::unexpected(span_result.error());
        const Beat span = *span_result;
        // Convert from whole-note units to quarter-note units (BPM reference)
        const double span_quarters = span.to_float() * 4.0;

        const double bpm_start = effective_quarter_bpm(event);
        if (!std::isfinite(bpm_start) || bpm_start <= 0.0)
            return std::unexpected(ErrorCode::InvalidTempo);

        // A Linear transition belongs to its destination event. Therefore the
        // segment [event i, event i+1) ramps exactly when event i+1 is Linear.
        if (i + 1 < tempo_map.size() &&
            tempo_map[i + 1].transition_type == TempoTransitionType::Linear) {
            const double bpm_target = effective_quarter_bpm(tempo_map[i + 1]);
            if (!std::isfinite(bpm_target) || bpm_target <= 0.0)
                return std::unexpected(ErrorCode::InvalidTempo);

            auto full_span_result = checked_sub(tempo_beats[i + 1], event_beat);
            if (!full_span_result || *full_span_result <= Beat::zero())
                return std::unexpected(ErrorCode::InvalidTempo);
            const double fraction = span.to_float() / full_span_result->to_float();
            const double bpm_end = bpm_start + fraction * (bpm_target - bpm_start);
            if (std::abs(bpm_end - bpm_start) < 1e-10) {
                total_seconds += 60.0 * span_quarters / bpm_start;
            } else {
                // Closed-form: Δt = 60·quarters·ln(B₂/B₁)/(B₂−B₁).
                total_seconds +=
                    60.0 * span_quarters * std::log(bpm_end / bpm_start) / (bpm_end - bpm_start);
            }
        } else {
            // Immediate and MetricModulation changes occur at the next point.
            total_seconds += 60.0 * span_quarters / bpm_start;
        }
    }

    return total_seconds;
}

Result<double> score_time_to_real_time(ScoreTime time,
                                       const TempoMap& tempo_map,
                                       const TimeSignatureMap& time_map) {
    auto abs_beat = score_time_to_absolute_beat(time, time_map);
    if (!abs_beat) return std::unexpected(abs_beat.error());

    return absolute_beat_to_real_time(*abs_beat, tempo_map, time_map);
}

Result<PositiveRational> effective_quarter_tempo_at(ScoreTime time,
                                                    const TempoMap& tempo_map,
                                                    const TimeSignatureMap& time_map) {
    if (tempo_map.empty()) return std::unexpected(ErrorCode::InvalidTempo);
    // Reuse the clock path's complete S24 defensive checks even for a zero
    // integration span, so direct query callers cannot bypass metric or enum
    // coherence merely because they do not request RealTime.
    auto validated = absolute_beat_to_real_time(Beat::zero(), tempo_map, time_map);
    if (!validated) return std::unexpected(validated.error());
    auto point = score_time_to_absolute_beat(time, time_map);
    if (!point) return std::unexpected(point.error());
    auto tempo_beats = compute_tempo_beats(tempo_map, time_map);
    if (!tempo_beats || tempo_beats->empty() || tempo_beats->front() != Beat::zero() ||
        *point < Beat::zero())
        return std::unexpected(ErrorCode::InvalidTempo);

    std::size_t active = 0;
    for (std::size_t index = 1; index < tempo_beats->size(); ++index) {
        if ((*tempo_beats)[index] > *point) break;
        active = index;
    }

    auto rate = exact_effective_quarter_rate(tempo_map[active]);
    if (!rate) return std::unexpected(rate.error());

    if (active + 1 < tempo_map.size() &&
        tempo_map[active + 1].transition_type == TempoTransitionType::Linear &&
        *point < (*tempo_beats)[active + 1]) {
        auto target_rate = exact_effective_quarter_rate(tempo_map[active + 1]);
        auto full_span = checked_sub((*tempo_beats)[active + 1], (*tempo_beats)[active]);
        auto elapsed = checked_sub(*point, (*tempo_beats)[active]);
        if (!target_rate || !full_span || !elapsed || *full_span <= Beat::zero() ||
            tempo_map[active + 1].linear_duration != *full_span)
            return std::unexpected(ErrorCode::InvalidTempo);
        auto fraction = checked_div(*elapsed, *full_span);
        auto delta = checked_sub(*target_rate, *rate);
        if (!fraction || !delta) return std::unexpected(ErrorCode::ArithmeticOverflow);
        auto interpolated_delta = checked_mul(*delta, *fraction);
        if (!interpolated_delta) return std::unexpected(interpolated_delta.error());
        auto interpolated = checked_add(*rate, *interpolated_delta);
        if (!interpolated) return std::unexpected(interpolated.error());
        rate = *interpolated;
    }

    const Beat reduced = rate->reduce();
    if (reduced.numerator() <= 0) return std::unexpected(ErrorCode::InvalidTempo);
    return PositiveRational{reduced.numerator(), reduced.denominator()};
}

// =============================================================================
// TickTime (SS-IR §5.4)
// =============================================================================

Result<std::int64_t> score_time_to_tick(ScoreTime time, const TimeSignatureMap& time_map, int ppq) {
    auto abs_beat = score_time_to_absolute_beat(time, time_map);
    if (!abs_beat) return std::unexpected(abs_beat.error());

    return absolute_beat_to_tick(*abs_beat, ppq);
}

} // namespace sunny::core
