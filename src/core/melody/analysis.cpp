/**
 * @file analysis.cpp
 * @brief Melody analysis implementation
 *
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <sunny/core/melody/analysis.hpp>

namespace sunny::core {

Result<std::vector<ContourDirection>> extract_contour(std::span<const MidiNote> notes) {
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

Result<std::vector<MidiNote>> contour_reduction(std::span<const MidiNote> notes) {
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

Result<std::vector<MotionType>> classify_motion(std::span<const MidiNote> notes) {
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

Result<bool> is_predominantly_conjunct(std::span<const MidiNote> notes, double threshold) {
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

Result<MelodyStatistics> compute_melody_statistics(std::span<const MidiNote> notes) {
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
    for (auto note : notes)
        sum += static_cast<double>(note);
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

std::vector<TendencyTone> standard_tendency_tones(std::span<const Interval> intervals) {
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

bool follows_tendency(MidiNote from,
                      MidiNote to,
                      PitchClass key_root,
                      std::span<const Interval> intervals) {
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

// A scale-degree coordinate carries its register, including degrees below
// a non-C root. Missing coordinates denote chromatic notes, not a valid -1.
std::optional<int>
note_to_degree(MidiNote note, PitchClass root, std::span<const Interval> intervals) {
    int offset = static_cast<int>(note) - static_cast<int>(root);
    int octave = offset / 12;
    int pitch_offset = offset % 12;
    if (pitch_offset < 0) {
        pitch_offset += 12;
        --octave;
    }
    for (std::size_t d = 0; d < intervals.size(); ++d) {
        if (intervals[d] == pitch_offset) {
            return octave * static_cast<int>(intervals.size()) + static_cast<int>(d);
        }
    }
    return std::nullopt;
}

bool valid_sequence_controls(std::size_t note_count,
                             int min_length,
                             int min_reps,
                             SequenceLayout layout) {
    return note_count >= 2 &&
           note_count <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
           min_length >= 1 && min_reps >= 2 &&
           (layout == SequenceLayout::Adjacent || layout == SequenceLayout::SharedEndpoint);
}

std::vector<DetectedSequence> detect_sequences(std::span<const std::optional<int>> coordinates,
                                               int min_length,
                                               int min_reps,
                                               SequenceLayout layout,
                                               bool is_real) {
    std::vector<DetectedSequence> results;
    const auto n = coordinates.size();
    const auto required_reps = static_cast<std::size_t>(min_reps);
    const auto max_length = layout == SequenceLayout::Adjacent
                                ? (n / required_reps == 0 ? 0 : n / required_reps - 1)
                                : (n - 1) / required_reps;
    for (auto length = static_cast<std::size_t>(min_length); length <= max_length; ++length) {
        const auto stride = length + (layout == SequenceLayout::Adjacent ? 1 : 0);
        for (std::size_t start = 0; start + length < n; ++start) {
            if (!coordinates[start]) continue;
            bool valid_pattern = true;
            for (std::size_t k = 1; k <= length; ++k) {
                if (!coordinates[start + k]) {
                    valid_pattern = false;
                    break;
                }
            }
            if (!valid_pattern) continue;
            auto next = start + stride;
            if (next + length >= n || !coordinates[next]) continue;
            const int increment = *coordinates[next] - *coordinates[start];
            int reps = 1;
            while (next + length < n) {
                // int64 avoids an overflowing increment*reps even for very
                // long inputs. Admitted MIDI/scale coordinates remain small.
                const auto expected_shift = static_cast<std::int64_t>(increment) * reps;
                bool match = true;
                for (std::size_t k = 0; k <= length; ++k) {
                    if (!coordinates[next + k] ||
                        *coordinates[next + k] - *coordinates[start + k] != expected_shift) {
                        match = false;
                        break;
                    }
                }
                if (!match) break;
                ++reps;
                next += stride;
            }
            if (reps >= min_reps) {
                results.push_back({start, length, reps, increment, is_real, layout});
            }
        }
    }
    return results;
}

} // namespace

Result<std::vector<DetectedSequence>> detect_real_sequences(std::span<const MidiNote> notes,
                                                            int min_length,
                                                            int min_reps,
                                                            SequenceLayout layout) {
    if (!valid_sequence_controls(notes.size(), min_length, min_reps, layout)) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }
    std::vector<std::optional<int>> coordinates;
    coordinates.reserve(notes.size());
    for (auto note : notes)
        coordinates.push_back(static_cast<int>(note));
    return detect_sequences(coordinates, min_length, min_reps, layout, true);
}

Result<std::vector<DetectedSequence>> detect_tonal_sequences(std::span<const MidiNote> notes,
                                                             PitchClass key_root,
                                                             std::span<const Interval> intervals,
                                                             int min_length,
                                                             int min_reps,
                                                             SequenceLayout layout) {
    if (!valid_sequence_controls(notes.size(), min_length, min_reps, layout)) {
        return std::unexpected(ErrorCode::InvalidMelody);
    }
    if (intervals.empty() || intervals.size() > 12 || intervals.front() != 0 ||
        intervals.back() > 11 ||
        std::adjacent_find(intervals.begin(), intervals.end(), std::greater_equal<>{}) !=
            intervals.end()) {
        return std::unexpected(ErrorCode::InvalidScaleName);
    }
    std::vector<std::optional<int>> coordinates;
    coordinates.reserve(notes.size());
    for (auto note : notes)
        coordinates.push_back(note_to_degree(note, key_root, intervals));
    return detect_sequences(coordinates, min_length, min_reps, layout, false);
}

} // namespace sunny::core
