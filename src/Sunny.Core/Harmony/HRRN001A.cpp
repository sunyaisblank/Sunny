/**
 * @file HRRN001A.cpp
 * @brief Roman numeral parsing and chord generation
 *
 * Component: HRRN001A
 */

#include "HRRN001A.h"
#include "../Pitch/PTMN001A.h"
#include "../Scale/SCDF001A.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Sunny::Core {

namespace {

// Chord quality intervals (Formal Spec Appendix B)
// Values are semitone offsets above root for voicing generation.
// 13th chords omit the 11th to avoid minor 9th clash with the 3rd.
const std::unordered_map<std::string_view, std::vector<Interval>> QUALITY_INTERVALS = {
    // Triads
    {"major", {0, 4, 7}},
    {"minor", {0, 3, 7}},
    {"diminished", {0, 3, 6}},
    {"dim", {0, 3, 6}},
    {"augmented", {0, 4, 8}},
    {"aug", {0, 4, 8}},
    {"sus2", {0, 2, 7}},
    {"sus4", {0, 5, 7}},
    {"sus", {0, 5, 7}},
    {"5", {0, 7}},
    {"power", {0, 7}},
    // Seventh chords
    {"7", {0, 4, 7, 10}},
    {"dom7", {0, 4, 7, 10}},
    {"dominant7", {0, 4, 7, 10}},
    {"maj7", {0, 4, 7, 11}},
    {"major7", {0, 4, 7, 11}},
    {"m7", {0, 3, 7, 10}},
    {"min7", {0, 3, 7, 10}},
    {"minor7", {0, 3, 7, 10}},
    {"dim7", {0, 3, 6, 9}},
    {"diminished7", {0, 3, 6, 9}},
    {"m7b5", {0, 3, 6, 10}},
    {"half-diminished", {0, 3, 6, 10}},
    {"mM7", {0, 3, 7, 11}},
    {"minMaj7", {0, 3, 7, 11}},
    {"aug7", {0, 4, 8, 10}},
    {"+7", {0, 4, 8, 10}},
    {"augmaj7", {0, 4, 8, 11}},
    {"+maj7", {0, 4, 8, 11}},
    // Added-tone and sixth chords
    {"add9", {0, 4, 7, 14}},
    {"add11", {0, 4, 7, 17}},
    {"6", {0, 4, 7, 9}},
    {"m6", {0, 3, 7, 9}},
    {"69", {0, 4, 7, 9, 14}},
    {"6/9", {0, 4, 7, 9, 14}},
    // Ninth chords
    {"9", {0, 4, 7, 10, 14}},
    {"maj9", {0, 4, 7, 11, 14}},
    {"m9", {0, 3, 7, 10, 14}},
    // 11th chords
    {"11", {0, 4, 7, 10, 14, 17}},
    {"maj11", {0, 4, 7, 11, 14, 17}},
    {"m11", {0, 3, 7, 10, 14, 17}},
    // 13th chords (11th omitted per §5.3: avoids m9 between 3rd and 11th)
    {"13", {0, 4, 7, 10, 14, 21}},
    {"maj13", {0, 4, 7, 11, 14, 21}},
    {"m13", {0, 3, 7, 10, 14, 21}},
    // Altered extensions
    {"7b9", {0, 4, 7, 10, 13}},
    {"dom7b9", {0, 4, 7, 10, 13}},
    {"7#9", {0, 4, 7, 10, 15}},
    {"dom7s9", {0, 4, 7, 10, 15}},
    {"7#11", {0, 4, 7, 10, 18}},
    {"dom7s11", {0, 4, 7, 10, 18}},
    {"7b13", {0, 4, 7, 10, 20}},
    {"dom7b13", {0, 4, 7, 10, 20}},
    {"7alt", {0, 4, 10, 13, 15, 18, 20}},
    {"alt", {0, 4, 10, 13, 15, 18, 20}},
};

// Parse Roman numeral base — only consumes [IiVv] characters
bool parse_numeral_base(std::string_view s, int& degree, bool& is_upper) {
    if (s.empty()) return false;

    // Check case
    is_upper = std::isupper(static_cast<unsigned char>(s[0]));

    // Consume only Roman numeral characters (I, i, V, v)
    std::string lower;
    for (char c : s) {
        char lc = std::tolower(static_cast<unsigned char>(c));
        if (lc == 'i' || lc == 'v') {
            lower += lc;
        } else {
            break;
        }
    }

    if (lower == "i") degree = 0;
    else if (lower == "ii") degree = 1;
    else if (lower == "iii") degree = 2;
    else if (lower == "iv") degree = 3;
    else if (lower == "v") degree = 4;
    else if (lower == "vi") degree = 5;
    else if (lower == "vii") degree = 6;
    else return false;

    return true;
}

// Count consumed Roman numeral characters and return the length
std::size_t numeral_length(std::string_view s) {
    std::size_t len = 0;
    for (char c : s) {
        char lc = std::tolower(static_cast<unsigned char>(c));
        if (lc == 'i' || lc == 'v') {
            ++len;
        } else {
            break;
        }
    }
    return len;
}

// Parse figured bass inversion suffix from end of string.
// Returns (suffix_length, FiguredBass).
std::pair<std::size_t, FiguredBass> parse_inversion_suffix(std::string_view s) {
    // Check longest suffixes first
    if (s.size() >= 2) {
        auto tail2 = s.substr(s.size() - 2);
        if (tail2 == "65") return {2, FiguredBass::First};
        if (tail2 == "64") return {2, FiguredBass::Second};
        if (tail2 == "43") return {2, FiguredBass::Second};
        if (tail2 == "42") return {2, FiguredBass::Third};
    }
    if (!s.empty()) {
        // "6" as the final character, but not if preceded by digits that
        // form part of an extension (e.g. "13" should not be split as "1" + "3→6")
        char last = s.back();
        if (last == '6') {
            // Avoid false match on "m6", "add6", extension "6" in chord quality.
            // Only treat trailing "6" as inversion if it follows a numeral or
            // a quality modifier — heuristic: the character before "6" is not a digit.
            if (s.size() >= 2) {
                char before = s[s.size() - 2];
                if (std::isdigit(static_cast<unsigned char>(before))) {
                    return {0, FiguredBass::Root};
                }
            }
            return {1, FiguredBass::First};
        }
    }
    return {0, FiguredBass::Root};
}

}  // namespace

