/**
 * @file workflows.cpp
 * @brief Timbre IR workflow functions — implementation
 *
 *
 */

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <sunny/core/timbre/validation.hpp>
#include <sunny/core/timbre/workflows.hpp>

namespace sunny::core {

namespace {

// =============================================================================
// Path parsing utilities
// =============================================================================

using ParameterPath = std::vector<std::string>;

/// Parse the canonical parameter-path grammar without discarding any spelling.
///
/// path       := identifier (index)* ('.' identifier (index)*)*
/// identifier := [A-Za-z_][A-Za-z0-9_]*
/// index      := '[' ('0' | [1-9][0-9]*) ']'
///
/// "source.oscillators[0].tune_cents" →
/// ["source", "oscillators", "[0]", "tune_cents"]
std::optional<ParameterPath> split_path(const std::string& path) {
    if (path.empty()) return std::nullopt;

    std::vector<std::string> segments;
    std::size_t cursor = 0;
    while (cursor < path.size()) {
        const auto first = static_cast<unsigned char>(path[cursor]);
        if (!(std::isalpha(first) || path[cursor] == '_')) return std::nullopt;
        const auto identifier_start = cursor++;
        while (cursor < path.size()) {
            const auto value = static_cast<unsigned char>(path[cursor]);
            if (!(std::isalnum(value) || path[cursor] == '_')) break;
            ++cursor;
        }
        segments.emplace_back(path.substr(identifier_start, cursor - identifier_start));

        while (cursor < path.size() && path[cursor] == '[') {
            const auto index_start = cursor++;
            const auto digits_start = cursor;
            while (cursor < path.size() && std::isdigit(static_cast<unsigned char>(path[cursor])))
                ++cursor;
            if (digits_start == cursor || cursor >= path.size() || path[cursor] != ']')
                return std::nullopt;
            const auto digits = path.substr(digits_start, cursor - digits_start);
            if (digits.size() > 1 && digits.front() == '0') return std::nullopt;
            ++cursor;
            segments.emplace_back(path.substr(index_start, cursor - index_start));
        }

        if (cursor == path.size()) break;
        if (path[cursor] != '.') return std::nullopt;
        ++cursor;
        if (cursor == path.size()) return std::nullopt;
    }
    return segments;
}

/// Parse one already-tokenised canonical index without narrowing.
std::optional<std::size_t> parse_index(const std::string& segment) {
    if (segment.size() < 3 || segment.front() != '[' || segment.back() != ']') return std::nullopt;
    std::size_t index = 0;
    const auto* first = segment.data() + 1;
    const auto* last = segment.data() + segment.size() - 1;
    const auto [position, error] = std::from_chars(first, last, index);
    if (error != std::errc{} || position != last) return std::nullopt;
    return index;
}

ErrorCode invalid_path() {
    return ErrorCode::InvalidModTarget;
}

ErrorCode not_found() {
    return ErrorCode::TimbreNotFound;
}

ErrorCode duplicate_id() {
    return ErrorCode::TimbreDuplicateId;
}

bool exact_field(const ParameterPath& segments, std::size_t position, std::string_view field) {
    return position < segments.size() && position + 1 == segments.size() &&
           segments[position] == field;
}

std::optional<std::size_t>
bounded_index(const ParameterPath& segments, std::size_t position, std::size_t extent) {
    if (position >= segments.size()) return std::nullopt;
    auto index = parse_index(segments[position]);
    if (!index || *index >= extent) return std::nullopt;
    return index;
}

// =============================================================================
// Exact parameter-path resolvers
// =============================================================================

Result<float*> unresolved_parameter() {
    return std::unexpected(invalid_path());
}

Result<float*>
resolve_point(std::pair<float, float>& point, const ParameterPath& segments, std::size_t position) {
    if (exact_field(segments, position, "x")) return &point.first;
    if (exact_field(segments, position, "y")) return &point.second;
    return unresolved_parameter();
}

Result<float*> resolve_points(std::vector<std::pair<float, float>>& points,
                              const ParameterPath& segments,
                              std::size_t position) {
    auto index = bounded_index(segments, position, points.size());
    if (!index) return unresolved_parameter();
    return resolve_point(points[*index], segments, position + 1);
}

Result<float*>
resolve_waveform(Waveform& waveform, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "super_saw_detune")) return &waveform.super_saw_detune;
    if (segments[position] == "custom_harmonics") {
        auto index = bounded_index(segments, position + 1, waveform.custom_harmonics.size());
        if (!index) return unresolved_parameter();
        auto& harmonic = waveform.custom_harmonics[*index];
        if (exact_field(segments, position + 2, "amplitude")) return &harmonic.amplitude;
        if (exact_field(segments, position + 2, "phase")) return &harmonic.phase;
    }
    return unresolved_parameter();
}

Result<float*>
resolve_envelope(Envelope& envelope, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "velocity_sensitivity"))
        return &envelope.velocity_sensitivity;
    if (exact_field(segments, position, "key_tracking")) return &envelope.key_tracking;
    if (segments[position] == "stages") {
        auto index = bounded_index(segments, position + 1, envelope.stages.size());
        if (!index) return unresolved_parameter();
        auto& stage = envelope.stages[*index];
        if (exact_field(segments, position + 2, "duration")) return &stage.duration;
        if (exact_field(segments, position + 2, "target_level")) return &stage.target_level;
        if (exact_field(segments, position + 2, "curvature")) return &stage.curvature;
    }
    return unresolved_parameter();
}

Result<float*> resolve_filter(Filter& filter, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "cutoff")) return &filter.cutoff;
    if (exact_field(segments, position, "resonance")) return &filter.resonance;
    if (exact_field(segments, position, "drive")) return &filter.drive;
    if (exact_field(segments, position, "key_tracking")) return &filter.key_tracking;
    if (exact_field(segments, position, "envelope_depth") ||
        exact_field(segments, position, "env_depth"))
        return &filter.envelope_depth;
    if (segments[position] == "config") {
        if (exact_field(segments, position + 1, "bandwidth")) return &filter.config.bandwidth;
        if (exact_field(segments, position + 1, "comb_feedback"))
            return &filter.config.comb_feedback;
    }
    if (segments[position] == "envelope" && filter.envelope)
        return resolve_envelope(*filter.envelope, segments, position + 1);
    return unresolved_parameter();
}

