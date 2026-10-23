/**
 * @file mutations.hpp
 * @brief Score IR mutations — atomic document modifications with undo
 *
 *
 * Implements the mutation operations from SS-IR §11. Each mutation:
 * 1. Modifies document state atomically
 * 2. Runs incremental validation on the affected region
 * 3. Marks affected annotation layers as stale
 * 4. Increments document version counter
 * 5. Pushes a full pre-mutation document snapshot onto the undo stack
 *
 * Undo and redo swap the live document with a stored snapshot. Multi-part
 * mutations are atomic: a mid-mutation failure restores the pre-mutation
 * snapshot before the error propagates.
 *
 * Successful mutations and history restoration may replace Score storage.
 * Callers must reacquire nested references/pointers/iterators from the current
 * Score; stable typed identities, not addresses, identify restored content.
 *
 * Invariant: undo restores pre-mutation content with its original typed IDs;
 * version and observed/retired identity reservations remain monotonic.
 * Invariant: version counter increases monotonically, never reused.
 * At UINT64_MAX, every state-changing operation returns ArithmeticOverflow
 * before changing the Score or its UndoStack. Raw mutation entry points check
 * exhaustion before operation-specific arguments, so exhaustion has precedence.
 */

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <sunny/core/pitch/diatonic_interval.hpp>
#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

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
 * @brief Undo entry storing a full document snapshot
 *
 * Holds the document state captured before the mutation was applied.
 * Undo swaps the live document with this snapshot; redo swaps back.
 * The shared_ptr keeps moves between the undo and redo stacks cheap
 * and the snapshot immutable once captured.
 */
struct UndoEntry {
    std::uint64_t version;              ///< Version of the snapshot state
    std::shared_ptr<const Score> state; ///< Document before the mutation
    std::string description;            ///< Human-readable mutation name
};

/**
 * @brief Undo stack for a Score document
 *
 * Maintains undo and redo stacks of document snapshots. Each mutation
 * pushes its pre-mutation state. Undo saves the live document onto the
 * redo stack and restores the popped snapshot; redo mirrors this.
 *
 * Invariant: undo_entries.size() <= capacity. Every entry is a full Score
 * copy, so recording beyond the capacity discards the oldest snapshot rather
 * than growing without bound. Redo entries are created only by undo, so they
 * never outnumber the capacity either.
 */
struct UndoStack {
    /// Default number of retained undo snapshots.
    static constexpr std::size_t DEFAULT_CAPACITY = 64;

    std::vector<UndoEntry> undo_entries;
    std::vector<UndoEntry> redo_entries;
    std::size_t capacity = DEFAULT_CAPACITY; ///< Zero disables history

    std::uint32_t group_depth = 0;
    bool group_has_snapshot = false;
    std::string group_description;

    [[nodiscard]] bool can_undo() const noexcept { return !undo_entries.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_entries.empty(); }

    /**
     * @brief Record the document state captured before one mutation
     *
     * Clears redo history. Inside a group only the group's first snapshot is
     * kept, labelled with the group description, because it covers the whole
     * group. Postcondition: the capacity invariant holds.
     */
    void record(Score before, std::string description);

    void begin_group(std::string description);
    void end_group() noexcept;
};

/**
 * @brief RAII guard for undo grouping
 *
 * Calls begin_group on construction and end_group on destruction,
 * ensuring the group closes even if an exception unwinds the stack.
 */
struct UndoGroup {
    UndoStack& stack;
    explicit UndoGroup(UndoStack& s, std::string desc) : stack(s) {
        stack.begin_group(std::move(desc));
    }
    ~UndoGroup() noexcept { stack.end_group(); }
    UndoGroup(const UndoGroup&) = delete;
    UndoGroup& operator=(const UndoGroup&) = delete;
};

// =============================================================================
// Event-Level Mutations (SS-IR §11.2)
// =============================================================================

/**
 * @brief Insert a note into a voice at a given offset
 *
 * Replaces the covered rest span while preserving prefix/suffix silence.
 * An exactly coincident NoteGroup receives the note as a chord member;
 * partial musical overlap, an invalid range, or a non-filling result fails
 * without mutating the score.
 */
