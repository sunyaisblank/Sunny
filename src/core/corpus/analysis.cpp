/**
 * @file analysis.cpp
 * @brief Corpus IR analytical decomposition — implementation
 *
 *
 * Each analytical domain is a pure function Score → DomainRecord.
 * The functions traverse the Score IR document model, extract note
 * content, and compose existing theory engine operations to produce
 * statistical and structural analysis data.
 *
 * Theory engine dependencies:
 *    — chord recognition and Roman numeral generation
 *    — harmonic function classification
 *    — cadence detection
 *    — melody statistics and contour
 *    — form classification
 *    — voice-leading constraint checking
 *    — pitch class extraction and MIDI conversion
 */

#include <sunny/core/corpus/analysis.hpp>

// Include only the theory algorithms used directly by this decomposition;
// shared cadence and form data types come from the score document model.
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/melody/analysis.hpp>
#include <sunny/core/pitch/pitch_class_set.hpp>
#include <sunny/core/pitch/spelled_pitch.hpp>

namespace sunny::core {

namespace {

// =========================================================================
// Shared helpers
// =========================================================================

/// Extract the active key signature at a given bar from the key map.
KeySignature key_at_bar(const Score& score, std::uint32_t bar) {
    KeySignature key = score.key_map.front().key;
    for (const auto& entry : score.key_map) {
        if (entry.position.bar <= bar)
            key = entry.key;
        else
            break;
    }
    return key;
}

/// Collect all NoteGroups at a given bar across all parts/voices.
/// Returns pitch class sets suitable for chord recognition.
std::vector<PitchClassSet> collect_pcs_per_beat(const Score& score, std::uint32_t bar_index) {
    std::vector<PitchClassSet> result;
    // Aggregate all sounding pitches at each distinct offset
    std::map<Beat, PitchClassSet> offset_pcs;
    for (const auto& part : score.parts) {
        if (bar_index >= part.measures.size()) continue;
        const auto& measure = part.measures[bar_index];
        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                const auto* ng = event.as_note_group();
                if (!ng) continue;
                for (const auto& note : ng->notes) {
                    offset_pcs[event.offset].insert(pc(note.pitch));
                }
            }
        }
    }
    for (const auto& [offset, pcs] : offset_pcs) {
        if (pcs.size() >= 2) result.push_back(pcs);
    }
    return result;
}

/// Extract a melodic line (sequence of MIDI notes) from a single voice.
std::vector<MidiNote> extract_melody_line(const Part& part, std::uint8_t voice_idx) {
    std::vector<MidiNote> notes;
    for (const auto& measure : part.measures) {
        for (const auto& voice : measure.voices) {
            if (voice.voice_index != voice_idx) continue;
            for (const auto& event : voice.events) {
                const auto* ng = event.as_note_group();
                if (!ng || ng->notes.empty()) continue;
                // Take the highest note as the melody note
                MidiNote highest = 0;
                for (const auto& note : ng->notes) {
                    auto m = midi(note.pitch);
                    if (m && *m > highest) highest = *m;
                }
                if (highest > 0) notes.push_back(highest);
            }
        }
    }
    return notes;
}

/// Dynamic level to string for map keys
std::string dynamic_name(DynamicLevel level) {
    switch (level) {
    case DynamicLevel::pppp:
        return "pppp";
    case DynamicLevel::ppp:
        return "ppp";
    case DynamicLevel::pp:
        return "pp";
    case DynamicLevel::p:
        return "p";
    case DynamicLevel::mp:
        return "mp";
    case DynamicLevel::mf:
        return "mf";
    case DynamicLevel::f:
        return "f";
    case DynamicLevel::ff:
        return "ff";
    case DynamicLevel::fff:
        return "fff";
    case DynamicLevel::ffff:
        return "ffff";
    case DynamicLevel::fp:
        return "fp";
    case DynamicLevel::sfz:
        return "sfz";
    case DynamicLevel::sfp:
        return "sfp";
    case DynamicLevel::rfz:
        return "rfz";
    }
    return "mf";
}

float dynamic_intensity(DynamicLevel level) {
    switch (level) {
    case DynamicLevel::pppp:
        return 0.05f;
    case DynamicLevel::ppp:
        return 0.1f;
    case DynamicLevel::pp:
        return 0.2f;
    case DynamicLevel::p:
        return 0.3f;
    case DynamicLevel::mp:
        return 0.4f;
    case DynamicLevel::mf:
        return 0.55f;
    case DynamicLevel::f:
    case DynamicLevel::fp:
        return 0.7f;
    case DynamicLevel::sfp:
        return 0.8f;
    case DynamicLevel::ff:
    case DynamicLevel::rfz:
        return 0.85f;
    case DynamicLevel::sfz:
        return 0.9f;
    case DynamicLevel::fff:
        return 0.95f;
    case DynamicLevel::ffff:
        return 1.0f;
    }
    return 0.5f;
}

