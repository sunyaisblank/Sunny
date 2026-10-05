/** Checked preparation of session-wide monotonic identity allocations. */
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sunny/core/types/music_types.hpp>

namespace sunny::infrastructure::mcp_detail {

struct SessionIdBatch {
    std::uint64_t first;
    std::uint64_t count;
    std::uint64_t next;
};

/**
 * Prepare a complete contiguous allocation without changing any session state.
 *
 * Global counters are persisted high-water values: they must exceed all actually
 * represented IDs, including imported values, and never search retired holes.
 * UINT64_MAX is the exhausted counter marker, so the last allocatable ID is
 * UINT64_MAX-1. Local Score/Channel lowest-free reservations have a separate
 * contract and must not use this helper.
 *
 * The caller publishes batch.next only after its entire candidate succeeds.
 */
[[nodiscard]] inline sunny::core::Result<SessionIdBatch>
checked_session_id_batch(std::uint64_t next, std::uint64_t observed_max, std::uint64_t count = 1) {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (next == 0 || next == maximum || observed_max == maximum || count > maximum - next)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    if (next <= observed_max) return std::unexpected(sunny::core::ErrorCode::InvariantViolation);
    return SessionIdBatch{next, count, next + count};
}

/** Observe both the store key and root ID; never overwrite an occupied slot. */
template <typename Store>
[[nodiscard]] std::uint64_t maximum_session_store_id(const Store& documents) {
    std::uint64_t maximum = 0;
    for (const auto& [id, document] : documents)
        maximum = std::max({maximum, id, document.id.value});
    return maximum;
}

} // namespace sunny::infrastructure::mcp_detail