[[nodiscard]] Result<MutationResult> insert_note(Score& score,
                                                 PartId part,
                                                 std::uint32_t bar,
                                                 std::uint8_t voice_index,
                                                 Beat offset,
                                                 Note note,
                                                 Beat duration,
                                                 UndoStack* undo = nullptr);

/**
 * @brief Insert one zero-duration harmonic symbol at an exact voice offset
 *
 * The symbol may coincide with measured content. Its structured numeral,
 * inversion, and degree payload is validated atomically with the document.
 */
[[nodiscard]] Result<MutationResult> insert_chord_symbol(Score& score,
                                                         PartId part,
                                                         std::uint32_t bar,
                                                         std::uint8_t voice_index,
                                                         Beat offset,
                                                         ChordSymbolEvent symbol,
                                                         UndoStack* undo = nullptr);

/**
 * @brief Delete an event by id, replacing measured content with an equal rest
 *
 * A slur/glissando mate is cleared atomically. Paired ottava/pedal Directions
 * are removed together, including an exact duplicate sustain PartDirective.
 */
[[nodiscard]] Result<MutationResult>
delete_event(Score& score, EventId event_id, UndoStack* undo = nullptr);

/**
 * @brief Change the pitch of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_pitch(Score& score,
                                                  EventId event_id,
                                                  std::uint8_t note_index,
                                                  SpelledPitch new_pitch,
                                                  UndoStack* undo = nullptr);

/**
 * @brief Change one measured event's fixed-onset duration atomically
 *
 * The replacement may consume only rest coverage; shortening creates rest
 * coverage. A collision with musical content, a measure-boundary crossing, or
 * an invalid tuplet result rejects the mutation without changing the score.
 */
[[nodiscard]] Result<MutationResult>
modify_duration(Score& score, EventId event_id, Beat new_duration, UndoStack* undo = nullptr);

/**
 * @brief Change the velocity of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_velocity(Score& score,
                                                     EventId event_id,
                                                     std::uint8_t note_index,
                                                     VelocityValue new_velocity,
                                                     UndoStack* undo = nullptr);

/**
 * @brief Change the Note Off velocity of a note within a NoteGroup
 */
[[nodiscard]] Result<MutationResult> modify_release_velocity(Score& score,
                                                             EventId event_id,
                                                             std::uint8_t note_index,
                                                             std::uint8_t new_release_velocity,
                                                             UndoStack* undo = nullptr);

/**
 * @brief Set or change the articulation on a note
 */
[[nodiscard]] Result<MutationResult> set_articulation(Score& score,
                                                      EventId event_id,
                                                      std::uint8_t note_index,
                                                      std::optional<ArticulationType> articulation,
                                                      UndoStack* undo = nullptr);

/**
 * @brief Insert a dynamic marking at a position in a part
 */
[[nodiscard]] Result<MutationResult> set_dynamic(Score& score,
                                                 PartId part_id,
                                                 ScoreTime position,
                                                 DynamicLevel level,
                                                 UndoStack* undo = nullptr);

/**
 * @brief Insert a hairpin (crescendo/diminuendo)
 */
[[nodiscard]] Result<MutationResult>
insert_hairpin(Score& score,
               PartId part_id,
               ScoreTime start,
               ScoreTime end,
               HairpinType type,
               std::optional<DynamicLevel> target = std::nullopt,
               UndoStack* undo = nullptr);

/**
 * @brief Set or clear tie-forward on a note
 */
[[nodiscard]] Result<MutationResult> set_tie(
    Score& score, EventId event_id, std::uint8_t note_index, bool tied, UndoStack* undo = nullptr);

/**
 * @brief Transpose all notes in an event by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_event(Score& score,
                                                     EventId event_id,
                                                     DiatonicInterval interval,
                                                     UndoStack* undo = nullptr);

// =============================================================================
// Measure-Level Mutations (SS-IR §11.3)
// =============================================================================

/**
 * @brief Insert empty measures after a given bar in all parts
 *
 * Updates global maps (tempo, key, time signature) accordingly.
 */
