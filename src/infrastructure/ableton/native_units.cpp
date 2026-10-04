#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <sunny/infrastructure/ableton/detail/native_unit_numeric.hpp>
#include <sunny/infrastructure/ableton/native_units.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::LiveNativeParameterCapability;
using sunny::core::LiveNativePhysicalUnit;
using sunny::core::Result;
using Unit = LiveNativePhysicalUnit;
using native_unit_detail::arithmetic_equal;
using native_unit_detail::descriptor;
using native_unit_detail::display_reading;

bool fields(const json& value, std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&value](auto name) { return value.contains(name); });
}

bool finite(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool integer(const json& value, std::uint64_t upper) {
    return value.is_number_integer() &&
           (value.is_number_unsigned()
                ? value.get<std::uint64_t>() <= upper
                : value.get<std::int64_t>() >= 0 &&
                      static_cast<std::uint64_t>(value.get<std::int64_t>()) <= upper);
}

bool text(const json& value, std::size_t maximum = 4096) {
    return value.is_string() && !value.get_ref<const std::string&>().empty() &&
           value.get_ref<const std::string&>().size() <= maximum;
}

bool nullable_positive(const json& value) {
    return value.is_null() || (finite(value) && value.get<double>() > 0.0);
}

bool index(std::string_view value) {
    return !value.empty() &&
           std::ranges::all_of(value, [](unsigned char c) { return c >= '0' && c <= '9'; });
}

bool device_path(const LomPath& path) {
    if (!path.is_canonical()) return false;
    const auto& s = path.segments;
    return (s.size() == 5 && (s[1] == "tracks" || s[1] == "return_tracks") && index(s[2]) &&
            s[3] == "devices" && index(s[4])) ||
           (s.size() == 4 && s[1] == "master_track" && s[2] == "devices" && index(s[3]));
}

const char* unit_name(Unit unit) {
    switch (unit) {
    case Unit::Decibels:
        return "Decibels";
    case Unit::Hertz:
        return "Hertz";
    case Unit::QualityFactor:
        return "QualityFactor";
    case Unit::Percent:
        return "Percent";
    case Unit::StereoBalance:
        return "StereoBalance";
    case Unit::Milliseconds:
        return "Milliseconds";
    }
    return "";
}

const LiveNativeParameterCapability* capability(const std::string& id) {
    const auto registry = sunny::core::live_native_parameter_registry();
    const auto found = std::ranges::find(registry, id, &LiveNativeParameterCapability::id);
    return found != registry.end() &&
                   found->kind == sunny::core::LiveNativeParameterKind::Continuous &&
                   found->physical_unit
               ? &*found
               : nullptr;
}

const json* payload(const LomRequest& request) {
    if (request.type != LomRequestType::CallMethod ||
        request.property_or_method != "sunny_resolve_native_display_value" ||
        !device_path(request.path) || request.args.size() != 1)
        return nullptr;
    const auto* value = std::get_if<json>(&request.args.front());
    if (value == nullptr || !fields(*value, {"capability_id", "target", "tolerance"}) ||
        !text(value->at("capability_id"), 64) || !finite(value->at("target")) ||
        !finite(value->at("tolerance")) || value->at("tolerance").get<double>() < 0.0 ||
        !capability(value->at("capability_id").get<std::string>()))
        return nullptr;
    return value;
}

std::optional<std::size_t> lookup(const json& population, std::string_view name) {
    std::optional<std::size_t> result;
    for (std::size_t i = 0; i < population.size(); ++i) {
        const auto& member = population[i];
        if (member.at("name") == name || member.at("original_name") == name) {
            if (result || member.at("original_name") != name) return std::nullopt;
            result = i;
        }
    }
    return result;
}

bool modes_valid(const LiveNativeParameterCapability& entry,
                 const json& population,
                 const json& modes,
                 const json& scale_display) {
    if (!modes.is_object()) return false;
    if (entry.device_class_name == "Drift") {
        const bool filter = entry.parameter_original_name == "LP Freq";
        if (modes.size() != (filter ? 3U : 2U) || !scale_display.is_null()) return false;
        for (const auto* name : {"voice_mode", "voice_count"}) {
            if (!modes.contains(name) || !fields(modes.at(name), {"index", "value_items", "label"}))
                return false;
            const auto& mode = modes.at(name);
            const auto& items = mode.at("value_items");
            if (!items.is_array() || items.empty() || items.size() > 64 ||
                !integer(mode.at("index"), items.size() - 1))
                return false;
            std::set<std::string> unique;
            for (const auto& item : items)
                if (!text(item) || !unique.insert(item.get<std::string>()).second) return false;
            if (mode.at("label") != items.at(mode.at("index").get<std::size_t>())) return false;
        }
        return !filter || (lookup(population, "LP Type") && modes.contains("LP Type") &&
                           descriptor(modes.at("LP Type"), true, true) &&
                           modes.at("LP Type").at("value_items").size() == 2);
    }
    auto enum_mode =
        [&](std::string_view name, std::string_view label, bool off_on, bool eligible = true) {
            if (!lookup(population, name) || !modes.contains(name) ||
                !descriptor(modes.at(name), true, eligible) || modes.at(name).at("label") != label)
                return false;
            if (!off_on) return true;
            const auto& items = modes.at(name).at("value_items");
            return items.size() == 2 && ((items[0] == "Off" && items[1] == "On") ||
                                         (items[0] == "On" && items[1] == "Off"));
        };
    if (entry.device_class_name == "StereoGain") {
        const bool width = entry.parameter_original_name == "Stereo Width";
        const bool alternative = width && std::ranges::any_of(population, [](const auto& member) {
                                     return member.at("name") == "Mid/Side Balance" ||
                                            member.at("original_name") == "Mid/Side Balance";
                                 });
        if (modes.size() != (alternative ? 4U : 3U) || !scale_display.is_null() ||
            !enum_mode("Channel Mode", "Stereo", false) || !enum_mode("Mono", "Off", true) ||
            !enum_mode("Mute", "Off", true))
            return false;
        if (alternative) {
            if (!lookup(population, "Mid/Side Balance") || !modes.contains("Mid/Side Balance"))
                return false;
            const auto& mode = modes.at("Mid/Side Balance");
            return fields(mode, {"is_enabled", "state"}) && mode.at("is_enabled").is_boolean() &&
                   integer(mode.at("state"), 2) &&
                   (!mode.at("is_enabled").get<bool>() || mode.at("state") != 0);
        }
        return true;
    }
    if (entry.device_class_name != "Eq8" || modes.size() != (entry.band ? 4U : 3U) ||
        !modes.contains("global_mode") || !integer(modes.at("global_mode"), 0) ||
        !lookup(population, "Scale") || !modes.contains("Scale") ||
        !descriptor(modes.at("Scale"), false, false) || !lookup(population, "Adaptive Q") ||
        !modes.contains("Adaptive Q") || !descriptor(modes.at("Adaptive Q"), true, false))
        return false;
    const auto& adaptive = modes.at("Adaptive Q");
    if (!enum_mode("Adaptive Q", adaptive.at("label").get<std::string>(), true, false) ||
        (entry.band && !enum_mode(std::to_string(entry.band) + " Filter On A", "On", true)) ||
        !fields(scale_display, {"display", "display_value", "display_increment"}) ||
        !text(scale_display.at("display"), 80) || !finite(scale_display.at("display_value")) ||
        !finite(scale_display.at("display_increment")))
        return false;
    const auto reading =
        display_reading(scale_display.at("display").get<std::string>(), Unit::Percent, false);
    return reading && arithmetic_equal(reading->value, scale_display.at("display_value")) &&
           arithmetic_equal(*reading->increment, scale_display.at("display_increment"));
}

Result<NativeDisplayCandidate>
parse_candidate(const LomRequest& request, const json& intent, const json& value) {
    auto malformed = []() -> Result<NativeDisplayCandidate> {
        return std::unexpected(ErrorCode::ProtocolError);
    };
    const auto* entry = capability(intent.at("capability_id").get<std::string>());
    if (!fields(value,
                {"schema_version",
                 "device_class_name",
                 "parameter_original_name",
                 "parameter_index",
                 "unit",
                 "target",
                 "display_tolerance",
                 "internal_value",
                 "display",
                 "display_value",
                 "display_increment",
                 "absolute_display_error",
                 "balance_full_scale",
                 "descriptor",
                 "modes",
                 "eq8_scale_display",
                 "population",
                 "formatter_calls",
                 "samples",
                 "qualification",
                 "source_commit",
                 "host_qualified",
                 "native_knob_only",
                 "coverage_limits"}) ||
        !integer(value.at("schema_version"), 1) || value.at("schema_version") != 1 ||
        value.at("device_class_name") != entry->device_class_name ||
        value.at("parameter_original_name") != entry->parameter_original_name ||
        value.at("unit") != unit_name(*entry->physical_unit) || !finite(value.at("target")) ||
        !finite(value.at("display_tolerance")) ||
        value.at("target").get<double>() != intent.at("target").get<double>() ||
        value.at("display_tolerance").get<double>() != intent.at("tolerance").get<double>() ||
        value.at("qualification") != "ObservedNativeDisplayCandidate" ||
        value.at("source_commit") !=
            sunny::core::live_native_registry_provenance().python_source_commit ||
        value.at("host_qualified") != false || value.at("native_knob_only") != true ||
        !descriptor(value.at("descriptor"), false, true) ||
        !integer(value.at("formatter_calls"), SUNNY_NATIVE_DISPLAY_FORMATTER_BUDGET) ||
        !integer(value.at("parameter_index"), 511) || !text(value.at("display"), 80) ||
        !finite(value.at("internal_value")) || !finite(value.at("display_value")) ||
        !nullable_positive(value.at("display_increment")) ||
        !nullable_positive(value.at("balance_full_scale")) ||
        !finite(value.at("absolute_display_error")))
        return malformed();
    const auto& population = value.at("population");
    if (!population.is_array() || population.empty() || population.size() > 512) return malformed();
    for (const auto& member : population)
        if (!fields(member, {"name", "original_name"}) || !text(member.at("name")) ||
            !text(member.at("original_name")))
            return malformed();
    const auto found = lookup(population, entry->parameter_original_name);
    if (!found || *found != value.at("parameter_index").get<std::uint32_t>() ||
        !modes_valid(*entry, population, value.at("modes"), value.at("eq8_scale_display")))
        return malformed();
    const auto& limits = value.at("coverage_limits");
    if (!limits.is_array() || limits.empty() || limits.size() > 16) return malformed();
    std::set<std::string> unique_limits;
    for (const auto& limit : limits)
        if (!text(limit) || !unique_limits.insert(limit.get<std::string>()).second)
            return malformed();
    const auto unit = *entry->physical_unit;
    const auto numeric =
        native_unit_detail::numerical_display_proof(value,
                                                    unit,
                                                    intent.at("target"),
                                                    intent.at("tolerance"),
                                                    entry->id == "utility.gain",
                                                    entry->device_class_name == "Eq8" ? 1 : 0);
    if (!numeric) return malformed();
    NativeDisplayCandidate result;
    result.device_path = request.path.to_string();
    result.capability_id = entry->id;
    result.device_class_name = entry->device_class_name;
    result.parameter_original_name = entry->parameter_original_name;
    result.parameter_index = static_cast<std::uint32_t>(*found);
    result.unit = unit;
    result.target = numeric->target;
    result.tolerance = numeric->tolerance;
    result.internal_value = numeric->internal_value;
    result.display = numeric->display;
    result.display_value = numeric->display_value;
    result.display_increment = numeric->display_increment;
    result.absolute_display_error = numeric->absolute_display_error;
    result.balance_full_scale = numeric->balance_full_scale;
    result.descriptor = numeric->descriptor;
    result.formatter_calls = numeric->formatter_calls;
    result.samples = numeric->samples;
    result.population = population;
    result.modes = value.at("modes");
    result.eq8_scale_display = value.at("eq8_scale_display");
    result.qualification = value.at("qualification");
    result.source_commit = value.at("source_commit");
    result.coverage_limits = limits.get<std::vector<std::string>>();
    result.evidence = value;
    return result;
}
} // namespace

