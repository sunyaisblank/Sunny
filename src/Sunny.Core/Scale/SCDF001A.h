/**
 * @file SCDF001A.h
 * @brief Scale definitions
 *
 * Component: SCDF001A
 * Domain: SC (Scale) | Category: DF (Definition)
 *
 * Contains 35+ built-in scale definitions with intervals and metadata.
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace Sunny::Core {

// =============================================================================
// Built-in Scales
// =============================================================================

/// Major scale intervals
constexpr std::array<Interval, 7> SCALE_MAJOR = {0, 2, 4, 5, 7, 9, 11};

/// Natural minor scale intervals
constexpr std::array<Interval, 7> SCALE_MINOR = {0, 2, 3, 5, 7, 8, 10};

/// Harmonic minor scale intervals
constexpr std::array<Interval, 7> SCALE_HARMONIC_MINOR = {0, 2, 3, 5, 7, 8, 11};

/// Melodic minor (ascending) intervals
constexpr std::array<Interval, 7> SCALE_MELODIC_MINOR = {0, 2, 3, 5, 7, 9, 11};

/// Dorian mode intervals
constexpr std::array<Interval, 7> SCALE_DORIAN = {0, 2, 3, 5, 7, 9, 10};

/// Phrygian mode intervals
constexpr std::array<Interval, 7> SCALE_PHRYGIAN = {0, 1, 3, 5, 7, 8, 10};

/// Lydian mode intervals
constexpr std::array<Interval, 7> SCALE_LYDIAN = {0, 2, 4, 6, 7, 9, 11};

/// Mixolydian mode intervals
constexpr std::array<Interval, 7> SCALE_MIXOLYDIAN = {0, 2, 4, 5, 7, 9, 10};

/// Locrian mode intervals
constexpr std::array<Interval, 7> SCALE_LOCRIAN = {0, 1, 3, 5, 6, 8, 10};

/// Pentatonic major intervals
constexpr std::array<Interval, 5> SCALE_PENTATONIC_MAJOR = {0, 2, 4, 7, 9};

/// Pentatonic minor intervals
constexpr std::array<Interval, 5> SCALE_PENTATONIC_MINOR = {0, 3, 5, 7, 10};

/// Blues scale intervals
constexpr std::array<Interval, 6> SCALE_BLUES = {0, 3, 5, 6, 7, 10};

/// Whole tone scale intervals
constexpr std::array<Interval, 6> SCALE_WHOLE_TONE = {0, 2, 4, 6, 8, 10};

/// Diminished (half-whole) scale intervals
constexpr std::array<Interval, 8> SCALE_DIMINISHED_HW = {0, 1, 3, 4, 6, 7, 9, 10};

/// Diminished (whole-half) scale intervals
constexpr std::array<Interval, 8> SCALE_DIMINISHED_WH = {0, 2, 3, 5, 6, 8, 9, 11};

/// Chromatic scale intervals
constexpr std::array<Interval, 12> SCALE_CHROMATIC = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

// =============================================================================
// Scale Registry
// =============================================================================

/**
 * @brief Find a built-in scale by name
 *
 * @param name Scale name (case-insensitive)
 * @return Scale definition or nullopt
 */
[[nodiscard]] std::optional<ScaleDefinition> find_scale(std::string_view name);

/**
 * @brief Get list of all available scale names
 */
[[nodiscard]] std::span<const std::string_view> list_scale_names();

/**
 * @brief Total number of built-in scales
 */
[[nodiscard]] constexpr std::size_t scale_count() noexcept;

}  // namespace Sunny::Core
