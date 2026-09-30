/**
 * @file musicxml.hpp
 * @brief MusicXML reader/writer (score-partwise)
 *
 *
 * Reads and writes MusicXML (score-partwise) using pugixml.
 * Preserves enharmonic spelling through SpelledPitch.
 *
 * Invariants:
 * - parse_musicxml ∘ write_musicxml = identity for notes, traditional keys,
 *   and time signatures in this low-level profile
 * - SpelledPitch enharmonic spelling is preserved through round-trip
 * - source divisions declarations are normalized into exact Beat durations;
 *   they are encoding-local state rather than a public score property
 * - one non-stacked harmony chord per <harmony> is retained at an exact
 *   measure-local offset; root-or-numeral, kind, bass, inversion, and ordered
 *   integer-semitone degree operations are represented explicitly
 * - compact writer timing is exact within positive int-valued divisions and
 *   duration units; malformed or overflowing rational timing is rejected
 * - optional graphical type/dots are emitted only when exactly derivable
 * - flat NoteEvent adapters accept one sequential part, preserve rests/gaps
 *   and equal-duration chords, and reject overlap or discarded attack/release
 *   velocity state
 * - non-traditional key-step/key-alter signatures are rejected because this
 *   compact model does not store their explicit diatonic alteration sequence
 * - harmony function/stacking/frame/multi-staff constructs, fractional
 *   harmony alterations, and unretained harmony attributes are rejected
 * - integer divisions/durations and integer pitch alterations only; MusicXML
 *   decimal timing, microtones, additive/composite metre, string voice labels,
 *   non-numeric measure labels, cursor backup/forward, multi-staff, grace/cue,
 *   unpitched, and transposition constructs are rejected rather than defaulted
 */

#pragma once

#include <optional>
#include <span>
#include <string>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/score/document.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

/// A single note in MusicXML representation
struct MusicXmlNote {
    sunny::core::SpelledPitch pitch;
    sunny::core::Beat duration;
    bool is_rest = false;
    bool is_chord = false; ///< part of a chord (not the first note)
    int voice = 1;
};

/** One exact, measure-local harmony point in the compact profile. */
struct MusicXmlHarmony {
    sunny::core::Beat offset{};
    sunny::core::ChordSymbolEvent symbol{};
};

/// A single measure
struct MusicXmlMeasure {
    int number;
    std::vector<MusicXmlNote> notes;
    std::vector<MusicXmlHarmony> harmonies;
    std::optional<std::pair<int, int>> time_signature; ///< if changes
    // Compatibility-derived analysis fields for the traditional key profile.
    // Modal keys preserve their tonic where derivable; key_is_major is populated
    // only for major/Ionian and minor/Aeolian families.
    std::optional<sunny::core::SpelledPitch> key_tonic;
    std::optional<bool> key_is_major;
    // Exact MusicXML traditional-key evidence.  When key_fifths is populated,
    // the writer treats it as authoritative and preserves an optional mode
    // element verbatim from MusicXML's closed mode vocabulary.
    std::optional<int> key_fifths;
    std::optional<std::string> key_mode;
};

/// A single part
struct MusicXmlPart {
    std::string id;
    std::string name;
    std::vector<MusicXmlMeasure> measures;
};

/// A MusicXML score
struct MusicXmlScore {
    std::string title;
    std::vector<MusicXmlPart> parts;
};

/// Parse MusicXML (score-partwise) from string
[[nodiscard]] sunny::core::Result<MusicXmlScore> parse_musicxml(std::string_view xml);

/// Serialise to MusicXML string
[[nodiscard]] sunny::core::Result<std::string> write_musicxml(const MusicXmlScore& score);

/// Flatten one sequential compact MusicXML part to exact NoteEvents
[[nodiscard]] sunny::core::Result<std::vector<sunny::core::NoteEvent>>
musicxml_to_note_events(const MusicXmlScore& score);

/// Convert an ordered non-overlapping NoteEvent stream to basic MusicXML
/// Non-default release velocity is rejected rather than silently discarded.
[[nodiscard]] sunny::core::Result<MusicXmlScore>
note_events_to_musicxml(std::span<const sunny::core::NoteEvent> events, int key_lof = 0);

} // namespace sunny::infrastructure::formats
