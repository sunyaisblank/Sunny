/** Strict startup configuration, validated before workspace or transport effects. */
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/runtime_configuration_contract.hpp>

namespace sunny::infrastructure {

using RuntimeEnvironment = std::map<std::string, std::string>;

struct WorkspaceStartupConfiguration {
    std::filesystem::path path;
    bool recover_backup = false;
};

struct ClientRuntimeConfiguration {
    std::optional<TcpConfig> transport;
    std::optional<WorkspaceStartupConfiguration> workspace;
    bool legacy_environment = true;
};

/** Snapshot only selected Sunny runtime inputs; never mutate process environment. */
[[nodiscard]] RuntimeEnvironment runtime_environment();
/** Reject a complete schema1 document, including its unconsumed role, on any violation. */
void validate_runtime_configuration(std::string_view bytes);
/** Validate a selected client file and return typed JSON, without environment merging or startup.
 */
[[nodiscard]] std::string validate_client_configuration_file(const std::filesystem::path&);
/** Read the selected file or explicit legacy compatibility settings before startup effects. */
[[nodiscard]] ClientRuntimeConfiguration load_client_configuration(const RuntimeEnvironment&);
/** Exclusive output only; never change the input environment, config, workspace or ledger. */
void migrate_client_environment(const RuntimeEnvironment&, const std::filesystem::path& output);

} // namespace sunny::infrastructure
