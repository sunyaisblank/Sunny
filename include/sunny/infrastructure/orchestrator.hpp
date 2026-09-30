/**
 * @file orchestrator.hpp
 * @brief Operation Orchestrator
 *
 *
 * Coordinates high-level music operations:
 * - Progression generation with voice leading
 * - Rhythm application
 * - Transactional delivery with undo/redo history
 *
 * Every operation is delivered through a BridgeDelivery borrowed for the
 * duration of the call. History records only what the delivery
 * acknowledged, so undo reverts only effects an operation actually made.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
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

/// Outcome of delivering an ordered batch of bridge messages
struct DispatchReport {
    /// Length of the acknowledged prefix of the batch
    std::size_t sent{0};
    /// Messages not acknowledged: the one that failed and every later one,
    /// none of which was sent
    std::size_t failed{0};
    /// The failed message was sent but no valid response arrived, so
    /// whether it took effect is unknown
    bool indeterminate{false};
    std::vector<std::string> errors;

    [[nodiscard]] bool all_ok() const { return failed == 0; }
};

/**
 * @brief Delivery contract for bridge messages
 *
 * Pre:  messages are in the order their effects must be applied
 * Post: the messages in [0, sent) were acknowledged; delivery stopped at the
 *       first failure, so no message after it was sent
 */
class BridgeDelivery {
  public:
    virtual ~BridgeDelivery() = default;

    [[nodiscard]] virtual DispatchReport dispatch(const std::vector<BridgeMessage>& messages) = 0;
};

/**
 * @brief Acknowledges and records every message without executing it
 *
 * The delivery for offline planning (the Python binding) and for tests that
 * inspect the exact message sequence an operation produces.
 */
class RecordingDelivery final : public BridgeDelivery {
  public:
    [[nodiscard]] DispatchReport dispatch(const std::vector<BridgeMessage>& messages) override;

    /// Hand over every recorded message and clear the record
    [[nodiscard]] std::vector<BridgeMessage> drain_messages();
    [[nodiscard]] std::size_t pending_message_count() const { return recorded_.size(); }

  private:
    std::vector<BridgeMessage> recorded_;
};

/// What an operation, undo, or redo did to the Live set
enum class OperationOutcome : std::uint8_t {
    NotAttempted,     ///< Rejected before any message was sent; nothing changed
    Applied,          ///< Every message acknowledged; history updated
    NotApplied,       ///< The first message was refused; nothing changed
    RolledBack,       ///< A later message failed and the compensation was acknowledged
    PartiallyApplied, ///< A later message failed and the compensation failed too
    Indeterminate,    ///< A message's effect is unknown; inspect the set before retrying
};

[[nodiscard]] std::string_view to_string(OperationOutcome outcome) noexcept;

/// Orchestrator result
struct OrchestratorResult {
    OperationOutcome outcome{OperationOutcome::NotAttempted};
    std::string operation_id;
    std::string message;
    /// Messages acknowledged in the forward (or, for undo, inverse) delivery
    std::size_t commands_sent{0};
    /// Delivery and compensation errors in the order they occurred
    std::vector<std::string> errors;

    [[nodiscard]] bool success() const { return outcome == OperationOutcome::Applied; }
};

/**
 * @brief Operation Orchestrator
 *
 * Coordinates theory computation with Ableton operations. Thread-safe via an
 * internal mutex held for the whole of each call, delivery included, so
 * history always matches the deliveries that produced it.
 */
class Orchestrator {
  public:
    Orchestrator() = default;

    // High-level Live operations. Numeric beat arguments use the Live/MIDI
    // quarter-note coordinate and are converted to Sunny Beat internally.
    // Each is recorded in history only when every message was acknowledged.
    [[nodiscard]] OrchestratorResult
    create_progression_clip(BridgeDelivery& delivery,
                            int track_index,
                            int slot_index,
                            const std::string& root,
                            const std::string& scale,
                            const std::vector<std::string>& numerals,
                            int octave = 4,
                            double duration_beats = 4.0);

    [[nodiscard]] OrchestratorResult apply_euclidean_rhythm(BridgeDelivery& delivery,
                                                            int track_index,
                                                            int slot_index,
                                                            int pulses,
                                                            int steps,
                                                            sunny::core::MidiNote pitch,
                                                            double step_duration = 0.25);

    /// Arpeggiate each numeral's chord in turn in the given key; the clip
    /// loops with period (total steps) x step_duration.
    [[nodiscard]] OrchestratorResult apply_arpeggio(BridgeDelivery& delivery,
                                                    int track_index,
                                                    int slot_index,
                                                    const std::string& root,
                                                    const std::string& scale,
                                                    const std::vector<std::string>& numerals,
                                                    const std::string& direction,
                                                    double step_duration = 0.25);

    /// Revert the latest operation; it moves to the redo stack only when the
    /// inverse was acknowledged.
    [[nodiscard]] OrchestratorResult undo(BridgeDelivery& delivery);
    /// Reapply the latest undone operation; it moves to the undo stack only
    /// when every forward message was acknowledged.
    [[nodiscard]] OrchestratorResult redo(BridgeDelivery& delivery);
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    void clear_history();

    // Configuration
    void set_max_undo_levels(std::size_t levels);

  private:
    /// Invariant: inverse reverts the effects of any non-empty acknowledged
    /// prefix of forward_messages. Every operation opens by creating a clip
    /// that the later messages write into, so deleting that clip reverts
    /// any prefix; a single inverse message also makes partial undo
    /// impossible by construction.
    struct HistoryEntry {
        std::vector<BridgeMessage> forward_messages;
        BridgeMessage inverse;
    };

    std::deque<HistoryEntry> undo_stack_;
    std::deque<HistoryEntry> redo_stack_;
    std::size_t max_undo_levels_{100};
    std::uint64_t next_operation_id_{1};
    mutable std::mutex mutex_;

    std::string generate_operation_id();
    [[nodiscard]] static OrchestratorResult deliver_forward(BridgeDelivery& delivery,
                                                            const HistoryEntry& entry);
    [[nodiscard]] OrchestratorResult record_clip_operation(BridgeDelivery& delivery,
                                                           HistoryEntry entry,
                                                           std::string applied_message);
    void push_undo(HistoryEntry entry);
};

} // namespace sunny::infrastructure
