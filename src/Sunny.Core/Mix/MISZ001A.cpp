/**
 * @file MISZ001A.cpp
 * @brief Mix IR serialisation — implementation
 *
 * Component: MISZ001A
 * Domain: MI (Mix IR) | Category: SZ (Serialisation)
 *
 * Tests: TSMI004A
 */

#include "MISZ001A.h"

namespace Sunny::Core {

using json = nlohmann::json;

namespace {

// =============================================================================
// Shared low-level helpers
// =============================================================================

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

// =============================================================================
// SpatialPosition
// =============================================================================

json spatial_j(const SpatialPosition& s) {
    json j;
    j["pan"] = s.pan;
    j["depth"] = s.depth;
    j["elevation"] = s.elevation;
    j["width"] = s.width;
    j["pan_law"] = static_cast<int>(s.pan_law);
    j["spatial_mode"] = static_cast<int>(s.spatial_mode);
    if (s.pan_law == PanLaw::Custom)
        j["custom_center_attenuation"] = s.custom_center_attenuation;
    if (s.spatial_mode == SpatialMode::ObjectBased)
        j["object_id"] = s.object_id;
    return j;
}

SpatialPosition spatial_f(const json& j) {
    SpatialPosition s;
    s.pan = j.at("pan").get<float>();
    s.depth = j.at("depth").get<float>();
    s.elevation = j.at("elevation").get<float>();
    s.width = j.at("width").get<float>();
    s.pan_law = static_cast<PanLaw>(j.at("pan_law").get<int>());
    s.spatial_mode = static_cast<SpatialMode>(j.at("spatial_mode").get<int>());
    if (j.contains("custom_center_attenuation"))
        s.custom_center_attenuation = j["custom_center_attenuation"].get<float>();
    if (j.contains("object_id"))
        s.object_id = j["object_id"].get<std::uint32_t>();
    return s;
}

// =============================================================================
// Fader, RelativeLevel
// =============================================================================

json fader_j(const Fader& f) {
    json j;
    j["level_db"] = f.level_db;
    if (f.relative_level) {
        json rl;
        const auto& ref = f.relative_level->reference;
        rl["ref_type"] = static_cast<int>(ref.type);
        if (ref.type == LevelReferenceType::MasterTarget)
            rl["lufs"] = ref.lufs;
        if (ref.type == LevelReferenceType::Channel) {
            rl["channel_id"] = ref.channel_id.value;
            rl["relationship"] = ref.relationship;
        }
        if (ref.type == LevelReferenceType::Group)
            rl["group_id"] = ref.group_id.value;
        rl["offset_db"] = f.relative_level->offset_db;
        j["relative"] = rl;
    }
    return j;
}

Fader fader_f(const json& j) {
    Fader f;
    f.level_db = j.at("level_db").get<float>();
    if (j.contains("relative")) {
        const auto& rl = j["relative"];
        RelativeLevel rel;
        rel.reference.type = static_cast<LevelReferenceType>(
            rl.at("ref_type").get<int>());
        if (rl.contains("lufs"))
            rel.reference.lufs = rl["lufs"].get<float>();
        if (rl.contains("channel_id"))
            rel.reference.channel_id = ChannelStripId{rl["channel_id"].get<std::uint64_t>()};
        if (rl.contains("relationship"))
            rel.reference.relationship = rl["relationship"].get<std::string>();
        if (rl.contains("group_id"))
            rel.reference.group_id = GroupBusId{rl["group_id"].get<std::uint64_t>()};
        rel.offset_db = rl.at("offset_db").get<float>();
        f.relative_level = rel;
    }
    return f;
}

// =============================================================================
// AuxSendLevel
// =============================================================================

json aux_send_j(const AuxSendLevel& s) {
    return json{
        {"aux_bus_id", s.aux_bus_id.value},
        {"level_db", s.level_db},
        {"pre_fader", s.pre_fader},
        {"enabled", s.enabled}
    };
}

AuxSendLevel aux_send_f(const json& j) {
    return AuxSendLevel{
        AuxBusId{j.at("aux_bus_id").get<std::uint64_t>()},
        j.at("level_db").get<float>(),
        j.at("pre_fader").get<bool>(),
        j.at("enabled").get<bool>()
    };
}

// =============================================================================
// DynamicEQConfig
// =============================================================================

json dynamic_eq_j(const DynamicEQConfig& d) {
    return json{
        {"threshold", d.threshold},
        {"ratio", d.ratio},
        {"attack", d.attack},
        {"release", d.release}
    };
}

DynamicEQConfig dynamic_eq_f(const json& j) {
    return DynamicEQConfig{
        j.at("threshold").get<float>(),
        j.at("ratio").get<float>(),
        j.at("attack").get<float>(),
        j.at("release").get<float>()
    };
}

// =============================================================================
// MixEQBand, MixEQ
// =============================================================================

json eq_band_j(const MixEQBand& b) {
    json j;
    j["frequency"] = b.frequency;
    j["gain"] = b.gain;
    j["q"] = b.q;
    j["band_type"] = static_cast<int>(b.band_type);
    if (b.dynamic) j["dynamic"] = dynamic_eq_j(*b.dynamic);
    return j;
}

MixEQBand eq_band_f(const json& j) {
    MixEQBand b;
    b.frequency = j.at("frequency").get<float>();
    b.gain = j.at("gain").get<float>();
    b.q = j.at("q").get<float>();
    b.band_type = static_cast<MixEQBandType>(j.at("band_type").get<int>());
    if (j.contains("dynamic")) b.dynamic = dynamic_eq_f(j["dynamic"]);
    return b;
}

json eq_j(const MixEQ& eq) {
    json j;
    json bands_arr = json::array();
    for (const auto& b : eq.bands) bands_arr.push_back(eq_band_j(b));
    j["bands"] = bands_arr;
    j["linear_phase"] = eq.linear_phase;
    j["auto_gain"] = eq.auto_gain;
    return j;
}

MixEQ eq_f(const json& j) {
    MixEQ eq;
    for (const auto& b : j.at("bands")) eq.bands.push_back(eq_band_f(b));
    eq.linear_phase = j.at("linear_phase").get<bool>();
    eq.auto_gain = j.at("auto_gain").get<bool>();
    return eq;
}

// =============================================================================
// SidechainConfig
// =============================================================================

json sidechain_j(const MixSidechainConfig& sc) {
    json j;
    j["source"] = static_cast<int>(sc.source);
    if (sc.source == SidechainSourceType::ExternalChannel)
        j["channel_id"] = sc.channel_id.value;
    if (sc.source == SidechainSourceType::ExternalBus)
        j["bus_id"] = sc.bus_id.value;
    if (sc.filter) {
        j["filter"] = json{
            {"filter_type", static_cast<int>(sc.filter->filter_type)},
            {"frequency", sc.filter->frequency},
            {"q", sc.filter->q}
        };
    }
    return j;
}

MixSidechainConfig sidechain_f(const json& j) {
    MixSidechainConfig sc;
    sc.source = static_cast<SidechainSourceType>(j.at("source").get<int>());
    if (j.contains("channel_id"))
        sc.channel_id = ChannelStripId{j["channel_id"].get<std::uint64_t>()};
    if (j.contains("bus_id"))
        sc.bus_id = GroupBusId{j["bus_id"].get<std::uint64_t>()};
    if (j.contains("filter")) {
        SidechainFilter f;
        f.filter_type = static_cast<MixEQBandType>(
            j["filter"].at("filter_type").get<int>());
        f.frequency = j["filter"].at("frequency").get<float>();
        f.q = j["filter"].at("q").get<float>();
        sc.filter = f;
    }
    return sc;
}

// =============================================================================
// MixCompressor, MixGate, MixLimiter, MixMultibandDynamics
// =============================================================================

json compressor_j(const MixCompressor& c) {
    return json{
        {"threshold", c.threshold}, {"ratio", c.ratio},
        {"attack", c.attack}, {"release", c.release},
        {"knee", c.knee}, {"makeup_gain", c.makeup_gain},
        {"detection", static_cast<int>(c.detection)},
        {"topology", static_cast<int>(c.topology)},
        {"sidechain", sidechain_j(c.sidechain)},
        {"stereo_link", c.stereo_link}
    };
}

MixCompressor compressor_f(const json& j) {
    MixCompressor c;
    c.threshold = j.at("threshold").get<float>();
    c.ratio = j.at("ratio").get<float>();
    c.attack = j.at("attack").get<float>();
    c.release = j.at("release").get<float>();
    c.knee = j.at("knee").get<float>();
    c.makeup_gain = j.at("makeup_gain").get<float>();
    c.detection = static_cast<DetectionMode>(j.at("detection").get<int>());
    c.topology = static_cast<CompressorTopology>(j.at("topology").get<int>());
    c.sidechain = sidechain_f(j.at("sidechain"));
    c.stereo_link = j.at("stereo_link").get<float>();
    return c;
}

json gate_j(const MixGate& g) {
    return json{
        {"threshold", g.threshold}, {"ratio", g.ratio},
        {"attack", g.attack}, {"hold", g.hold}, {"release", g.release},
        {"range", g.range},
        {"sidechain", sidechain_j(g.sidechain)}
    };
}

MixGate gate_f(const json& j) {
    MixGate g;
    g.threshold = j.at("threshold").get<float>();
    g.ratio = j.at("ratio").get<float>();
    g.attack = j.at("attack").get<float>();
    g.hold = j.at("hold").get<float>();
    g.release = j.at("release").get<float>();
    g.range = j.at("range").get<float>();
    g.sidechain = sidechain_f(j.at("sidechain"));
    return g;
}

json limiter_j(const MixLimiter& l) {
    return json{
        {"ceiling", l.ceiling}, {"release", l.release},
        {"lookahead", l.lookahead},
        {"algorithm", static_cast<int>(l.algorithm)}
    };
}

MixLimiter limiter_f(const json& j) {
    MixLimiter l;
    l.ceiling = j.at("ceiling").get<float>();
    l.release = j.at("release").get<float>();
    l.lookahead = j.at("lookahead").get<float>();
    l.algorithm = static_cast<LimiterAlgorithm>(j.at("algorithm").get<int>());
    return l;
}

json mb_band_j(const MultibandDynamicsBand& b) {
    json j;
    if (b.compressor) j["compressor"] = compressor_j(*b.compressor);
    if (b.expander) j["expander"] = gate_j(*b.expander);
    j["gain"] = b.gain;
    j["solo"] = b.solo;
    return j;
}

MultibandDynamicsBand mb_band_f(const json& j) {
    MultibandDynamicsBand b;
    if (j.contains("compressor")) b.compressor = compressor_f(j["compressor"]);
    if (j.contains("expander")) b.expander = gate_f(j["expander"]);
    b.gain = j.at("gain").get<float>();
    b.solo = j.at("solo").get<bool>();
    return b;
}

json multiband_j(const MixMultibandDynamics& m) {
    json j;
    j["crossover_frequencies"] = m.crossover_frequencies;
    json bands_arr = json::array();
    for (const auto& b : m.bands) bands_arr.push_back(mb_band_j(b));
    j["bands"] = bands_arr;
    j["crossover_slope"] = static_cast<int>(m.crossover_slope);
    return j;
}

MixMultibandDynamics multiband_f(const json& j) {
    MixMultibandDynamics m;
    for (const auto& f : j.at("crossover_frequencies"))
        m.crossover_frequencies.push_back(f.get<float>());
    for (const auto& b : j.at("bands")) m.bands.push_back(mb_band_f(b));
    m.crossover_slope = static_cast<CrossoverSlope>(
        j.at("crossover_slope").get<int>());
    return m;
}

// =============================================================================
// Saturation
// =============================================================================

json saturation_j(const MixSaturation& s) {
    json j;
    j["type"] = static_cast<int>(s.algorithm.type);
    j["tape_speed"] = static_cast<int>(s.algorithm.tape_speed);
    j["tape_bias"] = s.algorithm.tape_bias;
    j["tube_model"] = s.algorithm.tube_model;
    j["console_type"] = static_cast<int>(s.algorithm.console_type);
    j["drive"] = s.drive;
    j["mix"] = s.mix;
    j["output_level"] = s.output_level;
    return j;
}

MixSaturation saturation_f(const json& j) {
    MixSaturation s;
    s.algorithm.type = static_cast<SaturationTypeTag>(j.at("type").get<int>());
    s.algorithm.tape_speed = static_cast<TapeSpeed>(j.at("tape_speed").get<int>());
    s.algorithm.tape_bias = j.at("tape_bias").get<float>();
    s.algorithm.tube_model = j.at("tube_model").get<std::string>();
    s.algorithm.console_type = static_cast<ConsoleType>(
        j.at("console_type").get<int>());
    s.drive = j.at("drive").get<float>();
    s.mix = j.at("mix").get<float>();
    s.output_level = j.at("output_level").get<float>();
    return s;
}

// =============================================================================
// StereoProcessor
// =============================================================================

json stereo_j(const MixStereoProcessor& s) {
    json j;
    j["width"] = s.width;
    j["mid_side_balance"] = s.mid_side_balance;
    if (s.mono_below) j["mono_below"] = *s.mono_below;
    return j;
}

MixStereoProcessor stereo_f(const json& j) {
    MixStereoProcessor s;
    s.width = j.at("width").get<float>();
    s.mid_side_balance = j.at("mid_side_balance").get<float>();
    if (j.contains("mono_below")) s.mono_below = j["mono_below"].get<float>();
    return s;
}

// =============================================================================
// MixEffect, MixEffectChain
// =============================================================================

json effect_j(const MixEffect& fx) {
    json j;
    j["id"] = fx.id.value;
    j["enabled"] = fx.enabled;

    std::visit([&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, MixEQ>) {
            j["type"] = "eq";
            j["params"] = eq_j(p);
        } else if constexpr (std::is_same_v<T, MixCompressor>) {
            j["type"] = "compressor";
            j["params"] = compressor_j(p);
        } else if constexpr (std::is_same_v<T, MixGate>) {
            j["type"] = "gate";
            j["params"] = gate_j(p);
        } else if constexpr (std::is_same_v<T, MixLimiter>) {
            j["type"] = "limiter";
            j["params"] = limiter_j(p);
        } else if constexpr (std::is_same_v<T, MixMultibandDynamics>) {
            j["type"] = "multiband";
            j["params"] = multiband_j(p);
        } else if constexpr (std::is_same_v<T, MixSaturation>) {
            j["type"] = "saturation";
            j["params"] = saturation_j(p);
        } else if constexpr (std::is_same_v<T, MixStereoProcessor>) {
            j["type"] = "stereo";
            j["params"] = stereo_j(p);
        }
    }, fx.parameters);

