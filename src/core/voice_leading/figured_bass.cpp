/**
 * @file figured_bass.cpp
 * @brief Figured Bass Realisation implementation
 *
 */

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <functional>
#include <sunny/core/voice_leading/figured_bass.hpp>

namespace sunny::core {

// =============================================================================
// Figured bass interval lookup
// =============================================================================

std::vector<int> figured_bass_intervals(std::string_view shorthand) {
    if (shorthand.empty() || shorthand == "5/3" || shorthand == "53") {
        return {3, 5};
    }
    if (shorthand == "6" || shorthand == "6/3" || shorthand == "63") {
        return {3, 6};
    }
    if (shorthand == "6/4" || shorthand == "64") {
        return {4, 6};
    }
    if (shorthand == "7") {
        return {3, 5, 7};
    }
    if (shorthand == "6/5" || shorthand == "65") {
        return {3, 5, 6};
    }
    if (shorthand == "4/3" || shorthand == "43") {
        return {3, 4, 6};
    }
    if (shorthand == "4/2" || shorthand == "42" || shorthand == "2") {
        return {2, 4, 6};
    }
    return {};
}

// =============================================================================
// Parsing
// =============================================================================

Result<FiguredBassSymbol> parse_figured_bass(std::string_view text) {
    FiguredBassSymbol symbol;

    // First try as standard shorthand
    auto standard = figured_bass_intervals(text);
    if (!standard.empty()) {
        for (int iv : standard) {
            symbol.figures.push_back({iv, FigureAccidental::Natural});
        }
        return symbol;
    }

    // Explicit figures are single digits, separated by '/'. Compact strings
    // are accepted only for the conventional shorthands above.
    std::size_t pos = 0;
    while (pos < text.size()) {
        FigureAccidental acc = FigureAccidental::Natural;
        if (text[pos] == '#' || text[pos] == 'b') {
            acc = text[pos++] == '#' ? FigureAccidental::Sharp : FigureAccidental::Flat;
        }
        if (pos == text.size() || text[pos] < '1' || text[pos] > '9') {
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
        }
        symbol.figures.push_back({text[pos++] - '0', acc});
        if (symbol.figures.size() > MAX_FIGURED_BASS_VOICES) {
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
        }
        if (pos == text.size()) break;
        if (text[pos++] != '/' || pos == text.size()) {
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
        }
    }
    if (symbol.figures.empty()) return std::unexpected(ErrorCode::VoiceLeadingFailed);
    return symbol;
}

// =============================================================================
// Realisation
// =============================================================================

namespace {

struct UpperRequirement {
    PitchClass pitch_class;
    int minimum; // Actual MIDI pitch of the required generic interval.
};

struct BassRequirements {
    MidiNote bass;
    std::vector<UpperRequirement> upper;
};

Result<void> validate_figures(const FiguredBassSymbol& symbol, std::span<const Interval> scale) {
    if (scale.size() != 7 || scale.front() != 0 || scale.back() > 11 ||
        std::adjacent_find(scale.begin(), scale.end(), std::greater_equal<>{}) != scale.end()) {
        return std::unexpected(ErrorCode::InvalidScaleName);
    }
    if (symbol.figures.empty() || symbol.figures.size() > MAX_FIGURED_BASS_VOICES) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }
    for (const auto& fig : symbol.figures) {
        if (fig.interval < 1 || fig.interval > 9 ||
            (fig.accidental != FigureAccidental::Natural &&
             fig.accidental != FigureAccidental::Sharp &&
             fig.accidental != FigureAccidental::Flat)) {
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
        }
    }
    return {};
}

int alteration(FigureAccidental accidental) {
    if (accidental == FigureAccidental::Sharp) return 1;
    if (accidental == FigureAccidental::Flat) return -1;
    return 0;
}

Result<BassRequirements> requirements(MidiNote bass,
                                      const FiguredBassSymbol& symbol,
                                      PitchClass root,
                                      std::span<const Interval> scale) {
    auto valid = validate_figures(symbol, scale);
    if (!valid) return std::unexpected(valid.error());
    int degree = -1;
    const int offset = (static_cast<int>(pitch_class(bass)) - static_cast<int>(root) + 12) % 12;
    for (int d = 0; d < 7; ++d) {
        if (scale[d] == offset) degree = d;
    }
    if (degree < 0) return std::unexpected(ErrorCode::InvalidSpelledPitch);
    BassRequirements result{bass, {}};
    for (const auto& fig : symbol.figures) {
        const int target = degree + fig.interval - 1;
        const int minimum = static_cast<int>(bass) + scale[target % 7] + 12 * (target / 7) -
                            scale[degree] + alteration(fig.accidental);
        result.upper.push_back({PitchClass::wrapped(minimum), minimum});
    }
    return result;
}

Result<BassRequirements> requirements(SpelledPitch bass,
                                      const FiguredBassSymbol& symbol,
                                      SpelledPitch root,
                                      std::span<const Interval> scale) {
    auto valid = validate_figures(symbol, scale);
    if (!valid) return std::unexpected(valid.error());
    if (bass.letter >= 7 || root.letter >= 7) {
        return std::unexpected(ErrorCode::InvalidLetterName);
    }
    auto bass_midi = midi(bass);
    if (!bass_midi) return std::unexpected(bass_midi.error());
    BassRequirements result{*bass_midi, {}};
    for (const auto& fig : symbol.figures) {
        const int target_letter = bass.letter + fig.interval - 1;
        const int letter = target_letter % 7;
        const int degree = (letter - root.letter + 7) % 7;
        // Spell each scale degree on the next letter above the tonic.
        // This keeps Cb/B#, and chromatic Bb versus A#, distinct.
        const int key_accidental = static_cast<int>(nat(root.letter)) + root.accidental +
                                   scale[degree] - static_cast<int>(nat(letter)) -
                                   12 * ((root.letter + degree) / 7);
        const int minimum = 12 * (bass.octave + 1 + target_letter / 7) +
                            static_cast<int>(nat(letter)) + key_accidental +
                            alteration(fig.accidental);
        result.upper.push_back({PitchClass::wrapped(minimum), minimum});
    }
    return result;
}

FiguredBassRealisation assemble(MidiNote bass, std::vector<MidiNote> upper) {
    FiguredBassRealisation result{bass, std::move(upper), {bass}};
    std::sort(result.upper.begin(), result.upper.end());
    result.all_notes.insert(result.all_notes.end(), result.upper.begin(), result.upper.end());
    return result;
}

Result<FiguredBassRealisation> place_upper(const BassRequirements& req, int octave) {
    if (octave < -1 || octave > 9) return std::unexpected(ErrorCode::InvalidOctave);
    if (octave == -1) octave = std::clamp(static_cast<int>(req.bass) / 12, 0, 9);
    std::vector<MidiNote> upper;
    for (const auto& target : req.upper) {
        int note = 12 * (octave + 1) + static_cast<int>(target.pitch_class);
        const int lower_bound = std::max(static_cast<int>(req.bass) + 1, target.minimum);
        while (note < lower_bound)
            note += 12;
        auto checked = MidiNote::from_int(note);
        if (!checked) return std::unexpected(checked.error());
        upper.push_back(*checked);
    }
    return assemble(req.bass, std::move(upper));
}

// Exact finite search for sorted-voice L1 motion, with target figure identity
// retained. The state is (used figures, last MIDI pitch); at most 2^12*128
// states. Equal pitches are allowed for independent upper voices.
Result<std::vector<MidiNote>> lead_upper(std::span<const MidiNote> source,
                                         const BassRequirements& target) {
    const auto count = target.upper.size();
    const auto complete = (std::size_t{1} << count) - 1;
    constexpr int infinity = 4096; // Every 12-voice MIDI cost is <=1524.
    std::vector<int> costs((complete + 1) * 128, -1);
    std::vector<int> choices(costs.size(), -1);
    std::vector<std::vector<int>> candidates(count);
    for (std::size_t j = 0; j < count; ++j) {
        const auto& required = target.upper[j];
        const int minimum = std::max(static_cast<int>(target.bass) + 1, required.minimum);
        for (int pitch = static_cast<int>(required.pitch_class); pitch <= 127; pitch += 12) {
            if (pitch >= minimum) candidates[j].push_back(pitch);
        }
    }
    std::function<int(std::size_t, int)> solve = [&](std::size_t mask, int last) -> int {
        if (mask == complete) return 0;
        const auto state = mask * 128 + static_cast<std::size_t>(last);
        if (costs[state] >= 0) return costs[state];
        int best = infinity;
        const auto voice = std::popcount(mask);
        for (std::size_t j = 0; j < count; ++j) {
            if (mask & (std::size_t{1} << j)) continue;
            for (int pitch : candidates[j]) {
                if (pitch < last) continue;
                const int cost = std::abs(static_cast<int>(source[voice]) - pitch) +
                                 solve(mask | (std::size_t{1} << j), pitch);
                if (cost < best) {
                    best = cost;
                    choices[state] = static_cast<int>(j) * 128 + pitch;
                }
            }
        }
        return costs[state] = best;
    };
    int last = static_cast<int>(target.bass);
    if (solve(0, last) == infinity) return std::unexpected(ErrorCode::VoiceLeadingFailed);
    std::vector<MidiNote> upper;
    std::size_t mask = 0;
    while (mask != complete) {
        const int choice = choices[mask * 128 + static_cast<std::size_t>(last)];
        const auto j = static_cast<std::size_t>(choice / 128);
        last = choice % 128;
        auto note = MidiNote::from_int(last);
        if (!note) return std::unexpected(note.error());
        upper.push_back(*note);
        mask |= std::size_t{1} << j;
    }
    return upper;
}

template <class Event, class Root>
Result<FiguredBassSequenceResult>
realise_sequence(std::span<const Event> events, Root root, std::span<const Interval> scale) {
    if (events.empty()) return std::unexpected(ErrorCode::VoiceLeadingFailed);
    FiguredBassSequenceResult result;
    result.realisations.reserve(events.size());
    for (const auto& event : events) {
        auto req = requirements(event.bass_note, event.symbol, root, scale);
        if (!req) return std::unexpected(req.error());
        if (!result.realisations.empty() &&
            result.realisations.back().upper.size() == req->upper.size()) {
            auto upper = lead_upper(result.realisations.back().upper, *req);
            if (!upper) return std::unexpected(upper.error());
            result.realisations.push_back(assemble(req->bass, std::move(*upper)));
        } else {
            auto direct = place_upper(*req, -1);
            if (!direct) return std::unexpected(direct.error());
            result.realisations.push_back(std::move(*direct));
        }
    }
    return result;
}

} // namespace

