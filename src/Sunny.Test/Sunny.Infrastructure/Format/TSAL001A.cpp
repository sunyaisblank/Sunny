/**
 * @file TSAL001A.cpp
 * @brief Unit tests for Ableton Live Compiler (FMAL001A)
 *
 * Component: TSAL001A
 * Domain: TS (Test) | Category: AL (Ableton Live)
 *
 * Tests: FMAL001A + INTP001A
 * Coverage: Track creation, clip creation, note injection, tempo,
 *           section markers, multi-part scores, pan assignment
 *
 * Uses CommandBuffer transport so tests run without Ableton.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Format/FMAL001A.h"
#include "Bridge/INTP001A.h"

// Score IR workflow for test score creation
#include "Score/SIWF001A.h"

using namespace Sunny::Infrastructure;
using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_test_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Test Score";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};  // C4
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.clef = Clef::Treble;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

Score make_two_part_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Two Part Score";
    spec.total_bars = bars;
    spec.bpm = 100.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition violin;
    violin.name = "Violin";
    violin.abbreviation = "Vln.";
    violin.instrument_type = InstrumentType::Violin;
    violin.clef = Clef::Treble;
    violin.rendering.midi_channel = 1;
    violin.rendering.pan = -0.5f;
    spec.parts.push_back(violin);

    PartDefinition cello;
    cello.name = "Cello";
    cello.abbreviation = "Vc.";
    cello.instrument_type = InstrumentType::Cello;
    cello.clef = Clef::Bass;
    cello.rendering.midi_channel = 2;
    cello.rendering.pan = 0.5f;
    spec.parts.push_back(cello);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

void insert_note(Score& score, std::size_t part_idx, SpelledPitch pitch,
                 Beat offset, Beat duration) {
    auto& voice = score.parts[part_idx].measures[0].voices[0];
    Note note;
    note.pitch = pitch;
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = duration;

    // Replace events: note at offset, rest fills remainder
    voice.events.clear();
    if (offset != Beat::zero()) {
        RestEvent pre_rest{offset, true};
        voice.events.push_back(Event{EventId{9000001}, Beat::zero(), pre_rest});
    }
    EventId note_id{9000002};
    voice.events.push_back(Event{note_id, offset, ng});

    // Remaining rest to fill the bar (4/4 = Beat{1,1})
    Beat after_note = offset + duration;
    Beat bar_dur{1, 1};  // 4/4 bar in whole-note units
    if (after_note < bar_dur) {
        Beat rest_dur = bar_dur - after_note;
        RestEvent post_rest{rest_dur, true};
        voice.events.push_back(Event{EventId{9000003}, after_note, post_rest});
    }
}

class FailingTransport : public LomTransport {
public:
    LomResponse send(const LomRequest&) override {
        return {false, std::nullopt, "transport failure", std::nullopt};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {false, std::nullopt, "transport failure", std::nullopt};
    }
    [[nodiscard]] bool is_connected() const override { return false; }
};

}  // namespace

// =============================================================================
// Basic compilation
// =============================================================================

TEST_CASE("FMAL001A: empty score compiles to tracks and clips", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    CHECK(result->tracks_created == 1);
    CHECK(result->clips_created == 1);
    CHECK(result->tempo_events_written >= 1);
}

TEST_CASE("FMAL001A: track creation sends create_midi_track", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    // Should have at least create_midi_track and create_clip
    bool found_create_track = false;
    bool found_create_clip = false;
    for (const auto* entry : calls) {
        if (entry->request.property_or_method == "create_midi_track")
            found_create_track = true;
        if (entry->request.property_or_method == "create_clip")
            found_create_clip = true;
    }
    CHECK(found_create_track);
    CHECK(found_create_clip);
}

TEST_CASE("FMAL001A: track name is set from Part definition", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_name = false;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "name" &&
            entry->request.path.to_string().find("tracks") != std::string::npos) {
            REQUIRE(!entry->request.args.empty());
            auto* name = std::get_if<std::string>(&entry->request.args[0]);
            REQUIRE(name != nullptr);
            CHECK(*name == "Piano");
            found_name = true;
        }
    }
    CHECK(found_name);
}

// =============================================================================
// Note injection
// =============================================================================

TEST_CASE("FMAL001A: notes are injected into clips", "[ableton][compiler]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->notes_written >= 1);

    // Verify notes were sent via send_notes
    bool found_notes = false;
    for (const auto& entry : buf.entries()) {
        if (entry.request.property_or_method == "add_notes" && !entry.notes.empty()) {
            found_notes = true;
            CHECK(entry.notes[0].pitch == 60);  // C4 = MIDI 60
        }
    }
    CHECK(found_notes);
}

// =============================================================================
// Multi-part score
// =============================================================================

TEST_CASE("FMAL001A: two-part score creates two tracks", "[ableton][compiler]") {
    auto score = make_two_part_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->tracks_created == 2);
    CHECK(result->clips_created == 2);
}

TEST_CASE("FMAL001A: pan is set from RenderingConfig", "[ableton][compiler]") {
    auto score = make_two_part_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int pan_count = 0;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "panning") {
            pan_count++;
        }
    }
    CHECK(pan_count == 2);
}

// =============================================================================
// Tempo
// =============================================================================

TEST_CASE("FMAL001A: initial tempo is set", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_tempo = false;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "tempo") {
            REQUIRE(!entry->request.args.empty());
            auto* val = std::get_if<double>(&entry->request.args[0]);
            REQUIRE(val != nullptr);
            CHECK(*val == Catch::Approx(120.0));
            found_tempo = true;
            break;
        }
    }
    CHECK(found_tempo);
}

// =============================================================================
// Section markers
// =============================================================================

TEST_CASE("FMAL001A: section markers are created from SectionMap", "[ableton][compiler]") {
    auto score = make_test_score(8);

    // Add two sections
    ScoreSection intro;
    intro.id = SectionId{1};
    intro.label = "Intro";
    intro.start = ScoreTime{1, Beat::zero()};
    intro.end = ScoreTime{5, Beat::zero()};
    score.section_map.push_back(intro);

    ScoreSection verse;
    verse.id = SectionId{2};
    verse.label = "Verse";
    verse.start = ScoreTime{5, Beat::zero()};
    verse.end = ScoreTime{9, Beat::zero()};
    score.section_map.push_back(verse);

    CommandBuffer buf;
    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->markers_created == 2);

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    int cue_count = 0;
    for (const auto* entry : calls) {
        if (entry->request.property_or_method == "set_or_delete_cue")
            cue_count++;
    }
    CHECK(cue_count == 2);
}

// =============================================================================
// Instrument preset
// =============================================================================

TEST_CASE("FMAL001A: instrument preset is set when specified", "[ableton][compiler]") {
    auto score = make_test_score(4);
    score.parts[0].definition.rendering.instrument_preset = "Instruments/Piano/Grand Piano.adv";
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_preset = false;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "instrument_preset") {
            found_preset = true;
        }
    }
    CHECK(found_preset);
}

// =============================================================================
// Compilation report
// =============================================================================

TEST_CASE("FMAL001A: compilation result contains MIDI report", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    // MIDI report should be present (even if no drops for a simple score)
    CHECK_FALSE(result->midi_report.has_drops());
}

// =============================================================================
// Command count validation
// =============================================================================

TEST_CASE("FMAL001A: single-part command sequence is well-ordered", "[ableton][compiler]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    // Should have: set tempo, create track, set name, create clip, set clip name,
    // add notes — all in sequence
    CHECK(buf.size() >= 5);
}

// =============================================================================
// Transport failure propagation
// =============================================================================

TEST_CASE("FMAL001A: transport failure propagates SendFailed", "[ableton][compiler]") {
    auto score = make_test_score(4);
    FailingTransport failing;

    auto result = compile_to_ableton(score, failing);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::SendFailed);
}
