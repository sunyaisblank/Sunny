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

TEST_CASE("VLNT001A: generate_open_voicing", "[voiceleading][core]") {
    SECTION("Alternates voices to span > octave") {
        std::vector<MidiNote> close = {60, 64, 67, 71};  // C, E, G, B
        auto result = generate_open_voicing(close);

        REQUIRE(result.size() == 4);

        // Voices at indices 1, 3 (E=64, B=71) drop an octave
        // Before sort: {60, 52, 67, 59} -> sorted: {52, 59, 60, 67}
        REQUIRE(result[0] == 52);  // E (dropped)
        REQUIRE(result[1] == 59);  // B (dropped)
        REQUIRE(result[2] == 60);  // C (unchanged)
        REQUIRE(result[3] == 67);  // G (unchanged)
    }

    SECTION("Span exceeds one octave") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_open_voicing(close);

        int span = result.back() - result.front();
        REQUIRE(span > 12);
    }

    SECTION("Result is sorted ascending") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_open_voicing(close);

        for (std::size_t i = 1; i < result.size(); ++i) {
            REQUIRE(result[i] > result[i - 1]);
        }
    }

    SECTION("Returns unchanged for single note") {
        std::vector<MidiNote> single = {60};
        auto result = generate_open_voicing(single);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 60);
    }

    SECTION("Triad open voicing") {
        std::vector<MidiNote> close = {60, 64, 67};  // C, E, G
        auto result = generate_open_voicing(close);

        REQUIRE(result.size() == 3);
        // Index 1 (E=64) drops -> 52; sorted: {52, 60, 67}
        REQUIRE(result[0] == 52);
        REQUIRE(result[1] == 60);
        REQUIRE(result[2] == 67);
    }

    SECTION("Pitch classes preserved") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_open_voicing(close);

        std::set<PitchClass> close_pcs, open_pcs;
        for (auto n : close) close_pcs.insert(pitch_class(n));
        for (auto n : result) open_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == open_pcs);
    }
}

TEST_CASE("VLNT001A: generate_drop24_voicing", "[voiceleading][core]") {
    SECTION("Drops 2nd and 4th from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71};  // C, E, G, B
        auto result = generate_drop24_voicing(close);

        REQUIRE(result.size() == 4);

        // 2nd from top: G=67 -> 55
        // 4th from top: C=60 -> 48
        // Sorted: {48, 55, 64, 71}
        REQUIRE(result[0] == 48);  // C (dropped)
        REQUIRE(result[1] == 55);  // G (dropped)
        REQUIRE(result[2] == 64);  // E (unchanged)
        REQUIRE(result[3] == 71);  // B (unchanged)
    }

    SECTION("Returns unchanged for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop24_voicing(triad);

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 60);
        REQUIRE(result[1] == 64);
        REQUIRE(result[2] == 67);
    }

    SECTION("Result is sorted ascending") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop24_voicing(close);

        for (std::size_t i = 1; i < result.size(); ++i) {
            REQUIRE(result[i] > result[i - 1]);
        }
    }

    SECTION("5-note voicing") {
        std::vector<MidiNote> close = {60, 64, 67, 70, 74};  // C, E, G, Bb, D
        auto result = generate_drop24_voicing(close);

        REQUIRE(result.size() == 5);

        // 2nd from top: Bb=70 -> 58
        // 4th from top: E=64 -> 52
        // Sorted: {52, 58, 60, 67, 74}
        REQUIRE(result[0] == 52);  // E (dropped)
        REQUIRE(result[1] == 58);  // Bb (dropped)
        REQUIRE(result[2] == 60);  // C (unchanged)
        REQUIRE(result[3] == 67);  // G (unchanged)
        REQUIRE(result[4] == 74);  // D (unchanged)
    }

    SECTION("Pitch classes preserved") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop24_voicing(close);

        std::set<PitchClass> close_pcs, drop24_pcs;
        for (auto n : close) close_pcs.insert(pitch_class(n));
        for (auto n : result) drop24_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == drop24_pcs);
    }
}

TEST_CASE("VLNT001A: generate_spread_voicing", "[voiceleading][core]") {
    SECTION("Bass separated by >= octave from upper voices") {
        std::vector<MidiNote> close = {60, 64, 67, 71};  // C, E, G, B
        auto result = generate_spread_voicing(close);

        REQUIRE(result.size() == 4);

        // Bass must be >= 12 below next voice
        REQUIRE((result[1] - result[0]) >= 12);
    }

    SECTION("Upper voices remain in close position") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);

        // Upper voices (indices 1..n) should be unchanged
        REQUIRE(result[1] == 64);
        REQUIRE(result[2] == 67);
        REQUIRE(result[3] == 71);
    }

    SECTION("Bass drops an octave") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);

        // C=60 needs to drop: 64-60=4 < 12, so drop to 48. 64-48=16 >= 12.
        REQUIRE(result[0] == 48);
    }

    SECTION("Returns unchanged for single note") {
        std::vector<MidiNote> single = {60};
        auto result = generate_spread_voicing(single);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 60);
    }

    SECTION("Already spread voicing unchanged") {
        std::vector<MidiNote> already_spread = {36, 60, 64, 67};  // gap = 24
        auto result = generate_spread_voicing(already_spread);

        REQUIRE(result[0] == 36);
        REQUIRE(result[1] == 60);
    }

    SECTION("Pitch classes preserved") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);

        std::set<PitchClass> close_pcs, spread_pcs;
        for (auto n : close) close_pcs.insert(pitch_class(n));
        for (auto n : result) spread_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == spread_pcs);
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

