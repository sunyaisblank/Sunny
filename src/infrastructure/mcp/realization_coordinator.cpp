#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <sunny/core/timbre/live_capabilities.hpp>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/mcp/realization_coordinator.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;

// This is selected-coordinator output admission, not a global MCP policy.
// A legal request ID can occupy almost the entire admitted stdio input. The
// fixed reserve includes JSON-RPC/newline framing and current modern MCP meta.
constexpr std::size_t coordinator_framing_reserve = 4096;
constexpr std::size_t coordinator_max_mixer_tracks = 4096;

struct ReplyBytes {
    std::size_t value = 0;
    bool valid = true;

    void add(std::size_t bytes, std::size_t count = 1) {
        const auto maximum = std::numeric_limits<std::size_t>::max();
        if (!valid || (count != 0 && bytes > maximum / count) || value > maximum - bytes * count) {
            valid = false;
            return;
        }
        value += bytes * count;
    }
    void add_json(const json& value, std::size_t count = 1) {
        const auto bytes = managed_detail::json_wire_bound(value);
        if (!bytes)
            valid = false;
        else
            add(*bytes, count);
    }
};

bool compact_payload_reservation_fits(const ReplyBytes& payload) {
    ReplyBytes framed;
    framed.valid = payload.valid;
    // server.cpp emits result.dump() as JSON text AND structuredContent. A
    // quoted compact JSON string adds at most one escape per input byte, so
    // the two copies require at most 3B bytes, even for controls/UTF-8 names.
    framed.add(payload.value, 3);
    framed.add(MCP_MAX_INPUT_BYTES);
    framed.add(coordinator_framing_reserve);
    return framed.valid && framed.value <= managed_detail::managed_response_limit;
}

bool complete_coordinator_reply_fits(const json& output) {
    try {
        // Refuse nonfinite, invalid UTF-8 or excessively deep values before
        // dump could silently turn nonfinite values into null.
        if (!managed_detail::json_wire_bound(output)) return false;
        const bool error = output.contains("error") ||
                           (output.contains("success") && output.at("success") == false);
        json result{{"content", json::array({{{"type", "text"}, {"text", output.dump()}}})},
                    {"structuredContent", output},
                    {"isError", error},
                    {"resultType", "complete"}};
        const json envelope{{"jsonrpc", "2.0"}, {"id", nullptr}, {"result", std::move(result)}};
        // These are the actual server.run serializer options. Metadata is in
        // the fixed reserve; the null placeholder is conservatively retained
        // in addition to the worst accepted ID, rather than subtracted.
        ReplyBytes bytes;
        bytes.add(envelope.dump(-1, ' ', false, json::error_handler_t::replace).size());
        bytes.add(MCP_MAX_INPUT_BYTES);
        bytes.add(coordinator_framing_reserve);
        return bytes.valid && bytes.value <= managed_detail::managed_response_limit;
    } catch (const json::exception&) {
        return false;
    }
}

std::size_t planned_mixer_population(const json& plan) {
    std::optional<std::size_t> observed;
    std::size_t additions = 0;
    for (const auto& part : plan.at("parts")) {
        if (part.at("clip_action") == "create") ++additions;
        if (part.contains("current_static_mixer_inspection")) {
            const auto count = part.at("current_static_mixer_inspection")
                                   .at("inspection")
                                   .at("before")
                                   .at("solo_cohort")
                                   .size();
            observed = std::max(observed.value_or(0), count);
        }
    }
    for (const auto& route : plan.at("routing"))
        if (route.at("selection").at("kind") == "create_return") ++additions;
    // All captured snapshots belong to the same guarded current Set. Foreign
    // collection drift is refused; only the selected Track/Return additions
    // can enlarge a successful final cohort. Without a snapshot use the real
    // producer limit, never a fabricated smaller population.
    if (!observed) return coordinator_max_mixer_tracks;
    if (*observed > coordinator_max_mixer_tracks ||
        additions > coordinator_max_mixer_tracks - *observed)
        throw std::runtime_error("StaticMixerUnavailable: known selected Track/Return additions "
                                 "exceed the final native Mixer cohort limit before dispatch");
    return *observed + additions;
}

