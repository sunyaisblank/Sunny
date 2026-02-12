/**
 * @file SIDC001A.h
 * @brief Score IR document hierarchy — Score, Part, Measure, Voice, Event
 *
 * Component: SIDC001A
 * Domain: SI (Score IR) | Category: DC (Document)
 *
 * Defines the complete document model for the Score IR: the Score root,
 * Parts with instrument definitions, Measures with Voices, Events (notes,
 * rests, chord symbols, directions), annotation layers, and global maps.
 *
 * Composes with Theory Spec types (SpelledPitch, Beat, ChordVoicing) and
 * Score IR foundation types (SITP001A).
 *
 * Invariants:
 * - Score has >= 1 Part
 * - Every Part has exactly total_bars Measures
 * - Every Measure has >= 1 Voice
 * - Every Voice's events fill exactly the measure duration
 * - Events within a Voice are non-overlapping and ordered by offset
 * - All Note pitches are concert (sounding), never written
 * - Event offset in [0, measure_duration)
 */

#pragma once

#include "SITP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Harmony/HRCD001A.h"

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Instrument Classification (SS-IR §3.2.1)
// =============================================================================

/**
 * @brief Specific instrument type within the orchestral taxonomy
 *
 * Flat enumeration of all instruments. Use instrument_family() to obtain
 * the top-level InstrumentFamily classification. Electronic/custom types
 * carry additional metadata in PartDefinition::custom_descriptor.
 */
enum class InstrumentType : std::uint8_t {
    // Bowed Strings
    Violin, Viola, Cello, DoubleBass,
    // Plucked Strings
    Harp, Guitar, Lute, Banjo, Mandolin,
    // Woodwinds
    Flute, Piccolo, AltoFlute, BassFlute,
    Oboe, EnglishHorn, BassOboe,
    Clarinet, BassClarinet, EbClarinet,
    Bassoon, Contrabassoon,
    SopranoSax, AltoSax, TenorSax, BaritoneSax,
    // Brass
    FrenchHorn, Trumpet, Cornet,
    Trombone, BassTrombone,
    Tuba, Euphonium,
    // Pitched Percussion
    Timpani, Xylophone, Marimba, Vibraphone,
    Glockenspiel, TubularBells, Celesta,
    // Unpitched Percussion
    SnareDrum, BassDrum, Cymbals, TrianglePerc,
    Tambourine, TamTam, Castanets, WoodBlock,
    // Keyboard
    Piano, Harpsichord, Organ, Accordion,
    // Voice
    SopranoVoice, MezzoSoprano, AltoVoice,
    TenorVoice, BaritoneVoice, BassVoice,
    // Electronic
    Synthesiser, Sampler, DrumMachine, Effect,
    // Generic
    Custom
};

/**
 * @brief Map specific instrument to its top-level family
 */
