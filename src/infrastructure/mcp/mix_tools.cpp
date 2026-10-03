/**
 * @file mix_tools.cpp
 * @brief MCP Tool Registration — Mix IR implementation
 *
 *
 * Maps MCP tool calls to Mix IR workflow functions.
 * Graph state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "evidence_encoding.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

/// Validate that an integer is within a contiguous enum range [0, max].
template <typename E> std::optional<E> checked_enum(const json& encoded, int max_val) {
    const auto val = detail::checked_integer<int>(encoded, "enum value");
    if (val < 0 || val > max_val) return std::nullopt;
    return static_cast<E>(val);
}

template <typename E>
std::optional<E>
checked_enum_or(const json& object, const char* key, int default_value, int max_val) {
    if (!object.contains(key)) return checked_enum<E>(json(default_value), max_val);
    return checked_enum<E>(object.at(key), max_val);
}

// Enum max values
constexpr int OutputFormat_Max = 6;        // Binaural
constexpr int MixRole_Max = 7;             // Dialogue
constexpr int DepthPosition_Max = 5;       // VeryFar
constexpr int MixEQBandType_Max = 5;       // TiltShelf
constexpr int DetectionMode_Max = 2;       // Envelope
constexpr int CompressorTopology_Max = 1;  // FeedBack
constexpr int SidechainSourceType_Max = 2; // ExternalBus
constexpr int LimiterAlgorithm_Max = 2;    // ISP
constexpr int SaturationTypeTag_Max = 5;   // Hard
constexpr int MixDelayMode_Max = 2;        // PingPong
constexpr int MixReverbAlgorithm_Max = 7;  // Shimmer
constexpr int InterpolationMode_Max = 3;   // Exponential
constexpr int LoudnessStandard_Max = 5;    // Custom
constexpr int SeatingTemplate_Max = 1;     // European
constexpr int MappingCurveType_Max = 5;    // Custom

std::optional<OrchestralSection> orchestral_section(const std::string& name) {
    static const std::map<std::string, OrchestralSection> sections{
        {"violin_1", OrchestralSection::ViolinI},
        {"violin_2", OrchestralSection::ViolinII},
        {"viola", OrchestralSection::Viola},
        {"cello", OrchestralSection::Cello},
        {"double_bass", OrchestralSection::DoubleBass},
        {"flutes", OrchestralSection::Flutes},
        {"oboes", OrchestralSection::Oboes},
        {"clarinets", OrchestralSection::Clarinets},
        {"bassoons", OrchestralSection::Bassoons},
        {"horns", OrchestralSection::Horns},
        {"trumpets", OrchestralSection::Trumpets},
        {"trombones", OrchestralSection::Trombones},
        {"tuba", OrchestralSection::Tuba},
        {"timpani", OrchestralSection::Timpani},
        {"percussion", OrchestralSection::Percussion},
        {"harp", OrchestralSection::Harp}};
    const auto found = sections.find(name);
    if (found == sections.end()) return std::nullopt;
    return found->second;
}

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json graph_not_found(std::uint64_t id) {
    return error_response("Mix graph not found: " + std::to_string(id));
}

const char* fader_target_type_name(FaderTargetType type) {
    switch (type) {
    case FaderTargetType::Channel:
        return "channel";
    case FaderTargetType::Group:
        return "group";
    case FaderTargetType::Master:
        return "master";
    }
    return "unknown";
}

const char* fader_resolution_status_name(FaderLevelResolutionStatus status) {
    switch (status) {
    case FaderLevelResolutionStatus::Explicit:
        return "explicit";
    case FaderLevelResolutionStatus::Resolved:
        return "resolved";
    case FaderLevelResolutionStatus::RequiresLoudnessMeasurement:
        return "requires_loudness_measurement";
    case FaderLevelResolutionStatus::BlockedByUnresolvedReference:
        return "blocked_by_unresolved_reference";
    }
    return "unknown";
}

json level_reference_j(const std::optional<LevelReference>& reference) {
    if (!reference) return nullptr;
    json encoded;
    if (reference->type == LevelReferenceType::MasterTarget) {
        encoded = {{"type", "master_target"}, {"lufs", reference->lufs}};
    } else if (reference->type == LevelReferenceType::Channel) {
        encoded = {{"type", "channel"},
                   {"id", reference->channel_id.value},
                   {"relationship", reference->relationship}};
    } else {
        encoded = {{"type", "group"}, {"id", reference->group_id.value}};
    }
    return encoded;
}

json fader_level_resolution_j(const RelativeLevelResolution& resolution) {
    json levels = json::array();
    for (const auto& level : resolution.levels) {
        levels.push_back(
            {{"target_type", fader_target_type_name(level.target_type)},
             {"target_id", level.target_id},
             {"explicit_level_db", level.explicit_level_db},
             {"resolved_level_db",
              level.resolved_level_db ? json(*level.resolved_level_db) : json(nullptr)},
             {"status", fader_resolution_status_name(level.status)},
             {"reference", level_reference_j(level.reference)},
             {"offset_db", level.offset_db}});
    }
    return {{"complete", resolution.complete()},
            {"relative_levels_total", resolution.relative_levels_total},
            {"relative_levels_resolved", resolution.relative_levels_resolved},
            {"relative_levels_unresolved", resolution.relative_levels_unresolved},
            {"levels", std::move(levels)}};
}

const MixEffect* find_effect(const MixGraph& graph, MixEffectId effect_id) {
    const MixEffect* match = nullptr;
    const auto inspect = [&](const MixEffectChain& chain) {
        for (const auto& effect : chain.effects) {
            if (effect.id == effect_id) {
                if (match != nullptr) return false;
                match = &effect;
            }
        }
        return true;
    };
    for (const auto& channel : graph.channels)
        if (!inspect(channel.insert_chain)) return nullptr;
    for (const auto& group : graph.group_buses)
        if (!inspect(group.insert_chain)) return nullptr;
    for (const auto& aux : graph.aux_buses)
        if (!inspect(aux.effect_chain)) return nullptr;
    if (!inspect(graph.master_bus.insert_chain)) return nullptr;
    return match;
}

/// Build a MixEQ from JSON parameters
std::optional<MixEQ> build_eq(const json& params) {
    MixEQ eq;
    eq.linear_phase = params.value("linear_phase", false);
    eq.auto_gain = params.value("auto_gain", false);
    if (params.contains("bands")) {
        for (const auto& b : params["bands"]) {
            MixEQBand band;
            band.frequency = static_cast<float>(b.value("frequency", 1000.0));
            band.gain = static_cast<float>(b.value("gain", 0.0));
            band.q = static_cast<float>(b.value("q", 1.0));
            auto bt = checked_enum_or<MixEQBandType>(b, "type", 0, MixEQBandType_Max);
            if (!bt) return std::nullopt;
            band.band_type = *bt;
            if (b.contains("dynamic")) {
                DynamicEQConfig dyn;
                dyn.threshold = static_cast<float>(b["dynamic"].value("threshold", -20.0));
                dyn.ratio = static_cast<float>(b["dynamic"].value("ratio", 2.0));
                dyn.attack = static_cast<float>(b["dynamic"].value("attack", 10.0));
                dyn.release = static_cast<float>(b["dynamic"].value("release", 100.0));
                band.dynamic = dyn;
            }
            eq.bands.push_back(band);
        }
    }
    return eq;
}

/// Build a MixCompressor from JSON parameters
std::optional<MixCompressor> build_compressor(const json& params) {
    MixCompressor comp;
    comp.threshold = static_cast<float>(params.value("threshold", -20.0));
    comp.ratio = static_cast<float>(params.value("ratio", 4.0));
    comp.attack = static_cast<float>(params.value("attack", 10.0));
    comp.release = static_cast<float>(params.value("release", 100.0));
    comp.knee = static_cast<float>(params.value("knee", 0.0));
    comp.makeup_gain = static_cast<float>(params.value("makeup_gain", 0.0));
    auto det = checked_enum_or<DetectionMode>(params, "detection", 1, DetectionMode_Max);
    if (!det) return std::nullopt;
    comp.detection = *det;
    auto top = checked_enum_or<CompressorTopology>(params, "topology", 0, CompressorTopology_Max);
    if (!top) return std::nullopt;
    comp.topology = *top;
    comp.stereo_link = static_cast<float>(params.value("stereo_link", 1.0));

    if (params.contains("sidechain_source")) {
        auto src = checked_enum_or<SidechainSourceType>(
            params, "sidechain_source", 0, SidechainSourceType_Max);
        if (!src) return std::nullopt;
        comp.sidechain.source = *src;
        comp.sidechain.channel_id = ChannelStripId{detail::checked_integer_or<std::uint64_t>(
            params, "sidechain_channel_id", 0, "sidechain channel id")};
        comp.sidechain.bus_id = GroupBusId{detail::checked_integer_or<std::uint64_t>(
            params, "sidechain_bus_id", 0, "sidechain bus id")};
    }
    return comp;
}

/// Build a MixEffect from JSON parameters
std::optional<MixEffect> build_effect(const json& params, std::uint64_t effect_id) {
    MixEffect effect;
    effect.id = MixEffectId{effect_id};
    effect.enabled = params.value("enabled", true);

    auto type = params.at("effect_type").get<std::string>();

    if (type == "eq") {
        auto eq = build_eq(params);
        if (!eq) return std::nullopt;
        effect.parameters = std::move(*eq);
    } else if (type == "compressor") {
        auto compressor = build_compressor(params);
        if (!compressor) return std::nullopt;
        effect.parameters = std::move(*compressor);
    } else if (type == "gate") {
        MixGate gate;
        gate.threshold = static_cast<float>(params.value("threshold", -40.0));
        gate.ratio = static_cast<float>(params.value("ratio", 10.0));
        gate.attack = static_cast<float>(params.value("attack", 0.5));
        gate.hold = static_cast<float>(params.value("hold", 50.0));
        gate.release = static_cast<float>(params.value("release", 100.0));
        gate.range = static_cast<float>(params.value("range", -80.0));
        effect.parameters = gate;
    } else if (type == "limiter") {
        MixLimiter lim;
        lim.ceiling = static_cast<float>(params.value("ceiling", -1.0));
        lim.release = static_cast<float>(params.value("release", 100.0));
        lim.lookahead = static_cast<float>(params.value("lookahead", 5.0));
        auto alg = checked_enum_or<LimiterAlgorithm>(params, "algorithm", 1, LimiterAlgorithm_Max);
        if (!alg) return std::nullopt;
        lim.algorithm = *alg;
        effect.parameters = lim;
    } else if (type == "saturation") {
        MixSaturation sat;
        sat.drive = static_cast<float>(params.value("drive", 0.0));
        sat.mix = static_cast<float>(params.value("mix", 1.0));
        sat.output_level = static_cast<float>(params.value("output_level", 0.0));
        auto st =
            checked_enum_or<SaturationTypeTag>(params, "saturation_type", 0, SaturationTypeTag_Max);
        if (!st) return std::nullopt;
        sat.algorithm.type = *st;
        effect.parameters = sat;
    } else if (type == "stereo") {
        MixStereoProcessor stereo;
        stereo.width = static_cast<float>(params.value("width", 1.0));
        stereo.mid_side_balance = static_cast<float>(params.value("mid_side_balance", 0.5));
        if (params.contains("mono_below"))
            stereo.mono_below = static_cast<float>(params["mono_below"].get<double>());
        effect.parameters = stereo;
    } else if (type == "multiband") {
        MixMultibandDynamics mb;
        if (params.contains("crossover_frequencies")) {
            for (const auto& f : params["crossover_frequencies"])
                mb.crossover_frequencies.push_back(static_cast<float>(f.get<double>()));
        }
        // Each band gets default compressor settings; detailed per-band
        // configuration uses set_parameter or subsequent tool calls
        auto band_count = mb.crossover_frequencies.size() + 1;
        for (std::size_t i = 0; i < band_count; ++i) {
            MultibandDynamicsBand band;
            band.compressor = MixCompressor{};
            mb.bands.push_back(band);
        }
        effect.parameters = std::move(mb);
    } else if (type == "delay") {
        MixDelay delay;
        delay.tempo_synced = params.value("tempo_synced", false);
        delay.delay_ms = static_cast<float>(params.value("delay_ms", 500.0));
        const auto beat_numerator = detail::checked_integer_or<std::int64_t>(
            params, "beat_numerator", 1, "delay beat numerator");
        const auto beat_denominator = detail::checked_integer_or<std::int64_t>(
            params, "beat_denominator", 4, "delay beat denominator");
        const auto beat_division = Beat::from_ratio(beat_numerator, beat_denominator);
        if (!beat_division || beat_division->numerator() <= 0) return std::nullopt;
        delay.beat_division = *beat_division;
        delay.feedback = static_cast<float>(params.value("feedback", 0.3));
        auto mode = checked_enum_or<MixDelayMode>(params, "stereo_mode", 0, MixDelayMode_Max);
        if (!mode) return std::nullopt;
        delay.stereo_mode = *mode;
        delay.stereo_offset = static_cast<float>(params.value("stereo_offset", 0.0));
        delay.low_cut_hz = static_cast<float>(params.value("low_cut_hz", 80.0));
        delay.high_cut_hz = static_cast<float>(params.value("high_cut_hz", 12000.0));
        delay.modulation_rate = static_cast<float>(params.value("modulation_rate", 0.0));
        delay.modulation_depth = static_cast<float>(params.value("modulation_depth", 0.0));
        delay.mix = static_cast<float>(params.value("mix", 1.0));
        if (!std::isfinite(delay.delay_ms) || delay.delay_ms <= 0.0f || delay.delay_ms > 10000.0f ||
            !std::isfinite(delay.feedback) || delay.feedback < 0.0f || delay.feedback > 1.0f ||
            !std::isfinite(delay.stereo_offset) || delay.stereo_offset < 0.0f ||
            delay.stereo_offset > 1000.0f || !std::isfinite(delay.low_cut_hz) ||
            !std::isfinite(delay.high_cut_hz) || delay.low_cut_hz < 20.0f ||
            delay.high_cut_hz > 20000.0f || delay.low_cut_hz >= delay.high_cut_hz ||
            !std::isfinite(delay.modulation_rate) || delay.modulation_rate < 0.0f ||
            delay.modulation_rate > 20.0f || !std::isfinite(delay.modulation_depth) ||
            delay.modulation_depth < 0.0f || delay.modulation_depth > 100.0f ||
            !std::isfinite(delay.mix) || delay.mix < 0.0f || delay.mix > 1.0f) {
            return std::nullopt;
        }
        effect.parameters = delay;
    } else if (type == "reverb") {
        MixReverb reverb;
        auto algorithm = checked_enum_or<MixReverbAlgorithm>(
            params, "reverb_algorithm", 0, MixReverbAlgorithm_Max);
        if (!algorithm) return std::nullopt;
        reverb.algorithm = *algorithm;
        reverb.impulse_response = params.value("impulse_response", "");
        reverb.shimmer_pitch = static_cast<float>(params.value("shimmer_pitch", 12.0));
        reverb.decay_time = static_cast<float>(params.value("decay_time", 1.5));
        reverb.pre_delay = static_cast<float>(params.value("pre_delay", 20.0));
        reverb.damping = static_cast<float>(params.value("damping", 0.5));
        reverb.diffusion = static_cast<float>(params.value("diffusion", 0.7));
        reverb.size = static_cast<float>(params.value("size", 0.5));
        reverb.early_reflections_level =
            static_cast<float>(params.value("early_reflections_level", 0.5));
        reverb.low_cut_hz = static_cast<float>(params.value("low_cut_hz", 80.0));
        reverb.high_cut_hz = static_cast<float>(params.value("high_cut_hz", 12000.0));
        reverb.mix = static_cast<float>(params.value("mix", 1.0));
        auto unit_interval = [](float value) {
            return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
        };
        if ((reverb.algorithm == MixReverbAlgorithm::Convolution &&
             reverb.impulse_response.empty()) ||
            !std::isfinite(reverb.shimmer_pitch) || reverb.shimmer_pitch < -24.0f ||
            reverb.shimmer_pitch > 24.0f || !std::isfinite(reverb.decay_time) ||
            reverb.decay_time <= 0.0f || reverb.decay_time > 120.0f ||
            !std::isfinite(reverb.pre_delay) || reverb.pre_delay < 0.0f ||
            reverb.pre_delay > 500.0f || !unit_interval(reverb.damping) ||
            !unit_interval(reverb.diffusion) || !unit_interval(reverb.size) ||
            !unit_interval(reverb.early_reflections_level) || !unit_interval(reverb.mix) ||
            !std::isfinite(reverb.low_cut_hz) || !std::isfinite(reverb.high_cut_hz) ||
            reverb.low_cut_hz < 20.0f || reverb.high_cut_hz > 20000.0f ||
            reverb.low_cut_hz >= reverb.high_cut_hz) {
            return std::nullopt;
        }
        effect.parameters = std::move(reverb);
    } else {
        return std::nullopt;
    }

    return effect;
}

} // anonymous namespace

void register_mix_tools(McpServer& server, std::shared_ptr<MixSession> session) {
    if (!session) session = std::make_shared<MixSession>();

    // =========================================================================
    // create_mix_graph
    // =========================================================================
    server.register_tool(
        "create_mix_graph",
        "Initialise a Mix IR graph with one ChannelStrip per Score IR Part",
        {{"type", "object"},
         {"properties",
          {{"part_ids",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Score IR Part IDs to create channels for"}}}}},
         {"required", json::array({"part_ids"})}},
        [session](const json& params) -> json {
            std::vector<PartId> parts;
            for (const auto& pid : params.at("part_ids"))
                parts.push_back(PartId{detail::checked_integer<std::uint64_t>(pid, "part id")});
            auto gid = session->next_graph_id;
            auto graph = create_mix_graph(MixGraphId{gid}, parts);
            session->graphs.emplace(gid, std::move(graph));
            ++session->next_graph_id;
            return {{"graph_id", gid}, {"channel_count", parts.size()}, {"success", true}};
        });

    // =========================================================================
    // create_group_bus
    // =========================================================================
    server.register_tool(
        "create_group_bus",
        "Create a group bus and assign channels to it",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"name", {{"type", "string"}, {"description", "Group bus name (e.g. 'Strings')"}}},
           {"member_channel_ids",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Channel IDs to assign to this group"}}}}},
         {"required", json::array({"graph_id", "name"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            auto gid = GroupBusId{session->next_group_id};
            std::vector<ChannelStripId> members;
            if (params.contains("member_channel_ids")) {
                for (const auto& id : params["member_channel_ids"])
                    members.push_back(
                        ChannelStripId{detail::checked_integer<std::uint64_t>(id, "channel id")});
            }
            auto r = create_group_bus(*g, gid, params.at("name").get<std::string>(), members);
            if (!r) return error_response("Failed to create group bus");
            ++session->next_group_id;
            return {{"group_bus_id", gid.value}, {"success", true}};
        });

    // =========================================================================
    // create_aux_bus
    // =========================================================================
    server.register_tool(
        "create_aux_bus",
        "Create an auxiliary bus (reverb, delay, etc.)",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"name", {{"type", "string"}, {"description", "Aux bus name (e.g. 'Concert Hall')"}}}}},
         {"required", json::array({"graph_id", "name"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            auto aid = AuxBusId{session->next_aux_id};
            auto r = create_aux_bus(*g, aid, params.at("name").get<std::string>());
            if (!r) return error_response("Failed to create aux bus");
            ++session->next_aux_id;
            return {{"aux_bus_id", aid.value}, {"success", true}};
        });

    // =========================================================================
    // assign_channel_to_group
    // =========================================================================
    server.register_tool(
        "assign_channel_to_group",
        "Route a channel to a group bus",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"group_id", {{"type", "integer"}, {"description", "Target group bus ID"}}}}},
         {"required", json::array({"graph_id", "channel_id", "group_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto r = assign_channel_to_group(*g,
                                             ChannelStripId{detail::checked_integer<std::uint64_t>(
                                                 params.at("channel_id"), "channel id")},
                                             GroupBusId{detail::checked_integer<std::uint64_t>(
                                                 params.at("group_id"), "group id")});
            if (!r) return error_response("Channel or group not found");
            return {{"success", true}};
        });

    // =========================================================================
    // assign_group_to_group
    // =========================================================================
    server.register_tool(
        "assign_group_to_group",
        "Route one group bus into another with exact mirrored nesting",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"child_group_id", {{"type", "integer"}, {"description", "Child group bus ID"}}},
           {"parent_group_id",
            {{"type", "integer"}, {"description", "Destination parent group bus ID"}}}}},
         {"required", json::array({"graph_id", "child_group_id", "parent_group_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* graph = session->find(graph_id);
            if (!graph) return graph_not_found(graph_id);
            const auto result =
                assign_group_to_group(*graph,
                                      GroupBusId{detail::checked_integer<std::uint64_t>(
                                          params.at("child_group_id"), "child group id")},
                                      GroupBusId{detail::checked_integer<std::uint64_t>(
                                          params.at("parent_group_id"), "parent group id")});
            if (!result)
                return error_response("Failed to route group bus: " +
                                      std::to_string(static_cast<int>(result.error())));
            return {{"success", true}};
        });

    // =========================================================================
    // route_group_to_master
    // =========================================================================
    server.register_tool(
        "route_group_to_master",
        "Route a group bus directly to the Mix master and remove stale parent edges",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"group_id", {{"type", "integer"}, {"description", "Group bus ID"}}}}},
         {"required", json::array({"graph_id", "group_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* graph = session->find(graph_id);
            if (!graph) return graph_not_found(graph_id);
            const auto result =
                route_group_to_master(*graph,
                                      GroupBusId{detail::checked_integer<std::uint64_t>(
                                          params.at("group_id"), "group id")});
            if (!result)
                return error_response("Failed to route group bus: " +
                                      std::to_string(static_cast<int>(result.error())));
            return {{"success", true}};
        });

    // =========================================================================
    // set_channel_send
    // =========================================================================
    server.register_tool(
        "set_channel_send",
        "Set a channel's send level to an aux bus",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"aux_id", {{"type", "integer"}, {"description", "Aux bus ID"}}},
           {"level_db", {{"type", "number"}, {"description", "Send level in dB"}}},
           {"pre_fader",
            {{"type", "boolean"}, {"description", "Pre-fader send (default false)"}}}}},
         {"required", json::array({"graph_id", "channel_id", "aux_id", "level_db"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto r = set_channel_send(
                *g,
                ChannelStripId{
                    detail::checked_integer<std::uint64_t>(params.at("channel_id"), "channel id")},
                AuxBusId{detail::checked_integer<std::uint64_t>(params.at("aux_id"), "aux bus id")},
                static_cast<float>(params.at("level_db").get<double>()),
                params.value("pre_fader", false));
            if (!r) {
                if (r.error() == ErrorCode::MixNotFound)
                    return error_response("Channel or aux bus not found");
                return error_response("Send level must be finite and at most +" +
                                      std::to_string(static_cast<int>(MIX_LEVEL_CEILING_DB)) +
                                      " dB");
            }
            return {{"success", true}};
        });

    // =========================================================================
    // apply_seating_template
    // =========================================================================
    server.register_tool(
        "apply_seating_template",
        "Apply an orchestral seating preset to spatial positions",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"template", {{"type", "integer"}, {"description", "0=American, 1=European"}}},
           {"sections",
            {{"type", "array"},
             {"description",
              "Array of {channel_id, section}; section is one of violin_1, violin_2, viola, "
              "cello, double_bass, flutes, oboes, clarinets, bassoons, horns, trumpets, "
              "trombones, tuba, timpani, percussion, harp. Unlisted channels are unchanged."}}}}},
         {"required", json::array({"graph_id", "sections"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto tmpl =
                checked_enum_or<SeatingTemplate>(params, "template", 0, SeatingTemplate_Max);
            if (!tmpl) return error_response("Invalid seating template");
            std::vector<SeatingAssignment> assignments;
            for (const auto& encoded : params.at("sections")) {
                const auto section = orchestral_section(encoded.at("section").get<std::string>());
                if (!section)
                    return error_response("Unknown orchestral section: " +
                                          encoded.at("section").get<std::string>());
                assignments.push_back({ChannelStripId{detail::checked_integer<std::uint64_t>(
                                           encoded.at("channel_id"), "channel id")},
                                       *section});
            }
            auto r = apply_seating_template(*g, *tmpl, assignments);
            if (!r) {
                if (r.error() == ErrorCode::MixNotFound) return error_response("Channel not found");
                return error_response("Each channel may be seated only once");
            }
            json placed = json::array();
            for (const auto& assignment : assignments)
                placed.push_back(assignment.channel_id.value);
            return {{"success", true}, {"placed_channels", placed}};
        });

    // =========================================================================
    // set_output_format
    // =========================================================================
    server.register_tool(
        "set_output_format",
        "Set stereo, surround, or immersive output format",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"format",
            {{"type", "integer"},
             {"description",
              "OutputFormat enum: 0=Stereo, 1=LCR, 2=Quad, 3=5.1, 4=7.1, 5=Atmos, 6=Binaural"}}}}},
         {"required", json::array({"graph_id", "format"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto fmt = checked_enum<OutputFormat>(params.at("format"), OutputFormat_Max);
            if (!fmt) return error_response("Invalid output format");
            set_output_format(*g, *fmt);
            return {{"success", true}};
        });

    // =========================================================================
    // add_channel_effect
    // =========================================================================
    server.register_tool(
        "add_channel_effect",
        "Add a mix-stage processor to a channel",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"effect_type",
            {{"type", "string"},
             {"description",
              "eq|compressor|gate|limiter|saturation|stereo|multiband|delay|reverb"}}},
           {"enabled", {{"type", "boolean"}, {"description", "Effect enabled (default true)"}}},
           {"bands",
            {{"type", "array"},
             {"description", "EQ bands: [{frequency, gain, q, type, dynamic}]"}}},
           {"linear_phase", {{"type", "boolean"}, {"description", "Linear phase EQ"}}},
           {"auto_gain", {{"type", "boolean"}, {"description", "Auto gain EQ"}}},
           {"threshold", {{"type", "number"}, {"description", "Compressor/gate threshold dB"}}},
           {"ratio", {{"type", "number"}, {"description", "Compressor/gate ratio"}}},
           {"attack", {{"type", "number"}, {"description", "Attack ms"}}},
           {"release", {{"type", "number"}, {"description", "Release ms"}}},
           {"knee", {{"type", "number"}, {"description", "Compressor knee dB"}}},
           {"makeup_gain", {{"type", "number"}, {"description", "Compressor makeup gain dB"}}},
           {"ceiling", {{"type", "number"}, {"description", "Limiter ceiling dBFS"}}},
           {"drive", {{"type", "number"}, {"description", "Saturation drive 0-1"}}},
           {"mix", {{"type", "number"}, {"description", "Dry/wet mix 0-1"}}},
           {"tempo_synced", {{"type", "boolean"}, {"description", "Tempo-sync delay time"}}},
           {"delay_ms", {{"type", "number"}, {"description", "Free delay time in ms"}}},
           {"beat_numerator", {{"type", "integer"}, {"description", "Synced delay numerator"}}},
           {"beat_denominator", {{"type", "integer"}, {"description", "Synced delay denominator"}}},
           {"feedback", {{"type", "number"}, {"description", "Delay feedback 0-1"}}},
           {"stereo_mode",
            {{"type", "integer"}, {"description", "Delay mode: 0=Mono, 1=Stereo, 2=PingPong"}}},
           {"stereo_offset", {{"type", "number"}, {"description", "Stereo delay offset"}}},
           {"reverb_algorithm",
            {{"type", "integer"},
             {"description", "Reverb algorithm: 0=Algorithmic through 7=Shimmer"}}},
           {"impulse_response",
            {{"type", "string"}, {"description", "Convolution impulse response identifier"}}},
           {"shimmer_pitch", {{"type", "number"}, {"description", "Shimmer pitch semitones"}}},
           {"decay_time", {{"type", "number"}, {"description", "Reverb RT60 seconds"}}},
           {"pre_delay", {{"type", "number"}, {"description", "Reverb pre-delay ms"}}},
           {"damping", {{"type", "number"}, {"description", "Reverb damping 0-1"}}},
           {"diffusion", {{"type", "number"}, {"description", "Reverb diffusion 0-1"}}},
           {"size", {{"type", "number"}, {"description", "Reverb size 0-1"}}},
           {"early_reflections_level",
            {{"type", "number"}, {"description", "Early-reflection level 0-1"}}},
           {"low_cut_hz", {{"type", "number"}, {"description", "Effect low-cut frequency"}}},
           {"high_cut_hz", {{"type", "number"}, {"description", "Effect high-cut frequency"}}},
           {"modulation_rate", {{"type", "number"}, {"description", "Delay modulation rate Hz"}}},
           {"modulation_depth", {{"type", "number"}, {"description", "Delay modulation depth ms"}}},
           {"width", {{"type", "number"}, {"description", "Stereo width"}}},
           {"mono_below", {{"type", "number"}, {"description", "Mono below Hz"}}},
           {"sidechain_source",
            {{"type", "integer"},
             {"description", "SidechainSourceType: 0=Internal, 1=ExternalChannel, 2=ExternalBus"}}},
           {"sidechain_channel_id",
            {{"type", "integer"}, {"description", "Sidechain source channel ID"}}},
           {"sidechain_bus_id", {{"type", "integer"}, {"description", "Sidechain source bus ID"}}},
           {"crossover_frequencies",
            {{"type", "array"}, {"description", "Multiband crossover points Hz"}}}}},
         {"required", json::array({"graph_id", "channel_id", "effect_type"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto eid = session->next_effect_id;
            auto effect = build_effect(params, eid);
            if (!effect) return error_response("Unknown or invalid mix effect configuration");
            auto r = add_channel_effect(*g,
                                        ChannelStripId{detail::checked_integer<std::uint64_t>(
                                            params.at("channel_id"), "channel id")},
                                        std::move(*effect));
            if (!r) return error_response("Channel not found");
            ++session->next_effect_id;
            return {{"effect_id", eid}, {"success", true}};
        });

    // =========================================================================
    // add_bus_effect
    // =========================================================================
    server.register_tool(
        "add_bus_effect",
        "Add processing to a group bus",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"group_id", {{"type", "integer"}, {"description", "Group bus ID"}}},
           {"effect_type",
            {{"type", "string"},
             {"description",
              "eq|compressor|gate|limiter|saturation|stereo|multiband|delay|reverb"}}},
           {"enabled", {{"type", "boolean"}, {"description", "Effect enabled"}}},
           {"threshold", {{"type", "number"}, {"description", "Compressor/gate threshold dB"}}},
           {"ratio", {{"type", "number"}, {"description", "Ratio"}}},
           {"attack", {{"type", "number"}, {"description", "Attack ms"}}},
           {"release", {{"type", "number"}, {"description", "Release ms"}}},
           {"bands", {{"type", "array"}, {"description", "EQ bands"}}},
           {"drive", {{"type", "number"}, {"description", "Saturation drive"}}},
           {"mix", {{"type", "number"}, {"description", "Dry/wet"}}},
           {"tempo_synced", {{"type", "boolean"}, {"description", "Tempo-sync delay time"}}},
           {"delay_ms", {{"type", "number"}, {"description", "Free delay time ms"}}},
           {"beat_numerator", {{"type", "integer"}, {"description", "Synced delay numerator"}}},
           {"beat_denominator", {{"type", "integer"}, {"description", "Synced delay denominator"}}},
           {"feedback", {{"type", "number"}, {"description", "Delay feedback 0-1"}}},
           {"stereo_mode", {{"type", "integer"}, {"description", "Delay stereo mode"}}},
           {"stereo_offset", {{"type", "number"}, {"description", "Stereo delay offset"}}},
           {"modulation_rate", {{"type", "number"}, {"description", "Delay modulation rate Hz"}}},
           {"modulation_depth", {{"type", "number"}, {"description", "Delay modulation depth ms"}}},
           {"reverb_algorithm", {{"type", "integer"}, {"description", "Reverb algorithm"}}},
           {"impulse_response",
            {{"type", "string"}, {"description", "Convolution impulse response identifier"}}},
           {"shimmer_pitch", {{"type", "number"}, {"description", "Shimmer pitch semitones"}}},
           {"decay_time", {{"type", "number"}, {"description", "Reverb RT60 seconds"}}},
           {"pre_delay", {{"type", "number"}, {"description", "Reverb pre-delay ms"}}},
           {"damping", {{"type", "number"}, {"description", "Reverb damping 0-1"}}},
           {"diffusion", {{"type", "number"}, {"description", "Reverb diffusion 0-1"}}},
           {"size", {{"type", "number"}, {"description", "Reverb size 0-1"}}},
           {"early_reflections_level",
            {{"type", "number"}, {"description", "Early-reflection level 0-1"}}},
           {"low_cut_hz", {{"type", "number"}, {"description", "Effect low-cut frequency"}}},
           {"high_cut_hz", {{"type", "number"}, {"description", "Effect high-cut frequency"}}},
           {"width", {{"type", "number"}, {"description", "Stereo width"}}},
           {"crossover_frequencies",
            {{"type", "array"}, {"description", "Multiband crossovers"}}}}},
         {"required", json::array({"graph_id", "group_id", "effect_type"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto eid = session->next_effect_id;
            auto effect = build_effect(params, eid);
            if (!effect) return error_response("Unknown or invalid mix effect configuration");
            auto r = add_bus_effect(*g,
                                    GroupBusId{detail::checked_integer<std::uint64_t>(
                                        params.at("group_id"), "group id")},
                                    std::move(*effect));
            if (!r) return error_response("Group bus not found");
            ++session->next_effect_id;
            return {{"effect_id", eid}, {"success", true}};
        });

    // =========================================================================
    // add_aux_effect
    // =========================================================================
    server.register_tool(
        "add_aux_effect",
        "Add processing to an aux bus effect chain",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"aux_id", {{"type", "integer"}, {"description", "Aux bus ID"}}},
           {"effect_type",
            {{"type", "string"},
             {"description",
              "eq|compressor|gate|limiter|saturation|stereo|multiband|delay|reverb"}}},
           {"enabled", {{"type", "boolean"}, {"description", "Effect enabled"}}},
           {"threshold", {{"type", "number"}, {"description", "Threshold dB"}}},
           {"ratio", {{"type", "number"}, {"description", "Ratio"}}},
           {"attack", {{"type", "number"}, {"description", "Attack ms"}}},
           {"release", {{"type", "number"}, {"description", "Release ms"}}},
           {"bands", {{"type", "array"}, {"description", "EQ bands"}}},
           {"drive", {{"type", "number"}, {"description", "Saturation drive"}}},
           {"mix", {{"type", "number"}, {"description", "Dry/wet"}}},
           {"tempo_synced", {{"type", "boolean"}, {"description", "Tempo-sync delay time"}}},
           {"delay_ms", {{"type", "number"}, {"description", "Free delay time ms"}}},
           {"beat_numerator", {{"type", "integer"}, {"description", "Synced delay numerator"}}},
           {"beat_denominator", {{"type", "integer"}, {"description", "Synced delay denominator"}}},
           {"feedback", {{"type", "number"}, {"description", "Delay feedback 0-1"}}},
           {"stereo_mode", {{"type", "integer"}, {"description", "Delay stereo mode"}}},
           {"stereo_offset", {{"type", "number"}, {"description", "Stereo delay offset"}}},
           {"modulation_rate", {{"type", "number"}, {"description", "Delay modulation rate Hz"}}},
           {"modulation_depth", {{"type", "number"}, {"description", "Delay modulation depth ms"}}},
           {"reverb_algorithm", {{"type", "integer"}, {"description", "Reverb algorithm"}}},
           {"impulse_response",
            {{"type", "string"}, {"description", "Convolution impulse response identifier"}}},
           {"shimmer_pitch", {{"type", "number"}, {"description", "Shimmer pitch semitones"}}},
           {"decay_time", {{"type", "number"}, {"description", "Reverb RT60 seconds"}}},
           {"pre_delay", {{"type", "number"}, {"description", "Reverb pre-delay ms"}}},
           {"damping", {{"type", "number"}, {"description", "Reverb damping 0-1"}}},
           {"diffusion", {{"type", "number"}, {"description", "Reverb diffusion 0-1"}}},
           {"size", {{"type", "number"}, {"description", "Reverb size 0-1"}}},
           {"early_reflections_level",
            {{"type", "number"}, {"description", "Early-reflection level 0-1"}}},
           {"low_cut_hz", {{"type", "number"}, {"description", "Effect low-cut frequency"}}},
           {"high_cut_hz", {{"type", "number"}, {"description", "Effect high-cut frequency"}}},
           {"width", {{"type", "number"}, {"description", "Stereo width"}}},
           {"crossover_frequencies",
            {{"type", "array"}, {"description", "Multiband crossovers"}}}}},
         {"required", json::array({"graph_id", "aux_id", "effect_type"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto eid = session->next_effect_id;
            auto effect = build_effect(params, eid);
            if (!effect) return error_response("Unknown or invalid mix effect configuration");
            auto r = add_aux_effect(
                *g,
                AuxBusId{detail::checked_integer<std::uint64_t>(params.at("aux_id"), "aux bus id")},
                std::move(*effect));
            if (!r) return error_response("Aux bus not found");
            ++session->next_effect_id;
            return {{"effect_id", eid}, {"success", true}};
        });

    // =========================================================================
    // add_master_effect
    // =========================================================================
    server.register_tool(
        "add_master_effect",
        "Add processing to the master bus chain",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"effect_type",
            {{"type", "string"},
             {"description",
              "eq|compressor|gate|limiter|saturation|stereo|multiband|delay|reverb"}}},
           {"enabled", {{"type", "boolean"}, {"description", "Effect enabled"}}},
           {"threshold", {{"type", "number"}, {"description", "Threshold dB"}}},
           {"ratio", {{"type", "number"}, {"description", "Ratio"}}},
           {"attack", {{"type", "number"}, {"description", "Attack ms"}}},
           {"release", {{"type", "number"}, {"description", "Release ms"}}},
           {"bands", {{"type", "array"}, {"description", "EQ bands"}}},
           {"ceiling", {{"type", "number"}, {"description", "Limiter ceiling"}}},
           {"drive", {{"type", "number"}, {"description", "Saturation drive"}}},
           {"mix", {{"type", "number"}, {"description", "Dry/wet"}}},
           {"tempo_synced", {{"type", "boolean"}, {"description", "Tempo-sync delay time"}}},
           {"delay_ms", {{"type", "number"}, {"description", "Free delay time ms"}}},
           {"beat_numerator", {{"type", "integer"}, {"description", "Synced delay numerator"}}},
           {"beat_denominator", {{"type", "integer"}, {"description", "Synced delay denominator"}}},
           {"feedback", {{"type", "number"}, {"description", "Delay feedback 0-1"}}},
           {"stereo_mode", {{"type", "integer"}, {"description", "Delay stereo mode"}}},
           {"stereo_offset", {{"type", "number"}, {"description", "Stereo delay offset"}}},
           {"modulation_rate", {{"type", "number"}, {"description", "Delay modulation rate Hz"}}},
           {"modulation_depth", {{"type", "number"}, {"description", "Delay modulation depth ms"}}},
           {"reverb_algorithm", {{"type", "integer"}, {"description", "Reverb algorithm"}}},
           {"impulse_response",
            {{"type", "string"}, {"description", "Convolution impulse response identifier"}}},
           {"shimmer_pitch", {{"type", "number"}, {"description", "Shimmer pitch semitones"}}},
           {"decay_time", {{"type", "number"}, {"description", "Reverb RT60 seconds"}}},
           {"pre_delay", {{"type", "number"}, {"description", "Reverb pre-delay ms"}}},
           {"damping", {{"type", "number"}, {"description", "Reverb damping 0-1"}}},
           {"diffusion", {{"type", "number"}, {"description", "Reverb diffusion 0-1"}}},
           {"size", {{"type", "number"}, {"description", "Reverb size 0-1"}}},
           {"early_reflections_level",
            {{"type", "number"}, {"description", "Early-reflection level 0-1"}}},
           {"low_cut_hz", {{"type", "number"}, {"description", "Effect low-cut frequency"}}},
           {"high_cut_hz", {{"type", "number"}, {"description", "Effect high-cut frequency"}}},
           {"width", {{"type", "number"}, {"description", "Stereo width"}}},
           {"crossover_frequencies",
            {{"type", "array"}, {"description", "Multiband crossovers"}}}}},
         {"required", json::array({"graph_id", "effect_type"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto eid = session->next_effect_id;
            auto effect = build_effect(params, eid);
            if (!effect) return error_response("Unknown or invalid mix effect configuration");
            add_master_effect(*g, std::move(*effect));
            ++session->next_effect_id;
            return {{"effect_id", eid}, {"success", true}};
        });

    // =========================================================================
    // map_mix_effect_parameter
    // =========================================================================
    server.register_tool(
        "map_mix_effect_parameter",
        "Map one numeric MixEffect source path into the exact inserted Live DeviceParameter",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}}},
           {"effect_id", {{"type", "integer"}}},
           {"source_path", {{"type", "string"}}},
           {"parameter_name", {{"type", "string"}}},
           {"source_min", {{"type", "number"}}},
           {"source_max", {{"type", "number"}}},
           {"target_min", {{"type", "number"}}},
           {"target_max", {{"type", "number"}}},
           {"curve_type", {{"type", "integer"}, {"minimum", 0}, {"maximum", 5}}},
           {"custom_points",
            {{"type", "array"},
             {"items",
              {{"type", "array"},
               {"minItems", 2},
               {"maxItems", 2},
               {"items", {{"type", "number"}}}}}}},
           {"value_property", {{"type", "string"}, {"enum", {"value", "display_value"}}}}}},
         {"required",
          json::array({"graph_id",
                       "effect_id",
                       "source_path",
                       "parameter_name",
                       "source_min",
                       "source_max",
                       "target_min",
                       "target_max"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* graph = session->find(graph_id);
            if (!graph) return graph_not_found(graph_id);

            MixDeviceParameter mapping;
            mapping.parameter_name = params.at("parameter_name").get<std::string>();
            mapping.source_min = params.at("source_min").get<float>();
            mapping.source_max = params.at("source_max").get<float>();
            mapping.range_min = params.at("target_min").get<float>();
            mapping.range_max = params.at("target_max").get<float>();
            const auto curve =
                checked_enum_or<MappingCurveType>(params, "curve_type", 0, MappingCurveType_Max);
            if (!curve) return error_response("curve_type out of range");
            mapping.curve.type = *curve;
            if (params.contains("custom_points")) {
                for (const auto& point : params.at("custom_points")) {
                    if (!point.is_array() || point.size() != 2 || !point.at(0).is_number() ||
                        !point.at(1).is_number())
                        return error_response("custom_points must contain numeric [x, y] pairs");
                    mapping.curve.custom_points.emplace_back(point.at(0).get<float>(),
                                                             point.at(1).get<float>());
                }
            }
            const auto property = params.value("value_property", std::string{"value"});
            if (property == "display_value") {
                mapping.value_property = DeviceParameterValueProperty::DisplayValue;
            } else if (property != "value") {
                return error_response("value_property must be 'value' or 'display_value'");
            }

            const auto effect_id = MixEffectId{
                detail::checked_integer<std::uint64_t>(params.at("effect_id"), "mix effect id")};
            const auto source_path = params.at("source_path").get<std::string>();
            const auto result =
                map_mix_effect_parameter(*graph, effect_id, source_path, std::move(mapping));
            if (!result) {
                if (result.error() == ErrorCode::MixNotFound)
                    return error_response("Mix effect not found");
                if (result.error() == ErrorCode::MixDuplicateId)
                    return error_response("Mix effect identity is not unique");
                return error_response(
                    "source path, mapping domains, curve, or target parameter aliases are invalid");
            }

            const auto* effect = find_effect(*graph, effect_id);
            if (effect == nullptr) return error_response("Mix effect identity is not unique");
            const auto source_value = get_mix_effect_parameter(*effect, source_path);
            const auto& stored = effect->parameter_map.at(source_path);
            const auto target_value = map_mix_device_parameter_value(*source_value, stored);
            return {{"success", true},
                    {"source_path", source_path},
                    {"source_value", *source_value},
                    {"target_value", *target_value}};
        });

    // =========================================================================
    // set_channel_level
    // =========================================================================
    server.register_tool(
        "set_channel_level",
        "Set a channel's fader level in dB",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"level_db",
            {{"type", "number"}, {"maximum", 12.0}, {"description", "Fader level in dB"}}}}},
         {"required", json::array({"graph_id", "channel_id", "level_db"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto r = set_channel_level(*g,
                                       ChannelStripId{detail::checked_integer<std::uint64_t>(
                                           params.at("channel_id"), "channel id")},
                                       static_cast<float>(params.at("level_db").get<double>()));
            if (!r) {
                if (r.error() == ErrorCode::MixNotFound) return error_response("Channel not found");
                return error_response("Fader level is outside the model domain");
            }
            return {{"success", true}};
        });

    // =========================================================================
    // set_channel_relative_level
    // =========================================================================
    server.register_tool(
        "set_channel_relative_level",
        "Set and preflight a channel fader relationship against a channel, group, or loudness "
        "target",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}}},
           {"channel_id", {{"type", "integer"}}},
           {"reference_type",
            {{"type", "string"}, {"enum", {"master_target", "channel", "group"}}}},
           {"reference_id", {{"type", "integer"}}},
           {"lufs", {{"type", "number"}, {"maximum", 0.0}}},
           {"relationship", {{"type", "string"}}},
           {"offset_db", {{"type", "number"}}}}},
         {"required", json::array({"graph_id", "channel_id", "reference_type", "offset_db"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* graph = session->find(graph_id);
            if (!graph) return graph_not_found(graph_id);

            RelativeLevel relative;
            relative.offset_db = params.at("offset_db").get<float>();
            const auto type = params.at("reference_type").get<std::string>();
            if (type == "master_target") {
                if (!params.contains("lufs")) return error_response("master_target requires lufs");
                relative.reference.type = LevelReferenceType::MasterTarget;
                relative.reference.lufs = params.at("lufs").get<float>();
            } else if (type == "channel" || type == "group") {
                if (!params.contains("reference_id"))
                    return error_response(type + " reference requires reference_id");
                const auto reference_id = detail::checked_integer<std::uint64_t>(
                    params.at("reference_id"), "relative fader reference id");
                if (type == "channel") {
                    relative.reference.type = LevelReferenceType::Channel;
                    relative.reference.channel_id = ChannelStripId{reference_id};
                    relative.reference.relationship = params.value("relationship", std::string{});
                } else {
                    relative.reference.type = LevelReferenceType::Group;
                    relative.reference.group_id = GroupBusId{reference_id};
                }
            } else {
                return error_response("unknown relative fader reference_type");
            }

            const auto channel_id = ChannelStripId{
                detail::checked_integer<std::uint64_t>(params.at("channel_id"), "channel id")};
            const auto applied =
                set_channel_relative_level(*graph, channel_id, std::move(relative));
            if (!applied)
                return error_response("channel or reference was not found, or the relationship is "
                                      "cyclic/out of domain");
            const auto resolution = resolve_relative_levels(*graph);
            if (!resolution) return error_response("relative fader resolution failed");
            return {{"success", true},
                    {"fader_level_resolution", fader_level_resolution_j(*resolution)}};
        });

    // =========================================================================
    // resolve_mix_fader_levels
    // =========================================================================
    server.register_tool(
        "resolve_mix_fader_levels",
        "Resolve channel/group/master fader constraints without contacting Ableton",
        {{"type", "object"},
         {"properties", {{"graph_id", {{"type", "integer"}}}}},
         {"required", json::array({"graph_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            const auto* graph = session->find(graph_id);
            if (!graph) return graph_not_found(graph_id);
            const auto resolution = resolve_relative_levels(*graph);
            if (!resolution)
                return error_response(
                    "relative fader graph contains an invalid reference or cycle");
            return {{"success", true},
                    {"fader_level_resolution", fader_level_resolution_j(*resolution)}};
        });

    // =========================================================================
    // set_channel_pan
    // =========================================================================
    server.register_tool(
        "set_channel_pan",
        "Set a channel's pan position (-1.0 left to +1.0 right)",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"pan", {{"type", "number"}, {"description", "Pan position -1.0 to +1.0"}}}}},
         {"required", json::array({"graph_id", "channel_id", "pan"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto r = set_channel_pan(*g,
                                     ChannelStripId{detail::checked_integer<std::uint64_t>(
                                         params.at("channel_id"), "channel id")},
                                     static_cast<float>(params.at("pan").get<double>()));
            if (!r) return error_response("Channel not found or pan out of range");
            return {{"success", true}};
        });

    // =========================================================================
    // set_channel_depth
    // =========================================================================
    server.register_tool(
        "set_channel_depth",
        "Set a channel's depth position (0.0 front to 1.0 back)",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"depth", {{"type", "number"}, {"description", "Depth 0.0 (front) to 1.0 (back)"}}}}},
         {"required", json::array({"graph_id", "channel_id", "depth"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto ch_id = ChannelStripId{
                detail::checked_integer<std::uint64_t>(params.at("channel_id"), "channel id")};
            // Use set_channel_spatial with current spatial, updating depth only
            for (auto& ch : g->channels) {
                if (ch.id == ch_id) {
                    auto spatial = ch.spatial;
                    spatial.depth = static_cast<float>(params.at("depth").get<double>());
                    auto r = set_channel_spatial(*g, ch_id, spatial);
                    if (!r) return error_response("Depth out of range [0, 1]");
                    return {{"success", true}};
                }
            }
            return error_response("Channel not found");
        });

    // =========================================================================
    // set_loudness_target
    // =========================================================================
    server.register_tool(
        "set_loudness_target",
        "Set the master bus loudness target",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"integrated_lufs",
            {{"type", "number"}, {"description", "Target integrated loudness LUFS"}}},
           {"true_peak_dbfs",
            {{"type", "number"}, {"description", "True peak ceiling dBFS (default -1.0)"}}},
           {"loudness_range_lu", {{"type", "number"}, {"description", "Target loudness range LU"}}},
           {"standard",
            {{"type", "integer"},
             {"description",
              "LoudnessStandard enum: 0=StreamingLoud, 1=StreamingDynamic, 2=Broadcast, 3=Film, "
              "4=Vinyl, 5=Custom"}}}}},
         {"required", json::array({"graph_id", "integrated_lufs"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            LoudnessTarget target;
            target.integrated_lufs = static_cast<float>(params.at("integrated_lufs").get<double>());
            target.true_peak_dbfs = static_cast<float>(params.value("true_peak_dbfs", -1.0));
            if (params.contains("loudness_range_lu"))
                target.loudness_range_lu =
                    static_cast<float>(params["loudness_range_lu"].get<double>());
            auto std =
                checked_enum_or<LoudnessStandard>(params, "standard", 0, LoudnessStandard_Max);
            if (!std) return error_response("Invalid loudness standard");
            target.standard = *std;
            if (!set_loudness_target(*g, target))
                return error_response(
                    "Loudness target requires integrated_lufs <= 0 LUFS, true_peak_dbfs <= 0 "
                    "dBTP and a finite non-negative loudness_range_lu");
            return {{"success", true}};
        });

    // =========================================================================
    // add_mix_automation
    // =========================================================================
    server.register_tool(
        "add_mix_automation",
        "Add parameter automation to the mix graph",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"target",
            {{"type", "string"},
             {"description", "Automation target path (e.g. channels[1].fader.level_db)"}}},
           {"breakpoints",
            {{"type", "array"}, {"description", "Array of {bar, beat_num, beat_den, value}"}}},
           {"interpolation",
            {{"type", "integer"},
             {"description", "InterpolationMode: 0=Step, 1=Linear, 2=Smooth, 3=Exponential"}}},
           {"intent", {{"type", "string"}, {"description", "Why this automation exists"}}}}},
         {"required", json::array({"graph_id", "target", "breakpoints"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            MixAutomation automation;
            automation.target = params.at("target").get<std::string>();
            auto interp = checked_enum_or<InterpolationMode>(
                params, "interpolation", 1, InterpolationMode_Max);
            if (!interp) return error_response("Invalid automation interpolation");
            automation.interpolation = *interp;
            if (params.contains("intent")) automation.intent = params["intent"].get<std::string>();

            for (const auto& bp : params.at("breakpoints")) {
                MixAutomationBreakpoint b;
                b.time.bar = detail::checked_integer<std::uint32_t>(bp.at("bar"), "automation bar");
                const auto beat =
                    Beat::from_ratio(detail::checked_integer_or<std::int64_t>(
                                         bp, "beat_num", 0, "automation beat numerator"),
                                     detail::checked_integer_or<std::int64_t>(
                                         bp, "beat_den", 1, "automation beat denominator"));
                if (!beat) return error_response("Invalid automation beat offset");
                b.time.beat = *beat;
                b.value = static_cast<float>(bp.at("value").get<double>());
                automation.breakpoints.push_back(b);
            }
            if (!add_automation(*g, std::move(automation)))
                return error_response(
                    "Automation requires a resolvable target, at least one breakpoint, finite "
                    "values, and strictly increasing times at or after bar 1 beat 0");
            return {{"success", true}};
        });

    // =========================================================================
    // create_reference_profile
    // =========================================================================
    server.register_tool(
        "create_reference_profile",
        "Create a reference profile for mix comparison",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"name", {{"type", "string"}, {"description", "Reference name"}}},
           {"source", {{"type", "string"}, {"description", "Source file path or URI"}}},
           {"integrated_lufs",
            {{"type", "number"}, {"description", "Reference integrated loudness"}}},
           {"true_peak", {{"type", "number"}, {"description", "Reference true peak dBFS"}}},
           {"loudness_range", {{"type", "number"}, {"description", "Reference loudness range LU"}}},
           {"avg_correlation", {{"type", "number"}, {"description", "Average stereo correlation"}}},
           {"avg_width", {{"type", "number"}, {"description", "Average stereo width"}}},
           {"tonal_balance",
            {{"type", "array"}, {"description", "Tonal balance curve [{frequency, level}]"}}}}},
         {"required", json::array({"graph_id", "name"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            ReferenceProfile ref;
            ref.id = ReferenceProfileId{session->next_ref_id};
            ref.name = params.at("name").get<std::string>();
            ref.source = params.value("source", "");
            ref.loudness_profile.integrated =
                static_cast<float>(params.value("integrated_lufs", 0.0));
            ref.loudness_profile.true_peak = static_cast<float>(params.value("true_peak", 0.0));
            ref.dynamic_profile.loudness_range =
                static_cast<float>(params.value("loudness_range", 0.0));
            ref.spatial_profile.average_correlation =
                static_cast<float>(params.value("avg_correlation", 1.0));
            ref.spatial_profile.average_width = static_cast<float>(params.value("avg_width", 0.0));

            if (params.contains("tonal_balance")) {
                for (const auto& pt : params["tonal_balance"]) {
                    ref.tonal_balance_curve.push_back(
                        {static_cast<float>(pt.at("frequency").get<double>()),
                         static_cast<float>(pt.at("level").get<double>())});
                }
            }

            const auto reference_id = ref.id.value;
            add_reference_profile(*g, std::move(ref));
            ++session->next_ref_id;
            return {{"reference_id", reference_id}, {"success", true}};
        });

    // =========================================================================
    // compare_to_reference
    // =========================================================================
    server.register_tool(
        "compare_to_reference",
        "Compare the current mix against a reference profile",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"reference_id", {{"type", "integer"}, {"description", "Reference profile ID"}}}}},
         {"required", json::array({"graph_id", "reference_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            auto r = compare_to_reference(*g,
                                          ReferenceProfileId{detail::checked_integer<std::uint64_t>(
                                              params.at("reference_id"), "reference id")});
            if (!r) return error_response("Reference profile not found");

            // A null difference is unavailable: its mix side has not been
            // measured or configured, so reporting a number would invent one.
            const auto optional_number = [](const std::optional<float>& value) -> json {
                return value ? json(*value) : json(nullptr);
            };
            json spectral = nullptr;
            if (r->spectral_deviation) {
                spectral = json::array();
                for (const auto& [hz, db] : *r->spectral_deviation)
                    spectral.push_back({{"frequency", hz}, {"deviation_db", db}});
            }
            json unavailable = json::array();
            if (!r->spectral_deviation)
                unavailable.push_back(
                    {{"field", "spectral_deviation"},
                     {"reason",
                      "requires a measured mix spectrum; Sunny performs no audio analysis"}});
            if (!r->width_difference)
                unavailable.push_back({{"field", "width_difference"},
                                       {"reason", "requires a measured mix stereo width"}});
            if (!r->loudness_difference)
                unavailable.push_back({{"field", "loudness_difference"},
                                       {"reason", "no master loudness target is configured"}});
            if (!r->dynamic_range_difference)
                unavailable.push_back(
                    {{"field", "dynamic_range_difference"},
                     {"reason", "no master loudness-range target is configured"}});

            return {{"loudness_difference", optional_number(r->loudness_difference)},
                    {"dynamic_range_difference", optional_number(r->dynamic_range_difference)},
                    {"width_difference", optional_number(r->width_difference)},
                    {"spectral_deviation", spectral},
                    {"basis", "configured targets versus reference measurements"},
                    {"unavailable", unavailable}};
        });

    // =========================================================================
    // set_channel_intent
    // =========================================================================
    server.register_tool(
        "set_channel_intent",
        "Set the mixing intent for a channel (role, frequency allocation, depth)",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
           {"role",
            {{"type", "integer"},
             {"description",
              "MixRole: 0=Lead, 1=Supporting, 2=Foundation, 3=Texture, 4=Rhythmic, 5=Ambient, "
              "6=Effect, 7=Dialogue"}}},
           {"depth_position",
            {{"type", "integer"},
             {"description",
              "DepthPosition: 0=FrontClose, 1=FrontMid, 2=Mid, 3=MidFar, 4=Far, 5=VeryFar"}}},
           {"fundamental_low",
            {{"type", "number"}, {"description", "Fundamental frequency range low Hz"}}},
           {"fundamental_high",
            {{"type", "number"}, {"description", "Fundamental frequency range high Hz"}}},
           {"presence_low", {{"type", "number"}, {"description", "Presence range low Hz"}}},
           {"presence_high", {{"type", "number"}, {"description", "Presence range high Hz"}}}}},
         {"required", json::array({"graph_id", "channel_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            ChannelIntent intent;
            auto role = checked_enum_or<MixRole>(params, "role", 1, MixRole_Max);
            if (!role) return error_response("Invalid mix role");
            intent.role_in_mix = *role;
            auto depth =
                checked_enum_or<DepthPosition>(params, "depth_position", 2, DepthPosition_Max);
            if (!depth) return error_response("Invalid depth position");
            intent.depth_position = *depth;
            intent.frequency_space.fundamental_low =
                static_cast<float>(params.value("fundamental_low", 0.0));
            intent.frequency_space.fundamental_high =
                static_cast<float>(params.value("fundamental_high", 0.0));
            intent.frequency_space.presence_low =
                static_cast<float>(params.value("presence_low", 0.0));
            intent.frequency_space.presence_high =
                static_cast<float>(params.value("presence_high", 0.0));

            auto r = set_channel_intent(*g,
                                        ChannelStripId{detail::checked_integer<std::uint64_t>(
                                            params.at("channel_id"), "channel id")},
                                        std::move(intent));
            if (!r) return error_response("Channel not found");
            return {{"success", true}};
        });

    // =========================================================================
    // set_group_intent
    // =========================================================================
    server.register_tool(
        "set_group_intent",
        "Set the mixing intent for a group bus",
        {{"type", "object"},
         {"properties",
          {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
           {"group_id", {{"type", "integer"}, {"description", "Group bus ID"}}},
           {"function", {{"type", "string"}, {"description", "Group function description"}}},
           {"balance", {{"type", "string"}, {"description", "Internal balance description"}}}}},
         {"required", json::array({"graph_id", "group_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            GroupIntent intent;
            intent.function = params.value("function", "");
            intent.internal_balance_description = params.value("balance", "");

            auto r = set_group_intent(*g,
                                      GroupBusId{detail::checked_integer<std::uint64_t>(
                                          params.at("group_id"), "group id")},
                                      std::move(intent));
            if (!r) return error_response("Group bus not found");
            return {{"success", true}};
        });

    // =========================================================================
    // validate_mix
    // =========================================================================
    server.register_tool(
        "validate_mix",
        "Run all validation rules on the mix graph",
        {{"type", "object"},
         {"properties", {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}}}},
         {"required", json::array({"graph_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);

            auto diags = validate(*g);
            json arr = json::array();
            for (const auto& d : diags)
                arr.push_back(mcp_detail::encode_diagnostic(d));

            bool valid = true;
            for (const auto& d : diags) {
                if (d.severity == ValidationSeverity::Error) {
                    valid = false;
                    break;
                }
            }
            return {{"valid", valid}, {"diagnostics", arr}};
        });

    // =========================================================================
    // get_mix_json
    // =========================================================================
    server.register_tool(
        "get_mix_json",
        "Serialise the current mix graph to JSON",
        {{"type", "object"},
         {"properties", {{"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}}}},
         {"required", json::array({"graph_id"})}},
        [session](const json& params) -> json {
            const auto graph_id =
                detail::checked_integer<std::uint64_t>(params.at("graph_id"), "mix graph id");
            auto* g = session->find(graph_id);
            if (!g) return graph_not_found(graph_id);
            return {{"mix_ir", mix_to_json(*g)}};
        });
}

} // namespace sunny::infrastructure
