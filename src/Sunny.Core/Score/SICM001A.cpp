/**
 * @file SICM001A.cpp
 * @brief Score IR MIDI compilation — implementation
 *
 * Component: SICM001A
 * Domain: SI (Score IR) | Category: CM (Compilation)
 */

#include "SICM001A.h"
#include "SITM001A.h"
#include "SIVD001A.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Sunny::Core {

namespace {

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

bool is_minor_key(const KeySignature& key) {
    auto ints = key.mode.get_intervals();
    // Minor third at scale degree 3 distinguishes minor from major
    return ints.size() >= 3 && ints[2] == 3;
}

std::uint8_t log2_power_of_two(int n) {
    std::uint8_t result = 0;
    while (n > 1) {
        n >>= 1;
        ++result;
    }
    return result;
}

/// Find the most recent explicit dynamic at or before a position in a part.
/// Scans all voices; defaults to mf if none found.
DynamicLevel find_dynamic_before(const Part& part, ScoreTime position) {
    DynamicLevel current = DynamicLevel::mf;
    for (const auto& measure : part.measures) {
        ScoreTime measure_start{measure.bar_number, Beat::zero()};
        if (measure_start > position) break;
        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                ScoreTime event_time{measure.bar_number, event.offset};
                if (event_time > position) continue;
                const auto* ng = event.as_note_group();
                if (!ng) continue;
                for (const auto& note : ng->notes) {
                    if (note.dynamic) current = *note.dynamic;
                }
            }
        }
    }
    return current;
}

/// Find hairpin active at a position for a part. Returns nullptr if none.
const Hairpin* find_active_hairpin(const Part& part, ScoreTime position) {
    for (const auto& h : part.hairpins) {
        if (h.start <= position && position < h.end) return &h;
    }
    return nullptr;
}

/// Find orchestration balance annotation for a part at a position.
std::optional<DynamicBalance> find_balance(
    const Score& score, PartId part_id, ScoreTime position
) {
    for (const auto& ann : score.orchestration_annotations) {
        if (ann.part_id == part_id &&
            ann.start <= position && position < ann.end &&
            ann.dynamic_balance) {
            return ann.dynamic_balance;
        }
    }
    return std::nullopt;
}

/// Compute interpolation ratio within a hairpin using ScoreTime as proxy.
/// Returns value in [0, 1].
double hairpin_ratio(const Hairpin& h, ScoreTime position) {
    double start = static_cast<double>(h.start.bar) + h.start.beat.to_float();
    double end = static_cast<double>(h.end.bar) + h.end.beat.to_float();
    double pos = static_cast<double>(position.bar) + position.beat.to_float();
    double span = end - start;
    if (span <= 0.0) return 0.0;
    return std::clamp((pos - start) / span, 0.0, 1.0);
}

/// Resolve MIDI velocity for a note through the four-stage pipeline:
/// base dynamic -> hairpin interpolation -> articulation offset -> balance factor.
std::uint8_t resolve_velocity(
    const Note& note,
    DynamicLevel current_dynamic,
    const Part& part,
    const Score& score,
    ScoreTime position
) {
    // 1. Base velocity from the note's own dynamic or the current tracking dynamic
    DynamicLevel effective = note.dynamic.value_or(current_dynamic);
    double vel = static_cast<double>(default_velocity(effective));

    // 2. Hairpin interpolation overrides the base if active
    const Hairpin* hairpin = find_active_hairpin(part, position);
    if (hairpin && hairpin->target) {
        DynamicLevel start_dyn = find_dynamic_before(part, hairpin->start);
        double start_vel = static_cast<double>(default_velocity(start_dyn));
        double target_vel = static_cast<double>(default_velocity(*hairpin->target));
        double ratio = hairpin_ratio(*hairpin, position);
        vel = start_vel + ratio * (target_vel - start_vel);
    }

    // 3. Articulation velocity offsets
    if (note.articulation) {
        switch (*note.articulation) {
            case ArticulationType::Accent:    vel += 20.0; break;
            case ArticulationType::Marcato:   vel += 30.0; break;
            case ArticulationType::Sforzando: vel = std::min(127.0, vel + 40.0); break;
            default: break;
        }
    }

    // 4. Orchestration balance factor
    auto balance = find_balance(score, part.id, position);
    if (balance) vel *= balance_factor(*balance);

    // 5. Clamp to valid MIDI velocity range
    int clamped = static_cast<int>(std::round(vel));
    return static_cast<std::uint8_t>(std::clamp(clamped, 1, 127));
}

/// Flat note reference for tie-chain traversal within a voice.
struct FlatNote {
    ScoreTime position;
    const NoteGroup* ng;
    EventId event_id;
};

}  // anonymous namespace

// =============================================================================
// compile_to_midi
// =============================================================================

