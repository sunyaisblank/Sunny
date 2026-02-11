/**
 * @file TSGI001A.cpp
 * @brief Unit tests for GIST001A (Generalised Interval System)
 *
 * Component: TSGI001A
 * Tests: GIST001A
 * Validates: Formal Spec §11.4
 *
 * GIS Axioms verified for each concrete instance:
 *   (A1) interval(s, s) == identity
 *   (A2) compose(interval(r,s), interval(s,t)) == interval(r,t)
 *   (A3) transpose(s, interval(s,t)) == t
 *   (A4) interval(s, transpose(s, i)) == i
 */

#include <catch2/catch_test_macros.hpp>

#include "Transform/GIST001A.h"

using namespace Sunny::Core;

// Pitch class constants (PitchClass = uint8_t)
constexpr PitchClass PC_C  = 0;
constexpr PitchClass PC_Db = 1;
constexpr PitchClass PC_D  = 2;
constexpr PitchClass PC_Eb = 3;
constexpr PitchClass PC_E  = 4;
constexpr PitchClass PC_F  = 5;
constexpr PitchClass PC_Gb = 6;
constexpr PitchClass PC_G  = 7;
constexpr PitchClass PC_Ab = 8;
constexpr PitchClass PC_A  = 9;
constexpr PitchClass PC_Bb = 10;
constexpr PitchClass PC_B  = 11;

// =============================================================================
// PitchClassGIS: Axioms
// =============================================================================

TEST_CASE("GIST001A: PitchClassGIS interval(s,s) == identity", "[gis][core]") {
    for (int i = 0; i < 12; ++i) {
        auto pc = static_cast<PitchClass>(i);
        REQUIRE(PitchClassGIS::interval(pc, pc) == PitchClassGIS::identity());
    }
}

TEST_CASE("GIST001A: PitchClassGIS transitivity", "[gis][core]") {
    PitchClass r = PC_C, s = PC_E, t = PC_Ab;
    auto rs = PitchClassGIS::interval(r, s);
    auto st = PitchClassGIS::interval(s, t);
    auto rt = PitchClassGIS::interval(r, t);
    REQUIRE(PitchClassGIS::compose(rs, st) == rt);
}

TEST_CASE("GIST001A: PitchClassGIS transpose roundtrip", "[gis][core]") {
    PitchClass s = PC_D, t = PC_Bb;
    auto i = PitchClassGIS::interval(s, t);
    REQUIRE(PitchClassGIS::transpose(s, i) == t);
}

TEST_CASE("GIST001A: PitchClassGIS interval-transpose inverse", "[gis][core]") {
    PitchClass s = PC_F;
    int i = 5;  // perfect fourth
    REQUIRE(PitchClassGIS::interval(s, PitchClassGIS::transpose(s, i)) == i);
}

TEST_CASE("GIST001A: PitchClassGIS inverse cancels", "[gis][core]") {
    int i = 7;  // perfect fifth
    REQUIRE(PitchClassGIS::compose(i, PitchClassGIS::inverse(i))
            == PitchClassGIS::identity());
}

TEST_CASE("GIST001A: PitchClassGIS interval values", "[gis][core]") {
    // C to E = 4 semitones (major third)
    REQUIRE(PitchClassGIS::interval(PC_C, PC_E) == 4);
    // E to C = 8 semitones (ascending)
    REQUIRE(PitchClassGIS::interval(PC_E, PC_C) == 8);
    // G to D = 7 semitones (perfect fifth)
    REQUIRE(PitchClassGIS::interval(PC_G, PC_D) == 7);
}

// =============================================================================
// PitchGIS: Axioms
// =============================================================================

TEST_CASE("GIST001A: PitchGIS interval(s,s) == identity", "[gis][core]") {
    REQUIRE(PitchGIS::interval(60, 60) == PitchGIS::identity());
    REQUIRE(PitchGIS::interval(0, 0) == PitchGIS::identity());
    REQUIRE(PitchGIS::interval(127, 127) == PitchGIS::identity());
}

TEST_CASE("GIST001A: PitchGIS transitivity", "[gis][core]") {
    int r = 48, s = 60, t = 67;  // C3, C4, G4
    auto rs = PitchGIS::interval(r, s);
    auto st = PitchGIS::interval(s, t);
    auto rt = PitchGIS::interval(r, t);
    REQUIRE(PitchGIS::compose(rs, st) == rt);
}

TEST_CASE("GIST001A: PitchGIS transpose roundtrip", "[gis][core]") {
    int s = 60, t = 72;  // C4, C5
    REQUIRE(PitchGIS::transpose(s, PitchGIS::interval(s, t)) == t);
}

TEST_CASE("GIST001A: PitchGIS interval-transpose inverse", "[gis][core]") {
    int s = 60, i = -5;
    REQUIRE(PitchGIS::interval(s, PitchGIS::transpose(s, i)) == i);
}

TEST_CASE("GIST001A: PitchGIS interval is signed", "[gis][core]") {
    // Ascending: C4→G4 = +7
    REQUIRE(PitchGIS::interval(60, 67) == 7);
    // Descending: G4→C4 = -7
    REQUIRE(PitchGIS::interval(67, 60) == -7);
}

