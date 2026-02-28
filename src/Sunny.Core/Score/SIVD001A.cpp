/**
 * @file SIVD001A.cpp
 * @brief Score IR validation — implementation
 *
 * Component: SIVD001A
 * Domain: SI (Score IR) | Category: VD (Validation)
 */

#include "SIVD001A.h"
#include "SITM001A.h"
#include "../VoiceLeading/VLNT001A.h"
#include "../PostTonal/SRTW001A.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

namespace Sunny::Core {

namespace {

// =============================================================================
// Helper: create diagnostic
// =============================================================================

Diagnostic make_diagnostic(
    ValidationSeverity severity,
    const std::string& rule,
    const std::string& message,
    int error_code,
    std::optional<ScoreTime> location = std::nullopt,
    std::optional<PartId> part = std::nullopt
) {
    return Diagnostic{severity, rule, message, location, part, error_code};
}

// =============================================================================
// S1: Every part has exactly total_bars measures
// =============================================================================

void validate_s1(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        if (part.measures.size() != score.metadata.total_bars) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S1",
                "Part '" + part.definition.name + "' has " +
                std::to_string(part.measures.size()) + " measures, expected " +
                std::to_string(score.metadata.total_bars),
                ScoreError::MeasureCountMismatch,
                std::nullopt, part.id
            ));
        }
    }
}

// =============================================================================
// S2: Every voice's events fill exactly the measure duration
// =============================================================================

void validate_s2(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            // Determine expected measure duration
            const TimeSignatureEntry* ts_entry = nullptr;
            for (const auto& e : score.time_map) {
                if (e.bar <= measure.bar_number) ts_entry = &e;
                else break;
            }
            if (!ts_entry) continue;

            Beat expected = measure.local_time
                ? measure.local_time->measure_duration()
                : ts_entry->time_signature.measure_duration();

            for (const auto& voice : measure.voices) {
                Beat total = Beat::zero();
                for (const auto& event : voice.events) {
                    total = total + event.duration();
                }
                if (!(total == expected)) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Error, "S2",
                        "Voice " + std::to_string(voice.voice_index) +
                        " in bar " + std::to_string(measure.bar_number) +
                        " of '" + part.definition.name +
                        "' has duration " + std::to_string(total.to_float()) +
                        ", expected " + std::to_string(expected.to_float()),
                        ScoreError::MeasureFillError,
                        ScoreTime{measure.bar_number, Beat::zero()}, part.id
                    ));
                }
            }
        }
    }
}

// =============================================================================
// S3: No overlapping events within a voice
// =============================================================================

void validate_s3(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (std::size_t i = 1; i < voice.events.size(); ++i) {
                    const auto& prev = voice.events[i - 1];
                    const auto& curr = voice.events[i];

                    Beat prev_end = prev.offset + prev.duration();
                    if (prev_end > curr.offset) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error, "S3",
                            "Overlapping events in voice " +
                            std::to_string(voice.voice_index) +
                            ", bar " + std::to_string(measure.bar_number),
                            ScoreError::OverlappingEvents,
                            ScoreTime{measure.bar_number, curr.offset}, part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S4: TempoMap has entry at bar 1
// =============================================================================

void validate_s4(const Score& score, std::vector<Diagnostic>& out) {
    if (score.tempo_map.empty() || score.tempo_map[0].position.bar != 1) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error, "S4",
            "TempoMap must have an entry at bar 1",
            ScoreError::TempoMapGap
        ));
    }
    for (std::size_t i = 0; i < score.tempo_map.size(); ++i) {
        if (score.tempo_map[i].position.bar < 1) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S4",
                "TempoMap entry has bar < 1 at index " + std::to_string(i),
                ScoreError::TempoMapGap,
                score.tempo_map[i].position
            ));
        }
        if (i > 0) {
            const auto& prev = score.tempo_map[i - 1].position;
            const auto& curr = score.tempo_map[i].position;
            if (curr.bar < prev.bar ||
                (curr.bar == prev.bar && curr.beat <= prev.beat)) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error, "S4",
                    "TempoMap entries not in ascending order at index "
                        + std::to_string(i),
                    ScoreError::TempoMapGap,
                    curr
                ));
            }
        }
    }
}

// =============================================================================
// S5: TimeSignatureMap has entry at bar 1
// =============================================================================

void validate_s5(const Score& score, std::vector<Diagnostic>& out) {
    if (score.time_map.empty() || score.time_map[0].bar != 1) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error, "S5",
            "TimeSignatureMap must have an entry at bar 1",
            ScoreError::TimeMapGap
        ));
    }
    for (std::size_t i = 0; i < score.time_map.size(); ++i) {
        if (score.time_map[i].bar < 1) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S5",
                "TimeSignatureMap entry has bar < 1 at index " + std::to_string(i),
                ScoreError::TimeMapGap,
                ScoreTime{score.time_map[i].bar, Beat::zero()}
            ));
        }
        if (i > 0 && score.time_map[i].bar <= score.time_map[i - 1].bar) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S5",
                "TimeSignatureMap entries not in ascending order at index "
                    + std::to_string(i),
                ScoreError::TimeMapGap,
                ScoreTime{score.time_map[i].bar, Beat::zero()}
            ));
        }
    }
}

