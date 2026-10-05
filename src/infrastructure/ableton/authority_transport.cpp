#include <algorithm>
#include <map>
#include <stdexcept>
#include <sunny/infrastructure/ableton/authority_transport.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace {
LomResponse decline(const std::string& error) {
    return {false, std::nullopt, error, LomDeliveryState::NotSent};
}
bool exact(const json& value, std::initializer_list<const char*> keys) {
    return value.is_object() && value.size() == keys.size() &&
           std::ranges::all_of(keys, [&](const char* key) { return value.contains(key); });
}
std::optional<json> payload(const LomResponse& r) {
    if (!r.success || r.delivery != LomDeliveryState::ResponseReceived || !r.value)
        return std::nullopt;
    const auto* value = std::get_if<json>(&*r.value);
    return value ? std::optional(*value) : std::nullopt;
}
std::optional<LomResponse> decode_inner_value(const json& value) {
    // The retained gateway wraps the original native value in its journal.
    // Restore the same variant types as a directly decoded bridge response.
    return LomProtocol::deserialize_response(json{
        {"bridge_protocol_version", SUNNY_BRIDGE_PROTOCOL_VERSION},
        {"success", true},
        {"value", value}}.dump());
}
std::optional<std::uint32_t> count(const LomResponse& r) {
    if (!r.success || !r.value) return std::nullopt;
    json v = std::visit([](const auto& value) { return json(value); }, *r.value);
    if (!v.is_number_integer() || v.get<double>() < 0 || v.get<double>() > 2147483647)
        return std::nullopt;
    return v.get<std::uint32_t>();
}
} // namespace
struct AuthorityLomTransport::Impl {
    struct State {
        std::optional<ManagedBridgeContext> epoch;
        std::optional<LegacyPlanningAuthority> authority;
        std::optional<LegacyWorkflowRecipe> recipe;
        std::weak_ptr<RealizationStore> store;
        std::optional<std::string> workspace_namespace;
        std::string workflow;
        std::size_t next = 0;
        bool poisoned = false;
    };
    LomTransport& physical;
    StoreProvider provider;
    NamespaceProvider ns;
    std::map<std::weak_ptr<RequestControl>, State, std::owner_less<std::weak_ptr<RequestControl>>>
        requests;
    Impl(LomTransport& p, StoreProvider s, NamespaceProvider n)
        : physical(p), provider(std::move(s)), ns(std::move(n)) {}
    State& state() {
        auto control = current_request_control();
        if (!control)
            throw std::runtime_error("Native authority requires a current admitted MCP request");
        for (auto it = requests.begin(); it != requests.end();) {
            if (it->first.expired())
                it = requests.erase(it);
            else
                ++it;
        }
        if (requests.size() >= 256 && !requests.contains(std::weak_ptr<RequestControl>(control)))
            throw std::runtime_error("Native request authority capacity exhausted");
        return requests[std::weak_ptr<RequestControl>(control)];
    }
    void admission() {
        const auto control = current_request_control();
        if (!control || native_request_revoked())
            throw std::runtime_error(
                "Native write authority revoked by cancellation, deadline, or closed input");
        if (!control->native_origin || !valid_native_origin(*control->native_origin))
            throw std::runtime_error(
                "Native preparation requires the original admission epoch; run doctor_ableton "
                "before submitting authoring requests");
    }
    ManagedBridgeContext context(State& s) {
        admission();
        const auto current_namespace = ns();
        if (s.workspace_namespace && *s.workspace_namespace != current_namespace)
            throw std::runtime_error("Original workspace namespace changed within request");
        s.workspace_namespace = current_namespace;
        auto context = managed_bridge_context(physical);
        if (!context) throw std::runtime_error("Native original document context unavailable");
        const auto& origin = *current_request_control()->native_origin;
        if (origin.bridge_instance != context->bridge_instance ||
            origin.document_token != context->document_token)
            throw std::runtime_error("Original native admission epoch changed before preparation");
        if (s.epoch && (s.epoch->bridge_instance != context->bridge_instance ||
                        s.epoch->document_token != context->document_token))
            throw std::runtime_error("Original native document epoch changed within the request");
        s.epoch = *context;
        return *context;
    }
    bool scope_ready(const json& reply, const LegacyPlanningAuthority& a) {
        if (!exact(reply,
                   {"schema_version",
                    "bridge_instance",
                    "document_token",
                    "scope_id",
                    "outcome",
                    "graph_revision",
                    "workflow_id",
                    "error"}) ||
            !reply.at("schema_version").is_number_integer() || reply.at("schema_version") != 1 ||
            reply.at("outcome") != "ready" || !reply.at("error").is_null() ||
            !reply.at("graph_revision").is_number_integer() ||
            reply.at("graph_revision") != a.graph_revision)
            return false;
        for (const auto& [key, value] : a.scope.items())
            if (reply.at(key) != value) return false;
        return reply.at("workflow_id").is_null();
    }
};
AuthorityLomTransport::AuthorityLomTransport(LomTransport& p, StoreProvider s, NamespaceProvider n)
    : impl_(std::make_unique<Impl>(p, std::move(s), std::move(n))) {}
