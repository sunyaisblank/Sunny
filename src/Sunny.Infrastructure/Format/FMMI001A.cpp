/**
 * @file FMMI001A.cpp
 * @brief Standard MIDI File reader/writer implementation
 *
 * Component: FMMI001A
 */

#include "FMMI001A.h"

#include <algorithm>
#include <cstring>
#include <numeric>

namespace Sunny::Infrastructure::Format {

namespace {

// =============================================================================
// VLQ (Variable-Length Quantity) codec
// =============================================================================

std::vector<uint8_t> encode_vlq(uint32_t value) {
    std::vector<uint8_t> result;
    // Encode 7 bits at a time, MSB first
    if (value == 0) {
        result.push_back(0);
        return result;
    }

    // First, collect bytes in reverse order
    std::vector<uint8_t> tmp;
    while (value > 0) {
        tmp.push_back(static_cast<uint8_t>(value & 0x7F));
        value >>= 7;
    }

    // Output MSB first with continuation bits
    for (int i = static_cast<int>(tmp.size()) - 1; i >= 0; --i) {
        uint8_t byte = tmp[static_cast<std::size_t>(i)];
        if (i > 0) byte |= 0x80;  // set continuation bit
        result.push_back(byte);
    }
    return result;
}

Sunny::Core::Result<uint32_t> decode_vlq(std::span<const uint8_t> data, std::size_t& pos) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        if (pos >= data.size()) {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
        }
        uint8_t byte = data[pos++];
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return value;
        }
    }
    return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
}

// =============================================================================
// Binary read helpers
// =============================================================================

uint16_t read_u16_be(std::span<const uint8_t> data, std::size_t pos) {
    return static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]);
}

uint32_t read_u32_be(std::span<const uint8_t> data, std::size_t pos) {
    return (static_cast<uint32_t>(data[pos]) << 24) |
           (static_cast<uint32_t>(data[pos + 1]) << 16) |
           (static_cast<uint32_t>(data[pos + 2]) << 8) |
           static_cast<uint32_t>(data[pos + 3]);
}

void write_u16_be(std::vector<uint8_t>& out, uint16_t val) {
    out.push_back(static_cast<uint8_t>(val >> 8));
    out.push_back(static_cast<uint8_t>(val & 0xFF));
}

void write_u32_be(std::vector<uint8_t>& out, uint32_t val) {
    out.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(val & 0xFF));
}

/// Track a pending note-on for pairing with note-off
struct PendingNote {
    uint32_t tick;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

}  // anonymous namespace

// =============================================================================
// parse_midi
// =============================================================================

