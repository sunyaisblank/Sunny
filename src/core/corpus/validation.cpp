/**
 * @file validation.cpp
 * @brief Corpus IR validation — implementation
 *
 *
 * Implements validation rules C1–C16 from Corpus Spec §8.
 */

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sunny/core/corpus/validation.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/score/validation.hpp>

namespace sunny::core {

namespace {

Diagnostic
make_diag(ValidationSeverity sev, const std::string& rule, const std::string& msg, ErrorCode code) {
    return {sev, rule, msg, std::nullopt, std::nullopt, code};
}

} // anonymous namespace

std::vector<Diagnostic> validate_ingested_work(const IngestedWork& work) {
    std::vector<Diagnostic> diags;
    const auto& ic = work.ingestion_confidence;

    // C16: Confidence dimensions and quantisation residuals have closed,
    // finite domains. Check this before threshold rules so NaN cannot evade
    // every ordered comparison.
    const auto valid_confidence = [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    };
    const auto valid_residual = [](float value) { return std::isfinite(value) && value >= 0.0f; };
    const bool confidence_domain_valid = valid_confidence(ic.key_confidence) &&
                                         valid_confidence(ic.metre_confidence) &&
                                         valid_confidence(ic.spelling_confidence) &&
                                         valid_confidence(ic.voice_separation_confidence) &&
                                         valid_residual(ic.quantisation_residual) &&
                                         valid_residual(ic.duration_quantisation_residual);
    if (!confidence_domain_valid) {
        diags.push_back(make_diag(ValidationSeverity::Error,
                                  "C16",
                                  "Ingestion confidence requires finite [0,1] dimensions and "
                                  "finite non-negative onset/duration quantisation residuals",
                                  ErrorCode::CorpusInvalidParameter));
    }

    // C1: Ingestion confidence below 0.7
    if (confidence_domain_valid &&
        (ic.key_confidence < 0.7f || ic.metre_confidence < 0.7f || ic.spelling_confidence < 0.7f ||
         ic.voice_separation_confidence < 0.7f)) {
        diags.push_back(make_diag(ValidationSeverity::Warning,
                                  "C1",
                                  "Ingestion confidence below 0.7 in one or more dimensions",
                                  ErrorCode::LowIngestionConfidence));
    }

    // C3: Key estimation confidence below 0.5
    if (confidence_domain_valid && ic.key_confidence < 0.5f) {
        diags.push_back(make_diag(ValidationSeverity::Warning,
                                  "C3",
                                  "Key estimation confidence below 0.5; key may be incorrect",
                                  ErrorCode::LowKeyConfidence));
    }

    // C2: Score validation failed (when Score is present)
    if (work.score) {
        auto score_diags = validate_structural(*work.score);
        for (const auto& sd : score_diags) {
            if (sd.severity == ValidationSeverity::Error) {
                diags.push_back(
                    make_diag(ValidationSeverity::Error,
                              "C2",
                              "Embedded Score fails structural validation: " + sd.message,
                              ErrorCode::ScoreValidationFailed));
                break;
            }
        }
    }

    // C4: Missing MIDI time signature (the ingester uses its explicit 4/4 default)
    if (ic.metre_confidence < 1.0f && ic.source_format == "midi") {
        diags.push_back(make_diag(ValidationSeverity::Info,
                                  "C4",
                                  "Time signature metadata absent; defaulted to 4/4 (MIDI source)",
                                  ErrorCode::InferredTimeSig));
    }

    // C5: Excessive voices (when Score is present, > 6 voices in any measure)
    if (work.score) {
        for (const auto& part : work.score->parts) {
            for (const auto& measure : part.measures) {
                if (measure.voices.size() > 6) {
                    diags.push_back(make_diag(ValidationSeverity::Warning,
                                              "C5",
                                              "Measure " + std::to_string(measure.bar_number) +
                                                  " has " + std::to_string(measure.voices.size()) +
                                                  " voices (> 6)",
                                              ErrorCode::ExcessiveVoices));
                    goto c5_done;
                }
            }
        }
    c5_done:;
    }

    if (!work.analysis_complete) {
        return diags;
    }

    // C6: Harmonic analysis coverage
    auto total_chords = std::uint32_t{0};
    for (const auto& [_, count] : work.analysis.harmonic_analysis.chord_vocabulary)
        total_chords += count;
    auto total_bars = work.analysis.formal_analysis.total_duration_bars;
    if (total_bars > 0 && total_chords > 0) {
        float coverage = static_cast<float>(total_chords) / static_cast<float>(total_bars);
        if (coverage < 0.8f) {
            diags.push_back(make_diag(ValidationSeverity::Error,
                                      "C6",
                                      "Harmonic analysis coverage below 80% of the work",
                                      ErrorCode::LowHarmonicCoverage));
        }
    }

    // C7: Over-segmentation (sections shorter than 4 bars)
    for (const auto& sec : work.analysis.formal_analysis.section_plan) {
        if (sec.length_bars > 0 && sec.length_bars < 4) {
            diags.push_back(make_diag(ValidationSeverity::Warning,
                                      "C7",
                                      "Formal section '" + sec.label + "' is shorter than 4 bars",
                                      ErrorCode::OverSegmentation));
            break; // report once
        }
    }

    // C8: No thematic units identified
    if (work.analysis.motivic_analysis.thematic_units.empty() && total_bars > 8) {
        diags.push_back(make_diag(ValidationSeverity::Warning,
                                  "C8",
                                  "No thematic units identified in the work",
                                  ErrorCode::NoThematicUnits));
    }

    // C9: Single-instrument work (no orchestration analysis possible)
    if (!work.analysis.orchestration_analysis.has_value() &&
        work.analysis.melodic_analysis.per_voice_analysis.size() > 1) {
        diags.push_back(make_diag(ValidationSeverity::Info,
                                  "C9",
                                  "Orchestration analysis not available",
                                  ErrorCode::SingleInstrument));
    }

    // Sort by severity: Error < Warning < Info
    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_composer_profile(const ComposerProfile& profile) {
    std::vector<Diagnostic> diags;

    // C10: Small corpus (fewer than 5 works)
    if (profile.works.size() < 5) {
        diags.push_back(make_diag(ValidationSeverity::Warning,
                                  "C10",
                                  "Style profile based on fewer than 5 works (" +
                                      std::to_string(profile.works.size()) + ")",
                                  ErrorCode::SmallCorpus));
    }
    // C11: Moderate corpus (fewer than 10 works)
    else if (profile.works.size() < 10) {
        diags.push_back(make_diag(ValidationSeverity::Info,
                                  "C11",
                                  "Style profile based on fewer than 10 works (" +
                                      std::to_string(profile.works.size()) + ")",
                                  ErrorCode::ModerateCorpus));
    }

    // C12: Small period corpus
    for (const auto& period : profile.period_profiles) {
        if (period.works.size() < 3) {
            diags.push_back(make_diag(ValidationSeverity::Warning,
                                      "C12",
                                      "Period '" + period.label + "' has fewer than 3 works (" +
                                          std::to_string(period.works.size()) + ")",
                                      ErrorCode::SmallPeriodCorpus));
        }
    }

    // C13: Weak signature patterns
    for (const auto& pat : profile.style_profile.signature_patterns) {
        if (pat.distinctiveness < 1.5f) {
            diags.push_back(make_diag(ValidationSeverity::Info,
                                      "C13",
                                      "Signature pattern '" + pat.description +
                                          "' has low distinctiveness (" +
                                          std::to_string(pat.distinctiveness) + " std devs)",
                                      ErrorCode::WeakSignature));
        }
    }

    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_corpus(const CorpusDatabase& corpus) {
    std::vector<Diagnostic> diags;

    const auto inconsistent = [&diags](const std::string& message) {
        diags.push_back(make_diag(
            ValidationSeverity::Error, "C14", message, ErrorCode::CorpusInconsistentReference));
    };

    std::map<std::uint64_t, std::uint64_t> owner_by_work;
    std::map<std::uint64_t, std::pair<std::uint64_t, std::string>> period_by_work;

    for (const auto& [work_key, work] : corpus.works) {
        if (work.id.value != work_key)
            inconsistent("Work map key " + std::to_string(work_key) +
                         " does not match embedded id " + std::to_string(work.id.value));
        auto work_diags = validate_ingested_work(work);
        diags.insert(diags.end(), work_diags.begin(), work_diags.end());
    }

    for (const auto& [composer_key, profile] : corpus.composers) {
        if (profile.id.value != composer_key)
            inconsistent("Composer map key " + std::to_string(composer_key) +
                         " does not match embedded id " + std::to_string(profile.id.value));

        std::set<std::uint64_t> composer_works;
        for (const auto work_id : profile.works) {
            if (!composer_works.insert(work_id.value).second) {
                inconsistent("Composer " + std::to_string(composer_key) +
                             " contains duplicate work reference " + std::to_string(work_id.value));
                continue;
            }

            const auto work = corpus.works.find(work_id.value);
            if (work == corpus.works.end()) {
                inconsistent("Composer " + std::to_string(composer_key) +
                             " references missing work " + std::to_string(work_id.value));
                continue;
            }
            if (work->second.metadata.composer.value != composer_key)
                inconsistent("Work " + std::to_string(work_id.value) +
                             " metadata owner disagrees with composer " +
                             std::to_string(composer_key));

            const auto [owner, inserted] = owner_by_work.emplace(work_id.value, composer_key);
            if (!inserted)
                inconsistent("Work " + std::to_string(work_id.value) +
                             " is referenced by multiple composer memberships (" +
                             std::to_string(owner->second) + " and " +
                             std::to_string(composer_key) + ")");
        }

        const auto validate_signature_examples = [&corpus, &inconsistent](
                                                     const StyleProfile& style,
                                                     const std::set<std::uint64_t>& allowed_works,
                                                     const std::string& scope) {
            std::set<std::uint64_t> pattern_ids;
            for (const auto& pattern : style.signature_patterns) {
                if (!pattern_ids.insert(pattern.id.value).second)
                    inconsistent(scope + " contains duplicate signature pattern id " +
                                 std::to_string(pattern.id.value));
                for (const auto& [work_id, _] : pattern.examples) {
                    if (!corpus.works.contains(work_id.value))
                        inconsistent(scope + " signature pattern " +
                                     std::to_string(pattern.id.value) +
                                     " references missing work " + std::to_string(work_id.value));
                    else if (!allowed_works.contains(work_id.value))
                        inconsistent(scope + " signature pattern " +
                                     std::to_string(pattern.id.value) +
                                     " references work outside its membership: " +
                                     std::to_string(work_id.value));
                }
            }
        };
        validate_signature_examples(
            profile.style_profile, composer_works, "Composer " + std::to_string(composer_key));

        std::set<std::string> period_labels;
        for (std::size_t i = 0; i < profile.period_profiles.size(); ++i) {
            const auto& period = profile.period_profiles[i];
            if (period.label.empty())
                inconsistent("Composer " + std::to_string(composer_key) +
                             " contains an empty period label");
            if (!period_labels.insert(period.label).second)
                inconsistent("Composer " + std::to_string(composer_key) +
                             " contains duplicate period label '" + period.label + "'");
            if (period.year_start > period.year_end)
                inconsistent("Period '" + period.label + "' has an inverted year range");

            if (period.year_start != 0 && period.year_end != 0) {
                for (std::size_t j = 0; j < i; ++j) {
                    const auto& other = profile.period_profiles[j];
                    if (other.year_start == 0 || other.year_end == 0) continue;
                    if (std::max(period.year_start, other.year_start) <=
                        std::min(period.year_end, other.year_end))
                        inconsistent("Composer " + std::to_string(composer_key) + " periods '" +
                                     other.label + "' and '" + period.label +
                                     "' have overlapping year ranges");
                }
            }

            std::set<std::uint64_t> period_works;
            for (const auto work_id : period.works) {
                if (!period_works.insert(work_id.value).second) {
                    inconsistent("Period '" + period.label +
                                 "' contains duplicate work reference " +
                                 std::to_string(work_id.value));
                    continue;
                }
                const auto work = corpus.works.find(work_id.value);
                if (work == corpus.works.end()) {
                    inconsistent("Period '" + period.label + "' references missing work " +
                                 std::to_string(work_id.value));
                    continue;
                }
                if (!composer_works.contains(work_id.value))
                    inconsistent("Period '" + period.label + "' references work " +
                                 std::to_string(work_id.value) +
                                 " outside its composer membership");
                if (!work->second.metadata.period || *work->second.metadata.period != period.label)
                    inconsistent("Work " + std::to_string(work_id.value) +
                                 " period metadata disagrees with period '" + period.label + "'");

                const auto [membership, inserted] =
                    period_by_work.emplace(work_id.value, std::pair{composer_key, period.label});
                if (!inserted)
                    inconsistent("Work " + std::to_string(work_id.value) +
                                 " is referenced by multiple period memberships ('" +
                                 membership->second.second + "' and '" + period.label + "')");
            }
            validate_signature_examples(
                period.profile, period_works, "Period '" + period.label + "'");
        }

        auto prof_diags = validate_composer_profile(profile);
        diags.insert(diags.end(), prof_diags.begin(), prof_diags.end());
    }

    for (const auto& [work_key, work] : corpus.works) {
        const auto owner = owner_by_work.find(work_key);
        if (work.metadata.composer.value == 0) {
            if (owner != owner_by_work.end())
                inconsistent("Unassigned work " + std::to_string(work_key) +
                             " has a composer reverse reference");
            if (work.metadata.period)
                inconsistent("Unassigned work " + std::to_string(work_key) +
                             " declares period metadata");
        } else {
            if (!corpus.composers.contains(work.metadata.composer.value))
                inconsistent("Work " + std::to_string(work_key) + " references missing composer " +
                             std::to_string(work.metadata.composer.value));
            if (owner == owner_by_work.end() || owner->second != work.metadata.composer.value)
                inconsistent("Work " + std::to_string(work_key) +
                             " is absent from its metadata owner's work list");
        }

        const auto period = period_by_work.find(work_key);
        if (work.metadata.period && period == period_by_work.end())
            inconsistent("Work " + std::to_string(work_key) + " declares period '" +
                         *work.metadata.period + "' without a matching period membership");
        if (!work.metadata.period && period != period_by_work.end())
            inconsistent("Work " + std::to_string(work_key) +
                         " has period membership without period metadata");
    }

    // C15: deterministic aggregate profiles must match their analysed
    // memberships. Run only after C14 succeeds so missing or contradictory
    // references do not produce secondary freshness noise. Signature patterns
    // are excluded by style_profile_is_fresh because their explicit detection
    // lifecycle is independent of aggregate refresh.
    const bool graph_is_consistent = std::ranges::none_of(
        diags, [](const Diagnostic& diagnostic) { return diagnostic.rule == "C14"; });
    if (graph_is_consistent) {
        for (const auto& [composer_key, profile] : corpus.composers) {
            if (!style_profile_is_fresh(corpus, profile.works, profile.style_profile))
                diags.push_back(make_diag(ValidationSeverity::Error,
                                          "C15",
                                          "Composer " + std::to_string(composer_key) +
                                              " has a stale derived style profile",
                                          ErrorCode::CorpusStaleAggregate));
            for (const auto& period : profile.period_profiles) {
                if (!style_profile_is_fresh(corpus, period.works, period.profile))
                    diags.push_back(
                        make_diag(ValidationSeverity::Error,
                                  "C15",
                                  "Period '" + period.label + "' has a stale derived style profile",
                                  ErrorCode::CorpusStaleAggregate));
            }
        }
    }

    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

bool is_corpus_valid(const CorpusDatabase& corpus) {
    auto diags = validate_corpus(corpus);
    for (const auto& d : diags)
        if (d.severity == ValidationSeverity::Error) return false;
    return true;
}

} // namespace sunny::core
