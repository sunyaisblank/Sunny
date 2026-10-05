#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <latch>
#include <sstream>
#include <sunny/infrastructure/ableton/authority_transport.hpp>
#include <sunny/infrastructure/mcp/core_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <thread>

using namespace sunny::infrastructure;
using json = nlohmann::json;
using namespace std::chrono_literals;

namespace {
json tool(int id) {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"method", "tools/call"},
            {"params", {{"name", "controlled"}, {"arguments", json::object()}}}};
}
json cancel(json id) {
    return {{"jsonrpc", "2.0"},
            {"method", "notifications/cancelled"},
            {"params", {{"requestId", std::move(id)}}}};
}

/** Independent interruptible boundary: input stays live while output is blocked. */
class OwnedIo final : public McpIo {
  public:
    void feed(const std::string& text) {
        {
            std::lock_guard lock(mutex_);
            bytes_.insert(bytes_.end(), text.begin(), text.end());
        }
        changed_.notify_all();
    }
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        changed_.notify_all();
    }
    void release_output() {
        {
            std::lock_guard lock(mutex_);
            blocked_output = false;
        }
        changed_.notify_all();
    }
    std::string output() {
        std::lock_guard lock(mutex_);
        return output_;
    }
    bool wait_reading() {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 1s, [&] { return reading_; });
    }
    bool wait_blocked() {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 1s, [&] { return writing_; });
    }
    bool wait_input_processed() {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, 1s, [&] { return bytes_.empty() && waiting_input_; });
    }
    McpReadResult read(char& byte, const std::atomic<bool>& running) override {
        std::unique_lock lock(mutex_);
        reading_ = true;
        changed_.notify_all();
        while (running.load() && !closed_ && bytes_.empty()) {
            waiting_input_ = true;
            changed_.notify_all();
            changed_.wait_for(lock, 10ms);
        }
        waiting_input_ = false;
        if (!running.load()) return McpReadResult::Stopped;
        if (bytes_.empty()) return McpReadResult::End;
        byte = bytes_.front();
        bytes_.pop_front();
        return McpReadResult::Byte;
    }
    bool write(std::string_view line,
               std::chrono::steady_clock::time_point deadline,
               const std::atomic<bool>& running) override {
        std::unique_lock lock(mutex_);
        writing_ = true;
        changed_.notify_all();
        if (write_budget > 0ms)
            deadline = std::min(deadline, std::chrono::steady_clock::now() + write_budget);
        while (blocked_output && running.load() && std::chrono::steady_clock::now() < deadline)
            changed_.wait_for(lock, 10ms);
        if (!running.load() || std::chrono::steady_clock::now() >= deadline) return false;
        output_.append(line);
        changed_.notify_all();
        return true;
    }
    bool blocked_output = false; // Set before run; change under lock through release_output.
    std::chrono::milliseconds write_budget{0};

  private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<char> bytes_;
    std::string output_;
    bool closed_ = false, reading_ = false, writing_ = false, waiting_input_ = false;
};

json origin_json() {
    const auto control = current_request_control();
    if (!control || !control->native_origin) return nullptr;
    return {{"bridge_instance", control->native_origin->bridge_instance},
            {"document_token", control->native_origin->document_token}};
}
json named_tool(int id, const std::string& name, json arguments = json::object()) {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"method", "tools/call"},
            {"params", {{"name", name}, {"arguments", std::move(arguments)}}}};
}
std::optional<json> wait_reply(OwnedIo& io, int id) {
    const auto until = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < until) {
        std::istringstream lines(io.output());
        std::string line;
        while (std::getline(lines, line)) {
            const auto reply = json::parse(line);
            if (reply.at("id") == id) return reply;
        }
        std::this_thread::sleep_for(1ms);
    }
    return std::nullopt;
}
json reply_value(const json& reply) {
    return reply.at("result").at("structuredContent");
}

