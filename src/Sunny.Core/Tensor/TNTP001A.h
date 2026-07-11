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
#include <compare>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Error Codes
// =============================================================================

/**
 * @brief Error codes for theory operations
 *
 * Layout:
 * - 2xxx: Validation errors
 * - 3xxx: Theory computation errors
 * - 4xxx: Infrastructure and format errors
 * - 5xxx: Score IR
 * - 6xxx: Timbre IR
 * - 7xxx: Mix IR
 * - 8xxx: Corpus IR
 *
 * This enum is the single error taxonomy: every value crossing a
 * Result<T> boundary or stored in a Diagnostic is an enumerator here.
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
    InvalidLetterName = 2116,
    InvalidSpelledPitch = 2117,
    InvalidIntervalQuality = 2118,
    InvalidTimeSignature = 2120,
    InvalidAppliedChord = 2125,
    InvalidToneRow = 2130,
    InvalidTriad = 2131,
    InvalidMelody = 2132,
    ArithmeticOverflow = 2135,
    InvalidBPM            = 2136,
    InvalidBarRange       = 2138,

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
    InvalidGeneratedScale = 3180,
    InvalidEDO           = 3190,
    InvalidJIRatio       = 3191,
    InvalidFrequency     = 3192,
    InvalidPartialNumber = 3193,
    InvalidKNetEdgeIndex  = 3195,
    ScaleNotFound         = 3010,
    TemperamentNotFound   = 3011,
    ChordNotRecognised    = 3012,
    UnknownChordQuality   = 3013,
    ForteNumberNotFound   = 3014,

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

    // Format errors (45xx)
    FormatError       = 4500,
    InvalidScalaFile  = 4501,
    InvalidMidiFile   = 4502,
    InvalidAbcFile    = 4503,
    InvalidMusicXml   = 4504,
    InvalidMidiPPQ    = 4505,
    InvalidMidiTempo  = 4506,
    InvalidMidiTimeSig = 4507,

    // Score IR errors (5xxx, structure §14.3)
    DocumentStructure   = 5000,
    MissingParts        = 5001,
    MeasureCountMismatch = 5002,
    EmptyVoice          = 5003,
    InvalidOffset       = 5100,
    OverlappingEvents   = 5101,
    TieMismatch         = 5102,
    MeasureFillError    = 5103,
    TupletSpanError     = 5104,
    InvalidScoreTime    = 5200,
    TempoMapGap         = 5201,
    TickConversionError = 5202,
    KeyMapGap           = 5203,
    TimeMapGap          = 5204,
    StaleHarmonicLayer  = 5300,
    InconsistentOrch    = 5301,
    OverlappingAnnotation = 5302,
    InconsistentOrchField = 5303,
    InvalidMutation     = 5400,
    InvariantViolation  = 5401,
    InvalidRegion       = 5402,
    PartNotFound        = 5403,
    MissingPreset       = 5500,
    UnmappedArticulation = 5501,
    SchemaVersionMismatch = 5600,
    CorruptDocument     = 5601,
    ValidationOnLoadFailed = 5602,

    // Timbre IR errors (6xxx)
    MissingProfile      = 6000,
    InvalidSource       = 6001,
    CutoffAboveNyquist  = 6010,
    ExcessiveDetune     = 6011,
    FMFeedbackUnstable  = 6012,
    EffectChainCycle    = 6020,
    InvalidModTarget    = 6021,
    TimbreInvalidParameter = 6022,
    TimbreNotFound      = 6023,
    TimbreDuplicateId   = 6024,
    StaleDescriptors    = 6030,
    UnmappedParameters  = 6031,

    // Mix IR errors (7xxx)
    MissingChannel      = 7000,
    SignalFlowCycle     = 7001,
    UnreachableMaster   = 7002,
    NoInsertProcessing  = 7003,
    SilentNotMuted      = 7004,
    InvalidSidechain    = 7005,
    MasterClipping      = 7010,
    LoudnessExceeded    = 7011,
    LowCorrelation      = 7012,
    SubBassPhase        = 7013,
    LoudnessRangeWide   = 7014,
    ChannelClipping     = 7015,
    SpectralDeviation   = 7016,
    NoChannelIntent     = 7020,
    NoGroupIntent       = 7021,
    LeadTooQuiet        = 7022,
    FoundationNoSubBass = 7023,
    FlatDepthStaging    = 7024,
    MixInvalidParameter = 7030,
    MixNotFound         = 7031,
    MixDuplicateId      = 7032,
    InvalidPath         = 7033,
    NestingDepthExceeded = 7034,

    // Corpus IR errors (8xxx)
    LowIngestionConfidence = 8000,
    ScoreValidationFailed  = 8001,
    LowKeyConfidence       = 8002,
    InferredTimeSig        = 8003,
    ExcessiveVoices        = 8004,
    LowHarmonicCoverage    = 8010,
    OverSegmentation       = 8011,
    NoThematicUnits        = 8012,
    SingleInstrument       = 8013,
    SmallCorpus            = 8020,
    ModerateCorpus         = 8021,
    SmallPeriodCorpus      = 8022,
    WeakSignature          = 8023,
    CorpusNotFound         = 8030,
    CorpusDuplicateId      = 8031,
    CorpusInvalidParameter = 8032,
    IngestionFailed        = 8033,
    AnalysisFailed         = 8034,
};

/**
 * @brief Result type using std::expected
 */
