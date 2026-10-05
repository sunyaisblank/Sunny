/** One reusable proof for the bounded native formatter search; adapters own
 * identities. */
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <sunny/infrastructure/ableton/detail/native_unit_decimal.hpp>
#include <sunny/infrastructure/ableton/native_units.hpp>

namespace sunny::infrastructure::native_unit_detail {
namespace proof_detail {
using nlohmann::json;
inline bool fields(const json& value, std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&value](auto name) { return value.contains(name); });
}

inline bool finite(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

inline bool integer(const json& value, std::uint64_t upper) {
    return value.is_number_integer() &&
           (value.is_number_unsigned()
                ? value.get<std::uint64_t>() <= upper
                : value.get<std::int64_t>() >= 0 &&
                      static_cast<std::uint64_t>(value.get<std::int64_t>()) <= upper);
}

inline bool text(const json& value, std::size_t maximum = 4096) {
    return value.is_string() && !value.get_ref<const std::string&>().empty() &&
           value.get_ref<const std::string&>().size() <= maximum;
}

inline bool nullable_positive(const json& value) {
    return value.is_null() || (finite(value) && value.get<double>() > 0.0);
}

inline bool descriptor(const json& value, bool quantized, bool eligible) {
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

inline bool arithmetic_equal(double first, double second) {
    if (!std::isfinite(first) || !std::isfinite(second)) return false;
    if (first == second) return true;
    const double scale = std::max(std::abs(first), std::abs(second));
    return std::abs(first - second) <=
           std::max(std::numeric_limits<double>::denorm_min(),
                    scale * (8.0 * std::numeric_limits<double>::epsilon()));
}

} // namespace proof_detail
using proof_detail::arithmetic_equal;
using proof_detail::descriptor;

struct NumericalDisplayProof {
    double target = 0.0, tolerance = 0.0, internal_value = 0.0, display_value = 0.0;
    double absolute_display_error = 0.0;
    std::string display;
    std::optional<double> display_increment, balance_full_scale;
    NativeDisplayParameterDescriptor descriptor;
    std::uint32_t formatter_calls = 0;
    std::vector<NativeDisplaySample> samples;
};

inline std::optional<NumericalDisplayProof> numerical_display_proof(const nlohmann::json& value,
                                                                    Unit unit,
                                                                    double expected_target,
                                                                    double expected_tolerance,
                                                                    bool gain_infinity,
                                                                    std::size_t extra_calls = 0) {
    using namespace proof_detail;
    try {
        if (!std::isfinite(expected_target) || !std::isfinite(expected_tolerance) ||
            expected_tolerance < 0 || !descriptor(value.at("descriptor"), false, true) ||
            !integer(value.at("formatter_calls"), SUNNY_NATIVE_DISPLAY_FORMATTER_BUDGET) ||
            !text(value.at("display"), 80) || !finite(value.at("internal_value")) ||
            !finite(value.at("display_value")) ||
            !nullable_positive(value.at("display_increment")) ||
            !nullable_positive(value.at("balance_full_scale")) ||
            !finite(value.at("absolute_display_error")) || !finite(value.at("target")) ||
            !finite(value.at("display_tolerance")) ||
            value.at("target").get<double>() != expected_target ||
            value.at("display_tolerance").get<double>() != expected_tolerance)
            return std::nullopt;
        const auto& domain = value.at("descriptor");
        const double minimum = domain.at("minimum"), maximum = domain.at("maximum");
        const double internal = value.at("internal_value");
        const double physical = value.at("display_value"),
                     error = value.at("absolute_display_error");
        const double target = expected_target, tolerance = expected_tolerance;
        if (internal < minimum || internal > maximum || error < 0.0) return std::nullopt;
        const bool balance = unit == Unit::StereoBalance;
        if (balance != !value.at("balance_full_scale").is_null()) return std::nullopt;
        const double full_scale = balance ? value.at("balance_full_scale").get<double>() : 1.0;
        const auto selected =
            display_reading(value.at("display").get<std::string>(), unit, gain_infinity);
        const auto& samples = value.at("samples");
        std::optional<DisplayReading> scale_reading;
        if (balance && samples.is_array() && samples.size() >= 17 && samples[16].is_object() &&
            samples[16].contains("display") && samples[16].at("display").is_string())
            scale_reading =
                display_reading(samples[16].at("display").get<std::string>(), unit, false);
        const auto decimal =
            selected ? decimal_display_comparison(*selected,
                                                  target,
                                                  tolerance,
                                                  full_scale,
                                                  110,
                                                  scale_reading ? &*scale_reading : nullptr)
                     : std::nullopt;
        if (!selected || selected->negative_infinity ||
            (balance && (!scale_reading || !arithmetic_equal(scale_reading->value, full_scale))) ||
            !decimal || !decimal->within_tolerance ||
            !arithmetic_equal(decimal->display_value, physical) ||
            !arithmetic_equal(decimal->absolute_display_error, error) ||
            selected->increment.has_value() != !value.at("display_increment").is_null() ||
            (selected->increment &&
             !arithmetic_equal(*decimal->display_increment, value.at("display_increment"))) ||
            ((unit == Unit::Percent || unit == Unit::Milliseconds) && physical < 0.0) ||
            (unit == Unit::Milliseconds && target < 0.0))
            return std::nullopt;
        if (!samples.is_array() || samples.size() < 20 ||
            samples.size() + extra_calls != value.at("formatter_calls").get<std::uint32_t>())
            return std::nullopt;
        std::map<double, std::pair<std::string, std::optional<double>>> ordered;
        std::vector<NativeDisplaySample> observations;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto& sample = samples[i];
            if (!fields(
                    sample,
                    {"internal_value", "display", "phase", "display_value", "negative_infinity"}) ||
                !finite(sample.at("internal_value")) || !text(sample.at("display"), 80) ||
                !sample.at("negative_infinity").is_boolean() ||
                sample.at("phase") != (i < 17                    ? "grid"
                                       : i >= samples.size() - 3 ? "repeat"
                                                                 : "search"))
                return std::nullopt;
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
                (!infinity && (unit == Unit::Percent || unit == Unit::Milliseconds) &&
                 reading->value < 0.0))
                return std::nullopt;
            std::optional<double> amount;
            if (!infinity) amount = sample.at("display_value").get<double>();
            const auto record = std::pair{sample.at("display").get<std::string>(), amount};
            if (const auto previous = ordered.find(position);
                previous != ordered.end() && previous->second != record)
                return std::nullopt;
            ordered[position] = record;
            observations.push_back({position, record.first, sample.at("phase"), amount, infinity});
            if (i < 17) {
                const double fraction = static_cast<double>(i) / 16.0;
                const double expected = i == 0    ? minimum
                                        : i == 16 ? maximum
                                                  : minimum * (1.0 - fraction) + maximum * fraction;
                if (!arithmetic_equal(position, expected)) return std::nullopt;
            }
        }
        std::optional<double> previous;
        bool finite_seen = false;
        for (const auto& [position, reading] : ordered) {
            (void)position;
            if (!reading.second) {
                if (finite_seen) return std::nullopt;
            } else {
                if (previous && *previous > *reading.second) return std::nullopt;
                previous = reading.second;
                finite_seen = true;
            }
        }
        const auto& low = observations.front();
        const auto& high = observations[16];
        if (low.internal_value != minimum || high.internal_value != maximum ||
            !high.display_value ||
            (low.display_value && *low.display_value >= *high.display_value) ||
            (low.display_value && target < *low.display_value) || target > *high.display_value ||
            (balance &&
             (!low.display_value || *low.display_value != -1.0 || *high.display_value != 1.0)))
            return std::nullopt;
        auto same_sample = [](const auto& a, const auto& b) {
            return a.internal_value == b.internal_value && a.display == b.display &&
                   a.display_value == b.display_value && a.negative_infinity == b.negative_infinity;
        };
        const auto count = observations.size();
        const auto& repeated = observations.back();
        if (!same_sample(low, observations[count - 3]) ||
            !same_sample(high, observations[count - 2]) || repeated.internal_value != internal ||
            repeated.display != value.at("display").get_ref<const std::string&>() ||
            repeated.display_value != std::optional{physical} || repeated.negative_infinity ||
            std::none_of(observations.begin(), observations.end() - 3, [&](const auto& sample) {
                return same_sample(sample, repeated);
            }))
            return std::nullopt;

        NumericalDisplayProof result;
        result.target = target;
        result.tolerance = tolerance;
        result.internal_value = internal;
        result.display = value.at("display");
        result.display_value = physical;
        result.absolute_display_error = error;
        if (!value.at("display_increment").is_null())
            result.display_increment = value.at("display_increment");
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
        return result;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}
} // namespace sunny::infrastructure::native_unit_detail
