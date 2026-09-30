/**
 * @file lilypond.cpp
 * @brief LilyPond notation writer implementation
 *
 */

#include <sstream>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/infrastructure/formats/lilypond.hpp>

namespace sunny::infrastructure::formats {

namespace {

/// LilyPond letter names (lowercase)
constexpr std::array<char, 7> LY_LETTER = {'c', 'd', 'e', 'f', 'g', 'a', 'b'};

/// Build octave tick marks
/// LilyPond: c' = C4 (middle C), c = C3, c'' = C5, c, = C2
std::string ly_octave_marks(int8_t octave) {
    std::string marks;
    if (octave >= 4) {
        for (int i = 0; i < octave - 3; ++i)
            marks += '\'';
    } else {
        for (int i = 0; i < 3 - octave; ++i)
            marks += ',';
    }
    return marks;
}

struct LilyGraphicDuration {
    std::string_view token;
    int numerator;
    int denominator;
};

std::optional<std::string> conventional_duration(sunny::core::Beat duration) {
    constexpr std::array<LilyGraphicDuration, 14> base_types = {{
        {"1024", 1, 1024},
        {"512", 1, 512},
        {"256", 1, 256},
        {"128", 1, 128},
        {"64", 1, 64},
        {"32", 1, 32},
        {"16", 1, 16},
        {"8", 1, 8},
        {"4", 1, 4},
        {"2", 1, 2},
        {"1", 1, 1},
        {"\\breve", 2, 1},
        {"\\longa", 4, 1},
        {"\\maxima", 8, 1},
    }};

    for (const auto& base : base_types) {
        const sunny::core::Beat base_duration{base.numerator, base.denominator};
        for (int dots = 0; dots <= 62; ++dots) {
            const auto denominator = std::uint64_t{1} << dots;
            const auto numerator = (denominator << 1) - 1;
            const auto candidate =
                sunny::core::checked_mul(base_duration,
                                         sunny::core::Beat{static_cast<std::int64_t>(numerator),
                                                           static_cast<std::int64_t>(denominator)});
            if (candidate && *candidate == duration) {
                std::string result{base.token};
                result.append(static_cast<std::size_t>(dots), '.');
                return result;
            }
        }
    }
    return std::nullopt;
}

} // anonymous namespace

sunny::core::Result<std::string> ly_pitch(sunny::core::SpelledPitch sp) {
    if (sp.letter >= LY_LETTER.size()) return std::unexpected(sunny::core::ErrorCode::FormatError);
    std::string result;
    result += LY_LETTER[sp.letter];

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

sunny::core::Result<std::string> ly_duration(sunny::core::Beat dur) {
    if (dur.numerator() <= 0) return std::unexpected(sunny::core::ErrorCode::FormatError);
    const auto reduced = dur.reduce();
    if (const auto conventional = conventional_duration(reduced)) return *conventional;
    std::string exact = "1*" + std::to_string(reduced.numerator());
    if (reduced.denominator() != 1) exact += "/" + std::to_string(reduced.denominator());
    return exact;
}

sunny::core::Result<std::string> ly_note(sunny::core::SpelledPitch sp, sunny::core::Beat dur) {
    auto pitch = ly_pitch(sp);
    if (!pitch) return std::unexpected(pitch.error());
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());
    return *pitch + *dur_str;
}

sunny::core::Result<std::string> ly_rest(sunny::core::Beat dur) {
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());
    return "r" + *dur_str;
}

sunny::core::Result<std::string> ly_chord(std::span<const sunny::core::SpelledPitch> pitches,
                                          sunny::core::Beat dur) {
    if (pitches.empty()) return std::unexpected(sunny::core::ErrorCode::FormatError);
    auto dur_str = ly_duration(dur);
    if (!dur_str) return std::unexpected(dur_str.error());

    std::string result = "<";
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        if (i > 0) result += ' ';
        auto pitch = ly_pitch(pitches[i]);
        if (!pitch) return std::unexpected(pitch.error());
        result += *pitch;
    }
    result += ">";
    result += *dur_str;
    return result;
}

sunny::core::Result<std::string> ly_key(sunny::core::SpelledPitch tonic, bool is_major) {
    // ly_pitch gives the pitch with octave marks; for \key we only need
    // the note name part (no octave). Produce a pitch at octave 3 to get
    // zero tick marks, since octave 3 = no marks in LilyPond.
    sunny::core::SpelledPitch no_octave{tonic.letter, tonic.accidental, 3};
    auto pitch = ly_pitch(no_octave);
    if (!pitch) return std::unexpected(pitch.error());
    return "\\key " + *pitch + " \\" + (is_major ? "major" : "minor");
}

sunny::core::Result<std::string> ly_time_signature(int num, int den) {
    if (num <= 0 || den <= 0) return std::unexpected(sunny::core::ErrorCode::FormatError);
    return "\\time " + std::to_string(num) + "/" + std::to_string(den);
}

sunny::core::Result<std::string> ly_fragment(std::span<const sunny::core::NoteEvent> events,
                                             int key_lof) {
    std::ostringstream out;
    sunny::core::Beat cursor = sunny::core::Beat::zero();
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];
        if (ev.start_time < sunny::core::Beat::zero() || ev.duration <= sunny::core::Beat::zero() ||
            ev.start_time < cursor)
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        if (ev.release_velocity != 64)
            return std::unexpected(sunny::core::ErrorCode::TargetValueUnrepresentable);

        if (ev.start_time > cursor) {
            const auto gap = sunny::core::checked_sub(ev.start_time, cursor);
            if (!gap) return std::unexpected(sunny::core::ErrorCode::FormatError);
            const auto rest = ly_rest(*gap);
            if (!rest) return std::unexpected(rest.error());
            if (out.tellp() > 0) out << ' ';
            out << *rest;
        }

        sunny::core::Result<std::string> event;
        if (ev.muted) {
            event = ly_rest(ev.duration);
        } else {
            const auto octave = static_cast<std::int8_t>(ev.pitch / 12 - 1);
            const auto pc = sunny::core::pitch_class(ev.pitch);
            const auto spelling = sunny::core::default_spelling(pc, key_lof, octave);
            event = ly_note(spelling, ev.duration);
        }
        if (!event) return std::unexpected(event.error());
        if (out.tellp() > 0) out << ' ';
        out << *event;

        const auto end = sunny::core::checked_add(ev.start_time, ev.duration);
        if (!end) return std::unexpected(sunny::core::ErrorCode::FormatError);
        cursor = *end;
    }
    return out.str();
}

} // namespace sunny::infrastructure::formats
