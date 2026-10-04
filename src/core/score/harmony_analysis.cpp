/**
 * @file harmony_analysis.cpp
 * @brief Score IR harmonic analysis — implementation
 *
 */

#include <algorithm>
#include <set>
#include <sunny/core/harmony/harmonic_function.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/score/harmony_analysis.hpp>
#include <sunny/core/score/projection.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/time.hpp>

namespace sunny::core {

namespace {

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

bool is_minor_key(const KeySignature& key) {
    auto ints = key.mode.get_intervals();
    return ints.size() >= 3 && ints[2] == 3;
}

ScoreHarmonicFunction map_function(HarmonicFunction hf) {
    switch (hf) {
    case HarmonicFunction::Tonic:
        return ScoreHarmonicFunction::Tonic;
    case HarmonicFunction::Subdominant:
        return ScoreHarmonicFunction::Predominant;
    case HarmonicFunction::Dominant:
        return ScoreHarmonicFunction::Dominant;
    }
    return ScoreHarmonicFunction::Ambiguous;
}

/// Build a ChordVoicing from recognised root, quality, and actual notes.
ChordVoicing build_voicing(PitchClass root,
                           const std::string& quality,
                           const std::vector<MidiNote>& midi_notes) {
    ChordVoicing voicing;
    voicing.root = root;
    voicing.quality = quality;
    voicing.notes = midi_notes;
    voicing.inversion = 0;

    if (!midi_notes.empty()) {
        PitchClass bass_pc = PitchClass::wrapped(midi_notes.front());
        if (bass_pc != root) {
            auto intervals = chord_quality_intervals(quality);
            if (intervals) {
                for (std::size_t i = 1; i < intervals->size(); ++i) {
                    PitchClass member = PitchClass::wrapped(root + (*intervals)[i]);
                    if (member == bass_pc) {
                        voicing.inversion = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
    }

    return voicing;
}

constexpr double HARMONY_CONFIDENCE_THRESHOLD = 0.5;

/// Derive from exact note/context changes in bars [start_bar, end_bar].
Result<HarmonicAnnotationLayer>
derive_for_range(const Score& score, std::uint32_t start_bar, std::uint32_t end_bar) {
    HarmonicAnnotationLayer layer;
    if (end_bar < start_bar) return layer;
    auto notes = project_symbolic_notes(score);
    if (!notes) return std::unexpected(notes.error());
    auto range_start = score_time_to_absolute_beat({start_bar, Beat::zero()}, score.time_map);
    auto range_end = score_time_to_absolute_beat({end_bar + 1, Beat::zero()}, score.time_map);
    if (!range_start || !range_end) return std::unexpected(ErrorCode::InvalidTimeSignature);
    std::vector<Beat> boundaries{*range_start, *range_end};
    for (const auto& key : score.key_map) {
        auto absolute = score_time_to_absolute_beat(key.position, score.time_map);
        if (!absolute) return std::unexpected(absolute.error());
        boundaries.push_back(*absolute);
    }
    auto slices = partition_symbolic_notes(*notes, boundaries);
    if (!slices) return std::unexpected(slices.error());
    PitchClassSet prev_pcs;
    std::vector<MidiNote> prev_notes;
    std::optional<std::size_t> current_idx;
    std::optional<KeySignature> previous_key;
    for (const auto& slice : *slices) {
        if (slice.start < *range_start || slice.start >= *range_end) continue;
        auto position_result =
            absolute_beat_to_score_time(slice.start, score.time_map, score.metadata.total_bars);
        if (!position_result) return std::unexpected(position_result.error());
        const ScoreTime position = *position_result;
        auto key_opt = query_key_at(score, position);
        if (!key_opt) return std::unexpected(ErrorCode::InvariantViolation);
        const KeySignature key = *key_opt;
        const PitchClass key_root = pc(key.root);
        const bool minor = is_minor_key(key);
        const auto scale_ints = key.mode.get_intervals();
        auto duration = checked_sub(std::min(slice.end, *range_end), slice.start);
        if (!duration) return std::unexpected(duration.error());
        PitchClassSet pcs;
        std::vector<MidiNote> midi_notes;
        for (auto index : slice.sounding_indices) {
            const auto& note = (*notes)[index];
            auto value = midi(note.pitch);
            if (!value) return std::unexpected(value.error());
            pcs.insert(pitch_class(*value));
            midi_notes.push_back(*value);
        }
        std::sort(midi_notes.begin(), midi_notes.end());
        if (pcs.empty()) {
            current_idx.reset();
            previous_key.reset();
            continue;
        }
        const bool key_changed = !previous_key || key != *previous_key;
        previous_key = key;
        if (pcs == prev_pcs && midi_notes == prev_notes && current_idx && !key_changed) {
            auto extended = checked_add(layer[*current_idx].duration, *duration);
            if (!extended) return std::unexpected(extended.error());
            layer[*current_idx].duration = *extended;
            continue;
        }
        prev_pcs = pcs;
        prev_notes = midi_notes;
        HarmonicAnnotation ann;
        ann.position = position;
        ann.duration = *duration;
        ann.key_context = key;
        auto recognised = recognize_chord(pcs);

        if (recognised) {
            auto& [root, quality] = *recognised;
            ann.chord = build_voicing(root, quality, midi_notes);

            auto numeral = chord_to_numeral(root, quality, key_root, scale_ints, minor);
            ann.roman_numeral = numeral ? *numeral : "?";

            // Derive function from the recognized root's degree
            // rather than calling analyze_chord_function, which
            // would re-derive the root independently and risk
            // divergence between numeral and function.
            if (numeral) {
                auto parsed = parse_roman_numeral_full(*numeral);
                if (parsed && parsed->degree >= 0 && parsed->degree < 7) {
                    static constexpr HarmonicFunction DEGREE_FN[7] = {
                        HarmonicFunction::Tonic,
                        HarmonicFunction::Subdominant,
                        HarmonicFunction::Tonic,
                        HarmonicFunction::Subdominant,
                        HarmonicFunction::Dominant,
                        HarmonicFunction::Tonic,
                        HarmonicFunction::Dominant,
                    };
                    ann.function = map_function(DEGREE_FN[parsed->degree]);
                } else {
                    ann.function = ScoreHarmonicFunction::Ambiguous;
                }
            } else {
                ann.function = ScoreHarmonicFunction::Ambiguous;
            }
            ann.confidence = 1.0f;
        } else {
            ann.chord.notes = midi_notes;
            ann.roman_numeral = "?";
            ann.function = ScoreHarmonicFunction::Ambiguous;
            ann.confidence = 0.3f;
        }

        layer.push_back(ann);
        current_idx = layer.size() - 1;
    }
    std::erase_if(layer, [](const HarmonicAnnotation& ann) {
        return ann.confidence < HARMONY_CONFIDENCE_THRESHOLD;
    });
    return layer;
}

} // anonymous namespace

// =============================================================================
// derive_harmonic_layer
// =============================================================================

Result<HarmonicAnnotationLayer> derive_harmonic_layer(const Score& score) {
    if (score.time_map.empty() || score.key_map.empty()) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }

    auto derived = derive_for_range(score, 1, score.metadata.total_bars);
    if (!derived) return std::unexpected(derived.error());
    auto layer = std::move(*derived);

    // Post-process: detect cadences at section boundaries and score end
    std::set<std::uint32_t> boundary_bars;
    for (const auto& sec : score.section_map) {
        if (sec.end.bar > 0) boundary_bars.insert(sec.end.bar);
    }
    boundary_bars.insert(score.metadata.total_bars + 1);

    for (std::size_t i = 1; i < layer.size(); ++i) {
        bool at_boundary = boundary_bars.contains(layer[i].position.bar) ||
                           boundary_bars.contains(layer[i].position.bar + 1);

        if (i == layer.size() - 1) at_boundary = true;

        if (at_boundary && !layer[i - 1].chord.notes.empty() && !layer[i].chord.notes.empty()) {
            auto key_opt = query_key_at(score, layer[i].position);
            if (!key_opt) continue;
            PitchClass key_root = pc(key_opt->root);
            bool minor = is_minor_key(*key_opt);

            auto cadence = detect_cadence(layer[i - 1].chord, layer[i].chord, key_root, minor);
            if (cadence.type != CadenceType::None) {
                layer[i].cadence = cadence.type;
            }
        }
    }

    return layer;
}

// =============================================================================
// refresh_stale_regions
// =============================================================================

VoidResult refresh_stale_regions(Score& score) {
    if (score.stale_harmonic_regions.empty()) return {};

    auto derived = derive_harmonic_layer(score);
    if (!derived) return std::unexpected(derived.error());
    std::vector<std::pair<Beat, Beat>> regions;
    for (const auto& region : score.stale_harmonic_regions) {
        auto start = score_time_to_absolute_beat(region.start, score.time_map);
        auto end = score_time_to_absolute_beat(region.end, score.time_map);
        if (!start || !end || *end <= *start)
            return std::unexpected(ErrorCode::InvalidTimeSignature);
        regions.emplace_back(*start, *end);
    }
    HarmonicAnnotationLayer replacement;
    const auto append_fragments = [&](const HarmonicAnnotationLayer& source,
                                      bool inside) -> VoidResult {
        for (const auto& annotation : source) {
            auto start = score_time_to_absolute_beat(annotation.position, score.time_map);
            if (!start) return std::unexpected(start.error());
            auto end = checked_add(*start, annotation.duration);
            if (!end || *end <= *start) return std::unexpected(ErrorCode::InvariantViolation);
            std::vector<Beat> cuts{*start, *end};
            for (const auto& region : regions) {
                if (*start < region.first && region.first < *end) cuts.push_back(region.first);
                if (*start < region.second && region.second < *end) cuts.push_back(region.second);
            }
            std::sort(cuts.begin(), cuts.end());
            cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
            for (std::size_t index = 0; index + 1 < cuts.size(); ++index) {
                const bool stale =
                    std::any_of(regions.begin(), regions.end(), [&](const auto& region) {
                        return region.first <= cuts[index] && cuts[index] < region.second;
                    });
                if (stale != inside) continue;
                auto position = absolute_beat_to_score_time(
                    cuts[index], score.time_map, score.metadata.total_bars);
                auto duration = checked_sub(cuts[index + 1], cuts[index]);
                if (!position || !duration) return std::unexpected(ErrorCode::InvariantViolation);
                auto fragment = annotation;
                fragment.position = *position;
                fragment.duration = *duration;
                if (cuts[index + 1] != *end) fragment.cadence.reset();
                replacement.push_back(std::move(fragment));
            }
        }
        return {};
    };
    auto kept = append_fragments(score.harmonic_annotations, false);
    if (!kept) return kept;
    auto refreshed = append_fragments(*derived, true);
    if (!refreshed) return refreshed;
    std::sort(replacement.begin(), replacement.end(), [](const auto& a, const auto& b) {
        return a.position < b.position;
    });
    score.harmonic_annotations = std::move(replacement);
    score.stale_harmonic_regions.clear();
    return {};
}

// =============================================================================
// clear_stale_harmonic_regions
// =============================================================================

void clear_stale_harmonic_regions(Score& score) {
    score.stale_harmonic_regions.clear();
}

} // namespace sunny::core
