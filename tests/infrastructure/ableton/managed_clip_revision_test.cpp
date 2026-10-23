#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <limits>
#include <sunny/infrastructure/ableton/managed_clip_revision.hpp>

using namespace sunny::infrastructure;
using nlohmann::json;
using sunny::core::ErrorCode;

namespace {
json note(int pitch, double start, double duration) {
    return {{"pitch", pitch},
            {"start_time", start},
            {"duration", duration},
            {"velocity", 96.0},
            {"mute", false},
            {"probability", 1.0},
            {"velocity_deviation", 0.0},
            {"release_velocity", 64.0}};
}
ManagedClipProjection old_part() {
    ManagedClipProjection result;
    result.clip_end = 8.0;
    // Already-compiled quarter-note units, not measures or meter-scaled beats.
    result.notes =
        json::array({note(60, 0.0, 2.0 / 3.0), note(64, 2.0 / 3.0, 1.0 / 3.0), note(69, 4.0, 1.0)});
    return result;
}
ManagedClipProjection final_part() {
    ManagedClipProjection result;
    result.clip_end = 12.0;
    result.signature_numerator = 3;
    result.signature_denominator = 8;
    result.notes = json::array({note(62, 0.0, 2.0 / 3.0),
                                note(66, 2.0 / 3.0, 1.0 / 3.0),
                                note(69, 4.0, 1.0),
                                note(62, 8.0, 1.0)});
    return result;
}
const std::vector<std::string> old_keys{"e5_n0", "e9_n0", "e12_n0"};
const std::vector<std::string> final_keys{"e5_n0", "e9_n0", "e12_n0", "e17_n0"};
void exact_geometry(const ManagedClipProjection& projection,
                    double end,
                    int numerator,
                    int denominator) {
    CHECK(projection.clip_end == end);
    CHECK(projection.signature_numerator == numerator);
    CHECK(projection.signature_denominator == denominator);
}
void contained_tails(const ManagedClipProjection& projection) {
    for (const auto& value : projection.notes) {
        const auto end = value.at("start_time").get<double>() + value.at("duration").get<double>();
        CHECK(std::isfinite(end));
        CHECK(end <= projection.clip_end);
    }
}
} // namespace

TEST_CASE("Managed simultaneous revision extends before literal later attacks",
          "[managed][clip-revision]") {
    const auto previous = old_part(), desired = final_part();
    const auto stages = plan_managed_clip_revision(previous, old_keys, desired, final_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 3);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::ExpandGeometry);
    exact_geometry(stages->at(0).projection, 12.0, 4, 4);
    CHECK(stages->at(0).projection.notes ==
          json::array(
              {note(60, 0.0, 2.0 / 3.0), note(64, 2.0 / 3.0, 1.0 / 3.0), note(69, 4.0, 1.0)}));
    CHECK(stages->at(0).desired_note_keys == old_keys);
    CHECK(stages->at(1).phase == ManagedClipRevisionPhase::UpdateNotePopulation);
    exact_geometry(stages->at(1).projection, 12.0, 4, 4);
    CHECK(stages->at(1).projection.notes == json::array({note(62, 0.0, 2.0 / 3.0),
                                                         note(66, 2.0 / 3.0, 1.0 / 3.0),
                                                         note(69, 4.0, 1.0),
                                                         note(62, 8.0, 1.0)}));
    CHECK(stages->at(1).desired_note_keys == final_keys);
    CHECK(stages->at(2).phase == ManagedClipRevisionPhase::FinalizeGeometry);
    exact_geometry(stages->at(2).projection, 12.0, 3, 8);
    CHECK(stages->at(2).projection.notes == stages->at(1).projection.notes);
    CHECK(stages->at(2).desired_note_keys == final_keys);
    for (const auto& stage : *stages)
        contained_tails(stage.projection);
}

TEST_CASE("Managed simultaneous reverse revision removes later attacks before shrink",
          "[managed][clip-revision]") {
    const auto stages = plan_managed_clip_revision(final_part(), final_keys, old_part(), old_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 2);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::UpdateNotePopulation);
    exact_geometry(stages->at(0).projection, 12.0, 3, 8);
    CHECK(stages->at(0).projection.notes ==
          json::array(
              {note(60, 0.0, 2.0 / 3.0), note(64, 2.0 / 3.0, 1.0 / 3.0), note(69, 4.0, 1.0)}));
    CHECK(stages->at(0).desired_note_keys == old_keys);
    CHECK(stages->at(1).phase == ManagedClipRevisionPhase::FinalizeGeometry);
    exact_geometry(stages->at(1).projection, 8.0, 4, 4);
    CHECK(stages->at(1).projection.notes == stages->at(0).projection.notes);
    CHECK(stages->at(1).desired_note_keys == old_keys);
    for (const auto& stage : *stages)
        contained_tails(stage.projection);
}

