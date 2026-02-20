/**
 * @file SIVW001A.cpp
 * @brief Score IR view system — implementation
 *
 * Component: SIVW001A
 * Domain: SI (Score IR) | Category: VW (View)
 */

#include "SIVW001A.h"
#include "SITM001A.h"

#include <algorithm>
#include <map>
#include <set>

namespace Sunny::Core {

namespace {

/// Copy global maps and metadata from source score
Score copy_score_skeleton(const Score& source) {
    Score result;
    result.id = ScoreId{source.id.value + 10000};  // Derived id
    result.metadata = source.metadata;
    result.tempo_map = source.tempo_map;
    result.key_map = source.key_map;
    result.time_map = source.time_map;
    result.section_map = source.section_map;
    result.rehearsal_marks = source.rehearsal_marks;
    result.harmonic_annotations = source.harmonic_annotations;
    result.version = 1;
    return result;
}

/// Get the active time signature at a given bar
const TimeSignatureEntry* find_time_sig(
    const TimeSignatureMap& time_map, std::uint32_t bar
) {
    const TimeSignatureEntry* active = nullptr;
    for (const auto& e : time_map) {
        if (e.bar <= bar) active = &e;
        else break;
    }
    return active;
}

/// Collect all notes from a measure across all voices
struct CollectedNote {
    Note note;
    Beat offset;
    Beat duration;
};

std::vector<CollectedNote> collect_notes_from_measure(const Measure& measure) {
    std::vector<CollectedNote> notes;
    for (const auto& voice : measure.voices) {
        for (const auto& event : voice.events) {
            const auto* ng = event.as_note_group();
            if (!ng) continue;
            for (const auto& note : ng->notes) {
                notes.push_back({note, event.offset, ng->duration});
            }
        }
    }
    return notes;
}

std::uint64_t g_view_event_id = 2000000;

EventId next_view_id() {
    return EventId{g_view_event_id++};
}

/// Build a Voice from collected notes, grouping simultaneous notes into chords
/// and filling gaps with rests. The notes vector is sorted by offset internally.
Voice build_voice_from_collected(
    std::vector<CollectedNote>& notes,
    Beat measure_dur,
    std::uint8_t voice_index
) {
    Voice voice{voice_index, {}, {}};

    if (notes.empty()) {
        voice.events.push_back(
            Event{next_view_id(), Beat::zero(), RestEvent{measure_dur, true}});
        return voice;
    }

    std::sort(notes.begin(), notes.end(),
        [](const CollectedNote& a, const CollectedNote& b) {
            return a.offset < b.offset;
        });

    Beat current = Beat::zero();
    std::size_t i = 0;
    while (i < notes.size()) {
        Beat off = notes[i].offset;
        if (current < off) {
            voice.events.push_back(
                Event{next_view_id(), current,
                      RestEvent{off - current, true}});
            current = off;
        }
        NoteGroup ng;
        Beat dur = notes[i].duration;
        while (i < notes.size() && notes[i].offset == off) {
            ng.notes.push_back(notes[i].note);
            dur = notes[i].duration;
            ++i;
        }
        ng.duration = dur;
        voice.events.push_back(
            Event{next_view_id(), current, std::move(ng)});
        current = current + dur;
    }
    if (current < measure_dur) {
        voice.events.push_back(
            Event{next_view_id(), current,
                  RestEvent{measure_dur - current, true}});
    }

    return voice;
}

/// Display name for an InstrumentFamily.
std::string family_display_name(InstrumentFamily f) {
    switch (f) {
        case InstrumentFamily::Strings:    return "Strings";
        case InstrumentFamily::Woodwinds:  return "Woodwinds";
        case InstrumentFamily::Brass:      return "Brass";
        case InstrumentFamily::Percussion: return "Percussion";
        case InstrumentFamily::Keyboard:   return "Keyboard";
        case InstrumentFamily::Voice:      return "Voices";
        case InstrumentFamily::Electronic: return "Electronic";
    }
    return "Other";
}

/// Check if any measure in a part contains note events.
bool part_has_notes(const Part& part) {
    for (const auto& m : part.measures) {
        for (const auto& v : m.voices) {
            for (const auto& e : v.events) {
                if (e.is_note_group()) return true;
            }
        }
    }
    return false;
}

}  // anonymous namespace

// =============================================================================
// piano_reduction
// =============================================================================

Score piano_reduction(const Score& score) {
    Score result = copy_score_skeleton(score);

    Part piano;
    piano.id = PartId{900000};
    piano.definition.name = "Piano Reduction";
    piano.definition.abbreviation = "Pno.";
    piano.definition.instrument_type = InstrumentType::Piano;
    piano.definition.staff_count = 2;
    piano.definition.clef = Clef::Treble;
    piano.definition.range = PitchRange{
        SpelledPitch{5, 0, 0}, SpelledPitch{0, 0, 8},
        SpelledPitch{0, 0, 2}, SpelledPitch{0, 0, 7}
    };

    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        const auto* ts_entry = find_time_sig(score.time_map, bar);
        Beat measure_dur = ts_entry
            ? ts_entry->time_signature.measure_duration()
            : Beat{1, 1};

        // Collect notes from all parts at this bar
        std::vector<CollectedNote> treble_notes;
        std::vector<CollectedNote> bass_notes;

        for (const auto& part : score.parts) {
            if (bar - 1 >= part.measures.size()) continue;
            auto collected = collect_notes_from_measure(part.measures[bar - 1]);
            for (const auto& cn : collected) {
                int mv = midi_value(cn.note.pitch);
                if (mv >= 60) {
                    treble_notes.push_back(cn);
                } else {
                    bass_notes.push_back(cn);
                }
            }
        }

        Voice treble_voice = build_voice_from_collected(treble_notes, measure_dur, 0);
        Voice bass_voice = build_voice_from_collected(bass_notes, measure_dur, 1);

        Measure measure{bar, {treble_voice, bass_voice},
                        std::nullopt, std::nullopt};
        piano.measures.push_back(std::move(measure));
    }

