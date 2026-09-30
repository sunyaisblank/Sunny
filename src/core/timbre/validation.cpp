/**
 * @file validation.cpp
 * @brief Timbre IR validation implementation
 *
 *
 */

#include <algorithm>
#include <cmath>
#include <set>
#include <string_view>
#include <sunny/core/timbre/validation.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace sunny::core {

namespace {

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

void add_diagnostic(std::vector<Diagnostic>& out,
                    ValidationSeverity sev,
                    const char* rule,
                    const std::string& msg,
                    ErrorCode code,
                    std::optional<PartId> part = std::nullopt) {
    out.push_back({sev, rule, msg, std::nullopt, part, code});
}

// -------------------------------------------------------------------------
// T2: SoundSource validity
// -------------------------------------------------------------------------

void validate_source(const SoundSourceData& src, std::vector<Diagnostic>& out, PartId part) {
    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, SubtractiveSynth>) {
                if (s.oscillators.empty()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "SubtractiveSynth requires at least one oscillator",
                                   ErrorCode::InvalidSource,
                                   part);
                }
                if (!s.oscillator_mix.empty() && s.oscillator_mix.size() != s.oscillators.size()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "oscillator_mix size must match oscillator count",
                                   ErrorCode::InvalidSource,
                                   part);
                }
            } else if constexpr (std::is_same_v<T, FMSynth>) {
                if (s.operators.empty()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "FMSynth requires at least one operator",
                                   ErrorCode::InvalidSource,
                                   part);
                }
                if (s.algorithm.use_preset &&
                    (s.algorithm.preset_number < 1 || s.algorithm.preset_number > 32)) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "FM algorithm preset number must be 1–32",
                                   ErrorCode::InvalidSource,
                                   part);
                }
            } else if constexpr (std::is_same_v<T, AdditiveSynth>) {
                if (s.partials.empty()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "AdditiveSynth requires at least one partial",
                                   ErrorCode::InvalidSource,
                                   part);
                }
            } else if constexpr (std::is_same_v<T, SamplerSource>) {
                if (s.library.empty()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "Sampler requires a non-empty library name",
                                   ErrorCode::InvalidSource,
                                   part);
                }
            } else if constexpr (std::is_same_v<T, HybridSource>) {
                if (s.layers.empty()) {
                    add_diagnostic(out,
                                   ValidationSeverity::Error,
                                   "T2",
                                   "HybridSource requires at least one layer",
                                   ErrorCode::InvalidSource,
                                   part);
                }
                for (const auto& layer : s.layers) {
                    if (layer) {
                        validate_source(*layer, out, part);
                    }
                }
            }
            // WavetableSynth, GranularSynth, PhysicalModelSource:
            // no structural minimums beyond default-constructed state
        },
        src.data);
}

// -------------------------------------------------------------------------
// T3: Filter cutoff vs Nyquist
// -------------------------------------------------------------------------

void check_filter_nyquist(const Filter& f,
                          float nyquist,
                          std::vector<Diagnostic>& out,
                          PartId part) {
    if (f.cutoff > nyquist) {
        add_diagnostic(out,
                       ValidationSeverity::Warning,
                       "T3",
                       "Filter cutoff (" + std::to_string(static_cast<int>(f.cutoff)) +
                           " Hz) exceeds Nyquist (" + std::to_string(static_cast<int>(nyquist)) +
                           " Hz)",
                       ErrorCode::CutoffAboveNyquist,
                       part);
    }
}

void check_filters_in_source(const SoundSourceData& src,
                             float nyquist,
                             std::vector<Diagnostic>& out,
                             PartId part) {
    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, SubtractiveSynth>) {
                check_filter_nyquist(s.filter, nyquist, out, part);
                if (s.filter_2) check_filter_nyquist(*s.filter_2, nyquist, out, part);
            } else if constexpr (std::is_same_v<T, WavetableSynth>) {
                if (s.filter) check_filter_nyquist(*s.filter, nyquist, out, part);
            } else if constexpr (std::is_same_v<T, SamplerSource>) {
                if (s.filter) check_filter_nyquist(*s.filter, nyquist, out, part);
            } else if constexpr (std::is_same_v<T, HybridSource>) {
                for (const auto& layer : s.layers) {
                    if (layer) check_filters_in_source(*layer, nyquist, out, part);
                }
            }
        },
        src.data);
}

