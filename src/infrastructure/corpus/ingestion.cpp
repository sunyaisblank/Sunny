/**
 * @file ingestion.cpp
 * @brief Corpus IR ingestion pipeline — implementation
 *
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sunny/core/corpus/analysis.hpp>
#include <sunny/core/pitch/midi_note.hpp>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/corpus/ingestion.hpp>
#include <sunny/infrastructure/formats/midi_file.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>

namespace sunny::infrastructure::corpus {

using namespace sunny::core;
using namespace sunny::infrastructure::formats;

namespace {

// =========================================================================
// Krumhansl-Kessler key profiles
// =========================================================================

// Major key profile (Krumhansl-Kessler 1990)
constexpr std::array<float, 12> KK_MAJOR = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};

// Minor key profile (Krumhansl-Kessler 1990)
constexpr std::array<float, 12> KK_MINOR = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};

struct KeyEstimate {
    PitchClass tonic;
    bool is_minor;
    float confidence;
};

/// Pearson correlation between two arrays of length 12
float pearson12(const std::array<float, 12>& x, const std::array<float, 12>& y) {
    float mx = 0.0f, my = 0.0f;
    for (int i = 0; i < 12; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= 12.0f;
    my /= 12.0f;

    float num = 0.0f, dx = 0.0f, dy = 0.0f;
    for (int i = 0; i < 12; ++i) {
        float a = x[i] - mx;
        float b = y[i] - my;
        num += a * b;
        dx += a * a;
        dy += b * b;
    }
    float denom = std::sqrt(dx * dy);
    return denom > 0.0f ? num / denom : 0.0f;
}

/// Estimate key from a pitch class histogram using Krumhansl-Schmuckler.
/// Correlates the observed PC distribution against all 24 major/minor
/// key profiles (one rotation per semitone) and returns the best match.
KeyEstimate estimate_key(const std::array<std::uint32_t, 12>& pc_hist) {
    // Convert histogram to float
    std::array<float, 12> observed{};
    std::uint32_t total = 0;
    for (int i = 0; i < 12; ++i) {
        observed[i] = static_cast<float>(pc_hist[i]);
        total += pc_hist[i];
    }
    if (total == 0) return {0, false, 0.0f};

    float best_r = -2.0f;
    PitchClass best_tonic = 0;
    bool best_minor = false;

    for (int rotation = 0; rotation < 12; ++rotation) {
        // Rotate the profile to match this tonic
        std::array<float, 12> rotated_major{};
        std::array<float, 12> rotated_minor{};
        for (int i = 0; i < 12; ++i) {
            rotated_major[(i + rotation) % 12] = KK_MAJOR[i];
            rotated_minor[(i + rotation) % 12] = KK_MINOR[i];
        }

        float r_major = pearson12(observed, rotated_major);
        float r_minor = pearson12(observed, rotated_minor);

        if (r_major > best_r) {
            best_r = r_major;
            best_tonic = PitchClass::wrapped(rotation);
            best_minor = false;
        }
        if (r_minor > best_r) {
            best_r = r_minor;
            best_tonic = PitchClass::wrapped(rotation);
            best_minor = true;
        }
    }

    // Map correlation to confidence [0, 1]
    float confidence = std::clamp((best_r + 1.0f) / 2.0f, 0.0f, 1.0f);
    return {best_tonic, best_minor, confidence};
}

// =========================================================================
// Quantisation
// =========================================================================

struct QuantisedNote {
    Beat start;
    Beat duration;
    MidiNote pitch;
    Velocity velocity;
    std::uint8_t release_velocity;
    std::size_t source_ordinal;
};

/// Quantise a non-negative beat to the nearest grid division using exact
/// rational arithmetic. Exact half-grid values round forward, matching the
/// historical positive-domain std::round policy without binary-float ties.
Result<Beat> quantise_beat(Beat beat, int grid) {
    if (beat < Beat::zero() || grid <= 0) return std::unexpected(ErrorCode::CorpusInvalidParameter);

    const auto scaled = checked_mul(beat, Beat{static_cast<std::int64_t>(grid), 1});
    if (!scaled) return std::unexpected(scaled.error());
    const auto denominator = scaled->denominator();
    auto rounded = scaled->numerator() / denominator;
    const auto remainder = scaled->numerator() % denominator;
    const auto round_up_threshold = denominator / 2 + denominator % 2;
    if (remainder >= round_up_threshold) {
        if (rounded == std::numeric_limits<std::int64_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        ++rounded;
    }
    return Beat{rounded, static_cast<std::int64_t>(grid)};
}

struct QuantisationResult {
    std::vector<QuantisedNote> notes;
    float onset_rms = 0.0f;
    float duration_rms = 0.0f;
};

/// Quantise NoteEvents and compute distinct onset and duration RMS residuals.
Result<QuantisationResult> quantise_notes(const std::vector<NoteEvent>& events, int grid) {
    QuantisationResult result;
    double onset_sum_sq = 0.0;
    double duration_sum_sq = 0.0;

    for (std::size_t source_ordinal = 0; source_ordinal < events.size(); ++source_ordinal) {
        const auto& ev = events[source_ordinal];
        auto q_start = quantise_beat(ev.start_time, grid);
        auto q_duration = quantise_beat(ev.duration, grid);
        if (!q_start || !q_duration)
            return std::unexpected(!q_start ? q_start.error() : q_duration.error());
        if (*q_duration <= Beat::zero()) *q_duration = Beat{1, static_cast<std::int64_t>(grid)};

        auto onset_delta = checked_sub(ev.start_time, *q_start);
        auto duration_delta = checked_sub(ev.duration, *q_duration);
        if (!onset_delta || !duration_delta)
            return std::unexpected(!onset_delta ? onset_delta.error() : duration_delta.error());
        const double onset_residual = std::abs(onset_delta->to_float());
        const double duration_residual = std::abs(duration_delta->to_float());
        onset_sum_sq += onset_residual * onset_residual;
        duration_sum_sq += duration_residual * duration_residual;

        result.notes.push_back(
            {*q_start, *q_duration, ev.pitch, ev.velocity, ev.release_velocity, source_ordinal});
    }

    if (!events.empty()) {
        const auto count = static_cast<double>(events.size());
        result.onset_rms = static_cast<float>(std::sqrt(onset_sum_sq / count));
        result.duration_rms = static_cast<float>(std::sqrt(duration_sum_sq / count));
    }
    return result;
}

// =========================================================================
// Voice separation
// =========================================================================

struct VoicedNote {
    Beat start;
    Beat duration;
    MidiNote pitch;
    Velocity velocity;
    std::uint8_t release_velocity;
    std::uint8_t voice;
};

struct VoiceSeparationResult {
    std::vector<VoicedNote> notes;
    float confidence = 1.0f;
    std::uint64_t complex_onsets = 0;
    std::uint64_t total_onsets = 0;
};

/// Register-based voice separation: simultaneous notes are ordered by descending pitch and then
/// retained source order, and assigned to the first free voice. Confidence counts both chordal
/// onsets and staggered onsets under a still-sounding note as assignment-complex onsets.
Result<VoiceSeparationResult> separate_voices(const std::vector<QuantisedNote>& notes) {
    if (notes.empty()) return VoiceSeparationResult{};

    // Group notes by start time
    std::map<Beat, std::vector<const QuantisedNote*>> by_onset;
    for (const auto& n : notes)
        by_onset[n.start].push_back(&n);

    std::vector<VoicedNote> result;
    std::vector<Beat> voice_free_at;
    std::uint64_t complex_onsets = 0;

    for (const auto& [onset, group] : by_onset) {
        // Sort by pitch descending (highest = voice 0)
        auto sorted = group;
        std::sort(sorted.begin(), sorted.end(), [](const QuantisedNote* a, const QuantisedNote* b) {
            if (a->pitch != b->pitch) return a->pitch > b->pitch;
            return a->source_ordinal < b->source_ordinal;
        });

        const bool sustained_overlap = std::ranges::any_of(
            voice_free_at, [onset](const Beat free_at) { return free_at > onset; });
        if (sorted.size() > 1 || sustained_overlap) ++complex_onsets;

        for (const auto* note : sorted) {
            auto available = std::find_if(voice_free_at.begin(),
                                          voice_free_at.end(),
                                          [onset](Beat free_at) { return free_at <= onset; });
            std::size_t voice_index = static_cast<std::size_t>(available - voice_free_at.begin());
            if (available == voice_free_at.end()) {
                constexpr auto MAX_VOICES =
                    static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max()) + 1;
                if (voice_free_at.size() >= MAX_VOICES)
                    return std::unexpected(ErrorCode::IngestionFailed);
                voice_index = voice_free_at.size();
                voice_free_at.push_back(Beat::zero());
            }

            const auto voice = static_cast<std::uint8_t>(voice_index);
            result.push_back({note->start,
                              note->duration,
                              note->pitch,
                              note->velocity,
                              note->release_velocity,
                              voice});
            voice_free_at[voice_index] = note->start + note->duration;
        }
    }

    float confidence = by_onset.empty() ? 1.0f
                                        : 1.0f - static_cast<float>(complex_onsets) /
                                                     static_cast<float>(by_onset.size());
    confidence = std::clamp(confidence, 0.3f, 1.0f);
    return VoiceSeparationResult{
        std::move(result), confidence, complex_onsets, static_cast<std::uint64_t>(by_onset.size())};
}

Result<ScoreTime> absolute_to_score_time_unbounded(Beat absolute,
                                                   const TimeSignatureMap& time_map) {
    if (absolute < Beat::zero() || time_map.empty() || time_map.front().bar != 1)
        return std::unexpected(ErrorCode::InvalidTimeSignature);

    Beat segment_start = Beat::zero();
    for (std::size_t index = 0; index < time_map.size(); ++index) {
        const auto& entry = time_map[index];
        auto measure_duration = checked_measure_duration(entry.time_signature);
        if (!measure_duration) return std::unexpected(measure_duration.error());

        if (index + 1 < time_map.size()) {
            if (time_map[index + 1].bar <= entry.bar)
                return std::unexpected(ErrorCode::InvalidTimeSignature);
            const auto measure_count = time_map[index + 1].bar - entry.bar;
            auto segment_duration =
                checked_mul(*measure_duration, Beat{static_cast<std::int64_t>(measure_count), 1});
            if (!segment_duration) return std::unexpected(segment_duration.error());
            auto segment_end = checked_add(segment_start, *segment_duration);
            if (!segment_end) return std::unexpected(segment_end.error());
            if (absolute >= *segment_end) {
                segment_start = *segment_end;
                continue;
            }
        }

        auto within = checked_sub(absolute, segment_start);
        auto ratio = within ? checked_div(*within, *measure_duration)
                            : Result<Beat>{std::unexpected(ErrorCode::ArithmeticOverflow)};
        if (!ratio || ratio->numerator() < 0)
            return std::unexpected(ratio ? ErrorCode::InvalidTimeSignature : ratio.error());
        const auto bar_offset = ratio->numerator() / ratio->denominator();
        if (static_cast<std::uint64_t>(bar_offset) >
            std::numeric_limits<std::uint32_t>::max() - entry.bar)
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        auto consumed = checked_mul(*measure_duration, Beat{bar_offset, 1});
        auto beat = consumed ? checked_sub(*within, *consumed)
                             : Result<Beat>{std::unexpected(ErrorCode::ArithmeticOverflow)};
        if (!beat) return std::unexpected(beat.error());
        return ScoreTime{entry.bar + static_cast<std::uint32_t>(bar_offset), *beat};
    }
    return std::unexpected(ErrorCode::InvalidTimeSignature);
}

struct MidiTimeSignatureIngestion {
    TimeSignatureMap map;
    std::uint64_t metronome_click_residuals = 0;
};

struct MidiChannelStateIngestion {
    std::vector<PartDirective> directives;
    std::uint64_t imported_control_changes = 0;
    std::uint64_t normalised_pedal_values = 0;
    std::uint64_t residual_control_changes = 0;
};

bool add_count(std::uint64_t& destination, std::size_t count) {
    if (count > std::numeric_limits<std::uint64_t>::max() - destination) return false;
    destination += static_cast<std::uint64_t>(count);
    return true;
}

Result<MidiChannelStateIngestion>
midi_channel_state(const MidiFile& midi,
                   const TimeSignatureMap& time_map,
                   std::optional<std::uint8_t> sole_note_channel) {
    MidiChannelStateIngestion result;
    if (!sole_note_channel) {
        if (!add_count(result.residual_control_changes, midi.control_changes.size()))
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        return result;
    }

    struct SwitchPoint {
        MidiControlChangeEvent event;
        std::size_t source_count = 1;
    };

    for (const auto controller : {std::uint8_t{64}, std::uint8_t{67}}) {
        std::vector<MidiControlChangeEvent> events;
        for (const auto& event : midi.control_changes) {
            if (event.channel == *sole_note_channel && event.controller == controller)
                events.push_back(event);
        }
        std::stable_sort(events.begin(), events.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
            if (lhs.track != rhs.track) return lhs.track < rhs.track;
            return lhs.order < rhs.order;
        });

        // Score spans cannot retain a same-tick pedal change/retake. Across Type-1
        // tracks the order is additionally not a single source sequence, so one
        // contradictory tick makes this controller's whole state timeline unsafe
        // to globalise into the generated Part.
        bool ambiguous = false;
        for (std::size_t index = 0; index < events.size();) {
            const bool state = events[index].value >= 64;
            std::size_t next = index + 1;
            while (next < events.size() && events[next].tick == events[index].tick) {
                if ((events[next].value >= 64) != state) ambiguous = true;
                ++next;
            }
            index = next;
        }
        if (ambiguous) {
            if (!add_count(result.residual_control_changes, events.size()))
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            continue;
        }

        std::vector<SwitchPoint> points;
        for (std::size_t index = 0; index < events.size();) {
            const bool state = events[index].value >= 64;
            const std::uint8_t canonical = state ? 127 : 0;
            std::size_t next = index + 1;
            std::size_t representative = index;
            while (next < events.size() && events[next].tick == events[index].tick) {
                if (events[representative].value != canonical && events[next].value == canonical)
                    representative = next;
                ++next;
            }
            points.push_back(SwitchPoint{events[representative], next - index});
            index = next;
        }

        std::optional<SwitchPoint> active;
        for (const auto& point : points) {
            const bool state = point.event.value >= 64;
            if (state) {
                if (active) {
                    if (!add_count(result.residual_control_changes, point.source_count))
                        return std::unexpected(ErrorCode::ArithmeticOverflow);
                } else {
                    active = point;
                }
                continue;
            }
            if (!active) {
                if (!add_count(result.residual_control_changes, point.source_count))
                    return std::unexpected(ErrorCode::ArithmeticOverflow);
                continue;
            }

            auto start = absolute_to_score_time_unbounded(
                tick_to_absolute_beat(active->event.tick, midi.ppq), time_map);
            auto end = absolute_to_score_time_unbounded(
                tick_to_absolute_beat(point.event.tick, midi.ppq), time_map);
            if (!start || !end || !(*start < *end))
                return std::unexpected(ErrorCode::IngestionFailed);

            result.directives.push_back(PartDirective{
                *start,
                *end,
                controller == 64 ? DirectiveType::SustainingPedal : DirectiveType::UnaCorda,
                0});
            if (!add_count(result.imported_control_changes, 2) ||
                !add_count(result.residual_control_changes, active->source_count - 1) ||
                !add_count(result.residual_control_changes, point.source_count - 1))
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            if (active->event.value != 127) {
                if (!add_count(result.normalised_pedal_values, 1))
                    return std::unexpected(ErrorCode::ArithmeticOverflow);
            }
            if (point.event.value != 0) {
                if (!add_count(result.normalised_pedal_values, 1))
                    return std::unexpected(ErrorCode::ArithmeticOverflow);
            }
            active.reset();
        }
        if (active && !add_count(result.residual_control_changes, active->source_count))
            return std::unexpected(ErrorCode::ArithmeticOverflow);
    }

    for (const auto& event : midi.control_changes) {
        if (event.channel != *sole_note_channel ||
            (event.controller != 64 && event.controller != 67)) {
            if (!add_count(result.residual_control_changes, 1))
                return std::unexpected(ErrorCode::ArithmeticOverflow);
        }
    }
    if (result.imported_control_changes > midi.control_changes.size() ||
        result.residual_control_changes !=
            midi.control_changes.size() - result.imported_control_changes)
        return std::unexpected(ErrorCode::InvariantViolation);
    return result;
}

Result<MidiTimeSignatureIngestion> midi_time_signature_map(const MidiFile& midi) {
    auto events = midi.time_signatures;
    std::stable_sort(events.begin(), events.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.tick < rhs.tick;
    });

    auto initial = make_time_signature(4, 4);
    if (!initial) return std::unexpected(initial.error());
    MidiTimeSignatureIngestion result{{{1, *initial}}, 0};
    Beat previous_absolute = Beat::zero();
    std::uint32_t previous_bar = 1;
    TimeSignature previous_signature = *initial;

    std::size_t index = 0;
    while (index < events.size()) {
        const auto event = events[index];
        std::size_t next = index + 1;
        while (next < events.size() && events[next].tick == event.tick) {
            if (events[next].numerator != event.numerator ||
                events[next].denominator != event.denominator ||
                events[next].clocks_per_metronome_click != event.clocks_per_metronome_click ||
                events[next].notated_32nds_per_quarter != event.notated_32nds_per_quarter)
                return std::unexpected(ErrorCode::IngestionFailed);
            ++next;
        }

        // Sunny fixes one notated quarter to one MIDI quarter. A different
        // `bb` changes the notated-32nd/MIDI-quarter relation and therefore
        // cannot enter the Score bar/tick algebra without silently rescaling.
        if (event.notated_32nds_per_quarter != 8)
            return std::unexpected(ErrorCode::TargetValueUnrepresentable);

        auto signature = make_time_signature(event.numerator, event.denominator);
        if (!signature) return std::unexpected(signature.error());
        const auto click_projection = project_smf_time_signature(*signature);
        if (event.clocks_per_metronome_click != click_projection.clocks_per_metronome_click) {
            if (result.metronome_click_residuals == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            result.metronome_click_residuals++;
        }
        const Beat absolute = tick_to_absolute_beat(event.tick, midi.ppq);
        auto elapsed = checked_sub(absolute, previous_absolute);
        auto previous_duration = checked_measure_duration(previous_signature);
        auto ratio = elapsed && previous_duration
                         ? checked_div(*elapsed, *previous_duration)
                         : Result<Beat>{std::unexpected(ErrorCode::ArithmeticOverflow)};
        if (!ratio || ratio->numerator() < 0 || ratio->numerator() % ratio->denominator() != 0)
            return std::unexpected(ErrorCode::IngestionFailed);
        const auto bars_elapsed = ratio->numerator() / ratio->denominator();
        if (static_cast<std::uint64_t>(bars_elapsed) >
            std::numeric_limits<std::uint32_t>::max() - previous_bar)
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        const auto event_bar = previous_bar + static_cast<std::uint32_t>(bars_elapsed);
        if (event.tick == 0) {
            result.map.front().time_signature = *signature;
        } else if (signature->groups() != result.map.back().time_signature.groups() ||
                   signature->denominator() != result.map.back().time_signature.denominator()) {
            result.map.push_back({event_bar, *signature});
        }
        previous_absolute = absolute;
        previous_bar = event_bar;
        previous_signature = *signature;
        index = next;
    }
    return result;
}

Result<KeySignature> midi_key_signature(const MidiKeySignatureEvent& event) {
    const auto mode = find_scale(event.minor ? "minor" : "major");
    if (!mode) return std::unexpected(mode.error());
    const int adjustment = event.minor ? -3 : 0;
    return KeySignature{
        from_line_of_fifths(event.accidentals - adjustment, 4), *mode, event.accidentals};
}

// =========================================================================
// Score construction helpers
// =========================================================================

/// Build a Score from voiced, quantised notes with an estimated key.
Result<Score> build_score_from_notes(const std::vector<VoicedNote>& notes,
                                     const std::string& title,
                                     const std::string& instrumentation,
                                     const TempoMap& tempo_map,
                                     const TimeSignatureMap& time_map,
                                     const KeySignatureMap& key_map,
                                     std::optional<std::uint8_t> source_note_channel,
                                     const std::vector<PartDirective>& part_directives,
                                     std::uint32_t total_bars) {
    if (tempo_map.empty() || time_map.empty() || key_map.empty())
        return std::unexpected(ErrorCode::IngestionFailed);

    // Build ScoreSpec
    ScoreSpec spec;
    spec.title = title;
    spec.total_bars = total_bars;
    spec.bpm = tempo_map.front().bpm.to_float();
    const auto initial_intervals = key_map.front().key.mode.get_intervals();
    spec.minor = initial_intervals.size() >= 3 && initial_intervals[2] == 3;
    spec.key_accidentals = key_map.front().key.accidentals;
    const int seed_adjustment = spec.minor ? -3 : 0;
    spec.key_root =
        from_line_of_fifths(static_cast<int>(key_map.front().key.accidentals) - seed_adjustment, 4);
    spec.time_sig_num = time_map.front().time_signature.numerator();
    spec.time_sig_den = time_map.front().time_signature.denominator();

    // Single part
    PartDefinition pd;
    pd.name = instrumentation;
    pd.abbreviation = instrumentation.substr(0, 3);
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    if (source_note_channel)
        pd.rendering.midi_channel = static_cast<std::uint8_t>(*source_note_channel + 1);
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    if (!score_result) return std::unexpected(score_result.error());

    Score score = std::move(*score_result);
    PartId part_id = score.parts[0].id;

    for (std::size_t index = 1; index < time_map.size(); ++index) {
        auto changed =
            set_time_signature(score, time_map[index].bar, time_map[index].time_signature);
        if (!changed) return std::unexpected(changed.error());
    }
    score.tempo_map = tempo_map;
    score.key_map = key_map;
    if (!is_compilable(score)) return std::unexpected(ErrorCode::IngestionFailed);

    std::uint8_t maximum_voice = 0;
    for (const auto& note : notes)
        maximum_voice = std::max(maximum_voice, note.voice);
    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        for (std::uint16_t voice = 1; voice <= maximum_voice; ++voice) {
            auto added = add_voice(score, bar, part_id, static_cast<std::uint8_t>(voice));
            if (!added) return std::unexpected(added.error());
        }
    }

    // Insert each note into the score
    for (const auto& vn : notes) {
        if (vn.start < Beat::zero() || vn.duration <= Beat::zero())
            return std::unexpected(ErrorCode::InvalidBeat);

        auto start = absolute_beat_to_score_time(vn.start, time_map, total_bars);
        if (!start || start->bar > total_bars) return std::unexpected(ErrorCode::IngestionFailed);
        std::uint32_t bar_index = start->bar - 1;
        Beat offset = start->beat;
        const auto active_key = query_key_at(score, *start);
        if (!active_key) return std::unexpected(ErrorCode::IngestionFailed);
        const int key_lof = line_of_fifths_position(active_key->root);
        SpelledPitch sp = default_spelling(
            PitchClass::wrapped(vn.pitch), key_lof, static_cast<std::int8_t>(vn.pitch / 12 - 1));

        struct NoteSegment {
            std::uint32_t bar_index;
            Beat offset;
            Beat duration;
        };
        std::vector<NoteSegment> segments;
        Beat remaining = vn.duration;
        while (remaining > Beat::zero()) {
            if (bar_index >= total_bars) return std::unexpected(ErrorCode::IngestionFailed);
            auto measure_duration =
                checked_measure_duration(query_time_signature_at(score, bar_index + 1));
            if (!measure_duration) return std::unexpected(measure_duration.error());
            const Beat available = *measure_duration - offset;
            if (available <= Beat::zero()) return std::unexpected(ErrorCode::IngestionFailed);
            const Beat segment = std::min(remaining, available);
            segments.push_back({bar_index, offset, segment});
            remaining = remaining - segment;
            ++bar_index;
            offset = Beat::zero();
        }

        // Insert destination first so every intermediate score satisfies S7: a tie source is
        // never temporarily present without its adjacent same-pitch continuation.
        for (std::size_t index = segments.size(); index-- > 0;) {
            Note note;
            note.pitch = sp;
            note.velocity = VelocityValue{std::nullopt, vn.velocity};
            note.release_velocity = vn.release_velocity;
            note.tie_forward = index + 1 < segments.size();
            const auto& segment = segments[index];
            auto inserted = insert_note(score,
                                        part_id,
                                        segment.bar_index + 1,
                                        vn.voice,
                                        segment.offset,
                                        note,
                                        segment.duration);
            if (!inserted) return std::unexpected(inserted.error());
        }
    }

    score.parts[0].part_directives = part_directives;

    if (!is_compilable(score)) return std::unexpected(ErrorCode::IngestionFailed);
    return score;
}

} // anonymous namespace

// =========================================================================
// MIDI Ingestion
// =========================================================================

Result<IngestedWork> ingest_midi(std::span<const std::uint8_t> midi_data,
                                 IngestedWorkId work_id,
                                 const IngestionOptions& options) {
    if (options.quantise_grid <= 0) return std::unexpected(ErrorCode::CorpusInvalidParameter);

    // Parse MIDI
    auto midi_result = parse_midi(midi_data);
    if (!midi_result) return std::unexpected(ErrorCode::IngestionFailed);

    const auto& midi = *midi_result;
    if (midi.notes.empty()) return std::unexpected(ErrorCode::IngestionFailed);

    // Convert to NoteEvents
    auto note_events_result = midi_to_note_events(midi);
    if (!note_events_result) return std::unexpected(note_events_result.error());
    auto note_events = std::move(*note_events_result);
    if (note_events.empty()) return std::unexpected(ErrorCode::IngestionFailed);

    std::set<std::uint8_t> note_channels;
    for (const auto& note : midi.notes)
        note_channels.insert(note.channel);
    const std::optional<std::uint8_t> sole_note_channel =
        note_channels.size() == 1 ? std::optional<std::uint8_t>{*note_channels.begin()}
                                  : std::nullopt;

    // Build PC histogram for key estimation
    std::array<std::uint32_t, 12> pc_hist{};
    for (const auto& ev : note_events)
        pc_hist[ev.pitch % 12]++;

    auto key_est = estimate_key(pc_hist);

    // Quantise
    auto quantisation = quantise_notes(note_events, options.quantise_grid);
    if (!quantisation) return std::unexpected(quantisation.error());

    // Voice separation
    auto separated = separate_voices(quantisation->notes);
    if (!separated) return std::unexpected(separated.error());
    auto voiced = std::move(separated->notes);

    auto imported_time_map = midi_time_signature_map(midi);
    if (!imported_time_map) return std::unexpected(ErrorCode::IngestionFailed);
    auto imported_channel_state =
        midi_channel_state(midi, imported_time_map->map, sole_note_channel);
    if (!imported_channel_state) return std::unexpected(ErrorCode::IngestionFailed);

    // Compute total bars from note extent
    Beat max_end = Beat::zero();
    for (const auto& vn : voiced) {
        const Beat end = vn.start + vn.duration;
        if (end > max_end) max_end = end;
    }
    auto note_end = absolute_to_score_time_unbounded(max_end, imported_time_map->map);
    if (!note_end) return std::unexpected(ErrorCode::IngestionFailed);
    std::uint32_t total_bars =
        note_end->beat == Beat::zero() && note_end->bar > 1 ? note_end->bar - 1 : note_end->bar;

    std::uint32_t maximum_metadata_tick = 0;
    for (const auto& event : midi.tempos)
        maximum_metadata_tick = std::max(maximum_metadata_tick, event.tick);
    for (const auto& event : midi.time_signatures)
        maximum_metadata_tick = std::max(maximum_metadata_tick, event.tick);
    for (const auto& event : midi.key_signatures)
        maximum_metadata_tick = std::max(maximum_metadata_tick, event.tick);
    auto metadata_position = absolute_to_score_time_unbounded(
        tick_to_absolute_beat(maximum_metadata_tick, midi.ppq), imported_time_map->map);
    if (!metadata_position) return std::unexpected(ErrorCode::IngestionFailed);
    total_bars =
        std::max(std::max(total_bars, metadata_position->bar), imported_time_map->map.back().bar);
    for (const auto& directive : imported_channel_state->directives) {
        total_bars = std::max(total_bars, directive.start.bar);
        const auto required_end_bar = directive.end.beat == Beat::zero() && directive.end.bar > 1
                                          ? directive.end.bar - 1
                                          : directive.end.bar;
        total_bars = std::max(total_bars, required_end_bar);
    }
    total_bars = std::max(total_bars, 1U);

    TempoMap imported_tempo_map;
    auto tempo_events = midi.tempos;
    std::stable_sort(tempo_events.begin(),
                     tempo_events.end(),
                     [](const auto& lhs, const auto& rhs) { return lhs.tick < rhs.tick; });
    if (tempo_events.empty() || tempo_events.front().tick != 0)
        imported_tempo_map.push_back(TempoEvent{SCORE_START,
                                                make_bpm(120),
                                                BeatUnit::Quarter,
                                                TempoTransitionType::Immediate,
                                                Beat::zero(),
                                                BeatUnit::Quarter,
                                                BeatUnit::Quarter});
    for (std::size_t index = 0; index < tempo_events.size();) {
        const auto event = tempo_events[index];
        std::size_t next = index + 1;
        while (next < tempo_events.size() && tempo_events[next].tick == event.tick) {
            if (tempo_events[next].microseconds_per_beat != event.microseconds_per_beat)
                return std::unexpected(ErrorCode::IngestionFailed);
            ++next;
        }
        const Beat reduced_bpm = Beat::normalise(60'000'000, event.microseconds_per_beat);
        const PositiveRational bpm{reduced_bpm.numerator(), reduced_bpm.denominator()};
        if (bpm.to_float() < Constants::TEMPO_MIN_BPM || bpm.to_float() > Constants::TEMPO_MAX_BPM)
            return std::unexpected(ErrorCode::IngestionFailed);
        auto position = absolute_to_score_time_unbounded(
            tick_to_absolute_beat(event.tick, midi.ppq), imported_time_map->map);
        if (!position || position->bar > total_bars)
            return std::unexpected(ErrorCode::IngestionFailed);
        TempoEvent imported{*position,
                            bpm,
                            BeatUnit::Quarter,
                            TempoTransitionType::Immediate,
                            Beat::zero(),
                            BeatUnit::Quarter,
                            BeatUnit::Quarter};
        if (event.tick == 0) {
            if (imported_tempo_map.empty())
                imported_tempo_map.push_back(imported);
            else
                imported_tempo_map.front() = imported;
        } else if (imported_tempo_map.empty() || imported_tempo_map.back().bpm != bpm) {
            imported_tempo_map.push_back(imported);
        }
        index = next;
    }

    const auto estimated_mode = find_scale(key_est.is_minor ? "minor" : "major");
    if (!estimated_mode) return std::unexpected(ErrorCode::IngestionFailed);
    KeySignature estimated_key{default_spelling(key_est.tonic, 0, 4), *estimated_mode, 0};
    const auto estimated_fifths = expected_key_accidentals(estimated_key);
    if (!estimated_fifths || *estimated_fifths < std::numeric_limits<std::int8_t>::min() ||
        *estimated_fifths > std::numeric_limits<std::int8_t>::max())
        return std::unexpected(ErrorCode::IngestionFailed);
    estimated_key.accidentals = static_cast<std::int8_t>(*estimated_fifths);
    KeySignatureMap imported_key_map{{SCORE_START, estimated_key}};
    auto key_events = midi.key_signatures;
    std::stable_sort(key_events.begin(), key_events.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.tick < rhs.tick;
    });
    for (std::size_t index = 0; index < key_events.size();) {
        const auto event = key_events[index];
        std::size_t next = index + 1;
        while (next < key_events.size() && key_events[next].tick == event.tick) {
            if (key_events[next].accidentals != event.accidentals ||
                key_events[next].minor != event.minor)
                return std::unexpected(ErrorCode::IngestionFailed);
            ++next;
        }
        auto key = midi_key_signature(event);
        auto position = absolute_to_score_time_unbounded(
            tick_to_absolute_beat(event.tick, midi.ppq), imported_time_map->map);
        if (!key || !position || position->bar > total_bars)
            return std::unexpected(ErrorCode::IngestionFailed);
        if (event.tick == 0)
            imported_key_map.front() = KeySignatureEntry{*position, *key};
        else if (imported_key_map.back().key != *key)
            imported_key_map.push_back(KeySignatureEntry{*position, *key});
        index = next;
    }

    // Build Score
    auto score_result = build_score_from_notes(voiced,
                                               options.title,
                                               options.instrumentation,
                                               imported_tempo_map,
                                               imported_time_map->map,
                                               imported_key_map,
                                               sole_note_channel,
                                               imported_channel_state->directives,
                                               total_bars);

    if (!score_result) return std::unexpected(ErrorCode::IngestionFailed);

    // Populate IngestedWork
    IngestedWork work;
    work.id = work_id;
    work.score = std::move(*score_result);

    // Metadata
    work.metadata.title = options.title;
    work.metadata.instrumentation = options.instrumentation;
    work.metadata.source_format = "midi";

    // Confidence
    const bool explicit_initial_key =
        std::any_of(midi.key_signatures.begin(), midi.key_signatures.end(), [](const auto& event) {
            return event.tick == 0;
        });
    const bool explicit_initial_metre =
        std::any_of(midi.time_signatures.begin(),
                    midi.time_signatures.end(),
                    [](const auto& event) { return event.tick == 0; });
    work.ingestion_confidence.key_confidence = explicit_initial_key ? 1.0f : key_est.confidence;
    work.ingestion_confidence.metre_confidence = explicit_initial_metre ? 1.0f : 0.8f;
    work.ingestion_confidence.spelling_confidence =
        std::clamp(key_est.confidence * 0.9f, 0.5f, 1.0f);
    work.ingestion_confidence.voice_separation_confidence = separated->confidence;
    work.ingestion_confidence.quantisation_residual = quantisation->onset_rms;
    work.ingestion_confidence.duration_quantisation_residual = quantisation->duration_rms;
    work.ingestion_confidence.source_format = "midi";
    if (separated->complex_onsets != 0)
        work.ingestion_confidence.manual_corrections.push_back(
            {"midi.voice_separation",
             std::to_string(separated->complex_onsets) + " of " +
                 std::to_string(separated->total_onsets) +
                 " quantised onset(s) required deterministic register allocation because notes "
                 "were simultaneous or a prior note was still sounding; source voice identity "
                 "was not present in SMF"});
    if (imported_time_map->metronome_click_residuals != 0)
        work.ingestion_confidence.manual_corrections.push_back(
            {"midi.time_signature_metronome_clicks",
             std::to_string(imported_time_map->metronome_click_residuals) +
                 " Time Signature metronome-click interval(s) differed from Sunny's deterministic "
                 "flat-metre projection and were not reinterpreted as ordered grouping"});
    if (note_channels.size() > 1)
        work.ingestion_confidence.manual_corrections.push_back(
            {"midi.note_channel_ownership",
             std::to_string(note_channels.size()) +
                 " source MIDI note channels were collapsed into one analytical Score Part; "
                 "channel-local routing and controller ownership were not invented"});
    if (midi.track_count > 1)
        work.ingestion_confidence.manual_corrections.push_back(
            {"midi.track_topology",
             "SMF Type 1 declared " + std::to_string(midi.track_count) +
                 " source tracks; the analytical Score projection retains their notes but not "
                 "track or empty-track ownership"});
    if (imported_channel_state->normalised_pedal_values != 0)
        work.ingestion_confidence.manual_corrections.push_back(
            {"midi.pedal_switch_values",
             std::to_string(imported_channel_state->normalised_pedal_values) +
                 " CC64/CC67 switch value(s) were semantically imported using the MIDI "
                 "0..63=off and 64..127=on thresholds; Score recompilation canonicalises them "
                 "to 0/127"});
    const auto record_parse_loss =
        [&](std::uint64_t count, std::string field, std::string event_class) {
            if (count == 0) return;
            work.ingestion_confidence.manual_corrections.push_back(
                {std::move(field),
                 std::to_string(count) + " valid " + std::move(event_class) +
                     " event(s) were outside the retained MIDI analysis profile"});
        };
    record_parse_loss(midi.parse_loss.meta_events, "midi.meta_events", "meta");
    record_parse_loss(midi.parse_loss.sysex_events, "midi.sysex_events", "SysEx");
    record_parse_loss(midi.parse_loss.polyphonic_aftertouch_events,
                      "midi.polyphonic_aftertouch_events",
                      "polyphonic-aftertouch");
    record_parse_loss(midi.parse_loss.channel_pressure_events,
                      "midi.channel_pressure_events",
                      "channel-pressure");
    record_parse_loss(midi.parse_loss.pitch_bend_events, "midi.pitch_bend_events", "pitch-bend");
    record_parse_loss(imported_channel_state->residual_control_changes,
                      "midi.control_change_events",
                      "control-change without an exact Score timeline carrier");
    std::uint64_t program_change_count = 0;
    if (!add_count(program_change_count, midi.program_changes.size()))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    record_parse_loss(program_change_count,
                      "midi.program_change_events",
                      "program-change without a source-semantic Score carrier");

    // Run analysis
    auto analysis = analyze_score(*work.score);
    if (!analysis) return std::unexpected(analysis.error());
    work.analysis = std::move(*analysis);
    work.analysis_complete = true;

    return work;
}

// =========================================================================
// MusicXML Ingestion
// =========================================================================

Result<IngestedWork> ingest_musicxml(std::string_view musicxml,
                                     IngestedWorkId work_id,
                                     const IngestionOptions& options) {
    auto xml_result = parse_musicxml(musicxml);
    if (!xml_result) return std::unexpected(ErrorCode::IngestionFailed);

    const auto& xml_score = *xml_result;
    if (xml_score.parts.empty()) return std::unexpected(ErrorCode::IngestionFailed);
    const auto measure_count = xml_score.parts.front().measures.size();
    if (measure_count == 0 || std::any_of(xml_score.parts.begin(),
                                          xml_score.parts.end(),
                                          [measure_count](const auto& part) {
                                              return part.measures.size() != measure_count;
                                          }))
        return std::unexpected(ErrorCode::IngestionFailed);

    if (measure_count > std::numeric_limits<std::uint32_t>::max() ||
        measure_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    const auto total_bars = static_cast<std::uint32_t>(measure_count);
    for (const auto& part : xml_score.parts) {
        for (std::size_t index = 0; index < measure_count; ++index) {
            if (part.measures[index].number != static_cast<int>(index + 1))
                return std::unexpected(ErrorCode::IngestionFailed);
        }
    }

    std::array<std::uint32_t, 12> pc_hist{};
    for (const auto& part : xml_score.parts)
        for (const auto& measure : part.measures)
            for (const auto& note : measure.notes)
                if (!note.is_rest) pc_hist[pc(note.pitch)]++;
    auto key_est = estimate_key(pc_hist);

    auto default_time = make_time_signature(4, 4);
    if (!default_time) return std::unexpected(ErrorCode::IngestionFailed);
    std::vector<TimeSignature> canonical_times(measure_count, *default_time);
    std::vector<std::optional<KeySignature>> canonical_keys(measure_count);

    for (std::size_t part_index = 0; part_index < xml_score.parts.size(); ++part_index) {
        TimeSignature active_time = *default_time;
        std::optional<KeySignature> active_key;
        for (std::size_t measure_index = 0; measure_index < measure_count; ++measure_index) {
            const auto& measure = xml_score.parts[part_index].measures[measure_index];
            if (measure.time_signature) {
                auto parsed_time = make_time_signature(measure.time_signature->first,
                                                       measure.time_signature->second);
                if (!parsed_time) return std::unexpected(ErrorCode::IngestionFailed);
                active_time = *parsed_time;
            }
            if (measure.key_fifths) {
                const auto mode_name = measure.key_mode.value_or("major");
                const auto mode = find_scale(mode_name);
                if (!mode || !measure.key_tonic ||
                    *measure.key_fifths < std::numeric_limits<std::int8_t>::min() ||
                    *measure.key_fifths > std::numeric_limits<std::int8_t>::max())
                    return std::unexpected(ErrorCode::IngestionFailed);
                active_key = KeySignature{
                    *measure.key_tonic, *mode, static_cast<std::int8_t>(*measure.key_fifths)};
            }

            if (part_index == 0) {
                canonical_times[measure_index] = active_time;
                canonical_keys[measure_index] = active_key;
            } else {
                const auto& expected_time = canonical_times[measure_index];
                if (active_time.groups() != expected_time.groups() ||
                    active_time.denominator() != expected_time.denominator() ||
                    active_key != canonical_keys[measure_index])
                    return std::unexpected(ErrorCode::IngestionFailed);
            }
        }
    }

    TimeSignatureMap imported_time_map{{1, canonical_times.front()}};
    for (std::size_t index = 1; index < canonical_times.size(); ++index) {
        if (canonical_times[index].groups() != canonical_times[index - 1].groups() ||
            canonical_times[index].denominator() != canonical_times[index - 1].denominator())
            imported_time_map.push_back(
                {static_cast<std::uint32_t>(index + 1), canonical_times[index]});
    }

    const auto estimated_mode = find_scale(key_est.is_minor ? "minor" : "major");
    if (!estimated_mode) return std::unexpected(ErrorCode::IngestionFailed);
    KeySignature initial_key{default_spelling(key_est.tonic, 0, 4), *estimated_mode, 0};
    const auto estimated_fifths = expected_key_accidentals(initial_key);
    if (!estimated_fifths || *estimated_fifths < std::numeric_limits<std::int8_t>::min() ||
        *estimated_fifths > std::numeric_limits<std::int8_t>::max())
        return std::unexpected(ErrorCode::IngestionFailed);
    initial_key.accidentals = static_cast<std::int8_t>(*estimated_fifths);
    if (canonical_keys.front()) initial_key = *canonical_keys.front();

    KeySignatureMap imported_key_map{{SCORE_START, initial_key}};
    std::optional<KeySignature> previous_key = canonical_keys.front();
    for (std::size_t index = 1; index < canonical_keys.size(); ++index) {
        if (canonical_keys[index] && canonical_keys[index] != previous_key) {
            imported_key_map.push_back(
                KeySignatureEntry{ScoreTime{static_cast<std::uint32_t>(index + 1), Beat::zero()},
                                  *canonical_keys[index]});
        }
        previous_key = canonical_keys[index];
    }

    const auto initial_intervals = initial_key.mode.get_intervals();
    key_est.tonic = pc(initial_key.root);
    key_est.is_minor = initial_intervals.size() >= 3 && initial_intervals[2] == 3;
    const bool explicit_initial_key = canonical_keys.front().has_value();
    const bool explicit_initial_metre =
        xml_score.parts.front().measures.front().time_signature.has_value();
    if (explicit_initial_key) key_est.confidence = 1.0f;

    ScoreSpec spec;
    spec.title = options.title.empty() ? xml_score.title : options.title;
    spec.total_bars = total_bars;
    spec.bpm = 120.0;
    spec.minor = key_est.is_minor;
    spec.key_accidentals = initial_key.accidentals;
    const int seed_adjustment = spec.minor ? -3 : 0;
    spec.key_root =
        from_line_of_fifths(static_cast<int>(initial_key.accidentals) - seed_adjustment, 4);
    spec.time_sig_num = imported_time_map.front().time_signature.numerator();
    spec.time_sig_den = imported_time_map.front().time_signature.denominator();

    // Create one part per MusicXML part
    for (const auto& xml_part : xml_score.parts) {
        PartDefinition pd;
        pd.name = xml_part.name.empty() ? options.instrumentation : xml_part.name;
        pd.abbreviation = pd.name.substr(0, 3);
        pd.instrument_type = InstrumentType::Piano;
        pd.clef = Clef::Treble;
        spec.parts.push_back(pd);
    }
    if (spec.parts.empty()) {
        PartDefinition pd;
        pd.name = options.instrumentation;
        pd.abbreviation = pd.name.substr(0, 3);
        pd.instrument_type = InstrumentType::Piano;
        pd.clef = Clef::Treble;
        spec.parts.push_back(pd);
    }

    auto score_result = create_score(spec);
    if (!score_result) return std::unexpected(ErrorCode::IngestionFailed);

    Score score = std::move(*score_result);
    for (std::size_t index = 1; index < imported_time_map.size(); ++index) {
        auto changed = set_time_signature(
            score, imported_time_map[index].bar, imported_time_map[index].time_signature);
        if (!changed) return std::unexpected(ErrorCode::IngestionFailed);
    }
    score.key_map = imported_key_map;
    if (!is_compilable(score)) return std::unexpected(ErrorCode::IngestionFailed);

    // Populate notes from MusicXML into Score
    for (std::size_t pi = 0; pi < xml_score.parts.size() && pi < score.parts.size(); ++pi) {
        PartId part_id = score.parts[pi].id;
        const auto& xml_part = xml_score.parts[pi];

        for (std::size_t mi = 0; mi < xml_part.measures.size() && mi < total_bars; ++mi) {
            const auto& xml_measure = xml_part.measures[mi];
            Beat offset = Beat::zero();
            Beat last_note_offset = Beat::zero();

            std::set<std::uint8_t> required_voices;
            for (const auto& xml_note : xml_measure.notes) {
                if (xml_note.voice < 1 || xml_note.voice > 256)
                    return std::unexpected(ErrorCode::IngestionFailed);
                required_voices.insert(static_cast<std::uint8_t>(xml_note.voice - 1));
            }
            for (const auto voice : required_voices) {
                if (voice == 0) continue;
                auto added = add_voice(score, static_cast<std::uint32_t>(mi + 1), part_id, voice);
                if (!added) return std::unexpected(ErrorCode::IngestionFailed);
            }

            for (const auto& xml_note : xml_measure.notes) {
                if (xml_note.is_rest) {
                    offset = offset + xml_note.duration;
                    continue;
                }

                Note note;
                note.pitch = xml_note.pitch;
                note.velocity = VelocityValue{std::nullopt, 80};

                auto v = static_cast<std::uint8_t>(std::max(0, xml_note.voice - 1));

                const Beat insertion_offset = xml_note.is_chord ? last_note_offset : offset;
                auto inserted = insert_note(score,
                                            part_id,
                                            static_cast<std::uint32_t>(mi + 1),
                                            v,
                                            insertion_offset,
                                            note,
                                            xml_note.duration);
                if (!inserted) return std::unexpected(ErrorCode::IngestionFailed);

                if (!xml_note.is_chord) {
                    last_note_offset = offset;
                    offset = offset + xml_note.duration;
                }
            }

            // MusicXML harmony has staff ownership but no independent voice
            // identity in this compact single-staff profile. Place it in the
            // primary Score voice at its reconstructed measure-local point.
            for (const auto& xml_harmony : xml_measure.harmonies) {
                auto inserted = insert_chord_symbol(score,
                                                    part_id,
                                                    static_cast<std::uint32_t>(mi + 1),
                                                    0,
                                                    xml_harmony.offset,
                                                    xml_harmony.symbol);
                if (!inserted) return std::unexpected(ErrorCode::IngestionFailed);
            }
        }
    }

    if (!is_compilable(score)) return std::unexpected(ErrorCode::IngestionFailed);

    // Populate IngestedWork
    IngestedWork work;
    work.id = work_id;
    work.score = std::move(score);

    work.metadata.title = spec.title;
    work.metadata.instrumentation = options.instrumentation;
    work.metadata.source_format = "musicxml";

    work.ingestion_confidence.key_confidence = key_est.confidence;
    work.ingestion_confidence.metre_confidence = explicit_initial_metre ? 1.0f : 0.8f;
    work.ingestion_confidence.spelling_confidence = 1.0f;
    work.ingestion_confidence.voice_separation_confidence = 1.0f;
    work.ingestion_confidence.quantisation_residual = 0.0f;
    work.ingestion_confidence.duration_quantisation_residual = 0.0f;
    work.ingestion_confidence.source_format = "musicxml";

    auto analysis = analyze_score(*work.score);
    if (!analysis) return std::unexpected(analysis.error());
    work.analysis = std::move(*analysis);
    work.analysis_complete = true;

    return work;
}

} // namespace sunny::infrastructure::corpus
