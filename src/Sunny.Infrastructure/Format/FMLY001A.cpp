/**
 * @file FMLY001A.cpp
 * @brief LilyPond notation writer implementation
 *
 * Component: FMLY001A
 */

#include "FMLY001A.h"

#include "../../Sunny.Core/Pitch/PTPC001A.h"

#include <sstream>

namespace Sunny::Infrastructure::Format {

namespace {

/// LilyPond letter names (lowercase)
constexpr std::array<char, 7> LY_LETTER = {'c', 'd', 'e', 'f', 'g', 'a', 'b'};

/// Build octave tick marks
/// LilyPond: c' = C4 (middle C), c = C3, c'' = C5, c, = C2
std::string ly_octave_marks(int8_t octave) {
    std::string marks;
    if (octave >= 4) {
        for (int i = 0; i < octave - 3; ++i) marks += '\'';
    } else {
        for (int i = 0; i < 3 - octave; ++i) marks += ',';
    }
    return marks;
}

/// Check whether a Beat is a simple power-of-two duration
/// Returns the LilyPond denominator (1, 2, 4, 8, 16, 32, 64) or 0 if not
int as_power_of_two_duration(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    if (r.numerator != 1) return 0;
    // Check denominator is power of 2 and in range
    auto d = r.denominator;
    if (d == 1 || d == 2 || d == 4 || d == 8 || d == 16 || d == 32 || d == 64) {
        return static_cast<int>(d);
    }
    return 0;
}

/// Check whether a Beat is a dotted duration (3/2 * base)
/// Returns the LilyPond denominator of the base or 0 if not dotted
int as_dotted_duration(Sunny::Core::Beat dur) {
    auto r = dur.reduce();
    if (r.numerator != 3) return 0;
    // 3/8 = dotted quarter (base = 4), 3/4 = dotted half (base = 2)
    // 3/16 = dotted eighth (base = 8), etc.
    // base_denom = denominator * 2 / 3... no, ratio is 3/(2*base)
    // dur = 3/d where d is the denominator.
    // dur = (base_dur) * 3/2 = (1/base) * 3/2 = 3/(2*base)
    // So d = 2*base, base = d/2
    if (r.denominator % 2 != 0) return 0;
    auto base = r.denominator / 2;
    if (base == 1 || base == 2 || base == 4 || base == 8 || base == 16 || base == 32) {
        return static_cast<int>(base);
    }
    return 0;
}

}  // anonymous namespace

std::string ly_pitch(Sunny::Core::SpelledPitch sp) {
    std::string result;
    result += LY_LETTER[sp.letter < 7 ? sp.letter : 0];

    // Accidentals: Dutch convention
    // Special cases for E-flat and A-flat: "ees" and "aes" (not "es")
    if (sp.accidental > 0) {
        for (int i = 0; i < sp.accidental; ++i) {
            result += "is";
        }
    } else if (sp.accidental < 0) {
        for (int i = 0; i < -sp.accidental; ++i) {
            if (i == 0 && (sp.letter == 2 || sp.letter == 5)) {
                // E (2) or A (5): first flat is "es" not just "s"
                result += "es";
            } else {
                result += "es";
            }
        }
    }

    result += ly_octave_marks(sp.octave);
    return result;
}

Sunny::Core::Result<std::string> ly_duration(Sunny::Core::Beat dur) {
    // Check power-of-two first
    int p2 = as_power_of_two_duration(dur);
    if (p2 > 0) {
        return std::to_string(p2);
    }

    // Check dotted
    int dotted = as_dotted_duration(dur);
    if (dotted > 0) {
        return std::to_string(dotted) + ".";
    }

    return std::unexpected(Sunny::Core::ErrorCode::FormatError);
}

Sunny::Core::Result<std::string> ly_note(Sunny::Core::SpelledPitch sp, Sunny::Core::Beat dur) {
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());
    return ly_pitch(sp) + *dur_str;
}

Sunny::Core::Result<std::string> ly_rest(Sunny::Core::Beat dur) {
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());
    return "r" + *dur_str;
}

Sunny::Core::Result<std::string> ly_chord(
    std::span<const Sunny::Core::SpelledPitch> pitches,
    Sunny::Core::Beat dur)
{
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());

    std::string result = "<";
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        if (i > 0) result += ' ';
        result += ly_pitch(pitches[i]);
    }
    result += ">";
    result += *dur_str;
    return result;
}

std::string ly_key(Sunny::Core::SpelledPitch tonic, bool is_major) {
    // ly_pitch gives the pitch with octave marks; for \key we only need
    // the note name part (no octave). Produce a pitch at octave 3 to get
    // zero tick marks, since octave 3 = no marks in LilyPond.
    Sunny::Core::SpelledPitch no_octave{tonic.letter, tonic.accidental, 3};
    return "\\key " + ly_pitch(no_octave) + " \\" + (is_major ? "major" : "minor");
}

std::string ly_time_signature(int num, int den) {
    return "\\time " + std::to_string(num) + "/" + std::to_string(den);
}

Sunny::Core::Result<std::string> ly_fragment(
    std::span<const Sunny::Core::NoteEvent> events,
    int key_lof)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (i > 0) out << ' ';
        const auto& ev = events[i];

        // Determine octave from MIDI pitch
        int8_t octave = static_cast<int8_t>(ev.pitch / 12 - 1);
        auto pc = Sunny::Core::pitch_class(ev.pitch);
        auto sp = Sunny::Core::default_spelling(pc, key_lof, octave);

        auto note = ly_note(sp, ev.duration);
        if (!note) return std::unexpected(note.error());
        out << *note;
    }
    return out.str();
}

}  // namespace Sunny::Infrastructure::Format
