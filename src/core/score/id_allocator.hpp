/**
 * @file id_allocator.hpp
 * @brief Checked document-local typed-ID allocation for Score IR construction
 */

#pragma once

#include <limits>
#include <set>
#include <sunny/core/score/document.hpp>

namespace sunny::core::detail {

/**
 * Allocate the lowest positive identifier not already present in one document.
 *
 * The allocator is deliberately local to a candidate operation. Rejection does
 * not consume hidden process state, deserialised high identifiers cannot
 * collide with a fixed global range, and the uint64_t domain never wraps.
 */
template <typename Identifier> class FreshIdAllocator {
  public:
    void include(Identifier id) { used_.insert(id.value); }

    [[nodiscard]] Result<Identifier> allocate() {
        while (used_.contains(candidate_)) {
            if (candidate_ == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            ++candidate_;
        }

        const auto value = candidate_;
        used_.insert(value);
        if (candidate_ != std::numeric_limits<std::uint64_t>::max()) ++candidate_;
        return Identifier{value};
    }

  private:
    std::set<std::uint64_t> used_;
    std::uint64_t candidate_ = 1;
};

inline FreshIdAllocator<EventId> event_id_allocator(const Score& score) {
    FreshIdAllocator<EventId> allocator;
    for (const auto& part : score.parts)
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                for (const auto& event : voice.events)
                    allocator.include(event.id);
    return allocator;
}

inline FreshIdAllocator<PartId> part_id_allocator(const Score& score) {
    FreshIdAllocator<PartId> allocator;
    for (const auto& part : score.parts)
        allocator.include(part.id);
    return allocator;
}

inline void include_section_ids(FreshIdAllocator<SectionId>& allocator,
                                const SectionMap& sections) {
    for (const auto& section : sections) {
        allocator.include(section.id);
        include_section_ids(allocator, section.children);
    }
}

inline FreshIdAllocator<SectionId> section_id_allocator(const Score& score) {
    FreshIdAllocator<SectionId> allocator;
    include_section_ids(allocator, score.section_map);
    return allocator;
}

} // namespace sunny::core::detail
