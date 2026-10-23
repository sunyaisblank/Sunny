/**
 * @file projection.hpp
 * @brief Exact symbolic note intervals and event-boundary partitions.
 */

#pragma once

#include <span>
#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

/**
 * One source Note's structural allocation, in absolute whole-note units.
 * Tuplet durations are already scaled in the Score. Grace notes retain their
 * positive Score allocation. This is symbolic sounding content, not an audio
 * estimate or the MIDI grace/articulation performance policy. A tied continuation
 * remains a separate provenance-bearing span, with attack=false.
 */
struct SymbolicNoteSpan {
    PartId part_id{};
    std::uint8_t voice_index = 0;
    EventId event_id{};
    std::size_t note_index = 0;
    ScoreTime source_position{};
    SpelledPitch pitch{};
    Beat start{};
    Beat end{};
    bool attack = true;
    bool grace = false;
    std::optional<GraceType> grace_type;
    // Stable identity of this individual note's original attack. Independent
    // unisons may share part/voice/pitch, so pitch alone cannot identify a tie.
    EventId attack_event_id{};
    std::size_t attack_note_index = 0;
};

/** A half-open interval of constant membership; indices address the input spans. */
struct SymbolicSlice {
    Beat start{};
    Beat end{};
    std::vector<std::size_t> sounding_indices;
    // Attacks occur exactly at start; sustained notes and tie continuations
    // remain in sounding_indices without creating another attack.
    std::vector<std::size_t> attack_indices;
};

/**
 * Project Score note allocations, retaining part/voice/event/note provenance.
 * The Score must have valid measured-event topology. Invalid durations, time
 * conversion, arithmetic, or broken duration ties return an error.
 */
[[nodiscard]] Result<std::vector<SymbolicNoteSpan>> project_symbolic_notes(const Score& score);

/** Fold each individual tie chain, retaining its head's identity and attack. */
[[nodiscard]] Result<std::vector<SymbolicNoteSpan>>
fold_symbolic_ties(std::span<const SymbolicNoteSpan> notes);

/**
 * Sweep exact starts/releases and optional context boundaries in increasing time.
 * Positive-duration, nonnegative spans are required. Releases precede attacks at
 * a shared boundary. Empty slices are retained between supplied boundaries.
 */
[[nodiscard]] Result<std::vector<SymbolicSlice>>
partition_symbolic_notes(std::span<const SymbolicNoteSpan> notes,
                         std::span<const Beat> extra_boundaries = {});

} // namespace sunny::core
