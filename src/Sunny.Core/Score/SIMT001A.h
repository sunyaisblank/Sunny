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
    Beat duration
);

/**
 * @brief Delete an event by id, replacing it with a rest of equal duration
 */
[[nodiscard]] Result<MutationResult> delete_event(
    Score& score,
    EventId event_id
);

/**
 * @brief Change the pitch of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_pitch(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    SpelledPitch new_pitch
);

/**
 * @brief Change the duration of an event
 */
[[nodiscard]] Result<MutationResult> modify_duration(
    Score& score,
    EventId event_id,
    Beat new_duration
);

/**
 * @brief Change the velocity of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_velocity(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    VelocityValue new_velocity
);

/**
 * @brief Set or change the articulation on a note
 */
[[nodiscard]] Result<MutationResult> set_articulation(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    std::optional<ArticulationType> articulation
);

/**
 * @brief Insert a dynamic marking at a position in a part
 */
[[nodiscard]] Result<MutationResult> set_dynamic(
    Score& score,
    PartId part_id,
    ScoreTime position,
    DynamicLevel level
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
    std::optional<DynamicLevel> target = std::nullopt
);

/**
 * @brief Set or clear tie-forward on a note
 */
[[nodiscard]] Result<MutationResult> set_tie(
    Score& score,
    EventId event_id,
    std::uint8_t note_index,
    bool tied
);

/**
 * @brief Transpose all notes in an event by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_event(
    Score& score,
    EventId event_id,
    DiatonicInterval interval
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
    std::uint32_t count
);

/**
 * @brief Delete measures from all parts
 *
 * Updates global maps accordingly.
 */
[[nodiscard]] Result<MutationResult> delete_measures(
    Score& score,
    std::uint32_t bar,
    std::uint32_t count
);

/**
 * @brief Set the time signature at a bar
 */
[[nodiscard]] Result<MutationResult> set_time_signature(
    Score& score,
    std::uint32_t bar,
    TimeSignature time_sig
);

/**
 * @brief Set the key signature at a position
 */
[[nodiscard]] Result<MutationResult> set_key_signature(
    Score& score,
    ScoreTime position,
    KeySignature key
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
    std::size_t position_in_order
);

/**
 * @brief Remove a part and all its content
 */
[[nodiscard]] Result<MutationResult> remove_part(
    Score& score,
    PartId part_id
);

// =============================================================================
// Region-Level Mutations (SS-IR §11.5)
// =============================================================================

/**
 * @brief A rectangular region of the score
 */
struct ScoreRegion {
    ScoreTime start;
    ScoreTime end;
    std::vector<PartId> parts;  ///< Empty = all parts
};

/**
 * @brief Transpose all notes in a region by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_region(
    Score& score,
    const ScoreRegion& region,
    DiatonicInterval interval
);

/**
 * @brief Replace all content in a region with rests
 */
[[nodiscard]] Result<MutationResult> delete_region(
    Score& score,
    const ScoreRegion& region
);

// =============================================================================
// Undo/Redo (SS-IR §11.7)
// =============================================================================

/**
 * @brief Opaque undo entry stored on the undo stack
 *
 * Contains a callable that reverses the mutation and the version
 * at which the mutation was applied.
 */
struct UndoEntry {
    std::uint64_t version;                ///< Version when mutation was applied
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

/**
 * @brief Undo the most recent mutation
 */
[[nodiscard]] VoidResult undo(Score& score, UndoStack& stack);

/**
 * @brief Redo the most recently undone mutation
 */
[[nodiscard]] VoidResult redo(Score& score, UndoStack& stack);

}  // namespace Sunny::Core
