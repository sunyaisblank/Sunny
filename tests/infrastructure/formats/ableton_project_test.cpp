/**
 * @file ableton_project_test.cpp
 * @brief Complete project to Ableton compilation tests
 */

#include <catch2/catch_test_macros.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;

namespace {

Score make_score() {
    ScoreSpec spec;
    spec.title = "Project";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.parts.resize(2);
    spec.parts[0].name = "One";
    spec.parts[0].instrument_type = InstrumentType::Synthesiser;
    spec.parts[1].name = "Two";
    spec.parts[1].instrument_type = InstrumentType::Synthesiser;
    auto score = create_score(spec);
    REQUIRE(score.has_value());
    score->section_map.push_back(ScoreSection{SectionId{1},
                                              "Intro",
                                              ScoreTime{1, Beat::zero()},
                                              ScoreTime{2, Beat::zero()},
                                              {},
                                              std::nullopt});
    return std::move(*score);
}

void insert_project_note(Score& score, std::size_t part_index) {
    auto& voice = score.parts.at(part_index).measures.at(0).voices.at(0);
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 4};
    voice.events = {
        Event{EventId{9100000 + part_index * 2}, Beat::zero(), group},
        Event{EventId{9100001 + part_index * 2}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}}};
}

TimbreProfile make_profile(std::uint64_t id, PartId part) {
    auto profile = create_timbre_profile(TimbreProfileId{id}, part, "Profile");
    SubtractiveSynth source;
    source.oscillators.push_back(Oscillator{});
    profile.source.data = std::move(source);
    return profile;
}

AbletonOutputRouteBinding project_master_route_binding() {
    return {{"Master", "master"}, {"1/2", "stereo_1_2"}, "named Live fixture/operator mapping"};
}

