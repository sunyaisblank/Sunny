/**
 * @file validation.hpp
 * @brief Cross-IR project validation
 *
 * A Sunny production is only compilable as a whole when its Score, Timbre,
 * and Mix documents are individually valid and their PartId bindings form
 * exact one-to-one correspondences. This view is deliberately non-owning so
 * session stores can preserve document identity and move-only timbre graphs.
 */

#pragma once

#include <span>
#include <sunny/core/mix/document.hpp>
#include <sunny/core/score/document.hpp>
#include <sunny/core/timbre/document.hpp>
#include <vector>

namespace sunny::core {

struct ProjectView {
    const Score& score;
    std::span<const TimbreProfile* const> timbre_profiles;
    const MixGraph& mix;
};

/**
 * @brief Validate every document and all Score↔Timbre↔Mix correspondences
 *
 * Diagnostics are stable-sorted by severity. Within a severity, Score,
 * Timbre, Timbre correspondence, Mix, and Mix correspondence order is kept.
 */
[[nodiscard]] std::vector<Diagnostic> validate_project(const ProjectView& project);

/** @brief True when validate_project contains no Error-level diagnostic. */
[[nodiscard]] bool is_project_compilable(const ProjectView& project);

} // namespace sunny::core
