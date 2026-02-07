/**
 * @file RTLF001A.h
 * @brief Lock-Free SPSC Ring Buffer and Atomic Parameter
 *
 * Component: RTLF001A
 * Domain: IN (Infrastructure) | Category: RT (RealTime)
 *
 * Header-only, templated single-producer single-consumer ring buffer
 * for audio thread communication. No system calls, no locks, no
 * allocation after construction.
 *
 * Invariants:
 * - Cache-line alignment on read/write positions (no false sharing)
 * - Memory ordering: release on push, acquire on pop
 * - Power-of-two capacity enforced at compile time
 * - No operations forbidden on the audio thread (Bencina rules):
 *   no malloc, no locks, no syscalls, no unbounded loops
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace Sunny::Infrastructure {

// =============================================================================
// SPSC Ring Buffer
// =============================================================================

/**
 * @brief Single-Producer Single-Consumer lock-free ring buffer
 *
 * @tparam T Element type (must be trivially copyable for audio safety)
 * @tparam Capacity Must be a power of two
 *
 * Usage:
 *   SpscRingBuffer<float, 256> ring;
 *
 *   // Producer (control thread):
 *   ring.try_push(1.0f);
 *
 *   // Consumer (audio thread):
 *   float val;
 *   if (ring.try_pop(val)) { ... }
 *
 * Preconditions:
 * - Exactly one thread calls try_push (producer)
 * - Exactly one thread calls try_pop (consumer)
 * - Capacity is a power of two (compile-time enforced)
 *
 * Postconditions:
 * - Elements dequeued in FIFO order
 * - No data loss unless buffer is full (try_push returns false)
 */
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    SpscRingBuffer() = default;

    // Non-copyable, non-movable (contains atomics)
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    /**
     * @brief Push an element (producer thread only)
     * @return true if element was enqueued, false if buffer is full
     */
    bool try_push(const T& item) {
        const auto w = write_pos_.load(std::memory_order_relaxed);
        const auto r = read_pos_.load(std::memory_order_acquire);

        if (w - r >= Capacity) {
            return false;  // Full
        }

        buffer_[w & mask_] = item;
        write_pos_.store(w + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop an element (consumer thread only)
     * @return true if element was dequeued, false if buffer is empty
     */
    bool try_pop(T& item) {
        const auto r = read_pos_.load(std::memory_order_relaxed);
        const auto w = write_pos_.load(std::memory_order_acquire);

        if (r == w) {
            return false;  // Empty
        }

        item = buffer_[r & mask_];
        read_pos_.store(r + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Approximate element count
     *
     * Safe to call from either thread; result is approximate since
     * the other thread may be concurrently pushing or popping.
     */
    [[nodiscard]] std::size_t size() const {
        const auto w = write_pos_.load(std::memory_order_acquire);
        const auto r = read_pos_.load(std::memory_order_acquire);
        return w - r;
    }

    [[nodiscard]] bool empty() const { return size() == 0; }

    [[nodiscard]] constexpr std::size_t capacity() const { return Capacity; }

private:
    static constexpr std::size_t mask_ = Capacity - 1;

    // Cache-line alignment prevents false sharing between producer and consumer
#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t cache_line = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t cache_line = 64;
#endif

    alignas(cache_line) std::atomic<std::size_t> write_pos_{0};
    alignas(cache_line) std::atomic<std::size_t> read_pos_{0};
    alignas(cache_line) std::array<T, Capacity> buffer_{};
};

// =============================================================================
// Atomic Parameter
// =============================================================================

/**
 * @brief Atomic parameter store for real-time access
 *
 * @tparam T Must be lock-free atomically accessible (typically float, int32_t)
 *
 * Usage:
 *   AtomicParameter<float> volume{1.0f};
 *
 *   // Control thread:
 *   volume.store(0.5f);
 *
 *   // Audio thread:
 *   float v = volume.load();
 *
 * Invariants:
 * - load() and store() are lock-free (compile-time enforced)
 * - No memory allocation in any operation
 */
template <typename T>
class AtomicParameter {
    static_assert(std::atomic<T>::is_always_lock_free,
                  "AtomicParameter requires a lock-free atomic type");

public:
    AtomicParameter() = default;
    explicit AtomicParameter(T initial) : value_(initial) {}

    // Non-copyable, non-movable
    AtomicParameter(const AtomicParameter&) = delete;
    AtomicParameter& operator=(const AtomicParameter&) = delete;

    /// Store value (control thread)
    void store(T value) {
        value_.store(value, std::memory_order_release);
    }

    /// Load value (audio thread — lock-free, no syscalls)
    [[nodiscard]] T load() const {
        return value_.load(std::memory_order_acquire);
    }

    /// Exchange value, return previous (control thread)
    T exchange(T value) {
        return value_.exchange(value, std::memory_order_acq_rel);
    }

private:
    std::atomic<T> value_{};
};

}  // namespace Sunny::Infrastructure
