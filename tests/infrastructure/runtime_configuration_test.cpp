/** Pure production parser and migration preserve existing durable data boundaries. */
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sunny/infrastructure/runtime_configuration.hpp>

using namespace sunny::infrastructure;
using json = nlohmann::json;
namespace {
json document() {
    return {{"configuration_schema_version", 1},
            {"client", {{"transport", {{"mode", "offline"}}}, {"workspace", nullptr}}},
            {"native", {{"bridge", {{"bind_host", "127.0.0.2"}, {"port", 65535}}}}}};
}
struct Directory {
    std::filesystem::path path;
    Directory() {
        static std::atomic<unsigned> serial{0};
        path = std::filesystem::temp_directory_path() /
               ("sunny-config-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(serial.fetch_add(1)));
        REQUIRE(std::filesystem::create_directory(path));
    }
    ~Directory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
void write(const std::filesystem::path& path, const std::string& body) {
    std::ofstream output(path, std::ios::binary);
    output << body;
    REQUIRE(output.good());
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
} // namespace
TEST_CASE("Configuration validates complete closed typed roles before effects",
          "[runtime_configuration]") {
    auto value = document();
    REQUIRE_NOTHROW(validate_runtime_configuration(value.dump()));
    for (const auto& invalid_port :
         {json(true), json(0), json(65536), json(-1), json(1.0), json("9001")}) {
        value = document();
        value["native"]["bridge"]["port"] = invalid_port;
        CHECK_THROWS(validate_runtime_configuration(value.dump()));
    }
    for (const auto& invalid_version : {json(true), json(2), json(1.0), json("1")}) {
        value = document();
        value["configuration_schema_version"] = invalid_version;
        CHECK_THROWS(validate_runtime_configuration(value.dump()));
    }
    value = document();
    value["native"]["bridge"]["bind_host"] = "0.0.0.0";
    CHECK_THROWS(validate_runtime_configuration(value.dump()));
    value = document();
    value["client"]["unexpected"] = true;
    CHECK_THROWS(validate_runtime_configuration(value.dump()));
    CHECK_THROWS(validate_runtime_configuration(
        R"({"configuration_schema_version":1,"configuration_schema_version":1,"native":{"bridge":{"bind_host":"127.0.0.1","port":1}}})"));
    CHECK_THROWS(validate_runtime_configuration(std::string(65537, ' ')));
    CHECK_THROWS(validate_runtime_configuration(
        R"({"configuration_schema_version":1,"x":[[[[[[[[[0]]]]]]]]]})"));
}
TEST_CASE("Configuration endpoint data and file selection are independent of workspace writes",
          "[runtime_configuration]") {
    Directory directory;
    const auto selected = directory.path / "config with spaces.json";
    const auto workspace = directory.path / "uncreated project" / "workspace.json";
    auto value = document();
    value["client"]["workspace"] = {{"path", workspace.string()}, {"recovery", "backup"}};
    for (const char* host : {"example.test", "host.docker.internal", "127.0.0.1", "::1"}) {
        value["client"]["transport"] = {{"mode", "tcp"}, {"host", host}, {"port", 49152}};
        write(selected, value.dump());
        const auto configured =
            load_client_configuration({{"SUNNY_CONFIG_PATH", selected.string()}});
        REQUIRE(configured.transport);
        CHECK(configured.transport->host == host);
        CHECK(configured.transport->port == 49152);
        REQUIRE(configured.workspace);
        CHECK(configured.workspace->path == workspace);
        CHECK(configured.workspace->recover_backup);
        CHECK_FALSE(configured.legacy_environment);
        CHECK(json::parse(validate_client_configuration_file(selected)) == value);
        CHECK_FALSE(std::filesystem::exists(workspace.parent_path()));
    }
    for (const char* key : {"SUNNY_ABLETON_HOST",
                            "SUNNY_TCP_PORT",
                            "SUNNY_WORKSPACE_PATH",
                            "SUNNY_WORKSPACE_RECOVERY",
                            "SUNNY_BIND_HOST"}) {
        CHECK_THROWS(
            load_client_configuration({{"SUNNY_CONFIG_PATH", selected.string()}, {key, "1"}}));
    }
    value.erase("client");
    write(selected, value.dump());
    CHECK_THROWS(load_client_configuration({{"SUNNY_CONFIG_PATH", selected.string()}}));
}
TEST_CASE("Legacy client migration is exclusive and preserves recovery and source data",
          "[runtime_configuration]") {
    Directory directory;
    const auto selected = directory.path / "new external config.json";
    const RuntimeEnvironment environment{{"SUNNY_WORKSPACE_PATH", "legacy work with spaces.json"},
                                         {"SUNNY_WORKSPACE_RECOVERY", "backup"},
                                         {"SUNNY_ABLETON_HOST", "example.test"},
                                         {"SUNNY_TCP_PORT", "65535"}};
    migrate_client_environment(environment, selected);
    const auto before = read(selected);
    const auto value = json::parse(before);
    CHECK(value["client"]["workspace"]["path"] ==
          std::filesystem::absolute("legacy work with spaces.json").string());
    CHECK(value["client"]["workspace"]["recovery"] == "backup");
    CHECK(value["client"]["transport"]["port"] == 65535);
    CHECK_THROWS(migrate_client_environment(environment, selected));
    CHECK(read(selected) == before);
    CHECK_THROWS(load_client_configuration({{"SUNNY_WORKSPACE_RECOVERY", "backup"}}));
    CHECK_THROWS(load_client_configuration(
        {{"SUNNY_WORKSPACE_PATH", "unused.json"}, {"SUNNY_TCP_PORT", "+9001"}}));
    CHECK_THROWS(migrate_client_environment({{"SUNNY_ABLETON_HOST", "bad host"}},
                                            directory.path / "unsafe.json"));
    CHECK_FALSE(std::filesystem::exists(directory.path / "unsafe.json"));
}
