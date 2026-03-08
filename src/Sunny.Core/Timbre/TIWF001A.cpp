/**
 * @file TIWF001A.cpp
 * @brief Timbre IR workflow functions — implementation
 *
 * Component: TIWF001A
 * Domain: TI (Timbre IR) | Category: WF (Workflow)
 *
 * Tests: TSTI005A
 */

#include "TIWF001A.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace Sunny::Core {

namespace {

// =============================================================================
// Path parsing utilities
// =============================================================================

/// Split a dot-separated path into segments, resolving array indices.
/// "source.oscillators[0].tune_cents" → ["source", "oscillators", "[0]", "tune_cents"]
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : path) {
        if (c == '.') {
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
        } else if (c == '[') {
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
            current.push_back('[');
        } else if (c == ']') {
            current.push_back(']');
            segments.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) segments.push_back(std::move(current));
    return segments;
}

/// Parse "[N]" → N, returns -1 on failure
int parse_index(const std::string& seg) {
    if (seg.size() < 3 || seg.front() != '[' || seg.back() != ']') return -1;
    int idx = 0;
    auto [ptr, ec] = std::from_chars(seg.data() + 1, seg.data() + seg.size() - 1, idx);
    if (ec != std::errc{}) return -1;
    return idx;
}

ErrorCode invalid_path() {
    return static_cast<ErrorCode>(TimbreError::InvalidModTarget);
}

ErrorCode not_found() {
    return static_cast<ErrorCode>(TimbreError::NotFound);
}

ErrorCode duplicate_id() {
    return static_cast<ErrorCode>(TimbreError::DuplicateId);
}

// =============================================================================
// Parameter path resolver — SubtractiveSynth
// =============================================================================

