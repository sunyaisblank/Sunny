/**
 * @file TSSG001A.cpp
 * @brief Unit tests for SCGN001A (Scale generation)
 *
 * Component: TSSG001A
 * Tests: SCGN001A
 *
 * Tests scale note generation and quantization.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Scale/SCGN001A.h"
#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

TEST_CASE("SCGN001A: generate_scale_notes basic", "[scale][core]") {
    SECTION("C major scale in octave 4") {
        auto result = generate_scale_notes(0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& notes = *result;
        REQUIRE(notes.size() == 7);

        // C4 = 60, D4 = 62, E4 = 64, F4 = 65, G4 = 67, A4 = 69, B4 = 71
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 62);  // D
        REQUIRE(notes[2] == 64);  // E
        REQUIRE(notes[3] == 65);  // F
        REQUIRE(notes[4] == 67);  // G
        REQUIRE(notes[5] == 69);  // A
        REQUIRE(notes[6] == 71);  // B
    }

    SECTION("Root pitch class invariant") {
        // result[0] % 12 == root_pc
        for (int root = 0; root < 12; ++root) {
            auto result = generate_scale_notes(
                static_cast<PitchClass>(root), SCALE_MAJOR, 4
            );
            REQUIRE(result.has_value());
            REQUIRE((*result)[0] % 12 == root);
        }
    }

    SECTION("Length equals interval count") {
        auto result = generate_scale_notes(0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == SCALE_MAJOR.size());

        auto pent = generate_scale_notes(0, SCALE_PENTATONIC_MAJOR, 4);
        REQUIRE(pent.has_value());
        REQUIRE(pent->size() == SCALE_PENTATONIC_MAJOR.size());
    }

    SECTION("All results in MIDI range") {
        for (int octave = 0; octave <= 9; ++octave) {
            auto result = generate_scale_notes(0, SCALE_MAJOR, octave);
            if (result) {
                for (auto note : *result) {
                    REQUIRE(note >= 0);
                    REQUIRE(note <= 127);
                }
            }
        }
    }
}

TEST_CASE("SCGN001A: generate_scale_notes transposition", "[scale][core]") {
    SECTION("G major scale starts on G") {
        auto result = generate_scale_notes(7, SCALE_MAJOR, 4);  // G = 7
        REQUIRE(result.has_value());
        REQUIRE((*result)[0] == 67);  // G4
        REQUIRE((*result)[0] % 12 == 7);
    }

    SECTION("All scale notes maintain correct intervals") {
        auto c_major = generate_scale_notes(0, SCALE_MAJOR, 4);
        auto g_major = generate_scale_notes(7, SCALE_MAJOR, 4);

        REQUIRE(c_major.has_value());
        REQUIRE(g_major.has_value());

        // Interval pattern should be identical
        for (std::size_t i = 1; i < c_major->size(); ++i) {
            int c_interval = (*c_major)[i] - (*c_major)[0];
            int g_interval = (*g_major)[i] - (*g_major)[0];
            REQUIRE(c_interval == g_interval);
        }
    }
}

TEST_CASE("SCGN001A: generate_scale_range", "[scale][core]") {
    SECTION("Two octave range") {
        auto result = generate_scale_range(0, SCALE_MAJOR, 4, 2);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 14);  // 7 notes * 2 octaves
    }

    SECTION("Notes span correct octave range") {
        auto result = generate_scale_range(0, SCALE_MAJOR, 4, 2);
        REQUIRE(result.has_value());

        // First note C4 = 60, last note B5 = 83
        REQUIRE((*result)[0] == 60);
        REQUIRE((*result)[7] == 72);   // C5
        REQUIRE((*result)[13] == 83);  // B5
    }

    SECTION("Single octave equals generate_scale_notes") {
        auto range = generate_scale_range(0, SCALE_MAJOR, 4, 1);
        auto single = generate_scale_notes(0, SCALE_MAJOR, 4);

        REQUIRE(range.has_value());
        REQUIRE(single.has_value());
        REQUIRE(range->size() == single->size());

        for (std::size_t i = 0; i < range->size(); ++i) {
            REQUIRE((*range)[i] == (*single)[i]);
        }
    }
}

TEST_CASE("SCGN001A: is_note_in_scale", "[scale][core]") {
    SECTION("C major scale degrees are in scale") {
        // C, D, E, F, G, A, B in any octave
        REQUIRE(is_note_in_scale(60, 0, SCALE_MAJOR));  // C
        REQUIRE(is_note_in_scale(62, 0, SCALE_MAJOR));  // D
        REQUIRE(is_note_in_scale(64, 0, SCALE_MAJOR));  // E
        REQUIRE(is_note_in_scale(65, 0, SCALE_MAJOR));  // F
        REQUIRE(is_note_in_scale(67, 0, SCALE_MAJOR));  // G
        REQUIRE(is_note_in_scale(69, 0, SCALE_MAJOR));  // A
        REQUIRE(is_note_in_scale(71, 0, SCALE_MAJOR));  // B
    }

    SECTION("Accidentals not in C major") {
        REQUIRE_FALSE(is_note_in_scale(61, 0, SCALE_MAJOR));  // C#
        REQUIRE_FALSE(is_note_in_scale(63, 0, SCALE_MAJOR));  // D#
        REQUIRE_FALSE(is_note_in_scale(66, 0, SCALE_MAJOR));  // F#
        REQUIRE_FALSE(is_note_in_scale(68, 0, SCALE_MAJOR));  // G#
        REQUIRE_FALSE(is_note_in_scale(70, 0, SCALE_MAJOR));  // A#
    }

    SECTION("Works across octaves") {
        // C in different octaves
        REQUIRE(is_note_in_scale(24, 0, SCALE_MAJOR));  // C1
        REQUIRE(is_note_in_scale(60, 0, SCALE_MAJOR));  // C4
        REQUIRE(is_note_in_scale(96, 0, SCALE_MAJOR));  // C7
    }

    SECTION("Transposed scales") {
        // G major: G, A, B, C, D, E, F#
        REQUIRE(is_note_in_scale(67, 7, SCALE_MAJOR));  // G
        REQUIRE(is_note_in_scale(66, 7, SCALE_MAJOR));  // F# (in G major)
        REQUIRE_FALSE(is_note_in_scale(65, 7, SCALE_MAJOR));  // F (not in G major)
    }
}

TEST_CASE("SCGN001A: quantize_to_scale", "[scale][core]") {
    SECTION("Notes in scale unchanged") {
        REQUIRE(quantize_to_scale(60, 0, SCALE_MAJOR) == 60);  // C stays C
        REQUIRE(quantize_to_scale(64, 0, SCALE_MAJOR) == 64);  // E stays E
    }

    SECTION("Sharps quantize to nearest scale degree") {
        // C# -> C or D (nearest)
        auto quantized = quantize_to_scale(61, 0, SCALE_MAJOR);
        REQUIRE((quantized == 60 || quantized == 62));

        // F# -> F or G
        quantized = quantize_to_scale(66, 0, SCALE_MAJOR);
        REQUIRE((quantized == 65 || quantized == 67));
    }

    SECTION("Preserves octave approximately") {
        auto quantized = quantize_to_scale(61, 0, SCALE_MAJOR);  // C#4
        REQUIRE(quantized >= 60);  // At least C4
        REQUIRE(quantized <= 62);  // At most D4
    }

    SECTION("Works with pentatonic") {
        // Pentatonic major: C, D, E, G, A
        REQUIRE(quantize_to_scale(60, 0, SCALE_PENTATONIC_MAJOR) == 60);  // C
        // F (65) should quantize to E (64) or G (67)
        auto quantized = quantize_to_scale(65, 0, SCALE_PENTATONIC_MAJOR);
        REQUIRE((quantized == 64 || quantized == 67));
    }
}

TEST_CASE("SCGN001A: Edge cases", "[scale][core]") {
    SECTION("Very low octave") {
        auto result = generate_scale_notes(0, SCALE_MAJOR, 0);
        REQUIRE(result.has_value());
        REQUIRE((*result)[0] == 12);  // C0 = MIDI 12 (C-1 = MIDI 0)
    }

    SECTION("Very high octave clamps to MIDI range") {
        auto result = generate_scale_notes(0, SCALE_MAJOR, 10);
        // May fail or clamp - implementation dependent
        if (result) {
            for (auto note : *result) {
                REQUIRE(note <= 127);
            }
        }
    }

    SECTION("Chromatic scale") {
        auto result = generate_scale_notes(0, SCALE_CHROMATIC, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 12);

        // All 12 pitch classes present
        for (int i = 0; i < 12; ++i) {
            REQUIRE((*result)[i] == 60 + i);
        }
    }
}
