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
#include <set>
#include <sunny/core/corpus/document.hpp>
#include <sunny/core/mix/document.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/document.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>
#include <sunny/infrastructure/mcp/project_state.hpp>
#include <vector>

namespace sunny::infrastructure {

struct ScoreSession {
    std::map<std::uint64_t, sunny::core::Score> scores;
    std::map<std::uint64_t, sunny::core::UndoStack> undo_stacks;
    std::uint64_t next_score_id = 1;
    // Active only inside the serialized project boundary. The project records
    // its complete before/after state, so inner Score-only snapshots are suppressed.
    std::optional<std::uint64_t> project_transaction_score;

    [[nodiscard]] sunny::core::Score* find(std::uint64_t id) {
        const auto it = scores.find(id);
        return it != scores.end() ? &it->second : nullptr;
    }

    [[nodiscard]] const sunny::core::Score* find(std::uint64_t id) const {
        const auto it = scores.find(id);
        return it != scores.end() ? &it->second : nullptr;
    }

    [[nodiscard]] sunny::core::UndoStack* undo_for(std::uint64_t id) {
        if (project_transaction_score == id) return nullptr;
        return &undo_stacks[id];
    }
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

/** Retired local identities and publication floors survive absence from active stores. */
struct ScoreNamespaceHistory {
    sunny::core::ScoreIdentityReservations identities;
    std::uint64_t version_floor = 0;
};

struct WorkspaceNamespaceHistory {
    std::map<std::uint64_t, ScoreNamespaceHistory> scores;
    std::map<std::uint64_t, std::set<std::uint64_t>> graph_channels;
    std::map<std::uint64_t, std::uint64_t> project_revision_floors;
};

class RealizationStore;
class WorkspaceWriter;

/** Process admissions are independent of native history and authored snapshots. */
struct WorkspaceWriterSession {
    std::vector<std::shared_ptr<WorkspaceWriter>> admissions;
};

[[nodiscard]] std::string new_workspace_namespace();

struct NativeWorkspaceMetadata {
    std::string workspace_namespace;
    std::optional<std::string> history_base_directory;
};

/** Operational history is outside authored undo and workspace backup snapshots. */
struct NativeRealizationSession {
    NativeWorkspaceMetadata metadata{new_workspace_namespace(), std::nullopt};
    bool namespace_is_new = true;
    bool namespace_saved_durably = false;
    std::optional<std::string> workspace_path;
    std::shared_ptr<RealizationStore> store;
    std::optional<std::string> history_error;
};

struct McpSession {
    std::shared_ptr<ScoreSession> score = std::make_shared<ScoreSession>();
    std::shared_ptr<TimbreSession> timbre = std::make_shared<TimbreSession>();
    std::shared_ptr<MixSession> mix = std::make_shared<MixSession>();
    std::shared_ptr<CorpusSession> corpus = std::make_shared<CorpusSession>();
    std::shared_ptr<ProjectSession> project = std::make_shared<ProjectSession>();
    std::shared_ptr<WorkspaceNamespaceHistory> namespace_history =
        std::make_shared<WorkspaceNamespaceHistory>();
    std::shared_ptr<ProjectDeploymentSession> deployment =
        std::make_shared<ProjectDeploymentSession>();
    std::shared_ptr<NativeRealizationSession> realization =
        std::make_shared<NativeRealizationSession>();
    std::shared_ptr<WorkspaceWriterSession> workspace_writer =
        std::make_shared<WorkspaceWriterSession>();
};

} // namespace sunny::infrastructure
