/**
 * @file types.hpp
 * @brief Score IR foundation types — enumerations, value types, identifiers
 *
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

#include <array>
#include <cassert>
#include <compare>
#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <string_view>
#include <sunny/core/pitch/diatonic_interval.hpp>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/rhythm/meter.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <variant>
#include <vector>

namespace sunny::core {

// =============================================================================
// Identifiers
// =============================================================================

/**
 * @brief Opaque typed identifier — equality-comparable, hashable
 *
 * Each Id<T> is unique within a Score document. Tag type T prevents
 * accidental comparison between Id<Score> and Id<Part>.
 */
template <typename T> struct Id {
    std::uint64_t value{};

    constexpr bool operator==(const Id&) const noexcept = default;
    constexpr auto operator<=>(const Id&) const noexcept = default;
};

} // namespace sunny::core

// Hash specialisation for Id<T>
template <typename T> struct std::hash<sunny::core::Id<T>> {
    std::size_t operator()(const sunny::core::Id<T>& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

namespace sunny::core {

// Forward declarations for Id tags
struct ScoreTag;
struct PartTag;
struct EventTag;
struct SectionTag;
struct BeamGroupTag;
struct TupletTag;

using ScoreId = Id<ScoreTag>;
using PartId = Id<PartTag>;
using EventId = Id<EventTag>;
using SectionId = Id<SectionTag>;
using BeamGroupId = Id<BeamGroupTag>;
using TupletId = Id<TupletTag>;

// =============================================================================
// PositiveRational — for BPM (rate, not duration)
// =============================================================================

/**
 * @brief Strictly positive rational number (p/q, p > 0, q > 0)
 *
 * Uses the same canonical rational representation as Beat but carries rate
 * semantics (e.g., beats per minute). Tempo algorithms explicitly project a
 * rate into exact Beat algebra; the types do not compose accidentally.
 */
class PositiveRational {
  public:
    constexpr PositiveRational() noexcept = default;

    /**
     * @brief Construct a canonical positive ratio at a programming-contract boundary.
     *
     * Dynamic or untrusted input must use from_ratio() so invalidity remains
     * representable as data rather than terminating the process.
     */
    constexpr PositiveRational(std::int64_t numerator, std::int64_t denominator) noexcept {
        const auto canonical = detail::canonical_beat_components(numerator, denominator);
        const bool valid = numerator > 0 && denominator > 0 && canonical.representable;
        assert(valid && "PositiveRational components must be positive and representable");
        if (!valid) std::terminate();
        numerator_ = canonical.numerator;
        denominator_ = canonical.denominator;
    }

    /** @brief Construct a positive ratio from dynamic input without violating the invariant. */
    [[nodiscard]] static constexpr Result<PositiveRational>
    from_ratio(std::int64_t numerator, std::int64_t denominator) noexcept {
        if (numerator <= 0 || denominator <= 0) return std::unexpected(ErrorCode::InvalidTempo);
        const auto canonical = detail::canonical_beat_components(numerator, denominator);
        if (!canonical.representable) return std::unexpected(ErrorCode::ArithmeticOverflow);
        return PositiveRational{canonical.numerator, canonical.denominator};
    }

    [[nodiscard]] constexpr std::int64_t numerator() const noexcept { return numerator_; }
    [[nodiscard]] constexpr std::int64_t denominator() const noexcept { return denominator_; }

    [[nodiscard]] constexpr double to_float() const noexcept {
        return static_cast<double>(numerator_) / static_cast<double>(denominator_);
    }

    constexpr bool operator==(const PositiveRational&) const noexcept = default;
    constexpr auto operator<=>(const PositiveRational& other) const noexcept {
        return Beat{numerator_, denominator_} <=> Beat{other.numerator_, other.denominator_};
    }

  private:
    std::int64_t numerator_{1};
    std::int64_t denominator_{1};
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
    constexpr bool operator<=(const ScoreTime& o) const noexcept { return !(o < *this); }
    constexpr bool operator>(const ScoreTime& o) const noexcept { return o < *this; }
    constexpr bool operator>=(const ScoreTime& o) const noexcept { return !(*this < o); }
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
enum class TempoTransitionType : std::uint8_t { Immediate, Linear, MetricModulation };

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
enum class Clef : std::uint8_t { Treble, Bass, Alto, Tenor, Percussion, Tab };

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

/// Articulation types (§4.5.4)
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

/// Dynamic level (§4.5.5)
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

/// Hairpin type (§4.5.5)
enum class HairpinType : std::uint8_t { Crescendo, Diminuendo };

/// Grace note type (§4.5.6)
enum class GraceType : std::uint8_t { Acciaccatura, Appoggiatura };

/// Ornament types (§4.5.7)
enum class OrnamentType : std::uint8_t {
    Trill,
    Mordent,
    InvertedMordent,
    Turn,
    InvertedTurn,
    Shake,
    Arpeggio
};

/// Arpeggio direction (§4.5.7)
enum class ArpeggioDirection : std::uint8_t { Up, Down, None };

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
enum class DynamicBalance : std::uint8_t { Foreground, MiddleGround, Background };

/// Harmonic function for Score IR annotations (§6.2)
enum class ScoreHarmonicFunction : std::uint8_t { Tonic, Predominant, Dominant, Ambiguous };

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

[[nodiscard]] constexpr std::string_view directive_name(DirectiveType directive) noexcept {
    switch (directive) {
    case DirectiveType::Mute:
        return "mute";
    case DirectiveType::Solo:
        return "solo";
    case DirectiveType::ConSordino:
        return "con sordino";
    case DirectiveType::SenzaSordino:
        return "senza sordino";
    case DirectiveType::Pizzicato:
        return "pizzicato";
    case DirectiveType::Arco:
        return "arco";
    case DirectiveType::Divisi:
        return "divisi";
    case DirectiveType::Tutti:
        return "tutti";
    case DirectiveType::Tacet:
        return "tacet";
    case DirectiveType::ColLegno:
        return "col legno";
    case DirectiveType::SulPonticello:
        return "sul ponticello";
    case DirectiveType::SulTasto:
        return "sul tasto";
    case DirectiveType::HalfPedal:
        return "half pedal";
    case DirectiveType::SustainingPedal:
        return "sustaining pedal";
    case DirectiveType::UnaCorda:
        return "una corda";
    case DirectiveType::TreCorde:
        return "tre corde";
    }
    return "unknown directive";
}

/// Validation diagnostic severity (§10.1)
enum class ValidationSeverity : std::uint8_t { Error, Warning, Info };

/// Slide direction for TechnicalDirection (§4.12)
enum class SlideDirection : std::uint8_t { Into, OutOf, Ascending, Descending };

/// Vibrato speed for TechnicalDirection (§4.12)
enum class VibratoSpeed : std::uint8_t { Slow, Normal, Fast, None };

// =============================================================================
// Small value types
// =============================================================================

/// Key signature (§2.4)
struct KeySignature {
    SpelledPitch root;   ///< Tonic (letter + accidental; octave ignored)
    ScaleDefinition mode{///< Scale type; the default key is C major
                         "major",
                         {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0},
                         7,
                         "Ionian mode"};
    std::int8_t accidentals{0}; ///< Signed count: +sharps, -flats

    constexpr bool operator==(const KeySignature& o) const noexcept {
        return root.letter == o.root.letter && root.accidental == o.root.accidental &&
               accidentals == o.accidentals && mode.name == o.mode.name &&
               mode.note_count == o.mode.note_count && mode.intervals == o.mode.intervals;
    }
};

[[nodiscard]] constexpr std::optional<int>
key_mode_fifths_adjustment(std::string_view mode) noexcept {
    if (mode == "major" || mode == "ionian") return 0;
    if (mode == "dorian") return -2;
    if (mode == "minor" || mode == "aeolian") return -3;
    if (mode == "phrygian") return -4;
    if (mode == "lydian") return 1;
    if (mode == "mixolydian") return -1;
    if (mode == "locrian") return -5;
    return std::nullopt;
}

[[nodiscard]] constexpr std::optional<int>
expected_key_accidentals(const KeySignature& key) noexcept {
    constexpr std::array tonic_fifths{0, 2, 4, -1, 1, 3, 5};
    if (key.root.letter >= tonic_fifths.size()) return std::nullopt;
    const auto adjustment = key_mode_fifths_adjustment(key.mode.name);
    if (!adjustment) return std::nullopt;
    return tonic_fifths[key.root.letter] + 7 * static_cast<int>(key.root.accidental) + *adjustment;
}

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

/// Velocity with semantic and numeric components (§4.5.3)
struct VelocityValue {
    std::optional<DynamicLevel> written{}; ///< Semantic dynamic if marked
    std::uint8_t value{};                  ///< MIDI velocity 0–127
};

/// Tempo event in the TempoMap (§2.3)
struct TempoEvent {
    ScoreTime position;
    PositiveRational bpm;
    BeatUnit beat_unit;
    TempoTransitionType transition_type; ///< How the previous tempo reaches this event
    Beat linear_duration;                ///< Previous-position-to-this-position span for Linear
    BeatUnit old_unit;                   ///< Previous subdivision for MetricModulation
    BeatUnit new_unit;                   ///< This event's beat unit for MetricModulation
};

/// Hairpin — gradual dynamic change (§4.5.5)
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
    std::uint8_t divisi_count; ///< Only used when directive == Divisi
};

/// Ornament (§4.5.7)
struct Ornament {
    OrnamentType type;
    Interval trill_interval;               ///< For Trill
    std::optional<std::int8_t> accidental; ///< For Trill accidental
    ArpeggioDirection arpeggio_direction;  ///< For Arpeggio
};

/// Validation diagnostic (§10)
struct Diagnostic {
    ValidationSeverity severity;
    std::string rule; ///< Rule code e.g. "S1", "M3", "R2"
    std::string message;
    std::optional<ScoreTime> location;
    std::optional<PartId> part;
    ErrorCode
        error_code; ///< Domain-ranged: 5xxx Score, 6xxx Timbre, 7xxx Mix, 8xxx Corpus, 9xxx project
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
    std::vector<std::uint8_t> fingering; ///< For Fingering
    std::uint8_t number;                 ///< For StringNumber, Position
    std::string pattern;                 ///< For BowingPattern
    SlideDirection slide_direction;      ///< For Slide
    std::int16_t bend_cents;             ///< For Bend
    VibratoSpeed vibrato_speed;          ///< For Vibrato
};

// =============================================================================
// Default velocity ranges (§4.5.3)
// =============================================================================

/// Return the default velocity midpoint for a DynamicLevel
[[nodiscard]] constexpr std::uint8_t default_velocity(DynamicLevel level) noexcept {
    switch (level) {
    case DynamicLevel::pppp:
        return 12;
    case DynamicLevel::ppp:
        return 24;
    case DynamicLevel::pp:
        return 40;
    case DynamicLevel::p:
        return 56;
    case DynamicLevel::mp:
        return 72;
    case DynamicLevel::mf:
        return 88;
    case DynamicLevel::f:
        return 104;
    case DynamicLevel::ff:
        return 116;
    case DynamicLevel::fff:
        return 123;
    case DynamicLevel::ffff:
        return 127;
    case DynamicLevel::fp:
        return 104; // attack velocity
    case DynamicLevel::sfz:
    case DynamicLevel::sfp:
        return 120; // attack velocity
    case DynamicLevel::rfz:
        return 110;
    }
    return 88; // default to mf
}

/// Return the exact default articulation duration ratio (§9.4).
[[nodiscard]] constexpr Beat articulation_duration_ratio(ArticulationType art) noexcept {
    switch (art) {
    case ArticulationType::Staccato:
        return Beat{1, 2};
    case ArticulationType::Staccatissimo:
        return Beat{1, 4};
    case ArticulationType::Tenuto:
        return Beat{1, 1};
    case ArticulationType::Portato:
        return Beat{3, 4};
    case ArticulationType::Marcato:
        return Beat{17, 20};
    case ArticulationType::Fermata:
        return Beat{7, 4};
    default:
        return Beat{1, 1};
    }
}

/// Floating compatibility view of the exact default duration ratio.
[[nodiscard]] constexpr double articulation_duration_factor(ArticulationType art) noexcept {
    return articulation_duration_ratio(art).to_float();
}

/// Return the dynamic balance velocity multiplier (§9.5)
[[nodiscard]] constexpr double balance_factor(DynamicBalance bal) noexcept {
    switch (bal) {
    case DynamicBalance::Foreground:
        return 1.00;
    case DynamicBalance::MiddleGround:
        return 0.85;
    case DynamicBalance::Background:
        return 0.70;
    }
    return 1.00;
}

} // namespace sunny::core
