/**
 * @file SITP001A.h
 * @brief Score IR foundation types — enumerations, value types, identifiers
 *
 * Component: SITP001A
 * Domain: SI (Score IR) | Category: TP (Types)
 *
 * Defines all enumerations, small value types, and identifier types required
 * by the Score IR document model (Formal Spec §0–§7). These types compose
 * with Theory Spec types (PitchClass, SpelledPitch, Interval, Beat) without
 * duplicating them.
 *
 * Invariants:
 * - PositiveRational has strictly positive numerator and denominator
 * - ScoreTime bar is 1-indexed; beat is non-negative
 * - All Id<T> are unique within a document
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"
#include "../Pitch/PTSP001A.h"
#include "../Pitch/PTDI001A.h"
#include "../Scale/SCDF001A.h"
#include "../Rhythm/RHTS001A.h"

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Identifiers
// =============================================================================

/**
 * @brief Opaque typed identifier — equality-comparable, hashable
 *
 * Each Id<T> is unique within a Score document. Tag type T prevents
 * accidental comparison between Id<Score> and Id<Part>.
 */
template <typename T>
struct Id {
    std::uint64_t value;

    constexpr bool operator==(const Id&) const noexcept = default;
    constexpr auto operator<=>(const Id&) const noexcept = default;
};

}  // namespace Sunny::Core

