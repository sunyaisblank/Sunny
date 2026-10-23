/**
 * @file orchestrator_test.cpp
 * @brief Operation Orchestrator tests
 *
 *
 * Tests default state, progression/rhythm operations,
 * undo/redo, history management, and message drain.
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <sunny/infrastructure/orchestrator.hpp>
#include <vector>

using namespace sunny::infrastructure;

// =============================================================================
// Default State
// =============================================================================

TEST_CASE("Orchestrator default state", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
    REQUIRE(delivery.pending_message_count() == 0);
}

// =============================================================================
// create_progression_clip
// =============================================================================

TEST_CASE("create_progression_clip with valid inputs", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    auto result =
        orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I", "IV", "V", "I"}, 4, 4.0);

    REQUIRE(result.success());
    REQUIRE_FALSE(result.operation_id.empty());
    REQUIRE(delivery.pending_message_count() > 0);
    REQUIRE(orch.can_undo());

    const auto messages = delivery.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(messages[1].notes[0].start_time == sunny::core::Beat::zero());
    CHECK(messages[1].notes[0].duration == sunny::core::Beat{9, 40});
}

TEST_CASE("create_progression_clip with invalid root", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    auto result = orch.create_progression_clip(delivery, 0, 0, "Z", "major", {"I", "IV"}, 4, 4.0);

    REQUIRE_FALSE(result.success());
    REQUIRE(delivery.pending_message_count() == 0);
}

TEST_CASE("create_progression_clip with invalid scale", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    auto result =
        orch.create_progression_clip(delivery, 0, 0, "C", "nonexistent_scale", {"I", "IV"}, 4, 4.0);

    REQUIRE_FALSE(result.success());
    REQUIRE(delivery.pending_message_count() == 0);
}

TEST_CASE("create_progression_clip rejects a numeral beyond the scale's degree count",
          "[infrastructure][orchestrator][scale]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    // Pentatonic major has five degrees, so VI has no scale root; it must not
    // resolve to the tonic through the unused slots of the interval array.
    auto result =
        orch.create_progression_clip(delivery, 0, 0, "C", "pentatonic_major", {"I", "VI"}, 4, 4.0);

    CHECK_FALSE(result.success());
    CHECK(result.message.find("VI") != std::string::npos);
    CHECK(delivery.pending_message_count() == 0);
    CHECK_FALSE(orch.can_undo());
}

TEST_CASE("create_progression_clip roots each numeral on the scale degree",
          "[infrastructure][orchestrator][scale]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    // Pentatonic major on C: degrees C D E G A, so V is rooted on A (69).
    REQUIRE(orch.create_progression_clip(delivery, 0, 0, "C", "pentatonic_major", {"V"}, 4, 4.0)
                .success());
    const auto messages = delivery.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(static_cast<int>(messages[1].notes[0].pitch) == 69);
}

TEST_CASE("apply_arpeggio rejects a numeral beyond the scale's degree count",
          "[infrastructure][orchestrator][scale][arpeggio]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    const auto result =
        orch.apply_arpeggio(delivery, 0, 0, "C", "pentatonic_minor", {"i", "vi"}, "up", 0.25);

    CHECK(result.outcome == OperationOutcome::NotAttempted);
    CHECK(result.message.find("vi") != std::string::npos);
    CHECK(delivery.pending_message_count() == 0);
}

TEST_CASE("apply_arpeggio walks the progression chord by chord in the requested key",
          "[infrastructure][orchestrator][arpeggio]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    // G major I-V descending: G-B-D (67 71 74) then D-F#-A (62 66 69), each high to low.
    REQUIRE(orch.apply_arpeggio(delivery, 1, 2, "G", "major", {"I", "V"}, "down", 0.5).success());

    const auto messages = delivery.drain_messages();
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].args == std::vector<std::string>{"3.000000"});
    std::vector<int> pitches;
    for (const auto& note : messages[1].notes)
        pitches.push_back(static_cast<int>(note.pitch));
    CHECK(pitches == std::vector<int>{74, 71, 67, 69, 66, 62});
    CHECK(messages[1].notes[3].start_time == sunny::core::Beat{3, 8});
}

// =============================================================================
// apply_euclidean_rhythm
// =============================================================================

TEST_CASE("apply_euclidean_rhythm with valid inputs", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    auto result = orch.apply_euclidean_rhythm(delivery, 0, 0, 3, 8, 60, 0.25);

    REQUIRE(result.success());
    REQUIRE(delivery.pending_message_count() > 0);
    REQUIRE(orch.can_undo());

    const auto messages = delivery.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(messages[1].notes[0].duration == sunny::core::Beat{1, 20});
}

// =============================================================================
// Undo / Redo
// =============================================================================

TEST_CASE("undo/redo round-trip", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    // Empty undo/redo
    REQUIRE_FALSE(orch.undo(delivery).success());
    REQUIRE_FALSE(orch.redo(delivery).success());

    // Create an operation
    REQUIRE(
        orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I", "V"}, 4, 4.0).success());
    (void)delivery.drain_messages(); // Clear messages

    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());

    // Undo
    REQUIRE(orch.undo(delivery).success());
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE(orch.can_redo());
    auto undo_messages = delivery.drain_messages();
    REQUIRE(undo_messages.size() == 1);
    CHECK(undo_messages[0].args == std::vector<std::string>{"delete_clip"});

    // Redo
    REQUIRE(orch.redo(delivery).success());
    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
    auto redo_messages = delivery.drain_messages();
    REQUIRE(redo_messages.size() == 2);
    CHECK(redo_messages[0].type == BridgeMessageType::CreateClip);
    CHECK(redo_messages[1].type == BridgeMessageType::AddNotes);
}

TEST_CASE("Euclidean undo and redo emit compensating commands", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;
    REQUIRE(orch.apply_euclidean_rhythm(delivery, 0, 2, 3, 8, 60, 0.25).success());
    (void)delivery.drain_messages();

    REQUIRE(orch.undo(delivery).success());
    auto undo_messages = delivery.drain_messages();
    REQUIRE(undo_messages.size() == 1);
    CHECK(undo_messages[0].args == std::vector<std::string>{"delete_clip"});

    REQUIRE(orch.redo(delivery).success());
    auto redo_messages = delivery.drain_messages();
    REQUIRE(redo_messages.size() == 2);
    CHECK(redo_messages[0].type == BridgeMessageType::CreateClip);
    CHECK(redo_messages[1].type == BridgeMessageType::AddNotes);
}

// =============================================================================
// Transactional delivery against a delivery that fails selected messages
// =============================================================================

namespace {

/// Records every message offered and fails the ones at chosen positions in
/// the overall message stream, stopping at the first failure as the
/// BridgeDelivery contract requires.
class ScriptedDelivery final : public BridgeDelivery {
  public:
    bool records_without_execution() const noexcept override { return true; }
    std::vector<BridgeMessage> offered;
    std::vector<std::size_t> failing_positions;
    bool indeterminate_failures{false};

    DispatchReport dispatch(const std::vector<BridgeMessage>& messages) override {
        DispatchReport report;
        for (const auto& message : messages) {
            if (report.failed > 0) {
                ++report.failed;
                continue;
            }
            offered.push_back(message);
            if (std::ranges::find(failing_positions, offered.size() - 1) !=
                failing_positions.end()) {
                ++report.failed;
                report.indeterminate = indeterminate_failures;
                report.errors.push_back(message.path + ": scripted failure");
            } else {
                ++report.sent;
            }
        }
        return report;
    }

    [[nodiscard]] bool offered_delete() const {
        return std::ranges::any_of(offered, [](const BridgeMessage& message) {
            return message.args == std::vector<std::string>{"delete_clip"};
        });
    }
};

} // namespace

TEST_CASE("a refused create_clip records no history and sends nothing more",
          "[infrastructure][orchestrator][undo]") {
    ScriptedDelivery delivery;
    delivery.failing_positions = {0};
    Orchestrator orch;

    const auto result = orch.apply_euclidean_rhythm(delivery, 0, 0, 3, 8, 60, 0.25);

    CHECK(result.outcome == OperationOutcome::NotApplied);
    CHECK(result.commands_sent == 0);
    CHECK(delivery.offered.size() == 1);
    CHECK_FALSE(delivery.offered_delete());
    CHECK_FALSE(orch.can_undo());
    CHECK_FALSE(orch.undo(delivery).success());
    CHECK(delivery.offered.size() == 1);
}

TEST_CASE("a failed note write deletes the created clip and records no history",
          "[infrastructure][orchestrator][undo]") {
    ScriptedDelivery delivery;
    delivery.failing_positions = {1};
    Orchestrator orch;

    const auto result = orch.create_progression_clip(delivery, 0, 3, "C", "major", {"I"}, 4, 4.0);

    CHECK(result.outcome == OperationOutcome::RolledBack);
    CHECK(result.commands_sent == 1);
    REQUIRE(delivery.offered.size() == 3);
    CHECK(delivery.offered[2].path == "song/tracks/0/clip_slots/3");
    CHECK(delivery.offered_delete());
    CHECK_FALSE(orch.can_undo());
}

TEST_CASE("an indeterminate note write whose compensation also fails is indeterminate",
          "[infrastructure][orchestrator][undo]") {
    ScriptedDelivery delivery;
    delivery.failing_positions = {1, 2};
    delivery.indeterminate_failures = true;
    Orchestrator orch;

    const auto result = orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I"}, 4, 4.0);

    CHECK(result.outcome == OperationOutcome::Indeterminate);
    CHECK(result.errors.size() == 2);
    CHECK_FALSE(orch.can_undo());
}

TEST_CASE("undo and redo move history only on acknowledged delivery",
          "[infrastructure][orchestrator][undo]") {
    ScriptedDelivery delivery;
    Orchestrator orch;
    REQUIRE(orch.apply_euclidean_rhythm(delivery, 0, 0, 3, 8, 60, 0.25).success());
    REQUIRE(orch.apply_euclidean_rhythm(delivery, 0, 1, 5, 8, 62, 0.25).success());

    // Position 4 is the first undo's delete_clip for slot 1.
    delivery.failing_positions = {4};
    CHECK(orch.undo(delivery).outcome == OperationOutcome::NotApplied);
    CHECK(orch.can_undo());
    CHECK_FALSE(orch.can_redo());

    REQUIRE(orch.undo(delivery).success());
    CHECK(delivery.offered.back().path == "song/tracks/0/clip_slots/1");
    CHECK(orch.can_redo());

    // Position 6 is the redo's create_clip; the entry must stay redoable.
    delivery.failing_positions = {6};
    CHECK(orch.redo(delivery).outcome == OperationOutcome::NotApplied);
    CHECK(orch.can_redo());

    REQUIRE(orch.redo(delivery).success());
    CHECK_FALSE(orch.can_redo());
    REQUIRE(orch.undo(delivery).success());
    REQUIRE(orch.undo(delivery).success());
    CHECK(delivery.offered.back().path == "song/tracks/0/clip_slots/0");
    CHECK_FALSE(orch.can_undo());
}

TEST_CASE("live-operation inputs reject invalid coordinates and time",
          "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;
    CHECK_FALSE(
        orch.create_progression_clip(delivery, -1, 0, "C", "major", {"I"}, 4, 4.0).success());
    CHECK_FALSE(orch.apply_euclidean_rhythm(delivery, 0, -1, 3, 8, 60, 0.25).success());
    CHECK_FALSE(
        orch.apply_arpeggio(delivery, 0, 0, "C", "major", {"I"}, "sideways", 0.25).success());
    CHECK_FALSE(orch.apply_arpeggio(delivery, 0, 0, "C", "major", {"I"}, "up", 0.0).success());
    CHECK_FALSE(
        orch.create_progression_clip(
                delivery, 0, 0, "C", "major", {"I"}, 4, std::numeric_limits<double>::infinity())
            .success());
    CHECK_FALSE(
        orch.apply_euclidean_rhythm(delivery, 0, 0, 3, 8, 60, std::numeric_limits<double>::max())
            .success());
    CHECK_FALSE(
        orch.apply_arpeggio(
                delivery, 0, 0, "C", "major", {"I"}, "up", std::numeric_limits<double>::max())
            .success());
    CHECK(delivery.pending_message_count() == 0);
}

// =============================================================================
// History Management
// =============================================================================

TEST_CASE("clear_history", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    REQUIRE(orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I"}, 4, 4.0).success());
    REQUIRE(orch.can_undo());

    orch.clear_history();
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
}

TEST_CASE("set_max_undo_levels", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;
    orch.set_max_undo_levels(2);

    REQUIRE(orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I"}, 4, 4.0).success());
    (void)delivery.drain_messages();
    REQUIRE(orch.create_progression_clip(delivery, 0, 1, "C", "major", {"IV"}, 4, 4.0).success());
    (void)delivery.drain_messages();
    REQUIRE(orch.create_progression_clip(delivery, 0, 2, "C", "major", {"V"}, 4, 4.0).success());
    (void)delivery.drain_messages();

    // Only 2 undo levels
    REQUIRE(orch.undo(delivery).success());
    REQUIRE(orch.undo(delivery).success());
    REQUIRE_FALSE(orch.undo(delivery).success());
}

// =============================================================================
// Message Queue
// =============================================================================

TEST_CASE("drain_messages returns pending and clears", "[infrastructure][orchestrator]") {
    RecordingDelivery delivery;
    Orchestrator orch;

    REQUIRE(
        orch.create_progression_clip(delivery, 0, 0, "C", "major", {"I", "V"}, 4, 4.0).success());
    REQUIRE(delivery.pending_message_count() > 0);

    auto messages = delivery.drain_messages();
    REQUIRE_FALSE(messages.empty());
    REQUIRE(delivery.pending_message_count() == 0);

    // Second drain returns empty
    auto messages2 = delivery.drain_messages();
    REQUIRE(messages2.empty());
}
