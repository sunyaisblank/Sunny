/**
 * @file TISZ001A.cpp
 * @brief Timbre IR serialisation — implementation
 *
 * Component: TISZ001A
 * Domain: TI (Timbre IR) | Category: SZ (Serialisation)
 *
 * Tests: TSTI004A
 */

#include "TISZ001A.h"

namespace Sunny::Core {

using json = nlohmann::json;

namespace {

// =============================================================================
// Shared low-level helpers
// =============================================================================

template <typename EnumT>
EnumT check_enum(int val, EnumT max_val, const char* name, const json& j) {
    if (val < 0 || val > static_cast<int>(max_val))
        throw json::other_error::create(604, std::string(name) + " out of range", &j);
    return static_cast<EnumT>(val);
}

json beat_j(const Beat& b) {
    return json{{"num", b.numerator}, {"den", b.denominator}};
}

Beat beat_f(const json& j) {
    return Beat{j.at("num").get<std::int64_t>(),
                j.at("den").get<std::int64_t>()};
}

json st_j(const ScoreTime& t) {
    return json{{"bar", t.bar}, {"beat", beat_j(t.beat)}};
}

ScoreTime st_f(const json& j) {
    return ScoreTime{j.at("bar").get<std::uint32_t>(),
                     beat_f(j.at("beat"))};
}

json sp_j(const SpelledPitch& p) {
    return json{{"letter", p.letter}, {"acc", p.accidental}, {"oct", p.octave}};
}

SpelledPitch sp_f(const json& j) {
    return SpelledPitch{
        static_cast<std::uint8_t>(j.at("letter").get<int>()),
        j.at("acc").get<std::int8_t>(),
        static_cast<std::int8_t>(j.at("oct").get<int>())};
}

// =============================================================================
// §2 Waveform, Oscillator, Filter
// =============================================================================

json waveform_j(const Waveform& w) {
    json j;
    j["type"] = static_cast<int>(w.type);
    if (w.type == WaveformType::SuperSaw) {
        j["ss_detune"] = w.super_saw_detune;
        j["ss_voices"] = w.super_saw_voices;
    }
    if (w.type == WaveformType::Custom && !w.custom_harmonics.empty()) {
        json arr = json::array();
        for (const auto& h : w.custom_harmonics)
            arr.push_back(json{{"n", h.partial_number}, {"a", h.amplitude}, {"p", h.phase}});
        j["harmonics"] = arr;
    }
    return j;
}

Waveform waveform_f(const json& j) {
    Waveform w;
    w.type = check_enum(j.at("type").get<int>(), WaveformType::Custom, "WaveformType", j);
    if (j.contains("ss_detune")) w.super_saw_detune = j["ss_detune"].get<float>();
    if (j.contains("ss_voices")) w.super_saw_voices = j["ss_voices"].get<std::uint8_t>();
    if (j.contains("harmonics")) {
        for (const auto& h : j["harmonics"])
            w.custom_harmonics.push_back({h.at("n").get<std::uint16_t>(),
                                           h.at("a").get<float>(),
                                           h.at("p").get<float>()});
    }
    return w;
}

json oscillator_j(const Oscillator& o) {
    return json{
        {"waveform", waveform_j(o.waveform)},
        {"tune_st", o.tune_semitones},
        {"tune_ct", o.tune_cents},
        {"phase", o.phase},
        {"pw", o.pulse_width},
        {"level", o.level}
    };
}

Oscillator oscillator_f(const json& j) {
    Oscillator o;
    o.waveform = waveform_f(j.at("waveform"));
    o.tune_semitones = j.at("tune_st").get<std::int8_t>();
    o.tune_cents = j.at("tune_ct").get<float>();
    o.phase = j.at("phase").get<float>();
    o.pulse_width = j.at("pw").get<float>();
    o.level = j.at("level").get<float>();
    return o;
}

json noise_j(const NoiseGenerator& n) {
    return json{{"colour", static_cast<int>(n.colour)}, {"level", n.level}};
}

NoiseGenerator noise_f(const json& j) {
    return {check_enum(j.at("colour").get<int>(), NoiseColour::Violet, "NoiseColour", j),
            j.at("level").get<float>()};
}

json filter_config_j(const FilterConfig& c) {
    return json{
        {"cat", static_cast<int>(c.category)},
        {"slope", static_cast<int>(c.slope)},
        {"bw", c.bandwidth},
        {"comb_fb", c.comb_feedback},
        {"vowel", static_cast<int>(c.vowel)}
    };
}

FilterConfig filter_config_f(const json& j) {
    FilterConfig c;
    c.category = check_enum(j.at("cat").get<int>(), FilterCategory::Diode, "FilterCategory", j);
    c.slope = check_enum(j.at("slope").get<int>(), FilterSlope::Pole4, "FilterSlope", j);
    c.bandwidth = j.at("bw").get<float>();
    c.comb_feedback = j.at("comb_fb").get<float>();
    c.vowel = check_enum(j.at("vowel").get<int>(), FormantVowel::U, "FormantVowel", j);
    return c;
}

// Forward declare for filter's envelope field
json envelope_j(const Envelope& e);
Envelope envelope_f(const json& j);

json filter_j(const Filter& f) {
    json j;
    j["config"] = filter_config_j(f.config);
    j["cutoff"] = f.cutoff;
    j["resonance"] = f.resonance;
    j["drive"] = f.drive;
    j["key_tracking"] = f.key_tracking;
    if (f.envelope) j["envelope"] = envelope_j(*f.envelope);
    j["env_depth"] = f.envelope_depth;
    return j;
}

Filter filter_f(const json& j) {
    Filter f;
    f.config = filter_config_f(j.at("config"));
    f.cutoff = j.at("cutoff").get<float>();
    f.resonance = j.at("resonance").get<float>();
    f.drive = j.at("drive").get<float>();
    f.key_tracking = j.at("key_tracking").get<float>();
    if (j.contains("envelope")) f.envelope = envelope_f(j.at("envelope"));
    f.envelope_depth = j.at("env_depth").get<float>();
    return f;
}

// =============================================================================
// §3 Envelope
// =============================================================================

json envelope_stage_j(const EnvelopeStage& s) {
    return json{
        {"dur", s.duration}, {"target", s.target_level},
        {"curve", static_cast<int>(s.curve)}, {"curv_amt", s.curvature}
    };
}

EnvelopeStage envelope_stage_f(const json& j) {
    return {j.at("dur").get<float>(),
            j.at("target").get<float>(),
            check_enum(j.at("curve").get<int>(), EnvelopeCurve::Step, "EnvelopeCurve", j),
            j.value("curv_amt", 1.0f)};
}

json envelope_j(const Envelope& e) {
    json j;
    j["stages"] = json::array();
    for (const auto& s : e.stages) j["stages"].push_back(envelope_stage_j(s));
    if (e.loop) {
        j["loop"] = json{{"start", e.loop->start_stage},
                          {"end", e.loop->end_stage},
                          {"count", e.loop->count}};
    }
    j["vel_sens"] = e.velocity_sensitivity;
    j["key_trk"] = e.key_tracking;
    return j;
}

Envelope envelope_f(const json& j) {
    Envelope e;
    for (const auto& s : j.at("stages")) e.stages.push_back(envelope_stage_f(s));
    if (j.contains("loop")) {
        e.loop = EnvelopeLoop{
            j["loop"].at("start").get<std::uint8_t>(),
            j["loop"].at("end").get<std::uint8_t>(),
            j["loop"].at("count").get<std::uint8_t>()};
    }
    e.velocity_sensitivity = j.value("vel_sens", 0.0f);
    e.key_tracking = j.value("key_trk", 0.0f);
    return e;
}

// =============================================================================
// §2.2.4–5 Unison, Portamento
// =============================================================================

json unison_j(const UnisonConfig& u) {
    return json{{"voices", u.voice_count}, {"detune", u.detune},
                {"spread", u.stereo_spread}, {"blend", u.blend}};
}

UnisonConfig unison_f(const json& j) {
    return {j.at("voices").get<std::uint8_t>(), j.at("detune").get<float>(),
            j.at("spread").get<float>(), j.at("blend").get<float>()};
}

json portamento_j(const PortamentoConfig& p) {
    return json{{"time", p.time}, {"mode", static_cast<int>(p.mode)},
                {"curve", static_cast<int>(p.curve)}};
}

PortamentoConfig portamento_f(const json& j) {
    return {j.at("time").get<float>(),
            check_enum(j.at("mode").get<int>(), PortamentoMode::Legato, "PortamentoMode", j),
            check_enum(j.at("curve").get<int>(), GlideCurve::SCurve, "GlideCurve", j)};
}

// =============================================================================
// §2.3 FM
// =============================================================================

json fm_operator_j(const FMOperator& op) {
    json j;
    j["ratio"] = op.ratio;
    if (op.fixed_frequency) j["fixed_freq"] = *op.fixed_frequency;
    j["level"] = op.level;
    j["envelope"] = envelope_j(op.envelope);
    j["detune"] = op.detune;
    j["waveform"] = waveform_j(op.waveform);
    return j;
}

FMOperator fm_operator_f(const json& j) {
    FMOperator op;
    op.ratio = j.at("ratio").get<float>();
    if (j.contains("fixed_freq")) op.fixed_frequency = j["fixed_freq"].get<float>();
    op.level = j.at("level").get<float>();
    op.envelope = envelope_f(j.at("envelope"));
    op.detune = j.at("detune").get<float>();
    op.waveform = waveform_f(j.at("waveform"));
    return op;
}

json fm_algorithm_j(const FMAlgorithm& a) {
    json j;
    j["use_preset"] = a.use_preset;
    j["preset"] = a.preset_number;
    if (!a.custom_routing.empty()) {
        json arr = json::array();
        for (const auto& r : a.custom_routing)
            arr.push_back(json{{"mod", r.modulator}, {"car", r.carrier}, {"depth", r.depth}});
        j["routing"] = arr;
    }
    return j;
}

FMAlgorithm fm_algorithm_f(const json& j) {
    FMAlgorithm a;
    a.use_preset = j.at("use_preset").get<bool>();
    a.preset_number = j.at("preset").get<std::uint8_t>();
    if (j.contains("routing")) {
        for (const auto& r : j["routing"])
            a.custom_routing.push_back({r.at("mod").get<std::uint8_t>(),
                                         r.at("car").get<std::uint8_t>(),
                                         r.at("depth").get<float>()});
    }
    return a;
}

// =============================================================================
// §2.5–2.8 Granular, Additive, PhysicalModel, Sampler
// =============================================================================

json grain_env_j(const GrainEnvelope& g) {
    json j;
    j["shape"] = static_cast<int>(g.shape);
    j["trap_atk"] = g.trapezoid_attack_ratio;
    j["trap_rel"] = g.trapezoid_release_ratio;
    return j;
}

GrainEnvelope grain_env_f(const json& j) {
    return {check_enum(j.at("shape").get<int>(), GrainEnvelopeShape::Rectangular,
                       "GrainEnvelopeShape", j),
            j.value("trap_atk", 0.25f), j.value("trap_rel", 0.25f)};
}

json partial_def_j(const PartialDefinition& p) {
    json j;
    j["ratio"] = p.ratio;
    j["amplitude"] = p.amplitude;
    j["phase"] = p.phase;
    if (p.envelope) j["envelope"] = envelope_j(*p.envelope);
    j["detune"] = p.detune;
    return j;
}

PartialDefinition partial_def_f(const json& j) {
    PartialDefinition p;
    p.ratio = j.at("ratio").get<float>();
    p.amplitude = j.at("amplitude").get<float>();
    p.phase = j.at("phase").get<float>();
    if (j.contains("envelope")) p.envelope = envelope_f(j.at("envelope"));
    p.detune = j.at("detune").get<float>();
    return p;
}

json phys_model_j(const PhysicalModelConfig& m) {
    return json{
        {"cat", static_cast<int>(m.category)},
        {"material", static_cast<int>(m.string_material)},
        {"open_end", m.tube_open_end},
        {"reed_stiff", m.reed_stiffness},
        {"lip_mass", m.lip_mass},
        {"bow_press", m.bow_pressure},
        {"bow_speed", m.bow_speed}
    };
}

PhysicalModelConfig phys_model_f(const json& j) {
    PhysicalModelConfig m;
    m.category = check_enum(j.at("cat").get<int>(), PhysicalModelCategory::Bowed,
                            "PhysicalModelCategory", j);
    m.string_material = check_enum(j.at("material").get<int>(), StringMaterial::Wire,
                                    "StringMaterial", j);
    m.tube_open_end = j.at("open_end").get<bool>();
    m.reed_stiffness = j.at("reed_stiff").get<float>();
    m.lip_mass = j.at("lip_mass").get<float>();
    m.bow_pressure = j.at("bow_press").get<float>();
    m.bow_speed = j.at("bow_speed").get<float>();
    return m;
}

json exciter_j(const ExciterConfig& e) {
    return json{{"type", static_cast<int>(e.type)}, {"amp", e.amplitude},
                {"bright", e.brightness}, {"noise", e.noise_mix}};
}

ExciterConfig exciter_f(const json& j) {
    return {check_enum(j.at("type").get<int>(), ExciterType::Strike, "ExciterType", j),
            j.at("amp").get<float>(), j.at("bright").get<float>(),
            j.at("noise").get<float>()};
}

json resonator_j(const ResonatorConfig& r) {
    return json{{"freq_ratio", r.frequency_ratio}, {"decay", r.decay},
                {"inharm", r.inharm}, {"modes", r.mode_count}};
}

ResonatorConfig resonator_f(const json& j) {
    return {j.at("freq_ratio").get<float>(), j.at("decay").get<float>(),
            j.at("inharm").get<float>(), j.at("modes").get<std::uint8_t>()};
}

json mic_pos_j(const MicrophonePosition& m) {
    return json{{"name", m.name}, {"level", m.level}, {"enabled", m.enabled}};
}

MicrophonePosition mic_pos_f(const json& j) {
    return {j.at("name").get<std::string>(), j.at("level").get<float>(),
            j.at("enabled").get<bool>()};
}

json sample_zone_j(const SampleZone& z) {
    return json{{"low_key", z.low_key}, {"high_key", z.high_key},
                {"low_vel", z.low_velocity}, {"high_vel", z.high_velocity},
                {"artic", z.articulation}, {"path", z.sample_path}};
}

SampleZone sample_zone_f(const json& j) {
    return {j.at("low_key").get<std::uint8_t>(), j.at("high_key").get<std::uint8_t>(),
            j.at("low_vel").get<std::uint8_t>(), j.at("high_vel").get<std::uint8_t>(),
            j.at("artic").get<std::string>(), j.at("path").get<std::string>()};
}

json sample_map_j(const SampleMap& m) {
    json arr = json::array();
    for (const auto& z : m.zones) arr.push_back(sample_zone_j(z));
    return arr;
}

SampleMap sample_map_f(const json& j) {
    SampleMap m;
    for (const auto& z : j) m.zones.push_back(sample_zone_f(z));
    return m;
}

// =============================================================================
// §4 Modulation
// =============================================================================

json lfo_rate_j(const LFORate& r) {
    json j;
    j["synced"] = r.synced;
    j["hz"] = r.hz;
    j["div"] = beat_j(r.division);
    return j;
}

LFORate lfo_rate_f(const json& j) {
    return {j.at("synced").get<bool>(), j.at("hz").get<float>(),
            beat_f(j.at("div"))};
}

json lfo_j(const LFO& l) {
    json j;
    j["waveform"] = static_cast<int>(l.waveform);
    j["rate"] = lfo_rate_j(l.rate);
    j["phase"] = l.phase;
    j["symmetry"] = l.symmetry;
    j["retrigger"] = l.retrigger;
    j["fade_in"] = l.fade_in;
    j["delay"] = l.delay;
    if (!l.custom_points.empty()) {
        json arr = json::array();
        for (const auto& p : l.custom_points) arr.push_back(json{p.first, p.second});
        j["custom"] = arr;
    }
    return j;
}

LFO lfo_f(const json& j) {
    LFO l;
    l.waveform = check_enum(j.at("waveform").get<int>(), LFOWaveformType::Custom,
                            "LFOWaveformType", j);
    l.rate = lfo_rate_f(j.at("rate"));
    l.phase = j.at("phase").get<float>();
    l.symmetry = j.at("symmetry").get<float>();
    l.retrigger = j.at("retrigger").get<bool>();
    l.fade_in = j.at("fade_in").get<float>();
    l.delay = j.at("delay").get<float>();
    if (j.contains("custom")) {
        for (const auto& p : j["custom"])
            l.custom_points.emplace_back(p[0].get<float>(), p[1].get<float>());
    }
    return l;
}

json step_seq_j(const StepSequencer& s) {
    return json{{"steps", s.steps}, {"dur", beat_j(s.step_duration)},
                {"smooth", s.smooth}, {"loop", static_cast<int>(s.loop_mode)}};
}

StepSequencer step_seq_f(const json& j) {
    StepSequencer s;
    s.steps = j.at("steps").get<std::vector<float>>();
    s.step_duration = beat_f(j.at("dur"));
    s.smooth = j.at("smooth").get<float>();
    s.loop_mode = check_enum(j.at("loop").get<int>(), LoopMode::OneShot, "LoopMode", j);
    return s;
}

json mapping_curve_j(const MappingCurve& c) {
    json j;
    j["type"] = static_cast<int>(c.type);
    if (!c.custom_points.empty()) {
        json arr = json::array();
        for (const auto& p : c.custom_points) arr.push_back(json{p.first, p.second});
        j["pts"] = arr;
    }
    return j;
}

MappingCurve mapping_curve_f(const json& j) {
    MappingCurve c;
    c.type = check_enum(j.at("type").get<int>(), MappingCurveType::Custom,
                        "MappingCurveType", j);
    if (j.contains("pts")) {
        for (const auto& p : j["pts"])
            c.custom_points.emplace_back(p[0].get<float>(), p[1].get<float>());
    }
    return c;
}

json macro_mapping_j(const MacroMapping& m) {
    return json{{"target", m.target}, {"min", m.min}, {"max", m.max},
                {"curve", mapping_curve_j(m.curve)}};
}

MacroMapping macro_mapping_f(const json& j) {
    return {j.at("target").get<std::string>(), j.at("min").get<float>(),
            j.at("max").get<float>(), mapping_curve_f(j.at("curve"))};
}

json macro_j(const MacroKnob& k) {
    json j;
    j["index"] = k.index;
    j["name"] = k.name;
    j["value"] = k.value;
    j["mappings"] = json::array();
    for (const auto& m : k.mappings) j["mappings"].push_back(macro_mapping_j(m));
    return j;
}

MacroKnob macro_f(const json& j) {
    MacroKnob k;
    k.index = j.at("index").get<std::uint8_t>();
    k.name = j.at("name").get<std::string>();
    k.value = j.at("value").get<float>();
    for (const auto& m : j.at("mappings")) k.mappings.push_back(macro_mapping_f(m));
    return k;
}

json mod_source_j(const ModulationSource& s) {
    json j;
    j["type"] = static_cast<int>(s.type);
    j["index"] = s.index;
    j["cc"] = s.cc_number;
    j["sc_part"] = s.sidechain_part.value;
    return j;
}

ModulationSource mod_source_f(const json& j) {
    ModulationSource s;
    s.type = check_enum(j.at("type").get<int>(), ModulationSourceType::MacroKnob,
                        "ModulationSourceType", j);
    s.index = j.at("index").get<std::uint8_t>();
    s.cc_number = j.at("cc").get<std::uint8_t>();
    s.sidechain_part.value = j.at("sc_part").get<std::uint64_t>();
    return s;
}

json mod_routing_j(const ModulationRouting& r) {
    json j;
    j["source"] = mod_source_j(r.source);
    j["target"] = r.target;
    j["depth"] = r.depth;
    if (r.via) j["via"] = mod_source_j(*r.via);
    return j;
}

ModulationRouting mod_routing_f(const json& j) {
    ModulationRouting r;
    r.source = mod_source_f(j.at("source"));
    r.target = j.at("target").get<std::string>();
    r.depth = j.at("depth").get<float>();
    if (j.contains("via")) r.via = mod_source_f(j.at("via"));
    return r;
}

json mod_matrix_j(const ModulationMatrix& m) {
    json j;
    j["routings"] = json::array();
    for (const auto& r : m.routings) j["routings"].push_back(mod_routing_j(r));
    j["lfos"] = json::array();
    for (const auto& l : m.lfos) j["lfos"].push_back(lfo_j(l));
    j["step_seqs"] = json::array();
    for (const auto& s : m.step_sequencers) j["step_seqs"].push_back(step_seq_j(s));
    j["macros"] = json::array();
    for (const auto& k : m.macro_knobs) j["macros"].push_back(macro_j(k));
    return j;
}

ModulationMatrix mod_matrix_f(const json& j) {
    ModulationMatrix m;
    for (const auto& r : j.at("routings")) m.routings.push_back(mod_routing_f(r));
    for (const auto& l : j.at("lfos")) m.lfos.push_back(lfo_f(l));
    for (const auto& s : j.at("step_seqs")) m.step_sequencers.push_back(step_seq_f(s));
    for (const auto& k : j.at("macros")) m.macro_knobs.push_back(macro_f(k));
    return m;
}

// =============================================================================
// §5 Effects
// =============================================================================

json dist_algo_j(const DistortionAlgorithm& a) {
    json j;
    j["type"] = static_cast<int>(a.type);
    j["bits"] = a.bit_depth;
    j["sr_red"] = a.sample_rate_reduction;
    if (!a.waveshaper_curve.empty()) {
        json arr = json::array();
        for (const auto& p : a.waveshaper_curve) arr.push_back(json{p.first, p.second});
        j["ws_curve"] = arr;
    }
    j["ring_freq"] = a.ring_mod_frequency;
    return j;
}

DistortionAlgorithm dist_algo_f(const json& j) {
    DistortionAlgorithm a;
    a.type = check_enum(j.at("type").get<int>(), DistortionAlgorithmType::RingModulation,
                        "DistortionAlgorithmType", j);
    a.bit_depth = j.at("bits").get<std::uint8_t>();
    a.sample_rate_reduction = j.at("sr_red").get<float>();
    if (j.contains("ws_curve")) {
        for (const auto& p : j["ws_curve"])
            a.waveshaper_curve.emplace_back(p[0].get<float>(), p[1].get<float>());
    }
    a.ring_mod_frequency = j.at("ring_freq").get<float>();
    return a;
}

json dist_fx_j(const DistortionEffect& e) {
    return json{{"algorithm", dist_algo_j(e.algorithm)}, {"drive", e.drive},
                {"tone", e.tone}, {"output", e.output_level}};
}

DistortionEffect dist_fx_f(const json& j) {
    return {dist_algo_f(j.at("algorithm")), j.at("drive").get<float>(),
            j.at("tone").get<float>(), j.at("output").get<float>()};
}

json delay_time_j(const DelayTime& t) {
    return json{{"synced", t.synced}, {"ms", t.ms}, {"div", beat_j(t.division)}};
}

DelayTime delay_time_f(const json& j) {
    return {j.at("synced").get<bool>(), j.at("ms").get<float>(),
            beat_f(j.at("div"))};
}

json delay_fx_j(const DelayEffect& e) {
    json j;
    j["time"] = delay_time_j(e.delay_time);
    j["feedback"] = e.feedback;
    j["stereo"] = static_cast<int>(e.stereo_mode);
    j["offset"] = e.stereo_offset;
    if (e.filter) j["filter"] = filter_j(*e.filter);
    j["mod_rate"] = e.modulation_rate;
    j["mod_depth"] = e.modulation_depth;
    return j;
}

DelayEffect delay_fx_f(const json& j) {
    DelayEffect e;
    e.delay_time = delay_time_f(j.at("time"));
    e.feedback = j.at("feedback").get<float>();
    e.stereo_mode = check_enum(j.at("stereo").get<int>(), StereoDelayMode::PingPong,
                                "StereoDelayMode", j);
    e.stereo_offset = j.at("offset").get<float>();
    if (j.contains("filter")) e.filter = filter_f(j.at("filter"));
    e.modulation_rate = j.at("mod_rate").get<float>();
    e.modulation_depth = j.at("mod_depth").get<float>();
    return e;
}

json reverb_algo_j(const ReverbAlgorithm& a) {
    json j;
    j["type"] = static_cast<int>(a.type);
    if (!a.impulse_path.empty()) j["impulse"] = a.impulse_path;
    j["shimmer_pitch"] = a.shimmer_pitch;
    return j;
}

ReverbAlgorithm reverb_algo_f(const json& j) {
    ReverbAlgorithm a;
    a.type = check_enum(j.at("type").get<int>(), ReverbAlgorithmType::Shimmer,
                        "ReverbAlgorithmType", j);
    if (j.contains("impulse")) a.impulse_path = j["impulse"].get<std::string>();
    a.shimmer_pitch = j.value("shimmer_pitch", 12.0f);
    return a;
}

json reverb_fx_j(const ReverbEffect& e) {
    return json{
        {"algorithm", reverb_algo_j(e.algorithm)}, {"decay", e.decay_time},
        {"pre_delay", e.pre_delay}, {"damping", e.damping},
        {"diffusion", e.diffusion}, {"size", e.size},
        {"er_level", e.early_reflections_level},
        {"eq_lo", e.eq_low_cut}, {"eq_hi", e.eq_high_cut}
    };
}

ReverbEffect reverb_fx_f(const json& j) {
    ReverbEffect e;
    e.algorithm = reverb_algo_f(j.at("algorithm"));
    e.decay_time = j.at("decay").get<float>();
    e.pre_delay = j.at("pre_delay").get<float>();
    e.damping = j.at("damping").get<float>();
    e.diffusion = j.at("diffusion").get<float>();
    e.size = j.at("size").get<float>();
    e.early_reflections_level = j.at("er_level").get<float>();
    e.eq_low_cut = j.at("eq_lo").get<float>();
    e.eq_high_cut = j.at("eq_hi").get<float>();
    return e;
}

json chorus_fx_j(const ChorusEffect& e) {
    return json{{"rate", e.rate}, {"depth", e.depth}, {"voices", e.voices},
                {"feedback", e.feedback}, {"spread", e.stereo_spread}};
}

ChorusEffect chorus_fx_f(const json& j) {
    return {j.at("rate").get<float>(), j.at("depth").get<float>(),
            j.at("voices").get<std::uint8_t>(), j.at("feedback").get<float>(),
            j.at("spread").get<float>()};
}

json phaser_fx_j(const PhaserEffect& e) {
    return json{{"rate", e.rate}, {"depth", e.depth}, {"stages", e.stages},
                {"feedback", e.feedback}, {"center", e.center_frequency}};
}

PhaserEffect phaser_fx_f(const json& j) {
    return {j.at("rate").get<float>(), j.at("depth").get<float>(),
            j.at("stages").get<std::uint8_t>(), j.at("feedback").get<float>(),
            j.at("center").get<float>()};
}

json flanger_fx_j(const FlangerEffect& e) {
    return json{{"rate", e.rate}, {"depth", e.depth},
                {"feedback", e.feedback}, {"manual", e.manual}};
}

FlangerEffect flanger_fx_f(const json& j) {
    return {j.at("rate").get<float>(), j.at("depth").get<float>(),
            j.at("feedback").get<float>(), j.at("manual").get<float>()};
}

json eq_band_j(const EQBand& b) {
    return json{{"freq", b.frequency}, {"gain", b.gain}, {"q", b.q},
                {"type", static_cast<int>(b.band_type)}};
}

EQBand eq_band_f(const json& j) {
    return {j.at("freq").get<float>(), j.at("gain").get<float>(),
            j.at("q").get<float>(),
            check_enum(j.at("type").get<int>(), EQBandType::HighCut, "EQBandType", j)};
}

json eq_fx_j(const EQEffect& e) {
    json arr = json::array();
    for (const auto& b : e.bands) arr.push_back(eq_band_j(b));
    return json{{"bands", arr}};
}

EQEffect eq_fx_f(const json& j) {
    EQEffect e;
    for (const auto& b : j.at("bands")) e.bands.push_back(eq_band_f(b));
    return e;
}

json sidechain_j(const SidechainConfig& s) {
    json j;
    j["source"] = s.source.value;
    if (s.filter) j["filter"] = filter_j(*s.filter);
    return j;
}

SidechainConfig sidechain_f(const json& j) {
    SidechainConfig s;
    s.source.value = j.at("source").get<std::uint64_t>();
    if (j.contains("filter")) s.filter = filter_f(j.at("filter"));
    return s;
}

json comp_fx_j(const CompressorEffect& e) {
    json j;
    j["threshold"] = e.threshold;
    j["ratio"] = e.ratio;
    j["attack"] = e.attack;
    j["release"] = e.release;
    j["knee"] = e.knee;
    j["makeup"] = e.makeup_gain;
    if (e.sidechain) j["sidechain"] = sidechain_j(*e.sidechain);
    return j;
}

CompressorEffect comp_fx_f(const json& j) {
    CompressorEffect e;
    e.threshold = j.at("threshold").get<float>();
    e.ratio = j.at("ratio").get<float>();
    e.attack = j.at("attack").get<float>();
    e.release = j.at("release").get<float>();
    e.knee = j.at("knee").get<float>();
    e.makeup_gain = j.at("makeup").get<float>();
    if (j.contains("sidechain")) e.sidechain = sidechain_f(j.at("sidechain"));
    return e;
}

// EffectParameters variant dispatch
json effect_params_j(const EffectParameters& p) {
    json j;
    std::visit([&](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, DistortionEffect>)  { j["type"] = "distortion"; j["params"] = dist_fx_j(e); }
        else if constexpr (std::is_same_v<T, DelayEffect>)  { j["type"] = "delay";      j["params"] = delay_fx_j(e); }
        else if constexpr (std::is_same_v<T, ReverbEffect>) { j["type"] = "reverb";     j["params"] = reverb_fx_j(e); }
        else if constexpr (std::is_same_v<T, ChorusEffect>) { j["type"] = "chorus";     j["params"] = chorus_fx_j(e); }
        else if constexpr (std::is_same_v<T, PhaserEffect>) { j["type"] = "phaser";     j["params"] = phaser_fx_j(e); }
        else if constexpr (std::is_same_v<T, FlangerEffect>){ j["type"] = "flanger";    j["params"] = flanger_fx_j(e); }
        else if constexpr (std::is_same_v<T, EQEffect>)     { j["type"] = "eq";         j["params"] = eq_fx_j(e); }
        else if constexpr (std::is_same_v<T, CompressorEffect>) { j["type"] = "compressor"; j["params"] = comp_fx_j(e); }
    }, p);
    return j;
}

EffectParameters effect_params_f(const json& j) {
    auto type = j.at("type").get<std::string>();
    const auto& p = j.at("params");
    if (type == "distortion")  return dist_fx_f(p);
    if (type == "delay")       return delay_fx_f(p);
    if (type == "reverb")      return reverb_fx_f(p);
    if (type == "chorus")      return chorus_fx_f(p);
    if (type == "phaser")      return phaser_fx_f(p);
    if (type == "flanger")     return flanger_fx_f(p);
    if (type == "eq")          return eq_fx_f(p);
    if (type == "compressor")  return comp_fx_f(p);
    throw json::other_error::create(604, "Unknown effect type: " + type, &j);
}

json effect_j(const Effect& e) {
    json j;
    j["id"] = e.id.value;
    j["params"] = effect_params_j(e.parameters);
    j["enabled"] = e.enabled;
    j["mix"] = e.mix;
    return j;
}

Effect effect_f(const json& j) {
    Effect e;
    e.id.value = j.at("id").get<std::uint64_t>();
    e.parameters = effect_params_f(j.at("params"));
    e.enabled = j.at("enabled").get<bool>();
    e.mix = j.at("mix").get<float>();
    return e;
}

json effect_chain_j(const EffectChain& c) {
    json j;
    j["effects"] = json::array();
    for (const auto& e : c.effects) j["effects"].push_back(effect_j(e));
    j["bypass"] = c.bypass_all;
    return j;
}

EffectChain effect_chain_f(const json& j) {
    EffectChain c;
    for (const auto& e : j.at("effects")) c.effects.push_back(effect_f(e));
    c.bypass_all = j.at("bypass").get<bool>();
    return c;
}

// =============================================================================
// §2 Sound Source variants
// =============================================================================

// Forward declaration for recursive HybridSource
json sound_source_j(const SoundSourceData& src);
SoundSourceData sound_source_f(const json& j);

json subtractive_j(const SubtractiveSynth& s) {
    json j;
    j["oscillators"] = json::array();
    for (const auto& o : s.oscillators) j["oscillators"].push_back(oscillator_j(o));
    if (s.noise) j["noise"] = noise_j(*s.noise);
    j["osc_mix"] = s.oscillator_mix;
    j["filter"] = filter_j(s.filter);
    if (s.filter_2) j["filter_2"] = filter_j(*s.filter_2);
    j["filter_routing"] = static_cast<int>(s.filter_routing);
    j["amplifier"] = envelope_j(s.amplifier);
    if (s.unison) j["unison"] = unison_j(*s.unison);
    if (s.portamento) j["portamento"] = portamento_j(*s.portamento);
    return j;
}

SubtractiveSynth subtractive_f(const json& j) {
    SubtractiveSynth s;
    for (const auto& o : j.at("oscillators")) s.oscillators.push_back(oscillator_f(o));
    if (j.contains("noise")) s.noise = noise_f(j.at("noise"));
    s.oscillator_mix = j.at("osc_mix").get<std::vector<float>>();
    s.filter = filter_f(j.at("filter"));
    if (j.contains("filter_2")) s.filter_2 = filter_f(j.at("filter_2"));
    s.filter_routing = check_enum(j.at("filter_routing").get<int>(),
        SubtractiveSynth::FilterRouting::Parallel, "FilterRouting", j);
    s.amplifier = envelope_f(j.at("amplifier"));
    if (j.contains("unison")) s.unison = unison_f(j.at("unison"));
    if (j.contains("portamento")) s.portamento = portamento_f(j.at("portamento"));
    return s;
}

json fm_synth_j(const FMSynth& s) {
    json j;
    j["operators"] = json::array();
    for (const auto& op : s.operators) j["operators"].push_back(fm_operator_j(op));
    j["algorithm"] = fm_algorithm_j(s.algorithm);
    j["feedback"] = s.feedback;
    return j;
}

FMSynth fm_synth_f(const json& j) {
    FMSynth s;
    for (const auto& op : j.at("operators")) s.operators.push_back(fm_operator_f(op));
    s.algorithm = fm_algorithm_f(j.at("algorithm"));
    s.feedback = j.at("feedback").get<float>();
    return s;
}

json wavetable_j(const WavetableSynth& s) {
    return json{
        {"wavetable", s.wavetable}, {"position", s.position},
        {"frames", s.frame_count},
        {"interp", static_cast<int>(s.interpolation)},
        {"filter", s.filter ? filter_j(*s.filter) : json(nullptr)},
        {"amplifier", envelope_j(s.amplifier)}
    };
}

WavetableSynth wavetable_f(const json& j) {
    WavetableSynth s;
    s.wavetable = j.at("wavetable").get<std::string>();
    s.position = j.at("position").get<float>();
    s.frame_count = j.at("frames").get<std::uint32_t>();
    s.interpolation = check_enum(j.at("interp").get<int>(),
        WavetableSynth::Interpolation::Spectral, "Interpolation", j);
    if (!j.at("filter").is_null()) s.filter = filter_f(j.at("filter"));
    s.amplifier = envelope_f(j.at("amplifier"));
    return s;
}

json granular_j(const GranularSynth& s) {
    return json{
        {"source", s.source}, {"grain_size", s.grain_size},
        {"density", s.grain_density}, {"position", s.position},
        {"pos_rand", s.position_random}, {"pitch_rand", s.pitch_random},
        {"grain_env", grain_env_j(s.grain_envelope)},
        {"spray", s.spray}, {"spread", s.stereo_spread},
        {"reverse_prob", s.reverse_probability}
    };
}

GranularSynth granular_f(const json& j) {
    GranularSynth s;
    s.source = j.at("source").get<std::string>();
    s.grain_size = j.at("grain_size").get<float>();
    s.grain_density = j.at("density").get<float>();
    s.position = j.at("position").get<float>();
    s.position_random = j.at("pos_rand").get<float>();
    s.pitch_random = j.at("pitch_rand").get<float>();
    s.grain_envelope = grain_env_f(j.at("grain_env"));
    s.spray = j.at("spray").get<float>();
    s.stereo_spread = j.at("spread").get<float>();
    s.reverse_probability = j.at("reverse_prob").get<float>();
    return s;
}

json additive_j(const AdditiveSynth& s) {
    json j;
    j["partial_count"] = s.partial_count;
    j["partials"] = json::array();
    for (const auto& p : s.partials) j["partials"].push_back(partial_def_j(p));
    j["envelope"] = envelope_j(s.global_envelope);
    return j;
}

AdditiveSynth additive_f(const json& j) {
    AdditiveSynth s;
    s.partial_count = j.at("partial_count").get<std::uint16_t>();
    for (const auto& p : j.at("partials")) s.partials.push_back(partial_def_f(p));
    s.global_envelope = envelope_f(j.at("envelope"));
    return s;
}

json phys_source_j(const PhysicalModelSource& s) {
    return json{
        {"model", phys_model_j(s.model)}, {"exciter", exciter_j(s.exciter)},
        {"resonator", resonator_j(s.resonator)}, {"coupling", s.coupling},
        {"damping", s.damping}, {"brightness", s.brightness}
    };
}

PhysicalModelSource phys_source_f(const json& j) {
    return {phys_model_f(j.at("model")), exciter_f(j.at("exciter")),
            resonator_f(j.at("resonator")),
            j.at("coupling").get<float>(), j.at("damping").get<float>(),
            j.at("brightness").get<float>()};
}

json sampler_j(const SamplerSource& s) {
    json j;
    j["library"] = s.library;
    j["preset"] = s.preset;
    j["sample_map"] = sample_map_j(s.sample_map);
    j["mics"] = json::array();
    for (const auto& m : s.microphone_positions) j["mics"].push_back(mic_pos_j(m));
    j["release"] = s.release_samples;
    j["rr_mode"] = static_cast<int>(s.round_robin_mode);
    if (s.envelope_override) j["env_override"] = envelope_j(*s.envelope_override);
    if (s.filter) j["filter"] = filter_j(*s.filter);
    j["tuning"] = s.tuning_offset;
    return j;
}

SamplerSource sampler_f(const json& j) {
    SamplerSource s;
    s.library = j.at("library").get<std::string>();
    s.preset = j.at("preset").get<std::string>();
    s.sample_map = sample_map_f(j.at("sample_map"));
    for (const auto& m : j.at("mics")) s.microphone_positions.push_back(mic_pos_f(m));
    s.release_samples = j.at("release").get<bool>();
    s.round_robin_mode = check_enum(j.at("rr_mode").get<int>(), RoundRobinMode::RoundRobin,
                                     "RoundRobinMode", j);
    if (j.contains("env_override")) s.envelope_override = envelope_f(j.at("env_override"));
    if (j.contains("filter")) s.filter = filter_f(j.at("filter"));
    s.tuning_offset = j.at("tuning").get<float>();
    return s;
}

json layer_routing_j(const LayerRouting& r) {
    json j;
    j["type"] = static_cast<int>(r.type);
    j["mix"] = r.mix_levels;
    if (!r.velocity_ranges.empty()) {
        json arr = json::array();
        for (const auto& v : r.velocity_ranges) arr.push_back(json{v.first, v.second});
        j["vel_ranges"] = arr;
    }
    if (!r.key_ranges.empty()) {
        json arr = json::array();
        for (const auto& k : r.key_ranges) arr.push_back(json{sp_j(k.first), sp_j(k.second)});
        j["key_ranges"] = arr;
    }
    j["xfade_src"] = mod_source_j(r.crossfade_source);
    j["xfade_curve"] = mapping_curve_j(r.crossfade_curve);
    return j;
}

LayerRouting layer_routing_f(const json& j) {
    LayerRouting r;
    r.type = check_enum(j.at("type").get<int>(), LayerRoutingType::Crossfade,
                        "LayerRoutingType", j);
    r.mix_levels = j.at("mix").get<std::vector<float>>();
    if (j.contains("vel_ranges")) {
        for (const auto& v : j["vel_ranges"])
            r.velocity_ranges.emplace_back(v[0].get<std::uint8_t>(), v[1].get<std::uint8_t>());
    }
    if (j.contains("key_ranges")) {
        for (const auto& k : j["key_ranges"])
            r.key_ranges.emplace_back(sp_f(k[0]), sp_f(k[1]));
    }
    r.crossfade_source = mod_source_f(j.at("xfade_src"));
    r.crossfade_curve = mapping_curve_f(j.at("xfade_curve"));
    return r;
}

// SoundSource variant dispatch (recursive for HybridSource)
json sound_source_j(const SoundSourceData& src) {
    json j;
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>)    { j["type"] = "subtractive";    j["data"] = subtractive_j(s); }
        else if constexpr (std::is_same_v<T, FMSynth>)        { j["type"] = "fm";             j["data"] = fm_synth_j(s); }
        else if constexpr (std::is_same_v<T, WavetableSynth>)  { j["type"] = "wavetable";      j["data"] = wavetable_j(s); }
        else if constexpr (std::is_same_v<T, GranularSynth>)   { j["type"] = "granular";       j["data"] = granular_j(s); }
        else if constexpr (std::is_same_v<T, AdditiveSynth>)   { j["type"] = "additive";       j["data"] = additive_j(s); }
        else if constexpr (std::is_same_v<T, PhysicalModelSource>) { j["type"] = "physical_model"; j["data"] = phys_source_j(s); }
        else if constexpr (std::is_same_v<T, SamplerSource>)   { j["type"] = "sampler";        j["data"] = sampler_j(s); }
        else if constexpr (std::is_same_v<T, HybridSource>) {
            j["type"] = "hybrid";
            json layers = json::array();
            for (const auto& layer : s.layers) {
                if (layer) layers.push_back(sound_source_j(*layer));
                else layers.push_back(nullptr);
            }
            j["data"] = json{{"layers", layers}, {"routing", layer_routing_j(s.routing)}};
        }
    }, src.data);
    return j;
}

