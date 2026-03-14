/**
 * @file MCPT003A.cpp
 * @brief MCP Tool Registration — Mix IR implementation
 *
 * Component: MCPT003A
 *
 * Maps MCP tool calls to Mix IR workflow functions (MIWF001A).
 * Graph state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "MCPT003A.h"

#include "Mix/MIWF001A.h"
#include "Mix/MISZ001A.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>

namespace Sunny::Infrastructure {

using json = nlohmann::json;
using namespace Sunny::Core;

namespace {

/// Validate that an integer is within a contiguous enum range [0, max].
template<typename E>
std::optional<E> checked_enum(int val, int max_val) {
    if (val < 0 || val > max_val) return std::nullopt;
    return static_cast<E>(val);
}

// Enum max values
constexpr int OutputFormat_Max       = 6;  // Binaural
constexpr int MixRole_Max            = 7;  // Dialogue
constexpr int DepthPosition_Max      = 5;  // VeryFar
constexpr int MixEQBandType_Max      = 5;  // TiltShelf
constexpr int DetectionMode_Max      = 2;  // Envelope
constexpr int CompressorTopology_Max = 1;  // FeedBack
constexpr int SidechainSourceType_Max = 2; // ExternalBus
constexpr int LimiterAlgorithm_Max   = 2;  // ISP
constexpr int SaturationTypeTag_Max  = 5;  // Hard
constexpr int InterpolationMode_Max  = 3;  // Exponential
constexpr int LoudnessStandard_Max   = 5;  // Custom
constexpr int SeatingTemplate_Max    = 1;  // European

/// Session-scoped graph store shared across all tool handlers
struct MixSession {
    std::map<std::uint64_t, MixGraph> graphs;
    std::uint64_t next_graph_id = 1;
    std::uint64_t next_group_id = 1;
    std::uint64_t next_aux_id = 1;
    std::uint64_t next_effect_id = 1;
    std::uint64_t next_ref_id = 1;

    MixGraph* find(std::uint64_t id) {
        auto it = graphs.find(id);
        return it != graphs.end() ? &it->second : nullptr;
    }
};

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json graph_not_found(std::uint64_t id) {
    return error_response("Mix graph not found: " + std::to_string(id));
}

/// Serialise a validation diagnostic to JSON
json diagnostic_j(const Diagnostic& d) {
    return {
        {"rule", d.rule},
        {"severity", d.severity == ValidationSeverity::Error ? "error" :
                     d.severity == ValidationSeverity::Warning ? "warning" : "info"},
        {"message", d.message}
    };
}

/// Build a MixEQ from JSON parameters
MixEQ build_eq(const json& params) {
    MixEQ eq;
    eq.linear_phase = params.value("linear_phase", false);
    eq.auto_gain = params.value("auto_gain", false);
    if (params.contains("bands")) {
        for (const auto& b : params["bands"]) {
            MixEQBand band;
            band.frequency = static_cast<float>(b.value("frequency", 1000.0));
            band.gain = static_cast<float>(b.value("gain", 0.0));
            band.q = static_cast<float>(b.value("q", 1.0));
            auto bt = checked_enum<MixEQBandType>(b.value("type", 0), MixEQBandType_Max);
            if (bt) band.band_type = *bt;
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
MixCompressor build_compressor(const json& params) {
    MixCompressor comp;
    comp.threshold = static_cast<float>(params.value("threshold", -20.0));
    comp.ratio = static_cast<float>(params.value("ratio", 4.0));
    comp.attack = static_cast<float>(params.value("attack", 10.0));
    comp.release = static_cast<float>(params.value("release", 100.0));
    comp.knee = static_cast<float>(params.value("knee", 0.0));
    comp.makeup_gain = static_cast<float>(params.value("makeup_gain", 0.0));
    auto det = checked_enum<DetectionMode>(params.value("detection", 1), DetectionMode_Max);
    if (det) comp.detection = *det;
    auto top = checked_enum<CompressorTopology>(params.value("topology", 0), CompressorTopology_Max);
    if (top) comp.topology = *top;
    comp.stereo_link = static_cast<float>(params.value("stereo_link", 1.0));

    if (params.contains("sidechain_source")) {
        auto src = checked_enum<SidechainSourceType>(
            params.value("sidechain_source", 0), SidechainSourceType_Max);
        if (src) comp.sidechain.source = *src;
        comp.sidechain.channel_id = ChannelStripId{
            params.value("sidechain_channel_id", static_cast<std::uint64_t>(0))};
        comp.sidechain.bus_id = GroupBusId{
            params.value("sidechain_bus_id", static_cast<std::uint64_t>(0))};
    }
    return comp;
}

/// Build a MixEffect from JSON parameters
MixEffect build_effect(const json& params, std::uint64_t effect_id) {
    MixEffect effect;
    effect.id = MixEffectId{effect_id};
    effect.enabled = params.value("enabled", true);

    auto type = params.at("effect_type").get<std::string>();

    if (type == "eq") {
        effect.parameters = build_eq(params);
    } else if (type == "compressor") {
        effect.parameters = build_compressor(params);
    } else if (type == "gate") {
        MixGate gate;
        gate.threshold = static_cast<float>(params.value("threshold", -40.0));
        gate.ratio = static_cast<float>(params.value("ratio", 10.0));
        gate.attack = static_cast<float>(params.value("attack", 0.5));
        gate.hold = static_cast<float>(params.value("hold", 50.0));
        gate.release = static_cast<float>(params.value("release", 100.0));
        gate.range = static_cast<float>(params.value("range", -80.0));
        effect.parameters = std::move(gate);
    } else if (type == "limiter") {
        MixLimiter lim;
        lim.ceiling = static_cast<float>(params.value("ceiling", -1.0));
        lim.release = static_cast<float>(params.value("release", 100.0));
        lim.lookahead = static_cast<float>(params.value("lookahead", 5.0));
        auto alg = checked_enum<LimiterAlgorithm>(params.value("algorithm", 1), LimiterAlgorithm_Max);
        if (alg) lim.algorithm = *alg;
        effect.parameters = std::move(lim);
    } else if (type == "saturation") {
        MixSaturation sat;
        sat.drive = static_cast<float>(params.value("drive", 0.0));
        sat.mix = static_cast<float>(params.value("mix", 1.0));
        sat.output_level = static_cast<float>(params.value("output_level", 0.0));
        auto st = checked_enum<SaturationTypeTag>(params.value("saturation_type", 0), SaturationTypeTag_Max);
        if (st) sat.algorithm.type = *st;
        effect.parameters = std::move(sat);
    } else if (type == "stereo") {
        MixStereoProcessor stereo;
        stereo.width = static_cast<float>(params.value("width", 1.0));
        stereo.mid_side_balance = static_cast<float>(params.value("mid_side_balance", 0.5));
        if (params.contains("mono_below"))
            stereo.mono_below = static_cast<float>(params["mono_below"].get<double>());
        effect.parameters = std::move(stereo);
    } else if (type == "multiband") {
        MixMultibandDynamics mb;
        if (params.contains("crossover_frequencies")) {
            for (auto f : params["crossover_frequencies"])
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
    } else {
        // Default to EQ for unrecognised types
        effect.parameters = MixEQ{};
    }

    return effect;
}

}  // anonymous namespace

void register_mix_tools(McpServer& server) {
    auto session = std::make_shared<MixSession>();

    // =========================================================================
    // create_mix_graph
    // =========================================================================
    server.register_tool(
        "create_mix_graph",
        "Initialise a Mix IR graph with one ChannelStrip per Score IR Part",
        {
            {"type", "object"},
            {"properties", {
                {"part_ids", {{"type", "array"}, {"items", {{"type", "integer"}}},
                              {"description", "Score IR Part IDs to create channels for"}}}
            }},
            {"required", json::array({"part_ids"})}
        },
        [session](const json& params) -> json {
            auto gid = session->next_graph_id++;
            std::vector<PartId> parts;
            for (auto pid : params.at("part_ids"))
                parts.push_back(PartId{pid.get<std::uint64_t>()});
            auto graph = wf_create_mix_graph(MixGraphId{gid}, parts);
            session->graphs.emplace(gid, std::move(graph));
            return {{"graph_id", gid}, {"channel_count", parts.size()}, {"success", true}};
        }
    );

    // =========================================================================
    // create_group_bus
    // =========================================================================
    server.register_tool(
        "create_group_bus",
        "Create a group bus and assign channels to it",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",          {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"name",              {{"type", "string"},  {"description", "Group bus name (e.g. 'Strings')"}}},
                {"member_channel_ids",{{"type", "array"},   {"items", {{"type", "integer"}}},
                                       {"description", "Channel IDs to assign to this group"}}}
            }},
            {"required", json::array({"graph_id", "name"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            auto gid = GroupBusId{session->next_group_id++};
            std::vector<ChannelStripId> members;
            if (params.contains("member_channel_ids")) {
                for (auto id : params["member_channel_ids"])
                    members.push_back(ChannelStripId{id.get<std::uint64_t>()});
            }
            auto r = wf_create_group_bus(*g, gid, params.at("name").get<std::string>(), members);
            if (!r) return error_response("Failed to create group bus");
            return {{"group_bus_id", gid.value}, {"success", true}};
        }
    );

    // =========================================================================
    // create_aux_bus
    // =========================================================================
    server.register_tool(
        "create_aux_bus",
        "Create an auxiliary bus (reverb, delay, etc.)",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"name",     {{"type", "string"},  {"description", "Aux bus name (e.g. 'Concert Hall')"}}}
            }},
            {"required", json::array({"graph_id", "name"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            auto aid = AuxBusId{session->next_aux_id++};
            auto r = wf_create_aux_bus(*g, aid, params.at("name").get<std::string>());
            if (!r) return error_response("Failed to create aux bus");
            return {{"aux_bus_id", aid.value}, {"success", true}};
        }
    );

    // =========================================================================
    // assign_channel_to_group
    // =========================================================================
    server.register_tool(
        "assign_channel_to_group",
        "Route a channel to a group bus",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"group_id",   {{"type", "integer"}, {"description", "Target group bus ID"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "group_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto r = wf_assign_channel_to_group(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                GroupBusId{params.at("group_id").get<std::uint64_t>()});
            if (!r) return error_response("Channel or group not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_channel_send
    // =========================================================================
    server.register_tool(
        "set_channel_send",
        "Set a channel's send level to an aux bus",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"aux_id",     {{"type", "integer"}, {"description", "Aux bus ID"}}},
                {"level_db",   {{"type", "number"},  {"description", "Send level in dB"}}},
                {"pre_fader",  {{"type", "boolean"}, {"description", "Pre-fader send (default false)"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "aux_id", "level_db"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto r = wf_set_channel_send(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                AuxBusId{params.at("aux_id").get<std::uint64_t>()},
                static_cast<float>(params.at("level_db").get<double>()),
                params.value("pre_fader", false));
            if (!r) return error_response("Channel or aux bus not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // apply_seating_template
    // =========================================================================
    server.register_tool(
        "apply_seating_template",
        "Apply an orchestral seating preset to spatial positions",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"template", {{"type", "integer"}, {"description", "0=American, 1=European"}}}
            }},
            {"required", json::array({"graph_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto tmpl = checked_enum<SeatingTemplate>(
                params.value("template", 0), SeatingTemplate_Max);
            if (!tmpl) return error_response("Invalid seating template");
            wf_apply_seating_template(*g, *tmpl);
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_output_format
    // =========================================================================
    server.register_tool(
        "set_output_format",
        "Set stereo, surround, or immersive output format",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"format",   {{"type", "integer"}, {"description", "OutputFormat enum: 0=Stereo, 1=LCR, 2=Quad, 3=5.1, 4=7.1, 5=Atmos, 6=Binaural"}}}
            }},
            {"required", json::array({"graph_id", "format"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto fmt = checked_enum<OutputFormat>(params.at("format").get<int>(), OutputFormat_Max);
            if (!fmt) return error_response("Invalid output format");
            wf_set_output_format(*g, *fmt);
            return {{"success", true}};
        }
    );

    // =========================================================================
    // add_channel_effect
    // =========================================================================
    server.register_tool(
        "add_channel_effect",
        "Add an EQ, compressor, gate, limiter, saturation, or stereo effect to a channel",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",    {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id",  {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"effect_type", {{"type", "string"},  {"description", "eq|compressor|gate|limiter|saturation|stereo|multiband"}}},
                {"enabled",     {{"type", "boolean"}, {"description", "Effect enabled (default true)"}}},
                {"bands",       {{"type", "array"},   {"description", "EQ bands: [{frequency, gain, q, type, dynamic}]"}}},
                {"linear_phase",{{"type", "boolean"}, {"description", "Linear phase EQ"}}},
                {"auto_gain",   {{"type", "boolean"}, {"description", "Auto gain EQ"}}},
                {"threshold",   {{"type", "number"},  {"description", "Compressor/gate threshold dB"}}},
                {"ratio",       {{"type", "number"},  {"description", "Compressor/gate ratio"}}},
                {"attack",      {{"type", "number"},  {"description", "Attack ms"}}},
                {"release",     {{"type", "number"},  {"description", "Release ms"}}},
                {"knee",        {{"type", "number"},  {"description", "Compressor knee dB"}}},
                {"makeup_gain", {{"type", "number"},  {"description", "Compressor makeup gain dB"}}},
                {"ceiling",     {{"type", "number"},  {"description", "Limiter ceiling dBFS"}}},
                {"drive",       {{"type", "number"},  {"description", "Saturation drive 0-1"}}},
                {"mix",         {{"type", "number"},  {"description", "Dry/wet mix 0-1"}}},
                {"width",       {{"type", "number"},  {"description", "Stereo width"}}},
                {"mono_below",  {{"type", "number"},  {"description", "Mono below Hz"}}},
                {"sidechain_source",     {{"type", "integer"}, {"description", "SidechainSourceType: 0=Internal, 1=ExternalChannel, 2=ExternalBus"}}},
                {"sidechain_channel_id", {{"type", "integer"}, {"description", "Sidechain source channel ID"}}},
                {"sidechain_bus_id",     {{"type", "integer"}, {"description", "Sidechain source bus ID"}}},
                {"crossover_frequencies",{{"type", "array"},   {"description", "Multiband crossover points Hz"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "effect_type"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto eid = session->next_effect_id++;
            auto effect = build_effect(params, eid);
            auto r = wf_add_channel_effect(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                std::move(effect));
            if (!r) return error_response("Channel not found");
            return {{"effect_id", eid}, {"success", true}};
        }
    );

    // =========================================================================
    // add_bus_effect
    // =========================================================================
    server.register_tool(
        "add_bus_effect",
        "Add processing to a group bus",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",    {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"group_id",    {{"type", "integer"}, {"description", "Group bus ID"}}},
                {"effect_type", {{"type", "string"},  {"description", "eq|compressor|gate|limiter|saturation|stereo|multiband"}}},
                {"enabled",     {{"type", "boolean"}, {"description", "Effect enabled"}}},
                {"threshold",   {{"type", "number"},  {"description", "Compressor/gate threshold dB"}}},
                {"ratio",       {{"type", "number"},  {"description", "Ratio"}}},
                {"attack",      {{"type", "number"},  {"description", "Attack ms"}}},
                {"release",     {{"type", "number"},  {"description", "Release ms"}}},
                {"bands",       {{"type", "array"},   {"description", "EQ bands"}}},
                {"drive",       {{"type", "number"},  {"description", "Saturation drive"}}},
                {"mix",         {{"type", "number"},  {"description", "Dry/wet"}}},
                {"width",       {{"type", "number"},  {"description", "Stereo width"}}},
                {"crossover_frequencies", {{"type", "array"}, {"description", "Multiband crossovers"}}}
            }},
            {"required", json::array({"graph_id", "group_id", "effect_type"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto eid = session->next_effect_id++;
            auto effect = build_effect(params, eid);
            auto r = wf_add_bus_effect(*g,
                GroupBusId{params.at("group_id").get<std::uint64_t>()},
                std::move(effect));
            if (!r) return error_response("Group bus not found");
            return {{"effect_id", eid}, {"success", true}};
        }
    );

    // =========================================================================
    // add_aux_effect
    // =========================================================================
    server.register_tool(
        "add_aux_effect",
        "Add processing to an aux bus effect chain",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",    {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"aux_id",      {{"type", "integer"}, {"description", "Aux bus ID"}}},
                {"effect_type", {{"type", "string"},  {"description", "eq|compressor|gate|limiter|saturation|stereo|multiband"}}},
                {"enabled",     {{"type", "boolean"}, {"description", "Effect enabled"}}},
                {"threshold",   {{"type", "number"},  {"description", "Threshold dB"}}},
                {"ratio",       {{"type", "number"},  {"description", "Ratio"}}},
                {"attack",      {{"type", "number"},  {"description", "Attack ms"}}},
                {"release",     {{"type", "number"},  {"description", "Release ms"}}},
                {"bands",       {{"type", "array"},   {"description", "EQ bands"}}},
                {"drive",       {{"type", "number"},  {"description", "Saturation drive"}}},
                {"mix",         {{"type", "number"},  {"description", "Dry/wet"}}},
                {"width",       {{"type", "number"},  {"description", "Stereo width"}}},
                {"crossover_frequencies", {{"type", "array"}, {"description", "Multiband crossovers"}}}
            }},
            {"required", json::array({"graph_id", "aux_id", "effect_type"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto eid = session->next_effect_id++;
            auto effect = build_effect(params, eid);
            auto r = wf_add_aux_effect(*g,
                AuxBusId{params.at("aux_id").get<std::uint64_t>()},
                std::move(effect));
            if (!r) return error_response("Aux bus not found");
            return {{"effect_id", eid}, {"success", true}};
        }
    );

    // =========================================================================
    // add_master_effect
    // =========================================================================
    server.register_tool(
        "add_master_effect",
        "Add processing to the master bus chain",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",    {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"effect_type", {{"type", "string"},  {"description", "eq|compressor|gate|limiter|saturation|stereo|multiband"}}},
                {"enabled",     {{"type", "boolean"}, {"description", "Effect enabled"}}},
                {"threshold",   {{"type", "number"},  {"description", "Threshold dB"}}},
                {"ratio",       {{"type", "number"},  {"description", "Ratio"}}},
                {"attack",      {{"type", "number"},  {"description", "Attack ms"}}},
                {"release",     {{"type", "number"},  {"description", "Release ms"}}},
                {"bands",       {{"type", "array"},   {"description", "EQ bands"}}},
                {"ceiling",     {{"type", "number"},  {"description", "Limiter ceiling"}}},
                {"drive",       {{"type", "number"},  {"description", "Saturation drive"}}},
                {"mix",         {{"type", "number"},  {"description", "Dry/wet"}}},
                {"width",       {{"type", "number"},  {"description", "Stereo width"}}},
                {"crossover_frequencies", {{"type", "array"}, {"description", "Multiband crossovers"}}}
            }},
            {"required", json::array({"graph_id", "effect_type"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto eid = session->next_effect_id++;
            auto effect = build_effect(params, eid);
            wf_add_master_effect(*g, std::move(effect));
            return {{"effect_id", eid}, {"success", true}};
        }
    );

    // =========================================================================
    // set_channel_level
    // =========================================================================
    server.register_tool(
        "set_channel_level",
        "Set a channel's fader level in dB",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"level_db",   {{"type", "number"},  {"description", "Fader level in dB"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "level_db"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto r = wf_set_channel_level(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                static_cast<float>(params.at("level_db").get<double>()));
            if (!r) return error_response("Channel not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_channel_pan
    // =========================================================================
    server.register_tool(
        "set_channel_pan",
        "Set a channel's pan position (-1.0 left to +1.0 right)",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"pan",        {{"type", "number"},  {"description", "Pan position -1.0 to +1.0"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "pan"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto r = wf_set_channel_pan(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                static_cast<float>(params.at("pan").get<double>()));
            if (!r) return error_response("Channel not found or pan out of range");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_channel_depth
    // =========================================================================
    server.register_tool(
        "set_channel_depth",
        "Set a channel's depth position (0.0 front to 1.0 back)",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id", {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"depth",      {{"type", "number"},  {"description", "Depth 0.0 (front) to 1.0 (back)"}}}
            }},
            {"required", json::array({"graph_id", "channel_id", "depth"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto ch_id = ChannelStripId{params.at("channel_id").get<std::uint64_t>()};
            // Use wf_set_channel_spatial with current spatial, updating depth only
            for (auto& ch : g->channels) {
                if (ch.id == ch_id) {
                    auto spatial = ch.spatial;
                    spatial.depth = static_cast<float>(params.at("depth").get<double>());
                    auto r = wf_set_channel_spatial(*g, ch_id, spatial);
                    if (!r) return error_response("Depth out of range [0, 1]");
                    return {{"success", true}};
                }
            }
            return error_response("Channel not found");
        }
    );

    // =========================================================================
    // set_loudness_target
    // =========================================================================
    server.register_tool(
        "set_loudness_target",
        "Set the master bus loudness target",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",         {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"integrated_lufs",  {{"type", "number"},  {"description", "Target integrated loudness LUFS"}}},
                {"true_peak_dbfs",   {{"type", "number"},  {"description", "True peak ceiling dBFS (default -1.0)"}}},
                {"loudness_range_lu",{{"type", "number"},  {"description", "Target loudness range LU"}}},
                {"standard",         {{"type", "integer"}, {"description", "LoudnessStandard enum: 0=StreamingLoud, 1=StreamingDynamic, 2=Broadcast, 3=Film, 4=Vinyl, 5=Custom"}}}
            }},
            {"required", json::array({"graph_id", "integrated_lufs"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            LoudnessTarget target;
            target.integrated_lufs = static_cast<float>(params.at("integrated_lufs").get<double>());
            target.true_peak_dbfs = static_cast<float>(params.value("true_peak_dbfs", -1.0));
            if (params.contains("loudness_range_lu"))
                target.loudness_range_lu = static_cast<float>(params["loudness_range_lu"].get<double>());
            auto std = checked_enum<LoudnessStandard>(params.value("standard", 0), LoudnessStandard_Max);
            if (std) target.standard = *std;
            wf_set_loudness_target(*g, target);
            return {{"success", true}};
        }
    );

    // =========================================================================
    // add_mix_automation
    // =========================================================================
    server.register_tool(
        "add_mix_automation",
        "Add parameter automation to the mix graph",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",      {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"target",        {{"type", "string"},  {"description", "Automation target path (e.g. channels[1].fader.level_db)"}}},
                {"breakpoints",   {{"type", "array"},   {"description", "Array of {bar, beat_num, beat_den, value}"}}},
                {"interpolation", {{"type", "integer"}, {"description", "InterpolationMode: 0=Step, 1=Linear, 2=Smooth, 3=Exponential"}}},
                {"intent",        {{"type", "string"},  {"description", "Why this automation exists"}}}
            }},
            {"required", json::array({"graph_id", "target", "breakpoints"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            MixAutomation automation;
            automation.target = params.at("target").get<std::string>();
            auto interp = checked_enum<InterpolationMode>(
                params.value("interpolation", 1), InterpolationMode_Max);
            if (interp) automation.interpolation = *interp;
            if (params.contains("intent"))
                automation.intent = params["intent"].get<std::string>();

            for (const auto& bp : params.at("breakpoints")) {
                MixAutomationBreakpoint b;
                b.time.bar = bp.at("bar").get<std::uint32_t>();
                b.time.beat = Beat{bp.value("beat_num", static_cast<std::int64_t>(0)),
                                   bp.value("beat_den", static_cast<std::int64_t>(1))};
                b.value = static_cast<float>(bp.at("value").get<double>());
                automation.breakpoints.push_back(b);
            }
            wf_add_automation(*g, std::move(automation));
            return {{"success", true}};
        }
    );

    // =========================================================================
    // create_reference_profile
    // =========================================================================
    server.register_tool(
        "create_reference_profile",
        "Create a reference profile for mix comparison",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",        {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"name",            {{"type", "string"},  {"description", "Reference name"}}},
                {"source",          {{"type", "string"},  {"description", "Source file path or URI"}}},
                {"integrated_lufs", {{"type", "number"},  {"description", "Reference integrated loudness"}}},
                {"true_peak",       {{"type", "number"},  {"description", "Reference true peak dBFS"}}},
                {"loudness_range",  {{"type", "number"},  {"description", "Reference loudness range LU"}}},
                {"avg_correlation", {{"type", "number"},  {"description", "Average stereo correlation"}}},
                {"avg_width",       {{"type", "number"},  {"description", "Average stereo width"}}},
                {"tonal_balance",   {{"type", "array"},   {"description", "Tonal balance curve [{frequency, level}]"}}}
            }},
            {"required", json::array({"graph_id", "name"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            ReferenceProfile ref;
            ref.id = ReferenceProfileId{session->next_ref_id++};
            ref.name = params.at("name").get<std::string>();
            ref.source = params.value("source", "");
            ref.loudness_profile.integrated = static_cast<float>(params.value("integrated_lufs", 0.0));
            ref.loudness_profile.true_peak = static_cast<float>(params.value("true_peak", 0.0));
            ref.dynamic_profile.loudness_range = static_cast<float>(params.value("loudness_range", 0.0));
            ref.spatial_profile.average_correlation = static_cast<float>(params.value("avg_correlation", 1.0));
            ref.spatial_profile.average_width = static_cast<float>(params.value("avg_width", 0.0));

            if (params.contains("tonal_balance")) {
                for (const auto& pt : params["tonal_balance"]) {
                    ref.tonal_balance_curve.push_back({
                        static_cast<float>(pt.at("frequency").get<double>()),
                        static_cast<float>(pt.at("level").get<double>())
                    });
                }
            }

            wf_add_reference_profile(*g, std::move(ref));
            return {{"reference_id", ref.id.value}, {"success", true}};
        }
    );

    // =========================================================================
    // compare_to_reference
    // =========================================================================
    server.register_tool(
        "compare_to_reference",
        "Compare the current mix against a reference profile",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",     {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"reference_id", {{"type", "integer"}, {"description", "Reference profile ID"}}}
            }},
            {"required", json::array({"graph_id", "reference_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            auto r = wf_compare_to_reference(*g,
                ReferenceProfileId{params.at("reference_id").get<std::uint64_t>()});
            if (!r) return error_response("Reference profile not found");

            json spectral = json::array();
            for (const auto& [hz, db] : r->spectral_deviation)
                spectral.push_back({{"frequency", hz}, {"deviation_db", db}});

            return {
                {"loudness_difference", r->loudness_difference},
                {"dynamic_range_difference", r->dynamic_range_difference},
                {"width_difference", r->width_difference},
                {"spectral_deviation", spectral}
            };
        }
    );

    // =========================================================================
    // set_channel_intent
    // =========================================================================
    server.register_tool(
        "set_channel_intent",
        "Set the mixing intent for a channel (role, frequency allocation, depth)",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",        {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"channel_id",      {{"type", "integer"}, {"description", "Channel strip ID"}}},
                {"role",            {{"type", "integer"}, {"description", "MixRole: 0=Lead, 1=Supporting, 2=Foundation, 3=Texture, 4=Rhythmic, 5=Ambient, 6=Effect, 7=Dialogue"}}},
                {"depth_position",  {{"type", "integer"}, {"description", "DepthPosition: 0=FrontClose, 1=FrontMid, 2=Mid, 3=MidFar, 4=Far, 5=VeryFar"}}},
                {"fundamental_low", {{"type", "number"},  {"description", "Fundamental frequency range low Hz"}}},
                {"fundamental_high",{{"type", "number"},  {"description", "Fundamental frequency range high Hz"}}},
                {"presence_low",    {{"type", "number"},  {"description", "Presence range low Hz"}}},
                {"presence_high",   {{"type", "number"},  {"description", "Presence range high Hz"}}}
            }},
            {"required", json::array({"graph_id", "channel_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            ChannelIntent intent;
            auto role = checked_enum<MixRole>(params.value("role", 1), MixRole_Max);
            if (role) intent.role_in_mix = *role;
            auto depth = checked_enum<DepthPosition>(params.value("depth_position", 2), DepthPosition_Max);
            if (depth) intent.depth_position = *depth;
            intent.frequency_space.fundamental_low = static_cast<float>(params.value("fundamental_low", 0.0));
            intent.frequency_space.fundamental_high = static_cast<float>(params.value("fundamental_high", 0.0));
            intent.frequency_space.presence_low = static_cast<float>(params.value("presence_low", 0.0));
            intent.frequency_space.presence_high = static_cast<float>(params.value("presence_high", 0.0));

            auto r = wf_set_channel_intent(*g,
                ChannelStripId{params.at("channel_id").get<std::uint64_t>()},
                std::move(intent));
            if (!r) return error_response("Channel not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_group_intent
    // =========================================================================
    server.register_tool(
        "set_group_intent",
        "Set the mixing intent for a group bus",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id",   {{"type", "integer"}, {"description", "Mix graph ID"}}},
                {"group_id",   {{"type", "integer"}, {"description", "Group bus ID"}}},
                {"function",   {{"type", "string"},  {"description", "Group function description"}}},
                {"balance",    {{"type", "string"},  {"description", "Internal balance description"}}}
            }},
            {"required", json::array({"graph_id", "group_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            GroupIntent intent;
            intent.function = params.value("function", "");
            intent.internal_balance_description = params.value("balance", "");

            auto r = wf_set_group_intent(*g,
                GroupBusId{params.at("group_id").get<std::uint64_t>()},
                std::move(intent));
            if (!r) return error_response("Group bus not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // validate_mix
    // =========================================================================
    server.register_tool(
        "validate_mix",
        "Run all validation rules on the mix graph",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}}
            }},
            {"required", json::array({"graph_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());

            auto diags = wf_validate(*g);
            json arr = json::array();
            for (const auto& d : diags) arr.push_back(diagnostic_j(d));

            bool valid = true;
            for (const auto& d : diags) {
                if (d.severity == ValidationSeverity::Error) { valid = false; break; }
            }
            return {{"valid", valid}, {"diagnostics", arr}};
        }
    );

    // =========================================================================
    // get_mix_json
    // =========================================================================
    server.register_tool(
        "get_mix_json",
        "Serialise the current mix graph to JSON",
        {
            {"type", "object"},
            {"properties", {
                {"graph_id", {{"type", "integer"}, {"description", "Mix graph ID"}}}
            }},
            {"required", json::array({"graph_id"})}
        },
        [session](const json& params) -> json {
            auto* g = session->find(params.at("graph_id").get<std::uint64_t>());
            if (!g) return graph_not_found(params.at("graph_id").get<std::uint64_t>());
            return {{"mix_ir", mix_to_json(*g)}};
        }
    );
}

}  // namespace Sunny::Infrastructure
