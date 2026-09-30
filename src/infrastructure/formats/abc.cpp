/**
 * @file abc.cpp
 * @brief ABC notation reader implementation
 *
 */

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/infrastructure/formats/abc.hpp>

namespace sunny::infrastructure::formats {

namespace {

/// Expand an exact traditional fifths count to per-natural-note accidentals.
std::array<int, 12> key_accidentals(int fifths) {
    std::array<int, 12> acc{}; // all zeros (naturals)

    if (fifths > 0) {
        // Sharp order: F C G D A E B
        static constexpr std::array<int, 7> SHARP_ORDER = {5, 0, 7, 2, 9, 4, 11};
        for (int i = 0; i < fifths && i < 7; ++i) {
            acc[SHARP_ORDER[i]] = 1;
        }
    } else if (fifths < 0) {
        // Flat order: B E A D G C F
        static constexpr std::array<int, 7> FLAT_ORDER = {11, 4, 9, 2, 7, 0, 5};
        for (int i = 0; i < -fifths && i < 7; ++i) {
            acc[FLAT_ORDER[i]] = -1;
        }
    }

    return acc;
}

struct ParsedKey {
    sunny::core::SpelledPitch tonic;
    sunny::core::PitchClass root;
    std::string mode;
    int fifths;
};

std::optional<std::pair<std::string_view, int>> abc_mode(std::string_view suffix) {
    std::string lower;
    lower.reserve(suffix.size());
    for (const auto ch : suffix)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

    if (lower.empty() || lower.starts_with("maj")) return {{"major", 0}};
    if (lower == "m" || lower.starts_with("min")) return {{"minor", -3}};
    if (lower.starts_with("ion")) return {{"ionian", 0}};
    if (lower.starts_with("aeo")) return {{"aeolian", -3}};
    if (lower.starts_with("mix")) return {{"mixolydian", -1}};
    if (lower.starts_with("dor")) return {{"dorian", -2}};
    if (lower.starts_with("phr")) return {{"phrygian", -4}};
    if (lower.starts_with("lyd")) return {{"lydian", 1}};
    if (lower.starts_with("loc")) return {{"locrian", -5}};
    return std::nullopt;
}

/// Parse the standard tonic + mode subset of ABC K: fields.  Explicit
/// accidental lists, clef/transposition modifiers, K:none, HP, and Hp are
/// outside this compact NoteEvent reader and are rejected rather than ignored.
sunny::core::Result<ParsedKey> parse_key(std::string_view k) {
    if (k.empty()) {
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    }

    // Trim whitespace
    while (!k.empty() && (k.front() == ' ' || k.front() == '\t'))
        k.remove_prefix(1);
    while (!k.empty() && (k.back() == ' ' || k.back() == '\t'))
        k.remove_suffix(1);

    if (k.empty()) {
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    }

    // Parse root note letter
    int base_pc;
    std::uint8_t letter;
    switch (std::toupper(static_cast<unsigned char>(k[0]))) {
    case 'C':
        base_pc = 0;
        letter = 0;
        break;
    case 'D':
        base_pc = 2;
        letter = 1;
        break;
    case 'E':
        base_pc = 4;
        letter = 2;
        break;
    case 'F':
        base_pc = 5;
        letter = 3;
        break;
    case 'G':
        base_pc = 7;
        letter = 4;
        break;
    case 'A':
        base_pc = 9;
        letter = 5;
        break;
    case 'B':
        base_pc = 11;
        letter = 6;
        break;
    default:
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    }

    std::size_t pos = 1;
    int accidental = 0;
    if (pos < k.size() && (k[pos] == '#' || k[pos] == 'b')) {
        accidental = k[pos] == '#' ? 1 : -1;
        base_pc = (base_pc + accidental + 12) % 12;
        ++pos;
    }

    while (pos < k.size() && (k[pos] == ' ' || k[pos] == '\t'))
        ++pos;
    const auto suffix = k.substr(pos);
    if (suffix.find_first_of(" \t") != std::string_view::npos)
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    const auto mode = abc_mode(suffix);
    if (!mode) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

    const sunny::core::SpelledPitch tonic{letter, static_cast<std::int8_t>(accidental), 4};
    const int fifths = sunny::core::line_of_fifths_position(tonic) + mode->second;
    if (fifths < -7 || fifths > 7) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    return ParsedKey{
        tonic, sunny::core::PitchClass::wrapped(base_pc), std::string{mode->first}, fifths};
}

/// ABC note letter to base MIDI note
/// Uppercase: C=60, D=62, E=64, F=65, G=67, A=69, B=71
/// Lowercase: c=72, d=74, e=76, f=77, g=79, a=81, b=83
int abc_letter_to_midi(char ch) {
    switch (ch) {
    case 'C':
        return 60;
    case 'D':
        return 62;
    case 'E':
        return 64;
    case 'F':
        return 65;
    case 'G':
        return 67;
    case 'A':
        return 69;
    case 'B':
        return 71;
    case 'c':
        return 72;
    case 'd':
        return 74;
    case 'e':
        return 76;
    case 'f':
        return 77;
    case 'g':
        return 79;
    case 'a':
        return 81;
    case 'b':
        return 83;
    default:
        return -1;
    }
}

bool is_abc_note(char ch) {
    return abc_letter_to_midi(ch) >= 0;
}

std::string_view trim_horizontal(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

std::optional<std::int64_t> parse_positive_integer(std::string_view value) {
    value = trim_horizontal(value);
    if (value.empty()) return std::nullopt;
    std::int64_t parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed <= 0)
        return std::nullopt;
    return parsed;
}

std::optional<sunny::core::Beat> parse_positive_rational(std::string_view value) {
    value = trim_horizontal(value);
    const auto slash = value.find('/');
    if (slash == std::string_view::npos) {
        const auto integer = parse_positive_integer(value);
        if (!integer) return std::nullopt;
        return sunny::core::Beat{*integer, 1};
    }
    if (value.find('/', slash + 1) != std::string_view::npos) return std::nullopt;
    const auto numerator = parse_positive_integer(value.substr(0, slash));
    const auto denominator = parse_positive_integer(value.substr(slash + 1));
    if (!numerator || !denominator) return std::nullopt;
    return sunny::core::Beat{*numerator, *denominator};
}

void derive_default_length(AbcHeader& header) {
    if (header.default_length_explicit) return;
    if (header.free_metre) {
        header.default_length = sunny::core::Beat{1, 8};
        return;
    }
    const sunny::core::Beat metre{header.metre_num, header.metre_den};
    header.default_length =
        metre < sunny::core::Beat{3, 4} ? sunny::core::Beat{1, 16} : sunny::core::Beat{1, 8};
}

struct ParsedLength {
    sunny::core::Beat multiplier;
    std::size_t next;
};

sunny::core::Result<ParsedLength> parse_note_length(std::string_view line, std::size_t start) {
    std::int64_t numerator = 1;
    std::int64_t denominator = 1;
    std::size_t cursor = start;

    if (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor]))) {
        const auto digit_end = line.find_first_not_of("0123456789", cursor);
        const auto token_end = digit_end == std::string_view::npos ? line.size() : digit_end;
        const auto [end, error] =
            std::from_chars(line.data() + cursor, line.data() + token_end, numerator);
        if (error != std::errc{} || end != line.data() + token_end || numerator <= 0)
            return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
        cursor = token_end;
    }

    std::size_t slash_count = 0;
    while (cursor < line.size() && line[cursor] == '/') {
        ++slash_count;
        ++cursor;
    }
    if (slash_count > 0) {
        if (cursor < line.size() && std::isdigit(static_cast<unsigned char>(line[cursor]))) {
            if (slash_count != 1) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
            const auto digit_end = line.find_first_not_of("0123456789", cursor);
            const auto token_end = digit_end == std::string_view::npos ? line.size() : digit_end;
            const auto [end, error] =
                std::from_chars(line.data() + cursor, line.data() + token_end, denominator);
            if (error != std::errc{} || end != line.data() + token_end || denominator <= 0)
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
            cursor = token_end;
        } else {
            if (slash_count > 62) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
            denominator = static_cast<std::int64_t>(std::uint64_t{1} << slash_count);
        }
    }

    return ParsedLength{sunny::core::Beat{numerator, denominator}, cursor};
}

