/**
 * @file FMMT001A.h
 * @brief Motivic Analysis and Transformations
 *
 * Component: FMMT001A
 * Domain: FM (Form) | Category: MT (Motivic Transformation)
 *
 * Formal Spec §10.4: A motif is a short melodic-rhythmic figure
 * that serves as a building block for larger structures. This component
 * provides motivic transformation operations and similarity detection.
 *
 * Transformations:
 *   Repetition, Transposition, Inversion, Retrograde,
 *   Retrograde-Inversion, Augmentation, Diminution,
 *   Fragmentation, Extension
 *
 * Invariants:
 * - retrograde(retrograde(m)) == m
 * - invert(invert(m)) == m
 * - transpose(m, 0) == m
 * - augment(diminish(m, k), k) == m (exact rational arithmetic)
 */

#pragma once

#include "FMST001A.h"
#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"

#include <span>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Motivic Transformation Type
// =============================================================================

/**
 * @brief Classification of motivic transformation
 */
enum class MotivicTransform {
    Repetition,
    Transposition,
    Inversion,
    Retrograde,
    RetrogradeInversion,
    Augmentation,
    Diminution,
    Fragmentation,
    Extension,
    Unknown
};

// =============================================================================
// Pitch-only Motif Operations
// =============================================================================

/**
 * @brief Transpose a pitch sequence by a given interval
 *
 * @param pitches Input pitch sequence
 * @param semitones Transposition amount
 * @return Transposed pitches (clamped to valid MIDI range)
 */
[[nodiscard]] std::vector<MidiNote> motif_transpose(
    std::span<const MidiNote> pitches,
    int semitones
);

/**
 * @brief Invert a pitch sequence around the first note
 *
 * Each interval from the first note is negated.
 *
 * @param pitches Input pitch sequence
 * @return Inverted pitches
 */
[[nodiscard]] std::vector<MidiNote> motif_invert(
    std::span<const MidiNote> pitches
);

/**
 * @brief Reverse a pitch sequence (retrograde)
 *
 * @param pitches Input pitch sequence
 * @return Reversed pitches
 */
[[nodiscard]] std::vector<MidiNote> motif_retrograde(
    std::span<const MidiNote> pitches
);

/**
 * @brief Apply retrograde-inversion to a pitch sequence
 *
 * Equivalent to invert(retrograde(pitches)).
 *
 * @param pitches Input pitch sequence
 * @return Retrograde-inverted pitches
 */
[[nodiscard]] std::vector<MidiNote> motif_retrograde_inversion(
    std::span<const MidiNote> pitches
);

// =============================================================================
// Duration Operations
// =============================================================================

/**
 * @brief Augment durations by a rational factor
 *
 * @param durations Input durations
 * @param factor Multiplication factor (e.g., Beat(2,1) for double)
 * @return Augmented durations
 */
[[nodiscard]] std::vector<Beat> motif_augment(
    std::span<const Beat> durations,
    Beat factor
);

/**
 * @brief Diminish durations by a rational factor
 *
 * @param durations Input durations
 * @param factor Division factor (e.g., Beat(2,1) for half speed)
 * @return Diminished durations
 */
[[nodiscard]] std::vector<Beat> motif_diminish(
    std::span<const Beat> durations,
    Beat factor
);

// =============================================================================
// Motif Similarity and Detection
// =============================================================================

/**
 * @brief Compute intervallic similarity between two pitch sequences
 *
 * Compares the interval sequences (successive differences).
 * Returns 1.0 for identical interval sequences, 0.0 for completely different.
 *
 * @param a First pitch sequence
 * @param b Second pitch sequence
 * @return Similarity ratio in [0, 1]
 */
[[nodiscard]] double motif_interval_similarity(
    std::span<const MidiNote> a,
    std::span<const MidiNote> b
);

/**
 * @brief Classify the transformation relating two pitch sequences
 *
 * Checks in order: Repetition, Transposition, Inversion, Retrograde,
 * Retrograde-Inversion. Returns the first match found.
 *
 * @param original Original motif pitches
 * @param transformed Transformed motif pitches
 * @return Detected transformation type
 */
[[nodiscard]] MotivicTransform classify_transformation(
    std::span<const MidiNote> original,
    std::span<const MidiNote> transformed
);

/**
 * @brief Extract a fragment (subsequence) of a motif's pitches
 *
 * @param pitches Full pitch sequence
 * @param start Start index
 * @param length Number of notes to extract
 * @return Fragment, or empty if bounds are invalid
 */
[[nodiscard]] std::vector<MidiNote> motif_fragment(
    std::span<const MidiNote> pitches,
    std::size_t start,
    std::size_t length
);

/**
 * @brief Get the standard name for a motivic transformation
 */
[[nodiscard]] constexpr std::string_view transform_name(MotivicTransform t) noexcept {
    switch (t) {
        case MotivicTransform::Repetition:            return "Repetition";
        case MotivicTransform::Transposition:         return "Transposition";
        case MotivicTransform::Inversion:             return "Inversion";
        case MotivicTransform::Retrograde:            return "Retrograde";
        case MotivicTransform::RetrogradeInversion:    return "Retrograde-Inversion";
        case MotivicTransform::Augmentation:           return "Augmentation";
        case MotivicTransform::Diminution:             return "Diminution";
        case MotivicTransform::Fragmentation:          return "Fragmentation";
        case MotivicTransform::Extension:              return "Extension";
        case MotivicTransform::Unknown:                return "Unknown";
    }
    return "Unknown";
}

}  // namespace Sunny::Core
