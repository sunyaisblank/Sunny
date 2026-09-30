/**
 * @file midi_file.hpp
 * @brief Standard MIDI File (SMF) reader/writer
 *
 *
 * Reads and writes Standard MIDI Files (Type 0 and Type 1).
 * Converts between tick-based MIDI events and Beat-based NoteEvents.
 *
 * Invariants:
 * - Type-0 write/parse preserves notes, tempo, time/key signatures, CC, and programs
 *   (the in-memory keyswitch ordering marker is intentionally not encoded)
 * - parsed tracks use the exact six-byte SMF header, end with the required
 *   End-of-Track event, consume the complete input, and contain paired
 *   positive-duration note intervals
 * - VLQ encoding is canonical (minimal bytes)
 * - PPQ (pulses per quarter note) is preserved through conversion
 */

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

/// A MIDI tempo change event
struct MidiTempoEvent {
    uint32_t tick;
    uint32_t microseconds_per_beat; ///< tempo in us/beat
    uint16_t track = 0;             ///< source SMF track
    uint32_t order = 0;             ///< one-based source event order; zero means unspecified
};

/// A MIDI time signature event
struct MidiTimeSignatureEvent {
    uint32_t tick;
    int numerator;
    int denominator;
    uint16_t track = 0;                      ///< source SMF track
    uint8_t clocks_per_metronome_click = 24; ///< SMF `cc` byte
    uint8_t notated_32nds_per_quarter = 8;   ///< SMF `bb` byte
    uint32_t order = 0; ///< one-based source event order; zero means unspecified
};

/// A MIDI key signature meta event
struct MidiKeySignatureEvent {
    uint32_t tick;
    int8_t accidentals; ///< Flats [-7, -1], neutral 0, sharps [1, 7]
    bool minor;
    uint16_t track = 0; ///< source SMF track
    uint32_t order = 0; ///< one-based source event order; zero means unspecified
};

/// A MIDI note event (tick-based, paired note-on/off)
struct MidiNoteEvent {
    uint32_t tick;
    uint32_t duration_ticks;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    bool keyswitch = false;       ///< Orders articulation switches before musical note-ons
    uint16_t track = 0;           ///< source SMF track
    uint8_t release_velocity = 0; ///< Note Off velocity [0, 127]
    uint32_t onset_order = 0;     ///< one-based source Note On order; zero means unspecified
    uint32_t release_order = 0;   ///< one-based source Note Off order; zero means unspecified
};

/// Valid SMF event classes parsed but not represented by MidiFile fields.
struct MidiParseLoss {
    uint64_t meta_events = 0;
    uint64_t sysex_events = 0;
    uint64_t polyphonic_aftertouch_events = 0;
    uint64_t channel_pressure_events = 0;
    uint64_t pitch_bend_events = 0;

    [[nodiscard]] constexpr bool empty() const noexcept {
        return meta_events == 0 && sysex_events == 0 && polyphonic_aftertouch_events == 0 &&
               channel_pressure_events == 0 && pitch_bend_events == 0;
    }
};

/// A MIDI control-change channel event
struct MidiControlChangeEvent {
    uint32_t tick;
    uint8_t channel;    ///< Wire channel [0, 15]
    uint8_t controller; ///< Controller [0, 127]
    uint8_t value;      ///< Value [0, 127]
    uint16_t track = 0; ///< source SMF track
    uint32_t order = 0; ///< one-based source event order; zero means unspecified
};

/// A MIDI program-change channel event
struct MidiProgramChangeEvent {
    uint32_t tick;
    uint8_t channel;    ///< Wire channel [0, 15]
    uint8_t program;    ///< Program [0, 127]
    uint16_t track = 0; ///< source SMF track
    uint32_t order = 0; ///< one-based source event order; zero means unspecified
};

/// Parsed Standard MIDI File
struct MidiFile {
    uint16_t format = 0;                   ///< 0 or 1 (writer accepts type 0)
    uint16_t ppq = 480;                    ///< pulses per quarter note
    uint16_t track_count = 1;              ///< declared tracks, including empty tracks
    std::vector<uint32_t> track_end_ticks; ///< exact EOT tick per track; empty derives on write
    std::vector<MidiNoteEvent> notes;
    std::vector<MidiTempoEvent> tempos;
    std::vector<MidiTimeSignatureEvent> time_signatures;
    std::vector<MidiKeySignatureEvent> key_signatures;
    std::vector<MidiControlChangeEvent> control_changes;
    std::vector<MidiProgramChangeEvent> program_changes;
    MidiParseLoss parse_loss; ///< explicit evidence for valid unmodelled source events
};

/// Parse SMF binary data
[[nodiscard]] sunny::core::Result<MidiFile> parse_midi(std::span<const uint8_t> data);

/// Write SMF Type 0 binary
[[nodiscard]] sunny::core::Result<std::vector<uint8_t>> write_midi(const MidiFile& file);

/// Convert the core compiler event model to a validated SMF type-0 model.
[[nodiscard]] sunny::core::Result<MidiFile>
compiled_midi_to_file(const sunny::core::CompiledMidi& compiled);

/// Project retained musical notes to whole-note-based NoteEvents.
/// Track/channel ownership is intentionally collapsed for analytical use;
/// the in-memory-only keyswitch marker is outside that projection and rejects.
[[nodiscard]] sunny::core::Result<std::vector<sunny::core::NoteEvent>>
midi_to_note_events(const MidiFile& file);

/// Convert whole-note-based sounding NoteEvents to channel-zero PPQ timing.
/// Muted source events reject because SMF cannot retain their mute state.
[[nodiscard]] sunny::core::Result<MidiFile> note_events_to_midi(
    std::span<const sunny::core::NoteEvent> events, uint16_t ppq = 480, double bpm = 120.0);

} // namespace sunny::infrastructure::formats
