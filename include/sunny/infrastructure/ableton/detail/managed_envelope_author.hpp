/** Known complete initial-lane ACK bound; native metadata remains observed. */
#pragma once
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>

namespace sunny::infrastructure::managed_detail {
inline bool envelope_author_response_fits(const nlohmann::json& request,
                                          const nlohmann::json& before) {
    using nlohmann::json;
    constexpr auto method = "sunny_managed_author_envelope";
    if (!operation_reservation_fits(request, method)) return false;
    const auto& selector = request.at("lane").at("parameter");
    const auto& mixer = before.at("manifest").at("mixer");
    const auto& parameter = selector.at("kind") == "send"
                                ? mixer.at("sends").at(selector.at("send_index").get<std::size_t>())
                                : mixer.at(selector.at("kind").get<std::string>());
    const json domain{{"matched_name", parameter.at("name")},
                      {"original_name", parameter.at("original_name")},
                      {"minimum", parameter.at("min")},
                      {"maximum", parameter.at("max")},
                      {"unit", "internal"},
                      {"state", parameter.at("state")},
                      {"automation_state", parameter.at("automation_state")}};
    json after;
    for (const auto* key : {"track_index",
                            "slot_index",
                            "manifest",
                            "content_fingerprint",
                            "note_identity",
                            "note_identity_fingerprint",
                            "track_tag",
                            "clip_tag",
                            "structural_boundary_complete",
                            "content_boundary_complete",
                            "unavailable_reasons",
                            "device_identity",
                            "device_identity_fingerprint"})
        if (before.contains(key)) after[key] = before.at(key);
    const auto count = after.at("note_identity").at("notes").size();
    after["manifest"]["notes"] = json::array();
    after["note_identity"]["notes"] = json::array();
    after["track_index"] = INT32_MAX;
    after["slot_index"] = INT32_MAX;
    after["manifest"]["clip"]["has_envelopes"] = true;
    after["structural_boundary_complete"] = false;
    after["observed_notes_match_request"] = false;
    after["observed_clip_properties_match_request"] = false;
    // Include the known newly-present lane's finite observation limitation.
    after["unavailable_reasons"].push_back(
        "Complete native envelope breakpoint population is unavailable");
    after["acknowledgement"] = {{"action", "created"},
                                {"steps_inserted", request.at("lane").at("points").size()},
                                {"parameter", domain}};
    const json journal{{"document_token", request.at("document_token")},
                       {"operation_id", request.at("operation_id")},
                       {"request_fingerprint", std::string(64, 'f')},
                       {"request", request},
                       {"name", method},
                       {"outcome", "acknowledged"},
                       {"native_mutation_started", true},
                       {"result", after}};
    const auto metadata = json_wire_bound(json{{"success", true}, {"value", journal}});
    const auto populations = note_array_bound(count, false) + note_array_bound(count, true) - 4;
    return metadata && *metadata <= managed_response_limit &&
           populations <= managed_response_limit - *metadata;
}
} // namespace sunny::infrastructure::managed_detail
