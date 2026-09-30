/**
 * @file control_transfer.hpp
 * @brief Shared bounded message-to-audio control transfer
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <sunny/core/types/music_types.hpp>
#include <sunny/render/signal_block.hpp>
#include <type_traits>
#include <utility>

namespace sunny::max {

inline constexpr std::size_t MAX_CONTROL_QUEUE_CAPACITY = 64;

/**
 * @brief Observable facts shared by bounded Max audio adapters.
 *
 * Counters are monotone for the lifetime of the adapter. `last_error` remains set until
 * `clear_error()`; successful work does not erase earlier failure evidence.
 */
struct ControlAdapterStatus {
    bool configured = false;
    double sample_rate = 0.0;
    std::int64_t maximum_frames = 0;
    std::uint64_t setup_failures = 0;
    std::uint64_t controls_enqueued = 0;
    std::uint64_t controls_applied = 0;
    std::uint64_t controls_rejected = 0;
    std::size_t controls_pending = 0;
    std::int64_t process_calls = 0;
    std::int64_t last_frame_count = 0;
    std::uint64_t process_failures = 0;
    sunny::core::ErrorCode last_error = sunny::core::ErrorCode::Ok;
};

// Preserve the established public name for modulation-adapter callers.
using ModulationAdapterStatus = ControlAdapterStatus;

namespace detail {

/** Validate a traditional zero-input/mono-output callback before pointer selection. */
[[nodiscard]] sunny::core::VoidResult
validate_generator_call(const sunny::render::SignalBlockContext& context,
                        std::int64_t frame_count,
                        std::int64_t input_count,
                        std::int64_t output_count,
                        double* const* outputs) noexcept;

/**
 * Silence one traditional zero-input/mono-output callback against the maximum
 * supplied by the current `dsp64` invocation. This path deliberately does not
 * depend on an admitted processor context, so a rejected chain rebuild cannot
 * fall back to processing or validating against stale setup state.
 */
[[nodiscard]] sunny::core::VoidResult silence_generator_output(std::int64_t maximum_frames,
                                                               std::int64_t frame_count,
                                                               std::int64_t input_count,
                                                               std::int64_t output_count,
                                                               double* const* outputs) noexcept;

template <typename T, std::size_t Capacity> class SerializedSpscQueue {
    static_assert(Capacity > 0);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "Max audio transfer requires lock-free size_t atomics");

  public:
    template <typename BeforePublish>
    [[nodiscard]] bool try_push(const T& value, BeforePublish&& before_publish) noexcept(
        noexcept(std::forward<BeforePublish>(before_publish)())) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_acquire);
        if (head - tail == Capacity) return false;
        entries_[head % Capacity] = value;
        std::forward<BeforePublish>(before_publish)();
        pending_.fetch_add(1, std::memory_order_release);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& value) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto head = head_.load(std::memory_order_acquire);
        if (tail == head) return false;
        value = entries_[tail % Capacity];
        pending_.fetch_sub(1, std::memory_order_release);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t pending() const noexcept {
        return pending_.load(std::memory_order_acquire);
    }

    /** Requires the producer and consumer to be quiescent for the whole visit. */
    template <typename Visitor>
    void for_each_pending(Visitor&& visitor) const noexcept(noexcept(visitor(entries_[0]))) {
        const auto tail = tail_.load(std::memory_order_acquire);
        const auto head = head_.load(std::memory_order_acquire);
        for (auto cursor = tail; cursor != head; ++cursor)
            visitor(entries_[cursor % Capacity]);
    }

  private:
    std::array<T, Capacity> entries_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::atomic<std::size_t> pending_{0};
};

class AdapterStatusState {
  public:
    void record_enqueued() noexcept;
    void record_applied() noexcept;
    void record_configuration(double sample_rate, std::int64_t maximum_frames) noexcept;
    void record_setup_failure(sunny::core::ErrorCode error) noexcept;
    void record_rejection(sunny::core::ErrorCode error) noexcept;
    void record_process_call(std::int64_t frame_count) noexcept;
    void record_process_failure(sunny::core::ErrorCode error) noexcept;
    void clear_error() noexcept;
    [[nodiscard]] ControlAdapterStatus snapshot() const noexcept;

  private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Max audio status requires lock-free uint64 atomics");
    static_assert(std::atomic<std::int64_t>::is_always_lock_free,
                  "Max audio status requires lock-free int64 atomics");
    static_assert(std::atomic<int>::is_always_lock_free,
                  "Max audio status requires lock-free integer atomics");

    std::atomic<std::uint64_t> configuration_generation_{0};
    std::atomic<bool> configured_{false};
    std::atomic<std::uint64_t> sample_rate_bits_{0};
    std::atomic<std::int64_t> maximum_frames_{0};
    std::atomic<std::uint64_t> setup_failures_{0};
    std::atomic<std::uint64_t> controls_enqueued_{0};
    std::atomic<std::uint64_t> controls_applied_{0};
    std::atomic<std::uint64_t> controls_rejected_{0};
    std::atomic<std::int64_t> process_calls_{0};
    std::atomic<std::int64_t> last_frame_count_{0};
    std::atomic<std::uint64_t> process_failures_{0};
    std::atomic<int> last_error_{static_cast<int>(sunny::core::ErrorCode::Ok)};
};

} // namespace detail

} // namespace sunny::max
