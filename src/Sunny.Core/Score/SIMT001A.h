/**
 * @file SIMT001A.h
 * @brief Score IR mutations — atomic document modifications with undo
 *
 * Component: SIMT001A
 * Domain: SI (Score IR) | Category: MT (Mutation)
 *
 * Implements the mutation operations from SS-IR §11. Each mutation:
 * 1. Modifies document state atomically
 * 2. Runs incremental validation on the affected region
 * 3. Marks affected annotation layers as stale
 * 4. Increments document version counter
 * 5. Pushes inverse operation onto the undo stack
 *
 * Invariant: mutation + inverse restores previous state exactly.
 * Invariant: version counter increases monotonically, never reused.
 */

#pragma once

#include "SIDC001A.h"
#include "../Pitch/PTDI001A.h"

#include <functional>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Mutation Result
// =============================================================================

/**
 * @brief Result of applying a mutation
 *
 * Contains diagnostics from incremental validation of the affected region.
 * The mutation has already been applied when this is returned.
 */
struct MutationResult {
    std::vector<Diagnostic> diagnostics;
};

// =============================================================================
// Undo/Redo (SS-IR §11.7)
// =============================================================================

/**
 * @brief Opaque undo entry stored on the undo stack
 *
 * Contains both a forward callable (re-applies the mutation) and an
 * inverse callable (restores previous state). Undo calls inverse;
 * redo calls forward.
 */
struct UndoEntry {
    std::uint64_t version;                ///< Version when mutation was applied
    std::function<VoidResult()> forward;  ///< Re-applies the mutation
    std::function<VoidResult()> inverse;  ///< Restores previous state
    std::string description;              ///< Human-readable mutation name
};

/**
 * @brief Undo stack for a Score document
 *
 * Maintains undo and redo stacks. Each mutation pushes its inverse
 * onto the undo stack. Undo pops and applies the inverse, pushing
 * the re-do onto the redo stack.
 */
struct UndoStack {
    std::vector<UndoEntry> undo_entries;
    std::vector<UndoEntry> redo_entries;

    [[nodiscard]] bool can_undo() const noexcept {
        return !undo_entries.empty();
    }
    [[nodiscard]] bool can_redo() const noexcept {
        return !redo_entries.empty();
    }
};

// =============================================================================
// Event-Level Mutations (SS-IR §11.2)
// =============================================================================

/**
 * @brief Insert a note into a voice at a given offset
 *
 * Creates a NoteGroup with a single note. Subsequent events are not
 * displaced — the caller must ensure the measure fills correctly.
 */
[[nodiscard]] Result<MutationResult> insert_note(
    Score& score,
    PartId part,
    std::uint32_t bar,
    std::uint8_t voice_index,
    Beat offset,
    Note note,
    Beat duration,
    UndoStack* undo = nullptr
);

/**
 * @brief Delete an event by id, replacing it with a rest of equal duration
 */
[[nodiscard]] Result<MutationResult> delete_event(
    Score& score,
    EventId event_id,
    UndoStack* undo = nullptr
);

/**
 * @brief Change the pitch of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_pitch(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    SpelledPitch new_pitch,
    UndoStack* undo = nullptr
);

/**
 * @brief Change the duration of an event
 */
[[nodiscard]] Result<MutationResult> modify_duration(
    Score& score,
    EventId event_id,
    Beat new_duration,
    UndoStack* undo = nullptr
);

/**
 * @brief Change the velocity of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_velocity(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    VelocityValue new_velocity,
    UndoStack* undo = nullptr
);

/**
 * @brief Set or change the articulation on a note
 */
[[nodiscard]] Result<MutationResult> set_articulation(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    std::optional<ArticulationType> articulation,
    UndoStack* undo = nullptr
);

/**
 * @brief Insert a dynamic marking at a position in a part
 */
[[nodiscard]] Result<MutationResult> set_dynamic(
    Score& score,
    PartId part_id,
    ScoreTime position,
    DynamicLevel level,
    UndoStack* undo = nullptr
);

/**
 * @brief Insert a hairpin (crescendo/diminuendo)
 */