Result<LomRequest> make_native_display_resolution_request(const LomPath& path,
                                                          const std::string& capability_id,
                                                          double target,
                                                          double tolerance) {
    if (!device_path(path) || !capability(capability_id) || !std::isfinite(target) ||
        !std::isfinite(tolerance) || tolerance < 0.0)
        return std::unexpected(ErrorCode::ProtocolError);
    return LomRequest{
        LomRequestType::CallMethod,
        path,
        "sunny_resolve_native_display_value",
        {json{{"capability_id", capability_id}, {"target", target}, {"tolerance", tolerance}}}};
}

Result<NativeDisplayResolution> parse_native_display_resolution(const LomRequest& request,
                                                                const json& response) {
    const auto* intent = payload(request);
    if (!intent || !response.is_object() || !response.contains("outcome"))
        return std::unexpected(ErrorCode::ProtocolError);
    const bool resolved = response.at("outcome") == "resolved";
    if (!(resolved ? fields(response,
                            {"schema_version",
                             "capability_id",
                             "target",
                             "tolerance",
                             "outcome",
                             "candidate"})
                   : fields(response,
                            {"schema_version",
                             "capability_id",
                             "target",
                             "tolerance",
                             "outcome",
                             "reason",
                             "diagnostic",
                             "formatter_calls"})) ||
        !integer(response.at("schema_version"), 1) || response.at("schema_version") != 1 ||
        response.at("capability_id") != intent->at("capability_id") ||
        !finite(response.at("target")) || !finite(response.at("tolerance")) ||
        response.at("target").get<double>() != intent->at("target").get<double>() ||
        response.at("tolerance").get<double>() != intent->at("tolerance").get<double>())
        return std::unexpected(ErrorCode::ProtocolError);
    NativeDisplayResolution result;
    result.evidence = response;
    if (resolved) {
        auto parsed = parse_candidate(request, *intent, response.at("candidate"));
        if (!parsed) return std::unexpected(parsed.error());
        result.status = NativeDisplayResolutionStatus::Candidate;
        result.formatter_calls = parsed->formatter_calls;
        result.candidate = std::move(*parsed);
    } else {
        if (response.at("outcome") != "declined" || !text(response.at("reason"), 64) ||
            !text(response.at("diagnostic")) ||
            !integer(response.at("formatter_calls"), SUNNY_NATIVE_DISPLAY_FORMATTER_BUDGET))
            return std::unexpected(ErrorCode::ProtocolError);
        result.status = NativeDisplayResolutionStatus::Declined;
        result.reason = response.at("reason");
        result.diagnostic = response.at("diagnostic");
        result.formatter_calls = response.at("formatter_calls");
    }
    return result;
}

Result<NativeDisplayResolution> resolve_native_display_value(const LomPath& path,
                                                             const std::string& capability_id,
                                                             double target,
                                                             double tolerance,
                                                             LomTransport& transport) {
    auto request = make_native_display_resolution_request(path, capability_id, target, tolerance);
    if (!request) return std::unexpected(request.error());
    if (transport.records_without_execution()) {
        NativeDisplayResolution result;
        result.reason = "ObservationUnavailable";
        result.diagnostic = "A recording-only transport cannot observe native display conversion";
        return result;
    }
    const auto response = transport.send(*request);
    if (!response.success) return std::unexpected(ErrorCode::SendFailed);
    if (!response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ProtocolError);
    return parse_native_display_resolution(*request, std::get<json>(*response.value));
}
} // namespace sunny::infrastructure
