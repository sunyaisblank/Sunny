/**
 * @file validation.hpp
 * @brief Mix IR validation — structural, audio quality, and intent rules
 *
 *
 * Implements the validation rules from Mix Spec §11:
 *
 *   Structural (X0-X11):
 *     X0 Error:   All typed identities are unique in their document scope
 *     X1 Error:   Every Score IR Part has a corresponding ChannelStrip
 *     X2 Error:   Signal flow graph is acyclic (DAG)
 *     X3 Error:   Every channel reaches the master bus
 *     X4 Warning: Channel has no insert processing
 *     X5 Warning: Channel fader is at -inf but not muted
 *     X6 Error:   Sidechain source or Aux-send target references a missing object
 *     X7 Error:   Effect parameter values violate their effect domain
 *     X8 Error:   Effect target mappings are unresolved, malformed, or aliasing
 *     X9 Error:   Relative fader constraints are invalid, cyclic, or out of domain
 *     X10 Error:  Group member lists and child routing references disagree
 *     X11 Error:  Aux sends are finite and unique per source/AuxBus pair
 *
 *   Audio quality (A1-A7): static analysis of parameter values
 *   Intent (I1-I5): consistency between declared intent and actual settings
 *
 * Validation produces a vector of Diagnostic. Error-level
 * diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_mix runs all MixGraph-internal rules (X0, X2-X11, I1-I5)
 * - validate_mix_correspondence runs X1 (Part ↔ ChannelStrip)
 * - Diagnostics are deterministic given the same input
 */

#pragma once

#include <sunny/core/mix/document.hpp>
#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Run all MixGraph-internal validation rules (X0, X2-X11 and I1-I5)
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
 * @return Error diagnostics for missing, duplicate, and unknown Part bindings
 */
[[nodiscard]] std::vector<Diagnostic> validate_mix_correspondence(const Score& score,
                                                                  const MixGraph& graph);

/**
 * @brief Check whether a MixGraph passes all Error-level rules
 */
[[nodiscard]] bool is_mix_valid(const MixGraph& graph);

} // namespace sunny::core