AbletonTargetSnapshot make_post_snapshot(const Score& score, const MixGraph& mix) {
    const auto parameter = nlohmann::json{{"value", 0.0},
                                          {"display_value", 0.0},
                                          {"minimum", 0.0},
                                          {"maximum", 1.0},
                                          {"is_quantized", false},
                                          {"default_value", 0.0},
                                          {"value_items", nullptr},
                                          {"state", 0},
                                          {"automation_state", 0},
                                          {"is_enabled", true}};
    auto activator_parameter = parameter;
    activator_parameter["value"] = 1.0;
    activator_parameter["display_value"] = 1.0;
    activator_parameter["is_quantized"] = true;
    activator_parameter["default_value"] = nullptr;
    activator_parameter["value_items"] = nlohmann::json::array({"Off", "On"});
    auto panning_parameter = parameter;
    panning_parameter["minimum"] = -1.0;
    auto mixer = nlohmann::json{{"volume", parameter},
                                {"track_activator", activator_parameter},
                                {"panning", panning_parameter},
                                {"sends", nlohmann::json::array()},
                                {"crossfade_assign", 1},
                                {"panning_mode", 0}};
    for (std::size_t index = 0; index < mix.aux_buses.size(); ++index)
        mixer["sends"].push_back(parameter);
    const auto master_mixer = nlohmann::json{{"volume", parameter},
                                             {"track_activator", activator_parameter},
                                             {"panning", panning_parameter},
                                             {"sends", nlohmann::json::array()},
                                             {"crossfade_assign", nullptr},
                                             {"panning_mode", 0}};
    const bool any_solo = std::any_of(
        mix.channels.begin(), mix.channels.end(), [](const auto& channel) { return channel.solo; });
    auto tracks = nlohmann::json::array();
    for (const auto& part : score.parts) {
        const auto channel =
            std::find_if(mix.channels.begin(), mix.channels.end(), [&](const auto& candidate) {
                return candidate.part_id == part.id;
            });
        REQUIRE(channel != mix.channels.end());
        auto track_mixer = mixer;
        track_mixer["track_activator"]["value"] = channel->mute ? 0.0 : 1.0;
        track_mixer["track_activator"]["display_value"] = channel->mute ? 0.0 : 1.0;
        tracks.push_back(
            {{"name", part.definition.name},
             {"devices",
              nlohmann::json::array({{{"name", "Analog"},
                                      {"class_display_name", "Analog"},
                                      {"class_name", "Analog"},
                                      {"type", 1},
                                      {"is_active", true},
                                      {"can_have_chains", false},
                                      {"latency_in_samples", 128},
                                      {"latency_in_ms", 2.9}}})},
             {"mixer", std::move(track_mixer)},
             {"clip_slot_count", 1},
             {"arrangement_clip_count", 0},
             {"take_lane_count", 0},
             {"clip_slots",
              nlohmann::json::array({{{"slot", 0},
                                      {"has_clip", true},
                                      {"has_stop_button", false},
                                      {"is_group_slot", false},
                                      {"controls_other_clips", false},
                                      {"is_playing", false},
                                      {"is_recording", false},
                                      {"is_triggered", false},
                                      {"playing_status", 0},
                                      {"will_record_on_start", false}}})},
             {"clips",
              nlohmann::json::array({{{"slot", 0},
                                      {"name", part.definition.name},
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
             {"input_routing_type", {{"display_name", "All Ins"}, {"identifier", "all_ins"}}},
             {"input_routing_channel",
              {{"display_name", "All Channels"}, {"identifier", "all_channels"}}},
             {"available_input_routing_types",
              {{"available_input_routing_types",
                nlohmann::json::array(
                    {{{"display_name", "No Input"}, {"identifier", "no_input"}},
                     {{"display_name", "All Ins"}, {"identifier", "all_ins"}}})}}},
             {"available_input_routing_channels",
              {{"available_input_routing_channels",
                nlohmann::json::array(
                    {{{"display_name", "All Channels"}, {"identifier", "all_channels"}},
                     {{"display_name", "Ch. 1"}, {"identifier", "channel_1"}}})}}},
             {"input_meter_level", 0.0},
             {"output_meter_level", 0.0},
             {"input_meter_left", 0.0},
             {"input_meter_right", 0.0},
             {"output_meter_left", 0.0},
             {"output_meter_right", 0.0},
             {"output_routing_type", {{"display_name", "Master"}, {"identifier", "master"}}},
             {"output_routing_channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
             {"available_output_routing_types",
              {{"available_output_routing_types",
                nlohmann::json::array(
                    {{{"display_name", "Master"}, {"identifier", "master"}},
                     {{"display_name", "Group 1"}, {"identifier", "group_1"}}})}}},
             {"available_output_routing_channels",
              {{"available_output_routing_channels",
                nlohmann::json::array({{{"display_name", "1/2"}, {"identifier", "stereo_1_2"}},
                                       {{"display_name", "1"}, {"identifier", "mono_1"}}})}}},
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
             {"mute", channel->mute},
             {"solo", channel->solo},
             {"muted_via_solo", any_solo && !channel->solo}});
    }
    auto return_tracks = nlohmann::json::array();
    for (const auto& aux : mix.aux_buses) {
        auto return_mixer = nlohmann::json{{"volume", parameter},
                                           {"track_activator", activator_parameter},
                                           {"panning", panning_parameter},
                                           {"sends", nlohmann::json::array()},
                                           {"crossfade_assign", 1},
                                           {"panning_mode", 0}};
        return_mixer["volume"]["display_value"] = aux.return_level;
        return_mixer["panning"]["value"] = aux.return_spatial.pan;
        return_tracks.push_back(
            {{"name", aux.name},
             {"devices", nlohmann::json::array()},
             {"mixer", std::move(return_mixer)},
             {"output_routing_type", {{"display_name", "Master"}, {"identifier", "master"}}},
             {"output_routing_channel", {{"display_name", "1/2"}, {"identifier", "stereo_1_2"}}},
             {"available_output_routing_types",
              {{"available_output_routing_types",
                nlohmann::json::array(
                    {{{"display_name", "Master"}, {"identifier", "master"}},
                     {{"display_name", "Group 1"}, {"identifier", "group_1"}}})}}},
             {"available_output_routing_channels",
              {{"available_output_routing_channels",
                nlohmann::json::array({{{"display_name", "1/2"}, {"identifier", "stereo_1_2"}},
                                       {{"display_name", "1"}, {"identifier", "mono_1"}}})}}},
             {"mute", false},
             {"solo", false},
             {"muted_via_solo", false}});
    }

    return {SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION,
            modeled_target_profile(AbletonVersion{12, 3, 5, "12.3.5 project postcondition test"}),
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
               {"intervals", nlohmann::json::array({0, 2, 4, 5, 7, 9, 11})},
               {"mode", true}}},
             {"tuning_system",
              {{"name", "12-TET"},
               {"pseudo_octave_in_cents", 1200.0},
               {"lowest_note", {{"opaque_fixture", "lowest"}}},
               {"highest_note", {{"opaque_fixture", "highest"}}},
               {"reference_pitch", {{"opaque_fixture", "reference"}}},
               {"note_tunings",
                {{"opaque_fixture",
                  nlohmann::json::array({0.0,
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
                                         1100.0})}}}}},
             {"scene_count", 1},
             {"scenes",
              nlohmann::json::array({{{"name", score.metadata.title},
                                      {"is_triggered", false},
                                      {"tempo_enabled", false},
                                      {"tempo", -1.0},
                                      {"time_signature_enabled", false},
                                      {"time_signature_numerator", -1},
                                      {"time_signature_denominator", -1}}})},
             {"tracks", std::move(tracks)},
             {"return_tracks", std::move(return_tracks)},
             {"master_track",
              {{"name", "Master"}, {"devices", nlohmann::json::array()}, {"mixer", master_mixer}}},
             {"cue_points", nlohmann::json::array({{{"name", "Intro"}, {"time", 0.0}}})}}};
}

class ProjectFailureTransport final : public LomTransport {
  public:
    Result<std::optional<LegacyPlanningAuthority>> capture_legacy_authority() override {
        return std::optional(
            LegacyPlanningAuthority{nlohmann::json{{"schema_version", 1},
                                                   {"bridge_instance", std::string(32, 'b')},
                                                   {"document_token", std::string(32, 'd')},
                                                   {"scope_id", std::string(32, 'a')}},
                                    0,
                                    std::string(32, '1')});
    }
    Result<void> activate_legacy_workflow(const LegacyWorkflowRecipe&) override { return {}; }

    explicit ProjectFailureTransport(std::size_t fail_at) : fail_at_(fail_at) {}

    LomResponse send(const LomRequest& request) override {
        if (attempts_++ == fail_at_) return {false, std::nullopt, std::string{"injected failure"}};
        state_.send(request);
        if (request.type == LomRequestType::SetProperty) {
            const auto requested = std::visit(
                [](const auto& value) { return nlohmann::json(value); }, request.args.front());
            return {true,
                    nlohmann::json{{"property", request.property_or_method},
                                   {"requested", requested},
                                   {"observed", requested}},
                    std::nullopt};
        }
        return {true, std::nullopt, std::nullopt};
    }

    LomResponse send_notes(const LomPath& path, const std::vector<LomNoteData>& notes) override {
        if (attempts_++ == fail_at_) return {false, std::nullopt, std::string{"injected failure"}};
        state_.send_notes(path, notes);
        std::vector<int> ids;
        ids.reserve(notes.size());
        for (std::size_t index = 0; index < notes.size(); ++index)
            ids.push_back(static_cast<int>(index + 1));
        return {true, std::move(ids), std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }
    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return state_.target_profile();
    }
    [[nodiscard]] Result<std::optional<AbletonTargetSnapshot>> target_snapshot() override {
        return state_.target_snapshot();
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> scene_count() override {
        return state_.scene_count();
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> return_track_count() override {
        return state_.return_track_count();
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> device_count(const LomPath& path) override {
        return state_.device_count(path);
    }
    [[nodiscard]] std::size_t attempts() const { return attempts_; }

  private:
    CommandBuffer state_;
    std::size_t fail_at_;
    std::size_t attempts_ = 0;
};

enum class FinalNoteMode : std::uint8_t { Exact, Extra, ProbabilityDivergent, Malformed };
enum class FinalParameterMode : std::uint8_t {
    Exact,
    Divergent,
    RangeDivergent,
    Disabled,
    Automated,
    Malformed,
    Extended,
    SendFailure,
};

class ProjectNoteTransport final : public LomTransport {
  public:
    Result<std::optional<LegacyPlanningAuthority>> capture_legacy_authority() override {
        return std::optional(
            LegacyPlanningAuthority{nlohmann::json{{"schema_version", 1},
                                                   {"bridge_instance", std::string(32, 'b')},
                                                   {"document_token", std::string(32, 'd')},
                                                   {"scope_id", std::string(32, 'a')}},
                                    0,
                                    std::string(32, '1')});
    }
    Result<void> activate_legacy_workflow(const LegacyWorkflowRecipe&) override { return {}; }

    ProjectNoteTransport(AbletonTargetSnapshot before,
                         AbletonTargetSnapshot after,
                         FinalNoteMode mode = FinalNoteMode::Exact,
                         FinalParameterMode parameter_mode = FinalParameterMode::Exact)
        : before_(std::move(before)), after_(std::move(after)), mode_(mode),
          parameter_mode_(parameter_mode) {}

    LomResponse send(const LomRequest& request) override {
        events.push_back(request.property_or_method);
        if (request.property_or_method == "get_notes_by_id" ||
            request.property_or_method == "get_all_notes_extended") {
            const bool final_query =
                request.property_or_method == "get_all_notes_extended" && ++all_note_queries_ == 2;
            if (final_query && mode_ == FinalNoteMode::Malformed)
                return {true,
                        nlohmann::json{
                            {"notes", nlohmann::json::array({{{"note_id", 100}, {"pitch", 60}}})}},
                        std::nullopt};
            nlohmann::json notes = nlohmann::json::array();
            for (std::size_t index = 0; index < inserted_notes_.size(); ++index) {
                const auto& note = inserted_notes_[index];
                notes.push_back({{"note_id", inserted_ids_[index]},
                                 {"pitch", static_cast<int>(note.pitch)},
                                 {"start_time", note.start_time},
                                 {"duration", note.duration},
                                 {"velocity", static_cast<int>(note.velocity)},
                                 {"mute", note.muted},
                                 {"probability", note.probability},
                                 {"velocity_deviation", note.velocity_deviation},
                                 {"release_velocity", note.release_velocity}});
                if (final_query && mode_ == FinalNoteMode::ProbabilityDivergent && index == 0)
                    notes.back()["probability"] = 0.5;
            }
            if (final_query && mode_ == FinalNoteMode::Extra) {
                notes.push_back({{"note_id", 999},
                                 {"pitch", 72},
                                 {"start_time", 1.0},
                                 {"duration", 1.0},
                                 {"velocity", 64.0},
                                 {"mute", false},
                                 {"probability", 1.0},
                                 {"velocity_deviation", 0.0},
                                 {"release_velocity", 64.0}});
            }
            return {true, nlohmann::json{{"notes", std::move(notes)}}, std::nullopt};
        }
        if (request.property_or_method == "sunny_set_cue") {
            const auto time = std::get<double>(request.args.at(0));
            const auto& name = std::get<std::string>(request.args.at(1));
            return {true,
                    nlohmann::json{{"action", "created"},
                                   {"requested_time", time},
                                   {"observed_time", time},
                                   {"requested_name", name},
                                   {"observed_name", name}},
                    std::nullopt};
        }
        if (request.property_or_method == "sunny_clear_all_envelopes")
            return {true, nlohmann::json{{"has_envelopes", false}}, std::nullopt};
        if (request.property_or_method == "insert_device") {
            const auto& name = std::get<std::string>(request.args.at(0));
            const auto index = std::get<int>(request.args.at(1));
            device_counts_[request.path.to_string()] = static_cast<std::uint32_t>(index + 1);
            return {true,
                    nlohmann::json{{"requested_name", name},
                                   {"requested_index", index},
                                   {"before_count", index},
                                   {"after_count", index + 1},
                                   {"device_index", index},
                                   {"name", name},
                                   {"class_display_name", name},
                                   {"class_name", name},
                                   {"type", index == 0 ? 1 : 2},
                                   {"is_active", true},
                                   {"can_have_chains", false},
                                   {"latency_in_samples", 128},
                                   {"latency_in_ms", 2.9},
                                   {"track_has_audio_output", true},
                                   {"track_has_midi_output", false}},
                    std::nullopt};
        }
        if (request.property_or_method == "sunny_set_device_parameter") {
            parameter_device_path_ = request.path.to_string();
            parameter_name_ = std::get<std::string>(request.args.at(0));
            parameter_requested_ = std::get<double>(request.args.at(1));
            parameter_property_ = std::get<std::string>(request.args.at(2));
            parameter_minimum_ =
                parameter_property_ == "display_value" ? 0.0 : std::get<double>(request.args.at(3));
            parameter_maximum_ =
                parameter_property_ == "display_value" ? 1.0 : std::get<double>(request.args.at(4));
            return {true,
                    nlohmann::json{{"matched_name", parameter_name_},
                                   {"original_name", parameter_name_},
                                   {"property", parameter_property_},
                                   {"requested", parameter_requested_},
                                   {"observed", parameter_requested_},
                                   {"minimum", parameter_minimum_},
                                   {"maximum", parameter_maximum_},
                                   {"is_quantized", false},
                                   {"default_value", parameter_minimum_},
                                   {"value_items", nullptr},
                                   {"is_enabled", true},
                                   {"state", 0},
                                   {"automation_state", 0}},
                    std::nullopt};
        }
        if (request.property_or_method == "sunny_get_device_parameter") {
            ++parameter_queries_;
            if (request.path.to_string() != parameter_device_path_ ||
                std::get<std::string>(request.args.at(0)) != parameter_name_ ||
                std::get<std::string>(request.args.at(1)) != parameter_property_)
                return {false, std::nullopt, std::string{"wrong final parameter query"}};
            if (parameter_mode_ == FinalParameterMode::SendFailure)
                return {false, std::nullopt, std::string{"injected observation failure"}};
            if (parameter_mode_ == FinalParameterMode::Malformed)
                return {true,
                        nlohmann::json{{"matched_name", parameter_name_},
                                       {"observed", parameter_requested_}},
                        std::nullopt};
            const double observed = parameter_requested_ +
                                    (parameter_mode_ == FinalParameterMode::Divergent ? 0.1 : 0.0);
            const double maximum =
                parameter_maximum_ +
                (parameter_mode_ == FinalParameterMode::RangeDivergent ? 1.0 : 0.0);
            nlohmann::json evidence = {
                {"matched_name", parameter_name_},
                {"original_name", parameter_name_},
                {"property", parameter_property_},
                {"observed", observed},
                {"minimum", parameter_minimum_},
                {"maximum", maximum},
                {"is_quantized", false},
                {"default_value", parameter_minimum_},
                {"value_items", nullptr},
                {"is_enabled", parameter_mode_ != FinalParameterMode::Disabled},
                {"state", 0},
                {"automation_state", parameter_mode_ == FinalParameterMode::Automated ? 2 : 0}};
            if (parameter_mode_ == FinalParameterMode::Extended) evidence["unexpected"] = true;
            return {true, std::move(evidence), std::nullopt};
        }
        if (request.type == LomRequestType::SetProperty) {
            const auto requested = std::visit(
                [](const auto& value) { return nlohmann::json(value); }, request.args.front());
            return {true,
                    nlohmann::json{{"property", request.property_or_method},
                                   {"requested", requested},
                                   {"observed", requested}},
                    std::nullopt};
        }
        return {true, std::nullopt, std::nullopt};
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>& notes) override {
        events.push_back("add_new_notes");
        inserted_notes_ = notes;
        inserted_ids_.clear();
        for (std::size_t index = 0; index < notes.size(); ++index)
            inserted_ids_.push_back(static_cast<int>(100 + index));
        return {true, inserted_ids_, std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }
    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return std::optional<AbletonTargetProfile>{before_.target_profile};
    }
    [[nodiscard]] Result<std::optional<AbletonTargetSnapshot>> target_snapshot() override {
        ++snapshot_calls_;
        events.push_back(snapshot_calls_ >= 3 ? "snapshot_after" : "snapshot_before");
        return std::optional<AbletonTargetSnapshot>{snapshot_calls_ >= 3 ? after_ : before_};
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> scene_count() override {
        return std::optional<std::uint32_t>{1};
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> return_track_count() override {
        return std::optional<std::uint32_t>{0};
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> device_count(const LomPath& path) override {
        const auto found = device_counts_.find(path.to_string());
        return std::optional<std::uint32_t>{found == device_counts_.end() ? 0 : found->second};
    }

    [[nodiscard]] std::size_t all_note_queries() const { return all_note_queries_; }
    [[nodiscard]] std::size_t parameter_queries() const { return parameter_queries_; }

    std::vector<std::string> events;

  private:
    AbletonTargetSnapshot before_;
    AbletonTargetSnapshot after_;
    FinalNoteMode mode_;
    FinalParameterMode parameter_mode_;
    std::size_t snapshot_calls_ = 0;
    std::size_t all_note_queries_ = 0;
    std::vector<LomNoteData> inserted_notes_;
    std::vector<int> inserted_ids_;
    std::size_t parameter_queries_ = 0;
    std::string parameter_device_path_;
    std::string parameter_name_;
    std::string parameter_property_;
    double parameter_requested_ = 0.0;
    double parameter_minimum_ = 0.0;
    double parameter_maximum_ = 1.0;
    std::map<std::string, std::uint32_t> device_counts_;
};

} // namespace

TEST_CASE("project compiler targets timbre and mix by PartId", "[ableton][project]") {
    auto score = make_score();
    insert_project_note(score, 0);
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    // Reverse timbre and mix collection order adversarially.
    const std::vector<const TimbreProfile*> profiles{&second, &first};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[1].id, score.parts[0].id});
    mix.channels[0].fader.level_db = -12.0f;
    mix.channels[1].fader.level_db = -3.0f;

    const ProjectView project{score, profiles, mix};
    CommandBuffer transport;
    const auto result = compile_project_to_ableton(project, transport);

    REQUIRE(result.has_value());
    CHECK(result->score.property_writes == 40);
    CHECK(result->score.property_writes_verified == 0);
    CHECK(result->score.property_deployments.size() == 40);
    CHECK(result->mix.property_writes == 14);
    CHECK(result->mix.property_writes_verified == 0);
    CHECK(result->mix.effects_verified == 0);
    CHECK(result->mix.device_deployments.empty());
    CHECK_FALSE(result->postconditions.observed);
    CHECK(result->postconditions.song_states_requested == 1);
    CHECK(result->postconditions.song_states_verified == 0);
    REQUIRE(result->postconditions.song_state.has_value());
    CHECK(result->postconditions.song_state->requested_scene_name == "Project");
    CHECK_FALSE(result->postconditions.song_state->requested_scene_triggered);
    CHECK(result->postconditions.song_state->observed_scene_triggered_states.empty());
    CHECK_FALSE(result->postconditions.song_state->scene_trigger_states_observed);
    CHECK_FALSE(result->postconditions.song_state->all_scene_launches_quiescent_verified);
    CHECK_FALSE(result->postconditions.song_state->observed_tempo.has_value());
    CHECK_FALSE(result->postconditions.song_state->observed_scale_name.has_value());
    CHECK_FALSE(result->postconditions.song_state->audible_pitch_verified);
    CHECK(result->postconditions.track_gates_requested == 2);
    CHECK(result->postconditions.track_gates_verified == 0);
    REQUIRE(result->postconditions.track_gates.size() == 2);
    CHECK(result->postconditions.track_gates[0].requested_crossfade_assign == 1);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_crossfade_assign.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].crossfade_neutral_verified);
    CHECK(result->postconditions.track_gates[0].requested_panning_mode == 0);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_panning_mode.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].stereo_panning_verified);
    CHECK(result->postconditions.track_gates[0].requested_track_activator == 1.0);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_track_activator.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].track_activator_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_mute.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_is_frozen.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].requested_has_audio_input);
    CHECK(result->postconditions.track_gates[0].requested_has_midi_input);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_has_audio_input.has_value());
    CHECK(result->postconditions.track_gates[0].observed_clip_slots.empty());
    CHECK_FALSE(result->postconditions.track_gates[0].clip_slot_states_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].clip_slot_runtime_quiescence_verified);
    CHECK(result->postconditions.track_gates[0].requested_arrangement_clip_count == 0);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_arrangement_clip_count.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].arrangement_clip_topology_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].arrangement_content_absent_verified);
    CHECK(result->postconditions.track_gates[0].requested_take_lane_count == 0);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_take_lane_count.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].take_lane_topology_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].take_lanes_absent_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_has_midi_input.has_value());
    CHECK(result->postconditions.track_gates[0].requested_input_meter_level == 0.0);
    CHECK(result->postconditions.track_gates[0].requested_output_meter_level == 0.0);
    CHECK(result->postconditions.track_gates[0].requested_input_meter_left == 0.0);
    CHECK(result->postconditions.track_gates[0].requested_input_meter_right == 0.0);
    CHECK(result->postconditions.track_gates[0].requested_output_meter_left == 0.0);
    CHECK(result->postconditions.track_gates[0].requested_output_meter_right == 0.0);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_input_meter_level.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_output_meter_level.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_input_meter_left.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_input_meter_right.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_output_meter_left.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].observed_output_meter_right.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].meter_levels_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].meter_hold_quiescence_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].momentary_meter_levels_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].meter_momentary_quiescence_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].meter_quiescence_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].continuous_input_silence_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].continuous_output_silence_verified);
    CHECK(result->postconditions.track_gates[0].input_routing.available_types.empty());
    CHECK(result->postconditions.track_gates[0].input_routing.available_channels.empty());
    CHECK_FALSE(result->postconditions.track_gates[0].input_routing.selected_input_observed);
    CHECK_FALSE(
        result->postconditions.track_gates[0].input_routing.selected_input_available_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].input_classification_verified);
    CHECK(result->postconditions.track_gates[0].output_routing.requested_destination ==
          AbletonOutputDestination::Master);
    CHECK_FALSE(
        result->postconditions.track_gates[0].output_routing.requested_group_id.has_value());
    CHECK_FALSE(
        result->postconditions.track_gates[0].output_routing.selected_type_identifier.has_value());
    CHECK(result->postconditions.track_gates[0].output_routing.available_types.empty());
    CHECK(result->postconditions.track_gates[0].output_routing.available_channels.empty());
    CHECK_FALSE(
        result->postconditions.track_gates[0].output_routing.selected_type_available_verified);
    CHECK_FALSE(
        result->postconditions.track_gates[0].output_routing.selected_channel_available_verified);
    CHECK_FALSE(
        result->postconditions.track_gates[0].output_routing.selected_output_available_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].output_routing.selected_output_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].output_routing.source_target_identity_mapped);
    CHECK_FALSE(result->postconditions.track_gates[0].output_routing.verified);
    CHECK_FALSE(result->postconditions.track_gates[0].unfrozen_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].requested_arm);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_arm.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].disarmed_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].requested_implicit_arm);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_implicit_arm.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].implicitly_disarmed_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].group_membership_observed);
    CHECK_FALSE(result->postconditions.track_gates[0].observed_group_track_index.has_value());
    CHECK_FALSE(result->postconditions.track_gates[0].ungrouped_verified);
    CHECK_FALSE(result->postconditions.track_gates[0].monitoring_state_observed);
    CHECK_FALSE(
        result->postconditions.track_gates[0].clip_output_not_suppressed_by_monitoring_verified);
    CHECK(result->postconditions.devices_requested == 2);
    CHECK(result->postconditions.devices_verified == 0);
    REQUIRE(result->postconditions.devices.size() == 2);
    CHECK(result->postconditions.devices[0].requested_type == 1);
    CHECK(result->postconditions.devices[0].expected_chain_size == 1);
    CHECK_FALSE(result->postconditions.devices[0].observed_active.has_value());
    CHECK(result->postconditions.clips_requested == 2);
    CHECK(result->postconditions.clips_verified == 0);
    REQUIRE(result->postconditions.clips.size() == 2);
    CHECK(result->postconditions.clips[0].requested_length == 4.0);
    CHECK(result->postconditions.clips[0].groove_state_available);
    CHECK_FALSE(result->postconditions.clips[0].requested_has_groove);
    CHECK(result->postconditions.clips[0].launch_state_available);
    CHECK(result->postconditions.clips[0].requested_launch_mode == 0);
    CHECK(result->postconditions.clips[0].requested_launch_quantization == 1);
    CHECK_FALSE(result->postconditions.clips[0].requested_legato);
    CHECK(result->postconditions.clips[0].requested_velocity_amount == 0.0);
    CHECK_FALSE(result->postconditions.clips[0].observed_length.has_value());
    CHECK_FALSE(result->postconditions.clips[0].observed_has_groove.has_value());
    CHECK_FALSE(result->postconditions.clips[0].groove_absence_verified);
    CHECK_FALSE(result->postconditions.clips[0].requested_has_envelopes);
    CHECK_FALSE(result->postconditions.clips[0].observed_has_envelopes.has_value());
    CHECK_FALSE(result->postconditions.clips[0].mpe_note_expression_state_observed);
    CHECK_FALSE(result->postconditions.clips[0].mpe_note_expression_neutrality_verified);
    CHECK_FALSE(result->postconditions.clips[0].envelope_absence_verified);
    CHECK_FALSE(result->postconditions.clips[0].launch_behavior_verified);
    CHECK_FALSE(result->postconditions.clips[0].follow_actions_observed);
    CHECK_FALSE(result->postconditions.clips[0].one_shot_playback_verified);
    CHECK_FALSE(result->postconditions.clips[0].midi_bank_program_state_observed);
    CHECK_FALSE(result->postconditions.clips[0].program_change_suppression_verified);
    CHECK_FALSE(result->postconditions.clips[0].audible_timing_verified);
    CHECK_FALSE(result->postconditions.clips[0].audible_velocity_verified);
    CHECK(result->postconditions.note_batches_requested == 1);
    CHECK(result->postconditions.note_batches_verified == 0);
    CHECK(result->postconditions.notes_requested == 1);
    CHECK(result->postconditions.notes_verified == 0);
    REQUIRE(result->postconditions.note_batches.size() == 1);
    CHECK(result->postconditions.note_batches[0].deployment_action ==
          AbletonNoteAction::RecordedOnly);
    REQUIRE(result->postconditions.note_batches[0].requested_notes.size() == 1);
    CHECK(result->postconditions.note_batches[0].requested_notes[0].pitch == 60);
    CHECK(result->postconditions.note_batches[0].requested_note_ids.empty());
    CHECK(result->postconditions.note_batches[0].observed_notes.empty());
    CHECK(result->postconditions.cues_requested == 1);
    CHECK(result->postconditions.cues_verified == 0);
    REQUIRE(result->postconditions.cues.size() == 1);
    CHECK(result->postconditions.cues[0].requested_name == "Intro");
    CHECK_FALSE(result->postconditions.cues[0].observed_name.has_value());
    CHECK(result->postconditions.master_track_gates_requested == 1);
    CHECK(result->postconditions.master_track_gates_verified == 0);
    REQUIRE(result->postconditions.master_track_gate.has_value());
    CHECK(result->postconditions.master_track_gate->requested_track_activator == 1.0);
    CHECK(result->postconditions.master_track_gate->requested_panning_mode == 0);
    CHECK(result->postconditions.master_track_gate->requested_pan == 0.0);
    CHECK_FALSE(result->postconditions.master_track_gate->observed_pan.has_value());
    CHECK_FALSE(result->postconditions.master_track_gate->verified);
    CHECK(result->postconditions.mixer_properties_requested == 9);
    CHECK(result->postconditions.mixer_properties_verified == 0);
    REQUIRE(result->postconditions.mixer_properties.size() == 9);
    CHECK(result->postconditions.mixer_properties[0].parameter_path ==
          "song/master_track/mixer_device/panning");
    CHECK_FALSE(result->postconditions.mixer_properties[0].observed_value.has_value());
    CHECK(result->postconditions.device_parameters_requested == 0);
    CHECK(result->postconditions.device_parameters_verified == 0);
    CHECK(result->postconditions.device_parameters.empty());
    REQUIRE(result->timbre.size() == 2);
    CHECK(result->timbre[0].part_id == score.parts[0].id);
    CHECK(result->timbre[0].track_index == 0);
    CHECK(result->timbre[0].compilation.devices_verified == 0);
    CHECK(result->timbre[0].compilation.device_deployments.size() == 1);
    CHECK(result->timbre[1].part_id == score.parts[1].id);
    CHECK(result->timbre[1].track_index == 1);

    bool part_two_fader_targets_track_one = false;
    for (const auto* entry : transport.find_by_type(LomRequestType::SetProperty)) {
        if (entry->request.path.to_string() == "song/tracks/1/mixer_device/volume" &&
            entry->request.property_or_method == "display_value" &&
            std::get<double>(entry->request.args.at(0)) == -12.0)
            part_two_fader_targets_track_one = true;
    }
    CHECK(part_two_fader_targets_track_one);
}

