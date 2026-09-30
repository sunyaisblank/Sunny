/**
 * @file musicxml_test.cpp
 * @brief MusicXML reader/writer unit tests
 *
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/formats/musicxml.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

std::size_t count_occurrences(const std::string& text, const std::string& fragment) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(fragment, position)) != std::string::npos) {
        ++count;
        position += fragment.size();
    }
    return count;
}

std::string harmony_document(std::string_view harmony, std::string_view prefix_note = {}) {
    return std::string{R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
  <part id="P1"><measure number="1">
    <attributes><divisions>4</divisions><time><beats>4</beats><beat-type>4</beat-type></time></attributes>
)"} + std::string{prefix_note} +
           std::string{harmony} +
           R"(<note><rest/><duration>16</duration><voice>1</voice></note>
  </measure></part>
</score-partwise>)";
}

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

std::string replace_once(std::string input, std::string_view from, std::string_view to) {
    const auto position = input.find(from);
    REQUIRE(position != std::string::npos);
    input.replace(position, from.size(), to);
    return input;
}

} // anonymous namespace

// =============================================================================
// Parse tests
// =============================================================================

TEST_CASE("parse single note C4 quarter", "[musicxml][format]") {
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    REQUIRE(r->parts.size() == 1);
    REQUIRE(r->parts[0].measures.size() == 1);
    REQUIRE(r->parts[0].measures[0].notes.size() == 1);

    const auto& note = r->parts[0].measures[0].notes[0];
    REQUIRE_FALSE(note.is_rest);
    REQUIRE(note.pitch.letter == 0); // C
    REQUIRE(note.pitch.accidental == 0);
    REQUIRE(note.pitch.octave == 4);
}

TEST_CASE("parse chord C-E-G", "[musicxml][format]") {
    auto r = parse_musicxml(CHORD_XML);
    REQUIRE(r.has_value());
    auto& notes = r->parts[0].measures[0].notes;
    REQUIRE(notes.size() == 3);
    REQUIRE_FALSE(notes[0].is_chord);
    REQUIRE(notes[1].is_chord);
    REQUIRE(notes[2].is_chord);
    REQUIRE(notes[0].pitch.letter == 0); // C
    REQUIRE(notes[1].pitch.letter == 2); // E
    REQUIRE(notes[2].pitch.letter == 4); // G
}

TEST_CASE("parse rest", "[musicxml][format]") {
    auto r = parse_musicxml(REST_XML);
    REQUIRE(r.has_value());
    REQUIRE(r->parts[0].measures[0].notes[0].is_rest);
}

TEST_CASE("compact MusicXML preserves typed harmony and exact local timing",
          "[musicxml][format][harmony][roundtrip]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};
    measure.notes.push_back({{}, Beat{1, 1}, true, false, 1});

    MusicXmlHarmony harmony;
    harmony.offset = Beat{1, 2};
    harmony.symbol.root = SpelledPitch{4, 0, 4};
    harmony.symbol.quality = "dominant";
    harmony.symbol.bass = SpelledPitch{6, 0, 3};
    harmony.symbol.roman = "V65";
    harmony.symbol.numeral = ChordNumeral{5, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
    harmony.symbol.inversion = 1;
    harmony.symbol.degrees = {{9, -1, ChordDegreeType::Add},
                              {5, 1, ChordDegreeType::Alter},
                              {3, 0, ChordDegreeType::Subtract}};
    measure.harmonies.push_back(harmony);
    part.measures.push_back(measure);
    source.parts.push_back(part);

    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());
    CHECK(xml->find("<numeral-root text=\"V65\">5</numeral-root>") != std::string::npos);
    CHECK(xml->find("<offset>2</offset>") != std::string::npos);

    const auto parsed = parse_musicxml(*xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts[0].measures[0].harmonies.size() == 1);
    const auto& restored = parsed->parts[0].measures[0].harmonies[0];
    CHECK(restored.offset == Beat{1, 2});
    CHECK(restored.symbol.root == SpelledPitch{4, 0, 4});
    CHECK(restored.symbol.quality == "dominant");
    CHECK(restored.symbol.bass == SpelledPitch{6, 0, 3});
    CHECK(restored.symbol.roman == "V65");
    CHECK(restored.symbol.numeral == harmony.symbol.numeral);
    CHECK(restored.symbol.inversion == 1);
    CHECK(restored.symbol.degrees == harmony.symbol.degrees);
}

TEST_CASE("MusicXML harmony offset is resolved against the live sequential cursor",
          "[musicxml][format][harmony][timing]") {
    const auto xml = harmony_document(
        R"(<harmony><root><root-step>G</root-step></root><kind>dominant</kind><offset>-2</offset></harmony>)",
        R"(<note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><voice>1</voice></note>)");
    const auto parsed = parse_musicxml(xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts[0].measures[0].harmonies.size() == 1);
    CHECK(parsed->parts[0].measures[0].harmonies[0].offset == Beat{1, 8});
}

TEST_CASE("compact MusicXML rejects harmony outside its exact semantic profile",
          "[musicxml][format][harmony][trust-boundary]") {
    SECTION("deprecated function branch") {
        CHECK_FALSE(
            parse_musicxml(harmony_document(
                               R"(<harmony><function>V</function><kind>dominant</kind></harmony>)"))
                .has_value());
    }
    SECTION("stacked harmony chords") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><root><root-step>C</root-step></root><root><root-step>E</root-step></root><kind>major</kind></harmony>)"))
                .has_value());
    }
    SECTION("numeral without explicit key") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><numeral><numeral-root>5</numeral-root></numeral><kind>dominant</kind></harmony>)"))
                .has_value());
    }
    SECTION("fractional numeral alteration") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><numeral><numeral-root>5</numeral-root><numeral-alter>0.5</numeral-alter><numeral-key><numeral-fifths>0</numeral-fifths><numeral-mode>major</numeral-mode></numeral-key></numeral><kind>dominant</kind></harmony>)"))
                .has_value());
    }
    SECTION("fractional degree alteration") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><root><root-step>C</root-step></root><kind>major</kind><degree><degree-value>9</degree-value><degree-alter>0.5</degree-alter><degree-type>add</degree-type></degree></harmony>)"))
                .has_value());
    }
    SECTION("non-primary staff") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><root><root-step>C</root-step></root><kind>major</kind><staff>2</staff></harmony>)"))
                .has_value());
    }
    SECTION("unretained semantic or display attributes") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony type="alternate"><root><root-step>C</root-step></root><kind use-symbols="yes">major</kind></harmony>)"))
                .has_value());
    }
    SECTION("negative absolute point") {
        CHECK_FALSE(
            parse_musicxml(
                harmony_document(
                    R"(<harmony><root><root-step>C</root-step></root><kind>major</kind><offset>-1</offset></harmony>)"))
                .has_value());
    }
}

TEST_CASE("compact MusicXML writer rejects ambiguous harmony state",
          "[musicxml][format][harmony][trust-boundary]") {
    MusicXmlScore score;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.notes.push_back({{}, Beat{1, 1}, true, false, 1});
    MusicXmlHarmony harmony;
    harmony.symbol.root = SpelledPitch{0, 0, 4};
    harmony.symbol.quality = "major";
    measure.harmonies.push_back(harmony);
    part.measures.push_back(measure);
    score.parts.push_back(part);

    SECTION("legacy Roman text has no root-branch semantic carrier") {
        score.parts[0].measures[0].harmonies[0].symbol.roman = "I";
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("unknown quality plus display extension cannot be separated") {
        auto& symbol = score.parts[0].measures[0].harmonies[0].symbol;
        symbol.quality = "quartal";
        symbol.extensions = {"add13"};
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("harmony pitch octaves are canonical because MusicXML omits them") {
        score.parts[0].measures[0].harmonies[0].symbol.root.octave = 5;
        CHECK_FALSE(write_musicxml(score).has_value());
    }
}

TEST_CASE("parse key signature", "[musicxml][format]") {
    // fifths=0 => C major
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    auto& m = r->parts[0].measures[0];
    REQUIRE(m.key_tonic.has_value());
    REQUIRE(m.key_tonic->letter == 0); // C
    REQUIRE(m.key_tonic->accidental == 0);
    REQUIRE(m.key_is_major.value_or(true));
    REQUIRE(m.key_fifths == 0);
    REQUIRE(m.key_mode == "major");
}

TEST_CASE("MusicXML traditional modal key preserves exact fifths mode and tonic",
          "[musicxml][format][key]") {
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
  <part id="P1"><measure number="1"><attributes>
    <divisions>1</divisions><key><fifths> 0 </fifths><mode>dorian</mode></key>
  </attributes></measure></part>
</score-partwise>)";

    const auto parsed = parse_musicxml(xml);
    REQUIRE(parsed.has_value());
    const auto& key = parsed->parts[0].measures[0];
    REQUIRE(key.key_fifths == 0);
    REQUIRE(key.key_mode == "dorian");
    REQUIRE(key.key_tonic.has_value());
    CHECK(key.key_tonic->letter == 1); // D
    CHECK(key.key_tonic->accidental == 0);
    CHECK_FALSE(key.key_is_major.has_value());

    const auto written = write_musicxml(*parsed);
    REQUIRE(written.has_value());
    CHECK(written->find("<fifths>0</fifths>") != std::string::npos);
    CHECK(written->find("<mode>dorian</mode>") != std::string::npos);
}

TEST_CASE("MusicXML low-level key profile rejects unrepresentable or contradictory keys",
          "[musicxml][format][key][trust-boundary]") {
    const std::string non_traditional = R"(<?xml version="1.0"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
  <part id="P1"><measure number="1"><attributes><divisions>1</divisions>
    <key><key-step>B</key-step><key-alter>-1</key-alter></key>
  </attributes></measure></part>
</score-partwise>)";
    CHECK_FALSE(parse_musicxml(non_traditional).has_value());

    auto parsed = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(parsed.has_value());
    auto& key = parsed->parts[0].measures[0];
    key.key_fifths = 1;
    key.key_tonic = SpelledPitch{0, 0, 4};
    CHECK_FALSE(write_musicxml(*parsed).has_value());

    key.key_fifths = 0;
    key.key_mode = "unsupported-mode";
    CHECK_FALSE(write_musicxml(*parsed).has_value());
}

TEST_CASE("MusicXML compact reader rejects values outside its exact numeric and timing profile",
          "[musicxml][format][trust-boundary]") {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"non-numeric measure label",
         replace_once(SINGLE_NOTE_XML, "measure number=\"1\"", "measure number=\"X1\"")},
        {"decimal divisions",
         replace_once(SINGLE_NOTE_XML, "<divisions>1</divisions>", "<divisions>1.5</divisions>")},
        {"missing initial divisions",
         replace_once(SINGLE_NOTE_XML, "<divisions>1</divisions>", "")},
        {"additive metre", replace_once(SINGLE_NOTE_XML, "<beats>4</beats>", "<beats>3+2</beats>")},
        {"microtonal alteration",
         replace_once(SINGLE_NOTE_XML, "<step>C</step>", "<step>C</step><alter>0.5</alter>")},
        {"invalid octave",
         replace_once(SINGLE_NOTE_XML, "<octave>4</octave>", "<octave>four</octave>")},
        {"string voice", replace_once(SINGLE_NOTE_XML, "<voice>1</voice>", "<voice>upper</voice>")},
        {"decimal duration",
         replace_once(SINGLE_NOTE_XML, "<duration>1</duration>", "<duration>0.5</duration>")},
        {"cursor backup",
         replace_once(
             SINGLE_NOTE_XML, "</note>", "</note><backup><duration>1</duration></backup>")},
        {"unpitched note",
         replace_once(SINGLE_NOTE_XML,
                      "<pitch><step>C</step><octave>4</octave></pitch>",
                      "<unpitched><display-step>C</display-step><display-octave>4</display-octave>"
                      "</unpitched>")},
    };

    for (const auto& [label, xml] : cases) {
        CAPTURE(label);
        const auto parsed = parse_musicxml(xml);
        CHECK_FALSE(parsed.has_value());
        if (!parsed) CHECK(parsed.error() == ErrorCode::InvalidMusicXml);
    }
}

TEST_CASE("MusicXML source divisions are normalized into exact Beat durations",
          "[musicxml][format][timing]") {
    const std::string xml = R"(<?xml version="1.0"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
  <part id="P1">
    <measure number="1"><attributes><divisions>1</divisions></attributes>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>1</duration></note>
    </measure>
    <measure number="2"><attributes><divisions>3</divisions></attributes>
      <note><pitch><step>D</step><octave>4</octave></pitch><duration>1</duration></note>
    </measure>
  </part>
</score-partwise>)";

    const auto parsed = parse_musicxml(xml);
    REQUIRE(parsed.has_value());
    CHECK(parsed->parts[0].measures[0].notes[0].duration == Beat{1, 4});
    CHECK(parsed->parts[0].measures[1].notes[0].duration == Beat{1, 12});

    const auto written = write_musicxml(*parsed);
    REQUIRE(written.has_value());
    CHECK(count_occurrences(*written, "<divisions>3</divisions>") == 1);
    CHECK(written->find("<duration>3</duration>") != std::string::npos);
    CHECK(written->find("<duration>1</duration>") != std::string::npos);
}

TEST_CASE("parse Eb major key", "[musicxml][format]") {
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

TEST_CASE("MusicXML parser reads time signatures", "[musicxml][format]") {
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

TEST_CASE("MusicXML single-note write-parse round trip", "[musicxml][format]") {
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
    note.pitch = SpelledPitch{0, 0, 4}; // C4
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

TEST_CASE("write-parse round-trip chord", "[musicxml][format]") {
    MusicXmlScore original;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure m;
    m.number = 1;
    m.time_signature = {4, 4};
    m.key_tonic = SpelledPitch{0, 0, 4};
    m.key_is_major = true;

    m.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1}); // C4
    m.notes.push_back({SpelledPitch{2, 0, 4}, Beat{1, 4}, false, true, 1});  // E4 chord
    m.notes.push_back({SpelledPitch{4, 0, 4}, Beat{1, 4}, false, true, 1});  // G4 chord

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

TEST_CASE("write-parse round-trip multi-measure", "[musicxml][format]") {
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

TEST_CASE("SpelledPitch preservation C# != Db", "[musicxml][format]") {
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
    REQUIRE(n.pitch.letter == 0);     // C (not D)
    REQUIRE(n.pitch.accidental == 1); // sharp (not flat)
}

// =============================================================================
// Conversion functions
// =============================================================================

TEST_CASE("musicxml_to_note_events", "[musicxml][format]") {
    auto r = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(r.has_value());
    auto events = musicxml_to_note_events(*r);
    REQUIRE(events.has_value());
    REQUIRE(events->size() == 1);
    REQUIRE((*events)[0].pitch == 60); // C4
}

TEST_CASE("note_events_to_musicxml", "[musicxml][format]") {
    std::array<NoteEvent, 2> events = {
        NoteEvent{60, Beat{0, 1}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 80},
    };
    auto score = note_events_to_musicxml(events, 0);
    REQUIRE(score.has_value());
    REQUIRE(score->parts.size() == 1);
    REQUIRE(score->parts[0].measures[0].notes.size() == 2);
    REQUIRE_FALSE(score->parts[0].measures[0].time_signature.has_value());
}

TEST_CASE("compact MusicXML and NoteEvent adapters preserve rests, gaps, and chords",
          "[musicxml][format][adapter][timing]") {
    const std::array<NoteEvent, 4> events = {
        NoteEvent{60, Beat{1, 4}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 80},
        NoteEvent{0, Beat{1, 2}, Beat{1, 8}, 0, true},
        NoteEvent{67, Beat{3, 4}, Beat{1, 4}, 80},
    };

    const auto score = note_events_to_musicxml(events, 0);
    REQUIRE(score.has_value());
    const auto& notes = score->parts[0].measures[0].notes;
    REQUIRE(notes.size() == 6);
    CHECK(notes[0].is_rest);       // leading quarter gap
    CHECK_FALSE(notes[1].is_rest); // C anchor
    CHECK(notes[2].is_chord);      // E chord member
    CHECK(notes[3].is_rest);       // explicit muted eighth
    CHECK(notes[4].is_rest);       // eighth gap before G
    CHECK_FALSE(notes[5].is_rest); // G

    const auto restored = musicxml_to_note_events(*score);
    REQUIRE(restored.has_value());
    REQUIRE(restored->size() == 6); // structural rests remain explicit muted events
    CHECK((*restored)[0].muted);
    CHECK((*restored)[0].start_time == Beat::zero());
    CHECK((*restored)[1].start_time == Beat{1, 4});
    CHECK((*restored)[2].start_time == Beat{1, 4});
    CHECK((*restored)[3].muted);
    CHECK((*restored)[4].muted);
    CHECK((*restored)[4].start_time == Beat{5, 8});
    CHECK((*restored)[4].duration == Beat{1, 8});
    CHECK((*restored)[5].pitch == 67);
    CHECK((*restored)[5].start_time == Beat{3, 4});
}

TEST_CASE("compact MusicXML NoteEvent adapters reject unrepresentable stream state",
          "[musicxml][format][adapter][trust-boundary]") {
    CHECK_FALSE(note_events_to_musicxml(std::array{NoteEvent{60, Beat::zero(), Beat{1, 4}, 79}}, 0)
                    .has_value());
    CHECK_FALSE(
        note_events_to_musicxml(std::array{NoteEvent{0, Beat::zero(), Beat{1, 4}, 80, true}}, 0)
            .has_value());
    CHECK_FALSE(note_events_to_musicxml(
                    std::array{
                        NoteEvent{60, Beat::zero(), Beat{1, 2}, 80},
                        NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 80},
                    },
                    0)
                    .has_value());
    CHECK_FALSE(note_events_to_musicxml(
                    std::array{
                        NoteEvent{60, Beat::zero(), Beat{1, 4}, 80},
                        NoteEvent{64, Beat::zero(), Beat{1, 8}, 80},
                    },
                    0)
                    .has_value());
    CHECK_FALSE(note_events_to_musicxml(std::array{NoteEvent{60, Beat::zero(), Beat{1, 4}, 80}},
                                        std::numeric_limits<int>::max())
                    .has_value());

    auto parsed = parse_musicxml(SINGLE_NOTE_XML);
    REQUIRE(parsed.has_value());
    parsed->parts.push_back(parsed->parts.front());
    parsed->parts.back().id = "P2";
    CHECK_FALSE(musicxml_to_note_events(*parsed).has_value());
}

TEST_CASE("compact MusicXML adapter rejects non-default release velocity",
          "[musicxml][format][adapter][release-velocity][trust-boundary]") {
    auto event = NoteEvent{60, Beat::zero(), Beat{1, 4}, 80};
    event.release_velocity = 23;
    const auto score = note_events_to_musicxml(std::array{event});
    REQUIRE_FALSE(score.has_value());
    CHECK(score.error() == ErrorCode::TargetValueUnrepresentable);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("invalid XML", "[musicxml][format]") {
    auto r = parse_musicxml("not xml at all");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMusicXml);
}

TEST_CASE("missing score-partwise", "[musicxml][format]") {
    auto r = parse_musicxml("<?xml version=\"1.0\"?><other/>");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidMusicXml);
}

TEST_CASE("empty score is rejected because MusicXML requires declared and actual parts",
          "[musicxml][format][trust-boundary]") {
    MusicXmlScore score;
    score.title = "Empty";
    auto xml = write_musicxml(score);
    REQUIRE_FALSE(xml.has_value());
    CHECK(xml.error() == ErrorCode::InvalidMusicXml);
}

TEST_CASE("MusicXML compact writer rejects invalid internal note states",
          "[musicxml][format][trust-boundary]") {
    MusicXmlScore score;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1});
    part.measures.push_back(measure);
    score.parts.push_back(part);

    SECTION("invalid pitch letter") {
        score.parts[0].measures[0].notes[0].pitch.letter = 7;
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("non-positive duration") {
        score.parts[0].measures[0].notes[0].duration = Beat::zero();
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("duration units outside the compact int domain are rejected") {
        score.parts[0].measures[0].notes[0].duration = Beat{std::numeric_limits<int>::max(), 1};
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("a divisions LCM outside the compact int domain is rejected") {
        score.parts[0].measures[0].notes[0].duration = Beat{1, 46337};
        score.parts[0].measures[0].notes.push_back(
            {SpelledPitch{1, 0, 4}, Beat{1, 46349}, false, false, 1});
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("non-numeric-profile voice") {
        score.parts[0].measures[0].notes[0].voice = 0;
        CHECK_FALSE(write_musicxml(score).has_value());
    }
    SECTION("orphan chord") {
        score.parts[0].measures[0].notes[0].is_chord = true;
        CHECK_FALSE(write_musicxml(score).has_value());
    }
}

TEST_CASE("divisions calculation", "[musicxml][format]") {
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
    REQUIRE((q_dur.numerator() * e_dur.denominator()) ==
            (2 * e_dur.numerator() * q_dur.denominator()));
}

TEST_CASE("MusicXML graphic duration is emitted only when exactly derivable",
          "[musicxml][format][duration]") {
    MusicXmlScore score;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 5}, false, false, 1});
    part.measures.push_back(measure);
    score.parts.push_back(part);

    SECTION("arbitrary rational duration omits optional type rather than inventing quarter") {
        const auto xml = write_musicxml(score);
        REQUIRE(xml.has_value());
        CHECK(xml->find("<duration>4</duration>") != std::string::npos);
        CHECK(xml->find("<type>") == std::string::npos);
    }

    SECTION("multiple augmentation dots and their base type are derived exactly") {
        score.parts[0].measures[0].notes[0].duration = Beat{7, 16};
        const auto xml = write_musicxml(score);
        REQUIRE(xml.has_value());
        CHECK(xml->find("<type>quarter</type>") != std::string::npos);
        CHECK(count_occurrences(*xml, "<dot") == 2);
    }
}
