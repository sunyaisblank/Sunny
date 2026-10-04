#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/native_timbre_plan.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using nlohmann::json;

namespace {
TimbreProfile profile() {
    auto result = create_timbre_profile(TimbreProfileId{17}, PartId{23}, "Explicit native voice");
    result.rendering.device_type.device_name = "Drift";
    auto& source = std::get<SubtractiveSynth>(result.source.data);
    source.filter.cutoff = 1250.0f;
    source.amplifier.stages = {{12.5f, 1.0f, EnvelopeCurve::Linear, 1.0f},
                               {275.0f, .625f, EnvelopeCurve::Exponential, .5f},
                               {640.0f, 0.0f, EnvelopeCurve::SCurve, 1.0f}};
    for (const auto& [path, native] :
         std::array{std::pair{"source.filter.cutoff", "LP Freq"},
                    std::pair{"source.amplifier.stages[0].duration", "Env 1 Attack"},
                    std::pair{"source.amplifier.stages[1].duration", "Env 1 Decay"},
                    std::pair{"source.amplifier.stages[2].duration", "Env 1 Release"}}) {
        DeviceParameter binding;
        binding.parameter_name = native;
        binding.range_min = -3.0f;
        binding.range_max = 7.0f;
        binding.source_min = 0.0f;
        binding.source_max = 30000.0f;
        result.rendering.parameter_map.emplace(path, binding);
    }
    result.rendering.parameter_map.at("source.amplifier.stages[1].duration").value_property =
        DeviceParameterValueProperty::DisplayValue;
    return result;
}

bool residual(const NativeTimbrePlan& plan, std::string_view path) {
    return std::ranges::any_of(plan.residuals,
                               [&](const auto& item) { return item.document_pointer == path; });
}
} // namespace

TEST_CASE(
    "Native Timbre selections read literal authored Hz and ms while preserving legacy mappings",
    "[ableton][native-timbre-plan]") {
    auto authored = profile();
    const auto before = timbre_to_json(authored);
    const std::array selections{
        NativeTimbreSelection{"source.amplifier.stages[2].duration", "drift.env.1.release", .5},
        NativeTimbreSelection{"source.filter.cutoff", "drift.lp.frequency", 1.0},
        NativeTimbreSelection{"source.amplifier.stages[0].duration", "drift.env.1.attack", .1},
        NativeTimbreSelection{"source.amplifier.stages[1].duration", "drift.env.1.decay", .25}};
    const auto planned = plan_native_timbre(authored, selections);
    REQUIRE(planned);
    REQUIRE(planned->intents.size() == 4);
    CHECK(planned->profile_id.value == 17);
    CHECK(planned->part_id.value == 23);
    CHECK(planned->intents[0].target == 640.0);
    CHECK(planned->intents[1].target == 1250.0);
    CHECK(planned->intents[2].target == 12.5);
    CHECK(planned->intents[3].target == 275.0);
    CHECK(planned->intents[1].unit == LiveNativePhysicalUnit::Hertz);
    CHECK(planned->intents[0].unit == LiveNativePhysicalUnit::Milliseconds);
    CHECK(planned->intents[1].original_binding.range_min == -3.0f);
    CHECK(planned->intents[1].original_binding.range_max == 7.0f);
    CHECK(planned->intents[3].original_binding.value_property ==
          DeviceParameterValueProperty::DisplayValue);
    CHECK(planned->retained_profile == before);
    CHECK(timbre_to_json(authored) == before);
    CHECK(planned->native_knob_only);
    CHECK_FALSE(planned->host_qualified);
    CHECK_FALSE(planned->dsp_equivalence_qualified);
    const auto encoded = native_timbre_plan_to_json(*planned);
    CHECK(encoded.at("selection_semantics") ==
          "ReadAuthoredPhysicalValueWithoutApplyingLegacyMapping");
    CHECK(encoded.at("retained_profile") == before);
    CHECK(encoded.at("intents")[1].at("original_binding") ==
          before.at("rendering").at("param_map").at("source.filter.cutoff"));
    CHECK(encoded.at("intents")[0].at("source_path") == selections[0].source_path);
    CHECK(encoded.at("intents")[0].at("tolerance") == .5);
}

