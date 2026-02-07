/**
 * @file WPOSC001A.h
 * @brief OSC 1.0 Wire Protocol Codec
 *
 * Component: WPOSC001A
 * Domain: IN (Infrastructure) | Category: WP (Wire Protocol)
 *
 * Minimal, allocation-free OSC 1.0 encoder/decoder.
 * No external dependencies.
 *
 * Invariants:
 * - Writer never heap-allocates; operates on caller-owned buffer
 * - All strings 4-byte aligned per OSC spec
 * - Reader validates packet structure; returns error on malformed input
 * - Encode-decode round-trip preserves all values exactly
 */

#pragma once

#include "Tensor/TNTP001A.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Sunny::Infrastructure {

// =============================================================================
// OSC Writer
// =============================================================================

/// Maximum arguments per OSC message
constexpr std::size_t OSC_MAX_ARGS = 64;

/**
 * @brief Fixed-buffer OSC message writer (no heap allocation)
 *
 * Two-phase design: add_*() methods capture arguments on the stack,
 * end_message() serialises address + type tags + data to the output buffer.
 *
 * Usage:
 *   std::array<std::byte, 1024> buf;
 *   OscWriter w(buf);
 *   w.begin_message("/some/address")
 *    .add_int32(42)
 *    .add_float32(3.14f)
 *    .add_string("hello")
 *    .end_message();
 *   auto packet = w.packet();
 *
 * Preconditions:
 * - begin_message() called before any add_*() calls
 * - end_message() called before packet()
 * - Buffer large enough for all data
 *
 * Postconditions:
 * - packet() returns a valid OSC message
 * - All strings are 4-byte aligned with null padding
 */
class OscWriter {
public:
    explicit OscWriter(std::span<std::byte> buffer);

    OscWriter& begin_message(std::string_view address);
    OscWriter& add_int32(std::int32_t v);
    OscWriter& add_float32(float v);
    OscWriter& add_string(std::string_view s);
    OscWriter& add_blob(std::span<const std::byte> data);
    OscWriter& end_message();

    [[nodiscard]] std::span<const std::byte> packet() const;
    [[nodiscard]] bool has_error() const { return error_; }
    [[nodiscard]] std::size_t bytes_written() const { return write_pos_; }

private:
    std::span<std::byte> buffer_;
    std::size_t write_pos_{0};

    // Deferred argument storage (stack-allocated)
    struct ArgEntry {
        char tag{'\0'};
        std::int32_t int_val{0};
        float float_val{0.0f};
        std::string_view str_val;
        std::span<const std::byte> blob_val;
    };
    std::array<ArgEntry, OSC_MAX_ARGS> args_{};
    std::size_t arg_count_{0};

    std::string_view address_;

    bool error_{false};
    bool message_started_{false};
    bool message_ended_{false};

    void write_padded_string(std::string_view s);
    void write_int32_be(std::int32_t v);
    void write_float32_be(float v);
    [[nodiscard]] static std::size_t align4(std::size_t pos);
    [[nodiscard]] bool has_space(std::size_t bytes) const;
};

// =============================================================================
// OSC Reader
// =============================================================================

/// OSC argument variant
struct OscArgument {
    enum class Type { Int32, Float32, String, Blob };
    Type type;
    std::int32_t int_value{0};
    float float_value{0.0f};
    std::string_view string_value;
    std::span<const std::byte> blob_value;
};

/**
 * @brief Zero-copy OSC message reader
 *
 * Usage:
 *   OscReader r(packet_bytes);
 *   if (!r.has_error()) {
 *       auto addr = r.address();
 *       auto& args = r.arguments();
 *   }
 *
 * Invariants:
 * - Does not modify or copy the input packet
 * - Returns error on malformed input (truncated, misaligned, invalid type tag)
 * - All string_view and span references point into the original packet
 */
class OscReader {
public:
    explicit OscReader(std::span<const std::byte> packet);

    [[nodiscard]] bool has_error() const { return error_; }
    [[nodiscard]] std::string_view address() const { return address_; }
    [[nodiscard]] std::string_view type_tag() const { return type_tag_; }
    [[nodiscard]] const std::vector<OscArgument>& arguments() const { return arguments_; }

private:
    std::span<const std::byte> packet_;
    std::string_view address_;
    std::string_view type_tag_;
    std::vector<OscArgument> arguments_;
    bool error_{false};

    void parse();
    [[nodiscard]] static std::size_t align4(std::size_t pos);
    std::string_view read_string(std::size_t& pos);
    std::int32_t read_int32(std::size_t& pos);
    float read_float32(std::size_t& pos);
};

}  // namespace Sunny::Infrastructure
