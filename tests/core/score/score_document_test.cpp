/**
 * @file score_document_test.cpp
 * @brief ScoreDocument concurrency and atomic-publication tests
 */

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <latch>
#include <limits>
#include <optional>
#include <sunny/core/score/score_document.hpp>
#include <sunny/core/score/workflows.hpp>
#include <thread>

using namespace std::chrono_literals;
using namespace sunny::core;

namespace {

Score make_score(std::string title = "Concurrent score") {
    ScoreSpec spec;
    spec.title = std::move(title);
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};

    PartDefinition part;
    part.name = "Piano";
    part.abbreviation = "Pno.";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(std::move(part));

    return create_score(spec).value();
}

ScoreDocument make_document(std::string title = "Concurrent score") {
    return ScoreDocument::create(make_score(std::move(title))).value();
}

} // namespace

TEST_CASE("ScoreDocument rejects an invalid initial value", "[score-ir][concurrency]") {
    Score invalid = make_score();
    invalid.parts.clear();

    const auto result = ScoreDocument::create(std::move(invalid));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvariantViolation);
}

TEST_CASE("ScoreDocument publishes immutable versioned snapshots", "[score-ir][concurrency]") {
    ScoreDocument document = make_document("Before");
    ScoreDocument second_handle = document;
    const auto before = document.snapshot();

    const auto result = document.transact([](Score& candidate) -> Result<int> {
        candidate.metadata.title = "After";
        return 7;
    });

    REQUIRE(result.has_value());
    CHECK(*result == before->version + 1);
    CHECK(document.version() == *result);
    CHECK(before->metadata.title == "Before");
    CHECK(second_handle.snapshot()->metadata.title == "After");
    CHECK(second_handle.snapshot()->version == *result);
    CHECK(second_handle.snapshot() != before);
}

TEST_CASE("ScoreDocument rejects version exhaustion before candidate work",
          "[score-ir][concurrency]") {
    Score exhausted = make_score();
    exhausted.version = std::numeric_limits<std::uint64_t>::max();
    ScoreDocument document = ScoreDocument::create(std::move(exhausted)).value();
    const auto before = document.snapshot();
    bool invoked = false;

    const auto result = document.transact([&](Score&) -> Result<void> {
        invoked = true;
        return {};
    });

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ArithmeticOverflow);
    CHECK_FALSE(invoked);
    CHECK(document.snapshot() == before);
}

TEST_CASE("ScoreDocument rejection preserves snapshot identity and version",
          "[score-ir][concurrency]") {
    ScoreDocument document = make_document();
    const auto before = document.snapshot();

    const auto callback_failure = document.transact([](Score& candidate) -> Result<void> {
        candidate.metadata.title = "Must not escape";
        return std::unexpected(ErrorCode::InvalidMutation);
    });
    REQUIRE_FALSE(callback_failure.has_value());
    CHECK(document.snapshot() == before);
    CHECK(document.version() == before->version);

    const auto invalid_candidate = document.transact([](Score& candidate) -> Result<void> {
        candidate.parts.clear();
        return {};
    });
    REQUIRE_FALSE(invalid_candidate.has_value());
    CHECK(invalid_candidate.error() == ErrorCode::InvalidMutation);
    CHECK(document.snapshot() == before);
    CHECK(document.version() == before->version);
}

TEST_CASE("ScoreDocument readers retain the old version during candidate work",
          "[score-ir][concurrency]") {
    ScoreDocument document = make_document("Committed");
    std::latch writer_entered{1};
    std::latch release_writer{1};
    std::optional<Result<std::uint64_t>> writer_result;

    std::thread writer([&] {
        writer_result = document.transact([&](Score& candidate) -> Result<void> {
            writer_entered.count_down();
            release_writer.wait();
            candidate.metadata.title = "New commit";
            return {};
        });
    });

    writer_entered.wait();
    auto reader = std::async(std::launch::async, [&] { return document.snapshot(); });
    const auto reader_status = reader.wait_for(250ms);
    release_writer.count_down();
    writer.join();

    REQUIRE(reader_status == std::future_status::ready);
    const auto during = reader.get();
    CHECK(during->metadata.title == "Committed");
    REQUIRE(writer_result.has_value());
    REQUIRE(writer_result->has_value());
    CHECK(document.snapshot()->metadata.title == "New commit");
}

TEST_CASE("ScoreDocument serializes writers on shared handles", "[score-ir][concurrency]") {
    ScoreDocument document = make_document();
    ScoreDocument second_handle = document;
    std::latch first_entered{1};
    std::latch release_first{1};
    std::latch second_attempted{1};
    std::atomic_bool second_entered{false};
    std::optional<Result<std::uint64_t>> first_result;

    std::thread first([&] {
        first_result = document.transact([&](Score& candidate) -> Result<void> {
            first_entered.count_down();
            release_first.wait();
            candidate.metadata.title = "First";
            return {};
        });
    });
    first_entered.wait();

    auto second = std::async(std::launch::async, [&] {
        second_attempted.count_down();
        return second_handle.transact([&](Score& candidate) -> Result<void> {
            second_entered.store(true, std::memory_order_release);
            candidate.metadata.title = "Second";
            return {};
        });
    });
    second_attempted.wait();
    const auto second_status = second.wait_for(250ms);
    const bool overlapped = second_entered.load(std::memory_order_acquire);
    release_first.count_down();
    first.join();
    const auto second_result = second.get();

    CHECK(second_status == std::future_status::timeout);
    CHECK_FALSE(overlapped);
    REQUIRE(first_result.has_value());
    REQUIRE(first_result->has_value());
    REQUIRE(second_result.has_value());
    CHECK(*second_result == **first_result + 1);
    CHECK(document.snapshot()->metadata.title == "Second");
}
