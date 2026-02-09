/**
 * @file TSTW001A.cpp
 * @brief Unit tests for SRTW001A (Twelve-tone serialism)
 *
 * Component: TSTW001A
 * Tests: SRTW001A
 */

#include <catch2/catch_test_macros.hpp>

#include "PostTonal/SRTW001A.h"
#include "Pitch/PTPC001A.h"
#include "Pitch/PTPS001A.h"

using namespace Sunny::Core;

// =============================================================================
// Row Construction
// =============================================================================

TEST_CASE("SRTW001A: make_tone_row valid", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());
    REQUIRE(row->elements == pcs);
}

TEST_CASE("SRTW001A: make_tone_row rejects duplicate", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10};
    auto row = make_tone_row(pcs);
    REQUIRE_FALSE(row.has_value());
    REQUIRE(row.error() == ErrorCode::InvalidToneRow);
}

TEST_CASE("SRTW001A: make_tone_row rejects wrong size", "[serialism][core]") {
    std::array<PitchClass, 6> pcs = {0, 1, 2, 3, 4, 5};
    auto row = make_tone_row(pcs);
    REQUIRE_FALSE(row.has_value());
}

// =============================================================================
// Row Forms
// =============================================================================

TEST_CASE("SRTW001A: row_prime P_0 is identity", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto p0 = row_prime(*row, 0);
    REQUIRE(p0 == *row);
}

TEST_CASE("SRTW001A: row_prime P_n transposes all elements", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto p3 = row_prime(*row, 3);
    for (int i = 0; i < 12; ++i) {
        REQUIRE(p3.elements[i] == (pcs[i] + 3) % 12);
    }
}

TEST_CASE("SRTW001A: row_inversion I_0", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto i0 = row_inversion(*row, 0);
    // I_0(x) = (0 - x) mod 12 = (12 - x) mod 12
    REQUIRE(i0.elements[0] == 0);
    REQUIRE(i0.elements[1] == 11);
    REQUIRE(i0.elements[2] == 10);
    REQUIRE(i0.elements[6] == 6);
}

TEST_CASE("SRTW001A: row_retrograde reverses P_n", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto r0 = row_retrograde(*row, 0);
    REQUIRE(r0.elements[0] == 10);
    REQUIRE(r0.elements[11] == 0);
}

TEST_CASE("SRTW001A: row_retrograde_inversion", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto ri0 = row_retrograde_inversion(*row, 0);
    auto i0 = row_inversion(*row, 0);
    // RI_0 should be I_0 reversed
    REQUIRE(ri0.elements[0] == i0.elements[11]);
    REQUIRE(ri0.elements[11] == i0.elements[0]);
}

TEST_CASE("SRTW001A: row_form dispatch", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    REQUIRE(row_form(*row, RowFormType::Prime, 0) == row_prime(*row, 0));
    REQUIRE(row_form(*row, RowFormType::Inversion, 3) == row_inversion(*row, 3));
    REQUIRE(row_form(*row, RowFormType::Retrograde, 5) == row_retrograde(*row, 5));
    REQUIRE(row_form(*row, RowFormType::RetrogradeInversion, 7) == row_retrograde_inversion(*row, 7));
}

TEST_CASE("SRTW001A: bijectivity invariant", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    // Every row form is a permutation of {0..11}
    auto check_permutation = [](const ToneRow& r) {
        std::array<bool, 12> seen{};
        for (auto pc : r.elements) {
            REQUIRE(pc < 12);
            seen[pc] = true;
        }
        for (bool s : seen) {
            REQUIRE(s);
        }
    };

    check_permutation(row_prime(*row, 5));
    check_permutation(row_inversion(*row, 3));
    check_permutation(row_retrograde(*row, 7));
    check_permutation(row_retrograde_inversion(*row, 11));
}

// =============================================================================
// Matrix
// =============================================================================

TEST_CASE("SRTW001A: generate_matrix rows are prime forms", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto matrix = generate_matrix(*row);

    // Row 0 should be P_0
    auto r0 = read_matrix_row(matrix, 0);
    REQUIRE(r0 == *row);

    // Each row is a valid permutation
    for (int i = 0; i < 12; ++i) {
        auto ri = read_matrix_row(matrix, i);
        std::array<bool, 12> seen{};
        for (auto pc : ri.elements) seen[pc] = true;
        for (bool s : seen) REQUIRE(s);
    }
}

TEST_CASE("SRTW001A: generate_matrix columns are inversion forms", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto matrix = generate_matrix(*row);

    // Column 0 should be I_0
    auto c0 = read_matrix_column(matrix, 0);
    auto i0 = row_inversion(*row, 0);
    REQUIRE(c0 == i0);
}

// =============================================================================
// Hexachord
// =============================================================================

TEST_CASE("SRTW001A: hexachord extraction", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto h1 = hexachord(*row, true);
    auto h2 = hexachord(*row, false);
    REQUIRE(h1.size() == 6);
    REQUIRE(h2.size() == 6);

    // Union should be all 12
    PitchClassSet all;
    all.insert(h1.begin(), h1.end());
    all.insert(h2.begin(), h2.end());
    REQUIRE(all.size() == 12);
}

TEST_CASE("SRTW001A: hexachord_complement union is aggregate", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto h1 = hexachord(*row, true);
    auto comp = hexachord_complement(h1);
    REQUIRE(comp.size() == 6);

    // h1 ∩ comp should be empty
    for (auto pc : h1) {
        REQUIRE(comp.find(pc) == comp.end());
    }
}

// =============================================================================
// Combinatoriality
// =============================================================================

TEST_CASE("SRTW001A: chromatic row is all-combinatorial", "[serialism][core]") {
    // {0,1,2,3,4,5,6,7,8,9,10,11} — first hex {0,1,2,3,4,5}, complement {6,7,8,9,10,11}
    // T_6({0..5}) = {6..11} = complement → P-comb at level 6
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto comb = analyse_combinatoriality(*row);
    REQUIRE(comb.p_combinatorial);
    REQUIRE(comb.p_level == 6);
    REQUIRE(comb.i_combinatorial);
    REQUIRE(comb.ri_combinatorial);
    REQUIRE(comb.all_combinatorial);
}

TEST_CASE("SRTW001A: Webern Op.21 row I-combinatoriality", "[serialism][core]") {
    // Webern Op.21: {9, 6, 7, 8, 4, 5, 11, 10, 2, 3, 0, 1}
    // This row is I-combinatorial
    std::array<PitchClass, 12> pcs = {9, 6, 7, 8, 4, 5, 11, 10, 2, 3, 0, 1};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto comb = analyse_combinatoriality(*row);
    REQUIRE(comb.i_combinatorial);
}

TEST_CASE("SRTW001A: double retrograde is identity", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto r0 = row_retrograde(*row, 0);
    auto rr0 = row_retrograde(r0, 0);
    REQUIRE(rr0 == *row);
}

TEST_CASE("SRTW001A: P_12 is identity", "[serialism][core]") {
    std::array<PitchClass, 12> pcs = {0, 11, 3, 1, 7, 8, 2, 6, 4, 9, 5, 10};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());

    auto p12 = row_prime(*row, 12);
    REQUIRE(p12 == *row);
}
