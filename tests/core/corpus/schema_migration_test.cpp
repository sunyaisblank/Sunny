/**
 * @file schema_migration_test.cpp
 * @brief Unit tests for Corpus IR schema v4 and v1/v2/v3 migration
 *
 *
 * Coverage: v1 back-compat fixtures (flat score_time, lenient reader),
 * v4 field-complete strict round-trips, refusal on malformed fields, and
 * validate-on-load for the corpus database.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/workflows.hpp>

using namespace sunny::core;

namespace {

// =============================================================================
// v1 golden fixtures — captured verbatim from the schema v1 writers
// =============================================================================

constexpr const char* V1_WORK_FIXTURE = R"json(
{
  "analysis": {
    "dynamic_analysis": {
      "climax_position": 0.6200000047683716,
      "hairpin_count": 14
    },
    "formal_analysis": {
      "form_type": 0,
      "proportions": [
        {
          "golden_ratio_proximity": 0.10000000149011612,
          "label": "Exposition",
          "proportion": 0.30000001192092896
        }
      ],
      "section_plan": [
        {
          "character": "sombre",
          "end_bar": 60,
          "key": "C# minor",
          "label": "Exposition",
          "length_bars": 60,
          "start_bar": 1,
          "subsections":[],
          "tempo": 60.0
        }
      ],
      "total_duration_bars": 200
    },
    "harmonic_analysis": {
      "cadence_inventory": [
        {
          "approach": [
            "ii6",
            "V7"
          ],
          "is_structural": true,
          "position": {
            "bar": 24,
            "beat_d": 1,
            "beat_n": 1
          },
          "section_context": "Exposition",
          "type": "PAC"
        }
      ],
      "chord_vocabulary": {
        "I": 90,
        "IV": 40,
        "V": 70
      },
      "harmonic_rhythm": {
        "changes_per_bar": [
          1.0,
          2.0
        ],
        "mean_rate": 1.5,
        "rate_by_section": {
          "A": 1.5
        },
        "variance": 0.25
      },
      "modulation_inventory": [
        {
          "from_key": "C# minor",
          "pivot_chord": "A major",
          "position": {
            "bar": 16,
            "beat_d": 1,
            "beat_n": 0
          },
          "technique": 0,
          "to_key": "E major"
        }
      ],
      "progression_inventory": [
        {
          "key_context": "C# minor",
          "length": 4,
          "occurrences": [
            {
              "bar": 4,
              "beat_d": 2,
              "beat_n": 1
            },
            {
              "bar": 12,
              "beat_d": 4,
              "beat_n": 3
            }
          ],
          "roman_numerals": [
            "I",
            "IV",
            "V",
            "I"
          ]
        }
      ],
      "tonal_plan": {
        "key_area_count": 3,
        "most_distant_key": "E major"
      }
    },
    "motivic_analysis": {
      "thematic_density": 0.800000011920929,
      "thematic_economy": 0.8999999761581421
    },
    "rhythmic_analysis": {
      "metrical_complexity": 0.4000000059604645,
      "rest_proportion": 0.10000000149011612,
      "syncopation_index": 0.20000000298023224
    },
    "textural_analysis": {
      "average_density": 3.5
    },
    "voice_leading_analysis": {
      "average_voice_independence": 0.699999988079071,
      "common_tone_retention_rate": 0.6000000238418579,
      "contrary_motion_proportion": 0.0,
      "parallel_fifths_count": 2,
      "parallel_octaves_count": 0
    }
  },
  "analysis_complete": true,
  "id": 42,
  "ingestion_confidence": {
    "key_confidence": 0.949999988079071,
    "manual_corrections": [
      {
        "description": "corrected C# minor",
        "field": "key"
      }
    ],
    "metre_confidence": 1.0,
    "quantisation_residual": 0.0,
    "source_format": "musicxml",
    "spelling_confidence": 1.0,
    "voice_separation_confidence": 1.0
  },
  "metadata": {
    "composer": 1,
    "instrumentation": "Piano",
    "is_reduction": false,
    "opus": "Op. 27 No. 2",
    "source_format": "musicxml",
    "tags": [
      "sonata",
      "piano"
    ],
    "title": "Moonlight Sonata",
    "year_composed": 1801
  },
  "schema_version": 1
}
)json";

constexpr const char* V1_PROFILE_FIXTURE = R"json(
{
  "birth_year": 1810,
  "death_year": 1849,
  "id": 1,
  "name": "Frederic Chopin",
  "period_profiles": [
    {
      "label": "Paris",
      "profile": {
        "confidence": 0.0,
        "formal_profile": {
          "average_work_length": 0.0,
          "climax_placement": 0.0
        },
        "harmonic_profile": {
          "chord_frequency": {},
          "chord_vocabulary_size": 0,
          "chromatic_density": 0.0,
          "harmonic_rhythm_mean": 0.0,
          "modulation_frequency": 0.0
        },
        "melodic_profile": {
          "average_phrase_length": 0.0,
          "chromaticism_rate": 0.0,
          "conjunct_proportion": 0.0
        },
        "rhythmic_profile": {
          "metrical_complexity": 0.0,
          "syncopation_index": 0.0
        },
        "sample_size": 0,
        "signature_patterns":[],
        "voice_leading_profile": {
          "common_tone_retention": 0.0,
          "voice_independence_index": 0.0
        }
      },
      "works": [
        1
      ],
      "year_end": 1849,
      "year_start": 1831
    }
  ],
  "schema_version": 1,
  "style_profile": {
    "confidence": 0.4000000059604645,
    "formal_profile": {
      "average_work_length": 0.0,
      "climax_placement": 0.6000000238418579
    },
    "harmonic_profile": {
      "chord_frequency": {
        "I": 0.30000001192092896,
        "V": 0.20000000298023224
      },
      "chord_vocabulary_size": 20,
      "chromatic_density": 0.0,
      "harmonic_rhythm_mean": 1.2000000476837158,
      "modulation_frequency": 0.0
    },
    "melodic_profile": {
      "average_phrase_length": 8.0,
      "chromaticism_rate": 0.0,
      "conjunct_proportion": 0.699999988079071
    },
    "rhythmic_profile": {
      "metrical_complexity": 0.0,
      "syncopation_index": 0.0
    },
    "sample_size": 2,
    "signature_patterns": [
      {
        "description": "Chromatic descent",
        "distinctiveness": 2.5,
        "domain": 0,
        "examples": [
          {
            "position": {
              "bar": 8,
              "beat_d": 4,
              "beat_n": 1
            },
            "work_id": 1
          }
        ],
        "id": 7
      }
    ],
    "voice_leading_profile": {
      "common_tone_retention": 0.0,
      "voice_independence_index": 0.0
    }
  },
  "tags": [
    "Romantic",
    "piano"
  ],
  "tradition": "Western classical",
  "works": [
    1,
    2
  ]
}
)json";

constexpr const char* V1_CORPUS_FIXTURE = R"json(
{
  "composers": {
    "1": {
      "birth_year": 1810,
      "death_year": 1849,
      "id": 1,
      "name": "Frederic Chopin",
      "period_profiles": [
        {
          "label": "Paris",
          "profile": {
            "confidence": 0.0,
            "formal_profile": {
              "average_work_length": 0.0,
              "climax_placement": 0.0
            },
            "harmonic_profile": {
              "chord_frequency": {},
              "chord_vocabulary_size": 0,
              "chromatic_density": 0.0,
              "harmonic_rhythm_mean": 0.0,
              "modulation_frequency": 0.0
            },
            "melodic_profile": {
              "average_phrase_length": 0.0,
              "chromaticism_rate": 0.0,
              "conjunct_proportion": 0.0
            },
            "rhythmic_profile": {
              "metrical_complexity": 0.0,
              "syncopation_index": 0.0
            },
            "sample_size": 0,
            "signature_patterns":[],
            "voice_leading_profile": {
              "common_tone_retention": 0.0,
              "voice_independence_index": 0.0
            }
          },
          "works": [
            1
          ],
          "year_end": 1849,
          "year_start": 1831
        }
      ],
      "schema_version": 1,
      "style_profile": {
        "confidence": 0.4000000059604645,
        "formal_profile": {
          "average_work_length": 0.0,
          "climax_placement": 0.6000000238418579
        },
        "harmonic_profile": {
          "chord_frequency": {
            "I": 0.30000001192092896,
            "V": 0.20000000298023224
          },
          "chord_vocabulary_size": 20,
          "chromatic_density": 0.0,
          "harmonic_rhythm_mean": 1.2000000476837158,
          "modulation_frequency": 0.0
        },
        "melodic_profile": {
          "average_phrase_length": 8.0,
          "chromaticism_rate": 0.0,
          "conjunct_proportion": 0.699999988079071
        },
        "rhythmic_profile": {
          "metrical_complexity": 0.0,
          "syncopation_index": 0.0
        },
        "sample_size": 2,
        "signature_patterns": [
          {
            "description": "Chromatic descent",
            "distinctiveness": 2.5,
            "domain": 0,
            "examples": [
              {
                "position": {
                  "bar": 8,
                  "beat_d": 4,
                  "beat_n": 1
                },
                "work_id": 1
              }
            ],
            "id": 7
          }
        ],
        "voice_leading_profile": {
          "common_tone_retention": 0.0,
          "voice_independence_index": 0.0
        }
      },
      "tags": [
        "Romantic",
        "piano"
      ],
      "tradition": "Western classical",
      "works": [
        1,
        2
      ]
    }
  },
  "schema_version": 1,
  "works": {
    "42": {
      "analysis": {
        "dynamic_analysis": {
          "climax_position": 0.6200000047683716,
          "hairpin_count": 14
        },
        "formal_analysis": {
          "form_type": 0,
          "proportions": [
            {
              "golden_ratio_proximity": 0.10000000149011612,
              "label": "Exposition",
              "proportion": 0.30000001192092896
            }
          ],
          "section_plan": [
            {
              "character": "sombre",
              "end_bar": 60,
              "key": "C# minor",
              "label": "Exposition",
              "length_bars": 60,
              "start_bar": 1,
              "subsections":[],
              "tempo": 60.0
            }
          ],
          "total_duration_bars": 200
        },
        "harmonic_analysis": {
          "cadence_inventory": [
            {
              "approach": [
                "ii6",
                "V7"
              ],
              "is_structural": true,
              "position": {
                "bar": 24,
                "beat_d": 1,
                "beat_n": 1
              },
              "section_context": "Exposition",
              "type": "PAC"
            }
          ],
          "chord_vocabulary": {
            "I": 90,
            "IV": 40,
            "V": 70
          },
          "harmonic_rhythm": {
            "changes_per_bar": [
              1.0,
              2.0
            ],
            "mean_rate": 1.5,
            "rate_by_section": {
              "A": 1.5
            },
            "variance": 0.25
          },
          "modulation_inventory": [
            {
              "from_key": "C# minor",
              "pivot_chord": "A major",
              "position": {
                "bar": 16,
                "beat_d": 1,
                "beat_n": 0
              },
              "technique": 0,
              "to_key": "E major"
            }
          ],
          "progression_inventory": [
            {
              "key_context": "C# minor",
              "length": 4,
              "occurrences": [
                {
                  "bar": 4,
                  "beat_d": 2,
                  "beat_n": 1
                },
                {
                  "bar": 12,
                  "beat_d": 4,
                  "beat_n": 3
                }
              ],
              "roman_numerals": [
                "I",
                "IV",
                "V",
                "I"
              ]
            }
          ],
          "tonal_plan": {
            "key_area_count": 3,
            "most_distant_key": "E major"
          }
        },
        "motivic_analysis": {
          "thematic_density": 0.800000011920929,
          "thematic_economy": 0.8999999761581421
        },
        "rhythmic_analysis": {
          "metrical_complexity": 0.4000000059604645,
          "rest_proportion": 0.10000000149011612,
          "syncopation_index": 0.20000000298023224
        },
        "textural_analysis": {
          "average_density": 3.5
        },
        "voice_leading_analysis": {
          "average_voice_independence": 0.699999988079071,
          "common_tone_retention_rate": 0.6000000238418579,
          "contrary_motion_proportion": 0.0,
          "parallel_fifths_count": 2,
          "parallel_octaves_count": 0
        }
      },
      "analysis_complete": true,
      "id": 42,
      "ingestion_confidence": {
        "key_confidence": 0.949999988079071,
        "manual_corrections": [
          {
            "description": "corrected C# minor",
            "field": "key"
          }
        ],
        "metre_confidence": 1.0,
        "quantisation_residual": 0.0,
        "source_format": "musicxml",
        "spelling_confidence": 1.0,
        "voice_separation_confidence": 1.0
      },
      "metadata": {
        "composer": 1,
        "instrumentation": "Piano",
        "is_reduction": false,
        "opus": "Op. 27 No. 2",
        "source_format": "musicxml",
        "tags": [
          "sonata",
          "piano"
        ],
        "title": "Moonlight Sonata",
        "year_composed": 1801
      },
      "schema_version": 1
    }
  }
}
)json";

// =============================================================================
// v4 document builders
// =============================================================================

ThematicUnit make_thematic_unit() {
    ThematicUnit unit;
    unit.id = ThematicUnitId{9};
    unit.label = "motto";
    unit.intervals = {2, -1, 3};
    unit.rhythm = {1.0f, 0.5f, 0.5f};
    unit.contour = {1, -1, 1};
    unit.occurrences.push_back(
        {ScoreTime{2, Beat{1, 4}}, PartId{3}, ThematicTransformation::Inverted, "C# minor"});
    return unit;
}

InstrumentCombination make_instrument_combination() {
    InstrumentCombination combination;
    combination.instruments = {"Violin", "Flute"};
    combination.frequency = 3;
    combination.typical_context = "lyrical doubling";
    combination.interval_relationship = 12;
    return combination;
}

DoublingPattern make_doubling_pattern() {
    return {"Cello", "Bassoon", -12, 4};
}

StyleProfile make_style_profile() {
    StyleProfile style;
    auto& harmonic = style.harmonic_profile;
    harmonic.chord_vocabulary_size = 20;
    harmonic.chord_frequency = {{"I", 0.3f}, {"V", 0.2f}};
    harmonic.preferred_progressions.push_back(
        {{"I", "vi", "IV", "V"}, 0.2f, 1, {"opening", "transition"}});
    harmonic.modulation_frequency = 0.15f;
    harmonic.modulation_technique_preference = {{0, 0.7f}, {3, 0.3f}};
    harmonic.preferred_key_relationships = {{"tonic", 0.6f}, {"relative", 0.4f}};
    harmonic.preferred_key_signatures = {{"C# minor", 3}, {"E major", 2}};
    harmonic.chromatic_density = 0.2f;
    harmonic.secondary_dominant_frequency = 0.18f;
    harmonic.augmented_sixth_frequency = 0.07f;
    harmonic.neapolitan_frequency = 0.04f;
    harmonic.harmonic_rhythm_mean = 1.2f;
    harmonic.harmonic_rhythm_variance = 0.3f;
    harmonic.cadence_type_distribution = {{0, 0.6f}, {3, 0.4f}};
    harmonic.deceptive_cadence_frequency = 0.11f;
    harmonic.tonal_ambiguity_index = 0.09f;

    auto& melodic = style.melodic_profile;
    melodic.interval_distribution = {{-2, 0.2f}, {2, 0.4f}};
    melodic.preferred_intervals = {2, -2};
    melodic.conjunct_proportion = 0.7f;
    melodic.average_phrase_length = 8.0f;
    melodic.phrase_length_variance = 1.5f;
    melodic.contour_preferences = {{0, 0.3f}, {2, 0.7f}};
    melodic.typical_range = 17.0f;
    melodic.chromaticism_rate = 0.12f;
    melodic.scale_degree_emphasis = {{1, 0.4f}, {5, 0.3f}};
    melodic.ornament_density = 0.22f;
    melodic.sequence_frequency = 0.14f;
    melodic.leitmotif_usage = true;

    auto& rhythmic = style.rhythmic_profile;
    rhythmic.duration_distribution = {{"quarter", 0.5f}, {"eighth", 0.5f}};
    rhythmic.preferred_durations = {"eighth", "quarter"};
    rhythmic.preferred_metres = {{"4/4", 8}, {"3+3+2/8", 2}};
    rhythmic.syncopation_index = 0.25f;
    rhythmic.rhythmic_variety = 0.65f;
    rhythmic.metrical_complexity = 0.35f;
    rhythmic.tempo_mean = 92.0f;
    rhythmic.tempo_stddev = 8.0f;
    rhythmic.rubato_tendency = 0.3f;
    rhythmic.rhythmic_motif_consistency = 0.75f;

    auto& formal = style.formal_profile;
    formal.preferred_forms = {{0, 4}, {2, 1}};
    formal.average_work_length = 180.0f;
    formal.section_proportions = {{"Exposition", 0.3f}, {"Development", 0.25f}};
    formal.exposition_recapitulation_ratio = 0.95f;
    formal.development_proportion = 0.25f;
    formal.introduction_frequency = 0.2f;
    formal.coda_frequency = 0.8f;
    formal.climax_placement = 0.6f;
    formal.golden_ratio_adherence = 0.45f;
    formal.transition_technique = {"dominant prolongation", "sequential bridge"};

    auto& voice = style.voice_leading_profile;
    voice.parallel_fifths_tolerance = 0.01f;
    voice.parallel_octaves_tolerance = 0.02f;
    voice.preferred_motion_type = "contrary";
    voice.voice_independence_index = 0.8f;
    voice.common_tone_retention = 0.6f;
    voice.leading_tone_resolution_rate = 0.94f;
    voice.seventh_resolution_rate = 0.88f;
    voice.spacing_preference = "mixed";
    voice.voice_crossing_tolerance = 0.03f;

    auto& textural = style.textural_profile;
    textural.average_density = 3.4f;
    textural.density_range_low = 1.0f;
    textural.density_range_high = 7.0f;
    textural.texture_type_distribution = {{"homophonic", 0.6f}, {"polyphonic", 0.4f}};
    textural.register_span_preference = 36.0f;
    textural.density_dynamic_correlation = 0.72f;

    auto& dynamic = style.dynamic_profile;
    dynamic.dynamic_range_low = "pp";
    dynamic.dynamic_range_high = "ff";
    dynamic.most_frequent_dynamic = "p";
    dynamic.dynamic_change_rate = 0.21f;
    dynamic.subito_frequency = 0.08f;
    dynamic.climax_dynamic = "ff";
    dynamic.dynamic_arc_shape = ContourShape::Arch;

    OrchestrationStyleProfile orchestration;
    orchestration.preferred_instruments = {{"Piano", 0.9f}, {"Strings", 0.4f}};
    orchestration.signature_combinations.push_back(make_instrument_combination());
    orchestration.doubling_preferences.push_back(make_doubling_pattern());
    orchestration.melody_assignment_preference = {{"Violin", 0.6f}, {"Flute", 0.4f}};
    orchestration.tutti_proportion = 0.35f;
    orchestration.solo_proportion = 0.15f;
    orchestration.build_up_technique = {"instrument accretion", "register expansion"};
    orchestration.colour_signature = {"muted strings", "low clarinet"};
    style.orchestration_profile = std::move(orchestration);

    auto& motivic = style.motivic_profile;
    motivic.thematic_economy = 0.9f;
    motivic.preferred_transformations = {{0, 0.4f}, {3, 0.6f}};
    motivic.development_density = 0.7f;
    motivic.fragmentation_frequency = 0.45f;
    motivic.sequence_frequency = 0.3f;
    motivic.cross_movement_thematic_links = true;

    SignaturePattern harmonic_pattern;
    harmonic_pattern.id = SignaturePatternId{7};
    harmonic_pattern.description = "Chromatic descent";
    harmonic_pattern.domain = PatternDomain::Harmonic;
    harmonic_pattern.pattern_data = std::vector<std::string>{"i", "V/iv", "iv"};
    harmonic_pattern.distinctiveness = 2.5f;
    harmonic_pattern.examples = {{IngestedWorkId{1}, ScoreTime{8, Beat{1, 4}}}};
    style.signature_patterns.push_back(std::move(harmonic_pattern));

    SignaturePattern melodic_pattern;
    melodic_pattern.id = SignaturePatternId{8};
    melodic_pattern.description = "Sigh cell";
    melodic_pattern.domain = PatternDomain::Melodic;
    melodic_pattern.pattern_data = std::vector<std::int8_t>{-1, -2};
    melodic_pattern.distinctiveness = 2.1f;
    melodic_pattern.examples = {{IngestedWorkId{1}, ScoreTime{9, Beat{0, 1}}}};
    style.signature_patterns.push_back(std::move(melodic_pattern));

    SignaturePattern rhythmic_pattern;
    rhythmic_pattern.id = SignaturePatternId{10};
    rhythmic_pattern.description = "Dotted figure";
    rhythmic_pattern.domain = PatternDomain::Rhythmic;
    rhythmic_pattern.pattern_data = std::vector<float>{1.5f, 0.5f};
    rhythmic_pattern.distinctiveness = 1.9f;
    rhythmic_pattern.examples = {{IngestedWorkId{2}, ScoreTime{4, Beat{0, 1}}}};
    style.signature_patterns.push_back(std::move(rhythmic_pattern));

    SignaturePattern textural_pattern;
    textural_pattern.id = SignaturePatternId{11};
    textural_pattern.description = "Pedal veil";
    textural_pattern.domain = PatternDomain::Textural;
    textural_pattern.pattern_data = std::string{"sustained inner-voice pedal"};
    textural_pattern.distinctiveness = 2.8f;
    textural_pattern.examples = {{IngestedWorkId{2}, ScoreTime{12, Beat{1, 2}}}};
    style.signature_patterns.push_back(std::move(textural_pattern));

    style.sample_size = 2;
    style.confidence = 0.4f;
    return style;
}

IngestedWork make_work() {
    IngestedWork work;
    work.id = IngestedWorkId{42};
    work.metadata.title = "Moonlight Sonata";
    work.metadata.composer = ComposerRef{1};
    work.metadata.opus = "Op. 27 No. 2";
    work.metadata.year_composed = 1801;
    work.metadata.period = "Paris";
    work.metadata.genre = "Piano sonata";
    work.metadata.instrumentation = "Piano";
    work.metadata.source_format = "musicxml";
    work.metadata.source_description = "Urtext scan";
    work.metadata.is_reduction = true;
    work.metadata.original_instrumentation = "Solo piano";
    work.metadata.movement = "I. Adagio sostenuto";
    work.metadata.tags = {"sonata", "piano"};
    work.ingestion_confidence.key_confidence = 0.95f;
    work.ingestion_confidence.metre_confidence = 0.96f;
    work.ingestion_confidence.spelling_confidence = 0.97f;
    work.ingestion_confidence.voice_separation_confidence = 0.98f;
    work.ingestion_confidence.quantisation_residual = 0.0125f;
    work.ingestion_confidence.duration_quantisation_residual = 0.025f;
    work.ingestion_confidence.source_format = "musicxml";
    work.ingestion_confidence.manual_corrections.push_back({"key", "corrected C# minor"});
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 90}, {"V", 70}, {"IV", 40}};

    ProgressionPattern prog;
    prog.roman_numerals = {"I", "IV", "V", "I"};
    prog.length = 4;
    prog.occurrences = {ScoreTime{4, Beat{1, 2}}, ScoreTime{12, Beat{3, 4}}};
    prog.key_context = "C# minor";
    work.analysis.harmonic_analysis.progression_inventory.push_back(prog);

    ModulationEvent mod;
    mod.position = ScoreTime{16, Beat{0, 1}};
    mod.from_key = "C# minor";
    mod.to_key = "E major";
    mod.technique = ModulationTechnique::PivotChord;
    mod.pivot_chord = "A major";
    work.analysis.harmonic_analysis.modulation_inventory.push_back(mod);

    CadenceEvent cad;
    cad.position = ScoreTime{24, Beat{1, 1}};
    cad.type = "PAC";
    cad.approach = {"ii6", "V7"};
    cad.section_context = "Exposition";
    cad.is_structural = true;
    work.analysis.harmonic_analysis.cadence_inventory.push_back(cad);

    work.analysis.harmonic_analysis.harmonic_rhythm.changes_per_bar = {1.0f, 2.0f};
    work.analysis.harmonic_analysis.harmonic_rhythm.mean_rate = 1.5f;
    work.analysis.harmonic_analysis.harmonic_rhythm.variance = 0.25f;
    work.analysis.harmonic_analysis.harmonic_rhythm.rate_by_section = {{"A", 1.5f}};
    work.analysis.harmonic_analysis.chromatic_techniques.push_back(
        {ScoreTime{7, Beat{1, 2}}, "augmented sixth", "French sixth before dominant"});
    work.analysis.harmonic_analysis.tonicisation_frequency = {{2, 3}, {5, 8}};
    work.analysis.harmonic_analysis.tonal_plan.key_sequence = {{"C# minor", "tonic", 1},
                                                               {"E major", "relative", 16}};
    work.analysis.harmonic_analysis.tonal_plan.key_area_count = 3;
    work.analysis.harmonic_analysis.tonal_plan.most_distant_key = "E major";
    work.analysis.harmonic_analysis.tonal_plan.tonic_return_bar = 120;

    VoiceMelodicAnalysis voice;
    voice.part_id = PartId{3};
    voice.note_count = 42;
    voice.range_low = 40;
    voice.range_high = 84;
    voice.tessitura_low = 52;
    voice.tessitura_high = 72;
    voice.interval_distribution = {{-2, 8}, {2, 14}};
    voice.contour_inventory.push_back({ScoreTime{1, Beat{0, 1}},
                                       ScoreTime{4, Beat{0, 1}},
                                       ContourShape::Arch,
                                       12,
                                       12.0f,
                                       0.6f,
                                       0.1f});
    voice.scale_degree_distribution = {{1, 12}, {5, 9}};
    voice.leap_resolution_rate = 0.9f;
    voice.conjunct_proportion = 0.72f;
    voice.longest_ascending_run = 5;
    voice.longest_descending_run = 4;
    voice.chromaticism_rate = 0.18f;
    work.analysis.melodic_analysis.per_voice_analysis.push_back(std::move(voice));
    work.analysis.melodic_analysis.primary_melody_voice = PartId{3};
    work.analysis.melodic_analysis.thematic_material.push_back(make_thematic_unit());

    FormalSection expo;
    expo.label = "Exposition";
    expo.start_bar = 1;
    expo.end_bar = 60;
    expo.length_bars = 60;
    expo.key = "C# minor";
    expo.tempo = 60.0f;
    expo.character = "sombre";
    work.analysis.formal_analysis.section_plan.push_back(expo);
    work.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;
    work.analysis.formal_analysis.total_duration_bars = 200;
    work.analysis.formal_analysis.proportions.push_back({"Exposition", 0.3f, 0.1f});

    work.analysis.rhythmic_analysis.duration_distribution = {{"quarter", 30}, {"eighth", 60}};
    work.analysis.rhythmic_analysis.metre_distribution = {{"4/4", 180}, {"3/4", 20}};
    work.analysis.rhythmic_analysis.onset_density = {2.0f, 3.5f};

    work.analysis.rhythmic_analysis.syncopation_index = 0.2f;
    work.analysis.rhythmic_analysis.metrical_complexity = 0.4f;
    work.analysis.rhythmic_analysis.rest_proportion = 0.1f;
    work.analysis.rhythmic_analysis.rhythmic_motifs.push_back(
        {{1.5f, 0.5f}, 3, {ScoreTime{2, Beat{0, 1}}, ScoreTime{6, Beat{0, 1}}}});
    work.analysis.rhythmic_analysis.tempo_profile = {{ScoreTime{1, Beat{0, 1}}, 60.0f},
                                                     {ScoreTime{20, Beat{0, 1}}, 66.0f}};
    work.analysis.rhythmic_analysis.rubato_degree = 0.35f;
    work.analysis.rhythmic_analysis.note_density_by_section = {{"Exposition", 3.2f}};

    work.analysis.formal_analysis.tonal_plan = work.analysis.harmonic_analysis.tonal_plan;
    work.analysis.formal_analysis.thematic_assignment = {{"Exposition", {ThematicUnitId{9}}}};
    work.analysis.formal_analysis.symmetry_analysis =
        SymmetryAnalysis{true, false, 0.8f, "near-arch proportions"};

    work.analysis.voice_leading_analysis.parallel_fifths_count = 2;
    work.analysis.voice_leading_analysis.parallel_octaves_count = 1;
    work.analysis.voice_leading_analysis.contrary_motion_proportion = 0.45f;
    work.analysis.voice_leading_analysis.oblique_motion_proportion = 0.15f;
    work.analysis.voice_leading_analysis.similar_motion_proportion = 0.25f;
    work.analysis.voice_leading_analysis.parallel_motion_proportion = 0.15f;
    work.analysis.voice_leading_analysis.voice_crossing_count = 3;
    work.analysis.voice_leading_analysis.common_tone_retention_rate = 0.6f;
    work.analysis.voice_leading_analysis.average_voice_independence = 0.7f;
    work.analysis.voice_leading_analysis.resolution_patterns.push_back(
        {"leading tone", "tonic", 12, 0.92f});
    work.analysis.voice_leading_analysis.spacing_distribution = {{3, 10}, {7, 8}};

    work.analysis.textural_analysis.density_curve = {{ScoreTime{1, Beat{0, 1}}, 3},
                                                     {ScoreTime{12, Beat{0, 1}}, 5}};
    work.analysis.textural_analysis.average_density = 3.5f;
    work.analysis.textural_analysis.density_by_section = {{"Exposition", 3.5f}};
    work.analysis.textural_analysis.average_register_span = 34.0f;
    work.analysis.textural_analysis.spacing_profile = {0.6f, 0.4f, {{5, 4}, {12, 2}}};
    work.analysis.textural_analysis.texture_type_proportions = {{"homophonic", 0.8f}};

    work.analysis.dynamic_analysis.dynamic_range_low = "pp";
    work.analysis.dynamic_analysis.dynamic_range_high = "ff";
    work.analysis.dynamic_analysis.dynamic_distribution = {{"p", 8}, {"f", 3}};
    work.analysis.dynamic_analysis.climax_position = 0.62f;
    work.analysis.dynamic_analysis.hairpin_count = 14;
    work.analysis.dynamic_analysis.dynamic_change_rate = 0.25f;
    work.analysis.dynamic_analysis.dynamic_shape = {{ScoreTime{1, Beat{0, 1}}, 0.2f},
                                                    {ScoreTime{100, Beat{0, 1}}, 1.0f}};
    work.analysis.dynamic_analysis.subito_dynamics_count = 2;
    work.analysis.dynamic_analysis.dynamic_by_section = {{"Exposition", {"pp", "mf"}}};

    OrchestrationAnalysisRecord orchestration;
    orchestration.instrument_usage = {{"Piano", 1.0f}};
    orchestration.instrument_combinations.push_back(make_instrument_combination());
    orchestration.doubling_patterns.push_back(make_doubling_pattern());
    orchestration.melody_carrier_distribution = {{"right hand", 0.8f}, {"left hand", 0.2f}};
    orchestration.orchestral_crescendo_patterns.push_back(
        {ScoreTime{40, Beat{0, 1}},
         ScoreTime{44, Beat{0, 1}},
         {{"Cello", ScoreTime{40, Beat{0, 1}}}, {"Flute", ScoreTime{42, Beat{0, 1}}}},
         true});
    orchestration.density_orchestration_correlation = 0.66f;
    work.analysis.orchestration_analysis = std::move(orchestration);

    work.analysis.motivic_analysis.thematic_units.push_back(make_thematic_unit());
    work.analysis.motivic_analysis.transformation_inventory.push_back(
        {ThematicUnitId{9},
         ScoreTime{32, Beat{0, 1}},
         ThematicTransformation::Fragmented,
         "development"});
    work.analysis.motivic_analysis.developmental_techniques = {
        DevelopmentalTechnique::Fragmentation, DevelopmentalTechnique::Sequence};
    work.analysis.motivic_analysis.thematic_density = 0.8f;
    work.analysis.motivic_analysis.thematic_economy = 0.9f;
    return work;
}

ComposerProfile make_profile() {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Frederic Chopin";
    profile.birth_year = 1810;
    profile.death_year = 1849;
    profile.active_period = std::pair<std::uint16_t, std::uint16_t>{1830, 1849};
    profile.tradition = "Western classical";
    profile.works = {IngestedWorkId{1}, IngestedWorkId{2}};
    profile.tags = {"Romantic", "piano"};

    profile.style_profile = make_style_profile();

    PeriodProfile pp;
    pp.label = "Paris";
    pp.year_start = 1831;
    pp.year_end = 1849;
    pp.works = {IngestedWorkId{1}};
    pp.profile = make_style_profile();
    pp.profile.signature_patterns.clear();
    profile.period_profiles.push_back(pp);
    return profile;
}

CorpusDatabase make_corpus() {
    CorpusDatabase db;
    auto profile = make_profile();
    // Full-corpus fixtures satisfy graph and derived-profile consistency; the
    // standalone profile fixture above intentionally exercises every payload.
    profile.works = {IngestedWorkId{42}};
    profile.period_profiles[0].works = {IngestedWorkId{42}};
    for (auto& pattern : profile.style_profile.signature_patterns)
        for (auto& [work_id, _] : pattern.examples)
            work_id = IngestedWorkId{42};
    db.composers[1] = std::move(profile);
    auto work = make_work();
    work.metadata.period = "Paris";
    db.works[42] = std::move(work);
    auto signatures = std::move(db.composers[1].style_profile.signature_patterns);
    const auto rebuilt = rebuild_style_profile(db, ComposerProfileId{1});
    CHECK(rebuilt.has_value());
    db.composers[1].style_profile.signature_patterns = std::move(signatures);
    return db;
}

void check_keys(const nlohmann::json& value, std::initializer_list<const char*> expected_keys) {
    std::set<std::string> actual;
    for (const auto& [key, _] : value.items())
        actual.insert(key);
    std::set<std::string> expected;
    for (const auto* key : expected_keys)
        expected.emplace(key);
    CHECK(actual == expected);
}

nlohmann::json legacy_style_v2(nlohmann::json style) {
    style["harmonic_profile"] = {
        {"chord_vocabulary_size", style["harmonic_profile"]["chord_vocabulary_size"]},
        {"chord_frequency", style["harmonic_profile"]["chord_frequency"]},
        {"harmonic_rhythm_mean", style["harmonic_profile"]["harmonic_rhythm_mean"]},
        {"modulation_frequency", style["harmonic_profile"]["modulation_frequency"]},
        {"chromatic_density", style["harmonic_profile"]["chromatic_density"]}};
    style["melodic_profile"] = {
        {"conjunct_proportion", style["melodic_profile"]["conjunct_proportion"]},
        {"chromaticism_rate", style["melodic_profile"]["chromaticism_rate"]},
        {"average_phrase_length", style["melodic_profile"]["average_phrase_length"]}};
    style["rhythmic_profile"] = {
        {"syncopation_index", style["rhythmic_profile"]["syncopation_index"]},
        {"metrical_complexity", style["rhythmic_profile"]["metrical_complexity"]}};
    style["formal_profile"] = {
        {"average_work_length", style["formal_profile"]["average_work_length"]},
        {"climax_placement", style["formal_profile"]["climax_placement"]}};
    style["voice_leading_profile"] = {
        {"common_tone_retention", style["voice_leading_profile"]["common_tone_retention"]},
        {"voice_independence_index", style["voice_leading_profile"]["voice_independence_index"]}};
    style.erase("textural_profile");
    style.erase("dynamic_profile");
    style.erase("orchestration_profile");
    style.erase("motivic_profile");
    for (auto& pattern : style["signature_patterns"])
        pattern.erase("pattern_data");
    return style;
}

nlohmann::json legacy_work_v2(const IngestedWork& work) {
    auto encoded = ingested_work_to_json(work);
    encoded["schema_version"] = 2;
    encoded["ingestion_confidence"].erase("duration_quantisation_residual");
    auto& analysis = encoded["analysis"];

    auto harmonic = analysis["harmonic_analysis"];
    harmonic.erase("chromatic_techniques");
    harmonic.erase("tonicisation_frequency");
    harmonic["tonal_plan"].erase("key_sequence");
    harmonic["tonal_plan"].erase("tonic_return_bar");

    auto formal = analysis["formal_analysis"];
    formal.erase("tonal_plan");
    formal.erase("thematic_assignment");
    formal.erase("symmetry_analysis");

    const auto rhythmic = analysis["rhythmic_analysis"];
    const auto voice = analysis["voice_leading_analysis"];
    const auto textural = analysis["textural_analysis"];
    const auto dynamic = analysis["dynamic_analysis"];
    const auto motivic = analysis["motivic_analysis"];
    analysis = {{"harmonic_analysis", harmonic},
                {"formal_analysis", formal},
                {"rhythmic_analysis",
                 {{"syncopation_index", rhythmic["syncopation_index"]},
                  {"metrical_complexity", rhythmic["metrical_complexity"]},
                  {"rest_proportion", rhythmic["rest_proportion"]}}},
                {"voice_leading_analysis",
                 {{"parallel_fifths_count", voice["parallel_fifths_count"]},
                  {"parallel_octaves_count", voice["parallel_octaves_count"]},
                  {"contrary_motion_proportion", voice["contrary_motion_proportion"]},
                  {"common_tone_retention_rate", voice["common_tone_retention_rate"]},
                  {"average_voice_independence", voice["average_voice_independence"]}}},
                {"textural_analysis", {{"average_density", textural["average_density"]}}},
                {"dynamic_analysis",
                 {{"climax_position", dynamic["climax_position"]},
                  {"hairpin_count", dynamic["hairpin_count"]}}},
                {"motivic_analysis",
                 {{"thematic_density", motivic["thematic_density"]},
                  {"thematic_economy", motivic["thematic_economy"]}}}};
    return encoded;
}

nlohmann::json legacy_profile_v2(const ComposerProfile& profile) {
    auto encoded = composer_profile_to_json(profile);
    encoded["schema_version"] = 2;
    encoded.erase("active_period");
    encoded["style_profile"] = legacy_style_v2(std::move(encoded["style_profile"]));
    for (auto& period : encoded["period_profiles"])
        period["profile"] = legacy_style_v2(std::move(period["profile"]));
    return encoded;
}

nlohmann::json legacy_corpus_v2(const CorpusDatabase& corpus) {
    auto encoded = corpus_to_json(corpus);
    encoded["schema_version"] = 2;
    for (const auto& [id, profile] : corpus.composers)
        encoded["composers"][std::to_string(id)] = legacy_profile_v2(profile);
    for (const auto& [id, work] : corpus.works)
        encoded["works"][std::to_string(id)] = legacy_work_v2(work);
    return encoded;
}

nlohmann::json legacy_work_v3(const IngestedWork& work) {
    auto encoded = ingested_work_to_json(work);
    encoded["schema_version"] = 3;
    encoded["ingestion_confidence"].erase("duration_quantisation_residual");
    return encoded;
}

nlohmann::json legacy_profile_v3(const ComposerProfile& profile) {
    auto encoded = composer_profile_to_json(profile);
    encoded["schema_version"] = 3;
    return encoded;
}

nlohmann::json legacy_corpus_v3(const CorpusDatabase& corpus) {
    auto encoded = corpus_to_json(corpus);
    encoded["schema_version"] = 3;
    for (const auto& [id, profile] : corpus.composers)
        encoded["composers"][std::to_string(id)] = legacy_profile_v3(profile);
    for (const auto& [id, work] : corpus.works)
        encoded["works"][std::to_string(id)] = legacy_work_v3(work);
    return encoded;
}

} // anonymous namespace

// =============================================================================
// v1 back-compat: golden fixtures still load
// =============================================================================

TEST_CASE("v1 IngestedWork fixture still loads", "[corpus-ir][serialisation][v1]") {
    auto j = nlohmann::json::parse(V1_WORK_FIXTURE);
    auto result = ingested_work_from_json(j);
    REQUIRE(result.has_value());

    auto& w = *result;
    CHECK(w.id.value == 42);
    CHECK(w.metadata.title == "Moonlight Sonata");
    REQUIRE(w.metadata.opus.has_value());
    CHECK(*w.metadata.opus == "Op. 27 No. 2");
    CHECK(w.analysis_complete);
    CHECK(w.analysis.harmonic_analysis.chord_vocabulary.at("I") == 90);

    // Flat v1 score_time {"bar", "beat_n", "beat_d"} is read correctly
    REQUIRE(w.analysis.harmonic_analysis.progression_inventory.size() == 1);
    const auto& occ = w.analysis.harmonic_analysis.progression_inventory[0].occurrences;
    REQUIRE(occ.size() == 2);
    CHECK(occ[0] == ScoreTime{4, Beat{1, 2}});
    CHECK(occ[1] == ScoreTime{12, Beat{3, 4}});

    // Lenient v1 reader drops fields it never read; that behaviour is pinned here
    CHECK(w.analysis.harmonic_analysis.modulation_inventory.empty());
    CHECK(w.analysis.harmonic_analysis.cadence_inventory.empty());
}

TEST_CASE("v1 ComposerProfile fixture still loads", "[corpus-ir][serialisation][v1]") {
    auto j = nlohmann::json::parse(V1_PROFILE_FIXTURE);
    auto result = composer_profile_from_json(j);
    REQUIRE(result.has_value());

    auto& p = *result;
    CHECK(p.id.value == 1);
    CHECK(p.name == "Frederic Chopin");
    REQUIRE(p.birth_year.has_value());
    CHECK(*p.birth_year == 1810);
    CHECK(p.works.size() == 2);
    CHECK(p.style_profile.confidence == Catch::Approx(0.4f));
    CHECK(p.style_profile.harmonic_profile.chord_vocabulary_size == 20);
    REQUIRE(p.period_profiles.size() == 1);
    CHECK(p.period_profiles[0].label == "Paris");

    // Lenient v1 reader never read signature patterns; that behaviour is pinned here
    CHECK(p.style_profile.signature_patterns.empty());
}

TEST_CASE("v1 CorpusDatabase fixture still loads", "[corpus-ir][serialisation][v1]") {
    auto j = nlohmann::json::parse(V1_CORPUS_FIXTURE);
    auto result = corpus_from_json(j);
    REQUIRE(result.has_value());

    auto& c = *result;
    REQUIRE(c.composers.size() == 1);
    REQUIRE(c.works.size() == 1);
    CHECK(c.composers[1].name == "Frederic Chopin");
    CHECK(c.works[42].metadata.title == "Moonlight Sonata");
    CHECK(c.works[42].analysis.formal_analysis.total_duration_bars == 200);
}

TEST_CASE("v1 corpus load skips validate-on-load", "[corpus-ir][serialisation][v1]") {
    auto j = nlohmann::json::parse(V1_CORPUS_FIXTURE);
    // Force a C6 Error-severity diagnostic: harmonic coverage 10/200 < 0.8
    // on a work with analysis_complete == true.
    j["works"]["42"]["analysis"]["harmonic_analysis"]["chord_vocabulary"] = {{"I", 10}};
    auto result = corpus_from_json(j);
    REQUIRE(result.has_value());
    CHECK(result->works[42].analysis.harmonic_analysis.chord_vocabulary.at("I") == 10);
}

// =============================================================================
// v4 field-complete round-trips
// =============================================================================

TEST_CASE("v4 IngestedWork round-trip preserves complete analysis and confidence",
          "[corpus-ir][serialisation][v4]") {
    auto work = make_work();
    const auto encoded = ingested_work_to_json(work);
    auto result = ingested_work_from_json(encoded);
    REQUIRE(result.has_value());
    CHECK(ingested_work_to_json(*result) == encoded);

    auto& w = *result;
    CHECK(w.id.value == 42);
    CHECK(w.metadata.title == "Moonlight Sonata");
    CHECK(w.ingestion_confidence.key_confidence == Catch::Approx(0.95f));
    CHECK(w.ingestion_confidence.quantisation_residual == Catch::Approx(0.0125f));
    CHECK(w.ingestion_confidence.duration_quantisation_residual == Catch::Approx(0.025f));
    REQUIRE(w.ingestion_confidence.manual_corrections.size() == 1);
    CHECK(w.ingestion_confidence.manual_corrections[0].field == "key");

    const auto& ha = w.analysis.harmonic_analysis;
    REQUIRE(ha.modulation_inventory.size() == 1);
    CHECK(ha.modulation_inventory[0].from_key == "C# minor");
    CHECK(ha.modulation_inventory[0].to_key == "E major");
    CHECK(ha.modulation_inventory[0].position == ScoreTime{16, Beat{0, 1}});
    REQUIRE(ha.modulation_inventory[0].pivot_chord.has_value());
    CHECK(*ha.modulation_inventory[0].pivot_chord == "A major");

    REQUIRE(ha.cadence_inventory.size() == 1);
    CHECK(ha.cadence_inventory[0].type == "PAC");
    CHECK(ha.cadence_inventory[0].approach == std::vector<std::string>{"ii6", "V7"});
    CHECK(ha.cadence_inventory[0].is_structural);
    CHECK(ha.harmonic_rhythm.rate_by_section.at("A") == Catch::Approx(1.5f));

    REQUIRE(ha.progression_inventory.size() == 1);
    CHECK(ha.progression_inventory[0].occurrences[0] == ScoreTime{4, Beat{1, 2}});

    CHECK(w.analysis.rhythmic_analysis.rest_proportion == Catch::Approx(0.1f));
    CHECK(w.analysis.voice_leading_analysis.parallel_octaves_count == 1);
    CHECK(w.analysis.voice_leading_analysis.contrary_motion_proportion == Catch::Approx(0.45f));
    CHECK(w.analysis.textural_analysis.average_density == Catch::Approx(3.5f));
    CHECK(w.analysis.dynamic_analysis.climax_position == Catch::Approx(0.62f));
    CHECK(w.analysis.dynamic_analysis.hairpin_count == 14);
    REQUIRE(w.analysis.orchestration_analysis.has_value());
    CHECK(w.analysis.melodic_analysis.thematic_material[0].intervals ==
          std::vector<std::int8_t>{2, -1, 3});
    CHECK(w.analysis.formal_analysis.symmetry_analysis->description == "near-arch proportions");
}

TEST_CASE("v4 ComposerProfile round-trip preserves every style and pattern-data variant",
          "[corpus-ir][serialisation][v4]") {
    auto profile = make_profile();
    const auto encoded = composer_profile_to_json(profile);
    auto result = composer_profile_from_json(encoded);
    REQUIRE(result.has_value());
    CHECK(composer_profile_to_json(*result) == encoded);

    auto& p = *result;
    CHECK(p.name == "Frederic Chopin");
    REQUIRE(p.active_period.has_value());
    CHECK(*p.active_period == std::pair<std::uint16_t, std::uint16_t>{1830, 1849});
    CHECK(p.style_profile.melodic_profile.average_phrase_length == Catch::Approx(8.0f));
    CHECK(p.style_profile.formal_profile.climax_placement == Catch::Approx(0.6f));
    REQUIRE(p.style_profile.orchestration_profile.has_value());

    REQUIRE(p.style_profile.signature_patterns.size() == 4);
    const auto& pat = p.style_profile.signature_patterns[0];
    CHECK(pat.id.value == 7);
    CHECK(pat.description == "Chromatic descent");
    CHECK(pat.domain == PatternDomain::Harmonic);
    CHECK(pat.distinctiveness == Catch::Approx(2.5f));
    REQUIRE(pat.examples.size() == 1);
    CHECK(pat.examples[0].first.value == 1);
    CHECK(pat.examples[0].second == ScoreTime{8, Beat{1, 4}});
    CHECK(std::holds_alternative<std::vector<std::string>>(pat.pattern_data));
    CHECK(std::holds_alternative<std::vector<std::int8_t>>(
        p.style_profile.signature_patterns[1].pattern_data));
    CHECK(std::holds_alternative<std::vector<float>>(
        p.style_profile.signature_patterns[2].pattern_data));
    CHECK(std::holds_alternative<std::string>(p.style_profile.signature_patterns[3].pattern_data));
}

TEST_CASE("v4 JSON projection names every authoritative corpus field",
          "[corpus-ir][serialisation][v4][schema]") {
    const auto work = ingested_work_to_json(make_work());
    check_keys(work,
               {"schema_version",
                "id",
                "metadata",
                "ingestion_confidence",
                "analysis",
                "analysis_complete"});
    check_keys(work["metadata"],
               {"title",
                "composer",
                "opus",
                "year_composed",
                "period",
                "genre",
                "instrumentation",
                "source_format",
                "source_description",
                "is_reduction",
                "original_instrumentation",
                "movement",
                "tags"});
    check_keys(work["ingestion_confidence"],
               {"key_confidence",
                "metre_confidence",
                "spelling_confidence",
                "voice_separation_confidence",
                "quantisation_residual",
                "duration_quantisation_residual",
                "source_format",
                "manual_corrections"});
    check_keys(work["ingestion_confidence"]["manual_corrections"][0], {"field", "description"});

    const auto& analysis = work["analysis"];
    check_keys(analysis,
               {"harmonic_analysis",
                "melodic_analysis",
                "rhythmic_analysis",
                "formal_analysis",
                "voice_leading_analysis",
                "textural_analysis",
                "dynamic_analysis",
                "orchestration_analysis",
                "motivic_analysis"});
    const auto& harmonic = analysis["harmonic_analysis"];
    check_keys(harmonic,
               {"chord_vocabulary",
                "progression_inventory",
                "modulation_inventory",
                "harmonic_rhythm",
                "cadence_inventory",
                "chromatic_techniques",
                "tonicisation_frequency",
                "tonal_plan"});
    check_keys(harmonic["progression_inventory"][0],
               {"roman_numerals", "length", "occurrences", "key_context"});
    check_keys(harmonic["modulation_inventory"][0],
               {"position", "from_key", "to_key", "technique", "pivot_chord"});
    check_keys(harmonic["harmonic_rhythm"],
               {"changes_per_bar", "mean_rate", "variance", "rate_by_section"});
    check_keys(harmonic["cadence_inventory"][0],
               {"position", "type", "approach", "section_context", "is_structural"});
    check_keys(harmonic["chromatic_techniques"][0], {"position", "type", "description"});
    check_keys(harmonic["tonicisation_frequency"][0], {"key", "value"});
    check_keys(harmonic["tonal_plan"],
               {"key_sequence", "key_area_count", "most_distant_key", "tonic_return_bar"});
    check_keys(harmonic["tonal_plan"]["key_sequence"][0], {"key", "relationship", "start_bar"});

    const auto& melodic = analysis["melodic_analysis"];
    check_keys(melodic, {"per_voice_analysis", "primary_melody_voice", "thematic_material"});
    check_keys(melodic["per_voice_analysis"][0],
               {"part_id",
                "note_count",
                "range_low",
                "range_high",
                "tessitura_low",
                "tessitura_high",
                "interval_distribution",
                "contour_inventory",
                "scale_degree_distribution",
                "leap_resolution_rate",
                "conjunct_proportion",
                "longest_ascending_run",
                "longest_descending_run",
                "chromaticism_rate"});
    check_keys(melodic["per_voice_analysis"][0]["contour_inventory"][0],
               {"start",
                "end",
                "shape",
                "pitch_range",
                "duration_beats",
                "peak_position",
                "nadir_position"});
    check_keys(melodic["thematic_material"][0],
               {"id", "label", "intervals", "rhythm", "contour", "occurrences"});
    check_keys(melodic["thematic_material"][0]["occurrences"][0],
               {"position", "part_id", "transformation", "key"});

    const auto& rhythmic = analysis["rhythmic_analysis"];
    check_keys(rhythmic,
               {"duration_distribution",
                "metre_distribution",
                "onset_density",
                "syncopation_index",
                "rhythmic_motifs",
                "tempo_profile",
                "rubato_degree",
                "metrical_complexity",
                "rest_proportion",
                "note_density_by_section"});
    check_keys(rhythmic["rhythmic_motifs"][0], {"durations", "occurrences", "positions"});
    check_keys(rhythmic["tempo_profile"][0], {"position", "tempo"});

    const auto& formal = analysis["formal_analysis"];
    check_keys(formal,
               {"section_plan",
                "form_type",
                "total_duration_bars",
                "proportions",
                "tonal_plan",
                "thematic_assignment",
                "symmetry_analysis"});
    check_keys(formal["section_plan"][0],
               {"label",
                "start_bar",
                "end_bar",
                "length_bars",
                "key",
                "tempo",
                "character",
                "subsections"});
    check_keys(formal["proportions"][0], {"label", "proportion", "golden_ratio_proximity"});
    check_keys(formal["symmetry_analysis"],
               {"is_arch", "is_palindrome", "symmetry_index", "description"});

    const auto& voice = analysis["voice_leading_analysis"];
    check_keys(voice,
               {"parallel_fifths_count",
                "parallel_octaves_count",
                "contrary_motion_proportion",
                "oblique_motion_proportion",
                "similar_motion_proportion",
                "parallel_motion_proportion",
                "voice_crossing_count",
                "average_voice_independence",
                "common_tone_retention_rate",
                "resolution_patterns",
                "spacing_distribution"});
    check_keys(voice["resolution_patterns"][0],
               {"tendency_tone", "resolution", "frequency", "proportion_resolved"});

    const auto& textural = analysis["textural_analysis"];
    check_keys(textural,
               {"density_curve",
                "average_density",
                "density_by_section",
                "average_register_span",
                "spacing_profile",
                "texture_type_proportions"});
    check_keys(textural["density_curve"][0], {"position", "density"});
    check_keys(textural["spacing_profile"],
               {"close_proportion", "open_proportion", "gap_distribution"});

    const auto& dynamic = analysis["dynamic_analysis"];
    check_keys(dynamic,
               {"dynamic_range_low",
                "dynamic_range_high",
                "dynamic_distribution",
                "hairpin_count",
                "dynamic_change_rate",
                "dynamic_shape",
                "climax_position",
                "subito_dynamics_count",
                "dynamic_by_section"});
    check_keys(dynamic["dynamic_shape"][0], {"position", "value"});
    check_keys(dynamic["dynamic_by_section"]["Exposition"], {"low", "high"});

    const auto& orchestration = analysis["orchestration_analysis"];
    check_keys(orchestration,
               {"instrument_usage",
                "instrument_combinations",
                "doubling_patterns",
                "melody_carrier_distribution",
                "orchestral_crescendo_patterns",
                "density_orchestration_correlation"});
    check_keys(orchestration["instrument_combinations"][0],
               {"instruments", "frequency", "typical_context", "interval_relationship"});
    check_keys(orchestration["doubling_patterns"][0],
               {"source_instrument", "doubling_instrument", "interval", "frequency"});
    check_keys(orchestration["orchestral_crescendo_patterns"][0],
               {"start", "end", "instrument_entry_order", "register_expansion"});
    check_keys(orchestration["orchestral_crescendo_patterns"][0]["instrument_entry_order"][0],
               {"instrument", "position"});

    const auto& motivic = analysis["motivic_analysis"];
    check_keys(motivic,
               {"thematic_units",
                "transformation_inventory",
                "developmental_techniques",
                "thematic_density",
                "thematic_economy"});
    check_keys(motivic["transformation_inventory"][0],
               {"source_theme", "position", "transformation", "context"});

    const auto profile = composer_profile_to_json(make_profile());
    check_keys(profile,
               {"schema_version",
                "id",
                "name",
                "birth_year",
                "death_year",
                "active_period",
                "tradition",
                "works",
                "style_profile",
                "period_profiles",
                "tags"});
    check_keys(profile["active_period"], {"start", "end"});
    check_keys(profile["period_profiles"][0],
               {"label", "year_start", "year_end", "works", "profile"});

    const auto& style = profile["style_profile"];
    check_keys(style,
               {"harmonic_profile",
                "melodic_profile",
                "rhythmic_profile",
                "formal_profile",
                "voice_leading_profile",
                "textural_profile",
                "dynamic_profile",
                "orchestration_profile",
                "motivic_profile",
                "signature_patterns",
                "sample_size",
                "confidence"});
    check_keys(style["harmonic_profile"],
               {"chord_vocabulary_size",
                "chord_frequency",
                "preferred_progressions",
                "modulation_frequency",
                "modulation_technique_preference",
                "preferred_key_relationships",
                "preferred_key_signatures",
                "chromatic_density",
                "secondary_dominant_frequency",
                "augmented_sixth_frequency",
                "neapolitan_frequency",
                "harmonic_rhythm_mean",
                "harmonic_rhythm_variance",
                "cadence_type_distribution",
                "deceptive_cadence_frequency",
                "tonal_ambiguity_index"});
    check_keys(style["harmonic_profile"]["preferred_progressions"][0],
               {"progression", "frequency", "rank", "contexts"});
    check_keys(style["melodic_profile"],
               {"interval_distribution",
                "preferred_intervals",
                "conjunct_proportion",
                "average_phrase_length",
                "phrase_length_variance",
                "contour_preferences",
                "typical_range",
                "chromaticism_rate",
                "scale_degree_emphasis",
                "ornament_density",
                "sequence_frequency",
                "leitmotif_usage"});
    check_keys(style["rhythmic_profile"],
               {"duration_distribution",
                "preferred_durations",
                "preferred_metres",
                "syncopation_index",
                "rhythmic_variety",
                "metrical_complexity",
                "tempo_mean",
                "tempo_stddev",
                "rubato_tendency",
                "rhythmic_motif_consistency"});
    check_keys(style["formal_profile"],
               {"preferred_forms",
                "average_work_length",
                "section_proportions",
                "exposition_recapitulation_ratio",
                "development_proportion",
                "introduction_frequency",
                "coda_frequency",
                "climax_placement",
                "golden_ratio_adherence",
                "transition_technique"});
    check_keys(style["voice_leading_profile"],
               {"parallel_fifths_tolerance",
                "parallel_octaves_tolerance",
                "preferred_motion_type",
                "voice_independence_index",
                "common_tone_retention",
                "leading_tone_resolution_rate",
                "seventh_resolution_rate",
                "spacing_preference",
                "voice_crossing_tolerance"});
    check_keys(style["textural_profile"],
               {"average_density",
                "density_range_low",
                "density_range_high",
                "texture_type_distribution",
                "register_span_preference",
                "density_dynamic_correlation"});
    check_keys(style["dynamic_profile"],
               {"dynamic_range_low",
                "dynamic_range_high",
                "most_frequent_dynamic",
                "dynamic_change_rate",
                "subito_frequency",
                "climax_dynamic",
                "dynamic_arc_shape"});
    check_keys(style["orchestration_profile"],
               {"preferred_instruments",
                "signature_combinations",
                "doubling_preferences",
                "melody_assignment_preference",
                "tutti_proportion",
                "solo_proportion",
                "build_up_technique",
                "colour_signature"});
    check_keys(style["motivic_profile"],
               {"thematic_economy",
                "preferred_transformations",
                "development_density",
                "fragmentation_frequency",
                "sequence_frequency",
                "cross_movement_thematic_links"});
    check_keys(style["signature_patterns"][0],
               {"id", "description", "domain", "pattern_data", "distinctiveness", "examples"});
    check_keys(style["signature_patterns"][0]["pattern_data"], {"kind", "value"});
    check_keys(style["signature_patterns"][0]["examples"][0], {"work_id", "position"});
}

TEST_CASE("v4 CorpusDatabase round-trip", "[corpus-ir][serialisation][v4]") {
    auto db = make_corpus();
    const auto encoded = corpus_to_json(db);
    auto result = corpus_from_json(encoded);
    REQUIRE(result.has_value());
    CHECK(corpus_to_json(*result) == encoded);

    auto& c = *result;
    REQUIRE(c.composers.size() == 1);
    REQUIRE(c.works.size() == 1);
    CHECK(c.composers[1].name == "Frederic Chopin");
    CHECK(c.works[42].metadata.title == "Moonlight Sonata");
    CHECK(c.works[42].analysis.harmonic_analysis.modulation_inventory.size() == 1);
}

// =============================================================================
// v2/v3 strict migration: represented subsets survive, later fields default
// =============================================================================

TEST_CASE("v2 IngestedWork fixture migrates its historical represented subset",
          "[corpus-ir][serialisation][v2][migration]") {
    auto result = ingested_work_from_json(legacy_work_v2(make_work()));
    REQUIRE(result.has_value());
    CHECK(result->analysis.harmonic_analysis.modulation_inventory.size() == 1);
    CHECK(result->analysis.rhythmic_analysis.rest_proportion == Catch::Approx(0.1f));
    CHECK(result->analysis.melodic_analysis.per_voice_analysis.empty());
    CHECK(result->analysis.harmonic_analysis.chromatic_techniques.empty());
    CHECK_FALSE(result->analysis.orchestration_analysis.has_value());
}

TEST_CASE("v2 ComposerProfile fixture migrates without fabricating omitted profile fields",
          "[corpus-ir][serialisation][v2][migration]") {
    auto result = composer_profile_from_json(legacy_profile_v2(make_profile()));
    REQUIRE(result.has_value());
    CHECK(result->style_profile.harmonic_profile.chord_vocabulary_size == 20);
    CHECK(result->style_profile.melodic_profile.average_phrase_length == Catch::Approx(8.0f));
    CHECK_FALSE(result->active_period.has_value());
    CHECK(result->style_profile.textural_profile.texture_type_distribution.empty());
    REQUIRE(result->style_profile.signature_patterns.size() == 4);
    CHECK(
        std::get<std::vector<std::string>>(result->style_profile.signature_patterns[0].pattern_data)
            .empty());
}

TEST_CASE("v2 CorpusDatabase fixture still validates and loads",
          "[corpus-ir][serialisation][v2][migration]") {
    auto encoded = legacy_corpus_v2(make_corpus());
    encoded["composers"]["1"]["style_profile"]["harmonic_profile"]["chord_vocabulary_size"] = 999;
    auto result = corpus_from_json(encoded);
    REQUIRE(result.has_value());
    CHECK(result->composers.at(1).name == "Frederic Chopin");
    CHECK(result->composers.at(1).style_profile.harmonic_profile.chord_vocabulary_size == 999);
    CHECK(result->works.at(42).metadata.title == "Moonlight Sonata");
}

TEST_CASE("v3 IngestedWork migrates the historically unobserved duration residual",
          "[corpus-ir][serialisation][v3][migration]") {
    auto result = ingested_work_from_json(legacy_work_v3(make_work()));
    REQUIRE(result.has_value());
    CHECK(result->ingestion_confidence.quantisation_residual == Catch::Approx(0.0125f));
    CHECK(result->ingestion_confidence.duration_quantisation_residual == 0.0f);
}

TEST_CASE("v3 CorpusDatabase remains a strict migration input",
          "[corpus-ir][serialisation][v3][migration]") {
    auto result = corpus_from_json(legacy_corpus_v3(make_corpus()));
    REQUIRE(result.has_value());
    CHECK(result->works.at(42).ingestion_confidence.duration_quantisation_residual == 0.0f);
}

// =============================================================================
// v4 refusal: missing or malformed required fields
// =============================================================================

TEST_CASE("v4 work refuses missing required field", "[corpus-ir][serialisation][v4]") {
    auto j = ingested_work_to_json(make_work());
    j.erase("ingestion_confidence");
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 work requires and bounds complete quantisation evidence",
          "[corpus-ir][serialisation][v4]") {
    auto missing = ingested_work_to_json(make_work());
    missing["ingestion_confidence"].erase("duration_quantisation_residual");
    auto missing_result = ingested_work_from_json(missing);
    REQUIRE_FALSE(missing_result.has_value());
    CHECK(missing_result.error() == ErrorCode::FormatError);

    auto negative = ingested_work_to_json(make_work());
    negative["ingestion_confidence"]["duration_quantisation_residual"] = -0.25f;
    auto negative_result = ingested_work_from_json(negative);
    REQUIRE_FALSE(negative_result.has_value());
    CHECK(negative_result.error() == ErrorCode::FormatError);

    auto out_of_range = ingested_work_to_json(make_work());
    out_of_range["ingestion_confidence"]["key_confidence"] = 1.25f;
    auto out_of_range_result = ingested_work_from_json(out_of_range);
    REQUIRE_FALSE(out_of_range_result.has_value());
    CHECK(out_of_range_result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 profile refuses missing required field", "[corpus-ir][serialisation][v4]") {
    auto j = composer_profile_to_json(make_profile());
    j.erase("works");
    auto result = composer_profile_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 corpus refuses missing required field", "[corpus-ir][serialisation][v4]") {
    auto j = corpus_to_json(make_corpus());
    j.erase("works");
    auto result = corpus_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 refuses missing nested field", "[corpus-ir][serialisation][v4]") {
    auto j = ingested_work_to_json(make_work());
    j["analysis"]["harmonic_analysis"]["harmonic_rhythm"].erase("mean_rate");
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 refuses beat with zero denominator", "[corpus-ir][serialisation][v4]") {
    auto j = ingested_work_to_json(make_work());
    j["analysis"]["harmonic_analysis"]["progression_inventory"][0]["occurrences"][0]["beat"]
     ["den"] = 0;
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 refuses unknown signature-pattern data kinds", "[corpus-ir][serialisation][v4]") {
    auto j = composer_profile_to_json(make_profile());
    j["style_profile"]["signature_patterns"][0]["pattern_data"]["kind"] = "opaque";
    auto result = composer_profile_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("v4 refuses malformed newly required analysis fields", "[corpus-ir][serialisation][v4]") {
    auto missing = ingested_work_to_json(make_work());
    missing["analysis"]["melodic_analysis"].erase("thematic_material");
    auto missing_result = ingested_work_from_json(missing);
    REQUIRE_FALSE(missing_result.has_value());
    CHECK(missing_result.error() == ErrorCode::FormatError);

    auto overflow = ingested_work_to_json(make_work());
    overflow["analysis"]["melodic_analysis"]["per_voice_analysis"][0]["range_high"] = 128;
    auto overflow_result = ingested_work_from_json(overflow);
    REQUIRE_FALSE(overflow_result.has_value());
    CHECK(overflow_result.error() == ErrorCode::FormatError);

    auto missing_note_count = ingested_work_to_json(make_work());
    missing_note_count["analysis"]["melodic_analysis"]["per_voice_analysis"][0].erase("note_count");
    auto missing_note_count_result = ingested_work_from_json(missing_note_count);
    REQUIRE_FALSE(missing_note_count_result.has_value());
    CHECK(missing_note_count_result.error() == ErrorCode::FormatError);

    auto missing_metre = ingested_work_to_json(make_work());
    missing_metre["analysis"]["rhythmic_analysis"].erase("metre_distribution");
    auto missing_metre_result = ingested_work_from_json(missing_metre);
    REQUIRE_FALSE(missing_metre_result.has_value());
    CHECK(missing_metre_result.error() == ErrorCode::FormatError);

    auto missing_style_field = composer_profile_to_json(make_profile());
    missing_style_field["style_profile"]["harmonic_profile"].erase("preferred_key_relationships");
    auto missing_style_field_result = composer_profile_from_json(missing_style_field);
    REQUIRE_FALSE(missing_style_field_result.has_value());
    CHECK(missing_style_field_result.error() == ErrorCode::FormatError);
}

// =============================================================================
// Schema version refusal
// =============================================================================

TEST_CASE("missing schema_version is refused", "[corpus-ir][serialisation]") {
    auto work_j = ingested_work_to_json(make_work());
    work_j.erase("schema_version");
    auto work_result = ingested_work_from_json(work_j);
    REQUIRE_FALSE(work_result.has_value());
    CHECK(work_result.error() == ErrorCode::FormatError);

    auto profile_j = composer_profile_to_json(make_profile());
    profile_j.erase("schema_version");
    auto profile_result = composer_profile_from_json(profile_j);
    REQUIRE_FALSE(profile_result.has_value());
    CHECK(profile_result.error() == ErrorCode::FormatError);

    auto corpus_j = corpus_to_json(make_corpus());
    corpus_j.erase("schema_version");
    auto corpus_result = corpus_from_json(corpus_j);
    REQUIRE_FALSE(corpus_result.has_value());
    CHECK(corpus_result.error() == ErrorCode::FormatError);
}

TEST_CASE("schema_version 5 is refused", "[corpus-ir][serialisation]") {
    auto j = corpus_to_json(make_corpus());
    j["schema_version"] = 5;
    auto result = corpus_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

// =============================================================================
// Validate-on-load (corpus database, strict v2-v4 paths)
// =============================================================================

TEST_CASE("v4 corpus validate-on-load blocks Error-severity diagnostics",
          "[corpus-ir][serialisation][v4]") {
    auto db = make_corpus();
    // C6 Error: harmonic coverage 10/200 < 0.8 with analysis_complete == true
    db.works[42].analysis.harmonic_analysis.chord_vocabulary = {{"I", 10}};

    auto corpus_result = corpus_from_json(corpus_to_json(db));
    REQUIRE_FALSE(corpus_result.has_value());
    CHECK(corpus_result.error() == ErrorCode::ValidationOnLoadFailed);

    // The per-work entry point does not validate; the same document loads
    auto work_result = ingested_work_from_json(ingested_work_to_json(db.works[42]));
    CHECK(work_result.has_value());
}

TEST_CASE("v4 corpus validate-on-load blocks stale derived aggregates",
          "[corpus-ir][serialisation][v4][aggregate]") {
    auto encoded = corpus_to_json(make_corpus());
    encoded["composers"]["1"]["style_profile"]["harmonic_profile"]["harmonic_rhythm_mean"] = 99.0f;
    auto result = corpus_from_json(encoded);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ValidationOnLoadFailed);
}
