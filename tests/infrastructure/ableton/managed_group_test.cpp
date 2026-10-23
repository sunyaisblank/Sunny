#include "managed_envelope_revision_fixture.hpp"
#include "managed_group_fixture.hpp"
#include "managed_mixer_fixture.hpp"
#include "managed_note_update_fixture.hpp"
#include "managed_recovery_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <sunny/infrastructure/ableton/detail/managed_envelope_author.hpp>
#include <sunny/infrastructure/ableton/detail/managed_envelope_revision.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace {
const ManagedBridgeContext context{"bridge_a", "document_a"};
json authority() {
    // The literal hash below was independently computed with Python hashlib
    // and a separate typed SM1 encoder, not this consumer's digest helper.
    return json::parse(
        R"JSON({"schema_version":1,"context":{"bridge_instance":"bridge_a","document_token":"document_a"},"project_key":"project_a","group_key":"group_a","approved_preview_fingerprint":"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff","group_track_index":0,"member_binding_keys":["part_a","part_b"],"member_track_indices":[1,2],"selected_binding_key":"part_a","selected_track_index":1,"authority_origin":"explicit_current_group_adoption","historical_identity_proven":false})JSON");
}
void sign_group(json& observation) {
    observation["group_authority_fingerprint"] =
        *managed_detail::managed_digest(observation.at("group_authority"));
}
void grouped(json& observation, bool proof = true) {
    observation["track_index"] = 1;
    observation["manifest"]["track"]["is_grouped"] = true;
    observation["content_fingerprint"] =
        *managed_detail::managed_digest(observation.at("manifest"));
    observation["structural_boundary_complete"] = false;
    observation["content_boundary_complete"] = false;
    if (proof) {
        observation["group_authority"] = authority();
        observation["group_authority_fingerprint"] =
            "b6f5715a07dd5abe852c571c25e157b7c3e81df1f5796f7fb78d145294409b8f";
    }
}
ManagedBindingReceipt binding(bool proof = true) {
    auto observation = recovery_test_fixture::preview().at("observation");
    grouped(observation, proof);
    return {context, "project_a", "part_a", observation};
}
json changes(const ManagedBindingReceipt& selected) {
    const auto& note = selected.observation.at("note_identity").at("notes")[0];
    return json::array({{{"note_id", note.at("note_id")},
                         {"expected", managed_detail::semantic_note(note)},
                         {"updates", {{"velocity", 73.0}}}}});
}
json lane() {
    return {{"parameter", {{"kind", "panning"}}},
            {"interpolation", "step"},
            {"clip_end", 4.0},
            {"points",
             json::array({{{"time", 0.0}, {"value", -0.5}}, {{"time", 2.0}, {"value", 0.5}}})}};
}
class GroupPeer final : public LomTransport {
  public:
    json reply;
    LomResponse send(const LomRequest&) override {
        return {true, LomValue{reply}, std::nullopt, LomDeliveryState::ResponseReceived};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override { return {}; }
    bool is_connected() const noexcept override { return true; }
};
} // namespace

TEST_CASE("Group evidence closes literal current context selected member and typed hash",
          "[ableton][managed-group]") {
    auto selected = binding();
    CHECK(managed_detail::managed_digest(authority()) ==
          std::optional<std::string>{
              "b6f5715a07dd5abe852c571c25e157b7c3e81df1f5796f7fb78d145294409b8f"});
    REQUIRE(managed_detail::group_supplement_valid(selected.observation));
    REQUIRE(managed_detail::group_current_proof(selected.observation));
    CHECK(managed_detail::group_touched_boundary(
        selected.observation, context, "project_a", "part_a"));
    CHECK_FALSE(managed_detail::group_touched_boundary(
        selected.observation, {"bridge_b", "document_a"}, "project_a", "part_a"));
    CHECK_FALSE(managed_detail::group_touched_boundary(
        selected.observation, {"bridge_a", "document_b"}, "project_a", "part_a"));
    CHECK_FALSE(managed_detail::group_touched_boundary(
        selected.observation, context, "project_b", "part_a"));
    CHECK_FALSE(managed_detail::group_touched_boundary(
        selected.observation, context, "project_a", "part_b"));
    REQUIRE(managed_binding_from_json(managed_binding_to_json(selected)));
    selected.context.bridge_instance = "bridge_b";
    CHECK_FALSE(managed_binding_from_json(managed_binding_to_json(selected)));
}

