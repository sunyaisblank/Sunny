/**
 * @file CIWF001A.cpp
 * @brief Corpus IR workflow functions — implementation
 *
 * Component: CIWF001A
 *
 * Tests: TSCI005A
 */

#include "CIWF001A.h"
#include "CIAN001A.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace Sunny::Core {

namespace {

ErrorCode not_found() {
    return static_cast<ErrorCode>(CorpusError::NotFound);
}

ErrorCode duplicate_id() {
    return static_cast<ErrorCode>(CorpusError::DuplicateId);
}

ComposerProfile* find_composer(CorpusDatabase& corpus, ComposerProfileId id) {
    auto it = corpus.composers.find(id.value);
    return it != corpus.composers.end() ? &it->second : nullptr;
}

const ComposerProfile* find_composer(const CorpusDatabase& corpus, ComposerProfileId id) {
    auto it = corpus.composers.find(id.value);
    return it != corpus.composers.end() ? &it->second : nullptr;
}

IngestedWork* find_work(CorpusDatabase& corpus, IngestedWorkId id) {
    auto it = corpus.works.find(id.value);
    return it != corpus.works.end() ? &it->second : nullptr;
}

const IngestedWork* find_work(const CorpusDatabase& corpus, IngestedWorkId id) {
    auto it = corpus.works.find(id.value);
    return it != corpus.works.end() ? &it->second : nullptr;
}

/// Compute Jensen-Shannon divergence between two normalised distributions.
/// Maps are keyed by the same type; missing keys have probability 0.
template<typename K>
float js_divergence(const std::map<K, float>& p, const std::map<K, float>& q) {
    // Collect all keys
    std::map<K, std::pair<float, float>> combined;
    for (const auto& [k, v] : p) combined[k].first = v;
    for (const auto& [k, v] : q) combined[k].second = v;

    float div = 0.0f;
    for (const auto& [_, pq] : combined) {
        float pi = pq.first;
        float qi = pq.second;
        float mi = 0.5f * (pi + qi);
        if (mi > 0.0f) {
            if (pi > 0.0f) div += pi * std::log2(pi / mi);
            if (qi > 0.0f) div += qi * std::log2(qi / mi);
        }
    }
    return div * 0.5f;  // symmetric
}

/// Aggregate harmonic data from work analyses into a style profile.
void aggregate_harmonic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::map<std::string, std::uint32_t> total_chord_counts;
    float total_hr_mean = 0.0f;
    float total_modulations = 0.0f;

    for (const auto* a : analyses) {
        for (const auto& [chord, count] : a->harmonic_analysis.chord_vocabulary)
            total_chord_counts[chord] += count;
        total_hr_mean += a->harmonic_analysis.harmonic_rhythm.mean_rate;
        total_modulations += static_cast<float>(
            a->harmonic_analysis.modulation_inventory.size());
    }

    // Chord frequency (normalised)
    std::uint32_t grand_total = 0;
    for (const auto& [_, c] : total_chord_counts) grand_total += c;
    out.harmonic_profile.chord_vocabulary_size =
        static_cast<std::uint32_t>(total_chord_counts.size());
    if (grand_total > 0) {
        for (const auto& [chord, count] : total_chord_counts)
            out.harmonic_profile.chord_frequency[chord] =
                static_cast<float>(count) / static_cast<float>(grand_total);
    }

    auto n = static_cast<float>(analyses.size());
    out.harmonic_profile.harmonic_rhythm_mean = total_hr_mean / n;
    out.harmonic_profile.modulation_frequency = total_modulations / n;
}

