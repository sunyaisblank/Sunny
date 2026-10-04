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
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sunny/core/form/motif.hpp>
#include <sunny/core/form/structure.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/melody/analysis.hpp>
#include <sunny/core/pitch/pitch_class_set.hpp>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/score/harmony_analysis.hpp>
#include <sunny/core/score/projection.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>

namespace sunny::core {

namespace {

// =========================================================================
// Shared helpers
// =========================================================================

/// Exact key-map context includes intra-bar changes.
KeySignature key_at_position(const Score& score, ScoreTime position) {
    KeySignature key = score.key_map.front().key;
    for (const auto& entry : score.key_map) {
        if (entry.position > position) break;
        key = entry.key;
    }
    return key;
}

using MelodicLane = std::pair<PartId, std::uint8_t>;

struct MelodicAttack {
    SymbolicNoteSpan source;
    MidiNote pitch{};
    Beat duration{};
    KeySignature key{};
};

struct AnalysisNotes {
    std::vector<SymbolicNoteSpan> folded;
    std::map<MelodicLane, std::vector<MelodicAttack>> lanes;
};

Result<AnalysisNotes> analysis_notes(const Score& score) {
    if (score.key_map.empty()) return std::unexpected(ErrorCode::InvalidMutation);
    auto projected = project_symbolic_notes(score);
    if (!projected) return std::unexpected(projected.error());
    auto folded = fold_symbolic_ties(*projected);
    if (!folded) return std::unexpected(folded.error());
    AnalysisNotes result;
    result.folded = std::move(*folded);
    // Empty represented lanes still have a record, with note_count=0.
    for (const auto& part : score.parts)
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                result.lanes.try_emplace({part.id, voice.voice_index});

    std::map<MelodicLane, std::map<Beat, MelodicAttack>> attacks;
    for (const auto& note : result.folded) {
        auto pitch = midi(note.pitch);
        if (!pitch) return std::unexpected(pitch.error());
        auto duration = checked_sub(note.end, note.start);
        if (!duration) return std::unexpected(duration.error());
        auto& onsets = attacks[{note.part_id, note.voice_index}];
        const auto found = onsets.find(note.start);
        // Consider newly attacked heads only; a sustained higher note cannot
        // conceal a lower attack. Equal unisons keep stable source identity.
        if (found == onsets.end() || *pitch > found->second.pitch)
            onsets.insert_or_assign(
                note.start,
                MelodicAttack{
                    note, *pitch, *duration, key_at_position(score, note.source_position)});
    }
    for (auto& [lane, onsets] : attacks)
        for (auto& [start, attack] : onsets)
            result.lanes[lane].push_back(std::move(attack));
    return result;
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

Result<HarmonicAnalysisRecord> analyze_harmonic(const Score& score) {
    if (!is_compilable(score)) return std::unexpected(ErrorCode::InvalidMutation);
    HarmonicAnalysisRecord result;
    Score analytical = score;
    if (!analytical.stale_harmonic_regions.empty()) {
        auto refreshed = refresh_stale_regions(analytical);
        if (!refreshed) return std::unexpected(refreshed.error());
    }
    if (analytical.harmonic_annotations.empty()) {
        auto layer = derive_harmonic_layer(analytical);
        if (!layer) return std::unexpected(layer.error());
        analytical.harmonic_annotations = std::move(*layer);
    }
    for (const auto& ann : analytical.harmonic_annotations) {
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
            case sunny::core::CadenceType::None:
                continue;
            default:
                return std::unexpected(ErrorCode::InvariantViolation);
            }
            ce.section_context = section_for_bar(analytical.section_map, ann.position.bar);
            result.cadence_inventory.push_back(std::move(ce));
        }
    }

    // Build progression patterns from consecutive annotations
    if (analytical.harmonic_annotations.size() >= 2) {
        for (std::size_t i = 0; i + 1 < analytical.harmonic_annotations.size(); ++i) {
            std::vector<std::string> pair = {analytical.harmonic_annotations[i].roman_numeral,
                                             analytical.harmonic_annotations[i + 1].roman_numeral};
            // Check if this bigram already exists
            bool found = false;
            for (auto& prog : result.progression_inventory) {
                if (prog.roman_numerals == pair) {
                    prog.occurrences.push_back(analytical.harmonic_annotations[i].position);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ProgressionPattern pp;
                pp.roman_numerals = pair;
                pp.length = 2;
                pp.occurrences.push_back(analytical.harmonic_annotations[i].position);
                {
                    auto kn = to_spn(analytical.harmonic_annotations[i].key_context.root);
                    if (!kn) return std::unexpected(kn.error());
                    pp.key_context = *kn;
                }
                result.progression_inventory.push_back(std::move(pp));
            }
        }
    }

    // Harmonic rhythm: count chord changes per bar
    std::map<std::uint32_t, std::uint32_t> changes_per_bar;
    for (const auto& ann : analytical.harmonic_annotations)
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
        result.harmonic_rhythm.variance = var_sum / static_cast<float>(score.metadata.total_bars);
    }

