/**
 * @file orchestrator.hpp
 * @brief Operation Orchestrator
 *
 *
 * Coordinates high-level music operations:
 * - Progression generation with voice leading
 * - Rhythm application
 * - Undo/redo transaction management
 * - Message batching for Ableton bridge
 */

#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <string>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::infrastructure {

/// Message types for Ableton bridge
enum class BridgeMessageType : std::uint8_t {
    GetProperty,
    SetProperty,
    CallMethod,
    CreateClip,
    AddNotes,
    Batch
};

/// Bridge message
struct BridgeMessage {
    BridgeMessageType type{BridgeMessageType::CallMethod};
    std::string path;
    std::vector<std::string> args;
    std::vector<sunny::core::NoteEvent> notes; // For AddNotes
};

/// Orchestrator result
struct OrchestratorResult {
    bool success{false};
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
    Orchestrator() = default;

    // High-level Live operations. Numeric beat arguments use the Live/MIDI
    // quarter-note coordinate and are converted to Sunny Beat internally.
    [[nodiscard]] OrchestratorResult
    create_progression_clip(int track_index,
                            int slot_index,
                            const std::string& root,
                            const std::string& scale,
                            const std::vector<std::string>& numerals,
                            int octave = 4,
                            double duration_beats = 4.0);

    [[nodiscard]] OrchestratorResult apply_euclidean_rhythm(int track_index,
                                                            int slot_index,
                                                            int pulses,
                                                            int steps,
                                                            sunny::core::MidiNote pitch,
                                                            double step_duration = 0.25);

    [[nodiscard]] OrchestratorResult apply_arpeggio(int track_index,
                                                    int slot_index,
                                                    const std::vector<std::string>& numerals,
                                                    const std::string& direction,
                                                    double step_duration = 0.25);

    // Undo/redo
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    void clear_history();

    // Message queue
    [[nodiscard]] std::vector<BridgeMessage> drain_messages();
    [[nodiscard]] std::size_t pending_message_count() const;

    // Configuration
    void set_max_undo_levels(std::size_t levels);

  private:
    struct HistoryEntry {
        std::vector<BridgeMessage> forward_messages;
        std::vector<BridgeMessage> inverse_messages;
    };

    std::deque<HistoryEntry> undo_stack_;
    std::deque<HistoryEntry> redo_stack_;
    std::vector<BridgeMessage> pending_messages_;
    std::size_t max_undo_levels_{100};
    std::uint64_t next_operation_id_{1};
    mutable std::mutex mutex_;

    std::string generate_operation_id();
    void push_history(HistoryEntry entry);
    void queue_messages(std::span<const BridgeMessage> messages);
};

} // namespace sunny::infrastructure
