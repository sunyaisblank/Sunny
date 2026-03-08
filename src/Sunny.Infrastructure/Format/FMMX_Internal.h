/**
 * @file FMMX_Internal.h
 * @brief Shared MusicXML helper functions
 *
 * Internal header shared between FMMX001A (low-level reader/writer) and
 * FMMX002A (Score IR compiler). Not part of the public interface.
 *
 * Factored from FMMX001A.cpp's anonymous namespace to eliminate duplication
 * across the two MusicXML components.
 */

#pragma once

#include "../../Sunny.Core/Pitch/PTSP001A.h"
#include "../../Sunny.Core/Tensor/TNBT001A.h"

#include <array>
#include <cstdint>
#include <numeric>
#include <string>

namespace Sunny::Infrastructure::Format::MxmlInternal {

/// Letter index to MusicXML step character
constexpr std::array<char, 7> STEP_CHARS = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};

/// MusicXML step character to letter index; returns -1 for invalid input
inline int step_to_letter(char step) {
    switch (step) {
        case 'C': return 0; case 'D': return 1; case 'E': return 2;
        case 'F': return 3; case 'G': return 4; case 'A': return 5;
        case 'B': return 6;
        default: return -1;
    }
}

/// Convert line-of-fifths key centre to MusicXML <fifths> value.
/// For major keys the LoF position gives the fifths count directly;
/// minor keys add 3 (relative major is 3 LoF positions up).
inline int lof_to_fifths(Sunny::Core::SpelledPitch tonic, bool is_major) {
    int lof = Sunny::Core::line_of_fifths_position(tonic);
    if (!is_major) lof += 3;
    return lof;
}

/// Convert MusicXML <fifths> to a SpelledPitch tonic
inline Sunny::Core::SpelledPitch fifths_to_tonic(int fifths, bool is_major) {
    int lof = fifths;
    if (!is_major) lof -= 3;
    return Sunny::Core::from_line_of_fifths(lof, 4);
}

/// Convert Beat duration to MusicXML duration units given a divisions value
inline int beat_to_mxml_duration(Sunny::Core::Beat dur, int divisions) {
    auto r = dur.reduce();
    return static_cast<int>(
        static_cast<int64_t>(divisions) * 4 * r.numerator / r.denominator);
}

/// Convert MusicXML duration units to Beat
inline Sunny::Core::Beat mxml_duration_to_beat(int units, int divisions) {
    return Sunny::Core::Beat{units, 4 * static_cast<int64_t>(divisions)};
}

/// Get MusicXML <type> string for a duration
inline std::string duration_type_name(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    if (r == Sunny::Core::Beat{1, 1}) return "whole";
    if (r == Sunny::Core::Beat{1, 2}) return "half";
    if (r == Sunny::Core::Beat{1, 4}) return "quarter";
    if (r == Sunny::Core::Beat{1, 8}) return "eighth";
    if (r == Sunny::Core::Beat{1, 16}) return "16th";
    if (r == Sunny::Core::Beat{1, 32}) return "32nd";
    if (r == Sunny::Core::Beat{3, 8}) return "quarter";   // dotted quarter
    if (r == Sunny::Core::Beat{3, 4}) return "half";       // dotted half
    if (r == Sunny::Core::Beat{3, 16}) return "eighth";    // dotted eighth
    if (r == Sunny::Core::Beat{3, 2}) return "whole";      // dotted whole
    return "quarter";  // fallback
}

/// Test whether a reduced Beat represents a dotted note
inline bool is_dotted(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    return r.numerator == 3 &&
           (r.denominator == 8 || r.denominator == 4 ||
            r.denominator == 16 || r.denominator == 2);
}

/// Compute divisions (LCM of denominators) needed for a set of Beat durations.
/// Accepts a callable that iterates all durations. Template to avoid header
/// dependency on MusicXmlScore.
template <typename DurationIterator>
int compute_divisions_from(DurationIterator begin, DurationIterator end) {
    int64_t lcm_val = 1;
    for (auto it = begin; it != end; ++it) {
        auto r = it->reduce();
        int64_t beat_num = 4 * r.numerator;
        int64_t beat_den = r.denominator;
        int64_t g = std::gcd(beat_num, beat_den);
        int64_t needed_den = beat_den / g;
        lcm_val = std::lcm(lcm_val, needed_den);
    }
    return static_cast<int>(lcm_val < 1 ? 1 : lcm_val);
}

}  // namespace Sunny::Infrastructure::Format::MxmlInternal