    result.tonal_plan = analyze_tonal_plan(score);

    return result;
}

// =========================================================================
// Melodic Analysis
// =========================================================================

namespace {

Result<MelodicAnalysisRecord> melodic_from_notes(const AnalysisNotes& notes) {
    MelodicAnalysisRecord result;
    std::size_t max_notes = 0;
    for (const auto& [lane, attacks] : notes.lanes) {
        if (attacks.size() > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        VoiceMelodicAnalysis voice;
        voice.part_id = lane.first;
        voice.voice_index = lane.second;
        voice.note_count = static_cast<std::uint32_t>(attacks.size());
        if (!attacks.empty()) {
            std::vector<MidiNote> pitches;
            for (const auto& attack : attacks)
                pitches.push_back(attack.pitch);
            std::sort(pitches.begin(), pitches.end());
            voice.range_low = static_cast<std::int8_t>(pitches.front());
            voice.range_high = static_cast<std::int8_t>(pitches.back());
            voice.tessitura_low = static_cast<std::int8_t>(pitches[pitches.size() / 10]);
            voice.tessitura_high = static_cast<std::int8_t>(pitches[(pitches.size() * 9) / 10]);
            std::size_t chromatic = 0, conjunct = 0;
            for (std::size_t i = 0; i < attacks.size(); ++i) {
                const auto& attack = attacks[i];
                const auto intervals = attack.key.mode.get_intervals();
                const int offset = (static_cast<int>(pc(attack.source.pitch)) -
                                    static_cast<int>(pc(attack.key.root)) + 12) %
                                   12;
                const auto found = std::ranges::find(intervals, offset);
                const auto degree =
                    found == intervals.end()
                        ? std::uint8_t{0}
                        : static_cast<std::uint8_t>(std::distance(intervals.begin(), found) + 1);
                ++voice.scale_degree_distribution[degree];
                if (degree == 0) ++chromatic;
                if (i > 0) {
                    const int interval =
                        static_cast<int>(attack.pitch) - static_cast<int>(attacks[i - 1].pitch);
                    ++voice.interval_distribution[static_cast<std::int8_t>(interval)];
                    if (std::abs(interval) <= 2) ++conjunct;
                }
            }
            voice.chromaticism_rate = static_cast<float>(chromatic) / attacks.size();
            if (attacks.size() > 1)
                voice.conjunct_proportion = static_cast<float>(conjunct) / (attacks.size() - 1);
            if (attacks.size() > max_notes) {
                max_notes = attacks.size();
                result.primary_melody_voice = lane.first;
                result.primary_melody_voice_index = lane.second;
            }
        }
        result.per_voice_analysis.push_back(std::move(voice));
    }
    return result;
}

struct MetricalContext {
    std::vector<Beat> downbeats;
    std::map<PartId, std::vector<Beat>> group_starts;
    std::map<std::pair<PartId, std::uint32_t>, TimeSignature> metres;
};

Result<MetricalContext> metrical_context(const Score& score) {
    MetricalContext result;
    if (score.time_map.empty()) return std::unexpected(ErrorCode::InvalidTimeSignature);
    if (score.metadata.total_bars == std::numeric_limits<std::uint32_t>::max())
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    for (const auto& part : score.parts)
        if (part.measures.size() != score.metadata.total_bars)
            return std::unexpected(ErrorCode::InvalidMutation);
    TimeSignature global = score.time_map.front().time_signature;
    std::size_t entry = 0;
    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        while (entry + 1 < score.time_map.size() && score.time_map[entry + 1].bar <= bar)
            global = score.time_map[++entry].time_signature;
        auto start = score_time_to_absolute_beat({bar, Beat::zero()}, score.time_map);
        auto end = score_time_to_absolute_beat({bar + 1, Beat::zero()}, score.time_map);
        if (!start) return std::unexpected(start.error());
        if (!end) return std::unexpected(end.error());
        result.downbeats.push_back(*start);
        for (const auto& part : score.parts) {
            const auto& local = part.measures[bar - 1].local_time;
            const auto& metre = local ? *local : global;
            result.metres.emplace(std::pair{part.id, bar}, metre);
            int pulse = 0;
            for (const int group : metre.groups()) {
                if (pulse > 0) {
                    auto boundary = checked_add(*start, Beat{pulse, metre.denominator()});
                    if (!boundary) return std::unexpected(boundary.error());
                    // Score's global bar frame owns the next downbeat. Local
                    // grouping affects pulses represented inside that frame.
                    if (*boundary < *end) result.group_starts[part.id].push_back(*boundary);
                }
                pulse += group;
            }
        }
        if (bar == score.metadata.total_bars) result.downbeats.push_back(*end);
    }
    return result;
}

Result<bool> is_metrically_syncopating(const SymbolicNoteSpan& note,
                                       const MetricalContext& context) {
    const auto found = context.metres.find({note.part_id, note.source_position.bar});
    if (found == context.metres.end()) return std::unexpected(ErrorCode::InvalidMutation);
    const auto& metre = found->second;
    auto pulses = checked_mul(note.source_position.beat, Beat{metre.denominator(), 1});
    if (!pulses) return std::unexpected(pulses.error());
    int strength = 0;
    if (note.source_position.beat == Beat::zero())
        strength = 3;
    else if (pulses->denominator() == 1) {
        strength = 1;
        int group_start = 0;
        for (const int group : metre.groups()) {
            if (pulses->numerator() == group_start) {
                strength = 2;
                break;
            }
            group_start += group;
        }
    }
    if (strength == 3) return false;
    const auto past_stronger = [&](const std::vector<Beat>& boundaries) {
        const auto next = std::upper_bound(boundaries.begin(), boundaries.end(), note.start);
        return next != boundaries.end() && *next < note.end;
    };
    if (past_stronger(context.downbeats)) return true;
    if (strength < 2) {
        const auto groups = context.group_starts.find(note.part_id);
        if (groups != context.group_starts.end() && past_stronger(groups->second)) return true;
    }
    if (strength == 0) {
        // Only the next denominator pulse can be needed. Arithmetic avoids
        // enumerating potentially very large numerators for every bar.
        const auto next_pulse = pulses->numerator() / pulses->denominator() + 1;
        const Beat offset{next_pulse, metre.denominator()};
        if (offset < metre.measure_duration()) {
            auto bar_start = checked_sub(note.start, note.source_position.beat);
            if (!bar_start) return std::unexpected(bar_start.error());
            auto next = checked_add(*bar_start, offset);
            if (!next) return std::unexpected(next.error());
            if (*next < note.end) return true;
        }
    }
    return false;
}

Result<RhythmicAnalysisRecord> rhythmic_from_notes(const Score& score, const AnalysisNotes& notes) {
    RhythmicAnalysisRecord result;
    for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
        const TimeSignature* active = nullptr;
        for (const auto& entry : score.time_map) {
            if (entry.bar > bar) break;
            active = &entry.time_signature;
        }
        if (active) ++result.metre_distribution[metre_key(*active)];
    }

