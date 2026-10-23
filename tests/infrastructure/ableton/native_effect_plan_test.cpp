#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/native_effect_plan.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
namespace {
TimbreProfile authored_profile() {
    auto result = create_timbre_profile(TimbreProfileId{17}, PartId{23}, "Literal owned source");
    result.rendering.device_type.device_name = "Drift";
    result.insert_chain.effects.push_back(
        {EffectId{31}, EQEffect{{{733.0f, -5.0f, 1.75f, EQBandType::Peak}}}, true, 1.0f});
    return result;
}
MixGraph authored_mix() {
    auto result = create_mix_graph(MixGraphId{41}, {PartId{23}});
    auto& channel = result.channels.front();
    channel.input_trim = -7.5f;
    MixEQ eq;
    eq.bands = {{200.0f, 3.0f, 0.75f, MixEQBandType::LowShelf, std::nullopt},
                {8000.0f, -2.0f, 1.0f, MixEQBandType::HighShelf, std::nullopt}};
    channel.insert_chain.effects.emplace_back(MixEffectId{51}, eq);
    MixStereoProcessor stereo;
    stereo.width = 1.25f;
    channel.insert_chain.effects.emplace_back(MixEffectId{52}, stereo);
    return result;
}
const ManagedDevicePhysicalIntent& physical(const NativeEffectPlanEntry& entry,
                                            std::string_view capability) {
    for (const auto& item : entry.physical_intents)
        if (item.capability_id == capability) return item;
    FAIL("Literal expected capability missing");
    return entry.physical_intents.front();
}
std::string mode(const NativeEffectPlanEntry& entry, std::string_view capability) {
    for (const auto& item : entry.setup_modes)
        if (item.capability_id == capability) return item.label;
    return {};
}
} // namespace

TEST_CASE("Native effects preserve real units and authored signal order "
          "independently of selection order",
          "[ableton][native-effect-plan]") {
    auto profile = authored_profile();
    auto mix = authored_mix();
    const auto channel = mix.channels.front().id;
    const std::array selections{
        NativeEffectSelection{NativeEffectSelectionKind::MixEffect, 52},
        NativeEffectSelection{NativeEffectSelectionKind::MixInputTrim, channel.value},
        NativeEffectSelection{NativeEffectSelectionKind::TimbreEffect, 31},
        NativeEffectSelection{NativeEffectSelectionKind::MixEffect, 51}};
    const auto plan = plan_native_effects(profile, &mix, channel, selections);
    REQUIRE(plan);
    REQUIRE(plan->entries.size() == 4);
    CHECK(plan->entries[0].device_key == "timbre_11_effect_1f");
    CHECK(plan->entries[0].desired_chain_index == 1);
    CHECK(physical(plan->entries[0], "eq8.band.1.frequency").target == 733.0);
    CHECK(physical(plan->entries[0], "eq8.band.1.gain").target == -5.0);
    CHECK(physical(plan->entries[0], "eq8.band.1.q").target == 1.75);
    CHECK(mode(plan->entries[0], "eq8.band.1.type") == "Bell");
    CHECK(mode(plan->entries[0], "eq8.band.2.enabled") == "Off");
    CHECK(mode(plan->entries[0], "eq8.band.8.enabled") == "Off");
    REQUIRE(plan->entries[0].setup_physical_intents.size() == 2);
    CHECK(plan->entries[0].setup_physical_intents[0].capability_id == "eq8.scale");
    CHECK(plan->entries[0].setup_physical_intents[0].target == 100.0);
    CHECK(plan->entries[0].enable_modes[0].capability_id == "eq8.enabled");
    CHECK(physical(plan->entries[1], "utility.gain").target == -7.5);
    CHECK(physical(plan->entries[1], "utility.width").target == 100.0);
    CHECK(plan->entries[2].device_key == "mix_29_effect_33");
    CHECK(mode(plan->entries[2], "eq8.band.1.type") == "Low Shelf");
    CHECK(mode(plan->entries[2], "eq8.band.2.type") == "High Shelf");
    CHECK(physical(plan->entries[3], "utility.width").target == 125.0);
    CHECK(physical(plan->entries[3], "utility.gain").target == 0.0);
    CHECK(plan->entries[3].desired_chain_index == 4);
    CHECK(plan->retained_profile == timbre_to_json(profile));
    CHECK(*plan->retained_mix == mix_to_json(mix));
    CHECK_FALSE(plan->dsp_equivalence_qualified);
    CHECK_FALSE(plan->host_qualified);
    CHECK_FALSE(plan->residuals.empty());
    const auto encoded = native_effect_plan_to_json(*plan);
    CHECK(encoded.at("entries")[0].at("setup_properties")[0].at("label") == "Stereo");
    CHECK(encoded.at("retained_mix") == mix_to_json(mix));
}