TEST_CASE("project compiler preflight sends nothing for invalid correspondence",
          "[ableton][project]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    const std::vector<const TimbreProfile*> profiles{&first};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    CommandBuffer transport;

    const auto result = compile_project_to_ableton(project, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProjectValidationFailed);
    CHECK(transport.entries().empty());
}

TEST_CASE("project plans retain route assumptions and final snapshots verify exact bindings",
          "[ableton][project][routing]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(score.parts[0].id, project_master_route_binding());
    bindings.part_tracks.emplace(score.parts[1].id, project_master_route_binding());
    CommandBuffer transport;

    auto plan = plan_project_to_ableton(project, bindings, transport);
    REQUIRE(plan.has_value());
    CHECK(plan->output_routing_bindings == bindings);
    CHECK(plan->preview.output_routing_bindings == bindings);
    CHECK(plan->output_routing_binding_state["part_tracks"].size() == 2);
    std::vector<std::string> route_mutations;
    for (const auto& mutation : plan->mutations) {
        if (mutation.request.property_or_method.starts_with("sunny_set_output_routing_"))
            route_mutations.push_back(mutation.request.property_or_method);
    }
    REQUIRE(route_mutations.size() == 4);
    CHECK(route_mutations == std::vector<std::string>{"sunny_set_output_routing_type",
                                                      "sunny_set_output_routing_channel",
                                                      "sunny_set_output_routing_type",
                                                      "sunny_set_output_routing_channel"});

    auto attempt = apply_project_ableton_plan(*plan, project, transport);
    REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
    REQUIRE(attempt.compilation.has_value());
    CHECK(attempt.compilation->mix.output_routes_written == 2);
    CHECK(attempt.compilation->mix.output_routes_verified == 0);
    auto snapshot = make_post_snapshot(score, mix);
    auto evidence = evaluate_project_ableton_postconditions(
        project, attempt.compilation->part_tracks, snapshot, &*attempt.compilation);
    REQUIRE(evidence.has_value());
    REQUIRE(evidence->track_gates.size() == 2);
    for (const auto& gate : evidence->track_gates) {
        REQUIRE(gate.output_routing.requested_binding.has_value());
        CHECK(gate.output_routing.requested_binding->mapping_provenance ==
              "named Live fixture/operator mapping");
        CHECK(gate.output_routing.source_target_identity_mapped);
        CHECK(gate.output_routing.selected_output_available_verified);
        CHECK(gate.output_routing.verified);
    }

    snapshot.song_state["tracks"][0]["output_routing_channel"] = {{"display_name", "1"},
                                                                  {"identifier", "mono_1"}};
    auto divergent = evaluate_project_ableton_postconditions(
        project, attempt.compilation->part_tracks, snapshot, &*attempt.compilation);
    REQUIRE(divergent.has_value());
    CHECK(divergent->track_gates[0].output_routing.selected_output_available_verified);
    CHECK(divergent->track_gates[0].output_routing.source_target_identity_mapped);
    CHECK_FALSE(divergent->track_gates[0].output_routing.verified);
}