    double rest_duration = 0.0, allocated_duration = 0.0;
    std::uint64_t time_changes = score.time_map.empty() ? 0 : score.time_map.size() - 1;
    for (const auto& part : score.parts)
        for (const auto& measure : part.measures) {
            if (measure.local_time) ++time_changes;
            for (const auto& voice : measure.voices)
                for (const auto& event : voice.events) {
                    if (const auto* rest = event.as_rest()) {
                        rest_duration += rest->duration.to_float();
                        allocated_duration += rest->duration.to_float();
                    } else if (const auto* group = event.as_note_group()) {
                        // A chord occupies one voice allocation, independent
                        // of how many pitches or duplicate unisons it contains.
                        allocated_duration += group->duration.to_float();
                    }
                }
        }
    result.onset_density.resize(score.metadata.total_bars, 0.0f);
    auto metre = metrical_context(score);
    if (!metre) return std::unexpected(metre.error());
    std::uint64_t eligible = 0, syncopating = 0;
    for (const auto& note : notes.folded) {
        auto duration = checked_sub(note.end, note.start);
        if (!duration) return std::unexpected(duration.error());
        ++result.duration_distribution[beat_key(*duration)];
        if (note.source_position.bar == 0 || note.source_position.bar > score.metadata.total_bars)
            return std::unexpected(ErrorCode::InvalidMutation);
        ++result.onset_density[note.source_position.bar - 1];
        if (!note.grace) {
            ++eligible;
            auto sync = is_metrically_syncopating(note, *metre);
            if (!sync) return std::unexpected(sync.error());
            if (*sync) ++syncopating;
        }
    }
    if (eligible > 0) result.syncopation_index = static_cast<float>(syncopating) / eligible;
    if (allocated_duration > 0.0)
        result.rest_proportion = static_cast<float>(rest_duration / allocated_duration);
    if (score.metadata.total_bars > 0)
        result.metrical_complexity = static_cast<float>(time_changes) / score.metadata.total_bars;
    for (const auto& tempo : score.tempo_map) {
        auto effective =
            effective_quarter_tempo_at(tempo.position, score.tempo_map, score.time_map);
        if (!effective) return std::unexpected(effective.error());
        result.tempo_profile.emplace_back(tempo.position,
                                          static_cast<float>(effective->to_float()));
    }
    for (const auto& section : score.section_map) {
        std::size_t count = 0;
        for (const auto& note : notes.folded)
            if (note.source_position >= section.start && note.source_position < section.end)
                ++count;
        const auto bars =
            section.end.bar - section.start.bar + (section.end.beat > Beat::zero() ? 1U : 0U);
        if (bars > 0)
            result.note_density_by_section[section.label] = static_cast<float>(count) / bars;
    }
    return result;
}

} // namespace

// =========================================================================
// Rhythmic Analysis
// =========================================================================