    return j;
}

MixEffect effect_f(const json& j) {
    MixEffect fx;
    fx.id = MixEffectId{j.at("id").get<std::uint64_t>()};
    fx.enabled = j.at("enabled").get<bool>();

    std::string type = j.at("type").get<std::string>();
    const auto& params = j.at("params");
    if (type == "eq") fx.parameters = eq_f(params);
    else if (type == "compressor") fx.parameters = compressor_f(params);
    else if (type == "gate") fx.parameters = gate_f(params);
    else if (type == "limiter") fx.parameters = limiter_f(params);
    else if (type == "multiband") fx.parameters = multiband_f(params);
    else if (type == "saturation") fx.parameters = saturation_f(params);
    else if (type == "stereo") fx.parameters = stereo_f(params);

    return fx;
}

json chain_j(const MixEffectChain& c) {
    json arr = json::array();
    for (const auto& fx : c.effects) arr.push_back(effect_j(fx));
    return arr;
}

MixEffectChain chain_f(const json& j) {
    MixEffectChain c;
    for (const auto& fx : j) c.effects.push_back(effect_f(fx));
    return c;
}

// =============================================================================
// ChannelIntent
// =============================================================================

json frequency_alloc_j(const FrequencyAllocation& fa) {
    json j;
    j["fundamental_low"] = fa.fundamental_low;
    j["fundamental_high"] = fa.fundamental_high;
    j["presence_low"] = fa.presence_low;
    j["presence_high"] = fa.presence_high;
    if (fa.avoid_low) j["avoid_low"] = *fa.avoid_low;
    if (fa.avoid_high) j["avoid_high"] = *fa.avoid_high;
    return j;
}

FrequencyAllocation frequency_alloc_f(const json& j) {
    FrequencyAllocation fa;
    fa.fundamental_low = j.at("fundamental_low").get<float>();
    fa.fundamental_high = j.at("fundamental_high").get<float>();
    fa.presence_low = j.at("presence_low").get<float>();
    fa.presence_high = j.at("presence_high").get<float>();
    if (j.contains("avoid_low")) fa.avoid_low = j["avoid_low"].get<float>();
    if (j.contains("avoid_high")) fa.avoid_high = j["avoid_high"].get<float>();
    return fa;
}

json intent_j(const ChannelIntent& ci) {
    json j;
    j["role"] = static_cast<int>(ci.role_in_mix);
    j["frequency_space"] = frequency_alloc_j(ci.frequency_space);
    j["depth"] = static_cast<int>(ci.depth_position);
    json rationale = json::array();
    for (const auto& r : ci.processing_rationale) {
        rationale.push_back(json{
            {"effect_id", r.effect_id.value},
            {"purpose", r.purpose}
        });
    }
    j["rationale"] = rationale;
    return j;
}

ChannelIntent intent_f(const json& j) {
    ChannelIntent ci;
    ci.role_in_mix = static_cast<MixRole>(j.at("role").get<int>());
    ci.frequency_space = frequency_alloc_f(j.at("frequency_space"));
    ci.depth_position = static_cast<DepthPosition>(j.at("depth").get<int>());
    if (j.contains("rationale")) {
        for (const auto& r : j["rationale"]) {
            ci.processing_rationale.push_back({
                MixEffectId{r.at("effect_id").get<std::uint64_t>()},
                r.at("purpose").get<std::string>()
            });
        }
    }
    return ci;
}

// =============================================================================
// ChannelStrip
// =============================================================================

json channel_j(const ChannelStrip& ch) {
    json j;
    j["id"] = ch.id.value;
    j["part_id"] = ch.part_id.value;
    j["input_trim"] = ch.input_trim;
    j["polarity_invert"] = ch.polarity_invert;
    j["insert_chain"] = chain_j(ch.insert_chain);
    j["fader"] = fader_j(ch.fader);
    j["spatial"] = spatial_j(ch.spatial);
    j["mute"] = ch.mute;
    j["solo"] = ch.solo;
    json sends_arr = json::array();
    for (const auto& s : ch.sends) sends_arr.push_back(aux_send_j(s));
    j["sends"] = sends_arr;
    if (ch.group_assignment) j["group"] = ch.group_assignment->value;
    if (ch.intent) j["intent"] = intent_j(*ch.intent);
    return j;
}

ChannelStrip channel_f(const json& j) {
    ChannelStrip ch;
    ch.id = ChannelStripId{j.at("id").get<std::uint64_t>()};
    ch.part_id = PartId{j.at("part_id").get<std::uint64_t>()};
    ch.input_trim = j.at("input_trim").get<float>();
    ch.polarity_invert = j.at("polarity_invert").get<bool>();
    ch.insert_chain = chain_f(j.at("insert_chain"));
    ch.fader = fader_f(j.at("fader"));
    ch.spatial = spatial_f(j.at("spatial"));
    ch.mute = j.at("mute").get<bool>();
    ch.solo = j.at("solo").get<bool>();
    for (const auto& s : j.at("sends")) ch.sends.push_back(aux_send_f(s));
    if (j.contains("group"))
        ch.group_assignment = GroupBusId{j["group"].get<std::uint64_t>()};
    if (j.contains("intent"))
        ch.intent = intent_f(j["intent"]);
    return ch;
}

// =============================================================================
// GroupBus
// =============================================================================

json group_output_j(const GroupOutput& go) {
    json j;
    j["type"] = static_cast<int>(go.type);
    if (go.type == GroupOutputType::Group)
        j["parent_group_id"] = go.parent_group_id.value;
    return j;
}

GroupOutput group_output_f(const json& j) {
    GroupOutput go;
    go.type = static_cast<GroupOutputType>(j.at("type").get<int>());
    if (j.contains("parent_group_id"))
        go.parent_group_id = GroupBusId{j["parent_group_id"].get<std::uint64_t>()};
    return go;
}

json group_bus_j(const GroupBus& g) {
    json j;
    j["id"] = g.id.value;
    j["name"] = g.name;
    json members = json::array();
    for (const auto& m : g.member_channels) members.push_back(m.value);
    j["member_channels"] = members;
    json groups = json::array();
    for (const auto& m : g.member_groups) groups.push_back(m.value);
    j["member_groups"] = groups;
    j["insert_chain"] = chain_j(g.insert_chain);
    j["fader"] = fader_j(g.fader);
    j["spatial"] = spatial_j(g.spatial);
    j["mute"] = g.mute;
    j["solo"] = g.solo;
    json sends_arr = json::array();
    for (const auto& s : g.sends) sends_arr.push_back(aux_send_j(s));
    j["sends"] = sends_arr;
    j["output"] = group_output_j(g.output);
    if (g.intent) {
        j["intent"] = json{
            {"function", g.intent->function},
            {"internal_balance", g.intent->internal_balance_description}
        };
    }
    return j;
}

GroupBus group_bus_f(const json& j) {
    GroupBus g;
    g.id = GroupBusId{j.at("id").get<std::uint64_t>()};
    g.name = j.at("name").get<std::string>();
    for (const auto& m : j.at("member_channels"))
        g.member_channels.push_back(ChannelStripId{m.get<std::uint64_t>()});
    for (const auto& m : j.at("member_groups"))
        g.member_groups.push_back(GroupBusId{m.get<std::uint64_t>()});
    g.insert_chain = chain_f(j.at("insert_chain"));
    g.fader = fader_f(j.at("fader"));
    g.spatial = spatial_f(j.at("spatial"));
    g.mute = j.at("mute").get<bool>();
    g.solo = j.at("solo").get<bool>();
    for (const auto& s : j.at("sends")) g.sends.push_back(aux_send_f(s));
    g.output = group_output_f(j.at("output"));
    if (j.contains("intent")) {
        g.intent = GroupIntent{
            j["intent"].at("function").get<std::string>(),
            j["intent"].at("internal_balance").get<std::string>()
        };
    }
    return g;
}

// =============================================================================
// AuxBus
// =============================================================================

json aux_output_j(const AuxOutput& ao) {
    json j;
    j["type"] = static_cast<int>(ao.type);
    if (ao.type == AuxOutputType::Group)
        j["group_id"] = ao.group_id.value;
    return j;
}

AuxOutput aux_output_f(const json& j) {
    AuxOutput ao;
    ao.type = static_cast<AuxOutputType>(j.at("type").get<int>());
    if (j.contains("group_id"))
        ao.group_id = GroupBusId{j["group_id"].get<std::uint64_t>()};
    return ao;
}

json aux_bus_j(const AuxBus& a) {
    json j;
    j["id"] = a.id.value;
    j["name"] = a.name;
    j["effect_chain"] = chain_j(a.effect_chain);
    j["return_level"] = a.return_level;
    j["return_spatial"] = spatial_j(a.return_spatial);
    j["output"] = aux_output_j(a.output);
    if (a.intent) j["intent"] = *a.intent;
    return j;
}

AuxBus aux_bus_f(const json& j) {
    AuxBus a;
    a.id = AuxBusId{j.at("id").get<std::uint64_t>()};
    a.name = j.at("name").get<std::string>();
    a.effect_chain = chain_f(j.at("effect_chain"));
    a.return_level = j.at("return_level").get<float>();
    a.return_spatial = spatial_f(j.at("return_spatial"));
    a.output = aux_output_f(j.at("output"));
    if (j.contains("intent"))
        a.intent = j["intent"].get<std::string>();
    return a;
}

// =============================================================================
// MasterBus
// =============================================================================

json loudness_target_j(const LoudnessTarget& lt) {
    return json{
        {"integrated_lufs", lt.integrated_lufs},
        {"true_peak_dbfs", lt.true_peak_dbfs},
        {"loudness_range_lu", lt.loudness_range_lu.has_value() ? json(*lt.loudness_range_lu) : json(nullptr)},
        {"standard", static_cast<int>(lt.standard)}
    };
}

LoudnessTarget loudness_target_f(const json& j) {
    LoudnessTarget lt;
    lt.integrated_lufs = j.at("integrated_lufs").get<float>();
    lt.true_peak_dbfs = j.at("true_peak_dbfs").get<float>();
    if (!j.at("loudness_range_lu").is_null())
        lt.loudness_range_lu = j["loudness_range_lu"].get<float>();
    lt.standard = static_cast<LoudnessStandard>(j.at("standard").get<int>());
    return lt;
}

json dithering_j(const DitheringConfig& d) {
    return json{
        {"target_bit_depth", d.target_bit_depth},
        {"algorithm", static_cast<int>(d.algorithm)},
        {"noise_shaping_order", d.noise_shaping_order},
        {"pow_r_level", d.pow_r_level},
        {"auto_blank", d.auto_blank}
    };
}

DitheringConfig dithering_f(const json& j) {
    DitheringConfig d;
    d.target_bit_depth = j.at("target_bit_depth").get<std::uint8_t>();
    d.algorithm = static_cast<DitherAlgorithm>(j.at("algorithm").get<int>());
    d.noise_shaping_order = j.at("noise_shaping_order").get<std::uint8_t>();
    d.pow_r_level = j.at("pow_r_level").get<std::uint8_t>();
    d.auto_blank = j.at("auto_blank").get<bool>();
    return d;
}

json metering_j(const MeteringConfig& m) {
    return json{
        {"lufs", m.lufs}, {"true_peak", m.true_peak},
        {"rms", m.rms}, {"correlation", m.correlation},
        {"spectrum", m.spectrum}, {"dynamics", m.dynamics}
    };
}

MeteringConfig metering_f(const json& j) {
    return MeteringConfig{
        j.at("lufs").get<bool>(),
        j.at("true_peak").get<bool>(),
        j.at("rms").get<bool>(),
        j.at("correlation").get<bool>(),
        j.at("spectrum").get<bool>(),
        j.at("dynamics").get<bool>()
    };
}

json master_bus_j(const MasterBus& mb) {
    json j;
    j["insert_chain"] = chain_j(mb.insert_chain);
    j["fader"] = fader_j(mb.fader);
    j["output_format"] = static_cast<int>(mb.output_format);
    if (mb.atmos_config) {
        j["atmos"] = json{
            {"bed_channels", mb.atmos_config->bed_channels},
            {"object_count", mb.atmos_config->object_count}
        };
    }
    if (mb.dithering) j["dithering"] = dithering_j(*mb.dithering);
    if (mb.target_loudness) j["target_loudness"] = loudness_target_j(*mb.target_loudness);
    j["metering"] = metering_j(mb.metering);
    return j;
}

MasterBus master_bus_f(const json& j) {
    MasterBus mb;
    mb.insert_chain = chain_f(j.at("insert_chain"));
    mb.fader = fader_f(j.at("fader"));
    mb.output_format = static_cast<OutputFormat>(j.at("output_format").get<int>());
    if (j.contains("atmos")) {
        mb.atmos_config = AtmosConfig{
            j["atmos"].at("bed_channels").get<std::uint8_t>(),
            j["atmos"].at("object_count").get<std::uint8_t>()
        };
    }
    if (j.contains("dithering")) mb.dithering = dithering_f(j["dithering"]);
    if (j.contains("target_loudness"))
        mb.target_loudness = loudness_target_f(j["target_loudness"]);
    mb.metering = metering_f(j.at("metering"));
    return mb;
}

// =============================================================================
// ReferenceProfile
// =============================================================================

json band_energy_j(const BandEnergy& be) {
    return json{{"low", be.low_hz}, {"high", be.high_hz}, {"energy", be.energy_db}};
}

BandEnergy band_energy_f(const json& j) {
    return BandEnergy{j.at("low").get<float>(), j.at("high").get<float>(),
                      j.at("energy").get<float>()};
}

json spectral_profile_j(const SpectralProfile& sp) {
    json j;
    json spectrum = json::array();
    for (const auto& [hz, db] : sp.average_spectrum)
        spectrum.push_back(json{hz, db});
    j["average_spectrum"] = spectrum;
    j["spectral_centroid"] = sp.spectral_centroid;
    j["spectral_tilt"] = sp.spectral_tilt;
    json bands = json::array();
    for (const auto& b : sp.band_energies) bands.push_back(band_energy_j(b));
    j["band_energies"] = bands;
    return j;
}

SpectralProfile spectral_profile_f(const json& j) {
    SpectralProfile sp;
    for (const auto& pair : j.at("average_spectrum"))
        sp.average_spectrum.emplace_back(pair[0].get<float>(), pair[1].get<float>());
    sp.spectral_centroid = j.at("spectral_centroid").get<float>();
    sp.spectral_tilt = j.at("spectral_tilt").get<float>();
    for (const auto& b : j.at("band_energies"))
        sp.band_energies.push_back(band_energy_f(b));
    return sp;
}

json dynamic_profile_j(const DynamicProfile& dp) {
    return json{
        {"crest_factor", dp.crest_factor},
        {"loudness_range", dp.loudness_range},
        {"dynamic_range_dr", dp.dynamic_range_dr},
        {"plr", dp.peak_to_loudness_ratio}
    };
}

DynamicProfile dynamic_profile_f(const json& j) {
    return DynamicProfile{
        j.at("crest_factor").get<float>(),
        j.at("loudness_range").get<float>(),
        j.at("dynamic_range_dr").get<float>(),
        j.at("plr").get<float>()
    };
}

json loudness_profile_j(const LoudnessProfile& lp) {
    return json{
        {"integrated", lp.integrated},
        {"short_term_max", lp.short_term_max},
        {"momentary_max", lp.momentary_max},
        {"true_peak", lp.true_peak}
    };
}

LoudnessProfile loudness_profile_f(const json& j) {
    return LoudnessProfile{
        j.at("integrated").get<float>(),
        j.at("short_term_max").get<float>(),
        j.at("momentary_max").get<float>(),
        j.at("true_peak").get<float>()
    };
}

json spatial_profile_j(const SpatialProfile& sp) {
    return json{
        {"avg_correlation", sp.average_correlation},
        {"avg_width", sp.average_width},
        {"ms_ratio", sp.mid_side_ratio},
        {"lf_mono_coherence", sp.low_frequency_mono_coherence}
    };
}

SpatialProfile spatial_profile_f(const json& j) {
    return SpatialProfile{
        j.at("avg_correlation").get<float>(),
        j.at("avg_width").get<float>(),
        j.at("ms_ratio").get<float>(),
        j.at("lf_mono_coherence").get<float>()
    };
}

json reference_j(const ReferenceProfile& rp) {
    json j;
    j["id"] = rp.id.value;
    j["name"] = rp.name;
    if (rp.source) j["source"] = *rp.source;
    j["spectral"] = spectral_profile_j(rp.spectral_profile);
    j["dynamic"] = dynamic_profile_j(rp.dynamic_profile);
    j["loudness"] = loudness_profile_j(rp.loudness_profile);
    j["spatial"] = spatial_profile_j(rp.spatial_profile);
    json tbc = json::array();
    for (const auto& [hz, db] : rp.tonal_balance_curve)
        tbc.push_back(json{hz, db});
    j["tonal_balance_curve"] = tbc;
    return j;
}

ReferenceProfile reference_f(const json& j) {
    ReferenceProfile rp;
    rp.id = ReferenceProfileId{j.at("id").get<std::uint64_t>()};
    rp.name = j.at("name").get<std::string>();
    if (j.contains("source")) rp.source = j["source"].get<std::string>();
    rp.spectral_profile = spectral_profile_f(j.at("spectral"));
    rp.dynamic_profile = dynamic_profile_f(j.at("dynamic"));
    rp.loudness_profile = loudness_profile_f(j.at("loudness"));
    rp.spatial_profile = spatial_profile_f(j.at("spatial"));
    for (const auto& pair : j.at("tonal_balance_curve"))
        rp.tonal_balance_curve.emplace_back(pair[0].get<float>(), pair[1].get<float>());
    return rp;
}

// =============================================================================
// MixAutomation
// =============================================================================

json automation_bp_j(const MixAutomationBreakpoint& bp) {
    return json{{"time", st_j(bp.time)}, {"value", bp.value}};
}

MixAutomationBreakpoint automation_bp_f(const json& j) {
    return MixAutomationBreakpoint{st_f(j.at("time")), j.at("value").get<float>()};
}

json automation_j(const MixAutomation& a) {
    json j;
    j["target"] = a.target;
    json bps = json::array();
    for (const auto& bp : a.breakpoints) bps.push_back(automation_bp_j(bp));
    j["breakpoints"] = bps;
    j["interpolation"] = static_cast<int>(a.interpolation);
    if (a.intent) j["intent"] = *a.intent;
    return j;
}

MixAutomation automation_f(const json& j) {
    MixAutomation a;
    a.target = j.at("target").get<std::string>();
    for (const auto& bp : j.at("breakpoints"))
        a.breakpoints.push_back(automation_bp_f(bp));
    a.interpolation = static_cast<InterpolationMode>(
        j.at("interpolation").get<int>());
    if (j.contains("intent")) a.intent = j["intent"].get<std::string>();
    return a;
}

// =============================================================================
// MixAnnotation
// =============================================================================

json annotation_j(const MixAnnotation& a) {
    return json{{"scope", a.scope}, {"content", a.content}};
}

MixAnnotation annotation_f(const json& j) {
    return MixAnnotation{j.at("scope").get<std::string>(),
                         j.at("content").get<std::string>()};
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

nlohmann::json mix_to_json(const MixGraph& graph) {
    json j;
    j["schema_version"] = MIX_IR_SCHEMA_VERSION;
    j["id"] = graph.id.value;

    json channels_arr = json::array();
    for (const auto& ch : graph.channels) channels_arr.push_back(channel_j(ch));
    j["channels"] = channels_arr;

    json groups_arr = json::array();
    for (const auto& g : graph.group_buses) groups_arr.push_back(group_bus_j(g));
    j["group_buses"] = groups_arr;

    json aux_arr = json::array();
    for (const auto& a : graph.aux_buses) aux_arr.push_back(aux_bus_j(a));
    j["aux_buses"] = aux_arr;

    j["master_bus"] = master_bus_j(graph.master_bus);

    json annotations = json::array();
    for (const auto& a : graph.mix_annotations) annotations.push_back(annotation_j(a));
    j["annotations"] = annotations;

    json refs = json::array();
    for (const auto& r : graph.reference_profiles) refs.push_back(reference_j(r));
    j["reference_profiles"] = refs;

    json autos = json::array();
    for (const auto& a : graph.automation) autos.push_back(automation_j(a));
    j["automation"] = autos;

    j["output_format"] = static_cast<int>(graph.output_format);
    j["max_group_nesting_depth"] = graph.max_group_nesting_depth;

    return j;
}

Result<MixGraph> mix_from_json(const nlohmann::json& j) {
    try {
        int version = j.at("schema_version").get<int>();
        if (version != MIX_IR_SCHEMA_VERSION) {
            return std::unexpected(ErrorCode::FormatError);
        }

        MixGraph graph;
        graph.id = MixGraphId{j.at("id").get<std::uint64_t>()};

        for (const auto& ch : j.at("channels"))
            graph.channels.push_back(channel_f(ch));
        for (const auto& g : j.at("group_buses"))
            graph.group_buses.push_back(group_bus_f(g));
        for (const auto& a : j.at("aux_buses"))
            graph.aux_buses.push_back(aux_bus_f(a));

        graph.master_bus = master_bus_f(j.at("master_bus"));

        if (j.contains("annotations")) {
            for (const auto& a : j["annotations"])
                graph.mix_annotations.push_back(annotation_f(a));
        }
        if (j.contains("reference_profiles")) {
            for (const auto& r : j["reference_profiles"])
                graph.reference_profiles.push_back(reference_f(r));
        }
        if (j.contains("automation")) {
            for (const auto& a : j["automation"])
                graph.automation.push_back(automation_f(a));
        }

        graph.output_format = static_cast<OutputFormat>(
            j.at("output_format").get<int>());
        graph.max_group_nesting_depth = j.at("max_group_nesting_depth").get<std::uint8_t>();

        return graph;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

std::string mix_to_json_string(const MixGraph& graph, int indent) {
    return mix_to_json(graph).dump(indent);
}

Result<MixGraph> mix_from_json_string(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        return mix_from_json(j);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

}  // namespace Sunny::Core