// -------------------------------------------------------------------------
// T4: Oscillator detune
// -------------------------------------------------------------------------

void check_detune(const SoundSourceData& src, std::vector<Diagnostic>& out, PartId part) {
    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, SubtractiveSynth>) {
                for (const auto& osc : s.oscillators) {
                    if (std::abs(osc.tune_cents) > 100.0f) {
                        add_diagnostic(out,
                                       ValidationSeverity::Warning,
                                       "T4",
                                       "Oscillator detune exceeds +/-100 cents",
                                       ErrorCode::ExcessiveDetune,
                                       part);
                    }
                }
            } else if constexpr (std::is_same_v<T, HybridSource>) {
                for (const auto& layer : s.layers) {
                    if (layer) check_detune(*layer, out, part);
                }
            }
        },
        src.data);
}

// -------------------------------------------------------------------------
// T5: FM feedback stability
// -------------------------------------------------------------------------

constexpr float FM_FEEDBACK_THRESHOLD = 0.95f;

void check_fm_feedback(const SoundSourceData& src, std::vector<Diagnostic>& out, PartId part) {
    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, FMSynth>) {
                if (std::abs(s.feedback) > FM_FEEDBACK_THRESHOLD) {
                    add_diagnostic(out,
                                   ValidationSeverity::Warning,
                                   "T5",
                                   "FM feedback (" + std::to_string(s.feedback) +
                                       ") exceeds stability threshold (" +
                                       std::to_string(FM_FEEDBACK_THRESHOLD) + ")",
                                   ErrorCode::FMFeedbackUnstable,
                                   part);
                }
            } else if constexpr (std::is_same_v<T, HybridSource>) {
                for (const auto& layer : s.layers) {
                    if (layer) check_fm_feedback(*layer, out, part);
                }
            }
        },
        src.data);
}

// -------------------------------------------------------------------------
// T6b: Envelope loop stage indices
// -------------------------------------------------------------------------

void check_envelope_loop(const Envelope& env,
                         const char* context,
                         std::vector<Diagnostic>& out,
                         PartId part) {
    if (!env.loop) return;
    auto stage_count = static_cast<std::uint8_t>(env.stages.size());
    if (env.loop->start_stage >= stage_count || env.loop->end_stage >= stage_count) {
        add_diagnostic(out,
                       ValidationSeverity::Error,
                       "T6b",
                       std::string(context) + " envelope loop indices (" +
                           std::to_string(env.loop->start_stage) + ", " +
                           std::to_string(env.loop->end_stage) + ") exceed stage count " +
                           std::to_string(stage_count),
                       ErrorCode::InvalidSource,
                       part);
    }
    if (env.loop->start_stage > env.loop->end_stage) {
        add_diagnostic(out,
                       ValidationSeverity::Error,
                       "T6b",
                       std::string(context) + " envelope loop start_stage > end_stage",
                       ErrorCode::InvalidSource,
                       part);
    }
}

void check_envelope_loops(const TimbreProfile& profile, std::vector<Diagnostic>& out) {
    PartId part = profile.part_id;
    // Check source envelopes
    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, SubtractiveSynth>) {
                check_envelope_loop(s.amplifier, "SubtractiveSynth amplifier", out, part);
                if (s.filter.envelope)
                    check_envelope_loop(*s.filter.envelope, "SubtractiveSynth filter", out, part);
                if (s.filter_2 && s.filter_2->envelope)
                    check_envelope_loop(
                        *s.filter_2->envelope, "SubtractiveSynth filter_2", out, part);
            } else if constexpr (std::is_same_v<T, FMSynth>) {
                for (std::size_t i = 0; i < s.operators.size(); ++i)
                    check_envelope_loop(s.operators[i].envelope,
                                        ("FMOperator[" + std::to_string(i) + "]").c_str(),
                                        out,
                                        part);
            }
        },
        profile.source.data);
    // Check LFO envelopes not applicable (LFOs have no Envelope with loop)
}

