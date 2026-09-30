/**
 * @file compilation_workflows_test.cpp
 * @brief Infrastructure compilation wrappers unit tests
 *
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/compilation_workflows.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>

// Include Score IR creation function
#include <sunny/core/score/workflows.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;

namespace {

/// Create a simple valid score for testing compilation
Score make_test_score() {
    ScoreSpec spec;
    spec.title = "Test Score";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4}; // C
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    piano_def.clef = Clef::Treble;
    piano_def.rendering.midi_channel = 1;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

} // namespace

TEST_CASE("compile_to_musicxml produces parseable XML", "") {
    auto score = make_test_score();
    auto result = compile_to_musicxml(score);
    REQUIRE(result.has_value());
    REQUIRE(!result->xml.empty());

    // Verify the XML is parseable by the existing parser
    auto parsed = sunny::infrastructure::formats::parse_musicxml(result->xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts.size() == 1);
}

TEST_CASE("compile_to_lilypond produces.ly content with version header", "") {
    auto score = make_test_score();
    auto result = compile_to_lilypond(score);
    REQUIRE(result.has_value());
    REQUIRE(!result->ly.empty());

    // Verify the output starts with \version
    REQUIRE(result->ly.find("\\version") != std::string::npos);
}