// =============================================================================
// S6: KeySignatureMap has entry at bar 1
// =============================================================================

void validate_s6(const Score& score, std::vector<Diagnostic>& out) {
    if (score.key_map.empty() || score.key_map[0].position.bar != 1) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error, "S6",
            "KeySignatureMap must have an entry at bar 1",
            ScoreError::KeyMapGap
        ));
    }
    for (std::size_t i = 0; i < score.key_map.size(); ++i) {
        if (score.key_map[i].position.bar < 1) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S6",
                "KeySignatureMap entry has bar < 1 at index " + std::to_string(i),
                ScoreError::KeyMapGap,
                score.key_map[i].position
            ));
        }
        if (i > 0) {
            const auto& prev = score.key_map[i - 1].position;
            const auto& curr = score.key_map[i].position;
            if (curr.bar < prev.bar ||
                (curr.bar == prev.bar && curr.beat <= prev.beat)) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error, "S6",
                    "KeySignatureMap entries not in ascending order at index "
                        + std::to_string(i),
                    ScoreError::KeyMapGap,
                    curr
                ));
            }
        }
    }
}

// =============================================================================
// S7: Tied notes have matching pitch at adjacent positions
// =============================================================================

void validate_s7(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (std::size_t mi = 0; mi < part.measures.size(); ++mi) {
            const auto& measure = part.measures[mi];
            for (const auto& voice : measure.voices) {
                for (std::size_t ei = 0; ei < voice.events.size(); ++ei) {
                    const auto* ng = voice.events[ei].as_note_group();
                    if (!ng) continue;

                    for (std::size_t ni = 0; ni < ng->notes.size(); ++ni) {
                        if (!ng->notes[ni].tie_forward) continue;

                        // Find next note group in this voice
                        bool found = false;
                        if (ei + 1 < voice.events.size()) {
                            const auto* next_ng =
                                voice.events[ei + 1].as_note_group();
                            if (next_ng) {
                                for (const auto& note : next_ng->notes) {
                                    if (note.pitch == ng->notes[ni].pitch) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                        } else if (mi + 1 < part.measures.size()) {
                            // Check first event of next measure, same voice
                            for (const auto& v : part.measures[mi + 1].voices) {
                                if (v.voice_index == voice.voice_index &&
                                    !v.events.empty()) {
                                    const auto* next_ng =
                                        v.events[0].as_note_group();
                                    if (next_ng) {
                                        for (const auto& note : next_ng->notes) {
                                            if (note.pitch ==
                                                ng->notes[ni].pitch) {
                                                found = true;
                                                break;
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }

                        if (!found) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "S7",
                                "Tie forward on note with no matching pitch "
                                "at adjacent position",
                                ScoreError::TieMismatch,
                                ScoreTime{measure.bar_number,
                                    voice.events[ei].offset},
                                part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// S8: Tuplet events sum to tuplet's total span
// =============================================================================

void validate_s8(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                // Group events by tuplet id
                std::map<std::uint64_t, std::vector<const Event*>> tuplet_events;
                std::map<std::uint64_t, TupletContext> tuplet_ctx;

                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (ng && ng->tuplet_context) {
                        auto tid = ng->tuplet_context->id.value;
                        tuplet_events[tid].push_back(&event);
                        tuplet_ctx[tid] = *ng->tuplet_context;
                    }
                }

                for (const auto& [tid, events] : tuplet_events) {
                    Beat sum = Beat::zero();
                    for (const auto* ev : events) {
                        sum = sum + ev->duration();
                    }
                    const auto& ctx = tuplet_ctx[tid];
                    // Expected span = normal * normal_type
                    Beat expected = ctx.normal_type *
                        static_cast<std::int64_t>(ctx.normal);
                    if (!(sum == expected)) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error, "S8",
                            "Tuplet events do not sum to declared span",
                            ScoreError::TupletSpanError,
                            ScoreTime{measure.bar_number, Beat::zero()},
                            part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S9: Section spans at same nesting level do not overlap
// =============================================================================

void validate_sections_no_overlap(
    const std::vector<ScoreSection>& sections,
    std::vector<Diagnostic>& out
) {
    for (std::size_t i = 1; i < sections.size(); ++i) {
        if (sections[i].start < sections[i - 1].end) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S9",
                "Section '" + sections[i].label + "' overlaps with '" +
                sections[i - 1].label + "'",
                ScoreError::DocumentStructure,
                sections[i].start
            ));
        }
    }
    // Recurse into children
    for (const auto& section : sections) {
        validate_sections_no_overlap(section.children, out);
    }
}

void validate_s9(const Score& score, std::vector<Diagnostic>& out) {
    validate_sections_no_overlap(score.section_map, out);
}

// =============================================================================
// S10: Section hierarchy is properly nested
// =============================================================================

void validate_section_nesting(
    const std::vector<ScoreSection>& sections,
    std::vector<Diagnostic>& out
) {
    for (const auto& section : sections) {
        for (const auto& child : section.children) {
            if (child.start < section.start || section.end < child.end) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error, "S10",
                    "Child section '" + child.label +
                    "' extends beyond parent '" + section.label + "'",
                    ScoreError::DocumentStructure,
                    child.start
                ));
            }
        }
        validate_section_nesting(section.children, out);
    }
}

void validate_s10(const Score& score, std::vector<Diagnostic>& out) {
    validate_section_nesting(score.section_map, out);
}

// =============================================================================
// S0b: Voices non-empty in every measure (relabelled from S11)
// =============================================================================

void validate_s0b(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (measure.voices.empty()) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error, "S0b",
                    "Measure " + std::to_string(measure.bar_number) +
                    " has no voices",
                    ScoreError::EmptyVoice,
                    ScoreTime{measure.bar_number, Beat::zero()}, part.id
                ));
            }
        }
    }
}

// =============================================================================
// S11: Tone row completeness — every NoteGroup uses all 12 pitch classes
// =============================================================================

void validate_s11(const Score& score, std::vector<Diagnostic>& out) {
    if (!score.tone_row) return;

    // Collect all pitch classes used across note groups in the score
    std::set<PitchClass> used_pcs;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        used_pcs.insert(pc(note.pitch));
                    }
                }
            }
        }
    }

    if (used_pcs.size() < 12) {
        std::string missing;
        for (int pc = 0; pc < 12; ++pc) {
            if (!used_pcs.contains(static_cast<PitchClass>(pc))) {
                if (!missing.empty()) missing += ", ";
                missing += std::to_string(pc);
            }
        }
        out.push_back(make_diagnostic(
            ValidationSeverity::Error, "S11",
            "Tone row set: score is missing pitch classes: " + missing,
            ScoreError::InvariantViolation
        ));
    }
}

