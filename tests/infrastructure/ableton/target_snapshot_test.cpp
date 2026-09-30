/**
 * @file target_snapshot_test.cpp
 * @brief Adversarial target-snapshot parser tests
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/ableton/target_snapshot.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using nlohmann::json;

namespace {

json mixer_snapshot(std::size_t send_count = 0, bool crossfade_assign_available = true) {
    const auto parameter = json{{"value", 0.5},
                                {"display_value", 0.0},
                                {"minimum", 0.0},
                                {"maximum", 1.0},
                                {"is_quantized", false},
                                {"default_value", 0.0},
                                {"value_items", nullptr},
                                {"state", 0},
                                {"automation_state", 0},
                                {"is_enabled", true}};
    auto activator = parameter;
    activator["value"] = 1.0;
    activator["display_value"] = 1.0;
    activator["is_quantized"] = true;
    activator["default_value"] = nullptr;
    activator["value_items"] = json::array({"Off", "On"});
    auto panning = parameter;
    panning["minimum"] = -1.0;
    auto sends = json::array();
    for (std::size_t index = 0; index < send_count; ++index)
        sends.push_back(parameter);
    return {{"volume", parameter},
            {"track_activator", activator},
            {"panning", panning},
            {"sends", std::move(sends)},
            {"crossfade_assign", crossfade_assign_available ? json(1) : json(nullptr)},
            {"panning_mode", 0}};
}

json routing_dictionary(std::string display_name, std::string identifier) {
    return {{"display_name", std::move(display_name)}, {"identifier", std::move(identifier)}};
}

json routing_collection(std::string field, json options) {
    auto result = json::object();
    result[std::move(field)] = std::move(options);
    return result;
}

json tuning_snapshot() {
    return {{"name", "12-TET"},
            {"pseudo_octave_in_cents", 1200.0},
            {"lowest_note", {{"opaque_fixture", "lowest"}}},
            {"highest_note", {{"opaque_fixture", "highest"}}},
            {"reference_pitch", {{"opaque_fixture", "reference"}}},
            {"note_tunings",
             {{"opaque_fixture",
               json::array({0.0,
                            100.0,
                            200.0,
                            300.0,
                            400.0,
                            500.0,
                            600.0,
                            700.0,
                            800.0,
                            900.0,
                            1000.0,
                            1100.0})}}}};
}

json valid_snapshot() {
    const auto profile = modeled_target_profile(AbletonVersion{12, 3, 5, "12.3.5 test target"});
    return {
        {"schema_version", SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION},
        {"target_profile", target_profile_to_json(profile)},
        {"song",
         {{"tempo", 120.0},
          {"signature_numerator", 4},
          {"signature_denominator", 4},
          {"is_playing", false},
          {"is_counting_in", false},
          {"arrangement_overdub", false},
          {"overdub", false},
          {"record_mode", false},
          {"session_record", false},
          {"session_automation_record", false},
          {"is_ableton_link_enabled", false},
          {"is_ableton_link_start_stop_sync_enabled", false},
          {"tempo_follower_enabled", false},
          {"nudge_down", false},
          {"nudge_up", false},
          {"back_to_arranger", false},
          {"re_enable_automation_enabled", false},
          {"loop", false},
          {"metronome", false},
          {"scale",
           {{"root_note", 0},
            {"name", "Major"},
            {"intervals", json::array({0, 2, 4, 5, 7, 9, 11})},
            {"mode", true}}},
          {"tuning_system", tuning_snapshot()},
          {"scene_count", 2},
          {"scenes",
           json::array({{{"name", "Scene 1"},
                         {"is_triggered", false},
                         {"tempo_enabled", false},
                         {"tempo", -1.0},
                         {"time_signature_enabled", false},
                         {"time_signature_numerator", -1},
                         {"time_signature_denominator", -1}},
                        {{"name", "Scene 2"},
                         {"is_triggered", false},
                         {"tempo_enabled", true},
                         {"tempo", 128.0},
                         {"time_signature_enabled", true},
                         {"time_signature_numerator", 7},
                         {"time_signature_denominator", 8}}})},
          {"tracks",
           json::array(
               {{{"name", "Track"},
                 {"devices",
                  json::array({{{"name", "Operator"},
                                {"class_display_name", "Operator"},
                                {"class_name", "Operator"},
                                {"type", 1},
                                {"is_active", true},
                                {"can_have_chains", false},
                                {"latency_in_samples", 128},
                                {"latency_in_ms", 2.9}}})},
                 {"mixer", mixer_snapshot(1)},
                 {"clip_slot_count", 2},
                 {"arrangement_clip_count", 0},
                 {"take_lane_count", 0},
                 {"clip_slots",
                  json::array({{{"slot", 0},
                                {"has_clip", true},
                                {"has_stop_button", false},
                                {"is_group_slot", false},
                                {"controls_other_clips", false},
                                {"is_playing", false},
                                {"is_recording", false},
                                {"is_triggered", false},
                                {"playing_status", 0},
                                {"will_record_on_start", false}},
                               {{"slot", 1},
                                {"has_clip", false},
                                {"has_stop_button", true},
                                {"is_group_slot", false},
                                {"controls_other_clips", false},
                                {"is_playing", false},
                                {"is_recording", false},
                                {"is_triggered", false},
                                {"playing_status", 0},
                                {"will_record_on_start", false}}})},
                 {"clips",
                  json::array({{{"slot", 0},
                                {"name", "Clip"},
                                {"is_audio_clip", false},
                                {"is_midi_clip", true},
                                {"is_arrangement_clip", false},
                                {"is_session_clip", true},
                                {"is_take_lane_clip", false},
                                {"length", 4.0},
                                {"signature_numerator", 4},
                                {"signature_denominator", 4},
                                {"start_marker", 0.0},
                                {"end_marker", 4.0},
                                {"end_time", 4.0},
                                {"looping", false},
                                {"muted", false},
                                {"has_envelopes", false},
                                {"is_playing", false},
                                {"is_recording", false},
                                {"is_overdubbing", false},
                                {"is_triggered", false},
                                {"will_record_on_start", false},
                                {"launch_mode", 0},
                                {"launch_quantization", 1},
                                {"legato", false},
                                {"velocity_amount", 0.0},
                                {"has_groove", false}}})},
                 {"group_track_index", nullptr},
                 {"input_routing_type", routing_dictionary("All Ins", "all_ins")},
                 {"input_routing_channel", routing_dictionary("All Channels", "all_channels")},
                 {"available_input_routing_types",
                  routing_collection("available_input_routing_types",
                                     json::array({routing_dictionary("No Input", "no_input"),
                                                  routing_dictionary("All Ins", "all_ins")}))},
                 {"available_input_routing_channels",
                  routing_collection(
                      "available_input_routing_channels",
                      json::array({routing_dictionary("All Channels", "all_channels"),
                                   routing_dictionary("Ch. 1", "channel_1")}))},
                 {"input_meter_level", 0.0},
                 {"output_meter_level", 0.0},
                 {"input_meter_left", 0.0},
                 {"input_meter_right", 0.0},
                 {"output_meter_left", 0.0},
                 {"output_meter_right", 0.0},
                 {"output_routing_type", routing_dictionary("Master", "master")},
                 {"output_routing_channel", routing_dictionary("1/2", "stereo_1_2")},
                 {"available_output_routing_types",
                  routing_collection("available_output_routing_types",
                                     json::array({routing_dictionary("Master", "master"),
                                                  routing_dictionary("Group 1", "group_1")}))},
                 {"available_output_routing_channels",
                  routing_collection("available_output_routing_channels",
                                     json::array({routing_dictionary("1/2", "stereo_1_2"),
                                                  routing_dictionary("1", "mono_1")}))},
                 {"has_audio_input", false},
                 {"has_midi_input", true},
                 {"has_audio_output", true},
                 {"has_midi_output", false},
                 {"is_frozen", false},
                 {"arm", false},
                 {"implicit_arm", false},
                 {"back_to_arranger", false},
                 {"fired_slot_index", -1},
                 {"playing_slot_index", -1},
                 {"mute", false},
                 {"solo", false},
                 {"muted_via_solo", false}}})},
          {"return_tracks",
           json::array(
               {{{"name", "Return A"},
                 {"devices", json::array()},
                 {"mixer", mixer_snapshot()},
                 {"output_routing_type", routing_dictionary("Master", "master")},
                 {"output_routing_channel", routing_dictionary("1/2", "stereo_1_2")},
                 {"available_output_routing_types",
                  routing_collection("available_output_routing_types",
                                     json::array({routing_dictionary("Master", "master")}))},
                 {"available_output_routing_channels",
                  routing_collection("available_output_routing_channels",
                                     json::array({routing_dictionary("1/2", "stereo_1_2"),
                                                  routing_dictionary("1", "mono_1")}))},
                 {"mute", false},
                 {"solo", false},
                 {"muted_via_solo", false}}})},
          {"master_track",
           {{"name", "Master"}, {"devices", json::array()}, {"mixer", mixer_snapshot(0, false)}}},
          {"cue_points", json::array({{{"name", "Verse"}, {"time", 0.0}}})}}},
    };
}

} // namespace

TEST_CASE("target snapshot parser retains an exact structural precondition",
          "[ableton][target-snapshot]") {
    const auto encoded = valid_snapshot();
    const auto parsed = target_snapshot_from_json(encoded);
    REQUIRE(parsed.has_value());
    CHECK(target_snapshot_to_json(*parsed) == encoded);
    CHECK(equivalent_target_snapshot(*parsed, *parsed));

    auto changed = *parsed;
    changed.song_state["tempo"] = 121.0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["output_meter_right"] = 0.25;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    for (const auto* property : {"is_playing",
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
                                 "metronome"}) {
        changed = *parsed;
        changed.song_state[property] = true;
        CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));
    }

    changed = *parsed;
    changed.song_state["scenes"][0]["name"] = "Changed Scene";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["scenes"][1]["is_triggered"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["arrangement_clip_count"] = 1;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["take_lane_count"] = 1;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["looping"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["is_session_clip"] = false;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["end_time"] = 5.0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clip_slots"][1]["is_triggered"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["is_frozen"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["mixer"]["volume"]["maximum"] = 2.0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["mixer"]["volume"]["is_quantized"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["muted"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["has_groove"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["has_envelopes"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["is_recording"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["is_overdubbing"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["will_record_on_start"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["is_playing"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["is_triggered"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["clips"][0]["launch_mode"] = 2;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["devices"][0]["is_active"] = false;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["devices"][0]["can_have_chains"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["devices"][0]["latency_in_samples"] = 256;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["muted_via_solo"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["back_to_arranger"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["fired_slot_index"] = 0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["playing_slot_index"] = 0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["input_routing_type"]["display_name"] = "No Input";
    changed.song_state["tracks"][0]["input_routing_type"]["identifier"] = "no_input";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["available_input_routing_channels"]
                      ["available_input_routing_channels"][1]["display_name"] = "MIDI Ch. 1";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["input_meter_level"] = 0.25;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["output_routing_type"]["display_name"] = "Group 1";
    changed.song_state["tracks"][0]["output_routing_type"]["identifier"] = "group_1";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["return_tracks"][0]["output_routing_channel"]["identifier"] = "mono_1";
    changed.song_state["return_tracks"][0]["output_routing_channel"]["display_name"] = "1";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["available_output_routing_types"]
                      ["available_output_routing_types"][1]["display_name"] = "Renamed Group";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["return_tracks"][0]["muted_via_solo"] = true;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tracks"][0]["mixer"]["volume"]["automation_state"] = 1;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["master_track"]["mixer"]["track_activator"]["value"] = 0.0;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["master_track"]["mixer"]["panning"]["is_enabled"] = false;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["scale"]["root_note"] = 2;
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tuning_system"]["name"] = "Custom";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));

    changed = *parsed;
    changed.song_state["tuning_system"]["reference_pitch"]["opaque_fixture"] = "changed";
    CHECK_FALSE(equivalent_target_snapshot(*parsed, changed));
}

TEST_CASE("target snapshot parser rejects ambiguous or malformed topology",
          "[ableton][target-snapshot][validation]") {
    auto encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"].push_back(
        {{"slot", 0}, {"name", "Duplicate"}, {"is_midi_clip", true}, {"length", 1.0}});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["group_track_index"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["crossfade_assign"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["crossfade_assign"] = 3;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["panning_mode"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["master_track"]["mixer"]["panning_mode"] = 2;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["master_track"]["mixer"]["crossfade_assign"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["display_value"] = "0 dB";
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["value"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["minimum"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["maximum"] =
        std::numeric_limits<double>::infinity();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["minimum"] = 2.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["value"] = 2.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["is_quantized"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"].erase("minimum");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["panning"]["state"] = 3;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["panning"]["is_enabled"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["track_activator"].erase("is_enabled");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["master_track"]["mixer"].erase("track_activator");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["unexpected"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["sends"] = json::array();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tempo"] = std::numeric_limits<double>::infinity();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tempo"] = 19.999;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto* property : {"is_playing",
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
                                 "metronome"}) {
        encoded = valid_snapshot();
        encoded["song"][property] = 0;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["tempo"] = 999.001;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["signature_numerator"] = 100;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["signature_denominator"] = 32;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scale"]["root_note"] = 12;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scale"]["intervals"] = json::array({0, true});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scale"]["unexpected"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["pseudo_octave_in_cents"] = 0.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["pseudo_octave_in_cents"] = 1200;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["note_tunings"] = json::array({0.0});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["lowest_note"] = json::array();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["highest_note"]["nested"] =
        std::numeric_limits<double>::infinity();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["note_tunings"] = {{"first", json::array({0.0})},
                                                        {"second", json::array({100.0})}};
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"]["note_tunings"] = {
        {"opaque_fixture", json::array({0.0, "100"})}};
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tuning_system"].erase("reference_pitch");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"].erase("scene_count");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scene_count"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scenes"][0]["tempo"] = 120.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scenes"][0].erase("is_triggered");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["scenes"][0]["is_triggered"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["arrangement_clip_count"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("arrangement_clip_count");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["take_lane_count"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("take_lane_count");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["is_frozen"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clip_slots"][0].erase("is_triggered");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clip_slots"][0]["is_triggered"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clip_slots"][0]["playing_status"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clip_slots"][1]["has_clip"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["arm"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["implicit_arm"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("output_routing_type");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["output_routing_type"]["unexpected"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["output_routing_channel"]["identifier"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["return_tracks"][0]["output_routing_type"]["display_name"] = nullptr;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("has_midi_input");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["has_audio_input"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["input_routing_type"] = nullptr;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["available_input_routing_types"]["available_input_routing_types"] =
        json::array({routing_dictionary("No Input", "no_input")});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("input_meter_level");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("input_meter_left");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto& malformed :
         {json(0), json(-0.1), json(1.1), json(std::numeric_limits<double>::infinity())}) {
        encoded = valid_snapshot();
        encoded["song"]["tracks"][0]["input_meter_level"] = malformed;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["output_meter_level"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto& malformed :
         {json(0), json(-0.1), json(1.1), json(std::numeric_limits<double>::infinity())}) {
        encoded = valid_snapshot();
        encoded["song"]["tracks"][0]["output_meter_right"] = malformed;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["has_midi_input"] = false;
    encoded["song"]["tracks"][0]["input_routing_type"] = nullptr;
    encoded["song"]["tracks"][0]["input_routing_channel"] = nullptr;
    encoded["song"]["tracks"][0]["available_input_routing_types"] = nullptr;
    encoded["song"]["tracks"][0]["available_input_routing_channels"] = nullptr;
    encoded["song"]["tracks"][0]["input_meter_level"] = nullptr;
    encoded["song"]["tracks"][0]["output_meter_level"] = nullptr;
    CHECK(target_snapshot_from_json(encoded).has_value());

    encoded["song"]["tracks"][0]["input_routing_type"] = routing_dictionary("All Ins", "all_ins");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["input_routing_type"] = nullptr;
    encoded["song"]["tracks"][0]["input_meter_level"] = 0.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["has_audio_output"] = false;
    encoded["song"]["tracks"][0]["input_meter_left"] = nullptr;
    encoded["song"]["tracks"][0]["input_meter_right"] = nullptr;
    encoded["song"]["tracks"][0]["output_meter_left"] = nullptr;
    encoded["song"]["tracks"][0]["output_meter_right"] = nullptr;
    CHECK(target_snapshot_from_json(encoded).has_value());

    encoded["song"]["tracks"][0]["input_meter_left"] = 0.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0].erase("available_output_routing_types");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["available_output_routing_types"]["unexpected"] = json::array();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["available_output_routing_channels"]
           ["available_output_routing_channels"] = "1/2";
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["available_output_routing_types"]["available_output_routing_types"]
           [0]["identifier"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["available_output_routing_types"]
           ["available_output_routing_types"] =
               json::array({routing_dictionary("Group 1", "group_1")});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["return_tracks"][0]["available_output_routing_channels"]
           ["available_output_routing_channels"] = json::array();
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["back_to_arranger"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto* property : {"fired_slot_index", "playing_slot_index"}) {
        for (const auto& malformed : {json(true), json(-3), json(2)}) {
            encoded = valid_snapshot();
            encoded["song"]["tracks"][0][property] = malformed;
            CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
        }
    }

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["fired_slot_index"] = -2;
    encoded["song"]["tracks"][0]["playing_slot_index"] = 1;
    CHECK(target_snapshot_from_json(encoded).has_value());

    for (const auto* property : {"mute", "solo", "muted_via_solo"}) {
        encoded = valid_snapshot();
        encoded["song"]["return_tracks"][0][property] = 0;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["return_tracks"][0].erase("mute");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["return_tracks"][0]["unexpected"] = false;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["end_marker"] = -1.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto* property : {"is_audio_clip",
                                 "is_midi_clip",
                                 "is_arrangement_clip",
                                 "is_session_clip",
                                 "is_take_lane_clip"}) {
        encoded = valid_snapshot();
        encoded["song"]["tracks"][0]["clips"][0][property] = 0;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["is_audio_clip"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["is_arrangement_clip"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["is_session_clip"] = false;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["is_take_lane_clip"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["end_time"] = 3.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["end_time"] = 4;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["has_groove"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["has_envelopes"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    for (const auto* property :
         {"is_playing", "is_recording", "is_overdubbing", "is_triggered", "will_record_on_start"}) {
        encoded = valid_snapshot();
        encoded["song"]["tracks"][0]["clips"][0][property] = 0;
        CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
    }

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["launch_mode"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["launch_mode"] = 4;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["launch_quantization"] = 15;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["legato"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["velocity_amount"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["velocity_amount"] = 1.1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["type"] = 3;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["is_active"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["can_have_chains"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0].erase("can_have_chains");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["latency_in_samples"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["latency_in_samples"] = -1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["latency_in_ms"] = 2;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["devices"][0]["latency_in_ms"] = -0.1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"].erase("default_value");
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["default_value"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["default_value"] = 2.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["mixer"]["volume"]["value_items"] = json::array({"Low", "High"});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    auto& volume = encoded["song"]["tracks"][0]["mixer"]["volume"];
    volume["is_quantized"] = true;
    volume["default_value"] = nullptr;
    volume["value_items"] = json::array();
    REQUIRE(target_snapshot_from_json(encoded).has_value());

    volume["default_value"] = 0.0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    volume["default_value"] = nullptr;
    volume["value_items"] = json::array({"Low", 1});
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["has_audio_output"] = 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["song"]["tracks"][0]["clips"][0]["unexpected"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    encoded["schema_version"] = SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION + 1;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
}

TEST_CASE("target snapshot pitch, groove, and Arrangement/Take Lane topology availability is "
          "version-coupled",
          "[ableton][target-snapshot][tuning][groove][arrangement][take-lane][validation]") {
    auto encoded = valid_snapshot();
    auto profile = modeled_target_profile(AbletonVersion{11, 3, 0, "11.3.0"});
    encoded["target_profile"] = target_profile_to_json(profile);
    encoded["song"]["scale"] = nullptr;
    encoded["song"]["tuning_system"] = nullptr;
    REQUIRE(target_snapshot_from_json(encoded).has_value());

    encoded["song"]["scale"] = {{"root_note", 0},
                                {"name", "Major"},
                                {"intervals", json::array({0, 2, 4, 5, 7, 9, 11})},
                                {"mode", true}};
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    profile = modeled_target_profile(AbletonVersion{10, 1, 0, "10.1.0"});
    encoded["target_profile"] = target_profile_to_json(profile);
    encoded["song"]["scale"] = nullptr;
    encoded["song"]["tuning_system"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["has_groove"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["is_session_clip"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["is_take_lane_clip"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["launch_mode"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["launch_quantization"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["legato"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["velocity_amount"] = nullptr;
    encoded["song"]["tracks"][0]["arrangement_clip_count"] = nullptr;
    encoded["song"]["tracks"][0]["take_lane_count"] = nullptr;
    REQUIRE(target_snapshot_from_json(encoded).has_value());

    encoded["song"]["tracks"][0]["clips"][0]["is_session_clip"] = true;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["clips"][0]["is_session_clip"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["is_take_lane_clip"] = false;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["clips"][0]["is_take_lane_clip"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["has_groove"] = false;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["clips"][0]["has_groove"] = nullptr;
    encoded["song"]["tracks"][0]["arrangement_clip_count"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["arrangement_clip_count"] = nullptr;
    encoded["song"]["tracks"][0]["take_lane_count"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded["song"]["tracks"][0]["take_lane_count"] = nullptr;
    encoded["song"]["tracks"][0]["clips"][0]["launch_mode"] = 0;
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);

    encoded = valid_snapshot();
    profile = modeled_target_profile(AbletonVersion{12, 0, 5, "12.0.5"});
    encoded["target_profile"] = target_profile_to_json(profile);
    encoded["song"]["tuning_system"] = nullptr;
    REQUIRE(target_snapshot_from_json(encoded).has_value());

    encoded["song"]["tuning_system"] = {{"name", "12-TET"}, {"pseudo_octave_in_cents", 1200.0}};
    CHECK(target_snapshot_from_json(encoded).error() == ErrorCode::ProtocolError);
}
