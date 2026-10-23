#include <algorithm>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/legacy_authority.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace {
bool exact(const json& v, std::initializer_list<const char*> keys) {
    return v.is_object() && v.size() == keys.size() &&
           std::ranges::all_of(keys, [&](const char* k) { return v.contains(k); });
}
bool hex(const json& v, std::size_t n) {
    if (!v.is_string()) return false;
    const auto& s = v.get_ref<const std::string&>();
    return s.size() == n && std::ranges::all_of(s, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
bool integer(const json& v, unsigned minimum = 0, unsigned maximum = 2147483647) {
    return v.is_number_integer() && v.get<double>() >= minimum && v.get<double>() <= maximum;
}
bool bounded_text(const json& v, std::size_t maximum = 4096) {
    return v.is_string() && v.get_ref<const std::string&>().size() <= maximum;
}
bool epoch(const json& v) {
    return v.is_object() && v.contains("schema_version") && integer(v.at("schema_version"), 1, 1) &&
           hex(v.value("bridge_instance", json{}), 32) &&
           hex(v.value("document_token", json{}), 32);
}
bool scope(const json& v) {
    return exact(v, {"schema_version", "bridge_instance", "document_token", "scope_id"}) &&
           epoch(v) && hex(v.at("scope_id"), 32);
}
bool correlations(const json& v) {
    return epoch(v) && hex(v.value("scope_id", json{}), 32) &&
           hex(v.value("workflow_id", json{}), 32) && hex(v.value("operation_id", json{}), 32) &&
           integer(v.value("ordinal", json{}), 1, 4096) &&
           integer(v.value("graph_revision", json{}));
}
const char* name(LegacyOperationOutcome o) {
    switch (o) {
    case LegacyOperationOutcome::Prepared:
        return "prepared";
    case LegacyOperationOutcome::NativePrepared:
        return "native_prepared";
    case LegacyOperationOutcome::NotSent:
        return "not_sent";
    case LegacyOperationOutcome::Acknowledged:
        return "acknowledged";
    case LegacyOperationOutcome::Declined:
        return "declined";
    case LegacyOperationOutcome::Partial:
        return "partial";
    case LegacyOperationOutcome::Indeterminate:
        return "indeterminate";
    case LegacyOperationOutcome::UnknownEpoch:
        return "unknown_epoch";
    case LegacyOperationOutcome::UnknownOperation:
        return "unknown_operation";
    }
    return "invalid";
}
bool diagnostic(const json& v) {
    if (v.is_null()) return true;
    if (!exact(v, {"phase", "target_binding", "graph_revision", "created"}) ||
        !bounded_text(v.at("phase"), 128) ||
        (!v.at("target_binding").is_null() && !hex(v.at("target_binding"), 32)) ||
        !integer(v.at("graph_revision")) || !v.at("created").is_array() ||
        v.at("created").size() > 256 || v.dump().size() > 16384)
        return false;
    const auto& phase = v.at("phase").get_ref<const std::string&>();
    if (!std::ranges::all_of(phase, [](unsigned char c) { return c >= 32 && c <= 126; }))
        return false;
    for (const auto& child : v.at("created")) {
        if (!exact(child, {"binding_token", "kind", "path"}) ||
            !hex(child.at("binding_token"), 32) || !bounded_text(child.at("path"), 1024) ||
            !child.at("kind").is_string())
            return false;
        const auto kind = child.at("kind").get<std::string>();
        if (kind != "track" && kind != "return_track" && kind != "scene" && kind != "clip_slot" &&
            kind != "clip" && kind != "mixer_parameter" && kind != "device" &&
            kind != "device_parameter" && kind != "cue")
            return false;
        const auto path = child.at("path").get<std::string>();
        if (!LomPath::parse(path).is_canonical() || LomPath::parse(path).to_string() != path)
            return false;
    }
    return true;
}
bool journal_valid(const json& v, const LegacyOperationReceipt& receipt) {
    if (!exact(v,
               {"schema_version",
                "bridge_instance",
                "document_token",
                "scope_id",
                "workflow_id",
                "operation_id",
                "ordinal",
                "graph_revision",
                "fingerprint",
                "outcome",
                "native_mutation_started",
                "started_calls",
                "returned_calls",
                "result",
                "diagnostic",
                "error"}))
        return false;
    const auto token = legacy_token(receipt);
    for (const auto& [key, value] : token.items())
        if (v.at(key) != value || v.at(key).type() != value.type()) {
            // Signed/unsigned are the same JSON integer domain; other type substitutions are not.
            if (!(integer(v.at(key)) && integer(value) && v.at(key) == value)) return false;
        }
    if (!v.at("native_mutation_started").is_boolean() || !integer(v.at("started_calls"), 0, 4096) ||
        !integer(v.at("returned_calls"), 0, 4096) || !diagnostic(v.at("diagnostic")))
        return false;
    const auto started = v.at("started_calls").get<unsigned>(),
               returned = v.at("returned_calls").get<unsigned>();
    const auto& command = receipt.intent.at("command");
    const auto method = command.at("name").get<std::string>();
    unsigned maximum = method == "sunny_set_cue" ? 4U
                       : method == "sunny_author_step_envelope"
                           ? static_cast<unsigned>(command.at("args").at(0).at("points").size() + 1)
                           : 1U;
    if (started > maximum || returned > started || v.at("native_mutation_started") != (started > 0))
        return false;
    const auto& outcome = v.at("outcome");
    if (outcome == "prepared" || outcome == "declined" || outcome == "unknown_epoch" ||
        outcome == "unknown_operation") {
        if (started || !v.at("result").is_null() || !v.at("diagnostic").is_null()) return false;
        return outcome == "declined" ? bounded_text(v.at("error")) : v.at("error").is_null();
    }
    if (outcome == "partial")
        return started && v.at("result").is_null() && bounded_text(v.at("error"));
    if (outcome != "acknowledged" || !started || returned != started || !v.at("error").is_null())
        return false;
    const auto& result_value = v.at("result").at("value");
    bool structural = method == "create_midi_track" || method == "create_return_track" ||
                      method == "create_scene" || method == "create_clip" ||
                      method == "delete_clip" || method == "insert_device";
    unsigned required = 1;
    if (method == "sunny_set_cue") {
        if (!result_value.is_object() || !result_value.contains("action") ||
            (result_value.at("action") != "created" && result_value.at("action") != "updated"))
            return false;
        structural = result_value.at("action") == "created";
        required = structural ? 4U : 1U;
    } else if (method == "sunny_author_step_envelope") {
        if (!result_value.is_object() || !result_value.contains("action") ||
            !result_value.contains("steps_inserted") ||
            !integer(result_value.at("steps_inserted")) ||
            (result_value.at("action") != "created" && result_value.at("action") != "updated") ||
            result_value.at("steps_inserted") != command.at("args").at(0).at("points").size())
            return false;
        required = static_cast<unsigned>(command.at("args").at(0).at("points").size()) +
                   (result_value.at("action") == "created" ? 1U : 0U);
    }
    if (started != required) return false;
    const auto& r = v.at("result");
    return exact(r, {"value", "target_binding", "graph_revision"}) &&
           hex(r.at("target_binding"), 32) && integer(r.at("graph_revision")) &&
           r.at("graph_revision").get<unsigned>() ==
               receipt.intent.at("graph_revision").get<unsigned>() + (structural ? 1U : 0U);
}
Result<LegacyOperationReceipt>
observe(LegacyOperationReceipt r, const LomResponse& response, LegacyOperationStage stage) {
    r.stage = stage;
    r.delivery = response.delivery;
    r.journal.reset();
    r.error = response.error;
    if (r.error && r.error->size() > 4096) r.error->resize(4096);
    r.outcome = response.delivery == LomDeliveryState::NotSent
                    ? LegacyOperationOutcome::NotSent
                    : LegacyOperationOutcome::Indeterminate;
    if (!response.success || !response.value) return r;
    const auto* j = std::get_if<json>(&*response.value);
    bool valid = false;
    try {
        valid = j && journal_valid(*j, r);
    } catch (...) {
        valid = false;
    }
    if (response.delivery != LomDeliveryState::ResponseReceived || !valid) {
        // Complete but malformed post-dispatch evidence is uncertainty, never a safe retry.
        r.outcome = LegacyOperationOutcome::Indeterminate;
        r.error = "Invalid original-token legacy journal";
        return r;
    }
    r.journal = *j;
    const auto& o = j->at("outcome");
    r.outcome = o == "prepared"        ? LegacyOperationOutcome::NativePrepared
                : o == "acknowledged"  ? LegacyOperationOutcome::Acknowledged
                : o == "declined"      ? LegacyOperationOutcome::Declined
                : o == "partial"       ? LegacyOperationOutcome::Partial
                : o == "unknown_epoch" ? LegacyOperationOutcome::UnknownEpoch
                                       : LegacyOperationOutcome::UnknownOperation;
    r.error =
        j->at("error").is_null() ? std::nullopt : std::optional(j->at("error").get<std::string>());
    return r;
}
} // namespace
json legacy_command(const LomRequest& r) {
    auto v = json::parse(LomProtocol::serialize_request(r));
    v.erase("bridge_protocol_version");
    if (!v.contains("args")) v["args"] = json::array();
    return v;
}
Result<LomRequest> legacy_command_from_json(const json& v) {
    try {
        if (!exact(v, {"type", "path", "name", "args"}) || !v.at("args").is_array() ||
            !v.at("name").is_string())
            return std::unexpected(ErrorCode::ProtocolError);
        const auto method = v.at("name").get<std::string>();
        if (method.starts_with("sunny_legacy_") || method.starts_with("sunny_managed_") ||
            method.starts_with("sunny_ordinary_"))
            return std::unexpected(ErrorCode::ProtocolError);
        auto wire = v;
        wire["bridge_protocol_version"] = static_cast<std::uint64_t>(SUNNY_BRIDGE_PROTOCOL_VERSION);
        if (wire.at("args").empty()) wire.erase("args");
        return LomProtocol::deserialize_request(wire);
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
bool legacy_request_valid(const std::string& method, const json& v) noexcept {
    try {
        if (method == "sunny_legacy_scope")
            return exact(v, {"schema_version", "bridge_instance", "document_token", "scope_id"}) &&
                   epoch(v) && (v.at("scope_id").is_null() || hex(v.at("scope_id"), 32));
        if (method == "sunny_legacy_finish") return scope(v);
        if (method == "sunny_legacy_read") {
            if (!exact(v,
                       {"schema_version",
                        "bridge_instance",
                        "document_token",
                        "scope_id",
                        "graph_revision",
                        "command"}) ||
                !epoch(v) || !hex(v.at("scope_id"), 32) || !integer(v.at("graph_revision")))
                return false;
            auto command = legacy_command_from_json(v.at("command"));
            return command && LomProtocol::is_read_only_request(*command);
        }
        if (!correlations(v)) return false;
        if (method == "sunny_legacy_prepare") {
            if (!exact(v,
                       {"schema_version",
                        "bridge_instance",
                        "document_token",
                        "scope_id",
                        "workflow_id",
                        "operation_id",
                        "ordinal",
                        "graph_revision",
                        "command"}))
                return false;
            const auto command = legacy_command_from_json(v.at("command"));
            return command && !LomProtocol::is_read_only_request(*command) &&
                   managed_detail::managed_digest(v).has_value();
        }
        return (method == "sunny_legacy_execute" || method == "sunny_legacy_operation") &&
               exact(v,
                     {"schema_version",
                      "bridge_instance",
                      "document_token",
                      "scope_id",
                      "workflow_id",
                      "operation_id",
                      "ordinal",
                      "graph_revision",
                      "fingerprint"}) &&
               hex(v.at("fingerprint"), 64);
    } catch (...) {
        return false;
    }
}
json legacy_authority_to_json(const LegacyPlanningAuthority& a) {
    return {{"schema_version", 1},
            {"scope", a.scope},
            {"graph_revision", a.graph_revision},
            {"workspace_namespace", a.workspace_namespace}};
}
Result<LegacyPlanningAuthority> legacy_authority_from_json(const json& v) {
    try {
        if (!exact(v, {"schema_version", "scope", "graph_revision", "workspace_namespace"}) ||
            !integer(v.at("schema_version"), 1, 1) || !scope(v.at("scope")) ||
            !integer(v.at("graph_revision")) || !hex(v.at("workspace_namespace"), 32))
            return std::unexpected(ErrorCode::ProtocolError);
        return LegacyPlanningAuthority{v.at("scope"),
                                       v.at("graph_revision").get<std::uint32_t>(),
                                       v.at("workspace_namespace").get<std::string>()};
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
json legacy_recipe_to_json(const LegacyWorkflowRecipe& r) {
    return {{"schema_version", 1},
            {"kind", r.kind},
            {"intent_fingerprint", r.intent_fingerprint},
            {"authority", legacy_authority_to_json(r.authority)},
            {"commands", r.commands}};
}
Result<LegacyWorkflowRecipe> legacy_recipe_from_json(const json& v) {
    try {
        if (!exact(v, {"schema_version", "kind", "intent_fingerprint", "authority", "commands"}) ||
            !integer(v.at("schema_version"), 1, 1) || !hex(v.at("intent_fingerprint"), 64) ||
            !v.at("commands").is_array() || v.at("commands").size() > 4096)
            return std::unexpected(ErrorCode::ProtocolError);
        const auto kind = v.at("kind").get<std::string>();
        if (kind != "project_deployment" && kind != "bridge_messages" && kind != "single_request")
            return std::unexpected(ErrorCode::ProtocolError);
        auto a = legacy_authority_from_json(v.at("authority"));
        if (!a || a->graph_revision != 0) return std::unexpected(ErrorCode::ProtocolError);
        for (const auto& entry : v.at("commands")) {
            if (!exact(entry, {"command", "phase"}) || !integer(entry.at("phase"), 0, 2))
                return std::unexpected(ErrorCode::ProtocolError);
            auto c = legacy_command_from_json(entry.at("command"));
            if (!c || LomProtocol::is_read_only_request(*c))
                return std::unexpected(ErrorCode::ProtocolError);
        }
        return LegacyWorkflowRecipe{
            kind, v.at("intent_fingerprint").get<std::string>(), *a, v.at("commands")};
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
json legacy_token(const LegacyOperationReceipt& r) {
    auto v = r.intent;
    v.erase("command");
    v["fingerprint"] = *managed_detail::managed_digest(r.intent);
    return v;
}
Result<LegacyOperationReceipt> prepare_legacy_operation(const LegacyPlanningAuthority& a,
                                                        const std::string& w,
                                                        const std::string& o,
                                                        std::uint32_t ordinal,
                                                        const LomRequest& command) {
    if (!legacy_authority_from_json(legacy_authority_to_json(a)))
        return std::unexpected(ErrorCode::ProtocolError);
    auto intent = a.scope;
    intent["workflow_id"] = w;
    intent["operation_id"] = o;
    intent["ordinal"] = ordinal;
    intent["graph_revision"] = a.graph_revision;
    intent["command"] = legacy_command(command);
    if (!legacy_request_valid("sunny_legacy_prepare", intent))
        return std::unexpected(ErrorCode::ProtocolError);
    LegacyOperationReceipt result;
    result.intent = std::move(intent);
    return result;
}
Result<LegacyOperationReceipt> execute_legacy_operation(const LegacyOperationReceipt& r,
                                                        LomTransport& t) {
    if (r.outcome != LegacyOperationOutcome::Prepared || r.delivery != LomDeliveryState::NotSent ||
        r.journal || r.error || !legacy_receipt_from_json(legacy_receipt_to_json(r)))
        return std::unexpected(ErrorCode::ProtocolError);
    auto prepared = observe(
        r,
        t.send(LomProtocol::call_method(LomPaths::song(), "sunny_legacy_prepare", {r.intent})),
        LegacyOperationStage::Prepare);
    if (!prepared || prepared->outcome != LegacyOperationOutcome::NativePrepared) return prepared;
    return observe(r,
                   t.send(LomProtocol::call_method(
                       LomPaths::song(), "sunny_legacy_execute", {legacy_token(r)})),
                   LegacyOperationStage::Execute);
}
Result<LegacyOperationReceipt> reconcile_legacy_operation(const LegacyOperationReceipt& r,
                                                          LomTransport& t) {
    if (!legacy_receipt_from_json(legacy_receipt_to_json(r)))
        return std::unexpected(ErrorCode::ProtocolError);
    return observe(r,
                   t.send(LomProtocol::call_method(
                       LomPaths::song(), "sunny_legacy_operation", {legacy_token(r)})),
                   LegacyOperationStage::Query);
}
json legacy_receipt_to_json(const LegacyOperationReceipt& r) {
    return {{"schema_version", 1},
            {"intent", r.intent},
            {"delivery", static_cast<int>(r.delivery)},
            {"stage", static_cast<int>(r.stage)},
            {"outcome", name(r.outcome)},
            {"journal", r.journal ? *r.journal : json{}},
            {"error", r.error ? json(*r.error) : json{}}};
}
Result<LegacyOperationReceipt> legacy_receipt_from_json(const json& v) {
    try {
        if (!exact(
                v,
                {"schema_version", "intent", "delivery", "stage", "outcome", "journal", "error"}) ||
            !integer(v.at("schema_version"), 1, 1) ||
            !legacy_request_valid("sunny_legacy_prepare", v.at("intent")) ||
            !integer(v.at("delivery"), 0, 2) || !integer(v.at("stage"), 0, 3) ||
            (!v.at("error").is_null() && !bounded_text(v.at("error"))))
            return std::unexpected(ErrorCode::ProtocolError);
        LegacyOperationReceipt r;
        r.intent = v.at("intent");
        r.delivery = static_cast<LomDeliveryState>(v.at("delivery").get<int>());
        r.stage = static_cast<LegacyOperationStage>(v.at("stage").get<int>());
        bool found = false;
        for (auto o : {LegacyOperationOutcome::Prepared,
                       LegacyOperationOutcome::NativePrepared,
                       LegacyOperationOutcome::NotSent,
                       LegacyOperationOutcome::Acknowledged,
                       LegacyOperationOutcome::Declined,
                       LegacyOperationOutcome::Partial,
                       LegacyOperationOutcome::Indeterminate,
                       LegacyOperationOutcome::UnknownEpoch,
                       LegacyOperationOutcome::UnknownOperation})
            if (v.at("outcome") == name(o)) {
                r.outcome = o;
                found = true;
            }
        if (!found) return std::unexpected(ErrorCode::ProtocolError);
        if (!v.at("error").is_null()) r.error = v.at("error").get<std::string>();
        const bool native = r.outcome == LegacyOperationOutcome::NativePrepared ||
                            r.outcome == LegacyOperationOutcome::Acknowledged ||
                            r.outcome == LegacyOperationOutcome::Declined ||
                            r.outcome == LegacyOperationOutcome::Partial ||
                            r.outcome == LegacyOperationOutcome::UnknownEpoch ||
                            r.outcome == LegacyOperationOutcome::UnknownOperation;
        if (!v.at("journal").is_null()) {
            if (r.delivery != LomDeliveryState::ResponseReceived ||
                !journal_valid(v.at("journal"), r))
                return std::unexpected(ErrorCode::ProtocolError);
            r.journal = v.at("journal");
            const auto observed = observe(
                r,
                LomResponse{
                    true, LomValue(*r.journal), std::nullopt, LomDeliveryState::ResponseReceived},
                r.stage);
            if (!observed || observed->outcome != r.outcome || observed->error != r.error)
                return std::unexpected(ErrorCode::ProtocolError);
        } else if (native)
            return std::unexpected(ErrorCode::ProtocolError);
        if ((r.outcome == LegacyOperationOutcome::Prepared ||
             r.outcome == LegacyOperationOutcome::NotSent) &&
            r.delivery != LomDeliveryState::NotSent)
            return std::unexpected(ErrorCode::ProtocolError);
        if (r.outcome == LegacyOperationOutcome::Prepared &&
            (r.error || r.journal || r.stage != LegacyOperationStage::Unsent))
            return std::unexpected(ErrorCode::ProtocolError);
        if (r.outcome == LegacyOperationOutcome::Indeterminate &&
            r.delivery == LomDeliveryState::NotSent)
            return std::unexpected(ErrorCode::ProtocolError);
        if (r.outcome != LegacyOperationOutcome::Prepared &&
            r.stage == LegacyOperationStage::Unsent)
            return std::unexpected(ErrorCode::ProtocolError);
        return r;
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
} // namespace sunny::infrastructure