SoundSourceData sound_source_f(const json& j) {
    SoundSourceData src;
    auto type = j.at("type").get<std::string>();
    const auto& d = j.at("data");

    if (type == "subtractive")     src.data = subtractive_f(d);
    else if (type == "fm")         src.data = fm_synth_f(d);
    else if (type == "wavetable")  src.data = wavetable_f(d);
    else if (type == "granular")   src.data = granular_f(d);
    else if (type == "additive")   src.data = additive_f(d);
    else if (type == "physical_model") src.data = phys_source_f(d);
    else if (type == "sampler")    src.data = sampler_f(d);
    else if (type == "hybrid") {
        HybridSource h;
        for (const auto& layer : d.at("layers")) {
            if (layer.is_null()) continue;  // TI-5: skip null layers
            h.layers.push_back(std::make_unique<SoundSourceData>(sound_source_f(layer)));
        }
        h.routing = layer_routing_f(d.at("routing"));
        src.data = std::move(h);
    }
    else throw json::other_error::create(604, "Unknown source type: " + type, &j);

    return src;
}

// =============================================================================
// §6 Semantic, §7 Automation, §8 Presets, §9 Rendering
// =============================================================================

json semantic_j(const SemanticTimbreDescriptor& s) {
    return json{
        {"brightness", s.brightness}, {"warmth", s.warmth},
        {"roughness", s.roughness},
        {"attack", static_cast<int>(s.attack_character)},
        {"sustain", static_cast<int>(s.sustain_character)},
        {"width", s.width}, {"density", s.density},
        {"movement", s.movement}, {"weight", s.weight},
        {"tags", s.tags},
        {"derivation", static_cast<int>(s.derivation)}
    };
}

