/**
 * @file TSFB001A.cpp
 * @brief Unit tests for VLFB001A (Figured Bass Realisation)
 *
 * Component: TSFB001A
 * Tests: VLFB001A
 * Validates: Formal Spec §7.7
 */

#include <catch2/catch_test_macros.hpp>

#include "VoiceLeading/VLFB001A.h"
#include "VoiceLeading/VLNT001A.h"
#include "Scale/SCDF001A.h"
#include "Pitch/PTPC001A.h"

using namespace Sunny::Core;

// =============================================================================
// figured_bass_intervals shorthand lookup
// =============================================================================

TEST_CASE("VLFB001A: root position intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("");
    REQUIRE(ivs.size() == 2);
    REQUIRE(ivs[0] == 3);
    REQUIRE(ivs[1] == 5);

    REQUIRE(figured_bass_intervals("5/3") == ivs);
    REQUIRE(figured_bass_intervals("53") == ivs);
}

TEST_CASE("VLFB001A: first inversion intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("6");
    REQUIRE(ivs == std::vector<int>{3, 6});
    REQUIRE(figured_bass_intervals("6/3") == ivs);
}

TEST_CASE("VLFB001A: second inversion intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("6/4");
    REQUIRE(ivs == std::vector<int>{4, 6});
}

TEST_CASE("VLFB001A: seventh chord intervals", "[figured-bass][core]") {
    REQUIRE(figured_bass_intervals("7") == std::vector<int>{3, 5, 7});
    REQUIRE(figured_bass_intervals("6/5") == std::vector<int>{3, 5, 6});
    REQUIRE(figured_bass_intervals("4/3") == std::vector<int>{3, 4, 6});
    REQUIRE(figured_bass_intervals("4/2") == std::vector<int>{2, 4, 6});
    REQUIRE(figured_bass_intervals("2") == std::vector<int>{2, 4, 6});
}

TEST_CASE("VLFB001A: unknown shorthand returns empty", "[figured-bass][core]") {
    REQUIRE(figured_bass_intervals("xyz").empty());
}

// =============================================================================
// parse_figured_bass
// =============================================================================

TEST_CASE("VLFB001A: parse standard shorthands", "[figured-bass][core]") {
    auto result = parse_figured_bass("7");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 3);
    REQUIRE(result->figures[0].interval == 3);
    REQUIRE(result->figures[1].interval == 5);
    REQUIRE(result->figures[2].interval == 7);
}

TEST_CASE("VLFB001A: parse with accidentals", "[figured-bass][core]") {
    auto result = parse_figured_bass("#6");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 1);
    REQUIRE(result->figures[0].interval == 6);
    REQUIRE(result->figures[0].accidental == FigureAccidental::Sharp);
}

TEST_CASE("VLFB001A: parse compound figures", "[figured-bass][core]") {
    auto result = parse_figured_bass("#6/b3");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 2);
    REQUIRE(result->figures[0].interval == 6);
    REQUIRE(result->figures[0].accidental == FigureAccidental::Sharp);
    REQUIRE(result->figures[1].interval == 3);
    REQUIRE(result->figures[1].accidental == FigureAccidental::Flat);
}

TEST_CASE("VLFB001A: parse empty defaults to root position", "[figured-bass][core]") {
    auto result = parse_figured_bass("");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 2);
}

// =============================================================================
// realise_figured_bass
// =============================================================================

TEST_CASE("VLFB001A: root position C in C major", "[figured-bass][core]") {
    // Bass = C3 (48), figures = 5/3 → E, G above
    auto symbol = parse_figured_bass("5/3");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(
        48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->bass == 48);
    REQUIRE(result->upper.size() == 2);

    // Upper voices should be E and G (pitch classes 4 and 7)
    REQUIRE(pitch_class(result->upper[0]) == 4);  // E
    REQUIRE(pitch_class(result->upper[1]) == 7);  // G

    // All above bass
    for (auto note : result->upper) {
        REQUIRE(note > 48);
    }
}

