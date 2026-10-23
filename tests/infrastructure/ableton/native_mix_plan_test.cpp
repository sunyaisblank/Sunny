#include <catch2/catch_test_macros.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/infrastructure/ableton/native_mix_plan.hpp>
using namespace sunny::core;
using namespace sunny::infrastructure;

TEST_CASE(
    "Native owning Mix volume reads literal effective relative dB and keeps input trim distinct",
    "[ableton][native-mix-plan]") {
    auto mix = create_mix_graph(MixGraphId{21}, {PartId{31}, PartId{32}});
    mix.channels[0].input_trim = -7.5f;
    mix.channels[0].fader.level_db = 5.0f;
    mix.channels[1].fader.level_db = -3.0f;
    mix.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, mix.channels[1].id, "3 dB below", {}},
        -3.0f};
    const auto original = mix_to_json(mix);
    const auto plan = plan_native_mix_fader(mix, mix.channels[0].id, PartId{31}, 0.05);
    REQUIRE(plan);
    CHECK(plan->target_db == -6.0);
    CHECK(plan->tolerance_db == 0.05);
    CHECK(plan->source_json_pointer == "/channels/0/fader/relative");
    CHECK(plan->selected_resolution.status == FaderLevelResolutionStatus::Resolved);
    CHECK(plan->retained_mix == original);
    CHECK(mix_to_json(mix) == original);
    CHECK(std::ranges::any_of(plan->residuals, [](const auto& item) {
        return item.document_pointer == "/channels/0/input_trim";
    }));
    CHECK(std::ranges::any_of(plan->residuals, [](const auto& item) {
        return item.document_pointer == "/channels/0/fader/level_db";
    }));
    const auto encoded = native_mix_fader_plan_to_json(*plan);
    CHECK(encoded.at("target") == -6.0);
    CHECK(encoded.at("selected_resolution").at("resolved_level_db").is_number_float());
    CHECK(encoded.at("selected_resolution").at("resolved_level_db") == -6.0);
    CHECK_FALSE(encoded.at("dsp_equivalence_qualified").get<bool>());
}

TEST_CASE("Native owning Mix fader does not substitute explicit fallback for unmeasured LUFS",
          "[ableton][native-mix-plan][unavailable]") {
    auto mix = create_mix_graph(MixGraphId{21}, {PartId{31}, PartId{32}});
    mix.channels[0].fader.level_db = -6.0f;
    mix.channels[0].fader.relative_level =
        RelativeLevel{LevelReference{LevelReferenceType::MasterTarget, -16.0f, {}, {}, {}}, 0.0f};
    mix.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, mix.channels[0].id, {}, {}}, -3.0f};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto plan = plan_native_mix_fader(mix, mix.channels[i].id, PartId{31 + i}, 0.0);
        REQUIRE_FALSE(plan);
        CHECK(plan.error().reason == "UnresolvedLoudness");
    }
}

TEST_CASE("Native owning Mix volume preserves an above-host target for actual formatter admission",
          "[ableton][native-mix-plan][domain]") {
    auto mix = create_mix_graph(MixGraphId{21}, {PartId{31}});
    mix.channels[0].fader.level_db = 7.0f;
    const auto plan = plan_native_mix_fader(mix, mix.channels[0].id, PartId{31}, 0.0);
    REQUIRE(plan);
    CHECK(plan->target_db == 7.0);
    CHECK(plan->source_json_pointer == "/channels/0/fader/level_db");
    CHECK_FALSE(plan_native_mix_fader(mix, mix.channels[0].id, PartId{32}, 0.0));
    CHECK_FALSE(plan_native_mix_fader(mix, ChannelStripId{999}, PartId{31}, 0.0));
    CHECK_FALSE(plan_native_mix_fader(mix, mix.channels[0].id, PartId{31}, -1.0));
    mix.channels[0].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, mix.channels[0].id, {}, {}}, 1.0f};
    const auto cyclic = plan_native_mix_fader(mix, mix.channels[0].id, PartId{31}, 0.0);
    REQUIRE_FALSE(cyclic);
    CHECK(cyclic.error().reason == "InvalidMix");
}