TEST_CASE("Grouped read-only evidence without a specific proof grants no touched candidate",
          "[ableton][managed-group]") {
    const auto selected = binding(false);
    REQUIRE(managed_detail::group_supplement_valid(selected.observation));
    REQUIRE(managed_binding_from_json(managed_binding_to_json(selected)));
    CHECK_FALSE(managed_detail::group_current_proof(selected.observation));
    CHECK_FALSE(make_managed_note_update_request(context, "update", selected, changes(selected)));
    CHECK_FALSE(make_managed_note_population_request(
        context, "population", selected, changes(selected), json::array(), json::array()));
    CHECK_FALSE(make_managed_clip_geometry_request(context, "geometry", selected, 8.0, 4, 4));
    CHECK_FALSE(make_managed_envelope_request(context, "initial_lane", selected, lane()));
    CHECK_FALSE(make_managed_envelope_replacement_preview_request(context, selected, lane()));
    ManagedStaticMixerDesired desired;
    desired.mute = true;
    CHECK_FALSE(make_managed_static_mixer_preview_request(context, selected, desired, true));
}

TEST_CASE(
    "Exact current Group proof admits finite in-place candidates without destructive coverage",
    "[ableton][managed-group]") {
    const auto selected = binding();
    REQUIRE(make_managed_note_update_request(context, "update", selected, changes(selected)));
    REQUIRE(make_managed_note_population_request(
        context, "population", selected, changes(selected), json::array(), json::array()));
    REQUIRE(make_managed_clip_geometry_request(context, "geometry", selected, 8.0, 4, 4));
    REQUIRE(make_managed_envelope_request(context, "initial_lane", selected, lane()));
    REQUIRE(make_managed_envelope_replacement_preview_request(context, selected, lane()));
    ManagedStaticMixerDesired desired;
    desired.mute = true;
    REQUIRE(make_managed_static_mixer_preview_request(context, selected, desired, true));
    CHECK(selected.observation.at("structural_boundary_complete") == false);
    CHECK(selected.observation.at("content_boundary_complete") == false);
    CHECK_FALSE(make_managed_rebind_request({"bridge_b", "document_b"}, "rebind", selected));
    auto invalid = selected;
    invalid.observation["structural_boundary_complete"] = true;
    invalid.observation["content_boundary_complete"] = true;
    CHECK_FALSE(managed_binding_from_json(managed_binding_to_json(invalid)));
}

TEST_CASE("Rehashed Group contradictions and stale ungrouped proof remain unavailable",
          "[ableton][managed-group]") {
    using Fault = std::function<void(json&)>;
    const std::vector<Fault> faults{
        [](auto& o) { o["group_authority"]["schema_version"] = 1.0; },
        [](auto& o) { o["group_authority"]["schema_version"] = true; },
        [](auto& o) { o["group_authority"]["authority_origin"] = "name_matching"; },
        [](auto& o) { o["group_authority"]["historical_identity_proven"] = true; },
        [](auto& o) { o["group_authority"]["group_key"] = "group|a"; },
        [](auto& o) { o["group_authority"]["group_track_index"] = 1; },
        [](auto& o) { o["group_authority"]["group_track_index"] = UINT64_MAX; },
        [](auto& o) { o["group_authority"]["member_binding_keys"] = {"part_a", "part_a"}; },
        [](auto& o) { o["group_authority"]["member_track_indices"] = {1, 1}; },
        [](auto& o) { o["group_authority"]["member_binding_keys"] = json::array(); },
        [](auto& o) { o["group_authority"]["member_track_indices"] = {1}; },
        [](auto& o) { o["group_authority"]["selected_track_index"] = 2; },
        [](auto& o) { o["group_authority"]["selected_binding_key"] = "part_c"; },
        [](auto& o) {
            o["group_authority"]["approved_preview_fingerprint"] = std::string(64, 'F');
        },
        [](auto& o) { o["group_authority"]["context"]["unknown"] = true; },
        [](auto& o) { o["group_authority"]["unexpected"] = true; },
        [](auto& o) { o["track_index"] = 2; },
        [](auto& o) { o["track_tag"] = "Sunny|project_a|part_b|track"; },
        [](auto& o) { o["structural_boundary_complete"] = true; },
        [](auto& o) { o["content_boundary_complete"] = true; },
        [](auto& o) { o["manifest"]["track"]["is_grouped"] = false; }};
    for (const auto& fault : faults) {
        auto value = binding().observation;
        fault(value);
        sign_group(value); // Isolate semantic closure from fingerprint mismatch.
        CHECK_FALSE(managed_detail::group_supplement_valid(value));
    }
    auto value = binding().observation;
    value.erase("group_authority");
    CHECK_FALSE(managed_detail::group_supplement_valid(value));
    value = binding().observation;
    value.erase("group_authority_fingerprint");
    CHECK_FALSE(managed_detail::group_supplement_valid(value));
    value = binding().observation;
    value["group_authority_fingerprint"] = std::string(64, '0');
    CHECK_FALSE(managed_detail::group_supplement_valid(value));
}

