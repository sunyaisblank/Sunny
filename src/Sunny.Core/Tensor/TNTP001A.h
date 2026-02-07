/**
 * @file TNTP001A.h
 * @brief Core types for Sunny music theory computation
 *
 * Component: TNTP001A
 * Domain: TN (Tensor) | Category: TP (Types)
 *
 * This header defines the fundamental types used throughout Sunny.
 * All types are designed for:
 * - Exact integer arithmetic where possible
 * - Clear bounds and invariants
 * - Zero-cost abstractions
 *
 * Invariants:
 * - PitchClass ∈ [0, 11]
 * - MidiNote ∈ [0, 127]
 * - Velocity ∈ [1, 127]
 * - Beat uses rational arithmetic (no floating-point precision loss)
 */

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Fundamental Types
// =============================================================================

/**
 * @brief Pitch class in Z/12Z (exact integer arithmetic)
 *
 * Invariant: value ∈ [0, 11]
 */
using PitchClass = std::uint8_t;

/**
 * @brief MIDI note number
 *
 * Invariant: value ∈ [0, 127]
 * MIDI 60 = C4 (Middle C)
 */
using MidiNote = std::uint8_t;

/**
 * @brief MIDI velocity
 *
 * Invariant: value ∈ [1, 127] (0 is note-off)
 */
using Velocity = std::uint8_t;

/**
 * @brief Interval in semitones
 */
using Interval = std::int8_t;

// =============================================================================
// Error Codes
// =============================================================================

/**
 * @brief Error codes for theory operations
 *
 * Layout:
 * - 2xxx: Validation errors
 * - 3xxx: Theory computation errors
 * - 4xxx: Infrastructure errors
 */
enum class ErrorCode : int {
    Ok = 0,

    // Validation errors (2xxx)
    InvalidMidiNote = 2100,
    InvalidVelocity = 2101,
    InvalidPitchClass = 2102,
    InvalidTempo = 2103,
    InvalidScaleName = 2110,
    InvalidChordQuality = 2112,
    InvalidRomanNumeral = 2113,
    InvalidNoteName = 2114,
    InvalidOctave = 2115,

    // Theory computation errors (3xxx)
    ScaleGenerationFailed = 3100,
    ChordGenerationFailed = 3101,
    ProgressionParseFailed = 3102,
    VoiceLeadingFailed = 3110,
    EuclideanInvalidParams = 3121,
    TupletInvalidRatio = 3130,
    HarmonyAnalysisFailed = 3150,
    NegativeHarmonyFailed = 3151,
    InvalidPitchClassOp = 3170,

    // Infrastructure errors (4xxx)
    ConnectionFailed = 4100,
    ConnectionLost = 4101,
    SendFailed = 4102,
    ReceiveFailed = 4103,
    ProtocolError = 4110,
    SessionNotReady = 4200,
    TransactionFailed = 4250,
    McpParseError = 4300,
    McpToolNotFound = 4301,
    OscEncodeError = 4400,
    OscDecodeError = 4401,
};

/**
 * @brief Result type using std::expected
 */
template <typename T>
using Result = std::expected<T, ErrorCode>;

using VoidResult = std::expected<void, ErrorCode>;

// =============================================================================
// Constants
// =============================================================================

namespace Constants {

constexpr MidiNote MIDI_NOTE_MIN = 0;
constexpr MidiNote MIDI_NOTE_MAX = 127;
constexpr Velocity VELOCITY_MIN = 1;
constexpr Velocity VELOCITY_MAX = 127;
constexpr int PITCH_CLASS_COUNT = 12;
constexpr int OCTAVE_MIN = -1;
constexpr int OCTAVE_MAX = 9;
constexpr double TEMPO_MIN_BPM = 20.0;
constexpr double TEMPO_MAX_BPM = 999.0;
constexpr int EUCLIDEAN_MAX_STEPS = 64;

}  // namespace Constants

// =============================================================================
// Validation
// =============================================================================

[[nodiscard]] constexpr bool is_valid_midi_note(int note) noexcept {
    return note >= Constants::MIDI_NOTE_MIN && note <= Constants::MIDI_NOTE_MAX;
}

[[nodiscard]] constexpr bool is_valid_pitch_class(int pc) noexcept {
    return pc >= 0 && pc < Constants::PITCH_CLASS_COUNT;
}

[[nodiscard]] constexpr bool is_valid_velocity(int vel) noexcept {
    return vel >= Constants::VELOCITY_MIN && vel <= Constants::VELOCITY_MAX;
}

}  // namespace Sunny::Core
