/**
 * @file FMMI001A.h
 * @brief Standard MIDI File (SMF) reader/writer
 *
 * Component: FMMI001A
 * Domain: FM (Format) | Category: MI (MIDI)
 *
 * Reads and writes Standard MIDI Files (Type 0 and Type 1).
 * Converts between tick-based MIDI events and Beat-based NoteEvents.
 *
 * Invariants:
 * - parse_midi ∘ write_midi = identity (round-trip for notes, tempo, time sig)
 * - VLQ encoding is canonical (minimal bytes)
 * - PPQ (pulses per quarter note) is preserved through conversion
 */

#pragma once

#include "../../Sunny.Core/Tensor/TNTP001A.h"
#include "../../Sunny.Core/Tensor/TNBT001A.h"
#include "../../Sunny.Core/Tensor/TNEV001A.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Sunny::Infrastructure::Format {

/// A MIDI tempo change event
struct MidiTempoEvent {
    uint32_t tick;
    uint32_t microseconds_per_beat;  ///< tempo in us/beat
};

/// A MIDI time signature event
struct MidiTimeSignatureEvent {
    uint32_t tick;
    int numerator;
    int denominator;
};

/// A MIDI note event (tick-based, paired note-on/off)
struct MidiNoteEvent {
    uint32_t tick;
    uint32_t duration_ticks;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

/// Parsed Standard MIDI File
struct MidiFile {
    uint16_t format;         ///< 0 or 1
    uint16_t ppq;            ///< pulses per quarter note
    std::vector<MidiNoteEvent> notes;
    std::vector<MidiTempoEvent> tempos;
    std::vector<MidiTimeSignatureEvent> time_signatures;
};

/// Parse SMF binary data
[[nodiscard]] Sunny::Core::Result<MidiFile> parse_midi(std::span<const uint8_t> data);

/// Write SMF Type 0 binary
[[nodiscard]] Sunny::Core::Result<std::vector<uint8_t>> write_midi(const MidiFile& file);

/// Convert tick-based MidiFile to Beat-based NoteEvents
[[nodiscard]] std::vector<Sunny::Core::NoteEvent> midi_to_note_events(const MidiFile& file);

/// Convert Beat-based NoteEvents to tick-based MidiFile
[[nodiscard]] MidiFile note_events_to_midi(
    std::span<const Sunny::Core::NoteEvent> events,
    uint16_t ppq = 480,
    double bpm = 120.0);

}  // namespace Sunny::Infrastructure::Format