bool compact_final_reservation_fits(const json& selection, const json& plan) {
    ReplyBytes bytes;
    bytes.add(8192); // Fixed completion/scope fields and one bounded failure.
    std::size_t phases = 0, attempts = 0;
    const auto mixer_population = planned_mixer_population(plan);
    const auto mixer_row = *managed_detail::json_wire_bound(json{{"kind", "return"},
                                                                 {"index", UINT32_MAX},
                                                                 {"mute", false},
                                                                 {"solo", false},
                                                                 {"muted_via_solo", false}});
    const auto mixer = [&] {
        bytes.add(8192); // Desired/current scalars, display80, guard and keys.
        bytes.add(mixer_row + 2, mixer_population);
    };
    const auto resolutions = [&](const json& intents) {
        for (const auto& intent : intents) {
            bytes.add_json(intent);
            const auto id = intent.at("capability_id").get<std::string>();
            const auto registry = sunny::core::live_native_parameter_registry();
            const auto found =
                std::ranges::find(registry, id, &sunny::core::LiveNativeParameterCapability::id);
            // Actual resolution.original_name is required to equal this
            // registered name. Unreviewed coverage retains the text4096 bound.
            if (found == registry.end())
                bytes.add(6 * 4096 + 2);
            else
                bytes.add_json(found->parameter_original_name);
            // Exact key/intent/index/unit plus closed current_readback: finite
            // binary64 spellings32, display<=80 UTF-8 bytes, no populations.
            bytes.add(2048);
        }
    };
    for (const auto& part : plan.at("parts")) {
        const auto selected = std::ranges::find_if(selection.at("parts"), [&](const auto& p) {
            return p.at("part_id") == part.at("part_id");
        });
        if (selected == selection.at("parts").end()) return false;
        bytes.add(8192); // Binding guard: bounded keys/context/FPs/counts only.
        ++phases;
        attempts += part.value("clip_phase_count", std::size_t{1});
        if (part.contains("source_plan") || part.contains("effect_plan"))
            bytes.add(2048, SUNNY_MANAGED_MAX_DEVICES); // Closed declarations.
        if (part.contains("source_plan")) {
            bytes.add_json(part.at("source_plan").at("intents"), 2);
            resolutions(part.at("source_plan").at("intents"));
            ++phases;
            attempts += 2; // At most source insertion then physical update.
        }
        if (part.contains("effect_plan")) {
            const auto& effects = part.at("effect_plan");
            for (const auto& entry : effects.at("entries")) {
                // Full retained IR and residuals remain in the returned plan,
                // never in compact final evidence or phase summaries.
                json selected = json::object();
                for (const auto* field : {"enable_modes",
                                          "setup_modes",
                                          "setup_properties",
                                          "setup_physical_intents",
                                          "physical_intents",
                                          "final_modes"})
                    selected[field] = entry.at(field);
                bytes.add_json(selected, 2);
                resolutions(entry.at("setup_physical_intents"));
                resolutions(entry.at("physical_intents"));
                if (!entry.at("final_modes").empty())
                    // Historical bypass proof: ID/FP and up to32 merged
                    // physical targets/tolerances, not its parameter snapshot.
                    bytes.add(1024, SUNNY_MANAGED_MAX_DEVICE_TARGETS);
                attempts += 6;
            }
            ++phases;
        }
        if (selected->contains("static_mixer")) {
            mixer();
            phases += 4;
            attempts += 2;
        }
        if (selected->contains("pan_lane")) {
            // Two native parameter names<=1024 bytes (worst escaped controls),
            // at most128 time/value samples, finite extent and binding guard.
            bytes.add(32768);
            phases += selected->at("pan_lane").at("mode") == "replace" ? 2 : 1;
            ++attempts;
        }
    }
    for (const auto& route : plan.at("routing")) {
        phases += 2;
        ++attempts;
        if (route.at("selection").at("kind") == "send_level") {
            bytes.add(8192); // Own intent/readback/display80 and binding guard.
            bytes.add_json(route.at("plan").at("intent"), 2);
        }
    }
    for (const auto& ignored : selection.value("retire_part_ids", json::array())) {
        static_cast<void>(ignored);
        bytes.add(8192);
        mixer();
        phases += 3;
        attempts += 2;
    }
    if (selection.contains("song_settings_part_id")) {
        bytes.add(8192); // Desired/current tempo/meter scalars and guard only.
        phases += 2;
        ++attempts;
    }
    bytes.add(1024, phases); // Whitelisted phase status, no raw ACK/observation.
    // Global tokens, per-phase token arrays, and one final attempt token. The
    // existing Store bounds actual attempts; this adds no identity reservation.
    bytes.add(3 * 35, std::min(attempts, REALIZATION_STORE_MAX_ATTEMPTS));
    return compact_payload_reservation_fits(bytes);
}

