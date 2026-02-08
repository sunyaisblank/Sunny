/**
 * @file TSOR001A.cpp
 * @brief Tests for INOR001A (Operation Orchestrator)
 *
 * Component: TSOR001A
 * Domain: TS (Test) | Category: OR (Orchestrator)
 *
 * Tests default state, progression/rhythm operations,
 * undo/redo, history management, and message drain.
 */

#include <catch2/catch_test_macros.hpp>

#include "Application/INOR001A.h"

using namespace Sunny::Infrastructure;

// =============================================================================
// Default State
// =============================================================================

TEST_CASE("TSOR001A: Orchestrator default state", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
    REQUIRE(orch.pending_message_count() == 0);
}

// =============================================================================
// create_progression_clip
// =============================================================================

TEST_CASE("TSOR001A: create_progression_clip with valid inputs", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(
        0, 0, "C", "major", {"I", "IV", "V", "I"}, 4, 4.0
    );

    REQUIRE(result.success);
    REQUIRE_FALSE(result.operation_id.empty());
    REQUIRE(orch.pending_message_count() > 0);
    REQUIRE(orch.can_undo());
}

TEST_CASE("TSOR001A: create_progression_clip with invalid root", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(
        0, 0, "Z", "major", {"I", "IV"}, 4, 4.0
    );

    REQUIRE_FALSE(result.success);
    REQUIRE(orch.pending_message_count() == 0);
}

TEST_CASE("TSOR001A: create_progression_clip with invalid scale", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.create_progression_clip(
        0, 0, "C", "nonexistent_scale", {"I", "IV"}, 4, 4.0
    );

    REQUIRE_FALSE(result.success);
    REQUIRE(orch.pending_message_count() == 0);
}

// =============================================================================
// apply_euclidean_rhythm
// =============================================================================

TEST_CASE("TSOR001A: apply_euclidean_rhythm with valid inputs", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    auto result = orch.apply_euclidean_rhythm(0, 0, 3, 8, 60, 0.25);

    REQUIRE(result.success);
    REQUIRE(orch.pending_message_count() > 0);
}

// =============================================================================
// Undo / Redo
// =============================================================================

TEST_CASE("TSOR001A: undo/redo round-trip", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    // Empty undo/redo
    REQUIRE_FALSE(orch.undo());
    REQUIRE_FALSE(orch.redo());

    // Create an operation
    orch.create_progression_clip(0, 0, "C", "major", {"I", "V"}, 4, 4.0);
    orch.drain_messages();  // Clear messages

    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());

    // Undo
    REQUIRE(orch.undo());
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE(orch.can_redo());

    // Redo
    REQUIRE(orch.redo());
    REQUIRE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
}

// =============================================================================
// History Management
// =============================================================================

TEST_CASE("TSOR001A: clear_history", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    orch.create_progression_clip(0, 0, "C", "major", {"I"}, 4, 4.0);
    REQUIRE(orch.can_undo());

    orch.clear_history();
    REQUIRE_FALSE(orch.can_undo());
    REQUIRE_FALSE(orch.can_redo());
}

TEST_CASE("TSOR001A: set_max_undo_levels", "[infrastructure][orchestrator]") {
    Orchestrator orch;
    orch.set_max_undo_levels(2);

    orch.create_progression_clip(0, 0, "C", "major", {"I"}, 4, 4.0);
    orch.drain_messages();
    orch.create_progression_clip(0, 1, "C", "major", {"IV"}, 4, 4.0);
    orch.drain_messages();
    orch.create_progression_clip(0, 2, "C", "major", {"V"}, 4, 4.0);
    orch.drain_messages();

    // Only 2 undo levels
    REQUIRE(orch.undo());
    REQUIRE(orch.undo());
    REQUIRE_FALSE(orch.undo());
}

// =============================================================================
// Message Queue
// =============================================================================

TEST_CASE("TSOR001A: drain_messages returns pending and clears", "[infrastructure][orchestrator]") {
    Orchestrator orch;

    orch.create_progression_clip(0, 0, "C", "major", {"I", "V"}, 4, 4.0);
    REQUIRE(orch.pending_message_count() > 0);

    auto messages = orch.drain_messages();
    REQUIRE_FALSE(messages.empty());
    REQUIRE(orch.pending_message_count() == 0);

    // Second drain returns empty
    auto messages2 = orch.drain_messages();
    REQUIRE(messages2.empty());
}