/// Beat duration as a string key for distribution maps
std::string beat_key(Beat b) {
    if (b == Beat{1, 1}) return "whole";
    if (b == Beat{1, 2}) return "half";
    if (b == Beat{1, 4}) return "quarter";
    if (b == Beat{1, 8}) return "eighth";
    if (b == Beat{1, 16}) return "sixteenth";
    if (b == Beat{3, 8}) return "dotted-quarter";
    if (b == Beat{3, 4}) return "dotted-half";
    return std::to_string(b.numerator()) + "/" + std::to_string(b.denominator());
}

/// Find section label for a given bar
std::string section_for_bar(const SectionMap& sections, std::uint32_t bar) {
    for (const auto& sec : sections) {
        if (bar >= sec.start.bar && bar < sec.end.bar) return sec.label;
    }
    return "unknown";
}

std::string metre_key(const TimeSignature& signature) {
    const auto& groups = signature.groups();
    const bool conventional_simple =
        std::ranges::all_of(groups, [](int group) { return group == 1; });
    const bool conventional_compound =
        std::ranges::all_of(groups, [](int group) { return group == 3; });
    if (conventional_simple || conventional_compound)
        return std::to_string(signature.numerator()) + "/" +
               std::to_string(signature.denominator());

    std::string result;
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (i != 0) result += "+";
        result += std::to_string(groups[i]);
    }
    return result + "/" + std::to_string(signature.denominator());
}

std::string key_relationship_name(PitchClass tonic, PitchClass target) {
    static constexpr std::array<const char*, 12> names = {"tonic",
                                                          "minor_second",
                                                          "major_second",
                                                          "minor_third",
                                                          "major_third",
                                                          "perfect_fourth",
                                                          "tritone",
                                                          "perfect_fifth",
                                                          "minor_sixth",
                                                          "major_sixth",
                                                          "minor_seventh",
                                                          "major_seventh"};
    const auto displacement =
        static_cast<std::uint8_t>((static_cast<int>(target) - static_cast<int>(tonic) + 12) % 12);
    return names[displacement];
}

std::string key_signature_name(const KeySignature& key) {
    static constexpr std::array<char, 7> letters = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    std::string result(1, letters[key.root.letter < letters.size() ? key.root.letter : 0]);
    result.append(static_cast<std::size_t>(std::max<int>(key.root.accidental, 0)), '#');
    result.append(static_cast<std::size_t>(std::max<int>(-key.root.accidental, 0)), 'b');
    result += " ";
    result += key.mode.name;
    return result;
}

TonalPlan analyze_tonal_plan(const Score& score) {
    TonalPlan plan;
    if (score.key_map.empty()) return plan;

    const auto tonic = pc(score.key_map.front().key.root);
    const auto tonic_circle_position = (static_cast<int>(tonic) * 7) % 12;
    int greatest_distance = -1;
    bool left_tonic = false;
    std::set<std::string> key_areas;
    for (const auto& entry : score.key_map) {
        const auto root = pc(entry.key.root);
        const auto circle_position = (static_cast<int>(root) * 7) % 12;
        const auto circle_displacement = (circle_position - tonic_circle_position + 12) % 12;
        const auto distance = std::min(circle_displacement, 12 - circle_displacement);
        const auto key_name = key_signature_name(entry.key);
        key_areas.insert(key_name);
        plan.key_sequence.emplace_back(
            key_name, key_relationship_name(tonic, root), entry.position.bar);

        if (distance > greatest_distance) {
            greatest_distance = distance;
            plan.most_distant_key = key_name;
        }
        if (root != tonic) {
            left_tonic = true;
        } else if (left_tonic) {
            plan.tonic_return_bar = entry.position.bar;
        }
    }
    plan.key_area_count = static_cast<std::uint32_t>(key_areas.size());
    return plan;
}

} // anonymous namespace

// =========================================================================
// Harmonic Analysis
// =========================================================================

