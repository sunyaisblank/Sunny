#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace sunny::infrastructure {

inline constexpr auto MCP_REQUEST_TIMEOUT = std::chrono::seconds(120);

/** One request's lifetime. Cancellation never establishes that a native effect did not run. */
struct RequestControl {
    const std::string identity;
    const std::string method;
    const std::chrono::steady_clock::time_point deadline;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> input_closed{false};

    explicit RequestControl(std::string id,
                            std::string operation,
                            std::chrono::steady_clock::time_point until =
                                std::chrono::steady_clock::now() + MCP_REQUEST_TIMEOUT)
        : identity(std::move(id)), method(std::move(operation)), deadline(until) {}

    [[nodiscard]] bool expired() const { return std::chrono::steady_clock::now() >= deadline; }
    [[nodiscard]] bool stop_requested() const {
        return cancelled.load(std::memory_order_acquire) || expired();
    }
};

inline thread_local std::shared_ptr<RequestControl> current_request_control_value;

[[nodiscard]] inline std::shared_ptr<RequestControl> current_request_control() {
    return current_request_control_value;
}

class RequestControlScope {
  public:
    explicit RequestControlScope(std::shared_ptr<RequestControl> control)
        : previous_(std::move(current_request_control_value)) {
        current_request_control_value = std::move(control);
    }
    ~RequestControlScope() { current_request_control_value = std::move(previous_); }
    RequestControlScope(const RequestControlScope&) = delete;
    RequestControlScope& operator=(const RequestControlScope&) = delete;

  private:
    std::shared_ptr<RequestControl> previous_;
};

[[nodiscard]] inline bool request_stop_requested() {
    const auto control = current_request_control();
    return control && control->stop_requested();
}

/** EOF still permits local/read-only drain, but it cannot authorize another native mutation. */
[[nodiscard]] inline bool native_request_revoked() {
    const auto control = current_request_control();
    return control &&
           (control->stop_requested() || control->input_closed.load(std::memory_order_acquire));
}

} // namespace sunny::infrastructure