[[nodiscard]] constexpr InstrumentFamily instrument_family(
    InstrumentType t
) noexcept {
    switch (t) {
        case InstrumentType::Violin:
        case InstrumentType::Viola:
        case InstrumentType::Cello:
        case InstrumentType::DoubleBass:
        case InstrumentType::Harp:
        case InstrumentType::Guitar:
        case InstrumentType::Lute:
        case InstrumentType::Banjo:
        case InstrumentType::Mandolin:
            return InstrumentFamily::Strings;

        case InstrumentType::Flute:
        case InstrumentType::Piccolo:
        case InstrumentType::AltoFlute:
        case InstrumentType::BassFlute:
        case InstrumentType::Oboe:
        case InstrumentType::EnglishHorn:
        case InstrumentType::BassOboe:
        case InstrumentType::Clarinet:
        case InstrumentType::BassClarinet:
        case InstrumentType::EbClarinet:
        case InstrumentType::Bassoon:
        case InstrumentType::Contrabassoon:
        case InstrumentType::SopranoSax:
        case InstrumentType::AltoSax:
        case InstrumentType::TenorSax:
        case InstrumentType::BaritoneSax:
            return InstrumentFamily::Woodwinds;

        case InstrumentType::FrenchHorn:
        case InstrumentType::Trumpet:
        case InstrumentType::Cornet:
        case InstrumentType::Trombone:
        case InstrumentType::BassTrombone:
        case InstrumentType::Tuba:
        case InstrumentType::Euphonium:
            return InstrumentFamily::Brass;

        case InstrumentType::Timpani:
        case InstrumentType::Xylophone:
        case InstrumentType::Marimba:
        case InstrumentType::Vibraphone:
        case InstrumentType::Glockenspiel:
        case InstrumentType::TubularBells:
        case InstrumentType::Celesta:
        case InstrumentType::SnareDrum:
        case InstrumentType::BassDrum:
        case InstrumentType::Cymbals:
        case InstrumentType::TrianglePerc:
        case InstrumentType::Tambourine:
        case InstrumentType::TamTam:
        case InstrumentType::Castanets:
        case InstrumentType::WoodBlock:
            return InstrumentFamily::Percussion;

        case InstrumentType::Piano:
        case InstrumentType::Harpsichord:
        case InstrumentType::Organ:
        case InstrumentType::Accordion:
            return InstrumentFamily::Keyboard;

        case InstrumentType::SopranoVoice:
        case InstrumentType::MezzoSoprano:
        case InstrumentType::AltoVoice:
        case InstrumentType::TenorVoice:
        case InstrumentType::BaritoneVoice:
        case InstrumentType::BassVoice:
            return InstrumentFamily::Voice;

        case InstrumentType::Synthesiser:
        case InstrumentType::Sampler:
        case InstrumentType::DrumMachine:
        case InstrumentType::Effect:
        case InstrumentType::Custom:
            return InstrumentFamily::Electronic;
    }
    return InstrumentFamily::Electronic;
}

// =============================================================================
// Articulation Mapping (SS-IR §3.2.5)
// =============================================================================

/**
 * @brief How an articulation compiles to MIDI/DAW instructions
 *
 * Tagged union: inspect `type` to determine which fields are active.
 */
struct ArticulationMapping {
    enum class Type : std::uint8_t {
        Keyswitch,          ///< Send keyswitch note
        CC,                 ///< Send CC message
        VelocityLayer,      ///< Map to velocity range
        NoteDurationScale,  ///< Scale note duration
        ProgramChange,      ///< Send program change
        Combined            ///< Multiple simultaneous mappings
    };

    Type type;
    SpelledPitch keyswitch_pitch{};              ///< For Keyswitch
    std::uint8_t cc_number = 0;                  ///< For CC
    std::uint8_t cc_value = 0;                   ///< For CC
    std::uint8_t velocity_min = 0;               ///< For VelocityLayer
    std::uint8_t velocity_max = 127;             ///< For VelocityLayer
    float duration_scale = 1.0f;                 ///< For NoteDurationScale
    std::uint8_t program = 0;                    ///< For ProgramChange
    std::vector<ArticulationMapping> combined;   ///< For Combined
};

// =============================================================================
// Rendering Configuration (SS-IR §3.2.5)
// =============================================================================

/**
 * @brief DAW-specific compilation parameters for a Part
 */
struct RenderingConfig {
    std::optional<std::string> instrument_preset;  ///< Plugin/preset path
    std::uint8_t midi_channel = 1;                 ///< MIDI channel (1-16)
    std::map<ArticulationType, ArticulationMapping> articulation_map;
    std::uint8_t expression_cc = 11;               ///< CC for dynamics
    std::optional<float> pan;                      ///< Stereo pan (-1.0 to 1.0)
    std::optional<std::string> group;              ///< Group track assignment
};

// =============================================================================
// Part Definition (SS-IR §3.2)
// =============================================================================

/**
 * @brief Instrument specification and rendering configuration
 */
struct PartDefinition {
    std::string name;                         ///< Display name (e.g. "Violin I")
    std::string abbreviation;                 ///< Short name (e.g. "Vln. I")
    InstrumentType instrument_type;           ///< Specific instrument
    Interval transposition = 0;               ///< Written-to-sounding interval
    Clef clef = Clef::Treble;                ///< Default clef
    PitchRange range{};                       ///< Playable and comfortable range
    std::vector<ArticulationType> articulation_vocabulary;
    std::uint8_t staff_count = 1;             ///< Staves (2 for piano/organ)
    RenderingConfig rendering{};              ///< DAW compilation parameters
    std::optional<std::string> custom_descriptor;  ///< For electronic/custom types
};

