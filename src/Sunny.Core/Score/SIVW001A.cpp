/**
 * @file SIVW001A.cpp
 * @brief Score IR view system — implementation
 *
 * Component: SIVW001A
 * Domain: SI (Score IR) | Category: VW (View)
 */

#include "SIVW001A.h"
#include "SITM001A.h"
#include "SIQR001A.h"

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
    bool is_melody = false;
    bool is_bass = false;
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

/// Remove octave doublings within a staff at each offset.
/// For treble: keep melody-tagged note, else the highest octave per PC.
/// For bass: keep bass-tagged note, else the lowest octave per PC.
void dedup_octaves(std::vector<CollectedNote>& notes, bool is_treble) {
    if (notes.empty()) return;

    // Group by offset, then by pitch class
    std::sort(notes.begin(), notes.end(),
        [](const CollectedNote& a, const CollectedNote& b) {
            if (a.offset != b.offset) return a.offset < b.offset;
            return midi_value(a.note.pitch) < midi_value(b.note.pitch);
        });

    std::vector<CollectedNote> result;
    std::size_t i = 0;
    while (i < notes.size()) {
        Beat current_offset = notes[i].offset;
        // Collect all notes at this offset
        std::size_t j = i;
        while (j < notes.size() && notes[j].offset == current_offset) ++j;

        // Group by pitch class (mod 12)
        std::map<uint8_t, std::vector<std::size_t>> pc_groups;
        for (std::size_t k = i; k < j; ++k) {
            uint8_t pc = static_cast<uint8_t>(midi_value(notes[k].note.pitch) % 12);
            pc_groups[pc].push_back(k);
        }

        for (auto& [pc, indices] : pc_groups) {
            // Prefer melody/bass tagged note
            std::size_t best = indices[0];
            bool found_tagged = false;
            for (auto idx : indices) {
                if (is_treble && notes[idx].is_melody) { best = idx; found_tagged = true; break; }
                if (!is_treble && notes[idx].is_bass) { best = idx; found_tagged = true; break; }
            }
            if (!found_tagged) {
                // Treble: keep highest octave; bass: keep lowest
                for (auto idx : indices) {
                    int mv_best = midi_value(notes[best].note.pitch);
                    int mv_cur = midi_value(notes[idx].note.pitch);
                    if (is_treble ? (mv_cur > mv_best) : (mv_cur < mv_best))
                        best = idx;
                }
            }
            result.push_back(notes[best]);
        }

        i = j;
    }
    notes = std::move(result);
}

/// Limit to at most max_voices simultaneous notes per offset.
/// Preserves melody-tagged (treble) or bass-tagged (bass) notes.
void limit_voices(std::vector<CollectedNote>& notes, bool is_treble, std::uint8_t max_voices = 4) {
    if (notes.empty()) return;

    std::sort(notes.begin(), notes.end(),
        [](const CollectedNote& a, const CollectedNote& b) {
            if (a.offset != b.offset) return a.offset < b.offset;
            return midi_value(a.note.pitch) > midi_value(b.note.pitch);  // descending
        });

    std::vector<CollectedNote> result;
    std::size_t i = 0;
    while (i < notes.size()) {
        Beat current_offset = notes[i].offset;
        std::size_t j = i;
        while (j < notes.size() && notes[j].offset == current_offset) ++j;

        std::size_t count = j - i;
        if (count <= max_voices) {
            for (std::size_t k = i; k < j; ++k)
                result.push_back(notes[k]);
        } else {
            // Separate tagged note from the rest
            std::vector<std::size_t> tagged;
            std::vector<std::size_t> others;
            for (std::size_t k = i; k < j; ++k) {
                bool keep = is_treble ? notes[k].is_melody : notes[k].is_bass;
                if (keep) tagged.push_back(k);
                else others.push_back(k);
            }
            // Sort others by MIDI: treble prefers highest, bass prefers lowest
            if (is_treble) {
                std::sort(others.begin(), others.end(),
                    [&](std::size_t a, std::size_t b) {
                        return midi_value(notes[a].note.pitch) > midi_value(notes[b].note.pitch);
                    });
            } else {
                std::sort(others.begin(), others.end(),
                    [&](std::size_t a, std::size_t b) {
                        return midi_value(notes[a].note.pitch) < midi_value(notes[b].note.pitch);
                    });
            }
            // Take tagged + fill from others up to max_voices
            std::size_t slots = max_voices;
            for (auto idx : tagged) {
                if (slots == 0) break;
                result.push_back(notes[idx]);
                --slots;
            }
            for (auto idx : others) {
                if (slots == 0) break;
                result.push_back(notes[idx]);
                --slots;
            }
        }
        i = j;
    }
    notes = std::move(result);
}

