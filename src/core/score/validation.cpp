/**
 * @file validation.cpp
 * @brief Score IR validation — implementation
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/post_tonal/twelve_tone.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/tuplets.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>

namespace sunny::core {

namespace {

constexpr std::size_t MAX_ARTICULATION_MAPPING_DEPTH = 16;
constexpr std::size_t MAX_ARTICULATION_MAPPING_NODES = 1024;

bool valid_articulation_mapping(const ArticulationMapping& mapping,
                                std::size_t depth,
                                std::size_t& nodes) {
    if (depth > MAX_ARTICULATION_MAPPING_DEPTH || ++nodes > MAX_ARTICULATION_MAPPING_NODES)
        return false;

    using Type = ArticulationMapping::Type;
    const auto encoded = static_cast<std::uint8_t>(mapping.type);
    if (encoded > static_cast<std::uint8_t>(Type::Combined)) return false;

    if (mapping.type != Type::Combined && !mapping.combined.empty()) return false;

    switch (mapping.type) {
    case Type::Keyswitch:
        return is_valid_midi_note(midi_value(mapping.keyswitch_pitch));
    case Type::CC:
        return mapping.cc_number <= 127 && mapping.cc_value <= 127;
    case Type::VelocityLayer:
        return mapping.velocity_min >= 1 && mapping.velocity_min <= mapping.velocity_max &&
               mapping.velocity_max <= 127;
    case Type::NoteDurationScale:
        return std::isfinite(mapping.duration_scale) && mapping.duration_scale > 0.0F;
    case Type::ProgramChange:
        return mapping.program <= 127;
    case Type::Combined:
        if (mapping.combined.empty()) return false;
        for (const auto& child : mapping.combined)
            if (!valid_articulation_mapping(child, depth + 1, nodes)) return false;
        return true;
    }
    return false;
}

// =============================================================================
// Helper: create diagnostic
// =============================================================================

Diagnostic make_diagnostic(ValidationSeverity severity,
                           const std::string& rule,
                           const std::string& message,
                           ErrorCode error_code,
                           std::optional<ScoreTime> location = std::nullopt,
                           std::optional<PartId> part = std::nullopt) {
    return Diagnostic{severity, rule, message, location, part, error_code};
}

std::optional<Beat> global_measure_duration(const Score& score, std::uint32_t bar) {
    if (bar < 1 || bar > score.metadata.total_bars) return std::nullopt;
    const TimeSignatureEntry* active = nullptr;
    for (const auto& entry : score.time_map) {
        if (entry.bar > bar) break;
        active = &entry;
    }
    if (!active) return std::nullopt;
    auto duration = checked_measure_duration(active->time_signature);
    return duration ? std::optional<Beat>{*duration} : std::nullopt;
}

bool valid_global_point(const Score& score, ScoreTime point) {
    const auto duration = global_measure_duration(score, point.bar);
    return duration && point.beat >= Beat::zero() && point.beat < *duration;
}

bool valid_global_span_point(const Score& score, ScoreTime point, bool allow_terminal) {
    if (point.bar < 1) return false;
    if (allow_terminal && score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max() &&
        point.bar == score.metadata.total_bars + 1)
        return point.beat == Beat::zero();
    return valid_global_point(score, point);
}

std::optional<Beat>
scoped_part_measure_duration(const Score& score, const Part& part, std::uint32_t bar) {
    if (bar < 1 || bar > score.metadata.total_bars || bar > part.measures.size())
        return std::nullopt;
    const auto& measure = part.measures[bar - 1];
    if (measure.local_time) {
        auto duration = checked_measure_duration(*measure.local_time);
        return duration ? std::optional<Beat>{*duration} : std::nullopt;
    }
    return global_measure_duration(score, bar);
}

bool valid_scoped_part_point(const Score& score,
                             const Part& part,
                             ScoreTime point,
                             bool allow_terminal) {
    if (point.bar < 1) return false;
    if (allow_terminal && score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max() &&
        point.bar == score.metadata.total_bars + 1)
        return point.beat == Beat::zero();
    const auto duration = scoped_part_measure_duration(score, part, point.bar);
    return duration && point.beat >= Beat::zero() && point.beat < *duration;
}

// =============================================================================
// S1: Every part has exactly total_bars measures
// =============================================================================

void validate_s1(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        if (part.measures.size() != score.metadata.total_bars) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error,
                "S1",
                "Part '" + part.definition.name + "' has " + std::to_string(part.measures.size()) +
                    " measures, expected " + std::to_string(score.metadata.total_bars),
                ErrorCode::MeasureCountMismatch,
                std::nullopt,
                part.id));
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
                if (e.bar <= measure.bar_number)
                    ts_entry = &e;
                else
                    break;
            }
            if (!ts_entry) continue;

            auto expected_result = checked_measure_duration(
                measure.local_time ? *measure.local_time : ts_entry->time_signature);
            if (!expected_result)
                continue; // Defensive compatibility guard; construction makes this unreachable.
            const Beat expected = *expected_result;

            for (const auto& voice : measure.voices) {
                Beat cursor = Beat::zero();
                for (const auto& event : voice.events) {
                    if (event.offset < Beat::zero() || event.offset >= expected) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S2",
                                            "Event offset is outside the measure in voice " +
                                                std::to_string(voice.voice_index) + ", bar " +
                                                std::to_string(measure.bar_number),
                                            ErrorCode::MeasureFillError,
                                            ScoreTime{measure.bar_number, event.offset},
                                            part.id));
                    }

                    if (!event.is_note_group() && !event.is_rest()) continue;
                    const Beat duration = event.duration();
                    if (duration <= Beat::zero()) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S2",
                                            "Measured event has non-positive duration in voice " +
                                                std::to_string(voice.voice_index) + ", bar " +
                                                std::to_string(measure.bar_number),
                                            ErrorCode::MeasureFillError,
                                            ScoreTime{measure.bar_number, event.offset},
                                            part.id));
                        continue;
                    }
                    if (event.offset != cursor) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S2",
                                            "Measured events do not contiguously fill voice " +
                                                std::to_string(voice.voice_index) + " in bar " +
                                                std::to_string(measure.bar_number),
                                            ErrorCode::MeasureFillError,
                                            ScoreTime{measure.bar_number, event.offset},
                                            part.id));
                    }

                    auto end = checked_add(event.offset, duration);
                    if (!end) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S2",
                                            "Measured event span is arithmetically unrepresentable",
                                            ErrorCode::ArithmeticOverflow,
                                            ScoreTime{measure.bar_number, event.offset},
                                            part.id));
                        continue;
                    }
                    if (*end > expected) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S2",
                            "Measured event exceeds the measure boundary in voice " +
                                std::to_string(voice.voice_index) + ", bar " +
                                std::to_string(measure.bar_number),
                            ErrorCode::MeasureFillError,
                            ScoreTime{measure.bar_number, event.offset},
                            part.id));
                    }
                    cursor = *end;
                }
                if (cursor != expected) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Error,
                        "S2",
                        "Voice " + std::to_string(voice.voice_index) + " in bar " +
                            std::to_string(measure.bar_number) + " of '" + part.definition.name +
                            "' ends at " + std::to_string(cursor.to_float()) + ", expected " +
                            std::to_string(expected.to_float()),
                        ErrorCode::MeasureFillError,
                        ScoreTime{measure.bar_number, Beat::zero()},
                        part.id));
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
                std::optional<Beat> previous_offset;
                std::optional<Beat> previous_measured_end;
                for (const auto& event : voice.events) {
                    if (previous_offset && event.offset < *previous_offset) {
                        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                      "S3",
                                                      "Events are not ordered by offset in voice " +
                                                          std::to_string(voice.voice_index) +
                                                          ", bar " +
                                                          std::to_string(measure.bar_number),
                                                      ErrorCode::OverlappingEvents,
                                                      ScoreTime{measure.bar_number, event.offset},
                                                      part.id));
                    }
                    previous_offset = event.offset;

                    if (!event.is_note_group() && !event.is_rest()) continue;
                    const Beat duration = event.duration();
                    if (duration <= Beat::zero()) continue;
                    if (previous_measured_end && *previous_measured_end > event.offset) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S3",
                            "Overlapping events in voice " + std::to_string(voice.voice_index) +
                                ", bar " + std::to_string(measure.bar_number),
                            ErrorCode::OverlappingEvents,
                            ScoreTime{measure.bar_number, event.offset},
                            part.id));
                    }
                    auto end = checked_add(event.offset, duration);
                    if (end) previous_measured_end = *end;
                }
            }
        }
    }
}

// =============================================================================
// S4: TempoMap begins exactly at score origin and is strictly ordered
// =============================================================================

void validate_s4(const Score& score, std::vector<Diagnostic>& out) {
    if (score.tempo_map.empty() || score.tempo_map[0].position != SCORE_START) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S4",
                                      "TempoMap must begin exactly at ScoreTime(1, 0)",
                                      ErrorCode::TempoMapGap));
    }
    for (std::size_t i = 0; i < score.tempo_map.size(); ++i) {
        if (!valid_global_point(score, score.tempo_map[i].position)) {
            out.push_back(
                make_diagnostic(ValidationSeverity::Error,
                                "S4",
                                "TempoMap entry is outside the score or active measure at index " +
                                    std::to_string(i),
                                ErrorCode::TempoMapGap,
                                score.tempo_map[i].position));
        }
        if (i > 0) {
            const auto& prev = score.tempo_map[i - 1].position;
            const auto& curr = score.tempo_map[i].position;
            if (curr.bar < prev.bar || (curr.bar == prev.bar && curr.beat <= prev.beat)) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S4",
                                              "TempoMap entries not in ascending order at index " +
                                                  std::to_string(i),
                                              ErrorCode::TempoMapGap,
                                              curr));
            }
        }
    }
}

// =============================================================================
// S5: TimeSignatureMap has entry at bar 1
// =============================================================================

void validate_s5(const Score& score, std::vector<Diagnostic>& out) {
    if (score.time_map.empty() || score.time_map[0].bar != 1) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S5",
                                      "TimeSignatureMap must have an entry at bar 1",
                                      ErrorCode::TimeMapGap));
    }
    for (std::size_t i = 0; i < score.time_map.size(); ++i) {
        if (score.time_map[i].bar < 1 || score.time_map[i].bar > score.metadata.total_bars) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S5",
                                          "TimeSignatureMap entry is outside the score at index " +
                                              std::to_string(i),
                                          ErrorCode::TimeMapGap,
                                          ScoreTime{score.time_map[i].bar, Beat::zero()}));
        }
        if (i > 0 && score.time_map[i].bar <= score.time_map[i - 1].bar) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error,
                "S5",
                "TimeSignatureMap entries not in ascending order at index " + std::to_string(i),
                ErrorCode::TimeMapGap,
                ScoreTime{score.time_map[i].bar, Beat::zero()}));
        }
    }
}

// =============================================================================
// S6: KeySignatureMap has entry at bar 1
// =============================================================================

void validate_s6(const Score& score, std::vector<Diagnostic>& out) {
    if (score.key_map.empty() || score.key_map[0].position != SCORE_START) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S6",
                                      "KeySignatureMap must begin exactly at score origin",
                                      ErrorCode::KeyMapGap));
    }
    for (std::size_t i = 0; i < score.key_map.size(); ++i) {
        if (!valid_global_point(score, score.key_map[i].position)) {
            out.push_back(
                make_diagnostic(ValidationSeverity::Error,
                                "S6",
                                "KeySignatureMap entry is outside the score or active measure at "
                                "index " +
                                    std::to_string(i),
                                ErrorCode::KeyMapGap,
                                score.key_map[i].position));
        }
        if (i > 0) {
            const auto& prev = score.key_map[i - 1].position;
            const auto& curr = score.key_map[i].position;
            if (curr.bar < prev.bar || (curr.bar == prev.bar && curr.beat <= prev.beat)) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error,
                    "S6",
                    "KeySignatureMap entries not in ascending order at index " + std::to_string(i),
                    ErrorCode::KeyMapGap,
                    curr));
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

                        // Find the next measured event in this voice. Point
                        // directions/chord symbols do not interrupt temporal
                        // adjacency; a Rest or different-pitch NoteGroup does.
                        const Event* next_measured = nullptr;
                        for (std::size_t next_index = ei + 1; next_index < voice.events.size();
                             ++next_index) {
                            const auto& candidate = voice.events[next_index];
                            if (candidate.is_note_group() || candidate.is_rest()) {
                                next_measured = &candidate;
                                break;
                            }
                        }
                        for (std::size_t next_measure = mi + 1;
                             !next_measured && next_measure < part.measures.size();
                             ++next_measure) {
                            const Voice* continuation = nullptr;
                            for (const auto& candidate_voice : part.measures[next_measure].voices) {
                                if (candidate_voice.voice_index == voice.voice_index) {
                                    continuation = &candidate_voice;
                                    break;
                                }
                            }
                            if (!continuation) break;
                            for (const auto& candidate : continuation->events) {
                                if (candidate.is_note_group() || candidate.is_rest()) {
                                    next_measured = &candidate;
                                    break;
                                }
                            }
                        }

                        bool found = false;
                        if (next_measured) {
                            if (const auto* next_group = next_measured->as_note_group()) {
                                found = std::any_of(next_group->notes.begin(),
                                                    next_group->notes.end(),
                                                    [&](const Note& note) {
                                                        return !note.grace &&
                                                               note.pitch == ng->notes[ni].pitch;
                                                    });
                            }
                        }

                        if (!found) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error,
                                "S7",
                                "Tie forward on note with no matching pitch "
                                "at adjacent position",
                                ErrorCode::TieMismatch,
                                ScoreTime{measure.bar_number, voice.events[ei].offset},
                                part.id));
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
                TupletContextMap contexts;
                std::map<TupletId, ScoreTime> locations;
                for (const auto& event : voice.events) {
                    const auto* context = event_tuplet_context(event);
                    if (!context) continue;
                    const ScoreTime location{measure.bar_number, event.offset};
                    locations.try_emplace(context->id, location);
                    const auto [it, inserted] = contexts.try_emplace(context->id, *context);
                    if (inserted) continue;
                    const auto& canonical = it->second;
                    const bool same_definition =
                        canonical.actual == context->actual &&
                        canonical.normal == context->normal &&
                        canonical.normal_type.numerator() == context->normal_type.numerator() &&
                        canonical.normal_type.denominator() == context->normal_type.denominator() &&
                        canonical.nested_in == context->nested_in;
                    if (!same_definition) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S8",
                            "Events sharing a TupletId must share one identical context",
                            ErrorCode::TupletSpanError,
                            location,
                            part.id));
                    }
                }

                for (const auto& [tid, context] : contexts) {
                    const auto location = locations.at(tid);
                    if (context.actual == 0 || context.normal == 0 ||
                        context.normal_type <= Beat::zero()) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S8",
                            "Tuplet actual, normal, and normal_type must all be positive",
                            ErrorCode::TupletInvalidRatio,
                            location,
                            part.id));
                        continue;
                    }

                    const auto context_chain = tuplet_context_chain(&context, contexts);
                    if (!context_chain) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S8",
                            "Tuplet nesting must reference an acyclic context chain",
                            ErrorCode::TupletSpanError,
                            location,
                            part.id));
                        continue;
                    }

                    auto expected = checked_mul(context.normal_type,
                                                Beat{static_cast<std::int64_t>(context.normal), 1});
                    if (expected) {
                        // A child tuplet's structural allocation is its local normal span
                        // scaled by every enclosing (outer) tuplet ratio.
                        for (std::size_t i = 0; i + 1 < context_chain->size(); ++i) {
                            const auto* ancestor = (*context_chain)[i];
                            expected =
                                checked_mul(*expected,
                                            Beat{static_cast<std::int64_t>(ancestor->normal),
                                                 static_cast<std::int64_t>(ancestor->actual)});
                            if (!expected) break;
                        }
                    }

                    Beat sum = Beat::zero();
                    bool arithmetic_failed = !expected;
                    bool seen_member = false;
                    bool left_membership = false;
                    std::size_t direct_events = 0;
                    std::set<TupletId> direct_children;
                    for (const auto& event : voice.events) {
                        if (!event.is_note_group() && !event.is_rest()) continue;
                        const auto* leaf = event_tuplet_context(event);
                        const auto event_chain = tuplet_context_chain(leaf, contexts);
                        bool is_member = false;
                        if (event_chain) {
                            const auto member = std::find_if(event_chain->begin(),
                                                             event_chain->end(),
                                                             [tid](const TupletContext* candidate) {
                                                                 return candidate->id == tid;
                                                             });
                            is_member = member != event_chain->end();
                            if (is_member) {
                                if (left_membership) {
                                    out.push_back(make_diagnostic(
                                        ValidationSeverity::Error,
                                        "S8",
                                        "Tuplet members must form one contiguous measured span",
                                        ErrorCode::TupletSpanError,
                                        ScoreTime{measure.bar_number, event.offset},
                                        part.id));
                                }
                                seen_member = true;
                                auto next_sum = checked_add(sum, event.duration());
                                if (!next_sum)
                                    arithmetic_failed = true;
                                else
                                    sum = *next_sum;

                                const auto next = std::next(member);
                                if (next == event_chain->end())
                                    ++direct_events;
                                else
                                    direct_children.insert((*next)->id);
                            }
                        }
                        if (!is_member && seen_member) left_membership = true;
                    }

                    if (direct_events + direct_children.size() != context.actual) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S8",
                            "Tuplet direct events and child tuplets do not equal actual count",
                            ErrorCode::TupletSpanError,
                            location,
                            part.id));
                    }
                    if (arithmetic_failed || !expected || sum != *expected) {
                        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                      "S8",
                                                      "Tuplet members do not sum to the nested "
                                                      "structural span",
                                                      ErrorCode::TupletSpanError,
                                                      location,
                                                      part.id));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S9: Section spans at same nesting level do not overlap
// =============================================================================

void validate_sections_no_overlap(const std::vector<ScoreSection>& sections,
                                  std::vector<Diagnostic>& out) {
    for (std::size_t i = 1; i < sections.size(); ++i) {
        if (sections[i].start < sections[i - 1].end) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S9",
                                          "Section '" + sections[i].label + "' overlaps with '" +
                                              sections[i - 1].label + "'",
                                          ErrorCode::DocumentStructure,
                                          sections[i].start));
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

void validate_section_nesting(const std::vector<ScoreSection>& sections,
                              std::vector<Diagnostic>& out) {
    for (const auto& section : sections) {
        for (const auto& child : section.children) {
            if (child.start < section.start || section.end < child.end) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S10",
                                              "Child section '" + child.label +
                                                  "' extends beyond parent '" + section.label + "'",
                                              ErrorCode::DocumentStructure,
                                              child.start));
            }
        }
        validate_section_nesting(section.children, out);
    }
}

void validate_s10(const Score& score, std::vector<Diagnostic>& out) {
    validate_section_nesting(score.section_map, out);
}

// =============================================================================
// S27: Section payload and half-open span domains
// =============================================================================

void validate_section_domains(const Score& score,
                              const std::vector<ScoreSection>& sections,
                              std::vector<Diagnostic>& out) {
    for (const auto& section : sections) {
        const bool valid_start = valid_global_span_point(score, section.start, false);
        const bool valid_end = valid_global_span_point(score, section.end, true);
        const bool valid_function =
            !section.form_function || static_cast<std::uint8_t>(*section.form_function) <=
                                          static_cast<std::uint8_t>(FormFunction::Parenthetical);
        if (section.label.empty() || !valid_start || !valid_end ||
            (valid_start && valid_end && !(section.start < section.end)) || !valid_function) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S27",
                                          "Section requires a nonempty label, a non-empty in-score "
                                          "half-open span, and a valid form function",
                                          ErrorCode::DocumentStructure,
                                          section.start));
        }
        validate_section_domains(score, section.children, out);
    }
}

void validate_s27(const Score& score, std::vector<Diagnostic>& out) {
    validate_section_domains(score, score.section_map, out);
}

// =============================================================================
// S28: Complete sounding-pitch function
// =============================================================================

void validate_s28(const Score& score, std::vector<Diagnostic>& out) {
    if (!validate_score_tuning(score.tuning)) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error,
            "S28",
            "Score tuning requires a MIDI-domain reference note, a finite positive reference "
            "frequency, exactly one finite and renderable cent position per MIDI note, and zero "
            "cents at the reference note",
            ErrorCode::InvalidFrequency));
    }
}

// =============================================================================
// S0b: Voices non-empty in every measure (relabelled from S11)
// =============================================================================

void validate_s0b(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (measure.voices.empty()) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S0b",
                                              "Measure " + std::to_string(measure.bar_number) +
                                                  " has no voices",
                                              ErrorCode::EmptyVoice,
                                              ScoreTime{measure.bar_number, Beat::zero()},
                                              part.id));
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
            if (!used_pcs.contains(PitchClass::wrapped(pc))) {
                if (!missing.empty()) missing += ", ";
                missing += std::to_string(pc);
            }
        }
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S11",
                                      "Tone row set: score is missing pitch classes: " + missing,
                                      ErrorCode::InvariantViolation));
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
                                out.push_back(
                                    make_diagnostic(ValidationSeverity::Warning,
                                                    "M1",
                                                    "Note outside comfortable range for '" +
                                                        part.definition.name + "'",
                                                    ErrorCode::InconsistentOrch,
                                                    ScoreTime{measure.bar_number, event.offset},
                                                    part.id));
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
                                ValidationSeverity::Error,
                                "M2",
                                "Note outside absolute range for '" + part.definition.name + "'",
                                ErrorCode::InconsistentOrch,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id));
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
                    if (event.is_note_group()) {
                        has_notes = true;
                        break;
                    }
                }
                if (has_notes) break;
            }
            if (has_notes) break;
        }
        if (has_notes) break;
    }

    if (has_notes && score.harmonic_annotations.empty()) {
        out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                      "M8",
                                      "Score has note content but no harmonic annotations",
                                      ErrorCode::StaleHarmonicLayer));
    }
}

// =============================================================================
// R1: Part has no instrument preset (Warning)
// =============================================================================

void validate_r1(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        if (!part.definition.rendering.instrument_preset) {
            out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                          "R1",
                                          "Part '" + part.definition.name +
                                              "' has no instrument preset configured",
                                          ErrorCode::MissingPreset,
                                          std::nullopt,
                                          part.id));
        }
    }
}

// =============================================================================
// R2: Articulation used but not in vocabulary (Warning)
// =============================================================================

void validate_r2(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& vocab = part.definition.articulation_vocabulary;
        if (vocab.empty()) continue; // No vocabulary = all allowed

        std::set<ArticulationType> allowed(vocab.begin(), vocab.end());

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.articulation && !allowed.contains(*note.articulation)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Warning,
                                "R2",
                                "Articulation not in vocabulary for '" + part.definition.name + "'",
                                ErrorCode::UnmappedArticulation,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id));
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

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.articulation && !amap.contains(*note.articulation)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Warning,
                                "R3",
                                "No articulation mapping for '" + part.definition.name + "'",
                                ErrorCode::UnmappedArticulation,
                                ScoreTime{measure.bar_number, event.offset},
                                part.id));
                        }
                    }
                }
            }
        }
    }
}

// =============================================================================
// R6: RenderingConfig and articulation mapping domains (Error)
// =============================================================================

void validate_r6(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        if (!validate_rendering_config(part.definition.rendering)) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "R6",
                                          "Invalid rendering configuration for '" +
                                              part.definition.name + "'",
                                          ErrorCode::InvalidRenderingConfig,
                                          std::nullopt,
                                          part.id));
        }
    }
}

// =============================================================================
// R7: Explicit attack/release velocity and semantic dynamic coherence (Error)
// =============================================================================

void validate_r7(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group) continue;
                    for (const auto& note : group->notes) {
                        const bool invalid_attack_velocity = note.velocity.value > 127;
                        const bool invalid_release_velocity = note.release_velocity > 127;
                        const bool conflicting_semantics = note.dynamic && note.velocity.written &&
                                                           *note.dynamic != *note.velocity.written;
                        if (!invalid_attack_velocity && !invalid_release_velocity &&
                            !conflicting_semantics)
                            continue;
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "R7",
                            invalid_attack_velocity
                                ? "Explicit note attack velocity exceeds MIDI value 127"
                            : invalid_release_velocity
                                ? "Explicit note release velocity exceeds MIDI value 127"
                                : "Note dynamic conflicts with VelocityValue.written",
                            ErrorCode::InvalidRenderingConfig,
                            ScoreTime{measure.bar_number, event.offset},
                            part.id));
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
                    (void)prev_off;
                    const auto& [curr_ng, curr_off] = note_groups[idx];

                    // Check each pair of voice indices (i, j) within the NoteGroups
                    std::size_t min_notes = std::min(prev_ng->notes.size(), curr_ng->notes.size());

                    for (std::size_t i = 0; i < min_notes; ++i) {
                        for (std::size_t j = i + 1; j < min_notes; ++j) {
                            // Spellings outside MIDI range cannot form real
                            // voice pairs; skip them instead of analysing
                            // truncated values.
                            auto prev_lo_r =
                                MidiNote::from_int(midi_value(prev_ng->notes[i].pitch));
                            auto prev_hi_r =
                                MidiNote::from_int(midi_value(prev_ng->notes[j].pitch));
                            auto curr_lo_r =
                                MidiNote::from_int(midi_value(curr_ng->notes[i].pitch));
                            auto curr_hi_r =
                                MidiNote::from_int(midi_value(curr_ng->notes[j].pitch));
                            if (!prev_lo_r || !prev_hi_r || !curr_lo_r || !curr_hi_r) {
                                continue;
                            }
                            MidiNote prev_lo = *prev_lo_r;
                            MidiNote prev_hi = *prev_hi_r;
                            MidiNote curr_lo = *curr_lo_r;
                            MidiNote curr_hi = *curr_hi_r;

                            // Check parallel fifths (interval class 7)
                            if (has_parallel_motion(prev_lo, prev_hi, curr_lo, curr_hi, 7)) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Warning,
                                    "M3",
                                    "Parallel fifth detected in voice " +
                                        std::to_string(voice.voice_index) + ", bar " +
                                        std::to_string(measure.bar_number),
                                    ErrorCode::InvariantViolation,
                                    ScoreTime{measure.bar_number, curr_off},
                                    part.id));
                            }

                            // Check parallel octaves (interval class 0)
                            if (has_parallel_motion(prev_lo, prev_hi, curr_lo, curr_hi, 0)) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Warning,
                                    "M3",
                                    "Parallel octave detected in voice " +
                                        std::to_string(voice.voice_index) + ", bar " +
                                        std::to_string(measure.bar_number),
                                    ErrorCode::InvariantViolation,
                                    ScoreTime{measure.bar_number, curr_off},
                                    part.id));
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
                            ValidationSeverity::Warning,
                            "M4",
                            "Voice crossing between voice " +
                                std::to_string(measure.voices[vi].voice_index) + " and voice " +
                                std::to_string(measure.voices[vi + 1].voice_index) + " in bar " +
                                std::to_string(measure.bar_number),
                            ErrorCode::InvariantViolation,
                            ScoreTime{measure.bar_number, Beat::zero()},
                            part.id));
                        break; // One diagnostic per voice pair per measure
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
                    pitches.push_back({midi_value(ng->notes[0].pitch), event.offset});
                }

                for (std::size_t i = 1; i < pitches.size(); ++i) {
                    int leap = pitches[i].midi - pitches[i - 1].midi;
                    if (std::abs(leap) > 12) {
                        // Check if next interval recovers (opposite direction, step)
                        bool recovered = false;
                        if (i + 1 < pitches.size()) {
                            int recovery = pitches[i + 1].midi - pitches[i].midi;
                            // Opposite direction and step (<=2 semitones)
                            recovered =
                                ((leap > 0 && recovery < 0) || (leap < 0 && recovery > 0)) &&
                                std::abs(recovery) <= 2;
                        }

                        if (!recovered) {
                            out.push_back(
                                make_diagnostic(ValidationSeverity::Warning,
                                                "M5",
                                                "Large leap (" + std::to_string(std::abs(leap)) +
                                                    " semitones) without step recovery in voice " +
                                                    std::to_string(voice.voice_index) + ", bar " +
                                                    std::to_string(measure.bar_number),
                                                ErrorCode::InvariantViolation,
                                                ScoreTime{measure.bar_number, pitches[i].offset},
                                                part.id));
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
                if (ke.position.bar <= measure.bar_number)
                    key_entry = &ke;
                else
                    break;
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
                            if (interval > 0 && interval <= 2 && next_pc == key_root_pc) {
                                continue; // Properly resolved
                            }
                        }

                        out.push_back(
                            make_diagnostic(ValidationSeverity::Info,
                                            "M6",
                                            "Unresolved leading tone in voice " +
                                                std::to_string(voice.voice_index) + ", bar " +
                                                std::to_string(measure.bar_number),
                                            ErrorCode::InvariantViolation,
                                            ScoreTime{measure.bar_number, voice.events[ei].offset},
                                            part.id));
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
                    ScoreTime event_time{measure.bar_number, voice.events[ei].offset};
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
                        if (interval_from_root != 10 && interval_from_root != 11) continue;

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
                                continue; // Properly resolved
                            }
                        }

                        out.push_back(
                            make_diagnostic(ValidationSeverity::Info,
                                            "M7",
                                            "Unresolved chordal seventh in voice " +
                                                std::to_string(voice.voice_index) + ", bar " +
                                                std::to_string(measure.bar_number),
                                            ErrorCode::InvariantViolation,
                                            ScoreTime{measure.bar_number, voice.events[ei].offset},
                                            part.id));
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
                        ValidationSeverity::Info,
                        "M9",
                        "Part '" + part.definition.name + "' has " + std::to_string(gap_count) +
                            " consecutive bars without orchestration annotation"
                            " starting at bar " +
                            std::to_string(gap_start),
                        ErrorCode::InconsistentOrch,
                        ScoreTime{gap_start, Beat::zero()},
                        part.id));
                }
                gap_count = 0;
            }
        }
        // Check trailing gap
        if (gap_count > 8) {
            out.push_back(make_diagnostic(ValidationSeverity::Info,
                                          "M9",
                                          "Part '" + part.definition.name + "' has " +
                                              std::to_string(gap_count) +
                                              " consecutive bars without orchestration annotation"
                                              " starting at bar " +
                                              std::to_string(gap_start),
                                          ErrorCode::InconsistentOrch,
                                          ScoreTime{gap_start, Beat::zero()},
                                          part.id));
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
                    out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                                  "M10",
                                                  "Part '" + part.definition.name + "' has " +
                                                      std::to_string(gap_count) +
                                                      " consecutive bars without dynamic marking"
                                                      " starting at bar " +
                                                      std::to_string(gap_start),
                                                  ErrorCode::InconsistentOrch,
                                                  ScoreTime{gap_start, Beat::zero()},
                                                  part.id));
                }
                gap_count = 0;
            }
        }
        // Check trailing gap
        if (gap_count > 16) {
            out.push_back(make_diagnostic(ValidationSeverity::Info,
                                          "M10",
                                          "Part '" + part.definition.name + "' has " +
                                              std::to_string(gap_count) +
                                              " consecutive bars without dynamic marking"
                                              " starting at bar " +
                                              std::to_string(gap_start),
                                          ErrorCode::InconsistentOrch,
                                          ScoreTime{gap_start, Beat::zero()},
                                          part.id));
        }
    }
}

// =============================================================================
// R4: Grace allocation exceeds half the following ordinary note (Info)
// =============================================================================

void validate_r4(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        std::set<std::uint8_t> voice_indices;
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                voice_indices.insert(voice.voice_index);

        for (const auto voice_index : voice_indices) {
            struct LocatedGroup {
                ScoreTime position;
                const NoteGroup* group;
            };
            std::vector<LocatedGroup> groups;
            for (const auto& measure : part.measures) {
                for (const auto& voice : measure.voices) {
                    if (voice.voice_index != voice_index) continue;
                    for (const auto& event : voice.events) {
                        if (const auto* group = event.as_note_group()) {
                            groups.push_back({ScoreTime{measure.bar_number, event.offset}, group});
                        }
                    }
                }
            }

            for (std::size_t index = 0; index < groups.size(); ++index) {
                const auto& current = groups[index];
                const bool is_grace =
                    std::any_of(current.group->notes.begin(),
                                current.group->notes.end(),
                                [](const Note& note) { return note.grace.has_value(); });
                if (!is_grace) continue;

                const NoteGroup* following = nullptr;
                for (std::size_t next = index + 1; next < groups.size(); ++next) {
                    const bool contains_ordinary =
                        std::any_of(groups[next].group->notes.begin(),
                                    groups[next].group->notes.end(),
                                    [](const Note& note) { return !note.grace.has_value(); });
                    if (contains_ordinary) {
                        following = groups[next].group;
                        break;
                    }
                }
                if (following && current.group->duration > following->duration / 2) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Info,
                        "R4",
                        "Grace note allocation exceeds half the following note duration",
                        ErrorCode::InvariantViolation,
                        current.position,
                        part.id));
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
                    auto abs_result = score_time_to_absolute_beat(st, score.time_map);
                    if (!abs_result) continue;

                    Beat absolute = *abs_result;
                    std::int64_t tick = absolute_beat_to_tick(absolute);
                    Beat round_trip = tick_to_absolute_beat(tick);
                    std::int64_t tick2 = absolute_beat_to_tick(round_trip);

                    if (tick != tick2) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Warning,
                            "R5",
                            "Tick rounding residual in bar " + std::to_string(measure.bar_number) +
                                " at offset " + std::to_string(event.offset.to_float()),
                            ErrorCode::TickConversionError,
                            st,
                            part.id));
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
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S13",
                                          "Duplicate SectionId: " + std::to_string(s.id.value),
                                          ErrorCode::InvariantViolation,
                                          s.start));
        }
        validate_s13_sections(s.children, seen, out);
    }
}

void validate_s13(const Score& score, std::vector<Diagnostic>& out) {
    if (score.id.value == 0) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S13",
                                      "ScoreId zero is an unassigned sentinel",
                                      ErrorCode::InvariantViolation));
    }

    // EventId uniqueness
    std::set<std::uint64_t> seen_event_ids;
    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                for (const auto& e : v.events) {
                    if (!seen_event_ids.insert(e.id.value).second) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S13",
                                            "Duplicate EventId: " + std::to_string(e.id.value),
                                            ErrorCode::InvariantViolation,
                                            ScoreTime{m.bar_number, e.offset},
                                            part.id));
                    }
                }
            }
        }
    }

    // PartId uniqueness
    std::set<std::uint64_t> seen_part_ids;
    for (const auto& part : score.parts) {
        if (!seen_part_ids.insert(part.id.value).second) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S13",
                                          "Duplicate PartId: " + std::to_string(part.id.value),
                                          ErrorCode::InvariantViolation));
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

    std::set<std::uint64_t> seen_beam_group_ids;
    for (const auto& part : score.parts) {
        for (const auto& m : part.measures) {
            for (const auto& v : m.voices) {
                std::map<std::uint64_t, std::size_t> voice_event_indices;
                std::map<std::uint64_t, std::size_t> measured_event_indices;
                std::size_t measured_index = 0;
                for (std::size_t i = 0; i < v.events.size(); ++i) {
                    const auto& event = v.events[i];
                    voice_event_indices[event.id.value] = i;
                    if (event.is_note_group() || event.is_rest())
                        measured_event_indices[event.id.value] = measured_index++;
                }

                std::map<std::uint64_t, std::uint64_t> event_to_beam_group;
                for (const auto& bg : v.beam_groups) {
                    const auto location = ScoreTime{m.bar_number, Beat::zero()};
                    if (!seen_beam_group_ids.insert(bg.id.value).second) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S14",
                                            "Duplicate BeamGroupId: " + std::to_string(bg.id.value),
                                            ErrorCode::InvariantViolation,
                                            location,
                                            part.id));
                    }
                    if (bg.event_ids.size() < 2) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S14",
                                            "BeamGroup must contain at least two measured events",
                                            ErrorCode::InvariantViolation,
                                            location,
                                            part.id));
                    }
                    if (!bg.beam_breaks.empty()) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S14",
                            "BeamGroup beam_breaks is outside the compilable primary-beam "
                            "profile because it lacks beam-level and hook state",
                            ErrorCode::InvariantViolation,
                            location,
                            part.id));
                    }

                    std::set<std::uint64_t> group_event_ids;
                    std::optional<std::size_t> previous_event_index;
                    std::optional<std::size_t> previous_measured_index;
                    for (const auto eid : bg.event_ids) {
                        if (!group_event_ids.insert(eid.value).second) {
                            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                          "S14",
                                                          "BeamGroup contains duplicate EventId: " +
                                                              std::to_string(eid.value),
                                                          ErrorCode::InvariantViolation,
                                                          location,
                                                          part.id));
                            continue;
                        }

                        const auto voice_it = voice_event_indices.find(eid.value);
                        if (voice_it == voice_event_indices.end()) {
                            out.push_back(
                                make_diagnostic(ValidationSeverity::Error,
                                                "S14",
                                                "BeamGroup references non-existent EventId: " +
                                                    std::to_string(eid.value),
                                                ErrorCode::InvariantViolation,
                                                location,
                                                part.id));
                            continue;
                        }

                        const auto& event = v.events[voice_it->second];
                        const auto measured_it = measured_event_indices.find(eid.value);
                        if (measured_it == measured_event_indices.end()) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error,
                                "S14",
                                "BeamGroup member is not a NoteGroup or RestEvent: " +
                                    std::to_string(eid.value),
                                ErrorCode::InvariantViolation,
                                ScoreTime{m.bar_number, event.offset},
                                part.id));
                            continue;
                        }

                        if ((previous_event_index && voice_it->second <= *previous_event_index) ||
                            (previous_measured_index &&
                             measured_it->second != *previous_measured_index + 1)) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error,
                                "S14",
                                "BeamGroup members must be strictly score-ordered and contiguous "
                                "among measured events",
                                ErrorCode::InvariantViolation,
                                ScoreTime{m.bar_number, event.offset},
                                part.id));
                        }
                        previous_event_index = voice_it->second;
                        previous_measured_index = measured_it->second;

                        Beat written_duration = event.duration();
                        bool contains_grace = false;
                        if (const auto* group = event.as_note_group()) {
                            if (group->tuplet_context)
                                written_duration = group->tuplet_context->normal_type;
                            contains_grace =
                                std::ranges::any_of(group->notes, [](const Note& note) {
                                    return note.grace.has_value();
                                });
                            if (!group->beam_group || *group->beam_group != bg.id) {
                                out.push_back(make_diagnostic(
                                    ValidationSeverity::Error,
                                    "S14",
                                    "BeamGroup NoteGroup member lacks the matching back-reference",
                                    ErrorCode::InvariantViolation,
                                    ScoreTime{m.bar_number, event.offset},
                                    part.id));
                            }
                        } else if (const auto* rest = event.as_rest();
                                   rest && rest->tuplet_context) {
                            written_duration = rest->tuplet_context->normal_type;
                        }
                        if (written_duration <= Beat::zero() || written_duration >= Beat{1, 4} ||
                            contains_grace) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error,
                                "S14",
                                "BeamGroup members require a positive written duration shorter "
                                "than a quarter note and may not be grace NoteGroups",
                                ErrorCode::InvariantViolation,
                                ScoreTime{m.bar_number, event.offset},
                                part.id));
                        }

                        const auto [owner, inserted] =
                            event_to_beam_group.emplace(eid.value, bg.id.value);
                        if (!inserted && owner->second != bg.id.value) {
                            out.push_back(
                                make_diagnostic(ValidationSeverity::Error,
                                                "S14",
                                                "Event belongs to more than one BeamGroup: " +
                                                    std::to_string(eid.value),
                                                ErrorCode::InvariantViolation,
                                                ScoreTime{m.bar_number, event.offset},
                                                part.id));
                        }
                    }
                }

                for (const auto& event : v.events) {
                    const auto* group = event.as_note_group();
                    if (!group || !group->beam_group) continue;
                    const auto owner = event_to_beam_group.find(event.id.value);
                    if (owner == event_to_beam_group.end() ||
                        owner->second != group->beam_group->value) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S14",
                            "NoteGroup beam_group does not name a containing BeamGroup in its "
                            "voice and measure",
                            ErrorCode::InvariantViolation,
                            ScoreTime{m.bar_number, event.offset},
                            part.id));
                    }
                }

                // Check tuplet context references: if nested_in is set, verify
                // that some event in this voice shares that parent tuplet id
                std::set<std::uint64_t> tuplet_ids;
                for (const auto& e : v.events) {
                    if (const auto* context = event_tuplet_context(e))
                        tuplet_ids.insert(context->id.value);
                }
                for (const auto& e : v.events) {
                    const auto* context = event_tuplet_context(e);
                    if (context && context->nested_in) {
                        if (!tuplet_ids.contains(context->nested_in->value)) {
                            out.push_back(
                                make_diagnostic(ValidationSeverity::Error,
                                                "S14",
                                                "Tuplet nested_in references unknown TupletId: " +
                                                    std::to_string(context->nested_in->value),
                                                ErrorCode::InvariantViolation,
                                                ScoreTime{m.bar_number, e.offset},
                                                part.id));
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
                out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                              "S14",
                                              "NonChordTone references non-existent EventId: " +
                                                  std::to_string(nct.event_id.value),
                                              ErrorCode::InvariantViolation,
                                              ha.position));
            } else if (nct.note_index >= it->second) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Warning,
                    "S14",
                    "NonChordTone note_index " + std::to_string(nct.note_index) +
                        " out of bounds for EventId " + std::to_string(nct.event_id.value),
                    ErrorCode::InvariantViolation,
                    ha.position));
            }
        }
    }
}

// =============================================================================
// S15: Harmonic annotation ordering and overlap
// =============================================================================

void validate_s15(const Score& score, std::vector<Diagnostic>& out) {
    const auto& anns = score.harmonic_annotations;
    std::optional<Beat> previous_end;
    for (std::size_t i = 1; i < anns.size(); ++i) {
        if (anns[i].position < anns[i - 1].position) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S15",
                                          "Harmonic annotations not sorted by position",
                                          ErrorCode::OverlappingAnnotation,
                                          anns[i].position));
            break;
        }
        auto previous_start = score_time_to_absolute_beat(anns[i - 1].position, score.time_map);
        auto current_start = score_time_to_absolute_beat(anns[i].position, score.time_map);
        if (!previous_start || !current_start || anns[i - 1].duration <= Beat::zero()) continue;
        auto computed_end = checked_add(*previous_start, anns[i - 1].duration);
        if (!computed_end) continue;
        previous_end = *computed_end;
        if (*previous_end > *current_start) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S15",
                                          "Harmonic annotations overlap at position",
                                          ErrorCode::OverlappingAnnotation,
                                          anns[i].position));
        }
    }

    for (const auto& annotation : anns) {
        auto start = score_time_to_absolute_beat(annotation.position, score.time_map);
        if (!start || annotation.position.bar > score.metadata.total_bars ||
            annotation.duration <= Beat::zero()) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S15",
                                          "Harmonic annotation requires an in-score point and a "
                                          "positive exact duration",
                                          ErrorCode::OverlappingAnnotation,
                                          annotation.position));
            continue;
        }
        auto end = checked_add(*start, annotation.duration);
        std::optional<Beat> score_end;
        if (score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max()) {
            auto converted = score_time_to_absolute_beat(
                ScoreTime{score.metadata.total_bars + 1, Beat::zero()}, score.time_map);
            if (converted) score_end = *converted;
        }
        if (!end || !score_end || *end > *score_end) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S15",
                                          "Harmonic annotation exceeds the score extent",
                                          ErrorCode::OverlappingAnnotation,
                                          annotation.position));
        }

        bool invalid_payload =
            annotation.chord.notes.empty() || annotation.chord.quality.empty() ||
            annotation.roman_numeral.empty() ||
            (annotation.secondary_function && annotation.secondary_function->empty()) ||
            !std::isfinite(annotation.confidence) || annotation.confidence < 0.0F ||
            annotation.confidence > 1.0F ||
            static_cast<std::uint8_t>(annotation.function) >
                static_cast<std::uint8_t>(ScoreHarmonicFunction::Ambiguous) ||
            (annotation.cadence && static_cast<std::uint8_t>(*annotation.cadence) >
                                       static_cast<std::uint8_t>(CadenceType::None));
        for (std::size_t note = 1; note < annotation.chord.notes.size(); ++note)
            invalid_payload =
                invalid_payload || annotation.chord.notes[note - 1] >= annotation.chord.notes[note];
        for (const auto& non_chord_tone : annotation.non_chord_tones)
            invalid_payload =
                invalid_payload || static_cast<std::uint8_t>(non_chord_tone.type) >
                                       static_cast<std::uint8_t>(NonChordToneType::Pedal);

        const auto intervals = chord_quality_intervals(annotation.chord.quality);
        if (intervals) {
            invalid_payload =
                invalid_payload || annotation.chord.inversion < 0 ||
                static_cast<std::size_t>(annotation.chord.inversion) >= intervals->size();
            std::set<int> expected_pitch_classes;
            for (const auto interval : *intervals)
                expected_pitch_classes.insert(
                    PitchClass::wrapped(annotation.chord.root + interval));
            std::set<int> observed_pitch_classes;
            for (const auto note : annotation.chord.notes)
                observed_pitch_classes.insert(PitchClass::wrapped(note));
            invalid_payload = invalid_payload || observed_pitch_classes != expected_pitch_classes;
            if (!annotation.chord.notes.empty() && annotation.chord.inversion >= 0 &&
                static_cast<std::size_t>(annotation.chord.inversion) < intervals->size()) {
                const auto expected_bass = PitchClass::wrapped(
                    annotation.chord.root + (*intervals)[annotation.chord.inversion]);
                invalid_payload =
                    invalid_payload ||
                    PitchClass::wrapped(annotation.chord.notes.front()) != expected_bass;
            }
        } else {
            // Unregistered/non-tertian qualities remain representable, but
            // without an interval algebra only root-position identity is
            // tractable.
            invalid_payload =
                invalid_payload || annotation.chord.inversion != 0 ||
                (!annotation.chord.notes.empty() &&
                 PitchClass::wrapped(annotation.chord.notes.front()) != annotation.chord.root);
        }
        if (invalid_payload) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error,
                "S15",
                "Harmonic annotation requires a complete ascending voicing, coherent inversion, "
                "non-empty labels, closed enums, and finite confidence in [0,1]",
                ErrorCode::InvariantViolation,
                annotation.position));
        }
    }
}

// =============================================================================
// S16: Orchestration annotation field consistency
// =============================================================================

void validate_s16(const Score& score, std::vector<Diagnostic>& out) {
    const auto find_annotation_part = [&](PartId part_id) -> const Part* {
        const auto part =
            std::find_if(score.parts.begin(), score.parts.end(), [part_id](const Part& candidate) {
                return candidate.id == part_id;
            });
        return part == score.parts.end() ? nullptr : &*part;
    };

    for (const auto& ann : score.orchestration_annotations) {
        if (static_cast<std::uint8_t>(ann.role) >
            static_cast<std::uint8_t>(TexturalRole::Accompagnato)) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S16",
                                          "Orchestration role is outside the modelled enumeration",
                                          ErrorCode::InconsistentOrchField,
                                          ann.start,
                                          ann.part_id));
            continue;
        }
        const Part* owner = find_annotation_part(ann.part_id);
        if (!owner) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S16",
                                          "Orchestration annotation references a missing Part",
                                          ErrorCode::InconsistentOrchField,
                                          ann.start,
                                          ann.part_id));
            continue;
        }
        if (!valid_scoped_part_point(score, *owner, ann.start, false) ||
            !valid_scoped_part_point(score, *owner, ann.end, true) || !(ann.start < ann.end)) {
            out.push_back(
                make_diagnostic(ValidationSeverity::Error,
                                "S16",
                                "Orchestration annotation requires a non-empty in-score Part span",
                                ErrorCode::InconsistentOrchField,
                                ann.start,
                                ann.part_id));
        }
        switch (ann.role) {
        case TexturalRole::Doubling:
            if (!ann.doubled_part || *ann.doubled_part == ann.part_id ||
                !find_annotation_part(*ann.doubled_part)) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S16",
                                              "Doubling role requires a distinct existing "
                                              "doubled_part",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            break;
        case TexturalRole::PedalTone:
            if (!ann.pedal_pitch || ann.pedal_pitch->letter > 6) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S16",
                                              "PedalTone role missing pedal_pitch field",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            break;
        case TexturalRole::Dialogue:
            if (!ann.dialogue_partner || *ann.dialogue_partner == ann.part_id ||
                !find_annotation_part(*ann.dialogue_partner)) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S16",
                                              "Dialogue role requires a distinct existing "
                                              "dialogue_partner",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            break;
        default:
            // Non-role-specific: doubled_part, pedal_pitch, dialogue_partner
            // should not be populated
            if (ann.doubled_part) {
                out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                              "S16",
                                              "doubled_part set on non-Doubling role",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            if (ann.pedal_pitch) {
                out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                              "S16",
                                              "pedal_pitch set on non-PedalTone role",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            if (ann.dialogue_partner) {
                out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                              "S16",
                                              "dialogue_partner set on non-Dialogue role",
                                              ErrorCode::InconsistentOrchField,
                                              ann.start,
                                              ann.part_id));
            }
            break;
        }
    }

    for (const auto& part : score.parts) {
        std::vector<const OrchestrationAnnotation*> ordered;
        for (const auto& annotation : score.orchestration_annotations)
            if (annotation.part_id == part.id) ordered.push_back(&annotation);
        std::sort(ordered.begin(), ordered.end(), [](const auto* lhs, const auto* rhs) {
            return lhs->start < rhs->start;
        });
        for (std::size_t index = 1; index < ordered.size(); ++index) {
            if (ordered[index]->start < ordered[index - 1]->end) {
                out.push_back(
                    make_diagnostic(ValidationSeverity::Error,
                                    "S16",
                                    "Orchestration annotations for one Part may not overlap",
                                    ErrorCode::OverlappingAnnotation,
                                    ordered[index]->start,
                                    part.id));
            }
        }
    }
}

// =============================================================================
// S17: Grace-note group coherence
// =============================================================================

void validate_s17(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group || group->notes.empty()) continue;

                    const auto group_grace = group->notes.front().grace;
                    const bool incoherent =
                        std::any_of(group->notes.begin(),
                                    group->notes.end(),
                                    [&](const Note& note) { return note.grace != group_grace; });
                    if (incoherent) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Error,
                            "S17",
                            "A NoteGroup cannot mix ordinary notes or different grace-note types",
                            ErrorCode::InvariantViolation,
                            ScoreTime{measure.bar_number, event.offset},
                            part.id));
                    }

                    if (group_grace &&
                        std::any_of(group->notes.begin(), group->notes.end(), [](const Note& note) {
                            return note.tie_forward;
                        })) {
                        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                      "S17",
                                                      "Grace notes cannot carry duration ties",
                                                      ErrorCode::InvariantViolation,
                                                      ScoreTime{measure.bar_number, event.offset},
                                                      part.id));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S18: Measure numbering and Voice identity/order
// =============================================================================

void validate_s18(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (std::size_t measure_index = 0; measure_index < part.measures.size(); ++measure_index) {
            const auto& measure = part.measures[measure_index];
            const auto expected_bar = static_cast<std::uint64_t>(measure_index) + 1;
            if (expected_bar > std::numeric_limits<std::uint32_t>::max() ||
                measure.bar_number != expected_bar) {
                out.push_back(
                    make_diagnostic(ValidationSeverity::Error,
                                    "S18",
                                    "Measure vector position does not match its 1-based bar_number",
                                    ErrorCode::InvariantViolation,
                                    ScoreTime{measure.bar_number, Beat::zero()},
                                    part.id));
            }

            for (std::size_t voice_index = 1; voice_index < measure.voices.size(); ++voice_index) {
                if (measure.voices[voice_index - 1].voice_index >=
                    measure.voices[voice_index].voice_index) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Error,
                        "S18",
                        "Voice indices must be unique and strictly increasing within a measure",
                        ErrorCode::InvariantViolation,
                        ScoreTime{measure.bar_number, Beat::zero()},
                        part.id));
                    break;
                }
            }
        }
    }
}

// =============================================================================
// S19: Direction payload coherence
// =============================================================================

void validate_s19(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* direction = std::get_if<ScoreDirection>(&event.payload);
                    if (!direction) continue;
                    const ScoreTime location{measure.bar_number, event.offset};

                    if (static_cast<std::uint8_t>(direction->type) >
                        static_cast<std::uint8_t>(DirectionType::Caesura)) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Error,
                                            "S19",
                                            "Direction type is outside the modelled enumeration",
                                            ErrorCode::InvariantViolation,
                                            location,
                                            part.id));
                        continue;
                    }

                    const bool requires_text = direction->type == DirectionType::Text ||
                                               direction->type == DirectionType::TempoText;
                    if (requires_text && (!direction->text || direction->text->empty())) {
                        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                      "S19",
                                                      "Text and TempoText directions require "
                                                      "non-empty text",
                                                      ErrorCode::InvariantViolation,
                                                      location,
                                                      part.id));
                    } else if (!requires_text && direction->text) {
                        out.push_back(make_diagnostic(ValidationSeverity::Warning,
                                                      "S19",
                                                      "Text is populated for a non-text Direction",
                                                      ErrorCode::InvariantViolation,
                                                      location,
                                                      part.id));
                    }

                    if (direction->type == DirectionType::ClefChange) {
                        if (!direction->new_clef) {
                            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                          "S19",
                                                          "ClefChange direction requires new_clef",
                                                          ErrorCode::InvariantViolation,
                                                          location,
                                                          part.id));
                        }
                    } else if (direction->new_clef) {
                        out.push_back(
                            make_diagnostic(ValidationSeverity::Warning,
                                            "S19",
                                            "new_clef is populated for a non-ClefChange Direction",
                                            ErrorCode::InvariantViolation,
                                            location,
                                            part.id));
                    }

                    if (direction->type == DirectionType::OttavaStart) {
                        const int shift = static_cast<int>(direction->ottava_shift);
                        if (shift != 12 && shift != -12 && shift != 24 && shift != -24) {
                            out.push_back(make_diagnostic(
                                ValidationSeverity::Error,
                                "S19",
                                "OttavaStart requires ottava_shift of ±12 or ±24 semitones",
                                ErrorCode::InvariantViolation,
                                location,
                                part.id));
                        }
                    } else if (direction->ottava_shift != 0) {
                        out.push_back(make_diagnostic(
                            ValidationSeverity::Warning,
                            "S19",
                            "ottava_shift is populated for a non-OttavaStart Direction",
                            ErrorCode::InvariantViolation,
                            location,
                            part.id));
                    }
                }
            }
        }
    }
}

// =============================================================================
// S20: Part-scoped span and conditional-payload coherence
// =============================================================================

std::optional<Beat> part_measure_duration(const Score& score, const Part& part, std::uint32_t bar) {
    if (bar < 1 || bar > score.metadata.total_bars || bar > part.measures.size())
        return std::nullopt;
    const auto& measure = part.measures[bar - 1];
    if (measure.local_time) {
        auto duration = checked_measure_duration(*measure.local_time);
        return duration ? std::optional<Beat>{*duration} : std::nullopt;
    }

    const TimeSignatureEntry* active = nullptr;
    for (const auto& entry : score.time_map) {
        if (entry.bar > bar) break;
        active = &entry;
    }
    if (!active) return std::nullopt;
    auto duration = checked_measure_duration(active->time_signature);
    return duration ? std::optional<Beat>{*duration} : std::nullopt;
}

bool valid_part_point(const Score& score, const Part& part, ScoreTime point, bool allow_terminal) {
    if (point.bar < 1) return false;
    if (allow_terminal && score.metadata.total_bars < std::numeric_limits<std::uint32_t>::max() &&
        point.bar == score.metadata.total_bars + 1)
        return point.beat == Beat::zero();
    const auto duration = part_measure_duration(score, part, point.bar);
    return duration && point.beat >= Beat::zero() && point.beat < *duration;
}

void validate_s20(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        std::vector<const Hairpin*> ordered_hairpins;
        ordered_hairpins.reserve(part.hairpins.size());
        for (const auto& hairpin : part.hairpins) {
            const bool valid_start = valid_part_point(score, part, hairpin.start, false);
            const bool valid_end = valid_part_point(score, part, hairpin.end, true);
            if (!valid_start || !valid_end ||
                (valid_start && valid_end && !(hairpin.start < hairpin.end))) {
                out.push_back(
                    make_diagnostic(ValidationSeverity::Error,
                                    "S20",
                                    "Hairpin requires a non-empty in-score half-open span",
                                    ErrorCode::InvalidScoreTime,
                                    hairpin.start,
                                    part.id));
            }
            if (static_cast<std::uint8_t>(hairpin.type) >
                    static_cast<std::uint8_t>(HairpinType::Diminuendo) ||
                (hairpin.target && static_cast<std::uint8_t>(*hairpin.target) >
                                       static_cast<std::uint8_t>(DynamicLevel::rfz))) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S20",
                                              "Hairpin contains an out-of-domain enum value",
                                              ErrorCode::InvariantViolation,
                                              hairpin.start,
                                              part.id));
            }
            if (valid_start && valid_end && hairpin.start < hairpin.end)
                ordered_hairpins.push_back(&hairpin);
        }
        std::sort(ordered_hairpins.begin(),
                  ordered_hairpins.end(),
                  [](const Hairpin* lhs, const Hairpin* rhs) { return lhs->start < rhs->start; });
        for (std::size_t i = 1; i < ordered_hairpins.size(); ++i) {
            if (ordered_hairpins[i]->start < ordered_hairpins[i - 1]->end) {
                out.push_back(make_diagnostic(
                    ValidationSeverity::Error,
                    "S20",
                    "Part hairpins may not overlap because velocity precedence would be ambiguous",
                    ErrorCode::OverlappingAnnotation,
                    ordered_hairpins[i]->start,
                    part.id));
            }
        }

        for (const auto& directive : part.part_directives) {
            const bool valid_start = valid_part_point(score, part, directive.start, false);
            const bool valid_end = valid_part_point(score, part, directive.end, true);
            if (!valid_start || !valid_end ||
                (valid_start && valid_end && !(directive.start < directive.end))) {
                out.push_back(
                    make_diagnostic(ValidationSeverity::Error,
                                    "S20",
                                    "PartDirective requires a non-empty in-score half-open span",
                                    ErrorCode::InvalidScoreTime,
                                    directive.start,
                                    part.id));
            }
            if (static_cast<std::uint8_t>(directive.directive) >
                static_cast<std::uint8_t>(DirectiveType::TreCorde)) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S20",
                                              "PartDirective contains an out-of-domain type",
                                              ErrorCode::InvariantViolation,
                                              directive.start,
                                              part.id));
                continue;
            }
            if (directive.directive == DirectiveType::Divisi) {
                if (directive.divisi_count < 2) {
                    out.push_back(make_diagnostic(ValidationSeverity::Error,
                                                  "S20",
                                                  "Divisi requires divisi_count of at least two",
                                                  ErrorCode::InvariantViolation,
                                                  directive.start,
                                                  part.id));
                }
            } else if (directive.divisi_count != 0) {
                out.push_back(
                    make_diagnostic(ValidationSeverity::Warning,
                                    "S20",
                                    "divisi_count is populated for a non-Divisi PartDirective",
                                    ErrorCode::InvariantViolation,
                                    directive.start,
                                    part.id));
            }
        }
    }
}

// =============================================================================
// S21: Scale-definition closure and key-signature identity coherence
// =============================================================================

void validate_key_signature_identity(const KeySignature& key,
                                     ScoreTime location,
                                     std::optional<PartId> part,
                                     std::vector<Diagnostic>& out) {
    if (key.root.letter > 6 || key.mode.note_count == 0 ||
        key.mode.note_count > key.mode.intervals.size()) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error,
            "S21",
            "KeySignature root letter or positive scale note_count is outside its representable "
            "domain",
            ErrorCode::InvariantViolation,
            location,
            part));
        return;
    }

    bool valid_interval_profile = key.mode.intervals[0] == 0;
    for (std::size_t i = 0; i < key.mode.note_count; ++i) {
        const auto interval = key.mode.intervals[i];
        valid_interval_profile = valid_interval_profile && interval >= 0 && interval < 12;
        if (i > 0)
            valid_interval_profile = valid_interval_profile && key.mode.intervals[i - 1] < interval;
    }
    for (std::size_t i = key.mode.note_count; i < key.mode.intervals.size(); ++i)
        valid_interval_profile = valid_interval_profile && key.mode.intervals[i] == 0;
    if (!valid_interval_profile) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error,
            "S21",
            "KeySignature scale intervals must start at 0, increase strictly within [0,11], "
            "and leave unused storage zeroed",
            ErrorCode::InvariantViolation,
            location,
            part));
        return;
    }

    if (!key.mode.name.empty()) {
        const auto registered = find_scale(key.mode.name);
        if (registered &&
            (key.mode.name != registered->name || key.mode.note_count != registered->note_count ||
             key.mode.intervals != registered->intervals)) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S21",
                                          "KeySignature named scale '" + key.mode.name +
                                              "' contradicts its registered interval definition",
                                          ErrorCode::InvariantViolation,
                                          location,
                                          part));
            return;
        }
    }

    const auto expected = expected_key_accidentals(key);
    if (expected && *expected != key.accidentals) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error,
            "S21",
            "KeySignature tonic/mode implies " + std::to_string(*expected) +
                " fifths but the stored accidentals field is " + std::to_string(key.accidentals),
            ErrorCode::InvariantViolation,
            location,
            part));
    }
}

void validate_s21(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& entry : score.key_map)
        validate_key_signature_identity(entry.key, entry.position, std::nullopt, out);
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (measure.local_key) {
                validate_key_signature_identity(
                    *measure.local_key, ScoreTime{measure.bar_number, Beat::zero()}, part.id, out);
            }
        }
    }
    for (const auto& annotation : score.harmonic_annotations)
        validate_key_signature_identity(
            annotation.key_context, annotation.position, std::nullopt, out);
}

// =============================================================================
// S22: Staff topology and Voice ownership
// =============================================================================

void validate_s22(const Score& score, std::vector<Diagnostic>& out) {
    for (const auto& part : score.parts) {
        const auto& definition = part.definition;
        if (definition.staff_count == 0) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S22",
                                          "Part staff_count must be at least one",
                                          ErrorCode::InvariantViolation,
                                          std::nullopt,
                                          part.id));
        }
        if (!definition.staff_clefs.empty() &&
            definition.staff_clefs.size() != definition.staff_count) {
            out.push_back(make_diagnostic(
                ValidationSeverity::Error,
                "S22",
                "Part staff_clefs must be empty or contain exactly staff_count entries",
                ErrorCode::InvariantViolation,
                std::nullopt,
                part.id));
        }
        if (static_cast<std::uint8_t>(definition.clef) > static_cast<std::uint8_t>(Clef::Tab)) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S22",
                                          "Part default clef is out of domain",
                                          ErrorCode::InvariantViolation,
                                          std::nullopt,
                                          part.id));
        }
        for (const auto clef : definition.staff_clefs) {
            if (static_cast<std::uint8_t>(clef) > static_cast<std::uint8_t>(Clef::Tab)) {
                out.push_back(make_diagnostic(ValidationSeverity::Error,
                                              "S22",
                                              "Part staff_clefs contains an out-of-domain clef",
                                              ErrorCode::InvariantViolation,
                                              std::nullopt,
                                              part.id));
                break;
            }
        }
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                if (voice.staff_index >= definition.staff_count) {
                    out.push_back(make_diagnostic(
                        ValidationSeverity::Error,
                        "S22",
                        "Voice staff_index is outside the owning Part's staff_count",
                        ErrorCode::InvariantViolation,
                        ScoreTime{measure.bar_number, Beat::zero()},
                        part.id));
                }
            }
        }
    }
}

// =============================================================================
// S23: Paired notation endpoint coherence
// =============================================================================

struct StaffEndpoint {
    ScoreTime position;
    std::uint8_t voice_index;
    std::size_t event_order;
    DirectionType type;
};

void validate_s23(const Score& score, std::vector<Diagnostic>& out) {
    const auto endpoint_error =
        [&](const std::string& message, ScoreTime location, PartId part_id) {
            out.push_back(make_diagnostic(ValidationSeverity::Error,
                                          "S23",
                                          message,
                                          ErrorCode::InvariantViolation,
                                          location,
                                          part_id));
        };

    for (const auto& part : score.parts) {
        std::map<std::uint8_t, std::optional<ScoreTime>> active_slurs;
        std::map<std::uint8_t, std::optional<ScoreTime>> active_glissandi;
        std::map<std::uint8_t, std::vector<StaffEndpoint>> staff_endpoints;
        std::vector<StaffEndpoint> pedal_endpoints;

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                auto& active_slur = active_slurs[voice.voice_index];
                auto& active_glissando = active_glissandi[voice.voice_index];
                for (std::size_t event_order = 0; event_order < voice.events.size();
                     ++event_order) {
                    const auto& event = voice.events[event_order];
                    const ScoreTime location{measure.bar_number, event.offset};

                    if (const auto* group = event.as_note_group()) {
                        // Incoming endpoints precede outgoing endpoints at one
                        // musical position, matching MusicXML score order.
                        if (group->slur_end) {
                            if (!active_slur) {
                                endpoint_error("Slur stop has no active start", location, part.id);
                            } else {
                                if (!(*active_slur < location))
                                    endpoint_error(
                                        "Slur span must be non-empty", location, part.id);
                                active_slur.reset();
                            }
                        }
                        if (group->slur_start) {
                            if (active_slur) {
                                endpoint_error("Overlapping slurs require explicit identities that "
                                               "Sunny does not store",
                                               location,
                                               part.id);
                            } else {
                                active_slur = location;
                            }
                        }

                        std::size_t glissando_starts = 0;
                        std::size_t glissando_stops = 0;
                        for (const auto& note : group->notes) {
                            if (note.articulation == ArticulationType::GlissandoStart)
                                ++glissando_starts;
                            if (note.articulation == ArticulationType::GlissandoEnd)
                                ++glissando_stops;
                        }
                        if (glissando_starts > 1 || glissando_stops > 1) {
                            endpoint_error(
                                "Concurrent chord glissandi require numbered identities that "
                                "Sunny does not store",
                                location,
                                part.id);
                        }
                        if (glissando_stops > 0) {
                            if (!active_glissando) {
                                endpoint_error(
                                    "Glissando stop has no active start", location, part.id);
                            } else {
                                if (!(*active_glissando < location))
                                    endpoint_error(
                                        "Glissando span must be non-empty", location, part.id);
                                active_glissando.reset();
                            }
                        }
                        if (glissando_starts > 0) {
                            if (active_glissando) {
                                endpoint_error(
                                    "Overlapping glissandi require numbered identities that Sunny "
                                    "does not store",
                                    location,
                                    part.id);
                            } else {
                                active_glissando = location;
                            }
                        }
                    }

                    if (const auto* direction = std::get_if<ScoreDirection>(&event.payload)) {
                        if (direction->type == DirectionType::OttavaStart ||
                            direction->type == DirectionType::OttavaEnd) {
                            staff_endpoints[voice.staff_index].push_back(StaffEndpoint{
                                location, voice.voice_index, event_order, direction->type});
                        } else if (direction->type == DirectionType::PedalDown ||
                                   direction->type == DirectionType::PedalUp) {
                            pedal_endpoints.push_back(StaffEndpoint{
                                location, voice.voice_index, event_order, direction->type});
                        }
                    }
                }
            }
        }

        for (const auto& [voice_index, start] : active_slurs) {
            (void)voice_index;
            if (start) endpoint_error("Slur start has no stop", *start, part.id);
        }
        for (const auto& [voice_index, start] : active_glissandi) {
            (void)voice_index;
            if (start) endpoint_error("Glissando start has no stop", *start, part.id);
        }

        for (auto& [staff_index, endpoints] : staff_endpoints) {
            (void)staff_index;
            std::stable_sort(
                endpoints.begin(), endpoints.end(), [](const auto& lhs, const auto& rhs) {
                    if (lhs.position != rhs.position) return lhs.position < rhs.position;
                    if (lhs.voice_index != rhs.voice_index)
                        return lhs.voice_index < rhs.voice_index;
                    return lhs.event_order < rhs.event_order;
                });

            std::optional<ScoreTime> active_ottava;
            for (std::size_t i = 0; i < endpoints.size(); ++i) {
                const auto& endpoint = endpoints[i];
                if (i > 0 && endpoints[i - 1].position == endpoint.position) {
                    endpoint_error(
                        "Multiple ottava endpoints at one Staff time have no stored order",
                        endpoint.position,
                        part.id);
                }

                if (endpoint.type == DirectionType::OttavaStart) {
                    if (active_ottava)
                        endpoint_error("Overlapping ottavas require numbered identities that Sunny "
                                       "does not store",
                                       endpoint.position,
                                       part.id);
                    else
                        active_ottava = endpoint.position;
                } else if (endpoint.type == DirectionType::OttavaEnd) {
                    if (!active_ottava) {
                        endpoint_error(
                            "Ottava stop has no active start", endpoint.position, part.id);
                    } else {
                        if (!(*active_ottava < endpoint.position))
                            endpoint_error(
                                "Ottava span must be non-empty", endpoint.position, part.id);
                        active_ottava.reset();
                    }
                }
            }
            if (active_ottava) endpoint_error("Ottava start has no stop", *active_ottava, part.id);
        }

        // The physical damper pedal is Part-scoped even though a notation mark is
        // visually anchored to one Staff. This also matches one MIDI channel per Part.
        std::stable_sort(
            pedal_endpoints.begin(), pedal_endpoints.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.position != rhs.position) return lhs.position < rhs.position;
                if (lhs.voice_index != rhs.voice_index) return lhs.voice_index < rhs.voice_index;
                return lhs.event_order < rhs.event_order;
            });
        std::optional<ScoreTime> active_pedal;
        std::vector<std::pair<ScoreTime, ScoreTime>> direction_pedal_ranges;
        for (std::size_t i = 0; i < pedal_endpoints.size(); ++i) {
            const auto& endpoint = pedal_endpoints[i];
            if (i > 0 && pedal_endpoints[i - 1].position == endpoint.position) {
                endpoint_error(
                    "Multiple pedal endpoints at one Part time have no stored change semantics",
                    endpoint.position,
                    part.id);
            }
            if (endpoint.type == DirectionType::PedalDown) {
                if (active_pedal)
                    endpoint_error("Repeated PedalDown requires change/retake semantics that Sunny "
                                   "does not store",
                                   endpoint.position,
                                   part.id);
                else
                    active_pedal = endpoint.position;
            } else {
                if (!active_pedal) {
                    endpoint_error("PedalUp has no active PedalDown", endpoint.position, part.id);
                } else {
                    if (!(*active_pedal < endpoint.position))
                        endpoint_error("Pedal span must be non-empty", endpoint.position, part.id);
                    direction_pedal_ranges.emplace_back(*active_pedal, endpoint.position);
                    active_pedal.reset();
                }
            }
        }
        if (active_pedal) {
            // A terminal open sustain pedal explicitly means pedaling through the
            // final bar line, which LilyPond documents by omitting sustainOff.
            if (score.metadata.total_bars == std::numeric_limits<std::uint32_t>::max()) {
                endpoint_error("Terminal pedal endpoint overflows ScoreTime bar numbering",
                               *active_pedal,
                               part.id);
            } else {
                direction_pedal_ranges.emplace_back(
                    *active_pedal, ScoreTime{score.metadata.total_bars + 1, Beat::zero()});
            }
        }

        std::vector<std::pair<ScoreTime, ScoreTime>> directive_pedal_ranges;
        for (const auto& directive : part.part_directives) {
            if (directive.directive == DirectiveType::SustainingPedal)
                directive_pedal_ranges.emplace_back(directive.start, directive.end);
        }
        std::sort(directive_pedal_ranges.begin(), directive_pedal_ranges.end());
        for (std::size_t i = 1; i < directive_pedal_ranges.size(); ++i) {
            if (directive_pedal_ranges[i].first < directive_pedal_ranges[i - 1].second) {
                endpoint_error("SustainingPedal PartDirective spans may not overlap",
                               directive_pedal_ranges[i].first,
                               part.id);
            }
        }
        if (!direction_pedal_ranges.empty() && !directive_pedal_ranges.empty() &&
            direction_pedal_ranges != directive_pedal_ranges) {
            endpoint_error(
                "Direction and PartDirective sustaining-pedal ranges must agree exactly when both "
                "representations are present",
                direction_pedal_ranges.front().first,
                part.id);
        }
    }
}

// =============================================================================
// S24: Tempo rate and incoming-transition coherence
// =============================================================================

void validate_s24(const Score& score, std::vector<Diagnostic>& out) {
    const auto tempo_error = [&](const std::string& message, ScoreTime location) {
        out.push_back(make_diagnostic(
            ValidationSeverity::Error, "S24", message, ErrorCode::InvalidTempo, location));
    };
    const auto valid_beat_unit = [](BeatUnit unit) {
        return static_cast<std::uint8_t>(unit) <= static_cast<std::uint8_t>(BeatUnit::Sixteenth);
    };
    const auto exact_quarter_rate = [&](const TempoEvent& event) -> Result<Beat> {
        auto scaled = checked_mul(Beat{event.bpm.numerator(), event.bpm.denominator()},
                                  beat_unit_duration(event.beat_unit));
        if (!scaled) return std::unexpected(scaled.error());
        return checked_mul(*scaled, Beat{4, 1});
    };

    std::vector<std::optional<Beat>> absolute_positions;
    absolute_positions.reserve(score.tempo_map.size());
    for (const auto& event : score.tempo_map) {
        auto absolute = score_time_to_absolute_beat(event.position, score.time_map);
        if (absolute)
            absolute_positions.emplace_back(*absolute);
        else
            absolute_positions.emplace_back(std::nullopt);
    }

    for (std::size_t index = 0; index < score.tempo_map.size(); ++index) {
        const auto& event = score.tempo_map[index];
        const bool unit_valid = valid_beat_unit(event.beat_unit);
        const bool old_unit_valid = valid_beat_unit(event.old_unit);
        const bool new_unit_valid = valid_beat_unit(event.new_unit);
        if (!unit_valid || !old_unit_valid || !new_unit_valid)
            tempo_error("Tempo BeatUnit payload is outside the modelled enumeration",
                        event.position);

        const auto encoded_transition = static_cast<std::uint8_t>(event.transition_type);
        if (encoded_transition > static_cast<std::uint8_t>(TempoTransitionType::MetricModulation)) {
            tempo_error("Tempo transition type is outside the modelled enumeration",
                        event.position);
            continue;
        }

        if (index == 0 && event.transition_type != TempoTransitionType::Immediate)
            tempo_error("The initial tempo must use an Immediate transition", event.position);

        if (event.transition_type == TempoTransitionType::Linear) {
            if (index == 0) continue;
            if (event.linear_duration <= Beat::zero()) {
                tempo_error("A Linear tempo transition must have positive duration",
                            event.position);
                continue;
            }
            if (absolute_positions[index] && absolute_positions[index - 1]) {
                auto interval =
                    checked_sub(*absolute_positions[index], *absolute_positions[index - 1]);
                if (!interval) {
                    tempo_error("Tempo transition interval exceeds exact Beat arithmetic",
                                event.position);
                } else if (*interval != event.linear_duration) {
                    tempo_error("A Linear tempo duration must equal the complete interval from the "
                                "previous tempo event",
                                event.position);
                }
            }
            continue;
        }

        if (event.linear_duration != Beat::zero())
            tempo_error("Only a Linear tempo transition may carry linear_duration", event.position);

        if (event.transition_type != TempoTransitionType::MetricModulation || index == 0 ||
            !unit_valid || !old_unit_valid || !new_unit_valid)
            continue;

        if (event.beat_unit != event.new_unit) {
            tempo_error("A MetricModulation event's beat_unit must equal new_unit", event.position);
            continue;
        }

        const auto& previous = score.tempo_map[index - 1];
        if (!valid_beat_unit(previous.beat_unit)) continue;

        auto previous_quarter_rate = exact_quarter_rate(previous);
        auto current_quarter_rate = exact_quarter_rate(event);
        auto unit_ratio =
            checked_div(beat_unit_duration(event.new_unit), beat_unit_duration(event.old_unit));
        if (!previous_quarter_rate || !current_quarter_rate || !unit_ratio) {
            tempo_error("MetricModulation rate exceeds exact rational arithmetic", event.position);
            continue;
        }
        auto expected_quarter_rate = checked_mul(*previous_quarter_rate, *unit_ratio);
        if (!expected_quarter_rate) {
            tempo_error("MetricModulation rate exceeds exact rational arithmetic", event.position);
        } else if (*current_quarter_rate != *expected_quarter_rate) {
            tempo_error(
                "MetricModulation BPM does not equal the exact old-unit/new-unit derivation",
                event.position);
        }
    }
}

// =============================================================================
// S25: Event payload and closed-enum domains
// =============================================================================

void validate_s25(const Score& score, std::vector<Diagnostic>& out) {
    const auto event_error = [&](const std::string& message, ScoreTime location, PartId part_id) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S25",
                                      message,
                                      ErrorCode::InvariantViolation,
                                      location,
                                      part_id));
    };
    const auto valid_pitch = [](const SpelledPitch& pitch) { return pitch.letter <= 6; };

    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const ScoreTime location{measure.bar_number, event.offset};
                    if (const auto* group = event.as_note_group()) {
                        if (group->notes.empty())
                            event_error(
                                "NoteGroup must contain at least one Note", location, part.id);
                        for (const auto& note : group->notes) {
                            bool invalid = !valid_pitch(note.pitch) || note.velocity.value > 127 ||
                                           note.release_velocity > 127;
                            invalid =
                                invalid || (note.velocity.written &&
                                            static_cast<std::uint8_t>(*note.velocity.written) >
                                                static_cast<std::uint8_t>(DynamicLevel::rfz));
                            invalid = invalid || (note.dynamic &&
                                                  static_cast<std::uint8_t>(*note.dynamic) >
                                                      static_cast<std::uint8_t>(DynamicLevel::rfz));
                            invalid = invalid ||
                                      (note.articulation &&
                                       static_cast<std::uint8_t>(*note.articulation) >
                                           static_cast<std::uint8_t>(ArticulationType::BendDown));
                            invalid =
                                invalid || (note.grace &&
                                            static_cast<std::uint8_t>(*note.grace) >
                                                static_cast<std::uint8_t>(GraceType::Appoggiatura));
                            invalid = invalid || (note.notation_head &&
                                                  static_cast<std::uint8_t>(*note.notation_head) >
                                                      static_cast<std::uint8_t>(NoteHeadType::Cue));
                            if (note.ornament) {
                                invalid =
                                    invalid ||
                                    static_cast<std::uint8_t>(note.ornament->type) >
                                        static_cast<std::uint8_t>(OrnamentType::Arpeggio) ||
                                    static_cast<std::uint8_t>(note.ornament->arpeggio_direction) >
                                        static_cast<std::uint8_t>(ArpeggioDirection::None);
                            }
                            for (const auto& technical : note.technical) {
                                invalid =
                                    invalid ||
                                    static_cast<std::uint8_t>(technical.type) >
                                        static_cast<std::uint8_t>(
                                            TechnicalDirection::Type::Caesura) ||
                                    static_cast<std::uint8_t>(technical.slide_direction) >
                                        static_cast<std::uint8_t>(SlideDirection::Descending) ||
                                    static_cast<std::uint8_t>(technical.vibrato_speed) >
                                        static_cast<std::uint8_t>(VibratoSpeed::None);
                            }
                            for (const auto& lyric : note.lyrics) {
                                invalid =
                                    invalid || lyric.text.empty() || lyric.verse == 0 ||
                                    static_cast<std::uint8_t>(lyric.syllabic) >
                                        static_cast<std::uint8_t>(LyricSyllabic::End) ||
                                    (lyric.extend && lyric.syllabic != LyricSyllabic::Single &&
                                     lyric.syllabic != LyricSyllabic::End);
                            }
                            if (invalid) {
                                event_error("Note contains an out-of-domain pitch, velocity, enum, "
                                            "ornament, technical, or lyric payload",
                                            location,
                                            part.id);
                            }
                        }
                    } else if (const auto* chord = std::get_if<ChordSymbolEvent>(&event.payload)) {
                        bool invalid = !valid_pitch(chord->root) || chord->quality.empty() ||
                                       (chord->bass && !valid_pitch(*chord->bass)) ||
                                       (chord->roman && chord->roman->empty());
                        for (const auto& extension : chord->extensions)
                            invalid = invalid || extension.empty();
                        for (const auto& degree : chord->degrees) {
                            invalid = invalid || degree.value == 0 ||
                                      static_cast<std::uint8_t>(degree.type) >
                                          static_cast<std::uint8_t>(ChordDegreeType::Subtract);
                        }
                        if (chord->numeral) {
                            invalid =
                                invalid || chord->numeral->root < 1 || chord->numeral->root > 7 ||
                                chord->numeral->key.fifths < -7 || chord->numeral->key.fifths > 7 ||
                                static_cast<std::uint8_t>(chord->numeral->key.mode) >
                                    static_cast<std::uint8_t>(ChordNumeralMode::HarmonicMinor);

                            if (!invalid) {
                                const auto expected = derive_chord_numeral_root(*chord->numeral);
                                invalid = !expected || chord->root.letter != expected->letter ||
                                          chord->root.accidental != expected->accidental;
                            }
                        }
                        if (invalid)
                            event_error(
                                "ChordSymbol contains an empty or out-of-domain payload, or its "
                                "root contradicts its structured numeral",
                                location,
                                part.id);
                    }
                }
            }
        }
    }
}

// =============================================================================
// S26: Verse-specific lyric ownership and sequence topology
// =============================================================================

void validate_s26(const Score& score, std::vector<Diagnostic>& out) {
    const auto lyric_error = [&](const std::string& message, ScoreTime location, PartId part_id) {
        out.push_back(make_diagnostic(ValidationSeverity::Error,
                                      "S26",
                                      message,
                                      ErrorCode::InvariantViolation,
                                      location,
                                      part_id));
    };

    struct VerseState {
        bool word_open = false;
        std::optional<ScoreTime> extend_start;
        bool extend_has_following_note = false;
    };

    for (const auto& part : score.parts) {
        using Lane = std::pair<std::uint8_t, std::uint8_t>; // staff, voice
        std::map<Lane, std::map<std::uint16_t, VerseState>> lanes;

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                auto& verse_states = lanes[{voice.staff_index, voice.voice_index}];
                for (const auto& event : voice.events) {
                    const auto* group = event.as_note_group();
                    if (!group || group->notes.empty()) continue;
                    const ScoreTime location{measure.bar_number, event.offset};

                    for (std::size_t note_index = 1; note_index < group->notes.size();
                         ++note_index) {
                        if (!group->notes[note_index].lyrics.empty())
                            lyric_error("Only the first Note in a NoteGroup may own lyric "
                                        "syllables",
                                        location,
                                        part.id);
                    }

                    const auto& lyrics = group->notes.front().lyrics;
                    const bool grace_group = std::ranges::any_of(
                        group->notes, [](const Note& note) { return note.grace.has_value(); });
                    if (grace_group) {
                        if (!lyrics.empty())
                            lyric_error(
                                "Lyrics may attach only to ordinary NoteGroups", location, part.id);
                        continue;
                    }

                    std::set<std::uint16_t> present_verses;
                    std::uint16_t previous_verse = 0;
                    for (const auto& lyric : lyrics) {
                        if (!present_verses.insert(lyric.verse).second ||
                            lyric.verse <= previous_verse) {
                            lyric_error("Lyric verses at one onset must be unique and strictly "
                                        "increasing",
                                        location,
                                        part.id);
                        }
                        previous_verse = lyric.verse;
                    }

                    for (auto& [verse, state] : verse_states) {
                        if (state.extend_start && !present_verses.contains(verse))
                            state.extend_has_following_note = true;
                    }

                    for (const auto& lyric : lyrics) {
                        auto& state = verse_states[lyric.verse];
                        if (state.extend_start) {
                            if (!state.extend_has_following_note)
                                lyric_error("An extended lyric syllable must span at least one "
                                            "following ordinary NoteGroup",
                                            *state.extend_start,
                                            part.id);
                            state.extend_start.reset();
                            state.extend_has_following_note = false;
                        }

                        switch (lyric.syllabic) {
                        case LyricSyllabic::Single:
                            if (state.word_open)
                                lyric_error("A single lyric syllable cannot occur inside an open "
                                            "multi-syllable word",
                                            location,
                                            part.id);
                            break;
                        case LyricSyllabic::Begin:
                            if (state.word_open)
                                lyric_error("A lyric word cannot begin before the previous word "
                                            "ends",
                                            location,
                                            part.id);
                            state.word_open = true;
                            break;
                        case LyricSyllabic::Middle:
                            if (!state.word_open)
                                lyric_error("A middle lyric syllable requires an open word",
                                            location,
                                            part.id);
                            break;
                        case LyricSyllabic::End:
                            if (!state.word_open)
                                lyric_error("An ending lyric syllable requires an open word",
                                            location,
                                            part.id);
                            state.word_open = false;
                            break;
                        }

                        if (lyric.extend) {
                            state.extend_start = location;
                            state.extend_has_following_note = false;
                        }
                    }
                }
            }
        }

        for (const auto& [lane, verses] : lanes) {
            (void)lane;
            for (const auto& [verse, state] : verses) {
                (void)verse;
                if (state.word_open)
                    lyric_error("A multi-syllable lyric word has no ending syllable",
                                ScoreTime{score.metadata.total_bars, Beat::zero()},
                                part.id);
                if (state.extend_start && !state.extend_has_following_note)
                    lyric_error("An extended lyric syllable must span at least one following "
                                "ordinary NoteGroup",
                                *state.extend_start,
                                part.id);
            }
        }
    }
}

// =============================================================================
// Sorting helper
// =============================================================================

void sort_diagnostics(std::vector<Diagnostic>& diags) {
    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        // Error < Warning < Info
        if (a.severity != b.severity) return a.severity < b.severity;
        // Then by location
        if (a.location && b.location) return *a.location < *b.location;
        return a.location.has_value() && !b.location.has_value();
    });
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

std::vector<Diagnostic> validate_structural(const Score& score) {
    std::vector<Diagnostic> diags;

    if (score.parts.empty()) {
        diags.push_back(make_diagnostic(ValidationSeverity::Error,
                                        "S0",
                                        "Score must have at least one part",
                                        ErrorCode::MissingParts));
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
    validate_s13(score, diags);
    validate_s14(score, diags);
    validate_s15(score, diags);
    validate_s16(score, diags);
    validate_s17(score, diags);
    validate_s18(score, diags);
    validate_s19(score, diags);
    validate_s20(score, diags);
    validate_s21(score, diags);
    validate_s22(score, diags);
    validate_s23(score, diags);
    validate_s24(score, diags);
    validate_s25(score, diags);
    validate_s26(score, diags);
    validate_s27(score, diags);
    validate_s28(score, diags);

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
    validate_r6(score, diags);
    validate_r7(score, diags);

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
    return std::none_of(diags.begin(), diags.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

Result<void> validate_articulation_mapping(const ArticulationMapping& mapping) {
    std::size_t nodes = 0;
    if (!valid_articulation_mapping(mapping, 1, nodes))
        return std::unexpected(ErrorCode::InvalidRenderingConfig);
    return {};
}

Result<void> validate_rendering_config(const RenderingConfig& rendering) {
    if (rendering.midi_channel < 1 || rendering.midi_channel > 16 ||
        rendering.expression_cc > 127 ||
        (rendering.pan &&
         (!std::isfinite(*rendering.pan) || *rendering.pan < -1.0F || *rendering.pan > 1.0F)))
        return std::unexpected(ErrorCode::InvalidRenderingConfig);

    for (const auto& [articulation, mapping] : rendering.articulation_map) {
        if (static_cast<std::uint8_t>(articulation) >
                static_cast<std::uint8_t>(ArticulationType::BendDown) ||
            !validate_articulation_mapping(mapping))
            return std::unexpected(ErrorCode::InvalidRenderingConfig);
    }
    return {};
}

} // namespace sunny::core