[[nodiscard]] Result<MutationResult> insert_hairpin(
    Score& score,
    PartId part_id,
    ScoreTime start,
    ScoreTime end,
    HairpinType type,
    std::optional<DynamicLevel> target = std::nullopt,
    UndoStack* undo = nullptr
);

/**
 * @brief Set or clear tie-forward on a note
 */
[[nodiscard]] Result<MutationResult> set_tie(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    bool tied,
    UndoStack* undo = nullptr
);

/**
 * @brief Transpose all notes in an event by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_event(
    Score& score,
    EventId event_id,
    DiatonicInterval interval,
    UndoStack* undo = nullptr
);

// =============================================================================
// Measure-Level Mutations (SS-IR §11.3)
// =============================================================================

/**
 * @brief Insert empty measures after a given bar in all parts
 *
 * Updates global maps (tempo, key, time signature) accordingly.
 */
[[nodiscard]] Result<MutationResult> insert_measures(
    Score& score,
    std::uint32_t after_bar,
    std::uint32_t count,
    UndoStack* undo = nullptr
);

/**
 * @brief Delete measures from all parts
 *
 * Updates global maps accordingly.
 */
[[nodiscard]] Result<MutationResult> delete_measures(
    Score& score,
    std::uint32_t bar,
    std::uint32_t count,
    UndoStack* undo = nullptr
);

/**
 * @brief Set the time signature at a bar
 */
[[nodiscard]] Result<MutationResult> set_time_signature(
    Score& score,
    std::uint32_t bar,
    TimeSignature time_sig,
    UndoStack* undo = nullptr
);

/**
 * @brief Set the key signature at a position
 */
[[nodiscard]] Result<MutationResult> set_key_signature(
    Score& score,
    ScoreTime position,
    KeySignature key,
    UndoStack* undo = nullptr
);

// =============================================================================
// Part-Level Mutations (SS-IR §11.4)
// =============================================================================

/**
 * @brief Add a new part with empty measures
 */
[[nodiscard]] Result<MutationResult> add_part(
    Score& score,
    PartDefinition definition,
    std::size_t position_in_order,
    UndoStack* undo = nullptr
);

/**
 * @brief Remove a part and all its content
 */
[[nodiscard]] Result<MutationResult> remove_part(
    Score& score,
    PartId part_id,
    UndoStack* undo = nullptr
);

// =============================================================================
// Voice-Level Mutations (SS-IR §11.4.1)
// =============================================================================

/**
 * @brief Add a new voice to a measure in a part
 *
 * Creates a voice with the given voice_number containing a single
 * whole-measure rest event.
 */
[[nodiscard]] Result<MutationResult> add_voice(
    Score& score,
    std::uint32_t bar,
    PartId part_id,
    std::uint8_t voice_number,
    UndoStack* undo = nullptr
);

/**
 * @brief Remove a voice from a measure in a part
 *
 * Fails if the voice is the last remaining voice in the measure.
 */
[[nodiscard]] Result<MutationResult> remove_voice(
    Score& score,
    std::uint32_t bar,
    PartId part_id,
    std::uint8_t voice_number,
    UndoStack* undo = nullptr
);

// =============================================================================
// Part Management Mutations (SS-IR §11.4.2)
// =============================================================================

/**
 * @brief Reorder parts to match the given sequence
 *
 * All PartIds in new_order must exist and the count must match.
 */
[[nodiscard]] Result<MutationResult> reorder_parts(
    Score& score,
    std::vector<PartId> new_order,
    UndoStack* undo = nullptr
);

/**
 * @brief Add a directive to a part
 */
[[nodiscard]] Result<MutationResult> set_part_directive(
    Score& score,
    PartId part_id,
    PartDirective directive,
    UndoStack* undo = nullptr
);

/**
 * @brief Change the instrument type of a part
 */
[[nodiscard]] Result<MutationResult> assign_instrument(
    Score& score,
    PartId part_id,
    InstrumentType instrument,
    UndoStack* undo = nullptr
);

// =============================================================================
// Region-Level Mutations (SS-IR §11.5)
// =============================================================================

