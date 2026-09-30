/**
 * @file validation.hpp
 * @brief Score IR validation — structural, musical, and rendering rules
 *
 *
 * Implements the validation rules from SS-IR §8:
 *
 *   Structural (S1-S28): target-independent compilation preconditions
 *   Musical    (M1-M10): musical diagnostics, including degradable errors
 *   Rendering  (R1-R7):  target/rendering diagnostics
 *
 * Validation produces a vector of Diagnostic (defined in ).
 * `is_compilable` names structural compilability. A target compiler may add
 * blocking domain checks or explicitly degrade a non-structural diagnostic
 * into a typed CompilationReport; severity alone is not a target policy.
 *
 * Invariants:
 * - validate_score runs all applicable rules
 * - Diagnostics are deterministic given the same Score
 * - Structural rules are necessary and sufficient for compilation
 */

#pragma once

#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Run all validation rules on a Score document
 *
 * Returns diagnostics sorted by: severity (Error first), then position.
 * An empty result means the score is valid.
 *
 * @param score The score to validate
 * @return Vector of diagnostics (empty = valid)
 */
[[nodiscard]] std::vector<Diagnostic> validate_score(const Score& score);

/**
 * @brief Run only structural validation rules (S1-S28)
 *
 * These are the blocking rules that must pass before compilation.
 */
[[nodiscard]] std::vector<Diagnostic> validate_structural(const Score& score);

/**
 * @brief Run only musical validation rules (M1-M10)
 */
[[nodiscard]] std::vector<Diagnostic> validate_musical(const Score& score);

/**
 * @brief Run only rendering validation rules (R1-R7)
 */
[[nodiscard]] std::vector<Diagnostic> validate_rendering(const Score& score);

/**
 * @brief Validate one tagged articulation mapping independently of a Score
 *
 * Checks MIDI field domains, keyswitch representability, duration scale,
 * canonical Combined structure, and the bounded recursive mapping budget.
 */
[[nodiscard]] Result<void> validate_articulation_mapping(const ArticulationMapping& mapping);

/** @brief Validate the deployable domains of a part rendering configuration. */
[[nodiscard]] Result<void> validate_rendering_config(const RenderingConfig& rendering);

/**
 * @brief Check the target-independent structural compilation preconditions
 *
 * @param score The score to check
 * @return true if structural validation contains no Error diagnostic
 */
[[nodiscard]] bool is_compilable(const Score& score);

} // namespace sunny::core
