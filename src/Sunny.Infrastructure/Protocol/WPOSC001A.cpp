/**
 * @file WPOSC001A.cpp
 * @brief OSC 1.0 Wire Protocol Codec implementation
 *
 * Component: WPOSC001A
 *
 * Two-phase serialisation:
 * 1. add_*() captures arguments on the stack
 * 2. end_message() writes address + type tags + argument data to the buffer
 *
 * This avoids needing to know the type tag string length before writing
 * argument data, keeping the implementation simple and allocation-free.
 */

#include "WPOSC001A.h"

#include <cstring>

namespace Sunny::Infrastructure {

// =============================================================================
// OscWriter
// =============================================================================

OscWriter::OscWriter(std::span<std::byte> buffer)
    : buffer_(buffer) {}

OscWriter& OscWriter::begin_message(std::string_view address) {
    if (message_started_) {
        error_ = true;
        return *this;
    }

    if (address.empty() || address[0] != '/') {
        error_ = true;
        return *this;
    }

    message_started_ = true;
    write_pos_ = 0;
    arg_count_ = 0;
    address_ = address;

    return *this;
}

OscWriter& OscWriter::add_int32(std::int32_t v) {
    if (error_ || !message_started_ || message_ended_) {
        error_ = true;
        return *this;
    }
    if (arg_count_ >= OSC_MAX_ARGS) {
        error_ = true;
        return *this;
    }

    auto& arg = args_[arg_count_++];
    arg.tag = 'i';
    arg.int_val = v;
    return *this;
}

OscWriter& OscWriter::add_float32(float v) {
    if (error_ || !message_started_ || message_ended_) {
        error_ = true;
        return *this;
    }
    if (arg_count_ >= OSC_MAX_ARGS) {
        error_ = true;
        return *this;
    }

    auto& arg = args_[arg_count_++];
    arg.tag = 'f';
    arg.float_val = v;
    return *this;
}

OscWriter& OscWriter::add_string(std::string_view s) {
    if (error_ || !message_started_ || message_ended_) {
        error_ = true;
        return *this;
    }
    if (arg_count_ >= OSC_MAX_ARGS) {
        error_ = true;
        return *this;
    }

    auto& arg = args_[arg_count_++];
    arg.tag = 's';
    arg.str_val = s;
    return *this;
}

OscWriter& OscWriter::add_blob(std::span<const std::byte> data) {
    if (error_ || !message_started_ || message_ended_) {
        error_ = true;
        return *this;
    }
    if (arg_count_ >= OSC_MAX_ARGS) {
        error_ = true;
        return *this;
    }

    auto& arg = args_[arg_count_++];
    arg.tag = 'b';
    arg.blob_val = data;
    return *this;
}

OscWriter& OscWriter::end_message() {
    if (error_ || !message_started_ || message_ended_) {
        error_ = true;
        return *this;
    }

    // Phase 2: write everything to buffer

    // 1. Address string
    write_padded_string(address_);
    if (error_) return *this;

    // 2. Type tag string: "," + one char per argument + null + padding
    {
        std::size_t tag_len = 1 + arg_count_;  // comma + tags
        std::size_t padded_tag_len = align4(tag_len + 1);  // +1 for null
        if (!has_space(padded_tag_len)) {
            error_ = true;
            return *this;
        }

        auto* dst = reinterpret_cast<char*>(buffer_.data() + write_pos_);
        dst[0] = ',';
        for (std::size_t i = 0; i < arg_count_; ++i) {
            dst[1 + i] = args_[i].tag;
        }
        // Null-pad remainder
        std::memset(dst + tag_len, 0, padded_tag_len - tag_len);
        write_pos_ += padded_tag_len;
    }

    // 3. Argument data
    for (std::size_t i = 0; i < arg_count_; ++i) {
        const auto& arg = args_[i];
        switch (arg.tag) {
            case 'i':
                write_int32_be(arg.int_val);
                break;
            case 'f':
                write_float32_be(arg.float_val);
                break;
            case 's':
                write_padded_string(arg.str_val);
                break;
            case 'b': {
                auto blob_size = static_cast<std::int32_t>(arg.blob_val.size());
                write_int32_be(blob_size);
                if (error_) break;

                std::size_t padded_blob = align4(arg.blob_val.size());
                if (!has_space(padded_blob)) {
                    error_ = true;
                    break;
                }
                std::memcpy(buffer_.data() + write_pos_,
                            arg.blob_val.data(), arg.blob_val.size());
                // Zero-pad
                if (padded_blob > arg.blob_val.size()) {
                    std::memset(buffer_.data() + write_pos_ + arg.blob_val.size(),
                                0, padded_blob - arg.blob_val.size());
                }
                write_pos_ += padded_blob;
                break;
            }
            default:
                error_ = true;
                break;
        }
        if (error_) return *this;
    }

    message_ended_ = true;
    return *this;
}

std::span<const std::byte> OscWriter::packet() const {
    if (error_ || !message_ended_) {
        return {};
    }
    return buffer_.subspan(0, write_pos_);
}

void OscWriter::write_padded_string(std::string_view s) {
    std::size_t padded_len = align4(s.size() + 1);  // +1 for null terminator
    if (!has_space(padded_len)) {
        error_ = true;
        return;
    }

    std::memcpy(buffer_.data() + write_pos_, s.data(), s.size());
    std::memset(buffer_.data() + write_pos_ + s.size(), 0, padded_len - s.size());
    write_pos_ += padded_len;
}

void OscWriter::write_int32_be(std::int32_t v) {
    if (!has_space(4)) {
        error_ = true;
        return;
    }

    auto* dst = buffer_.data() + write_pos_;
    dst[0] = static_cast<std::byte>((v >> 24) & 0xFF);
    dst[1] = static_cast<std::byte>((v >> 16) & 0xFF);
    dst[2] = static_cast<std::byte>((v >> 8) & 0xFF);
    dst[3] = static_cast<std::byte>(v & 0xFF);
    write_pos_ += 4;
}

void OscWriter::write_float32_be(float v) {
    if (!has_space(4)) {
        error_ = true;
        return;
    }

    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));

    auto* dst = buffer_.data() + write_pos_;
    dst[0] = static_cast<std::byte>((bits >> 24) & 0xFF);
    dst[1] = static_cast<std::byte>((bits >> 16) & 0xFF);
    dst[2] = static_cast<std::byte>((bits >> 8) & 0xFF);
    dst[3] = static_cast<std::byte>(bits & 0xFF);
    write_pos_ += 4;
}

