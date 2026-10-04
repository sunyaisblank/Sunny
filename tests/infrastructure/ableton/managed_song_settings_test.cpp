#include "managed_song_settings_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/ableton/detail/managed_song_settings.hpp>
using namespace sunny::core;
using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext context{"bridge_a", "document_a"};
Result<LomRequest> preview_request(const json& value) {
    const auto& approved = value.at("preview");
    const ManagedBindingReceipt binding{context, "project_a", "part_a", value.at("observation")};
    const auto& d = approved.at("desired");
    auto request = make_managed_song_settings_preview_request(
        context,
        binding,
        {d.at("tempo"), d.at("signature_numerator"), d.at("signature_denominator")});
    if (request) {
        // The literal Python fixture approves integer120 for the no-op. Preserve
        // that exact typed input when testing its digest, rather than converting it.
        auto payload = std::get<json>(request->args.at(0));
        payload["desired"] = d;
        request->args.at(0) = std::move(payload);
    }
    return request;
}
json intent(const json& value) {
    const auto request = preview_request(value);
    if (!request) throw std::runtime_error("literal preview request failed");
    const auto parsed = parse_managed_song_settings_preview(*request, context, value);
    if (!parsed) throw std::runtime_error("literal preview failed");
    const auto apply = make_managed_song_settings_request(context, "song_a", *parsed);
    if (!apply) throw std::runtime_error("literal apply factory failed");
    return std::get<json>(apply->args.at(0));
}
void sign_preview(json& value) {
    value["preview_fingerprint"] = *managed_detail::managed_digest(value.at("preview"));
}
Score score(double bpm = 60.0, int numerator = 6, int denominator = 8) {
    ScoreSpec spec;
    spec.title = "Explicit Song scalars";
    spec.total_bars = 2;
    spec.bpm = bpm;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = numerator;
    spec.time_sig_den = denominator;
    PartDefinition part;
    part.name = "Clarinet";
    part.abbreviation = "Cl.";
    part.instrument_type = InstrumentType::Clarinet;
    spec.parts.push_back(part);
    const auto result = create_score(spec);
    if (!result) throw std::runtime_error("literal Score fixture failed");
    return *result;
}
} // namespace
TEST_CASE("Song preview closes literal source capture and explicit Set-wide intent",
          "[ableton][managed-song-settings]") {
    const auto value = song_settings_test_fixture::normal_preview();
    const auto request = preview_request(value);
    REQUIRE(request);
    CHECK(request->property_or_method == "sunny_managed_preview_song_settings");
    const auto parsed = parse_managed_song_settings_preview(*request, context, value);
    REQUIRE(parsed);
    CHECK(parsed->preview_fingerprint ==
          "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff");
    CHECK(parsed->approved_preview.at("scope").at("all_tracks_affected") == true);
    const auto& before = parsed->approved_preview.at("before");
    CHECK(before.at("settings").at("tempo") == 120.0);
    CHECK(before.at("tracks").at(1).at("name") == "User strings");
    CHECK(before.at("scenes").at(0).at("tempo") == 150.0);
    CHECK(before.at("cue_points").at(0).at("time") == 4.0);
    const auto payload = intent(value);
    CHECK(payload.at("explicit_set_wide_approval") == true);
    const auto result = song_settings_test_fixture::normal_result();
    REQUIRE(managed_song_detail::result_matches_request(payload, result));
    CHECK(managed_song_detail::acknowledged_native_start_valid(payload, result, true));
    CHECK_FALSE(managed_song_detail::acknowledged_native_start_valid(payload, result, false));
    const auto& after = result.at("song_settings").at("after");
    CHECK(after.at("settings") ==
          json{{"tempo", 92.5}, {"signature_numerator", 3}, {"signature_denominator", 8}});
    CHECK(after.at("scenes") == before.at("scenes"));
    CHECK(after.at("cue_points") == before.at("cue_points"));
    CHECK(result.at("note_identity").at("notes").at(0).at("note_id") == 41);
    CHECK(result.at("note_identity").at("notes").at(1).at("note_id") == 99);
}
TEST_CASE("Song no-op admits truthful false native-start only for its exact intent",
          "[ableton][managed-song-settings]") {
    const auto value = song_settings_test_fixture::noop_preview();
    CHECK(value.at("preview_fingerprint") ==
          "6d0e1b21bb901639814a124a2441d171a93c781563432104e97d68981282778c");
    const auto payload = intent(value);
    const auto result = song_settings_test_fixture::noop_result();
    CHECK(result.at("song_settings").at("started_fields").empty());
    CHECK(result.at("song_settings").at("after").at("settings").at("tempo").is_number_float());
    CHECK(managed_song_detail::acknowledged_native_start_valid(payload, result, false));
    CHECK_FALSE(managed_song_detail::acknowledged_native_start_valid(payload, result, true));
}
TEST_CASE("Song native mismatch remains evidence and cannot forge verified readback",
          "[ableton][managed-song-settings]") {
    const auto payload = intent(song_settings_test_fixture::normal_preview());
    auto result = song_settings_test_fixture::clamped_result();
    CHECK(result.at("song_settings").at("after").at("settings").at("tempo") == 90.0);
    CHECK(result.at("song_settings").at("desired_settings_match") == false);
    CHECK(managed_song_detail::result_matches_request(payload, result));
    result["song_settings"]["desired_settings_match"] = true;
    CHECK_FALSE(managed_song_detail::result_matches_request(payload, result));
    const std::vector<std::function<void(json&)>> forgeries{
        [](json& r) { r["song_settings"]["after"]["scenes"][0]["tempo"] = 170.0; },
        [](json& r) { r["song_settings"]["after"]["current_song_time"] = 4.0; },
        [](json& r) { r["song_settings"]["returned_fields"].erase(2); },
        [](json& r) { r["song_settings"]["historical_song_identity_proven"] = true; },
        [](json& r) {
            r["song_settings"]["approved_preview"]["scope"]["all_tracks_affected"] = false;
        },
        [](json& r) { r["song_settings"]["unexpected_native_uuid"] = "same"; }};
    for (const auto& forge : forgeries) {
        result = song_settings_test_fixture::normal_result();
        forge(result);
        CHECK_FALSE(managed_song_detail::result_matches_request(payload, result));
    }
}
TEST_CASE("Song partial setter failure preserves actual progress without compensation",
          "[ableton][managed-song-settings]") {
    const auto journal = song_settings_test_fixture::partial_journal();
    const auto& payload = journal.at("request");
    CHECK(managed_song_detail::partial_valid(payload, journal));
    CHECK(journal.at("song_settings_progress").at("started_fields") ==
          json::array({"signature_numerator", "signature_denominator"}));
    CHECK(journal.at("song_settings_progress").at("returned_fields") ==
          json::array({"signature_numerator"}));
    CHECK(journal.at("song_settings_partial").at("after").at("settings") ==
          json{{"tempo", 120.0}, {"signature_numerator", 3}, {"signature_denominator", 4}});
    for (const auto& forge : std::vector<std::function<void(json&)>>{
             [](json& j) { j["native_mutation_started"] = false; },
             [](json& j) { j["song_settings_progress"]["started_fields"][0] = "tempo"; },
             [](json& j) { j["song_settings_partial"]["before"]["settings"]["tempo"] = 119.0; },
             [](json& j) { j["song_settings_partial_unavailable"] = true; }}) {
        auto invalid = journal;
        forge(invalid);
        CHECK_FALSE(managed_song_detail::partial_valid(payload, invalid));
    }
}
TEST_CASE("Self-consistent Song previews require real active tempo and finite closed scope",
          "[ableton][managed-song-settings]") {
    const auto original = song_settings_test_fixture::normal_preview();
    const auto request = preview_request(original);
    REQUIRE(request);
    for (const auto& forge : std::vector<std::function<void(json&)>>{
             [](json& v) { v["preview"]["before"]["tempo_parameter"] = nullptr; },
             [](json& v) { v["preview"]["before"]["tempo_parameter"]["automation_state"] = 1; },
             [](json& v) { v["preview"]["before"]["tempo_parameter"]["is_enabled"] = false; },
             [](json& v) { v["preview"]["before"]["tempo_parameter"]["state"] = 2; },
             [](json& v) { v["preview"]["before"]["flags"]["is_playing"] = true; },
             [](json& v) { v["preview"]["before"]["settings"]["tempo"] = 120; },
             [](json& v) { v["preview"]["before"]["scenes"][1]["tempo"] = 120.0; },
             [](json& v) { v["preview"]["unavailable_domains"].erase(0); },
             [](json& v) { v["observation"]["track_tag"] = "Sunny|foreign|part_a|track"; }}) {
        auto invalid = original;
        forge(invalid);
        sign_preview(invalid);
        CHECK_FALSE(parse_managed_song_settings_preview(*request, context, invalid));
    }
    const auto parsed = parse_managed_song_settings_preview(*request, context, original);
    REQUIRE(parsed);
    auto forged = *parsed;
    forged.observation = nullptr;
    CHECK_FALSE(make_managed_song_settings_request(context, "song_a", forged));
    forged = *parsed;
    forged.project_key = "foreign";
    CHECK_FALSE(make_managed_song_settings_request(context, "song_a", forged));
}
TEST_CASE("Song planning reuses exact quarter tempo and rejects unsupported authored maps",
          "[ableton][managed-song-settings]") {
    auto authored = score();
    authored.tempo_map.front().beat_unit = BeatUnit::DottedQuarter;
    const auto planned = plan_managed_song_settings(authored);
    REQUIRE(planned);
    CHECK(planned->tempo == 90.0);
    CHECK(planned->signature_numerator == 6);
    CHECK(planned->signature_denominator == 8);
    for (const auto type : {TempoTransitionType::Linear, TempoTransitionType::MetricModulation}) {
        auto invalid = authored;
        invalid.tempo_map.front().transition_type = type;
        const auto rejected = plan_managed_song_settings(invalid);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().starts_with("UnsupportedTempoTransition:"));
    }
    auto invalid = authored;
    auto later = invalid.tempo_map.front();
    later.position.bar = 2;
    invalid.tempo_map.push_back(later);
    auto rejected = plan_managed_song_settings(invalid);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().starts_with("UnsupportedTempoMap:"));
    invalid = authored;
    invalid.time_map.push_back({2, TimeSignature({1, 1, 1}, 4)});
    rejected = plan_managed_song_settings(invalid);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().starts_with("UnsupportedMeterMap:"));
    invalid = authored;
    invalid.parts.front().measures.front().local_time = TimeSignature({1, 1, 1}, 4);
    rejected = plan_managed_song_settings(invalid);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().starts_with("UnsupportedLocalMeter:"));
    CHECK(plan_managed_song_settings(score(20.0)));
    CHECK(plan_managed_song_settings(score(999.0)));
    invalid = score();
    invalid.tempo_map.front().bpm = PositiveRational{19999, 1000};
    CHECK_FALSE(plan_managed_song_settings(invalid));
    invalid.tempo_map.front().bpm = PositiveRational{999001, 1000};
    CHECK_FALSE(plan_managed_song_settings(invalid));
}

