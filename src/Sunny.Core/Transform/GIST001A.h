/**
 * @file GIST001A.h
 * @brief Generalised Interval System (Lewin)
 *
 * Component: GIST001A
 * Domain: GI (Generalised Interval) | Category: ST (System)
 *
 * Formal Spec §11.4: A GIS is a triple (S, IVLS, int) where S is
 * a set of musical elements, IVLS is a group of intervals, and
 * int: S×S → IVLS satisfies:
 *   (1) int(s,t) · int(t,u) = int(s,u)         (transitivity)
 *   (2) For each s∈S and i∈IVLS, ∃! t∈S: int(s,t) = i  (simply transitive)
 *
 * Concrete instances:
 * - PitchClassGIS: S = Z/12Z, IVLS = (Z/12Z, +)
 * - PitchGIS:      S = Z,     IVLS = (Z, +)
 * - TimePointGIS:  S = Q,     IVLS = (Q, +)     (via Beat rational type)
 *
 * Invariants (for all GIS instances):
 * - interval(s, s) == identity
 * - compose(interval(r,s), interval(s,t)) == interval(r,t)
 * - transpose(s, interval(s,t)) == t
 * - interval(s, transpose(s, i)) == i
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"

#include <concepts>

namespace Sunny::Core {

// =============================================================================
// GIS Concept
// =============================================================================

/**
 * @brief Concept: a Generalised Interval System (Lewin §11.4)
 *
 * A type G models GeneralisedIntervalSystem if it provides:
 * - Element and Interval type aliases
 * - interval(s, t) → Interval   (the int function)
 * - transpose(s, i) → Element   (the simply transitive action)
 * - identity() → Interval       (group identity)
 * - inverse(i) → Interval       (group inverse)
 * - compose(i, j) → Interval    (group operation)
 */
template <typename G>
concept GeneralisedIntervalSystem = requires(
    typename G::Element s,
    typename G::Element t,
    typename G::Interval i,
    typename G::Interval j
) {
    { G::interval(s, t) } -> std::same_as<typename G::Interval>;
    { G::transpose(s, i) } -> std::same_as<typename G::Element>;
    { G::identity() } -> std::same_as<typename G::Interval>;
    { G::inverse(i) } -> std::same_as<typename G::Interval>;
    { G::compose(i, j) } -> std::same_as<typename G::Interval>;
};

// =============================================================================
// Pitch Class GIS: (Z/12Z, (Z/12Z, +), subtraction mod 12)
// =============================================================================

/**
 * @brief Pitch class space GIS
 *
 * S = Z/12Z (pitch classes 0–11)
 * IVLS = (Z/12Z, +)
 * int(s, t) = (t − s) mod 12
 */
struct PitchClassGIS {
    using Element = PitchClass;
    using Interval = int;

    [[nodiscard]] static constexpr Interval interval(Element s, Element t) noexcept {
        return ((static_cast<int>(t) - static_cast<int>(s)) % 12 + 12) % 12;
    }

    [[nodiscard]] static constexpr Element transpose(Element s, Interval i) noexcept {
        return static_cast<Element>(((static_cast<int>(s) + i) % 12 + 12) % 12);
    }

    [[nodiscard]] static constexpr Interval identity() noexcept { return 0; }

    [[nodiscard]] static constexpr Interval inverse(Interval i) noexcept {
        return (12 - (i % 12 + 12) % 12) % 12;
    }

    [[nodiscard]] static constexpr Interval compose(Interval a, Interval b) noexcept {
        return ((a + b) % 12 + 12) % 12;
    }
};

static_assert(GeneralisedIntervalSystem<PitchClassGIS>);

// =============================================================================
// Pitch GIS: (Z, (Z, +), subtraction)
// =============================================================================

/**
 * @brief Pitch space GIS (MIDI note numbers)
 *
 * S = Z (integer MIDI pitches)
 * IVLS = (Z, +)
 * int(s, t) = t − s
 */
struct PitchGIS {
    using Element = int;
    using Interval = int;

    [[nodiscard]] static constexpr Interval interval(Element s, Element t) noexcept {
        return t - s;
    }

    [[nodiscard]] static constexpr Element transpose(Element s, Interval i) noexcept {
        return s + i;
    }

    [[nodiscard]] static constexpr Interval identity() noexcept { return 0; }

    [[nodiscard]] static constexpr Interval inverse(Interval i) noexcept {
        return -i;
    }

    [[nodiscard]] static constexpr Interval compose(Interval a, Interval b) noexcept {
        return a + b;
    }
};

static_assert(GeneralisedIntervalSystem<PitchGIS>);

// =============================================================================
// Time Point GIS: (Q, (Q, +), subtraction) using Beat rational type
// =============================================================================

/**
 * @brief Time point space GIS (rational time)
 *
 * S = Q (rational time points via Beat type)
 * IVLS = (Q, +)
 * int(s, t) = t − s
 */
struct TimePointGIS {
    using Element = Beat;
    using Interval = Beat;

    [[nodiscard]] static constexpr Interval interval(Element s, Element t) noexcept {
        return t - s;
    }

    [[nodiscard]] static constexpr Element transpose(Element s, Interval i) noexcept {
        return s + i;
    }

    [[nodiscard]] static constexpr Interval identity() noexcept {
        return Beat::zero();
    }

    [[nodiscard]] static constexpr Interval inverse(Interval i) noexcept {
        return Beat::zero() - i;
    }

    [[nodiscard]] static constexpr Interval compose(Interval a, Interval b) noexcept {
        return a + b;
    }
};

static_assert(GeneralisedIntervalSystem<TimePointGIS>);

// =============================================================================
// Generic GIS operations (work with any GIS instance)
// =============================================================================

/**
 * @brief Compute interval path through a sequence of elements
 *
 * Returns the sequence of intervals: int(s[0],s[1]), int(s[1],s[2]), ...
 */
template <GeneralisedIntervalSystem G>
[[nodiscard]] std::vector<typename G::Interval> interval_sequence(
    std::span<const typename G::Element> elements
) {
    std::vector<typename G::Interval> result;
    if (elements.size() < 2) return result;
    result.reserve(elements.size() - 1);
    for (std::size_t i = 0; i + 1 < elements.size(); ++i) {
        result.push_back(G::interval(elements[i], elements[i + 1]));
    }
    return result;
}

/**
 * @brief Reconstruct elements from a starting element and interval sequence
 *
 * Given s[0] and intervals (i₁, i₂, ...), produces
 * s[0], s[0]+i₁, s[0]+i₁+i₂, ...
 */
template <GeneralisedIntervalSystem G>
[[nodiscard]] std::vector<typename G::Element> elements_from_intervals(
    typename G::Element start,
    std::span<const typename G::Interval> intervals
) {
    std::vector<typename G::Element> result;
    result.reserve(intervals.size() + 1);
    result.push_back(start);
    for (const auto& iv : intervals) {
        start = G::transpose(start, iv);
        result.push_back(start);
    }
    return result;
}

}  // namespace Sunny::Core