// =============================================================================
// Tuplet Context (SS-IR §4.9)
// =============================================================================

/**
 * @brief Groups events under a tuplet bracket
 *
 * Shared among all events in the tuplet via the id field.
 * Nested tuplets reference parent via nested_in.
 */
struct TupletContext {
    TupletId id;
    std::uint8_t actual;        ///< Notes in tuplet (m in m:n)
    std::uint8_t normal;        ///< Notes in normal division (n in m:n)
    Beat normal_type;           ///< Duration of each normal note
    std::optional<TupletId> nested_in;  ///< Parent tuplet if nested
};

// =============================================================================
// Note and NoteGroup (SS-IR §4.4-4.5)
// =============================================================================

/**
 * @brief Single pitched event with full performance metadata
 *
 * Pitch is always concert (sounding). Transposition to written pitch
 * is applied during compilation using PartDefinition::transposition.
 */
struct Note {
    SpelledPitch pitch;                              ///< Concert pitch
    VelocityValue velocity{};                        ///< Attack intensity
    std::optional<ArticulationType> articulation;     ///< Articulation marking
    std::optional<DynamicLevel> dynamic;             ///< Dynamic at this note
    std::optional<Ornament> ornament;                ///< Ornamental figure
    bool tie_forward = false;                        ///< Tied to next same pitch
    std::optional<GraceType> grace;                  ///< Grace note type
    std::vector<TechnicalDirection> technical;        ///< Instrument-specific
    std::optional<std::string> lyric;                ///< Lyric syllable
    std::optional<NoteHeadType> notation_head;       ///< Non-standard notehead
};

/**
 * @brief One or more simultaneous notes sharing onset and duration
 */
struct NoteGroup {
    std::vector<Note> notes;                          ///< Non-empty
    Beat duration;                                    ///< Notated duration (exact)
    std::optional<TupletContext> tuplet_context;       ///< Enclosing tuplet
    std::optional<BeamGroupId> beam_group;            ///< Beam group reference
    bool slur_start = false;
    bool slur_end = false;
};

// =============================================================================
// Rest (SS-IR §4.6)
// =============================================================================

/**
 * @brief Measured silence
 */
struct RestEvent {
    Beat duration;          ///< Duration of rest
    bool visible = true;    ///< Whether to render in notation
};

// =============================================================================
// Chord Symbol (SS-IR §4.8)
// =============================================================================

/**
 * @brief Lead-sheet / Roman numeral harmonic annotation as event
 */
struct ChordSymbolEvent {
    SpelledPitch root;                          ///< Chord root
    std::string quality;                        ///< Chord quality name
    std::optional<SpelledPitch> bass;           ///< Slash bass note
    std::optional<std::string> roman;           ///< Roman numeral in key
    std::vector<std::string> extensions;        ///< e.g. "add9", "#11"
};

// =============================================================================
// Direction (SS-IR §4.7)
// =============================================================================

/**
 * @brief Performance instruction / notation annotation types
 */
enum class DirectionType : std::uint8_t {
    Text,               ///< Arbitrary performance text
    TempoText,          ///< Tempo marking text (Allegro, etc.)
    ClefChange,         ///< Change of clef
    Coda,               ///< Coda navigation symbol
    Segno,              ///< Segno navigation symbol
    DoubleBarline,      ///< Visual barline marker
    FinalBarline,       ///< End barline
    RepeatStart,        ///< Repeat open
    RepeatEnd,          ///< Repeat close
    OttavaStart,        ///< 8va/8vb/15ma start
    OttavaEnd,          ///< Ottava end
    PedalDown,          ///< Sustain pedal engage
    PedalUp,            ///< Sustain pedal release
    BreathMark,         ///< Breath/phrasing mark
    Caesura              ///< Full stop / grand pause
};

/**
 * @brief Performance instruction event at a time point
 */
struct ScoreDirection {
    DirectionType type;
    std::optional<std::string> text;           ///< For Text, TempoText
    std::optional<Clef> new_clef;              ///< For ClefChange
    std::int8_t ottava_shift = 0;              ///< Semitones: +12, -12, +24, -24
};

