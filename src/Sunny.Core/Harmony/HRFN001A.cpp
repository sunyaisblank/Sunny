/**
 * @file HRFN001A.cpp
 * @brief Functional harmony analysis implementation
 *
 * Component: HRFN001A
 */

#include "HRFN001A.h"
#include "../Pitch/PTPC001A.h"

#include <algorithm>
#include <array>

namespace Sunny::Core {

namespace {

// Functional roles by scale degree in major key
// Degree 0=I, 1=ii, 2=iii, 3=IV, 4=V, 5=vi, 6=vii
constexpr std::array<HarmonicFunction, 7> MAJOR_FUNCTIONS = {
    HarmonicFunction::Tonic,       // I
    HarmonicFunction::Subdominant, // ii
    HarmonicFunction::Tonic,       // iii (tonic substitute)
    HarmonicFunction::Subdominant, // IV
    HarmonicFunction::Dominant,    // V
    HarmonicFunction::Tonic,       // vi (tonic substitute)
    HarmonicFunction::Dominant,    // vii° (dominant substitute)
};

// Functional roles in minor key
constexpr std::array<HarmonicFunction, 7> MINOR_FUNCTIONS = {
    HarmonicFunction::Tonic,       // i
    HarmonicFunction::Subdominant, // ii°
    HarmonicFunction::Tonic,       // III
    HarmonicFunction::Subdominant, // iv
    HarmonicFunction::Dominant,    // v (or V with raised 7th)
    HarmonicFunction::Subdominant, // VI
    HarmonicFunction::Dominant,    // vii°
};

// Major scale degrees (semitones from root)
constexpr std::array<int, 7> MAJOR_DEGREES = {0, 2, 4, 5, 7, 9, 11};

// Natural minor scale degrees
constexpr std::array<int, 7> MINOR_DEGREES = {0, 2, 3, 5, 7, 8, 10};

// Find scale degree from pitch class
int find_degree(PitchClass pc, PitchClass root, const std::array<int, 7>& degrees) {
    int interval = (pc - root + 12) % 12;
    for (int i = 0; i < 7; ++i) {
        if (degrees[i] == interval) {
            return i;
        }
    }
    // Chromatic note - find closest
    int best_deg = 0;
    int best_dist = 12;
    for (int i = 0; i < 7; ++i) {
        int dist = std::abs(degrees[i] - interval);
        dist = std::min(dist, 12 - dist);
        if (dist < best_dist) {
            best_dist = dist;
            best_deg = i;
        }
    }
    return best_deg;
}

// Determine chord quality from intervals
std::string determine_quality(const std::vector<int>& intervals) {
    if (intervals.size() < 2) return "power";

    bool has_m3 = std::find(intervals.begin(), intervals.end(), 3) != intervals.end();
    bool has_M3 = std::find(intervals.begin(), intervals.end(), 4) != intervals.end();
    bool has_d5 = std::find(intervals.begin(), intervals.end(), 6) != intervals.end();
    bool has_P5 = std::find(intervals.begin(), intervals.end(), 7) != intervals.end();
    bool has_A5 = std::find(intervals.begin(), intervals.end(), 8) != intervals.end();
    bool has_m7 = std::find(intervals.begin(), intervals.end(), 10) != intervals.end();
    bool has_M7 = std::find(intervals.begin(), intervals.end(), 11) != intervals.end();

    if (has_M3 && has_A5) return "augmented";
    if (has_m3 && has_d5) {
        if (has_m7) return "half-diminished";
        return "diminished";
    }
    if (has_M3 && has_P5) {
        if (has_M7) return "major7";
        if (has_m7) return "dominant7";
        return "major";
    }
    if (has_m3 && has_P5) {
        if (has_m7) return "minor7";
        if (has_M7) return "minorMaj7";
        return "minor";
    }
    if (has_P5) return "sus";

    return "unknown";
}

}  // namespace

ChordAnalysis analyze_chord_function(
    const PitchClassSet& chord_pcs,
    PitchClass key_root,
    bool is_minor
) {
    ChordAnalysis result{};

    if (chord_pcs.empty()) {
        result.root = key_root;
        result.quality = "unknown";
        result.function = HarmonicFunction::Tonic;
        result.numeral = "?";
        result.degree = 1;
        return result;
    }

    // Find chord root by detecting tertian structure (stacked thirds)
    // Try each pitch class as potential root and score by how tertian the result is
    PitchClass chord_root = *chord_pcs.begin();
    int best_score = -1;

    for (PitchClass candidate : chord_pcs) {
        int score = 0;
        for (PitchClass pc : chord_pcs) {
            int interval = (pc - candidate + 12) % 12;
            // Tertian intervals: minor 3rd (3), major 3rd (4), perfect 5th (7),
            // minor 7th (10), major 7th (11), root (0)
            if (interval == 0 || interval == 3 || interval == 4 ||
                interval == 7 || interval == 10 || interval == 11) {
                score += 2;
            }
            // Diminished 5th (6) and augmented 5th (8) also tertian
            if (interval == 6 || interval == 8) {
                score += 1;
            }
        }
        if (score > best_score) {
            best_score = score;
            chord_root = candidate;
        }
    }
    result.root = chord_root;

    // Calculate intervals from root
    std::vector<int> intervals;
    for (auto pc : chord_pcs) {
        int interval = (pc - chord_root + 12) % 12;
        intervals.push_back(interval);
    }
    std::sort(intervals.begin(), intervals.end());

    // Determine quality
    result.quality = determine_quality(intervals);

    // Find scale degree
    const auto& degrees = is_minor ? MINOR_DEGREES : MAJOR_DEGREES;
    const auto& functions = is_minor ? MINOR_FUNCTIONS : MAJOR_FUNCTIONS;

    int degree = find_degree(chord_root, key_root, degrees);
    result.degree = degree + 1;  // 1-indexed
    result.function = functions[degree];

    // Generate Roman numeral
    bool is_major_chord = (result.quality == "major" || result.quality == "major7" ||
                           result.quality == "dominant7" || result.quality == "augmented");
    result.numeral = degree_to_numeral(degree, is_major_chord);

    // Add quality modifiers
    if (result.quality == "diminished") result.numeral += "°";
    else if (result.quality == "augmented") result.numeral += "+";
    else if (result.quality == "half-diminished") result.numeral += "ø";
    else if (result.quality.find("7") != std::string::npos) result.numeral += "7";

    return result;
}

std::string degree_to_numeral(int degree, bool is_major) {
    static constexpr std::array<std::string_view, 7> UPPER = {
        "I", "II", "III", "IV", "V", "VI", "VII"
    };
    static constexpr std::array<std::string_view, 7> LOWER = {
        "i", "ii", "iii", "iv", "v", "vi", "vii"
    };

    if (degree < 0 || degree >= 7) {
        return "?";
    }

    return std::string(is_major ? UPPER[degree] : LOWER[degree]);
}

}  // namespace Sunny::Core