Result<float*> resolve_subtractive(SubtractiveSynth& sub,
                                   const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "filter" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "cutoff")        return &sub.filter.cutoff;
        if (field == "resonance")     return &sub.filter.resonance;
        if (field == "drive")         return &sub.filter.drive;
        if (field == "key_tracking")  return &sub.filter.key_tracking;
        if (field == "env_depth")     return &sub.filter.envelope_depth;
    }
    if (seg == "filter_2" && sub.filter_2 && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "cutoff")        return &sub.filter_2->cutoff;
        if (field == "resonance")     return &sub.filter_2->resonance;
        if (field == "drive")         return &sub.filter_2->drive;
        if (field == "key_tracking")  return &sub.filter_2->key_tracking;
        if (field == "env_depth")     return &sub.filter_2->envelope_depth;
    }
    if (seg == "oscillators" && pos + 2 < segs.size()) {
        int idx = parse_index(segs[pos + 1]);
        if (idx < 0 || static_cast<std::size_t>(idx) >= sub.oscillators.size())
            return std::unexpected(invalid_path());
        auto& osc = sub.oscillators[static_cast<std::size_t>(idx)];
        const auto& field = segs[pos + 2];
        if (field == "tune_cents")    return &osc.tune_cents;
        if (field == "phase")         return &osc.phase;
        if (field == "pulse_width")   return &osc.pulse_width;
        if (field == "level")         return &osc.level;
    }
    if (seg == "amplifier" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "velocity_sensitivity") return &sub.amplifier.velocity_sensitivity;
        if (field == "key_tracking")         return &sub.amplifier.key_tracking;
    }
    if (seg == "noise" && sub.noise && pos + 1 < segs.size()) {
        if (segs[pos + 1] == "level") return &sub.noise->level;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — FMSynth
// =============================================================================

Result<float*> resolve_fm(FMSynth& fm,
                          const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "feedback") return &fm.feedback;
    if (seg == "operators" && pos + 2 < segs.size()) {
        int idx = parse_index(segs[pos + 1]);
        if (idx < 0 || static_cast<std::size_t>(idx) >= fm.operators.size())
            return std::unexpected(invalid_path());
        auto& op = fm.operators[static_cast<std::size_t>(idx)];
        const auto& field = segs[pos + 2];
        if (field == "ratio")  return &op.ratio;
        if (field == "level")  return &op.level;
        if (field == "detune") return &op.detune;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — WavetableSynth
// =============================================================================

Result<float*> resolve_wavetable(WavetableSynth& wt,
                                 const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "position") return &wt.position;
    if (seg == "filter" && wt.filter && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "cutoff")    return &wt.filter->cutoff;
        if (field == "resonance") return &wt.filter->resonance;
    }
    if (seg == "amplifier" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "velocity_sensitivity") return &wt.amplifier.velocity_sensitivity;
        if (field == "key_tracking")         return &wt.amplifier.key_tracking;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — GranularSynth
// =============================================================================

Result<float*> resolve_granular(GranularSynth& gr,
                                const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "grain_size")        return &gr.grain_size;
    if (seg == "grain_density")     return &gr.grain_density;
    if (seg == "position")          return &gr.position;
    if (seg == "position_random")   return &gr.position_random;
    if (seg == "pitch_random")      return &gr.pitch_random;
    if (seg == "spray")             return &gr.spray;
    if (seg == "stereo_spread")     return &gr.stereo_spread;
    if (seg == "reverse_probability") return &gr.reverse_probability;
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — AdditiveSynth
// =============================================================================

Result<float*> resolve_additive(AdditiveSynth& add,
                                const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "partials" && pos + 2 < segs.size()) {
        int idx = parse_index(segs[pos + 1]);
        if (idx < 0 || static_cast<std::size_t>(idx) >= add.partials.size())
            return std::unexpected(invalid_path());
        auto& p = add.partials[static_cast<std::size_t>(idx)];
        const auto& field = segs[pos + 2];
        if (field == "ratio")     return &p.ratio;
        if (field == "amplitude") return &p.amplitude;
        if (field == "phase")     return &p.phase;
        if (field == "detune")    return &p.detune;
    }
    if (seg == "global_envelope" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "velocity_sensitivity") return &add.global_envelope.velocity_sensitivity;
        if (field == "key_tracking")         return &add.global_envelope.key_tracking;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — PhysicalModelSource
// =============================================================================

Result<float*> resolve_physical(PhysicalModelSource& pm,
                                const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "coupling")   return &pm.coupling;
    if (seg == "damping")    return &pm.damping;
    if (seg == "brightness") return &pm.brightness;
    if (seg == "exciter" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "amplitude")  return &pm.exciter.amplitude;
        if (field == "brightness") return &pm.exciter.brightness;
        if (field == "noise_mix")  return &pm.exciter.noise_mix;
    }
    if (seg == "resonator" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "frequency_ratio") return &pm.resonator.frequency_ratio;
        if (field == "decay")           return &pm.resonator.decay;
        if (field == "inharm")          return &pm.resonator.inharm;
    }
    if (seg == "model" && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "reed_stiffness") return &pm.model.reed_stiffness;
        if (field == "lip_mass")       return &pm.model.lip_mass;
        if (field == "bow_pressure")   return &pm.model.bow_pressure;
        if (field == "bow_speed")      return &pm.model.bow_speed;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — SamplerSource
// =============================================================================

Result<float*> resolve_sampler(SamplerSource& sam,
                               const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& seg = segs[pos];

    if (seg == "tuning_offset") return &sam.tuning_offset;
    if (seg == "filter" && sam.filter && pos + 1 < segs.size()) {
        const auto& field = segs[pos + 1];
        if (field == "cutoff")    return &sam.filter->cutoff;
        if (field == "resonance") return &sam.filter->resonance;
    }
    return std::unexpected(invalid_path());
}

// =============================================================================
// Parameter path resolver — SoundSource dispatch
// =============================================================================

Result<float*> resolve_source(SoundSourceData& src,
                              const std::vector<std::string>& segs, std::size_t pos) {
    return std::visit([&](auto& s) -> Result<float*> {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>)     return resolve_subtractive(s, segs, pos);
        else if constexpr (std::is_same_v<T, FMSynth>)         return resolve_fm(s, segs, pos);
        else if constexpr (std::is_same_v<T, WavetableSynth>)  return resolve_wavetable(s, segs, pos);
        else if constexpr (std::is_same_v<T, GranularSynth>)   return resolve_granular(s, segs, pos);
        else if constexpr (std::is_same_v<T, AdditiveSynth>)   return resolve_additive(s, segs, pos);
        else if constexpr (std::is_same_v<T, PhysicalModelSource>) return resolve_physical(s, segs, pos);
        else if constexpr (std::is_same_v<T, SamplerSource>)   return resolve_sampler(s, segs, pos);
        else return std::unexpected(invalid_path());
    }, src.data);
}

