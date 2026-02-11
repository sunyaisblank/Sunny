/**
 * @file PTPS001A.h
 * @brief Pitch class set operations
 *
 * Component: PTPS001A
 * Domain: PT (Pitch) | Category: PS (Pitch Set)
 *
 * Set-theoretic operations on pitch class collections.
 * Implements T_n and I_n on sets, interval vector, normal form, prime form.
 *
 * Invariants:
 * - pcs_transpose(pcs, 0) == pcs
 * - pcs_invert(pcs_invert(pcs, axis), axis) == pcs
 * - |interval_vector| == 6
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "PTPC001A.h"

#include <array>
#include <unordered_set>
#include <vector>

namespace Sunny::Core {

/// Pitch class set type
using PitchClassSet = std::unordered_set<PitchClass>;

/**
 * @brief Transpose a pitch class set by interval n
 *
 * @param pcs Input pitch class set
 * @param n Transposition interval
 * @return Transposed pitch class set
 */
[[nodiscard]] PitchClassSet pcs_transpose(const PitchClassSet& pcs, int n);

/**
 * @brief Invert a pitch class set around an axis
 *
 * @param pcs Input pitch class set
 * @param axis Inversion axis
 * @return Inverted pitch class set
 */
[[nodiscard]] PitchClassSet pcs_invert(const PitchClassSet& pcs, int axis = 0);

/**
 * @brief Compute the interval vector of a pitch class set
 *
 * Counts occurrences of each interval class (1-6) between all pairs.
 *
 * @param pcs Input pitch class set
 * @return Array of 6 integers: counts of ic1-ic6
 */
[[nodiscard]] std::array<int, 6> pcs_interval_vector(const PitchClassSet& pcs);

/**
 * @brief Compute the normal form of a pitch class set
 *
 * Normal form is the most compact rotation.
 *
 * @param pcs Input pitch class set
 * @return Vector of pitch classes in normal form order
 */
[[nodiscard]] std::vector<PitchClass> pcs_normal_form(const PitchClassSet& pcs);

/**
 * @brief Compute the prime form of a pitch class set
 *
 * Most compact of normal form and its inversion, transposed to 0.
 *
 * @param pcs Input pitch class set
 * @return Vector of pitch classes in prime form
 */
[[nodiscard]] std::vector<PitchClass> pcs_prime_form(const PitchClassSet& pcs);

/**
 * @brief Check if two PCS are equivalent under transposition
 */
[[nodiscard]] bool pcs_t_equivalent(const PitchClassSet& a, const PitchClassSet& b);

/**
 * @brief Check if two PCS are equivalent under transposition and inversion
 */
[[nodiscard]] bool pcs_ti_equivalent(const PitchClassSet& a, const PitchClassSet& b);

// =============================================================================
// Forte Number Catalogue (§12.4)
// =============================================================================

/**
 * @brief Look up the Forte number for a pitch class set
 *
 * Returns the standard Forte label (e.g., "3-11" for major/minor triad).
 * Covers cardinalities 2-10 (cardinalities 7-10 via complement derivation).
 *
 * @param pcs Input pitch class set
 * @return Forte number string or nullopt if not found
 */
[[nodiscard]] std::optional<std::string> forte_number(const PitchClassSet& pcs);

// =============================================================================
// Z-Relation (§12.5)
// =============================================================================

/**
 * @brief Check if two pitch class sets are Z-related
 *
 * Z-related sets share the same interval class vector but belong
 * to different TI-equivalence classes (different prime forms).
 *
 * @param a First pitch class set
 * @param b Second pitch class set
 * @return true if Z-related
 */
[[nodiscard]] bool pcs_z_related(const PitchClassSet& a, const PitchClassSet& b);

// =============================================================================
// Similarity Relations (§12.6)
// =============================================================================

/**
 * @brief R0: Maximum dissimilarity
 *
 * True if A and B have the same cardinality and their ICVs
 * differ in every entry.
 */
[[nodiscard]] bool similarity_R0(const PitchClassSet& a, const PitchClassSet& b);

/**
 * @brief R1: Minimum dissimilarity
 *
 * True if A and B have the same cardinality and their ICVs
 * differ in exactly one entry, by exactly 1.
 */
[[nodiscard]] bool similarity_R1(const PitchClassSet& a, const PitchClassSet& b);

/**
 * @brief R2: Single-entry difference
 *
 * True if A and B have the same cardinality and their ICVs
 * differ in exactly one entry (by any amount).
 */
[[nodiscard]] bool similarity_R2(const PitchClassSet& a, const PitchClassSet& b);

/**
 * @brief Rp: Subset embedding
 *
 * True if A and B have the same cardinality and one can be
 * embedded in the other after transposition (inclusion relation).
 * Formally: ∃n such that A ⊂ T_n(B) ∪ {one extra element},
 * or equivalently they share n-1 pitch classes under some T_n.
 */
[[nodiscard]] bool similarity_Rp(const PitchClassSet& a, const PitchClassSet& b);

}  // namespace Sunny::Core