// =============================================================================
// Event (SS-IR §4.3) — sum type
// =============================================================================

/// Payload discriminant for Event
using EventPayload = std::variant<
    NoteGroup,
    RestEvent,
    ChordSymbolEvent,
    ScoreDirection
>;

/**
 * @brief Atomic unit of musical content within a voice
 *
 * offset is relative to the start of the enclosing measure.
 * Invariant: offset in [Beat(0,1), measure_duration)
 */
struct Event {
    EventId id;
    Beat offset;            ///< Start time relative to measure start
    EventPayload payload;

    [[nodiscard]] bool is_note_group() const noexcept {
        return std::holds_alternative<NoteGroup>(payload);
    }
    [[nodiscard]] bool is_rest() const noexcept {
        return std::holds_alternative<RestEvent>(payload);
    }
    [[nodiscard]] bool is_chord_symbol() const noexcept {
        return std::holds_alternative<ChordSymbolEvent>(payload);
    }
    [[nodiscard]] bool is_direction() const noexcept {
        return std::holds_alternative<ScoreDirection>(payload);
    }

    [[nodiscard]] const NoteGroup* as_note_group() const noexcept {
        return std::get_if<NoteGroup>(&payload);
    }
    [[nodiscard]] const RestEvent* as_rest() const noexcept {
        return std::get_if<RestEvent>(&payload);
    }

    /// Duration of this event (0 for directionless events)
    [[nodiscard]] Beat duration() const noexcept {
        if (auto* ng = std::get_if<NoteGroup>(&payload)) return ng->duration;
        if (auto* r = std::get_if<RestEvent>(&payload)) return r->duration;
        return Beat::zero();
    }
};

// =============================================================================
// Beam Group (SS-IR §4.10)
// =============================================================================

/**
 * @brief Set of events sharing a beam in notation
 *
 * All events must belong to the same voice/measure and have
 * duration shorter than a quarter note.
 */
struct BeamGroup {
    BeamGroupId id;
    std::vector<EventId> event_ids;      ///< Ordered events under beam
    std::vector<std::uint8_t> beam_breaks;  ///< Indices where secondary beams break
};

// =============================================================================
// Voice and Measure (SS-IR §4.1-4.2)
// =============================================================================

/**
 * @brief Monophonic/chordal stream of events within a measure
 *
 * Events are ordered by offset; no overlapping note events.
 */
struct Voice {
    std::uint8_t voice_index;               ///< 0-indexed within measure
    std::vector<Event> events;              ///< Ordered by offset
    std::vector<BeamGroup> beam_groups;     ///< Beam groupings for this voice
};

/**
 * @brief Single bar within a part
 *
 * Invariant: total notated duration of each voice equals the measure
 * duration derived from the applicable TimeSignature.
 */
struct Measure {
    std::uint32_t bar_number;                ///< 1-indexed
    std::vector<Voice> voices;               ///< Non-empty
    std::optional<KeySignature> local_key;   ///< Local key override
    std::optional<TimeSignature> local_time; ///< Local time sig override
};

// =============================================================================
// Score Section (SS-IR §2.6) — hierarchical formal structure
// =============================================================================

/**
 * @brief Labelled time span for formal structure annotation
 *
 * Distinct from Form::Section (FMST001A) which represents analytical
 * phrase-level structure. ScoreSection is the document-level navigation
 * and structural annotation used in the SectionMap.
 */
struct ScoreSection {
    SectionId id;
    std::string label;                           ///< "A", "Verse", "Development"
    ScoreTime start;                             ///< Inclusive
    ScoreTime end;                               ///< Exclusive
    std::vector<ScoreSection> children;          ///< Nested subsections
    std::optional<FormFunction> form_function;   ///< Formal role
};

// =============================================================================
// Global Maps — ordered sequences over score time
// =============================================================================

/// Entry in KeySignatureMap (§2.4)
struct KeySignatureEntry {
    ScoreTime position;
    KeySignature key;
};