TEST_CASE("project postconditions expose Track output and external solo gating",
          "[ableton][project][postcondition]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    const AbletonPartTrackMap part_tracks{{score.parts[0].id, 0}, {score.parts[1].id, 1}};
    auto snapshot = make_post_snapshot(score, mix);

    auto evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->observed);
    CHECK(evidence->track_gates_requested == 2);
    CHECK(evidence->track_gates_verified == 2);
    CHECK(evidence->warnings.empty());
    REQUIRE(evidence->track_gates.size() == 2);
    CHECK(evidence->track_gates[0].expected_mixer_enabled);
    CHECK(evidence->track_gates[0].requested_crossfade_assign == 1);
    CHECK(evidence->track_gates[0].observed_crossfade_assign == 1);
    CHECK(evidence->track_gates[0].crossfade_neutral_verified);
    CHECK(evidence->track_gates[0].observed_panning_mode == 0);
    CHECK(evidence->track_gates[0].stereo_panning_verified);
    CHECK(evidence->track_gates[0].observed_track_activator == 1.0);
    CHECK(evidence->track_gates[0].observed_track_activator_minimum == 0.0);
    CHECK(evidence->track_gates[0].observed_track_activator_maximum == 1.0);
    CHECK(evidence->track_gates[0].observed_track_activator_is_quantized == true);
    CHECK(evidence->track_gates[0].observed_track_activator_is_enabled == true);
    CHECK(evidence->track_gates[0].observed_track_activator_state == 0);
    CHECK(evidence->track_gates[0].observed_track_activator_automation_state == 0);
    CHECK(evidence->track_gates[0].track_activator_range_verified);
    CHECK(evidence->track_gates[0].track_activator_quantization_verified);
    CHECK(evidence->track_gates[0].track_activator_verified);
    CHECK(evidence->track_gates[0].observed_muted_via_solo == false);
    CHECK(evidence->track_gates[0].observed_is_frozen == false);
    CHECK(evidence->track_gates[0].unfrozen_verified);
    CHECK(evidence->track_gates[0].observed_arm == false);
    CHECK(evidence->track_gates[0].disarmed_verified);
    CHECK(evidence->track_gates[0].observed_implicit_arm == false);
    CHECK(evidence->track_gates[0].implicitly_disarmed_verified);
    CHECK(evidence->track_gates[0].observed_has_audio_input == false);
    CHECK(evidence->track_gates[0].observed_has_midi_input == true);
    CHECK(evidence->track_gates[0].input_classification_verified);
    CHECK(evidence->track_gates[0].input_routing.selected_type_display_name == "All Ins");
    CHECK(evidence->track_gates[0].input_routing.selected_type_identifier == "all_ins");
    CHECK(evidence->track_gates[0].input_routing.selected_channel_display_name == "All Channels");
    CHECK(evidence->track_gates[0].input_routing.selected_channel_identifier == "all_channels");
    REQUIRE(evidence->track_gates[0].input_routing.available_types.size() == 2);
    CHECK(evidence->track_gates[0].input_routing.available_types[0].identifier == "no_input");
    REQUIRE(evidence->track_gates[0].input_routing.available_channels.size() == 2);
    CHECK(evidence->track_gates[0].input_routing.available_channels[1].identifier == "channel_1");
    CHECK(evidence->track_gates[0].input_routing.selected_input_observed);
    CHECK(evidence->track_gates[0].input_routing.selected_type_available_verified);
    CHECK(evidence->track_gates[0].input_routing.selected_channel_available_verified);
    CHECK(evidence->track_gates[0].input_routing.selected_input_available_verified);
    CHECK_FALSE(evidence->track_gates[0].input_routing.input_source_identity_mapped);
    CHECK_FALSE(evidence->track_gates[0].input_routing.external_input_neutrality_verified);
    CHECK(evidence->track_gates[0].observed_input_meter_level == 0.0);
    CHECK(evidence->track_gates[0].observed_output_meter_level == 0.0);
    CHECK(evidence->track_gates[0].meter_levels_observed);
    CHECK(evidence->track_gates[0].input_meter_hold_quiescent_verified);
    CHECK(evidence->track_gates[0].output_meter_hold_quiescent_verified);
    CHECK(evidence->track_gates[0].meter_hold_quiescence_verified);
    CHECK(evidence->track_gates[0].observed_input_meter_left == 0.0);
    CHECK(evidence->track_gates[0].observed_input_meter_right == 0.0);
    CHECK(evidence->track_gates[0].observed_output_meter_left == 0.0);
    CHECK(evidence->track_gates[0].observed_output_meter_right == 0.0);
    CHECK(evidence->track_gates[0].momentary_meter_levels_observed);
    CHECK(evidence->track_gates[0].input_stereo_momentary_quiescent_verified);
    CHECK(evidence->track_gates[0].output_stereo_momentary_quiescent_verified);
    CHECK(evidence->track_gates[0].meter_momentary_quiescence_verified);
    CHECK(evidence->track_gates[0].meter_quiescence_verified);
    CHECK_FALSE(evidence->track_gates[0].continuous_input_silence_verified);
    CHECK_FALSE(evidence->track_gates[0].continuous_output_silence_verified);
    CHECK(evidence->track_gates[0].output_routing.requested_destination ==
          AbletonOutputDestination::Master);
    CHECK_FALSE(evidence->track_gates[0].output_routing.requested_group_id.has_value());
    CHECK(evidence->track_gates[0].output_routing.selected_type_display_name == "Master");
    CHECK(evidence->track_gates[0].output_routing.selected_type_identifier == "master");
    CHECK(evidence->track_gates[0].output_routing.selected_channel_display_name == "1/2");
    CHECK(evidence->track_gates[0].output_routing.selected_channel_identifier == "stereo_1_2");
    REQUIRE(evidence->track_gates[0].output_routing.available_types.size() == 2);
    CHECK(evidence->track_gates[0].output_routing.available_types[1].display_name == "Group 1");
    CHECK(evidence->track_gates[0].output_routing.available_types[1].identifier == "group_1");
    REQUIRE(evidence->track_gates[0].output_routing.available_channels.size() == 2);
    CHECK(evidence->track_gates[0].output_routing.available_channels[1].display_name == "1");
    CHECK(evidence->track_gates[0].output_routing.available_channels[1].identifier == "mono_1");
    CHECK(evidence->track_gates[0].output_routing.selected_type_available_verified);
    CHECK(evidence->track_gates[0].output_routing.selected_channel_available_verified);
    CHECK(evidence->track_gates[0].output_routing.selected_output_available_verified);
    CHECK(evidence->track_gates[0].output_routing.selected_output_observed);
    CHECK_FALSE(evidence->track_gates[0].output_routing.source_target_identity_mapped);
    CHECK_FALSE(evidence->track_gates[0].output_routing.verified);
    CHECK(evidence->track_gates[0].observed_back_to_arranger == false);
    CHECK(evidence->track_gates[0].observed_fired_slot_index == -1);
    CHECK(evidence->track_gates[0].observed_playing_slot_index == -1);
    CHECK(evidence->track_gates[0].session_launch_quiescent_verified);
    CHECK(evidence->track_gates[0].track_arrangement_playback_aligned_verified);
    CHECK(evidence->track_gates[0].group_membership_observed);
    CHECK_FALSE(evidence->track_gates[0].observed_group_track_index.has_value());
    CHECK(evidence->track_gates[0].ungrouped_verified);
    CHECK_FALSE(evidence->track_gates[0].monitoring_state_observed);
    CHECK_FALSE(evidence->track_gates[0].clip_output_not_suppressed_by_monitoring_verified);

    auto rerouted = snapshot;
    rerouted.song_state["tracks"][0]["output_routing_type"] = {{"display_name", "Group 1"},
                                                               {"identifier", "group_1"}};
    auto rerouted_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, rerouted);
    REQUIRE(rerouted_evidence.has_value());
    CHECK(rerouted_evidence->track_gates_verified == 2);
    CHECK(rerouted_evidence->track_gates[0].output_routing.selected_type_identifier == "group_1");
    CHECK(rerouted_evidence->track_gates[0].output_routing.selected_output_available_verified);
    CHECK_FALSE(rerouted_evidence->track_gates[0].output_routing.source_target_identity_mapped);
    CHECK_FALSE(rerouted_evidence->track_gates[0].output_routing.verified);

    auto no_input_selection = snapshot;
    no_input_selection.song_state["tracks"][0]["input_routing_type"] = {
        {"display_name", "No Input"}, {"identifier", "no_input"}};
    auto no_input_selection_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, no_input_selection);
    REQUIRE(no_input_selection_evidence.has_value());
    CHECK(no_input_selection_evidence->track_gates_verified == 2);
    CHECK(no_input_selection_evidence->track_gates[0]
              .input_routing.selected_input_available_verified);
    CHECK_FALSE(
        no_input_selection_evidence->track_gates[0].input_routing.input_source_identity_mapped);
    CHECK_FALSE(no_input_selection_evidence->track_gates[0]
                    .input_routing.external_input_neutrality_verified);

    auto active_input_meter = snapshot;
    active_input_meter.song_state["tracks"][0]["input_meter_level"] = 0.25;
    auto active_input_meter_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, active_input_meter);
    REQUIRE(active_input_meter_evidence.has_value());
    CHECK(active_input_meter_evidence->track_gates_verified == 1);
    CHECK(active_input_meter_evidence->track_gates[0].meter_levels_observed);
    CHECK_FALSE(active_input_meter_evidence->track_gates[0].input_meter_hold_quiescent_verified);
    CHECK(active_input_meter_evidence->track_gates[0].output_meter_hold_quiescent_verified);
    CHECK_FALSE(active_input_meter_evidence->track_gates[0].meter_hold_quiescence_verified);
    CHECK_FALSE(active_input_meter_evidence->track_gates[0].continuous_input_silence_verified);

    auto active_output_meter = snapshot;
    active_output_meter.song_state["tracks"][0]["output_meter_level"] = 0.5;
    auto active_output_meter_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, active_output_meter);
    REQUIRE(active_output_meter_evidence.has_value());
    CHECK(active_output_meter_evidence->track_gates_verified == 1);
    CHECK(active_output_meter_evidence->track_gates[0].input_meter_hold_quiescent_verified);
    CHECK_FALSE(active_output_meter_evidence->track_gates[0].output_meter_hold_quiescent_verified);
    CHECK_FALSE(active_output_meter_evidence->track_gates[0].meter_hold_quiescence_verified);
    CHECK_FALSE(active_output_meter_evidence->track_gates[0].continuous_output_silence_verified);

    auto active_momentary_meter = snapshot;
    active_momentary_meter.song_state["tracks"][0]["output_meter_right"] = 0.25;
    auto active_momentary_meter_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, active_momentary_meter);
    REQUIRE(active_momentary_meter_evidence.has_value());
    CHECK(active_momentary_meter_evidence->track_gates_verified == 1);
    CHECK(active_momentary_meter_evidence->track_gates[0].meter_hold_quiescence_verified);
    CHECK(active_momentary_meter_evidence->track_gates[0].momentary_meter_levels_observed);
    CHECK_FALSE(
        active_momentary_meter_evidence->track_gates[0].output_stereo_momentary_quiescent_verified);
    CHECK_FALSE(
        active_momentary_meter_evidence->track_gates[0].meter_momentary_quiescence_verified);
    CHECK_FALSE(active_momentary_meter_evidence->track_gates[0].meter_quiescence_verified);

    auto wrong_input_class = snapshot;
    auto& wrong_input_track = wrong_input_class.song_state["tracks"][0];
    wrong_input_track["has_midi_input"] = false;
    wrong_input_track["input_routing_type"] = nullptr;
    wrong_input_track["input_routing_channel"] = nullptr;
    wrong_input_track["available_input_routing_types"] = nullptr;
    wrong_input_track["available_input_routing_channels"] = nullptr;
    wrong_input_track["input_meter_level"] = nullptr;
    wrong_input_track["output_meter_level"] = nullptr;
    auto wrong_input_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, wrong_input_class);
    REQUIRE(wrong_input_evidence.has_value());
    CHECK(wrong_input_evidence->track_gates_verified == 1);
    CHECK_FALSE(wrong_input_evidence->track_gates[0].input_classification_verified);
    CHECK_FALSE(wrong_input_evidence->track_gates[0].input_routing.selected_input_observed);
    CHECK(evidence->master_track_gates_requested == 1);
    CHECK(evidence->master_track_gates_verified == 1);
    REQUIRE(evidence->master_track_gate.has_value());
    CHECK(evidence->master_track_gate->observed_track_activator == 1.0);
    CHECK(evidence->master_track_gate->observed_track_activator_minimum == 0.0);
    CHECK(evidence->master_track_gate->observed_track_activator_maximum == 1.0);
    CHECK(evidence->master_track_gate->observed_track_activator_is_quantized == true);
    CHECK(evidence->master_track_gate->observed_track_activator_is_enabled == true);
    CHECK(evidence->master_track_gate->observed_track_activator_state == 0);
    CHECK(evidence->master_track_gate->observed_track_activator_automation_state == 0);
    CHECK(evidence->master_track_gate->observed_panning_mode == 0);
    CHECK(evidence->master_track_gate->observed_pan == 0.0);
    CHECK(evidence->master_track_gate->observed_pan_minimum == -1.0);
    CHECK(evidence->master_track_gate->observed_pan_maximum == 1.0);
    CHECK(evidence->master_track_gate->observed_pan_is_quantized == false);
    CHECK(evidence->master_track_gate->observed_pan_is_enabled == true);
    CHECK(evidence->master_track_gate->observed_pan_state == 0);
    CHECK(evidence->master_track_gate->observed_pan_automation_state == 0);
    CHECK(evidence->master_track_gate->track_activator_range_verified);
    CHECK(evidence->master_track_gate->track_activator_quantization_verified);
    CHECK(evidence->master_track_gate->track_activator_verified);
    CHECK(evidence->master_track_gate->stereo_panning_verified);
    CHECK(evidence->master_track_gate->pan_range_verified);
    CHECK(evidence->master_track_gate->pan_quantization_verified);
    CHECK(evidence->master_track_gate->neutral_pan_verified);
    CHECK(evidence->master_track_gate->verified);

    CommandBuffer compilation_transport;
    auto compilation = compile_project_to_ableton(project, compilation_transport);
    REQUIRE(compilation.has_value());
    auto device_evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
    REQUIRE(device_evidence.has_value());
    CHECK(device_evidence->devices_requested == 2);
    CHECK(device_evidence->devices_verified == 2);
    CHECK(device_evidence->song_states_requested == 1);
    CHECK(device_evidence->song_states_verified == 1);
    REQUIRE(device_evidence->song_state.has_value());
    CHECK(device_evidence->song_state->observed_scene_name == "Project");
    CHECK(device_evidence->song_state->observed_scene_triggered_states == std::vector<bool>{false});
    CHECK(device_evidence->song_state->scene_trigger_states_observed);
    CHECK(device_evidence->song_state->all_scene_launches_quiescent_verified);
    CHECK(device_evidence->track_gates[0].observed_clip_slots.size() == 1);
    CHECK(device_evidence->track_gates[0].observed_clip_slots[0].has_clip);
    CHECK_FALSE(device_evidence->track_gates[0].observed_clip_slots[0].has_stop_button);
    CHECK(device_evidence->track_gates[0].clip_slot_states_observed);
    CHECK(device_evidence->track_gates[0].clip_slot_non_group_semantics_verified);
    CHECK(device_evidence->track_gates[0].clip_slot_playback_idle_verified);
    CHECK(device_evidence->track_gates[0].clip_slot_recording_quiescence_verified);
    CHECK(device_evidence->track_gates[0].clip_slot_launch_quiescence_verified);
    CHECK(device_evidence->track_gates[0].clip_slot_runtime_quiescence_verified);
    CHECK(device_evidence->track_gates[0].observed_arrangement_clip_count == 0);
    CHECK(device_evidence->track_gates[0].arrangement_clip_topology_observed);
    CHECK(device_evidence->track_gates[0].arrangement_content_absent_verified);
    CHECK(device_evidence->track_gates[0].observed_take_lane_count == 0);
    CHECK(device_evidence->track_gates[0].take_lane_topology_observed);
    CHECK(device_evidence->track_gates[0].take_lanes_absent_verified);
    CHECK(device_evidence->song_state->observed_scale_root_note == 0);
    CHECK(device_evidence->song_state->observed_scale_name == "Major");
    const std::vector<int> expected_scale{0, 2, 4, 5, 7, 9, 11};
    CHECK(device_evidence->song_state->observed_scale_intervals == expected_scale);
    CHECK(device_evidence->song_state->observed_scale_mode == true);
    CHECK(device_evidence->song_state->observed_tuning_name == "12-TET");
    CHECK(device_evidence->song_state->observed_pseudo_octave_in_cents == 1200.0);
    CHECK(device_evidence->song_state->observed_tuning_lowest_note ==
          nlohmann::json{{"opaque_fixture", "lowest"}});
    CHECK(device_evidence->song_state->observed_tuning_highest_note ==
          nlohmann::json{{"opaque_fixture", "highest"}});
    CHECK(device_evidence->song_state->observed_tuning_reference_pitch ==
          nlohmann::json{{"opaque_fixture", "reference"}});
    REQUIRE(device_evidence->song_state->observed_tuning_note_tunings);
    CHECK(device_evidence->song_state->observed_tuning_note_tunings->at("opaque_fixture").size() ==
          12);
    CHECK(device_evidence->song_state->tuning_dictionary_payloads_observed);
    CHECK_FALSE(device_evidence->song_state->tuning_definition_fully_observed);
    CHECK_FALSE(device_evidence->song_state->per_track_tuning_bypass_observed);
    CHECK_FALSE(device_evidence->song_state->instrument_tuning_support_observed);
    CHECK_FALSE(device_evidence->song_state->audible_pitch_verified);
    REQUIRE(device_evidence->devices.size() == 2);
    CHECK(device_evidence->devices[0].observed_chain_size == 1);
    CHECK(device_evidence->devices[0].observed_class_display_name == "Analog");
    CHECK(device_evidence->devices[0].observed_latency_in_samples == 128);
    CHECK(device_evidence->devices[0].observed_latency_in_ms == 2.9);
    CHECK(device_evidence->devices[0].reported_latency_observed);
    CHECK_FALSE(device_evidence->devices[0].render_path_latency_fully_observed);
    CHECK(device_evidence->devices[0].observed_type == 1);
    CHECK(device_evidence->devices[0].observed_active == true);
    CHECK(device_evidence->devices[0].observed_can_have_chains == false);
    CHECK(device_evidence->devices[0].flat_device_verified);
    CHECK(device_evidence->clips_requested == 2);
    CHECK(device_evidence->clips_verified == 2);
    REQUIRE(device_evidence->clips.size() == 2);
    CHECK(device_evidence->clips[0].observed_length == 4.0);
    CHECK(device_evidence->clips[0].observed_is_audio_clip == false);
    CHECK(device_evidence->clips[0].observed_is_midi_clip == true);
    CHECK(device_evidence->clips[0].observed_is_arrangement_clip == false);
    CHECK(device_evidence->clips[0].observed_is_session_clip == true);
    CHECK(device_evidence->clips[0].observed_is_take_lane_clip == false);
    CHECK(device_evidence->clips[0].clip_identity_verified);
    CHECK(device_evidence->clips[0].observed_end_time == 4.0);
    CHECK(device_evidence->clips[0].playback_end_verified);
    CHECK(device_evidence->clips[0].requested_occupied_slot_indices ==
          std::vector<std::uint32_t>{0});
    CHECK(device_evidence->clips[0].observed_occupied_slot_indices ==
          std::vector<std::uint32_t>{0});
    CHECK(device_evidence->clips[0].exact_slot_occupancy_verified);
    CHECK(device_evidence->clips[0].observed_has_groove == false);
    CHECK(device_evidence->clips[0].groove_absence_verified);
    CHECK(device_evidence->clips[0].observed_has_envelopes == false);
    CHECK(device_evidence->clips[0].observed_is_playing == false);
    CHECK(device_evidence->clips[0].observed_is_recording == false);
    CHECK(device_evidence->clips[0].observed_is_overdubbing == false);
    CHECK(device_evidence->clips[0].observed_is_triggered == false);
    CHECK(device_evidence->clips[0].observed_will_record_on_start == false);
    CHECK(device_evidence->clips[0].runtime_state_observed);
    CHECK(device_evidence->clips[0].recording_quiescence_verified);
    CHECK(device_evidence->clips[0].playback_idle_verified);
    REQUIRE(device_evidence->song_state.has_value());
    CHECK(device_evidence->song_state->observed_is_playing == false);
    CHECK(device_evidence->song_state->transport_stopped_verified);
    CHECK(device_evidence->song_state->recording_modes_quiescent_verified);
    CHECK(device_evidence->song_state->public_tempo_controls_quiescent_verified);
    CHECK_FALSE(device_evidence->song_state->external_midi_sync_state_observed);
    CHECK_FALSE(device_evidence->song_state->tempo_automation_state_observed);
    CHECK_FALSE(device_evidence->song_state->tempo_stability_verified);
    CHECK(device_evidence->song_state->arrangement_playback_aligned_verified);
    CHECK(device_evidence->song_state->automation_overrides_quiescent_verified);
    CHECK(device_evidence->song_state->arrangement_loop_disabled_verified);
    CHECK(device_evidence->song_state->metronome_disabled_verified);
    CHECK_FALSE(device_evidence->clips[0].mpe_note_expression_state_observed);
    CHECK_FALSE(device_evidence->clips[0].mpe_note_expression_neutrality_verified);
    CHECK(device_evidence->clips[0].envelope_absence_verified);
    CHECK(device_evidence->clips[0].observed_launch_mode == 0);
    CHECK(device_evidence->clips[0].observed_launch_quantization == 1);
    CHECK(device_evidence->clips[0].observed_legato == false);
    CHECK(device_evidence->clips[0].observed_velocity_amount == 0.0);
    CHECK(device_evidence->clips[0].launch_behavior_verified);
    CHECK_FALSE(device_evidence->clips[0].follow_actions_observed);
    CHECK_FALSE(device_evidence->clips[0].one_shot_playback_verified);
    CHECK_FALSE(device_evidence->clips[0].midi_bank_program_state_observed);
    CHECK_FALSE(device_evidence->clips[0].program_change_suppression_verified);
    CHECK_FALSE(device_evidence->clips[0].audible_timing_verified);
    CHECK_FALSE(device_evidence->clips[0].audible_velocity_verified);
    CHECK(device_evidence->cues_requested == 1);
    CHECK(device_evidence->cues_verified == 1);
    REQUIRE(device_evidence->cues.size() == 1);
    CHECK(device_evidence->cues[0].observed_name == "Intro");
    CHECK(device_evidence->mixer_properties_requested == 9);
    CHECK(device_evidence->mixer_properties_verified == 9);
    REQUIRE(device_evidence->mixer_properties.size() == 9);
    CHECK_FALSE(device_evidence->mixer_properties[0].requested_is_quantized);
    CHECK(device_evidence->mixer_properties[0].observed_minimum == -1.0);
    CHECK(device_evidence->mixer_properties[0].observed_maximum == 1.0);
    CHECK(device_evidence->mixer_properties[0].observed_is_quantized == false);
    CHECK(device_evidence->mixer_properties[0].observed_is_enabled == true);
    CHECK(device_evidence->mixer_properties[0].observed_state == 0);
    CHECK(device_evidence->mixer_properties[0].observed_automation_state == 0);
    REQUIRE(device_evidence->mixer_properties[0].range_verified.has_value());
    CHECK(*device_evidence->mixer_properties[0].range_verified);
    CHECK(device_evidence->mixer_properties[0].quantization_verified);
    const auto activator_evidence = std::find_if(
        device_evidence->mixer_properties.begin(),
        device_evidence->mixer_properties.end(),
        [](const auto& candidate) {
            return candidate.parameter_path == "song/master_track/mixer_device/track_activator";
        });
    REQUIRE(activator_evidence != device_evidence->mixer_properties.end());
    CHECK(activator_evidence->requested_is_quantized);
    CHECK(activator_evidence->observed_is_quantized == true);
    REQUIRE(activator_evidence->range_verified.has_value());
    CHECK(*activator_evidence->range_verified);
    CHECK(activator_evidence->quantization_verified);
    CHECK(activator_evidence->verified);
    const auto display_parameter_evidence = std::find_if(
        device_evidence->mixer_properties.begin(),
        device_evidence->mixer_properties.end(),
        [](const auto& candidate) { return candidate.value_property == "display_value"; });
    REQUIRE(display_parameter_evidence != device_evidence->mixer_properties.end());
    CHECK_FALSE(display_parameter_evidence->requested_is_quantized);
    CHECK(display_parameter_evidence->observed_is_quantized == false);
    CHECK_FALSE(display_parameter_evidence->range_verified.has_value());
    CHECK(display_parameter_evidence->quantization_verified);
    CHECK(display_parameter_evidence->verified);

    auto divergent_mixer_snapshot = snapshot;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["volume"]["display_value"] = -6.0;
    auto mixer_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_mixer_snapshot, &*compilation);
    REQUIRE(mixer_evidence.has_value());
    CHECK(mixer_evidence->mixer_properties_verified == 8);

    divergent_mixer_snapshot = snapshot;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["panning"]["automation_state"] = 1;
    mixer_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_mixer_snapshot, &*compilation);
    REQUIRE(mixer_evidence.has_value());
    CHECK(mixer_evidence->mixer_properties_verified == 8);

    divergent_mixer_snapshot = snapshot;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["panning"]["is_quantized"] = true;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["panning"]["default_value"] = nullptr;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["panning"]["value_items"] =
        nlohmann::json::array({"Left", "Centre", "Right"});
    mixer_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_mixer_snapshot, &*compilation);
    REQUIRE(mixer_evidence.has_value());
    CHECK(mixer_evidence->mixer_properties_verified == 8);
    const auto divergent_pan =
        std::find_if(mixer_evidence->mixer_properties.begin(),
                     mixer_evidence->mixer_properties.end(),
                     [](const auto& candidate) {
                         return candidate.parameter_path == "song/tracks/0/mixer_device/panning";
                     });
    REQUIRE(divergent_pan != mixer_evidence->mixer_properties.end());
    CHECK_FALSE(divergent_pan->quantization_verified);
    CHECK_FALSE(divergent_pan->verified);

    divergent_mixer_snapshot = snapshot;
    divergent_mixer_snapshot.song_state["tracks"][0]["mixer"]["panning"]["is_enabled"] = false;
    mixer_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_mixer_snapshot, &*compilation);
    REQUIRE(mixer_evidence.has_value());
    CHECK(mixer_evidence->mixer_properties_verified == 8);

    auto divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["tempo"] = 121.0;
    auto song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->song_states_verified == 0);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    auto second_scene = divergent_song_snapshot.song_state["scenes"][0];
    second_scene["name"] = "Unexpected second row";
    second_scene["is_triggered"] = true;
    divergent_song_snapshot.song_state["scenes"].push_back(second_scene);
    divergent_song_snapshot.song_state["scene_count"] = 2;
    for (auto& track : divergent_song_snapshot.song_state["tracks"]) {
        track["clip_slot_count"] = 2;
        auto empty_slot = track["clip_slots"][0];
        empty_slot["slot"] = 1;
        empty_slot["has_clip"] = false;
        empty_slot["has_stop_button"] = true;
        track["clip_slots"].push_back(std::move(empty_slot));
    }
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->song_state->observed_scene_triggered_states ==
          std::vector<bool>{false, true});
    CHECK(song_evidence->song_state->scene_trigger_states_observed);
    CHECK_FALSE(song_evidence->song_state->all_scene_launches_quiescent_verified);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    second_scene = divergent_song_snapshot.song_state["scenes"][0];
    second_scene["name"] = "Idle second row";
    divergent_song_snapshot.song_state["scenes"].push_back(second_scene);
    divergent_song_snapshot.song_state["scene_count"] = 2;
    for (auto& track : divergent_song_snapshot.song_state["tracks"]) {
        track["clip_slot_count"] = 2;
        auto triggered_empty_slot = track["clip_slots"][0];
        triggered_empty_slot["slot"] = 1;
        triggered_empty_slot["has_clip"] = false;
        triggered_empty_slot["has_stop_button"] = true;
        triggered_empty_slot["is_triggered"] = true;
        track["clip_slots"].push_back(std::move(triggered_empty_slot));
    }
    divergent_song_snapshot.song_state["tracks"][1]["clip_slots"][1]["is_triggered"] = false;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->song_states_verified == 1);
    CHECK(song_evidence->track_gates_verified == 1);
    CHECK(song_evidence->track_gates[0].clip_slot_states_observed);
    CHECK_FALSE(song_evidence->track_gates[0].clip_slot_launch_quiescence_verified);
    CHECK_FALSE(song_evidence->track_gates[0].clip_slot_runtime_quiescence_verified);
    CHECK_FALSE(song_evidence->track_gates[0].verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["tracks"][0]["arrangement_clip_count"] = 1;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->track_gates[0].observed_arrangement_clip_count == 1);
    CHECK(song_evidence->track_gates[0].arrangement_clip_topology_observed);
    CHECK_FALSE(song_evidence->track_gates[0].arrangement_content_absent_verified);
    CHECK_FALSE(song_evidence->track_gates[0].verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["tracks"][0]["take_lane_count"] = 1;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->track_gates[0].observed_take_lane_count == 1);
    CHECK(song_evidence->track_gates[0].take_lane_topology_observed);
    CHECK_FALSE(song_evidence->track_gates[0].take_lanes_absent_verified);
    CHECK_FALSE(song_evidence->track_gates[0].verified);

    for (const auto* property : {"is_counting_in",
                                 "arrangement_overdub",
                                 "overdub",
                                 "record_mode",
                                 "session_record",
                                 "session_automation_record"}) {
        divergent_song_snapshot = snapshot;
        divergent_song_snapshot.song_state[property] = true;
        song_evidence = evaluate_project_ableton_postconditions(
            project, part_tracks, divergent_song_snapshot, &*compilation);
        REQUIRE(song_evidence.has_value());
        CHECK_FALSE(song_evidence->song_state->recording_modes_quiescent_verified);
        CHECK_FALSE(song_evidence->song_state->verified);
    }

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["is_playing"] = true;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK_FALSE(song_evidence->song_state->transport_stopped_verified);
    CHECK(song_evidence->song_state->recording_modes_quiescent_verified);
    CHECK(song_evidence->song_state->verified);

    for (const auto* property : {"is_ableton_link_enabled",
                                 "is_ableton_link_start_stop_sync_enabled",
                                 "tempo_follower_enabled",
                                 "nudge_down",
                                 "nudge_up"}) {
        divergent_song_snapshot = snapshot;
        divergent_song_snapshot.song_state[property] = true;
        song_evidence = evaluate_project_ableton_postconditions(
            project, part_tracks, divergent_song_snapshot, &*compilation);
        REQUIRE(song_evidence.has_value());
        CHECK_FALSE(song_evidence->song_state->public_tempo_controls_quiescent_verified);
        CHECK_FALSE(song_evidence->song_state->verified);
    }

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["back_to_arranger"] = true;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK_FALSE(song_evidence->song_state->arrangement_playback_aligned_verified);
    CHECK(song_evidence->song_state->automation_overrides_quiescent_verified);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["re_enable_automation_enabled"] = true;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->song_state->arrangement_playback_aligned_verified);
    CHECK_FALSE(song_evidence->song_state->automation_overrides_quiescent_verified);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["loop"] = true;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK_FALSE(song_evidence->song_state->arrangement_loop_disabled_verified);
    CHECK(song_evidence->song_state->metronome_disabled_verified);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["metronome"] = true;
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->song_state->arrangement_loop_disabled_verified);
    CHECK_FALSE(song_evidence->song_state->metronome_disabled_verified);
    CHECK_FALSE(song_evidence->song_state->verified);

    divergent_song_snapshot = snapshot;
    divergent_song_snapshot.song_state["cue_points"].push_back(
        {{"name", "Duplicate"}, {"time", 0.0}});
    song_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_song_snapshot, &*compilation);
    REQUIRE(song_evidence.has_value());
    CHECK(song_evidence->cues_verified == 0);
    CHECK_FALSE(song_evidence->cues[0].verified);

    auto divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["looping"] = true;
    auto clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_midi_clip"] = false;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_audio_clip"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK_FALSE(clip_evidence->clips[0].clip_identity_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_session_clip"] = false;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    CHECK(clip_evidence.error() == ErrorCode::ProtocolError);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["end_marker"] = 3.0;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["end_time"] = 3.0;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK_FALSE(clip_evidence->clips[0].playback_end_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["has_envelopes"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK_FALSE(clip_evidence->clips[0].envelope_absence_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_recording"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips[0].runtime_state_observed);
    CHECK_FALSE(clip_evidence->clips[0].recording_quiescence_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_overdubbing"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK_FALSE(clip_evidence->clips[0].recording_quiescence_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["will_record_on_start"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK_FALSE(clip_evidence->clips[0].recording_quiescence_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_playing"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips[0].recording_quiescence_verified);
    CHECK_FALSE(clip_evidence->clips[0].playback_idle_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["is_triggered"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK_FALSE(clip_evidence->clips[0].playback_idle_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["launch_mode"] = 2;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK_FALSE(clip_evidence->clips[0].launch_behavior_verified);
    CHECK_FALSE(clip_evidence->clips[0].one_shot_playback_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"][0]["has_groove"] = true;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK_FALSE(clip_evidence->clips[0].groove_absence_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"] = nlohmann::json::array();
    divergent_clip_snapshot.song_state["tracks"][0]["clip_slots"][0]["has_clip"] = false;
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK_FALSE(clip_evidence->clips[0].observed_length.has_value());

    divergent_clip_snapshot = snapshot;
    divergent_clip_snapshot.song_state["scene_count"] = 2;
    auto extra_scene = divergent_clip_snapshot.song_state["scenes"][0];
    extra_scene["name"] = "Pre-existing Scene";
    divergent_clip_snapshot.song_state["scenes"].push_back(std::move(extra_scene));
    for (auto& observed_track : divergent_clip_snapshot.song_state["tracks"]) {
        observed_track["clip_slot_count"] = 2;
        auto extra_slot = observed_track["clip_slots"][0];
        extra_slot["slot"] = 1;
        extra_slot["has_clip"] = false;
        observed_track["clip_slots"].push_back(std::move(extra_slot));
    }
    auto extra_clip = divergent_clip_snapshot.song_state["tracks"][0]["clips"][0];
    extra_clip["slot"] = 1;
    extra_clip["name"] = "Unowned Clip";
    divergent_clip_snapshot.song_state["tracks"][0]["clip_slots"][1]["has_clip"] = true;
    divergent_clip_snapshot.song_state["tracks"][0]["clips"].push_back(std::move(extra_clip));
    clip_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_clip_snapshot, &*compilation);
    REQUIRE(clip_evidence.has_value());
    CHECK(clip_evidence->clips_verified == 1);
    CHECK(clip_evidence->clips[0].observed_occupied_slot_indices ==
          std::vector<std::uint32_t>{0, 1});
    CHECK_FALSE(clip_evidence->clips[0].exact_slot_occupancy_verified);
    CHECK_FALSE(clip_evidence->clips[0].verified);

    auto divergent_device_snapshot = snapshot;
    divergent_device_snapshot.song_state["tracks"][0]["devices"][0]["is_active"] = false;
    device_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_device_snapshot, &*compilation);
    REQUIRE(device_evidence.has_value());
    CHECK(device_evidence->devices_verified == 1);
    CHECK_FALSE(device_evidence->devices[0].verified);

    divergent_device_snapshot = snapshot;
    divergent_device_snapshot.song_state["tracks"][0]["devices"][0]["can_have_chains"] = true;
    device_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_device_snapshot, &*compilation);
    REQUIRE(device_evidence.has_value());
    CHECK(device_evidence->devices_verified == 1);
    CHECK_FALSE(device_evidence->devices[0].flat_device_verified);
    CHECK_FALSE(device_evidence->devices[0].verified);

    divergent_device_snapshot = snapshot;
    divergent_device_snapshot.song_state["tracks"][0]["devices"].push_back(
        divergent_device_snapshot.song_state["tracks"][0]["devices"][0]);
    device_evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, divergent_device_snapshot, &*compilation);
    REQUIRE(device_evidence.has_value());
    CHECK(device_evidence->devices_verified == 1);
    CHECK(device_evidence->devices[0].observed_chain_size == 2);
    CHECK_FALSE(device_evidence->devices[0].verified);

    auto malformed_compilation = *compilation;
    malformed_compilation.timbre[0].compilation.device_deployments[0].device_path =
        "song/tracks/0/devices/1";
    CHECK(evaluate_project_ableton_postconditions(
              project, part_tracks, snapshot, &malformed_compilation)
              .error() == ErrorCode::ProtocolError);

    malformed_compilation = *compilation;
    const auto clip_path = LomPaths::clip(0, 0).to_string();
    auto clip_name =
        std::find_if(malformed_compilation.score.property_deployments.begin(),
                     malformed_compilation.score.property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "name";
                     });
    REQUIRE(clip_name != malformed_compilation.score.property_deployments.end());
    clip_name->requested = "Wrong";
    CHECK(evaluate_project_ableton_postconditions(
              project, part_tracks, snapshot, &malformed_compilation)
              .error() == ErrorCode::ProtocolError);

    malformed_compilation = *compilation;
    auto song_tempo =
        std::find_if(malformed_compilation.score.property_deployments.begin(),
                     malformed_compilation.score.property_deployments.end(),
                     [](const auto& deployment) {
                         return deployment.path == "song" && deployment.property == "tempo";
                     });
    REQUIRE(song_tempo != malformed_compilation.score.property_deployments.end());
    song_tempo->requested = "fast";
    CHECK(evaluate_project_ableton_postconditions(
              project, part_tracks, snapshot, &malformed_compilation)
              .error() == ErrorCode::ProtocolError);

    malformed_compilation = *compilation;
    REQUIRE_FALSE(malformed_compilation.mix.property_deployments.empty());
    malformed_compilation.mix.property_deployments[0].path =
        "song/tracks/0/mixer_device/sends/not-an-index";
    CHECK(evaluate_project_ableton_postconditions(
              project, part_tracks, snapshot, &malformed_compilation)
              .error() == ErrorCode::ProtocolError);

    snapshot.song_state["tracks"][0]["muted_via_solo"] = true;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    REQUIRE(evidence->warnings.size() == 1);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][1]["has_audio_output"] = false;
    snapshot.song_state["tracks"][1]["has_midi_output"] = true;
    snapshot.song_state["tracks"][1]["input_meter_left"] = nullptr;
    snapshot.song_state["tracks"][1]["input_meter_right"] = nullptr;
    snapshot.song_state["tracks"][1]["output_meter_left"] = nullptr;
    snapshot.song_state["tracks"][1]["output_meter_right"] = nullptr;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK_FALSE(evidence->track_gates[1].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["is_frozen"] = true;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK_FALSE(evidence->track_gates[0].unfrozen_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["arm"] = true;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK(evidence->track_gates[0].observed_arm == true);
    CHECK_FALSE(evidence->track_gates[0].disarmed_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["implicit_arm"] = true;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK(evidence->track_gates[0].observed_implicit_arm == true);
    CHECK_FALSE(evidence->track_gates[0].implicitly_disarmed_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["back_to_arranger"] = true;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK_FALSE(evidence->track_gates[0].track_arrangement_playback_aligned_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["fired_slot_index"] = 0;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK_FALSE(evidence->track_gates[0].session_launch_quiescent_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["playing_slot_index"] = 0;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK_FALSE(evidence->track_gates[0].track_arrangement_playback_aligned_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["group_track_index"] = 1;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK(evidence->track_gates[0].group_membership_observed);
    CHECK(evidence->track_gates[0].observed_group_track_index == 1);
    CHECK_FALSE(evidence->track_gates[0].ungrouped_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["mixer"]["crossfade_assign"] = 0;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK(evidence->track_gates[0].observed_crossfade_assign == 0);
    CHECK_FALSE(evidence->track_gates[0].crossfade_neutral_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["tracks"][0]["mixer"]["panning_mode"] = 1;
    evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 1);
    CHECK(evidence->track_gates[0].observed_panning_mode == 1);
    CHECK_FALSE(evidence->track_gates[0].stereo_panning_verified);
    CHECK_FALSE(evidence->track_gates[0].verified);

    for (const auto& [property, divergent_value] :
         std::vector<std::pair<std::string, nlohmann::json>>{{"value", 0.0},
                                                             {"is_quantized", false},
                                                             {"is_enabled", false},
                                                             {"state", 1},
                                                             {"automation_state", 1}}) {
        snapshot = make_post_snapshot(score, mix);
        snapshot.song_state["tracks"][0]["mixer"]["track_activator"][property] = divergent_value;
        if (property == "is_quantized") {
            snapshot.song_state["tracks"][0]["mixer"]["track_activator"]["default_value"] = 0.0;
            snapshot.song_state["tracks"][0]["mixer"]["track_activator"]["value_items"] = nullptr;
        }
        evidence = evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
        REQUIRE(evidence.has_value());
        CHECK(evidence->track_gates_verified == 1);
        CHECK_FALSE(evidence->track_gates[0].track_activator_verified);
        CHECK_FALSE(evidence->track_gates[0].verified);
    }

    snapshot.song_state["tracks"][0]["unexpected"] = true;
    CHECK(evaluate_project_ableton_postconditions(project, part_tracks, snapshot).error() ==
          ErrorCode::ProtocolError);
}

TEST_CASE("project postconditions close generated Aux Return Track audibility",
          "[ableton][project][postcondition][return-gate]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    AuxBus aux;
    aux.id = AuxBusId{200};
    aux.name = "Hall";
    aux.return_level = -6.0f;
    aux.return_spatial.pan = 0.25f;
    mix.aux_buses.push_back(aux);
    const ProjectView project{score, profiles, mix};
    const AbletonPartTrackMap part_tracks{{score.parts[0].id, 0}, {score.parts[1].id, 1}};
    CommandBuffer transport;
    const auto compilation = compile_project_to_ableton(project, transport);
    REQUIRE(compilation.has_value());
    CHECK(compilation->postconditions.return_track_gates_requested == 1);
    CHECK(compilation->postconditions.return_track_gates_verified == 0);
    REQUIRE(compilation->postconditions.return_track_gates.size() == 1);
    CHECK(compilation->postconditions.return_track_gates[0].aux_bus_id == aux.id);
    CHECK(compilation->postconditions.return_track_gates[0].track_index == 0);
    CHECK(compilation->postconditions.return_track_gates[0].output_routing.requested_destination ==
          AbletonOutputDestination::Master);
    CHECK_FALSE(
        compilation->postconditions.return_track_gates[0].output_routing.selected_output_observed);
    CHECK(compilation->postconditions.return_track_gates[0].output_routing.available_types.empty());
    CHECK(compilation->postconditions.return_track_gates[0]
              .output_routing.available_channels.empty());
    CHECK_FALSE(compilation->postconditions.return_track_gates[0]
                    .output_routing.selected_output_available_verified);

    auto snapshot = make_post_snapshot(score, mix);
    auto evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
    REQUIRE(evidence.has_value());
    CHECK(evidence->return_track_gates_requested == 1);
    CHECK(evidence->return_track_gates_verified == 1);
    REQUIRE(evidence->return_track_gates.size() == 1);
    const auto& gate = evidence->return_track_gates[0];
    CHECK(gate.requested_name == "Hall");
    CHECK_FALSE(gate.requested_mute);
    CHECK_FALSE(gate.requested_solo);
    CHECK(gate.requested_crossfade_assign == 1);
    CHECK(gate.requested_panning_mode == 0);
    CHECK(gate.requested_track_activator == 1.0);
    CHECK(gate.expected_mixer_enabled);
    CHECK(gate.observed_name == "Hall");
    CHECK(gate.observed_mute == false);
    CHECK(gate.observed_solo == false);
    CHECK(gate.observed_muted_via_solo == false);
    CHECK(gate.output_routing.requested_destination == AbletonOutputDestination::Master);
    CHECK_FALSE(gate.output_routing.requested_group_id.has_value());
    CHECK(gate.output_routing.selected_type_display_name == "Master");
    CHECK(gate.output_routing.selected_type_identifier == "master");
    CHECK(gate.output_routing.selected_channel_display_name == "1/2");
    CHECK(gate.output_routing.selected_channel_identifier == "stereo_1_2");
    REQUIRE(gate.output_routing.available_types.size() == 2);
    CHECK(gate.output_routing.available_types[0].display_name == "Master");
    CHECK(gate.output_routing.available_types[0].identifier == "master");
    REQUIRE(gate.output_routing.available_channels.size() == 2);
    CHECK(gate.output_routing.available_channels[0].display_name == "1/2");
    CHECK(gate.output_routing.available_channels[0].identifier == "stereo_1_2");
    CHECK(gate.output_routing.selected_type_available_verified);
    CHECK(gate.output_routing.selected_channel_available_verified);
    CHECK(gate.output_routing.selected_output_available_verified);
    CHECK(gate.output_routing.selected_output_observed);
    CHECK_FALSE(gate.output_routing.source_target_identity_mapped);
    CHECK_FALSE(gate.output_routing.verified);
    CHECK(gate.observed_crossfade_assign == 1);
    CHECK(gate.observed_panning_mode == 0);
    CHECK(gate.observed_track_activator == 1.0);
    CHECK(gate.observed_track_activator_minimum == 0.0);
    CHECK(gate.observed_track_activator_maximum == 1.0);
    CHECK(gate.observed_track_activator_is_quantized == true);
    CHECK(gate.observed_track_activator_is_enabled == true);
    CHECK(gate.observed_track_activator_state == 0);
    CHECK(gate.observed_track_activator_automation_state == 0);
    CHECK(gate.crossfade_neutral_verified);
    CHECK(gate.stereo_panning_verified);
    CHECK(gate.track_activator_range_verified);
    CHECK(gate.track_activator_quantization_verified);
    CHECK(gate.track_activator_verified);
    CHECK(gate.verified);

    for (const auto* property : {"mute", "solo", "muted_via_solo"}) {
        auto divergent = make_post_snapshot(score, mix);
        divergent.song_state["return_tracks"][0][property] = true;
        evidence =
            evaluate_project_ableton_postconditions(project, part_tracks, divergent, &*compilation);
        REQUIRE(evidence.has_value());
        CHECK(evidence->return_track_gates_verified == 0);
        CHECK_FALSE(evidence->return_track_gates[0].verified);
    }

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["return_tracks"][0]["mixer"]["crossfade_assign"] = 2;
    evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
    REQUIRE(evidence.has_value());
    CHECK_FALSE(evidence->return_track_gates[0].crossfade_neutral_verified);
    CHECK_FALSE(evidence->return_track_gates[0].verified);

    snapshot = make_post_snapshot(score, mix);
    snapshot.song_state["return_tracks"][0]["mixer"]["panning_mode"] = 1;
    evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
    REQUIRE(evidence.has_value());
    CHECK_FALSE(evidence->return_track_gates[0].stereo_panning_verified);
    CHECK_FALSE(evidence->return_track_gates[0].verified);

    for (const auto& [property, divergent_value] :
         std::vector<std::pair<std::string, nlohmann::json>>{{"value", 0.0},
                                                             {"is_quantized", false},
                                                             {"is_enabled", false},
                                                             {"state", 1},
                                                             {"automation_state", 1}}) {
        snapshot = make_post_snapshot(score, mix);
        snapshot.song_state["return_tracks"][0]["mixer"]["track_activator"][property] =
            divergent_value;
        if (property == "is_quantized") {
            snapshot.song_state["return_tracks"][0]["mixer"]["track_activator"]["default_value"] =
                0.0;
            snapshot.song_state["return_tracks"][0]["mixer"]["track_activator"]["value_items"] =
                nullptr;
        }
        evidence =
            evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
        REQUIRE(evidence.has_value());
        CHECK_FALSE(evidence->return_track_gates[0].track_activator_verified);
        CHECK_FALSE(evidence->return_track_gates[0].verified);
    }
}

TEST_CASE("output-route evidence preserves Sunny Group intent without guessing Live identities",
          "[ableton][project][postcondition][output-routing]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});

    GroupBus group;
    group.id = GroupBusId{300};
    group.name = "Submix";
    group.member_channels = {mix.channels[0].id};
    mix.group_buses.push_back(group);
    mix.channels[0].group_assignment = group.id;

    AuxBus aux;
    aux.id = AuxBusId{400};
    aux.name = "Parallel";
    aux.output.type = AuxOutputType::Group;
    aux.output.group_id = group.id;
    mix.aux_buses.push_back(aux);

    const ProjectView project{score, profiles, mix};
    const AbletonPartTrackMap part_tracks{{score.parts[0].id, 0}, {score.parts[1].id, 1}};
    CommandBuffer transport;
    const auto compilation = compile_project_to_ableton(project, transport);
    REQUIRE(compilation.has_value());

    const auto snapshot = make_post_snapshot(score, mix);
    const auto evidence =
        evaluate_project_ableton_postconditions(project, part_tracks, snapshot, &*compilation);
    REQUIRE(evidence.has_value());
    REQUIRE(evidence->track_gates.size() == 2);
    REQUIRE(evidence->return_track_gates.size() == 1);

    const auto& track_route = evidence->track_gates[0].output_routing;
    CHECK(track_route.requested_destination == AbletonOutputDestination::Group);
    CHECK(track_route.requested_group_id == group.id);
    CHECK(track_route.selected_type_identifier == "master");
    CHECK(track_route.selected_output_available_verified);
    CHECK(track_route.selected_output_observed);
    CHECK_FALSE(track_route.source_target_identity_mapped);
    CHECK_FALSE(track_route.verified);

    const auto& return_route = evidence->return_track_gates[0].output_routing;
    CHECK(return_route.requested_destination == AbletonOutputDestination::Group);
    CHECK(return_route.requested_group_id == group.id);
    CHECK(return_route.selected_type_identifier == "master");
    CHECK(return_route.selected_output_available_verified);
    CHECK(return_route.selected_output_observed);
    CHECK_FALSE(return_route.source_target_identity_mapped);
    CHECK_FALSE(return_route.verified);
}

TEST_CASE("project postconditions close the Main Track neutral output stage",
          "[ableton][project][postcondition][master-gate]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    const AbletonPartTrackMap part_tracks{{score.parts[0].id, 0}, {score.parts[1].id, 1}};

    const std::vector<std::pair<std::string, nlohmann::json>> divergences{
        {"/track_activator/value", 0.0},
        {"/track_activator/is_quantized", false},
        {"/track_activator/is_enabled", false},
        {"/track_activator/state", 1},
        {"/track_activator/automation_state", 1},
        {"/panning_mode", 1},
        {"/panning/value", 0.25},
        {"/panning/is_quantized", true},
        {"/panning/is_enabled", false},
        {"/panning/state", 1},
        {"/panning/automation_state", 1},
    };
    for (const auto& [pointer, divergent_value] : divergences) {
        auto snapshot = make_post_snapshot(score, mix);
        snapshot.song_state["master_track"]["mixer"][nlohmann::json::json_pointer(pointer)] =
            divergent_value;
        if (pointer == "/track_activator/is_quantized") {
            snapshot.song_state["master_track"]["mixer"]["track_activator"]["default_value"] = 0.0;
            snapshot.song_state["master_track"]["mixer"]["track_activator"]["value_items"] =
                nullptr;
        }
        if (pointer == "/panning/is_quantized") {
            snapshot.song_state["master_track"]["mixer"]["panning"]["default_value"] = nullptr;
            snapshot.song_state["master_track"]["mixer"]["panning"]["value_items"] =
                nlohmann::json::array({"Left", "Centre", "Right"});
        }
        const auto evidence =
            evaluate_project_ableton_postconditions(project, part_tracks, snapshot);
        REQUIRE(evidence.has_value());
        CHECK(evidence->master_track_gates_requested == 1);
        CHECK(evidence->master_track_gates_verified == 0);
        REQUIRE(evidence->master_track_gate.has_value());
        CHECK_FALSE(evidence->master_track_gate->verified);
    }
}

TEST_CASE("project postconditions preserve intentional solo gating",
          "[ableton][project][postcondition]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    mix.channels[0].solo = true;
    const ProjectView project{score, profiles, mix};
    const AbletonPartTrackMap part_tracks{{score.parts[0].id, 0}, {score.parts[1].id, 1}};

    const auto evidence = evaluate_project_ableton_postconditions(
        project, part_tracks, make_post_snapshot(score, mix));
    REQUIRE(evidence.has_value());
    CHECK(evidence->track_gates_verified == 2);
    CHECK(evidence->track_gates[0].expected_mixer_enabled);
    CHECK_FALSE(evidence->track_gates[1].expected_mixer_enabled);
    CHECK(evidence->track_gates[1].observed_muted_via_solo == true);
}

TEST_CASE("project compiler preflight sends nothing for an invalid target parameter map",
          "[ableton][project][preflight]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    first.rendering.parameter_map["source.filter.cutoff"] = {
        0, "Filter Freq", 0.0f, 1.0f, {MappingCurveType::Linear, {}}};
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    CommandBuffer transport;

    const auto result = compile_project_to_ableton(project, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProjectValidationFailed);
    CHECK(transport.entries().empty());
}

TEST_CASE("project plan is a read-only exact dry run and applies one guarded sequence",
          "[ableton][project][plan]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&second, &first};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[1].id, score.parts[0].id});
    const ProjectView project{score, profiles, mix};
    CommandBuffer target;
    target.set_scene_count(0);

    auto plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    CHECK(target.entries().empty());
    CHECK_FALSE(plan->mutations.empty());
    CHECK(plan->mutations.front().request.property_or_method == "create_scene");
    CHECK(plan->part_tracks.at(score.parts[0].id) == 0);
    CHECK(plan->part_tracks.at(score.parts[1].id) == 1);

    auto attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::Completed);
    REQUIRE(attempt.compilation.has_value());
    CHECK(attempt.mutation_journal.size() == plan->mutations.size());
    CHECK(target.entries().size() == plan->mutations.size());
    const auto scene_count = target.scene_count();
    REQUIRE(scene_count.has_value());
    REQUIRE(scene_count->has_value());
    CHECK(**scene_count == 1);
    CHECK_FALSE(attempt.target_may_be_partially_modified());
    CHECK(std::all_of(
        attempt.mutation_journal.begin(), attempt.mutation_journal.end(), [](const auto& entry) {
            return entry.outcome == AbletonMutationOutcome::RecordedOnly;
        }));

    const auto entries_after_apply = target.entries().size();
    attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::PlanConsumed);
    CHECK(attempt.error == ErrorCode::ProjectPlanConsumed);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().size() == entries_after_apply);
}

TEST_CASE("project apply re-queries exact complete Clip notes after aggregate mutation",
          "[ableton][project][postcondition][notes]") {
    auto score = make_score();
    insert_project_note(score, 0);
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};

    CommandBuffer seed;
    const auto before = seed.target_snapshot();
    REQUIRE(before.has_value());
    REQUIRE(before->has_value());

    SECTION("exact final identity and properties verify") {
        ProjectNoteTransport target{**before, make_post_snapshot(score, mix)};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
        REQUIRE(attempt.compilation.has_value());
        const auto& postconditions = attempt.compilation->postconditions;
        CHECK(postconditions.note_batches_requested == 1);
        CHECK(postconditions.note_batches_verified == 1);
        CHECK(postconditions.notes_requested == 1);
        CHECK(postconditions.notes_verified == 1);
        REQUIRE(postconditions.note_batches.size() == 1);
        CHECK(postconditions.note_batches[0].requested_note_ids == std::vector<int>{100});
        REQUIRE(postconditions.note_batches[0].observed_notes.size() == 1);
        CHECK(postconditions.note_batches[0].identity_verified);
        CHECK(postconditions.note_batches[0].properties_verified);
        CHECK(postconditions.note_batches[0].verified);
        CHECK(target.all_note_queries() == 2);
        REQUIRE(target.events.size() >= 2);
        CHECK(target.events[target.events.size() - 2] == "snapshot_after");
        CHECK(target.events.back() == "get_all_notes_extended");
    }

    SECTION("well-formed extra note is retained as target divergence") {
        ProjectNoteTransport target{**before, make_post_snapshot(score, mix), FinalNoteMode::Extra};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
        REQUIRE(attempt.compilation.has_value());
        const auto& postconditions = attempt.compilation->postconditions;
        CHECK(postconditions.note_batches_verified == 0);
        CHECK(postconditions.notes_verified == 0);
        REQUIRE(postconditions.note_batches[0].observed_notes.size() == 2);
        CHECK_FALSE(postconditions.note_batches[0].identity_verified);
        CHECK_FALSE(postconditions.note_batches[0].properties_verified);
        CHECK_FALSE(postconditions.note_batches[0].verified);
        CHECK_FALSE(postconditions.warnings.empty());
    }

    SECTION("well-formed final probability divergence is retained and incomplete") {
        ProjectNoteTransport target{
            **before, make_post_snapshot(score, mix), FinalNoteMode::ProbabilityDivergent};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
        REQUIRE(attempt.compilation.has_value());
        const auto& postconditions = attempt.compilation->postconditions;
        CHECK(postconditions.note_batches_verified == 0);
        CHECK(postconditions.notes_verified == 0);
        REQUIRE(postconditions.note_batches[0].observed_notes.size() == 1);
        CHECK(postconditions.note_batches[0].identity_verified);
        CHECK_FALSE(postconditions.note_batches[0].properties_verified);
        CHECK_FALSE(postconditions.note_batches[0].verified);
    }

    SECTION("malformed final note evidence fails closed after retaining the mutation journal") {
        ProjectNoteTransport target{
            **before, make_post_snapshot(score, mix), FinalNoteMode::Malformed};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        CHECK(attempt.status == AbletonProjectDeploymentStatus::ApplyFailed);
        CHECK(attempt.error == ErrorCode::ProtocolError);
        CHECK(attempt.compilation.has_value());
        CHECK(attempt.target_may_be_partially_modified());
        CHECK_FALSE(attempt.mutation_journal.empty());
    }
}

TEST_CASE("project apply selectively re-observes mapped DeviceParameters after aggregate mutation",
          "[ableton][project][postcondition][device-parameter]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    first.rendering.parameter_map["source.filter.cutoff"] = mapping;
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};

    CommandBuffer seed;
    const auto before = seed.target_snapshot();
    REQUIRE(before.has_value());
    REQUIRE(before->has_value());
    const auto after = make_post_snapshot(score, mix);

    SECTION("exact final identity value range and host state verify") {
        ProjectNoteTransport target{**before, after};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        CHECK(plan->preview.postconditions.device_parameters_requested == 1);
        REQUIRE(plan->preview.postconditions.device_parameters.size() == 1);
        CHECK(plan->preview.postconditions.device_parameters[0].deployment_action ==
              AbletonParameterAction::RecordedOnly);

        auto attempt = apply_project_ableton_plan(*plan, project, target);
        REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
        REQUIRE(attempt.compilation.has_value());
        const auto& postconditions = attempt.compilation->postconditions;
        CHECK(postconditions.device_parameters_requested == 1);
        CHECK(postconditions.device_parameters_verified == 1);
        REQUIRE(postconditions.device_parameters.size() == 1);
        const auto& evidence = postconditions.device_parameters[0];
        CHECK(evidence.origin == AbletonParameterOrigin::Timbre);
        CHECK(evidence.part_id == score.parts[0].id);
        CHECK(evidence.track_index == 0);
        CHECK(evidence.source_path == "source.filter.cutoff");
        CHECK(evidence.device_path == "song/tracks/0/devices/0");
        CHECK(evidence.requested_name == "Filter Freq");
        CHECK(evidence.deployment_action == AbletonParameterAction::Set);
        CHECK(evidence.matched_name == "Filter Freq");
        CHECK(evidence.original_name == "Filter Freq");
        CHECK(evidence.observed_value == evidence.requested_value);
        CHECK(evidence.observed_minimum == 0.0f);
        CHECK(evidence.observed_maximum == 1.0f);
        CHECK(evidence.observed_is_quantized == false);
        CHECK(evidence.observed_is_enabled == true);
        CHECK(evidence.observed_state == 0);
        CHECK(evidence.observed_automation_state == 0);
        CHECK(evidence.identity_verified);
        CHECK(evidence.value_verified);
        CHECK(evidence.range_verified == true);
        CHECK(evidence.enabled_verified);
        CHECK(evidence.state_verified);
        CHECK(evidence.automation_verified);
        CHECK(evidence.verified);
        CHECK(target.parameter_queries() == 1);
        REQUIRE(target.events.size() >= 2);
        CHECK(target.events[target.events.size() - 2] == "snapshot_after");
        CHECK(target.events.back() == "sunny_get_device_parameter");
        CHECK(std::none_of(attempt.mutation_journal.begin(),
                           attempt.mutation_journal.end(),
                           [](const auto& entry) {
                               return entry.request.property_or_method ==
                                      "sunny_get_device_parameter";
                           }));
    }

    SECTION("well-formed value and range divergence remain completed but incomplete") {
        for (const auto mode :
             {FinalParameterMode::Divergent, FinalParameterMode::RangeDivergent}) {
            ProjectNoteTransport target{**before, after, FinalNoteMode::Exact, mode};
            auto plan = plan_project_to_ableton(project, target);
            REQUIRE(plan.has_value());
            auto attempt = apply_project_ableton_plan(*plan, project, target);
            REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
            REQUIRE(attempt.compilation.has_value());
            const auto& postconditions = attempt.compilation->postconditions;
            CHECK(postconditions.device_parameters_verified == 0);
            REQUIRE(postconditions.device_parameters.size() == 1);
            const auto& evidence = postconditions.device_parameters[0];
            CHECK_FALSE(evidence.verified);
            if (mode == FinalParameterMode::Divergent) {
                CHECK_FALSE(evidence.value_verified);
                CHECK(evidence.range_verified == true);
            } else {
                CHECK(evidence.value_verified);
                CHECK(evidence.range_verified == false);
            }
            CHECK_FALSE(postconditions.warnings.empty());
        }
    }

    SECTION("display-value mappings retain host bounds without claiming range equivalence") {
        auto& display = first.rendering.parameter_map.at("source.filter.cutoff");
        display.value_property = DeviceParameterValueProperty::DisplayValue;
        display.range_min = 20.0f;
        display.range_max = 20000.0f;
        ProjectNoteTransport target{**before, after};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
        REQUIRE(attempt.compilation.has_value());
        const auto& evidence = attempt.compilation->postconditions.device_parameters.at(0);
        CHECK(evidence.value_property == DeviceParameterValueProperty::DisplayValue);
        CHECK(evidence.range_min == 20.0f);
        CHECK(evidence.range_max == 20000.0f);
        CHECK(evidence.observed_minimum == 0.0f);
        CHECK(evidence.observed_maximum == 1.0f);
        CHECK_FALSE(evidence.range_verified.has_value());
        CHECK(evidence.verified);
    }

    SECTION("disabled and automated final state remain observable divergence") {
        for (const auto mode : {FinalParameterMode::Disabled, FinalParameterMode::Automated}) {
            ProjectNoteTransport target{**before, after, FinalNoteMode::Exact, mode};
            auto plan = plan_project_to_ableton(project, target);
            REQUIRE(plan.has_value());
            auto attempt = apply_project_ableton_plan(*plan, project, target);
            REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
            REQUIRE(attempt.compilation.has_value());
            const auto& evidence = attempt.compilation->postconditions.device_parameters.at(0);
            CHECK_FALSE(evidence.verified);
            if (mode == FinalParameterMode::Disabled)
                CHECK_FALSE(evidence.enabled_verified);
            else
                CHECK_FALSE(evidence.automation_verified);
        }
    }

    SECTION("missing or extended final parameter evidence fails closed with journal retained") {
        for (const auto mode : {FinalParameterMode::Malformed, FinalParameterMode::Extended}) {
            ProjectNoteTransport target{**before, after, FinalNoteMode::Exact, mode};
            auto plan = plan_project_to_ableton(project, target);
            REQUIRE(plan.has_value());
            auto attempt = apply_project_ableton_plan(*plan, project, target);
            CHECK(attempt.status == AbletonProjectDeploymentStatus::ApplyFailed);
            CHECK(attempt.error == ErrorCode::ProtocolError);
            CHECK(attempt.compilation.has_value());
            CHECK(attempt.target_may_be_partially_modified());
            CHECK_FALSE(attempt.mutation_journal.empty());
        }
    }

    SECTION("failed final parameter query is distinguished from malformed evidence") {
        ProjectNoteTransport target{
            **before, after, FinalNoteMode::Exact, FinalParameterMode::SendFailure};
        auto plan = plan_project_to_ableton(project, target);
        REQUIRE(plan.has_value());
        auto attempt = apply_project_ableton_plan(*plan, project, target);
        CHECK(attempt.status == AbletonProjectDeploymentStatus::ApplyFailed);
        CHECK(attempt.error == ErrorCode::SendFailed);
        CHECK(attempt.compilation.has_value());
        CHECK(attempt.target_may_be_partially_modified());
    }
}

TEST_CASE("project final DeviceParameter obligations retain Mix effect identity and target path",
          "[ableton][project][postcondition][device-parameter][mix]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    MixCompressor compressor;
    compressor.threshold = -20.0f;
    MixDeviceParameter mapping;
    mapping.parameter_name = "Threshold";
    mapping.source_min = -60.0f;
    mapping.source_max = 0.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    constexpr std::uint64_t effect_id = 8101;
    mix.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{effect_id},
        compressor,
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", mapping}});
    const ProjectView project{score, profiles, mix};

    CommandBuffer seed;
    const auto before = seed.target_snapshot();
    REQUIRE(before.has_value());
    REQUIRE(before->has_value());
    auto after = make_post_snapshot(score, mix);
    after.song_state["tracks"][0]["devices"].push_back({{"name", "Compressor"},
                                                        {"class_display_name", "Compressor"},
                                                        {"class_name", "Compressor"},
                                                        {"type", 2},
                                                        {"is_active", true},
                                                        {"can_have_chains", false},
                                                        {"latency_in_samples", 256},
                                                        {"latency_in_ms", 5.8}});

    ProjectNoteTransport target{**before, std::move(after)};
    auto plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    auto attempt = apply_project_ableton_plan(*plan, project, target);
    REQUIRE(attempt.status == AbletonProjectDeploymentStatus::Completed);
    REQUIRE(attempt.compilation.has_value());
    CHECK(attempt.compilation->mix.parameters_mapped == 1);
    CHECK(attempt.compilation->mix.parameters_verified == 1);
    const auto& postconditions = attempt.compilation->postconditions;
    CHECK(postconditions.device_parameters_requested == 1);
    CHECK(postconditions.device_parameters_verified == 1);
    REQUIRE(postconditions.device_parameters.size() == 1);
    const auto& evidence = postconditions.device_parameters[0];
    CHECK(evidence.origin == AbletonParameterOrigin::Mix);
    CHECK(evidence.part_id == score.parts[0].id);
    CHECK(evidence.track_index == 0);
    CHECK(evidence.effect_id == MixEffectId{effect_id});
    CHECK(evidence.source_path == "threshold");
    CHECK(evidence.device_path == "song/tracks/0/devices/1");
    CHECK(evidence.requested_name == "Threshold");
    CHECK(evidence.deployment_action == AbletonParameterAction::Set);
    CHECK(evidence.verified);
    CHECK(target.parameter_queries() == 1);
}

TEST_CASE("project apply rejects changed project and target before mutation",
          "[ableton][project][plan][precondition]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    ProjectView project{score, profiles, mix};
    CommandBuffer target;
    auto plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());

    score.metadata.title = "Changed after planning";
    auto attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::ProjectChanged);
    CHECK(attempt.error == ErrorCode::ProjectPlanProjectChanged);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().empty());

    score.metadata.title = "Project";
    plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    score.tuning.name = "changed after planning";
    score.tuning.cents_from_reference[60] = -901.25;
    attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::ProjectChanged);
    CHECK(attempt.error == ErrorCode::ProjectPlanProjectChanged);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().empty());

    score.tuning = ScoreTuning{};
    plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    auto snapshot = target.target_snapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->has_value());
    snapshot->value().song_state["tempo"] = 121.0;
    target.set_target_snapshot(std::move(snapshot->value()));
    attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::TargetChanged);
    CHECK(attempt.error == ErrorCode::ProjectPlanTargetChanged);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().empty());

    plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    snapshot = target.target_snapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->has_value());
    snapshot->value().song_state["tuning_system"]["name"] = "Changed Tuning";
    target.set_target_snapshot(std::move(snapshot->value()));
    attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::TargetChanged);
    CHECK(attempt.error == ErrorCode::ProjectPlanTargetChanged);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().empty());
}

