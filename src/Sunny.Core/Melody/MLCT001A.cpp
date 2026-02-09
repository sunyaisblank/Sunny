/**
 * @file MLCT001A.cpp
 * @brief Melody analysis implementation
 *
 * Component: MLCT001A
 */

#include "MLCT001A.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Sunny::Core {

Result<std::vector<ContourDirection>> extract_contour(
    std::span<const MidiNote> notes
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    std::vector<ContourDirection> result;
    result.reserve(notes.size() - 1);

    for (std::size_t i = 1; i < notes.size(); ++i) {
        if (notes[i] > notes[i - 1]) {
            result.push_back(ContourDirection::Ascending);
        } else if (notes[i] < notes[i - 1]) {
            result.push_back(ContourDirection::Descending);
        } else {
            result.push_back(ContourDirection::Stationary);
        }
    }

    return result;
}

Result<std::vector<MidiNote>> contour_reduction(
    std::span<const MidiNote> notes
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    std::vector<MidiNote> current(notes.begin(), notes.end());

    // Remove consecutive duplicates first: keep only the first of each run
    {
        std::vector<MidiNote> deduped;
        deduped.push_back(current[0]);
        for (std::size_t i = 1; i < current.size(); ++i) {
            if (current[i] != current[i - 1]) {
                deduped.push_back(current[i]);
            }
        }
        current = std::move(deduped);
    }

    if (current.size() <= 2) return current;

    // Iterate until fixed point
    for (;;) {
        std::vector<MidiNote> reduced;
        reduced.push_back(current.front());

        for (std::size_t i = 1; i + 1 < current.size(); ++i) {
            bool is_max = current[i] > current[i - 1] && current[i] > current[i + 1];
            bool is_min = current[i] < current[i - 1] && current[i] < current[i + 1];
            if (is_max || is_min) {
                reduced.push_back(current[i]);
            }
        }

        reduced.push_back(current.back());

        if (reduced.size() == current.size()) break;
        current = std::move(reduced);

        if (current.size() <= 2) break;
    }

    return current;
}

Result<std::vector<MotionType>> classify_motion(
    std::span<const MidiNote> notes
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    std::vector<MotionType> result;
    result.reserve(notes.size() - 1);

    for (std::size_t i = 1; i < notes.size(); ++i) {
        int diff = std::abs(static_cast<int>(notes[i]) - static_cast<int>(notes[i - 1]));
        result.push_back(diff <= 2 ? MotionType::Conjunct : MotionType::Disjunct);
    }

    return result;
}

Result<bool> is_predominantly_conjunct(
    std::span<const MidiNote> notes,
    double threshold
) {
    auto motion = classify_motion(notes);
    if (!motion) return std::unexpected(motion.error());

    if (motion->empty()) return true;

    int conjunct = 0;
    for (auto m : *motion) {
        if (m == MotionType::Conjunct) ++conjunct;
    }

    double ratio = static_cast<double>(conjunct) / static_cast<double>(motion->size());
    return ratio >= threshold;
}