TEST_CASE("Native Timbre plan lists actual residual source and effect codec leaves",
          "[ableton][native-timbre-plan][coverage]") {
    auto authored = profile();
    EQEffect effect;
    effect.bands = {{733.0f, -5.0f, 1.75f, EQBandType::Peak}};
    REQUIRE(add_effect(authored, {EffectId{31}, effect, true, .5f}));
    const std::array selections{
        NativeTimbreSelection{"source.filter.cutoff", "drift.lp.frequency", 1.0}};
    const auto planned = plan_native_timbre(authored, selections);
    REQUIRE(planned);
    CHECK_FALSE(residual(*planned, "/source/data/filter/cutoff"));
    CHECK(residual(*planned, "/source/data/filter/resonance"));
    CHECK(residual(*planned, "/source/data/amplifier/stages/0/dur"));
    CHECK(residual(*planned, "/source/data/amplifier/stages/0/curve"));
    CHECK(residual(*planned, "/source/data/oscillators/0/phase"));
    CHECK(residual(*planned, "/insert_chain/effects/0/mix"));
    CHECK(std::ranges::any_of(planned->residuals, [](const auto& item) {
        return item.document_pointer.starts_with("/insert_chain/effects/0/params/");
    }));
    for (const auto& item : planned->residuals) {
        CHECK_FALSE(item.reason.empty());
        CHECK(planned->retained_profile.contains(json::json_pointer{item.document_pointer}));
    }
}

TEST_CASE("Native Timbre planning requires explicit Drift and exact authored native binding",
          "[ableton][native-timbre-plan][decline]") {
    auto authored = profile();
    const std::array selections{
        NativeTimbreSelection{"source.filter.cutoff", "drift.lp.frequency", 1.0}};
    authored.rendering.device_type.device_name.clear();
    auto planned = plan_native_timbre(authored, selections);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::UnsupportedDevice);
    authored.rendering.device_type.device_name = "Drift";
    authored.rendering.device_type.tag = DeviceTypeTag::Plugin;
    CHECK_FALSE(plan_native_timbre(authored, selections));
    authored.rendering.device_type.tag = DeviceTypeTag::NativeAbleton;
    authored.rendering.parameter_map.erase("source.filter.cutoff");
    planned = plan_native_timbre(authored, selections);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::MissingBinding);
    auto binding = DeviceParameter{};
    binding.parameter_name = "HP Freq";
    binding.source_max = 30000.0f;
    authored.rendering.parameter_map["source.filter.cutoff"] = binding;
    planned = plan_native_timbre(authored, selections);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::BindingMismatch);
}

TEST_CASE("Native Timbre selections reject duplicates unknown pairs and invalid tolerances",
          "[ableton][native-timbre-plan][decline]") {
    const auto authored = profile();
    for (const auto& selections : std::array<std::vector<NativeTimbreSelection>, 4>{
             std::vector<NativeTimbreSelection>{{"source.filter.cutoff", "utility.gain", 1.0}},
             std::vector<NativeTimbreSelection>{
                 {"source.filter.cutoff", "drift.lp.frequency", -1.0}},
             std::vector<NativeTimbreSelection>{{"source.filter.cutoff",
                                                 "drift.lp.frequency",
                                                 std::numeric_limits<double>::quiet_NaN()}},
             std::vector<NativeTimbreSelection>{
                 {"source.filter.cutoff", "drift.lp.frequency", 1.0},
                 {"source.filter.cutoff", "drift.lp.frequency", 1.0}}}) {
        const auto planned = plan_native_timbre(authored, selections);
        REQUIRE_FALSE(planned);
        CHECK(planned.error().reason == NativeTimbrePlanFailure::InvalidSelection);
    }
}

TEST_CASE(
    "Native ADSR time selection rejects an extra hold stage without rewriting authored timing",
    "[ableton][native-timbre-plan][decline]") {
    auto authored = profile();
    auto& amp = std::get<SubtractiveSynth>(authored.source.data).amplifier;
    amp.stages.insert(amp.stages.begin() + 1, {33.0f, 1.0f, EnvelopeCurve::Step, 1.0f});
    const auto before = timbre_to_json(authored);
    const std::array selection{
        NativeTimbreSelection{"source.amplifier.stages[1].duration", "drift.env.1.decay", 1.0}};
    const auto planned = plan_native_timbre(authored, selection);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::UnsupportedEnvelope);
    CHECK(timbre_to_json(authored) == before);
}

TEST_CASE("Native Timbre plan retains positive authored identity without remapping",
          "[ableton][native-timbre-plan][decline]") {
    auto authored = profile();
    const std::array selection{
        NativeTimbreSelection{"source.filter.cutoff", "drift.lp.frequency", 1.0}};
    authored.id = TimbreProfileId{0};
    auto planned = plan_native_timbre(authored, selection);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::InvalidProfile);
    authored.id = TimbreProfileId{17};
    authored.part_id = PartId{0};
    planned = plan_native_timbre(authored, selection);
    REQUIRE_FALSE(planned);
    CHECK(planned.error().reason == NativeTimbrePlanFailure::InvalidProfile);
}