HarmonicAnalysisRecord analyze_harmonic(const Score& score) {
    HarmonicAnalysisRecord result;

    // If the Score already has harmonic annotations, use them directly
    if (!score.harmonic_annotations.empty()) {
        for (const auto& ann : score.harmonic_annotations) {
            // Chord vocabulary
            result.chord_vocabulary[ann.roman_numeral]++;

            // Cadence inventory
            if (ann.cadence) {
                CadenceEvent ce;
                ce.position = ann.position;
                // Convert  CadenceType to string
                switch (*ann.cadence) {
                case sunny::core::CadenceType::PAC:
                    ce.type = "PAC";
                    break;
                case sunny::core::CadenceType::IAC:
                    ce.type = "IAC";
                    break;
                case sunny::core::CadenceType::Half:
                    ce.type = "HC";
                    break;
                case sunny::core::CadenceType::Plagal:
                    ce.type = "PC";
                    break;
                case sunny::core::CadenceType::Deceptive:
                    ce.type = "DC";
                    break;
                case sunny::core::CadenceType::PhrygianHalf:
                    ce.type = "Phrygian";
                    break;
                default:
                    ce.type = "PAC";
                    break;
                }
                ce.section_context = section_for_bar(score.section_map, ann.position.bar);
                result.cadence_inventory.push_back(std::move(ce));
            }
        }

        // Build progression patterns from consecutive annotations
        if (score.harmonic_annotations.size() >= 2) {
            for (std::size_t i = 0; i + 1 < score.harmonic_annotations.size(); ++i) {
                std::vector<std::string> pair = {score.harmonic_annotations[i].roman_numeral,
                                                 score.harmonic_annotations[i + 1].roman_numeral};
                // Check if this bigram already exists
                bool found = false;
                for (auto& prog : result.progression_inventory) {
                    if (prog.roman_numerals == pair) {
                        prog.occurrences.push_back(score.harmonic_annotations[i].position);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ProgressionPattern pp;
                    pp.roman_numerals = pair;
                    pp.length = 2;
                    pp.occurrences.push_back(score.harmonic_annotations[i].position);
                    {
                        auto kn = to_spn(score.harmonic_annotations[i].key_context.root);
                        pp.key_context = kn ? *kn : "C4";
                    }
                    result.progression_inventory.push_back(std::move(pp));
                }
            }
        }

        // Harmonic rhythm: count chord changes per bar
        std::map<std::uint32_t, std::uint32_t> changes_per_bar;
        for (const auto& ann : score.harmonic_annotations)
            changes_per_bar[ann.position.bar]++;

        float total_changes = 0.0f;
        for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
            float c = static_cast<float>(changes_per_bar[bar]);
            result.harmonic_rhythm.changes_per_bar.push_back(c);
            total_changes += c;
        }
        if (score.metadata.total_bars > 0) {
            result.harmonic_rhythm.mean_rate =
                total_changes / static_cast<float>(score.metadata.total_bars);
            float var_sum = 0.0f;
            for (float c : result.harmonic_rhythm.changes_per_bar) {
                float diff = c - result.harmonic_rhythm.mean_rate;
                var_sum += diff * diff;
            }
            result.harmonic_rhythm.variance =
                var_sum / static_cast<float>(score.metadata.total_bars);
        }

        result.tonal_plan = analyze_tonal_plan(score);

        return result;
    }

    // No pre-existing annotations: recognize chords from note content
    KeySignature current_key = score.key_map.front().key;
    auto scale_ints = current_key.mode.get_intervals();
    PitchClass key_pc = pc(current_key.root);

    for (std::uint32_t bar = 0; bar < score.metadata.total_bars; ++bar) {
        current_key = key_at_bar(score, bar + 1);
        scale_ints = current_key.mode.get_intervals();
        const bool is_minor_key = scale_ints.size() >= 3 && scale_ints[2] == 3;
        key_pc = pc(current_key.root);

        auto pcs_list = collect_pcs_per_beat(score, bar);
        std::uint32_t bar_chord_count = 0;

        for (const auto& pcs : pcs_list) {
            auto chord_result = recognize_chord(pcs);
            if (!chord_result) continue;

            auto [root, quality] = *chord_result;
            auto numeral = chord_to_numeral(root, quality, key_pc, scale_ints, is_minor_key);

            if (numeral) {
                result.chord_vocabulary[*numeral]++;
                bar_chord_count++;
            }
        }

        result.harmonic_rhythm.changes_per_bar.push_back(static_cast<float>(bar_chord_count));
    }

    // Compute mean and variance of harmonic rhythm
    float total = 0.0f;
    for (float c : result.harmonic_rhythm.changes_per_bar)
        total += c;
    if (score.metadata.total_bars > 0) {
        result.harmonic_rhythm.mean_rate = total / static_cast<float>(score.metadata.total_bars);
        float var_sum = 0.0f;
        for (float c : result.harmonic_rhythm.changes_per_bar) {
            float diff = c - result.harmonic_rhythm.mean_rate;
            var_sum += diff * diff;
        }
        result.harmonic_rhythm.variance = var_sum / static_cast<float>(score.metadata.total_bars);
    }

    result.tonal_plan = analyze_tonal_plan(score);

    return result;
}

