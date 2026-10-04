/**
 * @file analysis_test.cpp
 * @brief Unit tests for Corpus IR analysis engine
 *
 *
 * Coverage: Each analytical domain verified against a Score containing
 *           known musical content with independently computed expected values.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/corpus/analysis.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/workflows.hpp>

using namespace sunny::core;

// =============================================================================
// Helpers — build minimal valid Scores with known content
// =============================================================================

namespace {

/// Create a 4-bar Score in C major, 4/4, 120 BPM with one Piano part
Score make_basic_score() {
    ScoreSpec spec;
    spec.title = "Test Analysis";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4}; // C
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

/// Create a two-part Score (Violin + Cello)
Score make_two_part_score() {
    ScoreSpec spec;
    spec.title = "Two Part Analysis";
    spec.total_bars = 4;
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
    spec.parts.push_back(violin);

    PartDefinition cello;
    cello.name = "Cello";
    cello.abbreviation = "Vc.";
    cello.instrument_type = InstrumentType::Cello;
    cello.clef = Clef::Bass;
    cello.rendering.midi_channel = 2;
    spec.parts.push_back(cello);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

/// Insert a note at bar_idx (0-indexed), replacing the whole-bar rest
void put_note(Score& score,
              std::size_t part,
              std::uint32_t bar_idx,
              SpelledPitch pitch,
              Beat offset,
              Beat duration,
              std::optional<DynamicLevel> dyn = std::nullopt) {
    auto& voice = score.parts[part].measures[bar_idx].voices[0];
    voice.events.clear();

    Note note;
    note.pitch = pitch;
    note.velocity = VelocityValue{dyn, 80};
    if (dyn) note.dynamic = dyn;
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = duration;

    if (offset != Beat::zero()) {
        RestEvent pre{offset, true};
        voice.events.push_back(
            Event{EventId{90001 + part * 1000 + bar_idx * 10}, Beat::zero(), pre});
    }
    voice.events.push_back(Event{EventId{90002 + part * 1000 + bar_idx * 10}, offset, ng});

    Beat after = offset + duration;
    Beat bar_dur{1, 1};
    if (after < bar_dur) {
        RestEvent post{bar_dur - after, true};
        voice.events.push_back(Event{EventId{90003 + part * 1000 + bar_idx * 10}, after, post});
    }
}

SpelledPitch C4{0, 0, 4};
SpelledPitch D4{1, 0, 4};
SpelledPitch E4{2, 0, 4};
SpelledPitch F4{3, 0, 4};
SpelledPitch G4{4, 0, 4};
SpelledPitch C3{0, 0, 3};

} // namespace

// =============================================================================
// Full analysis
// =============================================================================

TEST_CASE("analyze_score produces all domains", "[corpus-ir][analysis]") {
    auto score = make_basic_score();
    auto wa = analyze_score(score);
    REQUIRE(wa);

    // Formal analysis should reflect 4 bars
    CHECK(wa->formal_analysis.total_duration_bars == 4);

    // Orchestration should be nullopt for single-part
    CHECK_FALSE(wa->orchestration_analysis.has_value());

    // Motivic analysis present but empty
    CHECK(wa->motivic_analysis.thematic_density == 0.0f);
}

// =============================================================================
// Melodic analysis
// =============================================================================

TEST_CASE("melodic range from note content", "[corpus-ir][analysis][melodic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, G4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 2, E4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 3, C4, Beat::zero(), Beat{1, 4});

    auto ma = analyze_melodic(score);
    REQUIRE(ma);
    REQUIRE(ma->per_voice_analysis.size() == 1);
    // C4 = MIDI 60, G4 = MIDI 67
    CHECK(ma->per_voice_analysis[0].note_count == 4);
    CHECK(ma->per_voice_analysis[0].range_low == 60);
    CHECK(ma->per_voice_analysis[0].range_high == 67);
}

TEST_CASE("empty melodic parts carry explicit zero evidence", "[corpus-ir][analysis][melodic]") {
    const auto ma = analyze_melodic(make_basic_score());
    REQUIRE(ma);
    REQUIRE(ma->per_voice_analysis.size() == 1);
    CHECK(ma->per_voice_analysis[0].note_count == 0);
}

TEST_CASE("melodic conjunct proportion for stepwise melody", "[corpus-ir][analysis][melodic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 2, E4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 3, F4, Beat::zero(), Beat{1, 4});

    auto ma = analyze_melodic(score);
    REQUIRE(ma);
    REQUIRE(ma->per_voice_analysis.size() == 1);
    // All intervals are steps (M2) — conjunct proportion should be 1.0
    CHECK(ma->per_voice_analysis[0].conjunct_proportion == Catch::Approx(1.0f));
}

// =============================================================================
// Rhythmic analysis
// =============================================================================

TEST_CASE("rhythmic duration distribution", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4});

    auto ra = analyze_rhythmic(score);
    REQUIRE(ra);
    // Two quarter notes inserted
    CHECK(ra->duration_distribution.count("quarter") > 0);
    CHECK(ra->duration_distribution["quarter"] >= 2);
}

TEST_CASE("rhythmic onset density per bar", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});

    auto ra = analyze_rhythmic(score);
    REQUIRE(ra);
    REQUIRE(ra->onset_density.size() == 4);
    // Bar 1 has 1 onset, bars 2-4 have 0
    CHECK(ra->onset_density[0] >= 1.0f);
}

TEST_CASE("tempo profile from tempo map", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    auto ra = analyze_rhythmic(score);
    REQUIRE(ra);
    REQUIRE(!ra->tempo_profile.empty());
    CHECK(ra->tempo_profile[0].second == Catch::Approx(120.0f));
}

TEST_CASE("rhythmic metre distribution follows the global time map",
          "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    const auto three_four = make_time_signature(3, 4);
    REQUIRE(three_four.has_value());
    score.time_map.push_back({3, *three_four});
    for (std::size_t bar = 2; bar < 4; ++bar)
        std::get<RestEvent>(score.parts[0].measures[bar].voices[0].events[0].payload).duration =
            Beat{3, 4};

    const auto ra = analyze_rhythmic(score);
    REQUIRE(ra);
    CHECK(ra->metre_distribution.at("4/4") == 2);
    CHECK(ra->metre_distribution.at("3/4") == 2);
}

// =============================================================================
// Formal analysis
// =============================================================================

TEST_CASE("formal analysis total bars", "[corpus-ir][analysis][formal]") {
    auto score = make_basic_score();
    auto fa = analyze_formal(score);
    REQUIRE(fa);
    CHECK(fa->total_duration_bars == 4);
}

TEST_CASE("formal analysis with sections", "[corpus-ir][analysis][formal]") {
    auto score = make_basic_score();
    ScoreSection sec_a;
    sec_a.id = SectionId{1};
    sec_a.label = "A";
    sec_a.start = ScoreTime{1, Beat::zero()};
    sec_a.end = ScoreTime{3, Beat::zero()};
    score.section_map.push_back(sec_a);

    ScoreSection sec_b;
    sec_b.id = SectionId{2};
    sec_b.label = "B";
    sec_b.start = ScoreTime{3, Beat::zero()};
    sec_b.end = ScoreTime{5, Beat::zero()};
    score.section_map.push_back(sec_b);

    auto fa = analyze_formal(score);
    REQUIRE(fa);
    REQUIRE(fa->section_plan.size() == 2);
    CHECK(fa->section_plan[0].label == "A");
    CHECK(fa->section_plan[0].length_bars == 2);
    CHECK(fa->section_plan[1].label == "B");
    CHECK(fa->section_plan[1].length_bars == 2);

    // Two sections → BinarySimple
    CHECK(fa->form_type == FormClassification::BinarySimple);
}

TEST_CASE("harmonic and formal tonal plans preserve key, relationship, and return bar",
          "[corpus-ir][analysis][tonal-plan]") {
    auto score = make_basic_score();
    auto dominant = score.key_map.front();
    dominant.position = {2, Beat::zero()};
    dominant.key.root = G4;
    dominant.key.accidentals = 1;
    score.key_map.push_back(dominant);
    auto tonic_return = score.key_map.front();
    tonic_return.position = {4, Beat::zero()};
    score.key_map.push_back(tonic_return);

    const auto harmonic = analyze_harmonic(score);
    REQUIRE(harmonic);
    const auto formal = analyze_formal(score);
    REQUIRE(formal);
    REQUIRE(harmonic->tonal_plan.key_sequence.size() == 3);
    CHECK(harmonic->tonal_plan.key_sequence[0] ==
          std::tuple<std::string, std::string, std::uint32_t>{"C major", "tonic", 1});
    CHECK(harmonic->tonal_plan.key_sequence[1] ==
          std::tuple<std::string, std::string, std::uint32_t>{"G major", "perfect_fifth", 2});
    CHECK(harmonic->tonal_plan.key_area_count == 2);
    CHECK(harmonic->tonal_plan.most_distant_key == "G major");
    REQUIRE(harmonic->tonal_plan.tonic_return_bar.has_value());
    CHECK(*harmonic->tonal_plan.tonic_return_bar == 4);
    CHECK(formal->tonal_plan.key_sequence == harmonic->tonal_plan.key_sequence);
}

TEST_CASE("ternary form ABA detected", "[corpus-ir][analysis][formal]") {
    ScoreSpec spec;
    spec.title = "ABA";
    spec.total_bars = 6;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.clef = Clef::Treble;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);
    auto score = *create_score(spec);

    score.section_map.push_back(
        ScoreSection{SectionId{1}, "A", {1, Beat::zero()}, {3, Beat::zero()}, {}, std::nullopt});
    score.section_map.push_back(
        ScoreSection{SectionId{2}, "B", {3, Beat::zero()}, {5, Beat::zero()}, {}, std::nullopt});
    score.section_map.push_back(
        ScoreSection{SectionId{3}, "A", {5, Beat::zero()}, {7, Beat::zero()}, {}, std::nullopt});

    auto fa = analyze_formal(score);
    REQUIRE(fa);
    CHECK(fa->form_type == FormClassification::Ternary);
}

// =============================================================================
// Textural analysis
// =============================================================================

TEST_CASE("textural density for two-part score", "[corpus-ir][analysis][textural]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4});
    put_note(score, 1, 0, C3, Beat::zero(), Beat{1, 4});

    auto ta = analyze_textural(score);
    // Bar 1 has both parts active → density 2
    REQUIRE(ta.density_curve.size() == 4);
    CHECK(ta.density_curve[0].second == 2);
    CHECK(ta.average_density > 0.0f);
}

TEST_CASE("textural register span", "[corpus-ir][analysis][textural]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4}); // MIDI 67
    put_note(score, 1, 0, C3, Beat::zero(), Beat{1, 4}); // MIDI 48

    auto ta = analyze_textural(score);
    // Span = 67 - 48 = 19 semitones in bar 1, 0 in bars 2-4
    CHECK(ta.average_register_span > 0.0f);
}

// =============================================================================
// Dynamic analysis
// =============================================================================

TEST_CASE("dynamic distribution from note markings", "[corpus-ir][analysis][dynamic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4}, DynamicLevel::f);
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4}, DynamicLevel::p);

    auto da = analyze_dynamic(score);
    CHECK(da.dynamic_distribution.count("f") > 0);
    CHECK(da.dynamic_distribution.count("p") > 0);
    CHECK(da.dynamic_range_low == "p");
    CHECK(da.dynamic_range_high == "f");
    CHECK(da.dynamic_change_rate == Catch::Approx(0.25f));
}

TEST_CASE("dynamic analysis does not fabricate an unmarked mf curve",
          "[corpus-ir][analysis][dynamic]") {
    const auto da = analyze_dynamic(make_basic_score());
    CHECK(da.dynamic_range_low.empty());
    CHECK(da.dynamic_range_high.empty());
    CHECK(da.dynamic_shape.empty());
    CHECK(da.climax_position == 0.0f);
}

TEST_CASE("accent dynamics use intensity rather than enum order for range",
          "[corpus-ir][analysis][dynamic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4}, DynamicLevel::f);
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4}, DynamicLevel::sfz);

    const auto da = analyze_dynamic(score);
    CHECK(da.dynamic_range_low == "f");
    CHECK(da.dynamic_range_high == "sfz");
    CHECK(da.subito_dynamics_count == 1);
}

TEST_CASE("hairpin count from part hairpins", "[corpus-ir][analysis][dynamic]") {
    auto score = make_basic_score();
    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    score.parts[0].hairpins.push_back(hp);

    auto da = analyze_dynamic(score);
    CHECK(da.hairpin_count == 1);
}

// =============================================================================
// Orchestration analysis
// =============================================================================

TEST_CASE("orchestration nullopt for single part", "[corpus-ir][analysis][orch]") {
    auto score = make_basic_score();
    auto oa = analyze_orchestration(score);
    CHECK_FALSE(oa.has_value());
}

TEST_CASE("orchestration instrument usage for two parts", "[corpus-ir][analysis][orch]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4});
    // Cello is silent

    auto oa = analyze_orchestration(score);
    REQUIRE(oa.has_value());
    CHECK(oa->instrument_usage.count("Violin") > 0);
    CHECK(oa->instrument_usage.count("Cello") > 0);
    // Violin plays in 1 of 4 bars = 0.25
    CHECK(oa->instrument_usage["Violin"] == Catch::Approx(0.25f));
    // Cello plays in 0 of 4 bars = 0.0
    CHECK(oa->instrument_usage["Cello"] == Catch::Approx(0.0f));
}

// =============================================================================
// Integration with analyze_work
// =============================================================================

TEST_CASE("analyze_work with Score populates analysis", "[corpus-ir][analysis][workflow]") {
    CorpusDatabase db;
    auto profile = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = profile;

    WorkMetadata meta;
    meta.title = "Test Work";
    meta.source_format = "test";
    auto work = create_ingested_work(IngestedWorkId{1}, meta);
    db.works[1] = work;
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());
    PeriodProfile period;
    period.label = "Early";
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, period).has_value());
    REQUIRE(
        assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early").has_value());

    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, E4, Beat::zero(), Beat{1, 4});

    auto r = analyze_work(db, IngestedWorkId{1}, &score);
    REQUIRE(r.has_value());

    CHECK(db.works[1].analysis_complete);
    // Melodic analysis should have found notes
    CHECK(!db.works[1].analysis.melodic_analysis.per_voice_analysis.empty());
    // Formal analysis should know total bars
    CHECK(db.works[1].analysis.formal_analysis.total_duration_bars == 4);
    // Rhythmic analysis should have duration data
    CHECK(!db.works[1].analysis.rhythmic_analysis.onset_density.empty());
    CHECK(db.composers[1].style_profile.sample_size == 1);
    CHECK(db.composers[1].period_profiles[0].profile.sample_size == 1);
}

// =============================================================================
// Regression: scale construction and tie folding (issue #12)
// =============================================================================

namespace {

struct LineNote {
    SpelledPitch pitch;
    Beat duration;
    bool tie_forward = false;
};

/// Replace one bar of voice 0 with consecutive notes; the bar must be filled exactly.
void put_line(Score& score, std::size_t part, std::uint32_t bar_idx, std::vector<LineNote> line) {
    auto& voice = score.parts[part].measures[bar_idx].voices[0];
    voice.events.clear();
    Beat offset = Beat::zero();
    std::uint64_t event_id = 91000 + 1000 * part + 16 * bar_idx;
    for (const auto& item : line) {
        Note note;
        note.pitch = item.pitch;
        note.velocity = VelocityValue{std::nullopt, 80};
        note.tie_forward = item.tie_forward;
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = item.duration;
        voice.events.push_back(Event{EventId{event_id++}, offset, group});
        offset = offset + item.duration;
    }
    REQUIRE(offset == Beat{1, 1});
}

Score make_score_in(SpelledPitch key_root, std::uint32_t bars) {
    ScoreSpec spec;
    spec.title = "Key";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = key_root;
    PartDefinition flute;
    flute.name = "Flute";
    flute.abbreviation = "Fl.";
    flute.instrument_type = InstrumentType::Flute;
    flute.clef = Clef::Treble;
    flute.rendering.midi_channel = 1;
    spec.parts.push_back(flute);
    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

std::uint32_t histogram_total(const std::map<std::uint8_t, std::uint32_t>& histogram) {
    std::uint32_t total = 0;
    for (const auto& [degree, count] : histogram)
        total += count;
    return total;
}

const Beat quarter{1, 4};

} // namespace

TEST_CASE("a G major arpeggio is wholly diatonic in G major",
          "[corpus-ir][analysis][melodic][regression]") {
    // G major = G + {0,2,4,5,7,9,11} = {G,A,B,C,D,E,F#}; G B D B are degrees 1 3 5 3.
    auto score = make_score_in(SpelledPitch{4, 0, 4}, 1);
    put_line(score,
             0,
             0,
             {{SpelledPitch{4, 0, 4}, quarter},
              {SpelledPitch{6, 0, 4}, quarter},
              {SpelledPitch{1, 0, 5}, quarter},
              {SpelledPitch{6, 0, 4}, quarter}});

    const auto voice = analyze_melodic(score)->per_voice_analysis.at(0);
    CHECK(voice.note_count == 4);
    CHECK(voice.chromaticism_rate == Catch::Approx(0.0f));
    CHECK(voice.scale_degree_distribution ==
          std::map<std::uint8_t, std::uint32_t>{{1, 1}, {3, 2}, {5, 1}});
}

TEST_CASE("one chromatic tone in five gives chromaticism 0.2 and a complete histogram",
          "[corpus-ir][analysis][melodic][regression]") {
    // C D E F# G in C major: F# is the only non-diatonic pitch; bucket 0 is chromatic.
    auto score = make_score_in(SpelledPitch{0, 0, 4}, 1);
    put_line(score,
             0,
             0,
             {{C4, quarter},
              {D4, Beat{1, 8}},
              {E4, Beat{1, 8}},
              {SpelledPitch{3, 1, 4}, quarter},
              {G4, quarter}});

    const auto voice = analyze_melodic(score)->per_voice_analysis.at(0);
    CHECK(voice.note_count == 5);
    CHECK(voice.chromaticism_rate == Catch::Approx(0.2f));
    CHECK(histogram_total(voice.scale_degree_distribution) == 5);
    CHECK(voice.scale_degree_distribution ==
          std::map<std::uint8_t, std::uint32_t>{{0, 1}, {1, 1}, {2, 1}, {3, 1}, {5, 1}});
}

TEST_CASE("a tied continuation counts as one sounding note without an extra interval or onset",
          "[corpus-ir][analysis][melodic][rhythmic][tie][regression]") {
    // Bar 1: C4 q, E4 q, G4 h tied; bar 2: G4 w (continuation); bar 3: E4 w.
    // Sounding notes C E G E: intervals +4 +3 -3; onsets per bar 3, 0, 1.
    auto score = make_score_in(SpelledPitch{0, 0, 4}, 3);
    put_line(score, 0, 0, {{C4, quarter}, {E4, quarter}, {G4, Beat{1, 2}, true}});
    put_line(score, 0, 1, {{G4, Beat{1, 1}}});
    put_line(score, 0, 2, {{E4, Beat{1, 1}}});

    const auto voice = analyze_melodic(score)->per_voice_analysis.at(0);
    CHECK(voice.note_count == 4);
    CHECK(voice.interval_distribution ==
          std::map<std::int8_t, std::uint32_t>{{-3, 1}, {3, 1}, {4, 1}});
    CHECK(histogram_total(voice.scale_degree_distribution) == 4);

    const auto rhythm = analyze_rhythmic(score);
    REQUIRE(rhythm);
    CHECK(rhythm->onset_density == std::vector<float>{3.0f, 0.0f, 1.0f});
    // The tied G sounds for a half plus a whole note: one 3/2 duration, not two.
    CHECK(rhythm->duration_distribution ==
          std::map<std::string, std::uint32_t>{{"quarter", 2}, {"whole", 1}, {"3/2", 1}});
}

// =============================================================================
// Exact projection and finite motif contracts (issue #27)
// =============================================================================

namespace {

void put_allocated_lane(Score& score,
                        std::size_t part,
                        std::uint32_t bar,
                        std::uint8_t voice_index,
                        const std::vector<LineNote>& line) {
    auto& voices = score.parts[part].measures[bar].voices;
    auto found = std::ranges::find(voices, voice_index, &Voice::voice_index);
    if (found == voices.end()) {
        Voice voice;
        voice.voice_index = voice_index;
        voices.push_back(voice);
        found = std::prev(voices.end());
    }
    found->events.clear();
    Beat offset = Beat::zero();
    std::uint64_t next = 1000000 + part * 100000 + voice_index * 1000 + bar * 20;
    for (const auto& item : line) {
        Note note;
        note.pitch = item.pitch;
        note.velocity = VelocityValue{std::nullopt, 80};
        note.tie_forward = item.tie_forward;
        NoteGroup group;
        group.notes = {note};
        group.duration = item.duration;
        found->events.push_back(Event{EventId{next++}, offset, group});
        offset = offset + item.duration;
    }
    REQUIRE(offset <= Beat{1, 1});
    if (offset < Beat{1, 1})
        found->events.push_back(Event{EventId{next}, offset, RestEvent{Beat{1, 1} - offset, true}});
}

} // namespace

TEST_CASE("all structural melodic lanes retain MIDI zero and their exact attack key",
          "[corpus-ir][analysis][melodic][regression]") {
    auto score = make_score_in(C4, 1);
    put_allocated_lane(score, 0, 0, 0, {{SpelledPitch{0, 0, -1}, Beat{1, 1}}});
    const SpelledPitch Fs4{3, 1, 4};
    put_allocated_lane(
        score, 0, 0, 5, {{Fs4, quarter}, {Fs4, quarter}, {Fs4, quarter}, {Fs4, quarter}});
    auto dominant = score.key_map.front();
    dominant.position = {1, Beat{1, 2}};
    dominant.key.root = G4;
    dominant.key.accidentals = 1;
    score.key_map.push_back(dominant);
    REQUIRE(is_compilable(score));

    const auto melody = analyze_melodic(score);
    REQUIRE(melody);
    REQUIRE(melody->per_voice_analysis.size() == 2);
    const auto& low = melody->per_voice_analysis[0];
    CHECK(low.voice_index == 0);
    CHECK(low.note_count == 1);
    CHECK(low.range_low == 0);
    CHECK(low.range_high == 0);
    const auto& high = melody->per_voice_analysis[1];
    CHECK(high.voice_index == 5);
    CHECK(high.note_count == 4);
    CHECK(high.chromaticism_rate == Catch::Approx(0.5f));
    CHECK(high.scale_degree_distribution == std::map<std::uint8_t, std::uint32_t>{{0, 2}, {7, 2}});
    CHECK(melody->primary_melody_voice == score.parts[0].id);
    CHECK(melody->primary_melody_voice_index == 5);
}

TEST_CASE("melodic interval inventory retains the full directed MIDI span",
          "[corpus-ir][analysis][melodic][regression]") {
    auto score = make_score_in(C4, 1);
    put_allocated_lane(score,
                       0,
                       0,
                       0,
                       {{SpelledPitch{0, 0, -1}, Beat{1, 2}}, {SpelledPitch{4, 0, 9}, Beat{1, 2}}});
    REQUIRE(is_compilable(score));
    const auto melody = analyze_melodic(score);
    REQUIRE(melody);
    CHECK(melody->per_voice_analysis.at(0).interval_distribution ==
          std::map<std::int8_t, std::uint32_t>{{127, 1}});
}

TEST_CASE("tied duplicate unisons do not conceal a lower newly attacked melody note",
          "[corpus-ir][analysis][melodic][rhythmic][tie][regression]") {
    auto score = make_score_in(C4, 1);
    auto note = [](SpelledPitch pitch, bool tie) {
        Note value;
        value.pitch = pitch;
        value.velocity = VelocityValue{std::nullopt, 80};
        value.tie_forward = tie;
        return value;
    };
    NoteGroup head;
    head.duration = Beat{1, 2};
    head.notes = {note(C4, true), note(C4, true), note(G4, true)};
    NoteGroup tail;
    tail.duration = Beat{1, 2};
    tail.notes = {note(C4, false), note(C4, false), note(G4, false), note(E4, false)};
    score.parts[0].measures[0].voices[0].events = {Event{EventId{1200001}, Beat::zero(), head},
                                                   Event{EventId{1200002}, Beat{1, 2}, tail}};
    REQUIRE(is_compilable(score));
    const auto melody = analyze_melodic(score);
    REQUIRE(melody);
    CHECK(melody->per_voice_analysis.at(0).note_count == 2);
    CHECK(melody->per_voice_analysis.at(0).interval_distribution ==
          std::map<std::int8_t, std::uint32_t>{{-3, 1}});
    const auto rhythm = analyze_rhythmic(score);
    REQUIRE(rhythm);
    CHECK(rhythm->onset_density == std::vector<float>{4.0f});
    CHECK(rhythm->duration_distribution ==
          std::map<std::string, std::uint32_t>{{"whole", 3}, {"half", 1}});
    CHECK(rhythm->rest_proportion == 0.0f);
}

TEST_CASE("literal motif occurrences preserve exact rational rhythm and all source context",
          "[corpus-ir][analysis][motivic][regression]") {
    auto score = make_score_in(C4, 2);
    put_allocated_lane(score, 0, 0, 3, {{C4, Beat{1, 8}}, {D4, quarter}, {E4, Beat{1, 8}}});
    put_allocated_lane(
        score,
        0,
        1,
        3,
        {{G4, quarter}, {SpelledPitch{5, 0, 4}, Beat{1, 2}}, {SpelledPitch{6, 0, 4}, quarter}});
    auto dominant = score.key_map.front();
    dominant.position = {2, Beat::zero()};
    dominant.key.root = G4;
    dominant.key.accidentals = 1;
    score.key_map.push_back(dominant);
    REQUIRE(is_compilable(score));
    const auto motifs = analyze_motivic(score);
    REQUIRE(motifs);
    REQUIRE(motifs->thematic_units.size() == 1);
    const auto& unit = motifs->thematic_units[0];
    CHECK(unit.intervals == std::vector<std::int8_t>{2, 2});
    CHECK(unit.rhythm == std::vector<float>{0.125f, 0.25f, 0.125f});
    REQUIRE(unit.occurrences.size() == 2);
    CHECK(unit.occurrences[0].position == ScoreTime{1, Beat::zero()});
    CHECK(unit.occurrences[0].end == ScoreTime{1, Beat{1, 2}});
    CHECK(unit.occurrences[0].part_id == score.parts[0].id);
    CHECK(unit.occurrences[0].voice_index == 3);
    CHECK(unit.occurrences[0].key == "C major");
    CHECK(unit.occurrences[1].position == ScoreTime{2, Beat::zero()});
    CHECK(unit.occurrences[1].end == ScoreTime{3, Beat::zero()});
    CHECK(unit.occurrences[1].voice_index == 3);
    CHECK(unit.occurrences[1].key == "G major");
    CHECK(unit.occurrences[1].transformation == ThematicTransformation::Augmented);
    REQUIRE(motifs->transformation_inventory.size() == 2);
    CHECK(motifs->transformation_inventory[0].transformation ==
          ThematicTransformation::TransposedExact);
    CHECK(motifs->transformation_inventory[1].transformation == ThematicTransformation::Augmented);
    CHECK(motifs->thematic_density == Catch::Approx(1.0f));
    const auto full = analyze_score(score);
    REQUIRE(full);
    CHECK(full->melodic_analysis.thematic_material.size() == 1);
    CHECK(full->evidence.at("motivic").observations == 2);
}

TEST_CASE("overlapping appearances alone are not repeated motif occurrences",
          "[corpus-ir][analysis][motivic][regression]") {
    auto score = make_score_in(C4, 1);
    put_allocated_lane(
        score, 0, 0, 0, {{C4, quarter}, {C4, quarter}, {C4, quarter}, {C4, quarter}});
    REQUIRE(is_compilable(score));
    const auto motifs = analyze_motivic(score);
    REQUIRE(motifs);
    CHECK(motifs->thematic_units.empty());
    const auto full = analyze_score(score);
    REQUIRE(full);
    CHECK(full->evidence.at("motivic").kind == AnalysisEvidenceKind::Heuristic);
    CHECK(full->evidence.at("motivic").observations == 3); // two triples and one four-note window
}

TEST_CASE("motif matching does not round duration ratios to stored floats",
          "[corpus-ir][analysis][motivic][regression]") {
    auto score = make_score_in(C4, 2);
    put_allocated_lane(score, 0, 0, 0, {{C4, quarter}, {D4, quarter}, {E4, quarter}});
    const Beat distinct{2500000001, 10000000000LL};
    CHECK(static_cast<float>(distinct.to_float()) == 0.25f);
    put_allocated_lane(score,
                       0,
                       1,
                       0,
                       {{SpelledPitch{0, 0, 5}, quarter},
                        {SpelledPitch{1, 0, 5}, distinct},
                        {SpelledPitch{2, 0, 5}, quarter}});
    REQUIRE(is_compilable(score));
    const auto motifs = analyze_motivic(score);
    REQUIRE(motifs);
    CHECK(motifs->thematic_units.empty());
}

TEST_CASE("ABACA labels classify as rondo and each section resolves its own ramp tempo",
          "[corpus-ir][analysis][formal][regression]") {
    auto score = make_score_in(C4, 5);
    const std::array labels{"A", "B", "A", "C", "A"};
    for (std::uint32_t index = 0; index < labels.size(); ++index)
        score.section_map.push_back({SectionId{100 + index},
                                     labels[index],
                                     {index + 1, Beat::zero()},
                                     {index + 2, Beat::zero()},
                                     {},
                                     std::nullopt});
    auto target = score.tempo_map.front();
    target.position = {5, Beat::zero()};
    target.bpm = PositiveRational{60, 1};
    target.transition_type = TempoTransitionType::Linear;
    target.linear_duration = Beat{4, 1};
    score.tempo_map.push_back(target);
    REQUIRE(is_compilable(score));
    const auto formal = analyze_formal(score);
    REQUIRE(formal);
    CHECK(formal->form_type == FormClassification::Rondo);
    REQUIRE(formal->section_plan.size() == 5);
    CHECK(formal->section_plan[0].tempo == 120.0f);
    CHECK(formal->section_plan[1].tempo == 105.0f);
    CHECK(formal->section_plan[2].tempo == 90.0f);
    CHECK(formal->section_plan[3].tempo == 75.0f);
    CHECK(formal->section_plan[4].tempo == 60.0f);
}

TEST_CASE("intra-bar voice leading observes actual simultaneous lane changes",
          "[corpus-ir][analysis][voice-leading][regression]") {
    auto score = make_two_part_score();
    put_line(score, 0, 0, {{C4, Beat{1, 2}}, {D4, Beat{1, 2}}});
    put_line(
        score, 1, 0, {{SpelledPitch{3, 0, 3}, Beat{1, 2}}, {SpelledPitch{4, 0, 3}, Beat{1, 2}}});
    REQUIRE(is_compilable(score));
    const auto motion = analyze_voice_leading(score);
    REQUIRE(motion);
    CHECK(motion->parallel_fifths_count == 1);
    CHECK(motion->parallel_octaves_count == 0);
    CHECK(motion->parallel_motion_proportion == 1.0f);
    CHECK(motion->spacing_distribution == std::map<std::int8_t, std::uint32_t>{{7, 2}});
}

TEST_CASE("projection errors propagate from every projection-based corpus domain",
          "[corpus-ir][analysis][tie][regression]") {
    auto score = make_score_in(C4, 2);
    put_allocated_lane(score, 0, 0, 0, {{C4, Beat{1, 1}, true}});
    put_allocated_lane(score, 0, 1, 0, {{D4, Beat{1, 1}}});
    CHECK_FALSE(is_compilable(score));
    CHECK_FALSE(analyze_melodic(score));
    CHECK_FALSE(analyze_rhythmic(score));
    CHECK_FALSE(analyze_motivic(score));
    CHECK_FALSE(analyze_voice_leading(score));
    CHECK_FALSE(analyze_score(score));
}

TEST_CASE("full analysis qualifies absent observations and unimplemented fields",
          "[corpus-ir][analysis][evidence][regression]") {
    const auto analysis = analyze_score(make_basic_score());
    REQUIRE(analysis);
    REQUIRE(analysis->evidence.size() == 9);
    for (const auto* domain :
         {"harmonic", "melodic", "voice_leading", "dynamic", "orchestration", "motivic"}) {
        const auto& evidence = analysis->evidence.at(domain);
        CHECK(evidence.kind == AnalysisEvidenceKind::Unavailable);
        CHECK(evidence.observations == 0);
        REQUIRE(evidence.unavailable_reason);
        CHECK_FALSE(evidence.unavailable_reason->empty());
        CHECK_FALSE(evidence.method.empty());
    }
    for (const auto* domain : {"rhythmic", "formal", "textural"}) {
        const auto& evidence = analysis->evidence.at(domain);
        CHECK(evidence.kind == AnalysisEvidenceKind::Heuristic);
        CHECK(evidence.observations == 4);
        CHECK_FALSE(evidence.unavailable_reason);
    }
    CHECK(analysis->textural_analysis.texture_type_proportions.empty());
    CHECK(analysis->dynamic_analysis.dynamic_by_section.empty());
}

namespace {

Score make_metrical_attack(TimeSignature metre, Beat offset, Beat duration) {
    auto score = make_score_in(C4, 1);
    score.time_map[0].time_signature = metre;
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();
    if (offset > Beat::zero())
        events.push_back(Event{EventId{1300001}, Beat::zero(), RestEvent{offset, true}});
    Note note;
    note.pitch = C4;
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup group;
    group.notes = {note};
    group.duration = duration;
    events.push_back(Event{EventId{1300002}, offset, group});
    const Beat end = offset + duration;
    REQUIRE(end <= metre.measure_duration());
    if (end < metre.measure_duration())
        events.push_back(
            Event{EventId{1300003}, end, RestEvent{metre.measure_duration() - end, true}});
    REQUIRE(is_compilable(score));
    return score;
}

} // namespace

TEST_CASE("simple metre syncopation requires strict sustain past a stronger boundary",
          "[corpus-ir][analysis][rhythmic][syncopation][regression]") {
    const TimeSignature four_four{{1, 1, 1, 1}, 4};
    const auto short_note =
        analyze_rhythmic(make_metrical_attack(four_four, Beat{1, 8}, Beat{1, 8}));
    REQUIRE(short_note);
    CHECK(short_note->syncopation_index == 0.0f); // ending exactly at quarter boundary
    const auto held_note =
        analyze_rhythmic(make_metrical_attack(four_four, Beat{1, 8}, Beat{1, 4}));
    REQUIRE(held_note);
    CHECK(held_note->syncopation_index == 1.0f); // sounding beyond quarter boundary
    const auto downbeat =
        analyze_rhythmic(make_metrical_attack(four_four, Beat::zero(), Beat{1, 1}));
    REQUIRE(downbeat);
    CHECK(downbeat->syncopation_index == 0.0f); // no stronger boundary than its attack
}

TEST_CASE("compound metre distinguishes interior denominator pulses from grouped beats",
          "[corpus-ir][analysis][rhythmic][syncopation][regression]") {
    const TimeSignature six_eight{{3, 3}, 8};
    const auto interior = analyze_rhythmic(make_metrical_attack(six_eight, Beat{1, 8}, Beat{1, 8}));
    REQUIRE(interior);
    CHECK(interior->syncopation_index == 0.0f); // next interior pulse has equal strength
    const auto exact_end =
        analyze_rhythmic(make_metrical_attack(six_eight, Beat{1, 8}, Beat{1, 4}));
    REQUIRE(exact_end);
    CHECK(exact_end->syncopation_index == 0.0f); // ends exactly at stronger 3/8 group start
    const auto held = analyze_rhythmic(make_metrical_attack(six_eight, Beat{1, 8}, Beat{3, 8}));
    REQUIRE(held);
    CHECK(held->syncopation_index == 1.0f);
}

TEST_CASE("additive metre retains its explicit group boundaries in syncopation",
          "[corpus-ir][analysis][rhythmic][syncopation][regression]") {
    const TimeSignature seven_eight{{2, 2, 3}, 8};
    const auto exact_end =
        analyze_rhythmic(make_metrical_attack(seven_eight, Beat{3, 8}, Beat{1, 8}));
    REQUIRE(exact_end);
    CHECK(exact_end->syncopation_index == 0.0f); // end is exactly second group's boundary, 4/8
    const auto held = analyze_rhythmic(make_metrical_attack(seven_eight, Beat{3, 8}, Beat{1, 4}));
    REQUIRE(held);
    CHECK(held->syncopation_index == 1.0f);
    CHECK(held->metre_distribution == std::map<std::string, std::uint32_t>{{"2+2+3/8", 1}});
}

TEST_CASE("syncopation folds tied sustain across a stronger bar downbeat once",
          "[corpus-ir][analysis][rhythmic][syncopation][tie][regression]") {
    auto score = make_score_in(C4, 2);
    put_note(score, 0, 0, C4, Beat{3, 4}, quarter);
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[1].payload)
        .notes[0]
        .tie_forward = true;
    put_note(score, 0, 1, C4, Beat::zero(), quarter);
    REQUIRE(is_compilable(score));
    const auto tied = analyze_rhythmic(score);
    REQUIRE(tied);
    CHECK(tied->syncopation_index == 1.0f);
    CHECK(tied->onset_density == std::vector<float>{1.0f, 0.0f});
    CHECK(tied->duration_distribution == std::map<std::string, std::uint32_t>{{"half", 1}});
    // Without the continuation, the same head ends exactly at the barline.
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[1].payload)
        .notes[0]
        .tie_forward = false;
    score.parts[0].measures[1].voices[0].events = {
        Event{EventId{1300100}, Beat::zero(), RestEvent{Beat{1, 1}, true}}};
    REQUIRE(is_compilable(score));
    const auto untied = analyze_rhythmic(score);
    REQUIRE(untied);
    CHECK(untied->syncopation_index == 0.0f);
}

TEST_CASE("allocated grace notes are excluded from metrical sustain eligibility",
          "[corpus-ir][analysis][rhythmic][syncopation][grace][regression]") {
    auto score = make_metrical_attack(TimeSignature{{1, 1, 1, 1}, 4}, Beat{1, 8}, Beat{1, 4});
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[1].payload);
    group.notes[0].grace = GraceType::Acciaccatura;
    REQUIRE(is_compilable(score));
    const auto rhythm = analyze_rhythmic(score);
    REQUIRE(rhythm);
    CHECK(rhythm->syncopation_index == 0.0f);
    CHECK(rhythm->onset_density == std::vector<float>{1.0f}); // structural attack still exists
}

TEST_CASE("voice leading retains same-part lanes, oblique motion and true crossings",
          "[corpus-ir][analysis][voice-leading][regression]") {
    auto score = make_score_in(C4, 1);
    put_allocated_lane(score, 0, 0, 0, {{C4, Beat{1, 2}}, {G4, Beat{1, 2}}});
    put_allocated_lane(score, 0, 0, 5, {{E4, Beat{1, 2}}, {D4, Beat{1, 2}}});
    REQUIRE(is_compilable(score));
    const auto crossing = analyze_voice_leading(score);
    REQUIRE(crossing);
    CHECK(crossing->voice_crossing_count == 1);
    CHECK(crossing->contrary_motion_proportion == 1.0f);
    put_allocated_lane(score, 0, 0, 5, {{E4, Beat{1, 1}}});
    REQUIRE(is_compilable(score));
    const auto oblique = analyze_voice_leading(score);
    REQUIRE(oblique);
    CHECK(oblique->oblique_motion_proportion == 1.0f);
    CHECK(oblique->parallel_fifths_count == 0);
}

TEST_CASE("nonoverlapping parts do not fabricate pair-motion or spacing samples",
          "[corpus-ir][analysis][voice-leading][evidence][regression]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), quarter);
    put_note(score, 1, 0, C3, Beat{1, 2}, quarter);
    REQUIRE(is_compilable(score));
    const auto analysis = analyze_score(score);
    REQUIRE(analysis);
    const auto& evidence = analysis->evidence.at("voice_leading");
    // Successive sounding pitch sets {G4}, {C3} compare one prior pitch and
    // retain none; this does not imply any simultaneous lane-pair observation.
    CHECK(evidence.kind == AnalysisEvidenceKind::Heuristic);
    CHECK(evidence.observations == 1);
    CHECK(analysis->voice_leading_analysis.common_tone_retention_rate == 0.0f);
    CHECK(std::ranges::find(evidence.unavailable_fields, "common_tone_retention_rate") ==
          evidence.unavailable_fields.end());
    for (const auto field : {"contrary_motion_proportion",
                             "similar_motion_proportion",
                             "oblique_motion_proportion",
                             "parallel_motion_proportion",
                             "parallel_fifths_count",
                             "parallel_octaves_count",
                             "voice_crossing_count",
                             "spacing_distribution"})
        CHECK(std::ranges::find(evidence.unavailable_fields, field) !=
              evidence.unavailable_fields.end());
    CHECK(analysis->voice_leading_analysis.spacing_distribution.empty());
}

TEST_CASE("motif windows fold individual tied durations and retain their exact final end",
          "[corpus-ir][analysis][motivic][tie][regression]") {
    auto score = make_score_in(C4, 4);
    for (std::uint32_t bar : {0U, 2U}) {
        put_allocated_lane(
            score, 0, bar, 4, {{C4, quarter}, {D4, quarter}, {E4, Beat{1, 2}, true}});
        put_allocated_lane(score, 0, bar + 1, 4, {{E4, quarter}});
    }
    REQUIRE(is_compilable(score));
    const auto motifs = analyze_motivic(score);
    REQUIRE(motifs);
    REQUIRE(motifs->thematic_units.size() == 1);
    const auto& motif = motifs->thematic_units[0];
    CHECK(motif.rhythm == std::vector<float>{0.25f, 0.25f, 0.75f});
    REQUIRE(motif.occurrences.size() == 2);
    CHECK(motif.occurrences[0].voice_index == 4);
    CHECK(motif.occurrences[0].end == ScoreTime{2, quarter});
    CHECK(motif.occurrences[1].position == ScoreTime{3, Beat::zero()});
    CHECK(motif.occurrences[1].end == ScoreTime{4, quarter});
    const auto melody = analyze_melodic(score);
    REQUIRE(melody);
    CHECK(melody->per_voice_analysis[1].note_count == 6);
}
