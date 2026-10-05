/**
 * @file undo_identity_test.cpp
 * @brief Characterisation tests for undo/redo document-state identity
 *
 *
 *
 * Pins the undo contract at the whole-document level (.h:
 * "mutation + inverse restores previous state exactly") by comparing
 * serialised JSON before mutation and after undo, with only the version
 * counter neutralised. Written against the closure-based engine before
 * its replacement; the snapshot engine must pass unchanged.
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/time.hpp>

using namespace sunny::core;
using json = nlohmann::json;

namespace {

Score make_valid_score(std::uint32_t total_bars = 4) {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Test";
    score.metadata.total_bars = total_bars;

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
    piano.definition.abbreviation = "Pno.";
    piano.definition.instrument_type = InstrumentType::Piano;
    piano.definition.range = PitchRange{
        SpelledPitch{5, 0, 0}, SpelledPitch{0, 0, 8}, SpelledPitch{0, 0, 3}, SpelledPitch{0, 0, 6}};

    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        RestEvent rest{ts->measure_duration(), true};
        Event event{EventId{bar * 1000}, Beat::zero(), rest};
        Voice voice{0, {event}, {}};
        Measure measure{bar, {voice}, std::nullopt, std::nullopt};
        piano.measures.push_back(measure);
    }

    score.parts.push_back(piano);
    return score;
}

/// Serialised document with volatile fields neutralised for identity
/// comparison: version and retired-ID reservations advance through undo.
/// Restored content retains exactly its original typed identities.
json canonical(const Score& score) {
    json j = score_to_json(score);
    j.erase("version");
    j.erase("identity_reservations");
    return j;
}

Note make_c4() {
    return Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}};
}

} // anonymous namespace

// =============================================================================
// Whole-document identity through undo
// =============================================================================

TEST_CASE("insert_note undo restores the exact document", "[score-ir][undo][identity]") {
    auto score = make_valid_score(2);
    UndoStack stack;
    const json before = canonical(score);

    auto result =
        insert_note(score, PartId{100}, 1, 0, Beat::zero(), make_c4(), Beat{1, 1}, &stack);
    REQUIRE(result.has_value());
    CHECK(canonical(score) != before);

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

TEST_CASE("transpose_region undo restores the exact document", "[score-ir][undo][identity]") {
    auto score = make_valid_score(2);
    UndoStack stack;
    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), make_c4(), Beat{1, 1}, nullptr)
                .has_value());
    const json before = canonical(score);

    ScoreRegion region{ScoreTime{1, Beat::zero()}, ScoreTime{3, Beat::zero()}, {}};
    auto result = transpose_region(score, region, DiatonicInterval{4, 2}, &stack);
    REQUIRE(result.has_value());

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

TEST_CASE("delete_measures undo restores the exact document", "[score-ir][undo][identity]") {
    auto score = make_valid_score(4);
    UndoStack stack;
    REQUIRE(insert_note(score, PartId{100}, 2, 0, Beat::zero(), make_c4(), Beat{1, 1}, nullptr)
                .has_value());
    const json before = canonical(score);

    REQUIRE(delete_measures(score, 2, 1, &stack).has_value());
    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

TEST_CASE("remove_part undo restores the exact document", "[score-ir][undo][identity]") {
    auto score = make_valid_score(2);
    UndoStack stack;

    PartDefinition second;
    second.name = "Violin";
    second.abbreviation = "Vln.";
    second.instrument_type = InstrumentType::Violin;
    REQUIRE(add_part(score, second, 1, nullptr).has_value());
    const json before = canonical(score);
    const PartId second_id = score.parts[1].id;

    REQUIRE(remove_part(score, second_id, &stack).has_value());
    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

TEST_CASE("set_time_signature undo restores the exact document", "[score-ir][undo][identity]") {
    auto score = make_valid_score(2);
    UndoStack stack;
    const json before = canonical(score);

    auto ts = make_time_signature(3, 4);
    REQUIRE(set_time_signature(score, 2, *ts, &stack).has_value());
    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

// =============================================================================
// Grouped mutations undo as one unit
// =============================================================================

TEST_CASE("grouped mutations undo as a single step to the exact "
          "pre-group document",
          "[score-ir][undo][identity][group]") {
    auto score = make_valid_score(2);
    UndoStack stack;
    const json before = canonical(score);

    {
        UndoGroup group(stack, "insert three notes");
        REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), make_c4(), Beat{1, 2}, &stack)
                    .has_value());
        REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat{1, 2}, make_c4(), Beat{1, 2}, &stack)
                    .has_value());
        REQUIRE(insert_note(score, PartId{100}, 2, 0, Beat::zero(), make_c4(), Beat{1, 1}, &stack)
                    .has_value());
    }

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
    CHECK_FALSE(stack.can_undo());
}

// =============================================================================
// Undo/redo cycles remain coherent (historical stale-id defect class)
// =============================================================================

TEST_CASE("insert-undo-redo-undo cycle converges on the original "
          "document",
          "[score-ir][undo][identity]") {
    auto score = make_valid_score(1);
    UndoStack stack;
    const json before = canonical(score);

    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), make_c4(), Beat{1, 1}, &stack)
                .has_value());
    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);

    REQUIRE(redo(score, stack).has_value());
    // One note group present after redo
    bool found_note = false;
    for (const auto& ev : score.parts[0].measures[0].voices[0].events) {
        if (ev.as_note_group() != nullptr) found_note = true;
    }
    CHECK(found_note);

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == before);
}

TEST_CASE("interleaved mutations and undos keep prior history "
          "usable",
          "[score-ir][undo][identity]") {
    auto score = make_valid_score(2);
    UndoStack stack;
    const json state0 = canonical(score);

    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), make_c4(), Beat{1, 1}, &stack)
                .has_value());
    const json state1 = canonical(score);

    REQUIRE(insert_note(score, PartId{100}, 2, 0, Beat::zero(), make_c4(), Beat{1, 1}, &stack)
                .has_value());

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == state1);

    REQUIRE(undo(score, stack).has_value());
    CHECK(canonical(score) == state0);
}
