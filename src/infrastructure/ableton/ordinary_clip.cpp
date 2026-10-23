#include <algorithm>
#include <cmath>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/ordinary_clip.hpp>

namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace {
bool exact(const json& value, std::initializer_list<const char*> keys) {
    return value.is_object() && value.size() == keys.size() &&
           std::ranges::all_of(keys, [&](const char* key) { return value.contains(key); });
}
bool hex(const json& value, std::size_t size) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return text.size() == size && std::ranges::all_of(text, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
bool index(const json& value, bool positive = false) {
    return value.is_number_integer() && value.get<double>() >= (positive ? 1 : 0) &&
           value.get<double>() <= 2147483647;
}
bool finite(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}
bool note(const json& value, double clip_end) {
    if (!exact(value,
               {"pitch",
                "start_time",
                "duration",
                "velocity",
                "mute",
                "probability",
                "velocity_deviation",
                "release_velocity"}))
        return false;
    return index(value.at("pitch")) && value.at("pitch").get<int>() <= 127 &&
           value.at("velocity").is_number_integer() && value.at("velocity").get<double>() >= 1 &&
           value.at("velocity").get<double>() <= 127 && value.at("mute").is_boolean() &&
           finite(value.at("start_time")) && value.at("start_time").get<double>() >= 0 &&
           finite(value.at("duration")) && value.at("duration").get<double>() > 0 &&
           value.at("start_time").get<double>() + value.at("duration").get<double>() <= clip_end &&
           value.at("probability").is_number_float() && value.at("probability") == 1.0 &&
           value.at("velocity_deviation").is_number_float() &&
           value.at("velocity_deviation") == 0.0 &&
           value.at("release_velocity").is_number_float() && finite(value.at("release_velocity")) &&
           value.at("release_velocity").get<double>() >= 0 &&
           value.at("release_velocity").get<double>() <= 127 &&
           std::floor(value.at("release_velocity").get<double>()) ==
               value.at("release_velocity").get<double>();
}
const char* outcome_name(OrdinaryClipOutcome outcome) {
    switch (outcome) {
    case OrdinaryClipOutcome::Prepared:
        return "prepared";
    case OrdinaryClipOutcome::NativePrepared:
        return "native_prepared";
    case OrdinaryClipOutcome::NotSent:
        return "not_sent";
    case OrdinaryClipOutcome::Acknowledged:
        return "acknowledged";
    case OrdinaryClipOutcome::Declined:
        return "declined";
    case OrdinaryClipOutcome::Partial:
        return "partial";
    case OrdinaryClipOutcome::Indeterminate:
        return "indeterminate";
    case OrdinaryClipOutcome::UnknownOperation:
        return "unknown_operation";
    case OrdinaryClipOutcome::UnknownEpoch:
        return "unknown_epoch";
    }
    return "invalid";
}
json token(const OrdinaryClipReceipt& receipt) {
    json result = receipt.intent;
    result.erase("action");
    result.erase("payload");
    result["fingerprint"] = *managed_detail::managed_digest(receipt.intent);
    return result;
}
bool journal_valid(const json& value, const json& intent) {
    if (!exact(value,
               {"schema_version",
                "bridge_instance",
                "document_token",
                "operation_id",
                "fingerprint",
                "action",
                "outcome",
                "native_mutation_started",
                "started_calls",
                "returned_calls",
                "result",
                "error"}))
        return false;
    const auto digest = managed_detail::managed_digest(intent);
    if (!digest || value.at("fingerprint") != *digest ||
        !value.at("schema_version").is_number_integer())
        return false;
    for (const auto* key : {"schema_version", "bridge_instance", "document_token", "operation_id"})
        if (value.at(key) != intent.at(key)) return false;
    if (value.at("action") != intent.at("action") ||
        !value.at("native_mutation_started").is_boolean() || !index(value.at("started_calls")) ||
        !index(value.at("returned_calls")))
        return false;
    const auto started = value.at("started_calls").get<unsigned>();
    const auto returned = value.at("returned_calls").get<unsigned>();
    const auto action = intent.at("action").get<std::string>();
    const unsigned maximum_calls = action == "undo" ? 1 : 2;
    if (started > maximum_calls || returned > started ||
        value.at("native_mutation_started") != (started > 0))
        return false;
    const auto& outcome = value.at("outcome");
    if (outcome == "prepared" || outcome == "declined")
        return !started && value.at("result").is_null() &&
               (outcome == "prepared" ? value.at("error").is_null()
                                      : value.at("error").is_string());
    if (outcome == "partial")
        return started && value.at("result").is_null() && value.at("error").is_string();
    if (outcome != "acknowledged" || !started || returned != started ||
        !value.at("error").is_null())
        return false;
    const auto& result = value.at("result");
    if (!exact(result, {"binding_token", "generation", "state", "content_fingerprint"}) ||
        !hex(result.at("binding_token"), 32) || !index(result.at("generation"), true) ||
        !hex(result.at("content_fingerprint"), 64))
        return false;
    if (action == "create") return result.at("state") == "clip" && result.at("generation") == 1;
    const auto& payload = intent.at("payload");
    return result.at("binding_token") == payload.at("binding_token") &&
           result.at("generation").get<std::uint64_t>() ==
               payload.at("generation").get<std::uint64_t>() + (action == "redo" ? 1 : 0) &&
           result.at("state") == (action == "undo" ? "empty" : "clip");
}
Result<OrdinaryClipReceipt> observed(OrdinaryClipReceipt original, const LomResponse& response) {
    original.delivery = response.delivery;
    original.journal.reset();
    original.error = response.error;
    original.outcome = response.delivery == LomDeliveryState::NotSent
                           ? OrdinaryClipOutcome::NotSent
                           : OrdinaryClipOutcome::Indeterminate;
    if (!response.success || !response.value) return original;
    if (response.delivery != LomDeliveryState::ResponseReceived)
        return std::unexpected(ErrorCode::ProtocolError);
    const auto* value = std::get_if<json>(&*response.value);
    if (!value) return std::unexpected(ErrorCode::ProtocolError);
    // Unknown observations are absence of authority, never evidence of absence.
    if (value->value("outcome", "") == "unknown_epoch" ||
        value->value("outcome", "") == "unknown_operation") {
        if (!exact(*value,
                   {"schema_version",
                    "bridge_instance",
                    "document_token",
                    "operation_id",
                    "fingerprint",
                    "action",
                    "outcome",
                    "native_mutation_started",
                    "started_calls",
                    "returned_calls",
                    "result",
                    "error"}) ||
            !value->at("schema_version").is_number_integer() ||
            (value->at("action") != "unknown" &&
             value->at("action") != original.intent.at("action")) ||
            value->at("native_mutation_started") != false ||
            !value->at("native_mutation_started").is_boolean() ||
            !index(value->at("started_calls")) || value->at("started_calls") != 0 ||
            !index(value->at("returned_calls")) || value->at("returned_calls") != 0 ||
            !value->at("result").is_null() || !value->at("error").is_null())
            return std::unexpected(ErrorCode::ProtocolError);
        auto expected = token(original);
        for (const auto* key :
             {"schema_version", "bridge_instance", "document_token", "operation_id", "fingerprint"})
            if (!value->contains(key) || value->at(key) != expected.at(key))
                return std::unexpected(ErrorCode::ProtocolError);
        original.outcome = value->at("outcome") == "unknown_epoch"
                               ? OrdinaryClipOutcome::UnknownEpoch
                               : OrdinaryClipOutcome::UnknownOperation;
        return original;
    }
    if (!journal_valid(*value, original.intent)) return std::unexpected(ErrorCode::ProtocolError);
    original.journal = *value;
    const auto outcome = value->at("outcome").get<std::string>();
    original.outcome = outcome == "prepared"       ? OrdinaryClipOutcome::NativePrepared
                       : outcome == "acknowledged" ? OrdinaryClipOutcome::Acknowledged
                       : outcome == "partial"      ? OrdinaryClipOutcome::Partial
                                                   : OrdinaryClipOutcome::Declined;
    original.error = value->at("error").is_string()
                         ? std::optional{value->at("error").get<std::string>()}
                         : std::nullopt;
    return original;
}
} // namespace

bool ordinary_request_valid(const std::string& name, const json& value) noexcept {
    try {
        if (!value.is_object() || !value.contains("schema_version") ||
            !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1 ||
            !hex(value.value("bridge_instance", json{}), 32) ||
            !hex(value.value("document_token", json{}), 32) ||
            !hex(value.value("operation_id", json{}), 32))
            return false;
        if (name == "sunny_ordinary_execute" || name == "sunny_ordinary_operation")
            return exact(value,
                         {"schema_version",
                          "bridge_instance",
                          "document_token",
                          "operation_id",
                          "fingerprint"}) &&
                   hex(value.at("fingerprint"), 64);
        if (name != "sunny_ordinary_prepare" ||
            !exact(value,
                   {"schema_version",
                    "bridge_instance",
                    "document_token",
                    "operation_id",
                    "action",
                    "payload"}) ||
            !value.at("action").is_string())
            return false;
        const auto& payload = value.at("payload");
        const auto action = value.at("action").get<std::string>();
        if (action == "undo" || action == "redo")
            return exact(payload, {"binding_token", "generation"}) &&
                   hex(payload.at("binding_token"), 32) && index(payload.at("generation"), true);
        if (action != "create" ||
            !exact(payload, {"track_index", "slot_index", "clip_end", "notes"}) ||
            !index(payload.at("track_index")) || !index(payload.at("slot_index")) ||
            !payload.at("clip_end").is_number_float() || !finite(payload.at("clip_end")) ||
            payload.at("clip_end").get<double>() <= 0 || !payload.at("notes").is_array() ||
            payload.at("notes").size() > 65536)
            return false;
        return std::ranges::all_of(payload.at("notes"), [&](const json& item) {
            return note(item, payload.at("clip_end").get<double>());
        });
    } catch (...) {
        return false;
    }
}
Result<OrdinaryClipReceipt> prepare_ordinary_clip(const ManagedBridgeContext& context,
                                                  const std::string& id,
                                                  const std::string& action,
                                                  const json& payload) {
    json intent{{"schema_version", 1},
                {"bridge_instance", context.bridge_instance},
                {"document_token", context.document_token},
                {"operation_id", id},
                {"action", action},
                {"payload", payload}};
    if (!ordinary_request_valid("sunny_ordinary_prepare", intent) ||
        !managed_detail::managed_digest(intent))
        return std::unexpected(ErrorCode::ProtocolError);
    return OrdinaryClipReceipt{std::move(intent)};
}
Result<OrdinaryClipReceipt> execute_ordinary_clip(const OrdinaryClipReceipt& prepared,
                                                  LomTransport& transport) {
    if (prepared.outcome != OrdinaryClipOutcome::Prepared ||
        prepared.delivery != LomDeliveryState::NotSent || prepared.journal ||
        !ordinary_receipt_from_json(ordinary_receipt_to_json(prepared)))
        return std::unexpected(ErrorCode::ProtocolError);
    auto result = observed(prepared,
                           transport.send(LomProtocol::call_method(
                               LomPaths::song(), "sunny_ordinary_prepare", {prepared.intent})));
    if (!result || result->outcome != OrdinaryClipOutcome::NativePrepared) return result;
    return observed(prepared,
                    transport.send(LomProtocol::call_method(
                        LomPaths::song(), "sunny_ordinary_execute", {token(prepared)})));
}
Result<OrdinaryClipReceipt> reconcile_ordinary_clip(const OrdinaryClipReceipt& original,
                                                    LomTransport& transport) {
    if (!ordinary_receipt_from_json(ordinary_receipt_to_json(original)))
        return std::unexpected(ErrorCode::ProtocolError);
    return observed(original,
                    transport.send(LomProtocol::call_method(
                        LomPaths::song(), "sunny_ordinary_operation", {token(original)})));
}
json ordinary_receipt_to_json(const OrdinaryClipReceipt& receipt) {
    return {{"schema_version", 1},
            {"intent", receipt.intent},
            {"delivery", static_cast<int>(receipt.delivery)},
            {"outcome", outcome_name(receipt.outcome)},
            {"journal", receipt.journal ? *receipt.journal : json{}},
            {"error", receipt.error ? json(*receipt.error) : json{}}};
}
Result<OrdinaryClipReceipt> ordinary_receipt_from_json(const json& value) {
    try {
        if (!exact(value,
                   {"schema_version", "intent", "delivery", "outcome", "journal", "error"}) ||
            !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1 ||
            !ordinary_request_valid("sunny_ordinary_prepare", value.at("intent")) ||
            !value.at("delivery").is_number_integer() || !value.at("outcome").is_string() ||
            (!value.at("error").is_null() && !value.at("error").is_string()))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto delivery = value.at("delivery").get<int>();
        if (delivery < 0 || delivery > 2) return std::unexpected(ErrorCode::ProtocolError);
        OrdinaryClipReceipt receipt;
        receipt.intent = value.at("intent");
        receipt.delivery = static_cast<LomDeliveryState>(delivery);
        bool found = false;
        for (auto outcome : {OrdinaryClipOutcome::Prepared,
                             OrdinaryClipOutcome::NativePrepared,
                             OrdinaryClipOutcome::NotSent,
                             OrdinaryClipOutcome::Acknowledged,
                             OrdinaryClipOutcome::Declined,
                             OrdinaryClipOutcome::Partial,
                             OrdinaryClipOutcome::Indeterminate,
                             OrdinaryClipOutcome::UnknownOperation,
                             OrdinaryClipOutcome::UnknownEpoch})
            if (value.at("outcome") == outcome_name(outcome)) {
                receipt.outcome = outcome;
                found = true;
            }
        if (!found) return std::unexpected(ErrorCode::ProtocolError);
        if (!value.at("error").is_null()) receipt.error = value.at("error").get<std::string>();
        if (!value.at("journal").is_null()) {
            if (receipt.delivery != LomDeliveryState::ResponseReceived ||
                !journal_valid(value.at("journal"), receipt.intent))
                return std::unexpected(ErrorCode::ProtocolError);
            receipt.journal = value.at("journal");
            if (value.at("error") != receipt.journal->at("error"))
                return std::unexpected(ErrorCode::ProtocolError);
            const auto native = receipt.journal->at("outcome");
            if ((native == "prepared" && receipt.outcome != OrdinaryClipOutcome::NativePrepared) ||
                (native == "acknowledged" &&
                 receipt.outcome != OrdinaryClipOutcome::Acknowledged) ||
                (native == "declined" && receipt.outcome != OrdinaryClipOutcome::Declined) ||
                (native == "partial" && receipt.outcome != OrdinaryClipOutcome::Partial))
                return std::unexpected(ErrorCode::ProtocolError);
        } else if (receipt.outcome == OrdinaryClipOutcome::NativePrepared ||
                   receipt.outcome == OrdinaryClipOutcome::Acknowledged ||
                   receipt.outcome == OrdinaryClipOutcome::Declined ||
                   receipt.outcome == OrdinaryClipOutcome::Partial)
            return std::unexpected(ErrorCode::ProtocolError);
        if ((receipt.outcome == OrdinaryClipOutcome::Prepared ||
             receipt.outcome == OrdinaryClipOutcome::NotSent) &&
            receipt.delivery != LomDeliveryState::NotSent)
            return std::unexpected(ErrorCode::ProtocolError);
        if (receipt.outcome == OrdinaryClipOutcome::Prepared && receipt.error)
            return std::unexpected(ErrorCode::ProtocolError);
        if (receipt.outcome == OrdinaryClipOutcome::Indeterminate &&
            receipt.delivery == LomDeliveryState::NotSent)
            return std::unexpected(ErrorCode::ProtocolError);
        if ((receipt.outcome == OrdinaryClipOutcome::UnknownEpoch ||
             receipt.outcome == OrdinaryClipOutcome::UnknownOperation) &&
            receipt.delivery != LomDeliveryState::ResponseReceived)
            return std::unexpected(ErrorCode::ProtocolError);
        return receipt;
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
} // namespace sunny::infrastructure