TEST_CASE("Managed staging avoids redundant extent and meter phases", "[managed][clip-revision]") {
    auto desired = final_part();
    desired.signature_numerator = 4;
    desired.signature_denominator = 4;
    auto stages = plan_managed_clip_revision(old_part(), old_keys, desired, final_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 2);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::ExpandGeometry);
    CHECK(stages->at(1).phase == ManagedClipRevisionPhase::UpdateNotePopulation);
    desired = old_part();
    desired.notes.at(0)["velocity"] = 47.0;
    stages = plan_managed_clip_revision(old_part(), old_keys, desired, old_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::UpdateNotePopulation);
    exact_geometry(stages->at(0).projection, 8.0, 4, 4);
    CHECK(stages->at(0).projection.notes.at(0).at("velocity") == 47.0);
}

TEST_CASE("Managed geometry only and unchanged revisions do not invent note writes",
          "[managed][clip-revision]") {
    auto desired = old_part();
    auto stages = plan_managed_clip_revision(old_part(), old_keys, desired, old_keys);
    REQUIRE(stages);
    CHECK(stages->empty());
    desired.clip_end = 12.0;
    desired.signature_numerator = 7;
    desired.signature_denominator = 8;
    stages = plan_managed_clip_revision(old_part(), old_keys, desired, old_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::FinalizeGeometry);
    exact_geometry(stages->at(0).projection, 12.0, 7, 8);
    CHECK(stages->at(0).projection.notes == old_part().notes);
    desired.clip_end = 5.0; // Exact existing A4 endpoint remains contained.
    stages = plan_managed_clip_revision(old_part(), old_keys, desired, old_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    exact_geometry(stages->at(0).projection, 5.0, 7, 8);
}

TEST_CASE("Managed attack association is by key rather than array position or note multiset",
          "[managed][clip-revision]") {
    auto reordered = old_part();
    reordered.notes =
        json::array({note(69, 4.0, 1.0), note(60, 0.0, 2.0 / 3.0), note(64, 2.0 / 3.0, 1.0 / 3.0)});
    const std::vector<std::string> reordered_keys{"e12_n0", "e5_n0", "e9_n0"};
    auto stages = plan_managed_clip_revision(old_part(), old_keys, reordered, reordered_keys);
    REQUIRE(stages);
    CHECK(stages->empty());
    reordered.clip_end = 12.0;
    stages = plan_managed_clip_revision(old_part(), old_keys, reordered, reordered_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::FinalizeGeometry);
    CHECK(stages->at(0).desired_note_keys == reordered_keys);
    CHECK(stages->at(0).projection.notes == reordered.notes);
    // Same note multiset associated with different Event IDs is a real revision.
    reordered.clip_end = 8.0;
    stages = plan_managed_clip_revision(old_part(), old_keys, reordered, old_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).phase == ManagedClipRevisionPhase::UpdateNotePopulation);
}

TEST_CASE("Managed staging preserves retained chord head identities", "[managed][clip-revision]") {
    auto previous = old_part();
    previous.notes = json::array({note(60, 0.0, 1.0), note(64, 0.0, 1.0)});
    const std::vector<std::string> keys{"e8_n0", "e8_n1"};
    auto desired = previous;
    desired.notes.at(0)["pitch"] = 62;
    desired.notes.at(1)["pitch"] = 66;
    auto stages = plan_managed_clip_revision(previous, keys, desired, keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).desired_note_keys == keys);
    desired.notes.push_back(note(69, 0.0, 1.0));
    const auto larger =
        plan_managed_clip_revision(previous, keys, desired, {"e8_n0", "e8_n1", "e8_n2"});
    REQUIRE_FALSE(larger);
    CHECK(larger.error() == ErrorCode::TargetValueUnrepresentable);
    desired = previous;
    // Same cardinality alone cannot authorize shifting native opaque head data.
    const auto shifted = plan_managed_clip_revision(previous, keys, desired, {"e8_n1", "e8_n2"});
    REQUIRE_FALSE(shifted);
    CHECK(shifted.error() == ErrorCode::TargetValueUnrepresentable);
    // Whole Event replacement may introduce a new chord, without transferring IDs.
    desired.notes.push_back(note(69, 0.0, 1.0));
    stages = plan_managed_clip_revision(previous, keys, desired, {"e10_n0", "e10_n1", "e10_n2"});
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
}