// Hash specialisation for Id<T>
template <typename T>
struct std::hash<Sunny::Core::Id<T>> {
    std::size_t operator()(const Sunny::Core::Id<T>& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

namespace Sunny::Core {

// Forward declarations for Id tags
struct ScoreTag;
struct PartTag;
struct EventTag;
struct SectionTag;
struct BeamGroupTag;
struct TupletTag;

using ScoreId   = Id<ScoreTag>;
using PartId    = Id<PartTag>;
using EventId   = Id<EventTag>;
using SectionId = Id<SectionTag>;
using BeamGroupId = Id<BeamGroupTag>;
using TupletId  = Id<TupletTag>;

// =============================================================================
// PositiveRational — for BPM (rate, not duration)
// =============================================================================

/**
 * @brief Strictly positive rational number (p/q, p > 0, q > 0)
 *
 * Same arithmetic as Beat but carries rate semantics (e.g., beats per minute).
 * Kept distinct from Beat to prevent accidental rate-duration composition.
 */
struct PositiveRational {
    std::int64_t numerator;
    std::int64_t denominator;

    [[nodiscard]] constexpr double to_float() const noexcept {
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    }

    constexpr bool operator==(const PositiveRational&) const noexcept = default;
    constexpr auto operator<=>(const PositiveRational& o) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * o.denominator;
        __int128 rhs = static_cast<__int128>(o.numerator) * denominator;
        if (lhs < rhs) return std::strong_ordering::less;
        if (lhs > rhs) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
};

[[nodiscard]] constexpr PositiveRational make_bpm(std::int64_t bpm) noexcept {
    return {bpm, 1};
}

// =============================================================================
// ScoreTime — primary temporal coordinate (§5.1)
// =============================================================================

/**
 * @brief Hierarchical time coordinate: (bar, beat-within-bar)
 *
 * bar is 1-indexed. beat is a Beat offset from the measure start.
 * Beat(0,1) = downbeat of the bar.
 */
struct ScoreTime {
    std::uint32_t bar;
    Beat beat;

    constexpr bool operator==(const ScoreTime&) const noexcept = default;

    constexpr bool operator<(const ScoreTime& o) const noexcept {
        if (bar != o.bar) return bar < o.bar;
        return beat < o.beat;
    }
    constexpr bool operator<=(const ScoreTime& o) const noexcept {
        return !(o < *this);
    }
    constexpr bool operator>(const ScoreTime& o) const noexcept {
        return o < *this;
    }
    constexpr bool operator>=(const ScoreTime& o) const noexcept {
        return !(*this < o);
    }
};

/// Score start: bar 1, beat 0
constexpr ScoreTime SCORE_START{1, Beat::zero()};

// =============================================================================
// Enumerations — §2–§4
// =============================================================================

/// Beat unit for tempo marking (§2.3)
enum class BeatUnit : std::uint8_t {
    Whole,
    Half,
    DottedHalf,
    Quarter,
    DottedQuarter,
    Eighth,
    DottedEighth,
    Sixteenth
};

/// How a tempo change transitions from the previous tempo (§2.3)
enum class TempoTransitionType : std::uint8_t {
    Immediate,
    Linear,
    MetricModulation
};

/// Form function classification (§2.6)
enum class FormFunction : std::uint8_t {
    Expository,
    Developmental,
    Transitional,
    Cadential,
    Introductory,
    Closing,
    Parenthetical
};

/// Clef types (§3.2.3)
enum class Clef : std::uint8_t {
    Treble,
    Bass,
    Alto,
    Tenor,
    Percussion,
    Tab
};

/// Instrument class — top-level families (§3.2.1)
enum class InstrumentFamily : std::uint8_t {
    Strings,
    Woodwinds,
    Brass,
    Percussion,
    Keyboard,
    Voice,
    Electronic
};

/// Articulation types (§4.5.2)
enum class ArticulationType : std::uint8_t {
    Staccato,
    Staccatissimo,
    Tenuto,
    Portato,
    Accent,
    Marcato,
    Sforzando,
    ForzandoPiano,
    Fermata,
    Trill,
    Mordent,
    InvertedMordent,
    Turn,
    InvertedTurn,
    Tremolo,
    Harmonic,
    GlissandoStart,
    GlissandoEnd,
    SnapPizzicato,
    DownBow,
    UpBow,
    OpenString,
    Stopped,
    Muted,
    BendUp,
    BendDown
};

/// Dynamic level (§4.5.3)
enum class DynamicLevel : std::uint8_t {
    pppp,
    ppp,
    pp,
    p,
    mp,
    mf,
    f,
    ff,
    fff,
    ffff,
    fp,
    sfz,
    sfp,
    rfz
};

/// Hairpin type (§4.5.3)
enum class HairpinType : std::uint8_t {
    Crescendo,
    Diminuendo
};

/// Grace note type (§4.5.4)
enum class GraceType : std::uint8_t {
    Acciaccatura,
    Appoggiatura
};

/// Ornament types (§4.5.5)
enum class OrnamentType : std::uint8_t {
    Trill,
    Mordent,
    InvertedMordent,
    Turn,
    InvertedTurn,
    Shake,
    Arpeggio
};

/// Arpeggio direction (§4.5.5)
enum class ArpeggioDirection : std::uint8_t {
    Up,
    Down,
    None
};

/// Non-chord tone types (§6.3)
enum class NonChordToneType : std::uint8_t {
    PassingTone,
    NeighborTone,
    Suspension,
    Retardation,
    Appoggiatura,
    EscapeTone,
    Cambiata,
    Anticipation,
    Pedal
};

/// Textural role for orchestration (§7.2)
enum class TexturalRole : std::uint8_t {
    Melody,
    CounterMelody,
    HarmonicFill,
    BassLine,
    RhythmicOstinato,
    Doubling,
    PedalTone,
    Obligato,
    Dialogue,
    Tutti,
    Tacet,
    Solo,
    Accompagnato
};

/// Texture classification (§7.3)
enum class TextureType : std::uint8_t {
    Monophonic,
    Homophonic,
    Polyphonic,
    Heterophonic,
    Homorhythmic,
    Antiphonal,
    Fugal,
    Chorale,
    Unison,
    MelodyAccompaniment
};

/// Relative dynamic prominence (§7.3)
enum class DynamicBalance : std::uint8_t {
    Foreground,
    MiddleGround,
    Background
};

/// Harmonic function for Score IR annotations (§6.2)
enum class ScoreHarmonicFunction : std::uint8_t {
    Tonic,
    Predominant,
    Dominant,
    Ambiguous
};

/// Notehead shapes (§4.11)
enum class NoteHeadType : std::uint8_t {
    Normal,
    Diamond,
    Cross,
    Slash,
    Triangle,
    CircleX,
    Square,
    Cue
};

/// Part directive types (§3.3)
enum class DirectiveType : std::uint8_t {
    Mute,
    Solo,
    ConSordino,
    SenzaSordino,
    Pizzicato,
    Arco,
    Divisi,
    Tutti,
    Tacet,
    ColLegno,
    SulPonticello,
    SulTasto,
    HalfPedal,
    SustainingPedal,
    UnaCorda,
    TreCorde
};

/// Validation diagnostic severity (§10.1)
enum class ValidationSeverity : std::uint8_t {
    Error,
    Warning,
    Info
};

/// Document lifecycle state (§2.1)
enum class DocumentState : std::uint8_t {
    Draft,
    Valid,
    Compiled,
    Locked
};

/// Slide direction for TechnicalDirection (§4.12)
enum class SlideDirection : std::uint8_t {
    Into,
    OutOf,
    Ascending,
    Descending
};

/// Vibrato speed for TechnicalDirection (§4.12)
enum class VibratoSpeed : std::uint8_t {
    Slow,
    Normal,
    Fast,
    None
};

// =============================================================================
// Small value types
// =============================================================================

/// Key signature (§2.4)
struct KeySignature {
    SpelledPitch root;          ///< Tonic (letter + accidental; octave ignored)
    ScaleDefinition mode;       ///< Scale type
    std::int8_t accidentals;    ///< Signed count: +sharps, -flats

    constexpr bool operator==(const KeySignature& o) const noexcept {
        return root == o.root && accidentals == o.accidentals;
    }
};

/// Pitch range for instrument (§3.2.2)
struct PitchRange {
    SpelledPitch absolute_low;
    SpelledPitch absolute_high;
    SpelledPitch comfortable_low;
    SpelledPitch comfortable_high;
};

/// Rehearsal mark (§2.7)
struct RehearsalMark {
    ScoreTime position;
    std::string label;
};

/// Velocity with semantic and numeric components (§4.5.1)
struct VelocityValue {
    std::optional<DynamicLevel> written;  ///< Semantic dynamic if marked
    std::uint8_t value;                   ///< MIDI velocity 0–127
};

/// Tempo event in the TempoMap (§2.3)
struct TempoEvent {
    ScoreTime position;
    PositiveRational bpm;
    BeatUnit beat_unit;
    TempoTransitionType transition_type;
    Beat linear_duration;                         ///< For Linear transitions
    BeatUnit old_unit;                            ///< For MetricModulation
    BeatUnit new_unit;                            ///< For MetricModulation
};

/// Hairpin — gradual dynamic change (§4.5.3)
struct Hairpin {
    ScoreTime start;
    ScoreTime end;
    HairpinType type;
    std::optional<DynamicLevel> target;
};

/// Part directive — scoped performance instruction (§3.3)
struct PartDirective {
    ScoreTime start;
    ScoreTime end;
    DirectiveType directive;
    std::uint8_t divisi_count;  ///< Only used when directive == Divisi
};

/// Ornament (§4.5.5)
struct Ornament {
    OrnamentType type;
    Interval trill_interval;                  ///< For Trill
    std::optional<std::int8_t> accidental;    ///< For Trill accidental
    ArpeggioDirection arpeggio_direction;      ///< For Arpeggio
};

/// Validation diagnostic (§10)
struct Diagnostic {
    ValidationSeverity severity;
    std::string rule;           ///< Rule code e.g. "S1", "M3", "R2"
    std::string message;
    std::optional<ScoreTime> location;
    std::optional<PartId> part;
    int error_code;             ///< From Score IR error code range (5000–5699)
};

/// Technical direction (§4.12)
struct TechnicalDirection {
    enum class Type : std::uint8_t {
        Fingering,
        StringNumber,
        Position,
        BowingPattern,
        BreathMark,
        Slide,
        HammerOn,
        PullOff,
        Bend,
        Vibrato,
        Caesura
    };
    Type type;
    std::vector<std::uint8_t> fingering;   ///< For Fingering
    std::uint8_t number;                    ///< For StringNumber, Position
    std::string pattern;                    ///< For BowingPattern
    SlideDirection slide_direction;         ///< For Slide
    std::int16_t bend_cents;               ///< For Bend
    VibratoSpeed vibrato_speed;             ///< For Vibrato
};

// =============================================================================
// Score IR Error Codes (§14.3)
// =============================================================================

namespace ScoreError {
    constexpr int DocumentStructure   = 5000;
    constexpr int MissingParts        = 5001;
    constexpr int MeasureCountMismatch = 5002;
    constexpr int EmptyVoice          = 5003;

    constexpr int InvalidOffset       = 5100;
    constexpr int OverlappingEvents   = 5101;
    constexpr int TieMismatch         = 5102;
    constexpr int MeasureFillError    = 5103;
    constexpr int TupletSpanError     = 5104;

    constexpr int InvalidScoreTime    = 5200;
    constexpr int TempoMapGap         = 5201;
    constexpr int TickConversionError = 5202;
    constexpr int KeyMapGap           = 5203;
    constexpr int TimeMapGap          = 5204;

    constexpr int StaleHarmonicLayer  = 5300;
    constexpr int InconsistentOrch    = 5301;

    constexpr int InvalidMutation     = 5400;
    constexpr int InvariantViolation  = 5401;

    constexpr int MissingPreset       = 5500;
    constexpr int UnmappedArticulation = 5501;

    constexpr int OverlappingAnnotation = 5302;
    constexpr int InconsistentOrchField = 5303;

    constexpr int SchemaVersionMismatch = 5600;
    constexpr int CorruptDocument      = 5601;
    constexpr int ValidationOnLoadFailed = 5602;
}  // namespace ScoreError

// =============================================================================
// Default velocity ranges (§4.5.3)
// =============================================================================

/// Return the default velocity midpoint for a DynamicLevel
[[nodiscard]] constexpr std::uint8_t default_velocity(DynamicLevel level) noexcept {
    switch (level) {
        case DynamicLevel::pppp: return 12;
        case DynamicLevel::ppp:  return 24;
        case DynamicLevel::pp:   return 40;
        case DynamicLevel::p:    return 56;
        case DynamicLevel::mp:   return 72;
        case DynamicLevel::mf:   return 88;
        case DynamicLevel::f:    return 104;
        case DynamicLevel::ff:   return 116;
        case DynamicLevel::fff:  return 123;
        case DynamicLevel::ffff: return 127;
        case DynamicLevel::fp:   return 104;  // attack velocity
        case DynamicLevel::sfz:  return 120;
        case DynamicLevel::sfp:  return 120;  // attack velocity
        case DynamicLevel::rfz:  return 110;
    }
    return 88;  // default to mf
}

/// Return the default articulation duration factor (§9.4)
[[nodiscard]] constexpr double articulation_duration_factor(ArticulationType art) noexcept {
    switch (art) {
        case ArticulationType::Staccato:       return 0.50;
        case ArticulationType::Staccatissimo:  return 0.25;
        case ArticulationType::Tenuto:         return 1.00;
        case ArticulationType::Portato:        return 0.75;
        case ArticulationType::Marcato:        return 0.85;
        case ArticulationType::Fermata:        return 1.75;
        default:                               return 1.00;
    }
}

/// Return the dynamic balance velocity multiplier (§9.5)
[[nodiscard]] constexpr double balance_factor(DynamicBalance bal) noexcept {
    switch (bal) {
        case DynamicBalance::Foreground:   return 1.00;
        case DynamicBalance::MiddleGround: return 0.85;
        case DynamicBalance::Background:   return 0.70;
    }
    return 1.00;
}

}  // namespace Sunny::Core

#pragma GCC diagnostic pop
