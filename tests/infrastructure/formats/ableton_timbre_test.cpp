/**
 * @file ableton_timbre_test.cpp
 * @brief Unit tests for Timbre IR → Ableton compiler
 *
 *
 * Coverage: Device loading, effect insertion, parameter mapping,
 *           preset loading, automation lanes
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/formats/ableton_timbre.hpp>

using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;
using namespace sunny::core;

namespace {

class ReadbackTransport final : public LomTransport {
  public:
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (request.property_or_method == "insert_device") {
            if (malformed_device_evidence)
                return {true, nlohmann::json{{"requested_name", "Analog"}}, std::nullopt};
            const auto& requested_name = std::get<std::string>(request.args.at(0));
            const auto requested_index = std::get<int>(request.args.at(1));
            return {
                true,
                nlohmann::json{
                    {"requested_name", requested_name},
                    {"requested_index", requested_index},
                    {"before_count", requested_index},
                    {"after_count", requested_index + 1},
                    {"device_index", requested_index},
                    {"name", requested_name},
                    {"class_display_name",
                     diverge_device_identity ? "Different Device" : requested_name},
                    {"class_name", requested_name},
                    {"type", requested_index == 0 ? device_type : 2},
                    {"is_active", device_active},
                    {"can_have_chains",
                     integral_can_have_chains ? nlohmann::json(device_can_have_chains ? 1 : 0)
                                              : nlohmann::json(device_can_have_chains)},
                    {"latency_in_samples", latency_in_samples},
                    {"latency_in_ms",
                     integral_latency_in_ms ? nlohmann::json(3) : nlohmann::json(latency_in_ms)},
                    {"track_has_audio_output", track_has_audio_output},
                    {"track_has_midi_output", track_has_midi_output}},
                std::nullopt};
        }
        if (request.property_or_method != "sunny_set_device_parameter")
            return {true, std::nullopt, std::nullopt};
        if (omit_parameter_evidence) return {true, std::nullopt, std::nullopt};
        const auto* requested = std::get_if<double>(&request.args.at(1));
        if (requested == nullptr) return {false, std::nullopt, std::string{"invalid test request"}};
        const auto& property = std::get<std::string>(request.args.at(2));
        const auto minimum = std::get<double>(request.args.at(3));
        const auto maximum = std::get<double>(request.args.at(4));
        nlohmann::json evidence = {{"matched_name", "Filter Freq"},
                                   {"original_name", "Filter Freq"},
                                   {"property", property},
                                   {"requested", *requested},
                                   {"observed", *requested},
                                   {"minimum", minimum},
                                   {"maximum", maximum},
                                   {"is_quantized", false},
                                   {"default_value", minimum},
                                   {"value_items", nullptr},
                                   {"is_enabled", true},
                                   {"state", parameter_state},
                                   {"automation_state", automation_state}};
        switch (parameter_domain_mode) {
        case 1:
            evidence["is_quantized"] = true;
            evidence["default_value"] = nullptr;
            evidence["value_items"] = nlohmann::json::array({"Low", "High"});
            break;
        case 2:
            evidence["is_quantized"] = true;
            evidence["value_items"] = nlohmann::json::array({"Low", "High"});
            break;
        case 3:
            evidence["value_items"] = nlohmann::json::array({"Low", "High"});
            break;
        case 4:
            evidence["default_value"] = 0;
            break;
        case 5:
            evidence["is_quantized"] = true;
            evidence["default_value"] = nullptr;
            evidence["value_items"] = nlohmann::json::array({"Low", 1});
            break;
        case 6:
            evidence["default_value"] = maximum + 1.0;
            break;
        default:
            break;
        }
        if (extra_parameter_evidence) evidence["unexpected"] = true;
        return {true, std::move(evidence), std::nullopt};
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {true, std::nullopt, std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return target;
    }

    [[nodiscard]] Result<std::optional<std::uint32_t>> device_count(const LomPath&) override {
        return std::optional<std::uint32_t>{existing_device_count};
    }

    std::optional<AbletonTargetProfile> target;
    std::vector<LomRequest> requests;
    bool omit_parameter_evidence = false;
    bool extra_parameter_evidence = false;
    bool malformed_device_evidence = false;
    bool diverge_device_identity = false;
    bool device_active = true;
    bool device_can_have_chains = false;
    bool integral_can_have_chains = false;
    bool track_has_audio_output = true;
    bool track_has_midi_output = false;
    std::uint32_t latency_in_samples = 128;
    double latency_in_ms = 2.9;
    bool integral_latency_in_ms = false;
    int device_type = 1;
    std::uint32_t existing_device_count = 0;
    int parameter_state = 0;
    int automation_state = 0;
    int parameter_domain_mode = 0;
};

/// Create a minimal TimbreProfile with SubtractiveSynth source
TimbreProfile make_subtractive_profile() {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Lead Synth");
    auto& sub = std::get<SubtractiveSynth>(p.source.data);
    sub.filter.cutoff = 8000.0f;

    return p;
}

/// Create a profile with FM source
TimbreProfile make_fm_profile() {
    auto p = create_timbre_profile(TimbreProfileId{2}, PartId{2}, "FM Bell");
    FMSynth fm;
    fm.operators.push_back(FMOperator{});
    p.source.data = std::move(fm);

    return p;
}

} // namespace

// =============================================================================
// Device loading
// =============================================================================

TEST_CASE("SubtractiveSynth maps to Analog", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->devices_requested == 1);
    CHECK(r->devices_created == 1);
    CHECK(r->devices_verified == 0);
    REQUIRE(r->device_deployments.size() == 1);
    CHECK_FALSE(r->device_deployments.front().observed_type.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "insert_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Analog") found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("live Timbre source insertion requires exact active instrument evidence",
          "[ableton][timbre][device][readback]") {
    auto profile = make_subtractive_profile();
    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});

    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_created == 1);
    CHECK(result->devices_verified == 1);
    REQUIRE(result->device_deployments.size() == 1);
    const auto& deployment = result->device_deployments.front();
    CHECK(deployment.device_path == "song/tracks/0/devices/0");
    CHECK(deployment.observed_class_display_name == "Analog");
    CHECK(deployment.observed_type == 1);
    CHECK(deployment.observed_active == true);
    CHECK(deployment.observed_can_have_chains == false);
    CHECK(deployment.flat_device_verified);
    CHECK(deployment.observed_latency_in_samples == 128);
    CHECK(deployment.observed_latency_in_ms == 2.9);
    CHECK(deployment.reported_latency_observed);
    CHECK_FALSE(deployment.render_path_latency_fully_observed);
    CHECK(deployment.observed_track_has_audio_output == true);
    CHECK(deployment.observed_track_has_midi_output == false);
    CHECK(deployment.output_verified);
    CHECK(deployment.verified);

    transport.device_type = 2;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("not reported as an instrument") != std::string::npos;
    }));

    transport.device_type = 1;
    transport.latency_in_samples =
        static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) + 1U;
    result = compile_timbre_to_ableton(profile, 0, transport);
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.latency_in_samples = 128;
    transport.integral_latency_in_ms = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.integral_latency_in_ms = false;
    transport.latency_in_ms = -0.1;
    result = compile_timbre_to_ableton(profile, 0, transport);
    CHECK(result.error() == ErrorCode::ProtocolError);
    transport.latency_in_ms = 2.9;

    transport.device_type = 1;
    transport.device_active = false;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("inactive") != std::string::npos;
    }));

    transport.device_active = true;
    transport.device_can_have_chains = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 0);
    CHECK_FALSE(result->device_deployments.front().flat_device_verified);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("is a Rack") != std::string::npos;
    }));

    transport.device_can_have_chains = false;
    transport.integral_can_have_chains = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.integral_can_have_chains = false;
    transport.diverge_device_identity = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("identity differs") != std::string::npos;
    }));

    transport.diverge_device_identity = false;
    transport.track_has_audio_output = false;
    transport.track_has_midi_output = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("instrument-generated audio output") != std::string::npos;
    }));

    transport.track_has_audio_output = true;
    transport.track_has_midi_output = false;
    transport.malformed_device_evidence = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

TEST_CASE("Timbre source insertion requires an observed empty insertable chain",
          "[ableton][timbre][device][preflight]") {
    auto profile = make_subtractive_profile();
    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    transport.existing_device_count = 1;

    const auto result = compile_timbre_to_ableton(profile, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetValueUnrepresentable);
    CHECK(
        std::none_of(transport.requests.begin(), transport.requests.end(), [](const auto& request) {
            return request.property_or_method == "insert_device";
        }));
}

TEST_CASE("FMSynth maps to Operator", "[ableton][timbre]") {
    auto profile = make_fm_profile();
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "insert_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Operator") found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("explicit device name overrides default", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    profile.rendering.device_type.tag = DeviceTypeTag::NativeAbleton;
    profile.rendering.device_type.device_name = "Wavetable";
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "insert_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Wavetable") found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("granular and hybrid sources are not replaced with merely similar native devices",
          "[ableton][timbre][conformance]") {
    auto profile = make_subtractive_profile();
    profile.source.data = GranularSynth{};
    CommandBuffer buf;

    auto granular = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(granular.has_value());
    CHECK(granular->devices_requested == 1);
    CHECK(granular->devices_created == 0);
    REQUIRE_FALSE(granular->warnings.empty());
    CHECK(granular->warnings[0].find("no faithful") != std::string::npos);

    auto hybrid_profile = make_subtractive_profile();
    const auto valid_subtractive = std::get<SubtractiveSynth>(hybrid_profile.source.data);
    auto layer1 = std::make_unique<SoundSourceData>();
    layer1->data = valid_subtractive;
    auto layer2 = std::make_unique<SoundSourceData>();
    layer2->data = valid_subtractive;
    HybridSource hybrid_source;
    hybrid_source.layers.push_back(std::move(layer1));
    hybrid_source.layers.push_back(std::move(layer2));
    hybrid_source.routing.mix_levels = {0.5f, 0.5f};
    hybrid_profile.source.data = std::move(hybrid_source);
    buf.clear();
    auto hybrid = compile_timbre_to_ableton(hybrid_profile, 0, buf);
    REQUIRE(hybrid.has_value());
    CHECK(hybrid->devices_created == 0);
    CHECK(buf.size() == 0);
}

TEST_CASE("plugin source gap does not assume an existing instrument for audio effects",
          "[ableton][timbre][conformance]") {
    auto profile = make_subtractive_profile();
    profile.rendering.device_type.tag = DeviceTypeTag::Plugin;
    profile.rendering.device_type.plugin_identifier = "com.example.synth";
    profile.insert_chain.effects.push_back(Effect{EffectId{1}, ReverbEffect{}, true, 1.0f});
    CommandBuffer buf;

    auto result = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(result.has_value());
    CHECK(result->devices_requested == 1);
    CHECK(result->devices_created == 0);
    CHECK(result->effects_requested == 1);
    CHECK(result->effects_inserted == 0);
    REQUIRE(result->warnings.size() == 2);
    CHECK(result->warnings[1].find("did not materialise an instrument") != std::string::npos);
    CHECK(buf.size() == 0);
}

TEST_CASE("current Live phaser and flanger intents map to Phaser-Flanger",
          "[ableton][timbre][conformance]") {
    auto profile = make_subtractive_profile();
    profile.insert_chain.effects.push_back(Effect{EffectId{1}, PhaserEffect{}, true, 1.0f});
    profile.insert_chain.effects.push_back(Effect{EffectId{2}, FlangerEffect{}, true, 1.0f});
    CommandBuffer buf;

    auto result = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(result.has_value());
    std::size_t combined_device_count = 0;
    for (const auto& entry : buf.entries()) {
        if (entry.request.property_or_method != "insert_device" || entry.request.args.empty())
            continue;
        const auto* name = std::get_if<std::string>(&entry.request.args[0]);
        if (name != nullptr && *name == "Phaser-Flanger") ++combined_device_count;
    }
    CHECK(combined_device_count == 2);
}

TEST_CASE("pre-12.3 target skips native Timbre devices before mutating the track",
          "[ableton][timbre][target-profile]") {
    auto profile = make_subtractive_profile();
    profile.insert_chain.effects.push_back(Effect{EffectId{1}, ReverbEffect{}, true, 1.0f});
    CommandBuffer buf;
    buf.set_target_profile(modeled_target_profile({12, 2, 0, "12.2.0"}));

    auto result = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(result.has_value());
    CHECK(result->devices_requested == 1);
    CHECK(result->devices_created == 0);
    CHECK(result->effects_requested == 1);
    CHECK(result->effects_inserted == 0);
    REQUIRE(result->warnings.size() >= 2);
    for (const auto& entry : buf.entries())
        CHECK(entry.request.property_or_method != "insert_device");
}

TEST_CASE("Timbre compilation declines an absent target profile before mutation",
          "[ableton][timbre][target-profile]") {
    auto profile = make_subtractive_profile();
    CommandBuffer transport;
    transport.set_target_profile(std::nullopt);

    const auto result = compile_timbre_to_ableton(profile, 0, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());

    auto contradictory = modeled_target_profile({12, 2, 0, "12.2.0"});
    contradictory.native_device_insertion = CapabilityState::Available;
    transport.set_target_profile(std::move(contradictory));
    const auto contradicted = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(contradicted.has_value());
    CHECK(contradicted.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());
}

// =============================================================================
// Effect chain
// =============================================================================

TEST_CASE("effects inserted in order", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();

    Effect reverb;
    reverb.id = EffectId{1};
    reverb.parameters = ReverbEffect{};
    reverb.mix = 0.3f;
    profile.insert_chain.effects.push_back(reverb);

    Effect delay;
    delay.id = EffectId{2};
    delay.parameters = DelayEffect{};
    delay.mix = 0.5f;
    profile.insert_chain.effects.push_back(delay);

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->effects_requested == 2);
    CHECK(r->effects_inserted == 2);
    CHECK(r->effects_verified == 0);
    CHECK(r->device_deployments.size() == 3);
    CHECK(r->parameters_mapped == 2);
    CHECK(r->parameters_verified == 0);
    REQUIRE(r->parameter_deployments.size() == 2);
    CHECK(r->parameter_deployments[0].ir_path == "insert_chain.effects[0].mix");
    CHECK(r->parameter_deployments[1].ir_path == "insert_chain.effects[1].mix");
}

TEST_CASE("live Timbre effects require exact active audio-effect evidence",
          "[ableton][timbre][device][readback]") {
    auto profile = make_subtractive_profile();
    profile.insert_chain.effects.push_back(Effect{EffectId{1}, ReverbEffect{}, true, 1.0f});
    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});

    const auto result = compile_timbre_to_ableton(profile, 0, transport);

    REQUIRE(result.has_value());
    CHECK(result->devices_verified == 1);
    CHECK(result->effects_inserted == 1);
    CHECK(result->effects_verified == 1);
    REQUIRE(result->device_deployments.size() == 2);
    CHECK(result->device_deployments[1].requested_name == "Reverb");
    CHECK(result->device_deployments[1].requested_index == 1);
    CHECK(result->device_deployments[0].requested_type == 1);
    CHECK(result->device_deployments[1].requested_type == 2);
    CHECK(result->device_deployments[1].observed_type == 2);
    CHECK(result->device_deployments[1].verified);
}

TEST_CASE("unverified live source blocks dependent effect and parameter mutations",
          "[ableton][timbre][device][dependency]") {
    auto profile = make_subtractive_profile();
    profile.insert_chain.effects.push_back(Effect{EffectId{1}, ReverbEffect{}, true, 1.0f});
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;
    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    transport.device_active = false;

    const auto result = compile_timbre_to_ableton(profile, 0, transport);

    REQUIRE(result.has_value());
    CHECK(result->devices_created == 1);
    CHECK(result->devices_verified == 0);
    CHECK(result->effects_requested == 1);
    CHECK(result->effects_inserted == 0);
    CHECK(result->parameters_mapped == 0);
    CHECK(
        std::none_of(transport.requests.begin(), transport.requests.end(), [](const auto& request) {
            return request.property_or_method == "sunny_set_device_parameter";
        }));
    CHECK(std::count_if(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
              return warning.find("not materialise an instrument") != std::string::npos ||
                     warning.find("target device was not materialised") != std::string::npos;
          }) == 2);
}

TEST_CASE("disabled effects are skipped", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();

    Effect reverb;
    reverb.id = EffectId{1};
    reverb.parameters = ReverbEffect{};
    reverb.enabled = false;
    profile.insert_chain.effects.push_back(reverb);

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->effects_requested == 0);
    CHECK(r->effects_inserted == 0);
}

// =============================================================================
// Parameter mapping
// =============================================================================

TEST_CASE("parameter mappings require an explicit IR source domain before mutation",
          "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    DeviceParameter dp;
    dp.device_index = 0;
    dp.parameter_name = "Filter Freq";
    dp.range_min = 20.0f;
    dp.range_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = dp;

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::TimbreInvalidParameter);
    CHECK(buf.entries().empty());
}

TEST_CASE("parameter mappings reject non-materialisable device indices before mutation",
          "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.device_index = 4;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    CommandBuffer transport;
    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TimbreInvalidParameter);
    CHECK(transport.entries().empty());
}

TEST_CASE("parameter mappings resolve IR values and record exact target writes",
          "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.device_index = 0;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    CommandBuffer transport;
    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->parameters_mapped == 1);
    CHECK(result->parameters_verified == 0);
    REQUIRE(result->parameter_deployments.size() == 1);
    const auto& deployment = result->parameter_deployments.front();
    CHECK(deployment.source_value == 8000.0f);
    CHECK(deployment.requested_value > 0.39f);
    CHECK(deployment.requested_value < 0.41f);
    CHECK(deployment.range_min == 0.0f);
    CHECK(deployment.range_max == 1.0f);
    CHECK_FALSE(deployment.matched_name.has_value());
    CHECK_FALSE(deployment.original_name.has_value());
    CHECK_FALSE(deployment.observed_minimum.has_value());
    CHECK_FALSE(deployment.observed_maximum.has_value());
    CHECK_FALSE(deployment.is_quantized.has_value());
    CHECK_FALSE(deployment.is_enabled.has_value());
    CHECK(deployment.action == AbletonParameterAction::RecordedOnly);
    CHECK_FALSE(deployment.verified);

    const auto calls = transport.find_by_type(LomRequestType::CallMethod);
    const auto request = std::find_if(calls.begin(), calls.end(), [](const auto* entry) {
        return entry->request.property_or_method == "sunny_set_device_parameter";
    });
    REQUIRE(request != calls.end());
    CHECK(std::get<std::string>((*request)->request.args.at(0)) == "Filter Freq");
    CHECK(std::get<std::string>((*request)->request.args.at(2)) == "value");
}

TEST_CASE("named Live targets require and expose parameter readback evidence",
          "[ableton][timbre][target-profile]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    mapping.value_property = DeviceParameterValueProperty::DisplayValue;
    mapping.range_min = 20.0f;
    mapping.range_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    CHECK(result->parameters_mapped == 1);
    CHECK(result->parameters_verified == 1);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK(result->parameter_deployments.front().verified);
    CHECK(result->parameter_deployments.front().range_min == 20.0f);
    CHECK(result->parameter_deployments.front().range_max == 20000.0f);
    CHECK(result->parameter_deployments.front().matched_name == "Filter Freq");
    CHECK(result->parameter_deployments.front().original_name == "Filter Freq");
    CHECK(result->parameter_deployments.front().observed_value == 8000.0f);
    CHECK(result->parameter_deployments.front().observed_minimum == 20.0f);
    CHECK(result->parameter_deployments.front().observed_maximum == 20000.0f);
    CHECK(result->parameter_deployments.front().is_quantized == false);
    CHECK(result->parameter_deployments.front().default_value == 20.0f);
    CHECK_FALSE(result->parameter_deployments.front().value_items.has_value());
    CHECK(result->parameter_deployments.front().is_enabled == true);
    CHECK(result->parameter_deployments.front().action == AbletonParameterAction::Set);
    CHECK(result->parameter_deployments.front().parameter_state == 0);
    CHECK(result->parameter_deployments.front().automation_state == 0);
}

TEST_CASE("DeviceParameter conditional domains are retained and fail closed",
          "[ableton][timbre][device-parameter][domain]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    mapping.range_min = 0.0f;
    mapping.range_max = 1.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    transport.parameter_domain_mode = 1;
    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE(result.has_value());
    REQUIRE(result->parameter_deployments.size() == 1);
    const auto& quantized = result->parameter_deployments.front();
    CHECK(quantized.is_quantized == true);
    CHECK_FALSE(quantized.default_value.has_value());
    REQUIRE(quantized.value_items.has_value());
    CHECK(*quantized.value_items == std::vector<std::string>{"Low", "High"});
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("do not prove continuous mapping semantics") != std::string::npos;
    }));

    for (const int malformed_mode : {2, 3, 4, 5, 6}) {
        transport.parameter_domain_mode = malformed_mode;
        result = compile_timbre_to_ableton(profile, 0, transport);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ProtocolError);
    }
}

TEST_CASE("writable but inactive or automated Live parameters remain incomplete evidence",
          "[ableton][timbre][device-parameter][state]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    mapping.value_property = DeviceParameterValueProperty::DisplayValue;
    mapping.range_min = 20.0f;
    mapping.range_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    transport.parameter_state = 1;
    transport.automation_state = 1;
    const auto result = compile_timbre_to_ableton(profile, 0, transport);

    REQUIRE(result.has_value());
    CHECK(result->parameters_verified == 1);
    REQUIRE(result->parameter_deployments.size() == 1);
    CHECK(result->parameter_deployments.front().parameter_state == 1);
    CHECK(result->parameter_deployments.front().automation_state == 1);
    CHECK(std::count_if(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
              return warning.find("inactive") != std::string::npos ||
                     warning.find("automation") != std::string::npos;
          }) == 2);

    transport.parameter_state = 3;
    const auto malformed = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(malformed.has_value());
    CHECK(malformed.error() == ErrorCode::ProtocolError);
}

TEST_CASE("named Live targets reject empty parameter acknowledgements",
          "[ableton][timbre][target-profile]") {
    auto profile = make_subtractive_profile();
    DeviceParameter mapping;
    mapping.parameter_name = "Filter Freq";
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = mapping;

    ReadbackTransport transport;
    transport.target = modeled_target_profile({12, 3, 0, "12.3.0"});
    transport.omit_parameter_evidence = true;
    auto result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.omit_parameter_evidence = false;
    transport.extra_parameter_evidence = true;
    result = compile_timbre_to_ableton(profile, 0, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

// =============================================================================
// Preset
// =============================================================================

TEST_CASE("unsupported preset path is reported", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    profile.rendering.preset_path = "Presets/Lead/BrightLead.adv";

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    REQUIRE(r->warnings.size() == 2);
    CHECK(r->warnings[0].find("Preset path") != std::string::npos);
    CHECK(r->warnings[1].find("source-specific parameter values") != std::string::npos);
    CHECK(buf.count_type(LomRequestType::SetProperty) == 0);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("unsupported Ableton timbre automation is reported", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    TimbreAutomation ta;
    ta.parameter_path = "source.filter.cutoff";
    ta.breakpoints.push_back({ScoreTime{1, Beat::zero()}, 1000.0f});
    profile.parameter_automation.push_back(ta);
    profile.parameter_automation.push_back(ta);

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->automation_lanes_requested == 2);
    CHECK(r->automation_lanes_written == 0);
    REQUIRE(r->warnings.size() == 2);
    CHECK(r->warnings[0].find("source-specific parameter values") != std::string::npos);
    CHECK(r->warnings[1].find("automation") != std::string::npos);
}

TEST_CASE("source and effect parameter gaps make Timbre compilation explicit",
          "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    Effect reverb;
    reverb.id = EffectId{1};
    reverb.parameters = ReverbEffect{};
    profile.insert_chain.effects.push_back(reverb);
    CommandBuffer buf;

    auto result = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(result.has_value());
    REQUIRE(result->warnings.size() == 2);
    CHECK(result->warnings[0].find("source-specific parameter values") != std::string::npos);
    CHECK(result->warnings[1].find("effect-specific parameter values") != std::string::npos);
}
