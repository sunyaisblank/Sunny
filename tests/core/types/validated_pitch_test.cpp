/**
 * @file validated_pitch_test.cpp
 * @brief Unit tests for hardened PitchClass and MidiNote value types
 *
 *
 * Coverage: checked construction (from_int refusal with error codes),
 *           euclidean wrapping for PitchClass, default and literal
 *           construction, ordering and equality, and refusal of an
 *           out-of-range pitch at the Score JSON deserialisation boundary.
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <set>
#include <sunny/core/rhythm/meter.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/types/music_types.hpp>
#include <vector>

using namespace sunny::core;

// =============================================================================
// Literal construction is compile-time checked
// =============================================================================

// These compile only because the literals satisfy the invariants; an
// out-of-range literal (e.g. PitchClass{12}) is a compile error, which is
// the contract that replaced the runtime assertions.
static_assert(PitchClass{0}.value() == 0);
static_assert(PitchClass{11}.value() == 11);
static_assert(MidiNote{0}.value() == 0);
static_assert(MidiNote{127}.value() == 127);

// Default construction yields the zero element.
static_assert(PitchClass{} == 0);
static_assert(MidiNote{} == 0);

// Euclidean wrapping is total over int, including negatives.
static_assert(PitchClass::wrapped(-1) == 11);
static_assert(PitchClass::wrapped(-13) == 11);
static_assert(PitchClass::wrapped(-12) == 0);
static_assert(PitchClass::wrapped(12) == 0);
static_assert(PitchClass::wrapped(25) == 1);

// from_int is usable in constant expressions.
static_assert(PitchClass::from_int(5).has_value());
static_assert(!PitchClass::from_int(12).has_value());
static_assert(!MidiNote::from_int(128).has_value());

// =============================================================================
// Checked construction
// =============================================================================

TEST_CASE("PitchClass::from_int refuses values outside [0, 11]",
          "[tensor][types][trust-boundary]") {
    SECTION("Below range") {
        auto pc = PitchClass::from_int(-1);
        REQUIRE_FALSE(pc.has_value());
        REQUIRE(pc.error() == ErrorCode::InvalidPitchClass);
    }

    SECTION("Above range") {
        auto pc = PitchClass::from_int(12);
        REQUIRE_FALSE(pc.has_value());
        REQUIRE(pc.error() == ErrorCode::InvalidPitchClass);
    }

    SECTION("Full valid range accepted with identity value") {
        for (int v = 0; v <= 11; ++v) {
            auto pc = PitchClass::from_int(v);
            REQUIRE(pc.has_value());
            REQUIRE(pc->value() == v);
        }
    }
}

TEST_CASE("MidiNote::from_int refuses values outside [0, 127]", "[tensor][types][trust-boundary]") {
    SECTION("Below range") {
        auto note = MidiNote::from_int(-1);
        REQUIRE_FALSE(note.has_value());
        REQUIRE(note.error() == ErrorCode::InvalidMidiNote);
    }

    SECTION("Above range") {
        auto note = MidiNote::from_int(128);
        REQUIRE_FALSE(note.has_value());
        REQUIRE(note.error() == ErrorCode::InvalidMidiNote);

        auto far = MidiNote::from_int(200);
        REQUIRE_FALSE(far.has_value());
        REQUIRE(far.error() == ErrorCode::InvalidMidiNote);
    }

    SECTION("Boundary values accepted") {
        REQUIRE(MidiNote::from_int(0).value() == 0);
        REQUIRE(MidiNote::from_int(60).value() == 60);
        REQUIRE(MidiNote::from_int(127).value() == 127);
    }
}

// =============================================================================
// Euclidean wrapping
// =============================================================================

TEST_CASE("PitchClass::wrapped is the euclidean mod-12 projection", "[tensor][types]") {
    SECTION("Negative inputs wrap upward") {
        REQUIRE(PitchClass::wrapped(-1) == 11);
        REQUIRE(PitchClass::wrapped(-13) == 11);
        REQUIRE(PitchClass::wrapped(-24) == 0);
    }

    SECTION("Identity on the canonical range") {
        for (int v = 0; v <= 11; ++v) {
            REQUIRE(PitchClass::wrapped(v) == v);
        }
    }

    SECTION("Period 12 for all residues") {
        for (int v = -36; v <= 36; ++v) {
            REQUIRE(PitchClass::wrapped(v) == PitchClass::wrapped(v + 12));
        }
    }
}

// =============================================================================
// Ordering and equality
// =============================================================================

TEST_CASE("PitchClass and MidiNote order and compare by value", "[tensor][types]") {
    SECTION("Equality against typed values and integers") {
        PitchClass pc{7};
        REQUIRE(pc == PitchClass{7});
        REQUIRE(pc != PitchClass{8});
        REQUIRE(pc == 7);
        REQUIRE(7 == pc);

        MidiNote note{60};
        REQUIRE(note == MidiNote{60});
        REQUIRE(note != MidiNote{61});
        REQUIRE(note == 60);
    }

    SECTION("Strict weak ordering drives sorted containers") {
        std::set<PitchClass> pcs{7, 0, 4};
        REQUIRE(pcs.size() == 3);
        REQUIRE(*pcs.begin() == 0);
        REQUIRE(*pcs.rbegin() == 7);

        std::vector<MidiNote> notes{72, 60, 67, 64};
        std::sort(notes.begin(), notes.end());
        REQUIRE(notes.front() == 60);
        REQUIRE(notes.back() == 72);
        REQUIRE(std::is_sorted(notes.begin(), notes.end()));
    }

    SECTION("Relational comparison against integers") {
        PitchClass pc{5};
        REQUIRE(pc < 6);
        REQUIRE(pc > 4);
        REQUIRE(pc <= 5);
        REQUIRE(0 <= pc);

        MidiNote note{120};
        REQUIRE(note > 119);
        REQUIRE(note <= 127);
    }
}

// =============================================================================
// JSON trust boundary
// =============================================================================

namespace {

/// Minimal complete score (tempo, key, time, one part) carrying one
/// harmonic annotation whose chord voicing exercises
/// chord_voicing_from_json on deserialisation.
Score make_annotated_score() {
    Score score;
    score.id = ScoreId{7};
    score.metadata.title = "Hardened Type Boundary";
    score.metadata.total_bars = 1;

    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    auto ts = make_time_signature(4, 4);
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    Part piano;
    piano.id = PartId{100};
    piano.definition.name = "Piano";
    piano.definition.instrument_type = InstrumentType::Piano;

    NoteGroup ng;
    Note n;
    n.pitch = SpelledPitch{0, 0, 4}; // C4
    n.velocity = VelocityValue{{}, 80};
    ng.notes.push_back(n);
    ng.duration = ts->measure_duration();
    Event event{EventId{1001}, Beat::zero(), ng};
    Voice voice{0, {event}, {}};
    Measure measure{1, {voice}, std::nullopt, std::nullopt};
    piano.measures.push_back(measure);
    score.parts.push_back(piano);

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord.notes = {60, 64, 67};
    ha.chord.root = 0;
    ha.chord.quality = "major";
    ha.chord.inversion = 0;
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    ha.confidence = 1.0f;
    score.harmonic_annotations.push_back(ha);

    return score;
}

} // anonymous namespace

TEST_CASE("Score JSON reader refuses chord pitch 200",
          "[tensor][types][trust-boundary][score-ir]") {
    auto original = make_annotated_score();
    auto j = score_to_json(original);

    // Control: the untampered document deserialises.
    REQUIRE(score_from_json(j).has_value());

    // A chord voicing note of 200 violates the MidiNote invariant; the
    // reader must refuse the document instead of truncating the pitch.
    j["harmonic_annotations"][0]["chord"]["notes"][0] = 200;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("Score JSON reader refuses chord root 12", "[tensor][types][trust-boundary][score-ir]") {
    auto original = make_annotated_score();
    auto j = score_to_json(original);

    // Control: the untampered document deserialises.
    REQUIRE(score_from_json(j).has_value());

    // A chord root of 12 violates the PitchClass invariant.
    j["harmonic_annotations"][0]["chord"]["root"] = 12;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}