// =========================================================================
// Melodic Analysis
// =========================================================================

MelodicAnalysisRecord analyze_melodic(const Score& score) {
    MelodicAnalysisRecord result;

    std::size_t max_notes = 0;
    PartId melody_voice{};

    for (const auto& part : score.parts) {
        VoiceMelodicAnalysis vma;
        vma.part_id = part.id;

        auto melody = extract_melody_line(part, 0);
        vma.note_count = static_cast<std::uint32_t>(melody.size());
        if (melody.empty()) {
            result.per_voice_analysis.push_back(std::move(vma));
            continue;
        }

        // Range
        auto [mn, mx] = std::minmax_element(melody.begin(), melody.end());
        vma.range_low = static_cast<std::int8_t>(*mn);
        vma.range_high = static_cast<std::int8_t>(*mx);

        // Statistics via
        auto stats = compute_melody_statistics(melody);
        if (stats) {
            vma.conjunct_proportion = static_cast<float>(stats->conjunct_ratio);

            // Interval distribution
            for (int i = 0; i < 25; ++i) {
                if (stats->interval_histogram[i] > 0) {
                    std::int8_t interval = static_cast<std::int8_t>(i - 12);
                    vma.interval_distribution[interval] =
                        static_cast<std::uint32_t>(stats->interval_histogram[i]);
                }
            }

            // Chromaticism rate and scale degree distribution
            KeySignature key = key_at_bar(score, 1);
            auto ints = key.mode.get_intervals();
            std::set<PitchClass> diatonic;
            std::vector<PitchClass> scale_pcs;
            PitchClass root = pc(key.root);
            PitchClass current = root;
            diatonic.insert(current);
            scale_pcs.push_back(current);
            for (auto iv : ints) {
                current = PitchClass::wrapped(current + iv);
                diatonic.insert(current);
                scale_pcs.push_back(current);
            }
            std::uint32_t chromatic_count = 0;
            std::uint32_t total_notes = 0;
            for (int i = 0; i < 12; ++i) {
                total_notes += stats->pitch_class_histogram[i];
                if (diatonic.find(PitchClass::wrapped(i)) == diatonic.end())
                    chromatic_count += stats->pitch_class_histogram[i];
            }
            if (total_notes > 0)
                vma.chromaticism_rate =
                    static_cast<float>(chromatic_count) / static_cast<float>(total_notes);

            // Scale degree distribution: map each PC to its scale degree
            for (std::size_t deg = 0; deg < scale_pcs.size() && deg < 8; ++deg) {
                auto pch = scale_pcs[deg];
                auto count = stats->pitch_class_histogram[pch];
                if (count > 0)
                    vma.scale_degree_distribution[static_cast<std::uint8_t>(deg + 1)] = count;
            }
        }

        // Track primary melody voice (most notes)
        if (melody.size() > max_notes) {
            max_notes = melody.size();
            melody_voice = part.id;
        }

        result.per_voice_analysis.push_back(std::move(vma));
    }

    result.primary_melody_voice = melody_voice;
    return result;
}

// =========================================================================
// Rhythmic Analysis
// =========================================================================

