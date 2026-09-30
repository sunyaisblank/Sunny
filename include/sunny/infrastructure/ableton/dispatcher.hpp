/**
 * @file dispatcher.hpp
 * @brief Bridge Message Dispatcher
 *
 *
 * Translates the Orchestrator's queued BridgeMessages into LomRequests
 * and delivers them through a LomTransport. This closes the seam between
 * the in-memory Orchestrator queue and the LOM wire protocol
 * (LomProtocol / LomTransport): without a dispatcher the queue
 * accumulates and nothing reaches Ableton.
 *
 * Failure philosophy: when the transport is absent or disconnected the
 * dispatcher reports offline and delivers nothing; callers decline the
 * operation loudly rather than pretending an in-memory mutation reached
 * the DAW.
 */

#pragma once

#include <string>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <vector>

namespace sunny::infrastructure {

/// Outcome of dispatching a batch of bridge messages
struct DispatchReport {
    std::size_t sent{0};
    std::size_t failed{0};
    std::vector<std::string> errors;

    [[nodiscard]] bool all_ok() const { return failed == 0; }
};

/**
 * @brief Delivers Orchestrator bridge messages over a LomTransport
 *
 * Pre:  transport may be null (offline mode) or connected
 * Post: dispatch() delivers every translatable message in order and
 *       reports per-message failures; no message is silently dropped
 */
class BridgeDispatcher {
  public:
    /// Construct with a borrowed transport; nullptr means offline
    explicit BridgeDispatcher(LomTransport* transport = nullptr) : transport_(transport) {}

    /// True when a connected transport is available
    [[nodiscard]] bool online() { return transport_ != nullptr && transport_->ensure_connected(); }

    /// Send a single request, reconnecting first when possible.
    [[nodiscard]] LomResponse request(const LomRequest& request);

    /// Probe the connected host's explicit version and capability model.
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetProfile>> target_profile();

    /**
     * @brief Translate and send a batch of messages in queue order
     *
     * Pre:  online() is true (callers decline before queueing otherwise)
     * Post: every message was sent and acknowledged, or its failure is
     *       recorded in the report with the transport's error text
     */
    [[nodiscard]] DispatchReport dispatch(const std::vector<BridgeMessage>& messages);

    /**
     * @brief Translate one non-AddNotes bridge message to a LomRequest
     *
     * AddNotes carries note payloads and maps to LomTransport::send_notes
     * instead; passing one here returns a CallMethod request without the
     * note data and is a caller error guarded in dispatch().
     */
    [[nodiscard]] static sunny::core::Result<LomRequest> to_lom_request(const BridgeMessage& msg);

  private:
    LomTransport* transport_;
};

} // namespace sunny::infrastructure