TEST_CASE("project apply rejects changed output-routing provenance before mutation",
          "[ableton][project][plan][routing][precondition]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(score.parts[0].id, project_master_route_binding());
    bindings.part_tracks.emplace(score.parts[1].id, project_master_route_binding());
    CommandBuffer target;

    auto plan = plan_project_to_ableton(project, bindings, target);
    REQUIRE(plan.has_value());
    plan->output_routing_bindings.part_tracks.at(score.parts[0].id).mapping_provenance =
        "changed after planning";

    const auto attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::ProjectChanged);
    CHECK(attempt.error == ErrorCode::ProjectPlanProjectChanged);
    CHECK(attempt.mutation_journal.empty());
    CHECK(target.entries().empty());
}

TEST_CASE("project command guard declines a divergent plan before the wrong mutation",
          "[ableton][project][plan][guard]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    CommandBuffer target;
    auto plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    REQUIRE_FALSE(plan->mutations.empty());
    plan->mutations.front().request.property_or_method = "tampered";

    const auto attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::PlanDiverged);
    CHECK(attempt.error == ErrorCode::ProjectPlanDiverged);
    REQUIRE(attempt.mutation_journal.size() == 1);
    CHECK(attempt.mutation_journal.front().outcome == AbletonMutationOutcome::DeclinedBeforeSend);
    CHECK_FALSE(attempt.target_may_be_partially_modified());
    CHECK(target.entries().empty());
}

