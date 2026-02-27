/**
 * @file PTSP001A.h
 * @brief Spelled pitch type and operations
 *
 * Component: PTSP001A
 * Domain: PT (Pitch) | Category: SP (Spelled Pitch)
 *
 * Represents pitch with enharmonic spelling preserved (C# ≠ Db).
 * Uses the line of fifths (LoF) as the canonical spelling coordinate.
 *
 * Invariants:
 * - letter ∈ [0, 6] where 0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B
 * - pc(sp) == ((nat(sp.letter) + sp.accidental) % 12 + 12) % 12
 * - from_line_of_fifths(line_of_fifths_position(sp), sp.octave) == sp
 */

#pragma once

#include "../Tensor/TNTP001A.h"

#include <array>
#include <cassert>
#include <string>
#include <string_view>

namespace Sunny::Core {

// =============================================================================
// SpelledPitch Type
// =============================================================================

/**
 * @brief Pitch with enharmonic spelling preserved
 *
 * Unlike PitchClass (which collapses enharmonics), SpelledPitch retains
 * the letter name and accidental, enabling correct interval naming,
 * key signature handling, and score notation.
 */
struct SpelledPitch {
    uint8_t letter;     ///< 0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B
    int8_t accidental;  ///< negative=flats, 0=natural, positive=sharps
    int8_t octave;      ///< -1..9 for MIDI-bounded, unbounded for theoretical