std::size_t OscWriter::align4(std::size_t pos) {
    return (pos + 3) & ~static_cast<std::size_t>(3);
}

bool OscWriter::has_space(std::size_t bytes) const {
    return write_pos_ + bytes <= buffer_.size();
}

// =============================================================================
// OscReader
// =============================================================================

OscReader::OscReader(std::span<const std::byte> packet)
    : packet_(packet) {
    if (packet_.empty()) {
        error_ = true;
        return;
    }
    parse();
}

void OscReader::parse() {
    std::size_t pos = 0;

    // Read address
    address_ = read_string(pos);
    if (error_ || address_.empty() || address_[0] != '/') {
        error_ = true;
        return;
    }

    // Read type tag string
    if (pos >= packet_.size()) {
        return;  // No type tags — valid but empty message
    }

    type_tag_ = read_string(pos);
    if (error_) return;

    // Type tag must start with ','
    if (type_tag_.empty() || type_tag_[0] != ',') {
        error_ = true;
        return;
    }

    // Parse arguments according to type tags
    for (std::size_t i = 1; i < type_tag_.size(); ++i) {
        if (pos > packet_.size()) {
            error_ = true;
            return;
        }

        OscArgument arg;
        switch (type_tag_[i]) {
            case 'i':
                arg.type = OscArgument::Type::Int32;
                arg.int_value = read_int32(pos);
                if (error_) return;
                break;

            case 'f':
                arg.type = OscArgument::Type::Float32;
                arg.float_value = read_float32(pos);
                if (error_) return;
                break;

            case 's':
                arg.type = OscArgument::Type::String;
                arg.string_value = read_string(pos);
                if (error_) return;
                break;

            case 'b': {
                arg.type = OscArgument::Type::Blob;
                if (pos + 4 > packet_.size()) {
                    error_ = true;
                    return;
                }
                std::int32_t blob_size = read_int32(pos);
                if (error_) return;
                if (blob_size < 0 ||
                    pos + static_cast<std::size_t>(blob_size) > packet_.size()) {
                    error_ = true;
                    return;
                }
                arg.blob_value = packet_.subspan(
                    pos, static_cast<std::size_t>(blob_size));
                pos += align4(static_cast<std::size_t>(blob_size));
                break;
            }

            default:
                error_ = true;
                return;
        }

        arguments_.push_back(arg);
    }
}

std::size_t OscReader::align4(std::size_t pos) {
    return (pos + 3) & ~static_cast<std::size_t>(3);
}

std::string_view OscReader::read_string(std::size_t& pos) {
    if (pos >= packet_.size()) {
        error_ = true;
        return {};
    }

    auto start = reinterpret_cast<const char*>(packet_.data() + pos);
    std::size_t max_len = packet_.size() - pos;
    std::size_t len = 0;
    while (len < max_len && start[len] != '\0') {
        ++len;
    }

    if (len == max_len) {
        error_ = true;
        return {};
    }

    std::string_view result(start, len);
    pos += align4(len + 1);
    return result;
}

std::int32_t OscReader::read_int32(std::size_t& pos) {
    if (pos + 4 > packet_.size()) {
        error_ = true;
        return 0;
    }

    auto bytes = packet_.data() + pos;
    std::int32_t v = (static_cast<std::int32_t>(bytes[0]) << 24) |
                     (static_cast<std::int32_t>(bytes[1]) << 16) |
                     (static_cast<std::int32_t>(bytes[2]) << 8) |
                     static_cast<std::int32_t>(bytes[3]);
    pos += 4;
    return v;
}

float OscReader::read_float32(std::size_t& pos) {
    if (pos + 4 > packet_.size()) {
        error_ = true;
        return 0.0f;
    }

    auto bytes = packet_.data() + pos;
    std::uint32_t bits = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                         (static_cast<std::uint32_t>(bytes[1]) << 16) |
                         (static_cast<std::uint32_t>(bytes[2]) << 8) |
                         static_cast<std::uint32_t>(bytes[3]);
    pos += 4;

    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

}  // namespace Sunny::Infrastructure