/** Literal independent model for the real registered six-read doctor. */
class OriginPeer final : public LomTransport {
  public:
    NativeOrigin epoch{std::string(32, 'a'), std::string(32, 'b')};
    std::function<void()> profile_hook;
    std::atomic<unsigned> stateful{0}, reads{0};
    bool is_connected() const override { return true; }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        ++stateful;
        return {false, std::nullopt, "Unexpected raw notes"};
    }
    LomResponse send(const LomRequest& request) override {
        ++reads;
        if (request.property_or_method == "sunny_get_target_profile") {
            if (profile_hook) profile_hook();
            auto profile = json::parse(
                R"({"bridge_protocol_version":47,"adapter":{"name":"Sunny Remote Script","runtime":"control_surface_python","contract":"version_coupled_private","source_sha256":"SOURCE"},"live":{"version":{"major":12,"minor":4,"bugfix":5,"string":"12.4.5"}},"capabilities":{"clip_add_new_notes":"available","track_insert_device_native":"available","automation_envelope_authoring":"unavailable","group_track_creation":"unavailable","arbitrary_browser_loading":"unavailable","structural_snapshot":"available","max_for_live":"unknown"}})");
            profile["adapter"]["source_sha256"] = SUNNY_BRIDGE_SOURCE_SHA256;
            return {true, profile, std::nullopt};
        }
        if (request.property_or_method == "sunny_managed_context")
            return {true,
                    json{{"schema_version", 1},
                         {"bridge_instance", epoch.bridge_instance},
                         {"document_token", epoch.document_token}},
                    std::nullopt};
        if (request.type == LomRequestType::GetProperty) return {true, false, std::nullopt};
        ++stateful;
        return {false, std::nullopt, "No stateful frame should reach this peer"};
    }
};
} // namespace

TEST_CASE("Reader pins a queued writer before replacement Set observation",
          "[mcp][native-origin][admission]") {
    OriginPeer peer;
    std::atomic<unsigned> store_opens{0};
    AuthorityLomTransport authority(
        peer,
        [&](bool) -> std::shared_ptr<RealizationStore> {
            ++store_opens;
            return nullptr;
        },
        [] { return std::string(32, '1'); });
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&authority);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);
    std::promise<void> held, release;
    auto permission = release.get_future().share();
    server.register_tool("hold", "Hold worker only", json::object(), [&](const json&) {
        held.set_value();
        permission.wait_for(2s);
        return json::object();
    });
    server.register_tool(
        "writer", "Exercise actual authority boundary", json::object(), [&](const json&) {
            const auto origin = origin_json();
            const auto result =
                authority.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
            return json{{"origin", origin}, {"success", result.success}};
        });
    OwnedIo io;
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed(named_tool(1, "doctor_ableton").dump() + "\n");
    const auto doctor = wait_reply(io, 1);
    io.feed(named_tool(2, "hold").dump() + "\n");
    const bool worker_held = held.get_future().wait_for(1s) == std::future_status::ready;
    io.feed(named_tool(3, "writer").dump() + "\n");
    const bool admitted = io.wait_input_processed();
    peer.epoch.document_token = std::string(32, 'c');
    release.set_value();
    const auto writer = wait_reply(io, 3);
    io.close();
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    REQUIRE(doctor);
    REQUIRE(reply_value(*doctor).at("read_only_ready") == true);
    REQUIRE(worker_held);
    REQUIRE(admitted);
    REQUIRE(writer);
    CHECK(reply_value(*writer).at("origin").at("document_token") == std::string(32, 'b'));
    CHECK(reply_value(*writer).at("success") == false);
    CHECK(peer.stateful == 0);
    CHECK(store_opens == 0);
}

