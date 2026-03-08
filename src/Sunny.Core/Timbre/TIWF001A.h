/**
 * @file TIWF001A.h
 * @brief Timbre IR workflow functions — agent-driven timbre configuration
 *
 * Component: TIWF001A
 * Domain: TI (Timbre IR) | Category: WF (Workflow)
 *
 * Provides entry-point functions for agent-driven timbre workflows
 * per Timbre Spec §11. These functions create, modify, and query
 * TimbreProfile objects at the compositional level.
 *
 * Functions divide into three categories:
 *   Mutation:  create, set, add, remove, reorder, load, morph
 *   Query:     get_parameter, search_presets, analyze_timbre
 *   Validation: validate (thin wrapper over TIVD001A)
 *
 * Invariants:
 * - Mutations leave the profile in a structurally valid state
 * - Parameter paths follow dot-separated segment notation
 * - Preset search is linear over the supplied library
 */

#pragma once

#include "TIDC001A.h"
#include "TIVD001A.h"
#include "TISZ001A.h"

namespace Sunny::Core {

// =============================================================================
// Profile Creation (§11: create_timbre_profile)
// =============================================================================

/**
 * @brief Create a new TimbreProfile bound to a Score IR Part.
 *
 * Returns a minimal valid profile with a default SubtractiveSynth source
 * (single sine oscillator, ADSR amplifier), empty effect chain, and
 * default semantic descriptors.
 */
[[nodiscard]] TimbreProfile wf_create_timbre_profile(
    TimbreProfileId id,
    PartId part_id,
    const std::string& name);

// =============================================================================
// Source Configuration (§11: set_sound_source)
// =============================================================================

/**
 * @brief Replace the sound source of a profile.
 *
 * Moves the supplied source into the profile, replacing whatever was
 * there previously.
 */
[[nodiscard]] Result<void> wf_set_sound_source(
    TimbreProfile& profile,
    SoundSourceData source);

// =============================================================================
// Effect Chain (§11: add_effect, remove_effect, reorder_effects)
// =============================================================================

/**
 * @brief Append an effect to the insert chain.
 */
[[nodiscard]] Result<void> wf_add_effect(
    TimbreProfile& profile,
    Effect effect);

/**
 * @brief Remove an effect from the insert chain by its EffectId.
 */
[[nodiscard]] Result<void> wf_remove_effect(
    TimbreProfile& profile,
    EffectId effect_id);

/**
 * @brief Reorder the insert chain to match the given sequence of EffectIds.
 *
 * The new_order vector must contain exactly the same set of EffectIds
 * currently present in the chain. Returns an error if any id is missing
 * or duplicated.
 */
[[nodiscard]] Result<void> wf_reorder_effects(
    TimbreProfile& profile,
    const std::vector<EffectId>& new_order);

// =============================================================================
// Parameter Access (§11: set_parameter)
// =============================================================================

/**
 * @brief Set a numeric parameter by dot-separated path.
 *
 * Paths follow the modulation target convention:
 *   source.filter.cutoff
 *   source.oscillators[0].tune_cents
 *   insert_chain.effects[0].mix
 *   modulation.lfos[0].rate.hz
 *
 * Returns TimbreError::InvalidModTarget if the path cannot be resolved.
 */
[[nodiscard]] Result<void> wf_set_parameter(
    TimbreProfile& profile,
    const std::string& path,
    float value);

/**
 * @brief Read a numeric parameter by dot-separated path.
 */
[[nodiscard]] Result<float> wf_get_parameter(
    const TimbreProfile& profile,
    const std::string& path);

// =============================================================================
// Macro Knobs (§11: create_macro, set_macro)
// =============================================================================

/**
 * @brief Add a macro knob to the modulation matrix.
 */
[[nodiscard]] Result<void> wf_create_macro(
    TimbreProfile& profile,
    MacroKnob macro);

/**
 * @brief Set a macro knob's value by index.
 */
[[nodiscard]] Result<void> wf_set_macro(
    TimbreProfile& profile,
    std::uint8_t macro_index,
    float value);

// =============================================================================
// Modulation (§11: add_modulation)
// =============================================================================

/**
 * @brief Add a modulation routing to the matrix.
 */
[[nodiscard]] Result<void> wf_add_modulation(
    TimbreProfile& profile,
    ModulationRouting routing);

// =============================================================================
// Automation (§11: add_automation)
// =============================================================================

/**
 * @brief Add timbral automation (parameter changes over score time).
 */
[[nodiscard]] Result<void> wf_add_automation(
    TimbreProfile& profile,
    TimbreAutomation automation);

// =============================================================================
// Semantic Descriptors (§11: set_semantic_descriptors, analyze_timbre)
// =============================================================================

/**
 * @brief Set or replace the semantic timbral descriptors.
 */
void wf_set_semantic_descriptors(
    TimbreProfile& profile,
    SemanticTimbreDescriptor descriptors);

/**
 * @brief Derive semantic descriptors from the current synthesis parameters.
 *
 * Uses heuristic analysis of source parameters, effect chain, and
 * modulation depth to estimate perceptual qualities. The result is
 * tagged with DerivationMode::ParameterDerived.
 */
[[nodiscard]] SemanticTimbreDescriptor wf_analyze_timbre(
    const TimbreProfile& profile);

// =============================================================================
// Presets (§11: search_presets, load_preset, save_preset, morph_presets)
// =============================================================================

/**
 * @brief Search criteria for preset lookup.
 */
struct PresetSearchQuery {
    std::optional<std::string> name_contains;
    std::vector<std::string> required_tags;
    std::optional<float> min_brightness;
    std::optional<float> max_brightness;
    std::optional<float> min_warmth;
    std::optional<float> max_warmth;
    std::size_t max_results = 10;
};

/**
 * @brief Search a preset library by name, tags, or semantic descriptors.
 *
 * Returns matching presets ordered by relevance (tag match count).
 */
[[nodiscard]] std::vector<TimbrePreset> wf_search_presets(
    const std::vector<TimbrePreset>& library,
    const PresetSearchQuery& query);

/**
 * @brief Apply a preset's parameter state to a profile.
 *
 * Overwrites each parameter listed in the preset's parameter_state
 * map using the path-based setter. Parameters not listed in the
 * preset remain unchanged.
 */
[[nodiscard]] Result<void> wf_load_preset(
    TimbreProfile& profile,
    const TimbrePreset& preset);

/**
 * @brief Capture the profile's current parameter state as a preset.
 *
 * Extracts key parameters (source, filter, effects) into the preset's
 * parameter_state map, and copies the current semantic descriptors.
 */
[[nodiscard]] TimbrePreset wf_save_preset(
    const TimbreProfile& profile,
    TimbrePresetId id,
    const std::string& name);

/**
 * @brief Schedule a preset morph over a score time range.
 */
[[nodiscard]] Result<void> wf_morph_presets(
    TimbreProfile& profile,
    PresetMorph morph);

// =============================================================================
// Validation (§11: validate_timbre)
// =============================================================================

/**
 * @brief Run full validation on a profile. Thin wrapper over TIVD001A.
 */
[[nodiscard]] std::vector<Diagnostic> wf_validate(
    const TimbreProfile& profile,
    float sample_rate_hz = 44100.0f);

}  // namespace Sunny::Core
