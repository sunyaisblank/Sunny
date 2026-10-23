/** Versioned whole-workspace persistence over existing canonical document stores. */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

inline constexpr int WORKSPACE_SCHEMA_VERSION = 2;

struct WorkspaceError {
    std::string message;
};

template <class Value> using WorkspaceResult = std::expected<Value, WorkspaceError>;

/** Admit this process before restoring or publishing a workspace. Canonical parent
 * aliases share admission; final symlinks/hardlinks are refused because atomic
 * replacement would separate their identities. Hidden *.sunny-writer.lock names
 * are reserved; stable sidecars are never removed.
 * All admitted main/backup paths remain owned until the session is destroyed. */
[[nodiscard]] WorkspaceResult<std::filesystem::path>
acquire_workspace_writer(const McpSession& session, const std::filesystem::path& path);

/** Detached candidate; no transport, Live identities, or deployment plans. */
struct WorkspaceState {
    ScoreSession score;
    TimbreSession timbre;
    MixSession mix;
    CorpusSession corpus;
    ProjectSession project;
    WorkspaceNamespaceHistory namespace_history;
    NativeWorkspaceMetadata native_realization;
    bool native_namespace_is_new = false;
    std::uint64_t next_plan_id = 1;
};

[[nodiscard]] WorkspaceResult<nlohmann::json> workspace_to_json(const McpSession& session);
[[nodiscard]] WorkspaceResult<WorkspaceState> workspace_from_json(const nlohmann::json& document);
[[nodiscard]] WorkspaceResult<WorkspaceState> read_workspace(const std::filesystem::path& path);

/** Caller supplies the serialized MCP request boundary; publication uses only swaps. */
[[nodiscard]] WorkspaceResult<nlohmann::json> open_workspace(const McpSession& session,
                                                             const std::filesystem::path& path);
[[nodiscard]] WorkspaceResult<nlohmann::json> import_workspace(const McpSession& session,
                                                               const std::filesystem::path& path);

enum class WorkspaceFileRole { Main, Backup };
enum class WorkspaceIoPhase {
    CreateTemporary,
    AfterPartialWrite,
    FileSync,
    Replace,
    DirectorySync
};
using WorkspaceIoFault = std::function<bool(WorkspaceFileRole, WorkspaceIoPhase)>;

struct WorkspaceSaveResult {
    bool success = false;
    // committed describes replacement of the requested main path.
    bool committed = false;
    bool durability_confirmed = false;
    bool backup_updated = false;
    std::string backup_status;
    std::string error;
};

/** Checked exclusive temp, file sync, atomic replacement, then directory sync. */
[[nodiscard]] WorkspaceSaveResult
save_workspace(const McpSession& session,
               const std::filesystem::path& path,
               const WorkspaceIoFault& fault = {},
               const NativeWorkspaceMetadata* publication = nullptr);

/** Validate the explicit .bak source; preview does not mutate files or session. */
[[nodiscard]] WorkspaceResult<nlohmann::json>
recover_workspace_backup(const McpSession& session, const std::filesystem::path& path, bool apply);

} // namespace sunny::infrastructure