TEST_CASE("Managed staging does not silently author unsupported retained note fields",
          "[managed][clip-revision]") {
    for (const auto* field : {"probability", "velocity_deviation"}) {
        auto desired = old_part();
        desired.notes.at(0)[field] = 0.5;
        const auto stages = plan_managed_clip_revision(old_part(), old_keys, desired, old_keys);
        REQUIRE_FALSE(stages);
        CHECK(stages.error() == ErrorCode::TargetValueUnrepresentable);
    }
    auto desired = old_part();
    auto added = note(67, 6.0, 1.0);
    added["probability"] = 0.25;
    added["velocity_deviation"] = -12.0;
    desired.notes.push_back(added);
    const auto stages = plan_managed_clip_revision(
        old_part(), old_keys, desired, {"e5_n0", "e9_n0", "e12_n0", "e20_n0"});
    REQUIRE(stages);
    REQUIRE(stages->size() == 1);
    CHECK(stages->at(0).projection.notes.at(3) == added);
}

TEST_CASE("Managed staging validates both full projections before returning any phase",
          "[managed][clip-revision]") {
    const std::vector<std::function<void(ManagedClipProjection&)>> malformed{
        [](auto& value) { value.clip_end = 0.0; },
        [](auto& value) { value.clip_end = std::numeric_limits<double>::infinity(); },
        [](auto& value) { value.clip_end = std::numeric_limits<double>::quiet_NaN(); },
        [](auto& value) { value.signature_numerator = 0; },
        [](auto& value) { value.signature_numerator = 100; },
        [](auto& value) { value.signature_denominator = 3; },
        [](auto& value) { value.notes = json::object(); },
        [](auto& value) { value.notes.at(0)["pitch"] = 60.0; },
        [](auto& value) { value.notes.at(0)["mute"] = 0; },
        [](auto& value) { value.notes.at(0)["velocity"] = 128.0; },
        [](auto& value) { value.notes.at(0)["probability"] = -0.01; },
        [](auto& value) { value.notes.at(0)["start_time"] = -0.01; },
        [](auto& value) { value.notes.at(0)["duration"] = 0.0; },
        [](auto& value) {
            value.notes.at(0)["duration"] = std::numeric_limits<double>::infinity();
        },
        [](auto& value) {
            value.notes.at(0)["release_velocity"] = std::numeric_limits<double>::quiet_NaN();
        },
        [](auto& value) { value.notes.at(0)["note_id"] = 101; },
        [](auto& value) { value.notes.at(0).erase("velocity_deviation"); }};
    for (std::size_t index = 0; index < malformed.size(); ++index) {
        INFO(index);
        auto previous = old_part(), desired = final_part();
        malformed.at(index)(previous);
        auto stages = plan_managed_clip_revision(previous, old_keys, desired, final_keys);
        REQUIRE_FALSE(stages);
        CHECK(stages.error() == ErrorCode::ProtocolError);
        previous = old_part();
        malformed.at(index)(desired);
        stages = plan_managed_clip_revision(previous, old_keys, desired, final_keys);
        REQUIRE_FALSE(stages);
        CHECK(stages.error() == ErrorCode::ProtocolError);
    }
}