/// Mark inner voices with cue-sized noteheads in a collected note vector.
/// At each unique offset, the highest and lowest notes retain their notehead;
/// all intermediate notes receive NoteHeadType::Cue.
void mark_inner_cue(std::vector<CollectedNote>& notes) {
    if (notes.empty()) return;

    // Sort by offset then by MIDI value
    std::sort(notes.begin(), notes.end(),
        [](const CollectedNote& a, const CollectedNote& b) {
            if (a.offset != b.offset) return a.offset < b.offset;
            return midi_value(a.note.pitch) < midi_value(b.note.pitch);
        });

    std::size_t i = 0;
    while (i < notes.size()) {
        Beat current_offset = notes[i].offset;
        std::size_t j = i;
        while (j < notes.size() && notes[j].offset == current_offset) ++j;

        std::size_t count = j - i;
        if (count > 2) {
            // Mark all except first (lowest) and last (highest) as Cue
            for (std::size_t k = i + 1; k < j - 1; ++k) {
                notes[k].note.notation_head = NoteHeadType::Cue;
            }
        }
        i = j;
    }
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

    // Determine whether orchestration annotations exist for melody/bass tagging
    bool has_orchestration = !score.orchestration_annotations.empty();

    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        const auto* ts_entry = find_time_sig(score.time_map, bar);
        Beat measure_dur = ts_entry
            ? ts_entry->time_signature.measure_duration()
            : Beat{1, 1};

        // Identify melody and bass parts for this bar via orchestration layer
        ScoreRegion bar_region;
        bar_region.start = ScoreTime{bar, Beat::zero()};
        bar_region.end = ScoreTime{bar + 1, Beat::zero()};

        std::set<std::uint64_t> melody_part_ids;
        std::set<std::uint64_t> bass_part_ids;
        if (has_orchestration) {
            auto melody_ids = query_parts_with_role(score, TexturalRole::Melody, bar_region);
            for (const auto& pid : melody_ids) melody_part_ids.insert(pid.value);
            auto bass_ids = query_parts_with_role(score, TexturalRole::BassLine, bar_region);
            for (const auto& pid : bass_ids) bass_part_ids.insert(pid.value);
        }

        // Collect melody MIDI values from melody parts for tagging
        std::set<int> melody_midi_at_bar;
        std::set<int> bass_midi_at_bar;
        if (has_orchestration) {
            for (const auto& pid_val : melody_part_ids) {
                auto mel = query_melody_for(score, PartId{pid_val}, bar_region);
                for (const auto& mn : mel)
                    melody_midi_at_bar.insert(midi_value(mn.pitch));
            }
            for (const auto& pid_val : bass_part_ids) {
                // Bass line: take the lowest note from voice 0
                for (const auto& part : score.parts) {
                    if (part.id.value != pid_val) continue;
                    if (bar - 1 >= part.measures.size()) continue;
                    if (part.measures[bar - 1].voices.empty()) continue;
                    const auto& v = part.measures[bar - 1].voices[0];
                    for (const auto& ev : v.events) {
                        const auto* ng = ev.as_note_group();
                        if (!ng) continue;
                        int lowest = 128;
                        for (const auto& n : ng->notes) {
                            int mv = midi_value(n.pitch);
                            if (mv < lowest) lowest = mv;
                        }
                        if (lowest < 128) bass_midi_at_bar.insert(lowest);
                    }
                }
            }
        }

        // Collect notes from all parts at this bar
        std::vector<CollectedNote> treble_notes;
        std::vector<CollectedNote> bass_notes;

        for (const auto& part : score.parts) {
            if (bar - 1 >= part.measures.size()) continue;
            auto collected = collect_notes_from_measure(part.measures[bar - 1]);
            for (auto& cn : collected) {
                int mv = midi_value(cn.note.pitch);
                // Tag melody and bass notes
                if (has_orchestration) {
                    if (melody_midi_at_bar.contains(mv)) cn.is_melody = true;
                    if (bass_midi_at_bar.contains(mv)) cn.is_bass = true;
                }
                if (mv >= 60) {
                    treble_notes.push_back(cn);
                } else {
                    bass_notes.push_back(cn);
                }
            }
        }

        // Post-processing: octave deduplication and voice limiting
        if (has_orchestration) {
            dedup_octaves(treble_notes, true);
            dedup_octaves(bass_notes, false);
            limit_voices(treble_notes, true, 4);
            limit_voices(bass_notes, false, 4);
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

            // Mark inner voices with cue noteheads
            mark_inner_cue(treble_notes);
            mark_inner_cue(bass_notes);

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

namespace {

/// Check whether every event in a voice is a rest
bool voice_is_all_rests(const Voice& voice) {
    for (const auto& event : voice.events) {
        if (event.is_note_group()) return false;
    }
    return true;
}

/// Find the highest sounding MIDI note across all parts except excluded_id
/// at the downbeat of the given bar. Returns nullopt if no note found.
std::optional<SpelledPitch> find_cue_pitch(
    const Score& score,
    PartId excluded_id,
    std::uint32_t bar
) {
    // First try: look for a Melody-role part via orchestration annotations
    ScoreRegion bar_region;
    bar_region.start = ScoreTime{bar, Beat::zero()};
    bar_region.end = ScoreTime{bar + 1, Beat::zero()};

    auto melody_parts = query_parts_with_role(score, TexturalRole::Melody, bar_region);
    for (const auto& melody_pid : melody_parts) {
        if (melody_pid == excluded_id) continue;
        auto melody = query_melody_for(score, melody_pid, bar_region);
        if (!melody.empty()) return melody[0].pitch;
    }

    // Fallback: scan all other parts for the highest sounding note at the downbeat
    int best_midi = -1;
    SpelledPitch best_pitch{};
    for (const auto& part : score.parts) {
        if (part.id == excluded_id) continue;
        if (bar - 1 >= part.measures.size()) continue;
        const auto& measure = part.measures[bar - 1];
        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                const auto* ng = event.as_note_group();
                if (!ng) continue;
                // Consider notes at the downbeat (offset == 0)
                if (event.offset > Beat::zero()) continue;
                for (const auto& note : ng->notes) {
                    int mv = midi_value(note.pitch);
                    if (mv > best_midi) {
                        best_midi = mv;
                        best_pitch = note.pitch;
                    }
                }
            }
        }
    }
    if (best_midi >= 0) return best_pitch;
    return std::nullopt;
}

}  // anonymous namespace

