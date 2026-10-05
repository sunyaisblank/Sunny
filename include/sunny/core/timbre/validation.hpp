/**
 * @file validation.hpp
 * @brief Timbre IR validation — structural and parameter-range rules
 *
 *
 * Implements the validation rules from Timbre Spec §10:
 *
 *   Structural (T1-T13):
 *     T1 Error:   Every Score IR Part has a corresponding TimbreProfile
 *     T2 Error:   SoundSource type is valid and all required fields present
 *     T3 Warning: Filter cutoff exceeds Nyquist frequency
 *     T4 Warning: Oscillator detune exceeds ±100 cents
 *     T5 Warning: FM feedback exceeds stability threshold
 *     T6 Error:   Effect chain contains a cycle (structurally impossible)
 *     T7 Error:   Modulation routing source/target/depth is invalid
 *     T8 Info:    Semantic descriptors are stale
 *     T9 Warning: TimbreRenderingConfig has unmapped parameters
 *     T10 Error:  Rendering mapping cannot be resolved or materialised
 *     T11 Error:  Timbral automation is structurally invalid
 *     T12 Error:  An owned modulation generator or macro is invalid
 *     T13 Error:  An intrinsic source/effect/descriptor domain is invalid,
 *                 an effect identity is zero/duplicated, or sample rate is invalid
 *
 * Validation produces a vector of Diagnostic. Error-level
 * diagnostics block compilation; warnings and info do not.
 *
 * Invariants:
 * - validate_timbre runs all single-profile rules (T2-T13)
 * - validate_timbre_correspondence runs T1
 * - Diagnostics are deterministic given the same input
 */

#pragma once

#include <span>
#include <sunny/core/score/document.hpp>
#include <sunny/core/timbre/document.hpp>
#include <vector>

namespace sunny::core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Run all single-profile validation rules (T2-T13) on a TimbreProfile
 *
 * @param profile           The timbre profile to validate
 * @param sample_rate_hz    System sample rate for Nyquist checks (default 44100)
 * @return Diagnostics sorted by severity (Error first)
 */
[[nodiscard]] std::vector<Diagnostic> validate_timbre(const TimbreProfile& profile,
                                                      float sample_rate_hz = 44100.0f);

/**
 * @brief Validate one complete timbral automation lane against a profile
 *
 * A valid lane has an exact resolvable target, a valid interpolation enum,
 * one or more finite-valued breakpoints at or after SCORE_START, and strictly
 * increasing breakpoint times. This predicate is shared by mutation and
 * whole-document validation so their admission boundaries cannot diverge.
 */
[[nodiscard]] Result<void> validate_timbre_automation(const TimbreProfile& profile,
                                                      const TimbreAutomation& automation);

/**
 * @brief Validate one complete modulation routing against a profile
 *
 * The source and optional via source must have a defined discriminator,
 * canonical payload, and any referenced matrix member must exist. The target
 * resolves exactly and depth is finite in [-1, 1].
 */
[[nodiscard]] Result<void> validate_modulation_routing(const TimbreProfile& profile,
                                                       const ModulationRouting& routing);

[[nodiscard]] Result<void> validate_modulation_lfo(const LFO& lfo);
[[nodiscard]] Result<void> validate_modulation_envelope(const Envelope& envelope);
[[nodiscard]] Result<void> validate_modulation_step_sequencer(const StepSequencer& sequencer);
[[nodiscard]] Result<void> validate_modulation_macro(const TimbreProfile& profile,
                                                     const MacroKnob& macro);

/**
 * @brief Run T1: verify every Score IR Part has a TimbreProfile
 *
 * @param score     The score document
 * @param profiles  All timbre profiles
 * @return Error diagnostics for missing, duplicate, and unknown Part bindings
 */
[[nodiscard]] std::vector<Diagnostic>
validate_timbre_correspondence(const Score& score, const std::vector<TimbreProfile>& profiles);

/**
 * @brief Non-owning T1 overload for project/session stores
 *
 * Null profile pointers are ignored. Callers that maintain identity-stable,
 * move-only TimbreProfiles can validate their bindings without copying them.
 */
[[nodiscard]] std::vector<Diagnostic>
validate_timbre_correspondence(const Score& score, std::span<const TimbreProfile* const> profiles);

/**
 * @brief Check whether a profile passes all Error-level rules
 */
[[nodiscard]] bool is_timbre_valid(const TimbreProfile& profile, float sample_rate_hz = 44100.0f);

} // namespace sunny::core
