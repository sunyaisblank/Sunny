#include <charconv>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/detail/managed_routing.hpp>
#include <sunny/infrastructure/ableton/detail/managed_song_settings.hpp>
#include <sunny/infrastructure/ableton/detail/native_unit_numeric.hpp>

namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace managed_routing_detail {
using namespace managed_detail;
namespace {
constexpr std::size_t maximum_tracks = 256;
bool equal(const json& first, const json& second) {
    const auto left = managed_digest(first), right = managed_digest(second);
    return left && right && left == right;
}
bool finite(const json& value, bool actual = false) {
    return value.is_number() && (!actual || value.is_number_float()) &&
           std::isfinite(value.get<double>());
}
bool hash_id(const json& value) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    std::int64_t number = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
           text == std::to_string(number);
}
bool guard_valid(const json& value) {
    return recovery_fields(value,
                           {"content_fingerprint",
                            "note_identity_fingerprint",
                            "device_identity_fingerprint"}) &&
           recovery_hex(value.at("content_fingerprint"), 64) &&
           recovery_hex(value.at("note_identity_fingerprint"), 64) &&
           (value.at("device_identity_fingerprint").is_null() ||
            recovery_hex(value.at("device_identity_fingerprint"), 64));
}
json observation_guard(const json& value) {
    return {{"content_fingerprint", value.at("content_fingerprint")},
            {"note_identity_fingerprint", value.at("note_identity_fingerprint")},
            {"device_identity_fingerprint", value.value("device_identity_fingerprint", json{})}};
}
bool parameter_valid(const json& value) {
    return recovery_fields(value,
                           {"name",
                            "original_name",
                            "value",
                            "min",
                            "max",
                            "is_quantized",
                            "is_enabled",
                            "state",
                            "automation_state"}) &&
           recovery_text(value.at("name"), 1024, true) &&
           recovery_text(value.at("original_name"), 1024, true) &&
           finite(value.at("value"), true) && finite(value.at("min"), true) &&
           finite(value.at("max"), true) && value.at("min") <= value.at("value") &&
           value.at("value") <= value.at("max") && value.at("is_quantized").is_boolean() &&
           value.at("is_enabled").is_boolean() && note_integer(value.at("state"), 0, 2) &&
           note_integer(value.at("automation_state"), 0, 2);
}
bool mixer_valid(const json& value, std::size_t sends) {
    if (!recovery_fields(value,
                         {"panning_mode",
                          "crossfade_assign",
                          "volume",
                          "panning",
                          "track_activator",
                          "sends"}) ||
        !note_integer(value.at("panning_mode"), 0, 1) ||
        !note_integer(value.at("crossfade_assign"), 0, 2) || !parameter_valid(value.at("volume")) ||
        !parameter_valid(value.at("panning")) || !parameter_valid(value.at("track_activator")) ||
        !value.at("sends").is_array() || value.at("sends").size() != sends)
        return false;
    return std::ranges::all_of(value.at("sends"), parameter_valid);
}
bool route_valid(const json& value, bool channel, std::size_t tracks, std::size_t returns) {
    if (!recovery_fields(value, {"display_name", "identifier", "attached_target"}) ||
        !recovery_text(value.at("display_name"), 1024, true) || !hash_id(value.at("identifier")))
        return false;
    const auto& target = value.at("attached_target");
    if (target.is_null()) return true;
    if (channel || !recovery_fields(target, {"kind", "index"})) return false;
    if (target.at("kind") == "main") return target.at("index").is_null();
    if (target.at("kind") == "track")
        return tracks > 0 &&
               note_integer(target.at("index"), 0, static_cast<std::int64_t>(tracks - 1));
    return target.at("kind") == "return" && returns > 0 &&
           note_integer(target.at("index"), 0, static_cast<std::int64_t>(returns - 1));
}
bool routes_valid(const json& value, std::size_t tracks, std::size_t returns) {
    if (!recovery_fields(
            value, {"output_type", "output_channel", "available_types", "available_channels"}))
        return false;
    for (const bool channel : {false, true}) {
        const auto& current = value.at(channel ? "output_channel" : "output_type");
        const auto& options = value.at(channel ? "available_channels" : "available_types");
        if (!route_valid(current, channel, tracks, returns) || !options.is_array() ||
            options.empty() || options.size() > maximum_tracks)
            return false;
        std::set<std::string> ids;
        bool found = false;
        for (const auto& option : options) {
            if (!route_valid(option, channel, tracks, returns) ||
                !ids.insert(option.at("identifier").get<std::string>()).second)
                return false;
            found = found || equal(option, current);
        }
        if (!found) return false;
    }
    return true;
}
bool binding_valid(const json& context,
                   const json& project,
                   const json& binding,
                   const json& observation,
                   bool read_only_group_bootstrap = false) {
    auto base = observation;
    base.erase("device_identity");
    base.erase("device_identity_fingerprint");
    base.erase("group_authority");
    base.erase("group_authority_fingerprint");
    if (!recovery_fields(base,
                         {"track_index",
                          "slot_index",
                          "manifest",
                          "content_fingerprint",
                          "note_identity",
                          "note_identity_fingerprint",
                          "track_tag",
                          "clip_tag",
                          "structural_boundary_complete",
                          "content_boundary_complete",
                          "unavailable_reasons"}) ||
        !recovery_key(project) || !recovery_key(binding) || !recovery_context(context) ||
        !guard_valid(observation_guard(observation)) ||
        !note_identity_valid(observation.at("note_identity")) ||
        observation.at("note_identity").at("notes").size() > 65536 ||
        observation.at("note_identity").at("entire_clip_population_observed") != true ||
        observation.at("content_boundary_complete") != false ||
        observation.at("track_tag") !=
            "Sunny|" + project.get<std::string>() + "|" + binding.get<std::string>() + "|track" ||
        observation.at("clip_tag") !=
            "Sunny|" + project.get<std::string>() + "|" + binding.get<std::string>() + "|clip")
        return false;
    if (!group_supplement_valid(observation) ||
        (!read_only_group_bootstrap &&
         !group_touched_boundary(
             observation,
             ManagedBridgeContext{context.at("bridge_instance"), context.at("document_token")},
             project.get<std::string>(),
             binding.get<std::string>())))
        return false;
    const auto& manifest = observation.at("manifest");
    if (manifest.at("track").at("name") != observation.at("track_tag") ||
        manifest.at("clip").at("name") != observation.at("clip_tag") ||
        manifest.at("notes").size() > 65536 ||
        manifest.at("mixer").at("sends").size() > maximum_tracks)
        return false;
    for (const auto& route : manifest.at("routing"))
        if (!recovery_fields(route, {"display_name", "identifier"}) ||
            !recovery_text(route.at("display_name"), 1024, true) ||
            !recovery_text(route.at("identifier"), 2048, true))
            return false;
    const ManagedBindingReceipt candidate{
        {context.at("bridge_instance"), context.at("document_token")},
        project,
        binding,
        observation};
    return managed_binding_from_json(managed_binding_to_json(candidate)).has_value();
}
bool manifest_valid(const json& context,
                    const json& project,
                    const json& binding,
                    const json& manifest,
                    const json& fingerprint) {
    if (!recovery_hex(fingerprint, 64) ||
        managed_digest(manifest) != std::optional{fingerprint.get<std::string>()} ||
        !manifest.is_object() || !manifest.contains("notes") || !manifest.at("notes").is_array() ||
        manifest.at("notes").size() > 65536)
        return false;
    // Reuse the canonical managed manifest codec without inventing native IDs.
    const json observation{{"track_index", 0},
                           {"slot_index", 0},
                           {"manifest", manifest},
                           {"content_fingerprint", fingerprint},
                           {"track_tag", manifest.at("track").at("name")},
                           {"clip_tag", manifest.at("clip").at("name")},
                           {"structural_boundary_complete", false},
                           {"content_boundary_complete", false},
                           {"unavailable_reasons", json::array()}};
    const ManagedBindingReceipt candidate{
        {context.at("bridge_instance"), context.at("document_token")},
        project,
        binding,
        observation};
    return managed_binding_from_json(managed_binding_to_json(candidate)).has_value();
}
const json* anchor(const json& preview) {
    if (preview.contains("group_only")) return nullptr;
    for (const auto& item : preview.at("affected_bindings"))
        if (item.at("project_key") == preview.at("project_key") &&
            item.at("binding_key") == preview.at("binding_key"))
            return &item;
    return nullptr;
}
std::size_t anchor_index(const json& preview) {
    return anchor(preview)->at("track_index").get<std::size_t>();
}
bool selected_valid(const json& preview) {
    const auto& kind = preview.at("intent").at("kind");
    const auto& selected = preview.at("selected");
    const auto& before = preview.at("before");
    if (kind == "create_return" || kind == "adopt_return") {
        if (!recovery_fields(selected, {"return_tag"}) ||
            !recovery_text(selected.at("return_tag"), 1024, true))
            return false;
        if (kind == "create_return")
            return selected.at("return_tag") ==
                   "Sunny|" + preview.at("project_key").get<std::string>() + "|" +
                       preview.at("intent").at("aux_key").get<std::string>() + "|return";
        const auto index = preview.at("intent").at("return_index").get<std::size_t>();
        return index < before.at("song").at("return_tracks").size() &&
               selected.at("return_tag") ==
                   before.at("song").at("return_tracks").at(index).at("name");
    }
    if (kind == "adopt_group") {
        if (!recovery_fields(selected, {"group_name", "member_count", "members"}) ||
            !recovery_text(selected.at("group_name"), 1024, true) ||
            !selected.at("members").is_array() ||
            selected.at("member_count") != selected.at("members").size() ||
            selected.at("members").size() != preview.at("intent").at("member_binding_keys").size())
            return false;
        const auto group_index = preview.at("intent").at("track_index").get<std::size_t>();
        if (group_index >= before.at("song").at("tracks").size() ||
            selected.at("group_name") != before.at("song").at("tracks").at(group_index).at("name"))
            return false;
        std::set<std::size_t> indices;
        for (std::size_t i = 0; i < selected.at("members").size(); ++i) {
            const auto& member = selected.at("members").at(i);
            if (!recovery_fields(member, {"binding_key", "track_index", "name"}) ||
                member.at("binding_key") != preview.at("intent").at("member_binding_keys").at(i) ||
                !note_integer(
                    member.at("track_index"), 0, static_cast<std::int64_t>(maximum_tracks - 1)) ||
                !recovery_text(member.at("name"), 1024, true))
                return false;
            const auto index = member.at("track_index").get<std::size_t>();
            if (index == group_index || index >= before.at("song").at("tracks").size() ||
                !indices.insert(index).second ||
                member.at("name") != before.at("song").at("tracks").at(index).at("name") ||
                member.at("name") != "Sunny|" + preview.at("project_key").get<std::string>() + "|" +
                                         member.at("binding_key").get<std::string>() + "|track")
                return false;
        }
        return true;
    }
    const auto& row = before.at("mixers").at(anchor_index(preview));
    if (kind == "output_type" || kind == "output_channel") {
        if (!recovery_fields(selected, {"route"})) return false;
        const auto& route = selected.at("route");
        const auto& routes = row.at("routing");
        const auto& options =
            routes.at(kind == "output_type" ? "available_types" : "available_channels");
        if (!std::ranges::any_of(options,
                                 [&route](const auto& item) { return equal(route, item); }) ||
            route.at("identifier") != preview.at("intent").at("route_identifier"))
            return false;
        const auto& target = kind == "output_type" ? route.at("attached_target")
                                                   : routes.at("output_type").at("attached_target");
        return !target.is_null() &&
               (preview.at("intent").at("destination") == "main" ? target.at("kind") == "main"
                                                                 : target.at("kind") == "track");
    }
    if (!recovery_fields(selected,
                         {"return_index",
                          "return_tag",
                          "candidate",
                          "tap_policy_observed",
                          "logical_send_complete"}) ||
        !note_integer(
            selected.at("return_index"), 0, static_cast<std::int64_t>(maximum_tracks - 1)) ||
        !recovery_text(selected.at("return_tag"), 1024, true) ||
        selected.at("tap_policy_observed") != false ||
        selected.at("logical_send_complete") != false)
        return false;
    const auto index = selected.at("return_index").get<std::size_t>();
    if (index >= before.at("song").at("return_tracks").size() ||
        selected.at("return_tag") != before.at("song").at("return_tracks").at(index).at("name"))
        return false;
    auto candidate = selected.at("candidate");
    if (!candidate.is_object() || !candidate.contains("_supplemental_observation") ||
        !candidate.at("_supplemental_observation").is_null())
        return false;
    candidate.erase("_supplemental_observation");
    if (!recovery_fields(candidate,
                         {"schema_version",
                          "unit",
                          "target",
                          "display_tolerance",
                          "internal_value",
                          "display",
                          "display_value",
                          "display_increment",
                          "balance_full_scale",
                          "absolute_display_error",
                          "formatter_calls",
                          "samples",
                          "descriptor"}) ||
        !note_integer(candidate.at("schema_version"), 1, 1) || candidate.at("unit") != "Decibels")
        return false;
    const auto proof =
        native_unit_detail::numerical_display_proof(candidate,
                                                    native_unit_detail::Unit::Decibels,
                                                    preview.at("intent").at("level_db"),
                                                    preview.at("intent").at("tolerance_db"),
                                                    true);
    if (!proof) return false;
    const auto& parameter = row.at("mixer").at("sends").at(index);
    const auto& descriptor = candidate.at("descriptor");
    return descriptor.at("minimum") == parameter.at("min") &&
           descriptor.at("maximum") == parameter.at("max") &&
           descriptor.at("value") == parameter.at("value") &&
           descriptor.at("is_enabled") == parameter.at("is_enabled") &&
           descriptor.at("is_quantized") == parameter.at("is_quantized") &&
           descriptor.at("state") == parameter.at("state") &&
           descriptor.at("automation_state") == parameter.at("automation_state");
}
json expected_phases(const json& preview) {
    const auto& kind = preview.at("intent").at("kind");
    if (kind == "create_return") return json::array({"create_return_track", "return_name"});
    if (kind == "adopt_return" || kind == "adopt_group") return json::array();
    const auto& row = preview.at("before").at("mixers").at(anchor_index(preview));
    if (kind == "send_level")
        return row.at("mixer")
                           .at("sends")
                           .at(preview.at("selected").at("return_index").get<std::size_t>())
                           .at("value") ==
                       preview.at("selected").at("candidate").at("internal_value")
                   ? json::array()
                   : json::array({"send_value"});
    const auto name = kind == "output_type" ? "output_type" : "output_channel";
    return equal(row.at("routing").at(name), preview.at("selected").at("route"))
               ? json::array()
               : json::array(
                     {kind == "output_type" ? "output_routing_type" : "output_routing_channel"});
}
bool progress_valid(const json& value, const json& expected, bool complete) {
    if (!recovery_fields(value, {"started", "returned"}) || !value.at("started").is_array() ||
        !value.at("returned").is_array())
        return false;
    const auto& started = value.at("started");
    const auto& returned = value.at("returned");
    if (started.size() > expected.size() || returned.size() > started.size() ||
        started.size() - returned.size() > 1 ||
        (complete && (started != expected || returned != expected)))
        return false;
    for (std::size_t i = 0; i < started.size(); ++i)
        if (started.at(i) != expected.at(i)) return false;
    for (std::size_t i = 0; i < returned.size(); ++i)
        if (returned.at(i) != started.at(i)) return false;
    return true;
}
bool part_delta_preserved(const json& preview, const json& original, const json& observation) {
    if (!binding_valid(preview.at("context"),
                       original.at("project_key"),
                       original.at("binding_key"),
                       observation) ||
        observation.at("track_index") != original.at("track_index") ||
        observation.at("slot_index") != original.at("slot_index") ||
        observation.at("note_identity_fingerprint") !=
            original.at("guard").at("note_identity_fingerprint") ||
        observation.value("device_identity_fingerprint", json{}) !=
            original.at("guard").at("device_identity_fingerprint") ||
        !equal(observation.value("group_authority", json{}),
               original.at("before_group_authority")) ||
        observation.value("group_authority_fingerprint", json{}) !=
            original.at("before_group_authority_fingerprint"))
        return false;
    auto normalized = observation.at("manifest");
    const auto& before = original.at("before_manifest");
    const auto& kind = preview.at("intent").at("kind");
    const bool is_anchor = original.at("project_key") == preview.at("project_key") &&
                           original.at("binding_key") == preview.at("binding_key");
    if (kind == "create_return") {
        if (normalized.at("mixer").at("sends").size() != before.at("mixer").at("sends").size() + 1)
            return false;
        normalized["mixer"]["sends"].erase(normalized.at("mixer").at("sends").size() - 1);
    } else if (kind == "send_level" && is_anchor) {
        const auto index = preview.at("selected").at("return_index").get<std::size_t>();
        normalized["mixer"]["sends"][index]["value"] =
            before.at("mixer").at("sends").at(index).at("value");
    } else if ((kind == "output_type" || kind == "output_channel") && is_anchor) {
        normalized["routing"]["output_routing_channel"] =
            before.at("routing").at("output_routing_channel");
        if (kind == "output_type")
            normalized["routing"]["output_routing_type"] =
                before.at("routing").at("output_routing_type");
    }
    return equal(normalized, before);
}
bool send_readback_valid(const json& preview, const json& after, const json& readback) {
    if (!recovery_fields(readback,
                         {"internal_value",
                          "display",
                          "display_value",
                          "absolute_display_error",
                          "negative_infinity",
                          "matches_intent"}) ||
        !finite(readback.at("internal_value"), true) ||
        !recovery_text(readback.at("display"), 80) ||
        !readback.at("negative_infinity").is_boolean() ||
        !readback.at("matches_intent").is_boolean())
        return false;
    const auto reading =
        native_unit_detail::display_reading(readback.at("display").get_ref<const std::string&>(),
                                            native_unit_detail::Unit::Decibels,
                                            true);
    if (!reading || readback.at("negative_infinity") != reading->negative_infinity) return false;
    const auto index = preview.at("selected").at("return_index").get<std::size_t>();
    if (readback.at("internal_value") !=
        after.at("mixers").at(anchor_index(preview)).at("mixer").at("sends").at(index).at("value"))
        return false;
    if (reading->negative_infinity)
        return readback.at("display_value").is_null() &&
               readback.at("absolute_display_error").is_null() &&
               readback.at("matches_intent") == false;
    const auto decimal =
        native_unit_detail::decimal_display_comparison(*reading,
                                                       preview.at("intent").at("level_db"),
                                                       preview.at("intent").at("tolerance_db"),
                                                       1.0,
                                                       110);
    return decimal && finite(readback.at("display_value"), true) &&
           finite(readback.at("absolute_display_error"), true) &&
           native_unit_detail::arithmetic_equal(decimal->display_value,
                                                readback.at("display_value")) &&
           native_unit_detail::arithmetic_equal(decimal->absolute_display_error,
                                                readback.at("absolute_display_error")) &&
           readback.at("matches_intent") == decimal->within_tolerance;
}
bool desired_match(const json& preview, const json& after, const json& readback) {
    const auto& kind = preview.at("intent").at("kind");
    if (kind == "adopt_group") return true;
    if (kind == "create_return" || kind == "adopt_return") {
        const auto index = kind == "create_return"
                               ? after.at("song").at("return_tracks").size() - 1
                               : preview.at("intent").at("return_index").get<std::size_t>();
        return after.at("song").at("return_tracks").at(index).at("name") ==
               preview.at("selected").at("return_tag");
    }
    if (kind == "send_level") return readback.at("matches_intent").get<bool>();
    const auto& routing = after.at("mixers").at(anchor_index(preview)).at("routing");
    const auto& old_target = preview.at("before")
                                 .at("mixers")
                                 .at(anchor_index(preview))
                                 .at("routing")
                                 .at("output_type")
                                 .at("attached_target");
    const auto& intended_target = kind == "output_type"
                                      ? preview.at("selected").at("route").at("attached_target")
                                      : old_target;
    return equal(routing.at(kind == "output_type" ? "output_type" : "output_channel"),
                 preview.at("selected").at("route")) &&
           equal(routing.at("output_type").at("attached_target"), intended_target);
}
} // namespace
bool intent_valid(const json& value) {
    try {
        if (!value.is_object() || !value.contains("kind")) return false;
        const auto& kind = value.at("kind");
        if (kind == "create_return")
            return recovery_fields(value, {"kind", "aux_key"}) && recovery_key(value.at("aux_key"));
        if (kind == "adopt_return")
            return recovery_fields(value, {"kind", "aux_key", "return_index"}) &&
                   recovery_key(value.at("aux_key")) &&
                   note_integer(value.at("return_index"), 0, INT32_MAX);
        if (kind == "adopt_group") {
            if (!recovery_fields(value,
                                 {"kind", "group_key", "track_index", "member_binding_keys"}) ||
                !recovery_key(value.at("group_key")) ||
                !note_integer(value.at("track_index"), 0, INT32_MAX) ||
                !value.at("member_binding_keys").is_array() ||
                value.at("member_binding_keys").empty() ||
                value.at("member_binding_keys").size() > maximum_tracks)
                return false;
            std::set<std::string> keys;
            for (const auto& key : value.at("member_binding_keys"))
                if (!recovery_key(key) || !keys.insert(key.get<std::string>()).second) return false;
            return true;
        }
        if (kind == "output_type" || kind == "output_channel")
            return recovery_fields(value,
                                   {"kind", "destination", "group_key", "route_identifier"}) &&
                   hash_id(value.at("route_identifier")) &&
                   ((value.at("destination") == "main" && value.at("group_key").is_null()) ||
                    (value.at("destination") == "group" && recovery_key(value.at("group_key"))));
        return kind == "send_level" &&
               recovery_fields(
                   value, {"kind", "aux_key", "level_db", "tolerance_db", "requested_pre_fader"}) &&
               recovery_key(value.at("aux_key")) && finite(value.at("level_db")) &&
               finite(value.at("tolerance_db")) && value.at("tolerance_db").get<double>() >= 0.0 &&
               value.at("requested_pre_fader").is_boolean();
    } catch (const json::exception&) {
        return false;
    }
}
bool frame_valid(const json& value) {
    try {
        if (!recovery_fields(value, {"song", "mixers"}) ||
            !managed_song_detail::snapshot_valid(value.at("song")) ||
            !value.at("mixers").is_array())
            return false;
        const auto tracks = value.at("song").at("tracks").size();
        const auto returns = value.at("song").at("return_tracks").size();
        if (tracks > maximum_tracks || returns > maximum_tracks ||
            value.at("mixers").size() != tracks + returns)
            return false;
        for (std::size_t index = 0; index < value.at("mixers").size(); ++index) {
            const auto& row = value.at("mixers").at(index);
            const bool player = index < tracks;
            const auto ordinal = player ? index : index - tracks;
            if (!recovery_fields(row,
                                 {"kind", "index", "name", "mute", "solo", "mixer", "routing"}) ||
                row.at("kind") != (player ? "track" : "return") ||
                !note_integer(row.at("index"), 0, static_cast<std::int64_t>(maximum_tracks - 1)) ||
                row.at("index") != ordinal || !recovery_text(row.at("name"), 1024, true) ||
                !row.at("mute").is_boolean() || !row.at("solo").is_boolean() ||
                row.at("name") != value.at("song")
                                      .at(player ? "tracks" : "return_tracks")
                                      .at(ordinal)
                                      .at("name") ||
                !mixer_valid(row.at("mixer"), returns) ||
                !routes_valid(row.at("routing"), tracks, returns))
                return false;
        }
        return canonical_managed_bytes(value).has_value();
    } catch (const json::exception&) {
        return false;
    }
}
bool readonly_body_valid(const json& value, bool inspection) {
    try {
        auto preview = value;
        if (inspection) {
            if (!preview.is_object() || !preview.contains("authority_origin") ||
                !preview.contains("native_mutation_started") ||
                !preview.contains("send_readback") || preview.at("authority_origin") != "none" ||
                !preview.at("native_mutation_started").is_boolean() ||
                preview.at("native_mutation_started") != false ||
                preview.contains("preview_token") || !preview.contains("intent") ||
                preview.at("intent").value("kind", "") != "send_level")
                return false;
            preview.erase("authority_origin");
            preview.erase("native_mutation_started");
            preview.erase("send_readback");
        }
        const bool group = preview.contains("group_only");
        if (inspection && group) return false;
        if (!(group ? recovery_fields(preview,
                                      {"schema_version",
                                       "preview_token",
                                       "group_only",
                                       "context",
                                       "project_key",
                                       "binding_key",
                                       "intent",
                                       "selector",
                                       "binding_guard",
                                       "before",
                                       "selected",
                                       "scope"})
                    : recovery_fields(
                          [&preview] {
                              auto fields = preview;
                              fields.erase("preview_token");
                              return fields;
                          }(),
                          {"schema_version",
                           "context",
                           "project_key",
                           "binding_key",
                           "intent",
                           "binding_guard",
                           "before",
                           "affected_bindings",
                           "selected",
                           "scope"})) ||
            !note_integer(preview.at("schema_version"), 1, 1) ||
            (!inspection && !recovery_hex(preview.at("preview_token"), 32)) ||
            !recovery_context(preview.at("context")) || !recovery_key(preview.at("project_key")) ||
            !recovery_key(preview.at("binding_key")) || !intent_valid(preview.at("intent")) ||
            !guard_valid(preview.at("binding_guard")) || !frame_valid(preview.at("before")))
            return false;
        if (group) {
            if (preview.at("group_only") != true ||
                preview.at("intent").at("kind") != "adopt_group" ||
                !recovery_fields(preview.at("selector"), {"track_index", "slot_index"}) ||
                !recovery_selector(preview.at("selector")) ||
                preview.at("scope") != json{{"group_membership_mutated", false},
                                            {"historical_identity_proven", false},
                                            {"part_authority_granted", false},
                                            {"device_authority_granted", false}})
                return false;
            const auto& keys = preview.at("intent").at("member_binding_keys");
            if (std::ranges::none_of(
                    keys, [&preview](const auto& key) { return key == preview.at("binding_key"); }))
                return false;
            if (!selected_valid(preview)) return false;
            for (const auto& member : preview.at("selected").at("members"))
                if (member.at("binding_key") == preview.at("binding_key"))
                    return member.at("track_index") == preview.at("selector").at("track_index");
            return false;
        }
        if (preview.at("intent").at("kind") == "adopt_group" ||
            preview.at("scope") != json{{"affects_send_population",
                                         preview.at("intent").at("kind") == "create_return"},
                                        {"tap_policy_observed", false},
                                        {"group_membership_mutated", false},
                                        {"historical_identity_proven", false}} ||
            !preview.at("affected_bindings").is_array() ||
            preview.at("affected_bindings").empty() ||
            preview.at("affected_bindings").size() > maximum_tracks)
            return false;
        std::set<std::pair<std::string, std::string>> keys;
        std::set<std::size_t> tracks;
        for (const auto& binding : preview.at("affected_bindings")) {
            if (!recovery_fields(binding,
                                 {"project_key",
                                  "binding_key",
                                  "guard",
                                  "track_index",
                                  "slot_index",
                                  "before_manifest",
                                  "before_group_authority",
                                  "before_group_authority_fingerprint"}) ||
                !recovery_key(binding.at("project_key")) ||
                !recovery_key(binding.at("binding_key")) || !guard_valid(binding.at("guard")) ||
                !note_integer(
                    binding.at("track_index"), 0, static_cast<std::int64_t>(maximum_tracks - 1)) ||
                !note_integer(binding.at("slot_index"), 0, INT32_MAX) ||
                !keys.emplace(binding.at("project_key"), binding.at("binding_key")).second ||
                !tracks.insert(binding.at("track_index").get<std::size_t>()).second ||
                !manifest_valid(preview.at("context"),
                                binding.at("project_key"),
                                binding.at("binding_key"),
                                binding.at("before_manifest"),
                                binding.at("guard").at("content_fingerprint")))
                return false;
            json group_observation{
                {"manifest", binding.at("before_manifest")},
                {"track_index", binding.at("track_index")},
                {"track_tag", binding.at("before_manifest").at("track").at("name")},
                {"clip_tag", binding.at("before_manifest").at("clip").at("name")},
                {"structural_boundary_complete", false},
                {"content_boundary_complete", false}};
            if (!binding.at("before_group_authority").is_null() ||
                !binding.at("before_group_authority_fingerprint").is_null()) {
                group_observation["group_authority"] = binding.at("before_group_authority");
                group_observation["group_authority_fingerprint"] =
                    binding.at("before_group_authority_fingerprint");
            }
            if (!group_touched_boundary(
                    group_observation,
                    ManagedBridgeContext{preview.at("context").at("bridge_instance"),
                                         preview.at("context").at("document_token")},
                    binding.at("project_key"),
                    binding.at("binding_key")))
                return false;
            const auto index = binding.at("track_index").get<std::size_t>();
            const auto slot = binding.at("slot_index").get<std::size_t>();
            const auto& native_tracks = preview.at("before").at("song").at("tracks");
            if (index >= native_tracks.size() ||
                slot >= native_tracks.at(index).at("clip_slots").size() ||
                native_tracks.at(index).at("clip_slots").at(slot).at("has_clip") != true ||
                native_tracks.at(index).at("name") !=
                    binding.at("before_manifest").at("track").at("name") ||
                native_tracks.at(index).at("clip_slots").at(slot).at("clip").at("name") !=
                    binding.at("before_manifest").at("clip").at("name") ||
                !equal(preview.at("before").at("mixers").at(index).at("mixer"),
                       binding.at("before_manifest").at("mixer")))
                return false;
        }
        const auto* owner = anchor(preview);
        return owner && equal(owner->at("guard"), preview.at("binding_guard")) &&
               selected_valid(preview);
    } catch (const json::exception&) {
        return false;
    }
}
bool preview_valid(const json& preview) {
    return readonly_body_valid(preview, false);
}
bool send_inspection_valid(const json& value) {
    try {
        return readonly_body_valid(value, true) &&
               send_readback_valid(value, value.at("before"), value.at("send_readback"));
    } catch (const json::exception&) {
        return false;
    }
}
bool request_valid(std::string_view method, const json& payload) {
    try {
        if (method == candidates_method)
            return recovery_fields(payload,
                                   {"document_token",
                                    "project_key",
                                    "binding_key",
                                    "expected_content_fingerprint",
                                    "expected_note_identity_fingerprint"}) &&
                   recovery_key(payload.at("document_token")) &&
                   recovery_key(payload.at("project_key")) &&
                   recovery_key(payload.at("binding_key")) &&
                   recovery_hex(payload.at("expected_content_fingerprint"), 64) &&
                   recovery_hex(payload.at("expected_note_identity_fingerprint"), 64);
        if (method == group_preview_method)
            return recovery_fields(
                       payload,
                       {"document_token", "project_key", "binding_key", "intent", "selector"}) &&
                   recovery_key(payload.at("document_token")) &&
                   recovery_key(payload.at("project_key")) &&
                   recovery_key(payload.at("binding_key")) && intent_valid(payload.at("intent")) &&
                   payload.at("intent").at("kind") == "adopt_group" &&
                   std::ranges::any_of(
                       payload.at("intent").at("member_binding_keys"),
                       [&payload](const auto& key) { return key == payload.at("binding_key"); }) &&
                   recovery_fields(payload.at("selector"), {"track_index", "slot_index"}) &&
                   recovery_selector(payload.at("selector"));
        if (!((method == preview_method || method == send_inspection_method)
                  ? recovery_fields(payload,
                                    {"document_token",
                                     "project_key",
                                     "binding_key",
                                     "expected_content_fingerprint",
                                     "expected_note_identity_fingerprint",
                                     "intent"})
                  : method == apply_method &&
                        recovery_fields(payload,
                                        {"document_token",
                                         "project_key",
                                         "binding_key",
                                         "expected_content_fingerprint",
                                         "expected_note_identity_fingerprint",
                                         "intent",
                                         "operation_id",
                                         "preview_token",
                                         "preview_fingerprint",
                                         "approved_preview",
                                         "explicit_current_routing_approval"})) ||
            !recovery_key(payload.at("document_token")) ||
            !recovery_key(payload.at("project_key")) || !recovery_key(payload.at("binding_key")) ||
            !recovery_hex(payload.at("expected_content_fingerprint"), 64) ||
            !recovery_hex(payload.at("expected_note_identity_fingerprint"), 64) ||
            !intent_valid(payload.at("intent")))
            return false;
        if (method == send_inspection_method)
            return payload.at("intent").at("kind") == "send_level";
        if (method == preview_method) return payload.at("intent").at("kind") != "adopt_group";
        const auto& preview = payload.at("approved_preview");
        return recovery_key(payload.at("operation_id")) &&
               recovery_hex(payload.at("preview_token"), 32) &&
               recovery_hex(payload.at("preview_fingerprint"), 64) &&
               payload.at("explicit_current_routing_approval") == true && preview_valid(preview) &&
               preview.at("context").at("document_token") == payload.at("document_token") &&
               preview.at("project_key") == payload.at("project_key") &&
               preview.at("binding_key") == payload.at("binding_key") &&
               preview.at("preview_token") == payload.at("preview_token") &&
               equal(preview.at("intent"), payload.at("intent")) &&
               preview.at("binding_guard").at("content_fingerprint") ==
                   payload.at("expected_content_fingerprint") &&
               preview.at("binding_guard").at("note_identity_fingerprint") ==
                   payload.at("expected_note_identity_fingerprint") &&
               managed_digest(preview) ==
                   std::optional{payload.at("preview_fingerprint").get<std::string>()};
    } catch (const json::exception&) {
        return false;
    }
}
bool observed_state_preserved(const json& preview, const json& after) {
    try {
        if (!preview_valid(preview) || !frame_valid(after)) return false;
        auto normalized = after;
        const auto& before = preview.at("before");
        const auto& kind = preview.at("intent").at("kind");
        if (kind == "create_return") {
            const auto old_returns = before.at("song").at("return_tracks").size();
            if (after.at("song").at("return_tracks").size() != old_returns + 1 ||
                after.at("mixers").size() != before.at("mixers").size() + 1)
                return false;
            normalized["song"]["return_tracks"].erase(old_returns);
            normalized["mixers"].erase(normalized.at("mixers").size() - 1);
            for (auto& row : normalized["mixers"]) {
                row["mixer"]["sends"].erase(old_returns);
                auto types = json::array();
                for (const auto& route : row.at("routing").at("available_types")) {
                    const auto& target = route.at("attached_target");
                    if (!target.is_null() && target.at("kind") == "return" &&
                        target.at("index") == old_returns)
                        continue;
                    types.push_back(route);
                }
                if (row.at("routing").at("available_types").size() > types.size() + 1) return false;
                row["routing"]["available_types"] = std::move(types);
            }
        } else if (kind == "send_level") {
            const auto index = preview.at("selected").at("return_index").get<std::size_t>();
            normalized["mixers"][anchor_index(preview)]["mixer"]["sends"][index]["value"] =
                before.at("mixers")
                    .at(anchor_index(preview))
                    .at("mixer")
                    .at("sends")
                    .at(index)
                    .at("value");
        } else if (kind == "output_type" || kind == "output_channel") {
            auto& selected = normalized["mixers"][anchor_index(preview)]["routing"];
            const auto& original = before.at("mixers").at(anchor_index(preview)).at("routing");
            selected["output_channel"] = original.at("output_channel");
            if (kind == "output_type") {
                selected["output_type"] = original.at("output_type");
                selected["available_channels"] = original.at("available_channels");
            }
        }
        return equal(normalized, before);
    } catch (const json::exception&) {
        return false;
    }
}
bool result_matches_request(const json& payload, const json& result) {
    try {
        if (!request_valid(apply_method, payload) || !result.is_object()) return false;
        const auto& preview = payload.at("approved_preview");
        if (preview.contains("group_only")) {
            if (!recovery_fields(result, {"group_adoption"})) return false;
            const auto& value = result.at("group_adoption");
            return recovery_fields(value,
                                   {"schema_version",
                                    "approved_preview",
                                    "after",
                                    "progress",
                                    "current_group_and_members_match",
                                    "authority_origin",
                                    "part_authority_granted",
                                    "device_authority_granted",
                                    "historical_identity_proven"}) &&
                   note_integer(value.at("schema_version"), 1, 1) &&
                   equal(value.at("approved_preview"), preview) && frame_valid(value.at("after")) &&
                   equal(value.at("after"), preview.at("before")) &&
                   progress_valid(value.at("progress"), json::array(), true) &&
                   value.at("current_group_and_members_match") == true &&
                   value.at("authority_origin") == "explicit_current_group_adoption" &&
                   value.at("part_authority_granted") == false &&
                   value.at("device_authority_granted") == false &&
                   value.at("historical_identity_proven") == false;
        }
        if (!result.contains("routing")) return false;
        auto base = result;
        base.erase("routing");
        const auto& routing = result.at("routing");
        if (!binding_valid(preview.at("context"),
                           preview.at("project_key"),
                           preview.at("binding_key"),
                           base) ||
            !recovery_fields(routing,
                             {"schema_version",
                              "approved_preview",
                              "after",
                              "progress",
                              "desired_match",
                              "untouched_observed_state_preserved",
                              "affected_observations",
                              "authority_origin",
                              "tap_policy_observed",
                              "logical_send_complete",
                              "send_readback"}) ||
            !note_integer(routing.at("schema_version"), 1, 1) ||
            !equal(routing.at("approved_preview"), preview) || !frame_valid(routing.at("after")) ||
            !progress_valid(routing.at("progress"), expected_phases(preview), true) ||
            !routing.at("desired_match").is_boolean() ||
            !routing.at("untouched_observed_state_preserved").is_boolean() ||
            routing.at("untouched_observed_state_preserved") !=
                observed_state_preserved(preview, routing.at("after")) ||
            routing.at("untouched_observed_state_preserved") != true ||
            routing.at("tap_policy_observed") != false ||
            routing.at("logical_send_complete") != false ||
            routing.at("authority_origin") != (preview.at("intent").at("kind") == "adopt_return"
                                                   ? "explicit_current_adoption"
                                                   : "explicit_current_routing") ||
            !routing.at("affected_observations").is_array() ||
            routing.at("affected_observations").size() != preview.at("affected_bindings").size())
            return false;
        bool owner_found = false;
        for (std::size_t i = 0; i < routing.at("affected_observations").size(); ++i) {
            const auto& observed = routing.at("affected_observations").at(i);
            const auto& original = preview.at("affected_bindings").at(i);
            if (!recovery_fields(observed, {"project_key", "binding_key", "observation"}) ||
                observed.at("project_key") != original.at("project_key") ||
                observed.at("binding_key") != original.at("binding_key") ||
                !part_delta_preserved(preview, original, observed.at("observation")))
                return false;
            const auto& observation = observed.at("observation");
            const auto index = observation.at("track_index").get<std::size_t>();
            if (!equal(observation.at("manifest").at("mixer"),
                       routing.at("after").at("mixers").at(index).at("mixer")) ||
                observation.at("manifest")
                        .at("routing")
                        .at("output_routing_type")
                        .at("display_name") != routing.at("after")
                                                   .at("mixers")
                                                   .at(index)
                                                   .at("routing")
                                                   .at("output_type")
                                                   .at("display_name") ||
                observation.at("manifest")
                        .at("routing")
                        .at("output_routing_channel")
                        .at("display_name") != routing.at("after")
                                                   .at("mixers")
                                                   .at(index)
                                                   .at("routing")
                                                   .at("output_channel")
                                                   .at("display_name"))
                return false;
            if (observed.at("project_key") == preview.at("project_key") &&
                observed.at("binding_key") == preview.at("binding_key")) {
                if (!equal(observation, base)) return false;
                owner_found = true;
            }
        }
        const auto& readback = routing.at("send_readback");
        if (preview.at("intent").at("kind") == "send_level") {
            if (!send_readback_valid(preview, routing.at("after"), readback)) return false;
        } else if (!readback.is_null())
            return false;
        return owner_found &&
               routing.at("desired_match") == desired_match(preview, routing.at("after"), readback);
    } catch (const json::exception&) {
        return false;
    }
}
bool acknowledged_native_start_valid(const json& payload, const json& result, bool native_started) {
    return result_matches_request(payload, result) &&
           native_started == !expected_phases(payload.at("approved_preview")).empty();
}
bool partial_valid(const json& payload, const json& journal) {
    try {
        if (!request_valid(apply_method, payload) || !journal.is_object() ||
            !journal.contains("native_mutation_started") ||
            !journal.at("native_mutation_started").is_boolean())
            return false;
        const bool acknowledged = journal.value("outcome", json{}) == "acknowledged";
        if (!journal.contains("routing_progress"))
            return !acknowledged && journal.at("native_mutation_started") == false &&
                   !journal.contains("routing_partial") &&
                   !journal.contains("routing_partial_unavailable");
        const auto& progress = journal.at("routing_progress");
        if (!progress_valid(
                progress, expected_phases(payload.at("approved_preview")), acknowledged) ||
            journal.at("native_mutation_started") != !progress.at("started").empty())
            return false;
        if (acknowledged) {
            if (!journal.contains("result") || journal.contains("routing_partial") ||
                journal.contains("routing_partial_unavailable") ||
                !acknowledged_native_start_valid(
                    payload, journal.at("result"), journal.at("native_mutation_started")))
                return false;
            const auto& result = journal.at("result");
            return equal(progress,
                         result.at(result.contains("group_adoption") ? "group_adoption" : "routing")
                             .at("progress"));
        }
        if (journal.contains("routing_partial") &&
            (!frame_valid(journal.at("routing_partial")) || progress.at("started").empty()))
            return false;
        if (journal.contains("routing_partial_unavailable") &&
            (!recovery_text(journal.at("routing_partial_unavailable"), 4096, true) ||
             progress.at("started").empty()))
            return false;
        return !(journal.contains("routing_partial") &&
                 journal.contains("routing_partial_unavailable"));
    } catch (const json::exception&) {
        return false;
    }
}
} // namespace managed_routing_detail
namespace {
json context_json(const ManagedBridgeContext& context) {
    return {{"bridge_instance", context.bridge_instance},
            {"document_token", context.document_token}};
}
} // namespace
Result<LomRequest> make_managed_routing_candidates_request(const ManagedBridgeContext& context,
                                                           const ManagedBindingReceipt& binding) {
    if (!managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !binding.observation.contains("note_identity_fingerprint") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{{"document_token", context.document_token},
                 {"project_key", binding.project_key},
                 {"binding_key", binding.binding_key},
                 {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
                 {"expected_note_identity_fingerprint",
                  binding.observation.at("note_identity_fingerprint")}};
    if (!managed_routing_detail::request_valid(managed_routing_detail::candidates_method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{managed_routing_detail::candidates_method}, {payload});
}
Result<json> parse_managed_routing_candidates(const LomRequest& request,
                                              const ManagedBridgeContext& context,
                                              const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != managed_routing_detail::candidates_method ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args.front()) ||
            !recovery_fields(value,
                             {"context",
                              "project_key",
                              "binding_key",
                              "observation",
                              "frame",
                              "native_mutation_started",
                              "authority_origin"}) ||
            value.at("context") != context_json(context) ||
            value.at("native_mutation_started") != false || value.at("authority_origin") != "none")
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args.front());
        const auto& observed = value.at("observation");
        const auto& frame = value.at("frame");
        if (!managed_routing_detail::request_valid(managed_routing_detail::candidates_method,
                                                   payload) ||
            payload.at("document_token") != context.document_token ||
            value.at("project_key") != payload.at("project_key") ||
            value.at("binding_key") != payload.at("binding_key") ||
            !managed_routing_detail::binding_valid(
                value.at("context"), value.at("project_key"), value.at("binding_key"), observed) ||
            observed.at("content_fingerprint") != payload.at("expected_content_fingerprint") ||
            observed.at("note_identity_fingerprint") !=
                payload.at("expected_note_identity_fingerprint") ||
            !managed_routing_detail::frame_valid(frame))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto index = observed.at("track_index").get<std::size_t>();
        const auto slot = observed.at("slot_index").get<std::size_t>();
        const auto& tracks = frame.at("song").at("tracks");
        if (index >= tracks.size() || slot >= tracks.at(index).at("clip_slots").size())
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& selected = tracks.at(index).at("clip_slots").at(slot);
        const auto& row = frame.at("mixers").at(index);
        const auto& manifest = observed.at("manifest");
        if (selected.at("has_clip") != true ||
            tracks.at(index).at("name") != observed.at("track_tag") ||
            selected.at("clip").at("name") != observed.at("clip_tag") ||
            !managed_routing_detail::equal(row.at("mixer"), manifest.at("mixer")) ||
            row.at("mute") != manifest.at("track").at("mute") ||
            row.at("solo") != manifest.at("track").at("solo"))
            return std::unexpected(ErrorCode::ProtocolError);
        for (const auto& [field, actual] : selected.at("clip").items())
            if (!manifest.at("clip").contains(field) || manifest.at("clip").at(field) != actual)
                return std::unexpected(ErrorCode::ProtocolError);
        for (const auto& [native, semantic] :
             {std::pair{"output_type", "output_routing_type"},
              std::pair{"output_channel", "output_routing_channel"}})
            if (row.at("routing").at(native).at("display_name") !=
                manifest.at("routing").at(semantic).at("display_name"))
                return std::unexpected(ErrorCode::ProtocolError);
        return value;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
namespace {
Result<LomRequest> routing_read_request(const ManagedBridgeContext& context,
                                        const ManagedBindingReceipt& binding,
                                        const json& intent,
                                        std::string_view method) {
    if (!managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !binding.observation.contains("note_identity_fingerprint") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{
        {"document_token", context.document_token},
        {"project_key", binding.project_key},
        {"binding_key", binding.binding_key},
        {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
        {"expected_note_identity_fingerprint", binding.observation.at("note_identity_fingerprint")},
        {"intent", intent}};
    if (!managed_routing_detail::request_valid(method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(LomPaths::song(), std::string{method}, {payload});
}
} // namespace
Result<LomRequest> make_managed_routing_preview_request(const ManagedBridgeContext& context,
                                                        const ManagedBindingReceipt& binding,
                                                        const json& intent) {
    return routing_read_request(context, binding, intent, managed_routing_detail::preview_method);
}
Result<LomRequest> make_managed_send_inspection_request(const ManagedBridgeContext& context,
                                                        const ManagedBindingReceipt& binding,
                                                        const json& intent) {
    return routing_read_request(
        context, binding, intent, managed_routing_detail::send_inspection_method);
}
Result<json> parse_managed_send_inspection(const LomRequest& request,
                                           const ManagedBridgeContext& context,
                                           const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != managed_routing_detail::send_inspection_method ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args.front()) ||
            !recovery_fields(value, {"outcome", "inspection", "observation"}) ||
            value.at("outcome") != "observed" ||
            !managed_routing_detail::send_inspection_valid(value.at("inspection")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args.front());
        const auto& body = value.at("inspection");
        const auto& observation = value.at("observation");
        if (!managed_routing_detail::request_valid(request.property_or_method, payload) ||
            payload.at("document_token") != context.document_token ||
            body.at("context") != context_json(context) ||
            body.at("project_key") != payload.at("project_key") ||
            body.at("binding_key") != payload.at("binding_key") ||
            !managed_routing_detail::equal(body.at("intent"), payload.at("intent")) ||
            body.at("binding_guard").at("content_fingerprint") !=
                payload.at("expected_content_fingerprint") ||
            body.at("binding_guard").at("note_identity_fingerprint") !=
                payload.at("expected_note_identity_fingerprint") ||
            !managed_routing_detail::binding_valid(body.at("context"),
                                                   body.at("project_key"),
                                                   body.at("binding_key"),
                                                   observation,
                                                   false) ||
            managed_routing_detail::observation_guard(observation) != body.at("binding_guard") ||
            !managed_routing_detail::equal(
                managed_routing_detail::anchor(body)->at("before_manifest"),
                observation.at("manifest")))
            return std::unexpected(ErrorCode::ProtocolError);
        return value;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<LomRequest> make_managed_group_preview_request(const ManagedBridgeContext& context,
                                                      const std::string& project,
                                                      const std::string& binding,
                                                      const json& selector,
                                                      const json& intent) {
    json payload{{"document_token", context.document_token},
                 {"project_key", project},
                 {"binding_key", binding},
                 {"selector", selector},
                 {"intent", intent}};
    if (!managed_detail::recovery_context(context_json(context)) ||
        !managed_routing_detail::request_valid(managed_routing_detail::group_preview_method,
                                               payload))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{managed_routing_detail::group_preview_method}, {payload});
}
Result<ManagedRoutingPreview> parse_managed_routing_preview(const LomRequest& request,
                                                            const ManagedBridgeContext& context,
                                                            const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            (request.property_or_method != managed_routing_detail::preview_method &&
             request.property_or_method != managed_routing_detail::group_preview_method) ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args.front()) ||
            !recovery_fields(value,
                             {"preview_token",
                              "preview_fingerprint",
                              "preview",
                              "observation",
                              "native_mutation_started",
                              "authority_origin"}) ||
            !recovery_hex(value.at("preview_token"), 32) ||
            !recovery_hex(value.at("preview_fingerprint"), 64) ||
            value.at("native_mutation_started") != false ||
            value.at("authority_origin") != "none" ||
            !managed_routing_detail::preview_valid(value.at("preview")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args.front());
        const auto& preview = value.at("preview");
        if (!managed_routing_detail::request_valid(request.property_or_method, payload) ||
            preview.at("context") != context_json(context) ||
            preview.at("project_key") != payload.at("project_key") ||
            preview.at("binding_key") != payload.at("binding_key") ||
            preview.at("preview_token") != value.at("preview_token") ||
            managed_digest(preview) !=
                std::optional{value.at("preview_fingerprint").get<std::string>()} ||
            managed_digest(preview.at("intent")) != managed_digest(payload.at("intent")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& observation = value.at("observation");
        const bool group =
            request.property_or_method == managed_routing_detail::group_preview_method;
        if (!managed_routing_detail::binding_valid(preview.at("context"),
                                                   preview.at("project_key"),
                                                   preview.at("binding_key"),
                                                   observation,
                                                   group) ||
            managed_routing_detail::observation_guard(observation) != preview.at("binding_guard") ||
            group != preview.contains("group_only"))
            return std::unexpected(ErrorCode::ProtocolError);
        if (group) {
            if (!managed_routing_detail::equal(preview.at("selector"), payload.at("selector")) ||
                observation.at("track_index") != preview.at("selector").at("track_index") ||
                observation.at("slot_index") != preview.at("selector").at("slot_index") ||
                observation.at("manifest").at("track").at("is_grouped") != true)
                return std::unexpected(ErrorCode::ProtocolError);
        } else if (payload.at("expected_content_fingerprint") !=
                       preview.at("binding_guard").at("content_fingerprint") ||
                   payload.at("expected_note_identity_fingerprint") !=
                       preview.at("binding_guard").at("note_identity_fingerprint") ||
                   !managed_routing_detail::equal(
                       managed_routing_detail::anchor(preview)->at("before_manifest"),
                       observation.at("manifest")))
            return std::unexpected(ErrorCode::ProtocolError);
        return ManagedRoutingPreview{context,
                                     preview.at("project_key"),
                                     preview.at("binding_key"),
                                     value.at("preview_token"),
                                     value.at("preview_fingerprint"),
                                     preview,
                                     observation};
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<LomRequest> make_managed_routing_request(const ManagedBridgeContext& context,
                                                const std::string& operation,
                                                const ManagedRoutingPreview& preview) {
    if (context.bridge_instance != preview.context.bridge_instance ||
        context.document_token != preview.context.document_token ||
        !managed_routing_detail::preview_valid(preview.approved_preview) ||
        preview.approved_preview.at("context") != context_json(context))
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{{"document_token", context.document_token},
                 {"operation_id", operation},
                 {"project_key", preview.project_key},
                 {"binding_key", preview.binding_key},
                 {"expected_content_fingerprint",
                  preview.approved_preview.at("binding_guard").at("content_fingerprint")},
                 {"expected_note_identity_fingerprint",
                  preview.approved_preview.at("binding_guard").at("note_identity_fingerprint")},
                 {"intent", preview.approved_preview.at("intent")},
                 {"preview_token", preview.preview_token},
                 {"preview_fingerprint", preview.preview_fingerprint},
                 {"approved_preview", preview.approved_preview},
                 {"explicit_current_routing_approval", true}};
    if (!managed_routing_detail::request_valid(managed_routing_detail::apply_method, payload) ||
        !managed_detail::operation_reservation_fits(payload, managed_routing_detail::apply_method))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{managed_routing_detail::apply_method}, {payload});
}
Result<std::vector<ManagedBindingReceipt>> managed_routing_affected_bindings(
    const ManagedBridgeContext& context, const json& request, const json& result) {
    try {
        if (!managed_routing_detail::result_matches_request(request, result) ||
            request.at("approved_preview").at("context") != context_json(context))
            return std::unexpected(ErrorCode::ProtocolError);
        std::vector<ManagedBindingReceipt> bindings;
        if (result.contains("group_adoption")) return bindings;
        for (const auto& item : result.at("routing").at("affected_observations"))
            bindings.push_back(
                {context, item.at("project_key"), item.at("binding_key"), item.at("observation")});
        return bindings;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
} // namespace sunny::infrastructure