// -------------------------------------------------------------------------
// T7: Complete modulation routing validation
// -------------------------------------------------------------------------

struct ModulationValidationFailure {
    ErrorCode code;
    std::string message;
};

std::optional<ModulationValidationFailure> modulation_source_failure(const TimbreProfile& profile,
                                                                     const ModulationSource& source,
                                                                     std::string_view role) {
    const auto invalid = [&](std::string detail) {
        return std::optional<ModulationValidationFailure>{ModulationValidationFailure{
            ErrorCode::InvalidModSource, std::string(role) + " modulation source " + detail}};
    };
    if (static_cast<std::underlying_type_t<ModulationSourceType>>(source.type) >
        static_cast<std::underlying_type_t<ModulationSourceType>>(ModulationSourceType::MacroKnob))
        return invalid("has an undefined type");

    const bool has_cc = source.cc_number != 0;
    const bool has_sidechain = source.sidechain_part.value != 0;
    const auto require_index_payload = [&]() -> std::optional<ModulationValidationFailure> {
        if (has_cc || has_sidechain) return invalid("has non-canonical payload fields");
        return std::nullopt;
    };

    switch (source.type) {
    case ModulationSourceType::LFO:
        if (auto failure = require_index_payload()) return failure;
        if (static_cast<std::size_t>(source.index) >= profile.modulation.lfos.size())
            return invalid("references a missing LFO");
        return std::nullopt;
    case ModulationSourceType::Envelope:
        if (auto failure = require_index_payload()) return failure;
        if (static_cast<std::size_t>(source.index) >= profile.modulation.envelopes.size())
            return invalid("references a missing envelope");
        return std::nullopt;
    case ModulationSourceType::StepSequencer:
        if (auto failure = require_index_payload()) return failure;
        if (static_cast<std::size_t>(source.index) >= profile.modulation.step_sequencers.size())
            return invalid("references a missing step sequencer");
        return std::nullopt;
    case ModulationSourceType::MacroKnob:
        if (auto failure = require_index_payload()) return failure;
        if (std::none_of(profile.modulation.macro_knobs.begin(),
                         profile.modulation.macro_knobs.end(),
                         [&](const MacroKnob& macro) { return macro.index == source.index; }))
            return invalid("references a missing macro knob");
        return std::nullopt;
    case ModulationSourceType::CC:
        if (source.index != 0 || has_sidechain) return invalid("has non-canonical payload fields");
        if (source.cc_number > 127) return invalid("has a CC number outside [0, 127]");
        return std::nullopt;
    case ModulationSourceType::AudioFollower:
        if (source.index != 0 || has_cc) return invalid("has non-canonical payload fields");
        if (!has_sidechain) return invalid("has no sidechain Part");
        if (source.sidechain_part == profile.part_id) return invalid("must reference another Part");
        return std::nullopt;
    default:
        if (source.index != 0 || has_cc || has_sidechain)
            return invalid("has non-canonical payload fields");
        return std::nullopt;
    }
}

std::optional<ModulationValidationFailure>
modulation_validation_failure(const TimbreProfile& profile, const ModulationRouting& routing) {
    if (routing.target.empty() || !get_parameter(profile, routing.target)) {
        return ModulationValidationFailure{
            ErrorCode::InvalidModTarget,
            "Modulation target '" + routing.target +
                "' does not resolve to an exact parameter in this TimbreProfile"};
    }
    if (!std::isfinite(routing.depth) || routing.depth < -1.0f || routing.depth > 1.0f) {
        return ModulationValidationFailure{ErrorCode::TimbreInvalidParameter,
                                           "Modulation depth must be finite and in [-1, 1]"};
    }
    if (auto failure = modulation_source_failure(profile, routing.source, "Primary"))
        return failure;
    if (routing.via) {
        if (auto failure = modulation_source_failure(profile, *routing.via, "Via")) return failure;
    }
    return std::nullopt;
}

void check_modulation_routings(const TimbreProfile& profile, std::vector<Diagnostic>& out) {
    for (const auto& routing : profile.modulation.routings) {
        if (auto failure = modulation_validation_failure(profile, routing)) {
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "T7",
                           failure->message,
                           failure->code,
                           profile.part_id);
        }
    }
}

