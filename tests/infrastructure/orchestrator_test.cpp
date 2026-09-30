/**
 * @file orchestrator_test.cpp
 * @brief Operation Orchestrator tests
 *
 *
 * Tests default state, progression/rhythm operations,
 * undo/redo, history management, and message drain.
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/orchestrator.hpp>

using namespace sunny::infrastructure;

// =============================================================================
// Default State
// =============================================================================

TEST_CASE("Orchestrator default state", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
    REQUIRE(orch.pending_message_count() == 0);
}

// =============================================================================
// create_progression_clip
// =============================================================================

TEST_CASE("create_progression_clip with valid inputs", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(0, 0, "C", "major", {"I", "IV", "V", "I"}, 4, 4.0);

    REQUIRE(result.success);
    REQUIRE_FALSE(result.operation_id.empty());
    REQUIRE(orch.pending_message_count() > 0);
    REQUIRE(orch.can_undo());

    const auto messages = orch.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(messages[1].notes[0].start_time == sunny::core::Beat::zero());
    CHECK(messages[1].notes[0].duration == sunny::core::Beat{9, 40});
}

TEST_CASE("create_progression_clip with invalid root", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(0, 0, "Z", "major", {"I", "IV"}, 4, 4.0);

    REQUIRE_FALSE(result.success);
    REQUIRE(orch.pending_message_count() == 0);
}

TEST_CASE("create_progression_clip with invalid scale", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(0, 0, "C", "nonexistent_scale", {"I", "IV"}, 4, 4.0);

    REQUIRE_FALSE(result.success);
    REQUIRE(orch.pending_message_count() == 0);
}

TEST_CASE("create_progression_clip rejects a numeral beyond the scale's degree count",
          "[infrastructure][orchestrator][scale]") {
    Orchestrator orch;

    // Pentatonic major has five degrees, so VI has no scale root; it must not
    // resolve to the tonic through the unused slots of the interval array.
    auto result = orch.create_progression_clip(0, 0, "C", "pentatonic_major", {"I", "VI"}, 4, 4.0);

    CHECK_FALSE(result.success);
    CHECK(result.message.find("VI") != std::string::npos);
    CHECK(orch.pending_message_count() == 0);
    CHECK_FALSE(orch.can_undo());
}

TEST_CASE("create_progression_clip roots each numeral on the scale degree",
          "[infrastructure][orchestrator][scale]") {
    Orchestrator orch;

    // Pentatonic major on C: degrees C D E G A, so V is rooted on A (69).
    REQUIRE(orch.create_progression_clip(0, 0, "C", "pentatonic_major", {"V"}, 4, 4.0).success);
    const auto messages = orch.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(static_cast<int>(messages[1].notes[0].pitch) == 69);
}

// =============================================================================
// apply_euclidean_rhythm
// =============================================================================

TEST_CASE("apply_euclidean_rhythm with valid inputs", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.apply_euclidean_rhythm(0, 0, 3, 8, 60, 0.25);

    REQUIRE(result.success);
    REQUIRE(orch.pending_message_count() > 0);
    REQUIRE(orch.can_undo());

    const auto messages = orch.drain_messages();
    REQUIRE(messages.size() == 2);
    REQUIRE_FALSE(messages[1].notes.empty());
    CHECK(messages[1].notes[0].duration == sunny::core::Beat{1, 20});
}

// =============================================================================
// Undo / Redo
// =============================================================================

TEST_CASE("undo/redo round-trip", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    // Empty undo/redo
    REQUIRE_FALSE(orch.undo());
    REQUIRE_FALSE(orch.redo());

    // Create an operation
    REQUIRE(orch.create_progression_clip(0, 0, "C", "major", {"I", "V"}, 4, 4.0).success);
    (void)orch.drain_messages(); // Clear messages

    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());

    // Undo
    REQUIRE(orch.undo());
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE(orch.can_redo());
    auto undo_messages = orch.drain_messages();
    REQUIRE(undo_messages.size() == 1);
    CHECK(undo_messages[0].args == std::vector<std::string>{"delete_clip"});

    // Redo
    REQUIRE(orch.redo());
    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
    auto redo_messages = orch.drain_messages();
    REQUIRE(redo_messages.size() == 2);
    CHECK(redo_messages[0].type == BridgeMessageType::CreateClip);
    CHECK(redo_messages[1].type == BridgeMessageType::AddNotes);
}

TEST_CASE("Euclidean undo and redo emit compensating commands", "[infrastructure][orchestrator]") {
    Orchestrator orch;
    REQUIRE(orch.apply_euclidean_rhythm(0, 2, 3, 8, 60, 0.25).success);
    (void)orch.drain_messages();

    REQUIRE(orch.undo());
    auto undo_messages = orch.drain_messages();
    REQUIRE(undo_messages.size() == 1);
    CHECK(undo_messages[0].args == std::vector<std::string>{"delete_clip"});

    REQUIRE(orch.redo());
    auto redo_messages = orch.drain_messages();
    REQUIRE(redo_messages.size() == 2);
    CHECK(redo_messages[0].type == BridgeMessageType::CreateClip);
    CHECK(redo_messages[1].type == BridgeMessageType::AddNotes);
}

TEST_CASE("live-operation inputs reject invalid coordinates and time",
          "[infrastructure][orchestrator]") {
    Orchestrator orch;
    CHECK_FALSE(orch.create_progression_clip(-1, 0, "C", "major", {"I"}, 4, 4.0).success);
    CHECK_FALSE(orch.apply_euclidean_rhythm(0, -1, 3, 8, 60, 0.25).success);
    CHECK_FALSE(orch.apply_arpeggio(0, 0, {"I"}, "sideways", 0.25).success);
    CHECK_FALSE(orch.apply_arpeggio(0, 0, {"I"}, "up", 0.0).success);
    CHECK_FALSE(orch.create_progression_clip(
                        0, 0, "C", "major", {"I"}, 4, std::numeric_limits<double>::infinity())
                    .success);
    CHECK_FALSE(
        orch.apply_euclidean_rhythm(0, 0, 3, 8, 60, std::numeric_limits<double>::max()).success);
    CHECK_FALSE(orch.apply_arpeggio(0, 0, {"I"}, "up", std::numeric_limits<double>::max()).success);
    CHECK(orch.pending_message_count() == 0);
}

// =============================================================================
// History Management
// =============================================================================

TEST_CASE("clear_history", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    REQUIRE(orch.create_progression_clip(0, 0, "C", "major", {"I"}, 4, 4.0).success);
    REQUIRE(orch.can_undo());

    orch.clear_history();
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
}

TEST_CASE("set_max_undo_levels", "[infrastructure][orchestrator]") {
    Orchestrator orch;
    orch.set_max_undo_levels(2);

    REQUIRE(orch.create_progression_clip(0, 0, "C", "major", {"I"}, 4, 4.0).success);
    (void)orch.drain_messages();
    REQUIRE(orch.create_progression_clip(0, 1, "C", "major", {"IV"}, 4, 4.0).success);
    (void)orch.drain_messages();
    REQUIRE(orch.create_progression_clip(0, 2, "C", "major", {"V"}, 4, 4.0).success);
    (void)orch.drain_messages();

    // Only 2 undo levels
    REQUIRE(orch.undo());
    REQUIRE(orch.undo());
    REQUIRE_FALSE(orch.undo());
}

// =============================================================================
// Message Queue
// =============================================================================

TEST_CASE("drain_messages returns pending and clears", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    REQUIRE(orch.create_progression_clip(0, 0, "C", "major", {"I", "V"}, 4, 4.0).success);
    REQUIRE(orch.pending_message_count() > 0);

    auto messages = orch.drain_messages();
    REQUIRE_FALSE(messages.empty());
    REQUIRE(orch.pending_message_count() == 0);

    // Second drain returns empty
    auto messages2 = orch.drain_messages();
    REQUIRE(messages2.empty());
}
