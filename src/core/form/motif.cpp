/**
 * @file motif.cpp
 * @brief Motivic Analysis and Transformations implementation
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sunny/core/form/motif.hpp>

namespace sunny::core {

// =============================================================================
// Pitch Operations
// =============================================================================

Result<std::vector<MidiNote>> motif_transpose(std::span<const MidiNote> pitches, int semitones) {
    if (!pitches.empty() && (semitones < -127 || semitones > 127)) {
        return std::unexpected(ErrorCode::InvalidMidiNote);
    }
    std::vector<MidiNote> result;
    result.reserve(pitches.size());
    for (auto p : pitches) {
        auto transposed = MidiNote::from_int(static_cast<int>(p) + semitones);
        if (!transposed) return std::unexpected(transposed.error());
        result.push_back(*transposed);
    }
    return result;
}

Result<std::vector<MidiNote>> motif_invert(std::span<const MidiNote> pitches) {
    if (pitches.empty()) return std::vector<MidiNote>{};

    std::vector<MidiNote> result;
    result.reserve(pitches.size());
    MidiNote axis = pitches[0];
    for (auto p : pitches) {
        int interval = static_cast<int>(p) - static_cast<int>(axis);
        auto inverted = MidiNote::from_int(static_cast<int>(axis) - interval);
        if (!inverted) return std::unexpected(inverted.error());
        result.push_back(*inverted);
    }
    return result;
}

std::vector<MidiNote> motif_retrograde(std::span<const MidiNote> pitches) {
    std::vector<MidiNote> result(pitches.begin(), pitches.end());
    std::reverse(result.begin(), result.end());
    return result;
}

Result<std::vector<MidiNote>> motif_retrograde_inversion(std::span<const MidiNote> pitches) {
    auto retro = motif_retrograde(pitches);
    return motif_invert(retro);
}

// =============================================================================
// Duration Operations
// =============================================================================

std::vector<Beat> motif_augment(std::span<const Beat> durations, Beat factor) {
    std::vector<Beat> result;
    result.reserve(durations.size());
    for (const auto& d : durations) {
        result.push_back(d * factor);
    }
    return result;
}

std::vector<Beat> motif_diminish(std::span<const Beat> durations, Beat factor) {
    std::vector<Beat> result;
    result.reserve(durations.size());
    for (const auto& d : durations) {
        result.push_back(d / factor);
    }
    return result;
}

// =============================================================================
// Similarity and Detection
// =============================================================================

namespace {

// Extract interval sequence (successive differences)
std::vector<int> interval_sequence(std::span<const MidiNote> pitches) {
    std::vector<int> intervals;
    if (pitches.size() < 2) return intervals;
    intervals.reserve(pitches.size() - 1);
    for (std::size_t i = 1; i < pitches.size(); ++i) {
        intervals.push_back(static_cast<int>(pitches[i]) - static_cast<int>(pitches[i - 1]));
    }
    return intervals;
}

} // namespace

double motif_interval_similarity(std::span<const MidiNote> a, std::span<const MidiNote> b) {
    auto iv_a = interval_sequence(a);
    auto iv_b = interval_sequence(b);

    if (iv_a.empty() && iv_b.empty()) return 1.0;
    if (iv_a.empty() || iv_b.empty()) return 0.0;

    std::size_t compare_len = std::min(iv_a.size(), iv_b.size());
    int matches = 0;
    for (std::size_t i = 0; i < compare_len; ++i) {
        if (iv_a[i] == iv_b[i]) ++matches;
    }

    // Penalise length difference
    std::size_t max_len = std::max(iv_a.size(), iv_b.size());
    return static_cast<double>(matches) / static_cast<double>(max_len);
}

MotivicTransform classify_transformation(std::span<const MidiNote> original,
                                         std::span<const MidiNote> transformed) {
    if (original.empty() || transformed.empty()) return MotivicTransform::Unknown;
    if (original.size() != transformed.size()) {
        // Could be fragmentation or extension
        if (transformed.size() < original.size()) {
            // Check if transformed is a prefix or suffix of original
            bool is_prefix = true;
            for (std::size_t i = 0; i < transformed.size(); ++i) {
                if (transformed[i] != original[i]) {
                    is_prefix = false;
                    break;
                }
            }
            if (is_prefix) return MotivicTransform::Fragmentation;

            bool is_suffix = true;
            std::size_t offset = original.size() - transformed.size();
            for (std::size_t i = 0; i < transformed.size(); ++i) {
                if (transformed[i] != original[offset + i]) {
                    is_suffix = false;
                    break;
                }
            }
            if (is_suffix) return MotivicTransform::Fragmentation;
        }
        return MotivicTransform::Unknown;
    }

    // Check exact repetition
    bool is_repetition = true;
    for (std::size_t i = 0; i < original.size(); ++i) {
        if (original[i] != transformed[i]) {
            is_repetition = false;
            break;
        }
    }
    if (is_repetition) return MotivicTransform::Repetition;

    // Check transposition (constant difference)
    int diff = static_cast<int>(transformed[0]) - static_cast<int>(original[0]);
    bool is_transposition = true;
    for (std::size_t i = 1; i < original.size(); ++i) {
        if (static_cast<int>(transformed[i]) - static_cast<int>(original[i]) != diff) {
            is_transposition = false;
            break;
        }
    }
    if (is_transposition) return MotivicTransform::Transposition;

    // Classification compares mathematical pitch relations. An intermediate
    // first-note-axis inversion need not itself fit the MIDI generation domain.
    int inv_sum = static_cast<int>(transformed[0]) + static_cast<int>(original[0]);
    bool is_inversion = true;
    for (std::size_t i = 0; i < original.size(); ++i) {
        if (static_cast<int>(transformed[i]) + static_cast<int>(original[i]) != inv_sum) {
            is_inversion = false;
            break;
        }
    }
    if (is_inversion) return MotivicTransform::Inversion;

    // Check retrograde
    int retro_diff = static_cast<int>(transformed[0]) - static_cast<int>(original.back());
    bool is_retrograde = true;
    for (std::size_t i = 0; i < original.size(); ++i) {
        if (static_cast<int>(transformed[i]) -
                static_cast<int>(original[original.size() - 1 - i]) !=
            retro_diff) {
            is_retrograde = false;
            break;
        }
    }
    if (is_retrograde) return MotivicTransform::Retrograde;

    // Check retrograde-inversion
    int ri_sum = static_cast<int>(transformed[0]) + static_cast<int>(original.back());
    bool is_ri = true;
    for (std::size_t i = 0; i < original.size(); ++i) {
        if (static_cast<int>(transformed[i]) +
                static_cast<int>(original[original.size() - 1 - i]) !=
            ri_sum) {
            is_ri = false;
            break;
        }
    }
    if (is_ri) return MotivicTransform::RetrogradeInversion;

    return MotivicTransform::Unknown;
}

std::vector<MidiNote>
motif_fragment(std::span<const MidiNote> pitches, std::size_t start, std::size_t length) {
    if (start >= pitches.size()) return {};
    std::size_t actual_len = std::min(length, pitches.size() - start);
    return {pitches.begin() + static_cast<std::ptrdiff_t>(start),
            pitches.begin() + static_cast<std::ptrdiff_t>(start + actual_len)};
}

} // namespace sunny::core