// -------------------------------------------------------------------------
// T12: Owned modulation generator and macro definitions
// -------------------------------------------------------------------------

std::optional<std::string> validate_unit_curve_points(
    const std::vector<std::pair<float, float>>& points, float minimum_y, std::string_view owner) {
    if (points.size() < 2 || points.front().first != 0.0f || points.back().first != 1.0f)
        return std::string(owner) + " custom curve must span x=0 to x=1 with at least two points";
    for (std::size_t index = 0; index < points.size(); ++index) {
        const auto [x, y] = points[index];
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0f || x > 1.0f || y < minimum_y ||
            y > 1.0f || (index != 0 && !(x > points[index - 1].first)))
            return std::string(owner) + " custom curve has an invalid point";
    }
    return std::nullopt;
}

std::optional<std::string> modulation_lfo_failure(const LFO& lfo) {
    if (static_cast<std::underlying_type_t<LFOWaveformType>>(lfo.waveform) >
        static_cast<std::underlying_type_t<LFOWaveformType>>(LFOWaveformType::Custom))
        return "LFO waveform is not a defined value";
    if (lfo.rate.synced) {
        if (!(lfo.rate.division > Beat::zero())) return "Synced LFO division must be positive";
    } else if (!std::isfinite(lfo.rate.hz) || lfo.rate.hz <= 0.0f) {
        return "Free LFO rate must be finite and positive";
    }
    if (!std::isfinite(lfo.phase) || lfo.phase < 0.0f || lfo.phase > 1.0f ||
        !std::isfinite(lfo.symmetry) || lfo.symmetry < 0.0f || lfo.symmetry > 1.0f)
        return "LFO phase and symmetry must be finite and in [0, 1]";
    if (!std::isfinite(lfo.fade_in) || lfo.fade_in < 0.0f || !std::isfinite(lfo.delay) ||
        lfo.delay < 0.0f)
        return "LFO fade and delay must be finite and non-negative";
    if (lfo.waveform == LFOWaveformType::Custom)
        return validate_unit_curve_points(lfo.custom_points, -1.0f, "LFO");
    if (!lfo.custom_points.empty()) return "A non-Custom LFO must not carry custom points";
    return std::nullopt;
}

std::optional<std::string> modulation_envelope_failure(const Envelope& envelope) {
    if (envelope.stages.empty()) return "Modulation envelope must contain at least one stage";
    for (const auto& stage : envelope.stages) {
        if (!std::isfinite(stage.duration) || stage.duration < 0.0f ||
            !std::isfinite(stage.target_level) || stage.target_level < 0.0f ||
            stage.target_level > 1.0f ||
            static_cast<std::underlying_type_t<EnvelopeCurve>>(stage.curve) >
                static_cast<std::underlying_type_t<EnvelopeCurve>>(EnvelopeCurve::Step) ||
            !std::isfinite(stage.curvature) || stage.curvature <= 0.0f)
            return "Modulation envelope contains an invalid stage";
    }
    if (!std::isfinite(envelope.velocity_sensitivity) || envelope.velocity_sensitivity < 0.0f ||
        envelope.velocity_sensitivity > 1.0f || !std::isfinite(envelope.key_tracking) ||
        envelope.key_tracking < 0.0f || envelope.key_tracking > 1.0f)
        return "Modulation envelope sensitivity values must be finite and in [0, 1]";
    if (envelope.loop && (envelope.loop->start_stage >= envelope.stages.size() ||
                          envelope.loop->end_stage >= envelope.stages.size() ||
                          envelope.loop->start_stage > envelope.loop->end_stage))
        return "Modulation envelope loop indices are invalid";
    return std::nullopt;
}

std::optional<std::string> modulation_step_failure(const StepSequencer& sequencer) {
    if (sequencer.steps.empty()) return "Step sequencer must contain at least one step";
    if (!(sequencer.step_duration > Beat::zero())) return "Step duration must be positive";
    if (!std::isfinite(sequencer.smooth) || sequencer.smooth < 0.0f || sequencer.smooth > 1.0f)
        return "Step smoothing must be finite and in [0, 1]";
    if (static_cast<std::underlying_type_t<LoopMode>>(sequencer.loop_mode) >
        static_cast<std::underlying_type_t<LoopMode>>(LoopMode::OneShot))
        return "Step loop mode is not a defined value";
    if (std::any_of(sequencer.steps.begin(), sequencer.steps.end(), [](float step) {
            return !std::isfinite(step) || step < 0.0f || step > 1.0f;
        }))
        return "Step values must be finite and in [0, 1]";
    return std::nullopt;
}