/// Aggregate melodic data.
void aggregate_melodic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::map<std::int8_t, std::uint32_t> total_intervals;
    float total_conjunct = 0.0f;
    float total_chromaticism = 0.0f;

    for (const auto* a : analyses) {
        for (const auto& voice : a->melodic_analysis.per_voice_analysis) {
            for (const auto& [interval, count] : voice.interval_distribution)
                total_intervals[interval] += count;
            total_conjunct += voice.conjunct_proportion;
            total_chromaticism += voice.chromaticism_rate;
        }
    }

    // Normalise intervals
    std::uint32_t interval_total = 0;
    for (const auto& [_, c] : total_intervals) interval_total += c;
    if (interval_total > 0) {
        for (const auto& [interval, count] : total_intervals)
            out.melodic_profile.interval_distribution[interval] =
                static_cast<float>(count) / static_cast<float>(interval_total);
    }

    std::size_t voice_count = 0;
    for (const auto* a : analyses)
        voice_count += a->melodic_analysis.per_voice_analysis.size();
    if (voice_count > 0) {
        out.melodic_profile.conjunct_proportion =
            total_conjunct / static_cast<float>(voice_count);
        out.melodic_profile.chromaticism_rate =
            total_chromaticism / static_cast<float>(voice_count);
    }
}

/// Aggregate rhythmic data.
void aggregate_rhythmic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_sync = 0.0f;
    float total_metric = 0.0f;

    for (const auto* a : analyses) {
        total_sync += a->rhythmic_analysis.syncopation_index;
        total_metric += a->rhythmic_analysis.metrical_complexity;
    }

    auto n = static_cast<float>(analyses.size());
    out.rhythmic_profile.syncopation_index = total_sync / n;
    out.rhythmic_profile.metrical_complexity = total_metric / n;
}

/// Aggregate formal data.
void aggregate_formal(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_length = 0.0f;
    std::map<std::uint8_t, std::uint32_t> form_counts;

    for (const auto* a : analyses) {
        total_length += static_cast<float>(a->formal_analysis.total_duration_bars);
        form_counts[static_cast<std::uint8_t>(a->formal_analysis.form_type)]++;
    }

    auto n = static_cast<float>(analyses.size());
    out.formal_profile.average_work_length = total_length / n;
    out.formal_profile.preferred_forms = form_counts;
}

/// Aggregate voice-leading data.
void aggregate_voice_leading(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_contrary = 0.0f;
    float total_common_tone = 0.0f;
    float total_independence = 0.0f;

    for (const auto* a : analyses) {
        total_contrary += a->voice_leading_analysis.contrary_motion_proportion;
        total_common_tone += a->voice_leading_analysis.common_tone_retention_rate;
        total_independence += a->voice_leading_analysis.average_voice_independence;
    }

    auto n = static_cast<float>(analyses.size());
    out.voice_leading_profile.common_tone_retention = total_common_tone / n;
    out.voice_leading_profile.voice_independence_index = total_independence / n;
}

