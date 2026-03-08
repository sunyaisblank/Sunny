/**
 * @file SIWF001A.h
 * @brief Score IR composition workflow functions
 *
 * Component: SIWF001A
 * Domain: SI (Score IR) | Category: WF (Workflow)
 *
 * Provides entry-point functions for agent-driven composition workflows
 * per spec §12.2. These functions create or structurally reshape scores
 * at the formal level, complementing SIQR001A (read-only queries) and
 * SIMT001A (atomic mutations on existing scores).
 *
 * Functions:
 * - create_score: build a valid Score from a compact specification
 * - set_formal_plan: assign section structure to a score
 * - set_section_harmony: write harmonic annotations into a region
 *
 * Invariants:
 * - create_score output passes validate_structural with no Error diagnostics
 * - set_formal_plan and set_section_harmony are undoable
 */

#pragma once

#include "SIDC001A.h"
#include "SIMT001A.h"
#include "SIVW001A.h"
#include "SIQR001A.h"
#include "SICM001A.h"

namespace Sunny::Core {

// =============================================================================
// Score Creation (§12.2.1)
// =============================================================================

/**
 * @brief Compact specification for creating a new score
 */
struct ScoreSpec {
    std::string title;
    std::uint32_t total_bars;
    double bpm;
    SpelledPitch key_root;
    bool minor = false;
    std::int8_t key_accidentals = 0;
    int time_sig_num = 4;
    int time_sig_den = 4;
    std::vector<PartDefinition> parts;
};

/**
 * @brief Build a valid Score from a compact specification
 *
 * Constructs tempo, key, and time signature maps from the spec fields,
 * creates parts with empty measures (whole-measure rests), assigns
 * sequential IDs, and validates the result.
 *
 * @param spec Score specification
 * @return Valid Score or error if validation fails
 */
[[nodiscard]] Result<Score> create_score(const ScoreSpec& spec);

// =============================================================================
// Formal Plan (§12.2.2)
// =============================================================================

/**
 * @brief Definition of a single formal section
 */
struct SectionDefinition {
    std::string label;
    std::uint32_t start_bar;
    std::uint32_t end_bar;
    std::optional<FormFunction> function;
};

/**
 * @brief Replace the section map with a formal plan
 *
 * Captures the old section map for undo, builds ScoreSection entries
 * from the definitions, and replaces score.section_map.
 *
 * @param score Target score (modified in place)
 * @param sections Section definitions
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_formal_plan(
    Score& score,
    std::vector<SectionDefinition> sections,
    UndoStack* undo = nullptr
);

// =============================================================================
// Section Harmony (§12.2.3)
// =============================================================================

/**
 * @brief A chord symbol at a specific position
 */
struct ChordSymbolEntry {
    ScoreTime position;
    SpelledPitch root;
    std::string quality;
    std::optional<SpelledPitch> bass;
};

/**
 * @brief Write harmonic annotations into a region
 *
 * For each entry, creates a HarmonicAnnotation with the chord voicing
 * derived from the root and quality. Captures old annotations in the
 * region for undo.
 *
 * @param score Target score (modified in place)
 * @param region Region to annotate
 * @param progression Chord progression entries
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_section_harmony(
    Score& score,
    const ScoreRegion& region,
    std::vector<ChordSymbolEntry> progression,
    UndoStack* undo = nullptr
);

// =============================================================================
// Workflow Types (§12.2.4)
// =============================================================================

/**
 * @brief A single melodic note for write_melody
 */
struct MelodyEntry {
    ScoreTime position;
    SpelledPitch pitch;
    Beat duration;
    std::optional<DynamicLevel> dynamic;
    std::optional<ArticulationType> articulation;
};

/**
 * @brief A single harmonic voicing for write_harmony
 *
 * Pitches are ordered bottom to top. Distribution across target parts
 * is handled by the workflow function.
 */
struct HarmonyEntry {
    ScoreTime position;
    std::vector<SpelledPitch> voicing;  // bottom to top
    Beat duration;
};

// =============================================================================
// Composition Tool (§12.2.5)
// =============================================================================

/**
 * @brief Add a new part to the score, appended at the end
 *
 * @param score Target score (modified in place)
 * @param definition Part specification
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_add_part(
    Score& score, PartDefinition definition, UndoStack* undo = nullptr);

// =============================================================================
// Arrangement Tools (§12.2.6)
// =============================================================================

/**
 * @brief Write a melody line into a voice within a part
 *
 * @param score Target score (modified in place)
 * @param part_id Target part
 * @param voice_index Voice within the part
 * @param melody Sequence of melody entries
 * @param undo Optional undo stack
 * @return MutationResult or error if part not found
 */
[[nodiscard]] Result<MutationResult> write_melody(
    Score& score, PartId part_id, std::uint8_t voice_index,
    const std::vector<MelodyEntry>& melody, UndoStack* undo = nullptr);

/**
 * @brief Distribute harmonic voicings across target parts
 *
 * Assigns pitches from each HarmonyEntry bottom-to-top across the
 * target parts. When voicing.size() exceeds target_parts.size(),
 * surplus pitches are grouped as chords in the last part.
 *
 * @param score Target score (modified in place)
 * @param target_parts Parts to receive the voicing (bottom to top)
 * @param chords Harmonic entries
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> write_harmony(
    Score& score, const std::vector<PartId>& target_parts,
    const std::vector<HarmonyEntry>& chords, UndoStack* undo = nullptr);

/**
 * @brief Copy note events from source to target part within a region
 *
 * @param score Target score (modified in place)
 * @param region Time region for the operation
 * @param source Source part
 * @param target Target part
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_reorchestrate(
    Score& score, const ScoreRegion& region,
    PartId source, PartId target, UndoStack* undo = nullptr);

/**
 * @brief Double a part at a semitone interval into another part
 *
 * @param score Target score (modified in place)
 * @param region Time region for the operation
 * @param source Source part
 * @param target Target part
 * @param interval Semitone transposition interval
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_double_part(
    Score& score, const ScoreRegion& region,
    PartId source, PartId target, std::int8_t interval,
    UndoStack* undo = nullptr);

/**
 * @brief Set the dynamic level for all note events in a region
 *
 * @param score Target score (modified in place)
 * @param region Region to modify
 * @param level Dynamic level to apply
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_set_dynamics(
    Score& score, const ScoreRegion& region,
    DynamicLevel level, UndoStack* undo = nullptr);

/**
 * @brief Set the articulation for all notes in a region
 *
 * @param score Target score (modified in place)
 * @param region Region to modify
 * @param articulation Articulation to apply
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_set_articulation(
    Score& score, const ScoreRegion& region,
    ArticulationType articulation, UndoStack* undo = nullptr);

// =============================================================================
// Detail Tools (§12.2.7)
// =============================================================================

/**
 * @brief Insert a single note into a specific location
 *
 * @param score Target score (modified in place)
 * @param part Target part
 * @param bar Bar number (1-indexed)
 * @param voice Voice index within the bar
 * @param offset Beat offset within the bar
 * @param note Note to insert
 * @param duration Duration of the note
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_insert_note(
    Score& score, PartId part, std::uint32_t bar,
    std::uint8_t voice, Beat offset,
    Note note, Beat duration, UndoStack* undo = nullptr);

/**
 * @brief Modify properties of an existing note within a NoteGroup
 *
 * Only the provided optional fields are changed; others are left intact.
 *
 * @param score Target score (modified in place)
 * @param event_id Event containing the note
 * @param note_index Index within the NoteGroup
 * @param pitch New pitch (if provided)
 * @param duration New duration (if provided)
 * @param velocity New velocity (if provided)
 * @param articulation New articulation (if provided)
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_modify_note(
    Score& score, EventId event_id, std::uint8_t note_index,
    std::optional<SpelledPitch> pitch,
    std::optional<Beat> duration,
    std::optional<VelocityValue> velocity,
    std::optional<ArticulationType> articulation,
    UndoStack* undo = nullptr);

/**
 * @brief Delete an event, replacing it with a rest of equal duration
 *
 * @param score Target score (modified in place)
 * @param event_id Event to delete
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_delete_event(
    Score& score, EventId event_id, UndoStack* undo = nullptr);

/**
 * @brief Transpose a single event or an entire region by a diatonic interval
 *
 * @param score Target score (modified in place)
 * @param target Either a single EventId or a ScoreRegion
 * @param interval Diatonic interval for transposition
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> wf_transpose(
    Score& score, std::variant<EventId, ScoreRegion> target,
    DiatonicInterval interval, UndoStack* undo = nullptr);

// =============================================================================
// Analysis Tools — read-only (§12.2.8)
// =============================================================================

/**
 * @brief Analyse harmonic content within a region
 *
 * @param score Source score (unmodified)
 * @param region Region to analyse
 * @return Harmonic annotations covering the region
 */
[[nodiscard]] std::vector<HarmonicAnnotation> wf_analyze_harmony(
    const Score& score, const ScoreRegion& region);

/**
 * @brief Retrieve orchestration annotations for a region
 *
 * @param score Source score (unmodified)
 * @param region Region to query
 * @return Part-role pairs active in the region
 */
[[nodiscard]] std::vector<std::pair<PartId, TexturalRole>> wf_get_orchestration(
    const Score& score, const ScoreRegion& region);

/**
 * @brief Produce a reduced view of the score
 *
 * Supported view types: "piano" (piano reduction), "short" (short score),
 * "skeleton" (harmonic skeleton). Defaults to piano reduction for
 * unrecognised types.
 *
 * @param score Source score (unmodified)
 * @param view_type Reduction type
 * @param region Optional region restriction
 * @return Reduced Score
 */
[[nodiscard]] Score wf_get_reduction(
    const Score& score, const std::string& view_type,
    const std::optional<ScoreRegion>& region = std::nullopt);

/**
 * @brief Run all validation rules on the score
 *
 * @param score Source score (unmodified)
 * @return All diagnostics (empty = valid)
 */
[[nodiscard]] std::vector<Diagnostic> wf_validate_score(const Score& score);

/**
 * @brief Produce a condensed form summary of the score's sections
 *
 * @param score Source score (unmodified)
 * @return Form summary entries for each section
 */
[[nodiscard]] std::vector<FormSummaryEntry> wf_get_form_summary(const Score& score);

// =============================================================================
// Compilation Tool (§12.2.9)
// =============================================================================

/**
 * @brief Compile the score to MIDI event data
 *
 * @param score Source score; must satisfy is_compilable()
 * @param ppq Pulses per quarter note (default 480)
 * @return CompiledMidiResult or error
 */
[[nodiscard]] Result<CompiledMidiResult> wf_compile_to_midi(
    const Score& score, int ppq = 480);

}  // namespace Sunny::Core
