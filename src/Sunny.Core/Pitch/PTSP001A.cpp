/**
 * @file PTSP001A.cpp
 * @brief Spelled pitch non-constexpr implementations
 *
 * Component: PTSP001A
 */

#include "PTSP001A.h"

#include <cctype>

namespace Sunny::Core {

Result<std::string> to_spn(SpelledPitch sp) {
    if (sp.letter >= 7) {
        return std::unexpected(ErrorCode::InvalidLetterName);
    }

    constexpr std::array<char, 7> LETTER_CHAR = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};

    std::string result;
    result += LETTER_CHAR[sp.letter];

    if (sp.accidental > 0) {
        for (int i = 0; i < sp.accidental; ++i) {
            result += '#';
        }
    } else if (sp.accidental < 0) {
        for (int i = 0; i < -sp.accidental; ++i) {
            result += 'b';
        }
    }

    result += std::to_string(static_cast<int>(sp.octave));
    return result;
}

Result<SpelledPitch> from_spn(std::string_view s) {
    if (s.empty()) {
        return std::unexpected(ErrorCode::InvalidSpelledPitch);
    }

    // Parse letter
    uint8_t letter;
    switch (std::toupper(static_cast<unsigned char>(s[0]))) {
        case 'C': letter = 0; break;
        case 'D': letter = 1; break;
        case 'E': letter = 2; break;
        case 'F': letter = 3; break;
        case 'G': letter = 4; break;
        case 'A': letter = 5; break;
        case 'B': letter = 6; break;
        default:
            return std::unexpected(ErrorCode::InvalidLetterName);
    }

    // Parse accidentals
    int accidental_acc = 0;
    std::size_t pos = 1;
    while (pos < s.size()) {
        if (s[pos] == '#') {
            accidental_acc += 1;
            ++pos;
        } else if (s[pos] == 'b') {
            accidental_acc -= 1;
            ++pos;
        } else if (s[pos] == 'x') {
            accidental_acc += 2;
            ++pos;
        } else {
            break;
        }
        if (accidental_acc < -127 || accidental_acc > 127) {
            return std::unexpected(ErrorCode::InvalidSpelledPitch);
        }
    }
    int8_t accidental = static_cast<int8_t>(accidental_acc);

    // Parse octave (remaining characters must form an integer)
    if (pos >= s.size()) {
        return std::unexpected(ErrorCode::InvalidSpelledPitch);
    }

    // Handle negative octave
    bool neg_octave = false;
    if (s[pos] == '-') {
        neg_octave = true;
        ++pos;
    }

    if (pos >= s.size()) {
        return std::unexpected(ErrorCode::InvalidSpelledPitch);
    }

    int octave_val = 0;
    bool has_digit = false;
    while (pos < s.size()) {
        if (s[pos] >= '0' && s[pos] <= '9') {
            octave_val = octave_val * 10 + (s[pos] - '0');
            has_digit = true;
            ++pos;
        } else {
            return std::unexpected(ErrorCode::InvalidSpelledPitch);
        }
    }

    if (!has_digit) {
        return std::unexpected(ErrorCode::InvalidSpelledPitch);
    }

    if (neg_octave) {
        octave_val = -octave_val;
    }

    return SpelledPitch{letter, accidental, static_cast<int8_t>(octave_val)};
}

SpelledPitch default_spelling(PitchClass target_pc, int key_lof, int8_t octave) {
    // Search the LoF neighbourhood centred on key_lof for a spelling that
    // matches target_pc. Prefer positions closest to the key centre.
    // Search radius of 6 covers all 12 pitch classes.
    int best_q = key_lof;
    int best_dist = 100;

    // Search outward from key centre. On equal distance, prefer the
    // sharp (positive LoF) direction by checking positive offsets first.
    for (int dist = 0; dist <= 6; ++dist) {
        for (int sign : {1, -1}) {
            if (dist == 0 && sign == -1) continue;
            int offset = dist * sign;
            int q = key_lof + offset;
            SpelledPitch candidate = from_line_of_fifths(q, octave);
            if (pc(candidate) == target_pc && dist < best_dist) {
                best_dist = dist;
                best_q = q;
                break;
            }
        }
        if (best_dist <= dist) break;
    }

    return from_line_of_fifths(best_q, octave);
}

}  // namespace Sunny::Core
