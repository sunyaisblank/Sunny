/**
 * @file TSFM001A.cpp
 * @brief Unit tests for Form and Motivic Analysis (§10.1–10.4)
 *
 * Component: TSFM001A
 * Tests: FMST001A, FMMT001A
 * Validates: Formal Spec §10.1–10.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Form/FMST001A.h"
#include "Form/FMMT001A.h"

using namespace Sunny::Core;
using Catch::Matchers::WithinAbs;

// =============================================================================
// §10.3 Sectional Form Classification (FMST001A)
// =============================================================================

TEST_CASE("FMST001A: classify strophic form", "[form][core]") {
    std::vector<std::string> labels = {"A", "A", "A", "A"};
    REQUIRE(classify_form(labels) == SectionalForm::Strophic);
}

TEST_CASE("FMST001A: classify binary form", "[form][core]") {
    std::vector<std::string> labels = {"A", "B"};
    REQUIRE(classify_form(labels) == SectionalForm::Binary);
}

TEST_CASE("FMST001A: classify ternary form", "[form][core]") {
    std::vector<std::string> labels = {"A", "B", "A"};
    REQUIRE(classify_form(labels) == SectionalForm::Ternary);
}

TEST_CASE("FMST001A: classify rounded binary form", "[form][core]") {
    std::vector<std::string> labels = {"A", "B", "A'"};
    REQUIRE(classify_form(labels) == SectionalForm::RoundedBinary);
}

TEST_CASE("FMST001A: classify rondo form", "[form][core]") {
    std::vector<std::string> labels = {"A", "B", "A", "C", "A"};
    REQUIRE(classify_form(labels) == SectionalForm::Rondo);
}

TEST_CASE("FMST001A: classify sonata form", "[form][core]") {
    std::vector<std::string> labels = {"Exposition", "Development", "Recapitulation"};
    REQUIRE(classify_form(labels) == SectionalForm::Sonata);
}

TEST_CASE("FMST001A: classify theme and variations", "[form][core]") {
    std::vector<std::string> labels = {"A", "A'", "A''", "A'''"};
    REQUIRE(classify_form(labels) == SectionalForm::ThemeVariations);
}

TEST_CASE("FMST001A: classify through-composed", "[form][core]") {
    std::vector<std::string> labels = {"A", "B", "C", "D"};
    REQUIRE(classify_form(labels) == SectionalForm::ThroughComposed);
}

TEST_CASE("FMST001A: single section is unknown", "[form][core]") {
    std::vector<std::string> labels = {"A"};
    REQUIRE(classify_form(labels) == SectionalForm::Unknown);
}

TEST_CASE("FMST001A: form_name returns correct strings", "[form][core]") {
    REQUIRE(form_name(SectionalForm::Binary) == "Binary");
    REQUIRE(form_name(SectionalForm::Ternary) == "Ternary");
    REQUIRE(form_name(SectionalForm::Rondo) == "Rondo");
    REQUIRE(form_name(SectionalForm::Sonata) == "Sonata");
    REQUIRE(form_name(SectionalForm::Strophic) == "Strophic");
}

// =============================================================================
// §10.2 Phrase Classification (FMST001A)
// =============================================================================

TEST_CASE("FMST001A: classify parallel period", "[form][core]") {
    // Antecedent ending with HC, consequent ending with PAC
    // Both begin with same pitches
    Motif m1{.label = "a", .span = {Beat{0, 1}, Beat{4, 1}},
             .pitches = {60, 62, 64, 65}, .durations = {Beat{1, 1}, Beat{1, 1}, Beat{1, 1}, Beat{1, 1}}};
    Motif m2{.label = "a", .span = {Beat{4, 1}, Beat{8, 1}},
             .pitches = {60, 62, 64, 67}, .durations = {Beat{1, 1}, Beat{1, 1}, Beat{1, 1}, Beat{1, 1}}};

    SubPhrase sp1{.motifs = {m1}, .span = {Beat{0, 1}, Beat{4, 1}}};
    SubPhrase sp2{.motifs = {m2}, .span = {Beat{4, 1}, Beat{8, 1}}};

    Phrase antecedent{.subphrases = {sp1}, .cadence = CadenceType::HC,
                      .span = {Beat{0, 1}, Beat{4, 1}}, .bar_count = 4};
    Phrase consequent{.subphrases = {sp2}, .cadence = CadenceType::PAC,
                      .span = {Beat{4, 1}, Beat{8, 1}}, .bar_count = 4};

    auto result = classify_period(antecedent, consequent);
    REQUIRE(result == PhraseType::ParallelPeriod);
}

TEST_CASE("FMST001A: classify contrasting period", "[form][core]") {
    Motif m1{.label = "a", .span = {Beat{0, 1}, Beat{4, 1}},
             .pitches = {60, 62, 64, 65}, .durations = {Beat{1, 1}, Beat{1, 1}, Beat{1, 1}, Beat{1, 1}}};
    Motif m2{.label = "b", .span = {Beat{4, 1}, Beat{8, 1}},
             .pitches = {72, 71, 69, 67}, .durations = {Beat{1, 1}, Beat{1, 1}, Beat{1, 1}, Beat{1, 1}}};

    SubPhrase sp1{.motifs = {m1}, .span = {Beat{0, 1}, Beat{4, 1}}};
    SubPhrase sp2{.motifs = {m2}, .span = {Beat{4, 1}, Beat{8, 1}}};

    Phrase antecedent{.subphrases = {sp1}, .cadence = CadenceType::HC,
                      .span = {Beat{0, 1}, Beat{4, 1}}, .bar_count = 4};
    Phrase consequent{.subphrases = {sp2}, .cadence = CadenceType::PAC,
                      .span = {Beat{4, 1}, Beat{8, 1}}, .bar_count = 4};

    auto result = classify_period(antecedent, consequent);
    REQUIRE(result == PhraseType::ContrastingPeriod);
}

TEST_CASE("FMST001A: non-period returns unclassified", "[form][core]") {
    Motif m1{.label = "a", .span = {Beat{0, 1}, Beat{4, 1}},
             .pitches = {60, 62, 64, 65}, .durations = {Beat{1, 1}, Beat{1, 1}, Beat{1, 1}, Beat{1, 1}}};
    SubPhrase sp1{.motifs = {m1}, .span = {Beat{0, 1}, Beat{4, 1}}};

    // Both end with PAC — not a valid period (needs HC + PAC)
    Phrase p1{.subphrases = {sp1}, .cadence = CadenceType::PAC,
              .span = {Beat{0, 1}, Beat{4, 1}}, .bar_count = 4};
    Phrase p2{.subphrases = {sp1}, .cadence = CadenceType::PAC,
              .span = {Beat{4, 1}, Beat{8, 1}}, .bar_count = 4};

    REQUIRE(classify_period(p1, p2) == PhraseType::Unclassified);
}

TEST_CASE("FMST001A: is_sentence with 8-bar structure", "[form][core]") {
    // Presentation: 2 motifs across 4 bars, continuation: 4 bars ending with PAC
    Motif basic_idea{.label = "a", .span = {Beat{0, 1}, Beat{2, 1}},
                     .pitches = {60, 62}, .durations = {Beat{1, 1}, Beat{1, 1}}};
    Motif repetition{.label = "a'", .span = {Beat{2, 1}, Beat{4, 1}},
                     .pitches = {60, 64}, .durations = {Beat{1, 1}, Beat{1, 1}}};
    Motif cont1{.label = "b", .span = {Beat{4, 1}, Beat{6, 1}},
                .pitches = {65, 67}, .durations = {Beat{1, 1}, Beat{1, 1}}};
    Motif cont2{.label = "c", .span = {Beat{6, 1}, Beat{8, 1}},
                .pitches = {69, 72}, .durations = {Beat{1, 1}, Beat{1, 1}}};

    SubPhrase pres_sp{.motifs = {basic_idea, repetition}, .span = {Beat{0, 1}, Beat{4, 1}}};
    SubPhrase cont_sp{.motifs = {cont1, cont2}, .span = {Beat{4, 1}, Beat{8, 1}}};

    Phrase presentation{.subphrases = {pres_sp}, .cadence = CadenceType::None,
                        .span = {Beat{0, 1}, Beat{4, 1}}, .bar_count = 4};
    Phrase continuation{.subphrases = {cont_sp}, .cadence = CadenceType::PAC,
                        .span = {Beat{4, 1}, Beat{8, 1}}, .bar_count = 4};

    PhraseGroup group{.phrases = {presentation, continuation},
                      .phrase_type = PhraseType::Unclassified,
                      .span = {Beat{0, 1}, Beat{8, 1}}};

    REQUIRE(is_sentence(group));
}

// =============================================================================
// §10.4 Motivic Transformations (FMMT001A)
// =============================================================================

TEST_CASE("FMMT001A: motif_transpose identity", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto result = motif_transpose(pitches, 0);
    REQUIRE(result.has_value());
    REQUIRE(*result == pitches);
}

TEST_CASE("FMMT001A: motif_transpose up", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto result = motif_transpose(pitches, 5);
    REQUIRE(result.has_value());
    REQUIRE(*result == std::vector<MidiNote>{65, 69, 72});
}

TEST_CASE("FMMT001A: motif_transpose exceeding range returns error", "[form][core]") {
    std::vector<MidiNote> pitches = {120, 125, 127};
    auto result = motif_transpose(pitches, 10);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::InvalidMidiNote);
}

TEST_CASE("FMMT001A: motif_invert around first note", "[form][core]") {
    // C E G → C Ab F (intervals +4,+7 become -4,-7)
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto result = motif_invert(pitches);
    REQUIRE(result.has_value());
    REQUIRE((*result)[0] == 60);   // Axis
    REQUIRE((*result)[1] == 56);   // 60 - 4
    REQUIRE((*result)[2] == 53);   // 60 - 7
}

TEST_CASE("FMMT001A: motif_invert is self-inverse", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67, 72};
    auto inv = motif_invert(pitches).value();
    auto inv_inv = motif_invert(inv).value();
    REQUIRE(inv_inv == std::vector<MidiNote>(pitches.begin(), pitches.end()));
}

TEST_CASE("FMMT001A: motif_invert exceeding range returns error", "[form][core]") {
    // Axis at 10, note at 127: inverted = 10 - (127-10) = -107
    std::vector<MidiNote> pitches = {10, 127};
    auto result = motif_invert(pitches);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::InvalidMidiNote);
}

TEST_CASE("FMMT001A: motif_retrograde reverses", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto result = motif_retrograde(pitches);
    REQUIRE(result == std::vector<MidiNote>{67, 64, 60});
}

TEST_CASE("FMMT001A: motif_retrograde is self-inverse", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67, 72};
    auto retro = motif_retrograde(pitches);
    auto retro_retro = motif_retrograde(retro);
    REQUIRE(retro_retro == std::vector<MidiNote>(pitches.begin(), pitches.end()));
}

TEST_CASE("FMMT001A: motif_retrograde_inversion", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto ri = motif_retrograde_inversion(pitches);
    REQUIRE(ri.has_value());
    // Retrograde: {67, 64, 60}. Invert around 67: {67, 70, 74}
    REQUIRE((*ri)[0] == 67);
    REQUIRE((*ri)[1] == 70);
    REQUIRE((*ri)[2] == 74);
}

TEST_CASE("FMMT001A: motif_augment doubles durations", "[form][core]") {
    std::vector<Beat> durations = {Beat{1, 1}, Beat(1, 2), Beat{2, 1}};
    auto result = motif_augment(durations, Beat{2, 1});
    REQUIRE(result[0] == Beat{2, 1});
    REQUIRE(result[1] == Beat{1, 1});
    REQUIRE(result[2] == Beat{4, 1});
}

TEST_CASE("FMMT001A: motif_diminish halves durations", "[form][core]") {
    std::vector<Beat> durations = {Beat{2, 1}, Beat{1, 1}, Beat{4, 1}};
    auto result = motif_diminish(durations, Beat{2, 1});
    REQUIRE(result[0] == Beat{1, 1});
    REQUIRE(result[1] == Beat(1, 2));
    REQUIRE(result[2] == Beat{2, 1});
}

TEST_CASE("FMMT001A: augment then diminish is identity", "[form][core]") {
    std::vector<Beat> durations = {Beat{1, 1}, Beat(1, 2), Beat(3, 4)};
    Beat factor{3, 1};
    auto aug = motif_augment(durations, factor);
    auto result = motif_diminish(aug, factor);
    REQUIRE(result == durations);
}

TEST_CASE("FMMT001A: motif_interval_similarity identical", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    REQUIRE_THAT(motif_interval_similarity(a, a), WithinAbs(1.0, 1e-10));
}

TEST_CASE("FMMT001A: motif_interval_similarity transposition", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    auto b = motif_transpose(a, 5).value();
    // Same intervals, so similarity should be 1.0
    REQUIRE_THAT(motif_interval_similarity(a, b), WithinAbs(1.0, 1e-10));
}

TEST_CASE("FMMT001A: motif_interval_similarity different", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    std::vector<MidiNote> b = {60, 61, 62, 63};
    double sim = motif_interval_similarity(a, b);
    REQUIRE(sim < 0.5);
}

TEST_CASE("FMMT001A: classify_transformation repetition", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67};
    REQUIRE(classify_transformation(a, a) == MotivicTransform::Repetition);
}

TEST_CASE("FMMT001A: classify_transformation transposition", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67};
    std::vector<MidiNote> b = {65, 69, 72};  // Up P4
    REQUIRE(classify_transformation(a, b) == MotivicTransform::Transposition);
}

TEST_CASE("FMMT001A: classify_transformation inversion", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67};
    auto inv = motif_invert(a).value();
    REQUIRE(classify_transformation(a, inv) == MotivicTransform::Inversion);
}

TEST_CASE("FMMT001A: classify_transformation retrograde", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67};
    auto retro = motif_retrograde(a);
    REQUIRE(classify_transformation(a, retro) == MotivicTransform::Retrograde);
}

TEST_CASE("FMMT001A: classify_transformation retrograde_inversion", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    auto ri = motif_retrograde_inversion(a).value();
    REQUIRE(classify_transformation(a, ri) == MotivicTransform::RetrogradeInversion);
}

TEST_CASE("FMMT001A: classify_transformation fragmentation (prefix)", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    std::vector<MidiNote> frag = {60, 64};  // First two notes
    REQUIRE(classify_transformation(a, frag) == MotivicTransform::Fragmentation);
}

TEST_CASE("FMMT001A: classify_transformation fragmentation (suffix)", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67, 72};
    std::vector<MidiNote> frag = {67, 72};  // Last two notes
    REQUIRE(classify_transformation(a, frag) == MotivicTransform::Fragmentation);
}

TEST_CASE("FMMT001A: classify_transformation unknown", "[form][core]") {
    std::vector<MidiNote> a = {60, 64, 67};
    std::vector<MidiNote> b = {72, 61, 55};  // Random
    REQUIRE(classify_transformation(a, b) == MotivicTransform::Unknown);
}

TEST_CASE("FMMT001A: motif_fragment extracts subsequence", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67, 72, 76};
    auto frag = motif_fragment(pitches, 1, 3);
    REQUIRE(frag == std::vector<MidiNote>{64, 67, 72});
}

TEST_CASE("FMMT001A: motif_fragment out of bounds returns empty", "[form][core]") {
    std::vector<MidiNote> pitches = {60, 64, 67};
    auto frag = motif_fragment(pitches, 5, 2);
    REQUIRE(frag.empty());
}

TEST_CASE("FMMT001A: transform_name returns correct strings", "[form][core]") {
    REQUIRE(transform_name(MotivicTransform::Repetition) == "Repetition");
    REQUIRE(transform_name(MotivicTransform::Transposition) == "Transposition");
    REQUIRE(transform_name(MotivicTransform::Retrograde) == "Retrograde");
    REQUIRE(transform_name(MotivicTransform::RetrogradeInversion) == "Retrograde-Inversion");
}