TEST_CASE("Batch admission cannot inherit a doctor ahead of the worker",
          "[mcp][native-origin][admission][batch]") {
    OriginPeer peer;
    std::promise<void> held, release;
    auto permission = release.get_future().share();
    peer.profile_hook = [&] {
        held.set_value();
        permission.wait_for(2s);
    };
    AuthorityLomTransport authority(
        peer, [](bool) { return nullptr; }, [] { return std::string(32, '1'); });
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&authority);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);
    server.register_tool("writer", "Observe immutable absence", json::object(), [&](const json&) {
        const auto origin = origin_json();
        const auto result =
            authority.send(LomProtocol::set_property(LomPaths::song(), "tempo", 140.0));
        return json{{"origin", origin}, {"success", result.success}};
    });
    server.register_tool("origin", "Observe future admission", json::object(), [](const json&) {
        return json{{"origin", origin_json()}};
    });
    OwnedIo io;
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed(json::array({named_tool(1, "doctor_ableton"), named_tool(2, "writer")}).dump() + "\n");
    const bool doctor_held = held.get_future().wait_for(1s) == std::future_status::ready;
    const bool admitted = io.wait_input_processed();
    release.set_value();
    const auto until = std::chrono::steady_clock::now() + 1s;
    while (io.output().empty() && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(1ms);
    const auto batch_text = io.output();
    io.feed(named_tool(3, "origin").dump() + "\n");
    // wait_reply handles single replies; the preceding batch is read separately.
    const auto future_until = std::chrono::steady_clock::now() + 1s;
    while (io.output().size() == batch_text.size() &&
           std::chrono::steady_clock::now() < future_until)
        std::this_thread::sleep_for(1ms);
    const auto future_text = io.output().substr(batch_text.size());
    io.close();
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    REQUIRE(doctor_held);
    REQUIRE(admitted);
    REQUIRE_FALSE(batch_text.empty());
    const auto batch = json::parse(batch_text);
    REQUIRE(batch.size() == 2);
    CHECK(reply_value(batch.at(0)).at("read_only_ready") == true);
    CHECK(reply_value(batch.at(1)).at("origin").is_null());
    CHECK(reply_value(batch.at(1)).at("success") == false);
    REQUIRE_FALSE(future_text.empty());
    CHECK(reply_value(json::parse(future_text)).at("origin").at("document_token") ==
          std::string(32, 'b'));
    CHECK(peer.stateful == 0);
}

TEST_CASE("A later doctor rebinds future admissions without editing an earlier queued pin",
          "[mcp][native-origin][admission]") {
    OriginPeer peer;
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&peer);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);
    server.register_tool("origin", "Observe pin", json::object(), [](const json&) {
        return json{{"origin", origin_json()}};
    });
    OwnedIo io;
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed(named_tool(1, "doctor_ableton").dump() + "\n");
    const auto first = wait_reply(io, 1);
    std::promise<void> held, release;
    auto permission = release.get_future().share();
    // The reader/worker has finished the first doctor before installing this hook.
    peer.epoch.document_token = std::string(32, 'c');
    peer.profile_hook = [&] {
        held.set_value();
        permission.wait_for(2s);
    };
    io.feed(named_tool(2, "doctor_ableton").dump() + "\n");
    const bool doctor_held = held.get_future().wait_for(1s) == std::future_status::ready;
    io.feed(named_tool(3, "origin").dump() + "\n");
    const bool admitted = io.wait_input_processed();
    release.set_value();
    const auto second = wait_reply(io, 2);
    const auto queued = wait_reply(io, 3);
    io.feed(named_tool(4, "origin").dump() + "\n");
    const auto future = wait_reply(io, 4);
    io.close();
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(doctor_held);
    REQUIRE(admitted);
    CHECK(reply_value(*second).at("read_only_ready") == true);
    REQUIRE(queued);
    CHECK(reply_value(*queued).at("origin").at("document_token") == std::string(32, 'b'));
    REQUIRE(future);
    CHECK(reply_value(*future).at("origin").at("document_token") == std::string(32, 'c'));
}

