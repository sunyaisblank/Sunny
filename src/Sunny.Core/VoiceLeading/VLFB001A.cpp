/**
 * @file VLFB001A.cpp
 * @brief Figured Bass Realisation implementation
 *
 * Component: VLFB001A
 */

#include "VLFB001A.h"
#include "VLNT001A.h"

#include <algorithm>
#include <cstdlib>

namespace Sunny::Core {

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

    // Parse individual figures separated by '/'
    // Each figure: optional accidental (#, b) + digit
    std::string_view remaining = text;
    while (!remaining.empty()) {
        FigureAccidental acc = FigureAccidental::Natural;

        // Skip separator
        if (remaining.front() == '/') {
            remaining.remove_prefix(1);
            if (remaining.empty()) break;
        }

        // Check for accidental prefix
        if (remaining.front() == '#') {
            acc = FigureAccidental::Sharp;
            remaining.remove_prefix(1);
        } else if (remaining.front() == 'b') {
            acc = FigureAccidental::Flat;
            remaining.remove_prefix(1);
        }

        // Parse digit
        if (remaining.empty() || remaining.front() < '1' || remaining.front() > '9') {
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
        }
        int interval = remaining.front() - '0';
        remaining.remove_prefix(1);

        symbol.figures.push_back({interval, acc});
    }

    if (symbol.figures.empty()) {
        // Default to root position triad
        symbol.figures.push_back({3, FigureAccidental::Natural});
        symbol.figures.push_back({5, FigureAccidental::Natural});
    }

    return symbol;
}

// =============================================================================
// Realisation
// =============================================================================

namespace {

/**
 * @brief Compute the pitch class that is 'generic_interval' diatonic steps
 * above bass_pc in the given scale.
 *
 * Generic interval 1 = unison, 2 = 2nd, 3 = 3rd, etc.
 *
 * @pre generic_interval >= 1
 * @pre scale is non-empty
 */
PitchClass diatonic_above(
    PitchClass bass_pc,
    int generic_interval,
    PitchClass key_root,
    std::span<const Interval> scale
) {
    // generic_interval < 1 is a precondition violation: 1 = unison, 2 = 2nd, etc.
    // Callers must validate; return key_root as a safe fallback.
    if (generic_interval < 1 || scale.empty()) {
        return key_root;
    }

    // Find the scale degree of the bass note
    int bass_offset = ((static_cast<int>(bass_pc) - static_cast<int>(key_root)) % 12 + 12) % 12;

    int bass_degree = -1;
    int scale_size = static_cast<int>(scale.size());
    for (int i = 0; i < scale_size; ++i) {
        if (scale[i] == bass_offset) {
            bass_degree = i;
            break;
        }
    }

    // If bass is not in scale, find nearest degree below
    if (bass_degree < 0) {
        for (int i = scale_size - 1; i >= 0; --i) {
            if (scale[i] < bass_offset) {
                bass_degree = i;
                break;
            }
        }
        if (bass_degree < 0) bass_degree = 0;
    }

    // Move up by (generic_interval - 1) scale degrees.
    // Safe positive modulo to prevent negative index when bass_degree
    // + generic_interval - 1 is negative (cannot happen with guard above,
    // but protects against future callers).
    int raw = bass_degree + generic_interval - 1;
    int target_degree = ((raw % scale_size) + scale_size) % scale_size;
    int target_offset = scale[target_degree];
    return static_cast<PitchClass>((static_cast<int>(key_root) + target_offset) % 12);
}

}  // namespace

Result<FiguredBassRealisation> realise_figured_bass(
    MidiNote bass_note,
    const FiguredBassSymbol& symbol,
    PitchClass key_root,
    std::span<const Interval> key_scale,
    int upper_octave
) {
    if (bass_note > 127) {
        return std::unexpected(ErrorCode::InvalidMidiNote);
    }
    if (key_scale.empty()) {
        return std::unexpected(ErrorCode::InvalidScaleName);
    }

    PitchClass bass_pc = pitch_class(bass_note);
    int bass_oct = (bass_note / 12) - 1;  // MIDI octave convention

    if (upper_octave < 0) {
        upper_octave = bass_oct + 1;
    }

    FiguredBassRealisation result;
    result.bass = bass_note;

    // Generate upper voices
    for (const auto& fig : symbol.figures) {
        PitchClass target_pc = diatonic_above(
            bass_pc, fig.interval, key_root, key_scale);

        // Apply accidental
        int pc_val = static_cast<int>(target_pc);
        if (fig.accidental == FigureAccidental::Sharp) {
            pc_val = (pc_val + 1) % 12;
        } else if (fig.accidental == FigureAccidental::Flat) {
            pc_val = (pc_val + 11) % 12;
        }

        // Place in upper octave, ensuring above bass
        MidiNote note = static_cast<MidiNote>((upper_octave + 1) * 12 + pc_val);
        while (note <= bass_note && note < 120) {
            note += 12;
        }

        result.upper.push_back(note);
    }

    // Sort upper voices ascending
    std::sort(result.upper.begin(), result.upper.end());

    // Build complete voicing
    result.all_notes.push_back(bass_note);
    result.all_notes.insert(
        result.all_notes.end(), result.upper.begin(), result.upper.end());

    return result;
}

// =============================================================================
// Sequence Realisation (voice-led)
// =============================================================================

Result<FiguredBassSequenceResult> realise_figured_bass_sequence(
    std::span<const FiguredBassEvent> events,
    PitchClass key_root,
    std::span<const Interval> key_scale
) {
    if (events.empty()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    FiguredBassSequenceResult seq;
    seq.realisations.reserve(events.size());

    // First event: direct realisation
    auto first = realise_figured_bass(
        events[0].bass_note, events[0].symbol, key_root, key_scale);
    if (!first) return std::unexpected(first.error());
    seq.realisations.push_back(std::move(*first));

    // Subsequent events: realise then voice-lead upper voices
    for (std::size_t i = 1; i < events.size(); ++i) {
        auto target = realise_figured_bass(
            events[i].bass_note, events[i].symbol, key_root, key_scale);
        if (!target) return std::unexpected(target.error());

        const auto& prev_upper = seq.realisations[i - 1].upper;
        const auto& new_upper = target->upper;

        // Voice-led connection requires matching cardinality between successive
        // upper-voice sets. When figured bass symbols produce different numbers of
        // upper voices (e.g., "5/3" yields 2 upper voices, "7" yields 3), the
        // cardinalities differ and optimal voice leading cannot establish a bijection.
        // In that case, fall back to direct realisation for the current chord,
        // accepting the positional discontinuity as unavoidable.
        if (!prev_upper.empty() && prev_upper.size() == new_upper.size()) {
            // Extract target pitch classes
            std::vector<PitchClass> target_pcs;
            target_pcs.reserve(new_upper.size());
            for (auto n : new_upper) {
                target_pcs.push_back(pitch_class(n));
            }

            auto vl = voice_lead_optimal(prev_upper, target_pcs);
            if (vl) {
                FiguredBassRealisation led;
                led.bass = events[i].bass_note;
                led.upper = vl->voiced_notes;
                std::sort(led.upper.begin(), led.upper.end());
                led.all_notes.push_back(led.bass);
                led.all_notes.insert(
                    led.all_notes.end(), led.upper.begin(), led.upper.end());
                seq.realisations.push_back(std::move(led));
                continue;
            }
        }

        // Fallback: use the direct realisation
        seq.realisations.push_back(std::move(*target));
    }

    return seq;
}

}  // namespace Sunny::Core
