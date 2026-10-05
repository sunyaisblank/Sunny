/**
 * @file lom_protocol.cpp
 * @brief LOM Bridge Protocol implementation
 *
 *
 * Serialisation uses nlohmann/json for correctness (proper string
 * escaping, numeric precision). Deserialisation parses JSON responses
 * from the Python Remote Script.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string_view>
#include <sunny/core/timbre/live_capabilities.hpp>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_envelope_revision.hpp>
#include <sunny/infrastructure/ableton/detail/managed_geometry.hpp>
#include <sunny/infrastructure/ableton/detail/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>
#include <sunny/infrastructure/ableton/detail/managed_routing.hpp>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_song_settings.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;

// =============================================================================
// LomPath
// =============================================================================

std::string LomPath::to_string() const {
    std::ostringstream oss;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) oss << "/";
        oss << segments[i];
    }
    return oss.str();
}

LomPath LomPath::parse(const std::string& path) {
    LomPath result;
    std::size_t start = 0;
    while (true) {
        const auto separator = path.find('/', start);
        result.segments.push_back(path.substr(start, separator - start));
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return result;
}

bool LomPath::is_canonical() const {
    if (segments.empty() || segments.front() != "song") return false;
    for (const auto& segment : segments) {
        if (segment.empty()) return false;
        bool numeric = true;
        for (const unsigned char character : segment) {
            const bool ascii_digit = character >= '0' && character <= '9';
            const bool ascii_letter =
                (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
            if (!ascii_digit) numeric = false;
            if (!ascii_digit && !ascii_letter && character != '_') return false;
        }
        if (numeric) {
            if (segment.size() > 1 && segment.front() == '0') return false;
            constexpr std::string_view maximum_index = "2147483647";
            if (segment.size() > maximum_index.size() ||
                (segment.size() == maximum_index.size() &&
                 std::string_view{segment} > maximum_index))
                return false;
        }
    }
    return true;
}

LomPath LomPath::child(const std::string& name) const {
    LomPath result = *this;
    result.segments.push_back(name);
    return result;
}

LomPath LomPath::child(int index) const {
    return child(std::to_string(index));
}

// =============================================================================
// Serialisation
// =============================================================================

namespace {

json lom_value_to_json(const LomValue& value) {
    return std::visit([](const auto& member) -> json { return member; }, value);
}

std::optional<int> json_to_int(const json& value) {
    if (value.is_number_unsigned()) {
        const auto candidate = value.get<std::uint64_t>();
        if (candidate <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            return static_cast<int>(candidate);
        return std::nullopt;
    }
    if (!value.is_number_integer()) return std::nullopt;
    const auto candidate = value.get<std::int64_t>();
    if (candidate < std::numeric_limits<int>::min() || candidate > std::numeric_limits<int>::max())
        return std::nullopt;
    return static_cast<int>(candidate);
}

std::optional<LomValue> json_to_lom_value(const json& j) {
    if (j.is_boolean()) {
        return j.get<bool>();
    } else if (j.is_number_integer()) {
        auto value = json_to_int(j);
        if (!value) return std::nullopt;
        return *value;
    } else if (j.is_number_float()) {
        return j.get<double>();
    } else if (j.is_string()) {
        return j.get<std::string>();
    } else if (j.is_array() && !j.empty()) {
        if (std::all_of(
                j.begin(), j.end(), [](const json& value) { return value.is_number_integer(); })) {
            std::vector<int> values;
            values.reserve(j.size());
            for (const auto& item : j) {
                auto value = json_to_int(item);
                if (!value) return std::nullopt;
                values.push_back(*value);
            }
            return values;
        } else if (std::all_of(
                       j.begin(), j.end(), [](const json& value) { return value.is_number(); })) {
            return j.get<std::vector<double>>();
        } else if (std::all_of(
                       j.begin(), j.end(), [](const json& value) { return value.is_string(); })) {
            return j.get<std::vector<std::string>>();
        }
    }
    // Live's note API and session inspection both use structured values.
    // Preserve them rather than silently replacing them with an empty string.
    return j;
}

json notes_to_json(const std::vector<LomNoteData>& notes) {
    json arr = json::array();
    for (const auto& n : notes) {
        arr.push_back({{"pitch", static_cast<int>(n.pitch)},
                       {"start_time", n.start_time},
                       {"duration", n.duration},
                       {"velocity", static_cast<int>(n.velocity)},
                       {"mute", n.muted},
                       {"probability", n.probability},
                       {"velocity_deviation", n.velocity_deviation},
                       {"release_velocity", n.release_velocity}});
    }
    return arr;
}

enum class PathKind {
    Song,
    Scene,
    Track,
    ReturnTrack,
    MasterTrack,
    ClipSlot,
    Clip,
    Device,
    MixerDevice,
    MasterMixerDevice,
    VolumeParameter,
    TrackActivatorParameter,
    ReturnActivatorParameter,
    MasterActivatorParameter,
    PanningParameter,
    MasterPanningParameter,
    SendParameter,
};

bool is_index(std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char character) {
        return character >= '0' && character <= '9';
    });
}

std::optional<PathKind> path_kind(const LomPath& path) {
    if (!path.is_canonical()) return std::nullopt;
    const auto& segments = path.segments;
    if (segments == std::vector<std::string>{"song"}) return PathKind::Song;
    if (segments.size() == 3 && segments[1] == "scenes" && is_index(segments[2]))
        return PathKind::Scene;

    std::size_t track_end = 0;
    PathKind track_kind;
    if (segments.size() >= 3 && segments[1] == "tracks" && is_index(segments[2])) {
        track_end = 3;
        track_kind = PathKind::Track;
    } else if (segments.size() >= 3 && segments[1] == "return_tracks" && is_index(segments[2])) {
        track_end = 3;
        track_kind = PathKind::ReturnTrack;
    } else if (segments.size() >= 2 && segments[1] == "master_track") {
        track_end = 2;
        track_kind = PathKind::MasterTrack;
    } else {
        return std::nullopt;
    }

    const auto tail_size = segments.size() - track_end;
    if (tail_size == 0) return track_kind;
    if (track_kind == PathKind::Track && tail_size == 2 && segments[track_end] == "clip_slots" &&
        is_index(segments[track_end + 1]))
        return PathKind::ClipSlot;
    if (track_kind == PathKind::Track && tail_size == 3 && segments[track_end] == "clip_slots" &&
        is_index(segments[track_end + 1]) && segments[track_end + 2] == "clip")
        return PathKind::Clip;
    if (tail_size == 2 && segments[track_end] == "devices" && is_index(segments[track_end + 1]))
        return PathKind::Device;
    if ((track_kind == PathKind::Track || track_kind == PathKind::ReturnTrack) && tail_size == 1 &&
        segments[track_end] == "mixer_device")
        return PathKind::MixerDevice;
    if (track_kind == PathKind::MasterTrack && tail_size == 1 &&
        segments[track_end] == "mixer_device")
        return PathKind::MasterMixerDevice;
    if (tail_size == 2 && segments[track_end] == "mixer_device" &&
        segments[track_end + 1] == "volume")
        return PathKind::VolumeParameter;
    if (tail_size == 2 && segments[track_end] == "mixer_device" &&
        segments[track_end + 1] == "track_activator") {
        if (track_kind == PathKind::Track) return PathKind::TrackActivatorParameter;
        if (track_kind == PathKind::ReturnTrack) return PathKind::ReturnActivatorParameter;
        return PathKind::MasterActivatorParameter;
    }
    if (tail_size == 2 && segments[track_end] == "mixer_device" &&
        segments[track_end + 1] == "panning")
        return track_kind == PathKind::MasterTrack ? PathKind::MasterPanningParameter
                                                   : PathKind::PanningParameter;
    if (track_kind == PathKind::Track && tail_size == 3 && segments[track_end] == "mixer_device" &&
        segments[track_end + 1] == "sends" && is_index(segments[track_end + 2]))
        return PathKind::SendParameter;
    return std::nullopt;
}

bool is_one_of(std::string_view value, std::initializer_list<std::string_view> allowed) {
    return std::ranges::find(allowed, value) != allowed.end();
}

bool finite_number(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool exact_live_release_velocity(const json& value) {
    if (!value.is_number_float()) return false;
    const double release_velocity = value.get<double>();
    return std::isfinite(release_velocity) && release_velocity >= 0.0 &&
           release_velocity <= 127.0 && std::trunc(release_velocity) == release_velocity;
}

bool protocol_index(const json& value, bool allow_append = false) {
    const auto parsed = json_to_int(value);
    return parsed && *parsed >= (allow_append ? -1 : 0);
}

bool valid_routing_dictionary(const json& value) {
    return value.is_object() && value.size() == 2 && value.contains("display_name") &&
           value.at("display_name").is_string() && value.contains("identifier") &&
           value.at("identifier").is_string();
}

bool valid_note_dictionary(const json& value) {
    if (!value.is_object() || value.size() != 1 || !value.contains("notes") ||
        !value.at("notes").is_array() || value.at("notes").empty())
        return false;
    for (const auto& note : value.at("notes")) {
        if (!note.is_object() || note.size() != 8 || !note.contains("pitch") ||
            !note.contains("start_time") || !note.contains("duration") ||
            !note.contains("velocity") || !note.contains("mute") || !note.contains("probability") ||
            !note.contains("velocity_deviation") || !note.contains("release_velocity"))
            return false;
        const auto pitch = json_to_int(note.at("pitch"));
        const auto velocity = json_to_int(note.at("velocity"));
        if (!pitch || *pitch < 0 || *pitch > 127 || !velocity || *velocity < 1 || *velocity > 127 ||
            !finite_number(note.at("start_time")) || note.at("start_time").get<double>() < 0.0 ||
            !finite_number(note.at("duration")) || note.at("duration").get<double>() <= 0.0 ||
            !note.at("mute").is_boolean() || !note.at("probability").is_number_float() ||
            note.at("probability").get<double>() != 1.0 ||
            !note.at("velocity_deviation").is_number_float() ||
            note.at("velocity_deviation").get<double>() != 0.0 ||
            !exact_live_release_velocity(note.at("release_velocity")))
            return false;
    }
    return true;
}

bool valid_note_return_fields(const json& value) {
    if (!value.is_array() || value.size() != 9) return false;

    std::set<std::string> fields;
    for (const auto& candidate : value) {
        if (!candidate.is_string() || !fields.insert(candidate.get<std::string>()).second)
            return false;
    }
    return fields == std::set<std::string>{"note_id",
                                           "pitch",
                                           "start_time",
                                           "duration",
                                           "velocity",
                                           "mute",
                                           "probability",
                                           "velocity_deviation",
                                           "release_velocity"};
}

bool valid_note_id_query(const json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("note_ids") ||
        !value.at("note_ids").is_array() || value.at("note_ids").empty() ||
        !value.contains("return") || !valid_note_return_fields(value.at("return")))
        return false;

    std::set<int> ids;
    for (const auto& candidate : value.at("note_ids")) {
        const auto id = json_to_int(candidate);
        if (!id || !ids.insert(*id).second) return false;
    }

    return true;
}

bool valid_all_notes_query(const json& value) {
    return value.is_object() && value.size() == 1 && value.contains("return") &&
           valid_note_return_fields(value.at("return"));
}

bool valid_ranged_notes_query(const json& value) {
    return value.is_object() && value.size() == 5 && value.contains("return") &&
           valid_note_return_fields(value.at("return")) && value.contains("from_pitch") &&
           json_to_int(value.at("from_pitch")) == 0 && value.contains("pitch_span") &&
           json_to_int(value.at("pitch_span")) == 128 && value.contains("from_time") &&
           finite_number(value.at("from_time")) && value.at("from_time").get<double>() == 0.0 &&
           value.contains("time_span") && finite_number(value.at("time_span")) &&
           value.at("time_span").get<double>() > 0.0;
}

bool valid_envelope_parameter(const json& value) {
    if (!value.is_object() || !value.contains("kind") || !value.at("kind").is_string())
        return false;
    const auto& kind = value.at("kind").get_ref<const std::string&>();
    if (kind == "volume" || kind == "panning") return value.size() == 1;
    if (kind == "send")
        return value.size() == 2 && value.contains("send_index") &&
               protocol_index(value.at("send_index"));
    return kind == "device" && value.size() == 3 && value.contains("device_index") &&
           protocol_index(value.at("device_index")) && value.contains("parameter_name") &&
           value.at("parameter_name").is_string() &&
           !value.at("parameter_name").get_ref<const std::string&>().empty();
}

bool valid_step_envelope_author(const json& value) {
    if (!value.is_object() || value.size() != 4 || !value.contains("parameter") ||
        !valid_envelope_parameter(value.at("parameter")) || !value.contains("interpolation") ||
        value.at("interpolation") != "step" || !value.contains("clip_end") ||
        !finite_number(value.at("clip_end")) || value.at("clip_end").get<double>() <= 0.0 ||
        !value.contains("points") || !value.at("points").is_array() || value.at("points").empty() ||
        value.at("points").size() > SUNNY_MANAGED_ENVELOPE_MAX_STEPS)
        return false;
    const auto& points = value.at("points");
    const double end = value.at("clip_end").get<double>();
    double previous = -1.0;
    for (const auto& point : points) {
        if (!point.is_object() || point.size() != 2 || !point.contains("time") ||
            !finite_number(point.at("time")) || !point.contains("value") ||
            !finite_number(point.at("value")))
            return false;
        const double time = point.at("time").get<double>();
        if (time <= previous || time >= end) return false;
        previous = time;
    }
    return points[0].at("time").get<double>() == 0.0;
}

bool valid_step_envelope_query(const json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("parameter") ||
        !valid_envelope_parameter(value.at("parameter")) || !value.contains("sample_times") ||
        !value.at("sample_times").is_array() || value.at("sample_times").empty())
        return false;
    double previous = -1.0;
    for (const auto& time : value.at("sample_times")) {
        if (!finite_number(time)) return false;
        const double sample_time = time.get<double>();
        if (sample_time < 0.0 || sample_time <= previous) return false;
        previous = sample_time;
    }
    return true;
}

bool managed_key(const json& value) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return !text.empty() && text.size() <= 64 && std::ranges::all_of(text, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

bool managed_fingerprint(const json& value) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return text.size() == 64 && std::ranges::all_of(text, [](unsigned char c) {
               return (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
           });
}

bool valid_managed_request(std::string_view name, const std::vector<json>& args) {
    if (name == "sunny_managed_context") return args.empty();
    if (args.size() != 1 || !args[0].is_object()) return false;
    const auto& value = args[0];
    if (is_one_of(name,
                  {"sunny_managed_routing_candidates",
                   "sunny_managed_inspect_send",
                   "sunny_managed_preview_routing",
                   "sunny_managed_preview_group",
                   "sunny_managed_apply_routing"}))
        return managed_routing_detail::request_valid(name, value);
    if (is_one_of(name,
                  {"sunny_managed_preview_static_mixer",
                   "sunny_managed_inspect_static_mixer",
                   "sunny_managed_adopt_static_mixer",
                   "sunny_managed_update_static_mixer"}))
        return managed_mixer_detail::request_valid(name, value);
    if (is_one_of(name,
                  {"sunny_managed_preview_envelope_replacement", "sunny_managed_replace_envelope"}))
        return managed_envelope_detail::request_valid(name, value);
    if (is_one_of(name,
                  {"sunny_managed_preview_song_settings",
                   "sunny_managed_inspect_song_settings",
                   "sunny_managed_apply_song_settings"}))
        return managed_song_detail::request_valid(name, value);
    if (is_one_of(name,
                  {"sunny_managed_insert_device",
                   "sunny_managed_update_device_parameters",
                   "sunny_managed_update_device_modes",
                   "sunny_managed_preview_devices",
                   "sunny_managed_inspect_devices",
                   "sunny_managed_adopt_devices"}))
        return managed_device_detail::device_request_valid(name, value);
    if (name == "sunny_managed_preview_adoption")
        return managed_detail::adoption_preview_request_valid(value);
    if (name == "sunny_managed_adopt_clip") return managed_detail::adoption_request_valid(value);
    if (!value.contains("document_token") || !managed_key(value.at("document_token"))) return false;
    if (name == "sunny_managed_operation")
        return value.size() == 2 && value.contains("operation_id") &&
               managed_key(value.at("operation_id"));
    for (const auto* key : {"project_key", "binding_key"})
        if (!value.contains(key) || !managed_key(value.at(key))) return false;
    if (name == "sunny_managed_observe") return value.size() == 3;
    if (name == "sunny_managed_sample_envelope")
        return value.size() == 5 && value.contains("parameter") && value.contains("sample_times") &&
               valid_step_envelope_query(json{{"parameter", value.at("parameter")},
                                              {"sample_times", value.at("sample_times")}}) &&
               value.at("sample_times").size() <= 65536 &&
               value.at("parameter").at("kind") != "device";
    if (!value.contains("operation_id") || !managed_key(value.at("operation_id"))) return false;
    if (name == "sunny_managed_rebind")
        return value.size() == 5 && value.contains("expected_manifest") &&
               value.at("expected_manifest").is_object() &&
               value.at("expected_manifest").contains("schema_version") &&
               json_to_int(value.at("expected_manifest").at("schema_version")) == 1;
    const bool guarded =
        name == "sunny_managed_replace_clip" || name == "sunny_managed_author_envelope" ||
        name == "sunny_managed_update_notes" || name == "sunny_managed_revise_note_population" ||
        name == "sunny_managed_update_clip_geometry";
    if (guarded && (!value.contains("expected_content_fingerprint") ||
                    !managed_fingerprint(value.at("expected_content_fingerprint"))))
        return false;
    if (name == "sunny_managed_author_envelope")
        return value.size() == 6 && value.contains("lane") &&
               valid_step_envelope_author(value.at("lane")) &&
               value.at("lane").at("parameter").at("kind") != "device";
    if (name == "sunny_managed_revise_note_population")
        return value.size() == 8 && managed_detail::population_request_valid(value);
    if (name == "sunny_managed_update_clip_geometry")
        return value.size() == 6 && value.contains("geometry") &&
               managed_detail::clip_geometry_valid(value.at("geometry"));
    if (name == "sunny_managed_update_notes")
        return value.size() == 6 && value.contains("changes") &&
               managed_detail::note_changes_valid(value.at("changes"));
    if (!is_one_of(name, {"sunny_managed_create_clip", "sunny_managed_replace_clip"}) ||
        value.size() != (guarded ? 9U : 8U))
        return false;
    for (const auto* key : {"clip_end", "signature_numerator", "signature_denominator", "notes"})
        if (!value.contains(key)) return false;
    const auto numerator = json_to_int(value.at("signature_numerator"));
    const auto denominator = json_to_int(value.at("signature_denominator"));
    if (!finite_number(value.at("clip_end")) || value.at("clip_end").get<double>() <= 0.0 ||
        !numerator || *numerator < 1 || *numerator > 99 || !denominator ||
        !is_one_of(std::to_string(*denominator), {"1", "2", "4", "8", "16"}) ||
        !value.at("notes").is_array())
        return false;
    if (!value.at("notes").empty() && !valid_note_dictionary(json{{"notes", value.at("notes")}}))
        return false;
    return std::ranges::all_of(value.at("notes"), [&value](const json& note) {
        return note.at("start_time").get<double>() < value.at("clip_end").get<double>();
    });
}

} // namespace

sunny::core::Result<void> LomProtocol::validate_request(const LomRequest& request) {
    const auto kind = path_kind(request.path);
    if (!kind || request.property_or_method.empty())
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);

    std::vector<json> args;
    args.reserve(request.args.size());
    for (const auto& arg : request.args)
        args.push_back(lom_value_to_json(arg));
    const auto& name = request.property_or_method;

    bool operation_allowed = false;
    switch (*kind) {
    case PathKind::Song:
        operation_allowed =
            (request.type == LomRequestType::GetProperty && is_one_of(name,
                                                                      {"tempo",
                                                                       "signature_numerator",
                                                                       "signature_denominator",
                                                                       "is_playing",
                                                                       "session_record",
                                                                       "record_mode",
                                                                       "current_song_time"})) ||
            (request.type == LomRequestType::SetProperty &&
             is_one_of(name, {"tempo", "signature_numerator", "signature_denominator"})) ||
            (request.type == LomRequestType::CallMethod &&
             is_one_of(name,
                       {"sunny_get_target_profile",
                        "sunny_get_target_snapshot",
                        "sunny_get_scene_count",
                        "sunny_get_track_count",
                        "sunny_get_return_track_count",
                        "sunny_get_remote_log",
                        "sunny_set_cue",
                        "sunny_managed_context",
                        "sunny_managed_operation",
                        "sunny_managed_observe",
                        "sunny_managed_sample_envelope",
                        "sunny_managed_create_clip",
                        "sunny_managed_replace_clip",
                        "sunny_managed_rebind",
                        "sunny_managed_author_envelope",
                        "sunny_managed_update_notes",
                        "sunny_managed_revise_note_population",
                        "sunny_managed_preview_adoption",
                        "sunny_managed_adopt_clip",
                        "sunny_managed_insert_device",
                        "sunny_managed_update_device_parameters",
                        "sunny_managed_update_device_modes",
                        "sunny_managed_preview_devices",
                        "sunny_managed_inspect_devices",
                        "sunny_managed_adopt_devices",
                        "sunny_managed_update_clip_geometry",
                        "sunny_managed_preview_song_settings",
                        "sunny_managed_inspect_song_settings",
                        "sunny_managed_apply_song_settings",
                        "sunny_managed_preview_envelope_replacement",
                        "sunny_managed_replace_envelope",
                        "sunny_managed_preview_static_mixer",
                        "sunny_managed_inspect_static_mixer",
                        "sunny_managed_adopt_static_mixer",
                        "sunny_managed_update_static_mixer",
                        "sunny_managed_routing_candidates",
                        "sunny_managed_inspect_send",
                        "sunny_managed_preview_routing",
                        "sunny_managed_preview_group",
                        "sunny_managed_apply_routing",
                        "create_scene",
                        "create_midi_track",
                        "create_return_track"}));
        break;
    case PathKind::Scene:
        operation_allowed = request.type == LomRequestType::SetProperty &&
                            is_one_of(name, {"name", "tempo_enabled", "time_signature_enabled"});
        break;
    case PathKind::Track:
        operation_allowed = (request.type == LomRequestType::SetProperty &&
                             is_one_of(name, {"name", "mute", "solo", "arm", "implicit_arm"})) ||
                            (request.type == LomRequestType::CallMethod &&
                             is_one_of(name,
                                       {"insert_device",
                                        "sunny_get_device_count",
                                        "sunny_set_output_routing_type",
                                        "sunny_set_output_routing_channel"}));
        break;
    case PathKind::ReturnTrack:
        operation_allowed = (request.type == LomRequestType::SetProperty &&
                             is_one_of(name, {"name", "mute", "solo"})) ||
                            (request.type == LomRequestType::CallMethod &&
                             is_one_of(name,
                                       {"insert_device",
                                        "sunny_get_device_count",
                                        "sunny_set_output_routing_type",
                                        "sunny_set_output_routing_channel"}));
        break;
    case PathKind::MasterTrack:
        operation_allowed = request.type == LomRequestType::CallMethod &&
                            is_one_of(name, {"insert_device", "sunny_get_device_count"});
        break;
    case PathKind::ClipSlot:
        operation_allowed = request.type == LomRequestType::CallMethod &&
                            is_one_of(name, {"create_clip", "delete_clip"});
        break;
    case PathKind::Clip:
        operation_allowed =
            (request.type == LomRequestType::SetProperty && is_one_of(name,
                                                                      {"name",
                                                                       "signature_numerator",
                                                                       "signature_denominator",
                                                                       "start_marker",
                                                                       "end_marker",
                                                                       "looping",
                                                                       "muted",
                                                                       "launch_mode",
                                                                       "launch_quantization",
                                                                       "legato",
                                                                       "velocity_amount",
                                                                       "groove"})) ||
            (request.type == LomRequestType::CallMethod && is_one_of(name,
                                                                     {"add_new_notes",
                                                                      "get_notes_by_id",
                                                                      "get_notes_extended",
                                                                      "get_all_notes_extended",
                                                                      "sunny_clear_all_envelopes",
                                                                      "sunny_author_step_envelope",
                                                                      "sunny_get_step_envelope"}));
        break;
    case PathKind::VolumeParameter:
        operation_allowed = request.type == LomRequestType::SetProperty && name == "display_value";
        break;
    case PathKind::PanningParameter:
        operation_allowed = request.type == LomRequestType::SetProperty && name == "value";
        break;
    case PathKind::MasterPanningParameter:
    case PathKind::TrackActivatorParameter:
    case PathKind::ReturnActivatorParameter:
    case PathKind::MasterActivatorParameter:
        operation_allowed = request.type == LomRequestType::SetProperty && name == "value";
        break;
    case PathKind::SendParameter:
        operation_allowed = request.type == LomRequestType::SetProperty && name == "display_value";
        break;
    case PathKind::Device:
        operation_allowed = request.type == LomRequestType::CallMethod &&
                            is_one_of(name,
                                      {"sunny_get_device_parameter",
                                       "sunny_set_device_parameter",
                                       "sunny_resolve_native_display_value"});
        break;
    case PathKind::MixerDevice:
        operation_allowed = request.type == LomRequestType::SetProperty &&
                            is_one_of(name, {"crossfade_assign", "panning_mode"});
        break;
    case PathKind::MasterMixerDevice:
        operation_allowed = request.type == LomRequestType::SetProperty && name == "panning_mode";
        break;
    }
    if (!operation_allowed) return std::unexpected(sunny::core::ErrorCode::ProtocolError);

    if (request.type == LomRequestType::GetProperty)
        return args.empty() ? sunny::core::Result<void>{}
                            : std::unexpected(sunny::core::ErrorCode::ProtocolError);

    if (request.type == LomRequestType::SetProperty) {
        if (args.size() != 1) return std::unexpected(sunny::core::ErrorCode::ProtocolError);
        const auto& value = args.front();
        bool valid = false;
        if (*kind == PathKind::Song && name == "tempo")
            valid =
                finite_number(value) && value.get<double>() >= 20.0 && value.get<double>() <= 999.0;
        else if ((*kind == PathKind::Song || *kind == PathKind::Clip) &&
                 name == "signature_numerator") {
            const auto integer = json_to_int(value);
            valid = integer && *integer >= 1 && *integer <= 99;
        } else if ((*kind == PathKind::Song || *kind == PathKind::Clip) &&
                   name == "signature_denominator") {
            const auto integer = json_to_int(value);
            valid = integer && is_one_of(std::to_string(*integer), {"1", "2", "4", "8", "16"});
        } else if (name == "name")
            valid = value.is_string();
        else if (*kind == PathKind::Track && (name == "arm" || name == "implicit_arm"))
            valid = value.is_boolean() && !value.get<bool>();
        else if (*kind == PathKind::ReturnTrack && (name == "mute" || name == "solo"))
            valid = value.is_boolean() && !value.get<bool>();
        else if (name == "mute" || name == "solo" || name == "tempo_enabled" ||
                 name == "time_signature_enabled" || name == "looping" || name == "muted")
            valid = value.is_boolean();
        else if (*kind == PathKind::Clip && name == "launch_mode") {
            const auto integer = json_to_int(value);
            valid = integer && *integer == 0;
        } else if (*kind == PathKind::Clip && name == "launch_quantization") {
            const auto integer = json_to_int(value);
            valid = integer && *integer == 1;
        } else if (*kind == PathKind::Clip && name == "legato")
            valid = value.is_boolean() && !value.get<bool>();
        else if (*kind == PathKind::Clip && name == "velocity_amount")
            valid = value.is_number_float() && finite_number(value) && value.get<double>() == 0.0;
        else if (*kind == PathKind::Clip && name == "groove")
            valid = value.is_null();
        else if (*kind == PathKind::MixerDevice && name == "crossfade_assign") {
            const auto integer = json_to_int(value);
            valid = integer && *integer == 1;
        } else if ((*kind == PathKind::MixerDevice || *kind == PathKind::MasterMixerDevice) &&
                   name == "panning_mode") {
            const auto integer = json_to_int(value);
            valid = integer && *integer == 0;
        } else if (*kind == PathKind::TrackActivatorParameter) {
            valid = value.is_number_float() && finite_number(value) &&
                    (value.get<double>() == 0.0 || value.get<double>() == 1.0);
        } else if (*kind == PathKind::ReturnActivatorParameter ||
                   *kind == PathKind::MasterActivatorParameter) {
            valid = value.is_number_float() && value.get<double>() == 1.0;
        } else if (*kind == PathKind::MasterPanningParameter) {
            valid = value.is_number_float() && value.get<double>() == 0.0;
        } else if (*kind == PathKind::Clip && (name == "start_marker" || name == "end_marker"))
            valid = finite_number(value) && value.get<double>() >= 0.0;
        else
            valid = finite_number(value);
        return valid ? sunny::core::Result<void>{}
                     : std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }

    bool valid = false;
    if (*kind == PathKind::Song) {
        if (name.starts_with("sunny_managed_"))
            valid = valid_managed_request(name, args);
        else if (is_one_of(name,
                           {"sunny_get_target_profile",
                            "sunny_get_target_snapshot",
                            "sunny_get_scene_count",
                            "sunny_get_track_count",
                            "sunny_get_return_track_count",
                            "create_return_track"}))
            valid = args.empty();
        else if (name == "create_scene" || name == "create_midi_track")
            valid = args.size() == 1 && protocol_index(args[0], true);
        else if (name == "sunny_get_remote_log")
            valid = (args.size() == 1 || args.size() == 2) && protocol_index(args[0]) &&
                    (args.size() == 1 ||
                     (args[1].is_string() && args[1].get_ref<const std::string&>().size() == 32 &&
                      std::ranges::all_of(args[1].get_ref<const std::string&>(), [](char c) {
                          return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                      })));
        else if (name == "sunny_set_cue")
            valid = args.size() == 2 && finite_number(args[0]) && args[0].get<double>() >= 0.0 &&
                    args[1].is_string();
    } else if (*kind == PathKind::Track || *kind == PathKind::ReturnTrack ||
               *kind == PathKind::MasterTrack) {
        if (name == "sunny_get_device_count")
            valid = args.empty();
        else if ((*kind == PathKind::Track || *kind == PathKind::ReturnTrack) &&
                 name == "sunny_set_output_routing_type")
            valid = args.size() == 1 && valid_routing_dictionary(args[0]);
        else if ((*kind == PathKind::Track || *kind == PathKind::ReturnTrack) &&
                 name == "sunny_set_output_routing_channel")
            valid = args.size() == 2 && valid_routing_dictionary(args[0]) &&
                    valid_routing_dictionary(args[1]);
        else if (name == "insert_device")
            valid = (args.size() == 1 || args.size() == 2) && args[0].is_string() &&
                    !args[0].get_ref<const std::string&>().empty() &&
                    (args.size() == 1 || protocol_index(args[1]));
    } else if (*kind == PathKind::ClipSlot) {
        valid = name == "delete_clip"
                    ? args.empty()
                    : args.size() == 1 && finite_number(args[0]) && args[0].get<double>() > 0.0;
    } else if (*kind == PathKind::Clip) {
        if (name == "sunny_clear_all_envelopes")
            valid = args.empty();
        else
            valid = args.size() == 1 &&
                    (name == "add_new_notes"                ? valid_note_dictionary(args[0])
                     : name == "get_notes_by_id"            ? valid_note_id_query(args[0])
                     : name == "get_all_notes_extended"     ? valid_all_notes_query(args[0])
                     : name == "get_notes_extended"         ? valid_ranged_notes_query(args[0])
                     : name == "sunny_author_step_envelope" ? valid_step_envelope_author(args[0])
                     : name == "sunny_get_step_envelope"    ? valid_step_envelope_query(args[0])
                                                            : false);
    } else if (*kind == PathKind::Device) {
        if (name == "sunny_resolve_native_display_value") {
            if (args.size() == 1 && args[0].is_object() && args[0].size() == 3 &&
                args[0].contains("capability_id") && args[0]["capability_id"].is_string() &&
                args[0].contains("target") && finite_number(args[0]["target"]) &&
                args[0].contains("tolerance") && finite_number(args[0]["tolerance"]) &&
                args[0]["tolerance"].get<double>() >= 0.0) {
                const auto registry = sunny::core::live_native_parameter_registry();
                const auto capability =
                    std::find_if(registry.begin(), registry.end(), [&](const auto& entry) {
                        return entry.id == args[0]["capability_id"].get_ref<const std::string&>();
                    });
                valid = capability != registry.end() &&
                        capability->kind == sunny::core::LiveNativeParameterKind::Continuous &&
                        capability->physical_unit.has_value();
            }
        } else if (name == "sunny_get_device_parameter")
            valid = args.size() == 2 && args[0].is_string() &&
                    !args[0].get_ref<const std::string&>().empty() && args[1].is_string() &&
                    is_one_of(args[1].get_ref<const std::string&>(), {"value", "display_value"});
        else
            valid = args.size() == 5 && args[0].is_string() &&
                    !args[0].get_ref<const std::string&>().empty() && finite_number(args[1]) &&
                    args[2].is_string() &&
                    is_one_of(args[2].get_ref<const std::string&>(), {"value", "display_value"}) &&
                    finite_number(args[3]) && finite_number(args[4]) &&
                    args[4].get<double>() > args[3].get<double>();
    }
    return valid ? sunny::core::Result<void>{}
                 : std::unexpected(sunny::core::ErrorCode::ProtocolError);
}

sunny::core::Result<void> LomProtocol::validate_notes(const LomPath& clip_path,
                                                      const std::vector<LomNoteData>& notes) {
    return validate_request(add_new_notes(clip_path, notes));
}

std::string LomProtocol::serialize_request(const LomRequest& request) {
    json j;

    j["bridge_protocol_version"] = SUNNY_BRIDGE_PROTOCOL_VERSION;

    switch (request.type) {
    case LomRequestType::GetProperty:
        j["type"] = "get";
        break;
    case LomRequestType::SetProperty:
        j["type"] = "set";
        break;
    case LomRequestType::CallMethod:
        j["type"] = "call";
        break;
    }

    j["path"] = request.path.to_string();
    j["name"] = request.property_or_method;

    if (!request.args.empty()) {
        json args_array = json::array();
        for (const auto& arg : request.args) {
            args_array.push_back(lom_value_to_json(arg));
        }
        j["args"] = args_array;
    }

    return j.dump();
}

sunny::core::Result<LomRequest> LomProtocol::deserialize_request(const json& value) {
    try {
        if (!value.is_object() || !value.contains("bridge_protocol_version") ||
            !value.at("bridge_protocol_version").is_number_unsigned() ||
            value.at("bridge_protocol_version").get<std::uint64_t>() !=
                SUNNY_BRIDGE_PROTOCOL_VERSION ||
            !value.contains("type") || !value.at("type").is_string() || !value.contains("path") ||
            !value.at("path").is_string() || !value.contains("name") ||
            !value.at("name").is_string())
            return std::unexpected(sunny::core::ErrorCode::ProtocolError);

        for (auto field = value.begin(); field != value.end(); ++field) {
            if (field.key() != "bridge_protocol_version" && field.key() != "type" &&
                field.key() != "path" && field.key() != "name" && field.key() != "args")
                return std::unexpected(sunny::core::ErrorCode::ProtocolError);
        }

        LomRequest request;
        const auto& type = value.at("type").get_ref<const std::string&>();
        if (type == "get")
            request.type = LomRequestType::GetProperty;
        else if (type == "set")
            request.type = LomRequestType::SetProperty;
        else if (type == "call")
            request.type = LomRequestType::CallMethod;
        else
            return std::unexpected(sunny::core::ErrorCode::ProtocolError);

        request.path = LomPath::parse(value.at("path").get<std::string>());
        request.property_or_method = value.at("name").get<std::string>();
        if (!request.path.is_canonical() ||
            request.path.to_string() != value.at("path").get_ref<const std::string&>() ||
            request.property_or_method.empty())
            return std::unexpected(sunny::core::ErrorCode::ProtocolError);

        if (value.contains("args")) {
            if (!value.at("args").is_array() || value.at("args").empty())
                return std::unexpected(sunny::core::ErrorCode::ProtocolError);
            request.args.reserve(value.at("args").size());
            for (const auto& argument : value.at("args")) {
                auto decoded = json_to_lom_value(argument);
                if (!decoded) return std::unexpected(sunny::core::ErrorCode::ProtocolError);
                request.args.push_back(std::move(*decoded));
            }
        }

        if (!validate_request(request) || json::parse(serialize_request(request)) != value)
            return std::unexpected(sunny::core::ErrorCode::ProtocolError);
        return request;
    } catch (const json::exception&) {
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }
}

std::optional<LomResponse> LomProtocol::deserialize_response(const std::string& input) {
    try {
        const auto j = json::parse(input);
        const auto permitted = [](const std::string& key) {
            return key == "bridge_protocol_version" || key == "success" || key == "value" ||
                   key == "error";
        };
        const bool has_unknown_field = [&] {
            for (auto field = j.begin(); field != j.end(); ++field)
                if (!permitted(field.key())) return true;
            return false;
        }();
        if (!j.is_object() || !j.contains("bridge_protocol_version") || has_unknown_field ||
            !j.at("bridge_protocol_version").is_number_unsigned() ||
            j.at("bridge_protocol_version").get<std::uint64_t>() != SUNNY_BRIDGE_PROTOCOL_VERSION ||
            !j.contains("success") || !j.at("success").is_boolean() ||
            (j.contains("error") && !j.at("error").is_string()) ||
            (j.at("success").get<bool>() && j.contains("error")) ||
            (!j.at("success").get<bool>() && (!j.contains("error") || j.contains("value"))))
            return std::nullopt;

        LomResponse response;
        response.success = j.at("success").get<bool>();
        if (j.contains("value") && !j.at("value").is_null())
            response.value = json_to_lom_value(j.at("value"));
        if (j.contains("value") && !j.at("value").is_null() && !response.value) return std::nullopt;
        if (j.contains("error")) response.error = j.at("error").get<std::string>();
        return response;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

std::string LomProtocol::serialize_notes(const std::vector<LomNoteData>& notes) {
    return notes_to_json(notes).dump();
}

LomRequest LomProtocol::add_new_notes(const LomPath& clip_path,
                                      const std::vector<LomNoteData>& notes) {
    return call_method(clip_path, "add_new_notes", {json{{"notes", notes_to_json(notes)}}});
}

sunny::core::Result<LomNoteData> LomProtocol::from_note_event(const sunny::core::NoteEvent& event) {
    if (event.start_time.numerator() < 0 || event.duration.numerator() <= 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidBeat);
    if (!sunny::core::is_valid_velocity(event.velocity))
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);

    LomNoteData data;
    data.pitch = event.pitch;
    // Sunny Beat values are whole-note fractions. The Live Object Model's
    // `beats` scalar is explicitly a quarter-note count.
    data.start_time = static_cast<double>(event.start_time.numerator()) * 4.0 /
                      static_cast<double>(event.start_time.denominator());
    data.duration = static_cast<double>(event.duration.numerator()) * 4.0 /
                    static_cast<double>(event.duration.denominator());
    data.velocity = event.velocity;
    data.muted = event.muted;
    data.release_velocity = static_cast<double>(event.release_velocity);
    return data;
}

// =============================================================================
// Request builders
// =============================================================================

LomRequest LomProtocol::get_property(const LomPath& path, const std::string& property) {
    LomRequest req;
    req.type = LomRequestType::GetProperty;
    req.path = path;
    req.property_or_method = property;
    return req;
}

LomRequest
LomProtocol::set_property(const LomPath& path, const std::string& property, const LomValue& value) {
    LomRequest req;
    req.type = LomRequestType::SetProperty;
    req.path = path;
    req.property_or_method = property;
    req.args.push_back(value);
    return req;
}

LomRequest LomProtocol::call_method(const LomPath& path,
                                    const std::string& method,
                                    const std::vector<LomValue>& args) {
    LomRequest req;
    req.type = LomRequestType::CallMethod;
    req.path = path;
    req.property_or_method = method;
    req.args = args;
    return req;
}

} // namespace sunny::infrastructure