TEST_CASE("Managed staging checks literal tails against their own extents without epsilon",
          "[managed][clip-revision]") {
    auto desired = old_part();
    desired.clip_end = 5.0;
    REQUIRE(plan_managed_clip_revision(old_part(), old_keys, desired, old_keys));
    desired.clip_end = std::nextafter(5.0, 0.0);
    CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, desired, old_keys));
    desired = final_part();
    desired.notes.at(3)["duration"] = 4.0;
    REQUIRE(plan_managed_clip_revision(old_part(), old_keys, desired, final_keys));
    // Use an increment whose actual endpoint is strictly beyond12; no epsilon.
    desired.notes.at(3)["duration"] = 4.000000000000002;
    CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, desired, final_keys));
    auto previous = old_part();
    previous.notes.at(2)["duration"] = 5.0; // Old end8 cannot contain endpoint9.
    CHECK_FALSE(plan_managed_clip_revision(previous, old_keys, final_part(), final_keys));
    desired = old_part();
    desired.clip_end = std::numeric_limits<double>::max();
    desired.notes.at(0)["start_time"] = std::numeric_limits<double>::max() / 2.0;
    desired.notes.at(0)["duration"] = std::numeric_limits<double>::max();
    CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, desired, old_keys));
    desired = old_part();
    desired.notes.at(2)["start_time"] = 8.0;
    desired.notes.at(2)["duration"] = std::numeric_limits<double>::denorm_min();
    CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, desired, old_keys));
}

TEST_CASE("Managed staging rejects malformed missing or duplicate attack keys",
          "[managed][clip-revision]") {
    for (const auto& key :
         {"e0_n0", "e01_n0", "e1_n00", "e1_n65536", "e18446744073709551616_n0", "e1_n-1", "1_0"}) {
        auto keys = old_keys;
        keys.at(0) = key;
        CHECK_FALSE(plan_managed_clip_revision(old_part(), keys, old_part(), old_keys));
        CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, old_part(), keys));
    }
    CHECK_FALSE(
        plan_managed_clip_revision(old_part(), {"e5_n0", "e5_n0", "e12_n0"}, old_part(), old_keys));
    CHECK_FALSE(plan_managed_clip_revision(old_part(), old_keys, old_part(), {"e5_n0", "e9_n0"}));
    auto boundary = old_keys;
    boundary.at(0) = "e18446744073709551615_n65535";
    const auto stages = plan_managed_clip_revision(old_part(), boundary, old_part(), boundary);
    REQUIRE(stages);
    CHECK(stages->empty());
}

TEST_CASE("Managed staging keeps outside Clip requests residual and does not mutate inputs",
          "[managed][clip-revision]") {
    auto previous = old_part(), desired = final_part();
    previous.outside_clip_requests.push_back(
        {LomRequestType::SetProperty, {{"old_residual"}}, "tempo", {90.0}});
    desired.outside_clip_requests.push_back(
        {LomRequestType::SetProperty, {{"final_residual"}}, "tempo", {120.0}});
    const auto previous_notes = previous.notes, desired_notes = desired.notes;
    auto stages = plan_managed_clip_revision(previous, old_keys, desired, final_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 3);
    for (const auto index : {0U, 1U, 2U}) {
        REQUIRE(stages->at(index).projection.outside_clip_requests.size() == 1);
        CHECK(stages->at(index).projection.outside_clip_requests.at(0).path.segments ==
              std::vector<std::string>{"final_residual"});
        CHECK(std::get<double>(
                  stages->at(index).projection.outside_clip_requests.at(0).args.at(0)) == 120.0);
    }
    stages->at(1).projection.notes.at(0)["pitch"] = 100;
    CHECK(previous.notes == previous_notes);
    CHECK(desired.notes == desired_notes);
    CHECK(stages->at(2).projection.notes.at(0).at("pitch") == 62);
    exact_geometry(previous, 8.0, 4, 4);
    exact_geometry(desired, 12.0, 3, 8);
}

TEST_CASE("Managed staging supports empty projections without inventing attacks",
          "[managed][clip-revision]") {
    auto empty = old_part();
    empty.notes = json::array();
    auto stages = plan_managed_clip_revision(empty, {}, empty, {});
    REQUIRE(stages);
    CHECK(stages->empty());
    stages = plan_managed_clip_revision(empty, {}, final_part(), final_keys);
    REQUIRE(stages);
    REQUIRE(stages->size() == 3);
    CHECK(stages->at(0).projection.notes.empty());
    CHECK(stages->at(0).desired_note_keys.empty());
    empty.clip_end = 1.0;
    stages = plan_managed_clip_revision(final_part(), final_keys, empty, {});
    REQUIRE(stages);
    REQUIRE(stages->size() == 2);
    CHECK(stages->at(0).projection.notes.empty());
    exact_geometry(stages->at(0).projection, 12.0, 3, 8);
    exact_geometry(stages->at(1).projection, 1.0, 4, 4);
}