TEST_CASE("Known complete Song journal bytes must fit before immutable fencing",
          "[ableton][managed-song-settings]") {
    auto value = song_settings_test_fixture::normal_preview();
    const auto request = preview_request(value);
    REQUIRE(request);
    auto scene = value.at("preview").at("before").at("scenes").at(0);
    std::string wide;
    for (int i = 0; i < 250; ++i)
        wide += "\xf0\x9f\x98\x80";
    scene["name"] = wide;
    for (int i = 0; i < 1450; ++i)
        value["preview"]["before"]["scenes"].push_back(scene);
    sign_preview(value);
    const auto parsed = parse_managed_song_settings_preview(*request, context, value);
    REQUIRE(parsed);
    const auto apply = make_managed_song_settings_request(context, "song_a", *parsed);
    REQUIRE_FALSE(apply);
    CHECK(apply.error() == ErrorCode::ManagedReplyCapacityExceeded);
}

TEST_CASE("Song early pending and declines require no fabricated phase progress",
          "[ableton][managed-song-settings]") {
    const auto payload = intent(song_settings_test_fixture::normal_preview());
    for (const auto* outcome : {"pending", "declined"}) {
        json journal{{"outcome", outcome}, {"native_mutation_started", false}};
        CHECK(managed_song_detail::partial_valid(payload, journal));
        journal["native_mutation_started"] = true;
        CHECK_FALSE(managed_song_detail::partial_valid(payload, journal));
        journal["native_mutation_started"] = false;
        journal["song_settings_partial_unavailable"] = true;
        CHECK_FALSE(managed_song_detail::partial_valid(payload, journal));
    }
    for (const auto& value : {song_settings_test_fixture::normal_preview(),
                              song_settings_test_fixture::noop_preview()}) {
        const auto request = intent(value);
        const auto result = value.at("preview").at("desired").at("tempo") == 120
                                ? song_settings_test_fixture::noop_result()
                                : song_settings_test_fixture::normal_result();
        const auto& supplement = result.at("song_settings");
        json journal{{"outcome", "acknowledged"},
                     {"native_mutation_started", !supplement.at("started_fields").empty()},
                     {"result", result}};
        CHECK_FALSE(managed_song_detail::partial_valid(request, journal));
        journal["song_settings_progress"] = {{"started_fields", supplement.at("started_fields")},
                                             {"returned_fields", supplement.at("returned_fields")}};
        CHECK(managed_song_detail::partial_valid(request, journal));
        journal["song_settings_progress"]["returned_fields"].push_back("tempo");
        CHECK_FALSE(managed_song_detail::partial_valid(request, journal));
        journal["song_settings_progress"] = {{"started_fields", supplement.at("started_fields")},
                                             {"returned_fields", supplement.at("returned_fields")}};
        journal["song_settings_partial_unavailable"] = true;
        CHECK_FALSE(managed_song_detail::partial_valid(request, journal));
    }
}

TEST_CASE("Observed early Song scalar clamp is a stopped partial operation",
          "[ableton][managed-song-settings]") {
    const auto journal = song_settings_test_fixture::early_clamped_journal();
    const auto& payload = journal.at("request");
    REQUIRE(managed_song_detail::partial_valid(payload, journal));
    const auto& progress = journal.at("song_settings_progress");
    CHECK(progress.at("started_fields") == json::array({"signature_numerator"}));
    CHECK(progress.at("returned_fields") == json::array({"signature_numerator"}));
    const auto& actual = journal.at("song_settings_partial").at("after").at("settings");
    CHECK(actual ==
          json{{"tempo", 120.0}, {"signature_numerator", 2}, {"signature_denominator", 4}});
    auto forged = journal;
    forged["outcome"] = "acknowledged";
    forged["result"] = song_settings_test_fixture::normal_result();
    CHECK_FALSE(managed_song_detail::partial_valid(payload, forged));
}