/// Aggregate textural data.
void aggregate_textural(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_density = 0.0f;
    float total_span = 0.0f;
    float min_density = 1e9f;
    float max_density = 0.0f;
    std::map<std::string, float> total_texture_type;
    std::uint32_t texture_sources = 0;

    for (const auto* a : analyses) {
        total_density += a->textural_analysis.average_density;
        total_span += a->textural_analysis.average_register_span;

        for (const auto& [st, density] : a->textural_analysis.density_curve) {
            float d = static_cast<float>(density);
            if (d < min_density) min_density = d;
            if (d > max_density) max_density = d;
        }

        for (const auto& [type, prop] : a->textural_analysis.texture_type_proportions) {
            total_texture_type[type] += prop;
            texture_sources++;
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.textural_profile.average_density = total_density / n;
    out.textural_profile.register_span_preference = total_span / n;
    out.textural_profile.density_range_low = (min_density < 1e9f) ? min_density : 0.0f;
    out.textural_profile.density_range_high = max_density;

    if (texture_sources > 0) {
        for (const auto& [type, total] : total_texture_type)
            out.textural_profile.texture_type_distribution[type] = total / n;
    }
}

/// Dynamic level ordering for comparison.
int dynamic_ordinal(const std::string& name) {
    static const std::map<std::string, int> order = {
        {"pppp", 0}, {"ppp", 1}, {"pp", 2}, {"p", 3}, {"mp", 4},
        {"mf", 5}, {"f", 6}, {"ff", 7}, {"fff", 8}, {"ffff", 9}
    };
    auto it = order.find(name);
    return it != order.end() ? it->second : 5;
}

/// Aggregate dynamic data.
void aggregate_dynamic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    int lowest_ordinal = 10;
    int highest_ordinal = -1;
    std::string lowest_name;
    std::string highest_name;
    float total_change_rate = 0.0f;
    float total_subito = 0.0f;
    std::map<std::string, std::uint32_t> total_dist;

    for (const auto* a : analyses) {
        int lo = dynamic_ordinal(a->dynamic_analysis.dynamic_range_low);
        int hi = dynamic_ordinal(a->dynamic_analysis.dynamic_range_high);
        if (lo < lowest_ordinal) {
            lowest_ordinal = lo;
            lowest_name = a->dynamic_analysis.dynamic_range_low;
        }
        if (hi > highest_ordinal) {
            highest_ordinal = hi;
            highest_name = a->dynamic_analysis.dynamic_range_high;
        }
        total_change_rate += a->dynamic_analysis.dynamic_change_rate;
        total_subito += static_cast<float>(a->dynamic_analysis.subito_dynamics_count);

        for (const auto& [dyn, count] : a->dynamic_analysis.dynamic_distribution)
            total_dist[dyn] += count;
    }

    auto n = static_cast<float>(analyses.size());
    out.dynamic_profile.dynamic_range_low = lowest_name;
    out.dynamic_profile.dynamic_range_high = highest_name;
    out.dynamic_profile.dynamic_change_rate = total_change_rate / n;
    out.dynamic_profile.subito_frequency = total_subito / n;

    // Most frequent dynamic
    std::uint32_t max_count = 0;
    for (const auto& [dyn, count] : total_dist) {
        if (count > max_count) {
            max_count = count;
            out.dynamic_profile.most_frequent_dynamic = dyn;
        }
    }
}

/// Aggregate orchestration data.
void aggregate_orchestration(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    std::map<std::string, float> total_usage;
    std::map<std::string, float> total_melody;
    std::uint32_t orch_count = 0;

    for (const auto* a : analyses) {
        if (!a->orchestration_analysis) continue;
        orch_count++;
        for (const auto& [inst, usage] : a->orchestration_analysis->instrument_usage)
            total_usage[inst] += usage;
        for (const auto& [inst, prop] : a->orchestration_analysis->melody_carrier_distribution)
            total_melody[inst] += prop;
    }

    if (orch_count == 0) return;

    OrchestrationStyleProfile osp;
    auto n = static_cast<float>(orch_count);
    for (const auto& [inst, total] : total_usage)
        osp.preferred_instruments[inst] = total / n;
    for (const auto& [inst, total] : total_melody)
        osp.melody_assignment_preference[inst] = total / n;
    out.orchestration_profile = std::move(osp);
}

/// Aggregate motivic data.
void aggregate_motivic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_economy = 0.0f;
    float total_density = 0.0f;

    for (const auto* a : analyses) {
        total_economy += a->motivic_analysis.thematic_economy;
        total_density += a->motivic_analysis.thematic_density;
    }

    auto n = static_cast<float>(analyses.size());
    out.motivic_profile.thematic_economy = total_economy / n;
    out.motivic_profile.development_density = total_density / n;
}

}  // anonymous namespace

// =============================================================================
// Profile Management
// =============================================================================

ComposerProfile wf_create_composer_profile(
    ComposerProfileId id,
    const std::string& name
) {
    ComposerProfile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

IngestedWork wf_create_ingested_work(
    IngestedWorkId id,
    const WorkMetadata& metadata
) {
    IngestedWork work;
    work.id = id;
    work.metadata = metadata;
    return work;
}

Result<void> wf_assign_work_to_composer(
    CorpusDatabase& corpus,
    IngestedWorkId work_id,
    ComposerProfileId composer_id
) {
    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // Check not already assigned
    for (const auto& wid : composer->works)
        if (wid == work_id) return {};  // already assigned, idempotent

    composer->works.push_back(work_id);
    work->metadata.composer = composer_id;
    return {};
}

Result<void> wf_assign_work_to_period(
    CorpusDatabase& corpus,
    IngestedWorkId work_id,
    ComposerProfileId composer_id,
    const std::string& period_label
) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    // Find the period
    for (auto& period : composer->period_profiles) {
        if (period.label == period_label) {
            // Check not already assigned to this period
            for (const auto& wid : period.works)
                if (wid == work_id) return {};
            period.works.push_back(work_id);
            return {};
        }
    }
    return std::unexpected(not_found());
}

