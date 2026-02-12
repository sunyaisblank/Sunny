/**
 * @file SIVD001A.cpp
 * @brief Score IR validation — implementation
 *
 * Component: SIVD001A
 * Domain: SI (Score IR) | Category: VD (Validation)
 */

#include "SIVD001A.h"
#include "SITM001A.h"

#include <algorithm>
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
// S11: Voices non-empty in every measure
// =============================================================================

void validate_s11(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (measure.voices.empty()) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error, "S11",
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

    sort_diagnostics(diags);
    return diags;
}

std::vector<Diagnostic> validate_musical(const Score& score) {
    std::vector<Diagnostic> diags;

    validate_m1(score, diags);
    validate_m2(score, diags);
    validate_m8(score, diags);

    sort_diagnostics(diags);
    return diags;
}

std::vector<Diagnostic> validate_rendering(const Score& score) {
    std::vector<Diagnostic> diags;

    validate_r1(score, diags);
    validate_r2(score, diags);
    validate_r3(score, diags);

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