// =============================================================================
// M1: Note outside comfortable range (Warning)
// =============================================================================

void validate_m1(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& range = part.definition.range;
        int comf_lo = midi_value(range.comfortable_low);
        int comf_hi = midi_value(range.comfortable_high);
        int abs_lo = midi_value(range.absolute_low);
        int abs_hi = midi_value(range.absolute_high);

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        if (mv < comf_lo || mv > comf_hi) {
                            // Check if still within absolute range
                            if (mv >= abs_lo && mv <= abs_hi) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Warning, "M1",
                                    "Note outside comfortable range for '" +
                                    part.definition.name + "'",
                                    ScoreError::InconsistentOrch,
                                    ScoreTime{measure.bar_number, event.offset},
                                    part.id
                                ));
                            }
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// M2: Note outside absolute range (Error)
// =============================================================================

void validate_m2(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& range = part.definition.range;
        int abs_lo = midi_value(range.absolute_low);
        int abs_hi = midi_value(range.absolute_high);

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        if (mv < abs_lo || mv > abs_hi) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "M2",
                                "Note outside absolute range for '" +
                                part.definition.name + "'",
                                ScoreError::InconsistentOrch,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// M8: Harmonic annotation layer is stale (Warning)
// =============================================================================

void validate_m8(const Score& score, std::vector<Diagnostic>& out) {
    // Stale if the score has notes but no harmonic annotations
    bool has_notes = false;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (event.is_note_group()) { has_notes = true; break; }
                }
                if (has_notes) break;
            }
            if (has_notes) break;
        }
        if (has_notes) break;
    }

    if (has_notes && score.harmonic_annotations.empty()) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Warning, "M8",
            "Score has note content but no harmonic annotations",
            ScoreError::StaleHarmonicLayer
        ));
    }
}

// =============================================================================
// R1: Part has no instrument preset (Warning)
// =============================================================================

void validate_r1(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        if (!part.definition.rendering.instrument_preset) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Warning, "R1",
                "Part '" + part.definition.name +
                "' has no instrument preset configured",
                ScoreError::MissingPreset,
                std::nullopt, part.id
            ));
        }
    }
}

// =============================================================================
// R2: Articulation used but not in vocabulary (Warning)
// =============================================================================

