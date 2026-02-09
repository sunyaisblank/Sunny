/**
 * @file TSMX001A.cpp
 * @brief Unit tests for FMMX001A (MusicXML reader/writer)
 *
 * Component: TSMX001A
 * Domain: TS (Test) | Category: MX (MusicXML)
 *
 * Tests: FMMX001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMMX001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

/// Minimal MusicXML with a single C4 quarter note
const char* SINGLE_NOTE_XML = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list>
    <score-part id="P1"><part-name>Piano</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>1</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
  </part>
</score-partwise>)";

/// MusicXML with a chord (C-E-G)
const char* CHORD_XML = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list>
    <score-part id="P1"><part-name>Piano</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
      </attributes>
      <note>
        <pitch><step>C</step><octave>4</octave></pitch>
        <duration>1</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>E</step><octave>4</octave></pitch>
        <duration>1</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
      <note>
        <chord/>
        <pitch><step>G</step><octave>4</octave></pitch>
        <duration>1</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
  </part>
</score-partwise>)";

/// MusicXML with a rest
const char* REST_XML = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list>
    <score-part id="P1"><part-name>Piano</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
      </attributes>
      <note>
        <rest/>
        <duration>1</duration>
        <voice>1</voice>
        <type>quarter</type>
      </note>
    </measure>
  </part>
</score-partwise>)";

}  // anonymous namespace

// =============================================================================
// Parse tests
// =============================================================================

TEST_CASE("FMMX001A: parse single note C4 quarter", "[musicxml][format]") {
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    REQUIRE(r->parts.size() == 1);
    REQUIRE(r->parts[0].measures.size() == 1);
    REQUIRE(r->parts[0].measures[0].notes.size() == 1);

    const auto& note = r->parts[0].measures[0].notes[0];
    REQUIRE_FALSE(note.is_rest);
    REQUIRE(note.pitch.letter == 0);       // C
    REQUIRE(note.pitch.accidental == 0);
    REQUIRE(note.pitch.octave == 4);
}

TEST_CASE("FMMX001A: parse chord C-E-G", "[musicxml][format]") {
    auto r = parse_musicxml(CHORD_XML);
    REQUIRE(r.has_value());
    auto& notes = r->parts[0].measures[0].notes;
    REQUIRE(notes.size() == 3);
    REQUIRE_FALSE(notes[0].is_chord);
    REQUIRE(notes[1].is_chord);
    REQUIRE(notes[2].is_chord);
    REQUIRE(notes[0].pitch.letter == 0);  // C
    REQUIRE(notes[1].pitch.letter == 2);  // E
    REQUIRE(notes[2].pitch.letter == 4);  // G
}

TEST_CASE("FMMX001A: parse rest", "[musicxml][format]") {
    auto r = parse_musicxml(REST_XML);
    REQUIRE(r.has_value());
    REQUIRE(r->parts[0].measures[0].notes[0].is_rest);
}

TEST_CASE("FMMX001A: parse key signature", "[musicxml][format]") {
    // fifths=0 => C major
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    auto& m = r->parts[0].measures[0];
    REQUIRE(m.key_tonic.has_value());
    REQUIRE(m.key_tonic->letter == 0);       // C
    REQUIRE(m.key_tonic->accidental == 0);
    REQUIRE(m.key_is_major.value_or(true));
}

TEST_CASE("FMMX001A: parse Eb major key", "[musicxml][format]") {
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>-3</fifths><mode>major</mode></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
      </attributes>
    </measure>
  </part>
</score-partwise>)";

    auto r = parse_musicxml(xml);
    REQUIRE(r.has_value());
    auto& m = r->parts[0].measures[0];
    REQUIRE(m.key_tonic.has_value());
    // Eb = letter 2 (E), accidental -1
    REQUIRE(m.key_tonic->letter == 2);
    REQUIRE(m.key_tonic->accidental == -1);
}

TEST_CASE("FMMX001A: parse time signature", "[musicxml][format]") {
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    auto& m = r->parts[0].measures[0];
    REQUIRE(m.time_signature.has_value());
    REQUIRE(m.time_signature->first == 4);
    REQUIRE(m.time_signature->second == 4);
}

// =============================================================================
// Write → Parse round-trip
// =============================================================================

TEST_CASE("FMMX001A: write-parse round-trip single note", "[musicxml][format]") {
    MusicXmlScore original;
    original.title = "Test";
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure m;
    m.number = 1;
    m.key_tonic = SpelledPitch{0, 0, 4};
    m.key_is_major = true;
    m.time_signature = {4, 4};

    MusicXmlNote note;
    note.pitch = SpelledPitch{0, 0, 4};  // C4
    note.duration = Beat{1, 4};
    note.voice = 1;
    m.notes.push_back(note);

    part.measures.push_back(m);
    original.parts.push_back(part);

    auto xml = write_musicxml(original);
    REQUIRE(xml.has_value());

    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts.size() == 1);
    REQUIRE(parsed->parts[0].measures[0].notes.size() == 1);

    auto& n = parsed->parts[0].measures[0].notes[0];
    REQUIRE(n.pitch.letter == 0);
    REQUIRE(n.pitch.accidental == 0);
    REQUIRE(n.pitch.octave == 4);
}

TEST_CASE("FMMX001A: write-parse round-trip chord", "[musicxml][format]") {
    MusicXmlScore original;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure m;
    m.number = 1;
    m.time_signature = {4, 4};
    m.key_tonic = SpelledPitch{0, 0, 4};
    m.key_is_major = true;

    m.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1});  // C4
    m.notes.push_back({SpelledPitch{2, 0, 4}, Beat{1, 4}, false, true, 1});   // E4 chord
    m.notes.push_back({SpelledPitch{4, 0, 4}, Beat{1, 4}, false, true, 1});   // G4 chord

    part.measures.push_back(m);
    original.parts.push_back(part);

    auto xml = write_musicxml(original);
    REQUIRE(xml.has_value());

    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    auto& notes = parsed->parts[0].measures[0].notes;
    REQUIRE(notes.size() == 3);
    REQUIRE_FALSE(notes[0].is_chord);
    REQUIRE(notes[1].is_chord);
    REQUIRE(notes[2].is_chord);
}

TEST_CASE("FMMX001A: write-parse round-trip multi-measure", "[musicxml][format]") {
    MusicXmlScore original;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    // Measure 1: C4 quarter
    MusicXmlMeasure m1;
    m1.number = 1;
    m1.time_signature = {4, 4};
    m1.key_tonic = SpelledPitch{0, 0, 4};
    m1.key_is_major = true;
    m1.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1});
    part.measures.push_back(m1);

    // Measure 2: E4 quarter
    MusicXmlMeasure m2;
    m2.number = 2;
    m2.notes.push_back({SpelledPitch{2, 0, 4}, Beat{1, 4}, false, false, 1});
    part.measures.push_back(m2);

    original.parts.push_back(part);

    auto xml = write_musicxml(original);
    REQUIRE(xml.has_value());

    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts[0].measures.size() == 2);
}

// =============================================================================
// Enharmonic preservation
// =============================================================================

TEST_CASE("FMMX001A: SpelledPitch preservation C# != Db", "[musicxml][format]") {
    MusicXmlScore original;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure m;
    m.number = 1;
    m.time_signature = {4, 4};
    m.key_tonic = SpelledPitch{0, 0, 4};
    m.key_is_major = true;

    // C#4
    m.notes.push_back({SpelledPitch{0, 1, 4}, Beat{1, 4}, false, false, 1});
    part.measures.push_back(m);
    original.parts.push_back(part);

    auto xml = write_musicxml(original);
    REQUIRE(xml.has_value());

    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    auto& n = parsed->parts[0].measures[0].notes[0];
    REQUIRE(n.pitch.letter == 0);       // C (not D)
    REQUIRE(n.pitch.accidental == 1);   // sharp (not flat)
}

// =============================================================================
// Conversion functions
// =============================================================================

TEST_CASE("FMMX001A: musicxml_to_note_events", "[musicxml][format]") {
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    auto events = musicxml_to_note_events(*r);
    REQUIRE(events.size() == 1);
    REQUIRE(events[0].pitch == 60);  // C4
}

TEST_CASE("FMMX001A: note_events_to_musicxml", "[musicxml][format]") {
    std::array<NoteEvent, 2> events = {
        NoteEvent{60, Beat{0, 1}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 80},
    };
    auto score = note_events_to_musicxml(events, 0);
    REQUIRE(score.parts.size() == 1);
    REQUIRE(score.parts[0].measures[0].notes.size() == 2);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("FMMX001A: invalid XML", "[musicxml][format]") {
    auto r = parse_musicxml("not xml at all");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMusicXml);
}

TEST_CASE("FMMX001A: missing score-partwise", "[musicxml][format]") {
    auto r = parse_musicxml("<?xml version=\"1.0\"?><other/>");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMusicXml);
}

TEST_CASE("FMMX001A: empty score produces valid XML", "[musicxml][format]") {
    MusicXmlScore score;
    score.title = "Empty";
    auto xml = write_musicxml(score);
    REQUIRE(xml.has_value());
    // Should parse back
    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts.empty());
}

TEST_CASE("FMMX001A: divisions calculation", "[musicxml][format]") {
    MusicXmlScore score;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure m;
    m.number = 1;
    m.time_signature = {4, 4};
    m.key_tonic = SpelledPitch{0, 0, 4};
    m.key_is_major = true;

    // Quarter note and eighth note in same score
    m.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1});
    m.notes.push_back({SpelledPitch{1, 0, 4}, Beat{1, 8}, false, false, 1});
    part.measures.push_back(m);
    score.parts.push_back(part);

    auto xml = write_musicxml(score);
    REQUIRE(xml.has_value());

    auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    // Both notes should parse back with correct relative durations
    auto& notes = parsed->parts[0].measures[0].notes;
    REQUIRE(notes.size() == 2);
    // The quarter note should be twice the duration of the eighth
    auto q_dur = notes[0].duration.reduce();
    auto e_dur = notes[1].duration.reduce();
    REQUIRE((q_dur.numerator * e_dur.denominator) == (2 * e_dur.numerator * q_dur.denominator));
}
