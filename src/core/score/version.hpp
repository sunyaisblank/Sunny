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

/** Capture pre-operation IDs privately; publish their union only on success. */
class ScoreIdentityCommit {
  public:
    explicit ScoreIdentityCommit(const Score& before)
        : observed_(collect_score_identities(before)) {}

    void retain(Score& candidate) const { retain_score_identities(candidate, observed_); }

    [[nodiscard]] VoidResult advance(Score& candidate) const {
        if (score_version_exhausted(candidate))
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        retain(candidate);
        return advance_score_version(candidate);
    }

  private:
    ScoreIdentityReservations observed_;
};

} // namespace sunny::core::detail