Result<void> wf_add_period_profile(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const PeriodProfile& period
) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // Check for duplicate period label
    for (const auto& p : composer->period_profiles)
        if (p.label == period.label) return std::unexpected(duplicate_id());

    composer->period_profiles.push_back(period);
    return {};
}

// =============================================================================
// Analysis
// =============================================================================

Result<void> wf_analyze_work(
    CorpusDatabase& corpus,
    IngestedWorkId work_id,
    const Score* score
) {
    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    const Score* effective_score = score;
    if (!effective_score && work->score)
        effective_score = &*work->score;

    if (effective_score)
        work->analysis = analyze_score(*effective_score);

    work->analysis_complete = true;
    return {};
}

Result<void> wf_rebuild_style_profile(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id
) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // Collect all analysed works
    std::vector<const WorkAnalysis*> analyses;
    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (work && work->analysis_complete)
            analyses.push_back(&work->analysis);
    }

    StyleProfile profile;
    profile.sample_size = static_cast<std::uint32_t>(analyses.size());

    if (!analyses.empty()) {
        // Confidence scales with sample size: 5 works = 0.5, 10 = 0.75, 20+ = 0.9+
        float n = static_cast<float>(analyses.size());
        profile.confidence = std::min(0.95f, 1.0f - 1.0f / (0.2f * n + 1.0f));

        aggregate_harmonic(profile, analyses);
        aggregate_melodic(profile, analyses);
        aggregate_rhythmic(profile, analyses);
        aggregate_formal(profile, analyses);
        aggregate_voice_leading(profile, analyses);
        aggregate_textural(profile, analyses);
        aggregate_dynamic(profile, analyses);
        aggregate_orchestration(profile, analyses);
        aggregate_motivic(profile, analyses);
    }

    composer->style_profile = std::move(profile);
    return {};
}

Result<void> wf_detect_signature_patterns(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id
) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // Signature pattern detection requires comparing the composer's
    // distributions against the corpus mean. For each chord progression,
    // interval, or rhythmic figure that appears significantly more often
    // for this composer than the corpus average, a SignaturePattern is
    // created with a distinctiveness score (z-score or frequency ratio).
    //
    // The current implementation identifies the most frequent chord
    // progressions as candidate signatures.
    std::vector<SignaturePattern> patterns;
    std::uint64_t pattern_id = 1;

    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (!work || !work->analysis_complete) continue;

        for (const auto& prog : work->analysis.harmonic_analysis.progression_inventory) {
            if (prog.occurrences.size() >= 3) {
                SignaturePattern pat;
                pat.id = SignaturePatternId{pattern_id++};
                pat.description = "Recurring progression";
                pat.domain = PatternDomain::Harmonic;
                pat.pattern_data = prog.roman_numerals;
                pat.distinctiveness = static_cast<float>(prog.occurrences.size());
                for (const auto& pos : prog.occurrences)
                    pat.examples.push_back({wid, pos});
                patterns.push_back(std::move(pat));
            }
        }
    }

    composer->style_profile.signature_patterns = std::move(patterns);
    return {};
}

// =============================================================================
// Query
// =============================================================================

Result<const StyleProfile*> wf_query_style_profile(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id
) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());
    return &composer->style_profile;
}

std::vector<AnnotatedExample> wf_find_examples(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::string& criterion
) {
    std::vector<AnnotatedExample> results;
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return results;

    // Search all works for passages matching the criterion.
    // The criterion is matched against section labels, thematic labels,
    // and analysis summaries. A production implementation would use
    // semantic matching; this provides keyword-based filtering.
    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (!work || !work->analysis_complete) continue;

        for (const auto& sec : work->analysis.formal_analysis.section_plan) {
            if (sec.label.find(criterion) != std::string::npos ||
                (sec.character && sec.character->find(criterion) != std::string::npos)) {
                AnnotatedExample ex;
                ex.work_id = wid;
                ex.region_start = ScoreTime{sec.start_bar, Beat{0, 1}};
                ex.region_end = ScoreTime{sec.end_bar, Beat{0, 1}};
                ex.relevance_score = 0.5f;
                ex.analysis_summary = sec.label;
                ex.formal_context = sec.label;
                results.push_back(std::move(ex));
            }
        }
    }

    // Sort by relevance
    std::sort(results.begin(), results.end(),
        [](const AnnotatedExample& a, const AnnotatedExample& b) {
            return a.relevance_score > b.relevance_score;
        });

    return results;
}