SemanticTimbreDescriptor semantic_f(const json& j) {
    SemanticTimbreDescriptor s;
    s.brightness = j.at("brightness").get<float>();
    s.warmth = j.at("warmth").get<float>();
    s.roughness = j.at("roughness").get<float>();
    s.attack_character = check_enum(j.at("attack").get<int>(), AttackCharacter::Explosive,
                                     "AttackCharacter", j);
    s.sustain_character = check_enum(j.at("sustain").get<int>(), SustainCharacter::Noisy,
                                      "SustainCharacter", j);
    s.width = j.at("width").get<float>();
    s.density = j.at("density").get<float>();
    s.movement = j.at("movement").get<float>();
    s.weight = j.at("weight").get<float>();
    s.tags = j.at("tags").get<std::vector<std::string>>();
    s.derivation = check_enum(j.at("derivation").get<int>(), DerivationMode::AudioDerived,
                               "DerivationMode", j);
    return s;
}

json automation_j(const TimbreAutomation& a) {
    json j;
    j["path"] = a.parameter_path;
    j["breakpoints"] = json::array();
    for (const auto& b : a.breakpoints)
        j["breakpoints"].push_back(json{{"time", st_j(b.time)}, {"value", b.value}});
    j["interp"] = static_cast<int>(a.interpolation);
    return j;
}

