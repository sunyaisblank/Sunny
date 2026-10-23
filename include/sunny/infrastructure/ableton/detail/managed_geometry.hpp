#pragma once

#include <array>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>

namespace sunny::infrastructure::managed_detail {
inline constexpr std::array geometry_properties{
    "end_marker", "signature_numerator", "signature_denominator"};

inline bool clip_geometry_valid(const nlohmann::json& geometry) {
    if (!geometry.is_object() || geometry.size() != 3) return false;
    for (const auto* property : geometry_properties)
        if (!geometry.contains(property)) return false;
    const auto& end = geometry.at("end_marker");
    if (!end.is_number() || !std::isfinite(end.get<double>()) || end.get<double>() <= 0.0 ||
        !note_integer(geometry.at("signature_numerator"), 1, 99))
        return false;
    const auto& denominator = geometry.at("signature_denominator");
    if (!note_integer(denominator, 1, 16)) return false;
    const auto value = denominator.get<int>();
    return value == 1 || value == 2 || value == 4 || value == 8 || value == 16;
}

inline bool clip_geometry_note_admission(const nlohmann::json& identity,
                                         const nlohmann::json& geometry) {
    if (!note_identity_valid(identity) || !clip_geometry_valid(geometry) ||
        identity.at("entire_clip_population_observed") != true)
        return false;
    const auto end = geometry.at("end_marker").get<double>();
    for (const auto& note : identity.at("notes")) {
        const auto start = note.at("start_time").get<double>();
        const auto duration = note.at("duration").get<double>();
        if (start < 0.0 || !std::isfinite(start + duration) || start + duration > end) return false;
    }
    return true;
}

inline nlohmann::json changed_geometry_properties(const nlohmann::json& before_clip,
                                                  const nlohmann::json& geometry) {
    auto result = nlohmann::json::array();
    for (const auto* property : geometry_properties)
        if (before_clip.at(property).get<double>() != geometry.at(property).get<double>())
            result.push_back(property);
    return result;
}

/// Recompute actual finite-domain comparisons; false evidence remains valid.
/// The loop relationship records the observed native behavior, not an ABI claim.
inline nlohmann::json clip_geometry_flags(const nlohmann::json& before_manifest,
                                          const nlohmann::json& before_identity,
                                          const nlohmann::json& before_device_fingerprint,
                                          const nlohmann::json& after,
                                          const nlohmann::json& geometry,
                                          bool end_submitted) {
    using nlohmann::json;
    const auto& actual = after.at("manifest");
    const auto& old_loop = before_manifest.at("clip").at("loop_end");
    const auto& new_loop = actual.at("clip").at("loop_end");
    const char* relationship = "unexpected_change";
    if (canonical_managed_bytes(old_loop) == canonical_managed_bytes(new_loop))
        relationship = "unchanged_inactive_boundary";
    else if (end_submitted && new_loop.get<double>() == geometry.at("end_marker").get<double>())
        relationship = "followed_end_marker";
    auto protected_manifest = actual;
    bool matched = true;
    for (const auto* property : geometry_properties) {
        matched = matched && actual.at("clip").at(property).get<double>() ==
                                 geometry.at(property).get<double>();
        protected_manifest["clip"][property] = before_manifest.at("clip").at(property);
    }
    if (std::string_view{relationship} != "unexpected_change")
        protected_manifest["clip"]["loop_end"] = old_loop;
    protected_manifest["notes"] = before_manifest.at("notes");
    const auto before_notes = note_map(before_identity);
    const auto after_notes = note_map(after.at("note_identity"));
    bool ids = before_notes.size() == after_notes.size();
    for (const auto& [id, note] : before_notes) {
        (void)note;
        ids = ids && after_notes.contains(id);
    }
    const auto after_device_fingerprint = after.value("device_identity_fingerprint", json(nullptr));
    return {
        {"observed_geometry_matches_request", matched},
        {"note_values_preserved",
         canonical_managed_bytes(before_identity) ==
             canonical_managed_bytes(after.at("note_identity"))},
        {"note_ids_preserved", ids},
        {"note_cardinality_preserved", before_notes.size() == after_notes.size()},
        {"other_finite_properties_preserved",
         canonical_managed_bytes(protected_manifest) == canonical_managed_bytes(before_manifest)},
        {"device_identity_preserved", after_device_fingerprint == before_device_fingerprint},
        {"loop_end_relationship", relationship}};
}

inline bool clip_geometry_response_fits(const nlohmann::json& request,
                                        const nlohmann::json& before) {
    using nlohmann::json;
    constexpr auto method = "sunny_managed_update_clip_geometry";
    if (!operation_reservation_fits(request, method)) return false;
    auto after = before;
    for (const auto* key : {"note_update",
                            "note_population_update",
                            "clip_geometry_update",
                            "acknowledgement",
                            "device_update",
                            "adoption"})
        after.erase(key);
    after["manifest"]["notes"] = json::array();
    after["note_identity"]["notes"] = json::array();
    after["observed_notes_match_request"] = false;
    after["observed_clip_properties_match_request"] = false;
    const auto properties =
        json::array({"end_marker", "signature_numerator", "signature_denominator"});
    json supplement{{"before_manifest", before.at("manifest")},
                    {"before_note_identity", before.at("note_identity")},
                    {"before_note_identity_fingerprint", before.at("note_identity_fingerprint")},
                    {"before_device_identity_fingerprint",
                     before.value("device_identity_fingerprint", json(nullptr))},
                    {"requested_geometry", request.at("geometry")},
                    {"submitted_properties", properties},
                    {"returned_properties", properties},
                    {"loop_end_relationship", "unchanged_inactive_boundary"}};
    for (const auto* flag : {"observed_geometry_matches_request",
                             "note_values_preserved",
                             "note_ids_preserved",
                             "note_cardinality_preserved",
                             "other_finite_properties_preserved",
                             "device_identity_preserved"})
        supplement[flag] = false;
    json journal{
        {"document_token", request.at("document_token")},
        {"operation_id", request.at("operation_id")},
        {"request_fingerprint", std::string(64, 'f')},
        {"request", request},
        {"name", method},
        {"outcome", "acknowledged"},
        {"native_mutation_started", true},
        {"progress", {{"started_properties", properties}, {"returned_properties", properties}}}};
    auto result = after;
    result["clip_geometry_update"] = supplement;
    journal["result"] = result;
    const auto count = before.at("note_identity").at("notes").size();
    const auto populations = note_array_bound(count, false) + note_array_bound(count, true) - 4;
    const auto fits = [populations](const json& response) {
        const auto metadata = json_wire_bound(response);
        return metadata && *metadata <= managed_response_limit &&
               populations <= managed_response_limit - *metadata;
    };
    if (!fits(json{{"success", true}, {"value", journal}})) return false;
    journal.erase("result");
    journal["outcome"] = "indeterminate";
    journal["partial_binding_retained"] = false;
    journal["progress"]["last_observation"] = after;
    std::string maximum_error;
    for (std::size_t i = 0; i < 1024; ++i)
        maximum_error += "\xF4\x8F\xBF\xBF";
    journal["error"] = maximum_error;
    return fits(json{{"success", true}, {"value", journal}});
}
} // namespace sunny::infrastructure::managed_detail