TEST_CASE("Recovery and selected envelope previews require the exact current member proof",
          "[ableton][managed-group]") {
    auto preview = recovery_test_fixture::preview();
    grouped(preview["observation"]);
    preview["selector"]["track_index"] = 1;
    preview.erase("preview_fingerprint");
    preview["preview_fingerprint"] = *managed_detail::managed_digest(preview);
    REQUIRE(managed_adoption_preview_from_json(preview));
    preview["observation"].erase("group_authority");
    preview["observation"].erase("group_authority_fingerprint");
    preview.erase("preview_fingerprint");
    preview["preview_fingerprint"] = *managed_detail::managed_digest(preview);
    CHECK_FALSE(managed_adoption_preview_from_json(preview));

    preview = json::parse(MANAGED_ENVELOPE_REVISION_FIXTURE).at("preview");
    grouped(preview["observation"]);
    preview.erase("preview_fingerprint");
    preview["preview_fingerprint"] = *managed_detail::managed_digest(preview);
    REQUIRE(managed_envelope_replacement_preview_from_json(preview));
    preview["observation"]["group_authority"]["context"]["document_token"] = "document_b";
    sign_group(preview["observation"]);
    preview.erase("preview_fingerprint");
    preview["preview_fingerprint"] = *managed_detail::managed_digest(preview);
    CHECK_FALSE(managed_envelope_replacement_preview_from_json(preview));
}

TEST_CASE("Static Mixer snapshot closes grouped member against actual track context",
          "[ableton][managed-group]") {
    auto preview = test::managed_mixer_fixture().at("adoption_preview").at("preview");
    auto& before = preview["before"];
    grouped(before["binding_observation"]);
    before["binding_observation"]["track_index"] = 0;
    before["binding_observation"]["group_authority"] =
        json::parse(MANAGED_GROUP_OBSERVATION).at("group_authority");
    sign_group(before["binding_observation"]);
    before["track_context"]["is_grouped"] = true;
    // Actual member 0, Group 1, then the original Return cohort.
    before["solo_cohort"].insert(before["solo_cohort"].begin() + 1,
                                 json{{"kind", "track"},
                                      {"index", 1},
                                      {"name", "Current strings"},
                                      {"mute", false},
                                      {"solo", false},
                                      {"muted_via_solo", false}});
    REQUIRE(managed_mixer_detail::snapshot_valid(before, preview));
    before["binding_observation"].erase("group_authority");
    before["binding_observation"].erase("group_authority_fingerprint");
    CHECK_FALSE(managed_mixer_detail::snapshot_valid(before, preview));
}

TEST_CASE("Actual source Group capture retains original approval and only finite member evidence",
          "[ableton][managed-group]") {
    const auto observation = json::parse(MANAGED_GROUP_OBSERVATION);
    REQUIRE(managed_detail::group_supplement_valid(observation));
    const ManagedBindingReceipt selected{context, "project_a", "part_a", observation};
    REQUIRE(managed_binding_from_json(managed_binding_to_json(selected)));
    CHECK(observation.at("group_authority_fingerprint") ==
          "efe27876dc344ef457a29937de88dfb53c1d7b52a04bf4b26c321ae2c681d5f2");
    CHECK(observation.at("group_authority").at("approved_preview_fingerprint") ==
          "4999f3d91c78adc0b96998c05f74a24c1b53b9470b07f5f04682f89e50f6ea2c");
    CHECK(observation.at("group_authority").at("group_track_index") == 1);
    CHECK(observation.at("group_authority").at("member_track_indices") == json::array({0}));
    CHECK(observation.at("note_identity").at("notes")[0].at("note_id") == 41);
    CHECK(observation.at("note_identity").at("notes")[1].at("note_id") == 99);
    REQUIRE(make_managed_note_update_request(context, "update", selected, changes(selected)));
    REQUIRE(make_managed_clip_geometry_request(context, "geometry", selected, 8.0, 4, 4));
    CHECK_FALSE(make_managed_rebind_request({"bridge_b", "document_b"}, "rebind", selected));
}

