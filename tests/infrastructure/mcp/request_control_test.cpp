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
    McpReadResult read(char& byte, const std::atomic<bool>& running) override {
        std::unique_lock lock(mutex_);
        reading_ = true;
        changed_.notify_all();
        while (running.load() && !closed_ && bytes_.empty())
            changed_.wait_for(lock, 10ms);
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
    bool closed_ = false, reading_ = false, writing_ = false;
};
} // namespace

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
