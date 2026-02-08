/**
 * @file TSVL001A.cpp
 * @brief Unit tests for VLNT001A (Nearest-tone voice leading)
 *
 * Component: TSVL001A
 * Tests: VLNT001A
 *
 * Tests voice leading algorithm and voicing generation.
 *
 * Key invariants:
 * - len(result) == len(source_pitches)
 * - {p % 12 | p in result} == set(target_pitch_classes)
 * - Total motion is minimized
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "VoiceLeading/VLNT001A.h"
#include "Pitch/PTPC001A.h"

#include <set>

using namespace Sunny::Core;

TEST_CASE("VLNT001A: voice_lead_nearest_tone basic", "[voiceleading][core]") {
    SECTION("Voice count preserved") {
        std::vector<MidiNote> source = {60, 64, 67};  // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};   // F, A, C

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == source.size());
    }

    SECTION("Target pitch classes achieved") {
        std::vector<MidiNote> source = {60, 64, 67};  // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};   // F, A, C

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        std::set<PitchClass> result_pcs;
        for (auto note : result->voiced_notes) {
            result_pcs.insert(pitch_class(note));
        }

        std::set<PitchClass> target_set(target.begin(), target.end());
        REQUIRE(result_pcs == target_set);
    }

    SECTION("Minimal motion for close voicings") {
        // C major -> F major: C->C, E->F, G->A
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0};

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        // Total motion should be small (ideally 0 + 1 + 2 = 3)
        REQUIRE(result->total_motion <= 6);
    }
}

TEST_CASE("VLNT001A: lock_bass option", "[voiceleading][core]") {
    SECTION("Bass takes root when locked") {
        std::vector<MidiNote> source = {48, 64, 67};  // C2, E4, G4
        std::vector<PitchClass> target = {5, 9, 0};   // F, A, C (F major)

        auto result = voice_lead_nearest_tone(source, target, true);  // lock_bass
        REQUIRE(result.has_value());

        // Bass should be F (pitch class 5)
        REQUIRE(pitch_class(result->voiced_notes[0]) == 5);
    }

    SECTION("Bass not locked moves freely") {
        std::vector<MidiNote> source = {48, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0};

        auto result = voice_lead_nearest_tone(source, target, false);
        REQUIRE(result.has_value());

        // Bass can be any target PC
        PitchClass bass_pc = pitch_class(result->voiced_notes[0]);
        bool valid = (bass_pc == 5 || bass_pc == 9 || bass_pc == 0);
        REQUIRE(valid);
    }
}

TEST_CASE("VLNT001A: Parallel motion detection", "[voiceleading][core]") {
    SECTION("Detects parallel fifths") {
        // Setup voices that would create parallel fifths
        std::vector<MidiNote> source = {60, 67};  // C, G (P5 apart)
        std::vector<PitchClass> target = {2, 9};  // D, A (also P5 apart)

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        // If both voices move in same direction maintaining P5,
        // has_parallel_fifths should be true
        if (result->voiced_notes[1] - result->voiced_notes[0] == 7) {
            REQUIRE(result->has_parallel_fifths == true);
        }
    }

    SECTION("Detects parallel octaves") {
        std::vector<MidiNote> source = {60, 72};  // C4, C5 (P8 apart)
        std::vector<PitchClass> target = {2, 2};  // Both to D

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        if (result->voiced_notes[1] - result->voiced_notes[0] == 12) {
            REQUIRE(result->has_parallel_octaves == true);
        }
    }
}

TEST_CASE("VLNT001A: generate_close_voicing", "[voiceleading][core]") {
    SECTION("Creates ascending voicing") {
        std::vector<PitchClass> pcs = {0, 4, 7};  // C, E, G
        auto result = generate_close_voicing(pcs, 4);

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] < result[1]);
        REQUIRE(result[1] < result[2]);
    }

    SECTION("Root in correct octave") {
        std::vector<PitchClass> pcs = {0, 4, 7};
        auto result = generate_close_voicing(pcs, 4);

        REQUIRE(result[0] == 60);  // C4
    }

    SECTION("All notes within octave + third") {
        std::vector<PitchClass> pcs = {0, 4, 7, 11};  // Cmaj7
        auto result = generate_close_voicing(pcs, 4);

        // Should span about an octave
        int span = result.back() - result.front();
        REQUIRE(span <= 16);  // Within major 10th
    }

    SECTION("Empty input returns empty") {
        std::vector<PitchClass> empty;
        auto result = generate_close_voicing(empty, 4);
        REQUIRE(result.empty());
    }
}

TEST_CASE("VLNT001A: generate_drop2_voicing", "[voiceleading][core]") {
    SECTION("Drops second from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71};  // C, E, G, B
        auto result = generate_drop2_voicing(close);

        REQUIRE(result.size() == 4);

        // Second from top (G=67) should drop an octave (to 55)
        // Result should be sorted: {55, 60, 64, 71}
        REQUIRE(result[0] == 55);  // G (dropped)
        REQUIRE(result[1] == 60);  // C
        REQUIRE(result[2] == 64);  // E
        REQUIRE(result[3] == 71);  // B
    }

    SECTION("Returns unchanged for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop2_voicing(triad);

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 60);
        REQUIRE(result[1] == 64);
        REQUIRE(result[2] == 67);
    }

    SECTION("Result is sorted ascending") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop2_voicing(close);

        for (std::size_t i = 1; i < result.size(); ++i) {
            REQUIRE(result[i] > result[i-1]);
        }
    }
}

TEST_CASE("VLNT001A: generate_drop3_voicing", "[voiceleading][core]") {
    SECTION("Drops third from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71};  // C, E, G, B
        auto result = generate_drop3_voicing(close);

        REQUIRE(result.size() == 4);

        // Third from top (E=64) should drop an octave (to 52)
        // Result should be sorted: {52, 60, 67, 71}
        REQUIRE(result[0] == 52);  // E (dropped)
        REQUIRE(result[1] == 60);  // C
        REQUIRE(result[2] == 67);  // G
        REQUIRE(result[3] == 71);  // B
    }

    SECTION("Returns unchanged for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop3_voicing(triad);

        REQUIRE(result.size() == 3);
    }
}

TEST_CASE("VLNT001A: has_parallel_motion", "[voiceleading][core]") {
    SECTION("Parallel fifths detected") {
        // C-G moving to D-A (both P5, same direction)
        bool result = has_parallel_motion(60, 67, 62, 69, 7);
        REQUIRE(result == true);
    }

    SECTION("Contrary motion not parallel") {
        // C-G moving to D-F (C up, G down)
        bool result = has_parallel_motion(60, 67, 62, 65, 7);
        REQUIRE(result == false);
    }

    SECTION("Parallel octaves detected") {
        // C-C moving to D-D (both P8, same direction)
        bool result = has_parallel_motion(60, 72, 62, 74, 0);
        REQUIRE(result == true);
    }

    SECTION("Different intervals not flagged") {
        // C-E moving to D-F (M3 to m3)
        bool result = has_parallel_motion(60, 64, 62, 65, 7);
        REQUIRE(result == false);
    }
}

TEST_CASE("VLNT001A: Edge cases", "[voiceleading][core]") {
    SECTION("Empty source returns empty result") {
        std::vector<MidiNote> source;
        std::vector<PitchClass> target = {0, 4, 7};

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.empty());
    }

    SECTION("Empty target returns error") {
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target;

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Single voice") {
        std::vector<MidiNote> source = {60};
        std::vector<PitchClass> target = {5};  // F

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == 1);
        REQUIRE(pitch_class(result->voiced_notes[0]) == 5);
    }

    SECTION("More voices than target PCs extends targets") {
        std::vector<MidiNote> source = {60, 64, 67, 72};  // 4 voices
        std::vector<PitchClass> target = {0, 4, 7};       // 3 PCs

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == 4);
    }
}

TEST_CASE("VLNT001A: Voice crossing prevention", "[voiceleading][core]") {
    SECTION("Voices maintain relative order when possible") {
        std::vector<MidiNote> source = {48, 60, 72};  // Bass, tenor, soprano
        std::vector<PitchClass> target = {5, 9, 0};   // F, A, C

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        // Check ascending order
        for (std::size_t i = 1; i < result->voiced_notes.size(); ++i) {
            REQUIRE(result->voiced_notes[i] > result->voiced_notes[i-1]);
        }
    }
}

TEST_CASE("VLNT001A: Common progressions", "[voiceleading][core]") {
    SECTION("I -> V in C major") {
        std::vector<MidiNote> I_chord = {60, 64, 67};    // C, E, G
        std::vector<PitchClass> V_pcs = {7, 11, 2};      // G, B, D

        auto result = voice_lead_nearest_tone(I_chord, V_pcs);
        REQUIRE(result.has_value());

        // Motion should be efficient
        REQUIRE(result->total_motion <= 6);
    }

    SECTION("ii -> V -> I in C major") {
        std::vector<MidiNote> ii_chord = {62, 65, 69};   // D, F, A

        // ii -> V
        std::vector<PitchClass> V_pcs = {7, 11, 2};
        auto v_result = voice_lead_nearest_tone(ii_chord, V_pcs);
        REQUIRE(v_result.has_value());

        // V -> I
        std::vector<PitchClass> I_pcs = {0, 4, 7};
        auto i_result = voice_lead_nearest_tone(v_result->voiced_notes, I_pcs);
        REQUIRE(i_result.has_value());

        // Both progressions should have reasonable motion
        REQUIRE(v_result->total_motion <= 8);
        REQUIRE(i_result->total_motion <= 8);
    }
}
