/**
 * @file FMAB001A.cpp
 * @brief ABC notation reader implementation
 *
 * Component: FMAB001A
 */

#include "FMAB001A.h"

#include "../../Sunny.Core/Pitch/PTPC001A.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace Sunny::Infrastructure::Format {

namespace {

/// Key signature accidentals for major keys (sharps/flats applied to scale degrees).
/// Maps pitch class of key root to the set of pitch classes that are sharped/flatted.
/// Returns an array where index = pitch class, value = accidental offset (-1, 0, or +1).
std::array<int, 12> key_accidentals(Sunny::Core::PitchClass root, bool is_minor) {
    // Convert minor root to relative major
    int major_root = is_minor ? (root + 3) % 12 : root;

    // Number of sharps/flats based on major key root
    // C=0, G=1#, D=2#, A=3#, E=4#, B=5#, F#=6#, Gb=6b, Db=5b, Ab=4b, Eb=3b, Bb=2b, F=1b
    static constexpr std::array<int, 12> SHARPS_COUNT = {
        0, 7, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5
    };
    // Positive = sharps, negative = flats
    int sf = SHARPS_COUNT[major_root];

    std::array<int, 12> acc{};  // all zeros (naturals)

    if (sf > 0) {
        // Sharp order: F C G D A E B
        static constexpr std::array<int, 7> SHARP_ORDER = {5, 0, 7, 2, 9, 4, 11};
        for (int i = 0; i < sf && i < 7; ++i) {
            acc[SHARP_ORDER[i]] = 1;
        }
    } else if (sf < 0) {
        // Flat order: B E A D G C F
        static constexpr std::array<int, 7> FLAT_ORDER = {11, 4, 9, 2, 7, 0, 5};
        for (int i = 0; i < -sf && i < 7; ++i) {
            acc[FLAT_ORDER[i]] = -1;
        }
    }

    return acc;
}

/// Parse ABC key field: "C", "Am", "D", "Bb", "F#m"
/// Returns (pitch_class, is_minor)
Sunny::Core::Result<std::pair<Sunny::Core::PitchClass, bool>> parse_key(std::string_view k) {
    if (k.empty()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidAbcFile);
    }

    // Trim whitespace
    while (!k.empty() && (k.front() == ' ' || k.front() == '\t')) k.remove_prefix(1);
    while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.remove_suffix(1);

    if (k.empty()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidAbcFile);
    }

    // Parse root note letter
    int base_pc;
    switch (std::toupper(static_cast<unsigned char>(k[0]))) {
        case 'C': base_pc = 0; break;
        case 'D': base_pc = 2; break;
        case 'E': base_pc = 4; break;
        case 'F': base_pc = 5; break;
        case 'G': base_pc = 7; break;
        case 'A': base_pc = 9; break;
        case 'B': base_pc = 11; break;
        default:
            return std::unexpected(Sunny::Core::ErrorCode::InvalidAbcFile);
    }

    std::size_t pos = 1;
    // Parse accidentals
    while (pos < k.size()) {
        if (k[pos] == '#') { base_pc = (base_pc + 1) % 12; ++pos; }
        else if (k[pos] == 'b' && (pos + 1 >= k.size() || k[pos + 1] != 'a')) {
            // 'b' is flat, but not if followed by 'a' (part of word)
            // Handle "Bb" vs "Bm": if at pos 1 and letter is B, then 'b' is flat
            base_pc = (base_pc + 11) % 12; ++pos;
        }
        else break;
    }

    // Check for minor
    bool is_minor = false;
    if (pos < k.size()) {
        auto rest = k.substr(pos);
        if (rest == "m" || rest == "min" || rest == "minor") {
            is_minor = true;
        }
    }

    return std::pair{static_cast<Sunny::Core::PitchClass>(base_pc), is_minor};
}

/// ABC note letter to base MIDI note
/// Uppercase: C=60, D=62, E=64, F=65, G=67, A=69, B=71
/// Lowercase: c=72, d=74, e=76, f=77, g=79, a=81, b=83
int abc_letter_to_midi(char ch) {
    switch (ch) {
        case 'C': return 60; case 'D': return 62; case 'E': return 64;
        case 'F': return 65; case 'G': return 67; case 'A': return 69;
        case 'B': return 71;
        case 'c': return 72; case 'd': return 74; case 'e': return 76;
        case 'f': return 77; case 'g': return 79; case 'a': return 81;
        case 'b': return 83;
        default: return -1;
    }
}