Result<float*>
resolve_subtractive(SubtractiveSynth& synth, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    const auto& segment = segments[position];
    if (segment == "filter") return resolve_filter(synth.filter, segments, position + 1);
    if (segment == "filter_2" && synth.filter_2)
        return resolve_filter(*synth.filter_2, segments, position + 1);
    if (segment == "oscillators") {
        auto index = bounded_index(segments, position + 1, synth.oscillators.size());
        if (!index) return unresolved_parameter();
        auto& oscillator = synth.oscillators[*index];
        if (exact_field(segments, position + 2, "tune_cents")) return &oscillator.tune_cents;
        if (exact_field(segments, position + 2, "phase")) return &oscillator.phase;
        if (exact_field(segments, position + 2, "pulse_width")) return &oscillator.pulse_width;
        if (exact_field(segments, position + 2, "level")) return &oscillator.level;
        if (position + 2 < segments.size() && segments[position + 2] == "waveform")
            return resolve_waveform(oscillator.waveform, segments, position + 3);
    }
    if (segment == "oscillator_mix") {
        auto index = bounded_index(segments, position + 1, synth.oscillator_mix.size());
        if (index && position + 2 == segments.size()) return &synth.oscillator_mix[*index];
    }
    if (segment == "amplifier") return resolve_envelope(synth.amplifier, segments, position + 1);
    if (segment == "noise" && synth.noise && exact_field(segments, position + 1, "level"))
        return &synth.noise->level;
    if (segment == "unison" && synth.unison) {
        if (exact_field(segments, position + 1, "detune")) return &synth.unison->detune;
        if (exact_field(segments, position + 1, "stereo_spread"))
            return &synth.unison->stereo_spread;
        if (exact_field(segments, position + 1, "blend")) return &synth.unison->blend;
    }
    if (segment == "portamento" && synth.portamento && exact_field(segments, position + 1, "time"))
        return &synth.portamento->time;
    return unresolved_parameter();
}

Result<float*> resolve_fm(FMSynth& synth, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "feedback")) return &synth.feedback;
    if (segments[position] == "operators") {
        auto index = bounded_index(segments, position + 1, synth.operators.size());
        if (!index) return unresolved_parameter();
        auto& op = synth.operators[*index];
        if (exact_field(segments, position + 2, "ratio")) return &op.ratio;
        if (exact_field(segments, position + 2, "fixed_frequency") && op.fixed_frequency)
            return &*op.fixed_frequency;
        if (exact_field(segments, position + 2, "level")) return &op.level;
        if (exact_field(segments, position + 2, "detune")) return &op.detune;
        if (position + 2 < segments.size() && segments[position + 2] == "envelope")
            return resolve_envelope(op.envelope, segments, position + 3);
        if (position + 2 < segments.size() && segments[position + 2] == "waveform")
            return resolve_waveform(op.waveform, segments, position + 3);
    }
    if (segments[position] == "algorithm" && position + 1 < segments.size() &&
        segments[position + 1] == "custom_routing") {
        auto index = bounded_index(segments, position + 2, synth.algorithm.custom_routing.size());
        if (index && exact_field(segments, position + 3, "depth"))
            return &synth.algorithm.custom_routing[*index].depth;
    }
    return unresolved_parameter();
}

Result<float*>
resolve_wavetable(WavetableSynth& synth, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "position")) return &synth.position;
    if (segments[position] == "filter" && synth.filter)
        return resolve_filter(*synth.filter, segments, position + 1);
    if (segments[position] == "amplifier")
        return resolve_envelope(synth.amplifier, segments, position + 1);
    return unresolved_parameter();
}

Result<float*>
resolve_granular(GranularSynth& synth, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "grain_size")) return &synth.grain_size;
    if (exact_field(segments, position, "grain_density")) return &synth.grain_density;
    if (exact_field(segments, position, "position")) return &synth.position;
    if (exact_field(segments, position, "position_random")) return &synth.position_random;
    if (exact_field(segments, position, "pitch_random")) return &synth.pitch_random;
    if (exact_field(segments, position, "spray")) return &synth.spray;
    if (exact_field(segments, position, "stereo_spread")) return &synth.stereo_spread;
    if (exact_field(segments, position, "reverse_probability")) return &synth.reverse_probability;
    if (segments[position] == "grain_envelope") {
        if (exact_field(segments, position + 1, "trapezoid_attack_ratio"))
            return &synth.grain_envelope.trapezoid_attack_ratio;
        if (exact_field(segments, position + 1, "trapezoid_release_ratio"))
            return &synth.grain_envelope.trapezoid_release_ratio;
    }
    return unresolved_parameter();
}

Result<float*>
resolve_additive(AdditiveSynth& synth, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (segments[position] == "partials") {
        auto index = bounded_index(segments, position + 1, synth.partials.size());
        if (!index) return unresolved_parameter();
        auto& partial = synth.partials[*index];
        if (exact_field(segments, position + 2, "ratio")) return &partial.ratio;
        if (exact_field(segments, position + 2, "amplitude")) return &partial.amplitude;
        if (exact_field(segments, position + 2, "phase")) return &partial.phase;
        if (exact_field(segments, position + 2, "detune")) return &partial.detune;
        if (position + 2 < segments.size() && segments[position + 2] == "envelope" &&
            partial.envelope)
            return resolve_envelope(*partial.envelope, segments, position + 3);
    }
    if (segments[position] == "global_envelope")
        return resolve_envelope(synth.global_envelope, segments, position + 1);
    return unresolved_parameter();
}

Result<float*>
resolve_physical(PhysicalModelSource& source, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "coupling")) return &source.coupling;
    if (exact_field(segments, position, "damping")) return &source.damping;
    if (exact_field(segments, position, "brightness")) return &source.brightness;
    if (segments[position] == "exciter") {
        if (exact_field(segments, position + 1, "amplitude")) return &source.exciter.amplitude;
        if (exact_field(segments, position + 1, "brightness")) return &source.exciter.brightness;
        if (exact_field(segments, position + 1, "noise_mix")) return &source.exciter.noise_mix;
    }
    if (segments[position] == "resonator") {
        if (exact_field(segments, position + 1, "frequency_ratio"))
            return &source.resonator.frequency_ratio;
        if (exact_field(segments, position + 1, "decay")) return &source.resonator.decay;
        if (exact_field(segments, position + 1, "inharm")) return &source.resonator.inharm;
    }
    if (segments[position] == "model") {
        if (exact_field(segments, position + 1, "reed_stiffness"))
            return &source.model.reed_stiffness;
        if (exact_field(segments, position + 1, "lip_mass")) return &source.model.lip_mass;
        if (exact_field(segments, position + 1, "bow_pressure")) return &source.model.bow_pressure;
        if (exact_field(segments, position + 1, "bow_speed")) return &source.model.bow_speed;
    }
    return unresolved_parameter();
}

Result<float*>
resolve_sampler(SamplerSource& source, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (exact_field(segments, position, "tuning_offset")) return &source.tuning_offset;
    if (segments[position] == "filter" && source.filter)
        return resolve_filter(*source.filter, segments, position + 1);
    if (segments[position] == "envelope_override" && source.envelope_override)
        return resolve_envelope(*source.envelope_override, segments, position + 1);
    if (segments[position] == "microphone_positions") {
        auto index = bounded_index(segments, position + 1, source.microphone_positions.size());
        if (index && exact_field(segments, position + 2, "level"))
            return &source.microphone_positions[*index].level;
    }
    return unresolved_parameter();
}

Result<float*>
resolve_source(SoundSourceData& source, const ParameterPath& segments, std::size_t position);

