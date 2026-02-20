/**
 * @file SICM001A.h
 * @brief Score IR MIDI compilation — end-to-end pipeline from Score to MIDI data
 *
 * Component: SICM001A
 * Domain: SI (Score IR) | Category: CM (Compilation)
 *
 * Consumes temporal conversion, velocity resolution, articulation duration,
 * and tick quantisation to produce MIDI event data from a Score document.
 * The output type CompiledMidi lives here rather than in Sunny.Infrastructure
 * to avoid a reverse dependency from Core to Infrastructure.
 *
 * Precondition: is_compilable(score) — no Error-level validation diagnostics.
 */

#pragma once

#include "SIDC001A.h"

#include <cstdint>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Compiled MIDI data types
// =============================================================================

struct MidiNoteData {
    std::int64_t tick;
    std::int64_t duration_ticks;
    std::uint8_t channel;      ///< From Part::definition.rendering.midi_channel
    std::uint8_t note;         ///< MIDI note number [0, 127]
    std::uint8_t velocity;     ///< Resolved velocity [1, 127]
};

struct MidiTempoData {
    std::int64_t tick;
    std::uint32_t microseconds_per_beat;
};

struct MidiTimeSigData {
    std::int64_t tick;
    std::uint8_t numerator;
    std::uint8_t denominator;  ///< Power-of-2 exponent for SMF encoding
};

struct MidiKeySigData {
    std::int64_t tick;
    std::int8_t accidentals;   ///< Negative = flats, positive = sharps
    bool minor;
};

struct CompiledMidi {
    std::uint16_t ppq = 480;
    std::vector<MidiNoteData> notes;
    std::vector<MidiTempoData> tempos;
    std::vector<MidiTimeSigData> time_signatures;
    std::vector<MidiKeySigData> key_signatures;
};

// =============================================================================
// Compilation API
// =============================================================================

/**
 * @brief Compile a Score to MIDI event data
 *
 * Traverses all parts, resolving velocity (base dynamic + hairpin
 * interpolation + articulation offset + orchestration balance),
 * duration (articulation factor, tie chain accumulation), and
 * position (ScoreTime -> AbsoluteBeat -> tick).
 *
 * @param score Source score; must satisfy is_compilable()
 * @param ppq Pulses per quarter note (default 480)
 * @return CompiledMidi or error if score has Error-level diagnostics
 */
[[nodiscard]] Result<CompiledMidi> compile_to_midi(
    const Score& score,
    int ppq = 480
);

}  // namespace Sunny::Core
