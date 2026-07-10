/**
 * @file INBR002A.h
 * @brief Bridge Message Dispatcher
 *
 * Component: INBR002A
 * Domain: IN (Infrastructure) | Category: BR (Bridge)
 *
 * Translates the Orchestrator's queued BridgeMessages into LomRequests
 * and delivers them through a LomTransport. This closes the seam between
 * the in-memory operation queue (INOR001A) and the wire protocol
 * (INBR001A / INTP001A): without a dispatcher the queue accumulates and
 * nothing reaches Ableton.
 *
 * Failure philosophy: when the transport is absent or disconnected the
 * dispatcher reports offline and delivers nothing; callers decline the
 * operation loudly rather than pretending an in-memory mutation reached
 * the DAW.
 */

#pragma once

#include "INBR001A.h"
#include "INTP001A.h"
#include "../Application/INOR001A.h"

#include <string>
#include <vector>

namespace Sunny::Infrastructure {

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
    explicit BridgeDispatcher(LomTransport* transport = nullptr)
        : transport_(transport) {}

    /// True when a connected transport is available
    [[nodiscard]] bool online() const {
        return transport_ != nullptr && transport_->is_connected();
    }

    /**
     * @brief Translate and send a batch of messages in queue order
     *
     * Pre:  online() is true (callers decline before queueing otherwise)
     * Post: every message was sent and acknowledged, or its failure is
     *       recorded in the report with the transport's error text
     */
    [[nodiscard]] DispatchReport dispatch(
        const std::vector<BridgeMessage>& messages);

    /**
     * @brief Translate one non-AddNotes bridge message to a LomRequest
     *
     * AddNotes carries note payloads and maps to LomTransport::send_notes
     * instead; passing one here returns a CallMethod request without the
     * note data and is a caller error guarded in dispatch().
     */
    [[nodiscard]] static LomRequest to_lom_request(const BridgeMessage& msg);

private:
    LomTransport* transport_;
};

}  // namespace Sunny::Infrastructure