// =============================================================================
// Parameter path resolver — insert chain effects
// =============================================================================

Result<float*> resolve_effect_params(EffectParameters& params,
                                     const std::vector<std::string>& segs, std::size_t pos) {
    if (pos >= segs.size()) return std::unexpected(invalid_path());
    const auto& field = segs[pos];

    return std::visit([&](auto& e) -> Result<float*> {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, DistortionEffect>) {
            if (field == "drive")  return &e.drive;
            if (field == "tone")   return &e.tone;
            if (field == "output") return &e.output_level;
        } else if constexpr (std::is_same_v<T, DelayEffect>) {
            if (field == "feedback") return &e.feedback;
            if (field == "mod_rate") return &e.modulation_rate;
            if (field == "mod_depth") return &e.modulation_depth;
        } else if constexpr (std::is_same_v<T, ReverbEffect>) {
            if (field == "decay")     return &e.decay_time;
            if (field == "pre_delay") return &e.pre_delay;
            if (field == "damping")   return &e.damping;
            if (field == "diffusion") return &e.diffusion;
            if (field == "size")      return &e.size;
        } else if constexpr (std::is_same_v<T, ChorusEffect>) {
            if (field == "rate")     return &e.rate;
            if (field == "depth")    return &e.depth;
            if (field == "feedback") return &e.feedback;
            if (field == "spread")   return &e.stereo_spread;
        } else if constexpr (std::is_same_v<T, PhaserEffect>) {
            if (field == "rate")     return &e.rate;
            if (field == "depth")    return &e.depth;
            if (field == "feedback") return &e.feedback;
            if (field == "center")   return &e.center_frequency;
        } else if constexpr (std::is_same_v<T, FlangerEffect>) {
            if (field == "rate")     return &e.rate;
            if (field == "depth")    return &e.depth;
            if (field == "feedback") return &e.feedback;
            if (field == "manual")   return &e.manual;
        } else if constexpr (std::is_same_v<T, EQEffect>) {
            // EQ bands addressed by index: bands[N].gain, etc.
        } else if constexpr (std::is_same_v<T, CompressorEffect>) {
            if (field == "threshold") return &e.threshold;
            if (field == "ratio")     return &e.ratio;
            if (field == "attack")    return &e.attack;
            if (field == "release")   return &e.release;
            if (field == "knee")      return &e.knee;
            if (field == "makeup")    return &e.makeup_gain;
        }
        return std::unexpected(invalid_path());
    }, params);
}

// =============================================================================
// Parameter path resolver — top level
// =============================================================================

Result<float*> resolve_path(TimbreProfile& profile,
                            const std::vector<std::string>& segs) {
    if (segs.empty()) return std::unexpected(invalid_path());
    const auto& root = segs[0];

    if (root == "source") {
        return resolve_source(profile.source, segs, 1);
    }
    if (root == "insert_chain" && segs.size() >= 2 && segs[1] == "effects" && segs.size() >= 3) {
        int idx = parse_index(segs[2]);
        if (idx < 0 || static_cast<std::size_t>(idx) >= profile.insert_chain.effects.size())
            return std::unexpected(invalid_path());
        auto& effect = profile.insert_chain.effects[static_cast<std::size_t>(idx)];
        if (segs.size() >= 4 && segs[3] == "mix") return &effect.mix;
        if (segs.size() >= 4) return resolve_effect_params(effect.parameters, segs, 3);
    }
    if (root == "modulation" && segs.size() >= 2) {
        if (segs[1] == "lfos" && segs.size() >= 4) {
            int idx = parse_index(segs[2]);
            if (idx < 0 || static_cast<std::size_t>(idx) >= profile.modulation.lfos.size())
                return std::unexpected(invalid_path());
            auto& lfo = profile.modulation.lfos[static_cast<std::size_t>(idx)];
            const auto& field = segs[3];
            if (field == "phase")    return &lfo.phase;
            if (field == "symmetry") return &lfo.symmetry;
            if (field == "fade_in")  return &lfo.fade_in;
            if (field == "delay")    return &lfo.delay;
            if (field == "rate" && segs.size() >= 5 && segs[4] == "hz") return &lfo.rate.hz;
        }
        if (segs[1] == "macro_knobs" && segs.size() >= 4) {
            int idx = parse_index(segs[2]);
            if (idx < 0 || static_cast<std::size_t>(idx) >= profile.modulation.macro_knobs.size())
                return std::unexpected(invalid_path());
            if (segs[3] == "value") return &profile.modulation.macro_knobs[static_cast<std::size_t>(idx)].value;
        }
    }
    if (root == "semantic_descriptors" && segs.size() >= 2) {
        auto& sd = profile.semantic_descriptors;
        const auto& field = segs[1];
        if (field == "brightness") return &sd.brightness;
        if (field == "warmth")     return &sd.warmth;
        if (field == "roughness")  return &sd.roughness;
        if (field == "width")      return &sd.width;
        if (field == "density")    return &sd.density;
        if (field == "movement")   return &sd.movement;
        if (field == "weight")     return &sd.weight;
    }
    return std::unexpected(invalid_path());
}