RhythmicAnalysisRecord analyze_rhythmic(const Score& score) {
    RhythmicAnalysisRecord result;
    std::uint32_t total_note_events = 0;
    double total_note_dur = 0.0;
    double total_rest_dur = 0.0;
    std::uint32_t time_sig_changes = 0;

    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        const TimeSignature* active = nullptr;
        for (const auto& entry : score.time_map) {
            if (entry.bar > bar) break;
            active = &entry.time_signature;
        }
        if (active) result.metre_distribution[metre_key(*active)]++;
    }

    for (std::uint32_t bar = 0; bar < score.metadata.total_bars; ++bar) {
        std::uint32_t bar_onsets = 0;

        for (const auto& part : score.parts) {
            if (bar >= part.measures.size()) continue;
            const auto& measure = part.measures[bar];

            // Check for time signature changes
            if (measure.local_time) time_sig_changes++;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (event.is_note_group()) {
                        const auto* ng = event.as_note_group();
                        std::string key = beat_key(ng->duration);
                        result.duration_distribution[key]++;
                        total_note_events++;
                        total_note_dur += ng->duration.to_float();
                        bar_onsets++;
                    } else if (event.is_rest()) {
                        const auto* r = event.as_rest();
                        total_rest_dur += r->duration.to_float();
                    }
                }
            }
        }

        result.onset_density.push_back(static_cast<float>(bar_onsets));
    }

    // Rest proportion
    const double total_dur = total_note_dur + total_rest_dur;
    if (total_dur > 0.0) {
        result.rest_proportion = static_cast<float>(total_rest_dur / total_dur);
    }

    // Syncopation index: ratio of onsets on weak metrical positions
    if (total_note_events > 0) {
        std::uint32_t weak_onsets = 0;
        for (const auto& part : score.parts) {
            for (std::uint32_t bar = 0;
                 bar < score.metadata.total_bars && bar < part.measures.size();
                 ++bar) {
                const auto& measure = part.measures[bar];
                for (const auto& voice : measure.voices) {
                    for (const auto& event : voice.events) {
                        if (!event.is_note_group()) continue;
                        // Weak positions: offbeat eighths and sixteenths
                        const double offset_in_quarters =
                            event.offset.to_float() * 4.0; // in quarter-note units
                        const double fractional_offset =
                            offset_in_quarters - std::floor(offset_in_quarters);
                        if (fractional_offset > 0.1) weak_onsets++;
                    }
                }
            }
        }
        result.syncopation_index =
            static_cast<float>(weak_onsets) / static_cast<float>(total_note_events);
    }

    // Metrical complexity: time sig changes / total bars + asymmetric penalty
    if (score.metadata.total_bars > 0) {
        result.metrical_complexity =
            static_cast<float>(time_sig_changes) / static_cast<float>(score.metadata.total_bars);
    }

    // Tempo profile from tempo map
    for (const auto& te : score.tempo_map) {
        result.tempo_profile.push_back({te.position, static_cast<float>(te.bpm.to_float())});
    }

    // Note density by section
    for (const auto& sec : score.section_map) {
        float section_onsets = 0.0f;
        std::uint32_t section_bars = 0;
        for (std::uint32_t bar = sec.start.bar;
             bar < sec.end.bar && bar <= score.metadata.total_bars;
             ++bar) {
            if (bar - 1 < result.onset_density.size())
                section_onsets += result.onset_density[bar - 1];
            section_bars++;
        }
        if (section_bars > 0)
            result.note_density_by_section[sec.label] =
                section_onsets / static_cast<float>(section_bars);
    }

    return result;
}

// =========================================================================
// Formal Analysis
// =========================================================================

FormalAnalysisRecord analyze_formal(const Score& score) {
    FormalAnalysisRecord result;
    result.total_duration_bars = score.metadata.total_bars;

    // Convert SectionMap to FormalSections
    for (const auto& sec : score.section_map) {
        FormalSection fs;
        fs.label = sec.label;
        fs.start_bar = sec.start.bar;
        fs.end_bar = sec.end.bar;
        fs.length_bars = sec.end.bar - sec.start.bar;
        {
            auto ks = key_at_bar(score, sec.start.bar);
            auto kn = to_spn(ks.root);
            fs.key = kn ? *kn : "C4";
        }
        if (!score.tempo_map.empty()) {
            fs.tempo = static_cast<float>(score.tempo_map.front().bpm.to_float());
        }
        result.section_plan.push_back(std::move(fs));
    }

    // Section proportions
    if (score.metadata.total_bars > 0) {
        float cumulative = 0.0f;
        for (const auto& fs : result.section_plan) {
            SectionProportion sp;
            sp.label = fs.label;
            sp.proportion =
                static_cast<float>(fs.length_bars) / static_cast<float>(score.metadata.total_bars);
            cumulative += sp.proportion;
            sp.golden_ratio_proximity = std::abs(cumulative - 0.618f);
            result.proportions.push_back(std::move(sp));
        }
    }

    // Classify form from section label pattern.
    // Inline pattern matching avoids including.h, which
    // conflicts with.h on the CadenceType enum.
    if (!result.section_plan.empty()) {
        std::vector<std::string> labels;
        labels.reserve(result.section_plan.size());
        for (const auto& fs : result.section_plan)
            labels.push_back(fs.label);

        // Check for common patterns
        if (labels.size() == 2) {
            result.form_type = FormClassification::BinarySimple;
        } else if (labels.size() == 3 && labels[0] == labels[2]) {
            result.form_type = FormClassification::Ternary;
        } else if (labels.size() >= 3 && labels[0] == labels[2] && labels[0] != labels[1]) {
            result.form_type = FormClassification::BinaryRounded;
        } else if (labels.size() >= 5) {
            // Check for rondo (A B A C A...)
            bool is_rondo = true;
            for (std::size_t i = 0; i < labels.size(); i += 2) {
                if (labels[i] != labels[0]) {
                    is_rondo = false;
                    break;
                }
            }
            if (is_rondo) result.form_type = FormClassification::Rondo;
        }
    }

    result.tonal_plan = analyze_tonal_plan(score);

    return result;
}