json compact_stopped_result(const json& original) {
    json result = json::object();
    for (const auto* field : {"success",
                              "state",
                              "error",
                              "attempt_id",
                              "dispatch_ordinal",
                              "dispatch_fenced",
                              "mutation_dispatched",
                              "phase_attempt_ids",
                              "retry_authorized",
                              "authority_granted",
                              "history_saved",
                              "history_may_have_committed",
                              "actual_receipt_outcome",
                              "effect_phase",
                              "device_key",
                              "selected_clip_phases_completed",
                              "selected_effect_phases_completed"})
        if (original.contains(field)) result[field] = original.at(field);
    // Keep the existing observable outcome without duplicating the original
    // immutable request/journal. Both remain available by the original token.
    if (original.contains("receipt"))
        result["receipt"] = {{"outcome", original.at("receipt").at("outcome")}};
    if (original.contains("actual_receipt"))
        result["actual_receipt_outcome"] = original.at("actual_receipt").at("outcome");
    return result;
}

json compact_capacity_failure(const json& original, const json& summaries, const json& attempts) {
    json phases = summaries;
    for (auto& phase : phases) {
        auto& result = phase.at("result");
        result.erase("error");
        if (result.contains("state")) {
            const auto state = managed_detail::json_wire_bound(result.at("state"));
            if (!state || *state > 512) {
                result.erase("state");
                result["state_text_unavailable"] = true;
            }
        }
    }
    json output{{"success", false},
                {"state", "reply_capacity_unavailable"},
                {"error",
                 "ReplyCapacityUnavailable: complete selected coordinator MCP reply "
                 "exceeds the finite limit; original operation outcomes are unchanged"},
                {"original_result_success", original.value("success", false)},
                {"selected_contract_completed", false},
                {"authority_granted", original.value("authority_granted", false)},
                {"retry_authorized", false},
                {"mutation_dispatched", !attempts.empty()},
                {"phase_attempt_ids", attempts},
                {"phases", std::move(phases)},
                {"scope", "selected_owning_project_native_contract"},
                {"complete_project_realization", false},
                {"host_qualified", false}};
    for (const auto* key : {"attempt_id",
                            "dispatch_ordinal",
                            "dispatch_fenced",
                            "history_saved",
                            "history_may_have_committed",
                            "actual_receipt_outcome"})
        if (original.contains(key)) output[key] = original.at(key);
    if (original.contains("state")) {
        const auto state = managed_detail::json_wire_bound(original.at("state"));
        if (state && *state <= 512) output["original_result_state"] = original.at("state");
    }
    if (original.contains("receipt"))
        output["original_receipt_outcome"] = original.at("receipt").at("outcome");
    return output;
}

