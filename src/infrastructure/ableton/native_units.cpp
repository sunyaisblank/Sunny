#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <sunny/infrastructure/ableton/native_units.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::LiveNativeParameterCapability;
using sunny::core::LiveNativePhysicalUnit;
using sunny::core::Result;
using Unit = LiveNativePhysicalUnit;

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

bool descriptor(const json& value, bool quantized, bool eligible) {
    if (!(quantized ? fields(value,
                             {"minimum",
                              "maximum",
                              "value",
                              "is_quantized",
                              "is_enabled",
                              "state",
                              "automation_state",
                              "value_items",
                              "label"})
                    : fields(value,
                             {"minimum",
                              "maximum",
                              "value",
                              "is_quantized",
                              "is_enabled",
                              "state",
                              "automation_state",
                              "default_value"})) ||
        !finite(value.at("minimum")) || !finite(value.at("maximum")) ||
        !finite(value.at("value")) || !value.at("is_quantized").is_boolean() ||
        value.at("is_quantized").get<bool>() != quantized || !value.at("is_enabled").is_boolean() ||
        !integer(value.at("state"), 2) || !integer(value.at("automation_state"), 2))
        return false;
    const auto minimum = value.at("minimum").get<double>();
    const auto maximum = value.at("maximum").get<double>();
    const auto current = value.at("value").get<double>();
    if (minimum >= maximum || current < minimum || current > maximum ||
        (eligible && (!value.at("is_enabled").get<bool>() || value.at("state") != 0 ||
                      value.at("automation_state") != 0)))
        return false;
    if (!quantized)
        return finite(value.at("default_value")) &&
               minimum <= value.at("default_value").get<double>() &&
               value.at("default_value").get<double>() <= maximum;
    const auto& items = value.at("value_items");
    if (!items.is_array() || items.size() < 2 || items.size() > 64 || minimum != 0.0 ||
        maximum != static_cast<double>(items.size() - 1) || std::trunc(current) != current ||
        !text(value.at("label")))
        return false;
    std::set<std::string> labels;
    for (const auto& item : items)
        if (!text(item) || !labels.insert(item.get<std::string>()).second) return false;
    return items.at(static_cast<std::size_t>(current)) == value.at("label");
}

// Allow only floating-point representation differences in encoded arithmetic;
// this is not an enlargement of the caller's physical display tolerance.
bool arithmetic_equal(double first, double second) {
    if (!std::isfinite(first) || !std::isfinite(second)) return false;
    if (first == second) return true;
    const double scale = std::max(std::abs(first), std::abs(second));
    return std::abs(first - second) <=
           std::max(std::numeric_limits<double>::denorm_min(),
                    scale * (8.0 * std::numeric_limits<double>::epsilon()));
}

struct DisplayReading {
    double value = 0.0;
    std::optional<double> increment;
    bool negative_infinity = false;
};