    [[nodiscard]] constexpr bool operator==(const SpelledPitch&) const noexcept = default;
};

// =============================================================================
// Constants
// =============================================================================

/// Pitch class of each natural letter: C=0, D=2, E=4, F=5, G=7, A=9, B=11
constexpr std::array<PitchClass, 7> NATURAL_PITCH_CLASS = {0, 2, 4, 5, 7, 9, 11};

/// Line-of-fifths base position for each letter: C=0, D=2, E=4, F=-1, G=1, A=3, B=5
constexpr std::array<int, 7> LOF_BASE = {0, 2, 4, -1, 1, 3, 5};

// =============================================================================
// Constexpr Operations
// =============================================================================

/**
 * @brief Natural pitch class for a letter index
 *
 * @param letter Letter index [0, 6]
 * @return Pitch class of the natural note
 * @pre letter ∈ [0, 6]
 */
[[nodiscard]] constexpr PitchClass nat(uint8_t letter) noexcept {
    assert(letter < 7 && "nat: letter must be in [0, 6]");
    return NATURAL_PITCH_CLASS[letter];
}

/**
 * @brief Pitch class of a spelled pitch
 *
 * @param sp Spelled pitch
 * @return Pitch class [0, 11]
 */
[[nodiscard]] constexpr PitchClass pc(SpelledPitch sp) noexcept {
    int raw = static_cast<int>(nat(sp.letter)) + sp.accidental;
    return static_cast<PitchClass>(((raw % 12) + 12) % 12);
}

/**
 * @brief MIDI value of a spelled pitch (unchecked)
 *
 * May return values outside [0, 127] for extreme spellings.
 *
 * @param sp Spelled pitch
 * @return Integer MIDI value
 */
[[nodiscard]] constexpr int midi_value(SpelledPitch sp) noexcept {
    return 12 * (sp.octave + 1) + static_cast<int>(nat(sp.letter)) + sp.accidental;
}

/**
 * @brief Checked MIDI note from spelled pitch
 *
 * @param sp Spelled pitch
 * @return MidiNote or InvalidMidiNote error
 */
[[nodiscard]] constexpr Result<MidiNote> midi(SpelledPitch sp) noexcept {
    int val = midi_value(sp);
    if (val < 0 || val > 127) {
        return std::unexpected(ErrorCode::InvalidMidiNote);
    }
    return static_cast<MidiNote>(val);
}

/**
 * @brief Test whether two spelled pitches are enharmonically equivalent
 *
 * @return true if both pitches produce the same MIDI value
 */
[[nodiscard]] constexpr bool enharmonic_equivalent(SpelledPitch a, SpelledPitch b) noexcept {
    return midi_value(a) == midi_value(b);
}

/**
 * @brief Line-of-fifths position
 *
 * Maps a spelled pitch to its position on the line of fifths:
 * ...Bbb(-9) Fb(-8) Cb(-7) Gb(-6) Db(-5) Ab(-4) Eb(-3) Bb(-2) F(-1) C(0)
 * G(1) D(2) A(3) E(4) B(5) F#(6) C#(7) G#(8) D#(9) A#(10) E#(11) B#(12)...
 *
 * @param sp Spelled pitch
 * @return LoF position (integer, unbounded)
 */
[[nodiscard]] constexpr int line_of_fifths_position(SpelledPitch sp) noexcept {
    assert(sp.letter < 7 && "line_of_fifths_position: letter must be in [0, 6]");
    return LOF_BASE[sp.letter] + 7 * sp.accidental;
}

/**
 * @brief Validate letter index
 *
 * @param l Value to test
 * @return true if l ∈ [0, 6]
 */
[[nodiscard]] constexpr bool is_valid_letter(int l) noexcept {
    return l >= 0 && l < Constants::DIATONIC_COUNT;
}

/**
 * @brief Reconstruct SpelledPitch from line-of-fifths position and octave
 *
 * The LoF position uniquely determines letter + accidental.
 * The mapping uses the F-C-G-D-A-E-B cycle (LoF positions -1..5 for naturals).
 *
 * @param q LoF position
 * @param octave Octave number
 * @return SpelledPitch
 */
[[nodiscard]] constexpr SpelledPitch from_line_of_fifths(int q, int8_t octave) noexcept {
    // Letter order on LoF: F(0), C(1), G(2), D(3), A(4), E(5), B(6)
    // LoF position of F = -1, so offset q by +1 to get into [0..6] for naturals
    // Natural range: q ∈ [-1, 5]

    // Floor division for negative values: (q + 1) maps F=0,C=1,...,B=6
    // Each group of 7 on the LoF corresponds to one accidental level
    int shifted = q + 1;  // F becomes 0
    int acc;
    int letter_idx;  // index into the F-C-G-D-A-E-B ordering

    if (shifted >= 0) {
        acc = shifted / 7;
        letter_idx = shifted % 7;
    } else {
        acc = (shifted - 6) / 7;  // floor division
        letter_idx = ((shifted % 7) + 7) % 7;
    }

    // Map LoF letter index to standard letter: F=3, C=0, G=4, D=1, A=5, E=2, B=6
    constexpr std::array<uint8_t, 7> LOF_TO_LETTER = {3, 0, 4, 1, 5, 2, 6};
    uint8_t letter = LOF_TO_LETTER[letter_idx];

    return SpelledPitch{letter, static_cast<int8_t>(acc), octave};
}

// =============================================================================
// Non-constexpr Functions (declared, defined in PTSP001A.cpp)
// =============================================================================

/**
 * @brief Format as scientific pitch notation (e.g. "C#4", "Bb3", "D##5")
 */
[[nodiscard]] Result<std::string> to_spn(SpelledPitch sp);

/**
 * @brief Parse scientific pitch notation
 *
 * Accepts: letter [accidentals] octave
 * - Letter: A-G (case-insensitive)
 * - Accidentals: # (sharp), b (flat), x (double sharp), ## (double sharp)
 * - Octave: integer (may be negative)
 *
 * @param s Input string
 * @return SpelledPitch or error
 */
[[nodiscard]] Result<SpelledPitch> from_spn(std::string_view s);

/**
 * @brief Default enharmonic spelling for a pitch class in a key context
 *
 * Uses line-of-fifths proximity to the key centre to choose the simplest spelling.
 *
 * @param target_pc Pitch class to spell [0, 11]
 * @param key_lof LoF position of the key centre (0 = C major)
 * @param octave Octave to assign
 * @return SpelledPitch with the preferred spelling
 */
[[nodiscard]] SpelledPitch default_spelling(PitchClass target_pc, int key_lof, int8_t octave);

}  // namespace Sunny::Core