std::optional<std::string> modulation_macro_failure(const TimbreProfile& profile,
                                                    const MacroKnob& macro) {
    if (macro.name.empty()) return "Macro name must not be empty";
    if (!std::isfinite(macro.value) || macro.value < 0.0f || macro.value > 1.0f)
        return "Macro value must be finite and in [0, 1]";
    for (const auto& mapping : macro.mappings) {
        if (mapping.target.empty() || !get_parameter(profile, mapping.target))
            return "Macro target '" + mapping.target + "' does not resolve exactly";
        if (!std::isfinite(mapping.min) || !std::isfinite(mapping.max))
            return "Macro mapping endpoints must be finite";
        if (static_cast<std::underlying_type_t<MappingCurveType>>(mapping.curve.type) >
            static_cast<std::underlying_type_t<MappingCurveType>>(MappingCurveType::Custom))
            return "Macro mapping curve is not a defined value";
        if (mapping.curve.type == MappingCurveType::Custom) {
            if (auto failure =
                    validate_unit_curve_points(mapping.curve.custom_points, 0.0f, "Macro"))
                return failure;
        } else if (!mapping.curve.custom_points.empty()) {
            return "A non-Custom macro curve must not carry custom points";
        }
    }
    return std::nullopt;
}

void check_modulation_definitions(const TimbreProfile& profile, std::vector<Diagnostic>& out) {
    const auto add_failure = [&](std::optional<std::string> failure) {
        if (failure) {
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "T12",
                           *failure,
                           ErrorCode::TimbreInvalidParameter,
                           profile.part_id);
        }
    };
    for (const auto& lfo : profile.modulation.lfos)
        add_failure(modulation_lfo_failure(lfo));
    for (const auto& envelope : profile.modulation.envelopes)
        add_failure(modulation_envelope_failure(envelope));
    for (const auto& sequencer : profile.modulation.step_sequencers)
        add_failure(modulation_step_failure(sequencer));

    std::set<std::uint8_t> macro_indices;
    for (const auto& macro : profile.modulation.macro_knobs) {
        add_failure(modulation_macro_failure(profile, macro));
        if (!macro_indices.insert(macro.index).second)
            add_failure(std::string{"Macro indices must be unique"});
    }
}

// -------------------------------------------------------------------------
// T11: Timbral automation structural validity
// -------------------------------------------------------------------------

struct AutomationValidationFailure {
    ErrorCode code;
    std::string message;
};

std::optional<AutomationValidationFailure>
automation_validation_failure(const TimbreProfile& profile, const TimbreAutomation& automation) {
    if (automation.parameter_path.empty() || !get_parameter(profile, automation.parameter_path)) {
        return AutomationValidationFailure{
            ErrorCode::InvalidModTarget,
            "Automation target '" + automation.parameter_path +
                "' does not resolve to an exact parameter in this TimbreProfile"};
    }
    if (static_cast<std::underlying_type_t<AutomationInterpolation>>(automation.interpolation) >
        static_cast<std::underlying_type_t<AutomationInterpolation>>(
            AutomationInterpolation::Exponential)) {
        return AutomationValidationFailure{ErrorCode::TimbreInvalidParameter,
                                           "Automation interpolation is not a defined value"};
    }
    if (automation.breakpoints.empty()) {
        return AutomationValidationFailure{ErrorCode::TimbreInvalidParameter,
                                           "Automation must contain at least one breakpoint"};
    }
    for (std::size_t i = 0; i < automation.breakpoints.size(); ++i) {
        const auto& breakpoint = automation.breakpoints[i];
        if (breakpoint.time < SCORE_START) {
            return AutomationValidationFailure{ErrorCode::InvalidScoreTime,
                                               "Automation breakpoint[" + std::to_string(i) +
                                                   "] precedes the first valid ScoreTime"};
        }
        if (!std::isfinite(breakpoint.value)) {
            return AutomationValidationFailure{ErrorCode::TimbreInvalidParameter,
                                               "Automation breakpoint[" + std::to_string(i) +
                                                   "] has a non-finite value"};
        }
        if (i != 0 && breakpoint.time <= automation.breakpoints[i - 1].time) {
            return AutomationValidationFailure{
                ErrorCode::InvalidScoreTime,
                "Automation breakpoint times must be strictly increasing"};
        }
    }
    return std::nullopt;
}

