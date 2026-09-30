/**
 * @file workflows.cpp
 * @brief Score IR composition workflow functions — implementation
 *
 */

#include "id_allocator.hpp"
#include "version.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/views.hpp>
#include <sunny/core/score/workflows.hpp>

namespace sunny::core {

namespace {

/// Push a pre-mutation snapshot onto the undo stack (if provided), clearing
/// redo. Mirrors the  helper: when group_depth > 0, only the first
/// snapshot of the group is kept because the earliest 'before' state covers
/// the whole group.
void push_snapshot(UndoStack* undo, std::optional<Score>&& before, std::string desc) {
    if (!undo || !before) return;
    const std::uint64_t snapshot_version = before->version;
    UndoEntry entry{
        snapshot_version, std::make_shared<const Score>(std::move(*before)), std::move(desc)};
    if (undo->group_depth > 0) {
        if (!undo->group_has_snapshot) {
            entry.description = undo->group_description;
            undo->undo_entries.push_back(std::move(entry));
            undo->redo_entries.clear();
            undo->group_has_snapshot = true;
        }
    } else {
        undo->undo_entries.push_back(std::move(entry));
        undo->redo_entries.clear();
    }
}

bool valid_workflow_region(const Score& score, const ScoreRegion& region) {
    if (score.metadata.total_bars == std::numeric_limits<std::uint32_t>::max() ||
        region.start.bar < 1 || region.start.bar > score.metadata.total_bars ||
        !(region.start < region.end) || region.end.bar < 1 ||
        region.end.bar > score.metadata.total_bars + 1 ||
        (region.end.bar == score.metadata.total_bars + 1 && region.end.beat != Beat::zero()))
        return false;
    auto start = score_time_to_absolute_beat(region.start, score.time_map);
    auto end = score_time_to_absolute_beat(region.end, score.time_map);
    if (!start || !end || *start >= *end) return false;

    std::set<std::uint64_t> seen_parts;
    for (const auto part_id : region.parts) {
        if (!seen_parts.insert(part_id.value).second) return false;
        if (std::none_of(score.parts.begin(), score.parts.end(), [part_id](const Part& part) {
                return part.id == part_id;
            }))
            return false;
    }
    return true;
}

const KeySignature* active_key_at(const Score& score, ScoreTime position) {
    const KeySignature* active = nullptr;
    for (const auto& entry : score.key_map) {
        if (entry.position > position) break;
        active = &entry.key;
    }
    return active;
}

Result<ChordVoicing> make_section_chord_voicing(const ChordSymbolEntry& entry) {
    if (entry.root.letter > 6) return std::unexpected(ErrorCode::InvalidMutation);
    const auto root_note = midi(entry.root);
    if (!root_note) return std::unexpected(ErrorCode::ChordGenerationFailed);
    const auto intervals = chord_quality_intervals(entry.quality);
    if (!intervals) return std::unexpected(ErrorCode::InvalidChordQuality);
    if (intervals->empty()) return std::unexpected(ErrorCode::ChordGenerationFailed);

    ChordVoicing voicing;
    voicing.root = pc(entry.root);
    voicing.quality = entry.quality;

    std::optional<std::size_t> inversion;
    std::optional<MidiNote> bass_note;
    if (entry.bass) {
        if (entry.bass->letter > 6) return std::unexpected(ErrorCode::InvalidMutation);
        const auto converted_bass = midi(*entry.bass);
        if (!converted_bass) return std::unexpected(ErrorCode::ChordGenerationFailed);
        bass_note = *converted_bass;
        const auto bass_pc = pc(*entry.bass);
        for (std::size_t index = 0; index < intervals->size(); ++index) {
            if (PitchClass::wrapped(voicing.root + (*intervals)[index]) == bass_pc) {
                inversion = index;
                break;
            }
        }
        // ChordVoicing has no independent non-chord slash-bass field. Reject
        // one rather than mislabelling it as a conventional inversion.
        if (!inversion) return std::unexpected(ErrorCode::InvalidMutation);
        voicing.notes.push_back(*bass_note);
        voicing.inversion = static_cast<int>(*inversion);
    }

    for (std::size_t index = 0; index < intervals->size(); ++index) {
        if (inversion && index == *inversion) continue;
        int value = static_cast<int>(*root_note) + (*intervals)[index];
        if (bass_note) {
            while (value <= static_cast<int>(*bass_note))
                value += 12;
        }
        auto note = MidiNote::from_int(value);
        if (!note) return std::unexpected(ErrorCode::ChordGenerationFailed);
        voicing.notes.push_back(*note);
    }
    std::sort(voicing.notes.begin(), voicing.notes.end());
    return voicing;
}

} // namespace

// =============================================================================
// create_score
// =============================================================================

Result<Score> create_score(const ScoreSpec& spec) {
    if (spec.id.value == 0) return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.total_bars == 0) return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.parts.empty()) return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.bpm < Constants::TEMPO_MIN_BPM || spec.bpm > Constants::TEMPO_MAX_BPM)
        return std::unexpected(ErrorCode::InvalidBPM);
    if (auto tuning = validate_score_tuning(spec.tuning); !tuning)
        return std::unexpected(tuning.error());

    auto ts = make_time_signature(spec.time_sig_num, spec.time_sig_den);
    if (!ts) return std::unexpected(ts.error());

    Score score;
    score.id = spec.id;
    score.tuning = spec.tuning;
    score.metadata.title = spec.title;
    score.metadata.total_bars = spec.total_bars;

    // Tempo map: one entry at SCORE_START
    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(static_cast<std::int64_t>(spec.bpm));
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    // Key map: one entry at SCORE_START
    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = spec.key_root;
    const auto mode = find_scale(spec.minor ? "minor" : "major");
    if (!mode) return std::unexpected(mode.error());
    key_entry.key.mode = *mode;
    if (spec.key_accidentals) {
        key_entry.key.accidentals = *spec.key_accidentals;
    } else {
        const auto derived = expected_key_accidentals(key_entry.key);
        if (!derived || *derived < std::numeric_limits<std::int8_t>::min() ||
            *derived > std::numeric_limits<std::int8_t>::max())
            return std::unexpected(ErrorCode::InvalidMutation);
        key_entry.key.accidentals = static_cast<std::int8_t>(*derived);
    }
    score.key_map.push_back(key_entry);

    // Time signature map: one entry at bar 1
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    // Create parts with empty measures (whole-measure rests)
    Beat measure_dur = ts->measure_duration();
    detail::FreshIdAllocator<PartId> part_ids;
    detail::FreshIdAllocator<EventId> event_ids;
    for (const auto& part_def : spec.parts) {
        Part part;
        auto part_id = part_ids.allocate();
        if (!part_id) return std::unexpected(part_id.error());
        part.id = *part_id;
        part.definition = part_def;

        for (std::uint32_t bar = 1; bar <= spec.total_bars; ++bar) {
            Measure measure{bar, {}, std::nullopt, std::nullopt};
            for (std::uint16_t staff = 0; staff < std::max<std::uint8_t>(part_def.staff_count, 1);
                 ++staff) {
                RestEvent rest{measure_dur, true};
                auto event_id = event_ids.allocate();
                if (!event_id) return std::unexpected(event_id.error());
                Event event{*event_id, Beat::zero(), rest};
                Voice voice{static_cast<std::uint8_t>(staff), {event}, {}};
                voice.staff_index = static_cast<std::uint8_t>(staff);
                measure.voices.push_back(std::move(voice));
            }
            part.measures.push_back(std::move(measure));
        }

        score.parts.push_back(std::move(part));
    }

    // Validate structural integrity
    auto diags = validate_structural(score);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error)
            return std::unexpected(ErrorCode::InvariantViolation);
    }

    return score;
}