// ScoreRegion is defined in SIDC001A.h

/**
 * @brief Transpose all notes in a region by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_region(
    Score& score,
    const ScoreRegion& region,
    DiatonicInterval interval,
    UndoStack* undo = nullptr
);

/**
 * @brief Replace all content in a region with rests
 */
[[nodiscard]] Result<MutationResult> delete_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo = nullptr
);

/**
 * @brief Copy events from a source region to a destination position
 */
[[nodiscard]] Result<MutationResult> copy_region(
    Score& score,
    const ScoreRegion& src,
    ScoreTime dest,
    UndoStack* undo = nullptr
);

/**
 * @brief Move events from a source region to a destination position
 *
 * Copies the region to dest, then replaces the source with rests.
 */
[[nodiscard]] Result<MutationResult> move_region(
    Score& score,
    const ScoreRegion& src,
    ScoreTime dest,
    UndoStack* undo = nullptr
);

/**
 * @brief Set the dynamic level on the first note of every NoteGroup in a region
 */
[[nodiscard]] Result<MutationResult> set_dynamic_region(
    Score& score,
    const ScoreRegion& region,
    DynamicLevel level,
    UndoStack* undo = nullptr
);

/**
 * @brief Scale velocity of all notes in a region by a factor
 *
 * Clamps the result to [0, 127].
 */
[[nodiscard]] Result<MutationResult> scale_velocity_region(
    Score& score,
    const ScoreRegion& region,
    double factor,
    UndoStack* undo = nullptr
);

/**
 * @brief Reverse the order of events within each voice in a region
 */
[[nodiscard]] Result<MutationResult> retrograde_region(
    Score& score,
    const ScoreRegion& region,
    UndoStack* undo = nullptr
);

/**
 * @brief Invert pitches around an axis pitch within a region
 */
[[nodiscard]] Result<MutationResult> invert_region(
    Score& score,
    const ScoreRegion& region,
    SpelledPitch axis,
    UndoStack* undo = nullptr
);

/**
 * @brief Multiply event durations in a region by a Beat factor
 */
[[nodiscard]] Result<MutationResult> augment_region(
    Score& score,
    const ScoreRegion& region,
    Beat factor,
    UndoStack* undo = nullptr
);

/**
 * @brief Divide event durations in a region by a Beat factor
 */
[[nodiscard]] Result<MutationResult> diminute_region(
    Score& score,
    const ScoreRegion& region,
    Beat factor,
    UndoStack* undo = nullptr
);

// =============================================================================
// Orchestration Mutations (SS-IR §11.6)
// =============================================================================

/**
 * @brief Copy all note events from a region to a target part at the same positions
 */
[[nodiscard]] Result<MutationResult> reorchestrate(
    Score& score,
    const ScoreRegion& region,
    PartId target,
    UndoStack* undo = nullptr
);

/**
 * @brief Copy notes from a region to a target part, transposing by semitone interval
 */
[[nodiscard]] Result<MutationResult> double_at_interval(
    Score& score,
    const ScoreRegion& region,
    PartId target,
    std::int8_t interval,
    UndoStack* undo = nullptr
);

/**
 * @brief Set the textural role for a part within a region
 */
[[nodiscard]] Result<MutationResult> set_texture_role(
    Score& score,
    const ScoreRegion& region,
    PartId part_id,
    TexturalRole role,
    UndoStack* undo = nullptr
);

/**
 * @brief Apply a voice leading style to a region (placeholder — bumps version)
 *
 * Full implementation requires harmonic analysis context.
 */
[[nodiscard]] Result<MutationResult> apply_voice_leading(
    Score& score,
    const ScoreRegion& region,
    VoiceLeadingStyle style,
    UndoStack* undo = nullptr
);

// =============================================================================
// Undo/Redo Operations
// =============================================================================

/**
 * @brief Undo the most recent mutation
 */
[[nodiscard]] VoidResult undo(Score& score, UndoStack& stack);

/**
 * @brief Redo the most recently undone mutation
 */
[[nodiscard]] VoidResult redo(Score& score, UndoStack& stack);

}  // namespace Sunny::Core