TEST_CASE("project apply retains an indeterminate journal after partial failure",
          "[ableton][project][plan][failure]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});
    const ProjectView project{score, profiles, mix};
    ProjectFailureTransport target{4};
    auto plan = plan_project_to_ableton(project, target);
    REQUIRE(plan.has_value());
    CHECK(target.attempts() == 0);

    const auto attempt = apply_project_ableton_plan(*plan, project, target);
    CHECK(attempt.status == AbletonProjectDeploymentStatus::ApplyFailed);
    CHECK(attempt.error == ErrorCode::SendFailed);
    REQUIRE(attempt.mutation_journal.size() == 5);
    CHECK(attempt.mutation_journal.back().outcome == AbletonMutationOutcome::Indeterminate);
    CHECK(attempt.target_may_be_partially_modified());
    CHECK(attempt.target_before.has_value());
    CHECK(attempt.target_after.has_value());
}

TEST_CASE("journal distinguishes a local pre-send decline from an uncertain sent mutation",
          "[ableton][project][plan][journal]") {
    TcpTransport disconnected;
    JournaledLomTransport journaled{disconnected};

    const auto response =
        journaled.send(LomProtocol::set_property(LomPaths::song(), "tempo", 121.0));

    CHECK_FALSE(response.success);
    CHECK(response.delivery == LomDeliveryState::NotSent);
    REQUIRE(journaled.journal().size() == 1);
    CHECK(journaled.journal().front().outcome == AbletonMutationOutcome::DeclinedBeforeSend);
    CHECK_FALSE(journaled.journal().front().target_may_have_mutated());
}