// =========================================================================
// Voice-Leading Analysis
// =========================================================================

VoiceLeadingAnalysisRecord analyze_voice_leading(const Score& score) {
    VoiceLeadingAnalysisRecord result;

    // We analyze consecutive vertical sonorities for motion types.
    // For each pair of measures, extract pitch sets and classify motion.

    std::uint32_t total_pairs = 0;
    std::uint32_t contrary_count = 0;
    std::uint32_t similar_count = 0;
    std::uint32_t oblique_count = 0;
    std::uint32_t parallel_count = 0;

    // Extract per-bar pitch sequences for each part
    for (std::size_t pi = 0; pi + 1 < score.parts.size(); ++pi) {
        const auto& upper = score.parts[pi];
        const auto& lower = score.parts[pi + 1];

        for (std::uint32_t bar = 0; bar + 1 < score.metadata.total_bars; ++bar) {
            if (bar >= upper.measures.size() || bar >= lower.measures.size()) continue;

            // Get first note of each part in consecutive bars
            auto get_first_midi = [](const Measure& m) -> int {
                for (const auto& voice : m.voices) {
                    for (const auto& ev : voice.events) {
                        const auto* ng = ev.as_note_group();
                        if (ng && !ng->notes.empty()) {
                            auto m_val = midi(ng->notes[0].pitch);
                            if (m_val) return *m_val;
                        }
                    }
                }
                return -1;
            };

            int upper1 = get_first_midi(upper.measures[bar]);
            int upper2 = get_first_midi(upper.measures[bar + 1]);
            int lower1 = get_first_midi(lower.measures[bar]);
            int lower2 = get_first_midi(lower.measures[bar + 1]);

            if (upper1 < 0 || upper2 < 0 || lower1 < 0 || lower2 < 0) continue;

            int upper_motion = upper2 - upper1;
            int lower_motion = lower2 - lower1;

            total_pairs++;

            if (upper_motion == 0 || lower_motion == 0) {
                oblique_count++;
            } else if ((upper_motion > 0 && lower_motion < 0) ||
                       (upper_motion < 0 && lower_motion > 0)) {
                contrary_count++;
            } else if (upper_motion == lower_motion) {
                parallel_count++;
                // Check for parallel fifths/octaves
                int interval1 = std::abs(upper1 - lower1) % 12;
                int interval2 = std::abs(upper2 - lower2) % 12;
                if (interval1 == 7 && interval2 == 7) result.parallel_fifths_count++;
                if (interval1 == 0 && interval2 == 0) result.parallel_octaves_count++;
            } else {
                similar_count++;
            }

            // Spacing distribution
            int spacing = std::abs(upper1 - lower1);
            if (spacing <= 127)
                result.spacing_distribution[static_cast<std::int8_t>(std::min(spacing, 24))]++;
        }
    }

    if (total_pairs > 0) {
        auto n = static_cast<float>(total_pairs);
        result.contrary_motion_proportion = static_cast<float>(contrary_count) / n;
        result.similar_motion_proportion = static_cast<float>(similar_count) / n;
        result.oblique_motion_proportion = static_cast<float>(oblique_count) / n;
        result.parallel_motion_proportion = static_cast<float>(parallel_count) / n;
    }

    return result;
}

// =========================================================================
// Textural Analysis
// =========================================================================