    result.parts.push_back(std::move(piano));
    return result;
}

// =============================================================================
// short_score
// =============================================================================

Score short_score(const Score& score) {
    Score result = copy_score_skeleton(score);

    // Group parts by instrument family
    std::map<InstrumentFamily, std::vector<const Part*>> family_groups;
    for (const auto& part : score.parts) {
        auto family = instrument_family(part.definition.instrument_type);
        family_groups[family].push_back(&part);
    }

    std::uint64_t part_id_counter = 900001;

    for (auto& [family, parts] : family_groups) {
        // Skip families with no note content
        bool has_notes = false;
        for (const auto* part : parts) {
            if (part_has_notes(*part)) { has_notes = true; break; }
        }
        if (!has_notes) continue;

        Part family_part;
        family_part.id = PartId{part_id_counter++};
        family_part.definition.name = family_display_name(family);
        family_part.definition.abbreviation =
            family_display_name(family).substr(0, 4) + ".";
        family_part.definition.instrument_type = parts[0]->definition.instrument_type;
        family_part.definition.staff_count = 2;

        for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
            const auto* ts_entry = find_time_sig(score.time_map, bar);
            Beat measure_dur = ts_entry
                ? ts_entry->time_signature.measure_duration()
                : Beat{1, 1};

            std::vector<CollectedNote> treble_notes;
            std::vector<CollectedNote> bass_notes;

            for (const auto* part : parts) {
                if (bar - 1 >= part->measures.size()) continue;
                auto collected = collect_notes_from_measure(part->measures[bar - 1]);
                for (const auto& cn : collected) {
                    int mv = midi_value(cn.note.pitch);
                    if (mv >= 60) {
                        treble_notes.push_back(cn);
                    } else {
                        bass_notes.push_back(cn);
                    }
                }
            }

            Voice treble = build_voice_from_collected(treble_notes, measure_dur, 0);
            Voice bass = build_voice_from_collected(bass_notes, measure_dur, 1);

            Measure measure{bar, {treble, bass}, std::nullopt, std::nullopt};
            family_part.measures.push_back(std::move(measure));
        }

        result.parts.push_back(std::move(family_part));
    }

    // Score invariant: at least one Part
    if (result.parts.empty()) {
        Part empty;
        empty.id = PartId{part_id_counter};
        empty.definition.name = "Empty";
        empty.definition.abbreviation = "Emp.";
        empty.definition.instrument_type = InstrumentType::Custom;

        for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
            const auto* ts_entry = find_time_sig(score.time_map, bar);
            Beat measure_dur = ts_entry
                ? ts_entry->time_signature.measure_duration()
                : Beat{1, 1};

            RestEvent rest{measure_dur, true};
            Event event{next_view_id(), Beat::zero(), rest};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            empty.measures.push_back(measure);
        }
        result.parts.push_back(std::move(empty));
    }

    return result;
}