// resolve_path returns float* for mutation; this wrapper provides read access.
// The const_cast is safe because resolve_path only computes offsets into the
// profile struct without writing, and the returned pointer is immediately
// re-qualified as const. The mutable version exists because workflow mutations
// (wf_set_parameter) require a writable pointer through the same path logic.
Result<const float*> resolve_path_const(const TimbreProfile& profile,
                                        const std::vector<std::string>& segs) {
    auto result = resolve_path(const_cast<TimbreProfile&>(profile), segs);
    if (!result) return std::unexpected(result.error());
    return static_cast<const float*>(*result);
}

// =============================================================================
// Heuristic timbre analysis
// =============================================================================

float clamp01(float v) { return std::min(1.0f, std::max(0.0f, v)); }

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
        if (attack_ms < 5.0f) d.attack_character = AttackCharacter::Percussive;
        else if (attack_ms < 30.0f) d.attack_character = AttackCharacter::Plucked;
        else if (attack_ms < 100.0f) d.attack_character = AttackCharacter::Gradual;
        else d.attack_character = AttackCharacter::Swelling;
    }

    // Sustain character from envelope shape
    if (sub.amplifier.stages.size() >= 2) {
        float sustain_level = sub.amplifier.stages[1].target_level;
        if (sustain_level > 0.9f) d.sustain_character = SustainCharacter::Steady;
        else if (sustain_level > 0.5f) d.sustain_character = SustainCharacter::Decaying;
        else d.sustain_character = SustainCharacter::Decaying;
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
        case ExciterType::Strike:  d.attack_character = AttackCharacter::Percussive; break;
        case ExciterType::Bow:     d.attack_character = AttackCharacter::Bowed; break;
        case ExciterType::Blow:    d.attack_character = AttackCharacter::Blown; break;
        case ExciterType::Impulse: d.attack_character = AttackCharacter::Plucked; break;
        default:                   d.attack_character = AttackCharacter::Gradual; break;
    }
    d.sustain_character = (pm.exciter.type == ExciterType::Bow || pm.exciter.type == ExciterType::Blow)
                          ? SustainCharacter::Steady : SustainCharacter::Decaying;
    d.derivation = DerivationMode::ParameterDerived;
    return d;
}

// =============================================================================
// Preset key parameter extraction
// =============================================================================

void extract_subtractive_params(const SubtractiveSynth& sub,
                                std::map<std::string, float>& params) {
    params["source.filter.cutoff"]    = sub.filter.cutoff;
    params["source.filter.resonance"] = sub.filter.resonance;
    params["source.filter.drive"]     = sub.filter.drive;
    params["source.filter.env_depth"] = sub.filter.envelope_depth;
    for (std::size_t i = 0; i < sub.oscillators.size(); ++i) {
        std::string prefix = "source.oscillators[" + std::to_string(i) + "].";
        params[prefix + "tune_cents"] = sub.oscillators[i].tune_cents;
        params[prefix + "level"]      = sub.oscillators[i].level;
        params[prefix + "pulse_width"] = sub.oscillators[i].pulse_width;
    }
    params["source.amplifier.velocity_sensitivity"] = sub.amplifier.velocity_sensitivity;
}

void extract_fm_params(const FMSynth& fm,
                       std::map<std::string, float>& params) {
    params["source.feedback"] = fm.feedback;
    for (std::size_t i = 0; i < fm.operators.size(); ++i) {
        std::string prefix = "source.operators[" + std::to_string(i) + "].";
        params[prefix + "ratio"]  = fm.operators[i].ratio;
        params[prefix + "level"]  = fm.operators[i].level;
        params[prefix + "detune"] = fm.operators[i].detune;
    }
}

