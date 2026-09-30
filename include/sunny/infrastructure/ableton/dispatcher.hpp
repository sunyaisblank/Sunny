/**
 * @file dispatcher.hpp
 * @brief Bridge Message Dispatcher
 *
 *
 * Implements the Orchestrator's BridgeDelivery contract over the LOM wire
 * protocol: each BridgeMessage becomes a LomRequest (or a note batch) sent
 * through a LomTransport.
 *
 * Failure philosophy: when the transport is absent or disconnected the
 * dispatcher reports offline and delivers nothing; callers decline the
 * operation loudly rather than pretending an in-memory mutation reached
 * the DAW. Delivery stops at the first failed message, because every later
 * message depends on the effects of the ones before it.
 */

#pragma once

#include <string>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/orchestrator.hpp>
#include <vector>

namespace sunny::infrastructure {

/**
 * @brief Delivers Orchestrator bridge messages over a LomTransport
 *
 * Pre:  transport may be null (offline mode) or connected
 * Post: dispatch() delivers messages in order up to the first failure and
 *       reports it; no message after a failure is sent
 */
class BridgeDispatcher final : public BridgeDelivery {
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
     * @brief Translate and send a batch of messages in order
     *
     * Pre:  online() is true (callers decline before delivering otherwise)
     * Post: report.sent messages were acknowledged; the next one failed with
     *       the transport's error text and nothing after it was sent;
     *       report.indeterminate is set when the failed message was sent
     *       without a valid response
     */
    [[nodiscard]] DispatchReport dispatch(const std::vector<BridgeMessage>& messages) override;

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