Result<MelodyStatistics> compute_melody_statistics(
    std::span<const MidiNote> notes
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    MelodyStatistics stats{};

    // Range
    auto [min_it, max_it] = std::minmax_element(notes.begin(), notes.end());
    stats.range = static_cast<int>(*max_it) - static_cast<int>(*min_it);

    // Tessitura (10th-90th percentile)
    std::vector<MidiNote> sorted(notes.begin(), notes.end());
    std::sort(sorted.begin(), sorted.end());
    std::size_t n = sorted.size();
    std::size_t idx10 = static_cast<std::size_t>(0.1 * static_cast<double>(n));
    std::size_t idx90 = static_cast<std::size_t>(0.9 * static_cast<double>(n));
    if (idx90 >= n) idx90 = n - 1;
    stats.tessitura = {sorted[idx10], sorted[idx90]};

    // Mean pitch
    double sum = 0.0;
    for (auto note : notes) sum += static_cast<double>(note);
    stats.mean_pitch = sum / static_cast<double>(n);

    // Interval histogram (directed, clamped to [-12, +12])
    stats.interval_histogram.fill(0);
    for (std::size_t i = 1; i < notes.size(); ++i) {
        int interval = static_cast<int>(notes[i]) - static_cast<int>(notes[i - 1]);
        int clamped = std::clamp(interval, -12, 12);
        stats.interval_histogram[clamped + 12]++;
    }

    // Pitch class histogram
    stats.pitch_class_histogram.fill(0);
    for (auto note : notes) {
        stats.pitch_class_histogram[note % 12]++;
    }

    // Conjunct ratio
    auto motion = classify_motion(notes);
    if (motion && !motion->empty()) {
        int conjunct = 0;
        for (auto m : *motion) {
            if (m == MotionType::Conjunct) ++conjunct;
        }
        stats.conjunct_ratio = static_cast<double>(conjunct) / static_cast<double>(motion->size());
    } else {
        stats.conjunct_ratio = 0.0;
    }

    return stats;
}

std::vector<TendencyTone> standard_tendency_tones(
    std::span<const Interval> intervals
) {
    std::vector<TendencyTone> tones;

    if (intervals.size() != 7) return tones;

    // Leading tone (degree 6) -> tonic (degree 0) if semitone below
    if (intervals[6] == 11) {
        tones.push_back({6, ContourDirection::Ascending, 0});
    }

    // Subdominant (degree 3) -> mediant (degree 2)
    tones.push_back({3, ContourDirection::Descending, 2});

    return tones;
}

bool follows_tendency(
    MidiNote from,
    MidiNote to,
    PitchClass key_root,
    std::span<const Interval> intervals
) {
    if (intervals.size() != 7) return false;

    PitchClass from_pc = pitch_class(from);
    PitchClass to_pc = pitch_class(to);

    // Find scale degree of 'from'
    int from_degree = -1;
    for (int d = 0; d < 7; ++d) {
        if (transpose(key_root, intervals[d]) == from_pc) {
            from_degree = d;
            break;
        }
    }
    if (from_degree == -1) return false;

    // Find scale degree of 'to'
    int to_degree = -1;
    for (int d = 0; d < 7; ++d) {
        if (transpose(key_root, intervals[d]) == to_pc) {
            to_degree = d;
            break;
        }
    }
    if (to_degree == -1) return false;

    auto tendencies = standard_tendency_tones(intervals);
    for (auto& tt : tendencies) {
        if (tt.scale_degree == from_degree && tt.resolution_degree == to_degree) {
            // Check direction
            if (tt.direction == ContourDirection::Ascending && to > from) return true;
            if (tt.direction == ContourDirection::Descending && to < from) return true;
        }
    }

    return false;
}

namespace {

std::vector<int> extract_directed_intervals(std::span<const MidiNote> notes) {
    std::vector<int> intervals;
    intervals.reserve(notes.size() - 1);
    for (std::size_t i = 1; i < notes.size(); ++i) {
        intervals.push_back(static_cast<int>(notes[i]) - static_cast<int>(notes[i - 1]));
    }
    return intervals;
}

// Map a MIDI note to its scale degree index (-1 if not in scale)
int note_to_degree(MidiNote note, PitchClass root, std::span<const Interval> intervals) {
    PitchClass pc = pitch_class(note);
    for (int d = 0; d < static_cast<int>(intervals.size()); ++d) {
        if (transpose(root, intervals[d]) == pc) return d;
    }
    return -1;
}

}  // namespace