json failure(std::string message) {
    return {{"success", false},
            {"state", "declined"},
            {"error", std::move(message)},
            {"mutation_dispatched", false},
            {"authority_granted", false}};
}

void closed(const json& value, std::initializer_list<std::string_view> keys) {
    if (!value.is_object()) throw std::runtime_error("Expected a closed selection object");
    for (const auto& [key, ignored] : value.items()) {
        static_cast<void>(ignored);
        if (std::ranges::find(keys, key) == keys.end())
            throw std::runtime_error("Unadvertised selection field: " + key);
    }
}

void selection_valid(const json& selection) {
    closed(selection,
           {"score_id",
            "expected_project_revision",
            "ppq",
            "parts",
            "routing",
            "retire_part_ids",
            "song_settings_part_id"});
    const auto& parts = selection.at("parts");
    if (!parts.is_array() || parts.size() > 64)
        throw std::runtime_error("Select at most 64 unique active Parts");
    std::set<json> ids;
    for (const auto& part : parts) {
        closed(part,
               {"part_id", "source_selections", "effect_selections", "static_mixer", "pan_lane"});
        if (!ids.insert(part.at("part_id")).second)
            throw std::runtime_error("Selected Part identities must be unique");
        if (part.contains("static_mixer"))
            closed(part.at("static_mixer"), {"domains", "volume_tolerance_db"});
        if (part.contains("pan_lane")) {
            closed(part.at("pan_lane"), {"lane_index", "mode"});
            const auto& mode = part.at("pan_lane").at("mode");
            if (mode != "absent" && mode != "replace")
                throw std::runtime_error("Select absent or replace for the selected pan lane");
        }
    }
    const auto& routing = selection.at("routing");
    if (!routing.is_array() || routing.size() > 128)
        throw std::runtime_error("Select at most 128 finite routing phases");
    std::set<json> route_targets, returns;
    std::map<json, std::set<std::string>> outputs;
    for (const auto& route : routing) {
        closed(route,
               {"part_id",
                "kind",
                "aux_id",
                "return_index",
                "send_tolerance_db",
                "group_id",
                "group_track_index",
                "selector",
                "route_identifier"});
        if (!ids.contains(route.at("part_id")))
            throw std::runtime_error("Routing anchors must be selected active Parts");
        const auto& kind = route.at("kind");
        if (kind == "output_type" || kind == "output_channel") {
            auto& selected = outputs[route.at("part_id")];
            selected.insert(kind.get<std::string>());
            if (selected.size() > 1)
                throw std::runtime_error(
                    "Apply output type, inspect the new actual channel cohort, "
                    "then approve a separate channel plan");
        }
        const auto target =
            json::array({route.at("part_id"),
                         kind,
                         route.value("aux_id", route.value("group_id", json(nullptr)))});
        if (!route_targets.insert(target).second)
            throw std::runtime_error("Duplicate selected routing target");
        if ((kind == "create_return" || kind == "adopt_return") &&
            !returns.insert(route.at("aux_id")).second)
            throw std::runtime_error("Create or adopt each selected Aux once");
    }
    if (selection.contains("retire_part_ids")) {
        const auto& retired = selection.at("retire_part_ids");
        if (!retired.is_array() || retired.size() > 64)
            throw std::runtime_error("Select at most 64 removed Parts for mute retirement");
        for (const auto& id : retired)
            if (!ids.insert(id).second)
                throw std::runtime_error(
                    "Retired Parts must be unique and separate from active Parts");
    }
    if (ids.empty()) throw std::runtime_error("Select at least one active or removed Part");
    if (selection.contains("song_settings_part_id") &&
        std::ranges::none_of(parts, [&](const auto& p) {
            return p.at("part_id") == selection.at("song_settings_part_id");
        }))
        throw std::runtime_error("Song settings need a selected active Part anchor");
}