void validate_r2(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& vocab = part.definition.articulation_vocabulary;
        if (vocab.empty()) continue;  // No vocabulary = all allowed

        std::set<ArticulationType> allowed(vocab.begin(), vocab.end());

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.articulation && !allowed.contains(*note.articulation)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Warning, "R2",
                                "Articulation not in vocabulary for '" +
                                part.definition.name + "'",
                                ScoreError::UnmappedArticulation,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// R3: Articulation used but no mapping defined (Warning)
// =============================================================================

void validate_r3(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& amap = part.definition.rendering.articulation_map;
        if (amap.empty()) continue;

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.articulation && !amap.contains(*note.articulation)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Warning, "R3",
                                "No articulation mapping for '" +
                                part.definition.name + "'",
                                ScoreError::UnmappedArticulation,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// M3: Parallel fifths/octaves (Warning)
// =============================================================================

void validate_m3(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                // Collect consecutive NoteGroups in this voice
                std::vector<std::pair<const NoteGroup*, Beat>> note_groups;
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (ng) note_groups.push_back({ng, event.offset});
                }

                for (std::size_t idx = 1; idx < note_groups.size(); ++idx) {
                    const auto& [prev_ng, prev_off] = note_groups[idx - 1];
                    const auto& [curr_ng, curr_off] = note_groups[idx];

                    // Check each pair of voice indices (i, j) within the NoteGroups
                    std::size_t min_notes = std::min(
                        prev_ng->notes.size(), curr_ng->notes.size());

                    for (std::size_t i = 0; i < min_notes; ++i) {
                        for (std::size_t j = i + 1; j < min_notes; ++j) {
                            MidiNote prev_lo = static_cast<MidiNote>(
                                midi_value(prev_ng->notes[i].pitch));
                            MidiNote prev_hi = static_cast<MidiNote>(
                                midi_value(prev_ng->notes[j].pitch));
                            MidiNote curr_lo = static_cast<MidiNote>(
                                midi_value(curr_ng->notes[i].pitch));
                            MidiNote curr_hi = static_cast<MidiNote>(
                                midi_value(curr_ng->notes[j].pitch));

                            // Check parallel fifths (interval class 7)
                            if (has_parallel_motion(
                                    prev_lo, prev_hi, curr_lo, curr_hi, 7)) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Warning, "M3",
                                    "Parallel fifth detected in voice " +
                                    std::to_string(voice.voice_index) +
                                    ", bar " + std::to_string(measure.bar_number),
                                    ScoreError::InvariantViolation,
                                    ScoreTime{measure.bar_number, curr_off},
                                    part.id
                                ));
                            }

                            // Check parallel octaves (interval class 0)
                            if (has_parallel_motion(
                                    prev_lo, prev_hi, curr_lo, curr_hi, 0)) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Warning, "M3",
                                    "Parallel octave detected in voice " +
                                    std::to_string(voice.voice_index) +
                                    ", bar " + std::to_string(measure.bar_number),
                                    ScoreError::InvariantViolation,
                                    ScoreTime{measure.bar_number, curr_off},
                                    part.id
                                ));
                            }
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// M4: Voice crossing (Warning)
// =============================================================================

void validate_m4(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            // Voices are ordered by voice_index; consecutive indices should
            // maintain pitch ordering at each event offset.
            if (measure.voices.size() < 2) continue;

            // Build per-voice map: offset -> (min_midi, max_midi) for NoteGroups
            struct VoiceRange {
                int lo;
                int hi;
            };
            using OffsetMap = std::map<double, VoiceRange>;

            std::vector<OffsetMap> voice_maps(measure.voices.size());
            for (std::size_t vi = 0; vi < measure.voices.size(); ++vi) {
                for (const auto& event : measure.voices[vi].events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    int lo = 127, hi = 0;
                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        if (mv < lo) lo = mv;
                        if (mv > hi) hi = mv;
                    }
                    double off_key = event.offset.to_float();
                    voice_maps[vi][off_key] = {lo, hi};
                }
            }

            // For consecutive voice pairs, check that voice N's highest midi
            // stays below voice N+1's lowest midi at shared offsets
            for (std::size_t vi = 0; vi + 1 < measure.voices.size(); ++vi) {
                for (const auto& [off_key, range_n] : voice_maps[vi]) {
                    auto it = voice_maps[vi + 1].find(off_key);
                    if (it == voice_maps[vi + 1].end()) continue;
                    const auto& range_n1 = it->second;

                    if (range_n.hi >= range_n1.lo) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Warning, "M4",
                            "Voice crossing between voice " +
                            std::to_string(measure.voices[vi].voice_index) +
                            " and voice " +
                            std::to_string(measure.voices[vi + 1].voice_index) +
                            " in bar " + std::to_string(measure.bar_number),
                            ScoreError::InvariantViolation,
                            ScoreTime{measure.bar_number, Beat::zero()},
                            part.id
                        ));
                        break;  // One diagnostic per voice pair per measure
                    }
                }
            }
        }
    }
}

// =============================================================================
// M5: Large leaps > octave without recovery (Warning)
// =============================================================================