Result<std::vector<DetectedSequence>> detect_real_sequences(
    std::span<const MidiNote> notes,
    int min_length,
    int min_reps
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    auto intervals = extract_directed_intervals(notes);
    std::vector<DetectedSequence> results;

    int n = static_cast<int>(intervals.size());

    for (int len = min_length; len <= n / min_reps; ++len) {
        for (int start = 0; start + len <= n; ++start) {
            // Extract pattern
            std::span<const int> pattern(intervals.data() + start, len);

            int reps = 1;
            int transposition = 0;

            // Check for subsequent repetitions at a fixed transposition
            int next = start + len;
            if (next + len > n) continue;

            // Determine transposition from first note difference
            transposition = static_cast<int>(notes[next]) - static_cast<int>(notes[start]);

            while (next + len <= n) {
                bool match = true;
                // Check interval pattern matches
                for (int k = 0; k < len; ++k) {
                    if (intervals[next + k] != pattern[k]) {
                        match = false;
                        break;
                    }
                }
                // Check transposition is consistent
                int actual_trans = static_cast<int>(notes[next]) - static_cast<int>(notes[start]);
                if (!match || actual_trans != transposition * reps) {
                    break;
                }
                ++reps;
                next += len;
            }

            if (reps >= min_reps) {
                // Check we haven't already recorded a sequence at this start with longer pattern
                bool duplicate = false;
                for (auto& existing : results) {
                    if (existing.start_index == static_cast<std::size_t>(start) &&
                        existing.pattern_length >= static_cast<std::size_t>(len)) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    results.push_back({
                        static_cast<std::size_t>(start),
                        static_cast<std::size_t>(len),
                        reps,
                        transposition,
                        true
                    });
                }
            }
        }
    }

    return results;
}

Result<std::vector<DetectedSequence>> detect_tonal_sequences(
    std::span<const MidiNote> notes,
    PitchClass key_root,
    std::span<const Interval> intervals,
    int min_length,
    int min_reps
) {
    if (notes.size() < 2) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }

    // Map notes to scale degrees
    std::vector<int> degrees;
    degrees.reserve(notes.size());
    for (auto note : notes) {
        degrees.push_back(note_to_degree(note, key_root, intervals));
    }

    // Extract diatonic interval pattern (degree differences)
    std::vector<int> degree_intervals;
    degree_intervals.reserve(degrees.size() - 1);
    for (std::size_t i = 1; i < degrees.size(); ++i) {
        if (degrees[i] == -1 || degrees[i - 1] == -1) {
            degree_intervals.push_back(999);  // non-diatonic marker
        } else {
            degree_intervals.push_back(degrees[i] - degrees[i - 1]);
        }
    }

    std::vector<DetectedSequence> results;
    int n = static_cast<int>(degree_intervals.size());

    for (int len = min_length; len <= n / min_reps; ++len) {
        for (int start = 0; start + len <= n; ++start) {
            // Check pattern has no non-diatonic markers
            bool has_invalid = false;
            for (int k = 0; k < len; ++k) {
                if (degree_intervals[start + k] == 999) {
                    has_invalid = true;
                    break;
                }
            }
            if (has_invalid) continue;

            int reps = 1;
            int next = start + len;

            while (next + len <= n) {
                bool match = true;
                for (int k = 0; k < len; ++k) {
                    if (degree_intervals[next + k] != degree_intervals[start + k]) {
                        match = false;
                        break;
                    }
                }
                if (!match) break;
                ++reps;
                next += len;
            }

            if (reps >= min_reps) {
                int transposition = (degrees[start + len] != -1 && degrees[start] != -1)
                    ? degrees[start + len] - degrees[start]
                    : 0;

                bool duplicate = false;
                for (auto& existing : results) {
                    if (existing.start_index == static_cast<std::size_t>(start) &&
                        existing.pattern_length >= static_cast<std::size_t>(len)) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    results.push_back({
                        static_cast<std::size_t>(start),
                        static_cast<std::size_t>(len),
                        reps,
                        transposition,
                        false
                    });
                }
            }
        }
    }

    return results;
}

}  // namespace Sunny::Core