void extract_wavetable_params(const WavetableSynth& wt,
                              std::map<std::string, float>& params) {
    params["source.position"] = wt.position;
    if (wt.filter) {
        params["source.filter.cutoff"]    = wt.filter->cutoff;
        params["source.filter.resonance"] = wt.filter->resonance;
    }
    params["source.amplifier.velocity_sensitivity"] = wt.amplifier.velocity_sensitivity;
    params["source.amplifier.key_tracking"]         = wt.amplifier.key_tracking;
}

void extract_granular_params(const GranularSynth& gr,
                             std::map<std::string, float>& params) {
    params["source.grain_size"]          = gr.grain_size;
    params["source.grain_density"]       = gr.grain_density;
    params["source.position"]            = gr.position;
    params["source.position_random"]     = gr.position_random;
    params["source.pitch_random"]        = gr.pitch_random;
    params["source.spray"]               = gr.spray;
    params["source.stereo_spread"]       = gr.stereo_spread;
    params["source.reverse_probability"] = gr.reverse_probability;
}

void extract_additive_params(const AdditiveSynth& add,
                             std::map<std::string, float>& params) {
    for (std::size_t i = 0; i < add.partials.size(); ++i) {
        std::string prefix = "source.partials[" + std::to_string(i) + "].";
        params[prefix + "ratio"]     = add.partials[i].ratio;
        params[prefix + "amplitude"] = add.partials[i].amplitude;
        params[prefix + "phase"]     = add.partials[i].phase;
        params[prefix + "detune"]    = add.partials[i].detune;
    }
    params["source.global_envelope.velocity_sensitivity"] = add.global_envelope.velocity_sensitivity;
    params["source.global_envelope.key_tracking"]         = add.global_envelope.key_tracking;
}

void extract_physical_params(const PhysicalModelSource& pm,
                             std::map<std::string, float>& params) {
    params["source.coupling"]   = pm.coupling;
    params["source.damping"]    = pm.damping;
    params["source.brightness"] = pm.brightness;
    params["source.exciter.amplitude"]  = pm.exciter.amplitude;
    params["source.exciter.brightness"] = pm.exciter.brightness;
    params["source.exciter.noise_mix"]  = pm.exciter.noise_mix;
    params["source.resonator.frequency_ratio"] = pm.resonator.frequency_ratio;
    params["source.resonator.decay"]           = pm.resonator.decay;
    params["source.resonator.inharm"]          = pm.resonator.inharm;
    params["source.model.reed_stiffness"] = pm.model.reed_stiffness;
    params["source.model.lip_mass"]       = pm.model.lip_mass;
    params["source.model.bow_pressure"]   = pm.model.bow_pressure;
    params["source.model.bow_speed"]      = pm.model.bow_speed;
}

void extract_sampler_params(const SamplerSource& sam,
                            std::map<std::string, float>& params) {
    params["source.tuning_offset"] = sam.tuning_offset;
    if (sam.filter) {
        params["source.filter.cutoff"]    = sam.filter->cutoff;
        params["source.filter.resonance"] = sam.filter->resonance;
    }
}

}  // anonymous namespace

// =============================================================================
// Profile Creation
// =============================================================================

TimbreProfile wf_create_timbre_profile(TimbreProfileId id, PartId part_id,
                                        const std::string& name) {
    TimbreProfile p;
    p.id = id;
    p.part_id = part_id;
    p.name = name;

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};  // Default sine oscillator
    sub.amplifier.stages = {
        {5.0f, 1.0f, EnvelopeCurve::Linear},
        {100.0f, 0.7f, EnvelopeCurve::Exponential},
        {200.0f, 0.0f, EnvelopeCurve::Exponential}
    };
    p.source.data = std::move(sub);

    return p;
}

// =============================================================================
// Source Configuration
// =============================================================================

Result<void> wf_set_sound_source(TimbreProfile& profile, SoundSourceData source) {
    SoundSourceData previous = std::move(profile.source);
    profile.source = std::move(source);

    auto diags = validate_timbre(profile);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) {
            profile.source = std::move(previous);
            return std::unexpected(static_cast<ErrorCode>(TimbreError::InvalidParameter));
        }
    }
    return {};
}