// =============================================================================
// TimePointGIS: Axioms
// =============================================================================

TEST_CASE("GIST001A: TimePointGIS interval(s,s) == identity", "[gis][core]") {
    Beat b(3, 4);
    REQUIRE(TimePointGIS::interval(b, b) == TimePointGIS::identity());
}

TEST_CASE("GIST001A: TimePointGIS transitivity", "[gis][core]") {
    Beat r(0, 1);      // beat 0
    Beat s(1, 2);      // beat 1/2
    Beat t(3, 4);      // beat 3/4
    auto rs = TimePointGIS::interval(r, s);
    auto st = TimePointGIS::interval(s, t);
    auto rt = TimePointGIS::interval(r, t);
    REQUIRE(TimePointGIS::compose(rs, st) == rt);
}

TEST_CASE("GIST001A: TimePointGIS transpose roundtrip", "[gis][core]") {
    Beat s(1, 4);
    Beat t(7, 8);
    REQUIRE(TimePointGIS::transpose(s, TimePointGIS::interval(s, t)) == t);
}

TEST_CASE("GIST001A: TimePointGIS interval-transpose inverse", "[gis][core]") {
    Beat s(0, 1);
    Beat i(5, 16);
    REQUIRE(TimePointGIS::interval(s, TimePointGIS::transpose(s, i)) == i);
}

TEST_CASE("GIST001A: TimePointGIS inverse cancels", "[gis][core]") {
    Beat i(3, 8);
    REQUIRE(TimePointGIS::compose(i, TimePointGIS::inverse(i))
            == TimePointGIS::identity());
}

// =============================================================================
// Generic GIS operations
// =============================================================================

TEST_CASE("GIST001A: interval_sequence pitch classes", "[gis][core]") {
    // C - E - G - C (major arpeggio)
    std::vector<PitchClass> elems = {PC_C, PC_E, PC_G, PC_C};
    auto ivs = interval_sequence<PitchClassGIS>(elems);
    REQUIRE(ivs.size() == 3);
    REQUIRE(ivs[0] == 4);   // C→E = 4
    REQUIRE(ivs[1] == 3);   // E→G = 3
    REQUIRE(ivs[2] == 5);   // G→C = 5
}

TEST_CASE("GIST001A: interval_sequence empty and single", "[gis][core]") {
    std::vector<PitchClass> empty;
    REQUIRE(interval_sequence<PitchClassGIS>(empty).empty());

    std::vector<PitchClass> single = {PC_C};
    REQUIRE(interval_sequence<PitchClassGIS>(single).empty());
}

TEST_CASE("GIST001A: elements_from_intervals reconstructs", "[gis][core]") {
    // Start at C, intervals [4, 3, 5] → C, E, G, C
    std::vector<int> ivs = {4, 3, 5};
    auto elems = elements_from_intervals<PitchClassGIS>(PC_C, ivs);
    REQUIRE(elems.size() == 4);
    REQUIRE(elems[0] == PC_C);
    REQUIRE(elems[1] == PC_E);
    REQUIRE(elems[2] == PC_G);
    REQUIRE(elems[3] == PC_C);
}

TEST_CASE("GIST001A: interval_sequence and elements_from_intervals roundtrip", "[gis][core]") {
    // Chromatic fragment in pitch space
    std::vector<int> pitches = {60, 63, 67, 72};
    auto ivs = interval_sequence<PitchGIS>(pitches);
    auto reconstructed = elements_from_intervals<PitchGIS>(60, ivs);
    REQUIRE(reconstructed == pitches);
}

TEST_CASE("GIST001A: TimePointGIS interval_sequence", "[gis][core]") {
    std::vector<Beat> beats = {Beat(0, 1), Beat(1, 4), Beat(1, 2), Beat(1, 1)};
    auto ivs = interval_sequence<TimePointGIS>(beats);
    REQUIRE(ivs.size() == 3);
    REQUIRE(ivs[0] == Beat(1, 4));
    REQUIRE(ivs[1] == Beat(1, 4));
    REQUIRE(ivs[2] == Beat(1, 2));
}

// =============================================================================
// GIS axiom exhaustive verification (PitchClassGIS over all Z/12Z)
// =============================================================================

TEST_CASE("GIST001A: PitchClassGIS all pairs satisfy axioms", "[gis][core]") {
    for (int si = 0; si < 12; ++si) {
        for (int ti = 0; ti < 12; ++ti) {
            auto s = static_cast<PitchClass>(si);
            auto t = static_cast<PitchClass>(ti);
            auto iv = PitchClassGIS::interval(s, t);

            // A3: transpose(s, interval(s,t)) == t
            REQUIRE(PitchClassGIS::transpose(s, iv) == t);

            // A4: interval(s, transpose(s, iv)) == iv
            REQUIRE(PitchClassGIS::interval(s, PitchClassGIS::transpose(s, iv)) == iv);
        }
    }
}