Score part_extract(const Score& score, PartId part_id,
                   std::uint32_t rest_threshold) {
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

            // Insert cue notes after long rest runs
            if (rest_threshold > 0 && result.parts.size() == 1) {
                auto& extracted = result.parts[0];

                // For each voice index, scan for consecutive rest runs
                if (!extracted.measures.empty()) {
                    std::uint8_t max_voices = 0;
                    for (const auto& m : extracted.measures) {
                        if (m.voices.size() > max_voices)
                            max_voices = static_cast<std::uint8_t>(m.voices.size());
                    }

                    for (std::uint8_t vi = 0; vi < max_voices; ++vi) {
                        std::uint32_t run_length = 0;
                        for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
                            std::size_t mi = bar - 1;
                            if (mi >= extracted.measures.size()) break;
                            auto& m = extracted.measures[mi];

                            bool is_rest = (vi < m.voices.size())
                                ? voice_is_all_rests(m.voices[vi])
                                : true;

                            if (is_rest) {
                                ++run_length;
                            } else {
                                if (run_length >= rest_threshold) {
                                    std::uint32_t cue_bar = bar - 1;
                                    auto pitch = find_cue_pitch(score, part_id, cue_bar);
                                    if (pitch && vi < extracted.measures[cue_bar - 1].voices.size()) {
                                        const auto* ts = find_time_sig(score.time_map, cue_bar);
                                        Beat beat_dur = ts
                                            ? Beat::normalise(1, ts->time_signature.denominator)
                                            : Beat{1, 4};

                                        Note cue_note;
                                        cue_note.pitch = *pitch;
                                        cue_note.notation_head = NoteHeadType::Cue;

                                        NoteGroup cue_ng;
                                        cue_ng.notes.push_back(cue_note);
                                        cue_ng.duration = beat_dur;

                                        auto& cue_voice = extracted.measures[cue_bar - 1].voices[vi];
                                        // Replace the first rest event with cue note + shortened rest
                                        if (!cue_voice.events.empty() && cue_voice.events[0].is_rest()) {
                                            Beat rest_dur = cue_voice.events[0].duration();
                                            cue_voice.events[0] = Event{
                                                next_view_id(), Beat::zero(), std::move(cue_ng)};
                                            if (rest_dur > beat_dur) {
                                                cue_voice.events.insert(
                                                    cue_voice.events.begin() + 1,
                                                    Event{next_view_id(), beat_dur,
                                                          RestEvent{rest_dur - beat_dur, true}});
                                            }
                                        }
                                    }
                                }
                                run_length = 0;
                            }
                        }
                        // Handle run extending to end of score
                        if (run_length >= rest_threshold) {
                            std::uint32_t cue_bar = score.metadata.total_bars;
                            auto pitch = find_cue_pitch(score, part_id, cue_bar);
                            if (pitch && vi < extracted.measures[cue_bar - 1].voices.size()) {
                                const auto* ts = find_time_sig(score.time_map, cue_bar);
                                Beat beat_dur = ts
                                    ? Beat::normalise(1, ts->time_signature.denominator)
                                    : Beat{1, 4};

                                Note cue_note;
                                cue_note.pitch = *pitch;
                                cue_note.notation_head = NoteHeadType::Cue;

                                NoteGroup cue_ng;
                                cue_ng.notes.push_back(cue_note);
                                cue_ng.duration = beat_dur;

                                auto& cue_voice = extracted.measures[cue_bar - 1].voices[vi];
                                if (!cue_voice.events.empty() && cue_voice.events[0].is_rest()) {
                                    Beat rest_dur = cue_voice.events[0].duration();
                                    cue_voice.events[0] = Event{
                                        next_view_id(), Beat::zero(), std::move(cue_ng)};
                                    if (rest_dur > beat_dur) {
                                        cue_voice.events.insert(
                                            cue_voice.events.begin() + 1,
                                            Event{next_view_id(), beat_dur,
                                                  RestEvent{rest_dur - beat_dur, true}});
                                    }
                                }
                            }
                        }
                    }
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
                        RestEvent rest_ev{ng->duration, true};
                        EventPayload payload{rest_ev};
                        Event ev{event.id, event.offset, std::move(payload)};
                        new_voice.events.push_back(std::move(ev));
                    } else {
                        EventPayload payload{std::move(new_ng)};
                        Event ev{event.id, event.offset, std::move(payload)};
                        new_voice.events.push_back(std::move(ev));
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