void validate_m5(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                // Collect consecutive NoteGroups
                struct NgInfo {
                    int midi;
                    Beat offset;
                };
                std::vector<NgInfo> pitches;
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng || ng->notes.empty()) continue;
                    // Use the first note as the representative pitch
                    pitches.push_back({
                        midi_value(ng->notes[0].pitch),
                        event.offset
                    });
                }

                for (std::size_t i = 1; i < pitches.size(); ++i) {
                    int leap = pitches[i].midi - pitches[i - 1].midi;
                    if (std::abs(leap) > 12) {
                        // Check if next interval recovers (opposite direction, step)
                        bool recovered = false;
                        if (i + 1 < pitches.size()) {
                            int recovery = pitches[i + 1].midi - pitches[i].midi;
                            // Opposite direction and step (<=2 semitones)
                            if (leap > 0 && recovery < 0 &&
                                std::abs(recovery) <= 2) {
                                recovered = true;
                            } else if (leap < 0 && recovery > 0 &&
                                       std::abs(recovery) <= 2) {
                                recovered = true;
                            }
                        }

                        if (!recovered) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Warning, "M5",
                                "Large leap (" + std::to_string(std::abs(leap)) +
                                " semitones) without step recovery in voice " +
                                std::to_string(voice.voice_index) +
                                ", bar " + std::to_string(measure.bar_number),
                                ScoreError::InvariantViolation,
                                ScoreTime{measure.bar_number, pitches[i].offset},
                                part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// M6: Unresolved leading tone (Info)
// =============================================================================

void validate_m6(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            // Find the applicable key for this measure
            const KeySignatureEntry* key_entry = nullptr;
            for (const auto& ke : score.key_map) {
                if (ke.position.bar <= measure.bar_number) key_entry = &ke;
                else break;
            }
            if (!key_entry) continue;

            int key_root_pc = pc(key_entry->key.root);
            int leading_tone_pc = (key_root_pc + 11) % 12;

            for (const auto& voice : measure.voices) {
                for (std::size_t ei = 0; ei < voice.events.size(); ++ei) {
                    const auto* ng = voice.events[ei].as_note_group();
                    if (!ng) continue;

                    for (const auto& note : ng->notes) {
                        int note_pc = pc(note.pitch);
                        if (note_pc != leading_tone_pc) continue;

                        int note_midi = midi_value(note.pitch);

                        // Find the next note in this voice
                        const NoteGroup* next_ng = nullptr;
                        if (ei + 1 < voice.events.size()) {
                            next_ng = voice.events[ei + 1].as_note_group();
                        }

                        if (next_ng && !next_ng->notes.empty()) {
                            int next_midi = midi_value(next_ng->notes[0].pitch);
                            int interval = next_midi - note_midi;
                            // Should resolve upward by 1 or 2 semitones to tonic
                            int next_pc = pc(next_ng->notes[0].pitch);
                            if (interval > 0 && interval <= 2 &&
                                next_pc == key_root_pc) {
                                continue;  // Properly resolved
                            }
                        }

                        out.push_back(make_diagnostic(
                            ValidationSeverity::Info, "M6",
                            "Unresolved leading tone in voice " +
                            std::to_string(voice.voice_index) +
                            ", bar " + std::to_string(measure.bar_number),
                            ScoreError::InvariantViolation,
                            ScoreTime{measure.bar_number,
                                voice.events[ei].offset},
                            part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// M7: Unresolved chordal seventh (Info)
// =============================================================================

void validate_m7(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (std::size_t ei = 0; ei < voice.events.size(); ++ei) {
                    const auto* ng = voice.events[ei].as_note_group();
                    if (!ng) continue;

                    // Find the applicable harmonic annotation at this position
                    ScoreTime event_time{measure.bar_number,
                        voice.events[ei].offset};
                    const HarmonicAnnotation* ha = nullptr;
                    for (const auto& ann : score.harmonic_annotations) {
                        if (ann.position <= event_time) {
                            // Check if event_time is within the annotation span
                            ha = &ann;
                        } else {
                            break;
                        }
                    }
                    if (!ha || ha->chord.empty()) continue;

                    PitchClass chord_root = ha->chord.root;

                    for (const auto& note : ng->notes) {
                        int note_pc = pc(note.pitch);

                        // Check if this note forms a seventh above the chord root
                        // (10 or 11 semitones above root = minor 7th or major 7th)
                        int interval_from_root = (note_pc - chord_root + 12) % 12;
                        if (interval_from_root != 10 && interval_from_root != 11)
                            continue;

                        int note_midi = midi_value(note.pitch);

                        // Find next note in this voice
                        const NoteGroup* next_ng = nullptr;
                        if (ei + 1 < voice.events.size()) {
                            next_ng = voice.events[ei + 1].as_note_group();
                        }

                        if (next_ng && !next_ng->notes.empty()) {
                            int next_midi = midi_value(next_ng->notes[0].pitch);
                            int motion = next_midi - note_midi;
                            // Should resolve downward by step (1 or 2 semitones)
                            if (motion < 0 && std::abs(motion) <= 2) {
                                continue;  // Properly resolved
                            }
                        }

                        out.push_back(make_diagnostic(
                            ValidationSeverity::Info, "M7",
                            "Unresolved chordal seventh in voice " +
                            std::to_string(voice.voice_index) +
                            ", bar " + std::to_string(measure.bar_number),
                            ScoreError::InvariantViolation,
                            ScoreTime{measure.bar_number,
                                voice.events[ei].offset},
                            part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// M9: Missing orchestration annotation (Info)
// =============================================================================

void validate_m9(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        // Check if this part has any orchestration annotations at all
        bool has_any_orch = false;
        for (const auto& ann : score.orchestration_annotations) {
            if (ann.part_id == part.id) {
                has_any_orch = true;
                break;
            }
        }
        if (!has_any_orch) continue;

        // Build a set of bars covered by orchestration annotations
        std::set<std::uint32_t> covered_bars;
        for (const auto& ann : score.orchestration_annotations) {
            if (ann.part_id != part.id) continue;
            for (std::uint32_t b = ann.start.bar; b < ann.end.bar; ++b) {
                covered_bars.insert(b);
            }
            // Include end bar if beat > 0
            if (ann.end.beat > Beat::zero()) {
                covered_bars.insert(ann.end.bar);
            }
        }

        // Scan for gaps of > 8 consecutive bars
        std::uint32_t gap_start = 0;
        std::uint32_t gap_count = 0;
        for (std::uint32_t b = 1; b <= score.metadata.total_bars; ++b) {
            if (!covered_bars.contains(b)) {
                if (gap_count == 0) gap_start = b;
                ++gap_count;
            } else {
                if (gap_count > 8) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Info, "M9",
                        "Part '" + part.definition.name +
                        "' has " + std::to_string(gap_count) +
                        " consecutive bars without orchestration annotation"
                        " starting at bar " + std::to_string(gap_start),
                        ScoreError::InconsistentOrch,
                        ScoreTime{gap_start, Beat::zero()}, part.id
                    ));
                }
                gap_count = 0;
            }
        }
        // Check trailing gap
        if (gap_count > 8) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Info, "M9",
                "Part '" + part.definition.name +
                "' has " + std::to_string(gap_count) +
                " consecutive bars without orchestration annotation"
                " starting at bar " + std::to_string(gap_start),
                ScoreError::InconsistentOrch,
                ScoreTime{gap_start, Beat::zero()}, part.id
            ));
        }
    }
}

// =============================================================================
// M10: Dynamic absent > 16 bars (Warning)
// =============================================================================

void validate_m10(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        // Collect bars that have a dynamic marking (note.dynamic) or hairpin
        std::set<std::uint32_t> dynamic_bars;

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.dynamic) {
                            dynamic_bars.insert(measure.bar_number);
                        }
                    }
                }
            }
        }

        // Also include bars covered by hairpins
        for (const auto& hairpin : part.hairpins) {
            for (std::uint32_t b = hairpin.start.bar; b <= hairpin.end.bar; ++b) {
                dynamic_bars.insert(b);
            }
        }

        // Scan for gaps > 16 consecutive bars without dynamics
        std::uint32_t gap_start = 0;
        std::uint32_t gap_count = 0;
        for (std::uint32_t b = 1; b <= score.metadata.total_bars; ++b) {
            if (!dynamic_bars.contains(b)) {
                if (gap_count == 0) gap_start = b;
                ++gap_count;
            } else {
                if (gap_count > 16) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Warning, "M10",
                        "Part '" + part.definition.name +
                        "' has " + std::to_string(gap_count) +
                        " consecutive bars without dynamic marking"
                        " starting at bar " + std::to_string(gap_start),
                        ScoreError::InconsistentOrch,
                        ScoreTime{gap_start, Beat::zero()}, part.id
                    ));
                }
                gap_count = 0;
            }
        }
        // Check trailing gap
        if (gap_count > 16) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Info, "M10",
                "Part '" + part.definition.name +
                "' has " + std::to_string(gap_count) +
                " consecutive bars without dynamic marking"
                " starting at bar " + std::to_string(gap_start),
                ScoreError::InconsistentOrch,
                ScoreTime{gap_start, Beat::zero()}, part.id
            ));
        }
    }
}

