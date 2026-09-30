/**
 * @file midi_file.cpp
 * @brief Standard MIDI File reader/writer implementation
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <sunny/core/score/time.hpp>
#include <sunny/infrastructure/formats/midi_file.hpp>

namespace sunny::infrastructure::formats {

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
        if (i > 0) byte |= 0x80; // set continuation bit
        result.push_back(byte);
    }
    return result;
}

sunny::core::Result<uint32_t> decode_vlq(std::span<const uint8_t> data, std::size_t& pos) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        if (pos >= data.size()) {
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        }
        uint8_t byte = data[pos++];
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return value;
        }
    }
    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
}

// =============================================================================
// Binary read helpers
// =============================================================================

sunny::core::Result<uint16_t> read_u16_be(std::span<const uint8_t> data, std::size_t pos) {
    if (pos + 2 > data.size()) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    return static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]);
}

sunny::core::Result<uint32_t> read_u32_be(std::span<const uint8_t> data, std::size_t pos) {
    if (pos + 4 > data.size()) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    return (static_cast<uint32_t>(data[pos]) << 24) | (static_cast<uint32_t>(data[pos + 1]) << 16) |
           (static_cast<uint32_t>(data[pos + 2]) << 8) | static_cast<uint32_t>(data[pos + 3]);
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
    uint32_t order;
};

} // anonymous namespace

// =============================================================================
// parse_midi
// =============================================================================

sunny::core::Result<MidiFile> parse_midi(std::span<const uint8_t> data) {
    // Minimum: MThd(4) + length(4) + format(2) + ntrks(2) + division(2) = 14
    if (data.size() < 14) {
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    }

    // Header chunk
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') {
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    }

    auto header_len_r = read_u32_be(data, 4);
    if (!header_len_r) return std::unexpected(header_len_r.error());
    uint32_t header_len = *header_len_r;
    if (header_len != 6 || data.size() < 8 + header_len) {
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    }

    MidiFile result;
    auto fmt_r = read_u16_be(data, 8);
    if (!fmt_r) return std::unexpected(fmt_r.error());
    result.format = *fmt_r;
    auto ntrks_r = read_u16_be(data, 10);
    if (!ntrks_r) return std::unexpected(ntrks_r.error());
    uint16_t ntrks = *ntrks_r;
    result.track_count = ntrks;
    result.track_end_ticks.resize(ntrks);
    auto ppq_r = read_u16_be(data, 12);
    if (!ppq_r) return std::unexpected(ppq_r.error());
    result.ppq = *ppq_r;
    if (result.format > 1 || ntrks == 0 || (result.format == 0 && ntrks != 1))
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    // SMPTE division uses the high bit and is outside Sunny's PPQ model.
    if (result.ppq == 0 || (result.ppq & 0x8000U) != 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiPPQ);

    // Parse tracks
    std::size_t pos = 8 + header_len;
    for (uint16_t t = 0; t < ntrks; ++t) {
        if (pos + 8 > data.size()) {
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        }
        if (data[pos] != 'M' || data[pos + 1] != 'T' || data[pos + 2] != 'r' ||
            data[pos + 3] != 'k') {
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        }

        auto track_len_r = read_u32_be(data, pos + 4);
        if (!track_len_r) return std::unexpected(track_len_r.error());
        uint32_t track_len = *track_len_r;
        pos += 8;

        if (pos + track_len > data.size()) {
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        }

        std::size_t track_end = pos + track_len;
        uint32_t abs_tick = 0;
        uint8_t running_status = 0;
        std::vector<PendingNote> pending;
        bool saw_end_of_track = false;
        std::uint32_t next_event_order = 1;

        while (pos < track_end) {
            auto delta = decode_vlq(data, pos);
            if (!delta) return std::unexpected(delta.error());
            if (*delta > std::numeric_limits<uint32_t>::max() - abs_tick)
                return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
            abs_tick += *delta;
            const auto event_order = next_event_order++;

            if (pos >= track_end) {
                return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
            }

            uint8_t status = data[pos];

            // Meta event
            if (status == 0xFF) {
                running_status = 0;
                ++pos;
                if (pos >= track_end)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                uint8_t meta_type = data[pos++];
                auto meta_len = decode_vlq(data, pos);
                if (!meta_len) return std::unexpected(meta_len.error());

                if (pos + *meta_len > track_end) {
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                }

                if (meta_type == 0x51) {
                    if (*meta_len != 3)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);
                    // Tempo
                    uint32_t uspb = (static_cast<uint32_t>(data[pos]) << 16) |
                                    (static_cast<uint32_t>(data[pos + 1]) << 8) |
                                    static_cast<uint32_t>(data[pos + 2]);
                    if (uspb == 0) return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);
                    result.tempos.push_back({abs_tick, uspb, t, event_order});
                } else if (meta_type == 0x58) {
                    if (*meta_len != 4)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTimeSig);
                    // Time signature
                    uint8_t den_exp = data[pos + 1];
                    if (den_exp > 7)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTimeSig);
                    int num = data[pos];
                    if (num == 0)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTimeSig);
                    int den = 1 << den_exp;
                    result.time_signatures.push_back(
                        {abs_tick, num, den, t, data[pos + 2], data[pos + 3], event_order});
                } else if (meta_type == 0x59) {
                    if (*meta_len != 2)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                    const auto accidentals = static_cast<std::int8_t>(data[pos]);
                    if (accidentals < -7 || accidentals > 7 || data[pos + 1] > 1)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                    result.key_signatures.push_back(
                        {abs_tick, accidentals, data[pos + 1] == 1, t, event_order});
                } else if (meta_type == 0x2F) {
                    if (*meta_len != 0 || saw_end_of_track || pos != track_end)
                        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                    saw_end_of_track = true;
                    result.track_end_ticks[t] = abs_tick;
                } else {
                    ++result.parse_loss.meta_events;
                }

                pos += *meta_len;
                continue;
            }

            // SysEx (skip)
            if (status == 0xF0 || status == 0xF7) {
                running_status = 0;
                ++pos;
                auto sysex_len = decode_vlq(data, pos);
                if (!sysex_len) return std::unexpected(sysex_len.error());
                if (*sysex_len > track_end - pos)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                pos += *sysex_len;
                ++result.parse_loss.sysex_events;
                continue;
            }

            // Channel messages
            if (status & 0x80) {
                if (status >= 0xF0) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                running_status = status;
                ++pos;
            }
            // else: running status reuse

            if (running_status < 0x80 || running_status >= 0xF0)
                return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);

            uint8_t msg_type = running_status & 0xF0;
            uint8_t channel = running_status & 0x0F;

            if (msg_type == 0x90 || msg_type == 0x80) {
                // Note On/Off: 2 data bytes
                if (track_end - pos < 2)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                uint8_t note = data[pos++];
                uint8_t vel = data[pos++];
                if (note > 127 || vel > 127)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);

                if (msg_type == 0x90 && vel > 0) {
                    // Note On
                    pending.push_back({abs_tick, channel, note, vel, event_order});
                } else {
                    // Note Off (or Note On with vel=0)
                    bool matched = false;
                    for (auto it = pending.begin(); it != pending.end(); ++it) {
                        if (it->note == note && it->channel == channel) {
                            if (abs_tick == it->tick)
                                return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                            result.notes.push_back({it->tick,
                                                    abs_tick - it->tick,
                                                    it->channel,
                                                    it->note,
                                                    it->velocity,
                                                    false,
                                                    t,
                                                    vel,
                                                    it->order,
                                                    event_order});
                            pending.erase(it);
                            matched = true;
                            break;
                        }
                    }
                    if (!matched) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                }
            } else if (msg_type == 0xA0 || msg_type == 0xB0 || msg_type == 0xE0) {
                // 2 data bytes: aftertouch, control change, pitch bend
                if (track_end - pos < 2)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                const uint8_t first = data[pos++];
                const uint8_t second = data[pos++];
                if (first > 127 || second > 127)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                if (msg_type == 0xB0) {
                    result.control_changes.push_back(
                        {abs_tick, channel, first, second, t, event_order});
                } else if (msg_type == 0xA0) {
                    ++result.parse_loss.polyphonic_aftertouch_events;
                } else {
                    ++result.parse_loss.pitch_bend_events;
                }
            } else if (msg_type == 0xC0 || msg_type == 0xD0) {
                // 1 data byte: program change, channel pressure
                if (track_end - pos < 1)
                    return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                const uint8_t value = data[pos++];
                if (value > 127) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
                if (msg_type == 0xC0) {
                    result.program_changes.push_back({abs_tick, channel, value, t, event_order});
                } else {
                    ++result.parse_loss.channel_pressure_events;
                }
            } else {
                return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
            }
        }

        if (!saw_end_of_track || !pending.empty())
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);

        pos = track_end;
    }

    if (pos != data.size()) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    return result;
}

// =============================================================================
// write_midi
// =============================================================================

sunny::core::Result<std::vector<uint8_t>> write_midi(const MidiFile& file) {
    if (file.format != 0) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (file.track_count != 1) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (!file.parse_loss.empty()) return std::unexpected(sunny::core::ErrorCode::FormatError);
    if (!file.track_end_ticks.empty() && file.track_end_ticks.size() != 1)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (file.ppq == 0 || (file.ppq & 0x8000U) != 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiPPQ);

    std::vector<uint8_t> out;

    // Header chunk
    out.push_back('M');
    out.push_back('T');
    out.push_back('h');
    out.push_back('d');
    write_u32_be(out, 6); // header length
    write_u16_be(out, 0); // format 0 (single track)
    write_u16_be(out, 1); // 1 track
    write_u16_be(out, file.ppq);

    // Build track data
    std::vector<uint8_t> track_data;

    // Collect all events with absolute ticks for sorting
    struct TrackEvent {
        uint32_t tick;
        int priority; // lower = earlier at same tick (tempo before notes)
        uint32_t source_order;
        std::vector<uint8_t> data;
    };
    std::vector<TrackEvent> events;

    // Tempo events
    for (const auto& t : file.tempos) {
        if (t.track != 0 || t.microseconds_per_beat == 0 || t.microseconds_per_beat > 0xFF'FF'FFU)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);
        std::vector<uint8_t> d = {0xFF, 0x51, 0x03};
        d.push_back(static_cast<uint8_t>((t.microseconds_per_beat >> 16) & 0xFF));
        d.push_back(static_cast<uint8_t>((t.microseconds_per_beat >> 8) & 0xFF));
        d.push_back(static_cast<uint8_t>(t.microseconds_per_beat & 0xFF));
        events.push_back({t.tick, 0, t.order, std::move(d)});
    }

    // Time signature events
    for (const auto& ts : file.time_signatures) {
        if (ts.track != 0 || ts.numerator <= 0 || ts.numerator > 255 || ts.denominator <= 0 ||
            (ts.denominator & (ts.denominator - 1)) != 0 || ts.denominator > 128)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiTimeSig);
        // Denominator as power of 2
        int den_pow = 0;
        int d = ts.denominator;
        while (d > 1) {
            d >>= 1;
            ++den_pow;
        }

        std::vector<uint8_t> ev = {0xFF, 0x58, 0x04};
        ev.push_back(static_cast<uint8_t>(ts.numerator));
        ev.push_back(static_cast<uint8_t>(den_pow));
        ev.push_back(ts.clocks_per_metronome_click);
        ev.push_back(ts.notated_32nds_per_quarter);
        events.push_back({ts.tick, 1, ts.order, std::move(ev)});
    }

    // Key signature events
    for (const auto& key : file.key_signatures) {
        if (key.track != 0 || key.accidentals < -7 || key.accidentals > 7)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        std::vector<uint8_t> data = {0xFF,
                                     0x59,
                                     0x02,
                                     static_cast<std::uint8_t>(key.accidentals),
                                     static_cast<std::uint8_t>(key.minor ? 1 : 0)};
        events.push_back({key.tick, 2, key.order, std::move(data)});
    }

    // Program changes precede CC and note-on events at the same tick.
    for (const auto& program : file.program_changes) {
        if (program.track != 0 || program.channel > 15 || program.program > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        events.push_back({program.tick,
                          10,
                          program.order,
                          {static_cast<std::uint8_t>(0xC0U | program.channel), program.program}});
    }

    for (const auto& cc : file.control_changes) {
        if (cc.track != 0 || cc.channel > 15 || cc.controller > 127 || cc.value > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        events.push_back(
            {cc.tick,
             20,
             cc.order,
             {static_cast<std::uint8_t>(0xB0U | cc.channel), cc.controller, cc.value}});
    }

    // Note events (expand to note-on and note-off pairs). SMF note endings identify only
    // channel/key, so overlapping instances of the same key are paired FIFO by the parser. Emit
    // equal-endpoint messages in onset order and reject nested endpoints that cannot preserve the
    // caller's interval pairing.
    std::vector<const MidiNoteEvent*> ordered_notes;
    ordered_notes.reserve(file.notes.size());
    for (const auto& n : file.notes) {
        if (n.track != 0 || n.channel > 15 || n.note > 127 || n.release_velocity > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        if (!sunny::core::is_valid_velocity(n.velocity))
            return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
        if (n.duration_ticks == 0) return std::unexpected(sunny::core::ErrorCode::InvalidBeat);
        if (n.duration_ticks > std::numeric_limits<uint32_t>::max() - n.tick)
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        ordered_notes.push_back(&n);
    }
    std::stable_sort(ordered_notes.begin(), ordered_notes.end(), [](const auto* a, const auto* b) {
        if (a->tick != b->tick) return a->tick < b->tick;
        if (a->onset_order != 0 && b->onset_order != 0) return a->onset_order < b->onset_order;
        return false;
    });

    std::array<std::uint32_t, 16 * 128> last_end{};
    std::array<std::uint32_t, 16 * 128> last_release_order{};
    std::array<bool, 16 * 128> saw_key{};
    for (const auto* note : ordered_notes) {
        const auto& n = *note;
        const auto key = static_cast<std::size_t>(n.channel) * 128 + n.note;
        const auto end = n.tick + n.duration_ticks;
        if (saw_key[key] && (end < last_end[key] || (end == last_end[key] && n.release_order != 0 &&
                                                     n.release_order < last_release_order[key])))
            return std::unexpected(sunny::core::ErrorCode::FormatError);
        saw_key[key] = true;
        last_end[key] = end;
        last_release_order[key] = n.release_order;

        // Note On
        std::vector<uint8_t> on_data = {
            static_cast<uint8_t>(0x90U | n.channel), n.note, n.velocity};
        events.push_back({n.tick, n.keyswitch ? 40 : 50, n.onset_order, std::move(on_data)});

        // Note Off
        std::vector<uint8_t> off_data = {
            static_cast<uint8_t>(0x80U | n.channel), n.note, n.release_velocity};
        events.push_back({end, 30, n.release_order, std::move(off_data)});
    }

    const bool has_explicit_order =
        std::ranges::any_of(events, [](const auto& event) { return event.source_order != 0; });
    const bool has_implicit_order =
        std::ranges::any_of(events, [](const auto& event) { return event.source_order == 0; });
    if (has_explicit_order && has_implicit_order)
        return std::unexpected(sunny::core::ErrorCode::FormatError);

    // Parsed files carry exact source order, which can affect same-tick playback semantics.
    // Manually constructed files with all-zero order use Sunny's canonical priority ordering.
    std::stable_sort(
        events.begin(), events.end(), [has_explicit_order](const auto& a, const auto& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return has_explicit_order ? a.source_order < b.source_order : a.priority < b.priority;
        });
    if (has_explicit_order) {
        std::uint32_t previous_order = 0;
        for (const auto& event : events) {
            if (event.source_order <= previous_order)
                return std::unexpected(sunny::core::ErrorCode::FormatError);
            previous_order = event.source_order;
        }
    }

    // Write events as delta-time + data
    uint32_t prev_tick = 0;
    for (const auto& ev : events) {
        uint32_t delta = ev.tick - prev_tick;
        if (delta > 0x0FFF'FFFFU)
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        prev_tick = ev.tick;
        auto vlq = encode_vlq(delta);
        track_data.insert(track_data.end(), vlq.begin(), vlq.end());
        track_data.insert(track_data.end(), ev.data.begin(), ev.data.end());
    }

    // End of track. A parsed file retains its declared endpoint, including
    // trailing silence; a manually built file derives it from the last event.
    const std::uint32_t end_tick =
        file.track_end_ticks.empty() ? prev_tick : file.track_end_ticks.front();
    if (end_tick < prev_tick) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    const auto end_delta = end_tick - prev_tick;
    if (end_delta > 0x0FFF'FFFFU)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    auto end_vlq = encode_vlq(end_delta);
    track_data.insert(track_data.end(), end_vlq.begin(), end_vlq.end());
    track_data.push_back(0xFF);
    track_data.push_back(0x2F);
    track_data.push_back(0x00);

    // Track chunk header
    out.push_back('M');
    out.push_back('T');
    out.push_back('r');
    out.push_back('k');
    if (track_data.size() > std::numeric_limits<uint32_t>::max())
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    write_u32_be(out, static_cast<uint32_t>(track_data.size()));
    out.insert(out.end(), track_data.begin(), track_data.end());

    return out;
}

// =============================================================================
// Conversion functions
// =============================================================================

sunny::core::Result<MidiFile> compiled_midi_to_file(const sunny::core::CompiledMidi& compiled) {
    if (compiled.ppq == 0 || (compiled.ppq & 0x8000U) != 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiPPQ);

    constexpr auto maximum_tick = std::numeric_limits<std::uint32_t>::max();
    const auto checked_tick = [](std::int64_t tick) -> sunny::core::Result<std::uint32_t> {
        if (tick < 0 || static_cast<std::uint64_t>(tick) > maximum_tick)
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        return static_cast<std::uint32_t>(tick);
    };
    const auto checked_channel = [](std::uint8_t channel) -> sunny::core::Result<std::uint8_t> {
        if (channel < 1 || channel > 16)
            return std::unexpected(sunny::core::ErrorCode::InvalidRenderingConfig);
        return static_cast<std::uint8_t>(channel - 1);
    };

    MidiFile file;
    file.format = 0;
    file.ppq = compiled.ppq;

    for (const auto& tempo : compiled.tempos) {
        auto tick = checked_tick(tempo.tick);
        if (!tick) return std::unexpected(tick.error());
        if (tempo.microseconds_per_beat == 0 || tempo.microseconds_per_beat > 0xFF'FF'FFU)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);
        file.tempos.push_back({*tick, tempo.microseconds_per_beat});
    }
    for (const auto& signature : compiled.time_signatures) {
        auto tick = checked_tick(signature.tick);
        if (!tick) return std::unexpected(tick.error());
        file.time_signatures.push_back(MidiTimeSignatureEvent{
            .tick = *tick,
            .numerator = signature.numerator,
            .denominator = signature.denominator,
            .track = 0,
            .clocks_per_metronome_click = signature.clocks_per_metronome_click,
            .notated_32nds_per_quarter = signature.notated_32nds_per_quarter,
            .order = 0});
    }
    for (const auto& signature : compiled.key_signatures) {
        auto tick = checked_tick(signature.tick);
        if (!tick) return std::unexpected(tick.error());
        if (signature.accidentals < -7 || signature.accidentals > 7)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        file.key_signatures.push_back({*tick, signature.accidentals, signature.minor});
    }

    for (const auto& program : compiled.program_changes) {
        auto tick = checked_tick(program.tick);
        if (!tick) return std::unexpected(tick.error());
        auto channel = checked_channel(program.channel);
        if (!channel) return std::unexpected(channel.error());
        if (program.program > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidRenderingConfig);
        file.program_changes.push_back({*tick, *channel, program.program});
    }
    for (const auto& cc : compiled.control_changes) {
        auto tick = checked_tick(cc.tick);
        if (!tick) return std::unexpected(tick.error());
        auto channel = checked_channel(cc.channel);
        if (!channel) return std::unexpected(channel.error());
        if (cc.controller > 127 || cc.value > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidRenderingConfig);
        file.control_changes.push_back({*tick, *channel, cc.controller, cc.value});
    }

    const auto append_note = [&](std::int64_t tick_value,
                                 std::int64_t duration_value,
                                 std::uint8_t channel_value,
                                 std::uint8_t note,
                                 std::uint8_t velocity,
                                 std::uint8_t release_velocity,
                                 bool keyswitch) -> sunny::core::Result<void> {
        auto tick = checked_tick(tick_value);
        if (!tick) return std::unexpected(tick.error());
        if (duration_value <= 0 || static_cast<std::uint64_t>(duration_value) > maximum_tick ||
            static_cast<std::uint64_t>(duration_value) > maximum_tick - *tick)
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        auto channel = checked_channel(channel_value);
        if (!channel) return std::unexpected(channel.error());
        if (note > 127) return std::unexpected(sunny::core::ErrorCode::InvalidMidiNote);
        if (!sunny::core::is_valid_velocity(velocity))
            return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
        if (release_velocity > 127) return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
        file.notes.push_back({*tick,
                              static_cast<std::uint32_t>(duration_value),
                              *channel,
                              note,
                              velocity,
                              keyswitch,
                              0,
                              release_velocity});
        return {};
    };

    for (const auto& keyswitch : compiled.keyswitches) {
        auto added = append_note(keyswitch.tick,
                                 keyswitch.duration_ticks,
                                 keyswitch.channel,
                                 keyswitch.note,
                                 keyswitch.velocity,
                                 0,
                                 true);
        if (!added) return std::unexpected(added.error());
    }
    for (const auto& note : compiled.notes) {
        auto added = append_note(note.tick,
                                 note.duration_ticks,
                                 note.channel,
                                 note.note,
                                 note.velocity,
                                 note.release_velocity,
                                 false);
        if (!added) return std::unexpected(added.error());
    }

    return file;
}

sunny::core::Result<std::vector<sunny::core::NoteEvent>> midi_to_note_events(const MidiFile& file) {
    if (file.format > 1) return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (file.track_count == 0 || (file.format == 0 && file.track_count != 1))
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (!file.track_end_ticks.empty() && file.track_end_ticks.size() != file.track_count)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
    if (file.ppq == 0 || (file.ppq & 0x8000U) != 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiPPQ);

    std::vector<sunny::core::NoteEvent> result;
    result.reserve(file.notes.size());

    for (const auto& n : file.notes) {
        if (n.track >= file.track_count || n.channel > 15 || n.keyswitch)
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        if (n.duration_ticks > std::numeric_limits<std::uint32_t>::max() - n.tick ||
            (!file.track_end_ticks.empty() &&
             n.tick + n.duration_ticks > file.track_end_ticks[n.track]))
            return std::unexpected(sunny::core::ErrorCode::InvalidMidiFile);
        // A note byte outside [0, 127] can only come from malformed input.
        auto pitch = sunny::core::MidiNote::from_int(n.note);
        if (!pitch) return std::unexpected(pitch.error());
        if (!sunny::core::is_valid_velocity(n.velocity))
            return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
        if (n.duration_ticks == 0) return std::unexpected(sunny::core::ErrorCode::InvalidBeat);
        // MIDI and Live count quarter notes; Sunny Beat counts whole notes.
        const auto denominator = 4 * static_cast<std::int64_t>(file.ppq);
        sunny::core::Beat start{static_cast<int64_t>(n.tick), denominator};
        sunny::core::Beat dur{static_cast<int64_t>(n.duration_ticks), denominator};

        result.push_back(
            {*pitch, start.reduce(), dur.reduce(), n.velocity, false, n.release_velocity});
    }

    return result;
}

sunny::core::Result<MidiFile>
note_events_to_midi(std::span<const sunny::core::NoteEvent> events, uint16_t ppq, double bpm) {
    if (ppq == 0 || (ppq & 0x8000U) != 0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiPPQ);
    if (!std::isfinite(bpm) || bpm <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);

    const double microseconds = 60'000'000.0 / bpm;
    if (!std::isfinite(microseconds) || microseconds < 1.0 || microseconds > 0xFF'FF'FF)
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiTempo);

    MidiFile file;
    file.format = 0;
    file.ppq = ppq;

    // Add tempo event
    const auto uspb = static_cast<uint32_t>(std::llround(microseconds));
    file.tempos.push_back({0, uspb});

    // Convert note events
    for (const auto& ev : events) {
        if (ev.muted) return std::unexpected(sunny::core::ErrorCode::FormatError);

        if (ev.start_time.numerator() < 0 || ev.duration.numerator() <= 0)
            return std::unexpected(sunny::core::ErrorCode::InvalidBeat);
        if (!sunny::core::is_valid_velocity(static_cast<int>(ev.velocity)))
            return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
        if (ev.release_velocity > 127)
            return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);

        const auto tick_value = sunny::core::absolute_beat_to_tick(ev.start_time, ppq);
        const auto duration_value = sunny::core::absolute_beat_to_tick(ev.duration, ppq);
        constexpr auto maximum_tick = std::numeric_limits<uint32_t>::max();
        if (tick_value < 0 || duration_value <= 0 ||
            static_cast<std::uint64_t>(tick_value) > maximum_tick ||
            static_cast<std::uint64_t>(duration_value) > maximum_tick ||
            static_cast<std::uint64_t>(duration_value) >
                maximum_tick - static_cast<std::uint64_t>(tick_value))
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

        const auto tick = static_cast<uint32_t>(tick_value);
        const auto dur_ticks = static_cast<uint32_t>(duration_value);

        file.notes.push_back({tick,
                              dur_ticks,
                              0, // channel 0
                              ev.pitch,
                              ev.velocity,
                              false,
                              0,
                              ev.release_velocity});
    }

    return file;
}

} // namespace sunny::infrastructure::formats