TEST_CASE("Grouped note ACK cannot outrun current context or silently change grouping",
          "[ableton][managed-group]") {
    auto journal = json::parse(managed_note_update_fixture).at("journal");
    grouped(journal["result"]);
    journal["result"]["note_update"]["before_manifest"]["track"]["is_grouped"] = true;
    const auto sign_request = [](json& value) {
        value["request"]["expected_content_fingerprint"] = *managed_detail::managed_digest(
            value.at("result").at("note_update").at("before_manifest"));
        value["request_fingerprint"] = *managed_detail::managed_digest(
            json{{"name", value.at("name")}, {"request", value.at("request")}});
    };
    sign_request(journal);
    auto execute = [](const json& value) {
        const auto request = LomProtocol::call_method(
            LomPaths::song(), "sunny_managed_update_notes", {value.at("request")});
        const auto prepared = prepare_managed_operation(context, request);
        REQUIRE(prepared);
        GroupPeer peer;
        peer.reply = value;
        const auto receipt = execute_managed_operation(*prepared, peer);
        REQUIRE(receipt);
        return *receipt;
    };
    REQUIRE(execute(journal).outcome == ManagedOperationOutcome::Acknowledged);
    auto changed = journal;
    changed["result"]["note_update"]["before_manifest"]["track"]["is_grouped"] = false;
    sign_request(changed);
    CHECK(execute(changed).outcome == ManagedOperationOutcome::Indeterminate);
    changed = journal;
    changed["result"]["group_authority"]["context"]["bridge_instance"] = "bridge_b";
    sign_group(changed["result"]);
    CHECK(execute(changed).outcome == ManagedOperationOutcome::Indeterminate);
}

TEST_CASE("Grouped initial-lane reply capacity includes its full current cohort proof",
          "[ableton][managed-group]") {
    auto observation = binding().observation;
    auto& proof = observation["group_authority"];
    proof["member_binding_keys"] = json::array();
    proof["member_track_indices"] = json::array();
    proof["member_binding_keys"].push_back("part_a");
    proof["member_track_indices"].push_back(1);
    for (int i = 1; i < 256; ++i) {
        proof["member_binding_keys"].push_back(std::string(60, 'a') + std::to_string(i));
        proof["member_track_indices"].push_back(i + 1);
    }
    sign_group(observation);
    const auto initial = make_managed_envelope_request(
        context, "capacity", {context, "project_a", "part_a", observation}, lane());
    REQUIRE(initial);
    const auto request = std::get<json>(initial->args[0]);
    const auto identity_note = observation.at("note_identity").at("notes")[0];
    const auto fits_count = [&](std::size_t count, bool with_proof) {
        auto value = observation;
        value["note_identity"]["notes"] = std::vector<json>(count, identity_note);
        if (!with_proof) {
            value.erase("group_authority");
            value.erase("group_authority_fingerprint");
        }
        return managed_detail::envelope_author_response_fits(request, value);
    };
    // Find the finite count boundary in logarithmic calls. The capacity helper
    // uses the known count with worst finite native values/IDs, never this note's
    // convenient wire width or semantic uniqueness as a substitute for a bound.
    std::size_t lower = 0, upper = 65536;
    REQUIRE(fits_count(lower, true));
    REQUIRE_FALSE(fits_count(upper, true));
    while (upper - lower > 1) {
        const auto middle = lower + (upper - lower) / 2;
        if (fits_count(middle, true))
            lower = middle;
        else
            upper = middle;
    }
    CHECK(fits_count(upper, false));
    CHECK_FALSE(fits_count(upper, true));
}
