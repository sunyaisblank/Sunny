/**
 * @file INOR001A.h
 * @brief Operation Orchestrator
 *
 * Component: INOR001A
 * Domain: IN (Infrastructure) | Category: OR (Orchestrator)
 *
 * Coordinates high-level music operations:
 * - Progression generation with voice leading
 * - Rhythm application
 * - Undo/redo transaction management
 * - Message batching for Ableton bridge
 */

#pragma once

#include "Tensor/TNTP001A.h"
#include "Tensor/TNEV001A.h"
#include "Scale/SCDF001A.h"

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Sunny::Infrastructure {

/// Operation for undo/redo
struct Operation {
    std::string id;
    std::string description;
    std::function<void()> execute;
    std::function<void()> undo;
};

/// Message types for Ableton bridge
enum class BridgeMessageType {
    GetProperty,
    SetProperty,
    CallMethod,
    CreateClip,
    AddNotes,
    Batch
};

/// Bridge message
struct BridgeMessage {
    BridgeMessageType type;
    std::string path;
    std::vector<std::string> args;
    std::vector<Core::NoteEvent> notes;  // For AddNotes
};

/// Orchestrator result
struct OrchestratorResult {
    bool success;
    std::string operation_id;
    std::string message;
};

/**
 * @brief Operation Orchestrator
 *
 * Coordinates theory computation with Ableton operations.
 * Thread-safe via internal mutex.
 */
class Orchestrator {
public:
    Orchestrator();

    // High-level operations
    OrchestratorResult create_progression_clip(
        int track_index,
        int slot_index,
        const std::string& root,
        const std::string& scale,
        const std::vector<std::string>& numerals,
        int octave = 4,
        double duration_beats = 4.0
    );

    OrchestratorResult apply_euclidean_rhythm(
        int track_index,
        int slot_index,
        int pulses,
        int steps,
        Core::MidiNote pitch,
        double step_duration = 0.25
    );

    OrchestratorResult apply_arpeggio(
        int track_index,
        int slot_index,
        const std::vector<std::string>& numerals,
        const std::string& direction,
        double step_duration = 0.25
    );

    // Undo/redo
    bool undo();
    bool redo();
    bool can_undo() const;
    bool can_redo() const;
    void clear_history();

    // Message queue
    std::vector<BridgeMessage> drain_messages();
    std::size_t pending_message_count() const;

    // Configuration
    void set_max_undo_levels(std::size_t levels);

private:
    std::deque<Operation> undo_stack_;
    std::deque<Operation> redo_stack_;
    std::vector<BridgeMessage> pending_messages_;
    std::size_t max_undo_levels_{100};
    mutable std::mutex mutex_;

    std::string generate_operation_id();
    void push_operation(Operation op);
    void queue_message(BridgeMessage msg);
};

}  // namespace Sunny::Infrastructure