Result<RhythmicAnalysisRecord> analyze_rhythmic(const Score& score) {
    auto notes = analysis_notes(score);
    if (!notes) return std::unexpected(notes.error());
    return rhythmic_from_notes(score, *notes);
}

// =========================================================================
// Formal Analysis
// =========================================================================

Result<FormalAnalysisRecord> analyze_formal(const Score& score) {
    if (score.key_map.empty()) return std::unexpected(ErrorCode::InvalidMutation);
    FormalAnalysisRecord result;
    result.total_duration_bars = score.metadata.total_bars;

    // Convert SectionMap to FormalSections
    for (const auto& sec : score.section_map) {
        FormalSection fs;
        fs.label = sec.label;
        fs.start_bar = sec.start.bar;
        fs.end_bar = sec.end.bar;
        fs.length_bars = sec.end.bar - sec.start.bar;
        fs.key = key_signature_name(key_at_position(score, sec.start));
        auto tempo = effective_quarter_tempo_at(sec.start, score.tempo_map, score.time_map);
        if (!tempo) return std::unexpected(tempo.error());
        fs.tempo = static_cast<float>(tempo->to_float());
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

    std::vector<std::string> labels;
    for (const auto& section : result.section_plan)
        labels.push_back(section.label);
    switch (classify_form(labels)) {
    case SectionalForm::Binary:
        result.form_type = FormClassification::BinarySimple;
        break;
    case SectionalForm::RoundedBinary:
        result.form_type = FormClassification::BinaryRounded;
        break;
    case SectionalForm::Ternary:
        result.form_type = FormClassification::Ternary;
        break;
    case SectionalForm::Rondo:
        result.form_type = FormClassification::Rondo;
        break;
    case SectionalForm::Sonata:
        result.form_type = FormClassification::SonataAllegro;
        break;
    case SectionalForm::ThemeVariations:
        result.form_type = FormClassification::ThemeAndVariations;
        break;
    case SectionalForm::Strophic:
        result.form_type = FormClassification::Strophic;
        break;
    case SectionalForm::ThroughComposed:
        result.form_type = FormClassification::ThroughComposed;
        break;
    case SectionalForm::Unknown:
        result.form_type = FormClassification::Other;
        break;
    }

    result.tonal_plan = analyze_tonal_plan(score);

    return result;
}

// =========================================================================
// Voice-Leading Analysis
// =========================================================================

namespace {

struct VoiceLeadingComputation {
    VoiceLeadingAnalysisRecord record;
    std::uint64_t motion_samples = 0;
    std::uint64_t spacing_samples = 0;
    std::uint64_t retention_observations = 0;
};

Result<VoiceLeadingComputation> voice_leading_from_notes(const AnalysisNotes& notes) {
    auto slices = partition_symbolic_notes(notes.folded);
    if (!slices) return std::unexpected(slices.error());
    VoiceLeadingComputation result;
    std::map<MelodicLane, int> previous;
    std::uint64_t contrary = 0, similar = 0, oblique = 0, parallel = 0;
    std::uint64_t retained = 0, retention_total = 0;
    for (const auto& slice : *slices) {
        std::map<MelodicLane, int> current;
        for (const auto index : slice.sounding_indices) {
            const auto& note = notes.folded[index];
            auto pitch = midi(note.pitch);
            if (!pitch) return std::unexpected(pitch.error());
            const MelodicLane lane{note.part_id, note.voice_index};
            const int value = static_cast<int>(*pitch);
            auto [found, inserted] = current.try_emplace(lane, value);
            if (!inserted) found->second = std::max(found->second, value);
        }
        // Representative notes remain in their structural lane; no sorting by
        // pitch that could hide a crossing. All lane pairs are sampled.
        for (auto first = current.begin(); first != current.end(); ++first) {
            for (auto second = std::next(first); second != current.end(); ++second) {
                ++result.record.spacing_distribution[static_cast<std::int8_t>(
                    std::abs(first->second - second->second))];
                ++result.spacing_samples;
                const auto old_first = previous.find(first->first);
                const auto old_second = previous.find(second->first);
                if (old_first == previous.end() || old_second == previous.end()) continue;
                const int motion_first = first->second - old_first->second;
                const int motion_second = second->second - old_second->second;
                if (motion_first == 0 && motion_second == 0) continue;
                ++result.motion_samples;
                if (motion_first == 0 || motion_second == 0)
                    ++oblique;
                else if ((motion_first > 0) != (motion_second > 0))
                    ++contrary;
                else if (motion_first == motion_second) {
                    ++parallel;
                    const int before = std::abs(old_first->second - old_second->second) % 12;
                    const int after = std::abs(first->second - second->second) % 12;
                    if (before == 7 && after == 7) ++result.record.parallel_fifths_count;
                    if (before == 0 && after == 0) ++result.record.parallel_octaves_count;
                } else
                    ++similar;
                const int before_order = old_first->second - old_second->second;
                const int after_order = first->second - second->second;
                if ((before_order < 0 && after_order > 0) || (before_order > 0 && after_order < 0))
                    ++result.record.voice_crossing_count;
            }
        }
        if (!previous.empty()) {
            std::set<int> before, after;
            for (const auto& [lane, pitch] : previous)
                before.insert(pitch);
            for (const auto& [lane, pitch] : current)
                after.insert(pitch);
            retention_total += before.size();
            result.retention_observations += before.size();
            for (const auto pitch : before)
                if (after.contains(pitch)) ++retained;
        }
        previous = std::move(current);
    }
    if (result.motion_samples > 0) {
        const auto total = static_cast<float>(result.motion_samples);
        result.record.contrary_motion_proportion = static_cast<float>(contrary) / total;
        result.record.similar_motion_proportion = static_cast<float>(similar) / total;
        result.record.oblique_motion_proportion = static_cast<float>(oblique) / total;
        result.record.parallel_motion_proportion = static_cast<float>(parallel) / total;
    }
    if (retention_total > 0)
        result.record.common_tone_retention_rate = static_cast<float>(retained) / retention_total;
    return result;
}

} // namespace

Result<VoiceLeadingAnalysisRecord> analyze_voice_leading(const Score& score) {
    auto notes = analysis_notes(score);
    if (!notes) return std::unexpected(notes.error());
    auto computation = voice_leading_from_notes(*notes);
    if (!computation) return std::unexpected(computation.error());
    return std::move(computation->record);
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

    // Instrument-name presence is a union of active global bars. Several
    // Parts may share a display name; later silent Parts cannot erase sound.
    std::map<std::string, std::set<std::uint32_t>> active_by_name;
    for (const auto& part : score.parts) {
        auto& active = active_by_name[part.definition.name];
        for (std::uint32_t bar = 0; bar < part.measures.size(); ++bar) {
            const bool sounding =
                std::ranges::any_of(part.measures[bar].voices, [](const auto& voice) {
                    return std::ranges::any_of(
                        voice.events, [](const auto& event) { return event.is_note_group(); });
                });
            if (sounding) active.insert(bar);
        }
    }
    for (const auto& [name, active] : active_by_name)
        result.instrument_usage[name] =
            score.metadata.total_bars > 0
                ? static_cast<float>(active.size()) / static_cast<float>(score.metadata.total_bars)
                : 0.0f;

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

namespace {

struct MotifWindow {
    MelodicLane lane;
    std::size_t index = 0;
    std::size_t length = 0;
};

struct MotivicComputation {
    MotivicAnalysisRecord record;
    std::uint64_t candidate_windows = 0;
};

Result<MotivicComputation> motivic_from_notes(const Score& score, const AnalysisNotes& notes) {
    // Ordered signatures contain exact directed intervals and exact duration
    // ratios. Float serialization is an output boundary, never a matching key.
    using Signature = std::pair<std::vector<std::int8_t>, std::vector<Beat>>;
    std::map<Signature, std::vector<MotifWindow>> groups;
    MotivicComputation result;
    std::size_t total_attacks = 0;
    for (const auto& [lane, attacks] : notes.lanes) {
        total_attacks += attacks.size();
        for (std::size_t length = 3; length <= 8; ++length) {
            for (std::size_t start = 0; start + length <= attacks.size(); ++start) {
                bool contiguous = true;
                Signature signature;
                for (std::size_t offset = 0; offset < length; ++offset) {
                    const auto& attack = attacks[start + offset];
                    if (offset > 0) {
                        const auto& previous = attacks[start + offset - 1];
                        if (previous.source.end != attack.source.start) {
                            contiguous = false;
                            break;
                        }
                        signature.first.push_back(static_cast<std::int8_t>(
                            static_cast<int>(attack.pitch) - static_cast<int>(previous.pitch)));
                    }
                    auto ratio = checked_div(attack.duration, attacks[start].duration);
                    if (!ratio) return std::unexpected(ratio.error());
                    signature.second.push_back(*ratio);
                }
                if (!contiguous) continue;
                ++result.candidate_windows;
                groups[std::move(signature)].push_back({lane, start, length});
            }
        }
    }

    std::set<std::pair<MelodicLane, std::size_t>> covered;
    for (auto& [signature, windows] : groups) {
        std::ranges::sort(windows, [&](const auto& left, const auto& right) {
            const auto& left_attack = notes.lanes.at(left.lane)[left.index];
            const auto& right_attack = notes.lanes.at(right.lane)[right.index];
            if (left_attack.source.start != right_attack.source.start)
                return left_attack.source.start < right_attack.source.start;
            return left.lane < right.lane;
        });
        std::map<MelodicLane, Beat> previous_end;
        std::vector<MotifWindow> selected;
        for (const auto& window : windows) {
            const auto& attacks = notes.lanes.at(window.lane);
            const auto found = previous_end.find(window.lane);
            if (found != previous_end.end() && attacks[window.index].source.start < found->second)
                continue;
            previous_end[window.lane] = attacks[window.index + window.length - 1].source.end;
            selected.push_back(window);
        }
        if (selected.size() < 2) continue;
        const auto& original = selected.front();
        const auto& original_lane = notes.lanes.at(original.lane);
        std::vector<MidiNote> original_pitches;
        ThematicUnit unit;
        unit.id = ThematicUnitId{result.record.thematic_units.size() + 1};
        unit.label =
            "exact-window-" + std::to_string(original.length) + "-" + std::to_string(unit.id.value);
        unit.intervals = signature.first;
        for (const auto interval : unit.intervals)
            unit.contour.push_back(static_cast<std::int8_t>((interval > 0) - (interval < 0)));
        for (std::size_t offset = 0; offset < original.length; ++offset) {
            original_pitches.push_back(original_lane[original.index + offset].pitch);
            unit.rhythm.push_back(
                static_cast<float>(original_lane[original.index + offset].duration.to_float()));
        }
        for (const auto& window : selected) {
            const auto& attacks = notes.lanes.at(window.lane);
            const auto& first = attacks[window.index];
            const auto& last = attacks[window.index + window.length - 1];
            std::vector<MidiNote> pitches;
            for (std::size_t offset = 0; offset < window.length; ++offset) {
                pitches.push_back(attacks[window.index + offset].pitch);
                covered.insert({window.lane, window.index + offset});
            }
            const auto pitch_transform = classify_transformation(original_pitches, pitches);
            if (pitch_transform != MotivicTransform::Repetition &&
                pitch_transform != MotivicTransform::Transposition)
                return std::unexpected(ErrorCode::InvariantViolation);
            auto scale = checked_div(first.duration, original_lane[original.index].duration);
            if (!scale) return std::unexpected(scale.error());
            ThematicOccurrence occurrence;
            occurrence.position = first.source.source_position;
            occurrence.part_id = window.lane.first;
            occurrence.voice_index = window.lane.second;
            auto end = absolute_beat_to_score_time(
                last.source.end, score.time_map, score.metadata.total_bars);
            if (!end) return std::unexpected(end.error());
            occurrence.end = *end;
            occurrence.key = key_signature_name(first.key);
            occurrence.transformation = pitch_transform == MotivicTransform::Transposition
                                            ? ThematicTransformation::TransposedExact
                                            : ThematicTransformation::Original;
            if (*scale != Beat{1, 1}) {
                // The single occurrence label gives duration scaling priority;
                // a concurrent exact transposition also has its own inventory entry.
                if (pitch_transform == MotivicTransform::Transposition)
                    result.record.transformation_inventory.push_back(
                        {unit.id,
                         occurrence.position,
                         ThematicTransformation::TransposedExact,
                         occurrence.key});
                occurrence.transformation = *scale > Beat{1, 1}
                                                ? ThematicTransformation::Augmented
                                                : ThematicTransformation::Diminished;
            }
            if (occurrence.transformation != ThematicTransformation::Original)
                result.record.transformation_inventory.push_back(
                    {unit.id, occurrence.position, occurrence.transformation, occurrence.key});
            unit.occurrences.push_back(std::move(occurrence));
        }
        result.record.thematic_units.push_back(std::move(unit));
    }
    if (total_attacks > 0)
        result.record.thematic_density = static_cast<float>(covered.size()) / total_attacks;
    // Economy and human developmental classifications have no finite contract
    // here; analyze_score explicitly marks those fields unavailable.
    return result;
}

} // namespace

Result<MelodicAnalysisRecord> analyze_melodic(const Score& score) {
    auto notes = analysis_notes(score);
    if (!notes) return std::unexpected(notes.error());
    auto result = melodic_from_notes(*notes);
    if (!result) return std::unexpected(result.error());
    auto motifs = motivic_from_notes(score, *notes);
    if (!motifs) return std::unexpected(motifs.error());
    result->thematic_material = std::move(motifs->record.thematic_units);
    return result;
}

Result<MotivicAnalysisRecord> analyze_motivic(const Score& score) {
    auto notes = analysis_notes(score);
    if (!notes) return std::unexpected(notes.error());
    auto computation = motivic_from_notes(score, *notes);
    if (!computation) return std::unexpected(computation.error());
    return std::move(computation->record);
}

// =========================================================================
// Full Analysis
// =========================================================================

Result<WorkAnalysis> analyze_score(const Score& score) {
    auto harmonic = analyze_harmonic(score);
    if (!harmonic) return std::unexpected(harmonic.error());
    auto notes = analysis_notes(score);
    if (!notes) return std::unexpected(notes.error());
    auto melodic = melodic_from_notes(*notes);
    if (!melodic) return std::unexpected(melodic.error());
    auto rhythmic = rhythmic_from_notes(score, *notes);
    if (!rhythmic) return std::unexpected(rhythmic.error());
    auto formal = analyze_formal(score);
    if (!formal) return std::unexpected(formal.error());
    auto voice_leading = voice_leading_from_notes(*notes);
    if (!voice_leading) return std::unexpected(voice_leading.error());
    auto motivic = motivic_from_notes(score, *notes);
    if (!motivic) return std::unexpected(motivic.error());
    WorkAnalysis wa;
    wa.harmonic_analysis = std::move(*harmonic);
    wa.melodic_analysis = std::move(*melodic);
    wa.rhythmic_analysis = std::move(*rhythmic);
    wa.formal_analysis = std::move(*formal);
    wa.voice_leading_analysis = std::move(voice_leading->record);
    wa.textural_analysis = analyze_textural(score);
    wa.dynamic_analysis = analyze_dynamic(score);
    wa.orchestration_analysis = analyze_orchestration(score);
    wa.motivic_analysis = std::move(motivic->record);
    wa.melodic_analysis.thematic_material = wa.motivic_analysis.thematic_units;

    const auto evidence = [&](std::string domain,
                              AnalysisEvidenceKind kind,
                              std::string method,
                              std::uint64_t observations,
                              std::vector<std::string> unavailable,
                              std::string reason = {}) {
        AnalysisEvidence item;
        item.kind = observations == 0 ? AnalysisEvidenceKind::Unavailable : kind;
        item.method = std::move(method);
        item.observations = observations;
        item.unavailable_fields = std::move(unavailable);
        if (observations == 0) item.unavailable_reason = std::move(reason);
        wa.evidence.emplace(std::move(domain), std::move(item));
    };
    std::uint64_t melodic_attacks = 0;
    for (const auto& [lane, attacks] : notes->lanes)
        melodic_attacks += attacks.size();
    std::uint64_t explicit_dynamics = 0, hairpins = 0;
    for (const auto& part : score.parts) {
        hairpins += part.hairpins.size();
        for (const auto& measure : part.measures)
            for (const auto& voice : measure.voices)
                for (const auto& event : voice.events)
                    if (const auto* group = event.as_note_group())
                        for (const auto& note : group->notes)
                            if (note.dynamic) ++explicit_dynamics;
    }

    evidence("harmonic",
             AnalysisEvidenceKind::Heuristic,
             "Exact symbolic sounding boundaries with active key/mode; supplied annotations or "
             "finite chord/Roman-numeral/cadence recognition; annotation counts per global bar.",
             notes->folded.size() + score.harmonic_annotations.size(),
             {"modulation_inventory",
              "chromatic_techniques",
              "tonicisation_frequency",
              "harmonic_rhythm.rate_by_section",
              "cadence_inventory.approach",
              "cadence_inventory.is_structural"},
             "No symbolic notes or supplied harmonic annotations.");
    std::vector<std::string> melodic_unavailable{
        "per_voice_analysis.contour_inventory",
        "per_voice_analysis.leap_resolution_rate",
        "per_voice_analysis.longest_ascending_run",
        "per_voice_analysis.longest_descending_run",
        "thematic_material.occurrences.SequentialRepetition"};
    if (std::ranges::any_of(wa.melodic_analysis.per_voice_analysis,
                            [](const auto& voice) { return voice.note_count == 0; })) {
        melodic_unavailable.insert(
            melodic_unavailable.end(),
            {"empty_lanes.range", "empty_lanes.tessitura", "empty_lanes.chromaticism_rate"});
    }
    if (std::ranges::any_of(wa.melodic_analysis.per_voice_analysis,
                            [](const auto& voice) { return voice.note_count < 2; }))
        melodic_unavailable.push_back("lanes_with_fewer_than_two_attacks.conjunct_proportion");
    evidence(
        "melodic",
        AnalysisEvidenceKind::Heuristic,
        "Per (PartId,voice_index), highest newly attacked MIDI note per exact onset after "
        "individual tie folding; full directed intervals, 10th/90th order-statistic tessitura, "
        "per-attack exact key/mode degrees; primary lane has most selected attacks.",
        melodic_attacks,
        std::move(melodic_unavailable),
        "No selected melodic attacks.");
    std::vector<std::string> rhythmic_unavailable{"rhythmic_motifs", "rubato_degree"};
    if (std::ranges::none_of(notes->folded, [](const auto& note) { return !note.grace; }))
        rhythmic_unavailable.push_back("syncopation_index");
    evidence(
        "rhythmic",
        AnalysisEvidenceKind::Heuristic,
        "Individual folded symbolic note durations and attacks (chord/unison multiplicity "
        "retained); "
        "rest proportion counts voice allocations once; nongrace attack syncopation share requires "
        "strict sustain past a later stronger boundary: downbeat=3, grouped beat start=2, "
        "denominator-unit interior pulse=1, other offset=0, including ties/bar crossings; "
        "local grouping applies inside global bar frames; global/local metre-change count per bar; "
        "effective quarter-BPM at tempo events; section attacks per touched global bar.",
        score.metadata.total_bars,
        std::move(rhythmic_unavailable),
        "No measured bars.");
    std::vector<std::string> formal_unavailable{"thematic_assignment",
                                                "symmetry_analysis",
                                                "section_plan.character",
                                                "section_plan.subsections"};
    if (score.section_map.empty()) formal_unavailable.push_back("form_type");
    if (std::ranges::any_of(score.section_map, [](const auto& section) {
            return section.start.beat != Beat::zero() || section.end.beat != Beat::zero();
        })) {
        formal_unavailable.push_back("partial_bar_sections.length_bars");
        formal_unavailable.push_back("partial_bar_sections.proportions");
    }
    evidence(
        "formal",
        AnalysisEvidenceKind::Heuristic,
        "Supplied section labels classified by the core finite form classifier; bar-index lengths "
        "and proportions; exact section-start key/mode and instantaneous effective quarter-BPM "
        "including ramps and metric modulation; declared key-map tonal plan.",
        score.metadata.total_bars,
        std::move(formal_unavailable),
        "No measured bars.");
    std::vector<std::string> voice_unavailable{"average_voice_independence", "resolution_patterns"};
    if (voice_leading->motion_samples == 0) {
        voice_unavailable.insert(voice_unavailable.end(),
                                 {"contrary_motion_proportion",
                                  "similar_motion_proportion",
                                  "oblique_motion_proportion",
                                  "parallel_motion_proportion",
                                  "parallel_fifths_count",
                                  "parallel_octaves_count",
                                  "voice_crossing_count"});
    }
    if (voice_leading->retention_observations == 0)
        voice_unavailable.push_back("common_tone_retention_rate");
    if (voice_leading->spacing_samples == 0) voice_unavailable.push_back("spacing_distribution");
    evidence(
        "voice_leading",
        AnalysisEvidenceKind::Heuristic,
        "Exact sounding-event boundary slices; highest sounding MIDI pitch per structural lane, "
        "all simultaneous lane pairs; moving consecutive pairs only; exact directed crossings "
        "and compound spacing; parallel perfect intervals require equal nonzero displacement; "
        "common-tone retention counts unique MIDI pitches retained from the previous slice; "
        "observations combine independent spacing pairs and previous unique-pitch comparisons.",
        voice_leading->spacing_samples + voice_leading->retention_observations,
        std::move(voice_unavailable),
        "No simultaneous lane-pair spacing or previous sounding pitch to compare.");
    // Part-count alone does not establish contrapuntal texture, so no musical
    // mono-/homo-/polyphonic classification is claimed by this bar inventory.
    wa.textural_analysis.texture_type_proportions.clear();
    evidence("textural",
             AnalysisEvidenceKind::Heuristic,
             "Per-global-bar part presence and note-register extrema; per-bar and section means "
             "describe inventories, not simultaneous sounding texture.",
             score.metadata.total_bars,
             {"texture_type_proportions", "spacing_profile"},
             "No measured bars.");
    // Written marks can support a traversal/bar profile, but mapping accented
    // intensities back to an ordinary section dynamic would invent a marking.
    wa.dynamic_analysis.dynamic_by_section.clear();
    std::vector<std::string> dynamic_unavailable{"dynamic_by_section"};
    if (explicit_dynamics == 0)
        dynamic_unavailable.insert(dynamic_unavailable.end(),
                                   {"dynamic_range_low",
                                    "dynamic_range_high",
                                    "dynamic_distribution",
                                    "dynamic_change_rate",
                                    "dynamic_shape",
                                    "climax_position",
                                    "subito_dynamics_count"});
    evidence("dynamic",
             AnalysisEvidenceKind::Heuristic,
             "Explicit written note dynamics and part hairpins; typed intensity order; "
             "change count in part/measure/voice/event/note traversal per bar; latest visited "
             "marked intensity carried to each bar and its first maximal bar as climax.",
             explicit_dynamics + hairpins,
             std::move(dynamic_unavailable),
             "No explicit written note dynamics or hairpins.");
    evidence(
        "orchestration",
        AnalysisEvidenceKind::Heuristic,
        "For multi-part scores, per-instrument-name bar presence and highest visited MIDI pitch "
        "per global bar as a melody-carrier heuristic; no sounding simultaneity or doubling claim.",
        wa.orchestration_analysis ? score.metadata.total_bars : 0,
        {"instrument_combinations",
         "doubling_patterns",
         "orchestral_crescendo_patterns",
         "density_orchestration_correlation"},
        "Orchestration analysis requires multiple parts.");
    evidence(
        "motivic",
        AnalysisEvidenceKind::Heuristic,
        "Exhaustive contiguous 3–8 selected-note windows per melodic lane: exact directed "
        "semitone intervals and exact rational duration ratios; greedy nonoverlap within each "
        "lane, independent occurrences across lanes. Repetition, exact transposition and uniform "
        "duration scaling only; scaling has occurrence-label priority, concurrent transposition "
        "is also inventoried. Density is the union of covered selected attacks / all selected "
        "attacks. Stored float rhythms are original whole-note durations, never matching keys.",
        motivic->candidate_windows,
        {"thematic_economy",
         "developmental_techniques",
         "transformation_inventory.Fragmented",
         "transformation_inventory.SequentialRepetition"},
        "No contiguous allocated melodic window of 3–8 notes.");
    return wa;
}

} // namespace sunny::core
