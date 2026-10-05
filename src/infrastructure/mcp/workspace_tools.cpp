/** Coherent workspace file operations through the serialized MCP boundary. */
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/mcp/workspace_state.hpp>
#include <sunny/infrastructure/mcp/workspace_tools.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;

namespace {

json error(const WorkspaceError& failure) {
    return {{"success", false}, {"error", failure.message}};
}

json path_schema(bool recovery = false) {
    json schema = {{"type", "object"},
                   {"properties", {{"path", {{"type", "string"}, {"minLength", 1}}}}},
                   {"required", {"path"}}};
    if (recovery) schema["properties"]["apply"] = {{"type", "boolean"}, {"default", false}};
    return schema;
}

} // namespace

void register_workspace_tools(McpServer& server, const McpSession& session) {
    auto domain = server.registration_scope(McpDocumentDomain::None);
    server.register_tool(
        "workspace_save",
        "Save all authored documents and owning bindings; preserve the previous valid file as .bak",
        path_schema(),
        [session](const json& params) {
            const auto admission =
                acquire_workspace_writer(session, params.at("path").get<std::string>());
            if (!admission) return error(admission.error());
            const auto& path = *admission;
            std::optional<std::string> source_path = std::filesystem::absolute(path).string();
            auto metadata = session.realization->metadata;
            // Establish history while the namespace is provably fresh. Once a
            // saved namespace is restored, missing history is never initialized.
            if (session.realization->namespace_is_new && !session.realization->store) {
                const auto base = std::filesystem::absolute(path).parent_path();
                auto history = RealizationStore::open(
                    base, metadata.workspace_namespace, RealizationStoreMode::InitializeNew);
                if (history) {
                    session.realization->store = std::move(*history);
                    session.realization->history_error.reset();
                } else {
                    session.realization->history_error = history.error().message;
                }
            }
            if (session.realization->store)
                metadata.history_base_directory =
                    session.realization->store->directory().parent_path().string();
            const auto saved = save_workspace(session, path, {}, &metadata);
            if (saved.committed) {
                session.realization->metadata.workspace_namespace.swap(
                    metadata.workspace_namespace);
                session.realization->metadata.history_base_directory.swap(
                    metadata.history_base_directory);
                session.realization->namespace_is_new = false;
                session.realization->namespace_saved_durably = saved.durability_confirmed;
                session.realization->workspace_path.swap(source_path);
            }
            json response = {{"success", saved.success},
                             {"committed", saved.committed},
                             {"durability_confirmed", saved.durability_confirmed},
                             {"backup_updated", saved.backup_updated},
                             {"backup_status", saved.backup_status}};
            response["native_history_available"] =
                session.realization->namespace_saved_durably && session.realization->store &&
                session.realization->store->native_writes_available();
            if (session.realization->history_error)
                response["native_history_error"] = *session.realization->history_error;
            if (!saved.error.empty()) response["error"] = saved.error;
            if (saved.committed && !saved.durability_confirmed)
                response["durability_status"] =
                    "The replacement is visible; durable directory synchronization is unconfirmed";
            return response;
        });
    server.register_tool("workspace_open",
                         "Validate a complete workspace privately, then replace authored state and "
                         "clear ephemeral history/plans",
                         path_schema(),
                         [session](const json& params) -> json {
                             auto opened =
                                 open_workspace(session, params.at("path").get<std::string>());
                             return opened ? std::move(*opened) : error(opened.error());
                         });
    server.register_tool("workspace_import",
                         "Import a valid workspace without remapping; store or shared-preset "
                         "identity collisions are refused",
                         path_schema(),
                         [session](const json& params) -> json {
                             auto imported =
                                 import_workspace(session, params.at("path").get<std::string>());
                             return imported ? std::move(*imported) : error(imported.error());
                         });
    server.register_tool("workspace_recover_backup",
                         "Validate the explicit .bak source; preview by default, apply=true "
                         "replaces authored state without repairing the main file",
                         path_schema(true),
                         [session](const json& params) -> json {
                             auto recovered =
                                 recover_workspace_backup(session,
                                                          params.at("path").get<std::string>(),
                                                          params.value("apply", false));
                             return recovered ? std::move(*recovered) : error(recovered.error());
                         });
}

} // namespace sunny::infrastructure
