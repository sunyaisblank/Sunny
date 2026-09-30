/**
 * @file abc.hpp
 * @brief ABC notation reader
 *
 *
 * Parses ABC notation (a text-based music notation format commonly used
 * for folk and traditional music) into NoteEvent sequences.
 *
 * Invariants:
 * - K: field must be present (terminates header)
 * - the supported K: profile is one tonic plus a standard major/minor/church
 *   mode; unsupported explicit signatures and clef/transposition modifiers
 *   are rejected rather than ignored
 * - Note pitches are computed from letter + octave modifiers + accidentals + key context
 * - Duration is relative to L: (default note length)
 * - absent M: is represented as free metre; absent L: follows the ABC 2.1
 *   meter-derived default; tempo retains its explicit beat unit
 * - the body profile is monophonic notes, z rests, plain bar lines, and
 *   comments; richer constructs are rejected rather than flattened
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

/// One retained string-valued ABC information field from the compact profile
struct AbcDescriptiveField {
    char code;
    std::string value;
};

/// Parsed ABC header fields
struct AbcHeader {
    std::int64_t reference_number = 0;                   ///< Required positive X: tune identifier
    std::string title;                                   ///< First and only supported T: field
    std::string key;                                     ///< K: field (e.g. "C", "Am", "D")
    int metre_num = 0;                                   ///< M: numerator; zero in free metre
    int metre_den = 0;                                   ///< M: denominator; zero in free metre
    bool metre_explicit = false;                         ///< Whether an M: field was present
    bool free_metre = true;                              ///< Absent M: and M:none are free metre
    sunny::core::Beat default_length{1, 8};              ///< Explicit or meter-derived L:
    bool default_length_explicit = false;                ///< Whether an L: field was present
    std::optional<int> tempo_bpm;                        ///< Q: positive integer count
    sunny::core::Beat tempo_unit{1, 4};                  ///< Beat unit associated with tempo_bpm
    std::vector<AbcDescriptiveField> descriptive_fields; ///< Ordered A/B/C/D/F/G/H/N/O/R/S/Z data
};

/// Result of parsing an ABC tune
struct AbcParseResult {
    AbcHeader header;
    std::vector<sunny::core::NoteEvent> notes;
    sunny::core::PitchClass key_root; ///< Compatibility pitch-class view
    bool is_minor;                    ///< True only for minor/Aeolian family
    sunny::core::SpelledPitch key_tonic;
    std::string key_mode; ///< Canonical major/minor/church-mode name
    int key_fifths = 0;   ///< Exact traditional signature in [-7, 7]
};

/// Parse a complete ABC tune
[[nodiscard]] sunny::core::Result<AbcParseResult> parse_abc(std::string_view text);

} // namespace sunny::infrastructure::formats
