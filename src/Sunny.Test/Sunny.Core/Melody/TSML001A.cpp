/**
 * @file TSML001A.cpp
 * @brief Unit tests for MLCT001A (Melody analysis)
 *
 * Component: TSML001A
 * Tests: MLCT001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Melody/MLCT001A.h"
#include "Scale/SCDF001A.h"
#include "Pitch/PTPC001A.h"

using namespace Sunny::Core;

// =============================================================================
// Contour
// =============================================================================

TEST_CASE("MLCT001A: ascending scale contour all ascending", "[melody][core]") {
    std::array<MidiNote, 8> notes = {60, 62, 64, 65, 67, 69, 71, 72};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    REQUIRE(contour->size() == 7);
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Ascending);
    }
}

TEST_CASE("MLCT001A: descending scale contour all descending", "[melody][core]") {
    std::array<MidiNote, 5> notes = {72, 71, 69, 67, 65};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Descending);
    }
}

TEST_CASE("MLCT001A: repeated notes contour all stationary", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 60, 60, 60};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Stationary);
    }
}

TEST_CASE("MLCT001A: mixed contour", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 64, 62, 67};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    REQUIRE(contour->size() == 3);
    REQUIRE((*contour)[0] == ContourDirection::Ascending);
    REQUIRE((*contour)[1] == ContourDirection::Descending);
    REQUIRE((*contour)[2] == ContourDirection::Ascending);
}

TEST_CASE("MLCT001A: single note returns error", "[melody][core]") {
    std::array<MidiNote, 1> notes = {60};
    auto contour = extract_contour(notes);
    REQUIRE_FALSE(contour.has_value());
    REQUIRE(contour.error() == ErrorCode::InvalidMelody);
}

// =============================================================================
// Contour Reduction
// =============================================================================

TEST_CASE("MLCT001A: contour reduction of arch", "[melody][core]") {
    // Arch shape: up then down — extrema are endpoints and the peak
    std::array<MidiNote, 7> notes = {60, 62, 64, 67, 64, 62, 60};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    // Should keep first (60), peak (67), last (60)
    REQUIRE(reduced->size() == 3);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 67);
    REQUIRE((*reduced)[2] == 60);
}

TEST_CASE("MLCT001A: contour reduction of ascending scale", "[melody][core]") {
    // Monotonic ascending: no interior maxima/minima, just keep first and last
    std::array<MidiNote, 5> notes = {60, 62, 64, 65, 67};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    REQUIRE(reduced->size() == 2);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 67);
}

// =============================================================================
// Motion Classification
// =============================================================================

TEST_CASE("MLCT001A: classify_motion step and leap", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 62, 67, 65};
    auto motion = classify_motion(notes);
    REQUIRE(motion.has_value());
    REQUIRE(motion->size() == 3);
    REQUIRE((*motion)[0] == MotionType::Conjunct);   // +2
    REQUIRE((*motion)[1] == MotionType::Disjunct);    // +5
    REQUIRE((*motion)[2] == MotionType::Conjunct);    // -2
}

TEST_CASE("MLCT001A: predominantly conjunct for scale", "[melody][core]") {
    std::array<MidiNote, 8> notes = {60, 62, 64, 65, 67, 69, 71, 72};
    auto result = is_predominantly_conjunct(notes);
    REQUIRE(result.has_value());
    REQUIRE(*result == true);
}

TEST_CASE("MLCT001A: not predominantly conjunct for arpeggiated", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 67, 72, 79};
    auto result = is_predominantly_conjunct(notes);
    REQUIRE(result.has_value());
    REQUIRE(*result == false);
}

// =============================================================================
// Statistics
// =============================================================================

TEST_CASE("MLCT001A: statistics range and mean", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 62, 64, 65, 67};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    REQUIRE(stats->range == 7);
    REQUIRE_THAT(stats->mean_pitch, Catch::Matchers::WithinAbs(63.6, 0.1));
}

TEST_CASE("MLCT001A: statistics tessitura", "[melody][core]") {
    std::array<MidiNote, 10> notes = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // 10th percentile: index 1 = 61, 90th percentile: index 9 = 69
    REQUIRE(stats->tessitura.first == 61);
    REQUIRE(stats->tessitura.second == 69);
}

TEST_CASE("MLCT001A: statistics interval histogram", "[melody][core]") {
    // All ascending by semitone
    std::array<MidiNote, 5> notes = {60, 61, 62, 63, 64};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // Interval +1 -> index 13, should be 4
    REQUIRE(stats->interval_histogram[13] == 4);
}

TEST_CASE("MLCT001A: statistics pitch class histogram", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 72, 48, 64};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // C (pc=0) appears 3 times (60, 72, 48), E (pc=4) appears once
    REQUIRE(stats->pitch_class_histogram[0] == 3);
    REQUIRE(stats->pitch_class_histogram[4] == 1);
}

// =============================================================================
// Tendency Tones
// =============================================================================

TEST_CASE("MLCT001A: standard_tendency_tones major", "[melody][core]") {
    auto tones = standard_tendency_tones(SCALE_MAJOR);
    REQUIRE_FALSE(tones.empty());

    // Should have leading tone -> tonic (degree 6 -> 0)
    bool found_leading = false;
    for (auto& tt : tones) {
        if (tt.scale_degree == 6 && tt.resolution_degree == 0 &&
            tt.direction == ContourDirection::Ascending) {
            found_leading = true;
        }
    }
    REQUIRE(found_leading);
}

TEST_CASE("MLCT001A: standard_tendency_tones minor no leading tone", "[melody][core]") {
    // Natural minor: degree 6 interval is 10 (not 11), so no leading tone tendency
    auto tones = standard_tendency_tones(SCALE_MINOR);
    bool has_leading = false;
    for (auto& tt : tones) {
        if (tt.scale_degree == 6 && tt.direction == ContourDirection::Ascending) {
            has_leading = true;
        }
    }
    REQUIRE_FALSE(has_leading);
}

TEST_CASE("MLCT001A: follows_tendency B->C in C major", "[melody][core]") {
    // B (71) -> C (72) in C major: leading tone resolving up
    REQUIRE(follows_tendency(71, 72, 0, SCALE_MAJOR));
}

TEST_CASE("MLCT001A: follows_tendency B->A does not follow", "[melody][core]") {
    // B (71) -> A (69): leading tone resolving down — not a standard tendency
    REQUIRE_FALSE(follows_tendency(71, 69, 0, SCALE_MAJOR));
}

// =============================================================================
// Real Sequence Detection
// =============================================================================

TEST_CASE("MLCT001A: detect_real_sequences ascending pattern", "[melody][core]") {
    // Pattern: C-D-E transposed up by step twice
    // C(60) D(62) E(64) | D(62) E(64) F#(66) | E(64) F#(66) G#(68)
    std::array<MidiNote, 9> notes = {60, 62, 64, 62, 64, 66, 64, 66, 68};
    auto seqs = detect_real_sequences(notes, 2, 2);
    REQUIRE(seqs.has_value());
    REQUIRE_FALSE(seqs->empty());

    bool found = false;
    for (auto& seq : *seqs) {
        if (seq.is_real && seq.repetition_count >= 2) {
            found = true;
        }
    }
    REQUIRE(found);
}

TEST_CASE("MLCT001A: no sequence found returns empty", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 67, 61, 70, 55};
    auto seqs = detect_real_sequences(notes, 3, 2);
    REQUIRE(seqs.has_value());
    REQUIRE(seqs->empty());
}

// =============================================================================
// Tonal Sequence Detection
// =============================================================================

TEST_CASE("MLCT001A: detect_tonal_sequences in C major", "[melody][core]") {
    // Ascending thirds tonal sequence in C major:
    // C(60) E(64) | D(62) F(65) | E(64) G(67)
    // Degrees:  0  2 | 1  3 | 2  4
    // Degree intervals: [+2, -1, +2, -1, +2]
    // Pattern [+2, -1] repeats at indices 0 and 2
    std::array<MidiNote, 6> notes = {60, 64, 62, 65, 64, 67};
    auto seqs = detect_tonal_sequences(notes, 0, SCALE_MAJOR, 2, 2);
    REQUIRE(seqs.has_value());
    REQUIRE_FALSE(seqs->empty());

    bool found = false;
    for (auto& seq : *seqs) {
        if (!seq.is_real && seq.repetition_count >= 2) {
            found = true;
        }
    }
    REQUIRE(found);
}

// =============================================================================
// Mutation-killing tests
// =============================================================================

TEST_CASE("MLCT001A: two-note input accepted by all functions", "[melody][core]") {
    std::array<MidiNote, 2> notes = {60, 72};

    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    REQUIRE(contour->size() == 1);
    REQUIRE((*contour)[0] == ContourDirection::Ascending);

    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    REQUIRE(reduced->size() == 2);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 72);

    auto motion = classify_motion(notes);
    REQUIRE(motion.has_value());

    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
}

TEST_CASE("MLCT001A: contour reduction with consecutive duplicates", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 60, 65, 65, 60};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    // After dedup: {60, 65, 60} — all are extrema, so reduction keeps all three
    REQUIRE(reduced->size() == 3);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 65);
    REQUIRE((*reduced)[2] == 60);
}

TEST_CASE("MLCT001A: conjunct threshold at exact boundary", "[melody][core]") {
    // Intervals: 60->62 = +2 (conjunct), 62->67 = +5 (disjunct)
    // conjunct_ratio = 1/2 = 0.5
    std::array<MidiNote, 3> notes = {60, 62, 67};

    auto result_at = is_predominantly_conjunct(notes, 0.5);
    REQUIRE(result_at.has_value());
    REQUIRE(*result_at == true);  // ratio (0.5) >= threshold (0.5)

    auto result_above = is_predominantly_conjunct(notes, 0.51);
    REQUIRE(result_above.has_value());
    REQUIRE(*result_above == false);  // ratio (0.5) < threshold (0.51)
}

TEST_CASE("MLCT001A: statistics with descending interval", "[melody][core]") {
    std::array<MidiNote, 2> notes = {72, 60};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // Interval = 60 - 72 = -12, clamped to -12, index = -12 + 12 = 0
    REQUIRE(stats->interval_histogram[0] == 1);
    // All other histogram entries should be zero
    for (int i = 1; i < 25; ++i) {
        REQUIRE(stats->interval_histogram[i] == 0);
    }
}

TEST_CASE("MLCT001A: contour reduction with plateau", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 65, 65, 60};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    // After dedup: {60, 65, 60} — 65 is local max, both 60s are endpoints
    REQUIRE(reduced->size() == 3);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 65);
    REQUIRE((*reduced)[2] == 60);
}
