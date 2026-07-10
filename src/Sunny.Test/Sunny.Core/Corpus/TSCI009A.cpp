/**
 * @file TSCI009A.cpp
 * @brief Unit tests for Corpus IR schema v2 migration (CISZ001A)
 *
 * Component: TSCI009A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CISZ001A
 * Coverage: v1 back-compat fixtures (flat score_time, lenient reader),
 * v2 strict round-trips, refusal on missing required fields, and
 * validate-on-load for the corpus database.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Corpus/CISZ001A.h"

using namespace Sunny::Core;

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
          "subsections": [],
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
        "signature_patterns": [],
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
            "signature_patterns": [],
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
              "subsections": [],
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
// v2 document builders
// =============================================================================

IngestedWork make_work() {
    IngestedWork work;
    work.id = IngestedWorkId{42};
    work.metadata.title = "Moonlight Sonata";
    work.metadata.composer = ComposerRef{1};
    work.metadata.opus = "Op. 27 No. 2";
    work.metadata.year_composed = 1801;
    work.metadata.instrumentation = "Piano";
    work.metadata.source_format = "musicxml";
    work.metadata.tags = {"sonata", "piano"};
    work.ingestion_confidence.key_confidence = 0.95f;
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
    work.analysis.harmonic_analysis.tonal_plan.key_area_count = 3;
    work.analysis.harmonic_analysis.tonal_plan.most_distant_key = "E major";

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

    work.analysis.rhythmic_analysis.syncopation_index = 0.2f;
    work.analysis.rhythmic_analysis.metrical_complexity = 0.4f;
    work.analysis.rhythmic_analysis.rest_proportion = 0.1f;
    work.analysis.voice_leading_analysis.parallel_fifths_count = 2;
    work.analysis.voice_leading_analysis.parallel_octaves_count = 1;
    work.analysis.voice_leading_analysis.contrary_motion_proportion = 0.45f;
    work.analysis.voice_leading_analysis.common_tone_retention_rate = 0.6f;
    work.analysis.voice_leading_analysis.average_voice_independence = 0.7f;
    work.analysis.textural_analysis.average_density = 3.5f;
    work.analysis.dynamic_analysis.climax_position = 0.62f;
    work.analysis.dynamic_analysis.hairpin_count = 14;
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
    profile.tradition = "Western classical";
    profile.works = {IngestedWorkId{1}, IngestedWorkId{2}};
    profile.tags = {"Romantic", "piano"};

    profile.style_profile.sample_size = 2;
    profile.style_profile.confidence = 0.4f;
    profile.style_profile.harmonic_profile.chord_vocabulary_size = 20;
    profile.style_profile.harmonic_profile.chord_frequency = {{"I", 0.3f}, {"V", 0.2f}};
    profile.style_profile.harmonic_profile.harmonic_rhythm_mean = 1.2f;
    profile.style_profile.melodic_profile.conjunct_proportion = 0.7f;
    profile.style_profile.melodic_profile.average_phrase_length = 8.0f;
    profile.style_profile.formal_profile.climax_placement = 0.6f;

    SignaturePattern pat;
    pat.id = SignaturePatternId{7};
    pat.description = "Chromatic descent";
    pat.domain = PatternDomain::Harmonic;
    pat.distinctiveness = 2.5f;
    pat.examples = {{IngestedWorkId{1}, ScoreTime{8, Beat{1, 4}}}};
    profile.style_profile.signature_patterns.push_back(pat);

    PeriodProfile pp;
    pp.label = "Paris";
    pp.year_start = 1831;
    pp.year_end = 1849;
    pp.works = {IngestedWorkId{1}};
    profile.period_profiles.push_back(pp);
    return profile;
}

CorpusDatabase make_corpus() {
    CorpusDatabase db;
    db.composers[1] = make_profile();
    db.works[42] = make_work();
    return db;
}

}  // anonymous namespace

// =============================================================================
// v1 back-compat: golden fixtures still load
// =============================================================================

TEST_CASE("CISZ001A: v1 IngestedWork fixture still loads", "[corpus-ir][serialisation][v1]") {
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

TEST_CASE("CISZ001A: v1 ComposerProfile fixture still loads", "[corpus-ir][serialisation][v1]") {
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

TEST_CASE("CISZ001A: v1 CorpusDatabase fixture still loads", "[corpus-ir][serialisation][v1]") {
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

TEST_CASE("CISZ001A: v1 corpus load skips validate-on-load", "[corpus-ir][serialisation][v1]") {
    auto j = nlohmann::json::parse(V1_CORPUS_FIXTURE);
    // Force a C6 Error-severity diagnostic: harmonic coverage 10/200 < 0.8
    // on a work with analysis_complete == true.
    j["works"]["42"]["analysis"]["harmonic_analysis"]["chord_vocabulary"] = {{"I", 10}};
    auto result = corpus_from_json(j);
    REQUIRE(result.has_value());
    CHECK(result->works[42].analysis.harmonic_analysis.chord_vocabulary.at("I") == 10);
}

// =============================================================================
// v2 round-trips
// =============================================================================

TEST_CASE("CISZ001A: v2 IngestedWork round-trip preserves modulations and cadences",
          "[corpus-ir][serialisation][v2]") {
    auto work = make_work();
    auto result = ingested_work_from_json(ingested_work_to_json(work));
    REQUIRE(result.has_value());

    auto& w = *result;
    CHECK(w.id.value == 42);
    CHECK(w.metadata.title == "Moonlight Sonata");
    CHECK(w.ingestion_confidence.key_confidence == Catch::Approx(0.95f));
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
}

TEST_CASE("CISZ001A: v2 ComposerProfile round-trip preserves signature patterns",
          "[corpus-ir][serialisation][v2]") {
    auto profile = make_profile();
    auto result = composer_profile_from_json(composer_profile_to_json(profile));
    REQUIRE(result.has_value());

    auto& p = *result;
    CHECK(p.name == "Frederic Chopin");
    CHECK(p.style_profile.melodic_profile.average_phrase_length == Catch::Approx(8.0f));
    CHECK(p.style_profile.formal_profile.climax_placement == Catch::Approx(0.6f));

    REQUIRE(p.style_profile.signature_patterns.size() == 1);
    const auto& pat = p.style_profile.signature_patterns[0];
    CHECK(pat.id.value == 7);
    CHECK(pat.description == "Chromatic descent");
    CHECK(pat.domain == PatternDomain::Harmonic);
    CHECK(pat.distinctiveness == Catch::Approx(2.5f));
    REQUIRE(pat.examples.size() == 1);
    CHECK(pat.examples[0].first.value == 1);
    CHECK(pat.examples[0].second == ScoreTime{8, Beat{1, 4}});
}

TEST_CASE("CISZ001A: v2 CorpusDatabase round-trip", "[corpus-ir][serialisation][v2]") {
    auto db = make_corpus();
    auto result = corpus_from_json(corpus_to_json(db));
    REQUIRE(result.has_value());

    auto& c = *result;
    REQUIRE(c.composers.size() == 1);
    REQUIRE(c.works.size() == 1);
    CHECK(c.composers[1].name == "Frederic Chopin");
    CHECK(c.works[42].metadata.title == "Moonlight Sonata");
    CHECK(c.works[42].analysis.harmonic_analysis.modulation_inventory.size() == 1);
}

// =============================================================================
// v2 refusal: missing required fields
// =============================================================================

TEST_CASE("CISZ001A: v2 work refuses missing required field", "[corpus-ir][serialisation][v2]") {
    auto j = ingested_work_to_json(make_work());
    j.erase("ingestion_confidence");
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("CISZ001A: v2 profile refuses missing required field", "[corpus-ir][serialisation][v2]") {
    auto j = composer_profile_to_json(make_profile());
    j.erase("works");
    auto result = composer_profile_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("CISZ001A: v2 corpus refuses missing required field", "[corpus-ir][serialisation][v2]") {
    auto j = corpus_to_json(make_corpus());
    j.erase("works");
    auto result = corpus_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("CISZ001A: v2 refuses missing nested field", "[corpus-ir][serialisation][v2]") {
    auto j = ingested_work_to_json(make_work());
    j["analysis"]["harmonic_analysis"]["harmonic_rhythm"].erase("mean_rate");
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("CISZ001A: v2 refuses beat with zero denominator", "[corpus-ir][serialisation][v2]") {
    auto j = ingested_work_to_json(make_work());
    j["analysis"]["harmonic_analysis"]["progression_inventory"][0]
     ["occurrences"][0]["beat"]["den"] = 0;
    auto result = ingested_work_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

// =============================================================================
// Schema version refusal
// =============================================================================

TEST_CASE("CISZ001A: missing schema_version is refused", "[corpus-ir][serialisation]") {
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

TEST_CASE("CISZ001A: schema_version 3 is refused", "[corpus-ir][serialisation]") {
    auto j = corpus_to_json(make_corpus());
    j["schema_version"] = 3;
    auto result = corpus_from_json(j);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

// =============================================================================
// Validate-on-load (corpus database, v2 only)
// =============================================================================

TEST_CASE("CISZ001A: v2 corpus validate-on-load blocks Error-severity diagnostics",
          "[corpus-ir][serialisation][v2]") {
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