// =============================================================================
// Effect Chain
// =============================================================================

Result<void> wf_add_effect(TimbreProfile& profile, Effect effect) {
    profile.insert_chain.effects.push_back(std::move(effect));
    return {};
}

Result<void> wf_remove_effect(TimbreProfile& profile, EffectId effect_id) {
    auto& effects = profile.insert_chain.effects;
    auto it = std::find_if(effects.begin(), effects.end(),
                           [&](const Effect& e) { return e.id.value == effect_id.value; });
    if (it == effects.end())
        return std::unexpected(not_found());
    effects.erase(it);
    return {};
}

Result<void> wf_reorder_effects(TimbreProfile& profile,
                                 const std::vector<EffectId>& new_order) {
    auto& effects = profile.insert_chain.effects;
    if (new_order.size() != effects.size())
        return std::unexpected(not_found());

    // TI-3: reject duplicate IDs in the reorder list
    for (std::size_t i = 0; i < new_order.size(); ++i) {
        for (std::size_t j = i + 1; j < new_order.size(); ++j) {
            if (new_order[i].value == new_order[j].value)
                return std::unexpected(duplicate_id());
        }
    }

    std::vector<Effect> reordered;
    reordered.reserve(effects.size());
    for (const auto& id : new_order) {
        auto it = std::find_if(effects.begin(), effects.end(),
                               [&](const Effect& e) { return e.id.value == id.value; });
        if (it == effects.end())
            return std::unexpected(not_found());
        reordered.push_back(std::move(*it));
    }
    effects = std::move(reordered);
    return {};
}

// =============================================================================
// Parameter Access
// =============================================================================

Result<void> wf_set_parameter(TimbreProfile& profile, const std::string& path,
                               float value) {
    auto segs = split_path(path);
    auto result = resolve_path(profile, segs);
    if (!result) return std::unexpected(result.error());

    // Store previous value for rollback if validation fails
    float previous = **result;
    **result = value;

    // Post-mutation validation: reject if any Error-level diagnostic appears
    auto diags = validate_timbre(profile);
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) {
            **result = previous;
            return std::unexpected(static_cast<ErrorCode>(TimbreError::InvalidParameter));
        }
    }

    return {};
}

Result<float> wf_get_parameter(const TimbreProfile& profile,
                                const std::string& path) {
    auto segs = split_path(path);
    auto result = resolve_path_const(profile, segs);
    if (!result) return std::unexpected(result.error());
    return **result;
}

// =============================================================================
// Macro Knobs
// =============================================================================

Result<void> wf_create_macro(TimbreProfile& profile, MacroKnob macro) {
    // TI-14: reject duplicate macro index
    for (const auto& m : profile.modulation.macro_knobs) {
        if (m.index == macro.index)
            return std::unexpected(duplicate_id());
    }
    profile.modulation.macro_knobs.push_back(std::move(macro));
    return {};
}

