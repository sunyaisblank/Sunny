/**
 * @file SIWF001A.cpp
 * @brief Score IR composition workflow functions — implementation
 *
 * Component: SIWF001A
 * Domain: SI (Score IR) | Category: WF (Workflow)
 */

#include "SIWF001A.h"
#include "SIVD001A.h"
#include "SIVW001A.h"
#include "SIQR001A.h"
#include "SICM001A.h"
#include "../Harmony/HRRN001A.h"
#include "../Scale/SCDF001A.h"

#include <algorithm>
#include <limits>

namespace Sunny::Core {

namespace {

std::uint64_t g_wf_event_id = 3000000;
std::uint64_t g_wf_part_id  = 500;
std::uint64_t g_wf_section_id = 1;

EventId next_wf_event_id() { return EventId{g_wf_event_id++}; }
PartId  next_wf_part_id()  { return PartId{g_wf_part_id++}; }

void push_undo(UndoStack* undo, Score& score, std::string desc,
               std::function<VoidResult()> forward,
               std::function<VoidResult()> inverse) {
    if (!undo) return;
    UndoEntry entry{score.version, std::move(forward), std::move(inverse), std::move(desc)};
    if (undo->group_depth > 0) {
        undo->pending_group.push_back(std::move(entry));
    } else {
        undo->undo_entries.push_back(std::move(entry));
        undo->redo_entries.clear();
    }
}

}  // namespace

// =============================================================================
// create_score
// =============================================================================

Result<Score> create_score(const ScoreSpec& spec) {
    if (spec.total_bars == 0)
        return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.parts.empty())
        return std::unexpected(ErrorCode::InvalidMutation);
    if (spec.bpm < Constants::TEMPO_MIN_BPM || spec.bpm > Constants::TEMPO_MAX_BPM)
        return std::unexpected(ErrorCode::InvalidBPM);

    auto ts = make_time_signature(spec.time_sig_num, spec.time_sig_den);
    if (!ts) return std::unexpected(ts.error());

    Score score;
    score.id = ScoreId{1};
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
    key_entry.key.accidentals = spec.key_accidentals;
    score.key_map.push_back(key_entry);