sunny::core::Result<void> append_event(AbcParseResult& result,
                                       sunny::core::Beat& current_time,
                                       sunny::core::MidiNote pitch,
                                       sunny::core::Beat multiplier,
                                       std::uint8_t velocity,
                                       bool muted) {
    const auto duration = sunny::core::checked_mul(result.header.default_length, multiplier);
    if (!duration || *duration <= sunny::core::Beat::zero())
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    const auto next_time = sunny::core::checked_add(current_time, *duration);
    if (!next_time) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    result.notes.push_back(sunny::core::NoteEvent{pitch, current_time, *duration, velocity, muted});
    current_time = *next_time;
    return {};
}

} // anonymous namespace

sunny::core::Result<AbcParseResult> parse_abc(std::string_view text) {
    if (text.empty()) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

    AbcParseResult result;
    bool found_reference = false;
    bool found_title = false;
    bool found_key = false;
    bool in_body = false;
    sunny::core::Beat current_time = sunny::core::Beat::zero();
    std::array<int, 12> key_acc{};
    std::array<int, 12> bar_acc{};
    bool bar_acc_set[12] = {};

    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) eol = text.size();
        std::string_view line = text.substr(pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line = line.substr(0, line.size() - 1);

        if (!in_body) {
            const auto trimmed = trim_horizontal(line);
            if (trimmed.starts_with("%%"))
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
            if (trimmed.empty() || trimmed.starts_with('%')) {
                pos = eol + 1;
                continue;
            }
            if (line.size() < 2 || line[1] != ':')
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

            const char field = line[0];
            auto raw_value = line.substr(2);
            if (const auto comment = raw_value.find('%'); comment != std::string_view::npos)
                raw_value = raw_value.substr(0, comment);
            const std::string_view value = trim_horizontal(raw_value);
            if (!found_reference && field != 'X')
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

            switch (field) {
            case 'X': {
                const auto reference = parse_positive_integer(value);
                if (found_reference || !reference)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                result.header.reference_number = *reference;
                found_reference = true;
                break;
            }
            case 'T':
                if (found_title) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                result.header.title = std::string(value);
                found_title = true;
                break;
            case 'M': {
                if (result.header.metre_explicit)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                result.header.metre_explicit = true;
                if (value == "none") {
                    result.header.free_metre = true;
                } else if (value == "C") {
                    result.header.metre_num = 4;
                    result.header.metre_den = 4;
                    result.header.free_metre = false;
                } else if (value == "C|") {
                    result.header.metre_num = 2;
                    result.header.metre_den = 2;
                    result.header.free_metre = false;
                } else {
                    const auto metre = parse_positive_rational(value);
                    if (!metre || metre->numerator() > std::numeric_limits<int>::max() ||
                        metre->denominator() > std::numeric_limits<int>::max())
                        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                    result.header.metre_num = static_cast<int>(metre->numerator());
                    result.header.metre_den = static_cast<int>(metre->denominator());
                    result.header.free_metre = false;
                }
                derive_default_length(result.header);
                break;
            }
            case 'L': {
                if (result.header.default_length_explicit)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                const auto length = parse_positive_rational(value);
                if (!length) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                result.header.default_length = *length;
                result.header.default_length_explicit = true;
                break;
            }
            case 'Q': {
                if (result.header.tempo_bpm)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                const auto equals = value.find('=');
                std::optional<std::int64_t> bpm;
                if (equals == std::string_view::npos) {
                    bpm = parse_positive_integer(value);
                    result.header.tempo_unit = result.header.default_length;
                } else {
                    if (value.find('=', equals + 1) != std::string_view::npos)
                        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                    const auto unit = parse_positive_rational(value.substr(0, equals));
                    bpm = parse_positive_integer(value.substr(equals + 1));
                    if (!unit) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                    result.header.tempo_unit = *unit;
                }
                if (!bpm || *bpm > std::numeric_limits<int>::max())
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                result.header.tempo_bpm = static_cast<int>(*bpm);
                break;
            }
            case 'K': {
                result.header.key = std::string(value);
                const auto key = parse_key(value);
                if (!key) return std::unexpected(key.error());
                result.key_root = key->root;
                result.key_tonic = key->tonic;
                result.key_mode = key->mode;
                result.key_fifths = key->fifths;
                result.is_minor = key->mode == "minor" || key->mode == "aeolian";
                key_acc = key_accidentals(result.key_fifths);
                derive_default_length(result.header);
                found_key = true;
                in_body = true;
                break;
            }
            // Descriptive fields do not affect this NoteEvent projection.
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'F':
            case 'G':
            case 'H':
            case 'N':
            case 'O':
            case 'R':
            case 'S':
            case 'Z':
                result.header.descriptive_fields.push_back({field, std::string{value}});
                break;
            default:
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
            }
        } else {
            if (line.size() >= 2 && line[1] == ':')
                return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

            std::size_t i = 0;
            while (i < line.size()) {
                const char ch = line[i];
                if (ch == ' ' || ch == '\t' || ch == '`') {
                    ++i;
                    continue;
                }
                if (ch == '%') break;
                if (ch == '|') {
                    if (i + 1 < line.size() && line[i + 1] == ':')
                        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                    std::fill(std::begin(bar_acc_set), std::end(bar_acc_set), false);
                    ++i;
                    continue;
                }
                if (ch == 'z') {
                    const auto length = parse_note_length(line, i + 1);
                    if (!length) return std::unexpected(length.error());
                    const auto appended = append_event(
                        result, current_time, sunny::core::MidiNote{}, length->multiplier, 0, true);
                    if (!appended) return std::unexpected(appended.error());
                    i = length->next;
                    continue;
                }

                int explicit_acc = 0;
                bool has_explicit_acc = false;
                if (ch == '^' || ch == '_' || ch == '=') {
                    has_explicit_acc = true;
                    if (ch == '=') {
                        ++i;
                    } else {
                        explicit_acc = ch == '^' ? 1 : -1;
                        ++i;
                        if (i < line.size() && line[i] == ch) {
                            explicit_acc *= 2;
                            ++i;
                        }
                    }
                    if (i < line.size() && (line[i] == '^' || line[i] == '_' || line[i] == '='))
                        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                }

                if (i >= line.size() || !is_abc_note(line[i]))
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

                std::int64_t midi_base = abc_letter_to_midi(line[i]);
                ++i;
                std::int64_t octave_steps = 0;
                while (i < line.size() && (line[i] == '\'' || line[i] == ',')) {
                    if (line[i] == '\'') {
                        if (octave_steps == std::numeric_limits<std::int64_t>::max())
                            return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                        ++octave_steps;
                    } else {
                        if (octave_steps == std::numeric_limits<std::int64_t>::min())
                            return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                        --octave_steps;
                    }
                    ++i;
                }
                // More than this cannot return to MIDI range with the compact
                // profile's at-most-double accidental.
                if (octave_steps < -16 || octave_steps > 16)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                midi_base += octave_steps * 12;

                const int natural_pc = static_cast<int>((midi_base % 12 + 12) % 12);
                int accidental = 0;
                if (has_explicit_acc) {
                    accidental = explicit_acc;
                    bar_acc[natural_pc] = explicit_acc;
                    bar_acc_set[natural_pc] = true;
                } else if (bar_acc_set[natural_pc]) {
                    accidental = bar_acc[natural_pc];
                } else {
                    accidental = key_acc[natural_pc];
                }
                const auto final_midi = midi_base + accidental;
                if (final_midi < 0 || final_midi > 127)
                    return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
                const auto pitch = sunny::core::MidiNote::from_int(static_cast<int>(final_midi));
                if (!pitch) return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);

                const auto length = parse_note_length(line, i);
                if (!length) return std::unexpected(length.error());
                const auto appended =
                    append_event(result, current_time, *pitch, length->multiplier, 80, false);
                if (!appended) return std::unexpected(appended.error());
                i = length->next;
            }
        }

        pos = eol + 1;
    }

    if (!found_reference || !found_key) {
        return std::unexpected(sunny::core::ErrorCode::InvalidAbcFile);
    }

    return result;
}

} // namespace sunny::infrastructure::formats
