/**
 * @file analysis_test.cpp
 * @brief Melody analysis unit tests
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <sunny/core/melody/analysis.hpp>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/scale/definitions.hpp>

using namespace sunny::core;

// =============================================================================
// Contour
// =============================================================================

TEST_CASE("ascending scale contour all ascending", "[melody][core]") {
    std::array<MidiNote, 8> notes = {60, 62, 64, 65, 67, 69, 71, 72};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    REQUIRE(contour->size() == 7);
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Ascending);
    }
}

TEST_CASE("descending scale contour all descending", "[melody][core]") {
    std::array<MidiNote, 5> notes = {72, 71, 69, 67, 65};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Descending);
    }
}

TEST_CASE("repeated notes contour all stationary", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 60, 60, 60};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    for (auto c : *contour) {
        REQUIRE(c == ContourDirection::Stationary);
    }
}

TEST_CASE("mixed contour", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 64, 62, 67};
    auto contour = extract_contour(notes);
    REQUIRE(contour.has_value());
    REQUIRE(contour->size() == 3);
    REQUIRE((*contour)[0] == ContourDirection::Ascending);
    REQUIRE((*contour)[1] == ContourDirection::Descending);
    REQUIRE((*contour)[2] == ContourDirection::Ascending);
}

TEST_CASE("single note returns error", "[melody][core]") {
    std::array<MidiNote, 1> notes = {60};
    auto contour = extract_contour(notes);
    REQUIRE_FALSE(contour.has_value());
    REQUIRE(contour.error() == ErrorCode::InvalidMelody);
}

// =============================================================================
// Contour Reduction
// =============================================================================

TEST_CASE("contour reduction of arch", "[melody][core]") {
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

TEST_CASE("contour reduction of ascending scale", "[melody][core]") {
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

TEST_CASE("classify_motion step and leap", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 62, 67, 65};
    auto motion = classify_motion(notes);
    REQUIRE(motion.has_value());
    REQUIRE(motion->size() == 3);
    REQUIRE((*motion)[0] == MotionType::Conjunct); // +2
    REQUIRE((*motion)[1] == MotionType::Disjunct); // +5
    REQUIRE((*motion)[2] == MotionType::Conjunct); // -2
}

TEST_CASE("predominantly conjunct for scale", "[melody][core]") {
    std::array<MidiNote, 8> notes = {60, 62, 64, 65, 67, 69, 71, 72};
    auto result = is_predominantly_conjunct(notes);
    REQUIRE(result.has_value());
    REQUIRE(*result == true);
}

TEST_CASE("not predominantly conjunct for arpeggiated", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 67, 72, 79};
    auto result = is_predominantly_conjunct(notes);
    REQUIRE(result.has_value());
    REQUIRE(*result == false);
}

// =============================================================================
// Statistics
// =============================================================================

TEST_CASE("statistics range and mean", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 62, 64, 65, 67};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    REQUIRE(stats->range == 7);
    REQUIRE_THAT(stats->mean_pitch, Catch::Matchers::WithinAbs(63.6, 0.1));
}

TEST_CASE("statistics tessitura", "[melody][core]") {
    std::array<MidiNote, 10> notes = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // 10th percentile: index 1 = 61, 90th percentile: index 9 = 69
    REQUIRE(stats->tessitura.first == 61);
    REQUIRE(stats->tessitura.second == 69);
}

TEST_CASE("statistics interval histogram", "[melody][core]") {
    // All ascending by semitone
    std::array<MidiNote, 5> notes = {60, 61, 62, 63, 64};
    auto stats = compute_melody_statistics(notes);
    REQUIRE(stats.has_value());
    // Interval +1 -> index 13, should be 4
    REQUIRE(stats->interval_histogram[13] == 4);
}

TEST_CASE("statistics pitch class histogram", "[melody][core]") {
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

TEST_CASE("standard_tendency_tones major", "[melody][core]") {
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

TEST_CASE("standard_tendency_tones minor no leading tone", "[melody][core]") {
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

TEST_CASE("follows_tendency B->C in C major", "[melody][core]") {
    // B (71) -> C (72) in C major: leading tone resolving up
    REQUIRE(follows_tendency(71, 72, 0, SCALE_MAJOR));
}

TEST_CASE("follows_tendency B->A does not follow", "[melody][core]") {
    // B (71) -> A (69): leading tone resolving down — not a standard tendency
    REQUIRE_FALSE(follows_tendency(71, 69, 0, SCALE_MAJOR));
}

// =============================================================================
// Real Sequence Detection
// =============================================================================

TEST_CASE("detect_real_sequences ascending pattern", "[melody][core]") {
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

TEST_CASE("no sequence found returns empty", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 67, 61, 70, 55};
    auto seqs = detect_real_sequences(notes, 3, 2);
    REQUIRE(seqs.has_value());
    REQUIRE(seqs->empty());
}

// =============================================================================
// Tonal Sequence Detection
// =============================================================================

TEST_CASE("detect_tonal_sequences in C major", "[melody][core]") {
    // Ascending thirds tonal sequence in C major:
    // C(60) E(64) | D(62) F(65) | E(64) G(67)
    // Degrees:  0  2 | 1  3 | 2  4
    // Each adjacent two-note motif has one interval (+2 degrees).
    std::array<MidiNote, 6> notes = {60, 64, 62, 65, 64, 67};
    auto seqs = detect_tonal_sequences(notes, 0, SCALE_MAJOR, 1, 2);
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

TEST_CASE("two-note input accepted by all functions", "[melody][core]") {
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

TEST_CASE("contour reduction with consecutive duplicates", "[melody][core]") {
    std::array<MidiNote, 5> notes = {60, 60, 65, 65, 60};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    // After dedup: {60, 65, 60} — all are extrema, so reduction keeps all three
    REQUIRE(reduced->size() == 3);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 65);
    REQUIRE((*reduced)[2] == 60);
}

TEST_CASE("conjunct threshold at exact boundary", "[melody][core]") {
    // Intervals: 60->62 = +2 (conjunct), 62->67 = +5 (disjunct)
    // conjunct_ratio = 1/2 = 0.5
    std::array<MidiNote, 3> notes = {60, 62, 67};

    auto result_at = is_predominantly_conjunct(notes, 0.5);
    REQUIRE(result_at.has_value());
    REQUIRE(*result_at == true); // ratio (0.5) >= threshold (0.5)

    auto result_above = is_predominantly_conjunct(notes, 0.51);
    REQUIRE(result_above.has_value());
    REQUIRE(*result_above == false); // ratio (0.5) < threshold (0.51)
}

TEST_CASE("statistics with descending interval", "[melody][core]") {
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

TEST_CASE("contour reduction with plateau", "[melody][core]") {
    std::array<MidiNote, 4> notes = {60, 65, 65, 60};
    auto reduced = contour_reduction(notes);
    REQUIRE(reduced.has_value());
    // After dedup: {60, 65, 60} — 65 is local max, both 60s are endpoints
    REQUIRE(reduced->size() == 3);
    REQUIRE((*reduced)[0] == 60);
    REQUIRE((*reduced)[1] == 65);
    REQUIRE((*reduced)[2] == 60);
}

TEST_CASE("sequence controls reject invalid domains without division or overflow",
          "[melody][core][theory-domain]") {
    const std::array<MidiNote, 3> notes{60, 62, 64};
    for (const auto& [length, reps] : std::array<std::pair<int, int>, 6>{
             {{2, 0},
              {2, -1},
              {2, 1},
              {0, 2},
              {-1, 2},
              {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}}}) {
        REQUIRE(detect_real_sequences(notes, length, reps).error() == ErrorCode::InvalidMelody);
        REQUIRE(detect_tonal_sequences(notes, 0, SCALE_MAJOR, length, reps).error() ==
                ErrorCode::InvalidMelody);
    }
    REQUIRE(detect_real_sequences(notes, std::numeric_limits<int>::max(), 2)->empty());
    REQUIRE(detect_real_sequences(notes, 1, std::numeric_limits<int>::max())->empty());
    REQUIRE(detect_real_sequences(notes, 1, 2, static_cast<SequenceLayout>(255)).error() ==
            ErrorCode::InvalidMelody);
    const std::array<Interval, 3> unsorted{0, 7, 2};
    const std::array<Interval, 3> duplicate{0, 2, 2};
    const std::array<Interval, 3> not_rooted{1, 2, 4};
    const std::array<Interval, 3> too_wide{0, 2, 12};
    for (auto scale : {std::span<const Interval>{},
                       std::span<const Interval>(unsorted),
                       std::span<const Interval>(duplicate),
                       std::span<const Interval>(not_rooted),
                       std::span<const Interval>(too_wide)}) {
        REQUIRE(detect_tonal_sequences(notes, 0, scale, 1, 2).error() ==
                ErrorCode::InvalidScaleName);
    }
}

TEST_CASE("real sequence layout distinguishes adjacent and shared endpoint motifs",
          "[melody][core][theory-domain]") {
    const std::array<MidiNote, 9> adjacent{60, 62, 64, 62, 64, 66, 64, 66, 68};
    auto detected = detect_real_sequences(adjacent, 2, 3);
    REQUIRE(detected.has_value());
    REQUIRE(detected->size() == 1);
    REQUIRE(detected->front().start_index == 0);
    REQUIRE(detected->front().pattern_length == 2);
    REQUIRE(detected->front().repetition_count == 3);
    REQUIRE(detected->front().transposition_interval == 2);
    REQUIRE(detected->front().layout == SequenceLayout::Adjacent);
    const std::array<MidiNote, 6> just_two{60, 62, 64, 62, 64, 66};
    REQUIRE(detect_real_sequences(just_two, 2, 2)->front().repetition_count == 2);

    const std::array<MidiNote, 5> shared{60, 62, 64, 66, 68};
    REQUIRE(detect_real_sequences(shared, 2, 2)->empty());
    auto overlap = detect_real_sequences(shared, 2, 2, SequenceLayout::SharedEndpoint);
    REQUIRE(overlap->size() == 1);
    REQUIRE(overlap->front().start_index == 0);
    REQUIRE(overlap->front().transposition_interval == 4);
    REQUIRE(overlap->front().layout == SequenceLayout::SharedEndpoint);
    const std::array<MidiNote, 6> exact_repeat{60, 62, 64, 60, 62, 64};
    REQUIRE(detect_real_sequences(exact_repeat, 2, 2)->front().transposition_interval == 0);
}

TEST_CASE("tonal sequences carry register and fixed degree transposition",
          "[melody][core][theory-domain]") {
    // A4-B4-C5 | B4-C5-D5: generic seconds retained, chromatic sizes vary.
    const std::array<MidiNote, 6> ascending{69, 71, 72, 71, 72, 74};
    auto result = detect_tonal_sequences(ascending, 0, SCALE_MAJOR, 2, 2);
    REQUIRE(result->size() == 1);
    REQUIRE(result->front().start_index == 0);
    REQUIRE(result->front().transposition_interval == 1);
    REQUIRE_FALSE(result->front().is_real);
    REQUIRE(detect_real_sequences(ascending, 2, 2)->empty());

    const std::array<MidiNote, 6> descending{74, 72, 71, 72, 71, 69};
    REQUIRE(
        detect_tonal_sequences(descending, 0, SCALE_MAJOR, 2, 2)->front().transposition_interval ==
        -1);
    // Below B(-1), C#-D#-E | D#-E-F#: valid negative degree coordinates.
    const std::array<MidiNote, 6> low{1, 3, 4, 3, 4, 6};
    REQUIRE(detect_tonal_sequences(low, 11, SCALE_MAJOR, 2, 2)->front().transposition_interval ==
            1);
    const std::array<MidiNote, 6> octave_jump{60, 62, 64, 72, 74, 76};
    REQUIRE(
        detect_tonal_sequences(octave_jump, 0, SCALE_MAJOR, 2, 2)->front().transposition_interval ==
        7);
    const std::array<MidiNote, 9> inconsistent{60, 62, 64, 62, 64, 65, 65, 67, 69};
    REQUIRE(detect_tonal_sequences(inconsistent, 0, SCALE_MAJOR, 2, 3)->empty());
    const std::array<MidiNote, 6> chromatic{60, 61, 64, 62, 63, 65};
    REQUIRE(detect_tonal_sequences(chromatic, 0, SCALE_MAJOR, 2, 2)->empty());
    const std::array<MidiNote, 5> shared{69, 71, 72, 74, 76};
    REQUIRE(detect_tonal_sequences(shared, 0, SCALE_MAJOR, 2, 2, SequenceLayout::SharedEndpoint)
                ->front()
                .transposition_interval == 2);
}
