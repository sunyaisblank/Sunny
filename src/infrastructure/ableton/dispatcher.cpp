/**
 * @file dispatcher.cpp
 * @brief Bridge Message Dispatcher implementation
 *
 */

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <sunny/infrastructure/ableton/dispatcher.hpp>

namespace sunny::infrastructure {

namespace {

/// Convert orchestrator note events to wire note data
sunny::core::Result<std::vector<LomNoteData>>
to_note_data(const std::vector<sunny::core::NoteEvent>& notes) {
    if (notes.empty()) return std::unexpected(sunny::core::ErrorCode::FormatError);
    std::vector<LomNoteData> data;
    data.reserve(notes.size());
    for (const auto& event : notes) {
        auto converted = LomProtocol::from_note_event(event);
        if (!converted) return std::unexpected(converted.error());
        data.push_back(*converted);
    }
    return data;
}

std::optional<double> parse_positive_double(std::string_view text) {
    double value = 0.0;
#if defined(__cpp_lib_to_chars)
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
#else
    // libc++ before 20 lacks floating-point from_chars; strtod needs a terminated
    // buffer and must not accept leading whitespace, which from_chars rejects.
    if (text.empty() || std::isspace(static_cast<unsigned char>(text.front()))) return std::nullopt;
    const std::string terminated(text);
    char* end = nullptr;
    errno = 0;
    value = std::strtod(terminated.c_str(), &end);
    if (errno != 0 || end != terminated.c_str() + terminated.size()) return std::nullopt;
#endif
    if (!std::isfinite(value) || value <= 0.0) return std::nullopt;
    return value;
}

} // namespace

sunny::core::Result<LomRequest> BridgeDispatcher::to_lom_request(const BridgeMessage& msg) {
    const LomPath path = LomPath::parse(msg.path);
    if (!path.is_canonical()) return std::unexpected(sunny::core::ErrorCode::FormatError);

    switch (msg.type) {
    case BridgeMessageType::CreateClip: {
        if (msg.args.size() != 1) return std::unexpected(sunny::core::ErrorCode::FormatError);
        auto duration = parse_positive_double(msg.args[0]);
        if (!duration) return std::unexpected(sunny::core::ErrorCode::FormatError);
        return LomProtocol::call_method(path, "create_clip", {*duration});
    }

    case BridgeMessageType::CallMethod: {
        // args[0] is the method name; the rest are string arguments
        if (msg.args.empty() || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string method = msg.args[0];
        std::vector<LomValue> args;
        for (std::size_t i = 1; i < msg.args.size(); ++i) {
            args.emplace_back(msg.args[i]);
        }
        return LomProtocol::call_method(path, method, args);
    }

    case BridgeMessageType::SetProperty: {
        // args[0] is the property name, args[1] its value
        if (msg.args.size() != 2 || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string property = msg.args[0];
        LomValue value = msg.args[1];
        return LomProtocol::set_property(path, property, value);
    }

    case BridgeMessageType::GetProperty: {
        if (msg.args.size() != 1 || msg.args[0].empty())
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        std::string property = msg.args[0];
        return LomProtocol::get_property(path, property);
    }

    case BridgeMessageType::AddNotes:
    case BridgeMessageType::Batch:
        break;
    }

    return std::unexpected(sunny::core::ErrorCode::FormatError);
}

DispatchReport BridgeDispatcher::dispatch(const std::vector<BridgeMessage>& messages) {
    DispatchReport report;

    if (!online()) {
        report.failed = messages.size();
        report.errors.emplace_back("transport offline: no Ableton connection");
        return report;
    }

    for (const auto& msg : messages) {
        if (report.failed > 0) {
            // Stop at the first failure: each message relies on the effects of
            // the ones before it, as add_new_notes relies on create_clip.
            ++report.failed;
            continue;
        }
        LomResponse response;
        if (msg.type == BridgeMessageType::AddNotes) {
            const auto path = LomPath::parse(msg.path);
            auto notes = to_note_data(msg.notes);
            if (!path.is_canonical() || !notes) {
                response = LomResponse{false,
                                       std::nullopt,
                                       std::string{"invalid AddNotes bridge message"},
                                       LomDeliveryState::NotSent};
            } else {
                response = transport_->send_notes(path, *notes);
            }
        } else {
            auto request = to_lom_request(msg);
            if (!request) {
                response = LomResponse{false,
                                       std::nullopt,
                                       std::string{"invalid bridge message"},
                                       LomDeliveryState::NotSent};
            } else {
                response = transport_->send(*request);
            }
        }

        if (response.success) {
            ++report.sent;
        } else {
            ++report.failed;
            report.indeterminate = response.delivery == LomDeliveryState::SentWithoutValidResponse;
            report.errors.push_back(msg.path + ": " + response.error.value_or("unknown error"));
        }
    }

    if (report.failed > 1) {
        report.errors.push_back(std::to_string(report.failed - 1) +
                                " later message(s) not sent after the failure");
    }
    return report;
}

std::string BridgeDispatcher::offline_reason() const {
    constexpr std::string_view SURFACE_HINT =
        "ensure the Sunny Remote Script control surface is active in Live.";
    if (transport_ == nullptr) {
        return "Ableton is not connected: SUNNY_ABLETON_HOST is not set. Set it (and optionally "
               "SUNNY_TCP_PORT) and " +
               std::string(SURFACE_HINT);
    }
    if (tcp_transport_ != nullptr) {
        if (const auto failure = tcp_transport_->last_connect_failure()) {
            return "Could not connect to Ableton: " + std::string(describe(*failure)) + ".";
        }
    }
    return "Could not connect to Ableton; " + std::string(SURFACE_HINT);
}

LomResponse BridgeDispatcher::request(const LomRequest& request) {
    if (!online()) {
        return LomResponse{false,
                           std::nullopt,
                           std::string{"transport offline: no Ableton connection"},
                           LomDeliveryState::NotSent};
    }
    return transport_->send(request);
}

sunny::core::Result<std::optional<AbletonTargetProfile>> BridgeDispatcher::target_profile() {
    if (!online()) return std::unexpected(sunny::core::ErrorCode::ConnectionFailed);
    return transport_->target_profile();
}

namespace {
DispatchReport ordinary_report(OrdinaryClipReceipt receipt, const std::string& ns) {
    DispatchReport report;
    report.workspace_namespace = ns;
    report.sent = receipt.outcome == OrdinaryClipOutcome::Acknowledged ? 1 : 0;
    report.failed = report.sent ? 0 : 1;
    report.indeterminate = receipt.outcome == OrdinaryClipOutcome::Indeterminate ||
                           receipt.outcome == OrdinaryClipOutcome::UnknownEpoch ||
                           receipt.outcome == OrdinaryClipOutcome::UnknownOperation ||
                           receipt.outcome == OrdinaryClipOutcome::NativePrepared;
    if (!report.sent)
        report.errors.push_back(receipt.error.value_or(
            "Ordinary native operation was not acknowledged; its original token is retained"));
    report.ordinary_receipt = std::move(receipt);
    return report;
}
} // namespace

DispatchReport BridgeDispatcher::dispatch_clip(const std::vector<BridgeMessage>& messages,
                                               const std::string& action,
                                               const std::optional<OrdinaryClipReceipt>& authority,
                                               const std::string& expected_namespace) {
    DispatchReport refused{0, 1, false, {}};
    try {
        if (!online() || records_without_execution()) {
            refused.errors.push_back(
                "Native Clip authority requires an executing connected transport");
            return refused;
        }
        auto store = ordinary_store_provider_ ? ordinary_store_provider_(true) : nullptr;
        if (!store || !store->native_writes_available()) {
            refused.errors.push_back(
                "Ordinary Clip mutation requires durable saved-workspace history");
            return refused;
        }
        const auto& ns = store->workspace_namespace();
        if (!expected_namespace.empty() && expected_namespace != ns) {
            refused.errors.push_back("Ordinary history belongs to a different workspace namespace");
            return refused;
        }
        ManagedBridgeContext context;
        nlohmann::json payload;
        if (action == "create") {
            const auto observed_context = managed_bridge_context(*transport_);
            if (!observed_context) {
                refused.errors.push_back("Shared native document context unavailable");
                return refused;
            }
            context = *observed_context;
            if (messages.size() != 2 || messages[0].type != BridgeMessageType::CreateClip ||
                messages[1].type != BridgeMessageType::AddNotes || messages[0].args.size() != 1) {
                refused.errors.push_back("Unsupported ordinary Clip transaction");
                return refused;
            }
            const auto path = LomPath::parse(messages[0].path);
            if (!path.is_canonical() || path.segments.size() != 5 || path.segments[0] != "song" ||
                path.segments[1] != "tracks" || path.segments[3] != "clip_slots" ||
                messages[1].path != messages[0].path + "/clip") {
                refused.errors.push_back("Invalid ordinary Clip target");
                return refused;
            }
            const auto length = parse_positive_double(messages[0].args[0]);
            if (!length) {
                refused.errors.push_back("Invalid ordinary Clip length");
                return refused;
            }
            nlohmann::json notes = nlohmann::json::array();
            for (const auto& event : messages[1].notes) {
                const auto converted = LomProtocol::from_note_event(event);
                if (!converted) {
                    refused.errors.push_back("Invalid ordinary note event");
                    return refused;
                }
                auto request =
                    LomProtocol::add_new_notes(LomPath::parse(messages[1].path), {*converted});
                notes.push_back(std::get<nlohmann::json>(request.args.at(0)).at("notes").at(0));
            }
            payload = {{"track_index", std::stoi(path.segments[2])},
                       {"slot_index", std::stoi(path.segments[4])},
                       {"clip_end", *length},
                       {"notes", notes}};
        } else {
            if (!authority || authority->outcome != OrdinaryClipOutcome::Acknowledged ||
                !ordinary_receipt_from_json(ordinary_receipt_to_json(*authority))) {
                refused.errors.push_back(
                    "Ordinary undo/redo requires a typed native acknowledgement");
                return refused;
            }
            context = {authority->intent.at("bridge_instance"),
                       authority->intent.at("document_token")};
            const auto& result = authority->journal->at("result");
            payload = {{"binding_token", result.at("binding_token")},
                       {"generation", result.at("generation")}};
        }
        const auto id = store->new_attempt_id();
        if (!id) {
            refused.errors.push_back(id.error().message);
            return refused;
        }
        const auto prepared = prepare_ordinary_clip(context, *id, action, payload);
        if (!prepared) {
            refused.errors.push_back("Ordinary immutable intent rejected");
            return refused;
        }
        auto permit = store->fence_ordinary(*prepared);
        if (!permit) {
            refused.errors.push_back(permit.error().message);
            return refused;
        }
        auto original = permit->take_prepared();
        if (!original) {
            refused.errors.push_back("Ordinary dispatch permit already consumed");
            return refused;
        }
        refused.workspace_namespace = ns;
        refused.ordinary_receipt = *original;
        refused.ordinary_receipt->outcome = OrdinaryClipOutcome::Indeterminate;
        refused.ordinary_receipt->delivery = LomDeliveryState::SentWithoutValidResponse;
        refused.indeterminate = true;
        auto result = execute_ordinary_clip(*original, *transport_);
        if (!result) {
            original->outcome = OrdinaryClipOutcome::Indeterminate;
            original->delivery = LomDeliveryState::SentWithoutValidResponse;
            original->error = "Malformed ordinary native response; original attempt is query-only";
            result = std::move(*original);
        }
        auto report = ordinary_report(*result, ns);
        const auto saved = store->append_ordinary_evidence(*id, *result);
        if (!saved) {
            report.failed = 1;
            report.indeterminate = true;
            report.errors.push_back("Native evidence publication failed: " + saved.error().message);
        }
        return report;
    } catch (const std::exception& error) {
        refused.errors.push_back(error.what());
        return refused;
    }
}

DispatchReport BridgeDispatcher::reconcile_clip(const OrdinaryClipReceipt& receipt,
                                                const std::string& ns) {
    DispatchReport refused{0, 1, true, {}};
    try {
        auto store = ordinary_store_provider_ ? ordinary_store_provider_(false) : nullptr;
        if (!store || store->workspace_namespace() != ns) {
            refused.errors.push_back(
                "Ordinary query requires its original durable workspace namespace");
            return refused;
        }
        const auto id = receipt.intent.at("operation_id").get<std::string>();
        const auto* retained = store->find_ordinary(id);
        if (!retained || retained->prepared.intent.dump() != receipt.intent.dump()) {
            refused.errors.push_back("Original ordinary dispatch fence is unavailable");
            return refused;
        }
        if (!online()) {
            refused.errors.push_back(offline_reason());
            return refused;
        }
        const auto result = reconcile_ordinary_clip(receipt, *transport_);
        if (!result) {
            refused.errors.push_back("Malformed ordinary query response");
            return refused;
        }
        auto report = ordinary_report(*result, ns);
        const auto saved = store->append_ordinary_evidence(id, *result);
        if (!saved)
            report.errors.push_back("Ordinary query evidence could not be published: " +
                                    saved.error().message);
        return report;
    } catch (const std::exception& error) {
        refused.errors.push_back(error.what());
        return refused;
    }
}

} // namespace sunny::infrastructure
