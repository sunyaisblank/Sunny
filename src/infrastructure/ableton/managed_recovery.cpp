#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
using namespace managed_detail;

json context_json(const ManagedBridgeContext& context) {
    return {{"bridge_instance", context.bridge_instance},
            {"document_token", context.document_token}};
}

json preview_metadata(json value) {
    value.erase("preview_fingerprint");
    value.erase("observation");
    return value;
}
bool adoption_response_fits(const json& request, const json& preview) {
    constexpr std::string_view method = "sunny_managed_adopt_clip";
    if (!operation_reservation_fits(request, method)) return false;
    auto actual = preview.at("observation");
    std::set<std::int32_t> ids;
    for (const auto& note : actual.at("note_identity").at("notes"))
        ids.insert(note.at("note_id").get<std::int32_t>());
    actual["adoption"] = {{"preview_token", request.at("preview_token")},
                          {"preview_fingerprint", request.at("preview_fingerprint")},
                          {"authority_origin", "explicit_adoption"},
                          {"historical_identity_proven", false},
                          {"allowed_domains", recovery_domains()},
                          {"preserved_unknown_domains", recovery_preserved_domains()},
                          {"approved_note_ids", ids},
                          {"devices_preserved", true},
                          {"preview_metadata", preview_metadata(preview)}};
    const json journal{{"document_token", request.at("document_token")},
                       {"operation_id", request.at("operation_id")},
                       {"request_fingerprint", std::string(64, 'f')},
                       {"request", request},
                       {"name", method},
                       {"outcome", "acknowledged"},
                       {"native_mutation_started", false},
                       {"result", actual}};
    const auto size = json_wire_bound(json{{"success", true}, {"value", journal}});
    return size && *size <= managed_response_limit;
}
} // namespace

namespace managed_detail {
bool adoption_preview_request_valid(const json& value) {
    return recovery_fields(value, {"document_token", "project_key", "binding_key", "selector"}) &&
           recovery_key(value.at("document_token")) && recovery_key(value.at("project_key")) &&
           recovery_key(value.at("binding_key")) && recovery_selector(value.at("selector"));
}

bool adoption_request_valid(const json& value) {
    return recovery_fields(value,
                           {"document_token",
                            "operation_id",
                            "project_key",
                            "binding_key",
                            "preview_token",
                            "preview_fingerprint",
                            "explicit_adoption"}) &&
           recovery_key(value.at("document_token")) && recovery_key(value.at("operation_id")) &&
           recovery_key(value.at("project_key")) && recovery_key(value.at("binding_key")) &&
           recovery_hex(value.at("preview_token"), 32) &&
           recovery_hex(value.at("preview_fingerprint"), 64) &&
           value.at("explicit_adoption") == true;
}

bool adoption_acknowledgement_valid(const json& intent, const json& result) {
    if (!adoption_request_valid(intent) || !result.is_object() || !result.contains("adoption"))
        return false;
    const auto& adoption = result.at("adoption");
    if (!recovery_fields(adoption,
                         {"preview_token",
                          "preview_fingerprint",
                          "authority_origin",
                          "historical_identity_proven",
                          "allowed_domains",
                          "preserved_unknown_domains",
                          "approved_note_ids",
                          "devices_preserved",
                          "preview_metadata"}) ||
        adoption.at("preview_token") != intent.at("preview_token") ||
        adoption.at("preview_fingerprint") != intent.at("preview_fingerprint") ||
        adoption.at("authority_origin") != "explicit_adoption" ||
        adoption.at("historical_identity_proven") != false ||
        adoption.at("allowed_domains") != recovery_domains() ||
        adoption.at("preserved_unknown_domains") != recovery_preserved_domains() ||
        adoption.at("devices_preserved") != true)
        return false;
    const auto& metadata = adoption.at("preview_metadata");
    if (!recovery_metadata(metadata) ||
        metadata.at("preview_token") != intent.at("preview_token") ||
        metadata.at("context").at("document_token") != intent.at("document_token") ||
        metadata.at("project_key") != intent.at("project_key") ||
        metadata.at("binding_key") != intent.at("binding_key"))
        return false;
    auto observation = result;
    observation.erase("adoption");
    if (!recovery_observation(metadata, observation)) return false;
    auto approved = metadata;
    approved["observation"] = observation;
    const auto digest = managed_digest(approved);
    if (!digest || *digest != intent.at("preview_fingerprint").get_ref<const std::string&>())
        return false;
    std::set<std::int32_t> observed_ids;
    for (const auto& note : observation.at("note_identity").at("notes"))
        observed_ids.insert(note.at("note_id").get<std::int32_t>());
    const auto& ids = adoption.at("approved_note_ids");
    if (!ids.is_array() || ids.size() != observed_ids.size()) return false;
    auto expected = observed_ids.begin();
    for (const auto& id : ids) {
        if (!note_integer(id, INT32_MIN, INT32_MAX) || id != *expected++) return false;
    }
    return true;
}
} // namespace managed_detail