    // Time signature map: one entry at bar 1
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    // Create parts with empty measures (whole-measure rests)
    Beat measure_dur = ts->measure_duration();
    for (const auto& part_def : spec.parts) {
        Part part;
        part.id = next_wf_part_id();
        part.definition = part_def;

        for (std::uint32_t bar = 1; bar <= spec.total_bars; ++bar) {
            RestEvent rest{measure_dur, true};
            Event event{next_wf_event_id(), Beat::zero(), rest};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
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

Result<MutationResult> set_formal_plan(
    Score& score,
    std::vector<SectionDefinition> sections,
    UndoStack* undo
) {
    // Capture old section map for undo
    auto old_map = score.section_map;

    // Build new section map
    SectionMap new_map;
    for (const auto& def : sections) {
        if (def.start_bar == 0 || def.end_bar < def.start_bar)
            return std::unexpected(ErrorCode::InvalidBarRange);
        if (def.end_bar == std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);

        ScoreSection sec;
        sec.id = SectionId{g_wf_section_id++};
        sec.label = def.label;
        sec.start = ScoreTime{def.start_bar, Beat::zero()};
        sec.end = ScoreTime{def.end_bar + 1, Beat::zero()};
        sec.form_function = def.function;
        new_map.push_back(std::move(sec));
    }

    score.section_map = new_map;
    ++score.version;

    push_undo(undo, score, "set_formal_plan",
        [&score, new_map]() -> VoidResult {
            score.section_map = new_map;
            ++score.version;
            return {};
        },
        [&score, old_map]() -> VoidResult {
            score.section_map = old_map;
            ++score.version;
            return {};
        });

    return MutationResult{{}};
}

// =============================================================================
// set_section_harmony
// =============================================================================

Result<MutationResult> set_section_harmony(
    Score& score,
    const ScoreRegion& region,
    std::vector<ChordSymbolEntry> progression,
    UndoStack* undo
) {
    // Capture old annotations in the region for undo
    std::vector<HarmonicAnnotation> old_annotations;
    std::vector<std::size_t> old_indices;
    for (std::size_t i = 0; i < score.harmonic_annotations.size(); ++i) {
        const auto& ha = score.harmonic_annotations[i];
        if (ha.position >= region.start && ha.position < region.end) {
            old_annotations.push_back(ha);
            old_indices.push_back(i);
        }
    }

    // Remove old annotations in the region (reverse order to preserve indices)
    for (auto it = old_indices.rbegin(); it != old_indices.rend(); ++it) {
        score.harmonic_annotations.erase(score.harmonic_annotations.begin()
                                         + static_cast<std::ptrdiff_t>(*it));
    }

    // Determine key context for Roman numeral computation
    KeySignature key_ctx{};
    if (!score.key_map.empty()) {
        key_ctx = score.key_map[0].key;
        for (const auto& ke : score.key_map) {
            if (ke.position <= region.start) key_ctx = ke.key;
            else break;
        }
    }

    PitchClass key_pc = pc(key_ctx.root);
    bool is_minor = (key_ctx.accidentals < 0 && key_ctx.root.letter == 5) ||
                    false;  // Simplified minor detection; rely on caller's context

    // Build new annotations
    std::vector<HarmonicAnnotation> new_annotations;
    for (std::size_t i = 0; i < progression.size(); ++i) {
        const auto& entry = progression[i];

        HarmonicAnnotation ha;
        ha.position = entry.position;

        // Compute duration: extends to next chord or region end
        if (i + 1 < progression.size()) {
            // Approximate duration as difference in bar positions
            auto bar_diff = progression[i + 1].position.bar - entry.position.bar;
            ha.duration = Beat{static_cast<int64_t>(bar_diff > 0 ? bar_diff : 1), 1};
        } else {
            auto bar_diff = region.end.bar - entry.position.bar;
            ha.duration = Beat{static_cast<int64_t>(bar_diff > 0 ? bar_diff : 1), 1};
        }

        // Build chord voicing
        PitchClass root_pc = pc(entry.root);
        ha.chord.root = root_pc;
        ha.chord.quality = entry.quality;

        ha.key_context = key_ctx;

        // Compute Roman numeral
        auto scale = is_minor
            ? std::span<const Interval>(SCALE_MINOR.data(), SCALE_MINOR.size())
            : std::span<const Interval>(SCALE_MAJOR.data(), SCALE_MAJOR.size());

        auto numeral = chord_to_numeral(root_pc, entry.quality, key_pc, scale, is_minor);
        ha.roman_numeral = numeral.has_value() ? *numeral : entry.quality;

        // Classify harmonic function by parsing the Roman numeral to
        // extract the base degree, then mapping degree to function via
        // a lookup table. This handles all suffixed numerals (V7, iii7)
        // and case variations automatically.
        auto parsed = parse_roman_numeral_full(ha.roman_numeral);
        if (parsed.has_value()) {
            // Degree 0-6 → function: T, PD, T, PD, D, T, D
            static constexpr ScoreHarmonicFunction DEGREE_FUNCTION[7] = {
                ScoreHarmonicFunction::Tonic,        // I/i
                ScoreHarmonicFunction::Predominant,   // II/ii
                ScoreHarmonicFunction::Tonic,         // III/iii
                ScoreHarmonicFunction::Predominant,   // IV/iv
                ScoreHarmonicFunction::Dominant,       // V/v
                ScoreHarmonicFunction::Tonic,         // VI/vi
                ScoreHarmonicFunction::Dominant,       // VII/vii
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
        score.harmonic_annotations.push_back(std::move(ha));
    }

    // Sort annotations by position
    std::sort(score.harmonic_annotations.begin(), score.harmonic_annotations.end(),
        [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
            return a.position < b.position;
        });

    ++score.version;

    // Capture the full post-mutation state for forward replay
    auto post_state = score.harmonic_annotations;

    push_undo(undo, score, "set_section_harmony",
        [&score, post_state]() -> VoidResult {
            score.harmonic_annotations = post_state;
            ++score.version;
            return {};
        },
        [&score, old_annotations, region]() -> VoidResult {
            // Inverse: remove annotations in region, restore old ones
            auto& annotations = score.harmonic_annotations;
            annotations.erase(
                std::remove_if(annotations.begin(), annotations.end(),
                    [&region](const HarmonicAnnotation& ha) {
                        return ha.position >= region.start && ha.position < region.end;
                    }),
                annotations.end());
            for (const auto& ha : old_annotations)
                annotations.push_back(ha);
            std::sort(annotations.begin(), annotations.end(),
                [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
                    return a.position < b.position;
                });
            ++score.version;
            return {};
        });

    return MutationResult{{}};
}

// =============================================================================
// wf_add_part
// =============================================================================

Result<MutationResult> wf_add_part(
    Score& score, PartDefinition definition, UndoStack* undo
) {
    return add_part(score, std::move(definition), score.parts.size(), undo);
}

// =============================================================================
// write_melody
// =============================================================================

Result<MutationResult> write_melody(
    Score& score, PartId part_id, std::uint8_t voice_index,
    const std::vector<MelodyEntry>& melody, UndoStack* undo
) {
    // Validate part_id exists
    bool found = false;
    for (const auto& part : score.parts) {
        if (part.id == part_id) { found = true; break; }
    }
    if (!found) return std::unexpected(ErrorCode::PartNotFound);

    std::optional<UndoGroup> group;
    if (undo) group.emplace(*undo, "write_melody");

    for (const auto& entry : melody) {
        Note note;
        note.pitch = entry.pitch;
        if (entry.dynamic) note.dynamic = *entry.dynamic;
        if (entry.articulation) note.articulation = *entry.articulation;

        auto result = insert_note(
            score, part_id, entry.position.bar, voice_index,
            entry.position.beat, note, entry.duration, undo);
        if (!result) return result;
    }

    return MutationResult{{}};
}

// =============================================================================
// write_harmony
// =============================================================================

Result<MutationResult> write_harmony(
    Score& score, const std::vector<PartId>& target_parts,
    const std::vector<HarmonyEntry>& chords, UndoStack* undo
) {
    if (target_parts.empty()) return std::unexpected(ErrorCode::InvalidMutation);

    // Validate all target parts exist
    for (const auto& pid : target_parts) {
        bool found = false;
        for (const auto& part : score.parts) {
            if (part.id == pid) { found = true; break; }
        }
        if (!found) return std::unexpected(ErrorCode::PartNotFound);
    }

    std::optional<UndoGroup> group;
    if (undo) group.emplace(*undo, "write_harmony");

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
                auto result = insert_note(
                    score, target_parts[pi], entry.position.bar, 0,
                    entry.position.beat, note, entry.duration, undo);
                if (!result) return result;
            } else {
                // Insert the first note to create the NoteGroup event, then
                // the remaining pitches need to be added. Since insert_note
                // creates a single-note NoteGroup, we insert the first note
                // and then find the event to extend it with additional notes.
                Note first_note;
                first_note.pitch = pitches_for_part[0];
                auto result = insert_note(
                    score, target_parts[pi], entry.position.bar, 0,
                    entry.position.beat, first_note, entry.duration, undo);
                if (!result) return result;

                // Locate the event we just inserted and add remaining notes
                for (auto& part : score.parts) {
                    if (part.id != target_parts[pi]) continue;
                    for (auto& measure : part.measures) {
                        if (measure.bar_number != entry.position.bar) continue;
                        for (auto& voice : measure.voices) {
                            if (voice.voice_index != 0) continue;
                            for (auto& event : voice.events) {
                                if (event.offset == entry.position.beat &&
                                    event.is_note_group()) {
                                    auto* ng = std::get_if<NoteGroup>(&event.payload);
                                    if (ng && ng->duration == entry.duration) {
                                        for (std::size_t ni = 1; ni < pitches_for_part.size(); ++ni) {
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

    return MutationResult{{}};
}

// =============================================================================
// wf_reorchestrate
// =============================================================================

Result<MutationResult> wf_reorchestrate(
    Score& score, const ScoreRegion& region,
    PartId source, PartId target, UndoStack* undo
) {
    ScoreRegion source_region = region;
    source_region.parts = {source};
    return reorchestrate(score, source_region, target, undo);
}

// =============================================================================
// wf_double_part
// =============================================================================

Result<MutationResult> wf_double_part(
    Score& score, const ScoreRegion& region,
    PartId source, PartId target, std::int8_t interval, UndoStack* undo
) {
    ScoreRegion source_region = region;
    source_region.parts = {source};
    return double_at_interval(score, source_region, target, interval, undo);
}

// =============================================================================
// wf_set_dynamics
// =============================================================================

Result<MutationResult> wf_set_dynamics(
    Score& score, const ScoreRegion& region,
    DynamicLevel level, UndoStack* undo
) {
    return set_dynamic_region(score, region, level, undo);
}

// =============================================================================
// wf_set_articulation
// =============================================================================

Result<MutationResult> wf_set_articulation(
    Score& score, const ScoreRegion& region,
    ArticulationType articulation, UndoStack* undo
) {
    std::optional<UndoGroup> group;
    if (undo) group.emplace(*undo, "wf_set_articulation");

    for (auto& part : score.parts) {
        // Skip parts not in the region (empty parts list means all parts)
        if (!region.parts.empty()) {
            bool in_region = false;
            for (const auto& pid : region.parts) {
                if (pid == part.id) { in_region = true; break; }
            }
            if (!in_region) continue;
        }

        for (auto& measure : part.measures) {
            ScoreTime measure_start{measure.bar_number, Beat::zero()};
            ScoreTime measure_end{measure.bar_number + 1, Beat::zero()};

            // Skip measures entirely before the region
            if (measure_end <= region.start) continue;
            // Stop if the measure starts at or after the region end
            if (measure_start >= region.end) break;

            for (auto& voice : measure.voices) {
                for (auto& event : voice.events) {
                    if (!event.is_note_group()) continue;

                    ScoreTime event_time{measure.bar_number, event.offset};
                    if (event_time < region.start || event_time >= region.end) continue;

                    const auto* ng = event.as_note_group();
                    if (!ng) continue;

                    for (std::uint8_t ni = 0; ni < static_cast<std::uint8_t>(ng->notes.size()); ++ni) {
                        auto result = set_articulation(
                            score, event.id, ni, articulation, undo);
                        if (!result) return result;
                    }
                }
            }
        }
    }

    return MutationResult{{}};
}

// =============================================================================
// wf_insert_note
// =============================================================================

Result<MutationResult> wf_insert_note(
    Score& score, PartId part, std::uint32_t bar,
    std::uint8_t voice, Beat offset,
    Note note, Beat duration, UndoStack* undo
) {
    return insert_note(score, part, bar, voice, offset, std::move(note), duration, undo);
}

// =============================================================================
// wf_modify_note
// =============================================================================

Result<MutationResult> wf_modify_note(
    Score& score, EventId event_id, std::uint8_t note_index,
    std::optional<SpelledPitch> pitch,
    std::optional<Beat> duration,
    std::optional<VelocityValue> velocity,
    std::optional<ArticulationType> articulation,
    UndoStack* undo
) {
    std::optional<UndoGroup> group;
    if (undo) group.emplace(*undo, "wf_modify_note");

    if (pitch) {
        auto result = modify_pitch(score, event_id, note_index, *pitch, undo);
        if (!result) return result;
    }

    if (duration) {
        auto result = modify_duration(score, event_id, *duration, undo);
        if (!result) return result;
    }

    if (velocity) {
        auto result = modify_velocity(score, event_id, note_index, *velocity, undo);
        if (!result) return result;
    }

    if (articulation) {
        auto result = set_articulation(score, event_id, note_index, *articulation, undo);
        if (!result) return result;
    }

    return MutationResult{{}};
}

// =============================================================================
// wf_delete_event
// =============================================================================

Result<MutationResult> wf_delete_event(
    Score& score, EventId event_id, UndoStack* undo
) {
    return delete_event(score, event_id, undo);
}

// =============================================================================
// wf_transpose
// =============================================================================

Result<MutationResult> wf_transpose(
    Score& score, std::variant<EventId, ScoreRegion> target,
    DiatonicInterval interval, UndoStack* undo
) {
    if (auto* eid = std::get_if<EventId>(&target)) {
        return transpose_event(score, *eid, interval, undo);
    }
    auto& region = std::get<ScoreRegion>(target);
    return transpose_region(score, region, interval, undo);
}

// =============================================================================
// wf_analyze_harmony
// =============================================================================

std::vector<HarmonicAnnotation> wf_analyze_harmony(
    const Score& score, const ScoreRegion& region
) {
    return query_harmony_range(score, region.start, region.end);
}

// =============================================================================
// wf_get_orchestration
// =============================================================================

std::vector<std::pair<PartId, TexturalRole>> wf_get_orchestration(
    const Score& score, const ScoreRegion& region
) {
    return query_orchestration(score, region);
}

// =============================================================================
// wf_get_reduction
// =============================================================================

Score wf_get_reduction(
    const Score& score, const std::string& view_type,
    const std::optional<ScoreRegion>& region
) {
    // When a region is specified, extract it first
    const Score* source = &score;
    std::optional<Score> region_score;
    if (region) {
        auto rv = region_view(score, *region);
        if (rv) {
            region_score = std::move(*rv);
            source = &(*region_score);
        }
    }

    if (view_type == "piano") return piano_reduction(*source);
    if (view_type == "short") return short_score(*source);
    if (view_type == "skeleton") return harmonic_skeleton(*source);

    // Default to piano reduction for unrecognised view types
    return piano_reduction(*source);
}

// =============================================================================
// wf_validate_score
// =============================================================================

std::vector<Diagnostic> wf_validate_score(const Score& score) {
    return validate_score(score);
}

// =============================================================================
// wf_get_form_summary
// =============================================================================

std::vector<FormSummaryEntry> wf_get_form_summary(const Score& score) {
    return query_form_summary(score);
}

// =============================================================================
// wf_compile_to_midi
// =============================================================================

Result<CompiledMidiResult> wf_compile_to_midi(
    const Score& score, int ppq
) {
    return compile_to_midi(score, ppq);
}

}  // namespace Sunny::Core