TEST_CASE("Publication requires this server's exact live doctor and resets at a fresh run",
          "[mcp][native-origin][publication][session]") {
    OriginPeer peer;
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&peer);
    McpServer server, other;
    register_sunny_tools(server, orchestrator, dispatcher);
    const auto observer = [](const json&) { return json{{"origin", origin_json()}}; };
    server.register_tool("origin", "Observe pin", json::object(), observer);
    other.register_tool("origin", "Observe separate session", json::object(), observer);
    server.register_tool("impostor", "Cannot publish", json::object(), [&](const json&) {
        return json{{"published", server.publish_native_origin(peer.epoch)}};
    });
    CHECK_FALSE(server.publish_native_origin(peer.epoch));
    CHECK_FALSE(server.publish_native_origin({std::string(32, 'A'), std::string(32, 'b')}));
    CHECK(reply_value(server.process_request(named_tool(1, "impostor"))).at("published") == false);
    auto counterfeit = std::make_shared<RequestControl>(
        "2", "tools/call", std::chrono::steady_clock::now() + 1s, std::nullopt, "doctor_ableton");
    {
        RequestControlScope scope(counterfeit);
        CHECK_FALSE(server.publish_native_origin(peer.epoch));
        CHECK_FALSE(other.publish_native_origin(peer.epoch));
    }
    CHECK(reply_value(server.process_request(named_tool(2, "doctor_ableton"))).at("success") ==
          true);
    CHECK_FALSE(
        reply_value(server.process_request(named_tool(3, "origin"))).at("origin").is_null());
    CHECK(reply_value(other.process_request(named_tool(3, "origin"))).at("origin").is_null());
    std::istringstream input(named_tool(4, "origin").dump() + "\n");
    std::ostringstream output;
    server.run(input, output);
    CHECK(reply_value(json::parse(output.str())).at("origin").is_null());
}

TEST_CASE("Cancelled or EOF doctor observations cannot publish future native authority",
          "[mcp][native-origin][publication][cancellation][eof]") {
    OriginPeer peer;
    std::promise<void> held, release;
    std::shared_ptr<RequestControl> doctor_control;
    auto permission = release.get_future().share();
    peer.profile_hook = [&] {
        doctor_control = current_request_control();
        held.set_value();
        permission.wait_for(2s);
    };
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&peer);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);
    server.register_tool("origin", "Observe pin", json::object(), [](const json&) {
        return json{{"origin", origin_json()}};
    });
    OwnedIo io;
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed(named_tool(1, "doctor_ableton").dump() + "\n");
    const bool doctor_held = held.get_future().wait_for(1s) == std::future_status::ready;
    bool eof = false;
    SECTION("Actual reader cancellation") {
        io.feed(cancel(1).dump() + "\n");
        CHECK(io.wait_input_processed());
    }
    SECTION("Actual input EOF") {
        eof = true;
        io.close();
        const auto until = std::chrono::steady_clock::now() + 1s;
        // The actual control is retained by the doctor's active worker.
        while (doctor_control && !doctor_control->input_closed &&
               std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(1ms);
        CHECK(doctor_control);
        if (doctor_control) CHECK(doctor_control->input_closed);
    }
    release.set_value();
    if (!eof) {
        io.feed(named_tool(2, "origin").dump() + "\n");
        const auto observation = wait_reply(io, 2);
        CHECK(observation.has_value());
        if (observation) CHECK(reply_value(*observation).at("origin").is_null());
        io.close();
    }
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    REQUIRE(doctor_held);
    CHECK(reply_value(server.process_request(named_tool(3, "origin"))).at("origin").is_null());
}

