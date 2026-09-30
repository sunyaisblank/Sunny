/**
 * @file serialization.cpp
 * @brief Mix IR serialisation — implementation
 *
 *
 */

#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/score/serialization_primitives.hpp>

namespace sunny::core {

using json = nlohmann::json;

namespace {

// =============================================================================
// Shared low-level helpers
// =============================================================================

template <typename EnumT>
EnumT check_enum(const json& encoded, EnumT maximum, const char* name, const json& context) {
    const auto value = detail::checked_integer<int>(encoded, name);
    if (value < 0 || value > static_cast<int>(maximum))
        throw json::other_error::create(604, std::string(name) + " out of range", &context);
    return static_cast<EnumT>(value);
}

// Shared scheme and guards live in; these aliases keep call sites terse
json st_j(const ScoreTime& t) {
    return score_time_to_json(t);
}
ScoreTime st_f(const json& j) {
    return score_time_from_json(j);
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
    if (s.pan_law == PanLaw::Custom) j["custom_center_attenuation"] = s.custom_center_attenuation;
    if (s.spatial_mode == SpatialMode::ObjectBased) j["object_id"] = s.object_id;
    return j;
}

SpatialPosition spatial_f(const json& j) {
    SpatialPosition s;
    s.pan = j.at("pan").get<float>();
    s.depth = j.at("depth").get<float>();
    s.elevation = j.at("elevation").get<float>();
    s.width = j.at("width").get<float>();
    s.pan_law = check_enum(j.at("pan_law"), PanLaw::Custom, "PanLaw", j);
    s.spatial_mode = check_enum(j.at("spatial_mode"), SpatialMode::ObjectBased, "SpatialMode", j);
    if (j.contains("custom_center_attenuation"))
        s.custom_center_attenuation = j["custom_center_attenuation"].get<float>();
    if (j.contains("object_id"))
        s.object_id = detail::checked_integer<std::uint32_t>(j["object_id"], "spatial object id");
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
        if (ref.type == LevelReferenceType::MasterTarget) rl["lufs"] = ref.lufs;
        if (ref.type == LevelReferenceType::Channel) {
            rl["channel_id"] = ref.channel_id.value;
            rl["relationship"] = ref.relationship;
        }
        if (ref.type == LevelReferenceType::Group) rl["group_id"] = ref.group_id.value;
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
        rel.reference.type =
            check_enum(rl.at("ref_type"), LevelReferenceType::Group, "LevelReferenceType", rl);
        if (rl.contains("lufs")) rel.reference.lufs = rl["lufs"].get<float>();
        if (rl.contains("channel_id"))
            rel.reference.channel_id = ChannelStripId{
                detail::checked_integer<std::uint64_t>(rl["channel_id"], "reference channel id")};
        if (rl.contains("relationship"))
            rel.reference.relationship = rl["relationship"].get<std::string>();
        if (rl.contains("group_id"))
            rel.reference.group_id = GroupBusId{
                detail::checked_integer<std::uint64_t>(rl["group_id"], "reference group id")};
        rel.offset_db = rl.at("offset_db").get<float>();
        f.relative_level = rel;
    }
    return f;
}

// =============================================================================
// AuxSendLevel
// =============================================================================

json aux_send_j(const AuxSendLevel& s) {
    return json{{"aux_bus_id", s.aux_bus_id.value},
                {"level_db", s.level_db},
                {"pre_fader", s.pre_fader},
                {"enabled", s.enabled}};
}

AuxSendLevel aux_send_f(const json& j) {
    return AuxSendLevel{
        AuxBusId{detail::checked_integer<std::uint64_t>(j.at("aux_bus_id"), "aux bus id")},
        j.at("level_db").get<float>(),
        j.at("pre_fader").get<bool>(),
        j.at("enabled").get<bool>()};
}

// =============================================================================
// DynamicEQConfig
// =============================================================================

json dynamic_eq_j(const DynamicEQConfig& d) {
    return json{{"threshold", d.threshold},
                {"ratio", d.ratio},
                {"attack", d.attack},
                {"release", d.release}};
}

DynamicEQConfig dynamic_eq_f(const json& j) {
    return DynamicEQConfig{j.at("threshold").get<float>(),
                           j.at("ratio").get<float>(),
                           j.at("attack").get<float>(),
                           j.at("release").get<float>()};
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
    b.band_type = check_enum(j.at("band_type"), MixEQBandType::TiltShelf, "MixEQBandType", j);
    if (j.contains("dynamic")) b.dynamic = dynamic_eq_f(j["dynamic"]);
    return b;
}

json eq_j(const MixEQ& eq) {
    json j;
    json bands_arr = json::array();
    for (const auto& b : eq.bands)
        bands_arr.push_back(eq_band_j(b));
    j["bands"] = bands_arr;
    j["linear_phase"] = eq.linear_phase;
    j["auto_gain"] = eq.auto_gain;
    return j;
}

MixEQ eq_f(const json& j) {
    MixEQ eq;
    for (const auto& b : j.at("bands"))
        eq.bands.push_back(eq_band_f(b));
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
    if (sc.source == SidechainSourceType::ExternalChannel) j["channel_id"] = sc.channel_id.value;
    if (sc.source == SidechainSourceType::ExternalBus) j["bus_id"] = sc.bus_id.value;
    if (sc.filter) {
        j["filter"] = json{{"filter_type", static_cast<int>(sc.filter->filter_type)},
                           {"frequency", sc.filter->frequency},
                           {"q", sc.filter->q}};
    }
    return j;
}

MixSidechainConfig sidechain_f(const json& j) {
    MixSidechainConfig sc;
    sc.source =
        check_enum(j.at("source"), SidechainSourceType::ExternalBus, "SidechainSourceType", j);
    if (j.contains("channel_id"))
        sc.channel_id = ChannelStripId{
            detail::checked_integer<std::uint64_t>(j["channel_id"], "sidechain channel id")};
    if (j.contains("bus_id"))
        sc.bus_id =
            GroupBusId{detail::checked_integer<std::uint64_t>(j["bus_id"], "sidechain bus id")};
    if (j.contains("filter")) {
        SidechainFilter f;
        f.filter_type = check_enum(
            j["filter"].at("filter_type"), MixEQBandType::TiltShelf, "MixEQBandType", j["filter"]);
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
    return json{{"threshold", c.threshold},
                {"ratio", c.ratio},
                {"attack", c.attack},
                {"release", c.release},
                {"knee", c.knee},
                {"makeup_gain", c.makeup_gain},
                {"detection", static_cast<int>(c.detection)},
                {"topology", static_cast<int>(c.topology)},
                {"sidechain", sidechain_j(c.sidechain)},
                {"stereo_link", c.stereo_link}};
}

MixCompressor compressor_f(const json& j) {
    MixCompressor c;
    c.threshold = j.at("threshold").get<float>();
    c.ratio = j.at("ratio").get<float>();
    c.attack = j.at("attack").get<float>();
    c.release = j.at("release").get<float>();
    c.knee = j.at("knee").get<float>();
    c.makeup_gain = j.at("makeup_gain").get<float>();
    c.detection = check_enum(j.at("detection"), DetectionMode::Envelope, "DetectionMode", j);
    c.topology =
        check_enum(j.at("topology"), CompressorTopology::FeedBack, "CompressorTopology", j);
    c.sidechain = sidechain_f(j.at("sidechain"));
    c.stereo_link = j.at("stereo_link").get<float>();
    return c;
}

json gate_j(const MixGate& g) {
    return json{{"threshold", g.threshold},
                {"ratio", g.ratio},
                {"attack", g.attack},
                {"hold", g.hold},
                {"release", g.release},
                {"range", g.range},
                {"sidechain", sidechain_j(g.sidechain)}};
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
    return json{{"ceiling", l.ceiling},
                {"release", l.release},
                {"lookahead", l.lookahead},
                {"algorithm", static_cast<int>(l.algorithm)}};
}

MixLimiter limiter_f(const json& j) {
    MixLimiter l;
    l.ceiling = j.at("ceiling").get<float>();
    l.release = j.at("release").get<float>();
    l.lookahead = j.at("lookahead").get<float>();
    l.algorithm = check_enum(j.at("algorithm"), LimiterAlgorithm::ISP, "LimiterAlgorithm", j);
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
    for (const auto& b : m.bands)
        bands_arr.push_back(mb_band_j(b));
    j["bands"] = bands_arr;
    j["crossover_slope"] = static_cast<int>(m.crossover_slope);
    return j;
}

MixMultibandDynamics multiband_f(const json& j) {
    MixMultibandDynamics m;
    for (const auto& f : j.at("crossover_frequencies"))
        m.crossover_frequencies.push_back(f.get<float>());
    for (const auto& b : j.at("bands"))
        m.bands.push_back(mb_band_f(b));
    m.crossover_slope =
        check_enum(j.at("crossover_slope"), CrossoverSlope::LinearPhase, "CrossoverSlope", j);
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
    s.algorithm.type = check_enum(j.at("type"), SaturationTypeTag::Hard, "SaturationTypeTag", j);
    s.algorithm.tape_speed = check_enum(j.at("tape_speed"), TapeSpeed::Ips30, "TapeSpeed", j);
    s.algorithm.tape_bias = j.at("tape_bias").get<float>();
    s.algorithm.tube_model = j.at("tube_model").get<std::string>();
    s.algorithm.console_type =
        check_enum(j.at("console_type"), ConsoleType::Generic, "ConsoleType", j);
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
// Delay and reverb
// =============================================================================

json delay_j(const MixDelay& delay) {
    return json{
        {"tempo_synced", delay.tempo_synced},
        {"delay_ms", delay.delay_ms},
        {"beat_division",
         {{"n", delay.beat_division.numerator()}, {"d", delay.beat_division.denominator()}}},
        {"feedback", delay.feedback},
        {"stereo_mode", static_cast<int>(delay.stereo_mode)},
        {"stereo_offset", delay.stereo_offset},
        {"low_cut_hz", delay.low_cut_hz},
        {"high_cut_hz", delay.high_cut_hz},
        {"modulation_rate", delay.modulation_rate},
        {"modulation_depth", delay.modulation_depth},
        {"mix", delay.mix}};
}

MixDelay delay_f(const json& j) {
    MixDelay delay;
    delay.tempo_synced = j.at("tempo_synced").get<bool>();
    delay.delay_ms = j.at("delay_ms").get<float>();
    const auto numerator = detail::checked_integer<std::int64_t>(j.at("beat_division").at("n"),
                                                                 "delay beat numerator");
    const auto denominator = detail::checked_integer<std::int64_t>(j.at("beat_division").at("d"),
                                                                   "delay beat denominator");
    const auto division = Beat::from_ratio(numerator, denominator);
    if (!division || division->numerator() <= 0) {
        throw json::other_error::create(604, "delay beat division must be positive", &j);
    }
    delay.beat_division = *division;
    delay.feedback = j.at("feedback").get<float>();
    delay.stereo_mode = check_enum(j.at("stereo_mode"), MixDelayMode::PingPong, "MixDelayMode", j);
    delay.stereo_offset = j.at("stereo_offset").get<float>();
    delay.low_cut_hz = j.at("low_cut_hz").get<float>();
    delay.high_cut_hz = j.at("high_cut_hz").get<float>();
    delay.modulation_rate = j.at("modulation_rate").get<float>();
    delay.modulation_depth = j.at("modulation_depth").get<float>();
    delay.mix = j.at("mix").get<float>();
    return delay;
}

json reverb_j(const MixReverb& reverb) {
    return json{{"algorithm", static_cast<int>(reverb.algorithm)},
                {"impulse_response", reverb.impulse_response},
                {"shimmer_pitch", reverb.shimmer_pitch},
                {"decay_time", reverb.decay_time},
                {"pre_delay", reverb.pre_delay},
                {"damping", reverb.damping},
                {"diffusion", reverb.diffusion},
                {"size", reverb.size},
                {"early_reflections_level", reverb.early_reflections_level},
                {"low_cut_hz", reverb.low_cut_hz},
                {"high_cut_hz", reverb.high_cut_hz},
                {"mix", reverb.mix}};
}

MixReverb reverb_f(const json& j) {
    MixReverb reverb;
    reverb.algorithm =
        check_enum(j.at("algorithm"), MixReverbAlgorithm::Shimmer, "MixReverbAlgorithm", j);
    reverb.impulse_response = j.at("impulse_response").get<std::string>();
    reverb.shimmer_pitch = j.at("shimmer_pitch").get<float>();
    reverb.decay_time = j.at("decay_time").get<float>();
    reverb.pre_delay = j.at("pre_delay").get<float>();
    reverb.damping = j.at("damping").get<float>();
    reverb.diffusion = j.at("diffusion").get<float>();
    reverb.size = j.at("size").get<float>();
    reverb.early_reflections_level = j.at("early_reflections_level").get<float>();
    reverb.low_cut_hz = j.at("low_cut_hz").get<float>();
    reverb.high_cut_hz = j.at("high_cut_hz").get<float>();
    reverb.mix = j.at("mix").get<float>();
    return reverb;
}

// =============================================================================
// MixEffect, MixEffectChain
// =============================================================================

json mapping_curve_j(const MappingCurve& curve) {
    json points = json::array();
    for (const auto& [x, y] : curve.custom_points)
        points.push_back(json::array({x, y}));
    return json{{"type", static_cast<int>(curve.type)}, {"custom_points", std::move(points)}};
}

MappingCurve mapping_curve_f(const json& j) {
    MappingCurve curve;
    curve.type = check_enum(j.at("type"), MappingCurveType::Custom, "MappingCurveType", j);
    for (const auto& point : j.at("custom_points")) {
        if (!point.is_array() || point.size() != 2)
            throw json::other_error::create(604, "mapping curve point must be [x, y]", &point);
        curve.custom_points.emplace_back(point.at(0).get<float>(), point.at(1).get<float>());
    }
    return curve;
}

json device_parameter_j(const MixDeviceParameter& mapping) {
    return json{{"parameter_name", mapping.parameter_name},
                {"range_min", mapping.range_min},
                {"range_max", mapping.range_max},
                {"curve", mapping_curve_j(mapping.curve)},
                {"source_min", mapping.source_min},
                {"source_max", mapping.source_max},
                {"value_property", static_cast<int>(mapping.value_property)}};
}

MixDeviceParameter device_parameter_f(const json& j) {
    MixDeviceParameter mapping;
    mapping.parameter_name = j.at("parameter_name").get<std::string>();
    mapping.range_min = j.at("range_min").get<float>();
    mapping.range_max = j.at("range_max").get<float>();
    mapping.curve = mapping_curve_f(j.at("curve"));
    mapping.source_min = j.at("source_min").get<float>();
    mapping.source_max = j.at("source_max").get<float>();
    mapping.value_property = check_enum(j.at("value_property"),
                                        DeviceParameterValueProperty::DisplayValue,
                                        "DeviceParameterValueProperty",
                                        j);
    return mapping;
}

json effect_j(const MixEffect& fx) {
    json j;
    j["id"] = fx.id.value;
    j["enabled"] = fx.enabled;

    std::visit(
        [&](const auto& p) {
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
            } else if constexpr (std::is_same_v<T, MixDelay>) {
                j["type"] = "delay";
                j["params"] = delay_j(p);
            } else if constexpr (std::is_same_v<T, MixReverb>) {
                j["type"] = "reverb";
                j["params"] = reverb_j(p);
            }
        },
        fx.parameters);

    json parameter_map = json::object();
    for (const auto& [path, mapping] : fx.parameter_map)
        parameter_map[path] = device_parameter_j(mapping);
    j["parameter_map"] = std::move(parameter_map);

    return j;
}

MixEffect effect_f(const json& j, int schema_version) {
    MixEffect fx;
    fx.id = MixEffectId{detail::checked_integer<std::uint64_t>(j.at("id"), "mix effect id")};
    fx.enabled = j.at("enabled").get<bool>();

    std::string type = j.at("type").get<std::string>();
    const auto& params = j.at("params");
    if (type == "eq")
        fx.parameters = eq_f(params);
    else if (type == "compressor")
        fx.parameters = compressor_f(params);
    else if (type == "gate")
        fx.parameters = gate_f(params);
    else if (type == "limiter")
        fx.parameters = limiter_f(params);
    else if (type == "multiband")
        fx.parameters = multiband_f(params);
    else if (type == "saturation")
        fx.parameters = saturation_f(params);
    else if (type == "stereo")
        fx.parameters = stereo_f(params);
    else if (type == "delay")
        fx.parameters = delay_f(params);
    else if (type == "reverb")
        fx.parameters = reverb_f(params);
    else
        throw json::other_error::create(604, "unknown mix effect type: " + type, &j);

    if (schema_version >= 3 && !j.contains("parameter_map"))
        throw json::other_error::create(604, "v3 mix effect requires parameter_map", &j);
    if (j.contains("parameter_map")) {
        if (!j.at("parameter_map").is_object())
            throw json::other_error::create(604, "parameter_map must be an object", &j);
        for (const auto& [path, encoded] : j.at("parameter_map").items())
            fx.parameter_map.emplace(path, device_parameter_f(encoded));
    }

    return fx;
}

json chain_j(const MixEffectChain& c) {
    json arr = json::array();
    for (const auto& fx : c.effects)
        arr.push_back(effect_j(fx));
    return arr;
}

MixEffectChain chain_f(const json& j, int schema_version) {
    MixEffectChain c;
    for (const auto& fx : j)
        c.effects.push_back(effect_f(fx, schema_version));
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
        rationale.push_back(json{{"effect_id", r.effect_id.value}, {"purpose", r.purpose}});
    }
    j["rationale"] = rationale;
    return j;
}

ChannelIntent intent_f(const json& j) {
    ChannelIntent ci;
    ci.role_in_mix = check_enum(j.at("role"), MixRole::Dialogue, "MixRole", j);
    ci.frequency_space = frequency_alloc_f(j.at("frequency_space"));
    ci.depth_position = check_enum(j.at("depth"), DepthPosition::VeryFar, "DepthPosition", j);
    if (j.contains("rationale")) {
        for (const auto& r : j["rationale"]) {
            ci.processing_rationale.push_back({MixEffectId{detail::checked_integer<std::uint64_t>(
                                                   r.at("effect_id"), "rationale effect id")},
                                               r.at("purpose").get<std::string>()});
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
    for (const auto& s : ch.sends)
        sends_arr.push_back(aux_send_j(s));
    j["sends"] = sends_arr;
    if (ch.group_assignment) j["group"] = ch.group_assignment->value;
    if (ch.intent) j["intent"] = intent_j(*ch.intent);
    return j;
}

ChannelStrip channel_f(const json& j, int schema_version) {
    ChannelStrip ch;
    ch.id = ChannelStripId{detail::checked_integer<std::uint64_t>(j.at("id"), "channel strip id")};
    ch.part_id = PartId{detail::checked_integer<std::uint64_t>(j.at("part_id"), "channel part id")};
    ch.input_trim = j.at("input_trim").get<float>();
    ch.polarity_invert = j.at("polarity_invert").get<bool>();
    ch.insert_chain = chain_f(j.at("insert_chain"), schema_version);
    ch.fader = fader_f(j.at("fader"));
    ch.spatial = spatial_f(j.at("spatial"));
    ch.mute = j.at("mute").get<bool>();
    ch.solo = j.at("solo").get<bool>();
    for (const auto& s : j.at("sends"))
        ch.sends.push_back(aux_send_f(s));
    if (j.contains("group"))
        ch.group_assignment =
            GroupBusId{detail::checked_integer<std::uint64_t>(j["group"], "channel group id")};
    if (j.contains("intent")) ch.intent = intent_f(j["intent"]);
    return ch;
}

// =============================================================================
// GroupBus
// =============================================================================

json group_output_j(const GroupOutput& go) {
    json j;
    j["type"] = static_cast<int>(go.type);
    if (go.type == GroupOutputType::Group) j["parent_group_id"] = go.parent_group_id.value;
    return j;
}

GroupOutput group_output_f(const json& j) {
    GroupOutput go;
    go.type = check_enum(j.at("type"), GroupOutputType::Group, "GroupOutputType", j);
    if (j.contains("parent_group_id"))
        go.parent_group_id = GroupBusId{
            detail::checked_integer<std::uint64_t>(j["parent_group_id"], "parent group id")};
    return go;
}

json group_bus_j(const GroupBus& g) {
    json j;
    j["id"] = g.id.value;
    j["name"] = g.name;
    json members = json::array();
    for (const auto& m : g.member_channels)
        members.push_back(m.value);
    j["member_channels"] = members;
    json groups = json::array();
    for (const auto& m : g.member_groups)
        groups.push_back(m.value);
    j["member_groups"] = groups;
    j["insert_chain"] = chain_j(g.insert_chain);
    j["fader"] = fader_j(g.fader);
    j["spatial"] = spatial_j(g.spatial);
    j["mute"] = g.mute;
    j["solo"] = g.solo;
    json sends_arr = json::array();
    for (const auto& s : g.sends)
        sends_arr.push_back(aux_send_j(s));
    j["sends"] = sends_arr;
    j["output"] = group_output_j(g.output);
    if (g.intent) {
        j["intent"] = json{{"function", g.intent->function},
                           {"internal_balance", g.intent->internal_balance_description}};
    }
    return j;
}

GroupBus group_bus_f(const json& j, int schema_version) {
    GroupBus g;
    g.id = GroupBusId{detail::checked_integer<std::uint64_t>(j.at("id"), "group bus id")};
    g.name = j.at("name").get<std::string>();
    for (const auto& m : j.at("member_channels"))
        g.member_channels.push_back(
            ChannelStripId{detail::checked_integer<std::uint64_t>(m, "member channel id")});
    for (const auto& m : j.at("member_groups"))
        g.member_groups.push_back(
            GroupBusId{detail::checked_integer<std::uint64_t>(m, "member group id")});
    g.insert_chain = chain_f(j.at("insert_chain"), schema_version);
    g.fader = fader_f(j.at("fader"));
    g.spatial = spatial_f(j.at("spatial"));
    g.mute = j.at("mute").get<bool>();
    g.solo = j.at("solo").get<bool>();
    for (const auto& s : j.at("sends"))
        g.sends.push_back(aux_send_f(s));
    g.output = group_output_f(j.at("output"));
    if (j.contains("intent")) {
        g.intent = GroupIntent{j["intent"].at("function").get<std::string>(),
                               j["intent"].at("internal_balance").get<std::string>()};
    }
    return g;
}

// =============================================================================
// AuxBus
// =============================================================================

json aux_output_j(const AuxOutput& ao) {
    json j;
    j["type"] = static_cast<int>(ao.type);
    if (ao.type == AuxOutputType::Group) j["group_id"] = ao.group_id.value;
    return j;
}

AuxOutput aux_output_f(const json& j) {
    AuxOutput ao;
    ao.type = check_enum(j.at("type"), AuxOutputType::Group, "AuxOutputType", j);
    if (j.contains("group_id"))
        ao.group_id = GroupBusId{
            detail::checked_integer<std::uint64_t>(j["group_id"], "aux output group id")};
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

AuxBus aux_bus_f(const json& j, int schema_version) {
    AuxBus a;
    a.id = AuxBusId{detail::checked_integer<std::uint64_t>(j.at("id"), "aux bus id")};
    a.name = j.at("name").get<std::string>();
    a.effect_chain = chain_f(j.at("effect_chain"), schema_version);
    a.return_level = j.at("return_level").get<float>();
    a.return_spatial = spatial_f(j.at("return_spatial"));
    a.output = aux_output_f(j.at("output"));
    if (j.contains("intent")) a.intent = j["intent"].get<std::string>();
    return a;
}

// =============================================================================
// MasterBus
// =============================================================================

json loudness_target_j(const LoudnessTarget& lt) {
    return json{{"integrated_lufs", lt.integrated_lufs},
                {"true_peak_dbfs", lt.true_peak_dbfs},
                {"loudness_range_lu",
                 lt.loudness_range_lu.has_value() ? json(*lt.loudness_range_lu) : json(nullptr)},
                {"standard", static_cast<int>(lt.standard)}};
}

LoudnessTarget loudness_target_f(const json& j) {
    LoudnessTarget lt;
    lt.integrated_lufs = j.at("integrated_lufs").get<float>();
    lt.true_peak_dbfs = j.at("true_peak_dbfs").get<float>();
    if (!j.at("loudness_range_lu").is_null())
        lt.loudness_range_lu = j["loudness_range_lu"].get<float>();
    lt.standard = check_enum(j.at("standard"), LoudnessStandard::Custom, "LoudnessStandard", j);
    return lt;
}

json dithering_j(const DitheringConfig& d) {
    return json{{"target_bit_depth", d.target_bit_depth},
                {"algorithm", static_cast<int>(d.algorithm)},
                {"noise_shaping_order", d.noise_shaping_order},
                {"pow_r_level", d.pow_r_level},
                {"auto_blank", d.auto_blank}};
}

DitheringConfig dithering_f(const json& j) {
    DitheringConfig d;
    d.target_bit_depth =
        detail::checked_integer<std::uint8_t>(j.at("target_bit_depth"), "dither target bit depth");
    d.algorithm = check_enum(j.at("algorithm"), DitherAlgorithm::PowR, "DitherAlgorithm", j);
    d.noise_shaping_order =
        detail::checked_integer<std::uint8_t>(j.at("noise_shaping_order"), "noise-shaping order");
    d.pow_r_level = detail::checked_integer<std::uint8_t>(j.at("pow_r_level"), "POW-r level");
    d.auto_blank = j.at("auto_blank").get<bool>();
    return d;
}

json metering_j(const MeteringConfig& m) {
    return json{{"lufs", m.lufs},
                {"true_peak", m.true_peak},
                {"rms", m.rms},
                {"correlation", m.correlation},
                {"spectrum", m.spectrum},
                {"dynamics", m.dynamics}};
}

MeteringConfig metering_f(const json& j) {
    return MeteringConfig{j.at("lufs").get<bool>(),
                          j.at("true_peak").get<bool>(),
                          j.at("rms").get<bool>(),
                          j.at("correlation").get<bool>(),
                          j.at("spectrum").get<bool>(),
                          j.at("dynamics").get<bool>()};
}

json master_bus_j(const MasterBus& mb) {
    json j;
    j["insert_chain"] = chain_j(mb.insert_chain);
    j["fader"] = fader_j(mb.fader);
    j["output_format"] = static_cast<int>(mb.output_format);
    if (mb.atmos_config) {
        j["atmos"] = json{{"bed_channels", mb.atmos_config->bed_channels},
                          {"object_count", mb.atmos_config->object_count}};
    }
    if (mb.dithering) j["dithering"] = dithering_j(*mb.dithering);
    if (mb.target_loudness) j["target_loudness"] = loudness_target_j(*mb.target_loudness);
    j["metering"] = metering_j(mb.metering);
    return j;
}

MasterBus master_bus_f(const json& j, int schema_version) {
    MasterBus mb;
    mb.insert_chain = chain_f(j.at("insert_chain"), schema_version);
    mb.fader = fader_f(j.at("fader"));
    mb.output_format = check_enum(j.at("output_format"), OutputFormat::Binaural, "OutputFormat", j);
    if (j.contains("atmos")) {
        mb.atmos_config = AtmosConfig{detail::checked_integer<std::uint8_t>(
                                          j["atmos"].at("bed_channels"), "Atmos bed channel count"),
                                      detail::checked_integer<std::uint8_t>(
                                          j["atmos"].at("object_count"), "Atmos object count")};
    }
    if (j.contains("dithering")) mb.dithering = dithering_f(j["dithering"]);
    if (j.contains("target_loudness")) mb.target_loudness = loudness_target_f(j["target_loudness"]);
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
    return BandEnergy{
        j.at("low").get<float>(), j.at("high").get<float>(), j.at("energy").get<float>()};
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
    for (const auto& b : sp.band_energies)
        bands.push_back(band_energy_j(b));
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
    return json{{"crest_factor", dp.crest_factor},
                {"loudness_range", dp.loudness_range},
                {"dynamic_range_dr", dp.dynamic_range_dr},
                {"plr", dp.peak_to_loudness_ratio}};
}

DynamicProfile dynamic_profile_f(const json& j) {
    return DynamicProfile{j.at("crest_factor").get<float>(),
                          j.at("loudness_range").get<float>(),
                          j.at("dynamic_range_dr").get<float>(),
                          j.at("plr").get<float>()};
}

json loudness_profile_j(const LoudnessProfile& lp) {
    return json{{"integrated", lp.integrated},
                {"short_term_max", lp.short_term_max},
                {"momentary_max", lp.momentary_max},
                {"true_peak", lp.true_peak}};
}

LoudnessProfile loudness_profile_f(const json& j) {
    return LoudnessProfile{j.at("integrated").get<float>(),
                           j.at("short_term_max").get<float>(),
                           j.at("momentary_max").get<float>(),
                           j.at("true_peak").get<float>()};
}

json spatial_profile_j(const SpatialProfile& sp) {
    return json{{"avg_correlation", sp.average_correlation},
                {"avg_width", sp.average_width},
                {"ms_ratio", sp.mid_side_ratio},
                {"lf_mono_coherence", sp.low_frequency_mono_coherence}};
}

SpatialProfile spatial_profile_f(const json& j) {
    return SpatialProfile{j.at("avg_correlation").get<float>(),
                          j.at("avg_width").get<float>(),
                          j.at("ms_ratio").get<float>(),
                          j.at("lf_mono_coherence").get<float>()};
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
    rp.id = ReferenceProfileId{
        detail::checked_integer<std::uint64_t>(j.at("id"), "reference profile id")};
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
    for (const auto& bp : a.breakpoints)
        bps.push_back(automation_bp_j(bp));
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
    a.interpolation =
        check_enum(j.at("interpolation"), InterpolationMode::Exponential, "InterpolationMode", j);
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
    return MixAnnotation{j.at("scope").get<std::string>(), j.at("content").get<std::string>()};
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

nlohmann::json mix_to_json(const MixGraph& graph) {
    json j;
    j["schema_version"] = MIX_IR_SCHEMA_VERSION;
    j["id"] = graph.id.value;

    json channels_arr = json::array();
    for (const auto& ch : graph.channels)
        channels_arr.push_back(channel_j(ch));
    j["channels"] = channels_arr;

    json groups_arr = json::array();
    for (const auto& g : graph.group_buses)
        groups_arr.push_back(group_bus_j(g));
    j["group_buses"] = groups_arr;

    json aux_arr = json::array();
    for (const auto& a : graph.aux_buses)
        aux_arr.push_back(aux_bus_j(a));
    j["aux_buses"] = aux_arr;

    j["master_bus"] = master_bus_j(graph.master_bus);

    json annotations = json::array();
    for (const auto& a : graph.mix_annotations)
        annotations.push_back(annotation_j(a));
    j["annotations"] = annotations;

    json refs = json::array();
    for (const auto& r : graph.reference_profiles)
        refs.push_back(reference_j(r));
    j["reference_profiles"] = refs;

    json autos = json::array();
    for (const auto& a : graph.automation)
        autos.push_back(automation_j(a));
    j["automation"] = autos;

    j["output_format"] = static_cast<int>(graph.output_format);
    j["max_group_nesting_depth"] = graph.max_group_nesting_depth;

    return j;
}

Result<MixGraph> mix_from_json(const nlohmann::json& j) {
    try {
        const auto version =
            detail::checked_integer<int>(j.at("schema_version"), "Mix schema version");
        if (version < 1 || version > MIX_IR_SCHEMA_VERSION) {
            return std::unexpected(ErrorCode::FormatError);
        }

        MixGraph graph;
        graph.id = MixGraphId{detail::checked_integer<std::uint64_t>(j.at("id"), "mix graph id")};

        for (const auto& ch : j.at("channels"))
            graph.channels.push_back(channel_f(ch, version));
        for (const auto& g : j.at("group_buses"))
            graph.group_buses.push_back(group_bus_f(g, version));
        for (const auto& a : j.at("aux_buses"))
            graph.aux_buses.push_back(aux_bus_f(a, version));

        graph.master_bus = master_bus_f(j.at("master_bus"), version);

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

        graph.output_format =
            check_enum(j.at("output_format"), OutputFormat::Binaural, "OutputFormat", j);
        graph.max_group_nesting_depth = detail::checked_integer<std::uint8_t>(
            j.at("max_group_nesting_depth"), "maximum group nesting depth");

        // Validate on load: structural (Error-severity) violations block
        for (const auto& diag : validate_mix(graph)) {
            if (diag.severity == ValidationSeverity::Error) {
                return std::unexpected(ErrorCode::ValidationOnLoadFailed);
            }
        }

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

} // namespace sunny::core
