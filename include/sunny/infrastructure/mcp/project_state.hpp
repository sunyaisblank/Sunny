/**
 * @file project_state.hpp
 * @brief Bound project identity and immutable authoring history
 *
 * The McpSession owns the canonical document stores. A binding joins existing
 * identities without copying documents or introducing a second project ID.
 * Snapshots are immutable history values, not another live document store.
 */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

namespace sunny::infrastructure {

struct ProjectSnapshot {
    std::uint64_t score_id = 0;
    std::uint64_t mix_graph_id = 0;
    std::vector<std::uint64_t> profile_ids;
    nlohmann::json documents;
};

struct ProjectHistoryEntry {
    std::shared_ptr<const ProjectSnapshot> before;
    std::shared_ptr<const ProjectSnapshot> after;
    std::string description;
    // Only these library entries belong to this edit. Undo must preserve
    // presets independently saved by other projects or scratch documents.
    std::vector<std::uint64_t> changed_preset_ids;
};

struct ProjectRecord {
    static constexpr std::size_t DEFAULT_HISTORY_CAPACITY = 64;

    std::uint64_t score_id = 0;
    std::uint64_t mix_graph_id = 0;
    std::vector<std::uint64_t> profile_ids;
    std::uint64_t revision = 1;
    // Score retains its own typed identity reservations. Channels additionally
    // retain observed identities, including retired/undone ones, in this owner.
    std::set<std::uint64_t> reserved_channel_ids;
    std::size_t history_capacity = DEFAULT_HISTORY_CAPACITY;
    std::vector<ProjectHistoryEntry> undo_entries;
    std::vector<ProjectHistoryEntry> redo_entries;
};

struct ProjectSession {
    std::map<std::uint64_t, ProjectRecord> projects;
};

} // namespace sunny::infrastructure
