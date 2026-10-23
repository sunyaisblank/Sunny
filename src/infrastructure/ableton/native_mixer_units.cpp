#include <array>
#include <set>
#include <sunny/infrastructure/ableton/detail/native_unit_numeric.hpp>
#include <sunny/infrastructure/ableton/native_mixer_units.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using namespace native_unit_detail::proof_detail;
bool actual_descriptor(const json& value, bool eligible) {
    if (!value.is_object() || !value.contains("is_quantized") ||
        !value.at("is_quantized").is_boolean())
        return false;
    const bool quantized = value.at("is_quantized");
    if (!descriptor(value, quantized, eligible)) return false;
    for (const auto* field : {"minimum", "maximum", "value"})
        if (!value.at(field).is_number_float()) return false;
    return quantized || value.at("default_value").is_number_float();
}
bool capture_domain(const json& value, bool eligible_volume) {
    if (!fields(value, {"panning_mode", "crossfade_assign", "parameters"}) ||
        !integer(value.at("panning_mode"), 1) || !integer(value.at("crossfade_assign"), 2) ||
        !value.at("parameters").is_array() || value.at("parameters").size() < 3 ||
        value.at("parameters").size() > 512)
        return false;
    const auto& parameters = value.at("parameters");
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto& member = parameters[i];
        if (!fields(member, {"kind", "send_index", "name", "original_name", "descriptor"}) ||
            !text(member.at("name")) || !text(member.at("original_name")) ||
            !actual_descriptor(member.at("descriptor"), i == 0 && eligible_volume))
            return false;
        if (i < 3) {
            constexpr std::array roles{"volume", "panning", "track_activator"};
            if (member.at("kind") != roles[i] || !member.at("send_index").is_null()) return false;
        } else if (member.at("kind") != "send" || !integer(member.at("send_index"), 509) ||
                   member.at("send_index") != i - 3)
            return false;
    }
    return parameters[0].at("descriptor").at("is_quantized") == false;
}
bool mixer_capture(const json& value, const json& candidate) {
    if (!capture_domain(value, true)) return false;
    const auto& first = value.at("parameters")[0];
    return first.at("name") == candidate.at("parameter_name") &&
           first.at("original_name") == candidate.at("parameter_original_name") &&
           first.at("descriptor") == candidate.at("descriptor");
}
} // namespace

bool native_mixer_detail::capture_valid(const nlohmann::json& value) {
    try {
        return capture_domain(value, false);
    } catch (const json::exception&) {
        return false;
    }
}

