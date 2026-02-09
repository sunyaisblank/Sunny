/**
 * @file MLCT001A.h
 * @brief Melody analysis (contour, statistics, tendency tones, sequence detection)
 *
 * Component: MLCT001A
 * Domain: ML (Melody) | Category: CT (Contour)
 *
 * Provides melodic analysis: contour extraction and reduction (Morris),
 * motion classification, statistics (range, tessitura, histograms),
 * tendency tone rules, and real/tonal sequence detection.
 *
 * Invariants:
 * - extract_contour returns n-1 directions for n notes
 * - contour_reduction converges to a fixed point
 * - conjunct_ratio ∈ [0.0, 1.0]
 * - interval_histogram indexed as directed_interval + 12
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Types
// =============================================================================

enum class ContourDirection : int8_t {
    Descending = -1,
    Stationary = 0,
    Ascending = +1
};

enum class MotionType { Conjunct, Disjunct };

struct MelodyStatistics {
    int range;
    std::pair<MidiNote, MidiNote> tessitura;    ///< 10th-90th percentile
    double mean_pitch;
    std::array<int, 25> interval_histogram;     ///< directed [-12..+12] -> index 0..24
    std::array<int, 12> pitch_class_histogram;
    double conjunct_ratio;
};

struct TendencyTone {
    int scale_degree;             ///< 0-based
    ContourDirection direction;
    int resolution_degree;        ///< 0-based
};

struct DetectedSequence {
    std::size_t start_index;
    std::size_t pattern_length;   ///< in intervals
    int repetition_count;
    int transposition_interval;
    bool is_real;                 ///< real (chromatic) vs tonal (diatonic)
};

// =============================================================================
// Contour
// =============================================================================

/**
 * @brief Extract contour direction between successive notes
 *
 * @param notes Melody (≥2 notes)
 * @return n-1 ContourDirection values, or InvalidMelody
 */
[[nodiscard]] Result<std::vector<ContourDirection>> extract_contour(
    std::span<const MidiNote> notes
);

/**
 * @brief Morris contour reduction
 *
 * Repeatedly removes interior notes that are neither local maxima nor minima,
 * keeping first and last, until the sequence stabilises.
 *
 * @param notes Melody (≥2 notes)
 * @return Reduced melody or InvalidMelody
 */
[[nodiscard]] Result<std::vector<MidiNote>> contour_reduction(
    std::span<const MidiNote> notes
);

// =============================================================================
// Motion
// =============================================================================

/**
 * @brief Classify each interval as conjunct (≤2 semitones) or disjunct (>2)
 */
[[nodiscard]] Result<std::vector<MotionType>> classify_motion(
    std::span<const MidiNote> notes
);

/**
 * @brief Check if the melody is predominantly conjunct
 *
 * @param threshold Proportion of conjunct intervals required (default 0.6)
 */
[[nodiscard]] Result<bool> is_predominantly_conjunct(
    std::span<const MidiNote> notes,
    double threshold = 0.6
);

// =============================================================================
// Statistics
// =============================================================================

/**
 * @brief Compute melody statistics
 */
[[nodiscard]] Result<MelodyStatistics> compute_melody_statistics(
    std::span<const MidiNote> notes
);

// =============================================================================
// Tendency Tones
// =============================================================================

/**
 * @brief Standard tendency tone rules for a scale
 *
 * Major: degree 6 (leading tone) -> up to 0 (tonic).
 * Minor: degree 6 (leading tone if raised) -> up to 0.
 */
[[nodiscard]] std::vector<TendencyTone> standard_tendency_tones(
    std::span<const Interval> intervals
);

/**
 * @brief Check if a melodic motion follows a tendency rule
 */
[[nodiscard]] bool follows_tendency(
    MidiNote from,
    MidiNote to,
    PitchClass key_root,
    std::span<const Interval> intervals
);

// =============================================================================
// Sequence Detection
// =============================================================================

/**
 * @brief Detect real (chromatic) sequences
 *
 * A real sequence is a pattern of directed intervals repeated at a
 * fixed transposition level.
 */
[[nodiscard]] Result<std::vector<DetectedSequence>> detect_real_sequences(
    std::span<const MidiNote> notes,
    int min_length = 3,
    int min_reps = 2
);

/**
 * @brief Detect tonal sequences within a scale
 *
 * A tonal sequence preserves diatonic interval patterns (in scale degrees)
 * rather than chromatic intervals.
 */
[[nodiscard]] Result<std::vector<DetectedSequence>> detect_tonal_sequences(
    std::span<const MidiNote> notes,
    PitchClass key_root,
    std::span<const Interval> intervals,
    int min_length = 3,
    int min_reps = 2
);

}  // namespace Sunny::Core
