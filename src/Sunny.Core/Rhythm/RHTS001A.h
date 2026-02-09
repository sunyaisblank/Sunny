/**
 * @file RHTS001A.h
 * @brief Time signature and metre analysis
 *
 * Component: RHTS001A
 * Domain: RH (Rhythm) | Category: TS (Time Signature)
 *
 * Represents time signatures with beat groupings, classifies metre type,
 * computes metrical weight hierarchies, and detects syncopation.
 *
 * Invariants:
 * - numerator() == sum(groups)
 * - denominator > 0 and is a power of 2
 * - measure_duration uses exact Beat arithmetic
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"

#include <vector>

namespace Sunny::Core {

/**
 * @brief Classification of metre type
 */
enum class MetreType {
    Simple,     ///< All groups equal, each group = 2 subdivisions (e.g. 4/4, 2/4, 3/4)
    Compound,   ///< All groups equal, each group = 3 subdivisions (e.g. 6/8, 9/8, 12/8)
    Asymmetric, ///< Unequal groups from a mix of 2s and 3s (e.g. 5/8, 7/8)
    Complex     ///< Groups that don't fit simple/compound/asymmetric
};

/**
 * @brief Time signature with beat grouping structure
 */
struct TimeSignature {
    std::vector<int> groups;  ///< Beat groupings, e.g. {3,3,2} for 8/8
    int denominator;          ///< Note value per pulse (must be power of 2)

    [[nodiscard]] int numerator() const noexcept {
        int sum = 0;
        for (int g : groups) sum += g;
        return sum;
    }

    [[nodiscard]] Beat measure_duration() const noexcept {
        return Beat{numerator(), denominator};
    }

    [[nodiscard]] Beat beat_duration() const noexcept {
        return Beat{1, denominator};
    }
};

/**
 * @brief Construct a simple time signature
 *
 * For simple metre (2/4, 3/4, 4/4): each beat is one group.
 * For compound metre (6/8, 9/8, 12/8): groups of 3.
 *
 * @param num Numerator (total pulses)
 * @param denom Denominator (note value, must be power of 2)
 * @return TimeSignature or error
 */
[[nodiscard]] Result<TimeSignature> make_time_signature(int num, int denom);

/**
 * @brief Construct a time signature with explicit groupings
 *
 * @param groups Beat groupings (e.g. {3,3,2} for 8/8 aksak)
 * @param denom Denominator (note value, must be power of 2)
 * @return TimeSignature or error
 */
[[nodiscard]] Result<TimeSignature> make_additive_time_signature(
    std::vector<int> groups,
    int denom
);

/**
 * @brief Classify the metre type of a time signature
 */
[[nodiscard]] MetreType classify_metre(const TimeSignature& ts) noexcept;

/**
 * @brief Compute metrical weight at a pulse position
 *
 * Higher values indicate stronger metric positions.
 * Beat 0 (downbeat) has the highest weight.
 *
 * @param ts Time signature
 * @param pulse_position Position in pulses from start of measure (0-based)
 * @return Metrical weight (higher = stronger)
 */
[[nodiscard]] int metrical_weight(const TimeSignature& ts, int pulse_position) noexcept;

/**
 * @brief Check if a note is syncopated
 *
 * A note is syncopated if it begins on a weak beat and sustains through
 * a stronger beat position.
 *
 * @param ts Time signature
 * @param onset_pos Onset position in pulses (0-based)
 * @param duration_pulses Note duration in pulses
 * @return true if syncopated
 */
[[nodiscard]] bool is_syncopated(
    const TimeSignature& ts,
    int onset_pos,
    int duration_pulses
) noexcept;

}  // namespace Sunny::Core
