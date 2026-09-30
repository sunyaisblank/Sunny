/**
 * @file ableton_mix_test.cpp
 * @brief Unit tests for Mix IR → Ableton compiler
 *
 *
 * Coverage: Group/return track creation, effect insertion,
 *           channel configuration, routing, master bus, automation
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <map>
#include <set>
#include <sunny/infrastructure/ableton/deployment.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/formats/ableton_mix.hpp>

using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;
using namespace sunny::core;

namespace {

class MixReadbackTransport final : public LomTransport {
  public:
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "sunny_set_output_routing_type") {
            const auto& requested = std::get<nlohmann::json>(request.args.at(0));
            const nlohmann::json selected_channel = {{"display_name", "1/2"},
                                                     {"identifier", "stereo_1_2"}};
            const nlohmann::json types = {
                {"available_output_routing_types", nlohmann::json::array({requested})}};
            const nlohmann::json channels = {
                {"available_output_routing_channels", nlohmann::json::array({selected_channel})}};
            if (malformed_route_evidence)
                return {true, nlohmann::json{{"requested_type", requested}}, std::nullopt};
            return {true,
                    nlohmann::json{{"requested_type", requested},
                                   {"available_output_routing_types_before", types},
                                   {"output_routing_type", requested},
                                   {"output_routing_channel", selected_channel},
                                   {"available_output_routing_types", types},
                                   {"available_output_routing_channels", channels}},
                    std::nullopt};
        }
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "sunny_set_output_routing_channel") {
            if (fail_route_channel)
                return {false,
                        std::nullopt,
                        std::string{"channel mutation failed"},
                        LomDeliveryState::ResponseReceived};
            const auto& requested_type = std::get<nlohmann::json>(request.args.at(0));
            const auto& requested_channel = std::get<nlohmann::json>(request.args.at(1));
            const nlohmann::json types = {
                {"available_output_routing_types", nlohmann::json::array({requested_type})}};
            auto available_channel = requested_channel;
            if (route_channel_unavailable)
                available_channel =
                    nlohmann::json{{"display_name", "Different"}, {"identifier", "different"}};
            const nlohmann::json channels = {
                {"available_output_routing_channels", nlohmann::json::array({available_channel})}};
            return {true,
                    nlohmann::json{{"requested_type", requested_type},
                                   {"requested_channel", requested_channel},
                                   {"output_routing_type_before", requested_type},
                                   {"available_output_routing_types_before", types},
                                   {"available_output_routing_channels_before", channels},
                                   {"output_routing_type", requested_type},
                                   {"output_routing_channel", requested_channel},
                                   {"available_output_routing_types", types},
                                   {"available_output_routing_channels", channels}},
                    std::nullopt};
        }
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "insert_device") {
            if (malformed_device_evidence)
                return {true, nlohmann::json{{"requested_name", "EQ Eight"}}, std::nullopt};
            const auto& requested_name = std::get<std::string>(request.args.at(0));
            const auto requested_index = std::get<int>(request.args.at(1));
            return {true,
                    nlohmann::json{{"requested_name", requested_name},
                                   {"requested_index", requested_index},
                                   {"before_count", requested_index},
                                   {"after_count", requested_index + 1},
                                   {"device_index", requested_index},
                                   {"name", requested_name},
                                   {"class_display_name",
                                    diverge_device_identity ? "Different Device" : requested_name},
                                   {"class_name", requested_name},
                                   {"type", device_type},
                                   {"is_active", device_active},
                                   {"can_have_chains", device_can_have_chains},
                                   {"latency_in_samples", latency_in_samples},
                                   {"latency_in_ms", latency_in_ms},
                                   {"track_has_audio_output", track_has_audio_output},
                                   {"track_has_midi_output", track_has_midi_output}},
                    std::nullopt};
        }
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "sunny_set_device_parameter") {
            if (omit_parameter_evidence) return {true, std::nullopt, std::nullopt};
            const auto& parameter_name = std::get<std::string>(request.args.at(0));
            const auto requested = std::get<double>(request.args.at(1));
            const auto& property = std::get<std::string>(request.args.at(2));
            const auto minimum = std::get<double>(request.args.at(3));
            const auto maximum = std::get<double>(request.args.at(4));
            return {true,
                    nlohmann::json{{"matched_name", parameter_name},
                                   {"original_name", parameter_name},
                                   {"property", property},
                                   {"requested", requested},
                                   {"observed", requested + parameter_observed_offset},
                                   {"minimum", minimum},
                                   {"maximum", maximum},
                                   {"is_quantized", false},
                                   {"default_value", minimum},
                                   {"value_items", nullptr},
                                   {"is_enabled", true},
                                   {"state", parameter_state},
                                   {"automation_state", automation_state}},
                    std::nullopt};
        }
        if (request.type != LomRequestType::SetProperty) return {true, std::nullopt, std::nullopt};
        if (omit_property_evidence) return {true, std::nullopt, std::nullopt};
        const auto requested =
            std::visit([](const auto& value) { return nlohmann::json(value); }, request.args.at(0));
        return {true,
                nlohmann::json{{"property", request.property_or_method},
                               {"requested", requested},
                               {"observed", requested}},
                std::nullopt};
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {true, std::nullopt, std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return std::optional<AbletonTargetProfile>{modeled_target_profile({12, 3, 0, "12.3.0"})};
    }

    [[nodiscard]] Result<std::optional<std::uint32_t>> return_track_count() override {
        return_track_count_reads++;
        return existing_return_track_count;
    }

    [[nodiscard]] Result<std::optional<std::uint32_t>> device_count(const LomPath&) override {
        return std::optional<std::uint32_t>{existing_device_count};
    }

    std::vector<LomRequest> requests;
    std::optional<std::uint32_t> existing_return_track_count = 0;
    std::uint32_t return_track_count_reads = 0;
    bool omit_property_evidence = false;
    bool omit_parameter_evidence = false;
    bool malformed_device_evidence = false;
    bool malformed_route_evidence = false;
    bool fail_route_channel = false;
    bool route_channel_unavailable = false;
    bool diverge_device_identity = false;
    bool device_active = true;
    bool device_can_have_chains = false;
    bool track_has_audio_output = true;
    bool track_has_midi_output = false;
    std::uint32_t latency_in_samples = 128;
    double latency_in_ms = 2.9;
    int device_type = 2;
    std::uint32_t existing_device_count = 0;
    double parameter_observed_offset = 0.0;
    int parameter_state = 0;
    int automation_state = 0;
};

MixDeviceParameter compressor_threshold_mapping() {
    MixDeviceParameter mapping;
    mapping.parameter_name = "Threshold";
    mapping.source_min = -60.0f;
    mapping.source_max = 0.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    return mapping;
}

/// Create a minimal MixGraph with two channels
MixGraph make_test_graph() {
    MixGraph g;
    g.id = MixGraphId{1};

    ChannelStrip ch1;
    ch1.id = ChannelStripId{1};
    ch1.part_id = PartId{1};
    ch1.fader.level_db = -3.0f;
    ch1.spatial.pan = -0.5f;
    g.channels.push_back(ch1);

    ChannelStrip ch2;
    ch2.id = ChannelStripId{2};
    ch2.part_id = PartId{2};
    ch2.fader.level_db = -6.0f;
    ch2.spatial.pan = 0.5f;
    g.channels.push_back(ch2);

    return g;
}

/// Create a MixGraph with group bus, aux bus, and effects
MixGraph make_full_graph() {
    auto g = make_test_graph();

    // Add a group bus
    GroupBus grp;
    grp.id = GroupBusId{1};
    grp.name = "Strings";
    grp.member_channels = {ChannelStripId{1}, ChannelStripId{2}};
    grp.fader.level_db = 0.0f;

    // Add EQ to group
    MixEffect eq;
    eq.id = MixEffectId{1};
    eq.parameters = MixEQ{};
    grp.insert_chain.effects.push_back(eq);
    g.group_buses.push_back(grp);
    g.channels[0].group_assignment = grp.id;
    g.channels[1].group_assignment = grp.id;

    // Add an aux bus (reverb return)
    AuxBus aux;
    aux.id = AuxBusId{1};
    aux.name = "Hall Reverb";
    aux.return_level = -10.0f;
    g.aux_buses.push_back(aux);

    // Add channel sends
    AuxSendLevel send;
    send.aux_bus_id = AuxBusId{1};
    send.level_db = -12.0f;
    send.enabled = true;
    g.channels[0].sends.push_back(send);
    g.channels[1].sends.push_back(send);

    // Add EQ to channel 1
    MixEffect ch_eq;
    ch_eq.id = MixEffectId{2};
    ch_eq.parameters = MixEQ{};
    g.channels[0].insert_chain.effects.push_back(ch_eq);

    // Add compressor to master
    MixEffect master_comp;
    master_comp.id = MixEffectId{3};
    master_comp.parameters = MixCompressor{};
    g.master_bus.insert_chain.effects.push_back(master_comp);

    return g;
}

AbletonOutputRouteBinding master_route_binding() {
    return {
        {"Master", "master"}, {"1/2", "stereo_1_2"}, "operator admission for fixture Live target"};
}

} // namespace

// =============================================================================
// Basic compilation
// =============================================================================

TEST_CASE("minimal graph configures channels", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->channels_requested == 2);
    CHECK(r->channels_configured == 2);
    CHECK(r->output_routes_requested == 2);
    CHECK(r->output_routes_written == 0);
    CHECK(r->output_routes_verified == 0);
    REQUIRE(r->output_route_residuals.size() == 2);
    CHECK(r->output_route_residuals[0].find("master_bus") != std::string::npos);
}

TEST_CASE("explicit output bindings emit two ordered mutations per materialisable route",
          "[ableton][mix][routing]") {
    auto graph = make_test_graph();
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(PartId{1}, master_route_binding());
    bindings.part_tracks.emplace(PartId{2}, master_route_binding());
    CommandBuffer transport;

    auto result = compile_mix_to_ableton(graph, 0, transport, bindings);
    REQUIRE(result.has_value());
    CHECK(result->output_routes_requested == 2);
    CHECK(result->output_routes_written == 2);
    CHECK(result->output_routes_verified == 0);
    REQUIRE(result->output_route_deployments.size() == 2);
    REQUIRE(result->output_route_residuals.size() == 2);
    for (const auto& deployment : result->output_route_deployments) {
        REQUIRE(deployment.binding.has_value());
        CHECK(deployment.action == AbletonOutputRouteAction::RecordedOnly);
        CHECK_FALSE(deployment.verified);
    }

    std::vector<const CommandBuffer::Entry*> route_commands;
    for (const auto& entry : transport.entries()) {
        if (entry.request.property_or_method.starts_with("sunny_set_output_routing_"))
            route_commands.push_back(&entry);
    }
    REQUIRE(route_commands.size() == 4);
    CHECK(route_commands[0]->request.property_or_method == "sunny_set_output_routing_type");
    CHECK(route_commands[1]->request.property_or_method == "sunny_set_output_routing_channel");
    CHECK(route_commands[2]->request.property_or_method == "sunny_set_output_routing_type");
    CHECK(route_commands[3]->request.property_or_method == "sunny_set_output_routing_channel");
}

TEST_CASE("live output routing requires exact advertised membership and final readback",
          "[ableton][mix][routing][readback]") {
    auto graph = make_test_graph();
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(PartId{1}, master_route_binding());
    bindings.part_tracks.emplace(PartId{2}, master_route_binding());
    MixReadbackTransport transport;

    auto result = compile_mix_to_ableton(graph, 0, transport, bindings);
    REQUIRE(result.has_value());
    CHECK(result->output_routes_requested == 2);
    CHECK(result->output_routes_written == 2);
    CHECK(result->output_routes_verified == 2);
    CHECK(result->output_route_residuals.empty());
    for (const auto& deployment : result->output_route_deployments) {
        CHECK(deployment.action == AbletonOutputRouteAction::Set);
        CHECK(deployment.requested_type_available_verified);
        CHECK(deployment.type_stage_verified);
        CHECK(deployment.requested_channel_available_verified);
        CHECK(deployment.channel_stage_verified);
        CHECK(deployment.final_membership_verified);
        CHECK(deployment.verified);
        REQUIRE(deployment.observed_type.has_value());
        CHECK(*deployment.observed_type == deployment.binding->type);
        REQUIRE(deployment.observed_channel.has_value());
        CHECK(*deployment.observed_channel == deployment.binding->channel);
    }
}

TEST_CASE("output routing rejects malformed evidence, absent membership, and invalid bindings",
          "[ableton][mix][routing][trust-boundary]") {
    auto graph = make_test_graph();
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(PartId{1}, master_route_binding());

    MixReadbackTransport malformed;
    malformed.malformed_route_evidence = true;
    auto result = compile_mix_to_ableton(graph, 0, malformed, bindings);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    MixReadbackTransport unavailable;
    unavailable.route_channel_unavailable = true;
    result = compile_mix_to_ableton(graph, 0, unavailable, bindings);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    auto unknown = bindings;
    unknown.part_tracks.emplace(PartId{999}, master_route_binding());
    CommandBuffer recorder;
    result = compile_mix_to_ableton(graph, 0, recorder, unknown);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::MixInvalidParameter);
    CHECK(recorder.entries().empty());

    auto missing_provenance = bindings;
    missing_provenance.part_tracks.at(PartId{1}).mapping_provenance.clear();
    result = compile_mix_to_ableton(graph, 0, recorder, missing_provenance);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::MixInvalidParameter);
    CHECK(recorder.entries().empty());

    GroupBus group;
    group.id = GroupBusId{1};
    group.member_channels = {graph.channels[0].id};
    graph.group_buses.push_back(group);
    graph.channels[0].group_assignment = group.id;
    result = compile_mix_to_ableton(graph, 0, recorder, bindings);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::MixInvalidParameter);
    CHECK(recorder.entries().empty());
}

TEST_CASE("routing stage failure is journalled as a possible partial target mutation",
          "[ableton][mix][routing][journal]") {
    auto graph = make_test_graph();
    graph.channels.resize(1);
    AbletonOutputRoutingBindings bindings;
    bindings.part_tracks.emplace(PartId{1}, master_route_binding());
    MixReadbackTransport underlying;
    underlying.fail_route_channel = true;
    JournaledLomTransport journalled{underlying};

    auto result = compile_mix_to_ableton(graph, 0, journalled, bindings);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::SendFailed);
    REQUIRE(journalled.journal().size() == 2);
    CHECK(journalled.journal()[0].request.property_or_method == "sunny_set_output_routing_type");
    CHECK(journalled.journal()[0].outcome == AbletonMutationOutcome::Acknowledged);
    CHECK(journalled.journal()[1].request.property_or_method == "sunny_set_output_routing_channel");
    CHECK(journalled.journal()[1].outcome == AbletonMutationOutcome::Indeterminate);
    CHECK(journalled.journal()[1].target_may_have_mutated());
}

TEST_CASE("live mixer property writes require and expose readback evidence",
          "[ableton][mix][target-profile]") {
    auto graph = make_test_graph();
    MixReadbackTransport transport;

    auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->property_writes == 14);
    CHECK(result->property_writes_verified == 14);
    REQUIRE(result->property_deployments.size() == 14);
    for (const auto& deployment : result->property_deployments) {
        CHECK(deployment.verified);
        REQUIRE(deployment.observed.has_value());
        CHECK(*deployment.observed == deployment.requested);
    }
}

TEST_CASE("live mixer property writes reject empty acknowledgements",
          "[ableton][mix][target-profile]") {
    auto graph = make_test_graph();
    MixReadbackTransport transport;
    transport.omit_property_evidence = true;

    auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

TEST_CASE("explicit Part map rejects missing and aliased track targets", "[ableton][mix]") {
    const auto graph = make_test_graph();
    CommandBuffer transport;

    AbletonPartTrackMap missing{{PartId{1}, 0}};
    auto result = compile_mix_to_ableton(graph, missing, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::MixInvalidParameter);
    CHECK(transport.entries().empty());

    AbletonPartTrackMap aliased{{PartId{1}, 0}, {PartId{2}, 0}};
    result = compile_mix_to_ableton(graph, aliased, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::MixInvalidParameter);
    CHECK(transport.entries().empty());
}

TEST_CASE("standalone Mix track ranges must fit the signed LOM index domain",
          "[ableton][mix][target-address]") {
    const auto graph = make_test_graph();
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, std::numeric_limits<int>::max(), transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetAddressUnrepresentable);
    CHECK(transport.entries().empty());
}

TEST_CASE("invalid aux references fail validation before Ableton mutation",
          "[ableton][mix][target-address]") {
    auto graph = make_test_graph();
    graph.channels.front().sends.push_back(AuxSendLevel{AuxBusId{999}, -12.0f, false, true});
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvariantViolation);
    CHECK(transport.entries().empty());
}

TEST_CASE("channel volume is set from fader", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int volume_count = 0;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "display_value" &&
            e->request.path.to_string().ends_with("mixer_device/volume"))
            volume_count++;
    }
    // 2 channel volumes + master volume
    CHECK(volume_count >= 2);
}

TEST_CASE("Mix compiler applies statically resolved relative fader levels",
          "[ableton][mix][relative-level]") {
    auto graph = make_test_graph();
    graph.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{
            LevelReferenceType::Channel, -14.0f, graph.channels[1].id, "3 dB below channel 2", {}},
        -3.0f};
    graph.master_bus.fader.relative_level = RelativeLevel{
        LevelReference{
            LevelReferenceType::Channel, -14.0f, graph.channels[0].id, "1 dB above channel 1", {}},
        1.0f};
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->fader_level_resolution.complete());
    CHECK(result->fader_level_resolution.relative_levels_total == 2);
    CHECK(result->fader_level_resolution.relative_levels_resolved == 2);
    CHECK(std::none_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("relative fader intent") != std::string::npos;
    }));

    std::optional<double> channel_level;
    std::optional<double> master_level;
    for (const auto& entry : transport.entries()) {
        if (entry.request.type != LomRequestType::SetProperty ||
            entry.request.property_or_method != "display_value")
            continue;
        if (entry.request.path.to_string() == "song/tracks/0/mixer_device/volume")
            channel_level = std::get<double>(entry.request.args.at(0));
        if (entry.request.path.to_string() == "song/master_track/mixer_device/volume")
            master_level = std::get<double>(entry.request.args.at(0));
    }
    REQUIRE(channel_level.has_value());
    REQUIRE(master_level.has_value());
    CHECK(*channel_level == Catch::Approx(-9.0));
    CHECK(*master_level == Catch::Approx(-8.0));
}

TEST_CASE("Mix compiler preserves loudness-relative faders as explicit measured residuals",
          "[ableton][mix][relative-level]") {
    auto graph = make_test_graph();
    graph.channels[0].fader.relative_level =
        RelativeLevel{LevelReference{LevelReferenceType::MasterTarget, -16.0f, {}, {}, {}}, 0.0f};
    graph.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[0].id, {}, {}}, -2.0f};
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK_FALSE(result->fader_level_resolution.complete());
    CHECK(result->fader_level_resolution.relative_levels_unresolved == 2);
    CHECK(std::count_if(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
              return warning.find("explicit fallback level was applied") != std::string::npos;
          }) == 2);

    std::optional<double> first;
    std::optional<double> second;
    for (const auto& entry : transport.entries()) {
        if (entry.request.type != LomRequestType::SetProperty ||
            entry.request.property_or_method != "display_value")
            continue;
        if (entry.request.path.to_string() == "song/tracks/0/mixer_device/volume")
            first = std::get<double>(entry.request.args.at(0));
        if (entry.request.path.to_string() == "song/tracks/1/mixer_device/volume")
            second = std::get<double>(entry.request.args.at(0));
    }
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == Catch::Approx(-3.0));
    CHECK(*second == Catch::Approx(-6.0));
}

TEST_CASE("Mix compiler rejects cyclic relative faders before target mutation",
          "[ableton][mix][relative-level][preflight]") {
    auto graph = make_test_graph();
    graph.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[1].id, {}, {}}, 0.0f};
    graph.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[0].id, {}, {}}, 0.0f};
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvariantViolation);
    CHECK(transport.entries().empty());
}

TEST_CASE("channel pan is set from spatial", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int pan_count = 0;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "value" &&
            e->request.path.to_string().ends_with("mixer_device/panning"))
            pan_count++;
    }
    CHECK(pan_count == 3);
}

// =============================================================================
// Group and return tracks
// =============================================================================

TEST_CASE("unsupported group tracks are reported without fictional calls", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->group_tracks_requested == 1);
    CHECK(r->group_tracks_created == 0);
    CHECK(r->output_routes_requested == 4);
    CHECK(r->output_routes_written == 0);
    CHECK(r->output_routes_verified == 0);
    CHECK(std::find(r->output_route_residuals.begin(),
                    r->output_route_residuals.end(),
                    "group_buses/1 -> master_bus") != r->output_route_residuals.end());
    CHECK(std::find(r->output_route_residuals.begin(),
                    r->output_route_residuals.end(),
                    "aux_buses/1 -> master_bus") != r->output_route_residuals.end());
    REQUIRE_FALSE(r->warnings.empty());
    CHECK(r->warnings[0].find("Group bus") != std::string::npos);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "create_group_track") found = true;
    }
    CHECK_FALSE(found);
}

TEST_CASE("enabled GroupBus sends remain requested but unconfigured", "[ableton][mix]") {
    auto graph = make_test_graph();
    AuxBus aux;
    aux.id = AuxBusId{1};
    aux.name = "Verb";
    graph.aux_buses.push_back(aux);
    GroupBus group;
    group.id = GroupBusId{1};
    group.name = "Group";
    group.sends.push_back({aux.id, -9.0f, false, true});
    graph.group_buses.push_back(group);
    CommandBuffer buffer;

    const auto result = compile_mix_to_ableton(graph, 0, buffer);

    REQUIRE(result.has_value());
    CHECK(result->sends_requested == 1);
    CHECK(result->sends_configured == 0);
    CHECK(result->send_levels_requested == 1);
    CHECK(result->send_levels_configured == 0);
    CHECK(result->send_modes_requested == 1);
    CHECK(result->send_modes_configured == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("enabled send(s) on Group bus") != std::string::npos;
    }));
    CHECK(buffer.find_by_path_prefix("song/tracks/0/mixer_device/sends").empty());
}

TEST_CASE("return tracks created for aux buses", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->return_tracks_requested == 1);
    CHECK(r->return_tracks_created == 1);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "create_return_track") found = true;
    }
    CHECK(found);
}

TEST_CASE("generated aux returns receive an explicit audible Stereo Pan gate",
          "[ableton][mix][return-gate]") {
    auto graph = make_full_graph();
    graph.aux_buses[0].return_spatial.pan = 0.25f;
    CommandBuffer buffer;

    const auto result = compile_mix_to_ableton(graph, 0, buffer);

    REQUIRE(result.has_value());
    REQUIRE(result->return_track_deployments.size() == 1);
    const auto& deployment = result->return_track_deployments.front();
    CHECK(deployment.aux_bus_id == graph.aux_buses[0].id);
    CHECK(deployment.track_index == 0);
    CHECK(deployment.requested_name == "Hall Reverb");
    CHECK_FALSE(deployment.requested_mute);
    CHECK_FALSE(deployment.requested_solo);
    CHECK(deployment.requested_track_activator == 1.0);
    CHECK(deployment.requested_crossfade_assign == 1);
    CHECK(deployment.requested_panning_mode == 0);
    CHECK(deployment.requested_pan == 0.25f);

    const std::set<std::pair<std::string, std::string>> required_writes{
        {"song/return_tracks/0", "mute"},
        {"song/return_tracks/0", "solo"},
        {"song/return_tracks/0/mixer_device", "crossfade_assign"},
        {"song/return_tracks/0/mixer_device", "panning_mode"},
        {"song/return_tracks/0/mixer_device/track_activator", "value"},
        {"song/return_tracks/0/mixer_device/panning", "value"},
    };
    std::set<std::pair<std::string, std::string>> observed_writes;
    for (const auto* entry : buffer.find_by_type(LomRequestType::SetProperty)) {
        const auto key =
            std::pair{entry->request.path.to_string(), entry->request.property_or_method};
        if (required_writes.contains(key)) observed_writes.insert(key);
    }
    CHECK(observed_writes == required_writes);
}

TEST_CASE("MasterBus receives an explicit enabled centered Stereo Pan projection",
          "[ableton][mix][master-gate]") {
    auto graph = make_test_graph();
    CommandBuffer buffer;

    const auto result = compile_mix_to_ableton(graph, 0, buffer);

    REQUIRE(result.has_value());
    REQUIRE(result->master_track_deployment.has_value());
    CHECK(result->master_track_deployment->requested_track_activator == 1.0);
    CHECK(result->master_track_deployment->requested_panning_mode == 0);
    CHECK(result->master_track_deployment->requested_pan == 0.0);
    const std::set<std::pair<std::string, std::string>> required_writes{
        {"song/master_track/mixer_device/track_activator", "value"},
        {"song/master_track/mixer_device", "panning_mode"},
        {"song/master_track/mixer_device/panning", "value"},
    };
    std::set<std::pair<std::string, std::string>> observed_writes;
    for (const auto* entry : buffer.find_by_type(LomRequestType::SetProperty)) {
        const auto key =
            std::pair{entry->request.path.to_string(), entry->request.property_or_method};
        if (required_writes.contains(key)) observed_writes.insert(key);
    }
    CHECK(observed_writes == required_writes);
}

TEST_CASE("aux buses decline unknown return-track state before mutation",
          "[ableton][mix][target-state]") {
    auto graph = make_full_graph();
    MixReadbackTransport transport;
    transport.existing_return_track_count = std::nullopt;

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
    CHECK(transport.return_track_count_reads == 1);
    CHECK(transport.requests.empty());
}

TEST_CASE("graphs without aux buses do not require return-track state",
          "[ableton][mix][target-state]") {
    auto graph = make_test_graph();
    MixReadbackTransport transport;
    transport.existing_return_track_count = std::nullopt;

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE(result.has_value());
    CHECK(transport.return_track_count_reads == 0);
    CHECK_FALSE(transport.requests.empty());
}

// =============================================================================
// Effect insertion
// =============================================================================

TEST_CASE("effects inserted on channels, groups, master", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    // 1 channel EQ + 1 master compressor. Group processing cannot be
    // materialised because public LOM cannot create group tracks.
    CHECK(r->effects_requested == 3);
    CHECK(r->effects_inserted == 2);
    CHECK(r->effects_verified == 0);
    CHECK(r->device_deployments.size() == 2);
}

TEST_CASE("EQ maps to EQ Eight device", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    REQUIRE(compile_mix_to_ableton(graph, 0, buf).has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found_eq = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "insert_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "EQ Eight") found_eq = true;
        }
    }
    CHECK(found_eq);
}

TEST_CASE("Compressor maps to Compressor device", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    REQUIRE(compile_mix_to_ableton(graph, 0, buf).has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found_comp = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "insert_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Compressor") found_comp = true;
        }
    }
    CHECK(found_comp);
}

TEST_CASE("spatial effects map to native Delay and Reverb devices", "[ableton][mix]") {
    auto graph = make_test_graph();
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{10}, MixDelay{}, true});
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{11}, MixReverb{}, true});
    CommandBuffer buf;

    REQUIRE(compile_mix_to_ableton(graph, 0, buf).has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found_delay = false;
    bool found_reverb = false;
    for (const auto* entry : calls) {
        if (entry->request.property_or_method != "insert_device") continue;
        const auto* argument = std::get_if<std::string>(&entry->request.args[0]);
        found_delay |= argument != nullptr && *argument == "Delay";
        found_reverb |= argument != nullptr && *argument == "Reverb";
    }
    CHECK(found_delay);
    CHECK(found_reverb);
}

TEST_CASE("mapped Mix parameters target the exact newly inserted device",
          "[ableton][mix][device-parameter][target-state]") {
    auto graph = make_test_graph();
    MixCompressor compressor;
    compressor.threshold = -20.0f;
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        compressor,
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", compressor_threshold_mapping()}});
    CommandBuffer transport;
    const auto track_path = LomPaths::track(0);
    transport.set_device_count(track_path, 2);

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->parameters_mapped == 1);
    CHECK(result->parameters_verified == 0);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK(result->parameter_deployments[0].device_path == "song/tracks/0/devices/2");
    CHECK(result->parameter_deployments[0].requested_value == Catch::Approx(2.0f / 3.0f));
    CHECK(result->parameter_deployments[0].range_min == 0.0f);
    CHECK(result->parameter_deployments[0].range_max == 1.0f);
    CHECK_FALSE(result->parameter_deployments[0].matched_name.has_value());
    CHECK_FALSE(result->parameter_deployments[0].is_quantized.has_value());
    CHECK_FALSE(result->parameter_deployments[0].is_enabled.has_value());
    CHECK(result->parameter_deployments[0].action == AbletonParameterAction::RecordedOnly);
    CHECK_FALSE(result->parameter_deployments[0].observed_value.has_value());
    REQUIRE(result->parameter_coverage.size() == 1);
    CHECK(result->parameter_coverage[0].source_parameters == 10);
    CHECK(result->parameter_coverage[0].explicitly_mapped == 1);
    CHECK(result->parameter_coverage[0].missing_paths.size() == 9);

    bool exact_parameter_write = false;
    for (const auto& entry : transport.entries()) {
        exact_parameter_write |= entry.request.path.to_string() == "song/tracks/0/devices/2" &&
                                 entry.request.property_or_method == "sunny_set_device_parameter";
    }
    CHECK(exact_parameter_write);
    const auto device_count = transport.device_count(track_path);
    REQUIRE(device_count.has_value());
    REQUIRE(device_count->has_value());
    CHECK(**device_count == 3);
}

TEST_CASE("live Mix parameter deployment requires identity range and readback evidence",
          "[ableton][mix][device-parameter][readback]") {
    auto graph = make_test_graph();
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        MixCompressor{},
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", compressor_threshold_mapping()}});
    MixReadbackTransport transport;
    transport.existing_device_count = 3;

    auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->effects_inserted == 1);
    CHECK(result->effects_verified == 1);
    REQUIRE(result->device_deployments.size() == 1);
    CHECK(result->device_deployments[0].requested_index == 3);
    CHECK(result->device_deployments[0].requested_type == 2);
    CHECK(result->device_deployments[0].observed_type == 2);
    CHECK(result->device_deployments[0].observed_latency_in_samples == 128);
    CHECK(result->device_deployments[0].observed_latency_in_ms == 2.9);
    CHECK(result->device_deployments[0].reported_latency_observed);
    CHECK_FALSE(result->device_deployments[0].render_path_latency_fully_observed);
    CHECK(result->device_deployments[0].verified);
    CHECK(result->parameters_mapped == 1);
    CHECK(result->parameters_verified == 1);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK(result->parameter_deployments[0].device_path == "song/tracks/0/devices/3");
    CHECK(result->parameter_deployments[0].range_min == 0.0f);
    CHECK(result->parameter_deployments[0].range_max == 1.0f);
    CHECK(result->parameter_deployments[0].matched_name == "Threshold");
    CHECK(result->parameter_deployments[0].original_name == "Threshold");
    REQUIRE(result->parameter_deployments[0].observed_value.has_value());
    CHECK(result->parameter_deployments[0].observed_minimum == 0.0f);
    CHECK(result->parameter_deployments[0].observed_maximum == 1.0f);
    CHECK(result->parameter_deployments[0].is_quantized == false);
    CHECK(result->parameter_deployments[0].is_enabled == true);
    CHECK(result->parameter_deployments[0].action == AbletonParameterAction::Set);
    CHECK(result->parameter_deployments[0].verified);
    CHECK(result->parameter_deployments[0].parameter_state == 0);
    CHECK(result->parameter_deployments[0].automation_state == 0);

    transport.omit_parameter_evidence = true;
    result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

TEST_CASE("live Mix effects retain wrong type activity and malformed insertion evidence",
          "[ableton][mix][device][readback]") {
    auto graph = make_test_graph();
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        MixCompressor{},
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", compressor_threshold_mapping()}});
    MixReadbackTransport transport;

    transport.device_type = 1;
    auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->effects_verified == 0);
    CHECK(result->parameters_mapped == 0);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK_FALSE(result->parameter_deployments[0].observed_value.has_value());
    CHECK(
        std::none_of(transport.requests.begin(), transport.requests.end(), [](const auto& request) {
            return request.property_or_method == "sunny_set_device_parameter";
        }));
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("not reported as an audio effect") != std::string::npos;
    }));

    transport.device_type = 2;
    transport.device_active = false;
    result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->effects_verified == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("inactive") != std::string::npos;
    }));

    transport.device_active = true;
    transport.device_can_have_chains = true;
    result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->effects_verified == 0);
    CHECK(result->parameters_mapped == 0);
    REQUIRE(result->device_deployments.size() == 1);
    CHECK(result->device_deployments[0].observed_can_have_chains == true);
    CHECK_FALSE(result->device_deployments[0].flat_device_verified);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("is a Rack") != std::string::npos;
    }));

    transport.device_can_have_chains = false;
    transport.malformed_device_evidence = true;
    result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

TEST_CASE("Mix readback exposes inactive and automated DeviceParameter state",
          "[ableton][mix][device-parameter][state]") {
    auto graph = make_test_graph();
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        MixCompressor{},
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", compressor_threshold_mapping()}});
    MixReadbackTransport transport;
    transport.parameter_state = 1;
    transport.automation_state = 2;

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE(result.has_value());
    CHECK(result->parameters_verified == 1);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK(result->parameter_deployments[0].parameter_state == 1);
    CHECK(result->parameter_deployments[0].automation_state == 2);
    CHECK(std::count_if(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
              return warning.find("inactive") != std::string::npos ||
                     warning.find("automation") != std::string::npos;
          }) == 2);
}

TEST_CASE("Mix parameter divergence is retained as evidence and warning",
          "[ableton][mix][device-parameter][readback]") {
    auto graph = make_test_graph();
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        MixCompressor{},
        true,
        std::map<std::string, MixDeviceParameter>{{"threshold", compressor_threshold_mapping()}});
    MixReadbackTransport transport;
    transport.parameter_observed_offset = 0.1;

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->parameters_mapped == 1);
    CHECK(result->parameters_verified == 0);
    REQUIRE(result->parameter_deployments[0].observed_value.has_value());
    CHECK_FALSE(result->parameter_deployments[0].verified);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("differed") != std::string::npos;
    }));
}

TEST_CASE("Mix compiler reports exact scalar and non-parameter residual coverage",
          "[ableton][mix][device-parameter][coverage]") {
    auto graph = make_test_graph();
    MixReverb reverb;
    reverb.algorithm = MixReverbAlgorithm::Convolution;
    reverb.impulse_response = "hall.wav";
    MixDeviceParameter mix_mapping;
    mix_mapping.parameter_name = "Dry/Wet";
    graph.channels[0].insert_chain.effects.emplace_back(
        MixEffectId{10},
        reverb,
        true,
        std::map<std::string, MixDeviceParameter>{{"mix", mix_mapping}});
    CommandBuffer transport;

    const auto result = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->parameter_sources_total == 10);
    CHECK(result->parameter_sources_explicitly_mapped == 1);
    CHECK(result->parameter_sources_unmapped == 9);
    REQUIRE(result->parameter_coverage.size() == 1);
    CHECK(result->parameter_coverage[0].non_parameter_residual_paths ==
          std::vector<std::string>{"impulse_response"});
}

TEST_CASE("pre-12.3 target configures Mix state but skips native effects",
          "[ableton][mix][target-profile]") {
    auto graph = make_full_graph();
    CommandBuffer buf;
    buf.set_target_profile(modeled_target_profile({11, 3, 42, "11.3.42"}));

    auto result = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(result.has_value());
    CHECK(result->channels_requested == graph.channels.size());
    CHECK(result->channels_configured == graph.channels.size());
    CHECK(result->effects_requested == 3);
    CHECK(result->effects_inserted == 0);
    REQUIRE_FALSE(result->warnings.empty());
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("Live 12.3+") != std::string::npos;
    }));
    for (const auto& entry : buf.entries())
        CHECK(entry.request.property_or_method != "insert_device");
}

TEST_CASE("Mix compilation declines an absent target profile before mutation",
          "[ableton][mix][target-profile]") {
    auto graph = make_test_graph();
    CommandBuffer transport;
    transport.set_target_profile(std::nullopt);

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());

    auto contradictory = modeled_target_profile({12, 2, 0, "12.2.0"});
    contradictory.native_device_insertion = CapabilityState::Available;
    transport.set_target_profile(std::move(contradictory));
    const auto contradicted = compile_mix_to_ableton(graph, 0, transport);
    REQUIRE_FALSE(contradicted.has_value());
    CHECK(contradicted.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());
}

// =============================================================================
// Sends
// =============================================================================

TEST_CASE("send levels configured", "[ableton][mix]") {
    auto graph = make_full_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->sends_requested == 2);
    CHECK(r->sends_configured == 0);
    CHECK(r->send_levels_requested == 2);
    CHECK(r->send_levels_configured == 2);
    CHECK(r->send_modes_requested == 2);
    CHECK(r->send_modes_configured == 0);
    CHECK(std::count_if(r->warnings.begin(), r->warnings.end(), [](const auto& warning) {
              return warning.find("post-fader send mode was not applied") != std::string::npos;
          }) == 2);
}

TEST_CASE("pre-fader send level does not masquerade as configured send mode",
          "[ableton][mix][send-mode]") {
    auto graph = make_full_graph();
    graph.channels[0].sends[0].pre_fader = true;
    CommandBuffer buffer;

    const auto result = compile_mix_to_ableton(graph, 0, buffer);

    REQUIRE(result.has_value());
    CHECK(result->send_levels_configured == 2);
    CHECK(result->send_modes_configured == 0);
    CHECK(result->sends_configured == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("pre-fader send mode was not applied") != std::string::npos;
    }));
}

TEST_CASE("new aux buses are offset by existing Ableton return tracks",
          "[ableton][mix][target-state]") {
    auto graph = make_full_graph();
    CommandBuffer buf;
    buf.set_return_track_count(2);

    auto result = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(result.has_value());

    bool named_new_return = false;
    bool addressed_new_send = false;
    for (const auto& entry : buf.entries()) {
        named_new_return |= entry.request.path.to_string() == "song/return_tracks/2" &&
                            entry.request.property_or_method == "name";
        addressed_new_send |= entry.request.path.to_string().ends_with("mixer_device/sends/2");
    }
    CHECK(named_new_return);
    CHECK(addressed_new_send);
}

TEST_CASE("the largest signed return index is representable without increment overflow",
          "[ableton][mix][target-address]") {
    auto graph = make_full_graph();
    CommandBuffer transport;
    transport.set_return_track_count(static_cast<std::uint32_t>(std::numeric_limits<int>::max()));

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE(result.has_value());
    CHECK(result->return_tracks_created == 1);
    const auto expected = "song/return_tracks/" + std::to_string(std::numeric_limits<int>::max());
    CHECK(
        std::any_of(transport.entries().begin(), transport.entries().end(), [&](const auto& entry) {
            return entry.request.path.to_string() == expected;
        }));
}

TEST_CASE("return ranges exceeding the signed LOM index domain fail before mutation",
          "[ableton][mix][target-address]") {
    auto graph = make_full_graph();
    auto second = graph.aux_buses.front();
    second.id = AuxBusId{2};
    second.name = "Second Return";
    graph.aux_buses.push_back(std::move(second));
    CommandBuffer transport;
    transport.set_return_track_count(static_cast<std::uint32_t>(std::numeric_limits<int>::max()));

    const auto result = compile_mix_to_ableton(graph, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetAddressUnrepresentable);
    CHECK(transport.entries().empty());
}

// =============================================================================
// Master bus
// =============================================================================

TEST_CASE("master fader set", "[ableton][mix]") {
    auto graph = make_test_graph();
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_master_vol = false;
    for (const auto* e : sets) {
        if (e->request.path.to_string().find("master_track") != std::string::npos &&
            e->request.property_or_method == "display_value" &&
            e->request.path.to_string().ends_with("mixer_device/volume")) {
            found_master_vol = true;
        }
    }
    CHECK(found_master_vol);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("unsupported Ableton mix automation is reported", "[ableton][mix]") {
    auto graph = make_test_graph();
    MixAutomation ma;
    ma.target = "channels[0].fader.level_db";
    graph.automation.push_back(ma);
    graph.automation.push_back(ma);

    CommandBuffer buf;
    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->automation_lanes_requested == 2);
    CHECK(r->automation_lanes_written == 0);
    REQUIRE_FALSE(r->warnings.empty());
    CHECK(r->warnings[0].find("automation") != std::string::npos);
}

TEST_CASE("unsupported Mix target settings are reported instead of silently dropped",
          "[ableton][mix]") {
    auto graph = make_test_graph();
    graph.output_format = OutputFormat::LCR;
    graph.master_bus.output_format = OutputFormat::LCR;
    graph.channels[0].input_trim = 3.0f;
    graph.channels[0].polarity_invert = true;
    graph.channels[0].spatial.depth = 0.5f;
    graph.master_bus.dithering = DitheringConfig{};

    CommandBuffer buf;
    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->warnings.size() == 6);
    CHECK(r->warnings[0].find("Non-stereo") != std::string::npos);
    CHECK(r->warnings[1].find("input trim") != std::string::npos);
    CHECK(r->warnings[2].find("polarity") != std::string::npos);
    CHECK(r->warnings[3].find("spatial") != std::string::npos);
    CHECK(r->warnings[4].find("dithering") != std::string::npos);
    CHECK(r->warnings[5].find("output routes") != std::string::npos);
}

// =============================================================================
// Mute/Solo
// =============================================================================

TEST_CASE("channel mute and solo states are applied exactly", "[ableton][mix]") {
    auto graph = make_test_graph();
    graph.channels[0].mute = true;
    graph.channels[0].solo = false;
    graph.channels[1].mute = false;
    graph.channels[1].solo = true;
    CommandBuffer buf;

    auto r = compile_mix_to_ableton(graph, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int mute_count = 0;
    int solo_count = 0;
    std::vector<bool> mute_values;
    std::vector<bool> solo_values;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "mute") {
            ++mute_count;
            mute_values.push_back(std::get<bool>(e->request.args[0]));
        }
        if (e->request.property_or_method == "solo") {
            ++solo_count;
            solo_values.push_back(std::get<bool>(e->request.args[0]));
        }
    }
    CHECK(mute_count == 2);
    CHECK(solo_count == 2);
    CHECK(mute_values == std::vector<bool>{true, false});
    CHECK(solo_values == std::vector<bool>{false, true});
}