[[nodiscard]] Result<MutationResult> insert_measures(Score& score,
                                                     std::uint32_t after_bar,
                                                     std::uint32_t count,
                                                     UndoStack* undo = nullptr);

/**
 * @brief Delete measures from all parts
 *
 * Updates global maps accordingly.
 */
[[nodiscard]] Result<MutationResult>
delete_measures(Score& score, std::uint32_t bar, std::uint32_t count, UndoStack* undo = nullptr);

/**
 * @brief Replace the complete score-level sounding-pitch function atomically
 *
 * Rejects an invalid reference or any non-finite/unrenderable table entry.
 */
[[nodiscard]] Result<MutationResult>
set_score_tuning(Score& score, ScoreTuning tuning, UndoStack* undo = nullptr);

/**
 * Replace the complete ordered TempoMap atomically, using exact typed rates.
 * Incoming Linear durations and MetricModulation rates must already satisfy
 * S24. No sorting, interpolation approximation, or payload repair is performed.
 */
[[nodiscard]] Result<MutationResult>
set_tempo_map(Score& score, TempoMap tempo_map, UndoStack* undo = nullptr);

/**
 * Annotate a standalone tuplet over already-scaled note/rest allocations.
 * Members must be nonempty, distinct, and form one contiguous
 * measured span in one voice/measure. Their sum must equal scaled_allocation
 * and normal * normal_type. actual counts nominal rhythmic units rather than
 * events, so mixed written durations are allowed. Durations/onsets/IDs remain
 * unchanged. Existing tuplet membership and nested editing are rejected; point events may lie
 * between members but cannot themselves be members. Returns the fresh TupletId.
 */
[[nodiscard]] Result<TupletId> create_tuplet_group(Score& score,
                                                   std::span<const EventId> members,
                                                   std::uint8_t actual,
                                                   std::uint8_t normal,
                                                   Beat normal_type,
                                                   Beat scaled_allocation,
                                                   UndoStack* undo = nullptr);

/**
 * Remove a standalone tuplet annotation while retaining all scaled durations.
 * Parent/child nesting conflicts reject the operation. The removed TupletId
 * remains reserved through deletion, undo/redo, and canonical serialization.
 */
[[nodiscard]] Result<MutationResult>
remove_tuplet_group(Score& score, TupletId tuplet_id, UndoStack* undo = nullptr);

/**
 * @brief Set the time signature at a bar atomically
 *
 * Retiles only rest coverage through the next meter change, recomputes exact
 * incoming Linear tempo durations, and rejects clipping musical/point events.
 */
[[nodiscard]] Result<MutationResult> set_time_signature(Score& score,
                                                        std::uint32_t bar,
                                                        const TimeSignature& time_sig,
                                                        UndoStack* undo = nullptr);

/**
 * @brief Set the key signature at an in-score, in-meter position
 *
 * Rejects a structurally invalid resulting key identity atomically.
 */
[[nodiscard]] Result<MutationResult>
set_key_signature(Score& score, ScoreTime position, KeySignature key, UndoStack* undo = nullptr);

// =============================================================================
// Part-Level Mutations (SS-IR §11.4)
// =============================================================================

/**
 * @brief Add a new part with empty measures
 */
[[nodiscard]] Result<MutationResult> add_part(Score& score,
                                              PartDefinition definition,
                                              std::size_t position_in_order,
                                              UndoStack* undo = nullptr);

/**
 * @brief Remove a part and all its content
 */
[[nodiscard]] Result<MutationResult>
remove_part(Score& score, PartId part_id, UndoStack* undo = nullptr);

// =============================================================================
// Voice-Level Mutations (SS-IR §11.4.1)
// =============================================================================

/**
 * @brief Add a new voice to a measure in a part
 *
 * Creates a voice with the given voice_number containing a single
 * whole-measure rest event.
 */