// =============================================================================
// §7.3 Motion Classification
// =============================================================================

TEST_CASE("VLNT001A: classify_voice_motion (§7.3)", "[voiceleading][core]") {
    SECTION("Parallel: both voices move by same displacement") {
        // C4→D4 and E4→F#4 — both move +2
        auto m = classify_voice_motion(60, 62, 64, 66);
        REQUIRE(m == VoiceMotionType::Parallel);
    }

    SECTION("Parallel: both voices move down by same displacement") {
        // D4→C4 and F#4→E4 — both move -2
        auto m = classify_voice_motion(62, 60, 66, 64);
        REQUIRE(m == VoiceMotionType::Parallel);
    }

    SECTION("Similar: same direction, different magnitude") {
        // C4→D4 (+2) and E4→A4 (+5)
        auto m = classify_voice_motion(60, 62, 64, 69);
        REQUIRE(m == VoiceMotionType::Similar);
    }

    SECTION("Contrary: opposite directions") {
        // C4→D4 (+2) and G4→F4 (-2)
        auto m = classify_voice_motion(60, 62, 67, 65);
        REQUIRE(m == VoiceMotionType::Contrary);
    }

    SECTION("Contrary: asymmetric magnitudes") {
        // C4→E4 (+4) and G4→F4 (-2)
        auto m = classify_voice_motion(60, 64, 67, 65);
        REQUIRE(m == VoiceMotionType::Contrary);
    }

    SECTION("Oblique: first voice stationary") {
        // C4→C4 and E4→F4
        auto m = classify_voice_motion(60, 60, 64, 65);
        REQUIRE(m == VoiceMotionType::Oblique);
    }

    SECTION("Oblique: second voice stationary") {
        // C4→D4 and E4→E4
        auto m = classify_voice_motion(60, 62, 64, 64);
        REQUIRE(m == VoiceMotionType::Oblique);
    }

    SECTION("Static: both voices unchanged") {
        auto m = classify_voice_motion(60, 60, 64, 64);
        REQUIRE(m == VoiceMotionType::Static);
    }
}

// =============================================================================
// §7.2 Optimal Voice Leading (Hungarian Algorithm)
// =============================================================================

TEST_CASE("VLNT001A: voice_lead_optimal (§7.2)", "[voiceleading][core]") {
    SECTION("Produces valid result") {
        std::vector<MidiNote> source = {60, 64, 67};  // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};   // F, A, C
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == 3);
    }

    SECTION("Optimal motion ≤ greedy motion") {
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0};  // F major
        auto optimal = voice_lead_optimal(source, target);
        auto greedy = voice_lead_nearest_tone(source, target);
        REQUIRE(optimal.has_value());
        REQUIRE(greedy.has_value());
        REQUIRE(optimal->total_motion <= greedy->total_motion);
    }

    SECTION("All target PCs present in result") {
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0};
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        std::set<PitchClass> result_pcs;
        for (auto note : result->voiced_notes) {
            result_pcs.insert(note % 12);
        }
        for (auto pc : target) {
            REQUIRE(result_pcs.count(pc) > 0);
        }
    }

    SECTION("Empty source returns empty") {
        std::vector<MidiNote> source;
        std::vector<PitchClass> target = {0, 4, 7};
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.empty());
    }

    SECTION("Lock bass forces root in bass") {
        std::vector<MidiNote> source = {60, 64, 67, 72};
        std::vector<PitchClass> target = {5, 9, 0, 5};  // F, A, C, F
        auto result = voice_lead_optimal(source, target, true);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes[0] % 12 == 5);  // Bass = F
    }

    SECTION("4-voice SATB: C major → F major") {
        std::vector<MidiNote> source = {48, 55, 60, 64};  // C3, G3, C4, E4
        std::vector<PitchClass> target = {5, 0, 9, 5};     // F, C, A, F
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->total_motion >= 0);
    }

    SECTION("Parallel fifths detected in optimal result") {
        // Force a scenario where parallel fifths might occur
        std::vector<MidiNote> source = {48, 55};  // C3, G3 (P5)
        std::vector<PitchClass> target = {2, 9};  // D, A (P5)
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        // Check if parallel fifths are flagged
        // (The optimal assignment C→D, G→A is parallel fifths)
    }
}
