/** Analytical methods, unavailable samples and schema migration are observable contracts. */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/validation.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/score/workflows.hpp>

using namespace sunny::core;
using json = nlohmann::json;

namespace {
IngestedWork observed_work() {
    IngestedWork work;
    work.id = IngestedWorkId{7};
    work.metadata.title = "Two exact occurrences";
    work.analysis_complete = true;
    VoiceMelodicAnalysis voice;
    voice.part_id = PartId{2};
    voice.voice_index = 3;
    voice.note_count = 6;
    work.analysis.melodic_analysis.per_voice_analysis = {voice};
    work.analysis.melodic_analysis.primary_melody_voice = PartId{2};
    work.analysis.melodic_analysis.primary_melody_voice_index = 3;
    ThematicUnit theme;
    theme.id = ThematicUnitId{1};
    theme.intervals = {2, -1};
    theme.rhythm = {0.25f, 0.25f, 0.5f};
    theme.contour = {1, -1};
    theme.occurrences = {{ScoreTime{1, Beat::zero()},
                          PartId{2},
                          ThematicTransformation::Original,
                          "C major",
                          3,
                          ScoreTime{2, Beat::zero()}},
                         {ScoreTime{3, Beat::zero()},
                          PartId{2},
                          ThematicTransformation::TransposedExact,
                          "G major",
                          3,
                          ScoreTime{4, Beat::zero()}}};
    work.analysis.melodic_analysis.thematic_material = {theme};
    work.analysis.motivic_analysis.thematic_units = {theme};
    work.analysis.evidence["motivic"] = {AnalysisEvidenceKind::ExactSymbolic,
                                         "repeated exact monophonic windows",
                                         std::nullopt,
                                         {"human_theme_interpretation"},
                                         6};
    work.analysis.evidence["dynamic"] = {AnalysisEvidenceKind::Unavailable,
                                         "notated semantic dynamics",
                                         "No semantic markings",
                                         {},
                                         0};
    return work;
}
} // namespace

TEST_CASE("Corpus v5 preserves exact thematic spans and analytical methods",
          "[corpus-ir][evidence][serialisation]") {
    const auto original = observed_work();
    const auto encoded = ingested_work_to_json(original);
    REQUIRE(encoded["schema_version"] == 5);
    const auto restored = ingested_work_from_json(encoded);
    REQUIRE(restored);
    CHECK(ingested_work_to_json(*restored) == encoded);
    const auto& occurrence = restored->analysis.motivic_analysis.thematic_units[0].occurrences[1];
    CHECK(occurrence.part_id == PartId{2});
    CHECK(occurrence.voice_index == 3);
    CHECK(occurrence.position == ScoreTime{3, Beat::zero()});
    CHECK(occurrence.end == ScoreTime{4, Beat::zero()});
    CHECK(restored->analysis.evidence.at("motivic").observations == 6);
    CHECK(restored->analysis.evidence.at("dynamic").kind == AnalysisEvidenceKind::Unavailable);
}

TEST_CASE("Corpus v4 migration retains supplied values with unknown voice and span provenance",
          "[corpus-ir][evidence][migration]") {
    auto encoded = ingested_work_to_json(observed_work());
    encoded["schema_version"] = 4;
    auto& analysis = encoded["analysis"];
    analysis.erase("evidence");
    analysis["melodic_analysis"].erase("primary_melody_voice_index");
    analysis["melodic_analysis"]["per_voice_analysis"][0].erase("voice_index");
    for (auto* themes : {&analysis["melodic_analysis"]["thematic_material"],
                         &analysis["motivic_analysis"]["thematic_units"]})
        for (auto& theme : *themes)
            for (auto& occurrence : theme["occurrences"]) {
                occurrence.erase("voice_index");
                occurrence.erase("end");
            }
    const auto restored = ingested_work_from_json(encoded);
    REQUIRE(restored);
    CHECK(restored->analysis.evidence.empty());
    CHECK_FALSE(restored->analysis.melodic_analysis.primary_melody_voice_index);
    CHECK_FALSE(restored->analysis.melodic_analysis.per_voice_analysis[0].voice_index);
    const auto& theme = restored->analysis.motivic_analysis.thematic_units[0];
    CHECK(theme.intervals == std::vector<std::int8_t>{2, -1});
    CHECK(theme.rhythm == std::vector<float>{0.25f, 0.25f, 0.5f});
    CHECK_FALSE(theme.occurrences[0].voice_index);
    CHECK_FALSE(theme.occurrences[0].end);
    CHECK(theme.occurrences[1].position == ScoreTime{3, Beat::zero()});
    CHECK(theme.occurrences[1].key == "G major");
}

