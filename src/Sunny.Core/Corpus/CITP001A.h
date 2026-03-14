/**
 * @file CITP001A.h
 * @brief Corpus IR foundation types — identifiers, enumerations, value types
 *
 * Component: CITP001A
 * Domain: CI (Corpus IR) | Category: TP (Types)
 *
 * Defines all identifiers, enumerations, and small value types required by
 * the Corpus IR document model (Corpus Spec §1–§5). These types compose
 * with Score IR types (PartId, ScoreTime, KeySignature) and reuse theory
 * engine types (Interval, SpelledPitch, PitchClass).
 *
 * Invariants:
 * - All Id<T> are unique within a Corpus database
 * - Normalised frequency values are in [0.0, 1.0]
 * - Sample sizes are reported alongside all aggregate statistics
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include "../Score/SITP001A.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Identifiers
// =============================================================================

struct IngestedWorkTag;
struct ComposerProfileTag;
struct ThematicUnitTag;
struct SignaturePatternTag;

using IngestedWorkId     = Id<IngestedWorkTag>;
using ComposerProfileId  = Id<ComposerProfileTag>;
using ThematicUnitId     = Id<ThematicUnitTag>;
using SignaturePatternId = Id<SignaturePatternTag>;

// ComposerRef is an alias used in metadata contexts
using ComposerRef = ComposerProfileId;

// =============================================================================
// Error Codes (8000–8099)
// =============================================================================

namespace CorpusError {
    // Ingestion (C1–C5)
    constexpr int LowIngestionConfidence  = 8000;  // C1
    constexpr int ScoreValidationFailed   = 8001;  // C2
    constexpr int LowKeyConfidence        = 8002;  // C3
    constexpr int InferredTimeSig         = 8003;  // C4
    constexpr int ExcessiveVoices         = 8004;  // C5

    // Analysis (C6–C9)
    constexpr int LowHarmonicCoverage     = 8010;  // C6
    constexpr int OverSegmentation        = 8011;  // C7
    constexpr int NoThematicUnits         = 8012;  // C8
    constexpr int SingleInstrument        = 8013;  // C9

    // Profile (C10–C13)
    constexpr int SmallCorpus             = 8020;  // C10
    constexpr int ModerateCorpus          = 8021;  // C11
    constexpr int SmallPeriodCorpus       = 8022;  // C12
    constexpr int WeakSignature           = 8023;  // C13

    // General
    constexpr int NotFound                = 8030;
    constexpr int DuplicateId             = 8031;
    constexpr int InvalidParameter        = 8032;
    constexpr int IngestionFailed         = 8033;
    constexpr int AnalysisFailed          = 8034;
}  // namespace CorpusError

// =============================================================================
// §1.3.2 Ingestion Confidence
// =============================================================================

struct ManualCorrection {
    std::string field;
    std::string description;
};

struct IngestionConfidence {
    float key_confidence = 1.0f;
    float metre_confidence = 1.0f;
    float spelling_confidence = 1.0f;
    float voice_separation_confidence = 1.0f;
    float quantisation_residual = 0.0f;        // ms
    std::string source_format;
    std::vector<ManualCorrection> manual_corrections;
};

// =============================================================================
// §1.6 WorkMetadata
// =============================================================================

struct WorkMetadata {
    std::string title;
    ComposerRef composer;
    std::optional<std::string> opus;
    std::optional<std::uint16_t> year_composed;
    std::optional<std::string> period;
    std::optional<std::string> genre;
    std::string instrumentation;
    std::string source_format;
    std::optional<std::string> source_description;
    bool is_reduction = false;
    std::optional<std::string> original_instrumentation;
    std::optional<std::string> movement;
    std::vector<std::string> tags;
};

// =============================================================================
// §2.3 Harmonic Analysis Types
// =============================================================================

struct ProgressionPattern {
    std::vector<std::string> roman_numerals;
    std::uint8_t length = 0;
    std::vector<ScoreTime> occurrences;
    std::string key_context;                    // simplified from KeySignature
};

enum class ModulationTechnique : std::uint8_t {
    PivotChord,
    ChromaticMediant,
    Enharmonic,
    DirectModulation,
    Sequential,
    CommonTone,
    Abrupt
};

struct ModulationEvent {
    ScoreTime position;
    std::string from_key;
    std::string to_key;
    ModulationTechnique technique = ModulationTechnique::PivotChord;
    std::optional<std::string> pivot_chord;
};

struct HarmonicRhythmProfile {
    std::vector<float> changes_per_bar;
    float mean_rate = 0.0f;
    float variance = 0.0f;
    std::map<std::string, float> rate_by_section;
};

/// Cadence type stored as a string to avoid name collision with
/// the CadenceType enum in HRCD001A.h (included via Score IR).
/// Values: "PAC", "IAC", "HC", "DC", "PC", "Phrygian"

struct CadenceEvent {
    ScoreTime position;
    std::string type;                   ///< e.g. "PAC", "IAC", "HC"
    std::vector<std::string> approach;
    std::string section_context;
    bool is_structural = false;
};

struct TonalPlan {
    std::vector<std::tuple<std::string, std::string, std::uint32_t>> key_sequence;
    std::uint32_t key_area_count = 1;
    std::string most_distant_key;
    std::optional<std::uint32_t> tonic_return_bar;
};

struct ChromaticEvent {
    ScoreTime position;
    std::string type;                           // e.g. "secondary dominant", "augmented sixth"
    std::string description;
};

struct HarmonicAnalysisRecord {
    std::map<std::string, std::uint32_t> chord_vocabulary;
    std::vector<ProgressionPattern> progression_inventory;
    std::vector<ModulationEvent> modulation_inventory;
    HarmonicRhythmProfile harmonic_rhythm;
    std::vector<CadenceEvent> cadence_inventory;
    std::vector<ChromaticEvent> chromatic_techniques;
    std::map<std::uint8_t, std::uint32_t> tonicisation_frequency;
    TonalPlan tonal_plan;
};

// =============================================================================
// §2.4 Melodic Analysis Types
// =============================================================================

enum class ContourShape : std::uint8_t {
    Ascending,
    Descending,
    Arch,
    InvertedArch,
    Stationary,
    Oscillating,
    AscendingArch,
    DescendingArch,
    Complex
};

struct ContourSegment {
    ScoreTime start;
    ScoreTime end;
    ContourShape shape = ContourShape::Stationary;
    std::int8_t pitch_range = 0;               // semitones
    float duration_beats = 0.0f;
    float peak_position = 0.5f;
    float nadir_position = 0.5f;
};

enum class ThematicTransformation : std::uint8_t {
    Original,
    TransposedExact,
    TransposedTonal,
    Inverted,
    Retrograde,
    RetrogradeInversion,
    Augmented,
    Diminished,
    Fragmented,
    Extended,
    SequentialRepetition,
    Ornamented,
    Simplified,
    Developed
};

struct ThematicOccurrence {
    ScoreTime position;
    PartId part_id{};
    ThematicTransformation transformation = ThematicTransformation::Original;
    std::string key;
};

struct ThematicUnit {
    ThematicUnitId id{};
    std::string label;
    std::vector<std::int8_t> intervals;         // interval sequence (semitones)
    std::vector<float> rhythm;                  // duration sequence (beats)
    std::vector<std::int8_t> contour;           // -1, 0, +1
    std::vector<ThematicOccurrence> occurrences;
};

struct VoiceMelodicAnalysis {
    PartId part_id{};
    std::int8_t range_low = 0;                 // MIDI note number
    std::int8_t range_high = 127;
    std::int8_t tessitura_low = 48;
    std::int8_t tessitura_high = 72;
    std::map<std::int8_t, std::uint32_t> interval_distribution;
    std::vector<ContourSegment> contour_inventory;
    std::map<std::uint8_t, std::uint32_t> scale_degree_distribution;
    float leap_resolution_rate = 0.0f;
    float conjunct_proportion = 0.0f;
    std::uint8_t longest_ascending_run = 0;
    std::uint8_t longest_descending_run = 0;
    float chromaticism_rate = 0.0f;
};

struct MelodicAnalysisRecord {
    std::vector<VoiceMelodicAnalysis> per_voice_analysis;
    PartId primary_melody_voice{};
    std::vector<ThematicUnit> thematic_material;
};

// =============================================================================
// §2.5 Rhythmic Analysis Types
// =============================================================================

struct RhythmicMotif {
    std::vector<float> durations;              // in beats
    std::uint32_t occurrences = 0;
    std::vector<ScoreTime> positions;
};

struct RhythmicAnalysisRecord {
    std::map<std::string, std::uint32_t> duration_distribution;
    std::vector<float> onset_density;          // per bar
    float syncopation_index = 0.0f;
    std::vector<RhythmicMotif> rhythmic_motifs;
    std::vector<std::pair<ScoreTime, float>> tempo_profile;
    float rubato_degree = 0.0f;
    float metrical_complexity = 0.0f;
    float rest_proportion = 0.0f;
    std::map<std::string, float> note_density_by_section;
};

// =============================================================================
// §2.6 Formal Analysis Types
// =============================================================================

enum class FormClassification : std::uint8_t {
    SonataAllegro,
    SonataRondo,
    Rondo,
    BinarySimple,
    BinaryRounded,
    Ternary,
    ThroughComposed,
    ThemeAndVariations,
    Fugue,
    Strophic,
    VerseChorus,
    AABA,
    TwelveBarBlues,
    Ritornello,
    Passacaglia,
    Chaconne,
    Concerto,
    Fantasia,
    Prelude,
    Suite,
    Other
};

struct FormalSection {
    std::string label;
    std::uint32_t start_bar = 0;
    std::uint32_t end_bar = 0;
    std::uint32_t length_bars = 0;
    std::string key;
    float tempo = 120.0f;
    std::optional<std::string> character;
    std::vector<FormalSection> subsections;
};

struct SectionProportion {
    std::string label;
    float proportion = 0.0f;
    float golden_ratio_proximity = 0.0f;
};

struct SymmetryAnalysis {
    bool is_arch = false;
    bool is_palindrome = false;
    float symmetry_index = 0.0f;
    std::string description;
};

struct FormalAnalysisRecord {
    std::vector<FormalSection> section_plan;
    FormClassification form_type = FormClassification::Other;
    std::uint32_t total_duration_bars = 0;
    std::vector<SectionProportion> proportions;
    TonalPlan tonal_plan;
    std::map<std::string, std::vector<ThematicUnitId>> thematic_assignment;
    std::optional<SymmetryAnalysis> symmetry_analysis;
};

// =============================================================================
// §2.7 Voice-Leading Analysis Types
// =============================================================================

struct ResolutionPattern {
    std::string tendency_tone;
    std::string resolution;
    std::uint32_t frequency = 0;
    float proportion_resolved = 0.0f;
};

struct VoiceLeadingAnalysisRecord {
    std::uint32_t parallel_fifths_count = 0;
    std::uint32_t parallel_octaves_count = 0;
    float contrary_motion_proportion = 0.0f;
    float oblique_motion_proportion = 0.0f;
    float similar_motion_proportion = 0.0f;
    float parallel_motion_proportion = 0.0f;
    std::uint32_t voice_crossing_count = 0;
    float average_voice_independence = 0.0f;
    float common_tone_retention_rate = 0.0f;
    std::vector<ResolutionPattern> resolution_patterns;
    std::map<std::int8_t, std::uint32_t> spacing_distribution;
};

// =============================================================================
// §2.8 Textural Analysis Types
// =============================================================================

struct SpacingProfile {
    float close_proportion = 0.0f;
    float open_proportion = 0.0f;
    std::map<std::int8_t, std::uint32_t> gap_distribution;
};

struct TexturalAnalysisRecord {
    std::vector<std::pair<ScoreTime, std::uint8_t>> density_curve;
    float average_density = 0.0f;
    std::map<std::string, float> density_by_section;
    float average_register_span = 0.0f;
    SpacingProfile spacing_profile;
    std::map<std::string, float> texture_type_proportions;
};

// =============================================================================
// §2.9 Dynamic Analysis Types
// =============================================================================

struct DynamicAnalysisRecord {
    std::string dynamic_range_low;
    std::string dynamic_range_high;
    std::map<std::string, std::uint32_t> dynamic_distribution;
    std::uint32_t hairpin_count = 0;
    float dynamic_change_rate = 0.0f;
    std::vector<std::pair<ScoreTime, float>> dynamic_shape;
    float climax_position = 0.0f;
    std::uint32_t subito_dynamics_count = 0;
    std::map<std::string, std::pair<std::string, std::string>> dynamic_by_section;
};

// =============================================================================
// §2.10 Orchestration Analysis Types
// =============================================================================

struct InstrumentCombination {
    std::vector<std::string> instruments;
    std::uint32_t frequency = 0;
    std::string typical_context;
    std::optional<std::int8_t> interval_relationship;
};

struct DoublingPattern {
    std::string source_instrument;
    std::string doubling_instrument;
    std::int8_t interval = 0;                  // semitones
    std::uint32_t frequency = 0;
};

struct OrchestraCrescendoPattern {
    ScoreTime start;
    ScoreTime end;
    std::vector<std::pair<std::string, ScoreTime>> instrument_entry_order;
    bool register_expansion = false;
};

struct OrchestrationAnalysisRecord {
    std::map<std::string, float> instrument_usage;
    std::vector<InstrumentCombination> instrument_combinations;
    std::vector<DoublingPattern> doubling_patterns;
    std::map<std::string, float> melody_carrier_distribution;
    std::vector<OrchestraCrescendoPattern> orchestral_crescendo_patterns;
    float density_orchestration_correlation = 0.0f;
};

// =============================================================================
// §2.11 Motivic Analysis Types
// =============================================================================

enum class DevelopmentalTechnique : std::uint8_t {
    Fragmentation,
    Sequence,
    Inversion,
    Augmentation,
    Diminution,
    Stretto,
    Combination,
    Liquidation,
    Reharmonisation,
    RegisterTransfer,
    Timbral
};

struct TransformationEvent {
    ThematicUnitId source_theme{};
    ScoreTime position;
    ThematicTransformation transformation = ThematicTransformation::Original;
    std::string context;
};

struct MotivicAnalysisRecord {
    std::vector<ThematicUnit> thematic_units;
    std::vector<TransformationEvent> transformation_inventory;
    std::vector<DevelopmentalTechnique> developmental_techniques;
    float thematic_density = 0.0f;
    float thematic_economy = 0.0f;
};

// =============================================================================
// §2.2 WorkAnalysis (aggregate)
// =============================================================================

struct WorkAnalysis {
    HarmonicAnalysisRecord harmonic_analysis;
    MelodicAnalysisRecord melodic_analysis;
    RhythmicAnalysisRecord rhythmic_analysis;
    FormalAnalysisRecord formal_analysis;
    VoiceLeadingAnalysisRecord voice_leading_analysis;
    TexturalAnalysisRecord textural_analysis;
    DynamicAnalysisRecord dynamic_analysis;
    std::optional<OrchestrationAnalysisRecord> orchestration_analysis;
    MotivicAnalysisRecord motivic_analysis;
};

// =============================================================================
// §3 Style Profile Types
// =============================================================================

struct RankedProgression {
    std::vector<std::string> progression;
    float frequency = 0.0f;
    std::uint32_t rank = 0;
    std::vector<std::string> contexts;
};

struct HarmonicStyleProfile {
    std::uint32_t chord_vocabulary_size = 0;
    std::map<std::string, float> chord_frequency;
    std::vector<RankedProgression> preferred_progressions;
    float modulation_frequency = 0.0f;
    std::map<std::uint8_t, float> modulation_technique_preference;
    float chromatic_density = 0.0f;
    float secondary_dominant_frequency = 0.0f;
    float augmented_sixth_frequency = 0.0f;
    float neapolitan_frequency = 0.0f;
    float harmonic_rhythm_mean = 0.0f;
    float harmonic_rhythm_variance = 0.0f;
    std::map<std::uint8_t, float> cadence_type_distribution;
    float deceptive_cadence_frequency = 0.0f;
    float tonal_ambiguity_index = 0.0f;
};

struct MelodicStyleProfile {
    std::map<std::int8_t, float> interval_distribution;
    float conjunct_proportion = 0.0f;
    float average_phrase_length = 0.0f;
    float phrase_length_variance = 0.0f;
    std::map<std::uint8_t, float> contour_preferences;
    float typical_range = 0.0f;                // semitones
    float chromaticism_rate = 0.0f;
    std::map<std::uint8_t, float> scale_degree_emphasis;
    float ornament_density = 0.0f;
    float sequence_frequency = 0.0f;
    bool leitmotif_usage = false;
};

struct RhythmicStyleProfile {
    std::map<std::string, float> duration_distribution;
    float syncopation_index = 0.0f;
    float rhythmic_variety = 0.0f;             // entropy
    float metrical_complexity = 0.0f;
    float tempo_mean = 120.0f;
    float tempo_stddev = 20.0f;
    float rubato_tendency = 0.0f;
    float rhythmic_motif_consistency = 0.0f;
};

struct FormalStyleProfile {
    std::map<std::uint8_t, std::uint32_t> preferred_forms;
    float average_work_length = 0.0f;          // bars
    std::map<std::string, float> section_proportions;
    std::optional<float> exposition_recapitulation_ratio;
    std::optional<float> development_proportion;
    float introduction_frequency = 0.0f;
    float coda_frequency = 0.0f;
    float climax_placement = 0.0f;
    float golden_ratio_adherence = 0.0f;
};

struct VoiceLeadingStyleProfile {
    float parallel_fifths_tolerance = 0.0f;
    float parallel_octaves_tolerance = 0.0f;
    std::string preferred_motion_type;
    float voice_independence_index = 0.0f;
    float common_tone_retention = 0.0f;
    float leading_tone_resolution_rate = 0.0f;
    float seventh_resolution_rate = 0.0f;
    std::string spacing_preference;
    float voice_crossing_tolerance = 0.0f;
};

struct TexturalStyleProfile {
    float average_density = 0.0f;
    float density_range_low = 0.0f;
    float density_range_high = 0.0f;
    std::map<std::string, float> texture_type_distribution;
    float register_span_preference = 0.0f;
    float density_dynamic_correlation = 0.0f;
};

struct DynamicStyleProfile {
    std::string dynamic_range_low;
    std::string dynamic_range_high;
    std::string most_frequent_dynamic;
    float dynamic_change_rate = 0.0f;
    float subito_frequency = 0.0f;
    std::string climax_dynamic;
    ContourShape dynamic_arc_shape = ContourShape::Arch;
};

struct OrchestrationStyleProfile {
    std::map<std::string, float> preferred_instruments;
    std::vector<InstrumentCombination> signature_combinations;
    std::vector<DoublingPattern> doubling_preferences;
    std::map<std::string, float> melody_assignment_preference;
    float tutti_proportion = 0.0f;
    float solo_proportion = 0.0f;
    std::vector<std::string> colour_signature;
};

struct MotivicStyleProfile {
    float thematic_economy = 0.0f;
    std::map<std::uint8_t, float> preferred_transformations;
    float development_density = 0.0f;
    float fragmentation_frequency = 0.0f;
    float sequence_frequency = 0.0f;
    bool cross_movement_thematic_links = false;
};

// =============================================================================
// §3.14 Signature Patterns
// =============================================================================

using PatternData = std::variant<
    std::vector<std::string>,                   // HarmonicProgression
    std::vector<std::int8_t>,                   // MelodicCell (intervals)
    std::vector<float>,                         // RhythmicFigure (durations) or FormalProportion
    std::string                                 // TexturalGesture, OrchestrationalSignature, etc.
>;

enum class PatternDomain : std::uint8_t {
    Harmonic,
    Melodic,
    Rhythmic,
    Cadential,
    Textural,
    Orchestrational,
    Formal,
    Registral,
    Dynamic
};

struct SignaturePattern {
    SignaturePatternId id{};
    std::string description;
    PatternDomain domain = PatternDomain::Harmonic;
    PatternData pattern_data;
    float distinctiveness = 0.0f;
    std::vector<std::pair<IngestedWorkId, ScoreTime>> examples;
};

// =============================================================================
// §3.4 StyleProfile (aggregate)
// =============================================================================

struct StyleProfile {
    HarmonicStyleProfile harmonic_profile;
    MelodicStyleProfile melodic_profile;
    RhythmicStyleProfile rhythmic_profile;
    FormalStyleProfile formal_profile;
    VoiceLeadingStyleProfile voice_leading_profile;
    TexturalStyleProfile textural_profile;
    DynamicStyleProfile dynamic_profile;
    std::optional<OrchestrationStyleProfile> orchestration_profile;
    MotivicStyleProfile motivic_profile;
    std::vector<SignaturePattern> signature_patterns;
    std::uint32_t sample_size = 0;
    float confidence = 0.0f;
};

// =============================================================================
// §4 Comparative Analysis Types
// =============================================================================

struct StyleComparison {
    ComposerProfileId composer_a{};
    ComposerProfileId composer_b{};
    float harmonic_divergence = 0.0f;
    float melodic_divergence = 0.0f;
    float rhythmic_divergence = 0.0f;
    float formal_divergence = 0.0f;
    std::vector<std::string> most_similar_dimensions;
    std::vector<std::string> most_divergent_dimensions;
    std::vector<SignaturePattern> shared_patterns;
    std::vector<SignaturePattern> distinctive_to_a;
    std::vector<SignaturePattern> distinctive_to_b;
};

enum class TrendDirection : std::uint8_t {
    Increasing,
    Decreasing,
    NonMonotonic,
    Stable
};

struct EvolutionaryTrend {
    std::string dimension;
    TrendDirection direction = TrendDirection::Stable;
    float early_value = 0.0f;
    float late_value = 0.0f;
    std::string description;
};

// =============================================================================
// §5.2 Query Result Types
// =============================================================================

struct AnnotatedExample {
    IngestedWorkId work_id{};
    ScoreTime region_start;
    ScoreTime region_end;
    float relevance_score = 0.0f;
    std::string analysis_summary;
    std::vector<std::string> harmonic_reduction;
    std::string formal_context;
};

struct Tendency {
    std::string domain;
    std::string observation;
    float confidence = 0.0f;
    std::uint32_t supporting_examples_count = 0;
};

struct HowWouldXHandleResult {
    std::vector<AnnotatedExample> relevant_examples;
    std::vector<Tendency> statistical_tendencies;
    std::vector<SignaturePattern> signature_patterns;
};

}  // namespace Sunny::Core

#pragma GCC diagnostic pop
