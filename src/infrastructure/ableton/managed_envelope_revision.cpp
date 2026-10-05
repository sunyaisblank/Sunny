#include <sunny/infrastructure/ableton/detail/managed_envelope_revision.hpp>

namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
Result<ManagedEnvelopeReplacementPreview>
managed_envelope_replacement_preview_from_json(const json& value) {
    try {
        if (!managed_envelope_detail::preview_valid(value))
            return std::unexpected(ErrorCode::ProtocolError);
        return ManagedEnvelopeReplacementPreview{
            {value.at("context").at("bridge_instance").get<std::string>(),
             value.at("context").at("document_token").get<std::string>()},
            value.at("project_key").get<std::string>(),
            value.at("binding_key").get<std::string>(),
            value.at("preview_token").get<std::string>(),
            value.at("preview_fingerprint").get<std::string>(),
            value};
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<LomRequest> make_managed_envelope_replacement_preview_request(
    const ManagedBridgeContext& context, const ManagedBindingReceipt& binding, const json& lane) {
    if (context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !managed_binding_from_json(managed_binding_to_json(binding)) ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        managed_envelope_detail::preview_method,
        {json{{"document_token", context.document_token},
              {"project_key", binding.project_key},
              {"binding_key", binding.binding_key},
              {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
              {"lane", lane}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    return request;
}
Result<ManagedEnvelopeReplacementPreview>
preview_managed_envelope_replacement(const ManagedBridgeContext& context,
                                     const ManagedBindingReceipt& binding,
                                     const json& lane,
                                     LomTransport& transport) {
    auto request = make_managed_envelope_replacement_preview_request(context, binding, lane);
    if (!request) return std::unexpected(request.error());
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ProtocolError);
    auto preview = managed_envelope_replacement_preview_from_json(std::get<json>(*response.value));
    if (!preview || preview->context.bridge_instance != context.bridge_instance ||
        preview->context.document_token != context.document_token ||
        preview->project_key != binding.project_key ||
        preview->binding_key != binding.binding_key || preview->evidence.at("lane") != lane ||
        preview->evidence.at("observation").at("content_fingerprint") !=
            binding.observation.at("content_fingerprint") ||
        preview->evidence.at("observation").at("note_identity_fingerprint") !=
            binding.observation.at("note_identity_fingerprint"))
        return std::unexpected(ErrorCode::ProtocolError);
    return preview;
}
Result<LomRequest>
make_managed_envelope_replacement_request(const ManagedBridgeContext& context,
                                          const std::string& operation_id,
                                          const ManagedEnvelopeReplacementPreview& preview) {
    const auto checked = managed_envelope_replacement_preview_from_json(preview.evidence);
    if (!checked || checked->context.bridge_instance != context.bridge_instance ||
        checked->context.document_token != context.document_token ||
        preview.context.bridge_instance != checked->context.bridge_instance ||
        preview.context.document_token != checked->context.document_token ||
        preview.project_key != checked->project_key ||
        preview.binding_key != checked->binding_key ||
        preview.preview_token != checked->preview_token ||
        preview.preview_fingerprint != checked->preview_fingerprint)
        return std::unexpected(ErrorCode::ProtocolError);
    auto request =
        LomProtocol::call_method(LomPaths::song(),
                                 managed_envelope_detail::replace_method,
                                 {json{{"document_token", context.document_token},
                                       {"operation_id", operation_id},
                                       {"project_key", preview.project_key},
                                       {"binding_key", preview.binding_key},
                                       {"preview_token", preview.preview_token},
                                       {"preview_fingerprint", preview.preview_fingerprint},
                                       {"explicit_selected_envelope_replacement", true},
                                       {"allow_unsampled_selected_state_overwrite", true}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_envelope_detail::response_fits(std::get<json>(request.args[0]), preview.evidence))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return request;
}
} // namespace sunny::infrastructure
