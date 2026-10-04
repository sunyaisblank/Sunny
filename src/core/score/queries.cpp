/**
 * @file queries.cpp
 * @brief Score IR agent interface queries — implementation
 *
 */

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <sunny/core/score/projection.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/time.hpp>

namespace sunny::core {

namespace {

/// Check if a part id is within a region's parts list (empty = all)
bool part_in_region(PartId pid, const std::vector<PartId>& parts) {
    if (parts.empty()) return true;
    for (const auto& p : parts) {
        if (p == pid) return true;
    }
    return false;
}

/// Check if a bar falls within a region
bool bar_in_region(std::uint32_t bar, const ScoreRegion& region) {
    ScoreTime bar_start{bar, Beat::zero()};
    ScoreTime bar_end{bar + 1, Beat::zero()};
    return !(bar_end <= region.start || bar_start >= region.end);
}

/// Check if a specific event time falls within a region
bool time_in_region(ScoreTime time, const ScoreRegion& region) {
    return time >= region.start && time < region.end;
}

/// Find deepest matching section recursively
const ScoreSection* find_section_at(const std::vector<ScoreSection>& sections, ScoreTime time) {
    for (const auto& sec : sections) {
        if (time >= sec.start && time < sec.end) {
            // Check children for more specific match
            const auto* child = find_section_at(sec.children, time);
            return child ? child : &sec;
        }
    }
    return nullptr;
}

} // anonymous namespace

// =============================================================================
// query_notes_in_range
// =============================================================================

std::vector<LocatedEvent> query_notes_in_range(const Score& score, const ScoreRegion& region) {
    std::vector<LocatedEvent> result;

    for (const auto& part : score.parts) {
        if (!part_in_region(part.id, region.parts)) continue;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (!event.is_note_group()) continue;
                    ScoreTime etime{measure.bar_number, event.offset};
                    if (time_in_region(etime, region)) {
                        result.push_back({etime, part.id, &event});
                    }
                }
            }
        }
    }

    return result;
}

// =============================================================================
// query_harmony_at
// =============================================================================

std::optional<HarmonicAnnotation> query_harmony_at(const Score& score, ScoreTime time) {
    auto absolute = score_time_to_absolute_beat(time, score.time_map);
    if (!absolute) return std::nullopt;
    const HarmonicAnnotation* best = nullptr;
    for (const auto& annotation : score.harmonic_annotations) {
        auto start = score_time_to_absolute_beat(annotation.position, score.time_map);
        if (!start) continue;
        auto end = checked_add(*start, annotation.duration);
        if (end && *start <= *absolute && *absolute < *end) best = &annotation;
    }
    if (best) return *best;
    return std::nullopt;
}

// =============================================================================
// query_tempo_at
// =============================================================================

std::optional<TempoEvent> query_tempo_at(const Score& score, ScoreTime time) {
    std::optional<TempoEvent> result;
    for (const auto& te : score.tempo_map) {
        if (te.position <= time) {
            result = te;
        } else {
            break;
        }
    }
    return result;
}

// =============================================================================
// query_key_at
// =============================================================================

std::optional<KeySignature> query_key_at(const Score& score, ScoreTime time) {
    std::optional<KeySignature> result;
    for (const auto& ke : score.key_map) {
        if (ke.position <= time) {
            result = ke.key;
        } else {
            break;
        }
    }
    return result;
}

// =============================================================================
// query_dynamics_at
// =============================================================================

std::optional<DynamicLevel> query_dynamics_at(const Part& part, ScoreTime time) {
    std::optional<DynamicLevel> result;
    std::optional<ScoreTime> latest;

    for (const auto& measure : part.measures) {
        if (measure.bar_number > time.bar) break;
        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                const ScoreTime event_time{measure.bar_number, event.offset};
                if (event_time > time) break;
                if (latest && event_time < *latest) continue;
                const auto* group = event.as_note_group();
                if (!group) continue;
                for (const auto& note : group->notes) {
                    const auto marking = note.dynamic ? note.dynamic : note.velocity.written;
                    if (marking) {
                        result = marking;
                        latest = event_time;
                    }
                }
            }
        }
    }
    return result;
}

std::optional<DynamicLevel> query_dynamics_at(const Score& score, PartId part_id, ScoreTime time) {
    for (const auto& part : score.parts)
        if (part.id == part_id) return query_dynamics_at(part, time);
    return std::nullopt;
}

// =============================================================================
// query_parts_playing_at
// =============================================================================

std::vector<PartId> query_parts_playing_at(const Score& score, ScoreTime time) {
    std::vector<PartId> result;
    auto absolute = score_time_to_absolute_beat(time, score.time_map);
    auto notes = project_symbolic_notes(score);
    if (!absolute || !notes) return result;
    for (const auto& note : *notes) {
        if (note.start <= *absolute && *absolute < note.end &&
            std::find(result.begin(), result.end(), note.part_id) == result.end())
            result.push_back(note.part_id);
    }
    return result;
}

// =============================================================================
// query_texture_at
// =============================================================================