[[nodiscard]] Result<MutationResult> add_voice(Score& score,
                                               std::uint32_t bar,
                                               PartId part_id,
                                               std::uint8_t voice_number,
                                               UndoStack* undo = nullptr);

/**
 * @brief Add a new voice to an explicit staff in a measure
 *
 * The staff index is 0-based and must be below the Part's staff_count.
 */
[[nodiscard]] Result<MutationResult> add_voice(Score& score,
                                               std::uint32_t bar,
                                               PartId part_id,
                                               std::uint8_t voice_number,
                                               std::uint8_t staff_index,
                                               UndoStack* undo = nullptr);

/**
 * @brief Remove a voice from a measure in a part
 *
 * Fails if the voice is the last remaining voice in the measure.
 */
[[nodiscard]] Result<MutationResult> remove_voice(Score& score,
                                                  std::uint32_t bar,
                                                  PartId part_id,
                                                  std::uint8_t voice_number,
                                                  UndoStack* undo = nullptr);

// =============================================================================
// Part Management Mutations (SS-IR §11.4.2)
// =============================================================================

/**
 * @brief Reorder parts to match the given sequence
 *
 * All PartIds in new_order must exist and the count must match.
 */
[[nodiscard]] Result<MutationResult>
reorder_parts(Score& score, const std::vector<PartId>& new_order, UndoStack* undo = nullptr);

/**
 * @brief Add a directive to a part
 */
[[nodiscard]] Result<MutationResult> set_part_directive(Score& score,
                                                        PartId part_id,
                                                        PartDirective directive,
                                                        UndoStack* undo = nullptr);

/**
 * @brief Apply the standard range, transposition and default clef of an instrument
 *
 * Concert note pitches, identity, explicit staff clefs, articulation mappings,
 * custom metadata and rendering configuration are preserved. Consequently
 * sounding MIDI stays the same and written-pitch export follows the new
 * instrument transposition. Pitched/unpitched percussion role changes reject
 * the edit because they require an explicit channel/key-map migration.
 */
[[nodiscard]] Result<MutationResult> assign_instrument(Score& score,
                                                       PartId part_id,
                                                       InstrumentType instrument,
                                                       UndoStack* undo = nullptr);

/**
 * @brief Set or remove one articulation mapping on a part
 *
 * The mapping is validated before the document is mutated. Passing nullopt
 * removes the mapping and restores the compiler's default articulation rule.
 */
[[nodiscard]] Result<MutationResult>
set_articulation_mapping(Score& score,
                         PartId part_id,
                         ArticulationType articulation,
                         std::optional<ArticulationMapping> mapping,
                         UndoStack* undo = nullptr);

// =============================================================================
// Region-Level Mutations (SS-IR §11.5)
// =============================================================================

// ScoreRegion is defined in.h

/**
 * @brief Transpose all notes in a region by a diatonic interval
 */
[[nodiscard]] Result<MutationResult> transpose_region(Score& score,
                                                      const ScoreRegion& region,
                                                      DiatonicInterval interval,
                                                      UndoStack* undo = nullptr);

/**
 * @brief Replace measured content in a region with rests and remove point events
 *
 * Any paired-notation endpoint deleted at the boundary atomically repairs its
 * mate, so a valid input cannot be left with an orphan S23 span.
 */
[[nodiscard]] Result<MutationResult>
delete_region(Score& score, const ScoreRegion& region, UndoStack* undo = nullptr);

/**
 * @brief Copy note events from a source region to a destination position
 *
 * Covered rests are replaced transactionally and exact coincident ordinary
 * notes form chords. Fully selected slur/glissando spans are preserved;
 * endpoints whose mates cross the selection boundary are stripped from clones.
 */
[[nodiscard]] Result<MutationResult>
copy_region(Score& score, const ScoreRegion& src, ScoreTime dest, UndoStack* undo = nullptr);

/**
 * @brief Move events from a source region to a destination position
 *
 * Copies the region to dest, then replaces the source with rests.
 */
