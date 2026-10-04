#pragma once
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_envelope_revision.hpp>

namespace sunny::infrastructure::managed_envelope_detail {
using managed_detail::recovery_fields;
using managed_detail::recovery_hex;
using managed_detail::recovery_key;
using nlohmann::json;
inline constexpr auto preview_method = "sunny_managed_preview_envelope_replacement";
inline constexpr auto replace_method = "sunny_managed_replace_envelope";
inline constexpr std::size_t step_call_budget = SUNNY_MANAGED_ENVELOPE_REPLACEMENT_MAX_STEPS;
inline json scope() {
    return {{"replacement_scope", "selected_parameter_envelope"},
            {"breakpoint_population_observed", false},
            {"unsampled_state_preservation_proven", false},
            {"other_parameter_envelopes_written", false},
            {"historical_envelope_identity_proven", false},
            {"same_parameter_modulation_preservation_proven", false},
            {"host_qualified", false}};
}
struct Step {
    double start;
    double duration;
    double value;
    double interior;
};
inline std::optional<std::vector<Step>> steps(const json& lane) {
    if (!recovery_fields(lane, {"parameter", "interpolation", "clip_end", "points"}) ||
        lane.at("parameter") != json{{"kind", "panning"}} || lane.at("interpolation") != "step" ||
        !lane.at("clip_end").is_number() || !std::isfinite(lane.at("clip_end").get<double>()) ||
        lane.at("clip_end").get<double>() <= 0.0 || !lane.at("points").is_array() ||
        lane.at("points").empty() || lane.at("points").size() > step_call_budget)
        return std::nullopt;
    for (const auto& point : lane.at("points"))
        if (!recovery_fields(point, {"time", "value"}) || !point.at("time").is_number() ||
            !point.at("value").is_number())
            return std::nullopt;
    std::vector<Step> result;
    double previous = -1.0;
    for (std::size_t i = 0; i < lane.at("points").size(); ++i) {
        const auto& point = lane.at("points")[i];
        if (!recovery_fields(point, {"time", "value"}) || !point.at("time").is_number() ||
            !point.at("value").is_number())
            return std::nullopt;
        const auto start = point.at("time").get<double>();
        const auto value = point.at("value").get<double>();
        if (!std::isfinite(start) || !std::isfinite(value) || start <= previous ||
            (i == 0 && start != 0.0) || value < -1.0 || value > 1.0)
            return std::nullopt;
        const auto& endpoint = i + 1 < lane.at("points").size()
                                   ? lane.at("points")[i + 1].value("time", json(nullptr))
                                   : lane.at("clip_end");
        if (!endpoint.is_number()) return std::nullopt;
        const auto end = endpoint.get<double>();
        const auto duration = end - start;
        const auto interior = start + duration / 2.0;
        if (!std::isfinite(end) || !std::isfinite(duration) || !std::isfinite(interior) ||
            duration <= 0.0 || !(start < interior && interior < end) || start + duration != end)
            return std::nullopt;
        result.push_back({start, duration, value, interior});
        previous = start;
    }
    return result;
}
inline bool request_valid(std::string_view name, const json& request) {
    if (!request.is_object()) return false;
    for (const auto* key : {"document_token", "project_key", "binding_key"})
        if (!request.contains(key) || !recovery_key(request.at(key))) return false;
    if (name == preview_method)
        return recovery_fields(request,
                               {"document_token",
                                "project_key",
                                "binding_key",
                                "expected_content_fingerprint",
                                "lane"}) &&
               recovery_hex(request.at("expected_content_fingerprint"), 64) &&
               steps(request.at("lane"));
    return name == replace_method &&
           recovery_fields(request,
                           {"document_token",
                            "operation_id",
                            "project_key",
                            "binding_key",
                            "preview_token",
                            "preview_fingerprint",
                            "explicit_selected_envelope_replacement",
                            "allow_unsampled_selected_state_overwrite"}) &&
           recovery_key(request.at("operation_id")) &&
           recovery_hex(request.at("preview_token"), 32) &&
           recovery_hex(request.at("preview_fingerprint"), 64) &&
           request.at("explicit_selected_envelope_replacement") == true &&
           request.at("allow_unsampled_selected_state_overwrite") == true;
}
inline bool observation_valid(const json& metadata, const json& observation) {
    if (!observation.is_object() || !observation.contains("track_tag") ||
        !observation.contains("clip_tag"))
        return false;
    auto selection = metadata;
    selection["selector"] =
        json{{"track_tag", observation.at("track_tag")}, {"clip_tag", observation.at("clip_tag")}};
    if (!managed_detail::recovery_observation(selection, observation) ||
        !observation.contains("note_identity"))
        return false;
    const auto& manifest = observation.at("manifest");
    const auto& track = manifest.at("track");
    const auto& clip = manifest.at("clip");
    if (manifest.at("entire_clip_population_observed") != true || clip.at("looping") != false ||
        clip.at("is_midi_clip") != true || clip.at("is_session_clip") != true ||
        clip.at("is_arrangement_clip") != false || clip.at("is_audio_clip") != false ||
        clip.at("start_marker").get<double>() != 0.0 ||
        clip.at("end_marker").get<double>() != metadata.at("lane").at("clip_end").get<double>() ||
        manifest.at("mixer").at("panning_mode") != 0)
        return false;
    for (const auto* state : {"arm", "implicit_arm", "is_frozen", "is_grouped"})
        if (track.at(state) != false) return false;
    for (const auto* state :
         {"is_playing", "is_recording", "is_overdubbing", "is_triggered", "will_record_on_start"})
        if (clip.at(state) != false) return false;
    const auto& pan = manifest.at("mixer").at("panning");
    for (const auto* name : {"value", "min", "max"})
        if (!pan.at(name).is_number_float()) return false;
    return pan.at("original_name") == "Track Panning" && pan.at("min").get<double>() == -1.0 &&
           pan.at("max").get<double>() == 1.0 && pan.at("is_quantized") == false &&
           pan.at("is_enabled") == true && pan.at("state") == 0 && pan.at("automation_state") != 2;
}
inline bool metadata_valid(const json& metadata) {
    return recovery_fields(metadata,
                           {"schema_version",
                            "context",
                            "project_key",
                            "binding_key",
                            "preview_token",
                            "lane",
                            "selected_envelope",
                            "scope",
                            "native_step_call_budget"}) &&
           managed_detail::note_integer(metadata.at("schema_version"), 1, 1) &&
           managed_detail::recovery_context(metadata.at("context")) &&
           recovery_key(metadata.at("project_key")) && recovery_key(metadata.at("binding_key")) &&
           recovery_hex(metadata.at("preview_token"), 32) && steps(metadata.at("lane")) &&
           metadata.at("scope") == scope() &&
           managed_detail::note_integer(
               metadata.at("native_step_call_budget"), step_call_budget, step_call_budget);
}
inline bool selected_valid(const json& metadata, const json& observation) {
    const auto& selected = metadata.at("selected_envelope");
    if (!recovery_fields(selected, {"has_envelope", "parameter", "samples"}) ||
        !selected.at("has_envelope").is_boolean() || !selected.at("samples").is_array())
        return false;
    const auto& domain = selected.at("parameter");
    const auto& actual = observation.at("manifest").at("mixer").at("panning");
    if (!recovery_fields(domain,
                         {"matched_name",
                          "original_name",
                          "minimum",
                          "maximum",
                          "unit",
                          "state",
                          "automation_state"}) ||
        domain.at("matched_name") != actual.at("name") ||
        domain.at("original_name") != actual.at("original_name") ||
        !domain.at("minimum").is_number_float() || !domain.at("maximum").is_number_float() ||
        domain.at("minimum").get<double>() != -1.0 || domain.at("maximum").get<double>() != 1.0 ||
        domain.at("unit") != "internal" ||
        !managed_detail::note_integer(domain.at("state"), 0, 0) ||
        !managed_detail::note_integer(domain.at("automation_state"), 0, 1) ||
        domain.at("automation_state") != actual.at("automation_state"))
        return false;
    const auto admitted = steps(metadata.at("lane"));
    const auto& samples = selected.at("samples");
    if (samples.size() != (selected.at("has_envelope") == true ? admitted->size() : 0U) ||
        (selected.at("has_envelope") == true &&
         observation.at("manifest").at("clip").at("has_envelopes") != true))
        return false;
    for (std::size_t i = 0; i < samples.size(); ++i)
        if (!recovery_fields(samples[i], {"time", "value"}) ||
            !samples[i].at("time").is_number_float() || !samples[i].at("value").is_number_float() ||
            samples[i].at("time").get<double>() != admitted->at(i).interior ||
            !std::isfinite(samples[i].at("value").get<double>()) ||
            samples[i].at("value").get<double>() < -1.0 ||
            samples[i].at("value").get<double>() > 1.0)
            return false;
    return true;
}
inline bool preview_valid(const json& preview) {
    if (!recovery_fields(preview,
                         {"schema_version",
                          "context",
                          "project_key",
                          "binding_key",
                          "preview_token",
                          "lane",
                          "observation",
                          "selected_envelope",
                          "scope",
                          "native_step_call_budget",
                          "preview_fingerprint"}) ||
        !recovery_hex(preview.at("preview_fingerprint"), 64))
        return false;
    auto metadata = preview;
    metadata.erase("observation");
    metadata.erase("preview_fingerprint");
    if (!metadata_valid(metadata) || !observation_valid(metadata, preview.at("observation")) ||
        !selected_valid(metadata, preview.at("observation")))
        return false;
    auto body = preview;
    body.erase("preview_fingerprint");
    return managed_detail::managed_digest(body) ==
           std::optional<std::string>{preview.at("preview_fingerprint").get<std::string>()};
}
inline json calls(const json& metadata) {
    auto result = json::array();
    if (metadata.at("selected_envelope").at("has_envelope") == true)
        result.push_back("clear_envelope");
    result.push_back("create_automation_envelope");
    for (std::size_t i = 0; i < metadata.at("lane").at("points").size(); ++i)
        result.push_back("insert_step");
    return result;
}
inline json flags(const json& before, const json& after) {
    auto manifest = after.at("manifest");
    manifest["clip"]["has_envelopes"] = before.at("manifest").at("clip").at("has_envelopes");
    return {{"note_ids_and_values_preserved",
             managed_detail::canonical_managed_bytes(before.at("note_identity")) ==
                 managed_detail::canonical_managed_bytes(after.at("note_identity"))},
            {"other_finite_properties_preserved",
             managed_detail::canonical_managed_bytes(manifest) ==
                 managed_detail::canonical_managed_bytes(before.at("manifest"))},
            {"device_identity_preserved",
             before.value("device_identity_fingerprint", json(nullptr)) ==
                 after.value("device_identity_fingerprint", json(nullptr))}};
}
inline bool result_matches_request(const json& request, const json& result) {
    if (!result.contains("envelope_replacement") || !result.contains("note_identity")) return false;
    const auto& update = result.at("envelope_replacement");
    if (!recovery_fields(update,
                         {"before_observation",
                          "preview_metadata",
                          "preview_fingerprint",
                          "actual_samples",
                          "observed_step_samples_match_request",
                          "note_ids_and_values_preserved",
                          "other_finite_properties_preserved",
                          "device_identity_preserved"}))
        return false;
    auto preview = update.at("preview_metadata");
    if (!preview.is_object()) return false;
    preview["observation"] = update.at("before_observation");
    preview["preview_fingerprint"] = update.at("preview_fingerprint");
    if (!preview_valid(preview) || request.at("preview_token") != preview.at("preview_token") ||
        request.at("preview_fingerprint") != preview.at("preview_fingerprint") ||
        request.at("document_token") != preview.at("context").at("document_token") ||
        request.at("project_key") != preview.at("project_key") ||
        request.at("binding_key") != preview.at("binding_key"))
        return false;
    const auto& samples = update.at("actual_samples");
    if (!samples.is_array()) return false;
    const auto admitted = steps(preview.at("lane"));
    if (samples.size() != admitted->size()) return false;
    bool matched = true;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (i >= admitted->size() || !recovery_fields(samples[i], {"time", "value"}) ||
            !samples[i].at("time").is_number_float() || !samples[i].at("value").is_number_float() ||
            !std::isfinite(samples[i].at("value").get<double>()) ||
            samples[i].at("time").get<double>() != admitted->at(i).interior ||
            samples[i].at("value").get<double>() < -1.0 ||
            samples[i].at("value").get<double>() > 1.0)
            return false;
        matched = matched && samples[i].at("value").get<double>() == admitted->at(i).value;
    }
    if (!update.at("observed_step_samples_match_request").is_boolean() ||
        update.at("observed_step_samples_match_request").get<bool>() != matched)
        return false;
    const auto comparisons = flags(update.at("before_observation"), result);
    for (const auto& [key, value] : comparisons.items())
        if (update.at(key) != value) return false;
    return true;
}
inline bool progress_valid(const json& result, const json& progress) {
    const auto expected = calls(result.at("envelope_replacement").at("preview_metadata"));
    return recovery_fields(progress, {"started_calls", "returned_calls"}) &&
           progress.at("started_calls") == expected && progress.at("returned_calls") == expected;
}
inline bool response_fits(const json& request, const json& preview) {
    if (!managed_detail::operation_reservation_fits(request, replace_method)) return false;
    const auto& before = preview.at("observation");
    auto after = before;
    after["manifest"]["notes"] = json::array();
    after["note_identity"]["notes"] = json::array();
    auto metadata = preview;
    metadata.erase("observation");
    metadata.erase("preview_fingerprint");
    auto samples = json::array();
    const auto admitted = steps(preview.at("lane"));
    for (const auto& step : *admitted)
        samples.push_back(json{{"time", step.interior}, {"value", 0.0}});
    auto result = after;
    result["envelope_replacement"] =
        json{{"before_observation", before},
             {"preview_metadata", metadata},
             {"preview_fingerprint", preview.at("preview_fingerprint")},
             {"actual_samples", samples},
             {"observed_step_samples_match_request", false},
             {"note_ids_and_values_preserved", false},
             {"other_finite_properties_preserved", false},
             {"device_identity_preserved", false}};
    auto phase_calls = calls(metadata);
    if (metadata.at("selected_envelope").at("has_envelope") != true)
        phase_calls.insert(phase_calls.begin(), "clear_envelope");
    json progress{{"started_calls", phase_calls}, {"returned_calls", phase_calls}};
    json journal{{"document_token", request.at("document_token")},
                 {"operation_id", request.at("operation_id")},
                 {"name", replace_method},
                 {"request", request},
                 {"request_fingerprint", std::string(64, 'f')},
                 {"outcome", "acknowledged"},
                 {"native_mutation_started", true},
                 {"progress", progress},
                 {"result", result}};
    const auto count = before.at("note_identity").at("notes").size();
    const auto populations = managed_detail::note_array_bound(count, false) +
                             managed_detail::note_array_bound(count, true) - 4;
    const auto fits = [populations](const json& value) {
        const auto metadata_bytes = managed_detail::json_wire_bound(value);
        return metadata_bytes && *metadata_bytes <= managed_detail::managed_response_limit &&
               populations <= managed_detail::managed_response_limit - *metadata_bytes;
    };
    if (!fits(json{{"success", true}, {"value", journal}})) return false;
    journal.erase("result");
    journal["outcome"] = "indeterminate";
    journal["partial_binding_retained"] = false;
    journal["progress"]["last_observation"] = after;
    std::string error;
    for (std::size_t i = 0; i < 1024; ++i)
        error += "\xF4\x8F\xBF\xBF";
    journal["error"] = error;
    return fits(json{{"success", true}, {"value", journal}});
}
} // namespace sunny::infrastructure::managed_envelope_detail