// =============================================================================
// R4: Grace note duration (Info)
// =============================================================================

void validate_r4(const Score& score, std::vector<Diagnostic>& out) {
    Beat max_grace_dur{1, 16};
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    bool has_grace = false;
                    for (const auto& note : ng->notes) {
                        if (note.grace) {
                            has_grace = true;
                            break;
                        }
                    }

                    if (has_grace && ng->duration > max_grace_dur) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Info, "R4",
                            "Grace note group duration exceeds 1/16 in bar " +
                            std::to_string(measure.bar_number),
                            ScoreError::InvariantViolation,
                            ScoreTime{measure.bar_number, event.offset},
                            part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// R5: Tick rounding error (Warning)
// =============================================================================

void validate_r5(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (!event.is_note_group() && !event.is_rest()) continue;

                    ScoreTime st{measure.bar_number, event.offset};
                    auto abs_result = score_time_to_absolute_beat(
                        st, score.time_map);
                    if (!abs_result) continue;

                    Beat absolute = *abs_result;
                    std::int64_t tick = absolute_beat_to_tick(absolute);
                    Beat round_trip = tick_to_absolute_beat(tick);
                    std::int64_t tick2 = absolute_beat_to_tick(round_trip);

                    if (tick != tick2) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Warning, "R5",
                            "Tick rounding residual in bar " +
                            std::to_string(measure.bar_number) +
                            " at offset " +
                            std::to_string(event.offset.to_float()),
                            ScoreError::TickConversionError,
                            st, part.id
                        ));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S12: Beat denominator > 0 for all Beat fields in document