TexturalAnalysisRecord analyze_textural(const Score& score) {
    TexturalAnalysisRecord result;

    float total_density = 0.0f;
    float total_span = 0.0f;
    std::uint32_t samples = 0;

    for (std::uint32_t bar = 0; bar < score.metadata.total_bars; ++bar) {
        std::uint8_t density = 0;
        int lowest = 127;
        int highest = 0;

        for (const auto& part : score.parts) {
            if (bar >= part.measures.size()) continue;
            const auto& measure = part.measures[bar];
            bool part_active = false;

            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    part_active = true;
                    for (const auto& note : ng->notes) {
                        auto m = midi(note.pitch);
                        if (m) {
                            if (*m < lowest) lowest = *m;
                            if (*m > highest) highest = *m;
                        }
                    }
                }
            }
            if (part_active) density++;
        }

        ScoreTime st{bar + 1, Beat::zero()};
        result.density_curve.push_back({st, density});
        total_density += static_cast<float>(density);

        if (highest >= lowest) {
            total_span += static_cast<float>(highest - lowest);
        }
        samples++;

        // Section density
        std::string sec = section_for_bar(score.section_map, bar + 1);
        result.density_by_section[sec] += static_cast<float>(density);
    }

    if (samples > 0) {
        result.average_density = total_density / static_cast<float>(samples);
        result.average_register_span = total_span / static_cast<float>(samples);
    }

    // Texture type classification per bar
    std::uint32_t mono_count = 0;
    std::uint32_t homo_count = 0;
    std::uint32_t poly_count = 0;
    for (const auto& [st, density] : result.density_curve) {
        if (density <= 1)
            mono_count++;
        else if (density <= 2)
            homo_count++;
        else
            poly_count++;
    }
    if (samples > 0) {
        auto s = static_cast<float>(samples);
        result.texture_type_proportions["monophonic"] = static_cast<float>(mono_count) / s;
        result.texture_type_proportions["homophonic"] = static_cast<float>(homo_count) / s;
        result.texture_type_proportions["polyphonic"] = static_cast<float>(poly_count) / s;
    }

    // Normalise section density to per-bar averages
    for (const auto& sec : score.section_map) {
        std::uint32_t sec_bars = sec.end.bar - sec.start.bar;
        if (sec_bars > 0 && result.density_by_section.count(sec.label))
            result.density_by_section[sec.label] /= static_cast<float>(sec_bars);
    }

    return result;
}

// =========================================================================
// Dynamic Analysis
// =========================================================================

DynamicAnalysisRecord analyze_dynamic(const Score& score) {
    DynamicAnalysisRecord result;

    // Collect all explicit dynamics from notes
    DynamicLevel lowest = DynamicLevel::ffff;
    DynamicLevel highest = DynamicLevel::pppp;
    float lowest_intensity = 0.0f;
    float highest_intensity = 0.0f;
    bool found_dynamic = false;
    std::uint32_t dynamic_changes = 0;
    std::optional<DynamicLevel> previous_dynamic;

    for (const auto& part : score.parts) {
        // Count hairpins
        result.hairpin_count += static_cast<std::uint32_t>(part.hairpins.size());

        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.dynamic) {
                            DynamicLevel d = *note.dynamic;
                            result.dynamic_distribution[dynamic_name(d)]++;

                            const auto intensity = dynamic_intensity(d);
                            if (!found_dynamic || intensity < lowest_intensity) {
                                lowest = d;
                                lowest_intensity = intensity;
                            }
                            if (!found_dynamic || intensity > highest_intensity) {
                                highest = d;
                                highest_intensity = intensity;
                            }
                            found_dynamic = true;

                            if (previous_dynamic && d != *previous_dynamic) dynamic_changes++;
                            if (d == DynamicLevel::fp || d == DynamicLevel::sfz ||
                                d == DynamicLevel::sfp || d == DynamicLevel::rfz)
                                result.subito_dynamics_count++;
                            previous_dynamic = d;
                        }
                    }
                }
            }
        }
    }

    if (found_dynamic) {
        result.dynamic_range_low = dynamic_name(lowest);
        result.dynamic_range_high = dynamic_name(highest);
    }

    if (score.metadata.total_bars > 0) {
        result.dynamic_change_rate =
            static_cast<float>(dynamic_changes) / static_cast<float>(score.metadata.total_bars);
    }

    // Per-bar dynamic level (last dynamic heard in that bar)
    std::optional<float> current_intensity;
    float max_intensity = 0.0f;
    float max_position = 0.0f;
    for (std::uint32_t bar = 0; bar < score.metadata.total_bars; ++bar) {
        for (const auto& part : score.parts) {
            if (bar >= part.measures.size()) continue;
            for (const auto& voice : part.measures[bar].voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        if (note.dynamic) current_intensity = dynamic_intensity(*note.dynamic);
                    }
                }
            }
        }
        if (!current_intensity) continue;
        result.dynamic_shape.push_back({ScoreTime{bar + 1, Beat::zero()}, *current_intensity});
        if (*current_intensity > max_intensity) {
            max_intensity = *current_intensity;
            max_position = static_cast<float>(bar + 1);
        }
    }

    if (score.metadata.total_bars > 0 && !result.dynamic_shape.empty())
        result.climax_position = max_position / static_cast<float>(score.metadata.total_bars);

    // Dynamic by section: map sections to their dynamic range
    for (const auto& sec : score.section_map) {
        std::string sec_low;
        std::string sec_high;
        int sec_lo_ord = 10;
        int sec_hi_ord = -1;
        for (const auto& [position, intensity] : result.dynamic_shape) {
            if (position.bar < sec.start.bar || position.bar >= sec.end.bar) continue;
            // Map intensity back to approximate ordinary dynamic name.
            const int ord = static_cast<int>(std::clamp(std::lround(intensity * 9.0f), 0L, 9L));
            if (ord < sec_lo_ord) {
                sec_lo_ord = ord;
                static const char* names[] = {
                    "pppp", "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff", "ffff"};
                sec_low = names[ord];
            }
            if (ord > sec_hi_ord) {
                sec_hi_ord = ord;
                static const char* names[] = {
                    "pppp", "ppp", "pp", "p", "mp", "mf", "f", "ff", "fff", "ffff"};
                sec_high = names[ord];
            }
        }
        if (!sec_low.empty()) result.dynamic_by_section[sec.label] = {sec_low, sec_high};
    }

    return result;
}