TimbreAutomation automation_f(const json& j) {
    TimbreAutomation a;
    a.parameter_path = j.at("path").get<std::string>();
    for (const auto& b : j.at("breakpoints"))
        a.breakpoints.push_back({st_f(b.at("time")), b.at("value").get<float>()});
    a.interpolation = check_enum(j.at("interp").get<int>(),
        AutomationInterpolation::Exponential, "AutomationInterpolation", j);
    return a;
}

json morph_j(const PresetMorph& m) {
    return json{
        {"from", m.from_preset.value}, {"to", m.to_preset.value},
        {"start", st_j(m.start)}, {"end", st_j(m.end)},
        {"curve", mapping_curve_j(m.curve)}
    };
}

PresetMorph morph_f(const json& j) {
    PresetMorph m;
    m.from_preset.value = j.at("from").get<std::uint64_t>();
    m.to_preset.value = j.at("to").get<std::uint64_t>();
    m.start = st_f(j.at("start"));
    m.end = st_f(j.at("end"));
    m.curve = mapping_curve_f(j.at("curve"));
    return m;
}

json preset_j(const TimbrePreset& p) {
    json j;
    j["id"] = p.id.value;
    j["name"] = p.name;
    if (p.description) j["desc"] = *p.description;
    j["semantic"] = semantic_j(p.semantic_descriptors);
    j["params"] = p.parameter_state;
    j["tags"] = p.tags;
    return j;
}