Result<FiguredBassRealisation> realise_figured_bass(MidiNote bass,
                                                    const FiguredBassSymbol& symbol,
                                                    PitchClass root,
                                                    std::span<const Interval> scale,
                                                    int upper_octave) {
    auto req = requirements(bass, symbol, root, scale);
    if (!req) return std::unexpected(req.error());
    return place_upper(*req, upper_octave);
}

Result<FiguredBassRealisation> realise_figured_bass(SpelledPitch bass,
                                                    const FiguredBassSymbol& symbol,
                                                    SpelledPitch root,
                                                    std::span<const Interval> scale,
                                                    int upper_octave) {
    auto req = requirements(bass, symbol, root, scale);
    if (!req) return std::unexpected(req.error());
    return place_upper(*req, upper_octave);
}

Result<FiguredBassSequenceResult> realise_figured_bass_sequence(
    std::span<const FiguredBassEvent> events, PitchClass root, std::span<const Interval> scale) {
    return realise_sequence(events, root, scale);
}

Result<FiguredBassSequenceResult>
realise_figured_bass_sequence(std::span<const SpelledFiguredBassEvent> events,
                              SpelledPitch root,
                              std::span<const Interval> scale) {
    return realise_sequence(events, root, scale);
}

} // namespace sunny::core