// =============================================================================
// set_formal_plan
// =============================================================================

Result<MutationResult>
set_formal_plan(Score& score, const std::vector<SectionDefinition>& sections, UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    // Build new section map
    SectionMap new_map;
    auto section_ids = detail::section_id_allocator(score);
    for (const auto& def : sections) {
        if (def.start_bar == 0 || def.end_bar < def.start_bar ||
            def.end_bar > score.metadata.total_bars)
            return std::unexpected(ErrorCode::InvalidBarRange);
        if (def.end_bar == std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);

        ScoreSection sec;
        auto section_id = section_ids.allocate();
        if (!section_id) return std::unexpected(section_id.error());
        sec.id = *section_id;
        sec.label = def.label;
        sec.start = ScoreTime{def.start_bar, Beat::zero()};
        sec.end = ScoreTime{def.end_bar + 1, Beat::zero()};
        sec.form_function = def.function;
        new_map.push_back(std::move(sec));
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    candidate.section_map = std::move(new_map);
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);

    push_snapshot(undo, std::move(before), "set_formal_plan");

    return MutationResult{{}};
}

// =============================================================================
// set_section_harmony
// =============================================================================

Result<MutationResult> set_section_harmony(Score& score,
                                           const ScoreRegion& region,
                                           std::vector<ChordSymbolEntry> progression,
                                           UndoStack* undo) {
    if (detail::score_version_exhausted(score))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    if (!valid_workflow_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    auto region_start = score_time_to_absolute_beat(region.start, score.time_map);
    auto region_end = score_time_to_absolute_beat(region.end, score.time_map);
    if (!region_start || !region_end || *region_start >= *region_end)
        return std::unexpected(ErrorCode::InvalidRegion);

    std::optional<ScoreTime> previous_position;
    for (const auto& entry : progression) {
        auto position = score_time_to_absolute_beat(entry.position, score.time_map);
        if (!position || entry.position.bar > score.metadata.total_bars ||
            *position < *region_start || *position >= *region_end ||
            (previous_position && entry.position <= *previous_position))
            return std::unexpected(ErrorCode::InvalidRegion);
        previous_position = entry.position;
    }

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    // Remove old annotations in the region
    candidate.harmonic_annotations.erase(std::remove_if(candidate.harmonic_annotations.begin(),
                                                        candidate.harmonic_annotations.end(),
                                                        [&region](const HarmonicAnnotation& ha) {
                                                            return ha.position >= region.start &&
                                                                   ha.position < region.end;
                                                        }),
                                         candidate.harmonic_annotations.end());

    // Build new annotations
    std::vector<HarmonicAnnotation> new_annotations;
    for (std::size_t i = 0; i < progression.size(); ++i) {
        const auto& entry = progression[i];

        HarmonicAnnotation ha;
        ha.position = entry.position;

        // Harmonic duration is exact AbsoluteBeat distance to the next chord
        // or the region boundary; it is not a bar-count approximation.
        auto start = score_time_to_absolute_beat(entry.position, candidate.time_map);
        const ScoreTime end_position =
            i + 1 < progression.size() ? progression[i + 1].position : region.end;
        auto end = score_time_to_absolute_beat(end_position, candidate.time_map);
        if (!start || !end) return std::unexpected(ErrorCode::InvalidRegion);
        auto duration = checked_sub(*end, *start);
        if (!duration || *duration <= Beat::zero())
            return std::unexpected(ErrorCode::InvalidRegion);
        ha.duration = *duration;

        auto voicing = make_section_chord_voicing(entry);
        if (!voicing) return std::unexpected(voicing.error());
        ha.chord = std::move(*voicing);

        const auto* key_context = active_key_at(candidate, entry.position);
        if (!key_context) return std::unexpected(ErrorCode::InvalidMutation);
        ha.key_context = *key_context;
        const PitchClass key_pc = pc(key_context->root);
        const auto scale = key_context->mode.get_intervals();
        const bool is_minor = scale.size() >= 3 && scale[2] == 3;

        // Compute Roman numeral
        auto numeral = chord_to_numeral(ha.chord.root, entry.quality, key_pc, scale, is_minor);
        if (!numeral) return std::unexpected(numeral.error());
        ha.roman_numeral = *numeral;

        // Classify harmonic function by parsing the Roman numeral to
        // extract the base degree, then mapping degree to function via
        // a lookup table. This handles all suffixed numerals (V7, iii7)
        // and case variations automatically.
        auto parsed = parse_roman_numeral_full(ha.roman_numeral);
        if (parsed.has_value()) {
            // Degree 0-6 → function: T, PD, T, PD, D, T, D
            static constexpr ScoreHarmonicFunction DEGREE_FUNCTION[7] = {
                ScoreHarmonicFunction::Tonic,       // I/i
                ScoreHarmonicFunction::Predominant, // II/ii
                ScoreHarmonicFunction::Tonic,       // III/iii
                ScoreHarmonicFunction::Predominant, // IV/iv
                ScoreHarmonicFunction::Dominant,    // V/v
                ScoreHarmonicFunction::Tonic,       // VI/vi
                ScoreHarmonicFunction::Dominant,    // VII/vii
            };
            int deg = parsed->degree;
            if (deg >= 0 && deg < 7)
                ha.function = DEGREE_FUNCTION[deg];
            else
                ha.function = ScoreHarmonicFunction::Ambiguous;
        } else {
            ha.function = ScoreHarmonicFunction::Ambiguous;
        }

        new_annotations.push_back(std::move(ha));
    }

    // Insert new annotations
    for (auto& ha : new_annotations) {
        candidate.harmonic_annotations.push_back(std::move(ha));
    }

    // Sort annotations by position
    std::sort(candidate.harmonic_annotations.begin(),
              candidate.harmonic_annotations.end(),
              [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
                  return a.position < b.position;
              });

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    if (auto advanced = detail::advance_score_version(candidate); !advanced)
        return std::unexpected(advanced.error());
    score = std::move(candidate);

    push_snapshot(undo, std::move(before), "set_section_harmony");

    return MutationResult{{}};
}

// =============================================================================
// add_part
// =============================================================================

Result<MutationResult> add_part(Score& score, PartDefinition definition, UndoStack* undo) {
    return add_part(score, std::move(definition), score.parts.size(), undo);
}

// =============================================================================
// write_melody
// =============================================================================

Result<MutationResult> write_melody(Score& score,
                                    PartId part_id,
                                    std::uint8_t voice_index,
                                    const std::vector<MelodyEntry>& melody,
                                    UndoStack* undo) {
    // Validate part_id exists
    bool found = false;
    for (const auto& part : score.parts) {
        if (part.id == part_id) {
            found = true;
            break;
        }
    }
    if (!found) return std::unexpected(ErrorCode::PartNotFound);
    if (melody.empty()) return MutationResult{{}};

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    for (const auto& entry : melody) {
        Note note;
        note.pitch = entry.pitch;
        note.dynamic = entry.dynamic;
        note.articulation = entry.articulation;

        auto result = insert_note(candidate,
                                  part_id,
                                  entry.position.bar,
                                  voice_index,
                                  entry.position.beat,
                                  note,
                                  entry.duration,
                                  nullptr);
        if (!result) return result;
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "write_melody");
    return MutationResult{{}};
}

// =============================================================================
// write_harmony
// =============================================================================

Result<MutationResult> write_harmony(Score& score,
                                     const std::vector<PartId>& target_parts,
                                     const std::vector<HarmonyEntry>& chords,
                                     UndoStack* undo) {
    if (target_parts.empty()) return std::unexpected(ErrorCode::InvalidMutation);

    // Validate all target parts exist
    for (const auto& pid : target_parts) {
        bool found = false;
        for (const auto& part : score.parts) {
            if (part.id == pid) {
                found = true;
                break;
            }
        }
        if (!found) return std::unexpected(ErrorCode::PartNotFound);
    }
    if (chords.empty()) return MutationResult{{}};

    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    for (const auto& entry : chords) {
        const auto& voicing = entry.voicing;
        std::size_t num_parts = target_parts.size();

        // Distribute pitches across parts, bottom to top. When the voicing
        // has more pitches than parts, surplus pitches are grouped as a
        // chord in the topmost part.
        for (std::size_t pi = 0; pi < num_parts; ++pi) {
            std::vector<SpelledPitch> pitches_for_part;

            if (pi < num_parts - 1) {
                // Each lower part gets one pitch (if available)
                if (pi < voicing.size()) {
                    pitches_for_part.push_back(voicing[pi]);
                }
            } else {
                // Last part gets all remaining pitches
                for (std::size_t vi = pi; vi < voicing.size(); ++vi) {
                    pitches_for_part.push_back(voicing[vi]);
                }
            }

            if (pitches_for_part.empty()) continue;

            if (pitches_for_part.size() == 1) {
                Note note;
                note.pitch = pitches_for_part[0];
                auto result = insert_note(candidate,
                                          target_parts[pi],
                                          entry.position.bar,
                                          0,
                                          entry.position.beat,
                                          note,
                                          entry.duration,
                                          nullptr);
                if (!result) return result;
            } else {
                // Insert the first note to create the NoteGroup event, then
                // the remaining pitches need to be added. Since insert_note
                // creates a single-note NoteGroup, we insert the first note
                // and then find the event to extend it with additional notes.
                Note first_note;
                first_note.pitch = pitches_for_part[0];
                auto result = insert_note(candidate,
                                          target_parts[pi],
                                          entry.position.bar,
                                          0,
                                          entry.position.beat,
                                          first_note,
                                          entry.duration,
                                          nullptr);
                if (!result) return result;

                // Locate the event we just inserted and add remaining notes
                for (auto& part : candidate.parts) {
                    if (part.id != target_parts[pi]) continue;
                    for (auto& measure : part.measures) {
                        if (measure.bar_number != entry.position.bar) continue;
                        for (auto& voice : measure.voices) {
                            if (voice.voice_index != 0) continue;
                            for (auto& event : voice.events) {
                                if (event.offset == entry.position.beat && event.is_note_group()) {
                                    auto* ng = std::get_if<NoteGroup>(&event.payload);
                                    if (ng && ng->duration == entry.duration) {
                                        for (std::size_t ni = 1; ni < pitches_for_part.size();
                                             ++ni) {
                                            Note extra;
                                            extra.pitch = pitches_for_part[ni];
                                            ng->notes.push_back(extra);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "write_harmony");
    return MutationResult{{}};
}

// =============================================================================
// reorchestrate
// =============================================================================

Result<MutationResult> reorchestrate(
    Score& score, const ScoreRegion& region, PartId source, PartId target, UndoStack* undo) {
    ScoreRegion source_region = region;
    source_region.parts = {source};
    return reorchestrate(score, source_region, target, undo);
}

// =============================================================================
// double_part
// =============================================================================

Result<MutationResult> double_part(Score& score,
                                   const ScoreRegion& region,
                                   PartId source,
                                   PartId target,
                                   std::int8_t interval,
                                   UndoStack* undo) {
    ScoreRegion source_region = region;
    source_region.parts = {source};
    return double_at_interval(score, source_region, target, interval, undo);
}

// =============================================================================
// set_dynamics
// =============================================================================

Result<MutationResult>
set_dynamics(Score& score, const ScoreRegion& region, DynamicLevel level, UndoStack* undo) {
    return set_dynamic_region(score, region, level, undo);
}

// =============================================================================
// set_articulation
// =============================================================================

Result<MutationResult> set_articulation(Score& score,
                                        const ScoreRegion& region,
                                        ArticulationType articulation,
                                        UndoStack* undo) {
    if (!valid_workflow_region(score, region)) return std::unexpected(ErrorCode::InvalidRegion);
    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;
    std::vector<std::pair<EventId, std::uint8_t>> targets;

    for (const auto& part : candidate.parts) {
        // Skip parts not in the region (empty parts list means all parts)
        if (!region.parts.empty()) {
            bool in_region = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) {
                    in_region = true;
                    break;
                }
            }
            if (!in_region) continue;
        }

        for (const auto& measure : part.measures) {
            ScoreTime measure_start{measure.bar_number, Beat::zero()};
            ScoreTime measure_end{measure.bar_number + 1, Beat::zero()};

            // Skip measures entirely before the region
            if (measure_end <= region.start) continue;
            // Stop if the measure starts at or after the region end
            if (measure_start >= region.end) break;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (!event.is_note_group()) continue;

                    ScoreTime event_time{measure.bar_number, event.offset};
                    if (event_time < region.start || event_time >= region.end) continue;

                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    for (std::uint8_t ni = 0; ni < static_cast<std::uint8_t>(ng->notes.size());
                         ++ni) {
                        targets.emplace_back(event.id, ni);
                    }
                }
            }
        }
    }

    if (targets.empty()) return MutationResult{{}};
    for (const auto& [event_id, note_index] : targets) {
        auto result = set_articulation(candidate, event_id, note_index, articulation, nullptr);
        if (!result) return result;
    }
    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "set_articulation");
    return MutationResult{{}};
}

// =============================================================================
// modify_note
// =============================================================================

Result<MutationResult> modify_note(Score& score,
                                   EventId event_id,
                                   std::uint8_t note_index,
                                   std::optional<SpelledPitch> pitch,
                                   std::optional<Beat> duration,
                                   std::optional<VelocityValue> velocity,
                                   std::optional<ArticulationType> articulation,
                                   UndoStack* undo) {
    return modify_note(
        score, event_id, note_index, pitch, duration, velocity, std::nullopt, articulation, undo);
}

Result<MutationResult> modify_note(Score& score,
                                   EventId event_id,
                                   std::uint8_t note_index,
                                   std::optional<SpelledPitch> pitch,
                                   std::optional<Beat> duration,
                                   std::optional<VelocityValue> velocity,
                                   std::optional<std::uint8_t> release_velocity,
                                   std::optional<ArticulationType> articulation,
                                   UndoStack* undo) {
    if (!pitch && !duration && !velocity && !release_velocity && !articulation)
        return MutationResult{{}};
    std::optional<Score> before;
    if (undo) before = score;
    Score candidate = score;

    if (pitch) {
        auto result = modify_pitch(candidate, event_id, note_index, *pitch, nullptr);
        if (!result) return result;
    }

    if (duration) {
        auto result = modify_duration(candidate, event_id, *duration, nullptr);
        if (!result) return result;
    }

    if (velocity) {
        auto result = modify_velocity(candidate, event_id, note_index, *velocity, nullptr);
        if (!result) return result;
    }

    if (release_velocity) {
        auto result =
            modify_release_velocity(candidate, event_id, note_index, *release_velocity, nullptr);
        if (!result) return result;
    }

    if (articulation) {
        auto result = set_articulation(candidate, event_id, note_index, articulation, nullptr);
        if (!result) return result;
    }

    if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
    score = std::move(candidate);
    push_snapshot(undo, std::move(before), "modify_note");
    return MutationResult{{}};
}

// =============================================================================
// transpose
// =============================================================================

Result<MutationResult> transpose(Score& score,
                                 std::variant<EventId, ScoreRegion> target,
                                 DiatonicInterval interval,
                                 UndoStack* undo) {
    if (auto* eid = std::get_if<EventId>(&target)) {
        return transpose_event(score, *eid, interval, undo);
    }
    auto& region = std::get<ScoreRegion>(target);
    return transpose_region(score, region, interval, undo);
}

// =============================================================================
// analyze_harmony
// =============================================================================

std::vector<HarmonicAnnotation> analyze_harmony(const Score& score, const ScoreRegion& region) {
    return query_harmony_range(score, region.start, region.end);
}

// =============================================================================
// get_orchestration
// =============================================================================

std::vector<std::pair<PartId, TexturalRole>> get_orchestration(const Score& score,
                                                               const ScoreRegion& region) {
    return query_orchestration(score, region);
}

// =============================================================================
// get_reduction
// =============================================================================

Result<Score> get_reduction(const Score& score,
                            ScoreId result_id,
                            const std::string& view_type,
                            const std::optional<ScoreRegion>& region) {
    if (result_id.value == 0) return std::unexpected(ErrorCode::InvalidMutation);
    if (view_type != "piano" && view_type != "short" && view_type != "skeleton")
        return std::unexpected(ErrorCode::InvalidMutation);

    const Score* source = &score;
    std::optional<Score> region_score;
    if (region) {
        auto rv = region_view(score, result_id, *region);
        if (!rv) return std::unexpected(rv.error());
        region_score = std::move(*rv);
        source = &(*region_score);
    }

    if (view_type == "piano") return piano_reduction(*source, result_id);
    if (view_type == "short") return short_score(*source, result_id);
    return harmonic_skeleton(*source, result_id);
}

// =============================================================================
// get_form_summary
// =============================================================================

std::vector<FormSummaryEntry> get_form_summary(const Score& score) {
    return query_form_summary(score);
}

} // namespace sunny::core