json common(const json& selection, const json& part_id) {
    json result{{"score_id", selection.at("score_id")},
                {"part_id", part_id},
                {"expected_project_revision", selection.at("expected_project_revision")}};
    if (selection.contains("ppq")) result["ppq"] = selection.at("ppq");
    return result;
}

void approval(const json& arguments, const char* name) {
    if (!arguments.contains(name) || !arguments.at(name).is_boolean() || arguments.at(name) != true)
        throw std::runtime_error(std::string("Explicit selected-plan approval required: ") + name);
}

json prepare_plan(const json& selection, const McpToolHandler& prepare) {
    selection_valid(selection);
    auto output = prepare(selection);
    if (!output.value("success", false)) return output;
    output["plan"]["selection"] = selection;
    if (!complete_coordinator_reply_fits(output))
        return failure("ReplyCapacityUnavailable: complete selected project plan "
                       "exceeds the finite limit");
    // The returned plan is an actual future public stdio input, whose limit is
    // smaller than native TCP replies. Reserve the framing, approvals and ID.
    const auto input_bound = managed_detail::json_wire_bound(output.at("plan"));
    if (!input_bound || *input_bound > MCP_MAX_INPUT_BYTES - 4096)
        return failure("ReplyCapacityUnavailable: approved plan cannot fit a "
                       "public apply request");
    if (!compact_final_reservation_fits(selection, output.at("plan")))
        return failure("ReplyCapacityUnavailable: complete selected coordinator MCP reply "
                       "cannot be reserved before native dispatch; select a smaller "
                       "aggregate");
    return output;
}
} // namespace

