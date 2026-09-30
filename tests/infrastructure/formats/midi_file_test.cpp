/**
 * @file midi_file_test.cpp
 * @brief Standard MIDI File reader/writer unit tests
 *
 *
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/formats/midi_file.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// Helpers: build minimal SMF data
// =============================================================================

namespace {

/// Build a minimal valid SMF Type 0 with an empty track
std::vector<uint8_t> make_empty_smf(uint16_t ppq = 480) {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    // Header length = 6
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    // Format 0
    data.push_back(0);
    data.push_back(0);
    // 1 track
    data.push_back(0);
    data.push_back(1);
    // PPQ
    data.push_back(static_cast<uint8_t>(ppq >> 8));
    data.push_back(static_cast<uint8_t>(ppq & 0xFF));
    // MTrk
    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');
    // Track length = 4 (just end-of-track)
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(4);
    // Delta=0, End of track
    data.push_back(0x00);
    data.push_back(0xFF);
    data.push_back(0x2F);
    data.push_back(0x00);
    return data;
}

/// Build SMF with a single note (C4, quarter note at tick 0)
std::vector<uint8_t> make_single_note_smf(uint16_t ppq = 480) {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0); // format 0
    data.push_back(0);
    data.push_back(1); // 1 track
    data.push_back(static_cast<uint8_t>(ppq >> 8));
    data.push_back(static_cast<uint8_t>(ppq & 0xFF));
    // MTrk
    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');

    // Track events:
    // delta=0, note on ch0 C4 vel=80
    // delta=ppq, note off ch0 C4 vel=0
    // delta=0, end of track
    std::vector<uint8_t> track;
    track.push_back(0x00); // delta
    track.push_back(0x90);
    track.push_back(60);
    track.push_back(80); // note on
    // VLQ encode ppq for delta
    if (ppq < 128) {
        track.push_back(static_cast<uint8_t>(ppq));
    } else {
        track.push_back(static_cast<uint8_t>((ppq >> 7) | 0x80));
        track.push_back(static_cast<uint8_t>(ppq & 0x7F));
    }
    track.push_back(0x80);
    track.push_back(60);
    track.push_back(0);    // note off
    track.push_back(0x00); // delta
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00); // end

    // Track length
    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());
    return data;
}

} // anonymous namespace

// =============================================================================
// Parse tests
// =============================================================================

TEST_CASE("parse minimal valid SMF", "[midi][format]") {
    auto data = make_empty_smf();
    auto r = parse_midi(data);
    REQUIRE(r.has_value());
    REQUIRE(r->format == 0);
    REQUIRE(r->ppq == 480);
    REQUIRE(r->notes.empty());
}

TEST_CASE("parse single note", "[midi][format]") {
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

TEST_CASE("parse Type 1 retains declared track ownership including empty tracks",
          "[midi][format][trust-boundary]") {
    auto data = make_single_note_smf(120);
    data[9] = 1;  // format 1
    data[11] = 2; // two tracks

    auto empty_track = make_empty_smf(120);
    data.insert(data.end(), empty_track.begin() + 14, empty_track.end());

    auto parsed = parse_midi(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->format == 1);
    CHECK(parsed->track_count == 2);
    REQUIRE(parsed->track_end_ticks.size() == 2);
    REQUIRE(parsed->notes.size() == 1);
    CHECK(parsed->notes[0].track == 0);
}

TEST_CASE("SMF End of Track tick preserves trailing silence", "[midi][format][trust-boundary]") {
    auto data = make_empty_smf(120);
    data[22] = 120; // EOT occurs one quarter note after the last retained event

    const auto parsed = parse_midi(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->track_end_ticks.size() == 1);
    CHECK(parsed->track_end_ticks[0] == 120);

    const auto written = write_midi(*parsed);
    REQUIRE(written.has_value());
    const auto reparsed = parse_midi(*written);
    REQUIRE(reparsed.has_value());
    CHECK(reparsed->track_end_ticks == parsed->track_end_ticks);
}

TEST_CASE("parse tempo event", "[midi][format]") {
    // Build SMF with tempo meta event: 120 BPM = 500000 us/beat
    std::vector<uint8_t> data;
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(1);
    data.push_back(0x01);
    data.push_back(0xE0); // ppq=480

    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');

    std::vector<uint8_t> track;
    // Tempo: 500000 = 0x07A120
    track.push_back(0x00); // delta
    track.push_back(0xFF);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(0x07);
    track.push_back(0xA1);
    track.push_back(0x20);
    // End of track
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);

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

TEST_CASE("MIDI parser reads time signatures", "[midi][format]") {
    std::vector<uint8_t> data;
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(1);
    data.push_back(0x01);
    data.push_back(0xE0);

    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');

    std::vector<uint8_t> track;
    // Time sig: 3/4 => nn=3, dd=2 (2^2=4), with non-default cc/bb bytes.
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x58);
    track.push_back(0x04);
    track.push_back(3);
    track.push_back(2);
    track.push_back(36);
    track.push_back(7);
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);

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
    REQUIRE(r->time_signatures[0].clocks_per_metronome_click == 36);
    REQUIRE(r->time_signatures[0].notated_32nds_per_quarter == 7);

    auto encoded = write_midi(*r);
    REQUIRE(encoded.has_value());
    auto reparsed = parse_midi(*encoded);
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed->time_signatures[0].clocks_per_metronome_click == 36);
    REQUIRE(reparsed->time_signatures[0].notated_32nds_per_quarter == 7);
}

// =============================================================================
// Write → Parse round-trip
// =============================================================================

TEST_CASE("MIDI single-note write-parse round trip", "[midi][format]") {
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

TEST_CASE("MIDI writer requires FIFO-representable same-key overlaps", "[midi][format]") {
    MidiFile file;
    file.notes.push_back({0, 480, 0, 60, 80, false, 0, 11});
    file.notes.push_back({120, 120, 0, 60, 90, false, 0, 22});

    auto nested = write_midi(file);
    REQUIRE_FALSE(nested.has_value());
    REQUIRE(nested.error() == sunny::core::ErrorCode::FormatError);

    file.notes[0].duration_ticks = 240;
    file.notes[1].duration_ticks = 240;
    auto fifo = write_midi(file);
    REQUIRE(fifo.has_value());
    auto reparsed = parse_midi(*fifo);
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed->notes.size() == 2);
    CHECK(reparsed->notes[0].tick == 0);
    CHECK(reparsed->notes[0].duration_ticks == 240);
    CHECK(reparsed->notes[0].release_velocity == 11);
    CHECK(reparsed->notes[1].tick == 120);
    CHECK(reparsed->notes[1].duration_ticks == 240);
    CHECK(reparsed->notes[1].release_velocity == 22);
}

TEST_CASE("MIDI writer preserves explicit source event order", "[midi][format]") {
    MidiFile file;
    file.notes.push_back({0, 120, 0, 60, 80, false, 0, 9, 1, 3});
    file.program_changes.push_back({0, 0, 10, 0, 2});

    auto encoded = write_midi(file);
    REQUIRE(encoded.has_value());
    auto parsed = parse_midi(*encoded);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.size() == 1);
    REQUIRE(parsed->program_changes.size() == 1);
    CHECK(parsed->notes[0].onset_order == 1);
    CHECK(parsed->program_changes[0].order == 2);
    CHECK(parsed->notes[0].release_order == 3);

    auto rewritten = write_midi(*parsed);
    REQUIRE(rewritten.has_value());
    CHECK(*rewritten == *encoded);

    parsed->program_changes[0].order = 0;
    auto mixed = write_midi(*parsed);
    REQUIRE_FALSE(mixed.has_value());
    CHECK(mixed.error() == sunny::core::ErrorCode::FormatError);
}

TEST_CASE("write-parse round-trip multiple notes", "[midi][format]") {
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

TEST_CASE("write-parse round-trip with tempo", "[midi][format]") {
    MidiFile original;
    original.format = 0;
    original.ppq = 480;
    original.tempos.push_back({0, 500000}); // 120 BPM
    original.notes.push_back({0, 480, 0, 60, 80});

    auto bytes = write_midi(original);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->tempos.size() == 1);
    REQUIRE(parsed->tempos[0].microseconds_per_beat == 500000);
}

TEST_CASE("MIDI channel and key control events survive write-parse round trip",
          "[midi][format][articulation]") {
    MidiFile original;
    original.key_signatures.push_back({0, -3, true});
    original.program_changes.push_back({12, 2, 41});
    original.control_changes.push_back({12, 2, 64, 127});
    original.notes.push_back({12, 120, 2, 60, 91});

    auto bytes = write_midi(original);
    REQUIRE(bytes);
    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed);

    REQUIRE(parsed->key_signatures.size() == 1);
    CHECK(parsed->key_signatures[0].accidentals == -3);
    CHECK(parsed->key_signatures[0].minor);
    REQUIRE(parsed->program_changes.size() == 1);
    CHECK(parsed->program_changes[0].tick == 12);
    CHECK(parsed->program_changes[0].channel == 2);
    CHECK(parsed->program_changes[0].program == 41);
    REQUIRE(parsed->control_changes.size() == 1);
    CHECK(parsed->control_changes[0].tick == 12);
    CHECK(parsed->control_changes[0].channel == 2);
    CHECK(parsed->control_changes[0].controller == 64);
    CHECK(parsed->control_changes[0].value == 127);
}

TEST_CASE("compiled MIDI conversion makes channel basis and keyswitch ordering explicit",
          "[midi][format][articulation]") {
    CompiledMidi compiled;
    compiled.ppq = 480;
    compiled.keyswitches.push_back({0, 1, 2, 24, 127, PartId{7}});
    compiled.program_changes.push_back({0, 2, 41, PartId{7}});
    compiled.control_changes.push_back({0, 2, 64, 127, PartId{7}});
    compiled.notes.push_back({0, 480, 2, 60, 80, PartId{7}});

    auto file = compiled_midi_to_file(compiled);
    REQUIRE(file);
    REQUIRE(file->notes.size() == 2);
    CHECK(file->notes[0].channel == 1);
    CHECK(file->notes[0].keyswitch);
    CHECK_FALSE(file->notes[1].keyswitch);
    CHECK(file->control_changes[0].channel == 1);
    CHECK(file->program_changes[0].channel == 1);

    auto bytes = write_midi(*file);
    REQUIRE(bytes);
    const std::vector<std::uint8_t> program{0xC1, 41};
    const std::vector<std::uint8_t> cc{0xB1, 64, 127};
    const std::vector<std::uint8_t> keyswitch{0x91, 24, 127};
    const std::vector<std::uint8_t> musical{0x91, 60, 80};
    const auto program_at =
        std::search(bytes->begin(), bytes->end(), program.begin(), program.end());
    const auto cc_at = std::search(bytes->begin(), bytes->end(), cc.begin(), cc.end());
    const auto keyswitch_at =
        std::search(bytes->begin(), bytes->end(), keyswitch.begin(), keyswitch.end());
    const auto musical_at =
        std::search(bytes->begin(), bytes->end(), musical.begin(), musical.end());
    REQUIRE(program_at != bytes->end());
    REQUIRE(cc_at != bytes->end());
    REQUIRE(keyswitch_at != bytes->end());
    REQUIRE(musical_at != bytes->end());
    CHECK(program_at < cc_at);
    CHECK(cc_at < keyswitch_at);
    CHECK(keyswitch_at < musical_at);
}

TEST_CASE("compiled MIDI conversion rejects invalid human-facing channels",
          "[midi][format][trust-boundary]") {
    CompiledMidi compiled;
    compiled.notes.push_back({0, 480, 0, 60, 80, PartId{1}});
    auto result = compiled_midi_to_file(compiled);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::InvalidRenderingConfig);
}

TEST_CASE("compiled MIDI conversion preserves the complete SMF time-signature payload",
          "[midi][format][meter]") {
    CompiledMidi compiled;
    compiled.ppq = 480;
    compiled.time_signatures.push_back({0, 6, 8, 36, 8});

    const auto file = compiled_midi_to_file(compiled);
    REQUIRE(file.has_value());
    REQUIRE(file->time_signatures.size() == 1);
    CHECK(file->time_signatures[0].clocks_per_metronome_click == 36);
    CHECK(file->time_signatures[0].notated_32nds_per_quarter == 8);

    const auto bytes = write_midi(*file);
    REQUIRE(bytes.has_value());
    const auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->time_signatures.size() == 1);
    CHECK(parsed->time_signatures[0].numerator == 6);
    CHECK(parsed->time_signatures[0].denominator == 8);
    CHECK(parsed->time_signatures[0].clocks_per_metronome_click == 36);
    CHECK(parsed->time_signatures[0].notated_32nds_per_quarter == 8);
}

// =============================================================================
// Conversion functions
// =============================================================================

TEST_CASE("midi_to_note_events", "[midi][format]") {
    MidiFile file;
    file.ppq = 480;
    file.notes.push_back({0, 480, 0, 60, 80, false, 0, 23});
    file.notes.push_back({480, 240, 0, 64, 90});

    auto events = midi_to_note_events(file);
    REQUIRE(events);
    REQUIRE(events->size() == 2);
    REQUIRE((*events)[0].pitch == 60);
    REQUIRE((*events)[0].start_time == Beat{0, 1});
    REQUIRE((*events)[0].duration == Beat{1, 4});
    CHECK((*events)[0].release_velocity == 23);
    REQUIRE((*events)[1].start_time == Beat{1, 4});
    REQUIRE((*events)[1].duration == Beat{1, 8});
}

TEST_CASE("note_events_to_midi", "[midi][format]") {
    std::array<NoteEvent, 2> events = {
        NoteEvent{60, Beat{0, 1}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 90},
    };

    auto file = note_events_to_midi(events, 480);
    REQUIRE(file);
    REQUIRE(file->ppq == 480);
    REQUIRE(file->notes.size() == 2);
    REQUIRE(file->notes[0].tick == 0);
    REQUIRE(file->notes[0].duration_ticks == 480);
    REQUIRE(file->notes[1].tick == 480);
    CHECK(file->notes[0].release_velocity == 64);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("invalid header magic", "[midi][format]") {
    std::vector<uint8_t> data = {'B', 'A', 'D', 'D', 0, 0, 0, 6, 0, 0, 0, 1, 0, 0};
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("truncated data", "[midi][format]") {
    std::vector<uint8_t> data = {'M', 'T', 'h', 'd'};
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("unterminated note-on is rejected instead of becoming a zero-duration note",
          "[midi][format][trust-boundary]") {
    auto data = make_empty_smf();
    data.resize(22);
    data[21] = 8; // track length
    const std::array<std::uint8_t, 8> track{0x00, 0x90, 60, 80, 0x00, 0xFF, 0x2F, 0x00};
    data.insert(data.end(), track.begin(), track.end());

    auto result = parse_midi(data);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("SMF parser requires an exact header and terminal End of Track",
          "[midi][format][trust-boundary]") {
    SECTION("header length must be the standard six bytes") {
        auto data = make_empty_smf();
        data[7] = 7;
        CHECK_FALSE(parse_midi(data));
    }

    SECTION("End of Track is required") {
        auto data = make_empty_smf();
        data.resize(data.size() - 4);
        data[21] = 0;
        CHECK_FALSE(parse_midi(data));
    }

    SECTION("End of Track must be the final event in its chunk") {
        auto data = make_empty_smf();
        data[21] = 8;
        data.insert(data.end(), {0x00, 0x90, 60, 80});
        CHECK_FALSE(parse_midi(data));
    }

    SECTION("bytes after the declared tracks are rejected") {
        auto data = make_empty_smf();
        data.push_back(0);
        CHECK_FALSE(parse_midi(data));
    }
}

TEST_CASE("SMF parser rejects unmatched and zero-duration note endings",
          "[midi][format][trust-boundary]") {
    SECTION("unmatched note off") {
        auto data = make_empty_smf();
        data.resize(22);
        data[21] = 8;
        data.insert(data.end(), {0x00, 0x80, 60, 0, 0x00, 0xFF, 0x2F, 0x00});
        CHECK_FALSE(parse_midi(data));
    }

    SECTION("same-tick note on and note off") {
        auto data = make_empty_smf();
        data.resize(22);
        data[21] = 12;
        data.insert(data.end(), {0x00, 0x90, 60, 80, 0x00, 0x80, 60, 0, 0x00, 0xFF, 0x2F, 0x00});
        CHECK_FALSE(parse_midi(data));
    }
}

TEST_CASE("invalid track magic", "[midi][format]") {
    auto data = make_empty_smf();
    // Corrupt the MTrk magic
    data[14] = 'X';
    auto r = parse_midi(data);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMidiFile);
}

TEST_CASE("empty note list produces valid SMF", "[midi][format]") {
    MidiFile file;
    file.format = 0;
    file.ppq = 480;

    auto bytes = write_midi(file);
    REQUIRE(bytes.has_value());

    auto parsed = parse_midi(*bytes);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.empty());
}

TEST_CASE("Type 0 writer rejects nonzero or contradictory track state",
          "[midi][format][trust-boundary]") {
    MidiFile file;
    file.notes.push_back({0, 480, 0, 60, 80});

    file.track_count = 2;
    CHECK_FALSE(write_midi(file));

    file.track_count = 1;
    file.notes[0].track = 1;
    CHECK_FALSE(write_midi(file));

    file.notes[0].track = 0;
    file.track_end_ticks = {479};
    CHECK_FALSE(write_midi(file));

    file.track_end_ticks = {480, 480};
    CHECK_FALSE(write_midi(file));
}

TEST_CASE("channel preservation", "[midi][format]") {
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

TEST_CASE("velocity preservation", "[midi][format]") {
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

TEST_CASE("SMF parser retains release velocity and reports valid unmodelled events",
          "[midi][format][loss-accounting]") {
    auto data = make_single_note_smf(120);

    // The explicit Note Off is four bytes before the final EOT event.
    data[data.size() - 5] = 73;
    const auto eot = data.end() - 4;
    data.insert(eot, {0x00, 0xE0, 0x00, 0x40});
    data[21] = static_cast<std::uint8_t>(data[21] + 4);

    auto parsed = parse_midi(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.size() == 1);
    CHECK(parsed->notes[0].release_velocity == 73);
    CHECK(parsed->parse_loss.pitch_bend_events == 1);
    CHECK_FALSE(parsed->parse_loss.empty());
    CHECK_FALSE(write_midi(*parsed));

    parsed->parse_loss = {};
    const auto rewritten = write_midi(*parsed);
    REQUIRE(rewritten.has_value());
    const auto reparsed = parse_midi(*rewritten);
    REQUIRE(reparsed.has_value());
    CHECK(reparsed->notes[0].release_velocity == 73);

    // The generic exact NoteEvent projection carries the retained Note Off intensity too.
    const auto projected = midi_to_note_events(*reparsed);
    REQUIRE(projected.has_value());
    REQUIRE(projected->size() == 1);
    CHECK(projected->front().release_velocity == 73);
}

TEST_CASE("PPQ 480 default resolution", "[midi][format]") {
    auto events = std::vector<NoteEvent>{{60, Beat{0, 1}, Beat{1, 4}, 80}};
    auto file = note_events_to_midi(events);
    REQUIRE(file);
    REQUIRE(file->ppq == 480);
}

TEST_CASE("NoteEvent MIDI conversion rejects invalid timing and tempo", "[midi][format]") {
    const std::vector<NoteEvent> valid{{60, Beat{0, 1}, Beat{1, 4}, 80}};
    REQUIRE_FALSE(note_events_to_midi(valid, 0));
    const auto smpte_ppq = note_events_to_midi(valid, 0x8000);
    REQUIRE_FALSE(smpte_ppq);
    CHECK(smpte_ppq.error() == ErrorCode::InvalidMidiPPQ);
    REQUIRE_FALSE(note_events_to_midi(valid, 480, std::numeric_limits<double>::quiet_NaN()));

    const std::vector<NoteEvent> invalid_duration{{60, Beat{0, 1}, Beat{0, 1}, 80}};
    REQUIRE_FALSE(note_events_to_midi(invalid_duration));

    const std::vector<NoteEvent> muted{{60, Beat{0, 1}, Beat{1, 4}, 80, true}};
    const auto muted_result = note_events_to_midi(muted);
    REQUIRE_FALSE(muted_result);
    CHECK(muted_result.error() == ErrorCode::FormatError);

    MidiFile invalid_file;
    invalid_file.ppq = 0;
    REQUIRE_FALSE(midi_to_note_events(invalid_file));

    invalid_file.ppq = 480;
    invalid_file.notes.push_back({0, 480, 0, 60, 0});
    const auto invalid_velocity = midi_to_note_events(invalid_file);
    REQUIRE_FALSE(invalid_velocity);
    REQUIRE(invalid_velocity.error() == ErrorCode::InvalidVelocity);

    invalid_file.notes.clear();
    invalid_file.ppq = 0x8000;
    REQUIRE_FALSE(midi_to_note_events(invalid_file));

    invalid_file.ppq = 480;
    invalid_file.format = 2;
    REQUIRE_FALSE(midi_to_note_events(invalid_file));

    invalid_file.format = 0;
    invalid_file.notes.push_back({0, 480, 16, 60, 80});
    REQUIRE_FALSE(midi_to_note_events(invalid_file));

    invalid_file.notes.front().channel = 0;
    invalid_file.notes.front().keyswitch = true;
    REQUIRE_FALSE(midi_to_note_events(invalid_file));

    invalid_file.notes.front().keyswitch = false;
    invalid_file.track_end_ticks = {479};
    REQUIRE_FALSE(midi_to_note_events(invalid_file));
}

// =============================================================================
// Trust boundary validation (PS-5)
// =============================================================================

TEST_CASE("PPQ zero returns InvalidMidiPPQ", "[midi][format]") {
    auto data = make_empty_smf(0);
    auto result = parse_midi(data);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidMidiPPQ);
}

TEST_CASE("tempo zero returns InvalidMidiTempo", "[midi][format]") {
    // Build SMF with a tempo meta event where uspb = 0
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0); // format 0
    data.push_back(0);
    data.push_back(1); // 1 track
    data.push_back(0);
    data.push_back(0x78); // ppq = 120
    // MTrk
    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');
    // Track events: delta=0, tempo meta (0x00 0x00 0x00 = 0 uspb), then end
    std::vector<uint8_t> track;
    track.push_back(0x00); // delta
    track.push_back(0xFF);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(0x00);
    track.push_back(0x00);
    track.push_back(0x00); // uspb = 0
    track.push_back(0x00); // delta
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00); // end
    // Track length
    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());

    auto result = parse_midi(data);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidMidiTempo);
}

TEST_CASE("time sig denominator exponent 8 returns InvalidMidiTimeSig", "[midi][format]") {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(1);
    data.push_back(0);
    data.push_back(0x78); // ppq = 120
    // MTrk
    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');
    std::vector<uint8_t> track;
    track.push_back(0x00); // delta
    track.push_back(0xFF);
    track.push_back(0x58);
    track.push_back(0x04);
    track.push_back(4);  // numerator = 4
    track.push_back(8);  // denominator exponent = 8 (overflow)
    track.push_back(24); // clocks per click
    track.push_back(8);  // 32nd notes per quarter
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);
    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());

    auto result = parse_midi(data);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidMidiTimeSig);
}

TEST_CASE("time sig numerator zero returns InvalidMidiTimeSig", "[midi][format]") {
    std::vector<uint8_t> data;
    // MThd
    data.push_back('M');
    data.push_back('T');
    data.push_back('h');
    data.push_back('d');
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(6);
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);
    data.push_back(1);
    data.push_back(0);
    data.push_back(0x78); // ppq = 120
    // MTrk
    data.push_back('M');
    data.push_back('T');
    data.push_back('r');
    data.push_back('k');
    std::vector<uint8_t> track;
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x58);
    track.push_back(0x04);
    track.push_back(0); // numerator = 0 (invalid)
    track.push_back(2); // denominator exponent = 2 (den = 4)
    track.push_back(24);
    track.push_back(8);
    track.push_back(0x00);
    track.push_back(0xFF);
    track.push_back(0x2F);
    track.push_back(0x00);
    uint32_t tlen = static_cast<uint32_t>(track.size());
    data.push_back(static_cast<uint8_t>((tlen >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((tlen >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(tlen & 0xFF));
    data.insert(data.end(), track.begin(), track.end());

    auto result = parse_midi(data);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidMidiTimeSig);
}

// =============================================================================
// Regression: compiled scores are writable SMF (issue #9)
// =============================================================================

namespace {

/// One sounding note recovered from raw SMF bytes.
struct IndependentNote {
    std::uint32_t start;
    std::uint32_t end;
    std::uint8_t channel;
    std::uint8_t key;
    bool operator==(const IndependentNote&) const = default;
};

/// Minimal SMF 1.0 reader written from the specification, independent of
/// parse_midi: VLQ deltas, running status, meta and SysEx skipping, and FIFO
/// Note Off pairing per (channel, key). A Note On with velocity 0 is a Note Off.
std::vector<IndependentNote> independent_smf_notes(const std::vector<std::uint8_t>& bytes) {
    std::size_t at = 0;
    const auto u32 = [&](std::size_t p) {
        return (std::uint32_t{bytes.at(p)} << 24U) | (std::uint32_t{bytes.at(p + 1)} << 16U) |
               (std::uint32_t{bytes.at(p + 2)} << 8U) | std::uint32_t{bytes.at(p + 3)};
    };
    const auto vlq = [&]() {
        std::uint32_t value = 0;
        for (;;) {
            const auto byte = bytes.at(at++);
            value = (value << 7U) | (byte & 0x7FU);
            if ((byte & 0x80U) == 0) return value;
        }
    };
    REQUIRE(std::string(bytes.begin(), bytes.begin() + 4) == "MThd");
    at = 8 + u32(4);
    std::vector<IndependentNote> notes;
    std::map<std::pair<int, int>, std::deque<std::uint32_t>> open;
    while (at < bytes.size()) {
        REQUIRE(std::string(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                            bytes.begin() + static_cast<std::ptrdiff_t>(at + 4)) == "MTrk");
        const auto end = at + 8 + u32(at + 4);
        at += 8;
        std::uint32_t tick = 0;
        std::uint8_t status = 0;
        while (at < end) {
            tick += vlq();
            if ((bytes.at(at) & 0x80U) != 0) status = bytes.at(at++);
            if (status == 0xFF) {
                ++at; // meta type
                at += vlq();
                continue;
            }
            if (status == 0xF0 || status == 0xF7) {
                at += vlq();
                continue;
            }
            const auto kind = status & 0xF0U;
            const auto channel = static_cast<std::uint8_t>(status & 0x0FU);
            const auto data1 = bytes.at(at++);
            const std::uint8_t data2 = (kind == 0xC0 || kind == 0xD0) ? 0 : bytes.at(at++);
            const auto key = std::pair{int{channel}, int{data1}};
            if (kind == 0x90 && data2 > 0) {
                open[key].push_back(tick);
            } else if (kind == 0x80 || kind == 0x90) {
                REQUIRE_FALSE(open[key].empty());
                notes.push_back({open[key].front(), tick, channel, data1});
                open[key].pop_front();
            }
        }
    }
    for (const auto& entry : open)
        CHECK(entry.second.empty());
    return notes;
}

ScoreSpec one_bar_spec(std::size_t part_count) {
    ScoreSpec spec;
    spec.title = "Regression";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    for (std::size_t index = 0; index < part_count; ++index) {
        PartDefinition part;
        part.name = "P" + std::to_string(index + 1);
        part.instrument_type = InstrumentType::Piano;
        spec.parts.push_back(part);
    }
    return spec;
}

std::vector<IndependentNote> write_and_read_back(const Score& score) {
    auto compiled = compile_to_midi(score, 480);
    REQUIRE(compiled.has_value());
    auto file = compiled_midi_to_file(compiled->midi);
    REQUIRE(file.has_value());
    auto bytes = write_midi(*file);
    REQUIRE(bytes.has_value());
    return independent_smf_notes(*bytes);
}

} // anonymous namespace

TEST_CASE("a two-part nested unison on the default channel writes a valid SMF",
          "[midi][format][regression]") {
    // Both parts keep RenderingConfig's default channel. Part A holds a whole
    // C4; part B plays a quarter C4 at the same onset. The file must write,
    // and an independent reader must recover one C4 sounding for the bar.
    auto score = create_score(one_bar_spec(2));
    REQUIRE(score.has_value());
    REQUIRE(score->parts[0].definition.rendering.midi_channel ==
            score->parts[1].definition.rendering.midi_channel);

    Note c4;
    c4.pitch = SpelledPitch{0, 0, 4};
    REQUIRE(insert_note(*score, score->parts[0].id, 1, 0, Beat::zero(), c4, Beat{1, 1}));
    REQUIRE(insert_note(*score, score->parts[1].id, 1, 0, Beat::zero(), c4, Beat{1, 4}));

    const auto notes = write_and_read_back(*score);
    REQUIRE(notes.size() == 1);
    CHECK(notes[0] == IndependentNote{0, 1920, 0, 60});
}

TEST_CASE("septuplet sixteenths on one key write back-to-back SMF notes",
          "[midi][format][regression]") {
    // Seven C4 notes of 1/28 whole note at PPQ 480 start at round(1920 i / 28)
    // = 0, 69, 137, 206, 274, 343, 411, and each ends where the next begins.
    auto score = create_score(one_bar_spec(1));
    REQUIRE(score.has_value());
    auto& voice = score->parts[0].measures[0].voices[0];
    voice.events.clear();
    Note c4;
    c4.pitch = SpelledPitch{0, 0, 4};
    NoteGroup group;
    group.notes.push_back(c4);
    group.duration = Beat{1, 28};
    for (int i = 0; i < 7; ++i)
        voice.events.push_back(
            Event{EventId{static_cast<std::uint64_t>(900 + i)}, Beat{i, 28}, group});
    voice.events.push_back(Event{EventId{999}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    const std::vector<std::uint32_t> boundaries{0, 69, 137, 206, 274, 343, 411, 480};
    const auto notes = write_and_read_back(*score);
    REQUIRE(notes.size() == 7);
    for (std::size_t i = 0; i < notes.size(); ++i) {
        CAPTURE(i);
        CHECK(notes[i].start == boundaries[i]);
        CHECK(notes[i].end == boundaries[i + 1]);
    }
}