Result<float*>
resolve_hybrid(HybridSource& source, const ParameterPath& segments, std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    if (segments[position] == "layers") {
        auto index = bounded_index(segments, position + 1, source.layers.size());
        if (!index || !source.layers[*index]) return unresolved_parameter();
        return resolve_source(*source.layers[*index], segments, position + 2);
    }
    if (segments[position] == "routing" && position + 1 < segments.size() &&
        segments[position + 1] == "mix_levels") {
        auto index = bounded_index(segments, position + 2, source.routing.mix_levels.size());
        if (index && position + 3 == segments.size()) return &source.routing.mix_levels[*index];
    }
    return unresolved_parameter();
}

Result<float*>
resolve_source(SoundSourceData& source, const ParameterPath& segments, std::size_t position) {
    return std::visit(
        [&](auto& value) -> Result<float*> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, SubtractiveSynth>)
                return resolve_subtractive(value, segments, position);
            else if constexpr (std::is_same_v<T, FMSynth>)
                return resolve_fm(value, segments, position);
            else if constexpr (std::is_same_v<T, WavetableSynth>)
                return resolve_wavetable(value, segments, position);
            else if constexpr (std::is_same_v<T, GranularSynth>)
                return resolve_granular(value, segments, position);
            else if constexpr (std::is_same_v<T, AdditiveSynth>)
                return resolve_additive(value, segments, position);
            else if constexpr (std::is_same_v<T, PhysicalModelSource>)
                return resolve_physical(value, segments, position);
            else if constexpr (std::is_same_v<T, SamplerSource>)
                return resolve_sampler(value, segments, position);
            else if constexpr (std::is_same_v<T, HybridSource>)
                return resolve_hybrid(value, segments, position);
            else
                return unresolved_parameter();
        },
        source.data);
}

Result<float*> resolve_effect_params(EffectParameters& parameters,
                                     const ParameterPath& segments,
                                     std::size_t position) {
    if (position >= segments.size()) return unresolved_parameter();
    return std::visit(
        [&](auto& effect) -> Result<float*> {
            using T = std::decay_t<decltype(effect)>;
            if constexpr (std::is_same_v<T, DistortionEffect>) {
                if (exact_field(segments, position, "drive")) return &effect.drive;
                if (exact_field(segments, position, "tone")) return &effect.tone;
                if (exact_field(segments, position, "output_level") ||
                    exact_field(segments, position, "output"))
                    return &effect.output_level;
                if (segments[position] == "algorithm") {
                    if (exact_field(segments, position + 1, "sample_rate_reduction"))
                        return &effect.algorithm.sample_rate_reduction;
                    if (exact_field(segments, position + 1, "ring_mod_frequency"))
                        return &effect.algorithm.ring_mod_frequency;
                }
            } else if constexpr (std::is_same_v<T, DelayEffect>) {
                if (segments[position] == "delay_time" && exact_field(segments, position + 1, "ms"))
                    return &effect.delay_time.ms;
                if (exact_field(segments, position, "feedback")) return &effect.feedback;
                if (exact_field(segments, position, "stereo_offset")) return &effect.stereo_offset;
                if (segments[position] == "filter" && effect.filter)
                    return resolve_filter(*effect.filter, segments, position + 1);
                if (exact_field(segments, position, "modulation_rate") ||
                    exact_field(segments, position, "mod_rate"))
                    return &effect.modulation_rate;
                if (exact_field(segments, position, "modulation_depth") ||
                    exact_field(segments, position, "mod_depth"))
                    return &effect.modulation_depth;
            } else if constexpr (std::is_same_v<T, ReverbEffect>) {
                if (segments[position] == "algorithm" &&
                    exact_field(segments, position + 1, "shimmer_pitch"))
                    return &effect.algorithm.shimmer_pitch;
                if (exact_field(segments, position, "decay_time") ||
                    exact_field(segments, position, "decay"))
                    return &effect.decay_time;
                if (exact_field(segments, position, "pre_delay")) return &effect.pre_delay;
                if (exact_field(segments, position, "damping")) return &effect.damping;
                if (exact_field(segments, position, "diffusion")) return &effect.diffusion;
                if (exact_field(segments, position, "size")) return &effect.size;
                if (exact_field(segments, position, "early_reflections_level"))
                    return &effect.early_reflections_level;
                if (exact_field(segments, position, "eq_low_cut")) return &effect.eq_low_cut;
                if (exact_field(segments, position, "eq_high_cut")) return &effect.eq_high_cut;
            } else if constexpr (std::is_same_v<T, ChorusEffect>) {
                if (exact_field(segments, position, "rate")) return &effect.rate;
                if (exact_field(segments, position, "depth")) return &effect.depth;
                if (exact_field(segments, position, "feedback")) return &effect.feedback;
                if (exact_field(segments, position, "stereo_spread") ||
                    exact_field(segments, position, "spread"))
                    return &effect.stereo_spread;
            } else if constexpr (std::is_same_v<T, PhaserEffect>) {
                if (exact_field(segments, position, "rate")) return &effect.rate;
                if (exact_field(segments, position, "depth")) return &effect.depth;
                if (exact_field(segments, position, "feedback")) return &effect.feedback;
                if (exact_field(segments, position, "center_frequency") ||
                    exact_field(segments, position, "center"))
                    return &effect.center_frequency;
            } else if constexpr (std::is_same_v<T, FlangerEffect>) {
                if (exact_field(segments, position, "rate")) return &effect.rate;
                if (exact_field(segments, position, "depth")) return &effect.depth;
                if (exact_field(segments, position, "feedback")) return &effect.feedback;
                if (exact_field(segments, position, "manual")) return &effect.manual;
            } else if constexpr (std::is_same_v<T, EQEffect>) {
                if (segments[position] == "bands") {
                    auto index = bounded_index(segments, position + 1, effect.bands.size());
                    if (!index) return unresolved_parameter();
                    auto& band = effect.bands[*index];
                    if (exact_field(segments, position + 2, "frequency")) return &band.frequency;
                    if (exact_field(segments, position + 2, "gain")) return &band.gain;
                    if (exact_field(segments, position + 2, "q")) return &band.q;
                }
            } else if constexpr (std::is_same_v<T, CompressorEffect>) {
                if (exact_field(segments, position, "threshold")) return &effect.threshold;
                if (exact_field(segments, position, "ratio")) return &effect.ratio;
                if (exact_field(segments, position, "attack")) return &effect.attack;
                if (exact_field(segments, position, "release")) return &effect.release;
                if (exact_field(segments, position, "knee")) return &effect.knee;
                if (exact_field(segments, position, "makeup_gain") ||
                    exact_field(segments, position, "makeup"))
                    return &effect.makeup_gain;
                if (segments[position] == "sidechain" && effect.sidechain &&
                    position + 1 < segments.size() && segments[position + 1] == "filter" &&
                    effect.sidechain->filter)
                    return resolve_filter(*effect.sidechain->filter, segments, position + 2);
            }
            return unresolved_parameter();
        },
        parameters);
}

