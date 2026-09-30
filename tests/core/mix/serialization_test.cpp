/**
 * @file serialization_test.cpp
 * @brief Unit tests for Mix IR serialisation
 *
 *
 * Coverage: JSON round-trip for MixGraph, ChannelStrip, GroupBus,
 *           AuxBus, MasterBus, ReferenceProfile, automation,
 *           schema version checking
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>

using namespace sunny::core;

namespace {

void erase_parameter_maps(nlohmann::json& value) {
    if (value.is_object()) {
        value.erase("parameter_map");
        for (auto& [key, child] : value.items()) {
            (void)key;
            erase_parameter_maps(child);
        }
    } else if (value.is_array()) {
        for (auto& child : value)
            erase_parameter_maps(child);
    }
}

// Helper: build a graph with representative data for round-trip testing
MixGraph make_full_graph() {
    auto graph = create_mix_graph(MixGraphId{42}, {PartId{1}, PartId{2}});

    // Channel 0: EQ + compressor, spatial positioning, intent
    graph.channels[0].input_trim = 3.0f;
    graph.channels[0].spatial.pan = -0.5f;
    graph.channels[0].spatial.depth = 0.3f;
    graph.channels[0].fader.level_db = -2.0f;
    graph.channels[0].fader.relative_level =
        RelativeLevel{LevelReference{LevelReferenceType::Channel,
                                     -14.0f,
                                     graph.channels[1].id,
                                     "3 dB below Channel 2",
                                     GroupBusId{}},
                      -3.0f};
    graph.channels[0].intent =
        ChannelIntent{MixRole::Lead,
                      FrequencyAllocation{82.0f, 988.0f, 2000.0f, 5000.0f, 200.0f, 400.0f},
                      DepthPosition::FrontClose,
                      {{MixEffectId{10}, "Remove low-end rumble"}}};

    MixEQ eq;
    eq.bands.push_back({80.0f, 0.0f, 0.7f, MixEQBandType::LowCut, std::nullopt});
    eq.bands.push_back(
        {3000.0f, -2.5f, 2.0f, MixEQBandType::Peak, DynamicEQConfig{-15.0f, 3.0f, 5.0f, 50.0f}});
    eq.linear_phase = true;
    MixDeviceParameter frequency_mapping;
    frequency_mapping.parameter_name = "1 Frequency A";
    frequency_mapping.source_min = 20.0f;
    frequency_mapping.source_max = 20000.0f;
    frequency_mapping.range_min = 20.0f;
    frequency_mapping.range_max = 20000.0f;
    frequency_mapping.curve.type = MappingCurveType::Logarithmic;
    frequency_mapping.value_property = DeviceParameterValueProperty::DisplayValue;
    graph.channels[0].insert_chain.effects.push_back(
        {MixEffectId{10}, eq, true, {{"bands[0].frequency", frequency_mapping}}});

    MixCompressor comp;
    comp.threshold = -18.0f;
    comp.sidechain.source = SidechainSourceType::ExternalChannel;
    comp.sidechain.channel_id = graph.channels[1].id;
    comp.sidechain.filter = SidechainFilter{MixEQBandType::LowCut, 200.0f, 1.0f};
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{11}, comp, true});

    // Channel 1: spatial, gate
    graph.channels[1].spatial.pan = 0.6f;
    MixGate gate;
    gate.threshold = -35.0f;
    graph.channels[1].insert_chain.effects.push_back({MixEffectId{20}, gate, true});

    // Group bus
    GroupBus grp;
    grp.id = GroupBusId{100};
    grp.name = "Strings";
    grp.member_channels = {graph.channels[0].id, graph.channels[1].id};
    grp.intent = GroupIntent{"Glue strings", "Violin I prominent"};
    graph.group_buses.push_back(grp);
    graph.channels[0].group_assignment = grp.id;
    graph.channels[1].group_assignment = grp.id;

    // Aux bus
    AuxBus aux;
    aux.id = AuxBusId{200};
    aux.name = "Hall Reverb";
    aux.return_level = -3.0f;
    aux.intent = "Shared concert hall space";
    MixReverb reverb;
    reverb.algorithm = MixReverbAlgorithm::Hall;
    reverb.decay_time = 2.8f;
    reverb.pre_delay = 35.0f;
    reverb.damping = 0.65f;
    aux.effect_chain.effects.push_back({MixEffectId{30}, reverb, true});
    MixDelay delay;
    delay.tempo_synced = true;
    delay.beat_division = Beat{3, 16};
    delay.feedback = 0.42f;
    delay.stereo_mode = MixDelayMode::PingPong;
    aux.effect_chain.effects.push_back({MixEffectId{31}, delay, true});
    graph.aux_buses.push_back(aux);

    graph.channels[0].sends.push_back({aux.id, -6.0f, false, true});

    // Master bus
    graph.master_bus.target_loudness =
        LoudnessTarget{-16.0f, -1.0f, 8.0f, LoudnessStandard::StreamingDynamic};
    graph.master_bus.dithering = DitheringConfig{16, DitherAlgorithm::NoiseShaping, 5, 2, true};
    MixLimiter lim;
    lim.ceiling = -0.5f;
    graph.master_bus.insert_chain.effects.push_back({MixEffectId{40}, lim, true});

    // Saturation on master
    MixSaturation sat;
    sat.algorithm.type = SaturationTypeTag::Tape;
    sat.algorithm.tape_speed = TapeSpeed::Ips15;
    sat.drive = 0.3f;
    sat.mix = 0.5f;
    graph.master_bus.insert_chain.effects.push_back({MixEffectId{41}, sat, true});

    // Stereo processor on master
    MixStereoProcessor stereo;
    stereo.width = 1.2f;
    stereo.mono_below = 200.0f;
    graph.master_bus.insert_chain.effects.push_back({MixEffectId{42}, stereo, true});

    // Reference profile
    ReferenceProfile ref;
    ref.id = ReferenceProfileId{300};
    ref.name = "Reference Track";
    ref.source = "/path/to/reference.wav";
    ref.spectral_profile.average_spectrum = {{100.0f, -5.0f}, {1000.0f, 0.0f}};
    ref.spectral_profile.spectral_centroid = 2500.0f;
    ref.spectral_profile.spectral_tilt = -3.0f;
    ref.spectral_profile.band_energies = {{20.0f, 60.0f, -8.0f}, {60.0f, 250.0f, -2.0f}};
    ref.dynamic_profile = {12.0f, 8.0f, 10.0f, 14.0f};
    ref.loudness_profile = {-14.0f, -10.0f, -8.0f, -0.5f};
    ref.spatial_profile = {0.85f, 0.6f, 1.2f, 0.95f};
    ref.tonal_balance_curve = {{100.0f, -2.0f}, {10000.0f, -4.0f}};
    graph.reference_profiles.push_back(ref);

    // Automation
    graph.automation.push_back(
        {"channels[0].fader.level_db",
         {{ScoreTime{1, Beat{0, 1}}, -6.0f}, {ScoreTime{5, Beat{0, 1}}, 0.0f}},
         InterpolationMode::Linear,
         "Fade in"});

    // Annotations
    graph.mix_annotations.push_back({"overall", "Warm orchestral mix"});

    graph.output_format = OutputFormat::Stereo;

    return graph;
}

} // anonymous namespace

// =============================================================================
// Round-trip: full MixGraph
// =============================================================================

TEST_CASE("full MixGraph round-trip preserves all fields", "[mix-ir][serialisation]") {
    auto original = make_full_graph();
    auto json = mix_to_json(original);
    auto result = mix_from_json(json);

    REQUIRE(result.has_value());
    const auto& rt = *result;

    // Top-level
    CHECK(rt.id == original.id);
    CHECK(rt.output_format == original.output_format);
    CHECK(rt.max_group_nesting_depth == original.max_group_nesting_depth);

    // Channels
    REQUIRE(rt.channels.size() == original.channels.size());
    CHECK(rt.channels[0].id == original.channels[0].id);
    CHECK(rt.channels[0].part_id == original.channels[0].part_id);
    CHECK(rt.channels[0].input_trim == Catch::Approx(3.0f));
    CHECK(rt.channels[0].spatial.pan == Catch::Approx(-0.5f));
    CHECK(rt.channels[0].spatial.depth == Catch::Approx(0.3f));
    CHECK(rt.channels[0].fader.level_db == Catch::Approx(-2.0f));
    REQUIRE(rt.channels[0].fader.relative_level.has_value());
    CHECK(rt.channels[0].fader.relative_level->offset_db == Catch::Approx(-3.0f));

    // Channel intent
    REQUIRE(rt.channels[0].intent.has_value());
    CHECK(rt.channels[0].intent->role_in_mix == MixRole::Lead);
    CHECK(rt.channels[0].intent->depth_position == DepthPosition::FrontClose);
    CHECK(rt.channels[0].intent->frequency_space.fundamental_low == Catch::Approx(82.0f));
    REQUIRE(rt.channels[0].intent->frequency_space.avoid_low.has_value());
    CHECK(*rt.channels[0].intent->frequency_space.avoid_low == Catch::Approx(200.0f));

    // Effect chain
    REQUIRE(rt.channels[0].insert_chain.effects.size() == 2);
    CHECK(std::holds_alternative<MixEQ>(rt.channels[0].insert_chain.effects[0].parameters));
    const auto& rt_eq = std::get<MixEQ>(rt.channels[0].insert_chain.effects[0].parameters);
    CHECK(rt_eq.bands.size() == 2);
    CHECK(rt_eq.linear_phase == true);
    CHECK(rt_eq.bands[0].band_type == MixEQBandType::LowCut);
    REQUIRE(rt_eq.bands[1].dynamic.has_value());
    CHECK(rt_eq.bands[1].dynamic->threshold == Catch::Approx(-15.0f));
    const auto& rt_mapping = rt.channels[0].insert_chain.effects[0].parameter_map;
    REQUIRE(rt_mapping.size() == 1);
    CHECK(rt_mapping.at("bands[0].frequency").parameter_name == "1 Frequency A");
    CHECK(rt_mapping.at("bands[0].frequency").curve.type == MappingCurveType::Logarithmic);
    CHECK(rt_mapping.at("bands[0].frequency").value_property ==
          DeviceParameterValueProperty::DisplayValue);

    CHECK(std::holds_alternative<MixCompressor>(rt.channels[0].insert_chain.effects[1].parameters));
    const auto& rt_comp =
        std::get<MixCompressor>(rt.channels[0].insert_chain.effects[1].parameters);
    CHECK(rt_comp.threshold == Catch::Approx(-18.0f));
    CHECK(rt_comp.sidechain.source == SidechainSourceType::ExternalChannel);
    REQUIRE(rt_comp.sidechain.filter.has_value());

    // Sends
    REQUIRE(rt.channels[0].sends.size() == 1);
    CHECK(rt.channels[0].sends[0].level_db == Catch::Approx(-6.0f));

    // Group assignment
    REQUIRE(rt.channels[0].group_assignment.has_value());
    CHECK(rt.channels[0].group_assignment->value == 100);

    // Group buses
    REQUIRE(rt.group_buses.size() == 1);
    CHECK(rt.group_buses[0].name == "Strings");
    CHECK(rt.group_buses[0].member_channels.size() == 2);
    REQUIRE(rt.group_buses[0].intent.has_value());
    CHECK(rt.group_buses[0].intent->function == "Glue strings");

    // Aux buses
    REQUIRE(rt.aux_buses.size() == 1);
    CHECK(rt.aux_buses[0].name == "Hall Reverb");
    CHECK(rt.aux_buses[0].return_level == Catch::Approx(-3.0f));
    REQUIRE(rt.aux_buses[0].intent.has_value());
    CHECK(*rt.aux_buses[0].intent == "Shared concert hall space");
    REQUIRE(rt.aux_buses[0].effect_chain.effects.size() == 2);
    REQUIRE(std::holds_alternative<MixReverb>(rt.aux_buses[0].effect_chain.effects[0].parameters));
    const auto& rt_reverb = std::get<MixReverb>(rt.aux_buses[0].effect_chain.effects[0].parameters);
    CHECK(rt_reverb.algorithm == MixReverbAlgorithm::Hall);
    CHECK(rt_reverb.decay_time == Catch::Approx(2.8f));
    REQUIRE(std::holds_alternative<MixDelay>(rt.aux_buses[0].effect_chain.effects[1].parameters));
    const auto& rt_delay = std::get<MixDelay>(rt.aux_buses[0].effect_chain.effects[1].parameters);
    CHECK(rt_delay.tempo_synced);
    CHECK(rt_delay.beat_division == (Beat{3, 16}));
    CHECK(rt_delay.stereo_mode == MixDelayMode::PingPong);

    // Master bus
    REQUIRE(rt.master_bus.target_loudness.has_value());
    CHECK(rt.master_bus.target_loudness->integrated_lufs == Catch::Approx(-16.0f));
    CHECK(rt.master_bus.target_loudness->standard == LoudnessStandard::StreamingDynamic);
    REQUIRE(rt.master_bus.dithering.has_value());
    CHECK(rt.master_bus.dithering->algorithm == DitherAlgorithm::NoiseShaping);
    REQUIRE(rt.master_bus.insert_chain.effects.size() == 3);

    // Limiter
    CHECK(std::holds_alternative<MixLimiter>(rt.master_bus.insert_chain.effects[0].parameters));
    // Saturation
    CHECK(std::holds_alternative<MixSaturation>(rt.master_bus.insert_chain.effects[1].parameters));
    const auto& rt_sat = std::get<MixSaturation>(rt.master_bus.insert_chain.effects[1].parameters);
    CHECK(rt_sat.algorithm.type == SaturationTypeTag::Tape);
    CHECK(rt_sat.algorithm.tape_speed == TapeSpeed::Ips15);
    CHECK(rt_sat.drive == Catch::Approx(0.3f));
    // Stereo
    CHECK(std::holds_alternative<MixStereoProcessor>(
        rt.master_bus.insert_chain.effects[2].parameters));
    const auto& rt_stereo =
        std::get<MixStereoProcessor>(rt.master_bus.insert_chain.effects[2].parameters);
    CHECK(rt_stereo.width == Catch::Approx(1.2f));
    REQUIRE(rt_stereo.mono_below.has_value());
    CHECK(*rt_stereo.mono_below == Catch::Approx(200.0f));

    // Reference profiles
    REQUIRE(rt.reference_profiles.size() == 1);
    CHECK(rt.reference_profiles[0].name == "Reference Track");
    CHECK(rt.reference_profiles[0].spectral_profile.spectral_centroid == Catch::Approx(2500.0f));
    CHECK(rt.reference_profiles[0].loudness_profile.integrated == Catch::Approx(-14.0f));
    CHECK(rt.reference_profiles[0].tonal_balance_curve.size() == 2);

    // Automation
    REQUIRE(rt.automation.size() == 1);
    CHECK(rt.automation[0].target == "channels[0].fader.level_db");
    CHECK(rt.automation[0].breakpoints.size() == 2);
    REQUIRE(rt.automation[0].intent.has_value());
    CHECK(*rt.automation[0].intent == "Fade in");

    // Annotations
    REQUIRE(rt.mix_annotations.size() == 1);
    CHECK(rt.mix_annotations[0].scope == "overall");
}

// =============================================================================
// String round-trip
// =============================================================================

TEST_CASE("Mix JSON string round-trip", "[mix-ir][serialisation]") {
    auto original = make_full_graph();
    auto json_str = mix_to_json_string(original);
    auto result = mix_from_json_string(json_str);

    REQUIRE(result.has_value());
    CHECK(result->channels.size() == original.channels.size());
    CHECK(result->id == original.id);
}

// =============================================================================
// Schema version
// =============================================================================

TEST_CASE("Mix parser rejects wrong schema version", "[mix-ir][serialisation]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    auto json = mix_to_json(graph);
    json["schema_version"] = 999;

    auto result = mix_from_json(json);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("Mix schema v3 reader migrates schema v1 and v2 documents without target mappings",
          "[mix-ir][serialisation][schema]") {
    for (int legacy_version : {1, 2}) {
        auto json = mix_to_json(make_full_graph());
        json["schema_version"] = legacy_version;
        erase_parameter_maps(json);

        auto result = mix_from_json(json);
        REQUIRE(result.has_value());
        CHECK(result->id == MixGraphId{42});
        CHECK(result->channels[0].insert_chain.effects[0].parameter_map.empty());
    }
}

TEST_CASE("Mix schema v3 requires an explicit parameter_map field",
          "[mix-ir][serialisation][schema]") {
    auto json = mix_to_json(make_full_graph());
    json["channels"][0]["insert_chain"][0].erase("parameter_map");

    const auto result = mix_from_json(json);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("Mix schema v3 rejects invalid parameter mappings", "[mix-ir][serialisation][schema]") {
    auto json = mix_to_json(make_full_graph());
    json["channels"][0]["insert_chain"][0]["parameter_map"]["bands[0].frequency"]["source_max"] =
        20.0;

    const auto result = mix_from_json(json);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ValidationOnLoadFailed);
}

TEST_CASE("Mix parser rejects unknown effect discriminators",
          "[mix-ir][serialisation][validation]") {
    auto json = mix_to_json(make_full_graph());
    json["channels"][0]["insert_chain"][0]["type"] = "fictional_processor";

    auto result = mix_from_json(json);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("Mix parser rejects invalid spatial-effect parameters",
          "[mix-ir][serialisation][validation]") {
    auto json = mix_to_json(make_full_graph());
    json["aux_buses"][0]["effect_chain"][0]["params"]["decay_time"] = -1.0;

    auto result = mix_from_json(json);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ValidationOnLoadFailed);
}

TEST_CASE("Mix parser rejects out-of-range enum values", "[mix-ir][serialisation]") {
    const auto valid_json = mix_to_json(make_full_graph());

    SECTION("top-level output format") {
        auto json = valid_json;
        json["output_format"] = 99;

        auto result = mix_from_json(json);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }

    SECTION("nested channel pan law") {
        auto json = valid_json;
        json["channels"][0]["spatial"]["pan_law"] = 99;

        auto result = mix_from_json(json);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }

    SECTION("nested eight-bit dither field") {
        auto json = valid_json;
        json["master_bus"]["dithering"]["target_bit_depth"] = 256;

        auto result = mix_from_json(json);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }
}

// =============================================================================
// Malformed JSON
// =============================================================================

TEST_CASE("rejects malformed JSON string", "[mix-ir][serialisation]") {
    auto result = mix_from_json_string("{not valid json}");
    CHECK_FALSE(result.has_value());
}

TEST_CASE("rejects JSON missing required fields", "[mix-ir][serialisation]") {
    nlohmann::json j;
    j["schema_version"] = MIX_IR_SCHEMA_VERSION;
    // Missing "id", "channels", etc.

    auto result = mix_from_json(j);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Minimal graph round-trip
// =============================================================================

TEST_CASE("empty graph round-trip", "[mix-ir][serialisation]") {
    auto graph = create_mix_graph(MixGraphId{1}, {});
    auto json = mix_to_json(graph);
    auto result = mix_from_json(json);

    REQUIRE(result.has_value());
    CHECK(result->channels.empty());
    CHECK(result->group_buses.empty());
    CHECK(result->aux_buses.empty());
}

// =============================================================================
// Gate round-trip
// =============================================================================

TEST_CASE("MixGate round-trip", "[mix-ir][serialisation]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
    MixGate gate;
    gate.threshold = -30.0f;
    gate.hold = 75.0f;
    gate.range = -60.0f;
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, gate, true});

    auto json = mix_to_json(graph);
    auto result = mix_from_json(json);
    REQUIRE(result.has_value());

    const auto& rt_gate = std::get<MixGate>(result->channels[0].insert_chain.effects[0].parameters);
    CHECK(rt_gate.threshold == Catch::Approx(-30.0f));
    CHECK(rt_gate.hold == Catch::Approx(75.0f));
    CHECK(rt_gate.range == Catch::Approx(-60.0f));
}

// =============================================================================
// Multiband dynamics round-trip
// =============================================================================

TEST_CASE("MixMultibandDynamics round-trip", "[mix-ir][serialisation]") {
    auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});

    MixMultibandDynamics mb;
    mb.crossover_frequencies = {200.0f, 2000.0f, 8000.0f};
    mb.crossover_slope = CrossoverSlope::LinearPhase;
    MultibandDynamicsBand band;
    band.compressor = MixCompressor{};
    band.gain = 2.0f;
    mb.bands = {band, MultibandDynamicsBand{}, MultibandDynamicsBand{}, MultibandDynamicsBand{}};
    graph.channels[0].insert_chain.effects.push_back({MixEffectId{1}, mb, true});

    auto json = mix_to_json(graph);
    auto result = mix_from_json(json);
    REQUIRE(result.has_value());

    const auto& rt_mb =
        std::get<MixMultibandDynamics>(result->channels[0].insert_chain.effects[0].parameters);
    CHECK(rt_mb.crossover_frequencies.size() == 3);
    CHECK(rt_mb.crossover_slope == CrossoverSlope::LinearPhase);
    CHECK(rt_mb.bands.size() == 4);
    REQUIRE(rt_mb.bands[0].compressor.has_value());
    CHECK(rt_mb.bands[0].gain == Catch::Approx(2.0f));
}