bool is_abc_note(char ch) {
    return abc_letter_to_midi(ch) >= 0;
}

}  // anonymous namespace

Sunny::Core::Result<AbcParseResult> parse_abc(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidAbcFile);
    }

    AbcParseResult result;
    result.header.default_length = Sunny::Core::Beat{1, 8};  // ABC default

    bool found_key = false;
    bool in_body = false;

    // Current position tracking for note events
    Sunny::Core::Beat current_time = Sunny::Core::Beat::zero();

    // Key signature accidentals
    std::array<int, 12> key_acc{};

    // Per-bar accidentals (reset at bar lines)
    std::array<int, 12> bar_acc{};
    bool bar_acc_set[12] = {};

    std::size_t pos = 0;
    while (pos < text.size()) {
        // Find end of line
        std::size_t eol = text.find('\n', pos);
        if (eol == std::string_view::npos) eol = text.size();
        std::string_view line = text.substr(pos, eol - pos);
        // Trim \r
        if (!line.empty() && line.back() == '\r') line = line.substr(0, line.size() - 1);

        if (!in_body) {
            // Header parsing
            if (line.size() >= 2 && line[1] == ':') {
                char field = line[0];
                std::string_view value = line.substr(2);
                // Trim leading space
                while (!value.empty() && value.front() == ' ') value.remove_prefix(1);

                switch (field) {
                    case 'T':
                        result.header.title = std::string(value);
                        break;
                    case 'M': {
                        // Parse metre: "4/4", "3/4", "6/8", "C"
                        if (value == "C") {
                            result.header.metre_num = 4;
                            result.header.metre_den = 4;
                        } else if (value == "C|") {
                            result.header.metre_num = 2;
                            result.header.metre_den = 2;
                        } else {
                            auto slash = value.find('/');
                            if (slash != std::string_view::npos) {
                                int n = 0, d = 0;
                                std::from_chars(value.data(), value.data() + slash, n);
                                std::from_chars(value.data() + slash + 1, value.data() + value.size(), d);
                                if (n > 0 && d > 0) {
                                    result.header.metre_num = n;
                                    result.header.metre_den = d;
                                }
                            }
                        }
                        break;
                    }
                    case 'L': {
                        // Parse default note length: "1/8", "1/4"
                        auto slash = value.find('/');
                        if (slash != std::string_view::npos) {
                            int n = 0, d = 0;
                            std::from_chars(value.data(), value.data() + slash, n);
                            std::from_chars(value.data() + slash + 1, value.data() + value.size(), d);
                            if (n > 0 && d > 0) {
                                result.header.default_length = Sunny::Core::Beat{n, d};
                            }
                        }
                        break;
                    }
                    case 'Q': {
                        // Parse tempo: "120" or "1/4=120"
                        auto eq = value.find('=');
                        if (eq != std::string_view::npos) {
                            auto bpm_sv = value.substr(eq + 1);
                            int bpm = 0;
                            std::from_chars(bpm_sv.data(), bpm_sv.data() + bpm_sv.size(), bpm);
                            if (bpm > 0) result.header.tempo_bpm = bpm;
                        } else {
                            int bpm = 0;
                            std::from_chars(value.data(), value.data() + value.size(), bpm);
                            if (bpm > 0) result.header.tempo_bpm = bpm;
                        }
                        break;
                    }
                    case 'K': {
                        result.header.key = std::string(value);
                        auto key = parse_key(value);
                        if (!key) {
                            return std::unexpected(key.error());
                        }
                        result.key_root = key->first;
                        result.is_minor = key->second;
                        key_acc = key_accidentals(result.key_root, result.is_minor);
                        found_key = true;
                        in_body = true;
                        break;
                    }
                    default:
                        break;  // Ignore unknown fields (X:, etc.)
                }
            }
        } else {
            // Body parsing: parse notes from the line
            std::size_t i = 0;
            while (i < line.size()) {
                char ch = line[i];

                // Skip whitespace, bar lines, decorations
                if (ch == ' ' || ch == '\t') { ++i; continue; }
                if (ch == '|' || ch == ':' || ch == '[' || ch == ']') {
                    if (ch == '|') {
                        // Reset bar accidentals
                        std::fill(std::begin(bar_acc_set), std::end(bar_acc_set), false);
                    }
                    ++i;
                    continue;
                }

                // Skip quoted chords "Am" etc
                if (ch == '"') {
                    auto close = line.find('"', i + 1);
                    i = (close != std::string_view::npos) ? close + 1 : line.size();
                    continue;
                }

                // Rest
                if (ch == 'z' || ch == 'Z') {
                    ++i;
                    // Parse duration multiplier
                    int64_t num = 1, den = 1;
                    // Number after rest
                    if (i < line.size() && line[i] >= '0' && line[i] <= '9') {
                        int v = 0;
                        auto [p, ec] = std::from_chars(line.data() + i, line.data() + line.size(), v);
                        if (ec == std::errc{}) {
                            num = v;
                            i = static_cast<std::size_t>(p - line.data());
                        }
                    }
                    if (i < line.size() && line[i] == '/') {
                        ++i;
                        if (i < line.size() && line[i] >= '0' && line[i] <= '9') {
                            int v = 0;
                            auto [p, ec] = std::from_chars(line.data() + i, line.data() + line.size(), v);
                            if (ec == std::errc{}) {
                                den = v;
                                i = static_cast<std::size_t>(p - line.data());
                            }
                        } else {
                            den = 2;  // bare '/' means /2
                        }
                    }

                    auto dur = Sunny::Core::Beat{
                        result.header.default_length.numerator * num,
                        result.header.default_length.denominator * den
                    };
                    // Add rest as a muted event
                    result.notes.push_back(Sunny::Core::NoteEvent{
                        0, current_time, dur, 0, true
                    });
                    current_time = current_time + dur;
                    continue;
                }

                // Accidentals before note
                int explicit_acc = 0;
                bool has_explicit_acc = false;
                while (i < line.size() && (line[i] == '^' || line[i] == '_' || line[i] == '=')) {
                    has_explicit_acc = true;
                    if (line[i] == '^') { explicit_acc++; ++i; }
                    else if (line[i] == '_') { explicit_acc--; ++i; }
                    else if (line[i] == '=') { explicit_acc = 0; ++i; }
                }

                // Note letter
                if (i < line.size() && is_abc_note(line[i])) {
                    int midi_base = abc_letter_to_midi(line[i]);
                    ++i;

                    // Octave modifiers
                    while (i < line.size() && line[i] == '\'') { midi_base += 12; ++i; }
                    while (i < line.size() && line[i] == ',') { midi_base -= 12; ++i; }

                    // Apply accidental
                    int natural_pc = midi_base % 12;
                    int acc_offset;
                    if (has_explicit_acc) {
                        acc_offset = explicit_acc;
                        bar_acc[natural_pc] = explicit_acc;
                        bar_acc_set[natural_pc] = true;
                    } else if (bar_acc_set[natural_pc]) {
                        acc_offset = bar_acc[natural_pc];
                    } else {
                        acc_offset = key_acc[natural_pc];
                    }
                    int final_midi = midi_base + acc_offset;

                    // Clamp to MIDI range
                    if (final_midi < 0) final_midi = 0;
                    if (final_midi > 127) final_midi = 127;

                    // Parse duration multiplier
                    int64_t num = 1, den = 1;
                    if (i < line.size() && line[i] >= '0' && line[i] <= '9') {
                        int v = 0;
                        auto [p, ec] = std::from_chars(line.data() + i, line.data() + line.size(), v);
                        if (ec == std::errc{}) {
                            num = v;
                            i = static_cast<std::size_t>(p - line.data());
                        }
                    }
                    if (i < line.size() && line[i] == '/') {
                        ++i;
                        if (i < line.size() && line[i] >= '0' && line[i] <= '9') {
                            int v = 0;
                            auto [p, ec] = std::from_chars(line.data() + i, line.data() + line.size(), v);
                            if (ec == std::errc{}) {
                                den = v;
                                i = static_cast<std::size_t>(p - line.data());
                            }
                        } else {
                            den = 2;
                        }
                    }

                    auto dur = Sunny::Core::Beat{
                        result.header.default_length.numerator * num,
                        result.header.default_length.denominator * den
                    };

                    result.notes.push_back(Sunny::Core::NoteEvent{
                        static_cast<Sunny::Core::MidiNote>(final_midi),
                        current_time,
                        dur,
                        80,
                        false
                    });
                    current_time = current_time + dur;
                    continue;
                }

                // Skip unknown characters
                ++i;
            }
        }

        pos = eol + 1;
    }

    if (!found_key) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidAbcFile);
    }

    return result;
}

}  // namespace Sunny::Infrastructure::Format
