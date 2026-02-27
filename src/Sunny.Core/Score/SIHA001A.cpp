/**
 * @file SIHA001A.cpp
 * @brief Score IR harmonic analysis — implementation
 *
 * Component: SIHA001A
 * Domain: SI (Score IR) | Category: HA (Harmonic Analysis)
 */

#include "SIHA001A.h"
#include "SIQR001A.h"
#include "../Harmony/HRRN001A.h"
#include "../Harmony/HRFN001A.h"

#include <algorithm>
#include <set>

namespace Sunny::Core {

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
        case HarmonicFunction::Tonic:       return ScoreHarmonicFunction::Tonic;
        case HarmonicFunction::Subdominant: return ScoreHarmonicFunction::Predominant;
        case HarmonicFunction::Dominant:    return ScoreHarmonicFunction::Dominant;
    }
    return ScoreHarmonicFunction::Ambiguous;
}

/// Collect pitch classes sounding at a given position across all parts.
PitchClassSet collect_sounding_pcs(const Score& score, ScoreTime position) {
    PitchClassSet pcs;

    for (const auto& part : score.parts) {
        if (position.bar < 1 || position.bar > part.measures.size()) continue;
        const auto& measure = part.measures[position.bar - 1];

        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                const auto* ng = event.as_note_group();
                if (!ng) continue;

                Beat event_end = event.offset + ng->duration;
                if (event.offset <= position.beat && position.beat < event_end) {
                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        pcs.insert(static_cast<PitchClass>(((mv % 12) + 12) % 12));
                    }
                }
            }
        }
    }

    return pcs;
}

/// Collect sorted MIDI note numbers sounding at a position.
std::vector<MidiNote> collect_sounding_midi(const Score& score, ScoreTime position) {
    std::vector<MidiNote> notes;

    for (const auto& part : score.parts) {
        if (position.bar < 1 || position.bar > part.measures.size()) continue;
        const auto& measure = part.measures[position.bar - 1];

        for (const auto& voice : measure.voices) {
            for (const auto& event : voice.events) {
                const auto* ng = event.as_note_group();
                if (!ng) continue;

                Beat event_end = event.offset + ng->duration;
                if (event.offset <= position.beat && position.beat < event_end) {
                    for (const auto& note : ng->notes) {
                        int mv = midi_value(note.pitch);
                        if (mv >= 0 && mv <= 127) {
                            notes.push_back(static_cast<MidiNote>(mv));
                        }
                    }
                }
            }
        }
    }

    std::sort(notes.begin(), notes.end());
    return notes;
}

