/**
 * @file lilypond.hpp
 * @brief LilyPond notation writer
 *
 *
 * Converts Sunny types to LilyPond notation strings using the Dutch
 * naming convention (cis, ees, fis, etc.).
 *
 * Invariants:
 * - ly_pitch produces valid LilyPond Dutch pitch names or rejects invalid spelling
 * - ly_duration preserves every positive rational through conventional tokens
 *   or an exact duration multiplier
 * - ly_fragment preserves pitch, mute/rest state, absolute onset, and duration;
 *   non-default release velocity is rejected because the compact notation
 *   profile has no carrier for it
 */

#pragma once

#include <span>
#include <string>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>

namespace sunny::infrastructure::formats {

/// SpelledPitch -> LilyPond note name (Dutch convention)
/// C4 -> "c'", C#4 -> "cis'", Bb3 -> "bes", D##5 -> "disis''"
[[nodiscard]] sunny::core::Result<std::string> ly_pitch(sunny::core::SpelledPitch sp);

/// Positive Beat duration -> exact LilyPond duration string
/// 1/4 -> "4", 3/8 -> "4.", arbitrary 5/8 -> "1*5/8"
[[nodiscard]] sunny::core::Result<std::string> ly_duration(sunny::core::Beat dur);

/// Single note: pitch + duration -> "cis'4"
[[nodiscard]] sunny::core::Result<std::string> ly_note(sunny::core::SpelledPitch sp,
                                                       sunny::core::Beat dur);

/// Rest -> "r4", "r2", etc.
[[nodiscard]] sunny::core::Result<std::string> ly_rest(sunny::core::Beat dur);

/// Chord -> "<c' e' g'>4"
[[nodiscard]] sunny::core::Result<std::string>
ly_chord(std::span<const sunny::core::SpelledPitch> pitches, sunny::core::Beat dur);

/// Key signature -> "\\key c \\major" or "\\key a \\minor"
[[nodiscard]] sunny::core::Result<std::string> ly_key(sunny::core::SpelledPitch tonic,
                                                      bool is_major);

/// Time signature -> "\\time 4/4"
[[nodiscard]] sunny::core::Result<std::string> ly_time_signature(int num, int den);

/// Sequence of NoteEvents -> LilyPond fragment
/// Uses key_lof for default spelling of MidiNote -> SpelledPitch
/// Rejects non-default release velocity rather than silently discarding it.
[[nodiscard]] sunny::core::Result<std::string>
ly_fragment(std::span<const sunny::core::NoteEvent> events, int key_lof = 0);

} // namespace sunny::infrastructure::formats
