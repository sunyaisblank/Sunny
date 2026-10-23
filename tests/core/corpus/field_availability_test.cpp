/** Per-field denominators preserve observed zeros and exclude unavailable defaults. */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/corpus/analysis.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/workflows.hpp>

using namespace sunny::core;

namespace {
Score field_score(std::size_t parts = 1, std::uint32_t bars = 1) {
    ScoreSpec spec;
    spec.id = ScoreId{1};
    spec.title = "Literal field evidence";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    for (std::size_t index = 0; index < parts; ++index) {
        PartDefinition definition;
        definition.name = "Piano " + std::to_string(index + 1);
        definition.instrument_type = InstrumentType::Piano;
        definition.clef = Clef::Treble;
        definition.rendering.midi_channel = static_cast<std::uint8_t>(index + 1);
        spec.parts.push_back(definition);
    }
    auto score = create_score(spec);
    REQUIRE(score);
    return *score;
}

void field_notes(Score& score,
                 std::size_t part,
                 const std::vector<SpelledPitch>& pitches,
                 bool grace = false,
                 std::size_t bar = 0) {
    auto& voice = score.parts[part].measures[bar].voices[0];
    voice.events.clear();
    const Beat duration{1, static_cast<std::int64_t>(pitches.size())};
    Beat offset = Beat::zero();
    std::uint64_t id = 1000 + part * 100 + bar * 1000;
    for (const auto pitch : pitches) {
        Note note;
        note.pitch = pitch;
        note.velocity = VelocityValue{std::nullopt, 80};
        if (grace) note.grace = GraceType::Acciaccatura;
        NoteGroup group;
        group.duration = duration;
        group.notes = {note};
        voice.events.push_back({EventId{id++}, offset, group});
        offset = offset + duration;
    }
    REQUIRE(is_compilable(score));
}

WorkAnalysis field_analysis(const Score& score) {
    auto result = analyze_score(score);
    REQUIRE(result);
    return *result;
}

CorpusDatabase field_corpus(const std::vector<WorkAnalysis>& analyses) {
    CorpusDatabase corpus;
    corpus.composers[1] = create_composer_profile(ComposerProfileId{1}, "Literal observations");
    for (std::size_t index = 0; index < analyses.size(); ++index) {
        IngestedWork work;
        work.id = IngestedWorkId{index + 1};
        work.analysis = analyses[index];
        work.analysis_complete = true;
        corpus.works[work.id.value] = work;
        REQUIRE(assign_work_to_composer(corpus, work.id, ComposerProfileId{1}));
    }
    return corpus;
}

void unavailable(WorkAnalysis& analysis,
                 const std::string& domain,
                 std::vector<std::string> fields) {
    analysis.evidence[domain] = {AnalysisEvidenceKind::Heuristic,
                                 "Explicit finite field contract",
                                 std::nullopt,
                                 std::move(fields),
                                 1};
}

constexpr SpelledPitch c{0, 0, 4}, d{1, 0, 4}, g{4, 0, 4};
} // namespace

TEST_CASE("one-note lanes do not dilute observed conjunct intervals",
          "[corpus-ir][aggregation][field-evidence]") {
    auto two = field_score();
    field_notes(two, 0, {c, d});
    auto one = field_score();
    field_notes(one, 0, {c});
    const auto observed = field_analysis(two);
    const auto insufficient = field_analysis(one);
    auto corpus = field_corpus({observed, insufficient});
    CHECK(corpus.composers.at(1).style_profile.melodic_profile.conjunct_proportion == 1.0f);
    CHECK(corpus.composers.at(1).style_profile.melodic_profile.typical_range == 1.0f);
    CHECK(style_profile_field_evidence(insufficient, "melodic", "conjunct_proportion") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(insufficient, "melodic", "typical_range") ==
          AnalysisEvidenceKind::Heuristic);

    SECTION("one-note lane in the same work does not exclude its observed companion") {
        auto mixed = field_score(2);
        field_notes(mixed, 0, {c, d});
        field_notes(mixed, 1, {c});
        const auto analysis = field_analysis(mixed);
        corpus = field_corpus({analysis});
        CHECK(corpus.composers.at(1).style_profile.melodic_profile.conjunct_proportion == 1.0f);
        CHECK(style_profile_field_evidence(analysis, "melodic", "conjunct_proportion") ==
              AnalysisEvidenceKind::Heuristic);
    }
    SECTION("a measured disjunct zero remains in the denominator") {
        auto leap = field_score();
        field_notes(leap, 0, {c, g});
        corpus = field_corpus({observed, insufficient, field_analysis(leap)});
        CHECK(corpus.composers.at(1).style_profile.melodic_profile.conjunct_proportion == 0.5f);
    }
}

