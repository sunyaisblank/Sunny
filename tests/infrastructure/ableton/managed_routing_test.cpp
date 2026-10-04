#include "managed_routing_candidates_fixture.hpp"
#include "managed_routing_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/detail/managed_routing.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
namespace detail = sunny::infrastructure::managed_routing_detail;

TEST_CASE("Literal native routing producer closes immutable previews and "
          "authorized receipts",
          "[ableton][managed][routing]") {
    const auto all = managed_routing_fixture();
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    for (const auto& [name, fixture] : all.items()) {
        INFO(name);
        const bool group = name == "group";
        const auto method = group ? detail::group_preview_method : detail::preview_method;
        REQUIRE(detail::request_valid(method, fixture.at("request")));
        REQUIRE(detail::frame_valid(fixture.at("preview").at("preview").at("before")));
        REQUIRE(detail::preview_valid(fixture.at("preview").at("preview")));
        const auto request = LomProtocol::call_method(
            LomPaths::song(), std::string{method}, {fixture.at("request")});
        const auto preview = parse_managed_routing_preview(request, context, fixture.at("preview"));
        REQUIRE(preview);
        const auto apply = make_managed_routing_request(context, "route_a", *preview);
        REQUIRE(apply);
        REQUIRE(detail::request_valid(detail::apply_method, fixture.at("applied_request")));
        REQUIRE(
            detail::result_matches_request(fixture.at("applied_request"), fixture.at("result")));
        REQUIRE(detail::acknowledged_native_start_valid(
            fixture.at("applied_request"),
            fixture.at("result"),
            fixture.at("operation").at("native_mutation_started")));
        auto journal = fixture.at("operation");
        journal["outcome"] = "acknowledged";
        journal["result"] = fixture.at("result");
        REQUIRE(detail::partial_valid(fixture.at("applied_request"), journal));
    }
}
TEST_CASE("Group-only grants cannot forge Clip authority or native mutation",
          "[ableton][managed][routing]") {
    const auto fixture = managed_routing_fixture().at("group");
    const auto& request = fixture.at("applied_request");
    auto result = fixture.at("result");
    REQUIRE(result.size() == 1);
    REQUIRE_FALSE(detail::acknowledged_native_start_valid(request, result, true));
    result["group_adoption"]["part_authority_granted"] = true;
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["manifest"] = fixture.at("preview").at("observation").at("manifest");
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["group_adoption"]["after"]["song"]["scenes"][0]["name"] = "Foreign scene changed";
    REQUIRE_FALSE(detail::result_matches_request(request, result));
}
TEST_CASE("Routing consumer recomputes exact Return and foreign Part preservation",
          "[ableton][managed][routing]") {
    const auto fixture = managed_routing_fixture().at("create");
    const auto& request = fixture.at("applied_request");
    auto result = fixture.at("result");
    result["routing"]["after"]["mixers"][0]["mixer"]["sends"][0]["value"] = 0.125;
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    auto& observed = result["routing"]["affected_observations"][0]["observation"];
    observed["manifest"]["clip"]["end_marker"] = 123.0;
    observed["content_fingerprint"] =
        *sunny::infrastructure::managed_detail::managed_digest(observed.at("manifest"));
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["routing"]["after"]["song"]["return_tracks"].push_back(
        result["routing"]["after"]["song"]["return_tracks"].back());
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["routing"]["affected_observations"].push_back(
        result["routing"]["affected_observations"].front());
    REQUIRE_FALSE(detail::result_matches_request(request, result));
}
TEST_CASE("Truthful send clamp is acknowledged but cannot forge physical success",
          "[ableton][managed][routing]") {
    const auto fixture = managed_routing_fixture().at("send");
    const auto& request = fixture.at("applied_request");
    auto result = fixture.at("result");
    result["routing"]["send_readback"]["display"] = "-23.999999999999 dB";
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["routing"]["tap_policy_observed"] = true;
    REQUIRE_FALSE(detail::result_matches_request(request, result));
    result = fixture.at("result");
    result["routing"]["logical_send_complete"] = true;
    REQUIRE_FALSE(detail::result_matches_request(request, result));
}
TEST_CASE("Routing partial journal preserves ordered actual phase evidence",
          "[ableton][managed][routing]") {
    const auto fixture = managed_routing_fixture().at("create");
    const auto& request = fixture.at("applied_request");
    json journal{{"outcome", "pending"}, {"native_mutation_started", false}};
    REQUIRE(detail::partial_valid(request, journal));
    journal["routing_progress"] = {{"started", {"create_return_track"}},
                                   {"returned", json::array()}};
    REQUIRE_FALSE(detail::partial_valid(request, journal));
    journal["native_mutation_started"] = true;
    REQUIRE(detail::partial_valid(request, journal));
    journal["routing_progress"]["started"] = {"return_name"};
    REQUIRE_FALSE(detail::partial_valid(request, journal));
    journal["routing_progress"] = {{"started", {"create_return_track"}},
                                   {"returned", {"create_return_track"}}};
    journal["routing_partial"] = fixture.at("preview").at("preview").at("before");
    REQUIRE(detail::partial_valid(request, journal));
    journal["routing_partial_unavailable"] = "native returned an invalid frame";
    REQUIRE_FALSE(detail::partial_valid(request, journal));
}