TEST_CASE("Native effects preserve both legacy mapping properties without "
          "applying their ranges",
          "[ableton][native-effect-plan][mapping]") {
    auto profile = authored_profile();
    DeviceParameter binding;
    binding.device_index = 1;
    binding.parameter_name = "1 Frequency A";
    binding.range_min = -3;
    binding.range_max = 7;
    binding.source_min = 0;
    binding.source_max = 30000;
    binding.value_property = DeviceParameterValueProperty::DisplayValue;
    profile.rendering.parameter_map["insert_chain.effects[0].bands[0].frequency"] = binding;
    const std::array selections{NativeEffectSelection{NativeEffectSelectionKind::TimbreEffect, 31}};
    const auto original = timbre_to_json(profile);
    const auto plan = plan_native_effects(profile, nullptr, std::nullopt, selections);
    REQUIRE(plan);
    CHECK(physical(plan->entries[0], "eq8.band.1.frequency").target == 733.0);
    CHECK(plan->entries[0].original_bindings == original.at("rendering").at("param_map"));
    CHECK(plan->retained_profile == original);
    profile.rendering.parameter_map.begin()->second.parameter_name = "2 Frequency A";
    CHECK_FALSE(plan_native_effects(profile, nullptr, std::nullopt, selections));
}

TEST_CASE("Native effect wet endpoints retain bypass slots and fractional wet "
          "declines",
          "[ableton][native-effect-plan][wet]") {
    auto profile = authored_profile();
    const std::array selections{NativeEffectSelection{NativeEffectSelectionKind::TimbreEffect, 31}};
    for (const float wet : {0.0f, 1.0f}) {
        profile.insert_chain.effects[0].mix = wet;
        const auto plan = plan_native_effects(profile, nullptr, std::nullopt, selections);
        REQUIRE(plan);
        CHECK(plan->entries[0].final_modes.empty() == (wet == 1.0f));
    }
    profile.insert_chain.effects[0].mix = 0.5f;
    const auto fractional = plan_native_effects(profile, nullptr, std::nullopt, selections);
    REQUIRE_FALSE(fractional);
    CHECK(fractional.error().reason == "UnsupportedWet");
    profile.insert_chain.effects[0].mix = 1;
    profile.insert_chain.bypass_all = true;
    const auto bypass = plan_native_effects(profile, nullptr, std::nullopt, selections);
    REQUIRE(bypass);
    REQUIRE(bypass->entries[0].final_modes.size() == 1);
    CHECK(bypass->entries[0].final_modes[0].label == "Off");
}

TEST_CASE("Native effect plans decline missing earlier stages and undeclared "
          "cut or coupled EQ semantics",
          "[ableton][native-effect-plan][decline]") {
    auto profile = authored_profile();
    auto mix = authored_mix();
    const auto channel = mix.channels[0].id;
    const std::array width_only{NativeEffectSelection{NativeEffectSelectionKind::MixEffect, 52}};
    const auto gap = plan_native_effects(profile, &mix, channel, width_only);
    REQUIRE_FALSE(gap);
    CHECK(gap.error().reason == "UnrealizedEarlierStage");
    const std::array eq_only{NativeEffectSelection{NativeEffectSelectionKind::TimbreEffect, 31}};
    std::get<EQEffect>(profile.insert_chain.effects[0].parameters).bands[0].band_type =
        EQBandType::LowCut;
    const auto cut = plan_native_effects(profile, nullptr, std::nullopt, eq_only);
    REQUIRE_FALSE(cut);
    CHECK(cut.error().reason == "UnsupportedEQ");
    profile.insert_chain.effects.clear();
    mix.channels[0].input_trim = 0;
    const std::array mix_eq{NativeEffectSelection{NativeEffectSelectionKind::MixEffect, 51}};
    std::get<MixEQ>(mix.channels[0].insert_chain.effects[0].parameters).auto_gain = true;
    const auto coupled = plan_native_effects(profile, &mix, channel, mix_eq);
    REQUIRE_FALSE(coupled);
    CHECK(coupled.error().reason == "UnsupportedEQ");
    CHECK_FALSE(plan_native_effects(profile, &mix, ChannelStripId{999}, mix_eq));
}
