/**
 * @file SCRN001A.h
 * @brief Scale relationships (degree naming, parallel/relative, borrowed chords, generated scales)
 *
 * Component: SCRN001A
 * Domain: SC (Scale) | Category: RN (Relationship)
 *
 * Analyses relationships between scales: relative/parallel classification,
 * common-tone counting, subset detection, modal interchange (borrowed chords),
 * and generator-based scale construction with deep scale property detection.
 *
 * Invariants:
 * - scale_degree_name only defined for 7-note scales
 * - classify_scale_relationship is symmetric
 * - scale_common_tone_count(s, s) == cardinality of s
 * - generated scale with generator g, cardinality k produces exactly k distinct PCs
 *   when gcd(g,12) divides 12 and k ≤ 12/gcd(g,12)
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Types
// =============================================================================

enum class ScaleDegreeName {
    Tonic, Supertonic, Mediant, Subdominant,
    Dominant, Submediant, LeadingTone, Subtonic
};

enum class ScaleRelationship { Relative, Parallel, None };

struct BorrowedChord {
    PitchClass root;
    std::string quality;
    int degree;               ///< 0-based scale degree in the parallel scale
    std::string source_mode;
};

struct GeneratedScaleResult {
    std::vector<PitchClass> pitch_classes;  ///< sorted ascending
    bool has_deep_scale_property;
};

// =============================================================================
// Degree Naming
// =============================================================================

/**
 * @brief Name a scale degree (7-note scales only)
 *
 * Degree 6 is LeadingTone if intervals[6]==11, Subtonic if intervals[6]==10.
 *
 * @param degree 0-based degree [0..6]
 * @param intervals Scale intervals from root
 * @return Degree name or nullopt for non-7-note scales
 */
[[nodiscard]] std::optional<ScaleDegreeName> scale_degree_name(
    int degree,
    std::span<const Interval> intervals
);

/**
 * @brief Convert degree name to display string
 */
[[nodiscard]] constexpr std::string_view degree_name_to_string(ScaleDegreeName name) noexcept {
    switch (name) {
        case ScaleDegreeName::Tonic:       return "Tonic";
        case ScaleDegreeName::Supertonic:  return "Supertonic";
        case ScaleDegreeName::Mediant:     return "Mediant";
        case ScaleDegreeName::Subdominant: return "Subdominant";
        case ScaleDegreeName::Dominant:    return "Dominant";
        case ScaleDegreeName::Submediant:  return "Submediant";
        case ScaleDegreeName::LeadingTone: return "Leading Tone";
        case ScaleDegreeName::Subtonic:    return "Subtonic";
    }
    return "Unknown";
}

// =============================================================================
// Scale Relationships
// =============================================================================

/**
 * @brief Classify relationship between two scales
 *
 * Relative: same PC set, different root.
 * Parallel: same root, different PC set.
 */
[[nodiscard]] ScaleRelationship classify_scale_relationship(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
);

/**
 * @brief Count common pitch classes between two scales
 */
[[nodiscard]] int scale_common_tone_count(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
);

/**
 * @brief Check if scale 1 is a subset of scale 2 (by PC set)
 */
[[nodiscard]] bool scale_is_subset(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
);

// =============================================================================
// Modal Interchange
// =============================================================================

/**
 * @brief Find chords borrowed from a parallel scale
 *
 * Generates triads on each degree of the parallel scale and returns
 * those whose root+quality combination is absent from the primary scale.
 */
[[nodiscard]] std::vector<BorrowedChord> find_borrowed_chords(
    PitchClass root,
    std::span<const Interval> primary,
    std::span<const Interval> parallel,
    std::string_view parallel_name
);

// =============================================================================
// Generated Scales
// =============================================================================

/**
 * @brief Generate a scale from a repeated generator interval
 *
 * S = {(root + j*g) % 12 : j=0..k-1}, sorted ascending.
 * Returns InvalidGeneratedScale if fewer than k distinct PCs result.
 */
[[nodiscard]] Result<GeneratedScaleResult> generate_scale_from_generator(
    PitchClass root,
    int generator,
    int cardinality
);

/**
 * @brief Check the deep scale property for a generator/cardinality pair
 *
 * True when gcd(generator, 12) == 1 and cardinality ≤ 12/gcd(generator, 12).
 */
[[nodiscard]] bool has_deep_scale_property(int generator, int cardinality);

}  // namespace Sunny::Core