Result<std::pair<int, bool>> parse_roman_numeral(std::string_view numeral) {
    if (numeral.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    int degree;
    bool is_upper;

    if (!parse_numeral_base(numeral, degree, is_upper)) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    return std::make_pair(degree, is_upper);
}

Result<ParsedNumeral> parse_roman_numeral_full(std::string_view numeral) {
    if (numeral.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    ParsedNumeral result{};
    result.accidental = 0;
    result.inversion = FiguredBass::Root;
    result.is_neapolitan = false;

    std::string_view remaining = numeral;

    // 1. Check for Neapolitan shorthand "N"
    if (remaining[0] == 'N') {
        result.is_neapolitan = true;
        result.degree = 1;          // II
        result.is_upper = true;     // Major
        result.accidental = -1;     // Flat
        remaining = remaining.substr(1);

        // Parse optional inversion suffix (N6 = Neapolitan sixth)
        if (!remaining.empty()) {
            auto [slen, inv] = parse_inversion_suffix(remaining); (void)slen;
            result.inversion = inv;
        }
        return result;
    }

    // 2. Parse accidental prefix (♭, b, ♯, #)
    if (!remaining.empty()) {
        // Multi-byte UTF-8: ♭ = 0xE2 0x99 0xAD, ♯ = 0xE2 0x99 0xAF
        if (remaining.size() >= 3 &&
            static_cast<unsigned char>(remaining[0]) == 0xE2 &&
            static_cast<unsigned char>(remaining[1]) == 0x99) {
            if (static_cast<unsigned char>(remaining[2]) == 0xAD) {
                result.accidental = -1;
                remaining = remaining.substr(3);
            } else if (static_cast<unsigned char>(remaining[2]) == 0xAF) {
                result.accidental = 1;
                remaining = remaining.substr(3);
            }
        } else if (remaining[0] == 'b' && remaining.size() > 1 &&
                   std::isupper(static_cast<unsigned char>(remaining[1]))) {
            // "b" followed by uppercase = flat accidental (e.g. bVII)
            result.accidental = -1;
            remaining = remaining.substr(1);
        } else if (remaining[0] == '#') {
            result.accidental = 1;
            remaining = remaining.substr(1);
        }
    }

    // 3. Parse Roman numeral base
    if (remaining.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    int degree;
    bool is_upper;
    if (!parse_numeral_base(remaining, degree, is_upper)) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    result.degree = degree;
    result.is_upper = is_upper;

    // Advance past the numeral characters
    std::size_t nlen = numeral_length(remaining);
    remaining = remaining.substr(nlen);

    // 4. Parse quality modifier (°, +, ø, dim, aug)
    if (!remaining.empty()) {
        // Multi-byte: ° = 0xC2 0xB0, ø = 0xC3 0xB8
        if (remaining.size() >= 2 &&
            static_cast<unsigned char>(remaining[0]) == 0xC2 &&
            static_cast<unsigned char>(remaining[1]) == 0xB0) {
            result.quality_mod = "°";
            remaining = remaining.substr(2);
        } else if (remaining.size() >= 2 &&
                   static_cast<unsigned char>(remaining[0]) == 0xC3 &&
                   static_cast<unsigned char>(remaining[1]) == 0xB8) {
            result.quality_mod = "ø";
            remaining = remaining.substr(2);
        } else if (remaining[0] == '+') {
            result.quality_mod = "+";
            remaining = remaining.substr(1);
        } else if (remaining[0] == 'o') {
            // ASCII 'o' as diminished (alternative to °)
            result.quality_mod = "°";
            remaining = remaining.substr(1);
        }
    }

    // 5. Parse inversion suffix from the end of remaining
    if (!remaining.empty()) {
        auto [slen, inv] = parse_inversion_suffix(remaining);
        if (slen > 0) {
            result.inversion = inv;
            remaining = remaining.substr(0, remaining.size() - slen);
        }
    }

    // 6. Remaining is the extension (7, 9, 11, 13, maj7, maj9, alt, b9, #9, etc.)
    result.extension = std::string(remaining);

    // Figured bass 65, 43, 42 imply a seventh chord
    if (result.extension.empty() &&
        (result.inversion == FiguredBass::First ||
         result.inversion == FiguredBass::Second ||
         result.inversion == FiguredBass::Third)) {
        // 6 alone is valid for triads (first inversion), but 65/43/42 imply 7th.
        // We only reach here if the suffix was 65/43/42 (2-char), since trailing "6"
        // for triads doesn't set extension. Mark as seventh chord.
        if (result.inversion == FiguredBass::Third ||
            (result.inversion == FiguredBass::First && numeral.find("65") != std::string_view::npos) ||
            (result.inversion == FiguredBass::Second && numeral.find("43") != std::string_view::npos)) {
            result.extension = "7";
        }
    }

    return result;
}

Result<ChordVoicing> generate_chord_from_numeral(
    std::string_view numeral,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    int octave
) {
    if (numeral.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    // Use the full parser
    auto parsed = parse_roman_numeral_full(numeral);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    const auto& pn = *parsed;

    // Get root from scale degree
    if (pn.degree < 0 || static_cast<std::size_t>(pn.degree) >= scale_intervals.size()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    PitchClass chord_root = transpose(key_root, scale_intervals[pn.degree]);

    // Apply chromatic alteration
    if (pn.accidental != 0) {
        chord_root = transpose(chord_root, pn.accidental);
    }

    // Determine chord quality from parsed components
    std::string quality;
    bool is_upper = pn.is_upper;
    const auto& ext = pn.extension;
    const auto& qm = pn.quality_mod;

    bool has_7 = ext.find('7') != std::string::npos;
    bool has_alt = ext.find("alt") != std::string::npos;
    bool has_b9 = ext.find("b9") != std::string::npos;
    bool has_s9 = ext.find("#9") != std::string::npos;
    bool has_s11 = ext.find("#11") != std::string::npos;
    bool has_b13 = ext.find("b13") != std::string::npos;
    bool has_13 = !has_b13 && ext.find("13") != std::string::npos;
    bool has_11 = !has_s11 && !has_13 && ext.find("11") != std::string::npos;
    bool has_9 = !has_b9 && !has_s9 && ext.find('9') != std::string::npos;
    bool has_maj = ext.find("maj") != std::string::npos;
    bool is_dim = (qm == "°");
    bool is_aug = (qm == "+");
    bool is_half_dim = (qm == "ø");

    if (has_alt) {
        quality = "alt";
    } else if (has_b9) {
        quality = "dom7b9";
    } else if (has_s9) {
        quality = "dom7s9";
    } else if (has_s11) {
        quality = "dom7s11";
    } else if (has_b13) {
        quality = "dom7b13";
    } else if (has_13) {
        quality = has_maj ? "maj13" : (is_upper ? "13" : "m13");
    } else if (has_11) {
        quality = has_maj ? "maj11" : (is_upper ? "11" : "m11");
    } else if (has_9) {
        quality = has_maj ? "maj9" : (is_upper ? "9" : "m9");
    } else if (is_half_dim) {
        quality = "m7b5";
    } else if (is_dim) {
        quality = has_7 ? "dim7" : "diminished";
    } else if (is_aug) {
        quality = has_7 ? "aug7" : "augmented";
    } else if (is_upper) {
        if (has_maj) quality = "maj7";
        else quality = has_7 ? "7" : "major";
    } else {
        if (has_maj) quality = "mM7";
        else quality = has_7 ? "m7" : "minor";
    }

    auto chord_result = generate_chord(chord_root, quality, octave);
    if (!chord_result.has_value()) {
        return chord_result;
    }

    // Apply inversion
    auto& chord = *chord_result;
    if (pn.inversion != FiguredBass::Root && chord.notes.size() >= 2) {
        int inv = static_cast<int>(pn.inversion);
        // Rotate: move 'inv' lowest notes up an octave
        int to_move = std::min(inv, static_cast<int>(chord.notes.size()) - 1);
        for (int i = 0; i < to_move; ++i) {
            if (chord.notes[i] + 12 <= 127) {
                chord.notes[i] += 12;
            }
        }
        std::sort(chord.notes.begin(), chord.notes.end());
        chord.inversion = inv;
    }

    return chord;
}

Result<ChordVoicing> generate_chord(
    PitchClass root,
    std::string_view quality,
    int octave
) {
    auto intervals_opt = chord_quality_intervals(quality);
    if (!intervals_opt) {
        return std::unexpected(ErrorCode::InvalidChordQuality);
    }

    const auto& intervals = *intervals_opt;

    auto base_midi = pitch_octave_to_midi(root, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    ChordVoicing voicing;
    voicing.root = root;
    voicing.quality = std::string(quality);
    voicing.inversion = 0;

    for (Interval interval : intervals) {
        int midi = *base_midi + interval;
        if (midi >= 0 && midi <= 127) {
            voicing.notes.push_back(static_cast<MidiNote>(midi));
        }
    }

    return voicing;
}

Result<std::vector<Interval>> chord_quality_intervals(std::string_view quality) {
    auto it = QUALITY_INTERVALS.find(quality);
    if (it != QUALITY_INTERVALS.end()) {
        return it->second;
    }

    // Try lowercase
    std::string lower;
    for (char c : quality) {
        lower += std::tolower(static_cast<unsigned char>(c));
    }

    it = QUALITY_INTERVALS.find(lower);
    if (it != QUALITY_INTERVALS.end()) {
        return it->second;
    }

    return std::unexpected(ErrorCode::UnknownChordQuality);
}

// =============================================================================
// §16.1.5 Chord Recognition
// =============================================================================

namespace {

// Quality entries for matching: name → sorted interval set (mod 12)
struct QualityEntry {
    std::string_view name;
    std::vector<Interval> intervals;  // sorted, mod 12
};

// Build the match table from QUALITY_INTERVALS (deduplicated by interval set)
std::vector<QualityEntry> build_match_table() {
    // Use canonical names only (avoid aliases)
    static const std::pair<std::string_view, std::vector<Interval>> CANONICAL[] = {
        {"major", {0, 4, 7}},
        {"minor", {0, 3, 7}},
        {"diminished", {0, 3, 6}},
        {"augmented", {0, 4, 8}},
        {"sus2", {0, 2, 7}},
        {"sus4", {0, 5, 7}},
        {"5", {0, 7}},
        {"7", {0, 4, 7, 10}},
        {"maj7", {0, 4, 7, 11}},
        {"m7", {0, 3, 7, 10}},
        {"dim7", {0, 3, 6, 9}},
        {"m7b5", {0, 3, 6, 10}},
        {"mM7", {0, 3, 7, 11}},
        {"aug7", {0, 4, 8, 10}},
        {"augmaj7", {0, 4, 8, 11}},
        // "6" and "m6" excluded: same PCS as m7/mM7 of different root
        {"9", {0, 2, 4, 7, 10}},
        {"maj9", {0, 2, 4, 7, 11}},
        {"m9", {0, 2, 3, 7, 10}},
    };

    std::vector<QualityEntry> table;
    for (const auto& [name, ivs] : CANONICAL) {
        QualityEntry entry;
        entry.name = name;
        entry.intervals = ivs;
        std::sort(entry.intervals.begin(), entry.intervals.end());
        table.push_back(entry);
    }

    // Sort by descending interval count (prefer more specific matches)
    std::sort(table.begin(), table.end(), [](const QualityEntry& a, const QualityEntry& b) {
        return a.intervals.size() > b.intervals.size();
    });

    return table;
}

}  // namespace

Result<std::pair<PitchClass, std::string>> recognize_chord(
    const PitchClassSet& pcs
) {
    if (pcs.size() < 2) return std::unexpected(ErrorCode::ChordNotRecognised);

    static const auto MATCH_TABLE = build_match_table();

    PitchClass best_root = 0;
    std::string best_quality;
    std::size_t best_match_size = 0;

    for (PitchClass candidate = 0; candidate < 12; ++candidate) {
        // Compute intervals above candidate root (mod 12), sorted
        std::vector<Interval> intervals;
        for (auto pc : pcs) {
            intervals.push_back(static_cast<Interval>((pc - candidate + 12) % 12));
        }
        std::sort(intervals.begin(), intervals.end());

        // Check if root (0) is present
        if (intervals.empty() || intervals[0] != 0) continue;

        // Match against known qualities
        for (const auto& entry : MATCH_TABLE) {
            if (entry.intervals == intervals && intervals.size() > best_match_size) {
                best_root = candidate;
                best_quality = std::string(entry.name);
                best_match_size = intervals.size();
            }
        }
    }

    if (best_match_size == 0) return std::unexpected(ErrorCode::ChordNotRecognised);
    return std::make_pair(best_root, best_quality);
}

// =============================================================================
// §16.1.6 Chord to Roman Numeral
// =============================================================================

Result<std::string> chord_to_numeral(
    PitchClass chord_root,
    std::string_view quality,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    bool is_minor
) {
    // Find scale degree
    int interval = (chord_root - key_root + 12) % 12;
    int degree = -1;
    int accidental = 0;

    for (std::size_t i = 0; i < scale_intervals.size(); ++i) {
        if (scale_intervals[i] == interval) {
            degree = static_cast<int>(i);
            break;
        }
    }

    // If not a diatonic degree, find the closest with an accidental.
    // Check both sharp (raised lower neighbour) and flat (lowered upper
    // neighbour) and pick the one with smaller accidental distance.
    // When tied, prefer sharp for degrees 1,2,4,5 and flat for 3,6,7
    // (conventional analytical spelling).
    if (degree < 0) {
        int flat_deg = -1;
        int sharp_deg = -1;

        for (std::size_t i = 0; i < scale_intervals.size(); ++i) {
            if (scale_intervals[i] == interval + 1)
                flat_deg = static_cast<int>(i);
            if (scale_intervals[i] == interval - 1)
                sharp_deg = static_cast<int>(i);
        }

        if (flat_deg >= 0 && sharp_deg >= 0) {
            // Both candidates exist; prefer sharp for degrees 0,1,3,4
            // (I, II, IV, V → #I, #II, #IV, #V preferred over bII, bIII, bV, bVI)
            // and flat for degrees 2,5,6 (III, VI, VII → bIII, bVI, bVII preferred)
            static constexpr bool PREFER_SHARP[7] = {
                true, true, false, true, true, false, false
            };
            if (PREFER_SHARP[sharp_deg]) {
                degree = sharp_deg;
                accidental = 1;
            } else {
                degree = flat_deg;
                accidental = -1;
            }
        } else if (flat_deg >= 0) {
            degree = flat_deg;
            accidental = -1;
        } else if (sharp_deg >= 0) {
            degree = sharp_deg;
            accidental = 1;
        }
    }

    if (degree < 0 || degree >= 7) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    // Determine case from quality: uppercase for major-type, lowercase for
    // minor-type. In a minor key the same rule applies — the chord's own
    // quality governs casing (e.g. V is uppercase because the chord is
    // major, i is lowercase because the chord is minor).
    bool is_major_quality = (quality == "major" || quality == "maj7" ||
                             quality == "7" || quality == "augmented" ||
                             quality == "aug7" || quality == "augmaj7" ||
                             quality == "9" || quality == "maj9" ||
                             quality == "6" || quality == "5");

    // In a minor key, the natural diatonic quality at each degree differs
    // from major. The casing still follows the chord quality — a major
    // chord on degree 4 (V) stays uppercase, a minor chord on degree 0
    // (i) stays lowercase. The is_minor flag is used here to inform
    // quality suffix conventions when needed (currently casing derives
    // entirely from quality, which already encodes major/minor).
    (void)is_minor;

    // Build numeral string
    std::string result;

    // Accidental prefix
    if (accidental == -1) result += "b";
    else if (accidental == 1) result += "#";

    // Roman numeral
    static constexpr std::array<std::string_view, 7> UPPER = {
        "I", "II", "III", "IV", "V", "VI", "VII"
    };
    static constexpr std::array<std::string_view, 7> LOWER = {
        "i", "ii", "iii", "iv", "v", "vi", "vii"
    };
    result += is_major_quality ? UPPER[degree] : LOWER[degree];

    // Quality suffix
    if (quality == "diminished") result += "\xC2\xB0";       // °
    else if (quality == "augmented") result += "+";
    else if (quality == "dim7") result += "\xC2\xB0" "7";    // °7
    else if (quality == "m7b5") result += "\xC3\xB8" "7";    // ø7
    else if (quality == "7" || quality == "m7") result += "7";
    else if (quality == "maj7" || quality == "mM7") result += "maj7";
    else if (quality == "aug7") { result += "+7"; }
    else if (quality == "augmaj7") { result += "+maj7"; }

    return result;
}

}  // namespace Sunny::Core
