/**
 * @file TSAL002A.cpp
 * @brief Unit tests for Timbre IR → Ableton compiler (FMAL002A)
 *
 * Component: TSAL002A
 * Domain: TS (Test) | Category: AL (Ableton Live)
 *
 * Tests: FMAL002A
 * Coverage: Device loading, effect insertion, parameter mapping,
 *           preset loading, automation lanes
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMAL002A.h"
#include "Bridge/INTP001A.h"

using namespace Sunny::Infrastructure;
using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

namespace {

/// Create a minimal TimbreProfile with SubtractiveSynth source
TimbreProfile make_subtractive_profile() {
    TimbreProfile p;
    p.id = TimbreProfileId{1};
    p.part_id = PartId{1};
    p.name = "Lead Synth";

    SubtractiveSynth sub;
    sub.filter.cutoff = 8000.0f;
    p.source.data = sub;

    return p;
}

/// Create a profile with FM source
TimbreProfile make_fm_profile() {
    TimbreProfile p;
    p.id = TimbreProfileId{2};
    p.part_id = PartId{2};
    p.name = "FM Bell";

    FMSynth fm;
    p.source.data = fm;

    return p;
}

}  // namespace

// =============================================================================
// Device loading
// =============================================================================

TEST_CASE("FMAL002A: SubtractiveSynth maps to Analog", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->devices_created == 1);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "load_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Analog") found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("FMAL002A: FMSynth maps to Operator", "[ableton][timbre]") {
    auto profile = make_fm_profile();
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "load_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Operator") found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("FMAL002A: explicit device name overrides default", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    profile.rendering.device_type.tag = DeviceTypeTag::NativeAbleton;
    profile.rendering.device_type.device_name = "Wavetable";
    CommandBuffer buf;

    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    bool found = false;
    for (const auto* e : calls) {
        if (e->request.property_or_method == "load_device") {
            auto* arg = std::get_if<std::string>(&e->request.args[0]);
            if (arg && *arg == "Wavetable") found = true;
        }
    }
    CHECK(found);
}

// =============================================================================
// Effect chain
// =============================================================================

TEST_CASE("FMAL002A: effects inserted in order", "[ableton][timbre]") {
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
    CHECK(r->effects_inserted == 2);
}

TEST_CASE("FMAL002A: disabled effects are skipped", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();

    Effect reverb;
    reverb.id = EffectId{1};
    reverb.parameters = ReverbEffect{};
    reverb.enabled = false;
    profile.insert_chain.effects.push_back(reverb);

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->effects_inserted == 0);
}

// =============================================================================
// Parameter mapping
// =============================================================================

TEST_CASE("FMAL002A: parameter map creates set_property calls", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    DeviceParameter dp;
    dp.device_index = 0;
    dp.parameter_name = "Filter Freq";
    dp.range_min = 20.0f;
    dp.range_max = 20000.0f;
    profile.rendering.parameter_map["source.filter.cutoff"] = dp;

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->parameters_mapped == 1);
}

// =============================================================================
// Preset
// =============================================================================

TEST_CASE("FMAL002A: preset path is set when specified", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    profile.rendering.preset_path = "Presets/Lead/BrightLead.adv";

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found = false;
    for (const auto* e : sets) {
        if (e->request.property_or_method == "preset") found = true;
    }
    CHECK(found);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("FMAL002A: automation lanes created", "[ableton][timbre]") {
    auto profile = make_subtractive_profile();
    TimbreAutomation ta;
    ta.parameter_path = "source.filter.cutoff";
    profile.parameter_automation.push_back(ta);

    CommandBuffer buf;
    auto r = compile_timbre_to_ableton(profile, 0, buf);
    REQUIRE(r.has_value());
    CHECK(r->automation_lanes == 1);
}