Result<float*> resolve_path(TimbreProfile& profile, const ParameterPath& segments) {
    if (segments.empty()) return unresolved_parameter();
    const auto& root = segments[0];
    if (root == "source") return resolve_source(profile.source, segments, 1);

    if (root == "insert_chain" && segments.size() >= 4 && segments[1] == "effects") {
        auto index = bounded_index(segments, 2, profile.insert_chain.effects.size());
        if (!index) return unresolved_parameter();
        auto& effect = profile.insert_chain.effects[*index];
        if (exact_field(segments, 3, "mix")) return &effect.mix;
        return resolve_effect_params(effect.parameters, segments, 3);
    }

    if (root == "modulation" && segments.size() >= 4) {
        if (segments[1] == "lfos") {
            auto index = bounded_index(segments, 2, profile.modulation.lfos.size());
            if (!index) return unresolved_parameter();
            auto& lfo = profile.modulation.lfos[*index];
            if (exact_field(segments, 3, "phase")) return &lfo.phase;
            if (exact_field(segments, 3, "symmetry")) return &lfo.symmetry;
            if (exact_field(segments, 3, "fade_in")) return &lfo.fade_in;
            if (exact_field(segments, 3, "delay")) return &lfo.delay;
            if (segments[3] == "rate" && exact_field(segments, 4, "hz")) return &lfo.rate.hz;
            if (segments[3] == "custom_points")
                return resolve_points(lfo.custom_points, segments, 4);
        }
        if (segments[1] == "envelopes") {
            auto index = bounded_index(segments, 2, profile.modulation.envelopes.size());
            if (!index) return unresolved_parameter();
            return resolve_envelope(profile.modulation.envelopes[*index], segments, 3);
        }
        if (segments[1] == "step_sequencers") {
            auto index = bounded_index(segments, 2, profile.modulation.step_sequencers.size());
            if (!index) return unresolved_parameter();
            auto& sequencer = profile.modulation.step_sequencers[*index];
            if (exact_field(segments, 3, "smooth")) return &sequencer.smooth;
            if (segments[3] == "steps") {
                auto step = bounded_index(segments, 4, sequencer.steps.size());
                if (step && segments.size() == 5) return &sequencer.steps[*step];
            }
        }
        if (segments[1] == "macro_knobs") {
            auto index = bounded_index(segments, 2, profile.modulation.macro_knobs.size());
            if (!index) return unresolved_parameter();
            auto& macro = profile.modulation.macro_knobs[*index];
            if (exact_field(segments, 3, "value")) return &macro.value;
            if (segments[3] == "mappings") {
                auto mapping_index = bounded_index(segments, 4, macro.mappings.size());
                if (!mapping_index) return unresolved_parameter();
                auto& mapping = macro.mappings[*mapping_index];
                if (exact_field(segments, 5, "min")) return &mapping.min;
                if (exact_field(segments, 5, "max")) return &mapping.max;
                if (segments.size() > 5 && segments[5] == "curve" && segments.size() > 6 &&
                    segments[6] == "custom_points")
                    return resolve_points(mapping.curve.custom_points, segments, 7);
            }
        }
    }

    if (root == "semantic_descriptors" && segments.size() == 2) {
        auto& descriptor = profile.semantic_descriptors;
        if (segments[1] == "brightness") return &descriptor.brightness;
        if (segments[1] == "warmth") return &descriptor.warmth;
        if (segments[1] == "roughness") return &descriptor.roughness;
        if (segments[1] == "width") return &descriptor.width;
        if (segments[1] == "density") return &descriptor.density;
        if (segments[1] == "movement") return &descriptor.movement;
        if (segments[1] == "weight") return &descriptor.weight;
    }
    return unresolved_parameter();
}

// resolve_path returns float* for mutation; this wrapper provides read access.
// The const_cast is safe because resolve_path only computes offsets into the
// profile struct without writing, and the returned pointer is immediately
// re-qualified as const. The mutable version exists because workflow mutations
// (set_parameter) require a writable pointer through the same path logic.
Result<const float*> resolve_path_const(const TimbreProfile& profile,
                                        const std::vector<std::string>& segs) {
    auto result = resolve_path(const_cast<TimbreProfile&>(profile), segs);
    if (!result) return std::unexpected(result.error());
    return static_cast<const float*>(*result);
}

// =============================================================================
// Heuristic timbre analysis
// =============================================================================

float clamp01(float v) {
    return std::min(1.0f, std::max(0.0f, v));
}