TEST_CASE("supplied thematic economy survives an unavailable automatic economy",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.motivic_analysis.thematic_economy = 0.8f;
    auto repeated = field_score();
    field_notes(repeated, 0, {c, d, g, c, d, g});
    const auto automatic = field_analysis(repeated);
    REQUIRE(automatic.evidence.at("motivic").kind == AnalysisEvidenceKind::Heuristic);
    auto corpus = field_corpus({supplied, automatic});
    CHECK(corpus.composers.at(1).style_profile.motivic_profile.thematic_economy ==
          Catch::Approx(0.8f));
    CHECK(style_profile_field_evidence(supplied, "motivic", "thematic_economy") ==
          AnalysisEvidenceKind::Unqualified);
    CHECK(style_profile_field_evidence(automatic, "motivic", "thematic_economy") ==
          AnalysisEvidenceKind::Unavailable);
    WorkAnalysis zero;
    zero.evidence["motivic"] = {
        AnalysisEvidenceKind::ExactSymbolic, "Supplied measured economy", std::nullopt, {}, 1};
    corpus = field_corpus({supplied, automatic, zero});
    CHECK(corpus.composers.at(1).style_profile.motivic_profile.thematic_economy ==
          Catch::Approx(0.4f));
}

TEST_CASE("rhythmic means exclude missing fields and retain measured zeros",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.rhythmic_analysis.syncopation_index = 1.0f;
    supplied.rhythmic_analysis.rubato_degree = 0.6f;
    const auto rest = field_analysis(field_score());
    CHECK(style_profile_field_evidence(rest, "rhythmic", "syncopation_index") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(rest, "rhythmic", "rubato_tendency") ==
          AnalysisEvidenceKind::Unavailable);
    auto grace = field_score();
    field_notes(grace, 0, {c, d}, true);
    CHECK(style_profile_field_evidence(field_analysis(grace), "rhythmic", "syncopation_index") ==
          AnalysisEvidenceKind::Unavailable);
    auto strong = field_score();
    field_notes(strong, 0, {c});
    const auto measured_zero = field_analysis(strong);
    REQUIRE(measured_zero.rhythmic_analysis.syncopation_index == 0.0f);
    CHECK(style_profile_field_evidence(measured_zero, "rhythmic", "syncopation_index") ==
          AnalysisEvidenceKind::Heuristic);
    const auto corpus = field_corpus({supplied, rest, field_analysis(grace), measured_zero});
    CHECK(corpus.composers.at(1).style_profile.rhythmic_profile.syncopation_index == 0.5f);
    CHECK(corpus.composers.at(1).style_profile.rhythmic_profile.rubato_tendency ==
          Catch::Approx(0.6f));
}

TEST_CASE("stationary sounding lanes retain common tones independently of pair motion",
          "[corpus-ir][analysis][field-evidence]") {
    auto score = field_score(2);
    field_notes(score, 0, {c, c});
    field_notes(score, 1, {g, g});
    const auto analysis = field_analysis(score);
    CHECK(analysis.voice_leading_analysis.common_tone_retention_rate == 1.0f);
    CHECK(style_profile_field_evidence(analysis, "voice_leading", "common_tone_retention") ==
          AnalysisEvidenceKind::Heuristic);
    CHECK(style_profile_field_evidence(analysis, "voice_leading", "preferred_motion_type") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(analysis, "voice_leading", "spacing_preference") ==
          AnalysisEvidenceKind::Heuristic);
    const auto corpus = field_corpus({analysis});
    CHECK(corpus.composers.at(1).style_profile.voice_leading_profile.common_tone_retention == 1.0f);
    WorkAnalysis supplied;
    supplied.voice_leading_analysis.average_voice_independence = 0.8f;
    supplied.voice_leading_analysis.parallel_fifths_count = 4;
    const auto mixed = field_corpus({supplied, analysis});
    CHECK(mixed.composers.at(1).style_profile.voice_leading_profile.voice_independence_index ==
          Catch::Approx(0.8f));
    CHECK(mixed.composers.at(1).style_profile.voice_leading_profile.parallel_fifths_tolerance ==
          4.0f);

    auto single = field_score();
    field_notes(single, 0, {c, c});
    const auto single_analysis = field_analysis(single);
    CHECK(style_profile_field_evidence(single_analysis, "voice_leading", "common_tone_retention") ==
          AnalysisEvidenceKind::Heuristic);
    CHECK(style_profile_field_evidence(single_analysis, "voice_leading", "spacing_preference") ==
          AnalysisEvidenceKind::Unavailable);
}

