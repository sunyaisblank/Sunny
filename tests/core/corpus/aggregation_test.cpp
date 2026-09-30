/**
 * @file aggregation_test.cpp
 * @brief Unit tests for Corpus IR aggregation completeness
 *
 *
 *        orchestration, and motivic domains)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sunny/core/corpus/workflows.hpp>

using namespace sunny::core;

namespace {

/// Build a minimal work with populated analysis fields.
IngestedWork make_analyzed_work(IngestedWorkId id,
                                float avg_density,
                                float avg_span,
                                float thematic_economy,
                                float thematic_density,
                                const std::string& dynamic_low,
                                const std::string& dynamic_high,
                                float dynamic_change_rate,
                                std::uint32_t subito_count) {
    IngestedWork work;
    work.id = id;
    work.analysis_complete = true;

    work.analysis.textural_analysis.average_density = avg_density;
    work.analysis.textural_analysis.average_register_span = avg_span;
    work.analysis.textural_analysis.density_curve.push_back(
        {ScoreTime{1, Beat::zero()}, static_cast<std::uint8_t>(avg_density)});

    work.analysis.dynamic_analysis.dynamic_range_low = dynamic_low;
    work.analysis.dynamic_analysis.dynamic_range_high = dynamic_high;
    work.analysis.dynamic_analysis.dynamic_change_rate = dynamic_change_rate;
    work.analysis.dynamic_analysis.subito_dynamics_count = subito_count;
    work.analysis.dynamic_analysis.dynamic_distribution[dynamic_low] = 5;
    work.analysis.dynamic_analysis.dynamic_distribution[dynamic_high] = 10;

    work.analysis.motivic_analysis.thematic_economy = thematic_economy;
    work.analysis.motivic_analysis.thematic_density = thematic_density;

    return work;
}

} // anonymous namespace

// =============================================================================
// Textural aggregation
// =============================================================================

TEST_CASE("Textural aggregation averages density and span", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 2.0f, 24.0f, 0, 0, "p", "f", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 4.0f, 36.0f, 0, 0, "pp", "ff", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    auto r = rebuild_style_profile(db, ComposerProfileId{1});
    REQUIRE(r.has_value());

    const auto& tp = db.composers[1].style_profile.textural_profile;
    CHECK_THAT(tp.average_density, Catch::Matchers::WithinAbs(3.0, 0.01));
    CHECK_THAT(tp.register_span_preference, Catch::Matchers::WithinAbs(30.0, 0.01));
}

// =============================================================================
// Dynamic aggregation
// =============================================================================

TEST_CASE("Dynamic aggregation finds extremes and most frequent", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0, 0, "pp", "f", 0.5f, 2);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0, 0, "p", "ff", 0.3f, 1);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)rebuild_style_profile(db, ComposerProfileId{1});

    const auto& dp = db.composers[1].style_profile.dynamic_profile;
    CHECK(dp.dynamic_range_low == "pp");
    CHECK(dp.dynamic_range_high == "ff");
    CHECK_THAT(dp.dynamic_change_rate, Catch::Matchers::WithinAbs(0.4, 0.01));
    CHECK_THAT(dp.subito_frequency, Catch::Matchers::WithinAbs(1.5, 0.01));
    CHECK_FALSE(dp.most_frequent_dynamic.empty());
}

// =============================================================================
// Orchestration aggregation
// =============================================================================

TEST_CASE("Orchestration aggregation merges instrument maps", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    IngestedWork w1;
    w1.id = IngestedWorkId{1};
    w1.analysis_complete = true;
    w1.analysis.orchestration_analysis = OrchestrationAnalysisRecord{};
    w1.analysis.orchestration_analysis->instrument_usage = {{"Violin", 0.8f}, {"Cello", 0.6f}};
    w1.analysis.orchestration_analysis->melody_carrier_distribution = {{"Violin", 0.9f},
                                                                       {"Cello", 0.1f}};

    IngestedWork w2;
    w2.id = IngestedWorkId{2};
    w2.analysis_complete = true;
    w2.analysis.orchestration_analysis = OrchestrationAnalysisRecord{};
    w2.analysis.orchestration_analysis->instrument_usage = {{"Violin", 1.0f}, {"Viola", 0.5f}};
    w2.analysis.orchestration_analysis->melody_carrier_distribution = {{"Violin", 0.7f},
                                                                       {"Viola", 0.3f}};

    db.works[1] = w1;
    db.works[2] = w2;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)rebuild_style_profile(db, ComposerProfileId{1});

    const auto& sp = db.composers[1].style_profile;
    REQUIRE(sp.orchestration_profile.has_value());
    const auto& op = *sp.orchestration_profile;
    CHECK(op.preferred_instruments.count("Violin") > 0);
    CHECK_THAT(op.preferred_instruments.at("Violin"), Catch::Matchers::WithinAbs(0.9, 0.01));
}

// =============================================================================
// Motivic aggregation
// =============================================================================

TEST_CASE("Motivic aggregation averages economy and density", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0.6f, 0.4f, "p", "f", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0.8f, 0.2f, "p", "f", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)rebuild_style_profile(db, ComposerProfileId{1});

    const auto& mp = db.composers[1].style_profile.motivic_profile;
    CHECK_THAT(mp.thematic_economy, Catch::Matchers::WithinAbs(0.7, 0.01));
    CHECK_THAT(mp.development_density, Catch::Matchers::WithinAbs(0.3, 0.01));
}

// =============================================================================
// Full 9-domain rebuild
// =============================================================================

TEST_CASE("rebuild_style_profile populates all sub-profiles", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 3.0f, 24.0f, 0.5f, 0.3f, "p", "ff", 0.4f, 1);
    w1.analysis.harmonic_analysis.chord_vocabulary = {{"I", 20}, {"V", 15}};
    w1.analysis.harmonic_analysis.harmonic_rhythm.mean_rate = 2.0f;
    w1.analysis.formal_analysis.total_duration_bars = 32;
    w1.analysis.formal_analysis.form_type = FormClassification::Ternary;
    w1.analysis.voice_leading_analysis.common_tone_retention_rate = 0.6f;
    w1.analysis.voice_leading_analysis.average_voice_independence = 0.5f;
    w1.analysis.rhythmic_analysis.syncopation_index = 0.1f;
    w1.analysis.rhythmic_analysis.metrical_complexity = 0.2f;

    VoiceMelodicAnalysis vma;
    vma.conjunct_proportion = 0.7f;
    vma.chromaticism_rate = 0.1f;
    w1.analysis.melodic_analysis.per_voice_analysis.push_back(vma);

    db.works[1] = w1;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});

    (void)rebuild_style_profile(db, ComposerProfileId{1});

    const auto& sp = db.composers[1].style_profile;
    CHECK(sp.sample_size == 1);
    CHECK(sp.confidence > 0.0f);
    CHECK(sp.harmonic_profile.chord_vocabulary_size == 2);
    CHECK(sp.melodic_profile.conjunct_proportion > 0.0f);
    CHECK(sp.rhythmic_profile.syncopation_index > 0.0f);
    CHECK(sp.formal_profile.average_work_length > 0.0f);
    CHECK(sp.voice_leading_profile.common_tone_retention > 0.0f);
    CHECK(sp.textural_profile.average_density > 0.0f);
    CHECK_FALSE(sp.dynamic_profile.dynamic_range_low.empty());
    CHECK(sp.motivic_profile.thematic_economy > 0.0f);
}

TEST_CASE("multi-work aggregation deterministically derives every supported style dimension",
          "[corpus-ir][aggregation][adversarial]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Test");

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 2.0f, 24.0f, 0.6f, 0.4f, "pp", "f", 0.2f, 1);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 4.0f, 36.0f, 0.8f, 0.2f, "p", "ff", 0.4f, 3);

    w1.analysis.harmonic_analysis.chord_vocabulary = {{"I", 3}, {"V", 1}};
    w2.analysis.harmonic_analysis.chord_vocabulary = {{"I", 1}, {"ii", 1}};
    w1.analysis.harmonic_analysis.progression_inventory.push_back(
        {{"I", "V"}, 2, {{1, Beat::zero()}, {3, Beat::zero()}}, "opening"});
    w2.analysis.harmonic_analysis.progression_inventory.push_back(
        {{"I", "V"}, 2, {{8, Beat::zero()}}, "closing"});
    w1.analysis.harmonic_analysis.modulation_inventory.push_back(
        {{4, Beat::zero()}, "C major", "G major", ModulationTechnique::PivotChord, "D minor"});
    w2.analysis.harmonic_analysis.modulation_inventory.push_back(
        {{4, Beat::zero()},
         "C major",
         "F major",
         ModulationTechnique::DirectModulation,
         std::nullopt});
    w2.analysis.harmonic_analysis.modulation_inventory.push_back(
        {{8, Beat::zero()},
         "F major",
         "C major",
         ModulationTechnique::DirectModulation,
         std::nullopt});
    w1.analysis.harmonic_analysis.tonal_plan.key_sequence = {{"C major", "tonic", 1},
                                                             {"G major", "perfect_fifth", 4}};
    w2.analysis.harmonic_analysis.tonal_plan.key_sequence = {{"C major", "tonic", 1},
                                                             {"F major", "perfect_fourth", 4}};
    w1.analysis.harmonic_analysis.chromatic_techniques = {
        {{2, Beat::zero()}, "secondary dominant", ""}, {{3, Beat::zero()}, "augmented sixth", ""}};
    w2.analysis.harmonic_analysis.chromatic_techniques = {{{2, Beat::zero()}, "Neapolitan", ""}};
    w1.analysis.harmonic_analysis.cadence_inventory = {{{4, Beat::zero()}, "PAC", {}, "", true},
                                                       {{8, Beat::zero()}, "DC", {}, "", false}};
    w2.analysis.harmonic_analysis.cadence_inventory = {{{8, Beat::zero()}, "IAC", {}, "", true}};
    w1.analysis.harmonic_analysis.harmonic_rhythm = {{}, 2.0f, 0.25f, {}};
    w2.analysis.harmonic_analysis.harmonic_rhythm = {{}, 4.0f, 0.75f, {}};

    VoiceMelodicAnalysis melody1;
    melody1.note_count = 5;
    melody1.range_low = 60;
    melody1.range_high = 72;
    melody1.interval_distribution = {{-2, 1}, {2, 3}};
    melody1.contour_inventory.push_back(
        {{1, Beat::zero()}, {4, Beat::zero()}, ContourShape::Arch, 12, 4.0f, 0.5f, 0.0f});
    melody1.scale_degree_distribution = {{1, 2}};
    melody1.conjunct_proportion = 0.8f;
    melody1.chromaticism_rate = 0.1f;
    VoiceMelodicAnalysis melody2;
    melody2.note_count = 2;
    melody2.range_low = 48;
    melody2.range_high = 72;
    melody2.interval_distribution = {{2, 1}};
    melody2.contour_inventory.push_back(
        {{1, Beat::zero()}, {4, Beat::zero()}, ContourShape::Descending, 24, 4.0f, 0.0f, 1.0f});
    melody2.scale_degree_distribution = {{5, 2}};
    melody2.conjunct_proportion = 0.4f;
    melody2.chromaticism_rate = 0.3f;
    w1.analysis.melodic_analysis.per_voice_analysis = {melody1, VoiceMelodicAnalysis{}};
    w2.analysis.melodic_analysis.per_voice_analysis = {melody2};
    ThematicUnit theme1;
    theme1.occurrences = {
        {{1, Beat::zero()}, PartId{1}, ThematicTransformation::SequentialRepetition, "C major"},
        {{2, Beat::zero()}, PartId{1}, ThematicTransformation::Original, "C major"}};
    ThematicUnit theme2;
    theme2.occurrences = {
        {{1, Beat::zero()}, PartId{1}, ThematicTransformation::SequentialRepetition, "C major"}};
    w1.analysis.melodic_analysis.thematic_material = {theme1};
    w2.analysis.melodic_analysis.thematic_material = {theme2};

    w1.analysis.rhythmic_analysis.duration_distribution = {{"quarter", 3}, {"eighth", 1}};
    w2.analysis.rhythmic_analysis.duration_distribution = {{"eighth", 2}};
    w1.analysis.rhythmic_analysis.metre_distribution = {{"4/4", 8}};
    w2.analysis.rhythmic_analysis.metre_distribution = {{"3+3+2/8", 4}};
    w1.analysis.rhythmic_analysis.tempo_profile = {{{1, Beat::zero()}, 60.0f}};
    w2.analysis.rhythmic_analysis.tempo_profile = {{{1, Beat::zero()}, 100.0f}};
    w1.analysis.rhythmic_analysis.syncopation_index = 0.2f;
    w2.analysis.rhythmic_analysis.syncopation_index = 0.4f;
    w1.analysis.rhythmic_analysis.metrical_complexity = 0.3f;
    w2.analysis.rhythmic_analysis.metrical_complexity = 0.5f;
    w1.analysis.rhythmic_analysis.rubato_degree = 0.1f;
    w2.analysis.rhythmic_analysis.rubato_degree = 0.3f;
    w1.analysis.rhythmic_analysis.rhythmic_motifs.push_back({{}, 2, {}});
    w2.analysis.rhythmic_analysis.rhythmic_motifs.push_back({{}, 1, {}});

    w1.analysis.formal_analysis.total_duration_bars = 100;
    w2.analysis.formal_analysis.total_duration_bars = 200;
    w1.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;
    w2.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;
    w1.analysis.formal_analysis.proportions = {
        {"Exposition", 0.4f, 0.0f}, {"Development", 0.2f, 0.0f}, {"Recapitulation", 0.3f, 0.0f}};
    w2.analysis.formal_analysis.proportions = {{"Exposition", 0.5f, 0.0f},
                                               {"Recapitulation", 0.5f, 0.0f}};
    w1.analysis.formal_analysis.section_plan = {
        {"Introduction", 1, 3, 2, "C major", 60.0f, std::nullopt, {}},
        {"Exposition", 3, 50, 47, "C major", 120.0f, std::nullopt, {}},
        {"Transition", 50, 60, 10, "G major", 120.0f, "sequence", {}},
        {"Coda", 90, 101, 11, "C major", 120.0f, std::nullopt, {}}};
    w2.analysis.formal_analysis.section_plan = {
        {"Exposition", 1, 100, 99, "C major", 100.0f, std::nullopt, {}},
        {"Recapitulation", 100, 201, 101, "C major", 100.0f, std::nullopt, {}}};

    w1.analysis.voice_leading_analysis = {2, 1, 0.5f, 0.2f, 0.2f, 0.1f, 2, 0.8f, 0.6f, {}, {}};
    w2.analysis.voice_leading_analysis = {0, 1, 0.3f, 0.2f, 0.3f, 0.2f, 4, 0.6f, 0.4f, {}, {}};
    w1.analysis.voice_leading_analysis.resolution_patterns = {{"leading tone", "tonic", 3, 1.0f},
                                                              {"chordal seventh", "down", 2, 0.5f}};
    w2.analysis.voice_leading_analysis.resolution_patterns = {{"leading tone", "tonic", 1, 0.0f},
                                                              {"7th", "down", 1, 1.0f}};
    w1.analysis.voice_leading_analysis.spacing_distribution = {{7, 6}, {19, 2}};
    w2.analysis.voice_leading_analysis.spacing_distribution = {{19, 2}};

    w1.analysis.textural_analysis.density_curve = {
        {{1, Beat::zero()}, 1}, {{2, Beat::zero()}, 2}, {{3, Beat::zero()}, 3}};
    w2.analysis.textural_analysis.density_curve = {
        {{1, Beat::zero()}, 3}, {{2, Beat::zero()}, 2}, {{3, Beat::zero()}, 1}};
    w1.analysis.textural_analysis.texture_type_proportions = {{"homophonic", 1.0f}};
    w2.analysis.textural_analysis.texture_type_proportions = {{"polyphonic", 1.0f}};
    w1.analysis.dynamic_analysis.dynamic_shape = {
        {{1, Beat::zero()}, 0.05f}, {{2, Beat::zero()}, 0.45f}, {{3, Beat::zero()}, 0.85f}};
    w2.analysis.dynamic_analysis.dynamic_shape = {
        {{1, Beat::zero()}, 0.05f}, {{2, Beat::zero()}, 0.45f}, {{3, Beat::zero()}, 0.85f}};
    w1.analysis.dynamic_analysis.climax_position = 0.5f;
    w2.analysis.dynamic_analysis.climax_position = 0.75f;
    w1.analysis.dynamic_analysis.dynamic_distribution = {{"p", 1}, {"f", 5}};
    w2.analysis.dynamic_analysis.dynamic_distribution = {{"p", 4}, {"f", 2}};

    OrchestrationAnalysisRecord orchestration1;
    orchestration1.instrument_usage = {{"Violin", 0.8f}};
    orchestration1.melody_carrier_distribution = {{"Violin", 0.9f}};
    orchestration1.instrument_combinations.push_back(
        {{"Violin", "Flute"}, 2, "tutti", std::nullopt});
    orchestration1.doubling_patterns.push_back({"Cello", "Bassoon", -12, 2});
    orchestration1.orchestral_crescendo_patterns.push_back(
        {{1, Beat::zero()},
         {3, Beat::zero()},
         {{"Cello", {1, Beat::zero()}}, {"Flute", {2, Beat::zero()}}},
         true});
    OrchestrationAnalysisRecord orchestration2;
    orchestration2.instrument_usage = {{"Violin", 1.0f}};
    orchestration2.melody_carrier_distribution = {{"Violin", 0.7f}};
    orchestration2.instrument_combinations.push_back(
        {{"Flute", "Violin"}, 3, "tutti", std::nullopt});
    orchestration2.doubling_patterns.push_back({"Cello", "Bassoon", -12, 3});
    w1.analysis.orchestration_analysis = orchestration1;
    w2.analysis.orchestration_analysis = orchestration2;

    w1.analysis.motivic_analysis.transformation_inventory = {
        {{}, {1, Beat::zero()}, ThematicTransformation::Fragmented, ""},
        {{}, {2, Beat::zero()}, ThematicTransformation::SequentialRepetition, ""}};
    w2.analysis.motivic_analysis.transformation_inventory = {
        {{}, {1, Beat::zero()}, ThematicTransformation::Fragmented, ""},
        {{}, {2, Beat::zero()}, ThematicTransformation::Original, ""}};

    db.works[1] = std::move(w1);
    db.works[2] = std::move(w2);
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1}).has_value());
    REQUIRE(rebuild_style_profile(db, ComposerProfileId{1}).has_value());

    const auto& style = db.composers.at(1).style_profile;
    CHECK_THAT(style.harmonic_profile.chord_frequency.at("I"),
               Catch::Matchers::WithinAbs(4.0 / 6.0, 0.001));
    CHECK(style.harmonic_profile.preferred_progressions[0].contexts ==
          std::vector<std::string>{"closing", "opening"});
    CHECK_THAT(style.harmonic_profile.modulation_frequency, Catch::Matchers::WithinAbs(1.5, 0.001));
    CHECK_THAT(style.harmonic_profile.chromatic_density, Catch::Matchers::WithinAbs(0.5, 0.001));
    CHECK(style.harmonic_profile.preferred_key_signatures.at("C major") == 2);
    CHECK_THAT(style.harmonic_profile.deceptive_cadence_frequency,
               Catch::Matchers::WithinAbs(0.5, 0.001));

    CHECK(style.melodic_profile.preferred_intervals == std::vector<std::int8_t>{2, -2});
    CHECK_THAT(style.melodic_profile.typical_range, Catch::Matchers::WithinAbs(18.0, 0.001));
    CHECK_THAT(style.melodic_profile.sequence_frequency,
               Catch::Matchers::WithinAbs(2.0 / 3.0, 0.001));
    CHECK_THAT(style.rhythmic_profile.rhythmic_variety, Catch::Matchers::WithinAbs(1.0, 0.001));
    CHECK_THAT(style.rhythmic_profile.tempo_mean, Catch::Matchers::WithinAbs(80.0, 0.001));
    CHECK_THAT(style.rhythmic_profile.tempo_stddev, Catch::Matchers::WithinAbs(20.0, 0.001));
    CHECK_THAT(style.rhythmic_profile.rhythmic_motif_consistency,
               Catch::Matchers::WithinAbs(0.5, 0.001));

    CHECK_THAT(style.formal_profile.average_work_length, Catch::Matchers::WithinAbs(150.0, 0.001));
    REQUIRE(style.formal_profile.exposition_recapitulation_ratio.has_value());
    CHECK_THAT(*style.formal_profile.exposition_recapitulation_ratio,
               Catch::Matchers::WithinAbs(0.875, 0.001));
    CHECK_THAT(style.formal_profile.introduction_frequency, Catch::Matchers::WithinAbs(0.5, 0.001));
    CHECK_THAT(style.formal_profile.coda_frequency, Catch::Matchers::WithinAbs(0.5, 0.001));
    CHECK(style.formal_profile.transition_technique == std::vector<std::string>{"sequence"});

    CHECK_THAT(style.voice_leading_profile.parallel_fifths_tolerance,
               Catch::Matchers::WithinAbs(1.0, 0.001));
    CHECK(style.voice_leading_profile.preferred_motion_type == "contrary");
    CHECK_THAT(style.voice_leading_profile.leading_tone_resolution_rate,
               Catch::Matchers::WithinAbs(0.75, 0.001));
    CHECK_THAT(style.voice_leading_profile.seventh_resolution_rate,
               Catch::Matchers::WithinAbs(2.0 / 3.0, 0.001));
    CHECK(style.voice_leading_profile.spacing_preference == "close");
    CHECK_THAT(style.voice_leading_profile.voice_crossing_tolerance,
               Catch::Matchers::WithinAbs(3.0, 0.001));

    CHECK_THAT(style.textural_profile.density_dynamic_correlation,
               Catch::Matchers::WithinAbs(0.0, 0.001));
    CHECK(style.dynamic_profile.climax_dynamic == "ff");
    CHECK(style.dynamic_profile.dynamic_arc_shape == ContourShape::Ascending);
    REQUIRE(style.orchestration_profile.has_value());
    CHECK(style.orchestration_profile->signature_combinations[0].frequency == 5);
    CHECK(style.orchestration_profile->doubling_preferences[0].frequency == 5);
    CHECK(style.orchestration_profile->build_up_technique ==
          std::vector<std::string>{"instrument accretion", "register expansion"});
    CHECK_THAT(style.motivic_profile.fragmentation_frequency,
               Catch::Matchers::WithinAbs(0.5, 0.001));
    CHECK_THAT(style.motivic_profile.sequence_frequency, Catch::Matchers::WithinAbs(0.25, 0.001));

    // These fields have no evidence carrier in WorkAnalysis and therefore remain explicitly
    // neutral rather than being fabricated by the deterministic rebuild.
    CHECK(style.harmonic_profile.tonal_ambiguity_index == 0.0f);
    CHECK(style.melodic_profile.average_phrase_length == 0.0f);
    CHECK(style.melodic_profile.phrase_length_variance == 0.0f);
    CHECK(style.melodic_profile.ornament_density == 0.0f);
    CHECK_FALSE(style.melodic_profile.leitmotif_usage);
    CHECK(style.orchestration_profile->tutti_proportion == 0.0f);
    CHECK(style.orchestration_profile->solo_proportion == 0.0f);
    CHECK(style.orchestration_profile->colour_signature.empty());
    CHECK_FALSE(style.motivic_profile.cross_movement_thematic_links);
}

// =============================================================================
// Motivic: zeros average correctly
// =============================================================================

TEST_CASE("Motivic zeros average to zero", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0.0f, 0.0f, "mf", "mf", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0.0f, 0.0f, "mf", "mf", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)rebuild_style_profile(db, ComposerProfileId{1});

    const auto& mp = db.composers[1].style_profile.motivic_profile;
    CHECK_THAT(mp.thematic_economy, Catch::Matchers::WithinAbs(0.0, 0.001));
    CHECK_THAT(mp.development_density, Catch::Matchers::WithinAbs(0.0, 0.001));
}
