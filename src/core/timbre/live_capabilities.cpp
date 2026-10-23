/**
 * @file live_capabilities.cpp
 * @brief Finite native Live source-candidate registry; no host access or guessed units
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <sunny/core/timbre/live_capabilities.hpp>
#include <tuple>
#include <utility>

namespace sunny::core {

namespace {

using Disposition = LiveNativePreflightDisposition;
using Kind = LiveNativeParameterKind;
using Mode = LiveNativeModeRequirement;
using Unit = LiveNativePhysicalUnit;

LiveNativeMappingPreflight decline(Disposition reason, std::string diagnostic) {
    return {reason, std::nullopt, std::move(diagnostic)};
}

struct ParameterLookup {
    const LiveNativeParameterProbe* parameter = nullptr;
    std::uint32_t index = 0;
    Disposition disposition = Disposition::ParameterMismatch;
};

ParameterLookup parameter_by_original_name(const LiveNativeDeviceProbe& device,
                                           const std::string& original_name) {
    ParameterLookup result;
    for (std::size_t index = 0; index < device.parameters.size(); ++index) {
        const auto& parameter = device.parameters[index];
        // Match the bridge's exact original/public-name resolver so a public
        // alias cannot make an otherwise unique original name ambiguous.
        if (*parameter.original_name == original_name || *parameter.name == original_name) {
            if (result.parameter) return {nullptr, 0, Disposition::AmbiguousParameter};
            result.parameter = &parameter;
            result.index = static_cast<std::uint32_t>(index);
        }
    }
    if (result.parameter && *result.parameter->original_name == original_name)
        result.disposition = Disposition::CandidateReady;
    else
        result.parameter = nullptr;
    return result;
}

Disposition validate_parameter(const LiveNativeParameterProbe& parameter,
                               Kind kind,
                               std::optional<std::uint32_t> expected_items = std::nullopt) {
    if (!parameter.actual_device_parameter || !parameter.minimum || !parameter.maximum ||
        !parameter.value || !parameter.is_quantized || !parameter.is_enabled || !parameter.state ||
        !parameter.automation_state)
        return Disposition::ObservationUnavailable;
    if (!*parameter.actual_device_parameter) return Disposition::ParameterMismatch;
    const double minimum = *parameter.minimum;
    const double maximum = *parameter.maximum;
    const double value = *parameter.value;
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(value) ||
        minimum >= maximum || value < minimum || value > maximum || *parameter.state > 2 ||
        *parameter.automation_state > 2)
        return Disposition::InvalidDomain;
    if (!*parameter.is_enabled || *parameter.state != 0) return Disposition::InactiveParameter;
    // Initial static/envelope preflight must not override playing or overridden
    // automation. Later envelope readback has a separate read-only contract.
    if (*parameter.automation_state != 0) return Disposition::ExistingAutomation;
    if (*parameter.is_quantized != (kind == Kind::Quantized)) return Disposition::InvalidDomain;
    if (kind == Kind::Quantized) {
        if (!parameter.value_items) return Disposition::ObservationUnavailable;
        const auto& items = *parameter.value_items;
        // Pinned Push2/device_options.py indexes labels by integer value and
        // writes zero-based indices. Admit that observed shape only.
        if (items.size() < 2 || items.size() > std::numeric_limits<std::uint32_t>::max() ||
            minimum != 0.0 || maximum != static_cast<double>(items.size() - 1) ||
            std::trunc(value) != value || (expected_items && items.size() != *expected_items))
            return Disposition::InvalidDomain;
        std::set<std::string> unique;
        for (const auto& label : items)
            if (label.empty() || !unique.insert(label).second) return Disposition::InvalidDomain;
    }
    return Disposition::CandidateReady;
}

Disposition require_enum_mode(const LiveNativeDeviceProbe& device,
                              const std::string& name,
                              const std::string& label,
                              bool on_off = false) {
    const auto found = parameter_by_original_name(device, name);
    if (!found.parameter)
        return found.disposition == Disposition::ParameterMismatch
                   ? Disposition::ObservationUnavailable
                   : found.disposition;
    const auto valid = validate_parameter(
        *found.parameter, Kind::Quantized, on_off ? std::optional<std::uint32_t>{2} : std::nullopt);
    if (valid != Disposition::CandidateReady) return valid;
    const auto& items = *found.parameter->value_items;
    if (on_off && (std::find(items.begin(), items.end(), "Off") == items.end() ||
                   std::find(items.begin(), items.end(), "On") == items.end()))
        return Disposition::UnsupportedMode;
    const auto index = static_cast<std::size_t>(*found.parameter->value);
    return items[index] == label ? Disposition::CandidateReady : Disposition::UnsupportedMode;
}

Disposition validate_modes(const LiveNativeParameterCapability& capability,
                           const LiveNativeDeviceProbe& device) {
    if (capability.mode == Mode::OwnedEffectSetup) return Disposition::CandidateReady;
    if (capability.mode == Mode::DriftVoice || capability.mode == Mode::DriftFilter) {
        for (const auto* name : {"voice_mode", "voice_count"}) {
            const auto index = device.integer_properties.find(std::string{name} + "_index");
            const auto list = device.string_list_properties.find(std::string{name} + "_list");
            if (index == device.integer_properties.end() ||
                list == device.string_list_properties.end())
                return Disposition::ObservationUnavailable;
            if (list->second.empty() || list->second.size() > 64 || index->second < 0 ||
                static_cast<std::size_t>(index->second) >= list->second.size())
                return Disposition::InvalidDomain;
            std::set<std::string> unique;
            for (const auto& item : list->second)
                if (item.empty() || !unique.insert(item).second) return Disposition::InvalidDomain;
        }
        if (capability.mode == Mode::DriftFilter) {
            const auto type = parameter_by_original_name(device, "LP Type");
            if (!type.parameter)
                return type.disposition == Disposition::ParameterMismatch
                           ? Disposition::ObservationUnavailable
                           : type.disposition;
            return validate_parameter(*type.parameter, Kind::Quantized, 2);
        }
    } else if (capability.mode == Mode::UtilityStereo) {
        for (const auto& [name, label] : std::array{std::pair{"Channel Mode", "Stereo"},
                                                    std::pair{"Mono", "Off"},
                                                    std::pair{"Mute", "Off"}}) {
            const auto valid =
                require_enum_mode(device, name, label, name != std::string{"Channel Mode"});
            if (valid != Disposition::CandidateReady) return valid;
        }
        if (capability.parameter_original_name == "Stereo Width") {
            const auto alternative = parameter_by_original_name(device, "Mid/Side Balance");
            if (alternative.disposition == Disposition::AmbiguousParameter)
                return alternative.disposition;
            if (alternative.parameter) {
                if (!alternative.parameter->is_enabled || !alternative.parameter->state)
                    return Disposition::ObservationUnavailable;
                if (*alternative.parameter->state > 2) return Disposition::InvalidDomain;
                if (*alternative.parameter->is_enabled && *alternative.parameter->state == 0)
                    return Disposition::UnsupportedMode;
            }
        }
    } else {
        const auto mode = device.integer_properties.find("global_mode");
        if (mode == device.integer_properties.end()) return Disposition::ObservationUnavailable;
        if (mode->second != 0) return Disposition::UnsupportedMode;
        if (capability.mode == Mode::Eq8StereoBandOn)
            return require_enum_mode(
                device, std::to_string(capability.band) + " Filter On A", "On", true);
    }
    return Disposition::CandidateReady;
}

} // namespace

const LiveNativeRegistryProvenance& live_native_registry_provenance() {
    static const LiveNativeRegistryProvenance provenance{
        LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION,
        {12, 4, 5},
        {12, 3, 0},
        {12, 4, UINT16_MAX},
        "https://docs.cycling74.com/apiref/lom/",
        "https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/",
        "e83d5192f321b24eb9daab843ac49a2d95d862b1",
        {"_Generic/Devices.py:394-404,560-565",
         "Push2/custom_bank_definitions.py:2619-2640",
         "Push2/device_options.py:49-83",
         "Push2/eq8.py:16-35",
         "Push2/custom_bank_definitions.py:1565-1625",
         "Push2/drift.py:45-52",
         "_MxDCore/LomTypes.py:375-390,425-429"},
        false};
    return provenance;
}

std::span<const LiveNativeParameterCapability> live_native_parameter_registry() {
    static const auto entries = [] {
        std::vector<LiveNativeParameterCapability> result;
        result.reserve(59);
        for (const auto& [id, parameter, unit] :
             std::array{std::tuple{"utility.gain", "Gain", Unit::Decibels},
                        std::tuple{"utility.balance", "Balance", Unit::StereoBalance},
                        std::tuple{"utility.width", "Stereo Width", Unit::Percent}})
            result.push_back(
                {id,
                 "Utility",
                 "StereoGain",
                 parameter,
                 Kind::Continuous,
                 std::nullopt,
                 unit,
                 Mode::UtilityStereo,
                 0,
                 "_Generic/Devices.py:560-565;Push2/custom_bank_definitions.py:2619-2640"});
        for (const auto& [id, name, count] :
             std::array{std::tuple{"utility.enabled", "Device On", 2u},
                        std::tuple{"utility.channel_mode", "Channel Mode", 4u},
                        std::tuple{"utility.mono", "Mono", 2u},
                        std::tuple{"utility.mute", "Mute", 2u},
                        std::tuple{"utility.left_invert", "Left Inv", 2u},
                        std::tuple{"utility.right_invert", "Right Inv", 2u},
                        std::tuple{"utility.bass_mono", "Bass Mono", 2u},
                        std::tuple{"utility.dc_filter", "DC Filter", 2u}})
            result.push_back(
                {id,
                 "Utility",
                 "StereoGain",
                 name,
                 Kind::Quantized,
                 count,
                 std::nullopt,
                 Mode::OwnedEffectSetup,
                 0,
                 "_Generic/Devices.py:560-565;Push2/custom_bank_definitions.py:2619-2640"});
        for (const auto& [id, name] : std::array{std::pair{"eq8.enabled", "Device On"},
                                                 std::pair{"eq8.adaptive_q", "Adaptive Q"}})
            result.push_back({id,
                              "EQ Eight",
                              "Eq8",
                              name,
                              Kind::Quantized,
                              2,
                              std::nullopt,
                              Mode::OwnedEffectSetup,
                              0,
                              "_Generic/Devices.py:394-404;Push2/device_options.py:84-98"});
        result.push_back({"eq8.scale",
                          "EQ Eight",
                          "Eq8",
                          "Scale",
                          Kind::Continuous,
                          std::nullopt,
                          Unit::Percent,
                          Mode::Eq8Stereo,
                          0,
                          "_Generic/Devices.py:404;Push2/custom_bank_definitions.py:1956"});
        struct BandControl {
            const char* id;
            const char* suffix;
            Kind kind;
            std::optional<std::uint32_t> enum_items;
            std::optional<Unit> unit;
        };
        const std::array controls{
            BandControl{"frequency", "Frequency A", Kind::Continuous, std::nullopt, Unit::Hertz},
            BandControl{"gain", "Gain A", Kind::Continuous, std::nullopt, Unit::Decibels},
            BandControl{"q", "Resonance A", Kind::Continuous, std::nullopt, Unit::QualityFactor},
            BandControl{"enabled", "Filter On A", Kind::Quantized, 2, std::nullopt},
            BandControl{"type", "Filter Type A", Kind::Quantized, 8, std::nullopt}};
        for (std::uint8_t band = 1; band <= 8; ++band)
            for (const auto& control : controls)
                result.push_back(
                    {"eq8.band." + std::to_string(band) + "." + control.id,
                     "EQ Eight",
                     "Eq8",
                     std::to_string(band) + " " + control.suffix,
                     control.kind,
                     control.enum_items,
                     control.unit,
                     control.kind == Kind::Quantized && std::string{control.id} == "enabled"
                         ? Mode::Eq8Stereo
                         : Mode::Eq8StereoBandOn,
                     band,
                     "_Generic/Devices.py:394-404"});
        result.push_back({"eq8.output_gain",
                          "EQ Eight",
                          "Eq8",
                          "Output Gain",
                          Kind::Continuous,
                          std::nullopt,
                          Unit::Decibels,
                          Mode::Eq8Stereo,
                          0,
                          "_Generic/Devices.py:404"});
        for (const auto& [id, parameter, unit, mode] : std::array{
                 std::tuple{"drift.lp.frequency", "LP Freq", Unit::Hertz, Mode::DriftFilter},
                 std::tuple{
                     "drift.env.1.attack", "Env 1 Attack", Unit::Milliseconds, Mode::DriftVoice},
                 std::tuple{
                     "drift.env.1.decay", "Env 1 Decay", Unit::Milliseconds, Mode::DriftVoice},
                 std::tuple{
                     "drift.env.1.release", "Env 1 Release", Unit::Milliseconds, Mode::DriftVoice}})
            result.push_back({id,
                              "Drift",
                              "Drift",
                              parameter,
                              Kind::Continuous,
                              std::nullopt,
                              unit,
                              mode,
                              0,
                              "Push2/custom_bank_definitions.py:1565-1625;Push2/drift.py:45-52"});
        return result;
    }();
    return entries;
}

LiveNativeMappingPreflight preflight_live_native_parameter(const std::string& capability_id,
                                                           const LiveNativeParameterIntent& intent,
                                                           const LiveNativeDeviceProbe& observed,
                                                           std::uint32_t expected_chain_index) {
    const auto& provenance = live_native_registry_provenance();
    if (observed.version < provenance.first_candidate_version ||
        observed.version > provenance.last_candidate_version)
        return decline(Disposition::UnknownRegistryCoverage,
                       "Version is outside this source-candidate registry scope; host support "
                       "remains unknown");
    const auto registry = live_native_parameter_registry();
    const auto entry = std::find_if(registry.begin(), registry.end(), [&](const auto& candidate) {
        return candidate.id == capability_id;
    });
    if (entry == registry.end())
        return decline(Disposition::UnknownCapability, "Parameter is outside the finite registry");
    if (!observed.class_name || !observed.chain_index || !observed.device_type ||
        !observed.is_active || !observed.can_have_chains || !observed.parameter_population_observed)
        return decline(
            Disposition::ObservationUnavailable,
            "Native device identity/order/activity or full parameter population was not observed");
    const std::uint8_t required_type = entry->device_class_name == "Drift" ? 1 : 2;
    if (*observed.class_name != entry->device_class_name ||
        *observed.device_type != required_type || !*observed.is_active || *observed.can_have_chains)
        return decline(Disposition::DeviceMismatch,
                       "Expected an active flat native device with the registered class and role");
    if (*observed.chain_index != expected_chain_index)
        return decline(Disposition::WrongChainOrder,
                       "Actual native chain order differs from the planned position");
    if (observed.parameters.size() > std::numeric_limits<std::uint32_t>::max())
        return decline(Disposition::InvalidDomain,
                       "Parameter population exceeds the admitted index domain");
    for (const auto& parameter : observed.parameters)
        if (!parameter.name || parameter.name->empty() || !parameter.original_name ||
            parameter.original_name->empty())
            return decline(Disposition::ObservationUnavailable,
                           "Complete original/public parameter names are needed to rule out "
                           "ambiguous aliases");
    const auto found = parameter_by_original_name(observed, entry->parameter_original_name);
    if (!found.parameter)
        return decline(found.disposition,
                       "Registered original parameter name is absent, replaced, or ambiguous");
    const auto valid =
        validate_parameter(*found.parameter, entry->kind, entry->expected_enum_items);
    if (valid != Disposition::CandidateReady)
        return decline(valid,
                       "Actual parameter type/domain/enablement/state or initial automation "
                       "differs from the required descriptor");
    const auto mode = validate_modes(*entry, observed);
    if (mode != Disposition::CandidateReady)
        return decline(
            mode, "Required native mode/band activation was not explicitly observed and matched");

    double value = 0.0;
    LiveNativeValueMapping mapping = LiveNativeValueMapping::InternalIdentity;
    if (const auto* internal = std::get_if<LiveNativeInternalValue>(&intent)) {
        value = internal->value;
        if (!std::isfinite(value))
            return decline(Disposition::InvalidIntent, "Requested internal value must be finite");
    } else if (const auto* physical = std::get_if<LiveNativePhysicalValue>(&intent)) {
        if (!std::isfinite(physical->value) || physical->unit < Unit::Decibels ||
            physical->unit > Unit::Milliseconds)
            return decline(Disposition::InvalidIntent,
                           "Requested physical value and unit must be valid and finite");
        return decline(Disposition::UnsupportedUnitMapping,
                       "Physical-to-internal conversion is unqualified; native bounds, names or "
                       "GUI units do not define it");
    } else {
        const auto& label = std::get<LiveNativeEnumChoice>(intent).label;
        if (entry->kind != Kind::Quantized || label.empty())
            return decline(Disposition::InvalidIntent,
                           "Enum intent requires a registered quantized parameter and an exact "
                           "advertised label");
        const auto& items = *found.parameter->value_items;
        const auto choice = std::find(items.begin(), items.end(), label);
        if (choice == items.end())
            return decline(Disposition::UnknownEnumChoice,
                           "Requested label is not advertised; no locale or semantic enum "
                           "translation is inferred");
        value = static_cast<double>(std::distance(items.begin(), choice));
        mapping = LiveNativeValueMapping::AdvertisedEnumIndex;
    }
    if (value < *found.parameter->minimum || value > *found.parameter->maximum ||
        (entry->kind == Kind::Quantized && std::trunc(value) != value))
        return decline(Disposition::ValueOutsideDomain,
                       "Requested native value is outside the actual continuous/quantized domain");

    LiveNativeParameterCandidate candidate;
    candidate.capability_id = entry->id;
    candidate.device_class_name = *observed.class_name;
    candidate.device_chain_index = *observed.chain_index;
    candidate.observed_parameter_index = found.index;
    candidate.parameter_original_name = entry->parameter_original_name;
    candidate.minimum = *found.parameter->minimum;
    candidate.maximum = *found.parameter->maximum;
    candidate.internal_value = value;
    candidate.mapping = mapping;
    candidate.continuous_internal_step_candidate = entry->kind == Kind::Continuous;
    return {Disposition::CandidateReady,
            std::move(candidate),
            "Runtime descriptors matched a source candidate; host, unit, envelope and persistence "
            "qualification remain separate"};
}

} // namespace sunny::core