TEST_CASE("Routing overlays preserve foreign typed bindings and Group-only emptiness",
          "[ableton][managed][routing]") {
    const auto all = managed_routing_fixture();
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const auto& fixture = all.at("foreign");
    const auto bindings = managed_routing_affected_bindings(
        context, fixture.at("applied_request"), fixture.at("result"));
    REQUIRE(bindings);
    REQUIRE(bindings->size() == 2);
    REQUIRE(bindings->at(0).project_key == "project_a");
    REQUIRE(bindings->at(1).project_key == "project_b");
    REQUIRE(bindings->at(1).binding_key == "part_b");
    REQUIRE(bindings->at(1).observation.at("note_identity").at("notes")[0].at("note_id") == 501);
    REQUIRE(bindings->at(1).observation.at("manifest").at("mixer").at("sends").size() == 2);
    const auto empty = managed_routing_affected_bindings(
        context, all.at("group").at("applied_request"), all.at("group").at("result"));
    REQUIRE(empty);
    REQUIRE(empty->empty());
    const ManagedBridgeContext foreign_context{"foreign_bridge", "document_a"};
    REQUIRE_FALSE(managed_routing_affected_bindings(
        foreign_context, fixture.at("applied_request"), fixture.at("result")));
    auto forged = fixture.at("result");
    auto& foreign = forged["routing"]["affected_observations"][1]["observation"];
    foreign["manifest"]["mixer"]["panning"]["value"] = 0.3;
    foreign["content_fingerprint"] =
        *sunny::infrastructure::managed_detail::managed_digest(foreign.at("manifest"));
    REQUIRE_FALSE(
        managed_routing_affected_bindings(context, fixture.at("applied_request"), forged));
}

TEST_CASE("Routing approvals bind preview token context and all admitted intent domains",
          "[ableton][managed][routing]") {
    auto fixture = managed_routing_fixture().at("create");
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const auto preview_request = LomProtocol::call_method(
        LomPaths::song(), std::string{detail::preview_method}, {fixture.at("request")});
    auto substituted_preview = fixture.at("preview");
    substituted_preview["preview_token"] = std::string(32, 'b');
    REQUIRE_FALSE(parse_managed_routing_preview(preview_request, context, substituted_preview));
    auto request = fixture.at("applied_request");
    request["preview_token"] = std::string(32, 'b');
    REQUIRE_FALSE(detail::request_valid(detail::apply_method, request));
    request = fixture.at("applied_request");
    request["explicit_current_routing_approval"] = false;
    REQUIRE_FALSE(detail::request_valid(detail::apply_method, request));
    const json positive_send{{"kind", "send_level"},
                             {"aux_key", "aux_room"},
                             {"level_db", 6.0},
                             {"tolerance_db", 0.1},
                             {"requested_pre_fader", false}};
    REQUIRE(detail::intent_valid(positive_send));
    auto invalid = positive_send;
    invalid["level_db"] = std::numeric_limits<double>::infinity();
    REQUIRE_FALSE(detail::intent_valid(invalid));
    invalid = positive_send;
    invalid["tolerance_db"] = -0.1;
    REQUIRE_FALSE(detail::intent_valid(invalid));
}

TEST_CASE("Routing candidate inspector closes actual source metadata and frame anchor",
          "[ableton][managed][routing][routing-candidates]") {
    const auto fixture = managed_routing_candidates_fixture();
    const auto& actual = fixture.at("actual");
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const ManagedBindingReceipt binding{context, "project_a", "part_a", actual.at("observation")};
    const auto request = make_managed_routing_candidates_request(context, binding);
    REQUIRE(request);
    REQUIRE(request->property_or_method == detail::candidates_method);
    REQUIRE(std::get<json>(request->args.front()) == fixture.at("request"));
    const auto parsed = parse_managed_routing_candidates(*request, context, actual);
    REQUIRE(parsed);
    REQUIRE(*parsed == actual);
    const auto& routes = parsed->at("frame").at("mixers").at(0).at("routing");
    REQUIRE(routes.at("available_types").at(0).at("attached_target") ==
            json{{"kind", "main"}, {"index", nullptr}});
    REQUIRE(routes.at("available_types").at(0).at("identifier").is_string());
    REQUIRE(parsed->at("native_mutation_started") == false);
    REQUIRE(parsed->at("authority_origin") == "none");
}
TEST_CASE("Routing candidate inspector rejects valid hashes attached to the wrong finite anchor",
          "[ableton][managed][routing][routing-candidates]") {
    const auto fixture = managed_routing_candidates_fixture();
    const ManagedBridgeContext context{"bridge_a", "document_a"};
    const auto request = LomProtocol::call_method(
        LomPaths::song(), std::string{detail::candidates_method}, {fixture.at("request")});
    auto actual = fixture.at("actual");
    actual["observation"]["track_index"] = 1;
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["frame"]["mixers"][0]["mixer"]["volume"]["value"] = 0.75;
    REQUIRE(detail::frame_valid(actual.at("frame")));
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["frame"]["song"]["tracks"][0]["clip_slots"][0]["clip"]["is_playing"] = true;
    REQUIRE(detail::frame_valid(actual.at("frame")));
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["observation"]["manifest"]["mixer"]["volume"]["value"] = 0.75;
    actual["observation"]["content_fingerprint"] =
        *sunny::infrastructure::managed_detail::managed_digest(
            actual.at("observation").at("manifest"));
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["context"]["bridge_instance"] = "foreign_bridge";
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["native_mutation_started"] = true;
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
    actual = fixture.at("actual");
    actual["preview_token"] = std::string(32, 'a');
    REQUIRE_FALSE(parse_managed_routing_candidates(request, context, actual));
}