TimbrePreset preset_f(const json& j) {
    TimbrePreset p;
    p.id.value = j.at("id").get<std::uint64_t>();
    p.name = j.at("name").get<std::string>();
    if (j.contains("desc")) p.description = j["desc"].get<std::string>();
    p.semantic_descriptors = semantic_f(j.at("semantic"));
    p.parameter_state = j.at("params").get<std::map<std::string, float>>();
    p.tags = j.at("tags").get<std::vector<std::string>>();
    return p;
}

json device_type_j(const DeviceType& d) {
    return json{
        {"tag", static_cast<int>(d.tag)}, {"device", d.device_name},
        {"plugin_fmt", static_cast<int>(d.plugin_format)},
        {"plugin_id", d.plugin_identifier}
    };
}

DeviceType device_type_f(const json& j) {
    DeviceType d;
    d.tag = check_enum(j.at("tag").get<int>(), DeviceTypeTag::Plugin, "DeviceTypeTag", j);
    d.device_name = j.at("device").get<std::string>();
    d.plugin_format = check_enum(j.at("plugin_fmt").get<int>(), PluginFormat::AU,
                                  "PluginFormat", j);
    d.plugin_identifier = j.at("plugin_id").get<std::string>();
    return d;
}

json device_param_j(const DeviceParameter& p) {
    return json{
        {"idx", p.device_index}, {"name", p.parameter_name},
        {"min", p.range_min}, {"max", p.range_max},
        {"curve", mapping_curve_j(p.curve)}
    };
}