// =============================================================================
// part_extract
// =============================================================================

Score part_extract(const Score& score, PartId part_id) {
    Score result = copy_score_skeleton(score);

    for (const auto& part : score.parts) {
        if (part.id == part_id) {
            result.parts.push_back(part);

            // Filter orchestration annotations to this part
            result.orchestration_annotations.clear();
            for (const auto& ann : score.orchestration_annotations) {
                if (ann.part_id == part_id) {
                    result.orchestration_annotations.push_back(ann);
                }
            }
            return result;
        }
    }

    // Part not found — return skeleton with empty part
    Part empty;
    empty.id = part_id;
    empty.definition.name = "Unknown";
    empty.definition.abbreviation = "?";
    empty.definition.instrument_type = InstrumentType::Custom;
    result.parts.push_back(std::move(empty));
    return result;
}

// =============================================================================
// harmonic_skeleton
// =============================================================================

Score harmonic_skeleton(const Score& score) {
    Score result = copy_score_skeleton(score);

    // Build a set of (event_id, note_index) pairs that are non-chord tones
    std::set<std::pair<std::uint64_t, std::uint8_t>> nct_set;
    for (const auto& ha : score.harmonic_annotations) {
        for (const auto& nct : ha.non_chord_tones) {
            nct_set.insert({nct.event_id.value, nct.note_index});
        }
    }

    for (const auto& part : score.parts) {
        Part new_part;
        new_part.id = part.id;
        new_part.definition = part.definition;
        new_part.part_directives = part.part_directives;
        new_part.hairpins = part.hairpins;

        for (const auto& measure : part.measures) {
            Measure new_measure{measure.bar_number, {}, measure.local_key,
                                measure.local_time};

            for (const auto& voice : measure.voices) {
                Voice new_voice{voice.voice_index, {}, voice.beam_groups};

                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) {
                        new_voice.events.push_back(event);
                        continue;
                    }

                    // Filter: keep only chord tones, remove grace notes
                    NoteGroup new_ng;
                    new_ng.duration = ng->duration;
                    new_ng.tuplet_context = ng->tuplet_context;
                    new_ng.beam_group = ng->beam_group;
                    new_ng.slur_start = ng->slur_start;
                    new_ng.slur_end = ng->slur_end;

                    for (std::uint8_t ni = 0; ni < ng->notes.size(); ++ni) {
                        const auto& note = ng->notes[ni];
                        // Skip grace notes
                        if (note.grace) continue;
                        // Skip non-chord tones
                        if (nct_set.contains({event.id.value, ni})) continue;
                        // Strip ornaments
                        Note clean = note;
                        clean.ornament = std::nullopt;
                        new_ng.notes.push_back(std::move(clean));
                    }

                    if (new_ng.notes.empty()) {
                        // All notes removed — convert to rest
                        new_voice.events.push_back(
                            Event{event.id, event.offset,
                                  RestEvent{ng->duration, true}});
                    } else {
                        new_voice.events.push_back(
                            Event{event.id, event.offset, std::move(new_ng)});
                    }
                }

                new_measure.voices.push_back(std::move(new_voice));
            }

            new_part.measures.push_back(std::move(new_measure));
        }

        result.parts.push_back(std::move(new_part));
    }

    return result;
}