TEST_CASE("VLFB001A: first inversion C in C major", "[figured-bass][core]") {
    // Bass = E3 (52), figures = 6 → G, C above (3rd + 6th above E)
    auto symbol = parse_figured_bass("6");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(
        52, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());

    // 3rd above E diatonically in C major = G (pc 7)
    // 6th above E diatonically in C major = C (pc 0)
    // Sorted ascending: C4 (60) < G4 (67)
    REQUIRE(pitch_class(result->upper[0]) == 0);  // C
    REQUIRE(pitch_class(result->upper[1]) == 7);  // G
}

TEST_CASE("VLFB001A: seventh chord realisation", "[figured-bass][core]") {
    // Bass = G3 (55), figures = 7 → B, D, F above
    auto symbol = parse_figured_bass("7");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(
        55, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->upper.size() == 3);

    // 3rd above G = B (11), 5th above G = D (2), 7th above G = F (5)
    std::vector<PitchClass> upper_pcs;
    for (auto n : result->upper) {
        upper_pcs.push_back(pitch_class(n));
    }
    // Check pitch classes present (order may vary due to sorting)
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 11) == 1);  // B
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 2) == 1);   // D
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 5) == 1);   // F
}

TEST_CASE("VLFB001A: all_notes includes bass", "[figured-bass][core]") {
    auto symbol = parse_figured_bass("");
    auto result = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->all_notes.front() == 48);
    REQUIRE(result->all_notes.size() == result->upper.size() + 1);
}

TEST_CASE("VLFB001A: all_notes sorted ascending", "[figured-bass][core]") {
    auto symbol = parse_figured_bass("7");
    auto result = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    for (std::size_t i = 1; i < result->all_notes.size(); ++i) {
        REQUIRE(result->all_notes[i] >= result->all_notes[i - 1]);
    }
}

TEST_CASE("VLFB001A: accidental sharp raises pitch class", "[figured-bass][core]") {
    // Bass = C3 (48), figure = #3 → should be E# = F (pc 5) instead of E (pc 4)
    FiguredBassSymbol symbol;
    symbol.figures.push_back({3, FigureAccidental::Sharp});

    auto result = realise_figured_bass(48, symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(pitch_class(result->upper[0]) == 5);  // F (E raised by semitone)
}

TEST_CASE("VLFB001A: accidental flat lowers pitch class", "[figured-bass][core]") {
    // Bass = C3 (48), figure = b3 → should be Eb (pc 3) instead of E (pc 4)
    FiguredBassSymbol symbol;
    symbol.figures.push_back({3, FigureAccidental::Flat});

    auto result = realise_figured_bass(48, symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(pitch_class(result->upper[0]) == 3);  // Eb
}

TEST_CASE("VLFB001A: invalid bass note rejected", "[figured-bass][core]") {
    FiguredBassSymbol symbol;
    symbol.figures.push_back({3, FigureAccidental::Natural});
    auto result = realise_figured_bass(200, symbol, 0, SCALE_MAJOR);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("VLFB001A: second inversion 6/4", "[figured-bass][core]") {
    // Bass = G3 (55), figures = 6/4 → C, E above (4th + 6th above G)
    auto symbol = parse_figured_bass("6/4");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(55, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());

    // 4th above G diatonically = C (pc 0)
    // 6th above G diatonically = E (pc 4)
    std::vector<PitchClass> upper_pcs;
    for (auto n : result->upper) {
        upper_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 0) == 1);  // C
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 4) == 1);  // E
}

// =============================================================================
// realise_figured_bass_sequence
// =============================================================================

TEST_CASE("VLFB001A: single-event sequence matches direct realisation", "[figured-bass][sequence][core]") {
    auto symbol = parse_figured_bass("5/3");
    REQUIRE(symbol.has_value());

    FiguredBassEvent event{48, *symbol};  // C3, root position

    auto seq = realise_figured_bass_sequence(
        std::span<const FiguredBassEvent>(&event, 1), 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 1);

    auto direct = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(direct.has_value());

    // Should produce same bass and pitch classes
    REQUIRE(seq->realisations[0].bass == direct->bass);
    REQUIRE(seq->realisations[0].upper.size() == direct->upper.size());
    for (std::size_t i = 0; i < direct->upper.size(); ++i) {
        REQUIRE(pitch_class(seq->realisations[0].upper[i]) ==
                pitch_class(direct->upper[i]));
    }
}