DeviceParameter device_param_f(const json& j) {
    DeviceParameter p;
    p.device_index = j.at("idx").get<std::uint8_t>();
    p.parameter_name = j.at("name").get<std::string>();
    p.range_min = j.at("min").get<float>();
    p.range_max = j.at("max").get<float>();
    p.curve = mapping_curve_f(j.at("curve"));
    return p;
}

json render_config_j(const TimbreRenderingConfig& c) {
    json j;
    j["device"] = device_type_j(c.device_type);
    j["param_map"] = json::object();
    for (const auto& [path, param] : c.parameter_map)
        j["param_map"][path] = device_param_j(param);
    if (c.preset_path) j["preset_path"] = *c.preset_path;
    return j;
}

TimbreRenderingConfig render_config_f(const json& j) {
    TimbreRenderingConfig c;
    c.device_type = device_type_f(j.at("device"));
    for (const auto& [path, param] : j.at("param_map").items())
        c.parameter_map[path] = device_param_f(param);
    if (j.contains("preset_path")) c.preset_path = j["preset_path"].get<std::string>();
    return c;
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

json timbre_to_json(const TimbreProfile& profile) {
    json j;
    j["schema_version"] = TIMBRE_IR_SCHEMA_VERSION;
    j["id"] = profile.id.value;
    j["part_id"] = profile.part_id.value;
    j["name"] = profile.name;
    j["source"] = sound_source_j(profile.source);
    j["modulation"] = mod_matrix_j(profile.modulation);
    j["insert_chain"] = effect_chain_j(profile.insert_chain);
    j["semantic"] = semantic_j(profile.semantic_descriptors);
    j["automation"] = json::array();
    for (const auto& a : profile.parameter_automation)
        j["automation"].push_back(automation_j(a));
    j["morphs"] = json::array();
    for (const auto& m : profile.preset_morphs)
        j["morphs"].push_back(morph_j(m));
    j["presets"] = json::array();
    for (const auto& p : profile.presets)
        j["presets"].push_back(preset_j(p));
    j["rendering"] = render_config_j(profile.rendering);
    return j;
}

Result<TimbreProfile> timbre_from_json(const json& j) {
    try {
        int version = j.at("schema_version").get<int>();
        if (version != TIMBRE_IR_SCHEMA_VERSION) {
            return std::unexpected(ErrorCode::FormatError);
        }

        TimbreProfile p;
        p.id.value = j.at("id").get<std::uint64_t>();
        p.part_id.value = j.at("part_id").get<std::uint64_t>();
        p.name = j.at("name").get<std::string>();
        p.source = sound_source_f(j.at("source"));
        p.modulation = mod_matrix_f(j.at("modulation"));
        p.insert_chain = effect_chain_f(j.at("insert_chain"));
        p.semantic_descriptors = semantic_f(j.at("semantic"));
        for (const auto& a : j.at("automation"))
            p.parameter_automation.push_back(automation_f(a));
        for (const auto& m : j.at("morphs"))
            p.preset_morphs.push_back(morph_f(m));
        for (const auto& pr : j.at("presets"))
            p.presets.push_back(preset_f(pr));
        p.rendering = render_config_f(j.at("rendering"));
        return p;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

std::string timbre_to_json_string(const TimbreProfile& profile, int indent) {
    return timbre_to_json(profile).dump(indent);
}

Result<TimbreProfile> timbre_from_json_string(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        return timbre_from_json(j);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

}  // namespace Sunny::Core