std::vector<AnnotatedExample> wf_get_progression_examples(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::vector<std::string>& roman_numerals
) {
    std::vector<AnnotatedExample> results;
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return results;

    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (!work || !work->analysis_complete) continue;

        for (const auto& prog : work->analysis.harmonic_analysis.progression_inventory) {
            if (prog.roman_numerals == roman_numerals) {
                for (const auto& pos : prog.occurrences) {
                    AnnotatedExample ex;
                    ex.work_id = wid;
                    ex.region_start = pos;
                    ex.region_end = pos;  // single point
                    ex.relevance_score = 1.0f;
                    ex.harmonic_reduction = roman_numerals;
                    results.push_back(std::move(ex));
                }
            }
        }
    }

    return results;
}

Result<FormalStyleProfile> wf_get_formal_template(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    FormClassification form_type
) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // Filter to works of the requested form type and aggregate
    std::vector<const WorkAnalysis*> matching;
    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (work && work->analysis_complete &&
            work->analysis.formal_analysis.form_type == form_type) {
            matching.push_back(&work->analysis);
        }
    }

    FormalStyleProfile result;
    result.preferred_forms[static_cast<std::uint8_t>(form_type)] =
        static_cast<std::uint32_t>(matching.size());

    if (!matching.empty()) {
        float total_length = 0.0f;
        for (const auto* a : matching)
            total_length += static_cast<float>(a->formal_analysis.total_duration_bars);
        result.average_work_length = total_length / static_cast<float>(matching.size());

        // Aggregate section proportions
        std::map<std::string, std::vector<float>> section_props;
        for (const auto* a : matching) {
            for (const auto& sp : a->formal_analysis.proportions)
                section_props[sp.label].push_back(sp.proportion);
        }
        for (const auto& [label, props] : section_props) {
            float sum = std::accumulate(props.begin(), props.end(), 0.0f);
            result.section_proportions[label] = sum / static_cast<float>(props.size());
        }
    }

    return result;
}

Result<HowWouldXHandleResult> wf_how_would_x_handle(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::string& situation
) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    HowWouldXHandleResult result;

    // Find relevant examples by keyword matching against formal sections
    result.relevant_examples = wf_find_examples(corpus, composer_id, situation);

    // Add statistical tendencies from the style profile
    const auto& sp = composer->style_profile;
    if (sp.sample_size > 0) {
        if (sp.harmonic_profile.harmonic_rhythm_mean > 0.0f) {
            Tendency t;
            t.domain = "harmonic";
            t.observation = "Average harmonic rhythm: " +
                std::to_string(sp.harmonic_profile.harmonic_rhythm_mean) +
                " changes per bar";
            t.confidence = sp.confidence;
            t.supporting_examples_count = sp.sample_size;
            result.statistical_tendencies.push_back(std::move(t));
        }

        if (sp.melodic_profile.conjunct_proportion > 0.0f) {
            Tendency t;
            t.domain = "melodic";
            t.observation = "Conjunct motion proportion: " +
                std::to_string(sp.melodic_profile.conjunct_proportion);
            t.confidence = sp.confidence;
            t.supporting_examples_count = sp.sample_size;
            result.statistical_tendencies.push_back(std::move(t));
        }
    }

    result.signature_patterns = sp.signature_patterns;

    return result;
}

// =============================================================================
// Comparison
// =============================================================================