/// Entry in TimeSignatureMap (§2.5)
struct TimeSignatureEntry {
    std::uint32_t bar;            ///< Bar where this takes effect
    TimeSignature time_signature;
};

/// TempoMap: ordered by position; at least one entry at bar 1
using TempoMap = std::vector<TempoEvent>;

/// KeySignatureMap: ordered by position; at least one entry at bar 1
using KeySignatureMap = std::vector<KeySignatureEntry>;

/// TimeSignatureMap: ordered by bar; at least one entry at bar 1
using TimeSignatureMap = std::vector<TimeSignatureEntry>;

/// SectionMap: tree of ScoreSections covering entire score duration
using SectionMap = std::vector<ScoreSection>;

// =============================================================================
// Annotation Layers (SS-IR §6-7)
// =============================================================================

/**
 * @brief Identifies a non-chord tone within the score (§6.3)
 */
struct NonChordToneAnnotation {
    EventId event_id;
    std::uint8_t note_index;         ///< Which note within NoteGroup
    NonChordToneType type;
};

/**
 * @brief Single harmonic analysis entry (§6.2)
 */
struct HarmonicAnnotation {
    ScoreTime position;
    Beat duration;                    ///< Duration of harmonic region
    ChordVoicing chord;              ///< The chord (Theory Spec type)
    KeySignature key_context;        ///< Local key for Roman numeral
    std::string roman_numeral;
    ScoreHarmonicFunction function;
    std::optional<std::string> secondary_function;  ///< e.g. "V/V"
    std::vector<NonChordToneAnnotation> non_chord_tones;
    std::optional<CadenceType> cadence;
    float confidence = 1.0f;         ///< 1.0 = certain
};

/// Harmonic annotation layer: ordered by position
using HarmonicAnnotationLayer = std::vector<HarmonicAnnotation>;

/**
 * @brief Single orchestration annotation entry (§7.3)
 */
struct OrchestrationAnnotation {
    PartId part_id;
    ScoreTime start;
    ScoreTime end;
    TexturalRole role;
    std::optional<TextureType> texture;
    std::optional<DynamicBalance> dynamic_balance;
};

/// Orchestration layer: ordered by start position
using OrchestrationLayer = std::vector<OrchestrationAnnotation>;

// =============================================================================
// Part (SS-IR §3.1)
// =============================================================================

/**
 * @brief Single instrumental line in the score
 *
 * Invariant: measures.size() == score.metadata.total_bars
 */
struct Part {
    PartId id;
    PartDefinition definition;
    std::vector<Measure> measures;
    std::vector<PartDirective> part_directives;
    std::vector<Hairpin> hairpins;
};

// =============================================================================
// Score Metadata (SS-IR §2.2)
// =============================================================================

/**
 * @brief Descriptive metadata for the score document
 */
struct ScoreMetadata {
    std::string title;
    std::optional<std::string> subtitle;
    std::optional<std::string> composer;
    std::optional<std::string> arranger;
    std::optional<std::string> opus;
    std::uint64_t created_at = 0;        ///< Unix timestamp
    std::uint64_t modified_at = 0;       ///< Unix timestamp
    std::uint32_t total_bars = 0;
    std::vector<std::string> tags;
};

// =============================================================================
// Score — top-level document (SS-IR §2.1)
// =============================================================================

/**
 * @brief Root node of the Score IR document hierarchy
 *
 * Invariants:
 * - parts is non-empty
 * - All parts have the same total duration (measured in score time)
 * - tempo_map, key_map, time_map defined for entire score duration
 * - tempo_map has entry at bar 1
 * - key_map has entry at bar 1
 * - time_map has entry at bar 1
 * - version increases monotonically (including through undo/redo)
 */
struct Score {
    ScoreId id;
    ScoreMetadata metadata;
    TempoMap tempo_map;
    KeySignatureMap key_map;
    TimeSignatureMap time_map;
    SectionMap section_map;
    std::vector<RehearsalMark> rehearsal_marks;
    std::vector<Part> parts;
    HarmonicAnnotationLayer harmonic_annotations;
    OrchestrationLayer orchestration_annotations;
    std::uint64_t version = 1;           ///< Monotonically increasing edit counter
};

}  // namespace Sunny::Core
