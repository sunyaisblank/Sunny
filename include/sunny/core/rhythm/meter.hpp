/**
 * @file meter.hpp
 * @brief Time signature and metre analysis
 *
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

#include <cassert>
#include <exception>
#include <limits>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <utility>
#include <vector>

namespace sunny::core {

/**
 * @brief Classification of metre type
 */
enum class MetreType {
    Simple,     ///< Equal groups of one or two pulses (e.g. canonical 4/4 or grouped 4/8)
    Compound,   ///< Equal groups of three pulses (e.g. 6/8, 9/8, 12/8)
    Asymmetric, ///< Unequal groups from a mix of twos and threes (e.g. 3+2/8, 3+2+2/8)
    Complex     ///< Groups that don't fit simple/compound/asymmetric
};

/**
 * @brief Time signature with beat grouping structure
 */
class TimeSignature {
  public:
    /** @brief Safe default metre: canonical 4/4 grouped as four quarter-note pulses. */
    TimeSignature() : groups_{1, 1, 1, 1} {}

    /** @brief Construct an already-admitted grouped metre at a programming-contract boundary. */
    TimeSignature(std::vector<int> groups, int denominator) noexcept
        : groups_(std::move(groups)), denominator_(denominator) {
        const bool valid = valid_components(groups_, denominator_);
        assert(valid && "TimeSignature requires positive groups and a power-of-two denominator");
        if (!valid) std::terminate();
    }

    /** @brief Construct a grouped metre from dynamic input without violating the invariant. */
    [[nodiscard]] static Result<TimeSignature> from_groups(std::vector<int> groups,
                                                           int denominator) {
        if (!valid_components(groups, denominator))
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        return TimeSignature{std::move(groups), denominator};
    }

    [[nodiscard]] const std::vector<int>& groups() const noexcept { return groups_; }
    [[nodiscard]] int denominator() const noexcept { return denominator_; }

    [[nodiscard]] int numerator() const noexcept {
        int sum = 0;
        for (int group : groups_)
            sum += group;
        return sum;
    }

    [[nodiscard]] Beat measure_duration() const noexcept { return Beat{numerator(), denominator_}; }

    [[nodiscard]] Beat beat_duration() const noexcept { return Beat{1, denominator_}; }

    bool operator==(const TimeSignature&) const noexcept = default;

  private:
    [[nodiscard]] static bool valid_components(const std::vector<int>& groups,
                                               int denominator) noexcept {
        if (groups.empty() || denominator <= 0 || (denominator & (denominator - 1)) != 0)
            return false;

        int numerator = 0;
        for (const int group : groups) {
            if (group <= 0 || numerator > std::numeric_limits<int>::max() - group) return false;
            numerator += group;
        }
        return true;
    }

    std::vector<int> groups_;
    int denominator_{4};
};

/**
 * @brief Return the exact measure duration through the legacy Result-shaped API
 *
 * TimeSignature construction now guarantees the grouped-metre invariant, so an
 * admitted object cannot make this compatibility helper fail.
 */
[[nodiscard]] Result<Beat> checked_measure_duration(const TimeSignature& ts) noexcept;

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
[[nodiscard]] Result<TimeSignature> make_additive_time_signature(std::vector<int> groups,
                                                                 int denom);

/**
 * @brief Whether flattening to numerator/denominator loses source grouping identity.
 *
 * Returns true when the stored grouping differs from the deterministic grouping
 * synthesized by make_time_signature() for the same flat signature.
 */
[[nodiscard]] bool has_distinct_meter_grouping(const TimeSignature& ts) noexcept;

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
[[nodiscard]] bool
is_syncopated(const TimeSignature& ts, int onset_pos, int duration_pulses) noexcept;

} // namespace sunny::core