SemanticTimbreDescriptor analyze_subtractive(const SubtractiveSynth& sub) {
    SemanticTimbreDescriptor d;

    // Brightness: higher cutoff → brighter, higher resonance adds edge
    float cutoff_norm = clamp01(sub.filter.cutoff / 20000.0f);
    d.brightness = cutoff_norm * 0.7f + sub.filter.resonance * 0.3f;

    // Warmth: inversely related to brightness; low-frequency content
    d.warmth = clamp01(1.0f - d.brightness * 0.6f);
    bool has_sub_osc = false;
    for (const auto& osc : sub.oscillators) {
        if (osc.tune_semitones <= -12) has_sub_osc = true;
    }
    if (has_sub_osc) d.warmth = clamp01(d.warmth + 0.2f);

    // Roughness: detuning between oscillators
    float max_detune = 0.0f;
    for (const auto& osc : sub.oscillators)
        max_detune = std::max(max_detune, std::abs(osc.tune_cents));
    d.roughness = clamp01(max_detune / 100.0f);

    // Attack character from amplifier envelope
    if (!sub.amplifier.stages.empty()) {
        float attack_ms = sub.amplifier.stages[0].duration;
        if (attack_ms < 5.0f)
            d.attack_character = AttackCharacter::Percussive;
        else if (attack_ms < 30.0f)
            d.attack_character = AttackCharacter::Plucked;
        else if (attack_ms < 100.0f)
            d.attack_character = AttackCharacter::Gradual;
        else
            d.attack_character = AttackCharacter::Swelling;
    }

    // Sustain character from envelope shape
    if (sub.amplifier.stages.size() >= 2) {
        float sustain_level = sub.amplifier.stages[1].target_level;
        d.sustain_character =
            sustain_level > 0.9f ? SustainCharacter::Steady : SustainCharacter::Decaying;
    }

    // Width: unison spread
    if (sub.unison) {
        d.width = clamp01(sub.unison->stereo_spread);
    }

    // Density: number of oscillators + unison voices
    float osc_count = static_cast<float>(sub.oscillators.size());
    float uni_count = sub.unison ? static_cast<float>(sub.unison->voice_count) : 1.0f;
    d.density = clamp01((osc_count * uni_count - 1.0f) / 15.0f);

    d.weight = clamp01(d.warmth * 0.5f + (has_sub_osc ? 0.3f : 0.0f));
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

SemanticTimbreDescriptor analyze_fm(const FMSynth& fm) {
    SemanticTimbreDescriptor d;
    d.brightness = clamp01(0.4f + fm.feedback * 0.6f);
    d.warmth = clamp01(1.0f - d.brightness * 0.5f);
    d.roughness = clamp01(fm.feedback * 0.8f);
    d.density = clamp01(static_cast<float>(fm.operators.size()) / 6.0f);
    d.attack_character = AttackCharacter::Percussive;
    d.sustain_character = SustainCharacter::Evolving;
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

SemanticTimbreDescriptor analyze_granular(const GranularSynth& gr) {
    SemanticTimbreDescriptor d;
    d.brightness = 0.5f;
    d.warmth = 0.5f;
    d.roughness = clamp01(gr.pitch_random / 100.0f + gr.position_random * 0.3f);
    d.density = clamp01(gr.grain_density / 60.0f);
    d.movement = clamp01(gr.position_random + gr.spray);
    d.width = clamp01(gr.stereo_spread);
    d.attack_character = AttackCharacter::Gradual;
    d.sustain_character = SustainCharacter::Evolving;
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

SemanticTimbreDescriptor analyze_additive(const AdditiveSynth& add) {
    SemanticTimbreDescriptor d;
    float high_content = 0.0f;
    float total_amp = 0.0f;
    for (const auto& p : add.partials) {
        total_amp += p.amplitude;
        if (p.ratio > 4.0f) high_content += p.amplitude;
    }
    d.brightness = total_amp > 0.0f ? clamp01(high_content / total_amp) : 0.5f;
    d.warmth = clamp01(1.0f - d.brightness * 0.5f);
    d.density = clamp01(static_cast<float>(add.partials.size()) / 32.0f);
    d.attack_character = AttackCharacter::Gradual;
    d.sustain_character = SustainCharacter::Steady;
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

SemanticTimbreDescriptor analyze_physical(const PhysicalModelSource& pm) {
    SemanticTimbreDescriptor d;
    d.brightness = pm.brightness;
    d.warmth = clamp01(1.0f - pm.brightness * 0.5f);
    d.roughness = clamp01(pm.resonator.inharm * 2.0f);
    d.weight = clamp01(pm.damping);

    switch (pm.exciter.type) {
    case ExciterType::Strike:
        d.attack_character = AttackCharacter::Percussive;
        break;
    case ExciterType::Bow:
        d.attack_character = AttackCharacter::Bowed;
        break;
    case ExciterType::Blow:
        d.attack_character = AttackCharacter::Blown;
        break;
    case ExciterType::Impulse:
        d.attack_character = AttackCharacter::Plucked;
        break;
    default:
        d.attack_character = AttackCharacter::Gradual;
        break;
    }
    d.sustain_character =
        (pm.exciter.type == ExciterType::Bow || pm.exciter.type == ExciterType::Blow)
            ? SustainCharacter::Steady
            : SustainCharacter::Decaying;
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

// =============================================================================
// Preset key parameter extraction
// =============================================================================

void extract_subtractive_params(const SubtractiveSynth& sub, std::map<std::string, float>& params) {
    params["source.filter.cutoff"] = sub.filter.cutoff;
    params["source.filter.resonance"] = sub.filter.resonance;
    params["source.filter.drive"] = sub.filter.drive;
    params["source.filter.env_depth"] = sub.filter.envelope_depth;
    for (std::size_t i = 0; i < sub.oscillators.size(); ++i) {
        std::string prefix = "source.oscillators[" + std::to_string(i) + "].";
        params[prefix + "tune_cents"] = sub.oscillators[i].tune_cents;
        params[prefix + "level"] = sub.oscillators[i].level;
        params[prefix + "pulse_width"] = sub.oscillators[i].pulse_width;
    }
    params["source.amplifier.velocity_sensitivity"] = sub.amplifier.velocity_sensitivity;
}

void extract_fm_params(const FMSynth& fm, std::map<std::string, float>& params) {
    params["source.feedback"] = fm.feedback;
    for (std::size_t i = 0; i < fm.operators.size(); ++i) {
        std::string prefix = "source.operators[" + std::to_string(i) + "].";
        params[prefix + "ratio"] = fm.operators[i].ratio;
        params[prefix + "level"] = fm.operators[i].level;
        params[prefix + "detune"] = fm.operators[i].detune;
    }
}

void extract_wavetable_params(const WavetableSynth& wt, std::map<std::string, float>& params) {
    params["source.position"] = wt.position;
    if (wt.filter) {
        params["source.filter.cutoff"] = wt.filter->cutoff;
        params["source.filter.resonance"] = wt.filter->resonance;
    }
    params["source.amplifier.velocity_sensitivity"] = wt.amplifier.velocity_sensitivity;
    params["source.amplifier.key_tracking"] = wt.amplifier.key_tracking;
}

void extract_granular_params(const GranularSynth& gr, std::map<std::string, float>& params) {
    params["source.grain_size"] = gr.grain_size;
    params["source.grain_density"] = gr.grain_density;
    params["source.position"] = gr.position;
    params["source.position_random"] = gr.position_random;
    params["source.pitch_random"] = gr.pitch_random;
    params["source.spray"] = gr.spray;
    params["source.stereo_spread"] = gr.stereo_spread;
    params["source.reverse_probability"] = gr.reverse_probability;
}

void extract_additive_params(const AdditiveSynth& add, std::map<std::string, float>& params) {
    for (std::size_t i = 0; i < add.partials.size(); ++i) {
        std::string prefix = "source.partials[" + std::to_string(i) + "].";
        params[prefix + "ratio"] = add.partials[i].ratio;
        params[prefix + "amplitude"] = add.partials[i].amplitude;
        params[prefix + "phase"] = add.partials[i].phase;
        params[prefix + "detune"] = add.partials[i].detune;
    }
    params["source.global_envelope.velocity_sensitivity"] =
        add.global_envelope.velocity_sensitivity;
    params["source.global_envelope.key_tracking"] = add.global_envelope.key_tracking;
}

void extract_physical_params(const PhysicalModelSource& pm, std::map<std::string, float>& params) {
    params["source.coupling"] = pm.coupling;
    params["source.damping"] = pm.damping;
    params["source.brightness"] = pm.brightness;
    params["source.exciter.amplitude"] = pm.exciter.amplitude;
    params["source.exciter.brightness"] = pm.exciter.brightness;
    params["source.exciter.noise_mix"] = pm.exciter.noise_mix;
    params["source.resonator.frequency_ratio"] = pm.resonator.frequency_ratio;
    params["source.resonator.decay"] = pm.resonator.decay;
    params["source.resonator.inharm"] = pm.resonator.inharm;
    params["source.model.reed_stiffness"] = pm.model.reed_stiffness;
    params["source.model.lip_mass"] = pm.model.lip_mass;
    params["source.model.bow_pressure"] = pm.model.bow_pressure;
    params["source.model.bow_speed"] = pm.model.bow_speed;
}

void extract_sampler_params(const SamplerSource& sam, std::map<std::string, float>& params) {
    params["source.tuning_offset"] = sam.tuning_offset;
    if (sam.filter) {
        params["source.filter.cutoff"] = sam.filter->cutoff;
        params["source.filter.resonance"] = sam.filter->resonance;
    }
}

} // anonymous namespace

// =============================================================================
// Profile Creation
// =============================================================================

TimbreProfile create_timbre_profile(TimbreProfileId id, PartId part_id, const std::string& name) {
    TimbreProfile p;
    p.id = id;
    p.part_id = part_id;
    p.name = name;

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}}; // Default sine oscillator
    sub.amplifier.stages = {{5.0f, 1.0f, EnvelopeCurve::Linear},
                            {100.0f, 0.7f, EnvelopeCurve::Exponential},
                            {200.0f, 0.0f, EnvelopeCurve::Exponential}};
    p.source.data = std::move(sub);

    return p;
}

// =============================================================================
// Source Configuration
// =============================================================================

Result<void> set_sound_source(TimbreProfile& profile, SoundSourceData source) {
    SoundSourceData previous = std::move(profile.source);
    profile.source = std::move(source);

    auto diags = validate_timbre(profile);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) {
            profile.source = std::move(previous);
            return std::unexpected(ErrorCode::TimbreInvalidParameter);
        }
    }
    return {};
}