TEST_CASE("Closed legacy caller pins do not rebind the session or fall back on malformed IDs",
          "[mcp][native-origin][explicit]") {
    OriginPeer peer;
    Orchestrator orchestrator;
    BridgeDispatcher dispatcher(&peer);
    McpServer server;
    register_sunny_tools(server, orchestrator, dispatcher);
    server.register_tool("legacy_ableton_request",
                         "Observe only admission",
                         json::object(),
                         [](const json&) { return json{{"origin", origin_json()}}; });
    server.register_tool("origin", "Observe pin", json::object(), [](const json&) {
        return json{{"origin", origin_json()}};
    });
    auto arguments = json{
        {"bridge_instance", std::string(32, 'c')},
        {"document_token", std::string(32, 'd')},
        {"command", {{"type", "set"}, {"path", "song"}, {"name", "tempo"}, {"args", {140.0}}}}};
    const auto explicit_call =
        server.process_request(named_tool(1, "legacy_ableton_request", arguments));
    CHECK(reply_value(explicit_call).at("origin").at("document_token") == std::string(32, 'd'));
    CHECK(reply_value(server.process_request(named_tool(2, "origin"))).at("origin").is_null());
    CHECK(reply_value(server.process_request(named_tool(3, "doctor_ableton"))).at("success") ==
          true);
    for (const auto& invalid : {json(nullptr),
                                json(true),
                                json(7),
                                json(std::string(32, 'D')),
                                json(std::string(31, 'd'))}) {
        arguments["document_token"] = invalid;
        const auto response =
            server.process_request(named_tool(4, "legacy_ableton_request", arguments));
        CHECK(reply_value(response).at("origin").is_null());
    }
    arguments["document_token"] = std::string(32, 'd');
    arguments["extra"] = true;
    CHECK(reply_value(server.process_request(named_tool(5, "legacy_ableton_request", arguments)))
              .at("origin")
              .is_null());
    arguments.erase("extra");
    arguments.erase("bridge_instance");
    CHECK(reply_value(server.process_request(named_tool(6, "legacy_ableton_request", arguments)))
              .at("origin")
              .is_null());
    CHECK(reply_value(server.process_request(named_tool(7, "origin")))
              .at("origin")
              .at("document_token") == std::string(32, 'b'));
}