// =========================================================================
// Orchestration Analysis
// =========================================================================

std::optional<OrchestrationAnalysisRecord> analyze_orchestration(const Score& score) {
    if (score.parts.size() < 2) return std::nullopt;

    OrchestrationAnalysisRecord result;

    // Instrument usage: proportion of measures where each part plays
    for (const auto& part : score.parts) {
        std::uint32_t active_bars = 0;
        for (const auto& measure : part.measures) {
            bool has_notes = false;
            for (const auto& voice : measure.voices) {
                for (const auto& event : voice.events) {
                    if (event.is_note_group()) {
                        has_notes = true;
                        break;
                    }
                }
                if (has_notes) break;
            }
            if (has_notes) active_bars++;
        }
        float usage =
            (score.metadata.total_bars > 0)
                ? static_cast<float>(active_bars) / static_cast<float>(score.metadata.total_bars)
                : 0.0f;
        result.instrument_usage[part.definition.name] = usage;
    }

    // Melody carrier: which part has the highest notes most often
    std::map<std::string, std::uint32_t> melody_counts;
    for (std::uint32_t bar = 0; bar < score.metadata.total_bars; ++bar) {
        int highest_note = -1;
        std::string carrier;
        for (const auto& part : score.parts) {
            if (bar >= part.measures.size()) continue;
            for (const auto& voice : part.measures[bar].voices) {
                for (const auto& event : voice.events) {
                    const auto* ng = event.as_note_group();
                    if (!ng) continue;
                    for (const auto& note : ng->notes) {
                        auto m = midi(note.pitch);
                        if (m && *m > highest_note) {
                            highest_note = *m;
                            carrier = part.definition.name;
                        }
                    }
                }
            }
        }
        if (highest_note >= 0) melody_counts[carrier]++;
    }

    std::uint32_t total_melody_bars = 0;
    for (const auto& [_, c] : melody_counts)
        total_melody_bars += c;
    if (total_melody_bars > 0) {
        for (const auto& [name, count] : melody_counts)
            result.melody_carrier_distribution[name] =
                static_cast<float>(count) / static_cast<float>(total_melody_bars);
    }

    return result;
}

// =========================================================================
// Motivic Analysis
// =========================================================================

MotivicAnalysisRecord analyze_motivic(const Score&) {
    MotivicAnalysisRecord result;

    // Motivic analysis requires pre-identified thematic units, which
    // depend on formal segmentation and human annotation. Without
    // pre-existing thematic units, we compute basic statistics:
    // thematic density = 0 (no identified themes) and economy = 0.

    // If thematic material is available through section annotations,
    // record it; otherwise leave empty.
    result.thematic_density = 0.0f;
    result.thematic_economy = 0.0f;

    return result;
}

// =========================================================================
// Full Analysis
// =========================================================================

WorkAnalysis analyze_score(const Score& score) {
    WorkAnalysis wa;
    wa.harmonic_analysis = analyze_harmonic(score);
    wa.melodic_analysis = analyze_melodic(score);
    wa.rhythmic_analysis = analyze_rhythmic(score);
    wa.formal_analysis = analyze_formal(score);
    wa.voice_leading_analysis = analyze_voice_leading(score);
    wa.textural_analysis = analyze_textural(score);
    wa.dynamic_analysis = analyze_dynamic(score);
    wa.orchestration_analysis = analyze_orchestration(score);
    wa.motivic_analysis = analyze_motivic(score);
    return wa;
}

} // namespace sunny::core
