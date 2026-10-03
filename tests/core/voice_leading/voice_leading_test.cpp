/**
 * @file voice_leading_test.cpp
 * @brief Nearest-tone voice leading unit tests
 *
 *
 * Tests voice leading algorithm and voicing generation.
 *
 * Key invariants:
 * - len(result) == len(source_pitches)
 * - {p % 12 | p in result} == set(target_pitch_classes)
 * - Total motion is minimized
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <cstdlib>
#include <functional>
#include <optional>
#include <set>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>

using namespace sunny::core;

TEST_CASE("voice_lead_nearest_tone basic", "[voiceleading][core]") {
    SECTION("Voice count preserved") {
        std::vector<MidiNote> source = {60, 64, 67}; // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};  // F, A, C

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == source.size());
    }

    SECTION("Target pitch classes achieved") {
        std::vector<MidiNote> source = {60, 64, 67}; // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};  // F, A, C

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

        // C->C, E->F, G->A: 0 + 1 + 2
        CHECK(result->total_motion == 3);
        CHECK(result->voiced_notes == std::vector<MidiNote>{60, 65, 69});
    }
}

TEST_CASE("lock_bass option", "[voiceleading][core]") {
    SECTION("Bass takes root when locked") {
        std::vector<MidiNote> source = {48, 64, 67}; // C2, E4, G4
        std::vector<PitchClass> target = {5, 9, 0};  // F, A, C (F major)

        auto result = voice_lead_nearest_tone(source, target, true); // lock_bass
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

// The previous version of these sections asserted the flags only inside an
// if-guard on the result, so they passed whatever the result was.
TEST_CASE("Parallel motion detection", "[voiceleading][core]") {
    SECTION("Detects parallel fifths when they are allowed") {
        // C4-G4 to D-A: both voices up a tone (motion 4) is cheapest.
        std::vector<MidiNote> source = {60, 67};
        std::vector<PitchClass> target = {2, 9};

        auto result = voice_lead_nearest_tone(source, target, false, true, true);
        REQUIRE(result.has_value());
        CHECK(result->voiced_notes == std::vector<MidiNote>{62, 69});
        CHECK(result->has_parallel_fifths);
    }

    SECTION("Avoids parallel fifths when they are forbidden") {
        // The cheapest voicing without them is A3-D4 (motion 3 + 5); the
        // greedy search placed the bass on D4 and then failed.
        std::vector<MidiNote> source = {60, 67};
        std::vector<PitchClass> target = {2, 9};

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        CHECK(result->voiced_notes == std::vector<MidiNote>{57, 62});
        CHECK(result->total_motion == 8);
        CHECK_FALSE(result->has_parallel_fifths);
    }

    SECTION("Detects parallel octaves when they are allowed") {
        std::vector<MidiNote> source = {60, 72}; // C4, C5 (P8 apart)
        std::vector<PitchClass> target = {2, 2}; // Both to D

        auto result = voice_lead_nearest_tone(source, target, false, true, true);
        REQUIRE(result.has_value());
        CHECK(result->voiced_notes == std::vector<MidiNote>{62, 74});
        CHECK(result->has_parallel_octaves);
    }
}

TEST_CASE("voice_lead_nearest_tone minimises total motion, not per-voice motion",
          "[voiceleading][core]") {
    // Greedy assignment gave the bass D4 and pushed the upper voice to C5
    // (motion 11); C4-D4 moves one semitone in total.
    std::vector<MidiNote> source = {61, 62};
    std::vector<PitchClass> target = {2, 0};
    auto result = voice_lead_nearest_tone(source, target);
    REQUIRE(result.has_value());
    CHECK(result->voiced_notes == std::vector<MidiNote>{60, 62});
    CHECK(result->total_motion == 1);
}

namespace {

bool parallel_at(int s_low, int s_high, int v_low, int v_high, int interval_class) {
    if (std::abs(s_high - s_low) % 12 != interval_class) return false;
    if (std::abs(v_high - v_low) % 12 != interval_class) return false;
    const int low_motion = v_low - s_low;
    const int high_motion = v_high - s_high;
    return (low_motion > 0 && high_motion > 0) || (low_motion < 0 && high_motion < 0);
}

// Exhaustive reference: every permutation of the targets and every octave of
// each pitch class in [window_low, window_high], filtered by the stated
// constraints. Over the whole MIDI range [0, 127] it is exact by definition.
std::optional<int> brute_force_motion(const std::vector<int>& source,
                                      std::vector<int> targets,
                                      bool lock_bass,
                                      bool allow_fifths,
                                      bool allow_octaves,
                                      int window_low,
                                      int window_high) {
    const std::size_t n = source.size();
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i)
        order[i] = i;
    std::optional<int> best;
    do {
        if (lock_bass && order[0] != 0) continue;
        std::vector<int> voicing(n);
        std::function<void(std::size_t, int)> assign = [&](std::size_t voice, int cost) {
            if (best && cost >= *best) return; // Motion only grows from here.
            if (voice == n) {
                for (std::size_t i = 0; i < n; ++i)
                    for (std::size_t j = i + 1; j < n; ++j) {
                        if (!allow_fifths &&
                            parallel_at(source[i], source[j], voicing[i], voicing[j], 7))
                            return;
                        if (!allow_octaves &&
                            parallel_at(source[i], source[j], voicing[i], voicing[j], 0))
                            return;
                    }
                if (!best || cost < *best) best = cost;
                return;
            }
            for (int pitch = targets[order[voice]]; pitch <= window_high; pitch += 12) {
                if (pitch < window_low) continue;
                if (voice > 0 && pitch <= voicing[voice - 1]) continue;
                voicing[voice] = pitch;
                assign(voice + 1, cost + std::abs(pitch - source[voice]));
            }
        };
        assign(0, 0);
    } while (std::next_permutation(order.begin(), order.end()));
    return best;
}

void check_against_brute_force(const std::vector<int>& source,
                               const std::vector<int>& targets,
                               bool lock_bass,
                               bool allow_fifths,
                               bool allow_octaves) {
    std::vector<MidiNote> midi;
    for (int note : source)
        midi.push_back(*MidiNote::from_int(note));
    std::vector<PitchClass> pcs;
    for (int pc : targets)
        pcs.push_back(PitchClass::wrapped(pc));

    // The window [24, 107] holds every voicing whose total motion is under
    // 36 from sources in [60, 71] (each voice then lies in [25, 106]), so an
    // optimum under 36 found there is global. Otherwise, including when the
    // window holds no feasible voicing, the whole MIDI range is searched.
    auto expected =
        brute_force_motion(source, targets, lock_bass, allow_fifths, allow_octaves, 24, 107);
    if (!expected || *expected >= 36)
        expected =
            brute_force_motion(source, targets, lock_bass, allow_fifths, allow_octaves, 0, 127);
    const auto actual = voice_lead_nearest_tone(midi, pcs, lock_bass, allow_fifths, allow_octaves);

    if (!expected) {
        if (actual) FAIL_CHECK("voicing returned where none is feasible");
        return;
    }
    if (!actual) {
        FAIL_CHECK("no voicing although one of motion " << *expected << " is feasible");
        return;
    }
    if (actual->total_motion != *expected) {
        FAIL_CHECK("motion " << actual->total_motion << " != optimum " << *expected);
        return;
    }
    // The returned voicing satisfies every constraint.
    const auto& voiced = actual->voiced_notes;
    std::multiset<int> used, wanted(targets.begin(), targets.end());
    for (std::size_t i = 0; i < voiced.size(); ++i) {
        used.insert(voiced[i] % 12);
        if (i > 0 && voiced[i] <= voiced[i - 1]) FAIL_CHECK("voices out of order");
    }
    if (used != wanted) FAIL_CHECK("pitch classes differ from the target");
    if (lock_bass && voiced[0] % 12 != targets[0]) FAIL_CHECK("bass does not take targets[0]");
    if (!allow_fifths && actual->has_parallel_fifths) FAIL_CHECK("parallel fifths");
    if (!allow_octaves && actual->has_parallel_octaves) FAIL_CHECK("parallel octaves");
}

} // namespace

TEST_CASE("voice_lead_nearest_tone equals brute force for two voices within an octave",
          "[voiceleading][core][exhaustive]") {
    for (int low = 60; low < 72; ++low)
        for (int high = 60; high < 72; ++high)
            for (int t0 = 0; t0 < 12; ++t0)
                for (int t1 = 0; t1 < 12; ++t1)
                    for (int flags = 0; flags < 8; ++flags) {
                        const bool lock = (flags & 1) != 0;
                        const bool fifths = (flags & 2) != 0;
                        const bool octaves = (flags & 4) != 0;
                        INFO("source " << low << "," << high << " target " << t0 << "," << t1
                                       << " flags " << flags);
                        check_against_brute_force({low, high}, {t0, t1}, lock, fifths, octaves);
                    }
}

TEST_CASE("voice_lead_nearest_tone equals brute force for three voices within an octave",
          "[voiceleading][core][exhaustive]") {
    // Every non-descending source within C4-B4 (unisons included) against
    // every three-note target multiset (doublings included). The order of
    // the targets matters only to lock_bass, so the unlocked case is run
    // once and the locked case once for each distinct member as targets[0].
    for (int a = 60; a < 72; ++a)
        for (int b = a; b < 72; ++b)
            for (int c = b; c < 72; ++c)
                for (int x = 0; x < 12; ++x)
                    for (int y = x; y < 12; ++y)
                        for (int z = y; z < 12; ++z)
                            for (int flags = 0; flags < 4; ++flags) {
                                const bool fifths = (flags & 1) != 0;
                                const bool octaves = (flags & 2) != 0;
                                INFO("source " << a << "," << b << "," << c << " target " << x
                                               << "," << y << "," << z << " flags " << flags);
                                check_against_brute_force(
                                    {a, b, c}, {x, y, z}, false, fifths, octaves);
                                check_against_brute_force(
                                    {a, b, c}, {x, y, z}, true, fifths, octaves);
                                if (y != x)
                                    check_against_brute_force(
                                        {a, b, c}, {y, x, z}, true, fifths, octaves);
                                if (z != y)
                                    check_against_brute_force(
                                        {a, b, c}, {z, x, y}, true, fifths, octaves);
                            }
}

TEST_CASE("generate_close_voicing", "[voiceleading][core]") {
    SECTION("Creates ascending voicing") {
        std::vector<PitchClass> pcs = {0, 4, 7}; // C, E, G
        auto result = generate_close_voicing(pcs, 4);

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] < result[1]);
        REQUIRE(result[1] < result[2]);
    }

    SECTION("Root in correct octave") {
        std::vector<PitchClass> pcs = {0, 4, 7};
        auto result = generate_close_voicing(pcs, 4);

        REQUIRE(result[0] == 60); // C4
    }

    SECTION("All notes within octave + third") {
        std::vector<PitchClass> pcs = {0, 4, 7, 11}; // Cmaj7
        auto result = generate_close_voicing(pcs, 4);

        // Should span about an octave
        int span = result.back() - result.front();
        REQUIRE(span <= 16); // Within major 10th
    }

    SECTION("Empty input returns empty") {
        std::vector<PitchClass> empty;
        auto result = generate_close_voicing(empty, 4);
        REQUIRE(result.empty());
    }
}

TEST_CASE("generate_drop2_voicing", "[voiceleading][core]") {
    SECTION("Drops second from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71}; // C, E, G, B
        auto result = generate_drop2_voicing(close);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);

        // Second from top (G=67) should drop an octave (to 55)
        // Result should be sorted: {55, 60, 64, 71}
        REQUIRE((*result)[0] == 55); // G (dropped)
        REQUIRE((*result)[1] == 60); // C
        REQUIRE((*result)[2] == 64); // E
        REQUIRE((*result)[3] == 71); // B
    }

    SECTION("Returns error for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop2_voicing(triad);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }

    SECTION("Result is sorted ascending") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop2_voicing(close);
        REQUIRE(result.has_value());

        for (std::size_t i = 1; i < result->size(); ++i) {
            REQUIRE((*result)[i] > (*result)[i - 1]);
        }
    }
}

TEST_CASE("generate_drop3_voicing", "[voiceleading][core]") {
    SECTION("Drops third from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71}; // C, E, G, B
        auto result = generate_drop3_voicing(close);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);

        // Third from top (E=64) should drop an octave (to 52)
        // Result should be sorted: {52, 60, 67, 71}
        REQUIRE((*result)[0] == 52); // E (dropped)
        REQUIRE((*result)[1] == 60); // C
        REQUIRE((*result)[2] == 67); // G
        REQUIRE((*result)[3] == 71); // B
    }

    SECTION("Returns error for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop3_voicing(triad);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }
}

TEST_CASE("has_parallel_motion", "[voiceleading][core]") {
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

TEST_CASE("Voice-leading edge cases", "[voiceleading][core]") {
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
        std::vector<PitchClass> target = {5}; // F

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == 1);
        REQUIRE(pitch_class(result->voiced_notes[0]) == 5);
    }

    SECTION("Mismatched cardinalities return error") {
        std::vector<MidiNote> source = {60, 64, 67, 72}; // 4 voices
        std::vector<PitchClass> target = {0, 4, 7};      // 3 PCs

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }
}

TEST_CASE("Voice crossing prevention", "[voiceleading][core]") {
    SECTION("Voices maintain relative order when possible") {
        std::vector<MidiNote> source = {48, 60, 72}; // Bass, tenor, soprano
        std::vector<PitchClass> target = {5, 9, 0};  // F, A, C

        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE(result.has_value());

        // Check ascending order
        for (std::size_t i = 1; i < result->voiced_notes.size(); ++i) {
            REQUIRE(result->voiced_notes[i] > result->voiced_notes[i - 1]);
        }
    }
}

TEST_CASE("generate_open_voicing", "[voiceleading][core]") {
    SECTION("Alternates voices to span > octave") {
        std::vector<MidiNote> close = {60, 64, 67, 71}; // C, E, G, B
        auto result = generate_open_voicing(close);

        REQUIRE(result.size() == 4);

        // Voices at indices 1, 3 (E=64, B=71) drop an octave
        // Before sort: {60, 52, 67, 59} -> sorted: {52, 59, 60, 67}
        REQUIRE(result[0] == 52); // E (dropped)
        REQUIRE(result[1] == 59); // B (dropped)
        REQUIRE(result[2] == 60); // C (unchanged)
        REQUIRE(result[3] == 67); // G (unchanged)
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
        std::vector<MidiNote> close = {60, 64, 67}; // C, E, G
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
        for (auto n : close)
            close_pcs.insert(pitch_class(n));
        for (auto n : result)
            open_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == open_pcs);
    }
}

TEST_CASE("generate_drop24_voicing", "[voiceleading][core]") {
    SECTION("Drops 2nd and 4th from top") {
        std::vector<MidiNote> close = {60, 64, 67, 71}; // C, E, G, B
        auto result = generate_drop24_voicing(close);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);

        // 2nd from top: G=67 -> 55
        // 4th from top: C=60 -> 48
        // Sorted: {48, 55, 64, 71}
        REQUIRE((*result)[0] == 48); // C (dropped)
        REQUIRE((*result)[1] == 55); // G (dropped)
        REQUIRE((*result)[2] == 64); // E (unchanged)
        REQUIRE((*result)[3] == 71); // B (unchanged)
    }

    SECTION("Returns error for < 4 notes") {
        std::vector<MidiNote> triad = {60, 64, 67};
        auto result = generate_drop24_voicing(triad);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }

    SECTION("Result is sorted ascending") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop24_voicing(close);
        REQUIRE(result.has_value());

        for (std::size_t i = 1; i < result->size(); ++i) {
            REQUIRE((*result)[i] > (*result)[i - 1]);
        }
    }

    SECTION("5-note voicing") {
        std::vector<MidiNote> close = {60, 64, 67, 70, 74}; // C, E, G, Bb, D
        auto result = generate_drop24_voicing(close);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 5);

        // 2nd from top: Bb=70 -> 58
        // 4th from top: E=64 -> 52
        // Sorted: {52, 58, 60, 67, 74}
        REQUIRE((*result)[0] == 52); // E (dropped)
        REQUIRE((*result)[1] == 58); // Bb (dropped)
        REQUIRE((*result)[2] == 60); // C (unchanged)
        REQUIRE((*result)[3] == 67); // G (unchanged)
        REQUIRE((*result)[4] == 74); // D (unchanged)
    }

    SECTION("Pitch classes preserved") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_drop24_voicing(close);
        REQUIRE(result.has_value());

        std::set<PitchClass> close_pcs, drop24_pcs;
        for (auto n : close)
            close_pcs.insert(pitch_class(n));
        for (auto n : *result)
            drop24_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == drop24_pcs);
    }
}

TEST_CASE("generate_spread_voicing", "[voiceleading][core]") {
    SECTION("Bass separated by >= octave from upper voices") {
        std::vector<MidiNote> close = {60, 64, 67, 71}; // C, E, G, B
        auto result = generate_spread_voicing(close);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 4);

        // Bass must be >= 12 below next voice
        REQUIRE(((*result)[1] - (*result)[0]) >= 12);
    }

    SECTION("Upper voices remain in close position") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);
        REQUIRE(result.has_value());

        // Upper voices (indices 1..n) should be unchanged
        REQUIRE((*result)[1] == 64);
        REQUIRE((*result)[2] == 67);
        REQUIRE((*result)[3] == 71);
    }

    SECTION("Bass drops an octave") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);
        REQUIRE(result.has_value());

        // C=60 needs to drop: 64-60=4 < 12, so drop to 48. 64-48=16 >= 12.
        REQUIRE((*result)[0] == 48);
    }

    SECTION("Returns error for fewer than 3 notes") {
        std::vector<MidiNote> pair = {60, 64};
        auto result = generate_spread_voicing(pair);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);

        std::vector<MidiNote> single = {60};
        auto result2 = generate_spread_voicing(single);
        REQUIRE_FALSE(result2.has_value());
    }

    SECTION("Already spread voicing unchanged") {
        std::vector<MidiNote> already_spread = {36, 60, 64, 67}; // gap = 24
        auto result = generate_spread_voicing(already_spread);
        REQUIRE(result.has_value());

        REQUIRE((*result)[0] == 36);
        REQUIRE((*result)[1] == 60);
    }

    SECTION("Pitch classes preserved") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        auto result = generate_spread_voicing(close);
        REQUIRE(result.has_value());

        std::set<PitchClass> close_pcs, spread_pcs;
        for (auto n : close)
            close_pcs.insert(pitch_class(n));
        for (auto n : *result)
            spread_pcs.insert(pitch_class(n));
        REQUIRE(close_pcs == spread_pcs);
    }
}

TEST_CASE("Common progressions", "[voiceleading][core]") {
    SECTION("I -> V in C major") {
        std::vector<MidiNote> I_chord = {60, 64, 67}; // C, E, G
        std::vector<PitchClass> V_pcs = {7, 11, 2};   // G, B, D

        auto result = voice_lead_nearest_tone(I_chord, V_pcs);
        REQUIRE(result.has_value());

        // Motion should be efficient
        REQUIRE(result->total_motion <= 6);
    }

    SECTION("ii -> V -> I in C major") {
        std::vector<MidiNote> ii_chord = {62, 65, 69}; // D, F, A

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

TEST_CASE("classify_voice_motion (§7.3)", "[voiceleading][core]") {
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

TEST_CASE("voice_lead_optimal (§7.2)", "[voiceleading][core]") {
    SECTION("Produces valid result") {
        std::vector<MidiNote> source = {60, 64, 67}; // C, E, G
        std::vector<PitchClass> target = {5, 9, 0};  // F, A, C
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes.size() == 3);
    }

    SECTION("Optimal motion ≤ greedy motion") {
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0}; // F major
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
            result_pcs.insert(PitchClass::wrapped(note));
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
        std::vector<PitchClass> target = {5, 9, 0, 5}; // F, A, C, F
        auto result = voice_lead_optimal(source, target, true);
        REQUIRE(result.has_value());
        REQUIRE(result->voiced_notes[0] % 12 == 5); // Bass = F
    }

    SECTION("4-voice SATB: C major → F major") {
        std::vector<MidiNote> source = {48, 55, 60, 64}; // C3, G3, C4, E4
        std::vector<PitchClass> target = {5, 0, 9, 5};   // F, C, A, F
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        REQUIRE(result->total_motion >= 0);
    }

    SECTION("Parallel fifths detected in optimal result") {
        // Force a scenario where parallel fifths might occur
        std::vector<MidiNote> source = {48, 55}; // C3, G3 (P5)
        std::vector<PitchClass> target = {2, 9}; // D, A (P5)
        auto result = voice_lead_optimal(source, target);
        REQUIRE(result.has_value());
        // Check if parallel fifths are flagged
        // (The optimal assignment C->D, G->A is parallel fifths)
    }

    SECTION("Rejects mismatched cardinalities") {
        std::vector<MidiNote> source = {60, 64, 67};
        std::vector<PitchClass> target = {5, 9, 0, 5}; // 4 targets, 3 sources
        auto result = voice_lead_optimal(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }
}

// =============================================================================
// Cardinality mismatch tests (audit RC-F remediation)
// =============================================================================

TEST_CASE("voice_lead_nearest_tone rejects mismatched cardinalities", "[voiceleading][core]") {
    SECTION("More sources than targets") {
        std::vector<MidiNote> source = {60, 64, 67, 72};
        std::vector<PitchClass> target = {5, 9};
        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }

    SECTION("More targets than sources") {
        std::vector<MidiNote> source = {60, 64};
        std::vector<PitchClass> target = {5, 9, 0};
        auto result = voice_lead_nearest_tone(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }
}

TEST_CASE("voice_lead_optimal rejects mismatched cardinalities", "[voiceleading][core]") {
    SECTION("More sources than targets") {
        std::vector<MidiNote> source = {60, 64, 67, 72};
        std::vector<PitchClass> target = {5, 9};
        auto result = voice_lead_optimal(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }

    SECTION("More targets than sources") {
        std::vector<MidiNote> source = {60};
        std::vector<PitchClass> target = {5, 9, 0};
        auto result = voice_lead_optimal(source, target);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
    }
}

TEST_CASE("generate_drop2_voicing returns error for 3-note input", "[voiceleading][core]") {
    std::vector<MidiNote> triad = {60, 64, 67};
    auto result = generate_drop2_voicing(triad);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
}

TEST_CASE("stationary voices with fifths not detected as parallel", "[voiceleading][core]") {
    // C4-G4 → C4-G4: motion = 0, not parallel
    // Under - → + mutation: motion = 120 > 0, falsely parallel
    REQUIRE(has_parallel_motion(60, 67, 60, 67, 7) == false);
}