sunny::core::Result<NativeMixerDisplayCandidate> parse_native_mixer_display_candidate(
    std::string_view parameter_kind, double target, double tolerance, const nlohmann::json& value) {
    const auto fail = []() -> sunny::core::Result<NativeMixerDisplayCandidate> {
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    };
    try {
        if (parameter_kind != "volume" ||
            !fields(value,
                    {"schema_version",
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
                     "formatter_calls",
                     "samples",
                     "current_display",
                     "parameter_kind",
                     "parameter_name",
                     "parameter_original_name",
                     "mixer_capture",
                     "track_context",
                     "device_cohort_count",
                     "qualification",
                     "source_commit",
                     "host_qualified",
                     "native_knob_only",
                     "coverage_limits"}) ||
            !integer(value.at("schema_version"), 1) || value.at("schema_version") != 1 ||
            value.at("parameter_kind") != "volume" || value.at("unit") != "Decibels" ||
            !text(value.at("parameter_name")) || !text(value.at("parameter_original_name")) ||
            value.at("qualification") != "ObservedNativeMixerDisplayCandidate" ||
            value.at("source_commit") !=
                sunny::core::live_native_registry_provenance().python_source_commit ||
            value.at("host_qualified") != false || value.at("native_knob_only") != true ||
            !integer(value.at("device_cohort_count"), 16) ||
            !value.at("balance_full_scale").is_null() ||
            !actual_descriptor(value.at("descriptor"), true) ||
            !mixer_capture(value.at("mixer_capture"), value))
            return fail();
        const auto& context = value.at("track_context");
        if (!fields(context,
                    {"mute",
                     "solo",
                     "arm",
                     "implicit_arm",
                     "is_frozen",
                     "is_grouped",
                     "back_to_arranger",
                     "has_audio_input",
                     "has_midi_input",
                     "has_audio_output",
                     "has_midi_output"}) ||
            !std::ranges::all_of(context, [](const auto& flag) { return flag.is_boolean(); }))
            return fail();
        for (const auto* name : {"target",
                                 "display_tolerance",
                                 "internal_value",
                                 "display_value",
                                 "absolute_display_error"})
            if (!value.at(name).is_number_float()) return fail();
        if (!value.at("display_increment").is_null() &&
            !value.at("display_increment").is_number_float())
            return fail();
        if (!value.at("samples").is_array()) return fail();
        for (const auto& sample : value.at("samples"))
            if (!sample.is_object() || !sample.contains("internal_value") ||
                !sample.at("internal_value").is_number_float() ||
                (sample.contains("display_value") && !sample.at("display_value").is_null() &&
                 !sample.at("display_value").is_number_float()))
                return fail();
        const auto numeric = native_unit_detail::numerical_display_proof(
            value, sunny::core::LiveNativePhysicalUnit::Decibels, target, tolerance, true, 1);
        if (!numeric) return fail();
        const auto& current = value.at("current_display");
        if (!fields(current,
                    {"internal_value",
                     "display",
                     "display_value",
                     "display_increment",
                     "negative_infinity"}) ||
            !current.at("internal_value").is_number_float() ||
            current.at("internal_value") != value.at("descriptor").at("value") ||
            !text(current.at("display"), 80) || !current.at("negative_infinity").is_boolean())
            return fail();
        const auto reading =
            native_unit_detail::display_reading(current.at("display").get<std::string>(),
                                                sunny::core::LiveNativePhysicalUnit::Decibels,
                                                true);
        if (!reading || current.at("negative_infinity") != reading->negative_infinity ||
            (reading->negative_infinity
                 ? (!current.at("display_value").is_null() ||
                    !current.at("display_increment").is_null())
                 : (!current.at("display_value").is_number_float() ||
                    !arithmetic_equal(reading->value, current.at("display_value")) ||
                    !current.at("display_increment").is_number_float() || !reading->increment ||
                    !arithmetic_equal(*reading->increment, current.at("display_increment")))))
            return fail();
        for (const auto& sample : numeric->samples)
            if (sample.internal_value == current.at("internal_value").get<double>() &&
                (sample.display != current.at("display").get_ref<const std::string&>() ||
                 sample.negative_infinity != reading->negative_infinity))
                return fail();
        const auto& limits = value.at("coverage_limits");
        if (!limits.is_array() || limits.empty() || limits.size() > 16) return fail();
        std::set<std::string> unique;
        for (const auto& item : limits)
            if (!text(item) || !unique.insert(item.get<std::string>()).second) return fail();
        NativeMixerDisplayCandidate result;
        result.parameter_kind = "volume";
        result.parameter_name = value.at("parameter_name");
        result.parameter_original_name = value.at("parameter_original_name");
        result.target = numeric->target;
        result.tolerance = numeric->tolerance;
        result.internal_value = numeric->internal_value;
        result.display_value = numeric->display_value;
        result.display = numeric->display;
        result.display_increment = numeric->display_increment;
        result.absolute_display_error = numeric->absolute_display_error;
        result.descriptor = numeric->descriptor;
        result.formatter_calls = numeric->formatter_calls;
        result.samples = numeric->samples;
        result.current_display = current;
        result.mixer_capture = value.at("mixer_capture");
        result.track_context = context;
        result.device_cohort_count = value.at("device_cohort_count");
        result.coverage_limits = limits.get<std::vector<std::string>>();
        result.evidence = value;
        return result;
    } catch (const json::exception&) {
        return fail();
    }
}
} // namespace sunny::infrastructure
