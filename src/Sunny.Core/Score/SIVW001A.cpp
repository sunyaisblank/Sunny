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

        // Build treble voice (index 0)
        Voice treble_voice{0, {}, {}};
        if (treble_notes.empty()) {
            treble_voice.events.push_back(
                Event{next_view_id(), Beat::zero(), RestEvent{measure_dur, true}});
        } else {
            // Group notes by offset
            std::sort(treble_notes.begin(), treble_notes.end(),
                [](const CollectedNote& a, const CollectedNote& b) {
                    return a.offset < b.offset;
                });

            Beat current = Beat::zero();
            std::size_t i = 0;
            while (i < treble_notes.size()) {
                Beat off = treble_notes[i].offset;
                if (current < off) {
                    treble_voice.events.push_back(
                        Event{next_view_id(), current,
                              RestEvent{off - current, true}});
                    current = off;
                }
                NoteGroup ng;
                Beat dur = treble_notes[i].duration;
                while (i < treble_notes.size() && treble_notes[i].offset == off) {
                    ng.notes.push_back(treble_notes[i].note);
                    dur = treble_notes[i].duration;
                    ++i;
                }
                ng.duration = dur;
                treble_voice.events.push_back(
                    Event{next_view_id(), current, std::move(ng)});
                current = current + dur;
            }
            if (current < measure_dur) {
                treble_voice.events.push_back(
                    Event{next_view_id(), current,
                          RestEvent{measure_dur - current, true}});
            }
        }

        // Build bass voice (index 1)
        Voice bass_voice{1, {}, {}};
        if (bass_notes.empty()) {
            bass_voice.events.push_back(
                Event{next_view_id(), Beat::zero(), RestEvent{measure_dur, true}});
        } else {
            std::sort(bass_notes.begin(), bass_notes.end(),
                [](const CollectedNote& a, const CollectedNote& b) {
                    return a.offset < b.offset;
                });

            Beat current = Beat::zero();
            std::size_t i = 0;
            while (i < bass_notes.size()) {
                Beat off = bass_notes[i].offset;
                if (current < off) {
                    bass_voice.events.push_back(
                        Event{next_view_id(), current,
                              RestEvent{off - current, true}});
                    current = off;
                }
                NoteGroup ng;
                Beat dur = bass_notes[i].duration;
                while (i < bass_notes.size() && bass_notes[i].offset == off) {
                    ng.notes.push_back(bass_notes[i].note);
                    dur = bass_notes[i].duration;
                    ++i;
                }
                ng.duration = dur;
                bass_voice.events.push_back(
                    Event{next_view_id(), current, std::move(ng)});
                current = current + dur;
            }
            if (current < measure_dur) {
                bass_voice.events.push_back(
                    Event{next_view_id(), current,
                          RestEvent{measure_dur - current, true}});
            }
        }

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

    Part satb;
    satb.id = PartId{900001};
    satb.definition.name = "SATB";
    satb.definition.abbreviation = "SATB";
    satb.definition.instrument_type = InstrumentType::SopranoVoice;
    satb.definition.staff_count = 2;
    satb.definition.range = PitchRange{
        SpelledPitch{0, 0, 2}, SpelledPitch{0, 0, 6},
        SpelledPitch{0, 0, 3}, SpelledPitch{0, 0, 5}
    };

    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        const auto* ts_entry = find_time_sig(score.time_map, bar);
        Beat measure_dur = ts_entry
            ? ts_entry->time_signature.measure_duration()
            : Beat{1, 1};

        // Collect all notes
        std::vector<CollectedNote> all_notes;
        for (const auto& part : score.parts) {
            if (bar - 1 >= part.measures.size()) continue;
            auto collected = collect_notes_from_measure(part.measures[bar - 1]);
            all_notes.insert(all_notes.end(), collected.begin(), collected.end());
        }

        // Sort by pitch descending for voice assignment
        std::sort(all_notes.begin(), all_notes.end(),
            [](const CollectedNote& a, const CollectedNote& b) {
                return midi_value(a.note.pitch) > midi_value(b.note.pitch);
            });

        // Distribute to 4 voices: S(0), A(1), T(2), B(3)
        std::array<std::vector<CollectedNote>, 4> voice_notes;
        for (std::size_t i = 0; i < all_notes.size(); ++i) {
            voice_notes[i % 4].push_back(all_notes[i]);
        }

        std::vector<Voice> voices;
        for (std::uint8_t vi = 0; vi < 4; ++vi) {
            Voice voice{vi, {}, {}};
            if (voice_notes[vi].empty()) {
                voice.events.push_back(
                    Event{next_view_id(), Beat::zero(),
                          RestEvent{measure_dur, true}});
            } else {
                // Sort by offset for event creation
                auto& vn = voice_notes[vi];
                std::sort(vn.begin(), vn.end(),
                    [](const CollectedNote& a, const CollectedNote& b) {
                        return a.offset < b.offset;
                    });

                Beat current = Beat::zero();
                std::size_t j = 0;
                while (j < vn.size()) {
                    Beat off = vn[j].offset;
                    if (current < off) {
                        voice.events.push_back(
                            Event{next_view_id(), current,
                                  RestEvent{off - current, true}});
                        current = off;
                    }
                    NoteGroup ng;
                    ng.duration = vn[j].duration;
                    ng.notes.push_back(vn[j].note);
                    ++j;
                    voice.events.push_back(
                        Event{next_view_id(), current, std::move(ng)});
                    current = current + ng.duration;
                }
                if (current < measure_dur) {
                    voice.events.push_back(
                        Event{next_view_id(), current,
                              RestEvent{measure_dur - current, true}});
                }
            }
            voices.push_back(std::move(voice));
        }

        Measure measure{bar, std::move(voices), std::nullopt, std::nullopt};
        satb.measures.push_back(std::move(measure));
    }

    result.parts.push_back(std::move(satb));
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