Result<StyleComparison> wf_compare_composers(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_a,
    ComposerProfileId composer_b
) {
    const auto* a = find_composer(corpus, composer_a);
    if (!a) return std::unexpected(not_found());
    const auto* b = find_composer(corpus, composer_b);
    if (!b) return std::unexpected(not_found());

    StyleComparison comparison;
    comparison.composer_a = composer_a;
    comparison.composer_b = composer_b;

    // Harmonic divergence (JS divergence on chord frequency distributions)
    comparison.harmonic_divergence = js_divergence(
        a->style_profile.harmonic_profile.chord_frequency,
        b->style_profile.harmonic_profile.chord_frequency);

    // Melodic divergence (JS divergence on interval distributions)
    comparison.melodic_divergence = js_divergence(
        a->style_profile.melodic_profile.interval_distribution,
        b->style_profile.melodic_profile.interval_distribution);

    // Rhythmic divergence (JS divergence on duration distributions)
    comparison.rhythmic_divergence = js_divergence(
        a->style_profile.rhythmic_profile.duration_distribution,
        b->style_profile.rhythmic_profile.duration_distribution);

    // Formal divergence (Euclidean distance on scalar features)
    float formal_diff = std::abs(
        a->style_profile.formal_profile.average_work_length -
        b->style_profile.formal_profile.average_work_length);
    comparison.formal_divergence = std::min(1.0f, formal_diff / 500.0f);

    // Classify dimensions
    struct DimScore { std::string name; float score; };
    std::vector<DimScore> dims = {
        {"harmonic", comparison.harmonic_divergence},
        {"melodic", comparison.melodic_divergence},
        {"rhythmic", comparison.rhythmic_divergence},
        {"formal", comparison.formal_divergence}
    };
    std::sort(dims.begin(), dims.end(),
        [](const DimScore& x, const DimScore& y) { return x.score < y.score; });

    if (!dims.empty()) {
        comparison.most_similar_dimensions.push_back(dims.front().name);
        comparison.most_divergent_dimensions.push_back(dims.back().name);
    }

    return comparison;
}

Result<EvolutionaryAnalysis> wf_analyze_evolution(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id
) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    EvolutionaryAnalysis result;
    result.composer = composer_id;
    result.periods = composer->period_profiles;

    // Detect trends across periods (requires at least 2 periods)
    if (composer->period_profiles.size() >= 2) {
        const auto& early = composer->period_profiles.front().profile;
        const auto& late = composer->period_profiles.back().profile;

        // Harmonic complexity trend
        {
            EvolutionaryTrend trend;
            trend.dimension = "harmonic_vocabulary";
            trend.early_value = static_cast<float>(early.harmonic_profile.chord_vocabulary_size);
            trend.late_value = static_cast<float>(late.harmonic_profile.chord_vocabulary_size);
            if (trend.late_value > trend.early_value * 1.1f) {
                trend.direction = TrendDirection::Increasing;
                trend.description = "Harmonic vocabulary expands";
            } else if (trend.late_value < trend.early_value * 0.9f) {
                trend.direction = TrendDirection::Decreasing;
                trend.description = "Harmonic vocabulary contracts";
            } else {
                trend.direction = TrendDirection::Stable;
                trend.description = "Harmonic vocabulary remains stable";
            }
            result.trends.push_back(std::move(trend));
        }

        // Chromatic density trend
        {
            EvolutionaryTrend trend;
            trend.dimension = "chromatic_density";
            trend.early_value = early.harmonic_profile.chromatic_density;
            trend.late_value = late.harmonic_profile.chromatic_density;
            if (trend.late_value > trend.early_value + 0.05f) {
                trend.direction = TrendDirection::Increasing;
                trend.description = "Chromaticism increases";
            } else if (trend.late_value < trend.early_value - 0.05f) {
                trend.direction = TrendDirection::Decreasing;
                trend.description = "Chromaticism decreases";
            } else {
                trend.direction = TrendDirection::Stable;
                trend.description = "Chromatic density stable";
            }
            result.trends.push_back(std::move(trend));
        }
    }

    return result;
}

// =============================================================================
// Corpus-Level
// =============================================================================

std::vector<Diagnostic> wf_validate_corpus(const CorpusDatabase& corpus) {
    return validate_corpus(corpus);
}

}  // namespace Sunny::Core