/// Build a ChordVoicing from recognised root, quality, and actual notes.
ChordVoicing build_voicing(
    PitchClass root,
    const std::string& quality,
    const std::vector<MidiNote>& midi_notes
) {
    ChordVoicing voicing;
    voicing.root = root;
    voicing.quality = quality;
    voicing.notes = midi_notes;
    voicing.inversion = 0;

    if (!midi_notes.empty()) {
        PitchClass bass_pc = static_cast<PitchClass>(midi_notes.front() % 12);
        if (bass_pc != root) {
            auto intervals = chord_quality_intervals(quality);
            if (intervals) {
                for (std::size_t i = 1; i < intervals->size(); ++i) {
                    PitchClass member = static_cast<PitchClass>(
                        (root + (*intervals)[i]) % 12);
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

/// Check if a position falls within any stale region.
bool in_stale_region(ScoreTime position, const std::vector<ScoreRegion>& regions) {
    for (const auto& r : regions) {
        if (position >= r.start && position < r.end) return true;
    }
    return false;
}

/// Derive annotations for bars in [start_bar, end_bar].
HarmonicAnnotationLayer derive_for_range(
    const Score& score,
    std::uint32_t start_bar,
    std::uint32_t end_bar
) {
    HarmonicAnnotationLayer layer;
    PitchClassSet prev_pcs;
    std::optional<std::size_t> current_idx;
    PitchClass prev_key_root = 255;  // sentinel — no previous key

    for (std::uint32_t bar = start_bar; bar <= end_bar; ++bar) {
        TimeSignature ts = query_time_signature_at(score, bar);
        KeySignature key = query_key_at(score, ScoreTime{bar, Beat::zero()});

        PitchClass key_root = pc(key.root);
        bool minor = is_minor_key(key);
        auto scale_ints = key.mode.get_intervals();

        int num_beats = ts.numerator();
        int denom = ts.denominator;

        for (int beat_idx = 0; beat_idx < num_beats; ++beat_idx) {
            Beat offset = Beat::normalise(beat_idx, denom);
            ScoreTime position{bar, offset};
            Beat beat_dur = Beat::normalise(1, denom);

            PitchClassSet pcs = collect_sounding_pcs(score, position);

            if (pcs.empty()) {
                current_idx = std::nullopt;
                prev_pcs.clear();
                continue;
            }

            // Force a new annotation when the key context changes, even
            // if the pitch class set is identical.
            bool key_changed = (key_root != prev_key_root);
            prev_key_root = key_root;

            if (pcs == prev_pcs && current_idx && !key_changed) {
                layer[*current_idx].duration = layer[*current_idx].duration + beat_dur;
                continue;
            }

            prev_pcs = pcs;

            HarmonicAnnotation ann;
            ann.position = position;
            ann.duration = beat_dur;
            ann.key_context = key;

            auto midi_notes = collect_sounding_midi(score, position);
            auto recognised = recognize_chord(pcs);

            if (recognised) {
                auto& [root, quality] = *recognised;
                ann.chord = build_voicing(root, quality, midi_notes);

                auto numeral = chord_to_numeral(
                    root, quality, key_root, scale_ints, minor);
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
    }

    return layer;
}

}  // anonymous namespace

// =============================================================================
// derive_harmonic_layer
// =============================================================================

Result<HarmonicAnnotationLayer> derive_harmonic_layer(const Score& score) {
    if (score.time_map.empty() || score.key_map.empty()) {
        return std::unexpected(ErrorCode::InvariantViolation);
    }

    auto layer = derive_for_range(score, 1, score.metadata.total_bars);

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

        if (at_boundary && !layer[i-1].chord.notes.empty() &&
            !layer[i].chord.notes.empty()) {
            KeySignature key = query_key_at(score, layer[i].position);
            PitchClass key_root = pc(key.root);
            bool minor = is_minor_key(key);

            auto cadence = detect_cadence(
                layer[i-1].chord, layer[i].chord, key_root, minor);
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

    // Remove annotations within stale regions
    std::erase_if(score.harmonic_annotations,
        [&](const HarmonicAnnotation& ann) {
            return in_stale_region(ann.position, score.stale_harmonic_regions);
        });

    // Re-derive for each stale region
    for (const auto& region : score.stale_harmonic_regions) {
        std::uint32_t start_bar = region.start.bar;
        std::uint32_t end_bar = region.end.bar;
        if (region.end.beat == Beat::zero() && end_bar > start_bar) {
            --end_bar;
        }

        auto new_annotations = derive_for_range(score, start_bar, end_bar);
        score.harmonic_annotations.insert(
            score.harmonic_annotations.end(),
            new_annotations.begin(),
            new_annotations.end());
    }

    // Sort by position
    std::sort(score.harmonic_annotations.begin(),
              score.harmonic_annotations.end(),
              [](const HarmonicAnnotation& a, const HarmonicAnnotation& b) {
                  return a.position < b.position;
              });

    score.stale_harmonic_regions.clear();
    return {};
}

// =============================================================================
// clear_stale_harmonic_regions
// =============================================================================

void clear_stale_harmonic_regions(Score& score) {
    score.stale_harmonic_regions.clear();
}

}  // namespace Sunny::Core
