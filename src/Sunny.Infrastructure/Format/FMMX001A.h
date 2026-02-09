/**
 * @file FMMX001A.h
 * @brief MusicXML reader/writer (score-partwise)
 *
 * Component: FMMX001A
 * Domain: FM (Format) | Category: MX (MusicXML)
 *
 * Reads and writes MusicXML (score-partwise) using pugixml.
 * Preserves enharmonic spelling through SpelledPitch.
 *
 * Invariants:
 * - parse_musicxml ∘ write_musicxml = identity (for notes, keys, time sigs)
 * - SpelledPitch enharmonic spelling is preserved through round-trip
 * - divisions per quarter note is consistent within a part
 */

#pragma once

#include "../../Sunny.Core/Tensor/TNTP001A.h"
#include "../../Sunny.Core/Pitch/PTSP001A.h"
#include "../../Sunny.Core/Tensor/TNBT001A.h"
#include "../../Sunny.Core/Tensor/TNEV001A.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Sunny::Infrastructure::Format {

/// A single note in MusicXML representation
struct MusicXmlNote {
    Sunny::Core::SpelledPitch pitch;
    Sunny::Core::Beat duration;
    bool is_rest = false;
    bool is_chord = false;  ///< part of a chord (not the first note)
    int voice = 1;
};

/// A single measure
struct MusicXmlMeasure {
    int number;
    std::vector<MusicXmlNote> notes;
    std::optional<std::pair<int, int>> time_signature;  ///< if changes
    std::optional<Sunny::Core::SpelledPitch> key_tonic; ///< if changes
    std::optional<bool> key_is_major;
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
    int divisions = 1;  ///< divisions per quarter note
};

/// Parse MusicXML (score-partwise) from string
[[nodiscard]] Sunny::Core::Result<MusicXmlScore> parse_musicxml(std::string_view xml);

/// Serialise to MusicXML string
[[nodiscard]] Sunny::Core::Result<std::string> write_musicxml(const MusicXmlScore& score);

/// Flatten a MusicXML score to NoteEvents (first part only)
[[nodiscard]] std::vector<Sunny::Core::NoteEvent> musicxml_to_note_events(
    const MusicXmlScore& score);

/// Convert NoteEvents to a basic MusicXML score
[[nodiscard]] MusicXmlScore note_events_to_musicxml(
    std::span<const Sunny::Core::NoteEvent> events,
    int key_lof = 0);

}  // namespace Sunny::Infrastructure::Format