// =============================================================================
// Effect Chain
// =============================================================================

Result<void> add_effect(TimbreProfile& profile, Effect effect) {
    profile.insert_chain.effects.push_back(std::move(effect));
    return {};
}

namespace {

/// Position of each effect's device among the chain's enabled effects, if it has one.
///
/// The compiler inserts the source at device 0 and each enabled effect after
/// it in chain order, so rendering-map device indices address effects by this
/// ordinal rather than by chain position.
std::vector<std::optional<std::uint32_t>> effect_devices(const std::vector<Effect>& effects) {
    std::vector<std::optional<std::uint32_t>> devices(effects.size());
    std::uint32_t next = 1;
    for (std::size_t i = 0; i < effects.size(); ++i)
        if (effects[i].enabled) devices[i] = next++;
    return devices;
}

/// Rewrite an insert-chain path through old→new effect positions.
///
/// Returns the path unchanged when it does not address an effect position,
/// and nullopt when it addresses an effect that no longer exists.
std::optional<std::string>
relocate_effect_path(const std::string& path,
                     const std::vector<std::optional<std::size_t>>& relocation) {
    auto segments = split_path(path);
    if (!segments || segments->size() < 3 || (*segments)[0] != "insert_chain" ||
        (*segments)[1] != "effects")
        return path;
    const auto index = parse_index((*segments)[2]);
    // An out-of-range reference was already unresolved; it stays for validation to report.
    if (!index || *index >= relocation.size()) return path;
    if (!relocation[*index]) return std::nullopt;
    (*segments)[2] = "[" + std::to_string(*relocation[*index]) + "]";
    std::string rebuilt;
    for (const auto& segment : *segments) {
        if (!rebuilt.empty() && segment.front() != '[') rebuilt += '.';
        rebuilt += segment;
    }
    return rebuilt;
}

/**
 * Carry every reference to an effect through a change of the effect chain.
 *
 * Modulation targets, macro targets, automation lanes and rendering-map keys
 * address effects by chain position, and rendering-map device indices by
 * enabled-effect ordinal. Both are rewritten from `before` to `after`
 * (matched by EffectId) so a reference keeps naming the same effect. Returns
 * false, leaving the references partially rewritten, when one names an effect
 * absent from `after`; callers restore from an EffectReferenceState.
 */
bool relocate_effect_references(TimbreProfile& profile,
                                const std::vector<Effect>& before,
                                const std::vector<Effect>& after) {
    std::vector<std::optional<std::size_t>> relocation(before.size());
    std::map<std::uint32_t, std::optional<std::uint32_t>> device_relocation;
    const auto old_devices = effect_devices(before);
    const auto new_devices = effect_devices(after);
    for (std::size_t i = 0; i < before.size(); ++i) {
        const auto found = std::ranges::find(
            after, before[i].id.value, [](const Effect& e) { return e.id.value; });
        if (found != after.end()) {
            const auto position = static_cast<std::size_t>(std::distance(after.begin(), found));
            relocation[i] = position;
            if (old_devices[i]) device_relocation[*old_devices[i]] = new_devices[position];
        } else if (old_devices[i]) {
            device_relocation[*old_devices[i]] = std::nullopt;
        }
    }

    const auto relocate = [&](std::string& path) {
        auto moved = relocate_effect_path(path, relocation);
        if (!moved) return false;
        path = std::move(*moved);
        return true;
    };
    for (auto& routing : profile.modulation.routings)
        if (!relocate(routing.target)) return false;
    for (auto& macro : profile.modulation.macro_knobs)
        for (auto& mapping : macro.mappings)
            if (!relocate(mapping.target)) return false;
    for (auto& automation : profile.parameter_automation)
        if (!relocate(automation.parameter_path)) return false;

    std::map<std::string, DeviceParameter> rendering;
    for (const auto& [key, value] : profile.rendering.parameter_map) {
        std::string path = key;
        DeviceParameter mapping = value;
        if (!relocate(path)) return false;
        if (const auto device = device_relocation.find(mapping.device_index);
            device != device_relocation.end()) {
            if (!device->second) return false;
            mapping.device_index = *device->second;
        }
        rendering.emplace(std::move(path), std::move(mapping));
    }
    profile.rendering.parameter_map = std::move(rendering);
    return true;
}

/// Copyable snapshot of every part of a profile that an effect-chain edit touches.
///
/// TimbreProfile itself is move-only (Hybrid sources own their layers), so
/// edits snapshot these parts and restore them on failure instead of
/// working on a whole-profile copy.
struct EffectReferenceState {
    std::vector<Effect> effects;
    ModulationMatrix modulation;
    std::vector<TimbreAutomation> automation;
    std::map<std::string, DeviceParameter> rendering;

    explicit EffectReferenceState(const TimbreProfile& profile)
        : effects(profile.insert_chain.effects), modulation(profile.modulation),
          automation(profile.parameter_automation), rendering(profile.rendering.parameter_map) {}

