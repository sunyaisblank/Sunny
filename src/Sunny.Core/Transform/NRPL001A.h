/**
 * @file NRPL001A.h
 * @brief Neo-Riemannian transformations (PLR operations)
 *
 * Component: NRPL001A
 * Domain: NR (Neo-Riemannian) | Category: PL (PLR)
 *
 * Implements the three neo-Riemannian operations P, L, R on the
 * 24-node Tonnetz graph (12 major + 12 minor triads), with BFS
 * shortest-path computation and compound operations.
 *
 * Invariants:
 * - P, L, R are involutions: applying twice returns the original triad
 * - The PLR graph is connected with diameter ≤ 6
 * - P preserves 2 common tones, R preserves 2, L preserves 2
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"

#include <array>
#include <span>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Triad quality: major or minor
 */
enum class TriadQuality { Major, Minor };

/**
 * @brief A triad identified by root and quality
 */
struct Triad {
    PitchClass root;
    TriadQuality quality;

    bool operator==(const Triad&) const = default;
};

/**
 * @brief The three neo-Riemannian operations
 */
enum class NROperation { P, R, L };

// =============================================================================
// Triad Construction
// =============================================================================

/**
 * @brief Construct a triad (validates pitch class)
 */
[[nodiscard]] Result<Triad> make_triad(PitchClass root, TriadQuality quality);

/**
 * @brief Get the three pitch classes of a triad
 *
 * @return {root, third, fifth}
 */
[[nodiscard]] std::array<PitchClass, 3> triad_pitch_classes(const Triad& t);

/**
 * @brief Identify a triad from a set of 3 pitch classes
 *
 * @return Triad or InvalidTriad if not a major/minor triad
 */
[[nodiscard]] Result<Triad> triad_from_pitch_classes(
    PitchClass a, PitchClass b, PitchClass c
);

// =============================================================================
// PLR Operations
// =============================================================================

/**
 * @brief Apply a single PLR operation
 *
 * | Op | (r, Major) ->        | (r, Minor) ->        |
 * |----|----------------------|----------------------|
 * | P  | (r, Minor)           | (r, Major)           |
 * | R  | ((r+9)%12, Minor)    | ((r+3)%12, Major)    |
 * | L  | ((r+4)%12, Minor)    | ((r+8)%12, Major)    |
 */
[[nodiscard]] Triad apply_plr(const Triad& t, NROperation op);

/**
 * @brief Apply a sequence of PLR operations left to right
 */
[[nodiscard]] Triad apply_plr_sequence(const Triad& t, std::span<const NROperation> ops);

// =============================================================================
// Graph Distance
// =============================================================================

/**
 * @brief BFS shortest distance between two triads on the PLR graph
 */
[[nodiscard]] int plr_distance(const Triad& from, const Triad& to);

/**
 * @brief BFS shortest path between two triads
 *
 * @return Sequence of operations transforming 'from' into 'to'
 */
[[nodiscard]] std::vector<NROperation> plr_path(const Triad& from, const Triad& to);

// =============================================================================
// Common Tones
// =============================================================================

/**
 * @brief Count pitch classes shared between two triads
 */
[[nodiscard]] int common_tone_count(const Triad& a, const Triad& b);

}  // namespace Sunny::Core