TEST_CASE("VLFB001A: two-chord I-V sequence in C major", "[figured-bass][sequence][core]") {
    // I: bass C3 (48), root position → E, G
    // V: bass G2 (43), root position → B, D
    auto sym_root = parse_figured_bass("5/3");
    REQUIRE(sym_root.has_value());

    std::vector<FiguredBassEvent> events = {
        {48, *sym_root},  // I
        {43, *sym_root},  // V
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 2);

    // First chord: bass C3, upper voices E and G
    REQUIRE(seq->realisations[0].bass == 48);

    // Second chord: bass G2, upper voices should contain B and D pitch classes
    REQUIRE(seq->realisations[1].bass == 43);
    std::vector<PitchClass> v_pcs;
    for (auto n : seq->realisations[1].upper) {
        v_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(v_pcs.begin(), v_pcs.end(), 11) == 1);  // B
    REQUIRE(std::count(v_pcs.begin(), v_pcs.end(), 2) == 1);   // D
}

TEST_CASE("VLFB001A: four-chord I-IV-V-I sequence", "[figured-bass][sequence][core]") {
    auto sym = parse_figured_bass("5/3");
    REQUIRE(sym.has_value());

    // I(C3) → IV(F2) → V(G2) → I(C3)
    std::vector<FiguredBassEvent> events = {
        {48, *sym},  // I:  C3
        {41, *sym},  // IV: F2
        {43, *sym},  // V:  G2
        {48, *sym},  // I:  C3
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 4);

    // Verify bass notes
    REQUIRE(seq->realisations[0].bass == 48);
    REQUIRE(seq->realisations[1].bass == 41);
    REQUIRE(seq->realisations[2].bass == 43);
    REQUIRE(seq->realisations[3].bass == 48);

    // Verify each realisation has correct number of upper voices
    for (auto& r : seq->realisations) {
        REQUIRE(r.upper.size() == 2);
        REQUIRE(r.all_notes.size() == 3);
        REQUIRE(r.all_notes.front() == r.bass);
    }

    // Verify pitch classes of final I chord
    std::vector<PitchClass> final_pcs;
    for (auto n : seq->realisations[3].upper) {
        final_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(final_pcs.begin(), final_pcs.end(), 4) == 1);  // E
    REQUIRE(std::count(final_pcs.begin(), final_pcs.end(), 7) == 1);  // G
}

TEST_CASE("VLFB001A: sequence with inversions I-V65-I6", "[figured-bass][sequence][core]") {
    auto sym_root = parse_figured_bass("5/3");
    auto sym_65 = parse_figured_bass("6/5");
    auto sym_6 = parse_figured_bass("6");
    REQUIRE(sym_root.has_value());
    REQUIRE(sym_65.has_value());
    REQUIRE(sym_6.has_value());

    // I(C3) → V6/5(B2) → I6(E3)
    std::vector<FiguredBassEvent> events = {
        {48, *sym_root},  // I:    C3, 5/3
        {47, *sym_65},    // V6/5: B2, 6/5
        {52, *sym_6},     // I6:   E3, 6
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 3);

    // V6/5 on B: 3rd=D, 5th=F, 6th=G → pitch classes 2, 5, 7
    std::vector<PitchClass> v65_pcs;
    for (auto n : seq->realisations[1].upper) {
        v65_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 2) == 1);   // D
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 5) == 1);   // F
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 7) == 1);   // G

    // I6 on E: 3rd=G, 6th=C → pitch classes 7, 0
    std::vector<PitchClass> i6_pcs;
    for (auto n : seq->realisations[2].upper) {
        i6_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(i6_pcs.begin(), i6_pcs.end(), 7) == 1);   // G
    REQUIRE(std::count(i6_pcs.begin(), i6_pcs.end(), 0) == 1);   // C
}