TEST_CASE("Cancellation reaches the exact active request without entering its execution mutex",
          "[mcp][cancellation][request-control]") {
    McpServer server;
    std::promise<void> started;
    std::atomic<unsigned> entered{0};
    server.register_tool(
        "controlled", "A controlled external wait", json::object(), [&](const json&) {
            ++entered;
            started.set_value();
            const auto control = current_request_control();
            if (!control) return json{{"missing_control", true}};
            const auto until = std::chrono::steady_clock::now() + 2s;
            while (!control->stop_requested() && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(1ms);
            return json{{"success", true}};
        });
    auto result = std::async(std::launch::async, [&] { return server.process_request(tool(7)); });
    REQUIRE(started.get_future().wait_for(1s) == std::future_status::ready);
    for (const auto& malformed : {cancel("7"), cancel(7.0), cancel(true)}) {
        CHECK(server.process_request(malformed).is_null());
        CHECK(result.wait_for(5ms) == std::future_status::timeout);
    }
    CHECK(server.process_request(cancel(7)).is_null());
    REQUIRE(result.wait_for(1s) == std::future_status::ready);
    CHECK(result.get().is_null());
    CHECK(entered == 1);
    CHECK_FALSE(current_request_control());
}

TEST_CASE("A request deadline declines before invoking the registered handler",
          "[mcp][cancellation][deadline]") {
    McpServer server;
    unsigned entered = 0;
    server.register_tool("controlled", "Must not begin", json::object(), [&](const json&) {
        ++entered;
        return json{{"success", true}};
    });
    auto control =
        std::make_shared<RequestControl>("7", "tools/call", std::chrono::steady_clock::now() - 1ms);
    RequestControlScope scope(control);
    const auto result = server.process_request(tool(7));
    CHECK(result.at("error").at("code") == -32000);
    CHECK(entered == 0);
    CHECK(native_request_revoked());
}

TEST_CASE("Actual stream input can cancel an active tool and still receive a later ping",
          "[mcp][stdio][cancellation]") {
    McpServer server;
    OwnedIo buffer;
    std::promise<void> started;
    std::atomic<unsigned> effects{0};
    server.register_tool(
        "controlled", "Wait before a modeled effect", json::object(), [&](const json&) {
            started.set_value();
            const auto control = current_request_control();
            if (!control) return json{{"missing_control", true}};
            const auto until = std::chrono::steady_clock::now() + 2s;
            while (!control->stop_requested() && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(1ms);
            if (!native_request_revoked()) ++effects;
            return json{{"success", true}};
        });
    auto running = std::async(std::launch::async, [&] { server.run(buffer); });
    buffer.feed(tool(7).dump() + "\n");
    REQUIRE(started.get_future().wait_for(1s) == std::future_status::ready);
    buffer.feed(cancel(7).dump() + "\n" + R"({"jsonrpc":"2.0","id":8,"method":"ping"})" + "\n");
    buffer.close();
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    const auto reply = json::parse(buffer.output());
    CHECK(reply.at("id") == 8);
    CHECK(reply.at("result") == json::object());
    CHECK(effects == 0);
}

TEST_CASE("Native permission ends at input EOF while offline response drain remains available",
          "[mcp][stdio][cancellation][eof]") {
    McpServer server;
    std::atomic<bool> revoked{false};
    server.register_tool(
        "controlled", "Observe EOF before any native write", json::object(), [&](const json&) {
            const auto control = current_request_control();
            const auto until = std::chrono::steady_clock::now() + 1s;
            while (control && !control->input_closed && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(1ms);
            revoked = native_request_revoked();
            return json{{"success", true}, {"native_write_allowed", !revoked.load()}};
        });
    std::istringstream input(tool(7).dump() + "\n");
    std::ostringstream output;
    server.run(input, output);
    CHECK(revoked);
    CHECK(
        json::parse(output.str()).at("result").at("structuredContent").at("native_write_allowed") ==
        false);
}

TEST_CASE("Oversized input is drained without retaining its tail or losing the next request",
          "[mcp][stdio][bounded]") {
    McpServer server;
    std::istringstream input(std::string(MCP_MAX_INPUT_BYTES + 17, 'x') + "\n" +
                             R"({"jsonrpc":"2.0","id":8,"method":"ping"})" + "\n");
    std::ostringstream output;
    server.run(input, output);
    std::istringstream replies(output.str());
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(replies, line)));
    CHECK(json::parse(line).at("error").at("code") == -32600);
    REQUIRE(static_cast<bool>(std::getline(replies, line)));
    CHECK(json::parse(line).at("id") == 8);
    CHECK_FALSE(static_cast<bool>(std::getline(replies, line)));
}

TEST_CASE("Malformed input cannot block cancellation behind unread error output",
          "[mcp][stdio][cancellation][backpressure]") {
    McpServer server;
    OwnedIo io;
    io.blocked_output = true;
    std::promise<void> started, cancelled;
    std::atomic<unsigned> effects{0};
    server.register_tool("controlled", "Wait for cancellation", json::object(), [&](const json&) {
        started.set_value();
        const auto control = current_request_control();
        const auto until = std::chrono::steady_clock::now() + 2s;
        while (control && !control->stop_requested() && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(1ms);
        if (!native_request_revoked()) ++effects;
        cancelled.set_value();
        return json{{"success", true}};
    });
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed("{\n");
    const bool writer_blocked = io.wait_blocked();
    io.feed(tool(7).dump() + "\n");
    const bool handler_started = started.get_future().wait_for(1s) == std::future_status::ready;
    io.feed("{\n" + cancel(7).dump() + "\n");
    const bool cancellation_arrived =
        cancelled.get_future().wait_for(1s) == std::future_status::ready;
    io.close();
    io.release_output();
    CHECK(writer_blocked);
    CHECK(handler_started);
    CHECK(cancellation_arrived);
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    CHECK(effects == 0);
    std::istringstream lines(io.output());
    std::string line;
    unsigned errors = 0;
    while (std::getline(lines, line)) {
        CHECK(json::parse(line).at("error").at("code") == -32700);
        ++errors;
    }
    CHECK(errors == 2);
}

TEST_CASE("Owned input wakes on stop and later admissions inherit revocation",
          "[mcp][stdio][cancellation][shutdown]") {
    McpServer server;
    OwnedIo io;
    std::atomic<unsigned> effects{0};
    server.register_tool("controlled", "Must not begin", json::object(), [&](const json&) {
        ++effects;
        return json{{"success", true}};
    });
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    const bool reader_waiting = io.wait_reading();
    server.stop();
    io.feed(tool(7).dump() + "\n");
    CHECK(reader_waiting);
    REQUIRE(running.wait_for(1s) == std::future_status::ready);
    running.get();
    CHECK(server.process_request(tool(8)).is_null());
    CHECK(effects == 0);
    CHECK(io.output().empty());
}

TEST_CASE("An output deadline revokes active native permission without requiring input EOF",
          "[mcp][stdio][cancellation][backpressure][deadline]") {
    McpServer server;
    OwnedIo io;
    io.blocked_output = true;
    io.write_budget = 500ms;
    std::promise<void> started;
    std::atomic<bool> revoked{false};
    server.register_tool("controlled", "Observe output failure", json::object(), [&](const json&) {
        started.set_value();
        const auto control = current_request_control();
        const auto until = std::chrono::steady_clock::now() + 1s;
        while (control && !control->stop_requested() && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(1ms);
        revoked = native_request_revoked();
        return json{{"success", true}};
    });
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed("{\n");
    const bool output_blocked = io.wait_blocked();
    io.feed(tool(7).dump() + "\n");
    const bool handler_started = started.get_future().wait_for(1s) == std::future_status::ready;
    CHECK(output_blocked);
    CHECK(handler_started);
    REQUIRE(running.wait_for(1s) == std::future_status::ready);
    CHECK_THROWS_WITH(running.get(), "MCP session could not preserve bounded I/O");
    CHECK(revoked);
    CHECK(server.process_request(tool(8)).is_null());
}

TEST_CASE("Saturated admission still returns null-ID errors for invalid requests",
          "[mcp][stdio][admission][bounded]") {
    McpServer server;
    OwnedIo io;
    std::promise<void> started;
    std::atomic<bool> release{false};
    server.register_tool(
        "controlled", "Hold serialized execution", json::object(), [&](const json&) {
            started.set_value();
            const auto until = std::chrono::steady_clock::now() + 2s;
            while (!release && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(1ms);
            return json{{"success", true}};
        });
    auto running = std::async(std::launch::async, [&] { server.run(io); });
    io.feed(tool(7).dump() + "\n");
    const bool handler_started = started.get_future().wait_for(1s) == std::future_status::ready;
    for (int id = 20; id < 44; ++id)
        io.feed(json{{"jsonrpc", "2.0"}, {"id", id}, {"method", "ping"}}.dump() + "\n");
    io.feed("1\n{}\n{\"jsonrpc\":\"2.0\",\"method\":\"notice\"}\n");
    const auto until = std::chrono::steady_clock::now() + 1s;
    while (io.output().find("missing jsonrpc") == std::string::npos &&
           std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(1ms);
    release = true;
    io.close();
    CHECK(handler_started);
    REQUIRE(running.wait_for(2s) == std::future_status::ready);
    running.get();
    std::istringstream lines(io.output());
    std::string line;
    unsigned invalid = 0;
    while (std::getline(lines, line)) {
        const auto response = json::parse(line);
        if (response.contains("error") && response.at("error").at("code") == -32600) {
            CHECK(response.at("id").is_null());
            ++invalid;
        }
    }
    CHECK(invalid == 2);
}
