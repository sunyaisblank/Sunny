/**
 * @file session.hpp
 * @brief Identity-stable stores shared by MCP registration groups
 *
 * Score, Timbre, Mix, and Corpus tools used to own isolated anonymous stores.
 * Public store types let cross-IR project tools resolve their referenced
 * documents without copying or weakening the identifiers exposed to clients.
 */

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <sunny/core/corpus/document.hpp>
#include <sunny/core/mix/document.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/document.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>
#include <vector>

namespace sunny::infrastructure {

struct ScoreSession {
    std::map<std::uint64_t, sunny::core::Score> scores;
    std::map<std::uint64_t, sunny::core::UndoStack> undo_stacks;
    std::uint64_t next_score_id = 1;

    [[nodiscard]] sunny::core::Score* find(std::uint64_t id) {
        const auto it = scores.find(id);
        return it != scores.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const sunny::core::Score* find(std::uint64_t id) const {
        const auto it = scores.find(id);
        return it != scores.end() ? &it->second : nullptr;
    }

    [[nodiscard]] sunny::core::UndoStack* undo_for(std::uint64_t id) { return &undo_stacks[id]; }
};

struct TimbreSession {
    std::map<std::uint64_t, sunny::core::TimbreProfile> profiles;
    std::vector<sunny::core::TimbrePreset> preset_library;
    std::uint64_t next_profile_id = 1;
    std::uint64_t next_effect_id = 1;
    std::uint64_t next_preset_id = 1;

    [[nodiscard]] sunny::core::TimbreProfile* find(std::uint64_t id) {
        const auto it = profiles.find(id);
        return it != profiles.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const sunny::core::TimbreProfile* find(std::uint64_t id) const {
        const auto it = profiles.find(id);
        return it != profiles.end() ? &it->second : nullptr;
    }
};

struct MixSession {
    std::map<std::uint64_t, sunny::core::MixGraph> graphs;
    std::uint64_t next_graph_id = 1;
    std::uint64_t next_group_id = 1;
    std::uint64_t next_aux_id = 1;
    std::uint64_t next_effect_id = 1;
    std::uint64_t next_ref_id = 1;

    [[nodiscard]] sunny::core::MixGraph* find(std::uint64_t id) {
        const auto it = graphs.find(id);
        return it != graphs.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const sunny::core::MixGraph* find(std::uint64_t id) const {
        const auto it = graphs.find(id);
        return it != graphs.end() ? &it->second : nullptr;
    }
};

struct CorpusSession {
    sunny::core::CorpusDatabase corpus;
    std::uint64_t next_composer_id = 1;
    std::uint64_t next_work_id = 1;
};

struct StoredProjectDeploymentPlan {
    std::uint64_t score_id = 0;
    std::vector<std::uint64_t> timbre_profile_ids;
    std::uint64_t mix_graph_id = 0;
    bool consumed = false;
    std::optional<formats::AbletonProjectDeploymentPlan> plan;
};

struct ProjectDeploymentSession {
    std::map<std::uint64_t, StoredProjectDeploymentPlan> plans;
    std::uint64_t next_plan_id = 1;
};

struct McpSession {
    std::shared_ptr<ScoreSession> score = std::make_shared<ScoreSession>();
    std::shared_ptr<TimbreSession> timbre = std::make_shared<TimbreSession>();
    std::shared_ptr<MixSession> mix = std::make_shared<MixSession>();
    std::shared_ptr<CorpusSession> corpus = std::make_shared<CorpusSession>();
    std::shared_ptr<ProjectDeploymentSession> deployment =
        std::make_shared<ProjectDeploymentSession>();
};

} // namespace sunny::infrastructure
