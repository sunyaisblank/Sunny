/**
 * @file TSLF001A.cpp
 * @brief Unit tests for RTLF001A (Lock-Free SPSC Ring Buffer)
 *
 * Component: TSLF001A
 * Domain: TS (Test) | Category: LF (Lock-Free)
 *
 * Tests: RTLF001A
 * Coverage: SPSC correctness, capacity bounds, AtomicParameter,
 *           concurrent push/pop
 */

#include <catch2/catch_test_macros.hpp>

#include "RealTime/RTLF001A.h"

#include <thread>
#include <vector>

using namespace Sunny::Infrastructure;

// =============================================================================
// SpscRingBuffer Basic Tests
// =============================================================================

TEST_CASE("RTLF001A: empty ring buffer", "[lockfree][spsc]") {
    SpscRingBuffer<int, 4> ring;

    CHECK(ring.empty());
    CHECK(ring.size() == 0);
    CHECK(ring.capacity() == 4);

    int val;
    CHECK_FALSE(ring.try_pop(val));
}

TEST_CASE("RTLF001A: push and pop single element", "[lockfree][spsc]") {
    SpscRingBuffer<int, 4> ring;

    REQUIRE(ring.try_push(42));
    CHECK(ring.size() == 1);
    CHECK_FALSE(ring.empty());

    int val = 0;
    REQUIRE(ring.try_pop(val));
    CHECK(val == 42);
    CHECK(ring.empty());
}

TEST_CASE("RTLF001A: FIFO ordering", "[lockfree][spsc]") {
    SpscRingBuffer<int, 8> ring;

    for (int i = 0; i < 5; ++i) {
        REQUIRE(ring.try_push(i * 10));
    }

    for (int i = 0; i < 5; ++i) {
        int val;
        REQUIRE(ring.try_pop(val));
        CHECK(val == i * 10);
    }
}

TEST_CASE("RTLF001A: full buffer rejects push", "[lockfree][spsc]") {
    SpscRingBuffer<int, 4> ring;

    // Fill to capacity
    for (int i = 0; i < 4; ++i) {
        REQUIRE(ring.try_push(i));
    }
    CHECK(ring.size() == 4);

    // Should reject
    CHECK_FALSE(ring.try_push(999));
    CHECK(ring.size() == 4);
}

TEST_CASE("RTLF001A: wrap-around correctness", "[lockfree][spsc]") {
    SpscRingBuffer<int, 4> ring;

    // Fill and drain several times to exercise wrap-around
    for (int cycle = 0; cycle < 5; ++cycle) {
        for (int i = 0; i < 4; ++i) {
            REQUIRE(ring.try_push(cycle * 100 + i));
        }
        for (int i = 0; i < 4; ++i) {
            int val;
            REQUIRE(ring.try_pop(val));
            CHECK(val == cycle * 100 + i);
        }
        CHECK(ring.empty());
    }
}

TEST_CASE("RTLF001A: interleaved push/pop", "[lockfree][spsc]") {
    SpscRingBuffer<int, 4> ring;

    REQUIRE(ring.try_push(1));
    REQUIRE(ring.try_push(2));

    int val;
    REQUIRE(ring.try_pop(val));
    CHECK(val == 1);

    REQUIRE(ring.try_push(3));
    REQUIRE(ring.try_push(4));
    REQUIRE(ring.try_push(5));

    REQUIRE(ring.try_pop(val));
    CHECK(val == 2);
    REQUIRE(ring.try_pop(val));
    CHECK(val == 3);
    REQUIRE(ring.try_pop(val));
    CHECK(val == 4);
    REQUIRE(ring.try_pop(val));
    CHECK(val == 5);

    CHECK(ring.empty());
}

TEST_CASE("RTLF001A: struct element type", "[lockfree][spsc]") {
    struct ParamUpdate {
        float value;
        float ramp_ms;
    };

    SpscRingBuffer<ParamUpdate, 8> ring;

    REQUIRE(ring.try_push({0.5f, 10.0f}));
    REQUIRE(ring.try_push({1.0f, 0.0f}));

    ParamUpdate u{};
    REQUIRE(ring.try_pop(u));
    CHECK(u.value == 0.5f);
    CHECK(u.ramp_ms == 10.0f);

    REQUIRE(ring.try_pop(u));
    CHECK(u.value == 1.0f);
    CHECK(u.ramp_ms == 0.0f);
}

// =============================================================================
// Concurrent Tests
// =============================================================================

TEST_CASE("RTLF001A: concurrent producer/consumer", "[lockfree][spsc]") {
    constexpr int COUNT = 10000;
    SpscRingBuffer<int, 256> ring;

    std::vector<int> received;
    received.reserve(COUNT);

    // Producer thread
    std::thread producer([&] {
        for (int i = 0; i < COUNT; ++i) {
            while (!ring.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    // Consumer (this thread)
    while (static_cast<int>(received.size()) < COUNT) {
        int val;
        if (ring.try_pop(val)) {
            received.push_back(val);
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();

    // Verify FIFO order and completeness
    REQUIRE(received.size() == COUNT);
    for (int i = 0; i < COUNT; ++i) {
        CHECK(received[i] == i);
    }
}

// =============================================================================
// AtomicParameter Tests
// =============================================================================

TEST_CASE("RTLF001A: AtomicParameter default value", "[lockfree][atomic]") {
    AtomicParameter<float> p;
    CHECK(p.load() == 0.0f);
}

TEST_CASE("RTLF001A: AtomicParameter initial value", "[lockfree][atomic]") {
    AtomicParameter<float> p{0.75f};
    CHECK(p.load() == 0.75f);
}

TEST_CASE("RTLF001A: AtomicParameter store and load", "[lockfree][atomic]") {
    AtomicParameter<int32_t> p{0};

    p.store(42);
    CHECK(p.load() == 42);

    p.store(-1);
    CHECK(p.load() == -1);
}

TEST_CASE("RTLF001A: AtomicParameter exchange", "[lockfree][atomic]") {
    AtomicParameter<float> p{1.0f};

    float old = p.exchange(2.0f);
    CHECK(old == 1.0f);
    CHECK(p.load() == 2.0f);
}

TEST_CASE("RTLF001A: AtomicParameter concurrent access", "[lockfree][atomic]") {
    AtomicParameter<int32_t> p{0};

    std::thread writer([&] {
        for (int i = 1; i <= 1000; ++i) {
            p.store(i);
        }
    });

    // Reader: just verify we never get a torn read
    int last = 0;
    int reads = 0;
    while (reads < 5000 && last < 1000) {
        int val = p.load();
        CHECK(val >= 0);
        CHECK(val <= 1000);
        if (val > last) {
            last = val;
        }
        ++reads;
        std::this_thread::yield();
    }

    writer.join();
    CHECK(p.load() == 1000);
}