Sunny::Core::Result<MidiFile> parse_midi(std::span<const uint8_t> data) {
    // Minimum: MThd(4) + length(4) + format(2) + ntrks(2) + division(2) = 14
    if (data.size() < 14) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
    }

    // Header chunk
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
    }

    uint32_t header_len = read_u32_be(data, 4);
    if (header_len < 6 || data.size() < 8 + header_len) {
        return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
    }

    MidiFile result;
    result.format = read_u16_be(data, 8);
    uint16_t ntrks = read_u16_be(data, 10);
    result.ppq = read_u16_be(data, 12);

    // Parse tracks
    std::size_t pos = 8 + header_len;
    for (uint16_t t = 0; t < ntrks; ++t) {
        if (pos + 8 > data.size()) {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
        }
        if (data[pos] != 'M' || data[pos + 1] != 'T' || data[pos + 2] != 'r' || data[pos + 3] != 'k') {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
        }

        uint32_t track_len = read_u32_be(data, pos + 4);
        pos += 8;

        if (pos + track_len > data.size()) {
            return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
        }

        std::size_t track_end = pos + track_len;
        uint32_t abs_tick = 0;
        uint8_t running_status = 0;
        std::vector<PendingNote> pending;

        while (pos < track_end) {
            auto delta = decode_vlq(data, pos);
            if (!delta) return std::unexpected(delta.error());
            abs_tick += *delta;

            if (pos >= track_end) {
                return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
            }

            uint8_t status = data[pos];

            // Meta event
            if (status == 0xFF) {
                ++pos;
                if (pos >= track_end) break;
                uint8_t meta_type = data[pos++];
                auto meta_len = decode_vlq(data, pos);
                if (!meta_len) return std::unexpected(meta_len.error());

                if (pos + *meta_len > track_end) {
                    return std::unexpected(Sunny::Core::ErrorCode::InvalidMidiFile);
                }

                if (meta_type == 0x51 && *meta_len == 3) {
                    // Tempo
                    uint32_t uspb = (static_cast<uint32_t>(data[pos]) << 16) |
                                    (static_cast<uint32_t>(data[pos + 1]) << 8) |
                                    static_cast<uint32_t>(data[pos + 2]);
                    result.tempos.push_back({abs_tick, uspb});
                } else if (meta_type == 0x58 && *meta_len >= 2) {
                    // Time signature
                    int num = data[pos];
                    int den = 1 << data[pos + 1];
                    result.time_signatures.push_back({abs_tick, num, den});
                }
                // End of track
                // (0x2F - just skip)

                pos += *meta_len;
                continue;
            }

            // SysEx (skip)
            if (status == 0xF0 || status == 0xF7) {
                ++pos;
                auto sysex_len = decode_vlq(data, pos);
                if (!sysex_len) return std::unexpected(sysex_len.error());
                pos += *sysex_len;
                continue;
            }

            // Channel messages
            if (status & 0x80) {
                running_status = status;
                ++pos;
            }
            // else: running status reuse

            uint8_t msg_type = running_status & 0xF0;
            uint8_t channel = running_status & 0x0F;

            if (msg_type == 0x90 || msg_type == 0x80) {
                // Note On/Off: 2 data bytes
                if (pos + 1 >= track_end) break;
                uint8_t note = data[pos++];
                uint8_t vel = data[pos++];

                if (msg_type == 0x90 && vel > 0) {
                    // Note On
                    pending.push_back({abs_tick, channel, note, vel});
                } else {
                    // Note Off (or Note On with vel=0)
                    for (auto it = pending.begin(); it != pending.end(); ++it) {
                        if (it->note == note && it->channel == channel) {
                            result.notes.push_back({
                                it->tick,
                                abs_tick - it->tick,
                                it->channel,
                                it->note,
                                it->velocity
                            });
                            pending.erase(it);
                            break;
                        }
                    }
                }
            } else if (msg_type == 0xA0 || msg_type == 0xB0 || msg_type == 0xE0) {
                // 2 data bytes: aftertouch, control change, pitch bend
                pos += 2;
            } else if (msg_type == 0xC0 || msg_type == 0xD0) {
                // 1 data byte: program change, channel pressure
                pos += 1;
            }
        }

        pos = track_end;
    }

    return result;
}

// =============================================================================
// write_midi
// =============================================================================