std::optional<DisplayReading> display_reading(std::string_view raw, Unit unit, bool gain_infinity) {
    if (raw.empty() || raw.size() > 80) return std::nullopt;
    const auto first = raw.find_first_not_of(" \t");
    if (first == std::string_view::npos) return std::nullopt;
    raw = raw.substr(first, raw.find_last_not_of(" \t") - first + 1);
    if (gain_infinity && (raw == "-inf dB" || raw == "-∞ dB" || raw == "−inf dB" || raw == "−∞ dB"))
        return DisplayReading{0.0, std::nullopt, true};
    if (unit == Unit::StereoBalance && raw == "C") return DisplayReading{};
    std::size_t position = 0;
    if (raw[position] == '+' || raw[position] == '-') ++position;
    const auto begin = position;
    while (position < raw.size() && raw[position] >= '0' && raw[position] <= '9')
        ++position;
    const auto integral_digits = position - begin;
    std::size_t fractional_digits = 0;
    if (position < raw.size() && raw[position] == '.') {
        const auto fractional_begin = ++position;
        while (position < raw.size() && raw[position] >= '0' && raw[position] <= '9')
            ++position;
        fractional_digits = position - fractional_begin;
        if (fractional_digits == 0) return std::nullopt;
    }
    if (integral_digits == 0 && fractional_digits == 0) return std::nullopt;
    auto token = raw.substr(0, position);
    auto suffix = raw.substr(position);
    if (const auto offset = suffix.find_first_not_of(" \t"); offset != std::string_view::npos)
        suffix.remove_prefix(offset);
    else
        suffix = {};
    double multiplier = 1.0;
    switch (unit) {
    case Unit::Decibels:
        if (suffix != "dB") return std::nullopt;
        break;
    case Unit::Hertz:
        if (suffix == "kHz")
            multiplier = 1000.0;
        else if (suffix != "Hz")
            return std::nullopt;
        break;
    case Unit::QualityFactor:
        if (!suffix.empty() || position != raw.size()) return std::nullopt;
        break;
    case Unit::Percent:
        if (suffix != "%") return std::nullopt;
        break;
    case Unit::StereoBalance:
        if ((suffix != "L" && suffix != "R") || token.front() == '+' || token.front() == '-')
            return std::nullopt;
        if (suffix == "L") multiplier = -1.0;
        break;
    }
    if (token.front() == '+') token.remove_prefix(1);
    double amount;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), amount);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return std::nullopt;
    const auto increment =
        std::pow(10.0, -static_cast<double>(fractional_digits)) * std::abs(multiplier);
    const auto physical = amount * multiplier;
    if (!std::isfinite(physical) || !std::isfinite(increment) || increment <= 0.0 ||
        ((unit == Unit::Hertz || unit == Unit::QualityFactor) && physical <= 0.0) ||
        (unit == Unit::StereoBalance && amount <= 0.0))
        return std::nullopt;
    return DisplayReading{physical, increment, false};
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
    const auto& domain = value.at("descriptor");
    const double minimum = domain.at("minimum"), maximum = domain.at("maximum");
    const double internal = value.at("internal_value"), target = intent.at("target");
    const double physical = value.at("display_value"), error = value.at("absolute_display_error");
    const double tolerance = intent.at("tolerance");
    if (internal < minimum || internal > maximum || error < 0.0 || error > tolerance ||
        !arithmetic_equal(error, std::abs(physical - target)) ||
        (tolerance == 0.0 && (error != 0.0 || physical != target)))
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
    const bool balance = unit == Unit::StereoBalance;
    const bool gain_infinity = entry->id == "utility.gain";
    if (balance != !value.at("balance_full_scale").is_null()) return malformed();
    const double full_scale = balance ? value.at("balance_full_scale").get<double>() : 1.0;
    const auto selected =
        display_reading(value.at("display").get<std::string>(), unit, gain_infinity);
    if (!selected || selected->negative_infinity ||
        !arithmetic_equal(selected->value / full_scale, physical) ||
        selected->increment.has_value() != !value.at("display_increment").is_null() ||
        (selected->increment &&
         !arithmetic_equal(*selected->increment / full_scale, value.at("display_increment"))) ||
        (unit == Unit::Percent && physical < 0.0))
        return malformed();
    const auto& samples = value.at("samples");
    const std::size_t extra_calls = entry->device_class_name == "Eq8" ? 1 : 0;
    if (!samples.is_array() || samples.size() < 20 ||
        samples.size() + extra_calls != value.at("formatter_calls").get<std::uint32_t>())
        return malformed();
    std::map<double, std::pair<std::string, std::optional<double>>> ordered;
    std::vector<NativeDisplaySample> observations;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        if (!fields(sample,
                    {"internal_value", "display", "phase", "display_value", "negative_infinity"}) ||
            !finite(sample.at("internal_value")) || !text(sample.at("display"), 80) ||
            !sample.at("negative_infinity").is_boolean() ||
            sample.at("phase") != (i < 17                    ? "grid"
                                   : i >= samples.size() - 3 ? "repeat"
                                                             : "search"))
            return malformed();
        const double position = sample.at("internal_value");
        const bool infinity = sample.at("negative_infinity");
        const auto reading =
            display_reading(sample.at("display").get<std::string>(), unit, gain_infinity);
        if (position < minimum || position > maximum || !reading ||
            reading->negative_infinity != infinity ||
            (infinity ? !sample.at("display_value").is_null()
                      : !finite(sample.at("display_value"))) ||
            (!infinity &&
             !arithmetic_equal(reading->value / full_scale, sample.at("display_value"))) ||
            (!infinity && unit == Unit::Percent && reading->value < 0.0))
            return malformed();
        std::optional<double> amount;
        if (!infinity) amount = sample.at("display_value").get<double>();
        const auto record = std::pair{sample.at("display").get<std::string>(), amount};
        if (const auto previous = ordered.find(position);
            previous != ordered.end() && previous->second != record)
            return malformed();
        ordered[position] = record;
        observations.push_back({position, record.first, sample.at("phase"), amount, infinity});
        if (i < 17) {
            const double fraction = static_cast<double>(i) / 16.0;
            const double expected = i == 0    ? minimum
                                    : i == 16 ? maximum
                                              : minimum * (1.0 - fraction) + maximum * fraction;
            if (!arithmetic_equal(position, expected)) return malformed();
        }
    }
    std::optional<double> previous;
    bool finite_seen = false;
    for (const auto& [position, reading] : ordered) {
        (void)position;
        if (!reading.second) {
            if (finite_seen) return malformed();
        } else {
            if (previous && *previous > *reading.second) return malformed();
            previous = reading.second;
            finite_seen = true;
        }
    }
    const auto& low = observations.front();
    const auto& high = observations[16];
    if (low.internal_value != minimum || high.internal_value != maximum || !high.display_value ||
        (low.display_value && *low.display_value >= *high.display_value) ||
        (low.display_value && target < *low.display_value) || target > *high.display_value ||
        (balance &&
         (!low.display_value || *low.display_value != -1.0 || *high.display_value != 1.0)))
        return malformed();
    auto same_sample = [](const auto& a, const auto& b) {
        return a.internal_value == b.internal_value && a.display == b.display &&
               a.display_value == b.display_value && a.negative_infinity == b.negative_infinity;
    };
    const auto count = observations.size();
    const auto& repeated = observations.back();
    if (!same_sample(low, observations[count - 3]) || !same_sample(high, observations[count - 2]) ||
        repeated.internal_value != internal ||
        repeated.display != value.at("display").get_ref<const std::string&>() ||
        repeated.display_value != std::optional{physical} || repeated.negative_infinity ||
        std::none_of(observations.begin(), observations.end() - 3, [&](const auto& sample) {
            return same_sample(sample, repeated);
        }))
        return malformed();
    NativeDisplayCandidate result;
    result.device_path = request.path.to_string();
    result.capability_id = entry->id;
    result.device_class_name = entry->device_class_name;
    result.parameter_original_name = entry->parameter_original_name;
    result.parameter_index = static_cast<std::uint32_t>(*found);
    result.unit = unit;
    result.target = target;
    result.tolerance = tolerance;
    result.internal_value = internal;
    result.display = value.at("display");
    result.display_value = physical;
    if (!value.at("display_increment").is_null())
        result.display_increment = value.at("display_increment");
    result.absolute_display_error = error;
    if (balance) result.balance_full_scale = full_scale;
    result.descriptor = {minimum,
                         maximum,
                         domain.at("value"),
                         domain.at("default_value"),
                         domain.at("is_enabled"),
                         domain.at("state"),
                         domain.at("automation_state")};
    result.formatter_calls = value.at("formatter_calls");
    result.samples = std::move(observations);
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