std::optional<TextureType> query_texture_at(const Score& score, ScoreTime time) {
    for (const auto& ann : score.orchestration_annotations) {
        if (ann.start <= time && time < ann.end && ann.texture) {
            return ann.texture;
        }
    }
    return std::nullopt;
}

// =============================================================================
// query_voice_count
// =============================================================================

std::uint8_t query_voice_count(const Score& score, std::uint32_t bar, PartId part_id) {
    for (const auto& part : score.parts) {
        if (part.id != part_id) continue;
        if (bar < 1 || bar > part.measures.size()) return 0;
        return static_cast<std::uint8_t>(part.measures[bar - 1].voices.size());
    }
    return 0;
}

// =============================================================================
// query_pitch_range
// =============================================================================

std::optional<std::pair<SpelledPitch, SpelledPitch>>
query_pitch_range(const Score& score, PartId part_id, const ScoreRegion& region) {
    int lowest = std::numeric_limits<int>::max();
    int highest = std::numeric_limits<int>::min();
    SpelledPitch lowest_sp{};
    SpelledPitch highest_sp{};
    bool found = false;

    for (const auto& part : score.parts) {
        if (part.id != part_id) continue;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    ScoreTime etime{measure.bar_number, event.offset};
                    if (!time_in_region(etime, region)) continue;

                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        if (mv < lowest) {
                            lowest = mv;
                            lowest_sp = note.pitch;
                        }
                        if (mv > highest) {
                            highest = mv;
                            highest_sp = note.pitch;
                        }
                        found = true;
                    }
                }
            }
        }
        break;
    }

    if (!found) return std::nullopt;
    return std::make_pair(lowest_sp, highest_sp);
}

// =============================================================================
// query_duration_total
// =============================================================================

Beat query_duration_total(const Score& score, PartId part_id, const ScoreRegion& region) {
    Beat total = Beat::zero();

    for (const auto& part : score.parts) {
        if (part.id != part_id) continue;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (!event.is_note_group()) continue;
                    ScoreTime etime{measure.bar_number, event.offset};
                    if (time_in_region(etime, region)) {
                        total = total + event.duration();
                    }
                }
            }
        }
        break;
    }

    return total;
}

// =============================================================================
// query_note_density
// =============================================================================

double query_note_density(const Score& score, const ScoreRegion& region) {
    int note_count = 0;
    int bar_count = 0;

    for (const auto& part : score.parts) {
        if (!part_in_region(part.id, region.parts)) continue;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            bool counted = false;
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    ScoreTime etime{measure.bar_number, event.offset};
                    if (time_in_region(etime, region)) {
                        note_count += static_cast<int>(ng->notes.size());
                        if (!counted) {
                            counted = true;
                        }
                    }
                }
            }
        }
    }

    // Count bars in region
    if (region.end.bar > region.start.bar) {
        bar_count = static_cast<int>(region.end.bar - region.start.bar);
        if (region.end.beat > Beat::zero()) ++bar_count;
    } else {
        bar_count = 1;
    }

    if (bar_count == 0) return 0.0;
    return static_cast<double>(note_count) / static_cast<double>(bar_count);
}

// =============================================================================
// query_section_at
// =============================================================================

std::optional<ScoreSection> query_section_at(const Score& score, ScoreTime time) {
    const auto* sec = find_section_at(score.section_map, time);
    if (sec) return *sec;
    return std::nullopt;
}

// =============================================================================
// query_diagnostics
// =============================================================================

std::vector<Diagnostic> query_diagnostics(const Score& score) {
    return validate_score(score);
}

// =============================================================================
// query_harmony_range
// =============================================================================

std::vector<HarmonicAnnotation>
query_harmony_range(const Score& score, ScoreTime start, ScoreTime end) {
    std::vector<HarmonicAnnotation> result;
    auto range_start = score_time_to_absolute_beat(start, score.time_map);
    auto range_end = score_time_to_absolute_beat(end, score.time_map);
    if (!range_start || !range_end || *range_end <= *range_start) return result;
    for (const auto& annotation : score.harmonic_annotations) {
        auto annotation_start = score_time_to_absolute_beat(annotation.position, score.time_map);
        if (!annotation_start) continue;
        auto annotation_end = checked_add(*annotation_start, annotation.duration);
        if (annotation_end && *annotation_start < *range_end && *annotation_end > *range_start)
            result.push_back(annotation);
    }
    return result;
}

// =============================================================================
// query_melody_for
// =============================================================================

std::vector<MelodyNote>
query_melody_for(const Score& score, PartId part_id, const ScoreRegion& region) {
    std::vector<MelodyNote> result;
    auto spans = project_symbolic_notes(score);
    if (!spans) return result;
    auto notes = fold_symbolic_ties(*spans);
    if (!notes) return result;
    std::map<EventId, const SymbolicNoteSpan*> highest_attacks;
    for (const auto& note : *notes) {
        if (note.part_id != part_id || note.voice_index != 0 ||
            !time_in_region(note.source_position, region))
            continue;
        auto [it, inserted] = highest_attacks.try_emplace(note.event_id, &note);
        if (!inserted && midi_value(note.pitch) > midi_value(it->second->pitch)) it->second = &note;
    }
    for (const auto& [event_id, note] : highest_attacks) {
        (void)event_id;
        auto duration = checked_sub(note->end, note->start);
        if (duration) result.push_back({note->pitch, *duration, note->source_position});
    }
    std::sort(result.begin(), result.end(), [](const MelodyNote& a, const MelodyNote& b) {
        return a.position < b.position;
    });
    return result;
}

