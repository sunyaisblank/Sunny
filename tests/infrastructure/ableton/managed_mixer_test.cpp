#include "managed_mixer_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/native_mix_plan.hpp>
using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
ManagedBridgeContext context{"bridge_a", "document_a"};
ManagedBindingReceipt binding(const json& preview) {
    return {context,
            "project_a",
            "part_a",
            preview.at("preview").at("before").at("binding_observation")};
}
ManagedStaticMixerDesired four() {
    return {ManagedMixerVolumeIntent{-6.0, 0.0}, -.25, true, true};
}
json payload(const LomRequest& value) {
    return std::get<json>(value.args.at(0));
}
void rehash_after(json& result) {
    auto& after = result.at("mixer_update").at("after").at("binding_observation");
    after["content_fingerprint"] = *managed_detail::managed_digest(after.at("manifest"));
    for (const auto& [key, value] : after.items())
        result[key] = value;
}
} // namespace
TEST_CASE("Managed static Mixer parses actual source preview and fences "
          "precise current grants",
          "[ableton][managed_mixer]") {
    auto fixture = test::managed_mixer_fixture();
    const auto& source = fixture.at("adoption_preview");
    auto request =
        make_managed_static_mixer_preview_request(context, binding(source), four(), true);
    REQUIRE(request);
    REQUIRE(request->property_or_method == managed_mixer_detail::preview_method);
    auto preview = parse_managed_static_mixer_preview(*request, context, source);
    REQUIRE(preview);
    CHECK(preview->approved_preview.at("candidates").at("volume").at("internal_value") == .5);
    CHECK(preview->approved_preview.at("candidates").at("volume").at("display") == "-6.00 dB");
    CHECK(preview->approved_preview.at("candidates")
              .at("volume")
              .at("current_display")
              .at("display") == "-25.66 dB");
    CHECK(preview->approved_preview.at("current_authority_domains").empty());
    CHECK_FALSE(make_managed_static_mixer_request(context, "grant", *preview, false));
    auto grant = make_managed_static_mixer_request(context, "grant", *preview, true);
    REQUIRE(grant);
    CHECK(payload(*grant) == fixture.at("adoption_journal").at("request"));
    CHECK(
        managed_mixer_detail::result_matches_request(managed_mixer_detail::adopt_method,
                                                     payload(*grant),
                                                     fixture.at("adoption_journal").at("result")));
    CHECK(managed_mixer_detail::acknowledged_native_start_valid(
        managed_mixer_detail::adopt_method,
        payload(*grant),
        fixture.at("adoption_journal").at("result"),
        false));
    CHECK_FALSE(managed_mixer_detail::acknowledged_native_start_valid(
        managed_mixer_detail::adopt_method,
        payload(*grant),
        fixture.at("adoption_journal").at("result"),
        true));
}
TEST_CASE("Managed static Mixer validates real four-role ACK and zero-write no-op",
          "[ableton][managed_mixer]") {
    const auto fixture = test::managed_mixer_fixture();
    for (const auto* prefix : {"update", "noop"}) {
        const auto& source = fixture.at(std::string(prefix) + "_preview");
        const auto& journal = fixture.at(std::string(prefix) + "_journal");
        const auto& desired = journal.at("request").at("desired");
        ManagedStaticMixerDesired typed{
            ManagedMixerVolumeIntent{desired.at("volume").at("target"),
                                     desired.at("volume").at("tolerance")},
            desired.at("pan"),
            desired.at("mute"),
            desired.at("solo")};
        auto request =
            make_managed_static_mixer_preview_request(context, binding(source), typed, false);
        REQUIRE(request);
        auto preview = parse_managed_static_mixer_preview(*request, context, source);
        REQUIRE(preview);
        auto update = make_managed_static_mixer_request(context, prefix, *preview, true);
        REQUIRE(update);
        CHECK(payload(*update) == journal.at("request"));
        CHECK(managed_mixer_detail::result_matches_request(
            managed_mixer_detail::update_method, payload(*update), journal.at("result")));
        const bool started = std::string_view(prefix) == "update";
        CHECK(managed_mixer_detail::acknowledged_native_start_valid(
            managed_mixer_detail::update_method, payload(*update), journal.at("result"), started));
        CHECK_FALSE(managed_mixer_detail::acknowledged_native_start_valid(
            managed_mixer_detail::update_method, payload(*update), journal.at("result"), !started));
    }
}
TEST_CASE("Managed static Mixer rejects rehashed malformed descriptors masks "
          "and candidate context",
          "[ableton][managed_mixer]") {
    const auto fixture = test::managed_mixer_fixture();
    const auto original = fixture.at("adoption_preview");
    auto request =
        make_managed_static_mixer_preview_request(context, binding(original), four(), true);
    REQUIRE(request);
    for (int fault = 0; fault < 9; ++fault) {
        auto bad = original;
        auto& p = bad.at("preview");
        if (fault == 0) p["schema_version"] = 1.0;
        if (fault == 1) p["before"]["mixer_capture"]["parameters"][0]["descriptor"]["value"] = 0;
        if (fault == 2) p["selected_domains"] = {"volume", "pan", "mute", "solo", "send"};
        if (fault == 3) p["selected_domains"] = {"solo", "volume", "pan", "mute"};
        if (fault == 4) p["scope"]["set_wide_audible_effect"] = false;
        if (fault == 5) p["before"]["selected_envelopes"]["pan"] = true;
        if (fault == 6) p["candidates"]["volume"]["current_display"]["display"] = "-25,66 dB";
        if (fault == 7) p["before"]["solo_cohort"][0]["solo"] = true;
        if (fault == 8) p["before"]["version"] = {12, 5, 0};
        bad["preview_fingerprint"] = *managed_detail::managed_digest(p);
        CHECK_FALSE(parse_managed_static_mixer_preview(*request, context, bad));
    }
}
TEST_CASE("Managed static Mixer closes untouched controls native notes and "
          "physical readback",
          "[ableton][managed_mixer]") {
    const auto fixture = test::managed_mixer_fixture();
    const auto& journal = fixture.at("update_journal");
    const auto& request = journal.at("request");
    for (int fault = 0; fault < 9; ++fault) {
        auto result = journal.at("result");
        auto& item = result.at("mixer_update");
        if (fault == 0) item["readback"]["volume"]["display"] = "-5.99 dB";
        if (fault == 1) item["readback"]["volume"]["display_value"] = -6;
        if (fault == 2) item["readback"]["pan"]["internal_value"] = -.5;
        if (fault == 3) item["readback"]["mute"]["track_activator_value"] = 1.0;
        if (fault == 4) item["returned_fields"] = {"volume", "pan", "solo", "mute"};
        if (fault == 5) item["after"]["solo_cohort"].back()["solo"] = true;
        if (fault == 6)
            item["after"]["binding_observation"]["manifest"]["mixer"]["sends"][0]["value"] = .5;
        if (fault == 7)
            item["after"]["binding_observation"]["manifest"]["clip"]["end_marker"] = 8.0;
        if (fault == 8) item["after"]["track_context"]["back_to_arranger"] = true;
        if (fault >= 5) rehash_after(result);
        CHECK_FALSE(managed_mixer_detail::result_matches_request(
            managed_mixer_detail::update_method, request, result));
    }
}
TEST_CASE("Managed static Mixer finite mask rejects arbitrary selector or "
          "missing owner",
          "[ableton][managed_mixer]") {
    const auto fixture = test::managed_mixer_fixture();
    const auto& source = fixture.at("adoption_preview");
    auto request =
        make_managed_static_mixer_preview_request(context, binding(source), four(), true);
    REQUIRE(request);
    auto bad = payload(*request);
    bad["desired"]["pan"] = 2.0;
    CHECK_FALSE(managed_mixer_detail::request_valid(managed_mixer_detail::preview_method, bad));
    bad = payload(*request);
    bad["selected_domains"] = {"volume", "volume"};
    CHECK_FALSE(managed_mixer_detail::request_valid(managed_mixer_detail::preview_method, bad));
    CHECK_FALSE(make_managed_static_mixer_preview_request(context, binding(source), {}, true));
    auto wrong = context;
    wrong.document_token = "next";
    CHECK_FALSE(make_managed_static_mixer_preview_request(wrong, binding(source), four(), true));
}
TEST_CASE("Managed static Mixer retains literal clamped and throwing native "
          "phase evidence",
          "[ableton][managed_mixer]") {
    const auto fixture = test::managed_mixer_fixture();
    for (const auto* failure : {"clamp_journal", "throw_journal"}) {
        const auto& journal = fixture.at(failure);
        CHECK(journal.at("outcome") == "indeterminate");
        CHECK(journal.at("native_mutation_started") == true);
        REQUIRE(managed_mixer_detail::partial_valid(journal.at("request"), journal));
        CHECK(journal.at("mixer_progress").at("started_fields") == json::array({"volume"}));
        CHECK(journal.at("mixer_partial")
                  .at("after")
                  .at("binding_observation")
                  .at("manifest")
                  .at("mixer")
                  .at("volume")
                  .at("value") == (std::string_view(failure) == "clamp_journal" ? .25 : .375));
        auto bad = journal;
        bad["mixer_progress"]["started_fields"] = {"pan"};
        CHECK_FALSE(managed_mixer_detail::partial_valid(journal.at("request"), bad));
        bad = journal;
        bad["mixer_partial"]["observed_after_available"] = false;
        CHECK_FALSE(managed_mixer_detail::partial_valid(journal.at("request"), bad));
        bad = journal;
        bad["mixer_partial"]["after"] = nullptr;
        bad["mixer_partial"]["observed_after_available"] = false;
        CHECK(managed_mixer_detail::partial_valid(journal.at("request"), bad));
        bad["mixer_partial"]["desired"]["mute"] = false;
        CHECK_FALSE(managed_mixer_detail::partial_valid(journal.at("request"), bad));
    }
}
TEST_CASE("Managed static Mixer rejects an oversized complete ACK before an "
          "owning fence",
          "[ableton][managed_mixer][capacity]") {
    auto source = test::managed_mixer_fixture().at("adoption_preview");
    auto& preview = source.at("preview");
    auto& cohort = preview.at("before").at("solo_cohort");
    const auto selected = cohort.at(0);
    const auto returned = cohort.back();
    cohort = json::array({selected});
    for (int index = 1; index < 4095; ++index)
        cohort.push_back({{"kind", "track"},
                          {"index", index},
                          {"name", std::string(1024, 'x')},
                          {"mute", false},
                          {"solo", false},
                          {"muted_via_solo", false}});
    cohort.push_back(returned);
    source["preview_fingerprint"] = *managed_detail::managed_digest(preview);
    const auto bytes = managed_detail::json_wire_bound(source);
    REQUIRE(bytes);
    CHECK(*bytes < managed_detail::managed_response_limit);
    auto request =
        make_managed_static_mixer_preview_request(context, binding(source), four(), true);
    REQUIRE(request);
    auto parsed = parse_managed_static_mixer_preview(*request, context, source);
    REQUIRE(parsed);
    auto prepared = make_managed_static_mixer_request(context, "oversized", *parsed, true);
    REQUIRE_FALSE(prepared);
    CHECK(prepared.error() == sunny::core::ErrorCode::ManagedReplyCapacityExceeded);
}
TEST_CASE("Owning Mix static planner preserves effective level algebra and "
          "every residual obligation",
          "[ableton][native_mix_plan]") {
    using namespace sunny::core;
    auto graph = create_mix_graph(MixGraphId{71}, {PartId{81}, PartId{82}});
    graph.channels[0].fader.level_db = -3.0f;
    graph.channels[1].fader.level_db = 5.0f;
    graph.channels[1].fader.relative_level = RelativeLevel{
        LevelReference{LevelReferenceType::Channel, -14.0f, graph.channels[0].id, "3dB below", {}},
        -3.0f};
    graph.channels[1].spatial.pan = -.25f;
    graph.channels[1].spatial.depth = .5f;
    graph.channels[1].input_trim = -7.5f;
    graph.channels[1].mute = true;
    graph.channels[1].solo = true;
    NativeMixStaticSelection selected{true, true, true, true, 0.0};
    auto plan = plan_native_mix_static(graph, graph.channels[1].id, PartId{82}, selected);
    REQUIRE(plan);
    auto encoded = native_mix_static_plan_to_json(*plan);
    CHECK(encoded.at("desired") == json{{"volume", {{"target", -6.0}, {"tolerance", 0.0}}},
                                        {"pan", -.25},
                                        {"mute", true},
                                        {"solo", true}});
    CHECK(plan->source_json_pointers.size() == 4);
    CHECK(plan->retained_mix.at("channels")[1].at("input_trim") == -7.5);
    CHECK(std::ranges::any_of(plan->residuals, [](const auto& r) {
        return r.document_pointer == "/channels/1/spatial/pan_law";
    }));
    CHECK(std::ranges::any_of(plan->residuals, [](const auto& r) {
        return r.document_pointer == "/channels/1/spatial/depth";
    }));
    CHECK(std::ranges::any_of(plan->residuals, [](const auto& r) {
        return r.document_pointer == "/channels/1/fader/level_db";
    }));
    CHECK_FALSE(encoded.at("dsp_equivalence_qualified").get<bool>());
    CHECK_FALSE(plan_native_mix_static(graph, graph.channels[1].id, PartId{81}, selected));
    CHECK_FALSE(plan_native_mix_static(
        graph, graph.channels[1].id, PartId{82}, {false, false, false, false, 0.0}));
}