// =============================================================================

void validate_s12(const Score& score, std::vector<Diagnostic>& out) {
    // Check tempo map
    for (const auto& te : score.tempo_map) {
        if (te.position.beat.denominator <= 0) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S12",
                "Beat denominator <= 0 in tempo map position",
                ScoreError::InvariantViolation,
                te.position
            ));
        }
        if (te.linear_duration.denominator <= 0) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S12",
                "Beat denominator <= 0 in tempo linear_duration",
                ScoreError::InvariantViolation,
                te.position
            ));
        }
    }

    // Check parts, measures, events
    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                for (const auto& e : v.events) {
                    if (e.offset.denominator <= 0) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error, "S12",
                            "Event offset has denominator <= 0",
                            ScoreError::InvariantViolation,
                            ScoreTime{m.bar_number, Beat::zero()}, part.id
                        ));
                    }
                    // Check duration fields in NoteGroup and RestEvent
                    if (const auto* ng = e.as_note_group()) {
                        if (ng->duration.denominator <= 0) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "S12",
                                "NoteGroup duration has denominator <= 0",
                                ScoreError::InvariantViolation,
                                ScoreTime{m.bar_number, e.offset}, part.id
                            ));
                        }
                    }
                    if (const auto* r = e.as_rest()) {
                        if (r->duration.denominator <= 0) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "S12",
                                "Rest duration has denominator <= 0",
                                ScoreError::InvariantViolation,
                                ScoreTime{m.bar_number, e.offset}, part.id
                            ));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// S13: EventId uniqueness
// =============================================================================

void validate_s13_sections(const std::vector<ScoreSection>& sections,
                           std::set<std::uint64_t>& seen,
                           std::vector<Diagnostic>& out) {
    for (const auto& s : sections) {
        if (!seen.insert(s.id.value).second) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S13",
                "Duplicate SectionId: " + std::to_string(s.id.value),
                ScoreError::InvariantViolation,
                s.start
            ));
        }
        validate_s13_sections(s.children, seen, out);
    }
}

void validate_s13(const Score& score, std::vector<Diagnostic>& out) {
    // EventId uniqueness
    std::set<std::uint64_t> seen_event_ids;
    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                for (const auto& e : v.events) {
                    if (!seen_event_ids.insert(e.id.value).second) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error, "S13",
                            "Duplicate EventId: " + std::to_string(e.id.value),
                            ScoreError::InvariantViolation,
                            ScoreTime{m.bar_number, e.offset}, part.id
                        ));
                    }
                }
            }
        }
    }

    // PartId uniqueness
    std::set<std::uint64_t> seen_part_ids;
    for (const auto& part : score.parts) {
        if (!seen_part_ids.insert(part.id.value).second) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error, "S13",
                "Duplicate PartId: " + std::to_string(part.id.value),
                ScoreError::InvariantViolation
            ));
        }
    }

    // SectionId uniqueness (recursive walk)
    std::set<std::uint64_t> seen_section_ids;
    validate_s13_sections(score.section_map, seen_section_ids, out);
}

// =============================================================================
// S14: BeamGroup/Tuplet referential integrity
// =============================================================================