    void restore(TimbreProfile& profile) {
        profile.insert_chain.effects = std::move(effects);
        profile.modulation = std::move(modulation);
        profile.parameter_automation = std::move(automation);
        profile.rendering.parameter_map = std::move(rendering);
    }
};

bool has_error(const std::vector<Diagnostic>& diagnostics) {
    return std::ranges::any_of(diagnostics, [](const Diagnostic& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

} // namespace

Result<void> remove_effect(TimbreProfile& profile, EffectId effect_id) {
    const auto& effects = profile.insert_chain.effects;
    auto it = std::find_if(effects.begin(), effects.end(), [&](const Effect& e) {
        return e.id.value == effect_id.value;
    });
    if (it == effects.end()) return std::unexpected(not_found());

    const bool valid_before = !has_error(validate_timbre(profile));
    EffectReferenceState previous(profile);
    auto remaining = effects;
    remaining.erase(remaining.begin() + std::distance(effects.begin(), it));
    // A reference to the removed effect cannot be retargeted without changing
    // its meaning, so the removal is refused and the profile left unchanged.
    if (!relocate_effect_references(profile, previous.effects, remaining)) {
        previous.restore(profile);
        return std::unexpected(ErrorCode::InvalidModTarget);
    }
    profile.insert_chain.effects = std::move(remaining);
    if (valid_before && has_error(validate_timbre(profile))) {
        previous.restore(profile);
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    }
    return {};
}

Result<void> reorder_effects(TimbreProfile& profile, const std::vector<EffectId>& new_order) {
    const auto& effects = profile.insert_chain.effects;
    if (new_order.size() != effects.size()) return std::unexpected(not_found());

    // TI-3: reject duplicate IDs in the reorder list
    for (std::size_t i = 0; i < new_order.size(); ++i) {
        for (std::size_t j = i + 1; j < new_order.size(); ++j) {
            if (new_order[i].value == new_order[j].value) return std::unexpected(duplicate_id());
        }
        if (std::ranges::find(effects, new_order[i], &Effect::id) == effects.end())
            return std::unexpected(not_found());
    }

    std::vector<Effect> reordered;
    reordered.reserve(effects.size());
    for (const auto& id : new_order) {
        auto it = std::find_if(effects.begin(), effects.end(), [&](const Effect& e) {
            return e.id.value == id.value;
        });
        reordered.push_back(*it);
    }
    EffectReferenceState previous(profile);
    // A permutation keeps every effect, so relocation cannot find a dangling reference.
    if (!relocate_effect_references(profile, previous.effects, reordered)) {
        previous.restore(profile);
        return std::unexpected(ErrorCode::InvariantViolation);
    }
    profile.insert_chain.effects = std::move(reordered);
    return {};
}

// =============================================================================
// Parameter Access
// =============================================================================

Result<void> set_parameter(TimbreProfile& profile, const std::string& path, float value) {
    auto segs = split_path(path);
    if (!segs) return std::unexpected(invalid_path());
    auto result = resolve_path(profile, *segs);
    if (!result) return std::unexpected(result.error());

    // Store previous value for rollback if validation fails
    float previous = **result;
    **result = value;

    // Post-mutation validation: reject if any Error-level diagnostic appears
    auto diags = validate_timbre(profile);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) {
            **result = previous;
            return std::unexpected(ErrorCode::TimbreInvalidParameter);
        }
    }

    return {};
}

Result<float> get_parameter(const TimbreProfile& profile, const std::string& path) {
    auto segs = split_path(path);
    if (!segs) return std::unexpected(invalid_path());
    auto result = resolve_path_const(profile, *segs);
    if (!result) return std::unexpected(result.error());
    return **result;
}

Result<float> map_device_parameter_value(float source_value, const DeviceParameter& mapping) {
    if (!std::isfinite(source_value) || !std::isfinite(mapping.source_min) ||
        !std::isfinite(mapping.source_max) || !std::isfinite(mapping.range_min) ||
        !std::isfinite(mapping.range_max) || !(mapping.source_max > mapping.source_min) ||
        !(mapping.range_max > mapping.range_min))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    if (mapping.curve.type != MappingCurveType::Custom && !mapping.curve.custom_points.empty())
        return std::unexpected(ErrorCode::TimbreInvalidParameter);

    const float source_tolerance =
        std::max(1.0e-6f, (mapping.source_max - mapping.source_min) * 1.0e-6f);
    if (source_value < mapping.source_min - source_tolerance ||
        source_value > mapping.source_max + source_tolerance)
        return std::unexpected(ErrorCode::TimbreInvalidParameter);

    const float normalised =
        std::clamp((source_value - mapping.source_min) / (mapping.source_max - mapping.source_min),
                   0.0f,
                   1.0f);
    float curved = normalised;
    switch (mapping.curve.type) {
    case MappingCurveType::Linear:
        break;
    case MappingCurveType::Exponential:
        curved = normalised * normalised;
        break;
    case MappingCurveType::Logarithmic:
        curved = std::sqrt(normalised);
        break;
    case MappingCurveType::SCurve:
        curved = normalised * normalised * (3.0f - 2.0f * normalised);
        break;
    case MappingCurveType::ReverseSCurve:
        curved = 2.0f * normalised - normalised * normalised * (3.0f - 2.0f * normalised);
        break;
    case MappingCurveType::Custom: {
        const auto& points = mapping.curve.custom_points;
        if (points.size() < 2 || points.front().first != 0.0f || points.back().first != 1.0f)
            return std::unexpected(ErrorCode::TimbreInvalidParameter);
        for (std::size_t index = 0; index < points.size(); ++index) {
            const auto [x, y] = points[index];
            if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0f || x > 1.0f || y < 0.0f ||
                y > 1.0f || (index > 0 && !(x > points[index - 1].first)))
                return std::unexpected(ErrorCode::TimbreInvalidParameter);
        }
        const auto upper = std::lower_bound(
            points.begin(), points.end(), normalised, [](const auto& point, float value) {
                return point.first < value;
            });
        if (upper == points.begin()) {
            curved = upper->second;
        } else if (upper == points.end()) {
            curved = points.back().second;
        } else {
            const auto& lower = *std::prev(upper);
            const float local = (normalised - lower.first) / (upper->first - lower.first);
            curved = lower.second + local * (upper->second - lower.second);
        }
        break;
    }
    }

    const float target = mapping.range_min + curved * (mapping.range_max - mapping.range_min);
    if (!std::isfinite(target)) return std::unexpected(ErrorCode::TimbreInvalidParameter);
    return target;
}

// =============================================================================
// Macro Knobs
// =============================================================================

Result<std::uint8_t> create_lfo(TimbreProfile& profile, LFO lfo) {
    if (profile.modulation.lfos.size() > std::numeric_limits<std::uint8_t>::max() ||
        !validate_modulation_lfo(lfo))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    const auto index = static_cast<std::uint8_t>(profile.modulation.lfos.size());
    profile.modulation.lfos.push_back(std::move(lfo));
    return index;
}

Result<std::uint8_t> create_modulation_envelope(TimbreProfile& profile, Envelope envelope) {
    if (profile.modulation.envelopes.size() > std::numeric_limits<std::uint8_t>::max() ||
        !validate_modulation_envelope(envelope))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    const auto index = static_cast<std::uint8_t>(profile.modulation.envelopes.size());
    profile.modulation.envelopes.push_back(std::move(envelope));
    return index;
}

Result<std::uint8_t> create_step_sequencer(TimbreProfile& profile, StepSequencer sequencer) {
    if (profile.modulation.step_sequencers.size() > std::numeric_limits<std::uint8_t>::max() ||
        !validate_modulation_step_sequencer(sequencer))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    const auto index = static_cast<std::uint8_t>(profile.modulation.step_sequencers.size());
    profile.modulation.step_sequencers.push_back(std::move(sequencer));
    return index;
}