TEST_CASE("guarded deployment validates but does not journal read-only observations",
          "[ableton][project][plan][journal][observation]") {
    CommandBuffer target;
    JournaledLomTransport journaled{target, std::nullopt, {}, true};
    const nlohmann::json query = {{"return",
                                   nlohmann::json::array({"note_id",
                                                          "pitch",
                                                          "start_time",
                                                          "duration",
                                                          "velocity",
                                                          "mute",
                                                          "probability",
                                                          "velocity_deviation",
                                                          "release_velocity"})}};

    const auto response = journaled.send(
        LomProtocol::call_method(LomPaths::clip(0, 0), "get_all_notes_extended", {query}));

    CHECK(response.success);
    CHECK(journaled.journal().empty());
    CHECK_FALSE(journaled.plan_diverged());
    CHECK(journaled.plan_consumed());
    REQUIRE(target.entries().size() == 1);
    CHECK(target.entries()[0].request.property_or_method == "get_all_notes_extended");

    const auto parameter_response =
        journaled.send(LomProtocol::call_method(LomPaths::track(0).child("devices").child(0),
                                                "sunny_get_device_parameter",
                                                {std::string{"Dry/Wet"}, std::string{"value"}}));
    CHECK(parameter_response.success);
    CHECK(journaled.journal().empty());
    CHECK_FALSE(journaled.plan_diverged());
    REQUIRE(target.entries().size() == 2);
    CHECK(target.entries()[1].request.property_or_method == "sunny_get_device_parameter");
}
