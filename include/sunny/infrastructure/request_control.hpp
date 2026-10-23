#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sunny::infrastructure {

inline constexpr auto MCP_REQUEST_TIMEOUT = std::chrono::seconds(120);

/** An immutable native identity observed before this request was admitted. */
struct NativeOrigin {
    std::string bridge_instance;
    std::string document_token;
    bool operator==(const NativeOrigin&) const = default;
};

[[nodiscard]] inline bool valid_native_origin(const NativeOrigin& origin) {
    const auto token = [](const std::string& value) {
        if (value.size() != 32) return false;
        for (const char c : value)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        return true;
    };
    return token(origin.bridge_instance) && token(origin.document_token);
}

/** One request's lifetime. Cancellation never establishes that a native effect did not run. */
struct RequestControl {
    const std::string identity;
    const std::string method;
    const std::chrono::steady_clock::time_point deadline;
    const std::optional<NativeOrigin> native_origin;
    const std::string tool_name;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> input_closed{false};

    explicit RequestControl(std::string id,
                            std::string operation,
                            std::chrono::steady_clock::time_point until =
                                std::chrono::steady_clock::now() + MCP_REQUEST_TIMEOUT,
                            std::optional<NativeOrigin> origin = std::nullopt,
                            std::string tool = {})
        : identity(std::move(id)), method(std::move(operation)), deadline(until),
          native_origin(std::move(origin)), tool_name(std::move(tool)) {}

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