TEST_CASE("Corpus v5 rejects contradictory evidence and malformed exact provenance",
          "[corpus-ir][evidence][validation]") {
    const auto encoded = ingested_work_to_json(observed_work());
    SECTION("unknown domain") {
        auto invalid = encoded;
        invalid["analysis"]["evidence"]["imaginary"] = invalid["analysis"]["evidence"]["motivic"];
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    for (const auto& [field, value] : std::vector<std::pair<std::string, json>>{
             {"kind", 4},
             {"method", ""},
             {"observations", 0},
             {"observations", -1},
             {"unavailable_reason", "Contradicts computed evidence"},
             {"unavailable_fields", json::array({"same", "same"})}}) {
        DYNAMIC_SECTION("invalid computed evidence " << field << "=" << value.dump()) {
            auto invalid = encoded;
            invalid["analysis"]["evidence"]["motivic"][field] = value;
            CHECK_FALSE(ingested_work_from_json(invalid));
        }
    }
    SECTION("unavailable sample cannot contribute observations") {
        auto invalid = encoded;
        invalid["analysis"]["evidence"]["dynamic"]["observations"] = 1;
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    SECTION("unavailable computation needs a reason") {
        auto invalid = encoded;
        invalid["analysis"]["evidence"]["dynamic"]["unavailable_reason"] = nullptr;
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    SECTION("missing new metadata is refused in v5") {
        auto invalid = encoded;
        invalid["analysis"].erase("evidence");
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    SECTION("out-of-domain voice") {
        auto invalid = encoded;
        invalid["analysis"]["motivic_analysis"]["thematic_units"][0]["occurrences"][0]
               ["voice_index"] = 256;
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    SECTION("empty exact span") {
        auto invalid = encoded;
        auto& occurrence =
            invalid["analysis"]["motivic_analysis"]["thematic_units"][0]["occurrences"][0];
        occurrence["end"] = occurrence["position"];
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
}

TEST_CASE("Unavailable analyses do not dilute domain averages while legacy data stays unqualified",
          "[corpus-ir][evidence][aggregation]") {
    CorpusDatabase corpus;
    corpus.composers[1] = create_composer_profile(ComposerProfileId{1}, "Finite samples");
    auto measured = observed_work();
    measured.analysis.motivic_analysis.thematic_density = 0.75f;
    measured.analysis.motivic_analysis.thematic_economy = 0.5f;
    auto unavailable = measured;
    unavailable.id = IngestedWorkId{8};
    unavailable.analysis.motivic_analysis = {};
    unavailable.analysis.evidence["motivic"] = {AnalysisEvidenceKind::Unavailable,
                                                "repeated exact monophonic windows",
                                                "No eligible windows",
                                                {},
                                                0};
    corpus.works[7] = measured;
    corpus.works[8] = unavailable;
    REQUIRE(assign_work_to_composer(corpus, IngestedWorkId{7}, ComposerProfileId{1}));
    REQUIRE(assign_work_to_composer(corpus, IngestedWorkId{8}, ComposerProfileId{1}));
    const auto& first = corpus.composers[1].style_profile;
    CHECK(first.sample_size == 2);
    CHECK(first.motivic_profile.development_density == Catch::Approx(0.75));
    CHECK(first.motivic_profile.thematic_economy == Catch::Approx(0.5));
    auto legacy = measured;
    legacy.id = IngestedWorkId{9};
    legacy.analysis.evidence.clear();
    legacy.analysis.motivic_analysis.thematic_density = 0.25f;
    legacy.analysis.motivic_analysis.thematic_economy = 0.25f;
    corpus.works[9] = legacy;
    REQUIRE(assign_work_to_composer(corpus, IngestedWorkId{9}, ComposerProfileId{1}));
    const auto& combined = corpus.composers[1].style_profile;
    CHECK(combined.sample_size == 3);
    CHECK(combined.motivic_profile.development_density == Catch::Approx(0.5));
    CHECK(combined.motivic_profile.thematic_economy == Catch::Approx(0.375));
    REQUIRE(corpus_from_json(corpus_to_json(corpus)));
}

TEST_CASE("Thematic voice provenance belongs to the occurrence's starting bar",
          "[corpus-ir][evidence][validation][provenance]") {
    ScoreSpec spec;
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.parts.resize(2);
    for (auto& part : spec.parts) {
        part.name = "Voice";
        part.instrument_type = InstrumentType::Synthesiser;
    }
    auto score = create_score(spec);
    REQUIRE(score);
    REQUIRE(add_voice(*score, 2, score->parts[1].id, 3));
    auto work = observed_work();
    auto& themes = work.analysis.motivic_analysis.thematic_units;
    themes[0].occurrences.resize(1);
    themes[0].occurrences[0].part_id = score->parts[1].id;
    work.analysis.melodic_analysis.thematic_material.clear();
    work.score = std::move(*score);
    const auto invalid = validate_ingested_work(work);
    CHECK(std::ranges::any_of(invalid,
                              [](const auto& diagnostic) { return diagnostic.rule == "C17"; }));
    themes[0].occurrences[0].position = ScoreTime{2, Beat::zero()};
    themes[0].occurrences[0].end = ScoreTime{3, Beat::zero()};
    CHECK(std::ranges::none_of(validate_ingested_work(work),
                               [](const auto& diagnostic) { return diagnostic.rule == "C17"; }));
    auto encoded = ingested_work_to_json(work);
    encoded["analysis"]["motivic_analysis"]["thematic_units"][0]["occurrences"][0]["position"]
           ["bar"] = 4294967295U;
    CHECK_FALSE(ingested_work_from_json(encoded));
}

TEST_CASE("Optional melodic lane identities close against the embedded Score",
          "[corpus-ir][evidence][validation][provenance]") {
    ScoreSpec spec;
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.parts.resize(2);
    for (auto& part : spec.parts) {
        part.name = "Voice";
        part.instrument_type = InstrumentType::Synthesiser;
    }
    auto score = create_score(spec);
    REQUIRE(score);
    REQUIRE(add_voice(*score, 2, score->parts[1].id, 3));
    auto work = observed_work();
    work.analysis.melodic_analysis.thematic_material.clear();
    work.analysis.motivic_analysis.thematic_units.clear();
    work.score = std::move(*score);
    REQUIRE(ingested_work_from_json(ingested_work_to_json(work)));
    const auto encoded = ingested_work_to_json(work);
    for (const auto field : {"part", "voice"}) {
        auto invalid = encoded;
        auto& melodic = invalid["analysis"]["melodic_analysis"];
        if (std::string(field) == "part")
            melodic["primary_melody_voice"] = 999;
        else
            melodic["primary_melody_voice_index"] = 4;
        CHECK_FALSE(ingested_work_from_json(invalid));
        invalid = encoded;
        auto& lane = invalid["analysis"]["melodic_analysis"]["per_voice_analysis"][0];
        if (std::string(field) == "part")
            lane["part_id"] = 999;
        else
            lane["voice_index"] = 4;
        CHECK_FALSE(ingested_work_from_json(invalid));
    }
    // The legacy absent index does not invent a lane and stays unqualified.
    auto legacy = work;
    legacy.analysis.melodic_analysis.primary_melody_voice_index.reset();
    legacy.analysis.melodic_analysis.primary_melody_voice = PartId{999};
    legacy.analysis.melodic_analysis.per_voice_analysis[0].voice_index.reset();
    legacy.analysis.melodic_analysis.per_voice_analysis[0].part_id = PartId{999};
    CHECK(ingested_work_from_json(ingested_work_to_json(legacy)));
}
