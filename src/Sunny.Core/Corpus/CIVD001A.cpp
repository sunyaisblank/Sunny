/**
 * @file CIVD001A.cpp
 * @brief Corpus IR validation — implementation
 *
 * Component: CIVD001A
 *
 * Implements validation rules C1–C13 from Corpus Spec §8.
 */

#include "CIVD001A.h"
#include "../Score/SIVD001A.h"

#include <algorithm>
#include <cmath>

namespace Sunny::Core {

namespace {

Diagnostic make_diag(ValidationSeverity sev, const std::string& rule,
                     const std::string& msg, int code) {
    return {sev, rule, msg, std::nullopt, std::nullopt, code};
}

}  // anonymous namespace

std::vector<Diagnostic> validate_ingested_work(const IngestedWork& work) {
    std::vector<Diagnostic> diags;
    const auto& ic = work.ingestion_confidence;

    // C1: Ingestion confidence below 0.7
    if (ic.key_confidence < 0.7f || ic.metre_confidence < 0.7f ||
        ic.spelling_confidence < 0.7f || ic.voice_separation_confidence < 0.7f) {
        diags.push_back(make_diag(ValidationSeverity::Warning, "C1",
            "Ingestion confidence below 0.7 in one or more dimensions",
            CorpusError::LowIngestionConfidence));
    }

    // C3: Key estimation confidence below 0.5
    if (ic.key_confidence < 0.5f) {
        diags.push_back(make_diag(ValidationSeverity::Warning, "C3",
            "Key estimation confidence below 0.5; key may be incorrect",
            CorpusError::LowKeyConfidence));
    }

    // C2: Score validation failed (when Score is present)
    if (work.score) {
        auto score_diags = validate_structural(*work.score);
        for (const auto& sd : score_diags) {
            if (sd.severity == ValidationSeverity::Error) {
                diags.push_back(make_diag(ValidationSeverity::Error, "C2",
                    "Embedded Score fails structural validation: " + sd.message,
                    CorpusError::ScoreValidationFailed));
                break;
            }
        }
    }

    // C4: Inferred time signature
    if (ic.metre_confidence < 1.0f && ic.source_format == "midi") {
        diags.push_back(make_diag(ValidationSeverity::Info, "C4",
            "Time signature inferred from content (MIDI source)",
            CorpusError::InferredTimeSig));
    }

    // C5: Excessive voices (when Score is present, > 6 voices in any measure)
    if (work.score) {
        for (const auto& part : work.score->parts) {
            for (const auto& measure : part.measures) {
                if (measure.voices.size() > 6) {
                    diags.push_back(make_diag(ValidationSeverity::Warning, "C5",
                        "Measure " + std::to_string(measure.bar_number) +
                        " has " + std::to_string(measure.voices.size()) + " voices (> 6)",
                        CorpusError::ExcessiveVoices));
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
            diags.push_back(make_diag(ValidationSeverity::Error, "C6",
                "Harmonic analysis coverage below 80% of the work",
                CorpusError::LowHarmonicCoverage));
        }
    }

    // C7: Over-segmentation (sections shorter than 4 bars)
    for (const auto& sec : work.analysis.formal_analysis.section_plan) {
        if (sec.length_bars > 0 && sec.length_bars < 4) {
            diags.push_back(make_diag(ValidationSeverity::Warning, "C7",
                "Formal section '" + sec.label + "' is shorter than 4 bars",
                CorpusError::OverSegmentation));
            break;  // report once
        }
    }

    // C8: No thematic units identified
    if (work.analysis.motivic_analysis.thematic_units.empty() &&
        total_bars > 8) {
        diags.push_back(make_diag(ValidationSeverity::Warning, "C8",
            "No thematic units identified in the work",
            CorpusError::NoThematicUnits));
    }

    // C9: Single-instrument work (no orchestration analysis possible)
    if (!work.analysis.orchestration_analysis.has_value() &&
        work.analysis.melodic_analysis.per_voice_analysis.size() > 1) {
        diags.push_back(make_diag(ValidationSeverity::Info, "C9",
            "Orchestration analysis not available",
            CorpusError::SingleInstrument));
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
        diags.push_back(make_diag(ValidationSeverity::Warning, "C10",
            "Style profile based on fewer than 5 works (" +
                std::to_string(profile.works.size()) + ")",
            CorpusError::SmallCorpus));
    }
    // C11: Moderate corpus (fewer than 10 works)
    else if (profile.works.size() < 10) {
        diags.push_back(make_diag(ValidationSeverity::Info, "C11",
            "Style profile based on fewer than 10 works (" +
                std::to_string(profile.works.size()) + ")",
            CorpusError::ModerateCorpus));
    }

    // C12: Small period corpus
    for (const auto& period : profile.period_profiles) {
        if (period.works.size() < 3) {
            diags.push_back(make_diag(ValidationSeverity::Warning, "C12",
                "Period '" + period.label + "' has fewer than 3 works (" +
                    std::to_string(period.works.size()) + ")",
                CorpusError::SmallPeriodCorpus));
        }
    }

    // C13: Weak signature patterns
    for (const auto& pat : profile.style_profile.signature_patterns) {
        if (pat.distinctiveness < 1.5f) {
            diags.push_back(make_diag(ValidationSeverity::Info, "C13",
                "Signature pattern '" + pat.description +
                    "' has low distinctiveness (" +
                    std::to_string(pat.distinctiveness) + " std devs)",
                CorpusError::WeakSignature));
        }
    }

    std::sort(diags.begin(), diags.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });

    return diags;
}

std::vector<Diagnostic> validate_corpus(const CorpusDatabase& corpus) {
    std::vector<Diagnostic> diags;

    for (const auto& [_, work] : corpus.works) {
        auto work_diags = validate_ingested_work(work);
        diags.insert(diags.end(), work_diags.begin(), work_diags.end());
    }

    for (const auto& [_, profile] : corpus.composers) {
        auto prof_diags = validate_composer_profile(profile);
        diags.insert(diags.end(), prof_diags.begin(), prof_diags.end());
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

}  // namespace Sunny::Core
