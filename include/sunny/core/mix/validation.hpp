/**
 * @file validation.hpp
 * @brief Mix IR validation — structural, audio quality, and intent rules
 *
 *
 * Implements the validation rules from Mix Spec §11:
 *
 *   Structural (X0-X13):
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
 *     X11 Error:  Aux sends are finite, at most MIX_LEVEL_CEILING_DB, and unique
 *                 per source/AuxBus pair
 *     X12 Error:  An automation lane has an unresolved target, undefined
 *                 interpolation, no breakpoints, a non-finite value, a time
 *                 before SCORE_START, or non-increasing times (§15.3(9))
 *     X13 Error:  The master loudness target lies outside BS.1770's domain
 *
 *   Audio quality (A1-A7): static analysis of parameter values
 *   Intent (I1-I5): consistency between declared intent and actual settings
 *
 * Validation produces a vector of Diagnostic. Error-level
 * diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_mix runs all MixGraph-internal rules (X0, X2-X13, I1-I5)
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
 * @brief Run all MixGraph-internal validation rules (X0, X2-X13 and I1-I5)
 *
 * @param graph   The mix graph to validate
 * @return Diagnostics sorted by severity (Error first)
 */
[[nodiscard]] std::vector<Diagnostic> validate_mix(const MixGraph& graph);

/**
 * @brief Check one automation lane against rule X12 in the context of a graph.
 *
 * The same predicate guards add_automation and whole-document validation, so
 * a lane admitted at mutation is never rejected on reload.
 */
[[nodiscard]] Result<void> validate_mix_automation(const MixGraph& graph,
                                                   const MixAutomation& automation);

/**
 * @brief Rule X13: integrated loudness at most 0 LUFS, true peak at most 0 dBTP,
 * and an optional loudness range that is finite and non-negative (ITU-R BS.1770,
 * EBU R128). All values are relative to digital full scale, so positive
 * targets are unreachable.
 */
[[nodiscard]] bool is_loudness_target_valid(const LoudnessTarget& target);

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