TEST_CASE("harmonic unsupported inventories do not reduce supplied field means",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.harmonic_analysis.chord_vocabulary = {{"I", 2}};
    supplied.harmonic_analysis.modulation_inventory = {
        {{1, Beat::zero()}, "C major", "G major", ModulationTechnique::PivotChord, std::nullopt}};
    supplied.harmonic_analysis.chromatic_techniques = {
        {{1, Beat::zero()}, "secondary dominant", "supplied"}};
    auto score = field_score();
    field_notes(score, 0, {c, d});
    const auto automatic = field_analysis(score);
    const auto corpus = field_corpus({supplied, automatic});
    const auto& profile = corpus.composers.at(1).style_profile.harmonic_profile;
    CHECK(profile.modulation_frequency == 1.0f);
    CHECK(profile.secondary_dominant_frequency == 1.0f);
    CHECK(profile.chromatic_density == 0.5f);
    CHECK(style_profile_field_evidence(automatic, "harmonic", "modulation_frequency") ==
          AnalysisEvidenceKind::Unavailable);
}

TEST_CASE("formal fields use their own form section and dynamic sources",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.formal_analysis.form_type = FormClassification::Ternary;
    supplied.formal_analysis.proportions = {{"A", 0.5f, 0.0f}};
    supplied.dynamic_analysis.climax_position = 0.75f;
    auto automatic = field_analysis(field_score());
    auto partial = automatic;
    partial.formal_analysis.proportions = {{"A", 0.0f, 0.0f}};
    unavailable(partial, "formal", {"form_type", "partial_bar_sections.proportions"});
    auto corpus = field_corpus({supplied, automatic, partial});
    const auto& profile = corpus.composers.at(1).style_profile.formal_profile;
    CHECK(profile.preferred_forms.size() == 1);
    CHECK(profile.preferred_forms.at(static_cast<std::uint8_t>(FormClassification::Ternary)) == 1);
    CHECK(profile.section_proportions.at("A") == 0.5f);
    CHECK(profile.climax_placement == 0.75f);
    CHECK(style_profile_field_evidence(automatic, "formal", "climax_placement") ==
          AnalysisEvidenceKind::Unavailable);
    WorkAnalysis zero;
    zero.evidence["dynamic"] = {
        AnalysisEvidenceKind::ExactSymbolic, "Measured initial climax", std::nullopt, {}, 1};
    corpus = field_corpus({supplied, automatic, zero});
    CHECK(corpus.composers.at(1).style_profile.formal_profile.climax_placement == 0.375f);
}

TEST_CASE("textural dynamic and orchestration values ignore explicitly missing source fields",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.textural_analysis.texture_type_proportions = {{"polyphonic", 1.0f}};
    supplied.dynamic_analysis.dynamic_change_rate = 2.0f;
    supplied.dynamic_analysis.subito_dynamics_count = 2;
    OrchestrationAnalysisRecord orchestration;
    orchestration.instrument_usage = {{"Piano", 0.8f}};
    orchestration.melody_carrier_distribution = {{"Piano", 0.6f}};
    supplied.orchestration_analysis = orchestration;
    auto missing = supplied;
    missing.textural_analysis.texture_type_proportions = {{"polyphonic", 0.0f}};
    missing.dynamic_analysis.dynamic_change_rate = 0.0f;
    missing.dynamic_analysis.subito_dynamics_count = 0;
    missing.orchestration_analysis->instrument_usage = {{"Piano", 0.0f}};
    missing.orchestration_analysis->melody_carrier_distribution = {{"Piano", 0.0f}};
    unavailable(missing, "textural", {"texture_type_proportions"});
    unavailable(missing, "dynamic", {"dynamic_change_rate", "subito_dynamics_count"});
    unavailable(missing, "orchestration", {"instrument_usage", "melody_carrier_distribution"});
    const auto corpus = field_corpus({supplied, missing});
    const auto& profile = corpus.composers.at(1).style_profile;
    CHECK(profile.textural_profile.texture_type_distribution.at("polyphonic") == 1.0f);
    CHECK(profile.dynamic_profile.dynamic_change_rate == 2.0f);
    CHECK(profile.dynamic_profile.subito_frequency == 2.0f);
    REQUIRE(profile.orchestration_profile);
    CHECK(profile.orchestration_profile->preferred_instruments.at("Piano") == Catch::Approx(0.8f));
    CHECK(profile.orchestration_profile->melody_assignment_preference.at("Piano") ==
          Catch::Approx(0.6f));
    CHECK(style_profile_field_evidence(supplied, "melodic", "ornament_density") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(supplied, "orchestration", "tutti_proportion") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(supplied, "harmonic", "tonal_ambiguity_index") ==
          AnalysisEvidenceKind::Unavailable);
}