Result<void> create_macro(TimbreProfile& profile, MacroKnob macro) {
    if (!validate_modulation_macro(profile, macro))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    // TI-14: reject duplicate macro index
    for (const auto& m : profile.modulation.macro_knobs) {
        if (m.index == macro.index) return std::unexpected(duplicate_id());
    }
    profile.modulation.macro_knobs.push_back(std::move(macro));
    return {};
}

Result<void> set_macro(TimbreProfile& profile, std::uint8_t macro_index, float value) {
    if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    for (auto& m : profile.modulation.macro_knobs) {
        if (m.index == macro_index) {
            m.value = value;
            return {};
        }
    }
    return std::unexpected(not_found());
}

// =============================================================================
// Modulation
// =============================================================================

Result<void> add_modulation(TimbreProfile& profile, ModulationRouting routing) {
    if (auto valid = validate_modulation_routing(profile, routing); !valid)
        return std::unexpected(valid.error());

    profile.modulation.routings.push_back(std::move(routing));
    return {};
}

// =============================================================================
// Automation
// =============================================================================

Result<void> add_automation(TimbreProfile& profile, TimbreAutomation automation) {
    if (auto valid = validate_timbre_automation(profile, automation); !valid)
        return std::unexpected(valid.error());

    profile.parameter_automation.push_back(std::move(automation));
    return {};
}

// =============================================================================
// Semantic Descriptors
// =============================================================================

void set_semantic_descriptors(TimbreProfile& profile, SemanticTimbreDescriptor descriptors) {
    profile.semantic_descriptors = std::move(descriptors);
}

SemanticTimbreDescriptor analyze_timbre(const TimbreProfile& profile) {
    SemanticTimbreDescriptor d;

    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, SubtractiveSynth>)
                d = analyze_subtractive(s);
            else if constexpr (std::is_same_v<T, FMSynth>)
                d = analyze_fm(s);
            else if constexpr (std::is_same_v<T, GranularSynth>)
                d = analyze_granular(s);
            else if constexpr (std::is_same_v<T, AdditiveSynth>)
                d = analyze_additive(s);
            else if constexpr (std::is_same_v<T, PhysicalModelSource>)
                d = analyze_physical(s);
            else {
                d.derivation = DerivationMode::ParameterDerived;
            }
        },
        profile.source.data);

    // Modulation increases movement
    if (!profile.modulation.lfos.empty() || !profile.modulation.step_sequencers.empty()) {
        float mod_depth = 0.0f;
        for (const auto& r : profile.modulation.routings)
            mod_depth = std::max(mod_depth, std::abs(r.depth));
        d.movement = clamp01(d.movement + mod_depth * 0.5f);
    }

    return d;
}

// =============================================================================
// Presets
// =============================================================================

std::vector<TimbrePreset> search_presets(const std::vector<TimbrePreset>& library,
                                         const PresetSearchQuery& query) {
    struct ScoredPreset {
        const TimbrePreset* preset;
        int score;
    };

    std::vector<ScoredPreset> candidates;
    for (const auto& p : library) {
        int score = 0;

        if (query.name_contains) {
            if (p.name.find(*query.name_contains) == std::string::npos) continue;
            score += 10;
        }

        bool all_tags = true;
        for (const auto& req : query.required_tags) {
            bool found = false;
            for (const auto& t : p.tags) {
                if (t == req) {
                    found = true;
                    break;
                }
            }
            if (found)
                score += 5;
            else
                all_tags = false;
        }
        if (!query.required_tags.empty() && !all_tags) continue;

        if (query.min_brightness && p.semantic_descriptors.brightness < *query.min_brightness)
            continue;
        if (query.max_brightness && p.semantic_descriptors.brightness > *query.max_brightness)
            continue;
        if (query.min_warmth && p.semantic_descriptors.warmth < *query.min_warmth) continue;
        if (query.max_warmth && p.semantic_descriptors.warmth > *query.max_warmth) continue;

        candidates.push_back({&p, score});
    }

    std::sort(candidates.begin(),
              candidates.end(),
              [](const ScoredPreset& a, const ScoredPreset& b) { return a.score > b.score; });

    std::vector<TimbrePreset> results;
    for (std::size_t i = 0; i < std::min(candidates.size(), query.max_results); ++i) {
        results.push_back(*candidates[i].preset);
    }
    return results;
}

Result<void> load_preset(TimbreProfile& profile, const TimbrePreset& preset) {
    // Record each overwritten value and roll back on the first failure, so a
    // failure part-way through leaves the profile exactly as it was. The
    // profile is move-only, so it cannot simply be copied and committed.
    std::vector<std::pair<float*, float>> applied;
    for (const auto& [path, value] : preset.parameter_state) {
        float* slot = nullptr;
        if (auto segments = split_path(path)) {
            if (auto resolved = resolve_path(profile, *segments)) slot = *resolved;
        }
        const float previous = slot != nullptr ? *slot : 0.0f;
        if (auto r = set_parameter(profile, path, value); !r) {
            for (auto restore = applied.rbegin(); restore != applied.rend(); ++restore)
                *restore->first = restore->second;
            return r;
        }
        applied.emplace_back(slot, previous);
    }
    profile.semantic_descriptors = preset.semantic_descriptors;
    return {};
}

TimbrePreset save_preset(const TimbreProfile& profile, TimbrePresetId id, const std::string& name) {
    TimbrePreset preset;
    preset.id = id;
    preset.name = name;
    preset.semantic_descriptors = profile.semantic_descriptors;

    std::visit(
        [&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, SubtractiveSynth>)
                extract_subtractive_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, FMSynth>)
                extract_fm_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, WavetableSynth>)
                extract_wavetable_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, GranularSynth>)
                extract_granular_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, AdditiveSynth>)
                extract_additive_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, PhysicalModelSource>)
                extract_physical_params(s, preset.parameter_state);
            else if constexpr (std::is_same_v<T, SamplerSource>)
                extract_sampler_params(s, preset.parameter_state);
        },
        profile.source.data);

    return preset;
}

Result<void>
morph_presets(TimbreProfile& profile, PresetMorph morph, const std::vector<TimbrePreset>& library) {
    // TI-11: morph interval must be non-degenerate (start < end) and lie in the score
    if (morph.start < SCORE_START || !(morph.start < morph.end))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    const auto known = [&](TimbrePresetId id) {
        return std::ranges::any_of(library,
                                   [&](const TimbrePreset& preset) { return preset.id == id; });
    };
    if (!known(morph.from_preset) || !known(morph.to_preset)) return std::unexpected(not_found());

    profile.preset_morphs.push_back(std::move(morph));
    return {};
}

// =============================================================================
// Validation
// =============================================================================

std::vector<Diagnostic> validate(const TimbreProfile& profile, float sample_rate_hz) {
    return validate_timbre(profile, sample_rate_hz);
}

} // namespace sunny::core