void check_automation(const TimbreProfile& profile, std::vector<Diagnostic>& out) {
    for (const auto& automation : profile.parameter_automation) {
        if (auto failure = automation_validation_failure(profile, automation)) {
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "T11",
                           failure->message,
                           failure->code,
                           profile.part_id);
        }
    }
}

// -------------------------------------------------------------------------
// T9: Unmapped rendering parameters
//
// If a TimbreRenderingConfig has a device_type set but parameter_map
// is empty, warn that the render may not produce correct results.
// -------------------------------------------------------------------------

void check_rendering_config(const TimbreProfile& profile,
                            std::vector<Diagnostic>& out,
                            PartId part) {
    const auto& cfg = profile.rendering;
    if (!cfg.device_type.device_name.empty() && cfg.parameter_map.empty()) {
        add_diagnostic(out,
                       ValidationSeverity::Warning,
                       "T9",
                       "TimbreRenderingConfig specifies a device but has no parameter mappings",
                       ErrorCode::UnmappedParameters,
                       part);
    }

    std::size_t requested_device_count = 1;
    if (!profile.insert_chain.bypass_all) {
        requested_device_count += static_cast<std::size_t>(
            std::count_if(profile.insert_chain.effects.begin(),
                          profile.insert_chain.effects.end(),
                          [](const Effect& effect) { return effect.enabled; }));
    }
    std::set<std::pair<std::uint32_t, std::string>> targets;
    for (const auto& [path, mapping] : cfg.parameter_map) {
        std::string error;
        if (path.empty()) {
            error = "Rendering parameter path must not be empty";
        } else if (mapping.parameter_name.empty()) {
            error = "Rendering parameter '" + path + "' has an empty target name";
        } else if (mapping.device_index >= requested_device_count) {
            error = "Rendering parameter '" + path +
                    "' targets a device index outside the "
                    "requested chain";
        } else if (!targets.emplace(mapping.device_index, mapping.parameter_name).second) {
            error = "Rendering parameter '" + path +
                    "' aliases another IR path onto the same target parameter";
        } else {
            auto source_value = get_parameter(profile, path);
            if (!source_value) {
                error = "Rendering parameter path '" + path + "' does not resolve";
            } else if (!map_device_parameter_value(*source_value, mapping)) {
                error = "Rendering parameter '" + path +
                        "' has an invalid domain, curve, or current value";
            }
        }
        if (!error.empty()) {
            add_diagnostic(out,
                           ValidationSeverity::Error,
                           "T10",
                           error,
                           ErrorCode::TimbreInvalidParameter,
                           part);
        }
    }
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

Result<void> validate_timbre_automation(const TimbreProfile& profile,
                                        const TimbreAutomation& automation) {
    if (auto failure = automation_validation_failure(profile, automation))
        return std::unexpected(failure->code);
    return {};
}

Result<void> validate_modulation_routing(const TimbreProfile& profile,
                                         const ModulationRouting& routing) {
    if (auto failure = modulation_validation_failure(profile, routing))
        return std::unexpected(failure->code);
    return {};
}

Result<void> validate_modulation_lfo(const LFO& lfo) {
    if (modulation_lfo_failure(lfo)) return std::unexpected(ErrorCode::TimbreInvalidParameter);
    return {};
}

Result<void> validate_modulation_envelope(const Envelope& envelope) {
    if (modulation_envelope_failure(envelope))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    return {};
}

Result<void> validate_modulation_step_sequencer(const StepSequencer& sequencer) {
    if (modulation_step_failure(sequencer))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    return {};
}

Result<void> validate_modulation_macro(const TimbreProfile& profile, const MacroKnob& macro) {
    if (modulation_macro_failure(profile, macro))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    return {};
}

std::vector<Diagnostic> validate_timbre(const TimbreProfile& profile, float sample_rate_hz) {
    std::vector<Diagnostic> diags;
    const float nyquist = sample_rate_hz / 2.0f;

    // T2: SoundSource validity
    validate_source(profile.source, diags, profile.part_id);

    // T3: Filter cutoff vs Nyquist (also check effect chain filters)
    check_filters_in_source(profile.source, nyquist, diags, profile.part_id);
    for (const auto& fx : profile.insert_chain.effects) {
        std::visit(
            [&](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, DelayEffect>) {
                    if (p.filter) check_filter_nyquist(*p.filter, nyquist, diags, profile.part_id);
                } else if constexpr (std::is_same_v<T, CompressorEffect>) {
                    if (p.sidechain && p.sidechain->filter) {
                        check_filter_nyquist(*p.sidechain->filter, nyquist, diags, profile.part_id);
                    }
                }
            },
            fx.parameters);
    }

    // T4: Oscillator detune
    check_detune(profile.source, diags, profile.part_id);

    // T5: FM feedback
    check_fm_feedback(profile.source, diags, profile.part_id);

    // T6: Effect chain cycle — structurally impossible with Vec; no-op check
    // Included for spec completeness.

    // T6b: Envelope loop stage indices
    check_envelope_loops(profile, diags);

    // T7: Modulation source, target, and depth
    check_modulation_routings(profile, diags);

    // T8: Stale semantic descriptors (requires tracking parameter
    // change timestamps; emit Info if derivation is not Manual and
    // no mechanism exists to verify staleness yet)

    // T9: Rendering config
    check_rendering_config(profile, diags, profile.part_id);

    // T11: Automation target, interpolation, breakpoint time/value/order
    check_automation(profile, diags);

    // T12: Owned modulation generator and macro definitions
    check_modulation_definitions(profile, diags);

    // Sort: Error first, then Warning, then Info
    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_timbre_correspondence(const Score& score,
                                                       const std::vector<TimbreProfile>& profiles) {
    std::vector<const TimbreProfile*> profile_view;
    profile_view.reserve(profiles.size());
    for (const auto& profile : profiles)
        profile_view.push_back(&profile);
    return validate_timbre_correspondence(score, profile_view);
}

std::vector<Diagnostic>
validate_timbre_correspondence(const Score& score, std::span<const TimbreProfile* const> profiles) {
    std::vector<Diagnostic> diags;

    std::unordered_set<std::uint64_t> score_parts;
    for (const auto& part : score.parts)
        score_parts.insert(part.id.value);

    std::unordered_map<std::uint64_t, std::size_t> profile_counts;
    for (const auto* p : profiles) {
        if (p == nullptr) continue;
        ++profile_counts[p->part_id.value];
        if (!score_parts.contains(p->part_id.value)) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "T1",
                           "TimbreProfile " + std::to_string(p->id.value) +
                               " references unknown Part " + std::to_string(p->part_id.value),
                           ErrorCode::UnknownProfilePart,
                           p->part_id);
        }
    }

    for (const auto& part : score.parts) {
        const auto count = profile_counts[part.id.value];
        if (count == 0) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "T1",
                           "Part '" + part.definition.name + "' has no corresponding TimbreProfile",
                           ErrorCode::MissingProfile,
                           part.id);
        } else if (count > 1) {
            add_diagnostic(diags,
                           ValidationSeverity::Error,
                           "T1",
                           "Part '" + part.definition.name + "' has " + std::to_string(count) +
                               " TimbreProfiles; exactly one is required",
                           ErrorCode::DuplicateProfileForPart,
                           part.id);
        }
    }

    return diags;
}

bool is_timbre_valid(const TimbreProfile& profile, float sample_rate_hz) {
    auto diags = validate_timbre(profile, sample_rate_hz);
    return std::none_of(diags.begin(), diags.end(), [](const Diagnostic& d) {
        return d.severity == ValidationSeverity::Error;
    });
}

} // namespace sunny::core