TEST_CASE("unavailable compound dependencies prevent cross-domain correlations",
          "[corpus-ir][aggregation][field-evidence]") {
    WorkAnalysis supplied;
    supplied.textural_analysis.density_curve = {{{1, Beat::zero()}, 1}, {{2, Beat::zero()}, 2}};
    supplied.dynamic_analysis.dynamic_shape = {{{1, Beat::zero()}, 0.2f},
                                               {{2, Beat::zero()}, 0.8f}};
    auto missing = supplied;
    missing.dynamic_analysis.dynamic_shape = {{{1, Beat::zero()}, 0.8f}, {{2, Beat::zero()}, 0.2f}};
    unavailable(missing, "dynamic", {"dynamic_shape"});
    const auto corpus = field_corpus({supplied, missing});
    CHECK(corpus.composers.at(1).style_profile.textural_profile.density_dynamic_correlation ==
          Catch::Approx(1.0f));
    CHECK(style_profile_field_evidence(missing, "textural", "density_dynamic_correlation") ==
          AnalysisEvidenceKind::Unavailable);
}

TEST_CASE("same-name instrument presence unions active bars and survives Part reordering",
          "[corpus-ir][analysis][field-evidence][orchestration]") {
    auto score = field_score(2, 2);
    score.parts[0].definition.name = "Piano";
    score.parts[1].definition.name = "Piano";
    field_notes(score, 0, {c});
    auto analysis = field_analysis(score);
    REQUIRE(analysis.orchestration_analysis);
    CHECK(analysis.orchestration_analysis->instrument_usage.at("Piano") == 0.5f);
    std::reverse(score.parts.begin(), score.parts.end());
    analysis = field_analysis(score);
    CHECK(analysis.orchestration_analysis->instrument_usage.at("Piano") == 0.5f);
    field_notes(score, 0, {g}, false, 1);
    analysis = field_analysis(score);
    CHECK(analysis.orchestration_analysis->instrument_usage.at("Piano") == 1.0f);
    std::reverse(score.parts.begin(), score.parts.end());
    CHECK(field_analysis(score).orchestration_analysis->instrument_usage.at("Piano") == 1.0f);
}

TEST_CASE(
    "unclassified automatic transformations cannot dilute supplied fragmentation or sequences",
    "[corpus-ir][aggregation][field-evidence][motivic]") {
    auto score = field_score();
    field_notes(score, 0, {c, d, g, g, SpelledPitch{5, 0, 4}, SpelledPitch{1, 0, 5}});
    const auto automatic = field_analysis(score);
    REQUIRE_FALSE(automatic.motivic_analysis.transformation_inventory.empty());
    CHECK(style_profile_field_evidence(automatic, "motivic", "fragmentation_frequency") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(automatic, "motivic", "sequence_frequency") ==
          AnalysisEvidenceKind::Unavailable);
    CHECK(style_profile_field_evidence(automatic, "melodic", "sequence_frequency") ==
          AnalysisEvidenceKind::Unavailable);
    WorkAnalysis supplied;
    supplied.motivic_analysis.transformation_inventory = {
        {ThematicUnitId{1}, {1, Beat::zero()}, ThematicTransformation::Fragmented, "supplied"},
        {ThematicUnitId{1},
         {1, Beat{1, 2}},
         ThematicTransformation::SequentialRepetition,
         "supplied"}};
    ThematicUnit unit;
    unit.occurrences = {
        {{1, Beat::zero()}, PartId{1}, ThematicTransformation::Original, "C major"},
        {{1, Beat{1, 2}}, PartId{1}, ThematicTransformation::SequentialRepetition, "G major"}};
    supplied.melodic_analysis.thematic_material = {unit};
    const auto corpus = field_corpus({supplied, automatic});
    const auto& profile = corpus.composers.at(1).style_profile;
    CHECK(profile.motivic_profile.fragmentation_frequency == 0.5f);
    CHECK(profile.motivic_profile.sequence_frequency == 0.5f);
    CHECK(profile.melodic_profile.sequence_frequency == 0.5f);
}