[[nodiscard]] Result<MutationResult>
move_region(Score& score, const ScoreRegion& src, ScoreTime dest, UndoStack* undo = nullptr);

/**
 * @brief Set the dynamic level on the first note of every NoteGroup in a region
 */
[[nodiscard]] Result<MutationResult> set_dynamic_region(Score& score,
                                                        const ScoreRegion& region,
                                                        DynamicLevel level,
                                                        UndoStack* undo = nullptr);

/**
 * @brief Scale velocity of all notes in a region by a factor
 *
 * Clamps the result to [0, 127].
 */
[[nodiscard]] Result<MutationResult> scale_velocity_region(Score& score,
                                                           const ScoreRegion& region,
                                                           double factor,
                                                           UndoStack* undo = nullptr);

/**
 * @brief Reverse measured payloads within each affected measure
 *
 * Reversed durations are repacked from the first selected onset; point events
 * retain their exact time. Slur/glissando spans touching the region are removed
 * with a MUT1 warning because the current Boolean endpoints have no stable
 * transformed identity.
 */
[[nodiscard]] Result<MutationResult>
retrograde_region(Score& score, const ScoreRegion& region, UndoStack* undo = nullptr);

/**
 * @brief Invert pitches around an axis pitch within a region
 */
[[nodiscard]] Result<MutationResult> invert_region(Score& score,
                                                   const ScoreRegion& region,
                                                   SpelledPitch axis,
                                                   UndoStack* undo = nullptr);

/**
 * @brief Multiply fixed-onset NoteGroup durations in a region by a Beat factor
 *
 * Rest coverage is reconstructed. Tuplet members and growth into musical
 * content are rejected atomically.
 */
[[nodiscard]] Result<MutationResult>
augment_region(Score& score, const ScoreRegion& region, Beat factor, UndoStack* undo = nullptr);

/**
 * @brief Divide fixed-onset NoteGroup durations in a region by a Beat factor
 *
 * Rest coverage is reconstructed. Tuplet members are rejected atomically.
 */
[[nodiscard]] Result<MutationResult>
diminute_region(Score& score, const ScoreRegion& region, Beat factor, UndoStack* undo = nullptr);

// =============================================================================
// Orchestration Mutations (SS-IR §11.6)
// =============================================================================

/**
 * @brief Copy all note events from a region to a target part at the same positions
 *
 * Voice-scoped slur/glissando metadata is not portable across Parts and is
 * removed with a MUT1 warning.
 */
[[nodiscard]] Result<MutationResult>
reorchestrate(Score& score, const ScoreRegion& region, PartId target, UndoStack* undo = nullptr);

/**
 * @brief Copy notes from a region to a target part, transposing by a diatonic interval
 *
 * Each copy is spelled apply_interval(source, interval), so D4 up a perfect
 * fifth is A4 rather than a D with seven sharps. A result outside the
 * SpelledPitch domain rejects the mutation. Voice-scoped slur/glissando
 * metadata is removed with a MUT1 warning.
 */
[[nodiscard]] Result<MutationResult> double_at_interval(Score& score,
                                                        const ScoreRegion& region,
                                                        PartId target,
                                                        DiatonicInterval interval,
                                                        UndoStack* undo = nullptr);

/**
 * @brief Set the textural role for a part within a region
 */
[[nodiscard]] Result<MutationResult> set_texture_role(Score& score,
                                                      const ScoreRegion& region,
                                                      PartId part_id,
                                                      TexturalRole role,
                                                      UndoStack* undo = nullptr);

/**
 * @brief Apply a voice leading style to notes in a region
 *
 * Reads harmonic annotations in the region and voice-leads notes at
 * each chord boundary using the nearest-tone algorithm, subject to
 * constraints determined by the style parameter.
 *
 * Precondition: harmonic annotations must exist in the region.
 */
[[nodiscard]] Result<MutationResult> apply_voice_leading(Score& score,
                                                         const ScoreRegion& region,
                                                         VoiceLeadingStyle style,
                                                         UndoStack* undo = nullptr);

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

} // namespace sunny::core
