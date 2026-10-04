/**
 * @file workflows.cpp
 * @brief Corpus IR workflow functions — implementation
 *
 *
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <numeric>
#include <set>
#include <sunny/core/corpus/analysis.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <tuple>

namespace sunny::core {

namespace {

ErrorCode not_found() {
    return ErrorCode::CorpusNotFound;
}

ErrorCode duplicate_id() {
    return ErrorCode::CorpusDuplicateId;
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

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contains_case_insensitive(const std::string& value, const std::string& token) {
    return lowercase(value).contains(lowercase(token));
}

std::optional<std::uint8_t> cadence_code(const std::string& name) {
    if (name == "PAC") return 0;
    if (name == "IAC") return 1;
    if (name == "HC") return 2;
    if (name == "DC") return 3;
    if (name == "PC") return 4;
    if (name == "Phrygian") return 5;
    return std::nullopt;
}

bool has_melodic_evidence(const VoiceMelodicAnalysis& voice) {
    return voice.note_count > 0 || voice.range_low != 0 || voice.range_high != 127 ||
           !voice.interval_distribution.empty() || !voice.contour_inventory.empty() ||
           !voice.scale_degree_distribution.empty() || voice.leap_resolution_rate != 0.0f ||
           voice.conjunct_proportion != 0.0f || voice.chromaticism_rate != 0.0f;
}

std::optional<float>
pearson_correlation(const std::vector<std::pair<ScoreTime, std::uint8_t>>& density,
                    const std::vector<std::pair<ScoreTime, float>>& dynamics) {
    std::map<ScoreTime, float> dynamic_by_position;
    for (const auto& [position, value] : dynamics)
        dynamic_by_position[position] = value;

    std::vector<std::pair<float, float>> samples;
    for (const auto& [position, value] : density) {
        const auto found = dynamic_by_position.find(position);
        if (found != dynamic_by_position.end())
            samples.emplace_back(static_cast<float>(value), found->second);
    }
    if (samples.size() < 2) return std::nullopt;

    float x_mean = 0.0f;
    float y_mean = 0.0f;
    for (const auto& [x, y] : samples) {
        x_mean += x;
        y_mean += y;
    }
    x_mean /= static_cast<float>(samples.size());
    y_mean /= static_cast<float>(samples.size());

    float covariance = 0.0f;
    float x_variance = 0.0f;
    float y_variance = 0.0f;
    for (const auto& [x, y] : samples) {
        const auto x_delta = x - x_mean;
        const auto y_delta = y - y_mean;
        covariance += x_delta * y_delta;
        x_variance += x_delta * x_delta;
        y_variance += y_delta * y_delta;
    }
    if (x_variance == 0.0f || y_variance == 0.0f) return std::nullopt;
    return covariance / std::sqrt(x_variance * y_variance);
}

std::string nearest_dynamic(float intensity) {
    static constexpr std::array<std::pair<const char*, float>, 10> levels = {{{"pppp", 0.05f},
                                                                              {"ppp", 0.1f},
                                                                              {"pp", 0.2f},
                                                                              {"p", 0.3f},
                                                                              {"mp", 0.4f},
                                                                              {"mf", 0.55f},
                                                                              {"f", 0.7f},
                                                                              {"ff", 0.85f},
                                                                              {"fff", 0.95f},
                                                                              {"ffff", 1.0f}}};
    const auto found =
        std::ranges::min_element(levels, [intensity](const auto& lhs, const auto& rhs) {
            return std::abs(lhs.second - intensity) < std::abs(rhs.second - intensity);
        });
    return found->first;
}

ContourShape classify_dynamic_arc(const std::vector<std::pair<ScoreTime, float>>& shape) {
    if (shape.size() < 2) return ContourShape::Stationary;

    const auto [minimum, maximum] =
        std::ranges::minmax_element(shape, {}, [](const auto& sample) { return sample.second; });
    const auto range = maximum->second - minimum->second;
    if (range <= 1e-5f) return ContourShape::Stationary;

    const auto maximum_index = static_cast<std::size_t>(std::distance(shape.begin(), maximum));
    const auto minimum_index = static_cast<std::size_t>(std::distance(shape.begin(), minimum));
    const auto last = shape.size() - 1;
    const auto endpoint_threshold = range * 0.25f;
    if (maximum_index > 0 && maximum_index < last &&
        maximum->second - shape.front().second >= endpoint_threshold &&
        maximum->second - shape.back().second >= endpoint_threshold)
        return ContourShape::Arch;
    if (minimum_index > 0 && minimum_index < last &&
        shape.front().second - minimum->second >= endpoint_threshold &&
        shape.back().second - minimum->second >= endpoint_threshold)
        return ContourShape::InvertedArch;

    std::uint32_t direction_changes = 0;
    int previous_direction = 0;
    for (std::size_t i = 1; i < shape.size(); ++i) {
        const auto delta = shape[i].second - shape[i - 1].second;
        const int direction = delta > 1e-5f ? 1 : (delta < -1e-5f ? -1 : 0);
        if (direction != 0) {
            if (previous_direction != 0 && direction != previous_direction) direction_changes++;
            previous_direction = direction;
        }
    }
    if (direction_changes >= 2) return ContourShape::Oscillating;

    const auto displacement = shape.back().second - shape.front().second;
    if (displacement >= endpoint_threshold) return ContourShape::Ascending;
    if (displacement <= -endpoint_threshold) return ContourShape::Descending;
    return ContourShape::Complex;
}

/// Compute Jensen-Shannon divergence between two normalised distributions.
/// Maps are keyed by the same type; missing keys have probability 0.
template <typename K>
float js_divergence(const std::map<K, float>& p, const std::map<K, float>& q) {
    // Collect all keys
    std::map<K, std::pair<float, float>> combined;
    for (const auto& [k, v] : p)
        combined[k].first = v;
    for (const auto& [k, v] : q)
        combined[k].second = v;

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
    return div * 0.5f; // symmetric
}

/// Aggregate harmonic data from work analyses into a style profile.
void aggregate_harmonic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::map<std::string, std::uint32_t> total_chord_counts;
    struct ProgressionAggregate {
        std::uint32_t occurrences = 0;
        std::set<std::string> contexts;
    };
    std::map<std::vector<std::string>, ProgressionAggregate> progressions;
    std::map<std::uint8_t, std::uint32_t> modulation_techniques;
    std::map<std::string, std::uint32_t> key_relationships;
    std::map<std::string, std::uint32_t> key_signatures;
    std::map<std::uint8_t, std::uint32_t> cadence_types;
    float total_hr_mean = 0.0f;
    float total_hr_variance = 0.0f;
    float total_modulations = 0.0f;
    float total_chromatic_density = 0.0f;
    float total_secondary_dominants = 0.0f;
    float total_augmented_sixths = 0.0f;
    float total_neapolitans = 0.0f;
    float total_deceptive_cadences = 0.0f;
    std::uint32_t total_progression_occurrences = 0;
    std::uint32_t total_modulation_events = 0;
    std::uint32_t total_key_relationships = 0;
    std::uint32_t total_cadences = 0;

    for (const auto* a : analyses) {
        std::uint32_t work_chords = 0;
        for (const auto& [chord, count] : a->harmonic_analysis.chord_vocabulary) {
            total_chord_counts[chord] += count;
            work_chords += count;
        }
        for (const auto& progression : a->harmonic_analysis.progression_inventory) {
            const auto occurrences = static_cast<std::uint32_t>(progression.occurrences.size());
            if (occurrences == 0 || progression.roman_numerals.empty()) continue;
            auto& aggregate = progressions[progression.roman_numerals];
            aggregate.occurrences += occurrences;
            total_progression_occurrences += occurrences;
            if (!progression.key_context.empty())
                aggregate.contexts.insert(progression.key_context);
        }
        for (const auto& modulation : a->harmonic_analysis.modulation_inventory) {
            modulation_techniques[static_cast<std::uint8_t>(modulation.technique)]++;
            total_modulation_events++;
        }
        for (const auto& [key, relationship, _] : a->harmonic_analysis.tonal_plan.key_sequence) {
            if (!key.empty()) key_signatures[key]++;
            if (!relationship.empty()) {
                key_relationships[relationship]++;
                total_key_relationships++;
            }
        }
        for (const auto& cadence : a->harmonic_analysis.cadence_inventory) {
            if (const auto code = cadence_code(cadence.type)) {
                cadence_types[*code]++;
                total_cadences++;
            }
            if (cadence.type == "DC") total_deceptive_cadences += 1.0f;
        }
        for (const auto& event : a->harmonic_analysis.chromatic_techniques) {
            const auto type = lowercase(event.type);
            if (type.contains("secondary dominant")) total_secondary_dominants += 1.0f;
            if (type.contains("augmented sixth")) total_augmented_sixths += 1.0f;
            if (type.contains("neapolitan")) total_neapolitans += 1.0f;
        }
        if (work_chords > 0)
            total_chromatic_density +=
                static_cast<float>(a->harmonic_analysis.chromatic_techniques.size()) /
                static_cast<float>(work_chords);
        total_hr_mean += a->harmonic_analysis.harmonic_rhythm.mean_rate;
        total_hr_variance += a->harmonic_analysis.harmonic_rhythm.variance;
        total_modulations += static_cast<float>(a->harmonic_analysis.modulation_inventory.size());
    }

    // Chord frequency (normalised)
    std::uint32_t grand_total = 0;
    for (const auto& [_, c] : total_chord_counts)
        grand_total += c;
    out.harmonic_profile.chord_vocabulary_size =
        static_cast<std::uint32_t>(total_chord_counts.size());
    if (grand_total > 0) {
        for (const auto& [chord, count] : total_chord_counts)
            out.harmonic_profile.chord_frequency[chord] =
                static_cast<float>(count) / static_cast<float>(grand_total);
    }

    auto n = static_cast<float>(analyses.size());
    out.harmonic_profile.harmonic_rhythm_mean = total_hr_mean / n;
    out.harmonic_profile.harmonic_rhythm_variance = total_hr_variance / n;
    out.harmonic_profile.modulation_frequency = total_modulations / n;
    out.harmonic_profile.chromatic_density = total_chromatic_density / n;
    out.harmonic_profile.secondary_dominant_frequency = total_secondary_dominants / n;
    out.harmonic_profile.augmented_sixth_frequency = total_augmented_sixths / n;
    out.harmonic_profile.neapolitan_frequency = total_neapolitans / n;
    out.harmonic_profile.deceptive_cadence_frequency = total_deceptive_cadences / n;

    std::vector<std::pair<std::vector<std::string>, ProgressionAggregate>> ranked(
        progressions.begin(), progressions.end());
    std::ranges::sort(ranked, [](const auto& lhs, const auto& rhs) {
        if (lhs.second.occurrences != rhs.second.occurrences)
            return lhs.second.occurrences > rhs.second.occurrences;
        return lhs.first < rhs.first;
    });
    std::uint32_t rank = 1;
    for (const auto& [progression, aggregate] : ranked) {
        out.harmonic_profile.preferred_progressions.push_back(
            {progression,
             total_progression_occurrences > 0
                 ? static_cast<float>(aggregate.occurrences) /
                       static_cast<float>(total_progression_occurrences)
                 : 0.0f,
             rank++,
             std::vector<std::string>(aggregate.contexts.begin(), aggregate.contexts.end())});
    }
    if (total_modulation_events > 0)
        for (const auto& [technique, count] : modulation_techniques)
            out.harmonic_profile.modulation_technique_preference[technique] =
                static_cast<float>(count) / static_cast<float>(total_modulation_events);
    if (total_key_relationships > 0)
        for (const auto& [relationship, count] : key_relationships)
            out.harmonic_profile.preferred_key_relationships[relationship] =
                static_cast<float>(count) / static_cast<float>(total_key_relationships);
    out.harmonic_profile.preferred_key_signatures = std::move(key_signatures);
    if (total_cadences > 0)
        for (const auto& [type, count] : cadence_types)
            out.harmonic_profile.cadence_type_distribution[type] =
                static_cast<float>(count) / static_cast<float>(total_cadences);
}

/// Aggregate melodic data.
void aggregate_melodic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::map<std::int8_t, std::uint32_t> total_intervals;
    std::map<std::uint8_t, std::uint32_t> contour_counts;
    std::map<std::uint8_t, std::uint32_t> scale_degree_counts;
    float total_conjunct = 0.0f;
    float total_chromaticism = 0.0f;
    float total_range = 0.0f;
    std::uint32_t total_contours = 0;
    std::uint32_t total_scale_degrees = 0;
    std::uint32_t total_thematic_occurrences = 0;
    std::uint32_t sequential_occurrences = 0;
    std::size_t voice_count = 0;

    for (const auto* a : analyses) {
        for (const auto& voice : a->melodic_analysis.per_voice_analysis) {
            if (!has_melodic_evidence(voice)) continue;
            for (const auto& [interval, count] : voice.interval_distribution)
                total_intervals[interval] += count;
            for (const auto& contour : voice.contour_inventory) {
                contour_counts[static_cast<std::uint8_t>(contour.shape)]++;
                total_contours++;
            }
            for (const auto& [degree, count] : voice.scale_degree_distribution) {
                scale_degree_counts[degree] += count;
                total_scale_degrees += count;
            }
            total_conjunct += voice.conjunct_proportion;
            total_chromaticism += voice.chromaticism_rate;
            total_range += static_cast<float>(static_cast<int>(voice.range_high) -
                                              static_cast<int>(voice.range_low));
            voice_count++;
        }
        for (const auto& unit : a->melodic_analysis.thematic_material) {
            for (const auto& occurrence : unit.occurrences) {
                total_thematic_occurrences++;
                if (occurrence.transformation == ThematicTransformation::SequentialRepetition)
                    sequential_occurrences++;
            }
        }
    }

    // Normalise intervals
    std::uint32_t interval_total = 0;
    for (const auto& [_, c] : total_intervals)
        interval_total += c;
    if (interval_total > 0) {
        for (const auto& [interval, count] : total_intervals)
            out.melodic_profile.interval_distribution[interval] =
                static_cast<float>(count) / static_cast<float>(interval_total);

        std::vector<std::pair<std::int8_t, std::uint32_t>> ranked(total_intervals.begin(),
                                                                  total_intervals.end());
        std::ranges::sort(ranked, [](const auto& lhs, const auto& rhs) {
            if (lhs.second != rhs.second) return lhs.second > rhs.second;
            return lhs.first < rhs.first;
        });
        for (const auto& [interval, _] : ranked)
            out.melodic_profile.preferred_intervals.push_back(interval);
    }

    if (voice_count > 0) {
        out.melodic_profile.conjunct_proportion = total_conjunct / static_cast<float>(voice_count);
        out.melodic_profile.chromaticism_rate =
            total_chromaticism / static_cast<float>(voice_count);
        out.melodic_profile.typical_range = total_range / static_cast<float>(voice_count);
    }
    if (total_contours > 0)
        for (const auto& [shape, count] : contour_counts)
            out.melodic_profile.contour_preferences[shape] =
                static_cast<float>(count) / static_cast<float>(total_contours);
    if (total_scale_degrees > 0)
        for (const auto& [degree, count] : scale_degree_counts)
            out.melodic_profile.scale_degree_emphasis[degree] =
                static_cast<float>(count) / static_cast<float>(total_scale_degrees);
    if (total_thematic_occurrences > 0)
        out.melodic_profile.sequence_frequency = static_cast<float>(sequential_occurrences) /
                                                 static_cast<float>(total_thematic_occurrences);
}

/// Aggregate rhythmic data.
void aggregate_rhythmic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::map<std::string, std::uint32_t> duration_counts;
    std::map<std::string, std::uint32_t> metre_counts;
    std::vector<float> tempos;
    float total_sync = 0.0f;
    float total_metric = 0.0f;
    float total_rubato = 0.0f;
    std::uint32_t recurrent_motifs = 0;
    std::uint32_t total_motifs = 0;

    for (const auto* a : analyses) {
        for (const auto& [duration, count] : a->rhythmic_analysis.duration_distribution)
            duration_counts[duration] += count;
        for (const auto& [metre, count] : a->rhythmic_analysis.metre_distribution)
            metre_counts[metre] += count;
        for (const auto& [_, tempo] : a->rhythmic_analysis.tempo_profile)
            tempos.push_back(tempo);
        for (const auto& motif : a->rhythmic_analysis.rhythmic_motifs) {
            total_motifs++;
            if (motif.occurrences > 1) recurrent_motifs++;
        }
        total_sync += a->rhythmic_analysis.syncopation_index;
        total_metric += a->rhythmic_analysis.metrical_complexity;
        total_rubato += a->rhythmic_analysis.rubato_degree;
    }

    auto n = static_cast<float>(analyses.size());
    out.rhythmic_profile.syncopation_index = total_sync / n;
    out.rhythmic_profile.metrical_complexity = total_metric / n;
    out.rhythmic_profile.rubato_tendency = total_rubato / n;
    out.rhythmic_profile.preferred_metres = std::move(metre_counts);

    std::uint32_t duration_total = 0;
    for (const auto& [_, count] : duration_counts)
        duration_total += count;
    if (duration_total > 0) {
        std::vector<std::pair<std::string, std::uint32_t>> ranked(duration_counts.begin(),
                                                                  duration_counts.end());
        std::ranges::sort(ranked, [](const auto& lhs, const auto& rhs) {
            if (lhs.second != rhs.second) return lhs.second > rhs.second;
            return lhs.first < rhs.first;
        });
        for (const auto& [duration, count] : ranked) {
            const auto probability = static_cast<float>(count) / static_cast<float>(duration_total);
            out.rhythmic_profile.duration_distribution[duration] = probability;
            out.rhythmic_profile.preferred_durations.push_back(duration);
            out.rhythmic_profile.rhythmic_variety -= probability * std::log2(probability);
        }
    }
    if (!tempos.empty()) {
        const auto tempo_total = std::accumulate(tempos.begin(), tempos.end(), 0.0f);
        out.rhythmic_profile.tempo_mean = tempo_total / static_cast<float>(tempos.size());
        float squared_deviation = 0.0f;
        for (const auto tempo : tempos) {
            const auto difference = tempo - out.rhythmic_profile.tempo_mean;
            squared_deviation += difference * difference;
        }
        out.rhythmic_profile.tempo_stddev =
            std::sqrt(squared_deviation / static_cast<float>(tempos.size()));
    }
    if (total_motifs > 0)
        out.rhythmic_profile.rhythmic_motif_consistency =
            static_cast<float>(recurrent_motifs) / static_cast<float>(total_motifs);
}

/// Aggregate formal data.
void aggregate_formal(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_length = 0.0f;
    float total_climax = 0.0f;
    float total_golden_adherence = 0.0f;
    std::uint32_t climax_sources = 0;
    float ratio_total = 0.0f;
    float development_total = 0.0f;
    std::uint32_t ratio_sources = 0;
    std::uint32_t development_sources = 0;
    std::uint32_t introductions = 0;
    std::uint32_t codas = 0;
    std::map<std::uint8_t, std::uint32_t> form_counts;
    std::map<std::string, std::pair<float, std::uint32_t>> section_proportions;
    std::map<std::string, std::uint32_t> transitions;

    for (const auto* a : analyses) {
        total_length += static_cast<float>(a->formal_analysis.total_duration_bars);
        if (!a->dynamic_analysis.dynamic_shape.empty() ||
            a->dynamic_analysis.climax_position != 0.0f) {
            total_climax += a->dynamic_analysis.climax_position;
            total_golden_adherence += std::clamp(
                1.0f - std::abs(a->dynamic_analysis.climax_position - 0.618f) / 0.618f, 0.0f, 1.0f);
            climax_sources++;
        }
        form_counts[static_cast<std::uint8_t>(a->formal_analysis.form_type)]++;

        float exposition = 0.0f;
        float recapitulation = 0.0f;
        float development = 0.0f;
        for (const auto& proportion : a->formal_analysis.proportions) {
            auto& aggregate = section_proportions[proportion.label];
            aggregate.first += proportion.proportion;
            aggregate.second++;
            if (contains_case_insensitive(proportion.label, "exposition"))
                exposition += proportion.proportion;
            if (contains_case_insensitive(proportion.label, "recapitulation"))
                recapitulation += proportion.proportion;
            if (contains_case_insensitive(proportion.label, "development"))
                development += proportion.proportion;
        }
        if (exposition > 0.0f && recapitulation > 0.0f) {
            ratio_total += recapitulation / exposition;
            ratio_sources++;
        }
        if (development > 0.0f) {
            development_total += development;
            development_sources++;
        }

        for (const auto& section : a->formal_analysis.section_plan) {
            if (contains_case_insensitive(section.label, "transition") && section.character &&
                !section.character->empty())
                transitions[*section.character]++;
        }
        if (!a->formal_analysis.section_plan.empty()) {
            const auto first = std::ranges::min_element(
                a->formal_analysis.section_plan, {}, &FormalSection::start_bar);
            const auto last = std::ranges::max_element(
                a->formal_analysis.section_plan, {}, &FormalSection::end_bar);
            if (contains_case_insensitive(last->label, "coda")) codas++;
            if (contains_case_insensitive(first->label, "introduction")) {
                const FormalSection* following = nullptr;
                for (const auto& candidate : a->formal_analysis.section_plan)
                    if (candidate.start_bar > first->start_bar &&
                        (!following || candidate.start_bar < following->start_bar))
                        following = &candidate;
                if (following && first->tempo > 0.0f && following->tempo > 0.0f &&
                    first->tempo < following->tempo)
                    introductions++;
            }
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.formal_profile.average_work_length = total_length / n;
    out.formal_profile.preferred_forms = form_counts;
    out.formal_profile.introduction_frequency = static_cast<float>(introductions) / n;
    out.formal_profile.coda_frequency = static_cast<float>(codas) / n;
    if (climax_sources > 0) {
        out.formal_profile.climax_placement = total_climax / static_cast<float>(climax_sources);
        out.formal_profile.golden_ratio_adherence =
            total_golden_adherence / static_cast<float>(climax_sources);
    }
    for (const auto& [label, aggregate] : section_proportions)
        out.formal_profile.section_proportions[label] =
            aggregate.first / static_cast<float>(aggregate.second);
    if (ratio_sources > 0)
        out.formal_profile.exposition_recapitulation_ratio =
            ratio_total / static_cast<float>(ratio_sources);
    if (development_sources > 0)
        out.formal_profile.development_proportion =
            development_total / static_cast<float>(development_sources);
    std::vector<std::pair<std::string, std::uint32_t>> ranked_transitions(transitions.begin(),
                                                                          transitions.end());
    std::ranges::sort(ranked_transitions, [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second) return lhs.second > rhs.second;
        return lhs.first < rhs.first;
    });
    for (const auto& [technique, _] : ranked_transitions)
        out.formal_profile.transition_technique.push_back(technique);
}

/// Aggregate voice-leading data.
void aggregate_voice_leading(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    std::array<float, 4> total_motion{};
    float total_parallel_fifths = 0.0f;
    float total_parallel_octaves = 0.0f;
    float total_voice_crossings = 0.0f;
    float total_common_tone = 0.0f;
    float total_independence = 0.0f;
    float leading_resolution_sum = 0.0f;
    float seventh_resolution_sum = 0.0f;
    std::uint32_t leading_resolution_weight = 0;
    std::uint32_t seventh_resolution_weight = 0;
    std::uint32_t close_spacing = 0;
    std::uint32_t open_spacing = 0;

    for (const auto* a : analyses) {
        const auto& voice = a->voice_leading_analysis;
        total_parallel_fifths += static_cast<float>(voice.parallel_fifths_count);
        total_parallel_octaves += static_cast<float>(voice.parallel_octaves_count);
        total_voice_crossings += static_cast<float>(voice.voice_crossing_count);
        total_motion[0] += voice.contrary_motion_proportion;
        total_motion[1] += voice.oblique_motion_proportion;
        total_motion[2] += voice.similar_motion_proportion;
        total_motion[3] += voice.parallel_motion_proportion;
        total_common_tone += voice.common_tone_retention_rate;
        total_independence += voice.average_voice_independence;

        for (const auto& pattern : voice.resolution_patterns) {
            const auto tendency = lowercase(pattern.tendency_tone);
            if (tendency.contains("leading") || tendency.contains("scale degree 7")) {
                leading_resolution_sum +=
                    pattern.proportion_resolved * static_cast<float>(pattern.frequency);
                leading_resolution_weight += pattern.frequency;
            }
            if (tendency.contains("seventh") || tendency.contains("7th")) {
                seventh_resolution_sum +=
                    pattern.proportion_resolved * static_cast<float>(pattern.frequency);
                seventh_resolution_weight += pattern.frequency;
            }
        }
        for (const auto& [spacing, count] : voice.spacing_distribution) {
            if (spacing <= 12)
                close_spacing += count;
            else
                open_spacing += count;
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.voice_leading_profile.parallel_fifths_tolerance = total_parallel_fifths / n;
    out.voice_leading_profile.parallel_octaves_tolerance = total_parallel_octaves / n;
    out.voice_leading_profile.voice_crossing_tolerance = total_voice_crossings / n;
    out.voice_leading_profile.common_tone_retention = total_common_tone / n;
    out.voice_leading_profile.voice_independence_index = total_independence / n;

    static constexpr std::array<const char*, 4> motion_names = {
        "contrary", "oblique", "similar", "parallel"};
    const auto motion = std::ranges::max_element(total_motion);
    if (motion != total_motion.end() && *motion > 0.0f)
        out.voice_leading_profile.preferred_motion_type =
            motion_names[static_cast<std::size_t>(std::distance(total_motion.begin(), motion))];
    if (leading_resolution_weight > 0)
        out.voice_leading_profile.leading_tone_resolution_rate =
            leading_resolution_sum / static_cast<float>(leading_resolution_weight);
    if (seventh_resolution_weight > 0)
        out.voice_leading_profile.seventh_resolution_rate =
            seventh_resolution_sum / static_cast<float>(seventh_resolution_weight);

    const auto spacing_total = close_spacing + open_spacing;
    if (spacing_total > 0) {
        const auto close_proportion =
            static_cast<float>(close_spacing) / static_cast<float>(spacing_total);
        out.voice_leading_profile.spacing_preference =
            close_proportion >= 0.6f ? "close" : (close_proportion <= 0.4f ? "open" : "mixed");
    }
}

/// Aggregate textural data.
void aggregate_textural(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_density = 0.0f;
    float total_span = 0.0f;
    float min_density = 1e9f;
    float max_density = 0.0f;
    std::map<std::string, float> total_texture_type;
    float total_correlation = 0.0f;
    std::uint32_t correlation_sources = 0;

    for (const auto* a : analyses) {
        total_density += a->textural_analysis.average_density;
        total_span += a->textural_analysis.average_register_span;

        for (const auto& [_, density] : a->textural_analysis.density_curve) {
            float d = static_cast<float>(density);
            if (d < min_density) min_density = d;
            if (d > max_density) max_density = d;
        }

        for (const auto& [type, prop] : a->textural_analysis.texture_type_proportions)
            total_texture_type[type] += prop;
        if (const auto correlation = pearson_correlation(a->textural_analysis.density_curve,
                                                         a->dynamic_analysis.dynamic_shape)) {
            total_correlation += *correlation;
            correlation_sources++;
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.textural_profile.average_density = total_density / n;
    out.textural_profile.register_span_preference = total_span / n;
    out.textural_profile.density_range_low = (min_density < 1e9f) ? min_density : 0.0f;
    out.textural_profile.density_range_high = max_density;

    for (const auto& [type, total] : total_texture_type)
        out.textural_profile.texture_type_distribution[type] = total / n;
    if (correlation_sources > 0)
        out.textural_profile.density_dynamic_correlation =
            total_correlation / static_cast<float>(correlation_sources);
}

std::optional<float> dynamic_intensity(const std::string& name) {
    static const std::map<std::string, float> intensity = {{"pppp", 0.05f},
                                                           {"ppp", 0.1f},
                                                           {"pp", 0.2f},
                                                           {"p", 0.3f},
                                                           {"mp", 0.4f},
                                                           {"mf", 0.55f},
                                                           {"f", 0.7f},
                                                           {"fp", 0.7f},
                                                           {"sfp", 0.8f},
                                                           {"ff", 0.85f},
                                                           {"rfz", 0.85f},
                                                           {"sfz", 0.9f},
                                                           {"fff", 0.95f},
                                                           {"ffff", 1.0f}};
    const auto found = intensity.find(name);
    return found != intensity.end() ? std::optional<float>{found->second} : std::nullopt;
}

/// Aggregate dynamic data.
void aggregate_dynamic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float lowest_intensity = 2.0f;
    float highest_intensity = -1.0f;
    std::string lowest_name;
    std::string highest_name;
    float total_change_rate = 0.0f;
    float total_subito = 0.0f;
    std::map<std::string, std::uint32_t> total_dist;
    std::map<std::string, std::uint32_t> climax_dynamics;
    std::map<ContourShape, std::uint32_t> arc_shapes;

    for (const auto* a : analyses) {
        const auto& dynamics = a->dynamic_analysis;
        if (const auto low = dynamic_intensity(dynamics.dynamic_range_low)) {
            if (*low < lowest_intensity) {
                lowest_intensity = *low;
                lowest_name = dynamics.dynamic_range_low;
            }
        }
        if (const auto high = dynamic_intensity(dynamics.dynamic_range_high)) {
            if (*high > highest_intensity) {
                highest_intensity = *high;
                highest_name = dynamics.dynamic_range_high;
            }
        }
        total_change_rate += dynamics.dynamic_change_rate;
        total_subito += static_cast<float>(dynamics.subito_dynamics_count);

        for (const auto& [dyn, count] : dynamics.dynamic_distribution)
            total_dist[dyn] += count;
        if (!dynamics.dynamic_shape.empty()) {
            const auto climax = std::ranges::max_element(
                dynamics.dynamic_shape, {}, [](const auto& sample) { return sample.second; });
            climax_dynamics[nearest_dynamic(climax->second)]++;
            arc_shapes[classify_dynamic_arc(dynamics.dynamic_shape)]++;
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.dynamic_profile.dynamic_range_low = lowest_name;
    out.dynamic_profile.dynamic_range_high = highest_name;
    out.dynamic_profile.dynamic_change_rate = total_change_rate / n;
    out.dynamic_profile.subito_frequency = total_subito / n;

    const auto most_frequent = std::ranges::max_element(
        total_dist, [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
    if (most_frequent != total_dist.end())
        out.dynamic_profile.most_frequent_dynamic = most_frequent->first;

    const auto climax_dynamic = std::ranges::max_element(
        climax_dynamics, [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
    if (climax_dynamic != climax_dynamics.end())
        out.dynamic_profile.climax_dynamic = climax_dynamic->first;

    const auto arc_shape = std::ranges::max_element(
        arc_shapes, [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });
    if (arc_shape != arc_shapes.end()) out.dynamic_profile.dynamic_arc_shape = arc_shape->first;
}

/// Aggregate orchestration data.
void aggregate_orchestration(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    std::map<std::string, float> total_usage;
    std::map<std::string, float> total_melody;
    using CombinationKey =
        std::tuple<std::vector<std::string>, std::string, std::optional<std::int8_t>>;
    std::map<CombinationKey, std::uint32_t> combinations;
    using DoublingKey = std::tuple<std::string, std::string, std::int8_t>;
    std::map<DoublingKey, std::uint32_t> doublings;
    std::set<std::string> build_up_techniques;
    std::uint32_t orch_count = 0;

    for (const auto* a : analyses) {
        if (!a->orchestration_analysis) continue;
        const auto& orchestration = *a->orchestration_analysis;
        orch_count++;
        for (const auto& [inst, usage] : orchestration.instrument_usage)
            total_usage[inst] += usage;
        for (const auto& [inst, prop] : orchestration.melody_carrier_distribution)
            total_melody[inst] += prop;
        for (const auto& combination : orchestration.instrument_combinations) {
            auto instruments = combination.instruments;
            std::ranges::sort(instruments);
            combinations[{std::move(instruments),
                          combination.typical_context,
                          combination.interval_relationship}] += combination.frequency;
        }
        for (const auto& doubling : orchestration.doubling_patterns)
            doublings[{
                doubling.source_instrument, doubling.doubling_instrument, doubling.interval}] +=
                doubling.frequency;
        for (const auto& crescendo : orchestration.orchestral_crescendo_patterns) {
            if (crescendo.register_expansion) build_up_techniques.insert("register expansion");
            if (crescendo.instrument_entry_order.size() > 1)
                build_up_techniques.insert("instrument accretion");
        }
    }

    if (orch_count == 0) return;

    OrchestrationStyleProfile osp;
    auto n = static_cast<float>(orch_count);
    for (const auto& [inst, total] : total_usage)
        osp.preferred_instruments[inst] = total / n;
    for (const auto& [inst, total] : total_melody)
        osp.melody_assignment_preference[inst] = total / n;
    for (const auto& [key, frequency] : combinations) {
        const auto& [instruments, context, interval] = key;
        osp.signature_combinations.push_back({instruments, frequency, context, interval});
    }
    for (const auto& [key, frequency] : doublings) {
        const auto& [source, doubling, interval] = key;
        osp.doubling_preferences.push_back({source, doubling, interval, frequency});
    }
    osp.build_up_technique.assign(build_up_techniques.begin(), build_up_techniques.end());
    out.orchestration_profile = std::move(osp);
}

/// Aggregate motivic data.
void aggregate_motivic(StyleProfile& out, const std::vector<const WorkAnalysis*>& analyses) {
    if (analyses.empty()) return;

    float total_economy = 0.0f;
    float total_density = 0.0f;
    std::map<std::uint8_t, std::uint32_t> transformations;
    std::uint32_t total_transformations = 0;
    std::uint32_t fragmentations = 0;
    std::uint32_t sequences = 0;

    for (const auto* a : analyses) {
        total_economy += a->motivic_analysis.thematic_economy;
        total_density += a->motivic_analysis.thematic_density;
        for (const auto& event : a->motivic_analysis.transformation_inventory) {
            transformations[static_cast<std::uint8_t>(event.transformation)]++;
            total_transformations++;
            if (event.transformation == ThematicTransformation::Fragmented) fragmentations++;
            if (event.transformation == ThematicTransformation::SequentialRepetition) sequences++;
        }
    }

    auto n = static_cast<float>(analyses.size());
    out.motivic_profile.thematic_economy = total_economy / n;
    out.motivic_profile.development_density = total_density / n;
    if (total_transformations > 0) {
        for (const auto& [transformation, count] : transformations)
            out.motivic_profile.preferred_transformations[transformation] =
                static_cast<float>(count) / static_cast<float>(total_transformations);
        out.motivic_profile.fragmentation_frequency =
            static_cast<float>(fragmentations) / static_cast<float>(total_transformations);
        out.motivic_profile.sequence_frequency =
            static_cast<float>(sequences) / static_cast<float>(total_transformations);
    }
}

std::vector<const WorkAnalysis*> collect_analyses(const CorpusDatabase& corpus,
                                                  const std::vector<IngestedWorkId>& work_ids) {
    std::vector<const WorkAnalysis*> analyses;
    std::set<std::uint64_t> seen;
    for (const auto work_id : work_ids) {
        if (!seen.insert(work_id.value).second) continue;
        const auto* work = find_work(corpus, work_id);
        if (work && work->analysis_complete) analyses.push_back(&work->analysis);
    }
    return analyses;
}

StyleProfile build_style_profile(const std::vector<const WorkAnalysis*>& analyses) {
    StyleProfile profile;
    profile.sample_size = static_cast<std::uint32_t>(analyses.size());

    if (!analyses.empty()) {
        // Confidence scales with sample size: 5 works = 0.5, 10 = 0.75, 20+ = 0.9+
        const float n = static_cast<float>(analyses.size());
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

    return profile;
}

void refresh_period_aggregates(const CorpusDatabase& corpus, ComposerProfile& composer) {
    for (auto& period : composer.period_profiles)
        period.profile = build_style_profile(collect_analyses(corpus, period.works));
}

void refresh_composer_aggregates(const CorpusDatabase& corpus, ComposerProfile& composer) {
    composer.style_profile = build_style_profile(collect_analyses(corpus, composer.works));
    refresh_period_aggregates(corpus, composer);
}

void refresh_all_aggregates(CorpusDatabase& corpus) {
    for (auto& [_, composer] : corpus.composers)
        refresh_composer_aggregates(corpus, composer);
}

void refresh_all_period_aggregates(CorpusDatabase& corpus) {
    for (auto& [_, composer] : corpus.composers)
        refresh_period_aggregates(corpus, composer);
}

bool bounded_ranges_overlap(const PeriodProfile& lhs, const PeriodProfile& rhs) {
    if (lhs.year_start == 0 || lhs.year_end == 0 || rhs.year_start == 0 || rhs.year_end == 0)
        return false;
    return std::max(lhs.year_start, rhs.year_start) <= std::min(lhs.year_end, rhs.year_end);
}

} // anonymous namespace

// =============================================================================
// Profile Management
// =============================================================================

ComposerProfile create_composer_profile(ComposerProfileId id, const std::string& name) {
    ComposerProfile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

IngestedWork create_ingested_work(IngestedWorkId id, const WorkMetadata& metadata) {
    IngestedWork work;
    work.id = id;
    work.metadata = metadata;
    return work;
}

Result<void> assign_work_to_composer(CorpusDatabase& corpus,
                                     IngestedWorkId work_id,
                                     ComposerProfileId composer_id) {
    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    // A work has at most one composer owner. Reassignment removes all stale
    // reverse references. An idempotent assignment preserves a valid period.
    const bool same_owner = work->metadata.composer == composer_id;
    std::optional<std::string> preserved_period;
    if (same_owner && work->metadata.period) {
        const auto period = std::ranges::find(
            composer->period_profiles, *work->metadata.period, &PeriodProfile::label);
        if (period != composer->period_profiles.end()) preserved_period = period->label;
    }

    for (auto& [_, profile] : corpus.composers) {
        std::erase(profile.works, work_id);
        for (auto& period : profile.period_profiles)
            std::erase(period.works, work_id);
    }

    composer->works.push_back(work_id);
    work->metadata.composer = composer_id;
    work->metadata.period = preserved_period;
    if (preserved_period) {
        auto period =
            std::ranges::find(composer->period_profiles, *preserved_period, &PeriodProfile::label);
        period->works.push_back(work_id);
    }
    refresh_all_aggregates(corpus);
    return {};
}

Result<void> assign_work_to_period(CorpusDatabase& corpus,
                                   IngestedWorkId work_id,
                                   ComposerProfileId composer_id,
                                   const std::string& period_label) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    if (work->metadata.composer != composer_id) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }
    if (std::ranges::count(composer->works, work_id) != 1) {
        return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto target = std::ranges::find(composer->period_profiles, period_label, &PeriodProfile::label);
    if (target == composer->period_profiles.end()) return std::unexpected(not_found());

    // Period membership is exclusive across the corpus graph.
    for (auto& [_, profile] : corpus.composers)
        for (auto& period : profile.period_profiles)
            std::erase(period.works, work_id);
    target->works.push_back(work_id);
    work->metadata.period = period_label;
    refresh_all_period_aggregates(corpus);
    return {};
}

Result<void> add_period_profile(CorpusDatabase& corpus,
                                ComposerProfileId composer_id,
                                const PeriodProfile& period) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    if (period.label.empty() || period.year_start > period.year_end)
        return std::unexpected(ErrorCode::CorpusInvalidParameter);

    std::set<std::uint64_t> requested_works;
    for (const auto& existing : composer->period_profiles) {
        if (existing.label == period.label) return std::unexpected(duplicate_id());
        if (bounded_ranges_overlap(existing, period))
            return std::unexpected(ErrorCode::CorpusInvalidParameter);
    }
    for (const auto work_id : period.works) {
        if (!requested_works.insert(work_id.value).second)
            return std::unexpected(ErrorCode::CorpusInvalidParameter);
        const auto* work = find_work(corpus, work_id);
        if (!work) return std::unexpected(not_found());
        if (work->metadata.composer != composer_id ||
            std::ranges::count(composer->works, work_id) != 1)
            return std::unexpected(ErrorCode::InvalidMutation);
    }

    auto normalised_period = period;
    normalised_period.profile = {};
    composer->period_profiles.push_back(std::move(normalised_period));
    auto& target = composer->period_profiles.back();
    for (const auto work_id : target.works) {
        for (auto& [_, profile] : corpus.composers)
            for (auto& candidate_period : profile.period_profiles)
                if (&candidate_period != &target) std::erase(candidate_period.works, work_id);
        corpus.works.at(work_id.value).metadata.period = target.label;
    }
    refresh_all_period_aggregates(corpus);
    return {};
}

Result<void> clear_work_period_assignment(CorpusDatabase& corpus, IngestedWorkId work_id) {
    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    for (auto& [_, composer] : corpus.composers)
        for (auto& period : composer.period_profiles)
            std::erase(period.works, work_id);
    work->metadata.period.reset();
    refresh_all_period_aggregates(corpus);
    return {};
}

Result<void> remove_ingested_work(CorpusDatabase& corpus, IngestedWorkId work_id) {
    if (!find_work(corpus, work_id)) return std::unexpected(not_found());

    for (auto& [_, composer] : corpus.composers) {
        std::erase(composer.works, work_id);
        for (auto& period : composer.period_profiles)
            std::erase(period.works, work_id);
    }
    corpus.works.erase(work_id.value);
    refresh_all_aggregates(corpus);
    return {};
}

// =============================================================================
// Analysis
// =============================================================================

Result<void> analyze_work(CorpusDatabase& corpus, IngestedWorkId work_id, const Score* score) {
    auto* work = find_work(corpus, work_id);
    if (!work) return std::unexpected(not_found());

    const Score* effective_score = score;
    if (!effective_score && work->score) effective_score = &*work->score;
    if (!effective_score) return std::unexpected(ErrorCode::InvalidMutation);

    auto analysis = analyze_score(*effective_score);
    work->analysis = std::move(analysis);
    work->analysis_complete = true;
    refresh_all_aggregates(corpus);
    return {};
}

Result<void> rebuild_style_profile(CorpusDatabase& corpus, ComposerProfileId composer_id) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    refresh_composer_aggregates(corpus, *composer);
    return {};
}

bool style_profile_is_fresh(const CorpusDatabase& corpus,
                            const std::vector<IngestedWorkId>& work_ids,
                            const StyleProfile& profile) {
    auto expected = build_style_profile(collect_analyses(corpus, work_ids));
    expected.signature_patterns = profile.signature_patterns;
    return expected == profile;
}

Result<void> detect_signature_patterns(CorpusDatabase& corpus,
                                       ComposerProfileId composer_id,
                                       SignatureDetectionEvidence* evidence) {
    auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    using Bigram = std::vector<std::string>;
    struct Counts {
        std::uint64_t windows = 0;
        std::map<Bigram, std::uint64_t> occurrences;
        std::map<Bigram, std::vector<std::pair<IngestedWorkId, ScoreTime>>> examples;
        std::vector<IngestedWorkId> works;
    };
    Counts target;
    Counts baseline;
    // Corpus map traversal visits each work once. Unassigned works are not a
    // comparative composer baseline, and the target is excluded from it.
    for (const auto& [_, work] : corpus.works) {
        if (!work.analysis_complete || work.metadata.composer.value == 0) continue;
        auto& counts = work.metadata.composer == composer_id ? target : baseline;
        const auto before = counts.windows;
        for (const auto& prog : work.analysis.harmonic_analysis.progression_inventory) {
            if (prog.length != 2 || prog.roman_numerals.size() != 2) continue;
            const auto n = static_cast<std::uint64_t>(prog.occurrences.size());
            counts.windows += n;
            counts.occurrences[prog.roman_numerals] += n;
            for (const auto& position : prog.occurrences)
                counts.examples[prog.roman_numerals].emplace_back(work.id, position);
        }
        if (counts.windows != before) counts.works.push_back(work.id);
    }
    SignatureDetectionEvidence support;
    support.target_windows = target.windows;
    support.baseline_windows = baseline.windows;
    support.target_works = target.works;
    support.baseline_works = baseline.works;
    support.available = target.windows > 0 && baseline.windows > 0;
    if (evidence) *evidence = support;

    std::vector<SignaturePattern> patterns;
    std::uint64_t pattern_id = 1;
    if (support.available) {
        for (const auto& [bigram, n] : target.occurrences) {
            const auto found = baseline.occurrences.find(bigram);
            const auto m = found == baseline.occurrences.end() ? std::uint64_t{0} : found->second;
            const double a = static_cast<double>(target.windows);
            const double b = static_cast<double>(baseline.windows);
            const double target_rate = static_cast<double>(n) / a;
            const double baseline_rate = static_cast<double>(m) / b;
            const double pooled = (static_cast<double>(n) + static_cast<double>(m)) / (a + b);
            const double variance = pooled * (1.0 - pooled) * (1.0 / a + 1.0 / b);
            if (target_rate <= baseline_rate || variance <= 0.0) continue;
            const double z = (target_rate - baseline_rate) / std::sqrt(variance);
            // A declared descriptive selection threshold; no significance or
            // perceptual-confidence interpretation is attached to it.
            if (z < 1.5) continue;
            SignaturePattern pat;
            pat.id = SignaturePatternId{pattern_id++};
            pat.description = "Observed harmonic bigram: target " + std::to_string(n) + "/" +
                              std::to_string(target.windows) + ", other composers " +
                              std::to_string(m) + "/" + std::to_string(baseline.windows) +
                              "; descriptive pooled-proportion z (uncalibrated)";
            pat.domain = PatternDomain::Harmonic;
            pat.pattern_data = bigram;
            pat.distinctiveness = static_cast<float>(z);
            pat.examples = target.examples.at(bigram);
            patterns.push_back(std::move(pat));
        }
    }

    composer->style_profile.signature_patterns = std::move(patterns);
    return {};
}

// =============================================================================
// Query
// =============================================================================

Result<const StyleProfile*> query_style_profile(const CorpusDatabase& corpus,
                                                ComposerProfileId composer_id) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());
    return &composer->style_profile;
}

std::vector<AnnotatedExample> find_examples(const CorpusDatabase& corpus,
                                            ComposerProfileId composer_id,
                                            const std::string& criterion) {
    std::vector<AnnotatedExample> results;
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return results;

    // This is explicit lexical passage retrieval, not semantic inference.
    // All significant query tokens must occur in the annotated section text.
    auto tokens = [](const std::string& input) {
        std::set<std::string> result;
        std::string token;
        const auto flush = [&] {
            static const std::set<std::string> ignored = {
                "a", "an", "the", "in", "of", "at", "section", "sections", "passage"};
            if (!token.empty() && !ignored.contains(token)) result.insert(token);
            token.clear();
        };
        for (const auto ch : input) {
            const auto c = static_cast<unsigned char>(ch);
            if (std::isalnum(c))
                token.push_back(static_cast<char>(std::tolower(c)));
            else
                flush();
        }
        flush();
        return result;
    };
    const auto required = tokens(criterion);
    if (required.empty()) return results;
    for (const auto& wid : composer->works) {
        const auto* work = find_work(corpus, wid);
        if (!work || !work->analysis_complete) continue;

        for (const auto& sec : work->analysis.formal_analysis.section_plan) {
            const auto available = tokens(sec.label + " " + sec.character.value_or(""));
            if (std::includes(
                    available.begin(), available.end(), required.begin(), required.end())) {
                AnnotatedExample ex;
                ex.work_id = wid;
                ex.region_start = ScoreTime{sec.start_bar, Beat{0, 1}};
                ex.region_end = ScoreTime{sec.end_bar, Beat{0, 1}};
                ex.relevance_score = 1.0f; // every significant lexical token matched
                ex.analysis_summary = "Annotated section lexical match: " + sec.label;
                ex.formal_context = sec.label;
                results.push_back(std::move(ex));
            }
        }
    }

    return results;
}

std::vector<AnnotatedExample>
get_progression_examples(const CorpusDatabase& corpus,
                         ComposerProfileId composer_id,
                         const std::vector<std::string>& roman_numerals) {
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
                    ex.region_end = pos; // single point
                    ex.relevance_score = 1.0f;
                    ex.harmonic_reduction = roman_numerals;
                    results.push_back(std::move(ex));
                }
            }
        }
    }

    return results;
}

Result<FormalStyleProfile> get_formal_template(const CorpusDatabase& corpus,
                                               ComposerProfileId composer_id,
                                               FormClassification form_type) {
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

Result<HowWouldXHandleResult> how_would_x_handle(const CorpusDatabase& corpus,
                                                 ComposerProfileId composer_id,
                                                 const std::string& situation) {
    const auto* composer = find_composer(corpus, composer_id);
    if (!composer) return std::unexpected(not_found());

    HowWouldXHandleResult result;

    // Find relevant examples by keyword matching against formal sections
    result.relevant_examples = find_examples(corpus, composer_id, situation);

    // Aggregate only observed bars belonging to the returned passages. A
    // missing per-bar series is unavailable, not an observed zero. Deduplicate
    // overlapping annotations so the denominator counts each bar once.
    const auto local_tendency =
        [&](const std::string& domain, const std::string& label, auto series) {
            double sum = 0.0;
            std::uint64_t bars = 0;
            std::uint32_t passages = 0;
            std::map<std::uint64_t, std::set<std::uint32_t>> matched_bars;
            for (const auto& example : result.relevant_examples) {
                const auto* work = find_work(corpus, example.work_id);
                if (!work || example.region_start.bar == 0 ||
                    example.region_end.bar <= example.region_start.bar ||
                    series(work->analysis).size() < example.region_end.bar - 1)
                    continue;
                const auto& observations = series(work->analysis);
                bool usable = true;
                for (auto bar = example.region_start.bar; bar < example.region_end.bar; ++bar)
                    if (!std::isfinite(observations[bar - 1]) || observations[bar - 1] < 0.0f)
                        usable = false;
                if (!usable) continue;
                ++passages;
                for (auto bar = example.region_start.bar; bar < example.region_end.bar; ++bar)
                    matched_bars[example.work_id.value].insert(bar);
            }
            for (const auto& [work_id, positions] : matched_bars) {
                const auto* work = find_work(corpus, IngestedWorkId{work_id});
                if (!work) continue;
                const auto& values = series(work->analysis);
                for (const auto bar : positions) {
                    if (bar == 0 || bar > values.size()) continue;
                    sum += values[bar - 1];
                    ++bars;
                }
            }
            if (bars == 0) return;
            Tendency tendency;
            tendency.domain = domain;
            tendency.observation = "Matched annotated passages: " + label + " = " +
                                   std::to_string(sum / static_cast<double>(bars)) + " per bar (" +
                                   std::to_string(sum) + " observations / " + std::to_string(bars) +
                                   " distinct bars)";
            tendency.confidence = 1.0f; // exact aggregation of supplied per-bar observations
            tendency.supporting_examples_count = passages;
            result.statistical_tendencies.push_back(std::move(tendency));
        };
    local_tendency("harmonic",
                   "recognized chord changes",
                   [](const WorkAnalysis& analysis) -> const std::vector<float>& {
                       return analysis.harmonic_analysis.harmonic_rhythm.changes_per_bar;
                   });
    local_tendency("rhythmic",
                   "symbolic attacks",
                   [](const WorkAnalysis& analysis) -> const std::vector<float>& {
                       return analysis.rhythmic_analysis.onset_density;
                   });

    for (const auto& pattern : composer->style_profile.signature_patterns) {
        auto contextual = pattern;
        std::erase_if(contextual.examples, [&](const auto& occurrence) {
            return std::ranges::none_of(result.relevant_examples, [&](const auto& example) {
                return occurrence.first == example.work_id &&
                       occurrence.second >= example.region_start &&
                       occurrence.second < example.region_end;
            });
        });
        if (!contextual.examples.empty())
            result.signature_patterns.push_back(std::move(contextual));
    }

    return result;
}

// =============================================================================
// Comparison
// =============================================================================

Result<StyleComparison> compare_composers(const CorpusDatabase& corpus,
                                          ComposerProfileId composer_a,
                                          ComposerProfileId composer_b) {
    const auto* a = find_composer(corpus, composer_a);
    if (!a) return std::unexpected(not_found());
    const auto* b = find_composer(corpus, composer_b);
    if (!b) return std::unexpected(not_found());

    StyleComparison comparison;
    comparison.composer_a = composer_a;
    comparison.composer_b = composer_b;

    // Harmonic divergence (JS divergence on chord frequency distributions)
    comparison.harmonic_divergence =
        js_divergence(a->style_profile.harmonic_profile.chord_frequency,
                      b->style_profile.harmonic_profile.chord_frequency);

    // Melodic divergence (JS divergence on interval distributions)
    comparison.melodic_divergence =
        js_divergence(a->style_profile.melodic_profile.interval_distribution,
                      b->style_profile.melodic_profile.interval_distribution);

    // Rhythmic divergence (JS divergence on duration distributions)
    comparison.rhythmic_divergence =
        js_divergence(a->style_profile.rhythmic_profile.duration_distribution,
                      b->style_profile.rhythmic_profile.duration_distribution);

    // Formal divergence (Euclidean distance on scalar features)
    float formal_diff = std::abs(a->style_profile.formal_profile.average_work_length -
                                 b->style_profile.formal_profile.average_work_length);
    comparison.formal_divergence = std::min(1.0f, formal_diff / 500.0f);

    // Classify dimensions
    struct DimScore {
        std::string name;
        float score;
    };
    std::vector<DimScore> dims = {{"harmonic", comparison.harmonic_divergence},
                                  {"melodic", comparison.melodic_divergence},
                                  {"rhythmic", comparison.rhythmic_divergence},
                                  {"formal", comparison.formal_divergence}};
    std::sort(dims.begin(), dims.end(), [](const DimScore& x, const DimScore& y) {
        return x.score < y.score;
    });

    if (!dims.empty()) {
        comparison.most_similar_dimensions.push_back(dims.front().name);
        comparison.most_divergent_dimensions.push_back(dims.back().name);
    }

    return comparison;
}

Result<EvolutionaryAnalysis> analyze_evolution(const CorpusDatabase& corpus,
                                               ComposerProfileId composer_id) {
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

} // namespace sunny::core