// =============================================================================
// region_view
// =============================================================================

Score region_view(const Score& score, const ScoreRegion& region) {
    std::uint32_t start_bar = region.start.bar;
    std::uint32_t end_bar = region.end.bar;
    // Include the end bar if the beat offset is non-zero
    if (region.end.beat > Beat::zero()) {
        end_bar = region.end.bar;
    } else if (end_bar > start_bar) {
        end_bar = end_bar - 1;
    }

    std::uint32_t num_bars = (end_bar >= start_bar) ? (end_bar - start_bar + 1) : 0;
    if (num_bars == 0) num_bars = 1;

    Score result = copy_score_skeleton(score);
    result.metadata.total_bars = num_bars;

    // Adjust tempo/key/time maps to the region
    result.tempo_map.clear();
    for (const auto& te : score.tempo_map) {
        if (te.position.bar >= start_bar && te.position.bar <= end_bar) {
            TempoEvent adj = te;
            adj.position.bar = te.position.bar - start_bar + 1;
            result.tempo_map.push_back(adj);
        }
    }
    if (result.tempo_map.empty() && !score.tempo_map.empty()) {
        // Carry forward the last tempo before the region
        for (auto it = score.tempo_map.rbegin(); it != score.tempo_map.rend(); ++it) {
            if (it->position.bar <= start_bar) {
                TempoEvent adj = *it;
                adj.position = SCORE_START;
                result.tempo_map.push_back(adj);
                break;
            }
        }
    }

    result.key_map.clear();
    for (const auto& ke : score.key_map) {
        if (ke.position.bar >= start_bar && ke.position.bar <= end_bar) {
            KeySignatureEntry adj = ke;
            adj.position.bar = ke.position.bar - start_bar + 1;
            result.key_map.push_back(adj);
        }
    }
    if (result.key_map.empty() && !score.key_map.empty()) {
        for (auto it = score.key_map.rbegin(); it != score.key_map.rend(); ++it) {
            if (it->position.bar <= start_bar) {
                KeySignatureEntry adj = *it;
                adj.position = SCORE_START;
                result.key_map.push_back(adj);
                break;
            }
        }
    }

    result.time_map.clear();
    for (const auto& te : score.time_map) {
        if (te.bar >= start_bar && te.bar <= end_bar) {
            TimeSignatureEntry adj = te;
            adj.bar = te.bar - start_bar + 1;
            result.time_map.push_back(adj);
        }
    }
    if (result.time_map.empty() && !score.time_map.empty()) {
        for (auto it = score.time_map.rbegin(); it != score.time_map.rend(); ++it) {
            if (it->bar <= start_bar) {
                TimeSignatureEntry adj = *it;
                adj.bar = 1;
                result.time_map.push_back(adj);
                break;
            }
        }
    }

    result.section_map.clear();

    // Extract parts
    for (const auto& part : score.parts) {
        if (!region.parts.empty()) {
            bool found = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) { found = true; break; }
            }
            if (!found) continue;
        }

        Part new_part;
        new_part.id = part.id;
        new_part.definition = part.definition;

        for (std::uint32_t bar = start_bar; bar <= end_bar && bar <= score.metadata.total_bars; ++bar) {
            if (bar - 1 < part.measures.size()) {
                Measure m = part.measures[bar - 1];
                m.bar_number = bar - start_bar + 1;
                new_part.measures.push_back(std::move(m));
            }
        }

        result.parts.push_back(std::move(new_part));
    }

    return result;
}

}  // namespace Sunny::Core
