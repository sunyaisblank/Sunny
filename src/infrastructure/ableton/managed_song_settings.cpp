#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_song_settings.hpp>

namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace managed_song_detail {
using namespace managed_detail;
namespace {
bool boolean(const json& object, std::string_view name) {
    return object.contains(name) && object.at(name).is_boolean();
}
bool actual_float(const json& value, double minimum = -std::numeric_limits<double>::max()) {
    return value.is_number_float() && std::isfinite(value.get<double>()) &&
           value.get<double>() >= minimum;
}
bool indexed_names(const json& values, bool slots) {
    if (!values.is_array() || values.size() > 4096) return false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& track = values.at(index);
        if (!(slots ? recovery_fields(track, {"index", "name", "clip_slots"})
                    : recovery_fields(track, {"index", "name"})) ||
            !note_integer(track.at("index"), 0, 4095) || track.at("index") != index ||
            !recovery_text(track.at("name"), 1024, true))
            return false;
        if (!slots) continue;
        const auto& clips = track.at("clip_slots");
        if (!clips.is_array() || clips.size() > 4096) return false;
        for (std::size_t slot_index = 0; slot_index < clips.size(); ++slot_index) {
            const auto& slot = clips.at(slot_index);
            if (!recovery_fields(slot, {"index", "has_clip", "clip"}) ||
                !note_integer(slot.at("index"), 0, 4095) || slot.at("index") != slot_index ||
                !boolean(slot, "has_clip"))
                return false;
            const auto& clip = slot.at("clip");
            if (slot.at("has_clip") == false) {
                if (!clip.is_null()) return false;
                continue;
            }
            if (!recovery_fields(clip,
                                 {"name",
                                  "is_playing",
                                  "is_recording",
                                  "is_overdubbing",
                                  "is_triggered",
                                  "will_record_on_start"}) ||
                !recovery_text(clip.at("name"), 1024, true))
                return false;
            for (auto field : {"is_playing",
                               "is_recording",
                               "is_overdubbing",
                               "is_triggered",
                               "will_record_on_start"})
                if (!boolean(clip, field)) return false;
        }
    }
    return true;
}
bool idle(const json& before, const json& desired) {
    if (before.at("current_song_time") != 0.0) return false;
    for (std::size_t i = 0; i < 12; ++i)
        if (before.at("flags").at(flag_fields[i]) != false) return false;
    for (const auto& scene : before.at("scenes"))
        if (scene.at("is_triggered") != false) return false;
    for (const auto& track : before.at("tracks"))
        for (const auto& slot : track.at("clip_slots")) {
            if (!slot.at("has_clip").get<bool>()) continue;
            for (auto name : {"is_playing",
                              "is_recording",
                              "is_overdubbing",
                              "is_triggered",
                              "will_record_on_start"})
                if (slot.at("clip").at(name) != false) return false;
        }
    if (before.at("settings").at("tempo") == desired.at("tempo")) return true;
    const auto& parameter = before.at("tempo_parameter");
    return !parameter.is_null() && parameter.at("is_enabled") == true &&
           parameter.at("state") == 0 && parameter.at("automation_state") == 0;
}
bool binding_closes(const json& preview, const json& observation) {
    const auto& ctx = preview.at("context");
    const ManagedBindingReceipt binding{{ctx.at("bridge_instance"), ctx.at("document_token")},
                                        preview.at("project_key"),
                                        preview.at("binding_key"),
                                        observation};
    if (!managed_binding_from_json(managed_binding_to_json(binding)) ||
        !group_touched_boundary(
            observation, binding.context, binding.project_key, binding.binding_key))
        return false;
    const auto& guard = preview.at("binding_guard");
    const auto device = observation.value("device_identity_fingerprint", json{});
    return observation.at("track_tag") == "Sunny|" + preview.at("project_key").get<std::string>() +
                                              "|" + preview.at("binding_key").get<std::string>() +
                                              "|track" &&
           observation.at("clip_tag") == "Sunny|" + preview.at("project_key").get<std::string>() +
                                             "|" + preview.at("binding_key").get<std::string>() +
                                             "|clip" &&
           observation.at("content_fingerprint") == guard.at("content_fingerprint") &&
           observation.at("note_identity_fingerprint") == guard.at("note_identity_fingerprint") &&
           device == guard.at("device_identity_fingerprint");
}
} // namespace
bool observation_closes(const json& preview, const json& observation) {
    try {
        return preview_valid(preview) && binding_closes(preview, observation);
    } catch (const json::exception&) {
        return false;
    }
}
bool settings_valid(const json& value, bool actual) {
    if (!recovery_fields(value, {"tempo", "signature_numerator", "signature_denominator"}) ||
        !value.at("tempo").is_number() || (actual && !value.at("tempo").is_number_float()) ||
        !std::isfinite(value.at("tempo").get<double>()) || value.at("tempo").get<double>() < 20.0 ||
        value.at("tempo").get<double>() > 999.0 ||
        !note_integer(value.at("signature_numerator"), 1, 99) ||
        !note_integer(value.at("signature_denominator"), 1, 16))
        return false;
    const auto denominator = value.at("signature_denominator").get<int>();
    return (denominator & (denominator - 1)) == 0;
}
bool snapshot_valid(const json& value) {
    try {
        if (!recovery_fields(value,
                             {"settings",
                              "current_song_time",
                              "loop_start",
                              "loop_length",
                              "flags",
                              "tempo_parameter",
                              "tracks",
                              "return_tracks",
                              "master_track",
                              "scenes",
                              "cue_points"}) ||
            !settings_valid(value.at("settings"), true) ||
            !actual_float(value.at("current_song_time"), 0.0) ||
            !actual_float(value.at("loop_start"), 0.0) ||
            !actual_float(value.at("loop_length"), 0.0) || value.at("loop_length") == 0.0 ||
            !value.at("flags").is_object() || value.at("flags").size() != flag_fields.size())
            return false;
        for (const auto name : flag_fields)
            if (!boolean(value.at("flags"), name)) return false;
        const auto& parameter = value.at("tempo_parameter");
        if (!parameter.is_null() &&
            (!recovery_fields(parameter, {"is_enabled", "state", "automation_state"}) ||
             !boolean(parameter, "is_enabled") || !note_integer(parameter.at("state"), 0, 2) ||
             !note_integer(parameter.at("automation_state"), 0, 2)))
            return false;
        if (!indexed_names(value.at("tracks"), true) ||
            !indexed_names(value.at("return_tracks"), false) ||
            !recovery_fields(value.at("master_track"), {"name"}) ||
            !recovery_text(value.at("master_track").at("name"), 1024, true))
            return false;
        const auto& scenes = value.at("scenes");
        if (!scenes.is_array() || scenes.size() > 4096) return false;
        for (const auto& scene : scenes) {
            if (!recovery_fields(scene,
                                 {"name",
                                  "is_triggered",
                                  "tempo_enabled",
                                  "tempo",
                                  "time_signature_enabled",
                                  "time_signature_numerator",
                                  "time_signature_denominator"}) ||
                !recovery_text(scene.at("name"), 1024, true) || !boolean(scene, "is_triggered") ||
                !boolean(scene, "tempo_enabled") || !boolean(scene, "time_signature_enabled") ||
                !actual_float(scene.at("tempo")) ||
                !note_integer(scene.at("time_signature_numerator"), -1, 99) ||
                !note_integer(scene.at("time_signature_denominator"), -1, 16))
                return false;
            if (scene.at("tempo_enabled") == true) {
                if (scene.at("tempo").get<double>() < 20.0 ||
                    scene.at("tempo").get<double>() > 999.0)
                    return false;
            } else if (scene.at("tempo") != -1.0)
                return false;
            if (scene.at("time_signature_enabled") == true) {
                const json signature{
                    {"tempo", 120.0},
                    {"signature_numerator", scene.at("time_signature_numerator")},
                    {"signature_denominator", scene.at("time_signature_denominator")}};
                if (!settings_valid(signature)) return false;
            } else if (scene.at("time_signature_numerator") != -1 ||
                       scene.at("time_signature_denominator") != -1)
                return false;
        }
        const auto& cues = value.at("cue_points");
        if (!cues.is_array() || cues.size() > 4096) return false;
        for (const auto& cue : cues)
            if (!recovery_fields(cue, {"name", "time"}) ||
                !recovery_text(cue.at("name"), 1024, true) || !actual_float(cue.at("time"), 0.0))
                return false;
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
json changed_fields(const json& before, const json& desired) {
    auto result = json::array();
    for (auto name : scalar_fields)
        if (before.at("settings").at(name) != desired.at(name)) result.push_back(name);
    return result;
}
bool untouched(const json& before, const json& after) {
    auto previous = before, current = after;
    previous.erase("settings");
    current.erase("settings");
    return managed_digest(previous) == managed_digest(current);
}
bool preview_valid(const json& preview) {
    try {
        if (!recovery_fields(preview,
                             {"schema_version",
                              "context",
                              "project_key",
                              "binding_key",
                              "preview_token",
                              "desired",
                              "before",
                              "binding_guard",
                              "scope",
                              "unavailable_domains"}) ||
            !note_integer(preview.at("schema_version"), 1, 1) ||
            !recovery_context(preview.at("context")) || !recovery_key(preview.at("project_key")) ||
            !recovery_key(preview.at("binding_key")) ||
            !recovery_hex(preview.at("preview_token"), 32) ||
            !settings_valid(preview.at("desired")) || !snapshot_valid(preview.at("before")) ||
            !idle(preview.at("before"), preview.at("desired")))
            return false;
        const auto& guard = preview.at("binding_guard");
        if (!recovery_fields(guard,
                             {"content_fingerprint",
                              "note_identity_fingerprint",
                              "device_identity_fingerprint"}) ||
            !recovery_hex(guard.at("content_fingerprint"), 64) ||
            !recovery_hex(guard.at("note_identity_fingerprint"), 64) ||
            (!guard.at("device_identity_fingerprint").is_null() &&
             !recovery_hex(guard.at("device_identity_fingerprint"), 64)))
            return false;
        return preview.at("scope") == json{{"set_wide", true},
                                           {"all_tracks_affected", true},
                                           {"historical_song_identity_proven", false},
                                           {"scene_overrides_preserved", true}} &&
               preview.at("unavailable_domains") ==
                   json::array({"arrangement_tempo_envelope_population",
                                "arrangement_time_signature_markers",
                                "unrelated_track_internal_content"});
    } catch (const json::exception&) {
        return false;
    }
}
bool request_valid(std::string_view method, const json& payload) {
    try {
        if (method == preview_method)
            return recovery_fields(payload,
                                   {"document_token",
                                    "project_key",
                                    "binding_key",
                                    "expected_content_fingerprint",
                                    "expected_note_identity_fingerprint",
                                    "desired"}) &&
                   recovery_key(payload.at("document_token")) &&
                   recovery_key(payload.at("project_key")) &&
                   recovery_key(payload.at("binding_key")) &&
                   recovery_hex(payload.at("expected_content_fingerprint"), 64) &&
                   recovery_hex(payload.at("expected_note_identity_fingerprint"), 64) &&
                   settings_valid(payload.at("desired"));
        if (method != apply_method ||
            !recovery_fields(payload,
                             {"document_token",
                              "project_key",
                              "binding_key",
                              "operation_id",
                              "expected_content_fingerprint",
                              "expected_note_identity_fingerprint",
                              "desired",
                              "preview_token",
                              "preview_fingerprint",
                              "approved_preview",
                              "explicit_set_wide_approval"}) ||
            !recovery_key(payload.at("operation_id")) ||
            payload.at("explicit_set_wide_approval") != true ||
            !recovery_hex(payload.at("preview_fingerprint"), 64) ||
            !preview_valid(payload.at("approved_preview")))
            return false;
        const auto& preview = payload.at("approved_preview");
        return payload.at("document_token") == preview.at("context").at("document_token") &&
               payload.at("project_key") == preview.at("project_key") &&
               payload.at("binding_key") == preview.at("binding_key") &&
               payload.at("preview_token") == preview.at("preview_token") &&
               payload.at("desired").dump() == preview.at("desired").dump() &&
               payload.at("expected_content_fingerprint") ==
                   preview.at("binding_guard").at("content_fingerprint") &&
               payload.at("expected_note_identity_fingerprint") ==
                   preview.at("binding_guard").at("note_identity_fingerprint") &&
               managed_digest(preview) ==
                   std::optional{payload.at("preview_fingerprint").get<std::string>()};
    } catch (const json::exception&) {
        return false;
    }
}
bool result_matches_request(const json& payload, const json& result) {
    try {
        if (!request_valid(apply_method, payload) || !result.is_object() ||
            !result.contains("song_settings"))
            return false;
        const auto& supplement = result.at("song_settings");
        if (!recovery_fields(supplement,
                             {"approved_preview",
                              "preview_fingerprint",
                              "before",
                              "after",
                              "desired",
                              "started_fields",
                              "returned_fields",
                              "authority_scope",
                              "historical_song_identity_proven",
                              "desired_settings_match",
                              "observed_untouched_state_preserved",
                              "clip_and_note_ids_preserved"}) ||
            supplement.at("approved_preview").dump() != payload.at("approved_preview").dump() ||
            supplement.at("preview_fingerprint") != payload.at("preview_fingerprint") ||
            supplement.at("before").dump() != payload.at("approved_preview").at("before").dump() ||
            supplement.at("desired").dump() != payload.at("desired").dump() ||
            !snapshot_valid(supplement.at("after")) ||
            supplement.at("authority_scope") != "explicit_current_set_settings" ||
            supplement.at("historical_song_identity_proven") != false)
            return false;
        const auto fields = changed_fields(supplement.at("before"), payload.at("desired"));
        auto actual = result;
        actual.erase("song_settings");
        const auto& preview = payload.at("approved_preview");
        const auto& ctx = preview.at("context");
        const ManagedBindingReceipt binding{{ctx.at("bridge_instance"), ctx.at("document_token")},
                                            preview.at("project_key"),
                                            preview.at("binding_key"),
                                            actual};
        if (!managed_binding_from_json(managed_binding_to_json(binding)) ||
            !group_touched_boundary(
                actual, binding.context, binding.project_key, binding.binding_key))
            return false;
        return supplement.at("started_fields") == fields &&
               supplement.at("returned_fields") == fields &&
               supplement.at("desired_settings_match") ==
                   (supplement.at("after").at("settings") == payload.at("desired")) &&
               supplement.at("observed_untouched_state_preserved") ==
                   untouched(supplement.at("before"), supplement.at("after")) &&
               supplement.at("clip_and_note_ids_preserved") == binding_closes(preview, actual);
    } catch (const json::exception&) {
        return false;
    }
}
bool acknowledged_native_start_valid(const json& payload, const json& result, bool native_started) {
    return result_matches_request(payload, result) &&
           native_started ==
               !changed_fields(payload.at("approved_preview").at("before"), payload.at("desired"))
                    .empty();
}
bool partial_valid(const json& payload, const json& journal) {
    try {
        if (!request_valid(apply_method, payload) || !journal.is_object() ||
            !boolean(journal, "native_mutation_started"))
            return false;
        const bool acknowledged =
            journal.contains("outcome") && journal.at("outcome") == "acknowledged";
        if (!journal.contains("song_settings_progress"))
            return !acknowledged && journal.at("native_mutation_started") == false &&
                   !journal.contains("song_settings_partial") &&
                   !journal.contains("song_settings_partial_unavailable");
        if (!recovery_fields(journal.at("song_settings_progress"),
                             {"started_fields", "returned_fields"}))
            return false;
        const auto expected =
            changed_fields(payload.at("approved_preview").at("before"), payload.at("desired"));
        const auto& progress = journal.at("song_settings_progress");
        const auto& started = progress.at("started_fields");
        const auto& returned = progress.at("returned_fields");
        if (!started.is_array() || !returned.is_array() || started.size() > expected.size() ||
            returned.size() > started.size() || started.size() - returned.size() > 1 ||
            journal.at("native_mutation_started") != !started.empty())
            return false;
        for (std::size_t i = 0; i < started.size(); ++i)
            if (started.at(i) != expected.at(i)) return false;
        for (std::size_t i = 0; i < returned.size(); ++i)
            if (returned.at(i) != started.at(i)) return false;
        if (acknowledged) {
            if (!journal.contains("result") ||
                !acknowledged_native_start_valid(
                    payload,
                    journal.at("result"),
                    journal.at("native_mutation_started").get<bool>()) ||
                journal.contains("song_settings_partial") ||
                journal.contains("song_settings_partial_unavailable"))
                return false;
            const auto& supplement = journal.at("result").at("song_settings");
            return started == supplement.at("started_fields") &&
                   returned == supplement.at("returned_fields");
        }
        if (journal.contains("song_settings_partial")) {
            const auto& partial = journal.at("song_settings_partial");
            if (!recovery_fields(
                    partial, {"before", "after", "desired", "started_fields", "returned_fields"}) ||
                partial.at("before").dump() != payload.at("approved_preview").at("before").dump() ||
                !snapshot_valid(partial.at("after")) ||
                partial.at("desired").dump() != payload.at("desired").dump() ||
                partial.at("started_fields") != started ||
                partial.at("returned_fields") != returned || started.empty())
                return false;
        }
        if (journal.contains("song_settings_partial_unavailable") &&
            journal.at("song_settings_partial_unavailable") != true)
            return false;
        return !(journal.contains("song_settings_partial") &&
                 journal.contains("song_settings_partial_unavailable"));
    } catch (const json::exception&) {
        return false;
    }
}
} // namespace managed_song_detail
json managed_song_settings_to_json(const ManagedSongSettings& desired) {
    return {{"tempo", desired.tempo},
            {"signature_numerator", desired.signature_numerator},
            {"signature_denominator", desired.signature_denominator}};
}
std::expected<ManagedSongSettings, std::string>
plan_managed_song_settings(const sunny::core::Score& score) {
    using namespace sunny::core;
    if (score.tempo_map.size() != 1)
        return std::unexpected(
            "UnsupportedTempoMap: exactly one constant origin tempo is supported");
    const auto& tempo = score.tempo_map.front();
    if (tempo.position != SCORE_START || tempo.transition_type != TempoTransitionType::Immediate)
        return std::unexpected("UnsupportedTempoTransition: Linear/MetricModulation or non-origin "
                               "tempo is unsupported");
    if (score.time_map.size() != 1 || score.time_map.front().bar != 1)
        return std::unexpected(
            "UnsupportedMeterMap: exactly one initial global meter is supported");
    const auto& meter = score.time_map.front().time_signature;
    for (const auto& part : score.parts)
        for (const auto& measure : part.measures)
            if (measure.local_time && *measure.local_time != meter)
                return std::unexpected(
                    "UnsupportedLocalMeter: distinct measure-local meters remain unimplemented");
    if (!is_compilable(score))
        return std::unexpected("InvalidScore: structural Score validation failed");
    const auto settings = ManagedSongSettings{
        effective_quarter_bpm(tempo), meter.numerator(), static_cast<int>(meter.denominator())};
    if (!managed_song_detail::settings_valid(managed_song_settings_to_json(settings)))
        return std::unexpected("UnrepresentableSongSettings: quarter "
                               "tempo20..999/numerator1..99/denominator1,2,4,8,16 required");
    return settings;
}
Result<LomRequest> make_managed_song_settings_preview_request(const ManagedBridgeContext& context,
                                                              const ManagedBindingReceipt& binding,
                                                              const ManagedSongSettings& desired) {
    if (!managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{
        {"document_token", context.document_token},
        {"project_key", binding.project_key},
        {"binding_key", binding.binding_key},
        {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
        {"expected_note_identity_fingerprint", binding.observation.at("note_identity_fingerprint")},
        {"desired", managed_song_settings_to_json(desired)}};
    if (!managed_song_detail::request_valid(managed_song_detail::preview_method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{managed_song_detail::preview_method}, {payload});
}
Result<ManagedSongSettingsPreview> parse_managed_song_settings_preview(
    const LomRequest& request, const ManagedBridgeContext& context, const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != managed_song_detail::preview_method ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args.at(0)) ||
            !recovery_fields(value,
                             {"schema_version",
                              "outcome",
                              "preview_token",
                              "preview_fingerprint",
                              "preview",
                              "observation"}) ||
            !note_integer(value.at("schema_version"), 1, 1) || value.at("outcome") != "previewed" ||
            !recovery_hex(value.at("preview_fingerprint"), 64) ||
            !managed_song_detail::preview_valid(value.at("preview")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args.at(0));
        const auto& preview = value.at("preview");
        if (!managed_song_detail::request_valid(managed_song_detail::preview_method, payload) ||
            preview.at("context") != json{{"bridge_instance", context.bridge_instance},
                                          {"document_token", context.document_token}} ||
            preview.at("project_key") != payload.at("project_key") ||
            preview.at("binding_key") != payload.at("binding_key") ||
            preview.at("desired").dump() != payload.at("desired").dump() ||
            preview.at("binding_guard").at("content_fingerprint") !=
                payload.at("expected_content_fingerprint") ||
            preview.at("binding_guard").at("note_identity_fingerprint") !=
                payload.at("expected_note_identity_fingerprint") ||
            preview.at("preview_token") != value.at("preview_token") ||
            managed_digest(preview) !=
                std::optional{value.at("preview_fingerprint").get<std::string>()})
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& observation = value.at("observation");
        if (!managed_song_detail::observation_closes(preview, observation))
            return std::unexpected(ErrorCode::ProtocolError);
        return ManagedSongSettingsPreview{context,
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
Result<LomRequest> make_managed_song_settings_request(const ManagedBridgeContext& context,
                                                      const std::string& operation,
                                                      const ManagedSongSettingsPreview& preview) {
    using namespace managed_detail;
    const auto& approved = preview.approved_preview;
    if (!managed_song_detail::observation_closes(approved, preview.observation) ||
        context.bridge_instance != preview.context.bridge_instance ||
        context.document_token != preview.context.document_token ||
        approved.at("context") != json{{"bridge_instance", context.bridge_instance},
                                       {"document_token", context.document_token}} ||
        approved.at("project_key") != preview.project_key ||
        approved.at("binding_key") != preview.binding_key ||
        approved.at("preview_token") != preview.preview_token ||
        managed_digest(approved) != std::optional{preview.preview_fingerprint})
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{
        {"document_token", context.document_token},
        {"operation_id", operation},
        {"project_key", preview.project_key},
        {"binding_key", preview.binding_key},
        {"expected_content_fingerprint", approved.at("binding_guard").at("content_fingerprint")},
        {"expected_note_identity_fingerprint",
         approved.at("binding_guard").at("note_identity_fingerprint")},
        {"desired", approved.at("desired")},
        {"preview_token", preview.preview_token},
        {"preview_fingerprint", preview.preview_fingerprint},
        {"approved_preview", approved},
        {"explicit_set_wide_approval", true}};
    if (!managed_song_detail::request_valid(managed_song_detail::apply_method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    // All approved Song bytes are known; the actual full Part population remains included.
    auto after = approved.at("before");
    after["settings"] = approved.at("desired");
    const auto fields =
        managed_song_detail::changed_fields(approved.at("before"), approved.at("desired"));
    auto actual = preview.observation;
    actual["song_settings"] = {{"approved_preview", approved},
                               {"preview_fingerprint", preview.preview_fingerprint},
                               {"before", approved.at("before")},
                               {"after", after},
                               {"desired", approved.at("desired")},
                               {"started_fields", fields},
                               {"returned_fields", fields},
                               {"authority_scope", "explicit_current_set_settings"},
                               {"historical_song_identity_proven", false},
                               {"desired_settings_match", true},
                               {"observed_untouched_state_preserved", true},
                               {"clip_and_note_ids_preserved", true}};
    const json progress{{"started_fields", fields}, {"returned_fields", fields}};
    const json journal{{"document_token", context.document_token},
                       {"operation_id", operation},
                       {"name", managed_song_detail::apply_method},
                       {"request", payload},
                       {"request_fingerprint", std::string(64, 'f')},
                       {"outcome", "acknowledged"},
                       {"native_mutation_started", !fields.empty()},
                       {"song_settings_progress", progress},
                       {"result", actual}};
    const auto bound = json_wire_bound(json{{"success", true}, {"value", journal}});
    auto partial = journal;
    partial.erase("result");
    partial["outcome"] = "indeterminate";
    std::string worst_error;
    for (int i = 0; i < 1024; ++i)
        worst_error += "\xf4\x8f\xbf\xbf";
    partial["error"] = std::move(worst_error);
    partial["song_settings_partial"] = {{"before", approved.at("before")},
                                        {"after", after},
                                        {"desired", approved.at("desired")},
                                        {"started_fields", fields},
                                        {"returned_fields", fields}};
    const auto partial_bound = json_wire_bound(json{{"success", true}, {"value", partial}});
    if (!operation_reservation_fits(payload, managed_song_detail::apply_method) || !bound ||
        *bound > managed_response_limit || !partial_bound ||
        *partial_bound > managed_response_limit)
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{managed_song_detail::apply_method}, {payload});
}
} // namespace sunny::infrastructure