AuthorityLomTransport::~AuthorityLomTransport() = default;
bool AuthorityLomTransport::is_connected() const {
    return impl_->physical.is_connected();
}
bool AuthorityLomTransport::records_without_execution() const {
    return impl_->physical.records_without_execution();
}
bool AuthorityLomTransport::ensure_connected() {
    return impl_->physical.ensure_connected();
}
Result<std::optional<LegacyPlanningAuthority>> AuthorityLomTransport::capture_legacy_authority() {
    if (records_without_execution()) return std::optional<LegacyPlanningAuthority>{};
    try {
        impl_->admission();
        auto& state = impl_->state();
        if (!state.workflow.empty())
            throw std::runtime_error("Cannot recapture authority during an active workflow");
        if (state.authority) return state.authority;
        const auto c = impl_->context(state);
        json request{{"schema_version", 1},
                     {"bridge_instance", c.bridge_instance},
                     {"document_token", c.document_token},
                     {"scope_id", nullptr}};
        auto reply = payload(impl_->physical.send(
            LomProtocol::call_method(LomPaths::song(), "sunny_legacy_scope", {request})));
        if (!reply) throw std::runtime_error("Original native graph capture unavailable");
        auto candidate = request;
        candidate["scope_id"] = reply->value("scope_id", json{});
        LegacyPlanningAuthority a{candidate, 0, impl_->ns()};
        if (!legacy_authority_from_json(legacy_authority_to_json(a)) ||
            !impl_->scope_ready(*reply, a))
            throw std::runtime_error("Original native graph capture declined or malformed");
        state.authority = a;
        return std::optional(a);
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<void> AuthorityLomTransport::activate_legacy_workflow(const LegacyWorkflowRecipe& recipe) {
    if (records_without_execution()) return {};
    try {
        impl_->admission();
        auto& state = impl_->state();
        if (!state.workflow.empty() || !legacy_recipe_from_json(legacy_recipe_to_json(recipe)) ||
            recipe.authority.workspace_namespace != impl_->ns())
            throw std::runtime_error("Workflow original authority/namespace unavailable");
        auto c = impl_->context(state);
        if (recipe.authority.scope.at("bridge_instance") != c.bridge_instance ||
            recipe.authority.scope.at("document_token") != c.document_token)
            throw std::runtime_error("Original plan epoch changed");
        state.authority = recipe.authority;
        auto store = impl_->provider(true);
        if (!store || store->workspace_namespace() != recipe.authority.workspace_namespace ||
            !store->native_writes_available())
            throw std::runtime_error(
                "Saved durable current workspace or native history is unavailable");
        auto reply = payload(impl_->physical.send(LomProtocol::call_method(
            LomPaths::song(), "sunny_legacy_scope", {recipe.authority.scope})));
        if (!reply || !impl_->scope_ready(*reply, recipe.authority))
            throw std::runtime_error("Original planning graph or before-state changed");
        auto id = store->new_attempt_id();
        if (!id) throw std::runtime_error(id.error().message);
        const auto fence = store->fence_legacy_workflow(*id, recipe);
        if (!fence) throw std::runtime_error(fence.error().message);
        state.authority = recipe.authority;
        state.recipe = recipe;
        state.store = std::move(store);
        state.workflow = *id;
        state.next = 0;
        state.poisoned = false;
        return {};
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<void> AuthorityLomTransport::finish_legacy_workflow(bool completed) {
    if (records_without_execution()) return {};
    try {
        auto& s = impl_->state();
        bool ok = true;
        if (!s.workflow.empty()) {
            const bool all =
                completed && !s.poisoned && s.recipe && s.next == s.recipe->commands.size();
            auto store = s.store.lock();
            if (!store) throw std::runtime_error("Original workflow store lifetime ended");
            const auto finalized = store->finalize_legacy_workflow(s.workflow, all);
            ok = finalized.has_value();
        }
        if (s.authority) {
            // A finite physical exchange only; failure never replays any effect.
            static_cast<void>(impl_->physical.send(LomProtocol::call_method(
                LomPaths::song(), "sunny_legacy_finish", {s.authority->scope})));
        }
        s.authority.reset();
        s.recipe.reset();
        s.store.reset();
        s.workflow.clear();
        s.next = 0;
        s.poisoned = false;
        return ok ? Result<void>{} : std::unexpected(ErrorCode::ProtocolError);
    } catch (...) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
LomResponse AuthorityLomTransport::send(const LomRequest& request) {
    if (records_without_execution()) return impl_->physical.send(request);
    if (!LomProtocol::validate_request(request))
        return decline("Request outside the closed Sunny command algebra");
    const auto& method = request.property_or_method;
    const bool read = LomProtocol::is_read_only_request(request);
    const bool family =
        method.starts_with("sunny_managed_") || method.starts_with("sunny_ordinary_");
    if (method.starts_with("sunny_legacy_")) {
        if (method == "sunny_legacy_operation") return impl_->physical.send(request);
        return decline("Legacy native authority calls require the durable gateway");
    }
    try {
        if (read) {
            auto control = current_request_control();
            if (!control) return impl_->physical.send(request);
            auto& s = impl_->state();
            if (family || !s.authority) {
                if (LomProtocol::requires_native_origin(request)) {
                    const auto c = impl_->context(s);
                    const auto frame = LomProtocol::native_frame_origin(request);
                    if (!frame || frame->document_token != c.document_token ||
                        (frame->bridge_instance && *frame->bridge_instance != c.bridge_instance))
                        return decline("Retained preview original epoch does not match request "
                                       "authority");
                }
                auto response = impl_->physical.send(request);
                if (method == "sunny_managed_context" && response.success) {
                    auto c = payload(response);
                    if (!c || !c->contains("bridge_instance") || !c->contains("document_token"))
                        return decline("Native context is malformed");
                    ManagedBridgeContext candidate{c->at("bridge_instance").get<std::string>(),
                                                   c->at("document_token").get<std::string>()};
                    if (s.epoch && (s.epoch->bridge_instance != candidate.bridge_instance ||
                                    s.epoch->document_token != candidate.document_token))
                        return decline("Original native epoch changed within request");
                    s.epoch = candidate;
                }
                return response;
            }
            auto args = s.authority->scope;
            args["graph_revision"] = s.authority->graph_revision;
            args["command"] = legacy_command(request);
            auto value = payload(impl_->physical.send(
                LomProtocol::call_method(LomPaths::song(), "sunny_legacy_read", {args})));
            if (!value ||
                !exact(*value,
                       {"schema_version",
                        "bridge_instance",
                        "document_token",
                        "scope_id",
                        "graph_revision",
                        "outcome",
                        "value",
                        "error"}) ||
                !value->at("schema_version").is_number_integer() ||
                value->at("schema_version") != 1 || value->at("outcome") != "observed" ||
                !value->at("error").is_null() || !value->at("graph_revision").is_number_integer() ||
                value->at("graph_revision") != s.authority->graph_revision)
                return decline("Original scoped native observation declined");
            for (const auto& [key, expected] : s.authority->scope.items())
                if (value->at(key) != expected)
                    return decline("Scoped observation original token mismatch");
            auto decoded = decode_inner_value(value->at("value"));
            if (!decoded) return decline("Scoped native value is outside the bridge value algebra");
            return std::move(*decoded);
        }
        impl_->admission();
        auto& s = impl_->state();
        if (family) {
            const auto c = impl_->context(s);
            const auto frame = LomProtocol::native_frame_origin(request);
            if (!frame || frame->document_token != c.document_token ||
                (frame->bridge_instance && *frame->bridge_instance != c.bridge_instance))
                return decline("Family operation original epoch does not match request authority");
            return impl_->physical.send(request);
        }
        bool standalone = s.workflow.empty();
        if (standalone) {
            auto a = capture_legacy_authority();
            if (!a || !a->has_value()) return decline("Original native graph capture unavailable");
            LegacyWorkflowRecipe recipe;
            recipe.kind = "single_request";
            recipe.authority = **a;
            recipe.intent_fingerprint = *managed_detail::managed_digest(legacy_command(request));
            recipe.commands.push_back({{"command", legacy_command(request)}, {"phase", 0}});
            if (!activate_legacy_workflow(recipe)) {
                static_cast<void>(finish_legacy_workflow(false));
                return decline("Durable single-request workflow activation declined");
            }
        }
        if (s.poisoned || !s.recipe || s.next >= s.recipe->commands.size() ||
            managed_detail::managed_digest(s.recipe->commands.at(s.next).at("command")) !=
                managed_detail::managed_digest(legacy_command(request)))
            return decline("Native workflow recipe diverged or is unresolved");
        auto store = s.store.lock();
        if (!store || !s.authority || s.authority->workspace_namespace != impl_->ns())
            return decline("Original workflow namespace/store changed");
        auto id = store->new_attempt_id();
        if (!id) return decline(id.error().message);
        auto prepared = prepare_legacy_operation(
            *s.authority, s.workflow, *id, static_cast<std::uint32_t>(s.next + 1), request);
        if (!prepared) return decline("Legacy immutable preparation failed");
        auto fence = store->fence_legacy_child(s.workflow, *prepared);
        if (!fence) {
            s.poisoned = true;
            return decline(fence.error().message);
        }
        auto permit = fence->take_prepared();
        if (!permit) {
            s.poisoned = true;
            return decline("Fresh dispatch permit already consumed");
        }
        LegacyOperationReceipt receipt = *permit;
        try {
            auto executed = execute_legacy_operation(*permit, impl_->physical);
            if (executed)
                receipt = *executed;
            else
                throw std::runtime_error("Legacy execution evidence unavailable");
        } catch (const std::exception& e) {
            receipt.outcome = LegacyOperationOutcome::Indeterminate;
            receipt.stage = LegacyOperationStage::Execute;
            receipt.delivery = LomDeliveryState::SentWithoutValidResponse;
            receipt.error = e.what();
        }
        const auto appended = store->append_legacy_evidence(s.workflow, receipt);
        LomResponse response;
        response.delivery = receipt.delivery;
        response.error = receipt.error;
        response.legacy_receipt = std::make_shared<LegacyOperationReceipt>(receipt);
        response.success = appended && receipt.outcome == LegacyOperationOutcome::Acknowledged;
        if (response.success) {
            const auto& result = receipt.journal->at("result");
            s.authority->graph_revision = result.at("graph_revision").get<std::uint32_t>();
            auto decoded = decode_inner_value(result.at("value"));
            if (decoded) {
                response.value = std::move(decoded->value);
                ++s.next;
            } else {
                response.success = false;
                response.error = "Acknowledged native value is outside the bridge value algebra";
                s.poisoned = true;
            }
        } else {
            s.poisoned = true;
            if (!appended)
                response.error = "Native receipt retained in response; durable append failed: " +
                                 appended.error().message;
            if (!response.error)
                response.error =
                    "Original native operation is unresolved; reconcile its original token";
        }
        if (standalone) {
            auto finished = finish_legacy_workflow(response.success);
            if (!finished && response.success) {
                response.success = false;
                response.error =
                    "Native operation acknowledged but durable workflow finalization failed";
            }
        }
        return response;
    } catch (const std::exception& e) {
        return decline(e.what());
    }
}
LomResponse AuthorityLomTransport::send_notes(const LomPath& p, const std::vector<LomNoteData>& n) {
    return send(LomProtocol::add_new_notes(p, n));
}
Result<std::optional<AbletonTargetProfile>> AuthorityLomTransport::target_profile() {
    if (records_without_execution()) return impl_->physical.target_profile();
    auto v = payload(send(LomProtocol::call_method(LomPaths::song(), "sunny_get_target_profile")));
    if (!v) return std::unexpected(ErrorCode::ProtocolError);
    auto parsed = target_profile_from_json(*v);
    if (!parsed) return std::unexpected(parsed.error());
    return std::optional(*parsed);
}
Result<std::optional<AbletonTargetSnapshot>> AuthorityLomTransport::target_snapshot() {
    if (records_without_execution()) return impl_->physical.target_snapshot();
    auto v = payload(send(LomProtocol::call_method(LomPaths::song(), "sunny_get_target_snapshot")));
    if (!v) return std::unexpected(ErrorCode::ProtocolError);
    auto parsed = target_snapshot_from_json(*v);
    if (!parsed) return std::unexpected(parsed.error());
    return std::optional(*parsed);
}
Result<std::optional<std::uint32_t>> AuthorityLomTransport::scene_count() {
    if (records_without_execution()) return impl_->physical.scene_count();
    auto v = count(send(LomProtocol::call_method(LomPaths::song(), "sunny_get_scene_count")));
    if (!v) return std::unexpected(ErrorCode::ProtocolError);
    return v;
}
Result<std::optional<std::uint32_t>> AuthorityLomTransport::return_track_count() {
    if (records_without_execution()) return impl_->physical.return_track_count();
    auto v =
        count(send(LomProtocol::call_method(LomPaths::song(), "sunny_get_return_track_count")));
    if (!v) return std::unexpected(ErrorCode::ProtocolError);
    return v;
}
Result<std::optional<std::uint32_t>> AuthorityLomTransport::device_count(const LomPath& p) {
    if (records_without_execution()) return impl_->physical.device_count(p);
    auto v = count(send(LomProtocol::call_method(p, "sunny_get_device_count")));
    if (!v) return std::unexpected(ErrorCode::ProtocolError);
    return v;
}
} // namespace sunny::infrastructure
