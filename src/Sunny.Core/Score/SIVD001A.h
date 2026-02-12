/**
 * @file SIVD001A.h
 * @brief Score IR validation — structural, musical, and rendering rules
 *
 * Component: SIVD001A
 * Domain: SI (Score IR) | Category: VD (Validation)
 *
 * Implements the validation rules from SS-IR §8:
 *
 *   Structural (S1-S11): Error severity, block compilation
 *   Musical    (M1-M10): Warning/Info severity, advisory
 *   Rendering  (R1-R5):  Warning/Info severity, advisory
 *
 * Validation produces a vector of Diagnostic (defined in SITP001A).
 * Error-level diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_score runs all applicable rules
 * - Diagnostics are deterministic given the same Score
 * - Structural rules are necessary and sufficient for compilation
 */

#pragma once

#include "SIDC001A.h"

#include <vector>

namespace Sunny::Core {

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
 * @brief Run only structural validation rules (S1-S11)
 *
 * These are the blocking rules that must pass before compilation.
 */
[[nodiscard]] std::vector<Diagnostic> validate_structural(const Score& score);

/**
 * @brief Run only musical validation rules (M1-M10)
 */
[[nodiscard]] std::vector<Diagnostic> validate_musical(const Score& score);

/**
 * @brief Run only rendering validation rules (R1-R5)
 */
[[nodiscard]] std::vector<Diagnostic> validate_rendering(const Score& score);

/**
 * @brief Check whether a score has any Error-level diagnostics
 *
 * @param score The score to check
 * @return true if no Error diagnostics; false if compilation should be blocked
 */
[[nodiscard]] bool is_compilable(const Score& score);

}  // namespace Sunny::Core