Result<CompiledMidi> compile_to_midi(const Score& score, int ppq) {
    if (!is_compilable(score)) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }

    CompiledMidi midi;
    midi.ppq = static_cast<std::uint16_t>(ppq);

    // --- Tempo meta events ---
    for (const auto& te : score.tempo_map) {
        auto abs_beat = score_time_to_absolute_beat(te.position, score.time_map);
        if (!abs_beat) continue;

        std::int64_t tick = absolute_beat_to_tick(*abs_beat, ppq);
        double qbpm = effective_quarter_bpm(te);
        auto uspb = static_cast<std::uint32_t>(60'000'000.0 / qbpm);

        midi.tempos.push_back(MidiTempoData{tick, uspb});
    }

    // --- Time signature meta events ---
    for (const auto& tse : score.time_map) {
        auto abs_beat = score_time_to_absolute_beat(
            ScoreTime{tse.bar, Beat::zero()}, score.time_map);
        if (!abs_beat) continue;

        std::int64_t tick = absolute_beat_to_tick(*abs_beat, ppq);
        auto num = static_cast<std::uint8_t>(tse.time_signature.numerator());
        std::uint8_t denom_exp = log2_power_of_two(tse.time_signature.denominator);

        midi.time_signatures.push_back(MidiTimeSigData{tick, num, denom_exp});
    }

    // --- Key signature meta events ---
    for (const auto& ke : score.key_map) {
        auto abs_beat = score_time_to_absolute_beat(ke.position, score.time_map);
        if (!abs_beat) continue;

        std::int64_t tick = absolute_beat_to_tick(*abs_beat, ppq);
        bool minor = is_minor_key(ke.key);

        midi.key_signatures.push_back(MidiKeySigData{tick, ke.key.accidentals, minor});
    }

    // --- Note events per part ---
    for (const auto& part : score.parts) {
        std::uint8_t channel = part.definition.rendering.midi_channel;

        // Collect all voice indices used across this part's measures
        std::set<std::uint8_t> voice_indices;
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                voice_indices.insert(v.voice_index);
            }
        }

        for (std::uint8_t vi : voice_indices) {
            // Build flat event list for this voice across all measures
            std::vector<FlatNote> flat;
            for (const auto& m : part.measures) {
                for (const auto& v : m.voices) {
                    if (v.voice_index != vi) continue;
                    for (const auto& event : v.events) {
                        const auto* ng = event.as_note_group();
                        if (!ng) continue;
                        flat.push_back({
                            ScoreTime{m.bar_number, event.offset},
                            ng,
                            event.id
                        });
                    }
                }
            }

            // Track consumed notes for tie handling
            std::set<std::pair<std::uint64_t, std::uint8_t>> consumed;
            DynamicLevel current_dynamic = DynamicLevel::mf;

            for (std::size_t fi = 0; fi < flat.size(); ++fi) {
                const auto& fn = flat[fi];
                const NoteGroup& ng = *fn.ng;

                for (std::uint8_t ni = 0; ni < ng.notes.size(); ++ni) {
                    if (consumed.contains({fn.event_id.value, ni})) continue;

                    const Note& note = ng.notes[ni];

                    // Update running dynamic tracker
                    if (note.dynamic) current_dynamic = *note.dynamic;

                    // Resolve velocity through four-stage pipeline
                    std::uint8_t velocity = resolve_velocity(
                        note, current_dynamic, part, score, fn.position);

                    int midi_note = midi_value(note.pitch);
                    if (midi_note < 0 || midi_note > 127) continue;

                    // Accumulate duration through tie chain
                    Beat total_duration = ng.duration;

                    if (note.tie_forward) {
                        SpelledPitch tied_pitch = note.pitch;
                        std::size_t search = fi + 1;
                        bool still_tied = true;

                        while (still_tied && search < flat.size()) {
                            still_tied = false;
                            const auto& next_fn = flat[search];
                            const NoteGroup& next_ng = *next_fn.ng;

                            for (std::uint8_t nni = 0; nni < next_ng.notes.size(); ++nni) {
                                if (consumed.contains({next_fn.event_id.value, nni})) continue;
                                if (next_ng.notes[nni].pitch == tied_pitch) {
                                    consumed.insert({next_fn.event_id.value, nni});
                                    total_duration = total_duration + next_ng.duration;

                                    if (next_ng.notes[nni].tie_forward) {
                                        still_tied = true;
                                        ++search;
                                    }
                                    break;
                                }
                            }
                        }
                    }

                    // Apply articulation duration factor
                    double dur_factor = 1.0;
                    if (note.articulation) {
                        dur_factor = articulation_duration_factor(*note.articulation);
                    }

                    // Convert to ticks
                    auto abs_start = score_time_to_absolute_beat(
                        fn.position, score.time_map);
                    if (!abs_start) continue;

                    std::int64_t start_tick = absolute_beat_to_tick(*abs_start, ppq);
                    std::int64_t dur_ticks = absolute_beat_to_tick(total_duration, ppq);

                    dur_ticks = static_cast<std::int64_t>(
                        static_cast<double>(dur_ticks) * dur_factor);
                    if (dur_ticks < 1) dur_ticks = 1;

                    midi.notes.push_back(MidiNoteData{
                        start_tick,
                        dur_ticks,
                        channel,
                        static_cast<std::uint8_t>(midi_note),
                        velocity
                    });
                }
            }
        }
    }

    // Sort notes by tick for sequential consumption
    std::sort(midi.notes.begin(), midi.notes.end(),
        [](const MidiNoteData& a, const MidiNoteData& b) {
            return a.tick < b.tick;
        });

    return midi;
}

// =============================================================================
// compile_to_note_events
// =============================================================================

Result<std::vector<NoteEvent>> compile_to_note_events(const Score& score) {
    std::vector<NoteEvent> events;

    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    ScoreTime position{measure.bar_number, event.offset};
                    auto abs_beat = score_time_to_absolute_beat(position, score.time_map);
                    if (!abs_beat) continue;

                    for (const auto& note : ng->notes) {
                        // Skip grace notes
                        if (note.grace) continue;

                        int mv = midi_value(note.pitch);
                        if (mv < 0 || mv > 127) continue;

                        NoteEvent ne;
                        ne.pitch = static_cast<MidiNote>(mv);
                        ne.start_time = *abs_beat;
                        ne.duration = ng->duration;
                        ne.velocity = note.velocity.value;
                        ne.muted = false;

                        events.push_back(ne);
                    }
                }
            }
        }
    }

    // Sort by start time
    std::sort(events.begin(), events.end(),
        [](const NoteEvent& a, const NoteEvent& b) {
            return a.start_time < b.start_time;
        });

    return events;
}

}  // namespace Sunny::Core