template <typename T>
using Result = std::expected<T, ErrorCode>;

using VoidResult = std::expected<void, ErrorCode>;

// =============================================================================
// Fundamental Types
// =============================================================================

/**
 * @brief Pitch class in Z/12Z (exact integer arithmetic)
 *
 * Invariant: value ∈ [0, 11], enforced at construction. Three entry points:
 * - integer literals construct at compile time (consteval, range-checked);
 * - PitchClass::wrapped(v) applies euclidean mod 12 where wrap-around is the
 *   intended semantic (transposition arithmetic, interval sums);
 * - PitchClass::from_int(v) refuses out-of-range input with
 *   ErrorCode::InvalidPitchClass at validated trust boundaries.
 * Reads convert implicitly to std::uint8_t, so arithmetic, indexing, and
 * comparison against integers work unchanged.
 */
class PitchClass {
public:
    constexpr PitchClass() noexcept = default;                       // 0
    consteval PitchClass(int v) : v_(static_cast<std::uint8_t>(v)) { // literals, compile-time checked
        if (v < 0 || v > 11) throw "PitchClass literal out of range [0, 11]";
    }
    [[nodiscard]] static constexpr Result<PitchClass> from_int(int v) noexcept {
        if (v < 0 || v > 11) return std::unexpected(ErrorCode::InvalidPitchClass);
        PitchClass pc;
        pc.v_ = static_cast<std::uint8_t>(v);
        return pc;
    }
    [[nodiscard]] static constexpr PitchClass wrapped(int v) noexcept {  // euclidean mod 12
        PitchClass pc;
        pc.v_ = static_cast<std::uint8_t>(((v % 12) + 12) % 12);
        return pc;
    }
    constexpr operator std::uint8_t() const noexcept { return v_; }     // implicit read
    [[nodiscard]] constexpr std::uint8_t value() const noexcept { return v_; }
    constexpr auto operator<=>(const PitchClass&) const noexcept = default;
    constexpr bool operator==(const PitchClass&) const noexcept = default;
    // Heterogeneous comparisons against int: without these, `pc == 5` is
    // ambiguous between the member operator (int -> PitchClass) and the
    // built-in operator (PitchClass -> uint8_t -> int).
    friend constexpr bool operator==(PitchClass lhs, int rhs) noexcept { return lhs.v_ == rhs; }
    friend constexpr std::strong_ordering operator<=>(PitchClass lhs, int rhs) noexcept {
        return lhs.v_ <=> rhs;
    }
private:
    std::uint8_t v_{0};
};

/**
 * @brief MIDI note number
 *
 * Invariant: value ∈ [0, 127], enforced at construction. MIDI 60 = C4
 * (Middle C). Integer literals construct at compile time (consteval,
 * range-checked); runtime values enter through MidiNote::from_int, which
 * refuses out-of-range input with ErrorCode::InvalidMidiNote. No wrapping
 * factory exists: unlike pitch classes, MIDI numbers have no modular
 * semantics, so out-of-range values are refused, never folded into range.
 * Reads convert implicitly to std::uint8_t.
 */
class MidiNote {
public:
    constexpr MidiNote() noexcept = default;                       // 0
    consteval MidiNote(int v) : v_(static_cast<std::uint8_t>(v)) { // literals, compile-time checked
        if (v < 0 || v > 127) throw "MidiNote literal out of range [0, 127]";
    }
    [[nodiscard]] static constexpr Result<MidiNote> from_int(int v) noexcept {
        if (v < 0 || v > 127) return std::unexpected(ErrorCode::InvalidMidiNote);
        MidiNote note;
        note.v_ = static_cast<std::uint8_t>(v);
        return note;
    }
    constexpr operator std::uint8_t() const noexcept { return v_; }   // implicit read
    [[nodiscard]] constexpr std::uint8_t value() const noexcept { return v_; }
    constexpr auto operator<=>(const MidiNote&) const noexcept = default;
    constexpr bool operator==(const MidiNote&) const noexcept = default;
    // Heterogeneous comparisons against int: see PitchClass.
    friend constexpr bool operator==(MidiNote lhs, int rhs) noexcept { return lhs.v_ == rhs; }
    friend constexpr std::strong_ordering operator<=>(MidiNote lhs, int rhs) noexcept {
        return lhs.v_ <=> rhs;
    }
private:
    std::uint8_t v_{0};
};

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
constexpr int DIATONIC_COUNT = 7;
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

/**
 * @brief Positive modulo 12 for pitch class computation
 *
 * Standard C++ truncation-toward-zero yields negative remainders for negative
 * operands. This function returns a value in [0, 11] for any integer input.
 */
[[nodiscard]] constexpr PitchClass mod12_positive(int value) noexcept {
    return PitchClass::wrapped(value);
}

}  // namespace Sunny::Core

// =============================================================================
// Hashing
// =============================================================================

// Hash support mirrors the underlying integer so unordered containers
// (e.g. PitchClassSet) behave exactly as they did for the raw alias.

template <>
struct std::hash<Sunny::Core::PitchClass> {
    [[nodiscard]] std::size_t operator()(Sunny::Core::PitchClass pc) const noexcept {
        return std::hash<std::uint8_t>{}(pc.value());
    }
};

template <>
struct std::hash<Sunny::Core::MidiNote> {
    [[nodiscard]] std::size_t operator()(Sunny::Core::MidiNote note) const noexcept {
        return std::hash<std::uint8_t>{}(note.value());
    }
};
