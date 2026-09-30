/**
 * @file validation_test.cpp
 * @brief Cross-IR project validation tests
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/workflows.hpp>

using namespace sunny::core;

namespace {

Score make_score() {
    ScoreSpec spec;
    spec.title = "Project";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.parts.resize(2);
    spec.parts[0].name = "One";
    spec.parts[0].instrument_type = InstrumentType::Synthesiser;
    spec.parts[1].name = "Two";
    spec.parts[1].instrument_type = InstrumentType::Synthesiser;
    auto score = create_score(spec);
    REQUIRE(score.has_value());
    return std::move(*score);
}

TimbreProfile make_profile(std::uint64_t id, PartId part) {
    auto profile = create_timbre_profile(TimbreProfileId{id}, part, "Profile");
    SubtractiveSynth source;
    source.oscillators.push_back(Oscillator{});
    profile.source.data = std::move(source);
    return profile;
}

} // namespace

TEST_CASE("project validation accepts exact cross-IR correspondence", "[project][validation]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});

    const ProjectView project{score, profiles, mix};
    const auto diagnostics = validate_project(project);
    CHECK(is_project_compilable(project));
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    }));
}

TEST_CASE("project validation rejects duplicate and missing bindings", "[project][validation]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto duplicate = make_profile(2, score.parts[0].id);
    const std::vector<const TimbreProfile*> profiles{&first, &duplicate};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[0].id});

    const ProjectView project{score, profiles, mix};
    const auto diagnostics = validate_project(project);
    CHECK_FALSE(is_project_compilable(project));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.error_code == ErrorCode::DuplicateProfileForPart;
    }));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.error_code == ErrorCode::MissingChannel;
    }));
}

TEST_CASE("project validation rejects null component references", "[project][validation]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    const std::vector<const TimbreProfile*> profiles{&first, &second, nullptr};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});

    const auto diagnostics = validate_project(ProjectView{score, profiles, mix});
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.error_code == ErrorCode::ProjectMissingComponent;
    }));
}

TEST_CASE("project validation closes AudioFollower Part references", "[project][validation]") {
    auto score = make_score();
    auto first = make_profile(1, score.parts[0].id);
    auto second = make_profile(2, score.parts[1].id);
    first.modulation.routings = {
        {{ModulationSourceType::AudioFollower, 0, 0, PartId{999}}, "source.filter.cutoff", 0.5f}};
    const std::vector<const TimbreProfile*> profiles{&first, &second};
    auto mix = create_mix_graph(MixGraphId{1}, {score.parts[0].id, score.parts[1].id});

    const auto diagnostics = validate_project(ProjectView{score, profiles, mix});
    CHECK_FALSE(is_project_compilable(ProjectView{score, profiles, mix}));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.rule == "P2" && diagnostic.error_code == ErrorCode::InvalidModSource;
    }));

    first.modulation.routings.front().source.sidechain_part = score.parts[1].id;
    const auto valid = validate_project(ProjectView{score, profiles, mix});
    CHECK(std::none_of(valid.begin(), valid.end(), [](const auto& diagnostic) {
        return diagnostic.rule == "P2";
    }));
}