Result<void> wf_set_macro(TimbreProfile& profile, std::uint8_t macro_index, float value) {
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

Result<void> wf_add_modulation(TimbreProfile& profile, ModulationRouting routing) {
    // TI-8: validate target path resolves to a parameter
    auto segs = split_path(routing.target);
    auto target_result = resolve_path(profile, segs);
    if (!target_result) return std::unexpected(invalid_path());

    // Validate depth in [-1, 1]
    if (routing.depth < -1.0f || routing.depth > 1.0f)
        return std::unexpected(static_cast<ErrorCode>(TimbreError::InvalidParameter));

    // Validate source index references an existing LFO, step sequencer, or macro
    bool source_valid = false;
    switch (routing.source.type) {
        case ModulationSourceType::LFO:
            source_valid = static_cast<std::size_t>(routing.source.index)
                           < profile.modulation.lfos.size();
            break;
        case ModulationSourceType::StepSequencer:
            source_valid = static_cast<std::size_t>(routing.source.index)
                           < profile.modulation.step_sequencers.size();
            break;
        case ModulationSourceType::MacroKnob: {
            auto idx = routing.source.index;
            source_valid = std::any_of(
                profile.modulation.macro_knobs.begin(),
                profile.modulation.macro_knobs.end(),
                [idx](const MacroKnob& m) { return m.index == idx; });
            break;
        }
        default:
            source_valid = true;  // velocity, keytrack, aftertouch, envelope, etc.
            break;
    }
    if (!source_valid)
        return std::unexpected(not_found());

    profile.modulation.routings.push_back(std::move(routing));
    return {};
}

// =============================================================================
// Automation
// =============================================================================

Result<void> wf_add_automation(TimbreProfile& profile, TimbreAutomation automation) {
    if (automation.breakpoints.empty())
        return std::unexpected(static_cast<ErrorCode>(TimbreError::InvalidParameter));

    // TI-7: validate that parameter_path resolves to a real parameter
    auto segs = split_path(automation.parameter_path);
    auto path_result = resolve_path(profile, segs);
    if (!path_result) return std::unexpected(invalid_path());

    profile.parameter_automation.push_back(std::move(automation));
    return {};
}

// =============================================================================
// Semantic Descriptors
// =============================================================================

void wf_set_semantic_descriptors(TimbreProfile& profile,
                                  SemanticTimbreDescriptor descriptors) {
    profile.semantic_descriptors = std::move(descriptors);
}

SemanticTimbreDescriptor wf_analyze_timbre(const TimbreProfile& profile) {
    SemanticTimbreDescriptor d;

    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>)     d = analyze_subtractive(s);
        else if constexpr (std::is_same_v<T, FMSynth>)         d = analyze_fm(s);
        else if constexpr (std::is_same_v<T, GranularSynth>)   d = analyze_granular(s);
        else if constexpr (std::is_same_v<T, AdditiveSynth>)   d = analyze_additive(s);
        else if constexpr (std::is_same_v<T, PhysicalModelSource>) d = analyze_physical(s);
        else {
            d.derivation = DerivationMode::ParameterDerived;
        }
    }, profile.source.data);

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

std::vector<TimbrePreset> wf_search_presets(const std::vector<TimbrePreset>& library,
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
                if (t == req) { found = true; break; }
            }
            if (found) score += 5;
            else all_tags = false;
        }
        if (!query.required_tags.empty() && !all_tags) continue;

        if (query.min_brightness && p.semantic_descriptors.brightness < *query.min_brightness) continue;
        if (query.max_brightness && p.semantic_descriptors.brightness > *query.max_brightness) continue;
        if (query.min_warmth && p.semantic_descriptors.warmth < *query.min_warmth) continue;
        if (query.max_warmth && p.semantic_descriptors.warmth > *query.max_warmth) continue;

        candidates.push_back({&p, score});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const ScoredPreset& a, const ScoredPreset& b) { return a.score > b.score; });

    std::vector<TimbrePreset> results;
    for (std::size_t i = 0; i < std::min(candidates.size(), query.max_results); ++i) {
        results.push_back(*candidates[i].preset);
    }
    return results;
}

Result<void> wf_load_preset(TimbreProfile& profile, const TimbrePreset& preset) {
    for (const auto& [path, value] : preset.parameter_state) {
        auto r = wf_set_parameter(profile, path, value);
        if (!r) return r;
    }
    profile.semantic_descriptors = preset.semantic_descriptors;
    return {};
}

TimbrePreset wf_save_preset(const TimbreProfile& profile, TimbrePresetId id,
                             const std::string& name) {
    TimbrePreset preset;
    preset.id = id;
    preset.name = name;
    preset.semantic_descriptors = profile.semantic_descriptors;

    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>)     extract_subtractive_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, FMSynth>)         extract_fm_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, WavetableSynth>)  extract_wavetable_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, GranularSynth>)   extract_granular_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, AdditiveSynth>)   extract_additive_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, PhysicalModelSource>) extract_physical_params(s, preset.parameter_state);
        else if constexpr (std::is_same_v<T, SamplerSource>)   extract_sampler_params(s, preset.parameter_state);
    }, profile.source.data);

    return preset;
}

Result<void> wf_morph_presets(TimbreProfile& profile, PresetMorph morph) {
    // TI-11: morph interval must be non-degenerate (start < end)
    if (!(morph.start < morph.end))
        return std::unexpected(static_cast<ErrorCode>(TimbreError::InvalidParameter));

    profile.preset_morphs.push_back(std::move(morph));
    return {};
}

// =============================================================================
// Validation
// =============================================================================

std::vector<Diagnostic> wf_validate(const TimbreProfile& profile, float sample_rate_hz) {
    return validate_timbre(profile, sample_rate_hz);
}

}  // namespace Sunny::Core
