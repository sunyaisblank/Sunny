/**
 * @file FMLY001A.h
 * @brief LilyPond notation writer
 *
 * Component: FMLY001A
 * Domain: FM (Format) | Category: LY (LilyPond)
 *
 * Converts Sunny types to LilyPond notation strings using the Dutch
 * naming convention (cis, ees, fis, etc.).
 *
 * Invariants:
 * - ly_pitch produces valid LilyPond Dutch pitch names
 * - ly_duration only succeeds for representable durations (powers of 2, dotted)
 * - ly_fragment preserves pitch and rhythm content
 */

#pragma once

#include "../../Sunny.Core/Tensor/TNTP001A.h"
#include "../../Sunny.Core/Pitch/PTSP001A.h"
#include "../../Sunny.Core/Tensor/TNBT001A.h"
#include "../../Sunny.Core/Tensor/TNEV001A.h"

#include <span>
#include <string>

namespace Sunny::Infrastructure::Format {

/// SpelledPitch -> LilyPond note name (Dutch convention)
/// C4 -> "c'", C#4 -> "cis'", Bb3 -> "bes", D##5 -> "disis''"
[[nodiscard]] std::string ly_pitch(Sunny::Core::SpelledPitch sp);

/// Beat duration -> LilyPond duration string
/// 1/4 -> "4", 1/2 -> "2", 3/8 -> "4.", 1/1 -> "1"
[[nodiscard]] Sunny::Core::Result<std::string> ly_duration(Sunny::Core::Beat dur);

/// Single note: pitch + duration -> "cis'4"
[[nodiscard]] Sunny::Core::Result<std::string> ly_note(
    Sunny::Core::SpelledPitch sp, Sunny::Core::Beat dur);

/// Rest -> "r4", "r2", etc.
[[nodiscard]] Sunny::Core::Result<std::string> ly_rest(Sunny::Core::Beat dur);

/// Chord -> "<c' e' g'>4"
[[nodiscard]] Sunny::Core::Result<std::string> ly_chord(
    std::span<const Sunny::Core::SpelledPitch> pitches,
    Sunny::Core::Beat dur);

/// Key signature -> "\\key c \\major" or "\\key a \\minor"
[[nodiscard]] std::string ly_key(Sunny::Core::SpelledPitch tonic, bool is_major);

/// Time signature -> "\\time 4/4"
[[nodiscard]] std::string ly_time_signature(int num, int den);

/// Sequence of NoteEvents -> LilyPond fragment
/// Uses key_lof for default spelling of MidiNote -> SpelledPitch
[[nodiscard]] Sunny::Core::Result<std::string> ly_fragment(
    std::span<const Sunny::Core::NoteEvent> events,
    int key_lof = 0);

}  // namespace Sunny::Infrastructure::Format
