/**
 * @file MIVD001A.h
 * @brief Mix IR validation — structural, audio quality, and intent rules
 *
 * Component: MIVD001A
 * Domain: MI (Mix IR) | Category: VD (Validation)
 *
 * Implements the validation rules from Mix Spec §11:
 *
 *   Structural (X1-X6):
 *     X1 Error:   Every Score IR Part has a corresponding ChannelStrip
 *     X2 Error:   Signal flow graph is acyclic (DAG)
 *     X3 Error:   Every channel reaches the master bus
 *     X4 Warning: Channel has no insert processing
 *     X5 Warning: Channel fader is at -inf but not muted
 *     X6 Error:   Sidechain source references non-existent channel or bus
 *
 *   Audio quality (A1-A7): static analysis of parameter values
 *   Intent (I1-I5): consistency between declared intent and actual settings
 *
 * Validation produces a vector of Diagnostic (SITP001A). Error-level
 * diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_mix runs all MixGraph-internal rules (X2-X6, A1-A7, I1-I5)
 * - validate_mix_correspondence runs X1 (Part ↔ ChannelStrip)
 * - Diagnostics are deterministic given the same input
 */

#pragma once

#include "MIDC001A.h"
#include "../Score/SIDC001A.h"

#include <vector>

namespace Sunny::Core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Run all MixGraph-internal validation rules (X2-X6, A-rules, I-rules)
 *
 * @param graph   The mix graph to validate
 * @return Diagnostics sorted by severity (Error first)
 */
[[nodiscard]] std::vector<Diagnostic> validate_mix(const MixGraph& graph);

/**
 * @brief Run X1: verify every Score IR Part has a ChannelStrip
 *
 * @param score   The score document
 * @param graph   The mix graph
 * @return One Error diagnostic per unmatched Part
 */
[[nodiscard]] std::vector<Diagnostic> validate_mix_correspondence(
    const Score& score,
    const MixGraph& graph);

/**
 * @brief Check whether a MixGraph passes all Error-level rules
 */
[[nodiscard]] bool is_mix_valid(const MixGraph& graph);

}  // namespace Sunny::Core