Sunny::Core::Result<std::vector<uint8_t>> write_midi(const MidiFile& file) {
    std::vector<uint8_t> out;

    // Header chunk
    out.push_back('M'); out.push_back('T'); out.push_back('h'); out.push_back('d');
    write_u32_be(out, 6);          // header length
    write_u16_be(out, 0);          // format 0 (single track)
    write_u16_be(out, 1);          // 1 track
    write_u16_be(out, file.ppq);

    // Build track data
    std::vector<uint8_t> track_data;

    // Collect all events with absolute ticks for sorting
    struct TrackEvent {
        uint32_t tick;
        int priority;  // lower = earlier at same tick (tempo before notes)
        std::vector<uint8_t> data;
    };
    std::vector<TrackEvent> events;

    // Tempo events
    for (const auto& t : file.tempos) {
        std::vector<uint8_t> d = {0xFF, 0x51, 0x03};
        d.push_back(static_cast<uint8_t>((t.microseconds_per_beat >> 16) & 0xFF));
        d.push_back(static_cast<uint8_t>((t.microseconds_per_beat >> 8) & 0xFF));
        d.push_back(static_cast<uint8_t>(t.microseconds_per_beat & 0xFF));
        events.push_back({t.tick, 0, std::move(d)});
    }

    // Time signature events
    for (const auto& ts : file.time_signatures) {
        // Denominator as power of 2
        int den_pow = 0;
        int d = ts.denominator;
        while (d > 1) { d >>= 1; ++den_pow; }

        std::vector<uint8_t> ev = {0xFF, 0x58, 0x04};
        ev.push_back(static_cast<uint8_t>(ts.numerator));
        ev.push_back(static_cast<uint8_t>(den_pow));
        ev.push_back(24);   // clocks per metronome click
        ev.push_back(8);    // 32nd notes per quarter note
        events.push_back({ts.tick, 1, std::move(ev)});
    }

    // Note events (expand to note-on and note-off pairs)
    for (const auto& n : file.notes) {
        // Note On
        std::vector<uint8_t> on_data = {
            static_cast<uint8_t>(0x90 | (n.channel & 0x0F)),
            n.note,
            n.velocity
        };
        events.push_back({n.tick, 2, std::move(on_data)});

        // Note Off
        std::vector<uint8_t> off_data = {
            static_cast<uint8_t>(0x80 | (n.channel & 0x0F)),
            n.note,
            0
        };
        events.push_back({n.tick + n.duration_ticks, 3, std::move(off_data)});
    }

    // Sort by tick, then priority
    std::sort(events.begin(), events.end(), [](const TrackEvent& a, const TrackEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.priority < b.priority;
    });

    // Write events as delta-time + data
    uint32_t prev_tick = 0;
    for (const auto& ev : events) {
        uint32_t delta = ev.tick - prev_tick;
        prev_tick = ev.tick;
        auto vlq = encode_vlq(delta);
        track_data.insert(track_data.end(), vlq.begin(), vlq.end());
        track_data.insert(track_data.end(), ev.data.begin(), ev.data.end());
    }

    // End of track
    auto end_vlq = encode_vlq(0);
    track_data.insert(track_data.end(), end_vlq.begin(), end_vlq.end());
    track_data.push_back(0xFF);
    track_data.push_back(0x2F);
    track_data.push_back(0x00);

    // Track chunk header
    out.push_back('M'); out.push_back('T'); out.push_back('r'); out.push_back('k');
    write_u32_be(out, static_cast<uint32_t>(track_data.size()));
    out.insert(out.end(), track_data.begin(), track_data.end());

    return out;
}

// =============================================================================
// Conversion functions
// =============================================================================

std::vector<Sunny::Core::NoteEvent> midi_to_note_events(const MidiFile& file) {
    std::vector<Sunny::Core::NoteEvent> result;
    result.reserve(file.notes.size());

    for (const auto& n : file.notes) {
        // Convert ticks to beats: tick / ppq = beats (as fraction)
        Sunny::Core::Beat start{static_cast<int64_t>(n.tick), static_cast<int64_t>(file.ppq)};
        Sunny::Core::Beat dur{static_cast<int64_t>(n.duration_ticks), static_cast<int64_t>(file.ppq)};

        result.push_back({
            n.note,
            start.reduce(),
            dur.reduce(),
            n.velocity > 0 ? n.velocity : static_cast<uint8_t>(80),
            false
        });
    }

    return result;
}

MidiFile note_events_to_midi(
    std::span<const Sunny::Core::NoteEvent> events,
    uint16_t ppq,
    double bpm)
{
    MidiFile file;
    file.format = 0;
    file.ppq = ppq;

    // Add tempo event
    uint32_t uspb = static_cast<uint32_t>(60000000.0 / bpm);
    file.tempos.push_back({0, uspb});

    // Convert note events
    for (const auto& ev : events) {
        if (ev.muted) continue;

        // Beat to ticks: beat * ppq
        uint32_t tick = static_cast<uint32_t>(
            ev.start_time.numerator * ppq / ev.start_time.denominator);
        uint32_t dur_ticks = static_cast<uint32_t>(
            ev.duration.numerator * ppq / ev.duration.denominator);

        file.notes.push_back({
            tick,
            dur_ticks,
            0,  // channel 0
            ev.pitch,
            ev.velocity
        });
    }

    return file;
}

}  // namespace Sunny::Infrastructure::Format
