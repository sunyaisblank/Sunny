/**
 * @file FMAB001A.h
 * @brief ABC notation reader
 *
 * Component: FMAB001A
 * Domain: FM (Format) | Category: AB (ABC notation)
 *
 * Parses ABC notation (a text-based music notation format commonly used
 * for folk and traditional music) into NoteEvent sequences.
 *
 * Invariants:
 * - K: field must be present (terminates header)
 * - Note pitches are computed from letter + octave modifiers + accidentals + key context
 * - Duration is relative to L: (default note length)
 */

#pragma once

#include "../../Sunny.Core/Tensor/TNTP001A.h"
#include "../../Sunny.Core/Tensor/TNBT001A.h"
#include "../../Sunny.Core/Tensor/TNEV001A.h"

#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Infrastructure::Format {

/// Parsed ABC header fields
struct AbcHeader {
    std::string title;                ///< T: field
    std::string key;                  ///< K: field (e.g. "C", "Am", "D")
    int metre_num = 4;                ///< M: numerator
    int metre_den = 4;                ///< M: denominator
    Sunny::Core::Beat default_length; ///< L: default note length
    int tempo_bpm = 120;              ///< Q: tempo
};

/// Result of parsing an ABC tune
struct AbcParseResult {
    AbcHeader header;
    std::vector<Sunny::Core::NoteEvent> notes;
    Sunny::Core::PitchClass key_root;
    bool is_minor;
};

/// Parse a complete ABC tune
[[nodiscard]] Sunny::Core::Result<AbcParseResult> parse_abc(std::string_view text);

}  // namespace Sunny::Infrastructure::Format
