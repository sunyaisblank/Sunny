/**
 * @file version.hpp
 * @brief Internal checked Score version operations
 */

#pragma once

#include <limits>
#include <sunny/core/score/document.hpp>

namespace sunny::core::detail {

[[nodiscard]] inline bool score_version_exhausted(const Score& score) noexcept {
    return score.version == std::numeric_limits<std::uint64_t>::max();
}

[[nodiscard]] inline VoidResult advance_score_version(Score& score) noexcept {
    if (score_version_exhausted(score)) return std::unexpected(ErrorCode::ArithmeticOverflow);
    ++score.version;
    return {};
}

} // namespace sunny::core::detail
