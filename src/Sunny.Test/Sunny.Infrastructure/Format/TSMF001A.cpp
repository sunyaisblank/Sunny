/**
 * @file TSMF001A.cpp
 * @brief Unit tests for FMMI001A (Standard MIDI File reader/writer)
 *
 * Component: TSMF001A
 * Domain: TS (Test) | Category: MF (MIDI File)
 *
 * Tests: FMMI001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Format/FMMI001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// Helpers: build minimal SMF data
// =============================================================================

namespace {

/// Build a minimal valid SMF Type 0 with an empty track
std::vector<uint8_t> make_empty_smf(uint16_t ppq = 480) {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M'); data.push_back('T'); data.push_back('h'); data.push_back('d');
    // Header length = 6
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(6);
    // Format 0
    data.push_back(0); data.push_back(0);
    // 1 track
    data.push_back(0); data.push_back(1);
    // PPQ
    data.push_back(static_cast<uint8_t>(ppq >> 8));
    data.push_back(static_cast<uint8_t>(ppq & 0xFF));
    // MTrk
    data.push_back('M'); data.push_back('T'); data.push_back('r'); data.push_back('k');
    // Track length = 4 (just end-of-track)
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(4);
    // Delta=0, End of track
    data.push_back(0x00);
    data.push_back(0xFF); data.push_back(0x2F); data.push_back(0x00);
    return data;
}

/// Build SMF with a single note (C4, quarter note at tick 0)
std::vector<uint8_t> make_single_note_smf(uint16_t ppq = 480) {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M'); data.push_back('T'); data.push_back('h'); data.push_back('d');
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(6);
    data.push_back(0); data.push_back(0);  // format 0
    data.push_back(0); data.push_back(1);  // 1 track
    data.push_back(static_cast<uint8_t>(ppq >> 8));
    data.push_back(static_cast<uint8_t>(ppq & 0xFF));
    // MTrk
    data.push_back('M'); data.push_back('T'); data.push_back('r'); data.push_back('k');

    // Track events:
    // delta=0, note on ch0 C4 vel=80
    // delta=ppq, note off ch0 C4 vel=0
    // delta=0, end of track
    std::vector<uint8_t> track;
    track.push_back(0x00);  // delta
    track.push_back(0x90); track.push_back(60); track.push_back(80);  // note on
    // VLQ encode ppq for delta
    if (ppq < 128) {
        track.push_back(static_cast<uint8_t>(ppq));
    } else {
        track.push_back(static_cast<uint8_t>((ppq >> 7) | 0x80));
        track.push_back(static_cast<uint8_t>(ppq & 0x7F));
    }
    track.push_back(0x80); track.push_back(60); track.push_back(0);   // note off
    track.push_back(0x00);  // delta
    track.push_back(0xFF); track.push_back(0x2F); track.push_back(0x00);  // end

    // Track length
    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());
    return data;
}

}  // anonymous namespace

// =============================================================================
// Parse tests
// =============================================================================

TEST_CASE("FMMI001A: parse minimal valid SMF", "[midi][format]") {
    auto data = make_empty_smf();
    auto r = parse_midi(data);
    REQUIRE(r.has_value());
    REQUIRE(r->format == 0);
    REQUIRE(r->ppq == 480);
    REQUIRE(r->notes.empty());
}

TEST_CASE("FMMI001A: parse single note", "[midi][format]") {
    auto data = make_single_note_smf(480);
    auto r = parse_midi(data);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 1);
    REQUIRE(r->notes[0].note == 60);
    REQUIRE(r->notes[0].velocity == 80);
    REQUIRE(r->notes[0].tick == 0);
    REQUIRE(r->notes[0].duration_ticks == 480);
    REQUIRE(r->notes[0].channel == 0);
}

TEST_CASE("FMMI001A: parse tempo event", "[midi][format]") {
    // Build SMF with tempo meta event: 120 BPM = 500000 us/beat
    std::vector<uint8_t> data;
    data.push_back('M'); data.push_back('T'); data.push_back('h'); data.push_back('d');
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(6);
    data.push_back(0); data.push_back(0);
    data.push_back(0); data.push_back(1);
    data.push_back(0x01); data.push_back(0xE0);  // ppq=480

    data.push_back('M'); data.push_back('T'); data.push_back('r'); data.push_back('k');

    std::vector<uint8_t> track;
    // Tempo: 500000 = 0x07A120
    track.push_back(0x00);  // delta
    track.push_back(0xFF); track.push_back(0x51); track.push_back(0x03);
    track.push_back(0x07); track.push_back(0xA1); track.push_back(0x20);
    // End of track
    track.push_back(0x00);
    track.push_back(0xFF); track.push_back(0x2F); track.push_back(0x00);

    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());

    auto r = parse_midi(data);
    REQUIRE(r.has_value());
    REQUIRE(r->tempos.size() == 1);
    REQUIRE(r->tempos[0].microseconds_per_beat == 500000);
}

TEST_CASE("FMMI001A: parse time signature", "[midi][format]") {
    std::vector<uint8_t> data;
    data.push_back('M'); data.push_back('T'); data.push_back('h'); data.push_back('d');
    data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(6);
    data.push_back(0); data.push_back(0);
    data.push_back(0); data.push_back(1);
    data.push_back(0x01); data.push_back(0xE0);

    data.push_back('M'); data.push_back('T'); data.push_back('r'); data.push_back('k');

    std::vector<uint8_t> track;
    // Time sig: 3/4 => nn=3, dd=2 (2^2=4), cc=24, bb=8
    track.push_back(0x00);
    track.push_back(0xFF); track.push_back(0x58); track.push_back(0x04);
    track.push_back(3); track.push_back(2); track.push_back(24); track.push_back(8);
    track.push_back(0x00);
    track.push_back(0xFF); track.push_back(0x2F); track.push_back(0x00);

    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());

    auto r = parse_midi(data);
    REQUIRE(r.has_value());
    REQUIRE(r->time_signatures.size() == 1);
    REQUIRE(r->time_signatures[0].numerator == 3);
    REQUIRE(r->time_signatures[0].denominator == 4);
}

// =============================================================================
// Write → Parse round-trip
// =============================================================================

TEST_CASE("FMMI001A: write-parse round-trip single note", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.notes.push_back({0, 480, 0, 60, 80});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->ppq == 480);
    REQUIRE(parsed->notes.size() == 1);
    REQUIRE(parsed->notes[0].note == 60);
    REQUIRE(parsed->notes[0].velocity == 80);
    REQUIRE(parsed->notes[0].tick == 0);
    REQUIRE(parsed->notes[0].duration_ticks == 480);
}

TEST_CASE("FMMI001A: write-parse round-trip multiple notes", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.notes.push_back({0, 480, 0, 60, 80});
    original.notes.push_back({480, 480, 0, 64, 90});
    original.notes.push_back({960, 480, 0, 67, 100});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.size() == 3);
    REQUIRE(parsed->notes[0].note == 60);
    REQUIRE(parsed->notes[1].note == 64);
    REQUIRE(parsed->notes[2].note == 67);
}

TEST_CASE("FMMI001A: write-parse round-trip with tempo", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.tempos.push_back({0, 500000});   // 120 BPM
    original.notes.push_back({0, 480, 0, 60, 80});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->tempos.size() == 1);
    REQUIRE(parsed->tempos[0].microseconds_per_beat == 500000);
}

// =============================================================================
// Conversion functions
// =============================================================================

TEST_CASE("FMMI001A: midi_to_note_events", "[midi][format]") {
    MidiFile file;
    file.ppq = 480;
    file.notes.push_back({0, 480, 0, 60, 80});
    file.notes.push_back({480, 240, 0, 64, 90});

    auto events = midi_to_note_events(file);
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].pitch == 60);
    REQUIRE(events[0].start_time == Beat{0, 1});
    REQUIRE(events[0].duration == Beat{1, 1});  // 480/480 = 1
    REQUIRE(events[1].start_time == Beat{1, 1});
    REQUIRE(events[1].duration == Beat{1, 2});   // 240/480 = 1/2
}

TEST_CASE("FMMI001A: note_events_to_midi", "[midi][format]") {
    std::array<NoteEvent, 2> events = {
        NoteEvent{60, Beat{0, 1}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 90},
    };

    auto file = note_events_to_midi(events, 480);
    REQUIRE(file.ppq == 480);
    REQUIRE(file.notes.size() == 2);
    REQUIRE(file.notes[0].tick == 0);
    REQUIRE(file.notes[0].duration_ticks == 120);  // 1/4 * 480 = 120
    REQUIRE(file.notes[1].tick == 120);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("FMMI001A: invalid header magic", "[midi][format]") {
    std::vector<uint8_t> data = {'B', 'A', 'D', 'D', 0, 0, 0, 6, 0, 0, 0, 1, 0, 0};
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("FMMI001A: truncated data", "[midi][format]") {
    std::vector<uint8_t> data = {'M', 'T', 'h', 'd'};
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("FMMI001A: invalid track magic", "[midi][format]") {
    auto data = make_empty_smf();
    // Corrupt the MTrk magic
    data[14] = 'X';
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("FMMI001A: empty note list produces valid SMF", "[midi][format]") {
    MidiFile file;
    file.format = 0;
    file.ppq = 480;

    auto bytes = write_midi(file);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.empty());
}

TEST_CASE("FMMI001A: channel preservation", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.notes.push_back({0, 480, 5, 60, 80});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes[0].channel == 5);
}

TEST_CASE("FMMI001A: velocity preservation", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.notes.push_back({0, 480, 0, 60, 127});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes[0].velocity == 127);
}

TEST_CASE("FMMI001A: PPQ 480 default resolution", "[midi][format]") {
    auto events = std::vector<NoteEvent>{
        {60, Beat{0, 1}, Beat{1, 4}, 80}
    };
    auto file = note_events_to_midi(events);
    REQUIRE(file.ppq == 480);
}