// =============================================================================
// query_chord_progression
// =============================================================================

std::vector<ChordEntry> query_chord_progression(const Score& score, const ScoreRegion& region) {
    std::vector<ChordEntry> result;

    for (const auto& ha : score.harmonic_annotations) {
        if (ha.position >= region.start && ha.position < region.end) {
            result.push_back({ha.position, ha.duration, ha.chord, ha.roman_numeral});
        }
    }

    return result;
}

// =============================================================================
// query_orchestration
// =============================================================================

std::vector<std::pair<PartId, TexturalRole>> query_orchestration(const Score& score,
                                                                 const ScoreRegion& region) {
    std::vector<std::pair<PartId, TexturalRole>> result;

    for (const auto& ann : score.orchestration_annotations) {
        // Overlaps if annotation starts before region end AND ends after region start
        if (ann.start < region.end && ann.end > region.start) {
            result.emplace_back(ann.part_id, ann.role);
        }
    }

    return result;
}

// =============================================================================
// query_parts_with_role
// =============================================================================

std::vector<PartId>
query_parts_with_role(const Score& score, TexturalRole role, const ScoreRegion& region) {
    std::vector<PartId> result;

    for (const auto& ann : score.orchestration_annotations) {
        if (ann.role != role) continue;
        if (ann.start < region.end && ann.end > region.start) {
            // Avoid duplicates
            bool already = false;
            for (const auto& pid : result) {
                if (pid == ann.part_id) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                result.push_back(ann.part_id);
            }
        }
    }

    return result;
}

// =============================================================================
// query_pitch_content
// =============================================================================

std::vector<PitchClass> query_pitch_content(const Score& score, const ScoreRegion& region) {
    std::set<PitchClass> pcs;

    for (const auto& part : score.parts) {
        if (!part_in_region(part.id, region.parts)) continue;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    ScoreTime etime{measure.bar_number, event.offset};
                    if (!time_in_region(etime, region)) continue;

                    for (const auto& note : ng->notes) {
                        pcs.insert(pc(note.pitch));
                    }
                }
            }
        }
    }

    return {pcs.begin(), pcs.end()};
}

// =============================================================================
// query_time_signature_at
// =============================================================================

TimeSignature query_time_signature_at(const Score& score, std::uint32_t bar) {
    TimeSignature result{{4}, 4}; // Default 4/4
    for (const auto& entry : score.time_map) {
        if (entry.bar <= bar) {
            result = entry.time_signature;
        } else {
            break;
        }
    }
    return result;
}

// =============================================================================
// query_sections
// =============================================================================

std::vector<ScoreSection> query_sections(const Score& score) {
    return score.section_map;
}

// =============================================================================
// query_form_summary
// =============================================================================

std::vector<FormSummaryEntry> query_form_summary(const Score& score) {
    std::vector<FormSummaryEntry> result;

    for (const auto& sec : score.section_map) {
        FormSummaryEntry entry;
        entry.label = sec.label;
        entry.start = sec.start;
        entry.end = sec.end;

        // Find key at section start
        auto key = query_key_at(score, sec.start);
        if (key) entry.key = *key;

        // Find tempo at section start
        auto tempo = effective_quarter_tempo_at(sec.start, score.tempo_map, score.time_map);
        entry.tempo_bpm = tempo ? tempo->to_float() : 0.0;

        result.push_back(std::move(entry));
    }

    return result;
}

// =============================================================================
// query_find_motif
// =============================================================================

std::vector<MotifOccurrence> query_find_motif(const Score& score,
                                              const std::vector<PitchClass>& pattern,
                                              const ScoreRegion& region) {
    std::vector<MotifOccurrence> result;
    if (pattern.empty()) return result;

    for (const auto& part : score.parts) {
        if (!part_in_region(part.id, region.parts)) continue;

        // Collect pitch classes in order from this part within the region
        struct PcWithPos {
            PitchClass pc;
            ScoreTime position;
        };
        std::vector<PcWithPos> pcs;

        for (const auto& measure : part.measures) {
            if (!bar_in_region(measure.bar_number, region)) continue;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng || ng->notes.empty()) continue;

                    ScoreTime etime{measure.bar_number, event.offset};
                    if (!time_in_region(etime, region)) continue;

                    pcs.push_back({pc(ng->notes[0].pitch), etime});
                }
            }
        }

        // Sliding window search for the pattern
        if (pcs.size() < pattern.size()) continue;

        for (std::size_t i = 0; i <= pcs.size() - pattern.size(); ++i) {
            bool match = true;
            for (std::size_t j = 0; j < pattern.size(); ++j) {
                if (pcs[i + j].pc != pattern[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                result.push_back({part.id, pcs[i].position});
            }
        }
    }

    return result;
}

} // namespace sunny::core