void register_project_realization_coordinator_tools(
    McpServer& server,
    std::shared_ptr<const NativeAuthoringTools> tools,
    McpToolHandler prepare,
    McpToolHandler verify,
    json source_schema,
    json effect_schema,
    json mixer_schema,
    json routing_schema) {
    for (auto* schema : {&mixer_schema, &routing_schema}) {
        for (const auto* name : {"score_id", "expected_project_revision", "ppq"}) {
            (*schema)["properties"].erase(name);
            auto& required = (*schema)["required"];
            required.erase(std::remove(required.begin(), required.end(), json(name)),
                           required.end());
        }
    }
    mixer_schema["properties"].erase("part_id");
    routing_schema["properties"].erase("projection_source");
    routing_schema["properties"].erase("historical_projection_attempt");
    auto& required_mixer = mixer_schema["required"];
    required_mixer.erase(std::remove(required_mixer.begin(), required_mixer.end(), json("part_id")),
                         required_mixer.end());
    json schema{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties",
         {{"score_id", {{"type", "integer"}, {"minimum", 1}}},
          {"expected_project_revision", {{"type", "integer"}, {"minimum", 1}}},
          {"ppq", {{"type", "integer"}, {"minimum", 1}, {"maximum", 32767}}},
          {"parts",
           {{"type", "array"},
            {"maxItems", 64},
            {"items",
             {{"type", "object"},
              {"additionalProperties", false},
              {"properties",
               {{"part_id", {{"type", "integer"}, {"minimum", 1}}},
                {"source_selections", std::move(source_schema)},
                {"effect_selections", std::move(effect_schema)},
                {"static_mixer", std::move(mixer_schema)},
                {"pan_lane",
                 {{"type", "object"},
                  {"additionalProperties", false},
                  {"properties",
                   {{"lane_index", {{"type", "integer"}, {"minimum", 0}}},
                    {"mode", {{"type", "string"}, {"enum", {"absent", "replace"}}}}}},
                  {"required", {"lane_index", "mode"}}}}}},
              {"required", {"part_id"}}}}}},
          {"routing", {{"type", "array"}, {"maxItems", 128}, {"items", std::move(routing_schema)}}},
          {"retire_part_ids",
           {{"type", "array"},
            {"maxItems", 64},
            {"uniqueItems", true},
            {"items", {{"type", "integer"}, {"minimum", 1}}}}},
          {"song_settings_part_id", {{"type", "integer"}, {"minimum", 1}}}}},
        {"required", {"score_id", "expected_project_revision", "parts", "routing"}}};
    server.register_tool("project_realization_plan",
                         "Preflight a finite selected owning project: exact "
                         "Parts, physical devices, static Mixer, "
                         "Step pan, routing, mute retirement and optional Song "
                         "settings; read current cohorts "
                         "without writes",
                         std::move(schema),
                         [prepare](const json& arguments) {
                             try {
                                 return prepare_plan(arguments, prepare);
                             } catch (const std::exception& e) {
                                 return failure(e.what());
                             }
                         });

    json apply_schema{{"type", "object"},
                      {"additionalProperties", false},
                      {"properties", {{"plan", {{"type", "object"}}}}},
                      {"required", {"plan", "explicit_plan_approval"}}};
    for (const auto* name : {"explicit_plan_approval",
                             "explicit_current_mixer_approval",
                             "explicit_set_wide_audible_approval",
                             "explicit_current_routing_approval",
                             "explicit_selected_envelope_replacement",
                             "allow_unsampled_selected_state_overwrite",
                             "explicit_mute_retirement",
                             "explicit_set_wide_approval"})
        apply_schema["properties"][name] = {{"type", "boolean"}, {"const", true}};
    server.register_tool(
        "project_realization_apply",
        "Apply the exact approved selected project using existing durable "
        "per-operation fences; "
        "stop at uncertain original tokens and freshly verify final finite "
        "cohorts",
        std::move(apply_schema),
        [tools, prepare, verify](const json& arguments) -> json {
            json phases = json::array(), attempts = json::array();
            const auto finish = [&](json output) {
                if (!output.value("success", false)) output = compact_stopped_result(output);
                json summaries = json::array();
                for (const auto& phase : phases) {
                    json summary{{"tool", phase.at("tool")},
                                 {"part_id", phase.at("part_id")},
                                 {"result", json::object()}};
                    const auto& result = phase.at("result");
                    for (const auto* field : {"success",
                                              "state",
                                              "error",
                                              "attempt_id",
                                              "dispatch_ordinal",
                                              "dispatch_fenced",
                                              "mutation_dispatched",
                                              "phase_attempt_ids",
                                              "retry_authorized",
                                              "history_saved",
                                              "history_may_have_committed",
                                              "actual_receipt_outcome",
                                              "selected_effect_phases_completed",
                                              "authority_granted"})
                        if (result.contains(field)) summary["result"][field] = result.at(field);
                    if (result.contains("receipt"))
                        summary["result"]["receipt_outcome"] = result.at("receipt").at("outcome");
                    if (result.contains("actual_receipt"))
                        summary["result"]["actual_receipt_outcome"] =
                            result.at("actual_receipt").at("outcome");
                    summaries.push_back(std::move(summary));
                }
                output["phases"] = summaries;
                output["phase_attempt_ids"] = attempts;
                output["mutation_dispatched"] = !attempts.empty();
                output["scope"] = "selected_owning_project_native_contract";
                output["complete_project_realization"] = false;
                output["host_qualified"] = false;
                if (complete_coordinator_reply_fits(output)) return output;
                auto compact = compact_capacity_failure(output, summaries, attempts);
                if (!complete_coordinator_reply_fits(compact)) {
                    // Defensive against unexpected diagnostic expansion. The
                    // original Store-bounded32hex tokens and stopped token stay
                    // intact; no native outcome or retry permission is inferred.
                    compact.erase("phases");
                    compact["phase_summaries_unavailable"] = true;
                }
                return compact;
            };
            try {
                approval(arguments, "explicit_plan_approval");
                const auto& selection = arguments.at("plan").at("selection");
                auto fresh = prepare_plan(selection, prepare);
                if (!fresh.value("success", false)) return finish(std::move(fresh));
                if (fresh.at("plan") != arguments.at("plan"))
                    return finish(failure("Selected plan, owning revision or actual native cohort "
                                          "changed; plan again"));
                for (const auto& part : selection.at("parts")) {
                    if (part.contains("static_mixer")) {
                        approval(arguments, "explicit_current_mixer_approval");
                        for (const auto& domain : part.at("static_mixer").at("domains"))
                            if (domain == "solo")
                                approval(arguments, "explicit_set_wide_audible_approval");
                    }
                    if (part.contains("pan_lane") && part.at("pan_lane").at("mode") == "replace") {
                        approval(arguments, "explicit_selected_envelope_replacement");
                        approval(arguments, "allow_unsampled_selected_state_overwrite");
                    }
                }
                if (!selection.at("routing").empty())
                    approval(arguments, "explicit_current_routing_approval");
                if (selection.contains("retire_part_ids") &&
                    !selection.at("retire_part_ids").empty()) {
                    approval(arguments, "explicit_mute_retirement");
                    approval(arguments, "explicit_current_mixer_approval");
                }
                if (selection.contains("song_settings_part_id"))
                    approval(arguments, "explicit_set_wide_approval");

                json stopped;
                const auto run = [&](const std::string& tool, const json& input) {
                    auto output = tools->at(tool)(input);
                    phases.push_back(
                        {{"tool", tool}, {"part_id", input.at("part_id")}, {"result", output}});
                    if (output.contains("phase_attempt_ids")) {
                        for (const auto& id : output.at("phase_attempt_ids"))
                            if (std::ranges::find(attempts, id) == attempts.end())
                                attempts.push_back(id);
                    } else if (output.value("dispatch_fenced", false) &&
                               output.value("mutation_dispatched", false)) {
                        // Native writers return their allocated fenced token; reads
                        // and unchanged revisions explicitly say no dispatch.
                        if (output.contains("attempt_id"))
                            attempts.push_back(output.at("attempt_id"));
                    }
                    if (!output.value("success", false)) {
                        stopped = std::move(output);
                        return false;
                    }
                    return true;
                };
                const auto route = [&](const json& selected) {
                    const auto resolved =
                        std::ranges::find_if(fresh.at("plan").at("routing"), [&](const auto& r) {
                            return r.at("selection") == selected;
                        });
                    if (resolved == fresh.at("plan").at("routing").end())
                        throw std::runtime_error("Selected routing preflight is unavailable");
                    auto input = common(selection, selected.at("part_id"));
                    input.update(resolved->at("resolved_selection"));
                    if (!run("project_realization_preview_routing", input)) return false;
                    input["preview"] = phases.back().at("result").at("preview");
                    input["explicit_current_routing_approval"] = true;
                    return run("project_realization_apply_routing", input);
                };
                const auto mixer = [&](const json& input) {
                    auto prepared = input;
                    prepared["purpose"] = "adopt";
                    if (!run("project_realization_preview_static_mixer", prepared)) return false;
                    prepared.erase("purpose");
                    prepared["preview"] = phases.back().at("result").at("preview");
                    prepared["explicit_current_mixer_approval"] = true;
                    if (arguments.contains("explicit_set_wide_audible_approval"))
                        prepared["explicit_set_wide_audible_approval"] = true;
                    if (!run("project_realization_adopt_static_mixer", prepared)) return false;
                    prepared.erase("preview");
                    prepared.erase("explicit_current_mixer_approval");
                    prepared.erase("explicit_set_wide_audible_approval");
                    prepared["purpose"] = "update";
                    if (!run("project_realization_preview_static_mixer", prepared)) return false;
                    prepared.erase("purpose");
                    prepared["preview"] = phases.back().at("result").at("preview");
                    prepared["explicit_current_mixer_approval"] = true;
                    if (arguments.contains("explicit_set_wide_audible_approval"))
                        prepared["explicit_set_wide_audible_approval"] = true;
                    return run("project_realization_apply_static_mixer", prepared);
                };
                for (const auto& selected : selection.at("routing"))
                    if (selected.at("kind") == "adopt_group" && !route(selected))
                        return finish(stopped);
                for (const auto& selected : fresh.at("plan").at("parts")) {
                    auto input = common(selection, selected.at("part_id"));
                    if (!run(selected.at("clip_action") == "create" ? "project_realization_create"
                                                                    : "project_realization_update",
                             input))
                        return finish(stopped);
                }
                for (const auto& selected : selection.at("routing"))
                    if ((selected.at("kind") == "create_return" ||
                         selected.at("kind") == "adopt_return") &&
                        !route(selected))
                        return finish(stopped);
                for (const auto& selected : selection.at("parts")) {
                    auto input = common(selection, selected.at("part_id"));
                    if (selected.contains("source_selections")) {
                        input["selections"] = selected.at("source_selections");
                        if (!run("project_realization_author_timbre", input))
                            return finish(stopped);
                        input.erase("selections");
                    }
                    if (selected.contains("effect_selections")) {
                        input["effect_selections"] = selected.at("effect_selections");
                        if (!run("project_realization_author_effects", input))
                            return finish(stopped);
                    }
                }
                for (const auto* kind : {"output_type", "output_channel", "send_level"})
                    for (const auto& selected : selection.at("routing"))
                        if (selected.at("kind") == kind && !route(selected)) return finish(stopped);
                for (const auto& selected : selection.at("parts"))
                    if (selected.contains("static_mixer")) {
                        auto input = common(selection, selected.at("part_id"));
                        input.update(selected.at("static_mixer"));
                        if (!mixer(input)) return finish(stopped);
                    }
                for (const auto& selected : selection.at("parts")) {
                    if (!selected.contains("pan_lane")) continue;
                    auto input = common(selection, selected.at("part_id"));
                    input["lane_index"] = selected.at("pan_lane").at("lane_index");
                    if (selected.at("pan_lane").at("mode") == "absent") {
                        if (!run("project_realization_author_mix_lane", input))
                            return finish(stopped);
                    } else {
                        if (!run("project_realization_preview_mix_lane_replacement", input))
                            return finish(stopped);
                        input["preview"] = phases.back().at("result").at("preview");
                        input["explicit_selected_envelope_replacement"] = true;
                        input["allow_unsampled_selected_state_overwrite"] = true;
                        if (!run("project_realization_replace_mix_lane", input))
                            return finish(stopped);
                    }
                }
                for (const auto& id : selection.value("retire_part_ids", json::array())) {
                    auto input = common(selection, id);
                    input.erase("ppq");
                    input["domains"] = json::array({"mute"});
                    // Retirement is the same selected current control grant,
                    // followed by the existing mute-only historical writer.
                    input["purpose"] = "adopt";
                    if (!run("project_realization_preview_static_mixer", input))
                        return finish(stopped);
                    input.erase("purpose");
                    input["preview"] = phases.back().at("result").at("preview");
                    input["explicit_current_mixer_approval"] = true;
                    if (!run("project_realization_adopt_static_mixer", input))
                        return finish(stopped);
                    input = common(selection, id);
                    input.erase("ppq");
                    input["explicit_mute_retirement"] = true;
                    if (!run("project_realization_retire_part", input)) return finish(stopped);
                }
                if (selection.contains("song_settings_part_id")) {
                    auto input = common(selection, selection.at("song_settings_part_id"));
                    if (!run("project_realization_preview_song_settings", input))
                        return finish(stopped);
                    input["preview"] = phases.back().at("result").at("preview");
                    input["explicit_set_wide_approval"] = true;
                    if (!run("project_realization_apply_song_settings", input))
                        return finish(stopped);
                }
                auto verified = verify(selection);
                verified["selected_contract_completed"] = verified.value("success", false);
                return finish(std::move(verified));
            } catch (const std::exception& e) {
                return finish(failure(e.what()));
            }
        });
}
} // namespace sunny::infrastructure
