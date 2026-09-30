/**
 * @file target_snapshot.cpp
 * @brief Ableton target-snapshot trust-boundary validation
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <set>
#include <string_view>
#include <sunny/infrastructure/ableton/target_snapshot.hpp>
#include <utility>

namespace sunny::infrastructure {

using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;

namespace {

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool valid_u32(const json& value) {
    if (value.is_number_unsigned())
        return value.get<std::uint64_t>() <= std::numeric_limits<std::uint32_t>::max();
    return value.is_number_integer() && value.get<std::int64_t>() >= 0 &&
           static_cast<std::uint64_t>(value.get<std::int64_t>()) <=
               std::numeric_limits<std::uint32_t>::max();
}

bool valid_finite_number(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool valid_opaque_json(const json& value) {
    if (value.is_number_float()) return std::isfinite(value.get<double>());
    if (value.is_array() || value.is_object())
        return std::all_of(value.begin(), value.end(), valid_opaque_json);
    return value.is_null() || value.is_boolean() || value.is_number_integer() ||
           value.is_number_unsigned() || value.is_string();
}

bool valid_nonnegative_lom_int(const json& value) {
    return valid_u32(value) &&
           value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
}

bool valid_scale_state(const json& scale) {
    if (!exact_fields(scale, {"root_note", "name", "intervals", "mode"}) ||
        !valid_u32(scale.at("root_note")) || scale.at("root_note").get<std::uint32_t>() > 11 ||
        !scale.at("name").is_string() || !scale.at("intervals").is_array() ||
        scale.at("intervals").empty() || scale.at("intervals").size() > 128 ||
        !scale.at("mode").is_boolean())
        return false;
    return std::all_of(
        scale.at("intervals").begin(), scale.at("intervals").end(), [](const auto& interval) {
            if (interval.is_number_unsigned())
                return interval.template get<std::uint64_t>() <=
                       static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
            return interval.is_number_integer() &&
                   interval.template get<std::int64_t>() >=
                       std::numeric_limits<std::int32_t>::min() &&
                   interval.template get<std::int64_t>() <=
                       std::numeric_limits<std::int32_t>::max();
        });
}

bool valid_tuning_state(const json& tuning) {
    if (!exact_fields(tuning,
                      {"name",
                       "pseudo_octave_in_cents",
                       "lowest_note",
                       "highest_note",
                       "reference_pitch",
                       "note_tunings"}) ||
        !tuning.at("name").is_string() || !tuning.at("pseudo_octave_in_cents").is_number_float() ||
        !valid_finite_number(tuning.at("pseudo_octave_in_cents")) ||
        tuning.at("pseudo_octave_in_cents").get<double>() <= 0.0 ||
        !tuning.at("lowest_note").is_object() || !valid_opaque_json(tuning.at("lowest_note")) ||
        !tuning.at("highest_note").is_object() || !valid_opaque_json(tuning.at("highest_note")) ||
        !tuning.at("reference_pitch").is_object() ||
        !valid_opaque_json(tuning.at("reference_pitch")))
        return false;

    const auto& note_tunings = tuning.at("note_tunings");
    if (!note_tunings.is_object() || note_tunings.size() != 1) return false;
    const auto entry = note_tunings.begin();
    const auto& values = entry.value();
    return values.is_array() && std::all_of(values.begin(), values.end(), valid_finite_number);
}

bool valid_routing_dictionary(const json& routing) {
    return exact_fields(routing, {"display_name", "identifier"}) &&
           routing.at("display_name").is_string() && routing.at("identifier").is_string();
}

bool valid_routing_collection(const json& collection,
                              std::string_view field,
                              const json& selected) {
    const auto key = std::string(field);
    if (!collection.is_object() || collection.size() != 1 || !collection.contains(key) ||
        !collection.at(key).is_array())
        return false;
    const auto& options = collection.at(key);
    return std::all_of(options.begin(), options.end(), valid_routing_dictionary) &&
           std::find(options.begin(), options.end(), selected) != options.end();
}

bool valid_input_routing_state(const json& track) {
    if (!track.at("has_audio_input").is_boolean() || !track.at("has_midi_input").is_boolean())
        return false;
    const bool input_capable =
        track.at("has_audio_input").get<bool>() || track.at("has_midi_input").get<bool>();
    if (!input_capable)
        return track.at("input_routing_type").is_null() &&
               track.at("input_routing_channel").is_null() &&
               track.at("available_input_routing_types").is_null() &&
               track.at("available_input_routing_channels").is_null();
    return valid_routing_dictionary(track.at("input_routing_type")) &&
           valid_routing_dictionary(track.at("input_routing_channel")) &&
           valid_routing_collection(track.at("available_input_routing_types"),
                                    "available_input_routing_types",
                                    track.at("input_routing_type")) &&
           valid_routing_collection(track.at("available_input_routing_channels"),
                                    "available_input_routing_channels",
                                    track.at("input_routing_channel"));
}

bool valid_track_meter_state(const json& track) {
    if (!track.at("has_audio_output").is_boolean()) return false;
    const bool audio_or_midi_track =
        track.at("has_audio_input").get<bool>() || track.at("has_midi_input").get<bool>();
    const auto valid_level = [](const json& value) {
        return value.is_number_float() && valid_finite_number(value) &&
               value.get<double>() >= 0.0 && value.get<double>() <= 1.0;
    };
    const bool valid_hold_levels =
        audio_or_midi_track
            ? valid_level(track.at("input_meter_level")) &&
                  valid_level(track.at("output_meter_level"))
            : track.at("input_meter_level").is_null() && track.at("output_meter_level").is_null();
    const bool has_audio_output = track.at("has_audio_output").get<bool>();
    const auto valid_momentary_level = [&](std::string_view field) {
        const auto& value = track.at(std::string(field));
        return has_audio_output ? valid_level(value) : value.is_null();
    };
    return valid_hold_levels && valid_momentary_level("input_meter_left") &&
           valid_momentary_level("input_meter_right") &&
           valid_momentary_level("output_meter_left") &&
           valid_momentary_level("output_meter_right");
}

bool valid_live_signature_numerator(const json& value) {
    return valid_u32(value) && value.get<std::uint32_t>() >= 1 && value.get<std::uint32_t>() <= 99;
}

bool valid_live_signature_denominator(const json& value) {
    if (!valid_u32(value)) return false;
    const auto denominator = value.get<std::uint32_t>();
    return denominator == 1 || denominator == 2 || denominator == 4 || denominator == 8 ||
           denominator == 16;
}

bool valid_device(const json& device) {
    if (!exact_fields(device,
                      {"name",
                       "class_display_name",
                       "class_name",
                       "type",
                       "is_active",
                       "can_have_chains",
                       "latency_in_samples",
                       "latency_in_ms"}) ||
        !device.at("name").is_string() || !device.at("class_display_name").is_string() ||
        !device.at("class_name").is_string() || !valid_u32(device.at("type")) ||
        !device.at("is_active").is_boolean() || !device.at("can_have_chains").is_boolean() ||
        !valid_nonnegative_lom_int(device.at("latency_in_samples")) ||
        !device.at("latency_in_ms").is_number_float() ||
        !valid_finite_number(device.at("latency_in_ms")) ||
        device.at("latency_in_ms").get<double>() < 0.0)
        return false;
    const auto type = device.at("type").get<std::uint32_t>();
    return type == 0 || type == 1 || type == 2 || type == 4;
}

bool valid_devices(const json& devices) {
    return devices.is_array() && std::all_of(devices.begin(), devices.end(), [](const auto& value) {
               return valid_device(value);
           });
}

bool valid_parameter_state(const json& value) {
    return valid_u32(value) && value.get<std::uint32_t>() <= 2;
}

bool valid_parameter_domain(const json& parameter) {
    const bool quantized = parameter.at("is_quantized").get<bool>();
    if (quantized) {
        if (!parameter.at("default_value").is_null() || !parameter.at("value_items").is_array())
            return false;
        return std::all_of(parameter.at("value_items").begin(),
                           parameter.at("value_items").end(),
                           [](const auto& item) { return item.is_string(); });
    }
    return parameter.at("default_value").is_number_float() &&
           valid_finite_number(parameter.at("default_value")) &&
           parameter.at("default_value").get<double>() >= parameter.at("minimum").get<double>() &&
           parameter.at("default_value").get<double>() <= parameter.at("maximum").get<double>() &&
           parameter.at("value_items").is_null();
}

bool valid_mixer_parameter(const json& parameter) {
    return exact_fields(parameter,
                        {"value",
                         "display_value",
                         "minimum",
                         "maximum",
                         "is_quantized",
                         "default_value",
                         "value_items",
                         "state",
                         "automation_state",
                         "is_enabled"}) &&
           parameter.at("value").is_number_float() && valid_finite_number(parameter.at("value")) &&
           parameter.at("display_value").is_number_float() &&
           valid_finite_number(parameter.at("display_value")) &&
           parameter.at("minimum").is_number_float() &&
           valid_finite_number(parameter.at("minimum")) &&
           parameter.at("maximum").is_number_float() &&
           valid_finite_number(parameter.at("maximum")) &&
           parameter.at("maximum").get<double>() >= parameter.at("minimum").get<double>() &&
           parameter.at("value").get<double>() >= parameter.at("minimum").get<double>() &&
           parameter.at("value").get<double>() <= parameter.at("maximum").get<double>() &&
           parameter.at("is_quantized").is_boolean() && valid_parameter_domain(parameter) &&
           valid_parameter_state(parameter.at("state")) &&
           valid_parameter_state(parameter.at("automation_state")) &&
           parameter.at("is_enabled").is_boolean();
}

bool valid_mixer(const json& mixer, bool crossfade_assign_available) {
    if (!exact_fields(mixer,
                      {"volume",
                       "track_activator",
                       "panning",
                       "sends",
                       "crossfade_assign",
                       "panning_mode"}) ||
        !valid_mixer_parameter(mixer.at("volume")) ||
        !valid_mixer_parameter(mixer.at("track_activator")) ||
        !valid_mixer_parameter(mixer.at("panning")) || !mixer.at("sends").is_array() ||
        !valid_u32(mixer.at("panning_mode")) || mixer.at("panning_mode").get<std::uint32_t>() > 1 ||
        (crossfade_assign_available ? (!valid_u32(mixer.at("crossfade_assign")) ||
                                       mixer.at("crossfade_assign").get<std::uint32_t>() > 2)
                                    : !mixer.at("crossfade_assign").is_null()))
        return false;
    return std::all_of(mixer.at("sends").begin(),
                       mixer.at("sends").end(),
                       [](const auto& parameter) { return valid_mixer_parameter(parameter); });
}

bool valid_clip(const json& clip,
                std::uint32_t slot_count,
                bool live_11_content_state_available,
                std::set<std::uint32_t>& slots) {
    if (!exact_fields(clip,
                      {"slot",
                       "name",
                       "is_audio_clip",
                       "is_midi_clip",
                       "is_arrangement_clip",
                       "is_session_clip",
                       "is_take_lane_clip",
                       "length",
                       "signature_numerator",
                       "signature_denominator",
                       "start_marker",
                       "end_marker",
                       "end_time",
                       "looping",
                       "muted",
                       "has_envelopes",
                       "is_playing",
                       "is_recording",
                       "is_overdubbing",
                       "is_triggered",
                       "will_record_on_start",
                       "launch_mode",
                       "launch_quantization",
                       "legato",
                       "velocity_amount",
                       "has_groove"}) ||
        !valid_u32(clip.at("slot")) || !clip.at("name").is_string() ||
        !clip.at("is_audio_clip").is_boolean() || !clip.at("is_midi_clip").is_boolean() ||
        clip.at("is_audio_clip").get<bool>() == clip.at("is_midi_clip").get<bool>() ||
        !clip.at("is_arrangement_clip").is_boolean() ||
        clip.at("is_arrangement_clip").get<bool>() || !valid_finite_number(clip.at("length")) ||
        clip.at("length").get<double>() < 0.0 ||
        !valid_live_signature_numerator(clip.at("signature_numerator")) ||
        !valid_live_signature_denominator(clip.at("signature_denominator")) ||
        !valid_finite_number(clip.at("start_marker")) ||
        !valid_finite_number(clip.at("end_marker")) || !clip.at("end_time").is_number_float() ||
        !valid_finite_number(clip.at("end_time")) ||
        clip.at("end_marker").get<double>() < clip.at("start_marker").get<double>() ||
        !clip.at("looping").is_boolean() || !clip.at("muted").is_boolean() ||
        !clip.at("has_envelopes").is_boolean() || !clip.at("is_playing").is_boolean() ||
        !clip.at("is_recording").is_boolean() || !clip.at("is_overdubbing").is_boolean() ||
        !clip.at("is_triggered").is_boolean() || !clip.at("will_record_on_start").is_boolean() ||
        (live_11_content_state_available
             ? (!clip.at("is_session_clip").is_boolean() ||
                !clip.at("is_session_clip").get<bool>() ||
                !clip.at("is_take_lane_clip").is_boolean() ||
                clip.at("is_take_lane_clip").get<bool>() || !valid_u32(clip.at("launch_mode")) ||
                clip.at("launch_mode").get<std::uint32_t>() > 3 ||
                !valid_u32(clip.at("launch_quantization")) ||
                clip.at("launch_quantization").get<std::uint32_t>() > 14 ||
                !clip.at("legato").is_boolean() || !clip.at("velocity_amount").is_number_float() ||
                !valid_finite_number(clip.at("velocity_amount")) ||
                clip.at("velocity_amount").get<double>() < 0.0 ||
                clip.at("velocity_amount").get<double>() > 1.0)
             : (!clip.at("is_session_clip").is_null() || !clip.at("is_take_lane_clip").is_null() ||
                !clip.at("launch_mode").is_null() || !clip.at("launch_quantization").is_null() ||
                !clip.at("legato").is_null() || !clip.at("velocity_amount").is_null())) ||
        (live_11_content_state_available ? !clip.at("has_groove").is_boolean()
                                         : !clip.at("has_groove").is_null()))
        return false;
    if (!clip.at("looping").get<bool>() &&
        clip.at("end_time").get<double>() != clip.at("end_marker").get<double>())
        return false;
    const auto slot = clip.at("slot").get<std::uint32_t>();
    return slot < slot_count && slots.insert(slot).second;
}

bool valid_clip_slot(const json& slot, std::uint32_t expected_index, bool& has_clip) {
    if (!exact_fields(slot,
                      {"slot",
                       "has_clip",
                       "has_stop_button",
                       "is_group_slot",
                       "controls_other_clips",
                       "is_playing",
                       "is_recording",
                       "is_triggered",
                       "playing_status",
                       "will_record_on_start"}) ||
        !valid_u32(slot.at("slot")) || slot.at("slot").get<std::uint32_t>() != expected_index ||
        !slot.at("has_clip").is_boolean() || !slot.at("has_stop_button").is_boolean() ||
        !slot.at("is_group_slot").is_boolean() || !slot.at("controls_other_clips").is_boolean() ||
        !slot.at("is_playing").is_boolean() || !slot.at("is_recording").is_boolean() ||
        !slot.at("is_triggered").is_boolean() || !valid_u32(slot.at("playing_status")) ||
        slot.at("playing_status").get<std::uint32_t>() > 2 ||
        !slot.at("will_record_on_start").is_boolean())
        return false;
    const auto playing_status = slot.at("playing_status").get<std::uint32_t>();
    const bool is_group_slot = slot.at("is_group_slot").get<bool>();
    if (slot.at("is_playing").get<bool>() != (playing_status != 0) ||
        slot.at("is_recording").get<bool>() != (playing_status == 2) ||
        (!is_group_slot && (playing_status != 0 || slot.at("controls_other_clips").get<bool>())))
        return false;
    has_clip = slot.at("has_clip").get<bool>();
    return true;
}

bool valid_track(const json& track, bool live_11_content_state_available) {
    if (!exact_fields(track,
                      {"name",
                       "devices",
                       "mixer",
                       "clip_slot_count",
                       "arrangement_clip_count",
                       "take_lane_count",
                       "clip_slots",
                       "clips",
                       "group_track_index",
                       "input_routing_type",
                       "input_routing_channel",
                       "available_input_routing_types",
                       "available_input_routing_channels",
                       "input_meter_level",
                       "output_meter_level",
                       "input_meter_left",
                       "input_meter_right",
                       "output_meter_left",
                       "output_meter_right",
                       "output_routing_type",
                       "output_routing_channel",
                       "available_output_routing_types",
                       "available_output_routing_channels",
                       "has_audio_input",
                       "has_midi_input",
                       "has_audio_output",
                       "has_midi_output",
                       "is_frozen",
                       "arm",
                       "implicit_arm",
                       "back_to_arranger",
                       "fired_slot_index",
                       "playing_slot_index",
                       "mute",
                       "solo",
                       "muted_via_solo"}) ||
        !track.at("name").is_string() || !valid_devices(track.at("devices")) ||
        !valid_mixer(track.at("mixer"), true) || !valid_u32(track.at("clip_slot_count")) ||
        (live_11_content_state_available ? !valid_u32(track.at("arrangement_clip_count"))
                                         : !track.at("arrangement_clip_count").is_null()) ||
        (live_11_content_state_available ? !valid_u32(track.at("take_lane_count"))
                                         : !track.at("take_lane_count").is_null()) ||
        !track.at("clip_slots").is_array() || !track.at("clips").is_array() ||
        !(track.at("group_track_index").is_null() || valid_u32(track.at("group_track_index"))) ||
        !valid_input_routing_state(track) || !valid_track_meter_state(track) ||
        !valid_routing_dictionary(track.at("output_routing_type")) ||
        !valid_routing_dictionary(track.at("output_routing_channel")) ||
        !valid_routing_collection(track.at("available_output_routing_types"),
                                  "available_output_routing_types",
                                  track.at("output_routing_type")) ||
        !valid_routing_collection(track.at("available_output_routing_channels"),
                                  "available_output_routing_channels",
                                  track.at("output_routing_channel")) ||
        !track.at("has_audio_output").is_boolean() || !track.at("has_midi_output").is_boolean() ||
        !track.at("is_frozen").is_boolean() || !track.at("arm").is_boolean() ||
        !track.at("implicit_arm").is_boolean() || !track.at("back_to_arranger").is_boolean() ||
        !track.at("mute").is_boolean() || !track.at("solo").is_boolean() ||
        !track.at("muted_via_solo").is_boolean())
        return false;

    const auto slot_count = track.at("clip_slot_count").get<std::uint32_t>();
    if (track.at("clip_slots").size() != slot_count) return false;
    const auto valid_slot_state = [slot_count](const json& value) {
        if (value.is_number_unsigned()) return value.get<std::uint64_t>() < slot_count;
        if (!value.is_number_integer()) return false;
        const auto index = value.get<std::int64_t>();
        return index == -2 || index == -1 ||
               (index >= 0 && static_cast<std::uint64_t>(index) < slot_count);
    };
    if (!valid_slot_state(track.at("fired_slot_index")) ||
        !valid_slot_state(track.at("playing_slot_index")))
        return false;
    std::set<std::uint32_t> occupied_slots;
    for (std::uint32_t index = 0; index < slot_count; ++index) {
        bool has_clip = false;
        if (!valid_clip_slot(track.at("clip_slots").at(index), index, has_clip)) return false;
        if (has_clip) occupied_slots.insert(index);
    }
    std::set<std::uint32_t> slots;
    for (const auto& clip : track.at("clips"))
        if (!valid_clip(clip, slot_count, live_11_content_state_available, slots)) return false;
    return slots == occupied_slots;
}

bool valid_track_like(const json& track, bool crossfade_assign_available) {
    return exact_fields(track, {"name", "devices", "mixer"}) && track.at("name").is_string() &&
           valid_devices(track.at("devices")) &&
           valid_mixer(track.at("mixer"), crossfade_assign_available);
}

bool valid_return_track(const json& track) {
    return exact_fields(track,
                        {"name",
                         "devices",
                         "mixer",
                         "output_routing_type",
                         "output_routing_channel",
                         "available_output_routing_types",
                         "available_output_routing_channels",
                         "mute",
                         "solo",
                         "muted_via_solo"}) &&
           track.at("name").is_string() && valid_devices(track.at("devices")) &&
           valid_mixer(track.at("mixer"), true) &&
           valid_routing_dictionary(track.at("output_routing_type")) &&
           valid_routing_dictionary(track.at("output_routing_channel")) &&
           valid_routing_collection(track.at("available_output_routing_types"),
                                    "available_output_routing_types",
                                    track.at("output_routing_type")) &&
           valid_routing_collection(track.at("available_output_routing_channels"),
                                    "available_output_routing_channels",
                                    track.at("output_routing_channel")) &&
           track.at("mute").is_boolean() && track.at("solo").is_boolean() &&
           track.at("muted_via_solo").is_boolean();
}

bool valid_cue(const json& cue) {
    return exact_fields(cue, {"name", "time"}) && cue.at("name").is_string() &&
           valid_finite_number(cue.at("time")) && cue.at("time").get<double>() >= 0.0;
}

bool is_integer_minus_one(const json& value) {
    return value.is_number_integer() && value.get<std::int64_t>() == -1;
}

bool valid_scene(const json& scene) {
    if (!exact_fields(scene,
                      {"name",
                       "is_triggered",
                       "tempo_enabled",
                       "tempo",
                       "time_signature_enabled",
                       "time_signature_numerator",
                       "time_signature_denominator"}) ||
        !scene.at("name").is_string() || !scene.at("is_triggered").is_boolean() ||
        !scene.at("tempo_enabled").is_boolean() || !scene.at("time_signature_enabled").is_boolean())
        return false;

    const bool tempo_enabled = scene.at("tempo_enabled").get<bool>();
    if (tempo_enabled) {
        if (!valid_finite_number(scene.at("tempo")) || scene.at("tempo").get<double>() == -1.0)
            return false;
    } else if (!valid_finite_number(scene.at("tempo")) || scene.at("tempo").get<double>() != -1.0) {
        return false;
    }

    const bool signature_enabled = scene.at("time_signature_enabled").get<bool>();
    if (signature_enabled) {
        return valid_live_signature_numerator(scene.at("time_signature_numerator")) &&
               valid_live_signature_denominator(scene.at("time_signature_denominator"));
    }
    return is_integer_minus_one(scene.at("time_signature_numerator")) &&
           is_integer_minus_one(scene.at("time_signature_denominator"));
}

bool valid_song_state(const json& song, const AbletonTargetProfile& profile) {
    if (!exact_fields(song,
                      {"tempo",
                       "signature_numerator",
                       "signature_denominator",
                       "is_playing",
                       "is_counting_in",
                       "arrangement_overdub",
                       "overdub",
                       "record_mode",
                       "session_record",
                       "session_automation_record",
                       "is_ableton_link_enabled",
                       "is_ableton_link_start_stop_sync_enabled",
                       "tempo_follower_enabled",
                       "nudge_down",
                       "nudge_up",
                       "back_to_arranger",
                       "re_enable_automation_enabled",
                       "loop",
                       "metronome",
                       "scale",
                       "tuning_system",
                       "scene_count",
                       "scenes",
                       "tracks",
                       "return_tracks",
                       "master_track",
                       "cue_points"}) ||
        !valid_finite_number(song.at("tempo")) || song.at("tempo").get<double>() < 20.0 ||
        song.at("tempo").get<double>() > 999.0 ||
        !valid_live_signature_numerator(song.at("signature_numerator")) ||
        !valid_live_signature_denominator(song.at("signature_denominator")) ||
        !song.at("is_playing").is_boolean() || !song.at("is_counting_in").is_boolean() ||
        !song.at("arrangement_overdub").is_boolean() || !song.at("overdub").is_boolean() ||
        !song.at("record_mode").is_boolean() || !song.at("session_record").is_boolean() ||
        !song.at("session_automation_record").is_boolean() ||
        !song.at("is_ableton_link_enabled").is_boolean() ||
        !song.at("is_ableton_link_start_stop_sync_enabled").is_boolean() ||
        !song.at("tempo_follower_enabled").is_boolean() || !song.at("nudge_down").is_boolean() ||
        !song.at("nudge_up").is_boolean() || !song.at("back_to_arranger").is_boolean() ||
        !song.at("re_enable_automation_enabled").is_boolean() || !song.at("loop").is_boolean() ||
        !song.at("metronome").is_boolean() || !valid_u32(song.at("scene_count")) ||
        !song.at("scenes").is_array() || !song.at("tracks").is_array() ||
        !song.at("return_tracks").is_array() || !valid_track_like(song.at("master_track"), false) ||
        !song.at("cue_points").is_array())
        return false;

    const bool scale_available = profile.live_version.at_least(12, 0, 5);
    if (scale_available != !song.at("scale").is_null() ||
        (scale_available && !valid_scale_state(song.at("scale"))))
        return false;
    const bool tuning_available = profile.live_version.at_least(12, 1);
    if (tuning_available != !song.at("tuning_system").is_null() ||
        (tuning_available && !valid_tuning_state(song.at("tuning_system"))))
        return false;

    const auto scene_count = song.at("scene_count").get<std::uint32_t>();
    if (song.at("scenes").size() != scene_count) return false;
    for (const auto& scene : song.at("scenes"))
        if (!valid_scene(scene)) return false;
    const auto return_count = song.at("return_tracks").size();
    const bool live_11_content_state_available = profile.live_version.at_least(11, 0);
    for (const auto& track : song.at("tracks"))
        if (!valid_track(track, live_11_content_state_available) ||
            track.at("clip_slot_count").get<std::uint32_t>() != scene_count ||
            track.at("mixer").at("sends").size() != return_count)
            return false;
    const auto track_count = song.at("tracks").size();
    for (const auto& track : song.at("tracks")) {
        if (!track.at("group_track_index").is_null() &&
            track.at("group_track_index").get<std::uint64_t>() >= track_count)
            return false;
    }
    for (const auto& track : song.at("return_tracks"))
        if (!valid_return_track(track)) return false;
    for (const auto& cue : song.at("cue_points"))
        if (!valid_cue(cue)) return false;
    return true;
}

} // namespace

Result<AbletonTargetSnapshot> target_snapshot_from_json(const json& value) {
    try {
        if (!exact_fields(value, {"schema_version", "target_profile", "song"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            value.at("schema_version").get<std::uint64_t>() != SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION)
            return std::unexpected(ErrorCode::ProtocolError);

        auto profile = target_profile_from_json(value.at("target_profile"));
        if (!profile) return std::unexpected(profile.error());
        if (!valid_song_state(value.at("song"), *profile))
            return std::unexpected(ErrorCode::ProtocolError);
        return AbletonTargetSnapshot{
            SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION, std::move(*profile), value.at("song")};
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}

json target_snapshot_to_json(const AbletonTargetSnapshot& snapshot) {
    return {{"schema_version", snapshot.schema_version},
            {"target_profile", target_profile_to_json(snapshot.target_profile)},
            {"song", snapshot.song_state}};
}

namespace {

// Track meters are peak-hold and momentary levels that follow the signal while
// the Set plays. They are evidence of activity, not of the structure a plan
// depends on, so comparing them would refuse an apply whenever anything sounds.
// Whether a meter is present at all follows the track's reported inputs and
// outputs, which remain compared.
constexpr std::array<std::string_view, 6> volatile_track_fields{"input_meter_level",
                                                                "output_meter_level",
                                                                "input_meter_left",
                                                                "input_meter_right",
                                                                "output_meter_left",
                                                                "output_meter_right"};

json structural_fingerprint(const AbletonTargetSnapshot& snapshot) {
    auto fingerprint = target_snapshot_to_json(snapshot);
    auto& song = fingerprint.at("song");
    if (song.contains("tracks") && song.at("tracks").is_array())
        for (auto& track : song.at("tracks"))
            if (track.is_object())
                for (const auto field : volatile_track_fields)
                    track.erase(std::string{field});
    return fingerprint;
}

} // namespace

bool equivalent_target_snapshot(const AbletonTargetSnapshot& lhs,
                                const AbletonTargetSnapshot& rhs) {
    return structural_fingerprint(lhs) == structural_fingerprint(rhs);
}

} // namespace sunny::infrastructure