Result<ManagedAdoptionPreview> managed_adoption_preview_from_json(const json& value) {
    if (!value.is_object() || !value.contains("observation") ||
        !value.contains("preview_fingerprint") ||
        !recovery_hex(value.at("preview_fingerprint"), 64))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto metadata = preview_metadata(value);
    if (!recovery_metadata(metadata) || !recovery_observation(metadata, value.at("observation")))
        return std::unexpected(ErrorCode::ProtocolError);
    auto approved = value;
    approved.erase("preview_fingerprint");
    const auto digest = managed_digest(approved);
    if (!digest || *digest != value.at("preview_fingerprint").get_ref<const std::string&>())
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& context = value.at("context");
    return ManagedAdoptionPreview{{context.at("bridge_instance").get<std::string>(),
                                   context.at("document_token").get<std::string>()},
                                  value.at("project_key").get<std::string>(),
                                  value.at("binding_key").get<std::string>(),
                                  value.at("preview_token").get<std::string>(),
                                  value.at("preview_fingerprint").get<std::string>(),
                                  value};
}

Result<LomRequest> make_managed_adoption_preview_request(const ManagedBridgeContext& context,
                                                         const std::string& project,
                                                         const std::string& binding,
                                                         const json& selector) {
    const json value{{"document_token", context.document_token},
                     {"project_key", project},
                     {"binding_key", binding},
                     {"selector", selector}};
    if (!recovery_context(context_json(context)) || !adoption_preview_request_valid(value))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(LomPaths::song(), "sunny_managed_preview_adoption", {value});
}

Result<ManagedAdoptionPreview> preview_managed_adoption(const ManagedBridgeContext& context,
                                                        const std::string& project,
                                                        const std::string& binding,
                                                        const json& selector,
                                                        LomTransport& transport) {
    const auto request = make_managed_adoption_preview_request(context, project, binding, selector);
    if (!request) return std::unexpected(request.error());
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto parsed = managed_adoption_preview_from_json(std::get<json>(*response.value));
    if (!parsed || parsed->context.bridge_instance != context.bridge_instance ||
        parsed->context.document_token != context.document_token ||
        parsed->project_key != project || parsed->binding_key != binding ||
        parsed->evidence.at("selector") != selector)
        return std::unexpected(ErrorCode::ProtocolError);
    return parsed;
}

Result<LomRequest> make_managed_adoption_request(const ManagedBridgeContext& context,
                                                 const std::string& operation,
                                                 const ManagedAdoptionPreview& preview) {
    const auto checked = managed_adoption_preview_from_json(preview.evidence);
    if (!checked || checked->context.bridge_instance != context.bridge_instance ||
        checked->context.document_token != context.document_token ||
        preview.context.bridge_instance != checked->context.bridge_instance ||
        preview.context.document_token != checked->context.document_token ||
        preview.project_key != checked->project_key ||
        preview.binding_key != checked->binding_key ||
        preview.preview_token != checked->preview_token ||
        preview.preview_fingerprint != checked->preview_fingerprint)
        return std::unexpected(ErrorCode::ProtocolError);
    const json value{{"document_token", context.document_token},
                     {"operation_id", operation},
                     {"project_key", checked->project_key},
                     {"binding_key", checked->binding_key},
                     {"preview_token", checked->preview_token},
                     {"preview_fingerprint", checked->preview_fingerprint},
                     {"explicit_adoption", true}};
    if (!adoption_request_valid(value)) return std::unexpected(ErrorCode::ProtocolError);
    if (!adoption_response_fits(value, checked->evidence))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return LomProtocol::call_method(LomPaths::song(), "sunny_managed_adopt_clip", {value});
}
} // namespace sunny::infrastructure
