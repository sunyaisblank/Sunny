/**
 * @file TSCD001A.cpp
 * @brief Unit tests for HRCD001A (Cadence detection)
 *
 * Component: TSCD001A
 * Tests: HRCD001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Harmony/HRCD001A.h"
#include "Harmony/HRRN001A.h"
#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

namespace {

// Helper to make a chord voicing in C major
ChordVoicing make_chord(PitchClass root, const std::string& quality, int octave = 4) {
    auto result = generate_chord(root, quality, octave);
    return result.has_value() ? *result : ChordVoicing{};
}

}  // namespace

TEST_CASE("HRCD001A: Perfect Authentic Cadence (PAC)", "[harmony][core]") {
    SECTION("V→I root position, soprano on tonic") {
        auto V = make_chord(7, "major", 4);   // G major: 67, 71, 74
        auto I = make_chord(0, "major", 4);    // C major: 60, 64, 67
        // Rearrange I so soprano is C (tonic)
        I.notes = {60, 64, 72};  // C4, E4, C5 — soprano on C
        I.root = 0;

        auto cad = detect_cadence(V, I, 0, false);
        REQUIRE(cad.type == CadenceType::PAC);
        REQUIRE(cad.penultimate_degree == 4);
        REQUIRE(cad.final_degree == 0);
        REQUIRE(cad.is_root_position == true);
        REQUIRE(cad.soprano_on_tonic == true);
    }
}

TEST_CASE("HRCD001A: Imperfect Authentic Cadence (IAC)", "[harmony][core]") {
    SECTION("V→I, soprano not on tonic") {
        auto V = make_chord(7, "major", 4);
        auto I = make_chord(0, "major", 4);  // C, E, G — soprano is G
        // Soprano = G(7), not tonic C(0)

        auto cad = detect_cadence(V, I, 0, false);
        REQUIRE(cad.type == CadenceType::IAC);
    }

    SECTION("V→I, first inversion") {
        auto V = make_chord(7, "major", 4);
        ChordVoicing I;
        I.root = 0;
        I.quality = "major";
        I.notes = {64, 67, 72};  // E4, G4, C5 — bass is E, first inversion
        I.inversion = 1;

        auto cad = detect_cadence(V, I, 0, false);
        // Bass is E(4), not C(0), so not root position
        REQUIRE(cad.type == CadenceType::IAC);
        REQUIRE(cad.is_root_position == false);
    }
}

TEST_CASE("HRCD001A: Half Cadence", "[harmony][core]") {
    SECTION("I→V is a half cadence") {
        auto I = make_chord(0, "major", 4);
        auto V = make_chord(7, "major", 4);

        auto cad = detect_cadence(I, V, 0, false);
        REQUIRE(cad.type == CadenceType::Half);
        REQUIRE(cad.final_degree == 4);
    }

    SECTION("ii→V is a half cadence") {
        auto ii = make_chord(2, "minor", 4);
        auto V = make_chord(7, "major", 4);

        auto cad = detect_cadence(ii, V, 0, false);
        REQUIRE(cad.type == CadenceType::Half);
    }

    SECTION("IV→V is a half cadence") {
        auto IV = make_chord(5, "major", 4);
        auto V = make_chord(7, "major", 4);

        auto cad = detect_cadence(IV, V, 0, false);
        REQUIRE(cad.type == CadenceType::Half);
    }
}

TEST_CASE("HRCD001A: Plagal Cadence", "[harmony][core]") {
    SECTION("IV→I is plagal") {
        auto IV = make_chord(5, "major", 4);
        auto I = make_chord(0, "major", 4);

        auto cad = detect_cadence(IV, I, 0, false);
        REQUIRE(cad.type == CadenceType::Plagal);
    }
}

TEST_CASE("HRCD001A: Deceptive Cadence", "[harmony][core]") {
    SECTION("V→vi is deceptive") {
        auto V = make_chord(7, "major", 4);
        auto vi = make_chord(9, "minor", 4);

        auto cad = detect_cadence(V, vi, 0, false);
        REQUIRE(cad.type == CadenceType::Deceptive);
        REQUIRE(cad.penultimate_degree == 4);
        REQUIRE(cad.final_degree == 5);
    }
}

TEST_CASE("HRCD001A: Phrygian Half Cadence", "[harmony][core]") {
    SECTION("iv6→V in minor") {
        ChordVoicing iv6;
        iv6.root = 5;  // F in C minor
        iv6.quality = "minor";
        iv6.inversion = 1;
        iv6.notes = {68, 72, 77};  // Ab4, C5, F5 — first inversion

        auto V = make_chord(7, "major", 4);

        auto cad = detect_cadence(iv6, V, 0, true);
        REQUIRE(cad.type == CadenceType::PhrygianHalf);
    }
}

TEST_CASE("HRCD001A: Non-cadence pairs", "[harmony][core]") {
    SECTION("I→IV is not a cadence") {
        auto I = make_chord(0, "major", 4);
        auto IV = make_chord(5, "major", 4);

        auto cad = detect_cadence(I, IV, 0, false);
        REQUIRE(cad.type == CadenceType::None);
    }

    SECTION("ii→IV is not a cadence") {
        auto ii = make_chord(2, "minor", 4);
        auto IV = make_chord(5, "major", 4);

        auto cad = detect_cadence(ii, IV, 0, false);
        REQUIRE(cad.type == CadenceType::None);
    }

    SECTION("Empty chords return None") {
        ChordVoicing empty;
        auto I = make_chord(0, "major", 4);
        auto cad = detect_cadence(empty, I, 0, false);
        REQUIRE(cad.type == CadenceType::None);
    }
}

TEST_CASE("HRCD001A: detect_cadences_in_progression", "[harmony][core]") {
    SECTION("I-IV-V-I progression has half and PAC") {
        auto I = make_chord(0, "major", 4);
        I.notes = {60, 64, 72};  // Soprano on C for PAC
        auto IV = make_chord(5, "major", 4);
        auto V = make_chord(7, "major", 4);
        auto I_end = I;

        std::vector<ChordVoicing> prog = {I, IV, V, I_end};
        auto cadences = detect_cadences_in_progression(prog, 0, false);

        // Should find at least: IV→V (half) at index 1, V→I (PAC) at index 2
        REQUIRE(cadences.size() >= 2);

        bool found_half = false;
        bool found_pac = false;
        for (const auto& [idx, cad] : cadences) {
            if (cad.type == CadenceType::Half && idx == 1) found_half = true;
            if (cad.type == CadenceType::PAC && idx == 2) found_pac = true;
        }
        REQUIRE(found_half);
        REQUIRE(found_pac);
    }

    SECTION("Single chord returns no cadences") {
        auto I = make_chord(0, "major", 4);
        std::vector<ChordVoicing> prog = {I};
        auto cadences = detect_cadences_in_progression(prog, 0, false);
        REQUIRE(cadences.empty());
    }

    SECTION("Empty progression returns no cadences") {
        std::vector<ChordVoicing> prog;
        auto cadences = detect_cadences_in_progression(prog, 0, false);
        REQUIRE(cadences.empty());
    }
}

TEST_CASE("HRCD001A: Cadences in G major", "[harmony][core]") {
    SECTION("V→I in G major = D→G") {
        auto D = make_chord(2, "major", 4);   // D major
        ChordVoicing G;
        G.root = 7;
        G.quality = "major";
        G.notes = {55, 59, 67};  // G3, B3, G4 — soprano on G (tonic)

        auto cad = detect_cadence(D, G, 7, false);
        REQUIRE(cad.type == CadenceType::PAC);
    }
}

TEST_CASE("HRCD001A: cadence in non-C key kills modular mutant", "[harmony][core]") {
    SECTION("G major V-I: penultimate_degree is 4") {
        // V in G major is D major (root PC 2).
        // find_scale_degree computes interval = (2 - 7 + 12) % 12 = 7,
        // which matches MAJOR_SCALE[4].  Under % → / mutation the expression
        // evaluates to (2 - 7 + 12) / 12 = 0, yielding degree 0 instead of 4.
        auto V = make_chord(2, "major", 4);   // D major
        ChordVoicing I;
        I.root = 7;
        I.quality = "major";
        I.notes = {55, 59, 67};  // G3, B3, G4 — soprano on tonic G

        auto cad = detect_cadence(V, I, 7, false);
        REQUIRE(cad.type == CadenceType::PAC);
        REQUIRE(cad.penultimate_degree == 4);
    }

    SECTION("PAC root position with bass at MIDI 60") {
        // Root position check: bass() % 12 == root.
        // With bass = 60 (C4) and root = 0: 60 % 12 = 0 == 0 → root position.
        // Under % → / mutation: 60 / 12 = 5 ≠ 0 → falsely non-root-position,
        // downgrading PAC to IAC.
        auto V = make_chord(7, "major", 4);   // G major
        ChordVoicing I;
        I.root = 0;
        I.quality = "major";
        I.notes = {60, 64, 72};  // C4, E4, C5 — bass on root C, soprano on tonic C

        auto cad = detect_cadence(V, I, 0, false);
        REQUIRE(cad.type == CadenceType::PAC);
        REQUIRE(cad.is_root_position == true);
    }
}

TEST_CASE("HRCD001A: chromatic chord root cadence in C major", "[harmony][core]") {
    // bII (Neapolitan) resolving to I in C major.
    // bII root = Db (PC 1), quality "major". I root = C (PC 0), quality "major".
    // find_scale_degree for PC 1 in C major: interval = (1 - 0 + 12) % 12 = 1.
    // No diatonic degree matches 1 exactly, so the chromatic fallback loop
    // runs. sharp_deg = 0 (scale[0]=0 == interval-1), flat_deg = 1 (scale[1]=2
    // == interval+1). PREFER_SHARP[0] is true, so the function returns degree 0
    // (interpreting Db as raised tonic rather than lowered supertonic).
    ChordVoicing bII;
    bII.root = 1;  // Db
    bII.quality = "major";
    bII.notes = {61, 65, 68};  // Db4, F4, Ab4

    ChordVoicing I;
    I.root = 0;  // C
    I.quality = "major";
    I.notes = {60, 64, 72};  // C4, E4, C5 — soprano on tonic

    auto cad = detect_cadence(bII, I, 0, false);
    // The chromatic fallback should resolve without crashing.
    // penultimate_degree is 0 (sharp interpretation of the chromatic root).
    CHECK(cad.penultimate_degree == 0);
    // final chord resolves to degree 0 (I) via diatonic match.
    CHECK(cad.final_degree == 0);
}