void validate_s14(const Score& score, std::vector<Diagnostic>& out) {
    // Build event-id → note-count map for NonChordTone referential integrity
    std::map<std::uint64_t, std::size_t> event_note_count;
    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                for (const auto& e : v.events) {
                    const auto* ng = e.as_note_group();
                    event_note_count[e.id.value] = ng ? ng->notes.size() : 0;
                }
            }
        }
    }

    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                // Collect all event IDs in this voice
                std::set<std::uint64_t> voice_event_ids;
                for (const auto& e : v.events) {
                    voice_event_ids.insert(e.id.value);
                }

                // Check beam groups reference valid events
                for (const auto& bg : v.beam_groups) {
                    for (const auto& eid : bg.event_ids) {
                        if (!voice_event_ids.contains(eid.value)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "S14",
                                "BeamGroup references non-existent EventId: " +
                                    std::to_string(eid.value),
                                ScoreError::InvariantViolation,
                                ScoreTime{m.bar_number, Beat::zero()}, part.id
                            ));
                        }
                    }
                }

                // Check tuplet context references: if nested_in is set, verify
                // that some event in this voice shares that parent tuplet id
                std::set<std::uint64_t> tuplet_ids;
                for (const auto& e : v.events) {
                    const auto* ng = e.as_note_group();
                    if (ng && ng->tuplet_context) {
                        tuplet_ids.insert(ng->tuplet_context->id.value);
                    }
                }
                for (const auto& e : v.events) {
                    const auto* ng = e.as_note_group();
                    if (ng && ng->tuplet_context && ng->tuplet_context->nested_in) {
                        if (!tuplet_ids.contains(ng->tuplet_context->nested_in->value)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error, "S14",
                                "Tuplet nested_in references unknown TupletId: " +
                                    std::to_string(ng->tuplet_context->nested_in->value),
                                ScoreError::InvariantViolation,
                                ScoreTime{m.bar_number, e.offset}, part.id
                            ));
                        }
                    }
                }
            }
        }
    }

    // NonChordTone referential integrity: event_id must exist, note_index in bounds
    for (const auto& ha : score.harmonic_annotations) {
        for (const auto& nct : ha.non_chord_tones) {
            auto it = event_note_count.find(nct.event_id.value);
            if (it == event_note_count.end()) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Warning, "S14",
                    "NonChordTone references non-existent EventId: " +
                        std::to_string(nct.event_id.value),
                    ScoreError::InvariantViolation,
                    ha.position
                ));
            } else if (nct.note_index >= it->second) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Warning, "S14",
                    "NonChordTone note_index " + std::to_string(nct.note_index) +
                        " out of bounds for EventId " + std::to_string(nct.event_id.value),
                    ScoreError::InvariantViolation,
                    ha.position
                ));
            }
        }
    }
}

// =============================================================================
// Sorting helper
// =============================================================================

void sort_diagnostics(std::vector<Diagnostic>& diags) {
    std::sort(diags.begin(), diags.end(),
        [](const Diagnostic& a, const Diagnostic& b) {
            // Error < Warning < Info
            if (a.severity != b.severity) return a.severity < b.severity;
            // Then by location
            if (a.location && b.location) return *a.location < *b.location;
            return a.location.has_value() && !b.location.has_value();
        }
    );
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::vector<Diagnostic> validate_structural(const Score& score) {
    std::vector<Diagnostic> diags;

    if (score.parts.empty()) {
        diags.push_back(make_diagnostic(
            ValidationSeverity::Error, "S0",
            "Score must have at least one part",
            ScoreError::MissingParts
        ));
        return diags;
    }

    validate_s0b(score, diags);
    validate_s1(score, diags);
    validate_s2(score, diags);
    validate_s3(score, diags);
    validate_s4(score, diags);
    validate_s5(score, diags);
    validate_s6(score, diags);
    validate_s7(score, diags);
    validate_s8(score, diags);
    validate_s9(score, diags);
    validate_s10(score, diags);
    validate_s11(score, diags);
    validate_s12(score, diags);
    validate_s13(score, diags);
    validate_s14(score, diags);

    sort_diagnostics(diags);
    return diags;
}

std::vector<Diagnostic> validate_musical(const Score& score) {
    std::vector<Diagnostic> diags;

    validate_m1(score, diags);
    validate_m2(score, diags);
    validate_m3(score, diags);
    validate_m4(score, diags);
    validate_m5(score, diags);
    validate_m6(score, diags);
    validate_m7(score, diags);
    validate_m8(score, diags);
    validate_m9(score, diags);
    validate_m10(score, diags);

    sort_diagnostics(diags);
    return diags;
}

std::vector<Diagnostic> validate_rendering(const Score& score) {
    std::vector<Diagnostic> diags;

    validate_r1(score, diags);
    validate_r2(score, diags);
    validate_r3(score, diags);
    validate_r4(score, diags);
    validate_r5(score, diags);

    sort_diagnostics(diags);
    return diags;
}

std::vector<Diagnostic> validate_score(const Score& score) {
    auto structural = validate_structural(score);
    auto musical = validate_musical(score);
    auto rendering = validate_rendering(score);

    structural.insert(structural.end(), musical.begin(), musical.end());
    structural.insert(structural.end(), rendering.begin(), rendering.end());

    sort_diagnostics(structural);
    return structural;
}

bool is_compilable(const Score& score) {
    auto diags = validate_structural(score);
    return diags.empty();
}

}  // namespace Sunny::Core
