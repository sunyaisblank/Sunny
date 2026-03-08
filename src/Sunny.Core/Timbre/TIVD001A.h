/**
 * @file TIVD001A.h
 * @brief Timbre IR validation — structural and parameter-range rules
 *
 * Component: TIVD001A
 * Domain: TI (Timbre IR) | Category: VD (Validation)
 *
 * Implements the validation rules from Timbre Spec §10:
 *
 *   Structural (T1-T9):
 *     T1 Error:   Every Score IR Part has a corresponding TimbreProfile
 *     T2 Error:   SoundSource type is valid and all required fields present
 *     T3 Warning: Filter cutoff exceeds Nyquist frequency
 *     T4 Warning: Oscillator detune exceeds ±100 cents
 *     T5 Warning: FM feedback exceeds stability threshold
 *     T6 Error:   Effect chain contains a cycle (structurally impossible)
 *     T7 Warning: Modulation routing targets a non-existent parameter path
 *     T8 Info:    Semantic descriptors are stale
 *     T9 Warning: TimbreRenderingConfig has unmapped parameters
 *
 * Validation produces a vector of Diagnostic (SITP001A). Error-level
 * diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_timbre runs all single-profile rules (T2-T9)
 * - validate_timbre_correspondence runs T1
 * - Diagnostics are deterministic given the same input
 */

#pragma once

#include "TIDC001A.h"
#include "../Score/SIDC001A.h"

#include <vector>

namespace Sunny::Core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Run all single-profile validation rules (T2-T9) on a TimbreProfile
 *
 * @param profile           The timbre profile to validate
 * @param sample_rate_hz    System sample rate for Nyquist checks (default 44100)
 * @return Diagnostics sorted by severity (Error first)
 */
[[nodiscard]] std::vector<Diagnostic> validate_timbre(
    const TimbreProfile& profile,
    float sample_rate_hz = 44100.0f);

/**
 * @brief Run T1: verify every Score IR Part has a TimbreProfile
 *
 * @param score     The score document
 * @param profiles  All timbre profiles
 * @return One Error diagnostic per unmatched Part
 */
[[nodiscard]] std::vector<Diagnostic> validate_timbre_correspondence(
    const Score& score,
    const std::vector<TimbreProfile>& profiles);

/**
 * @brief Check whether a profile passes all Error-level rules
 */
[[nodiscard]] bool is_timbre_valid(
    const TimbreProfile& profile,
    float sample_rate_hz = 44100.0f);

}  // namespace Sunny::Core
