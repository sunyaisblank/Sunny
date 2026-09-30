/**
 * @file tuplets.hpp
 * @brief Shared Score IR tuplet-context graph helpers
 */

#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <sunny/core/score/document.hpp>
#include <vector>

namespace sunny::core {

using TupletContextMap = std::map<TupletId, TupletContext>;

[[nodiscard]] inline const TupletContext* event_tuplet_context(const Event& event) noexcept {
    if (const auto* group = event.as_note_group())
        return group->tuplet_context ? &*group->tuplet_context : nullptr;
    if (const auto* rest = event.as_rest())
        return rest->tuplet_context ? &*rest->tuplet_context : nullptr;
    return nullptr;
}

/**
 * Collect one canonical definition for every context referenced in a Voice.
 * Structural validation owns conflicting-definition diagnostics; compilers call
 * this only after the shared preflight has succeeded.
 */
[[nodiscard]] inline TupletContextMap collect_tuplet_contexts(const Voice& voice) {
    TupletContextMap contexts;
    for (const auto& event : voice.events) {
        if (const auto* context = event_tuplet_context(event))
            contexts.try_emplace(context->id, *context);
    }
    return contexts;
}

/** Resolve an event's enclosing context chain in outermost-to-innermost order. */
[[nodiscard]] inline Result<std::vector<const TupletContext*>>
tuplet_context_chain(const TupletContext* innermost, const TupletContextMap& contexts) {
    std::vector<const TupletContext*> chain;
    if (!innermost) return chain;

    std::set<TupletId> visited;
    const TupletContext* current = innermost;
    while (current) {
        if (!visited.insert(current->id).second) return std::unexpected(ErrorCode::TupletSpanError);
        const auto canonical = contexts.find(current->id);
        if (canonical == contexts.end()) return std::unexpected(ErrorCode::TupletSpanError);
        current = &canonical->second;
        chain.push_back(current);
        if (!current->nested_in) break;
        const auto parent = contexts.find(*current->nested_in);
        if (parent == contexts.end()) return std::unexpected(ErrorCode::TupletSpanError);
        current = &parent->second;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

/** Product of actual/normal ratios for all active nesting levels. */
[[nodiscard]] inline Result<Beat>
cumulative_tuplet_written_ratio(const std::vector<const TupletContext*>& chain) {
    Beat ratio = Beat::one();
    for (const auto* context : chain) {
        if (context->actual == 0 || context->normal == 0)
            return std::unexpected(ErrorCode::TupletInvalidRatio);
        auto next = checked_mul(ratio,
                                Beat{static_cast<std::int64_t>(context->actual),
                                     static_cast<std::int64_t>(context->normal)});
        if (!next) return std::unexpected(next.error());
        ratio = *next;
    }
    return ratio;
}

} // namespace sunny::core
