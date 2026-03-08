/**
 * @file MCPT002A.cpp
 * @brief MCP Tool Registration — Timbre IR implementation
 *
 * Component: MCPT002A
 *
 * Maps MCP tool calls to Timbre IR workflow functions (TIWF001A).
 * Profile state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "MCPT002A.h"

#include "Timbre/TIWF001A.h"
#include "Timbre/TISZ001A.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>

namespace Sunny::Infrastructure {

using json = nlohmann::json;
using namespace Sunny::Core;

namespace {

/// Validate that an integer is within a contiguous enum range [0, max].
/// Returns the cast enum on success, or nullopt on out-of-range.
template<typename E>
std::optional<E> checked_enum(int val, int max_val) {
    if (val < 0 || val > max_val) return std::nullopt;
    return static_cast<E>(val);
}

// Enum max values (last enumerator as int)
constexpr int WaveformType_Max          = 6;  // Custom
constexpr int PhysicalModelCategory_Max = 7;  // Bowed
constexpr int ExciterType_Max           = 4;  // Strike
constexpr int EQBandType_Max            = 4;  // HighCut
constexpr int ModulationSourceType_Max  = 13; // MacroKnob
constexpr int AutomationInterpolation_Max = 3; // Exponential
constexpr int MappingCurveType_Max      = 5;  // Custom

/// Session-scoped profile store shared across all tool handlers
struct TimbreSession {
    std::map<std::uint64_t, TimbreProfile> profiles;
    std::vector<TimbrePreset> preset_library;
    std::uint64_t next_profile_id = 1;
    std::uint64_t next_effect_id = 1;
    std::uint64_t next_preset_id = 1;

    TimbreProfile* find(std::uint64_t id) {
        auto it = profiles.find(id);
        return it != profiles.end() ? &it->second : nullptr;
    }
};

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json profile_not_found(std::uint64_t id) {
    return error_response("Profile not found: " + std::to_string(id));
}

/// Build a SoundSourceData from JSON parameters
SoundSourceData build_source(const json& params) {
    SoundSourceData src;
    auto type = params.at("source_type").get<std::string>();

    if (type == "subtractive") {
        SubtractiveSynth sub;
        int osc_count = std::max(1, params.value("oscillator_count", 1));
        for (int i = 0; i < osc_count; ++i) {
            Oscillator osc;
            auto wf = checked_enum<WaveformType>(params.value("waveform", 0), WaveformType_Max);
            if (!wf) return src;  // fall through to default
            osc.waveform.type = *wf;
            sub.oscillators.push_back(osc);
        }
        sub.amplifier.stages = {
            {static_cast<float>(params.value("attack", 5.0)), 1.0f, EnvelopeCurve::Linear},
            {static_cast<float>(params.value("decay", 100.0)), static_cast<float>(params.value("sustain", 0.7)), EnvelopeCurve::Exponential},
            {static_cast<float>(params.value("release", 200.0)), 0.0f, EnvelopeCurve::Exponential}
        };
        sub.filter.cutoff = static_cast<float>(params.value("filter_cutoff", 1000.0));
        sub.filter.resonance = static_cast<float>(params.value("filter_resonance", 0.0));
        src.data = std::move(sub);
    } else if (type == "fm") {
        FMSynth fm;
        int op_count = std::max(1, params.value("operator_count", 4));
        for (int i = 0; i < op_count; ++i) {
            FMOperator op;
            op.ratio = static_cast<float>(i + 1);
            fm.operators.push_back(op);
        }
        fm.feedback = static_cast<float>(params.value("feedback", 0.0));
        src.data = std::move(fm);
    } else if (type == "wavetable") {
        WavetableSynth wt;
        wt.wavetable = params.value("wavetable", "basic_shapes");
        wt.position = static_cast<float>(params.value("position", 0.0));
        wt.amplifier.stages = {
            {5.0f, 1.0f, EnvelopeCurve::Linear},
            {100.0f, 0.7f, EnvelopeCurve::Exponential},
            {200.0f, 0.0f, EnvelopeCurve::Exponential}
        };
        src.data = std::move(wt);
    } else if (type == "granular") {
        GranularSynth gr;
        gr.source = params.value("audio_source", "");
        gr.grain_size = static_cast<float>(params.value("grain_size", 50.0));
        gr.grain_density = static_cast<float>(params.value("grain_density", 20.0));
        gr.position = static_cast<float>(params.value("position", 0.0));
        src.data = std::move(gr);
    } else if (type == "additive") {
        AdditiveSynth add;
        int count = std::clamp(params.value("partial_count", 8), 1, 1024);
        add.partial_count = static_cast<std::uint16_t>(count);
        for (int i = 0; i < count; ++i) {
            PartialDefinition p;
            p.ratio = static_cast<float>(i + 1);
            p.amplitude = 1.0f / static_cast<float>(i + 1);
            add.partials.push_back(p);
        }
        add.global_envelope.stages = {
            {20.0f, 1.0f, EnvelopeCurve::Linear},
            {100.0f, 0.7f, EnvelopeCurve::Exponential},
            {200.0f, 0.0f, EnvelopeCurve::Exponential}
        };
        src.data = std::move(add);
    } else if (type == "physical_model") {
        PhysicalModelSource pm;
        auto cat = checked_enum<PhysicalModelCategory>(params.value("model_category", 0), PhysicalModelCategory_Max);
        if (cat) pm.model.category = *cat;
        auto exc = checked_enum<ExciterType>(params.value("exciter_type", 4), ExciterType_Max);
        if (exc) pm.exciter.type = *exc;
        pm.brightness = static_cast<float>(params.value("brightness", 0.5));
        src.data = std::move(pm);
    } else if (type == "sampler") {
        SamplerSource sam;
        sam.library = params.value("library", "");
        sam.preset = params.value("preset", "");
        src.data = std::move(sam);
    } else {
        // Default to subtractive
        SubtractiveSynth sub;
        sub.oscillators = {Oscillator{}};
        sub.amplifier.stages = {{5.0f, 1.0f, EnvelopeCurve::Linear},
                                {100.0f, 0.7f, EnvelopeCurve::Exponential},
                                {200.0f, 0.0f, EnvelopeCurve::Exponential}};
        src.data = std::move(sub);
    }

    return src;
}

/// Build an Effect from JSON parameters
Effect build_effect(const json& params, std::uint64_t effect_id) {
    Effect effect;
    effect.id = EffectId{effect_id};
    effect.enabled = params.value("enabled", true);
    effect.mix = static_cast<float>(params.value("mix", 1.0));

    auto type = params.at("effect_type").get<std::string>();

    if (type == "distortion") {
        DistortionEffect e;
        e.drive = static_cast<float>(params.value("drive", 0.5));
        e.tone = static_cast<float>(params.value("tone", 0.5));
        effect.parameters = std::move(e);
    } else if (type == "delay") {
        DelayEffect e;
        e.delay_time.ms = static_cast<float>(params.value("delay_ms", 500.0));
        e.feedback = static_cast<float>(params.value("feedback", 0.3));
        effect.parameters = std::move(e);
    } else if (type == "reverb") {
        ReverbEffect e;
        e.decay_time = static_cast<float>(params.value("decay", 1.5));
        e.pre_delay = static_cast<float>(params.value("pre_delay", 20.0));
        e.damping = static_cast<float>(params.value("damping", 0.5));
        e.size = static_cast<float>(params.value("size", 0.5));
        effect.parameters = std::move(e);
    } else if (type == "chorus") {
        ChorusEffect e;
        e.rate = static_cast<float>(params.value("rate", 1.0));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        effect.parameters = std::move(e);
    } else if (type == "phaser") {
        PhaserEffect e;
        e.rate = static_cast<float>(params.value("rate", 0.5));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        e.stages = static_cast<std::uint8_t>(std::clamp(params.value("stages", 4), 2, 24));
        effect.parameters = std::move(e);
    } else if (type == "flanger") {
        FlangerEffect e;
        e.rate = static_cast<float>(params.value("rate", 0.3));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        e.feedback = static_cast<float>(params.value("feedback", 0.5));
        effect.parameters = std::move(e);
    } else if (type == "eq") {
        EQEffect e;
        if (params.contains("bands")) {
            for (const auto& b : params["bands"]) {
                EQBand band;
                band.frequency = static_cast<float>(b.value("frequency", 1000.0));
                band.gain = static_cast<float>(b.value("gain", 0.0));
                band.q = static_cast<float>(b.value("q", 1.0));
                auto bt = checked_enum<EQBandType>(b.value("type", 0), EQBandType_Max);
                if (bt) band.band_type = *bt;
                e.bands.push_back(band);
            }
        }
        effect.parameters = std::move(e);
    } else if (type == "compressor") {
        CompressorEffect e;
        e.threshold = static_cast<float>(params.value("threshold", -20.0));
        e.ratio = static_cast<float>(params.value("ratio", 4.0));
        e.attack = static_cast<float>(params.value("attack", 10.0));
        e.release = static_cast<float>(params.value("release", 100.0));
        effect.parameters = std::move(e);
    } else {
        return effect;  // TI-17: return default-constructed effect; caller should
                        // check effect.parameters holds monostate for unrecognised types
    }

    return effect;
}

/// Serialise a diagnostic to JSON
json diagnostic_j(const Diagnostic& d) {
    return {
        {"rule", d.rule},
        {"severity", d.severity == ValidationSeverity::Error ? "error" :
                     d.severity == ValidationSeverity::Warning ? "warning" : "info"},
        {"message", d.message}
    };
}

/// Serialise semantic descriptors to JSON
json semantic_to_json(const SemanticTimbreDescriptor& d) {
    return {
        {"brightness", d.brightness},
        {"warmth", d.warmth},
        {"roughness", d.roughness},
        {"attack_character", static_cast<int>(d.attack_character)},
        {"sustain_character", static_cast<int>(d.sustain_character)},
        {"width", d.width},
        {"density", d.density},
        {"movement", d.movement},
        {"weight", d.weight},
        {"tags", d.tags},
        {"derivation", static_cast<int>(d.derivation)}
    };
}

}  // anonymous namespace

void register_timbre_tools(McpServer& server) {
    auto session = std::make_shared<TimbreSession>();

    // =========================================================================
    // create_timbre_profile
    // =========================================================================
    server.register_tool(
        "create_timbre_profile",
        "Create a new TimbreProfile for a Score IR Part",
        {
            {"type", "object"},
            {"properties", {
                {"part_id",  {{"type", "integer"}, {"description", "Score IR Part ID to bind to"}}},
                {"name",     {{"type", "string"},  {"description", "Profile name"}}}
            }},
            {"required", json::array({"part_id", "name"})}
        },
        [session](const json& params) -> json {
            auto id = session->next_profile_id++;
            auto p = wf_create_timbre_profile(
                TimbreProfileId{id},
                PartId{params.at("part_id").get<std::uint64_t>()},
                params.at("name").get<std::string>());
            session->profiles.emplace(id, std::move(p));
            return {{"profile_id", id}, {"success", true}};
        }
    );

    // =========================================================================
    // set_sound_source
    // =========================================================================
    server.register_tool(
        "set_sound_source",
        "Set or change the sound source type and parameters",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",       {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"source_type",      {{"type", "string"},  {"description", "subtractive|fm|wavetable|granular|additive|physical_model|sampler"}}},
                {"oscillator_count", {{"type", "integer"}, {"description", "Number of oscillators (subtractive)"}}},
                {"waveform",         {{"type", "integer"}, {"description", "WaveformType enum value"}}},
                {"filter_cutoff",    {{"type", "number"},  {"description", "Filter cutoff Hz"}}},
                {"filter_resonance", {{"type", "number"},  {"description", "Filter resonance 0-1"}}},
                {"attack",           {{"type", "number"},  {"description", "Envelope attack ms"}}},
                {"decay",            {{"type", "number"},  {"description", "Envelope decay ms"}}},
                {"sustain",          {{"type", "number"},  {"description", "Envelope sustain level 0-1"}}},
                {"release",          {{"type", "number"},  {"description", "Envelope release ms"}}},
                {"operator_count",   {{"type", "integer"}, {"description", "Number of FM operators"}}},
                {"feedback",         {{"type", "number"},  {"description", "FM feedback amount 0-1"}}},
                {"wavetable",        {{"type", "string"},  {"description", "Wavetable reference name"}}},
                {"position",         {{"type", "number"},  {"description", "Wavetable/granular position 0-1"}}},
                {"grain_size",       {{"type", "number"},  {"description", "Grain size ms"}}},
                {"grain_density",    {{"type", "number"},  {"description", "Grains per second"}}},
                {"partial_count",    {{"type", "integer"}, {"description", "Number of additive partials"}}},
                {"model_category",   {{"type", "integer"}, {"description", "PhysicalModelCategory enum"}}},
                {"exciter_type",     {{"type", "integer"}, {"description", "ExciterType enum"}}},
                {"brightness",       {{"type", "number"},  {"description", "Physical model brightness"}}},
                {"library",          {{"type", "string"},  {"description", "Sampler library name"}}},
                {"preset",           {{"type", "string"},  {"description", "Sampler preset name"}}},
                {"audio_source",     {{"type", "string"},  {"description", "Audio source path (granular)"}}}
            }},
            {"required", json::array({"profile_id", "source_type"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto src = build_source(params);
            auto r = wf_set_sound_source(*p, std::move(src));
            if (!r) return error_response("Failed to set sound source");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // add_effect
    // =========================================================================
    server.register_tool(
        "add_effect",
        "Add an effect to the insert chain",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",  {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"effect_type", {{"type", "string"},  {"description", "distortion|delay|reverb|chorus|phaser|flanger|eq|compressor"}}},
                {"enabled",     {{"type", "boolean"}, {"description", "Effect enabled state"}}},
                {"mix",         {{"type", "number"},  {"description", "Dry/wet mix 0-1"}}},
                {"drive",       {{"type", "number"},  {"description", "Distortion drive"}}},
                {"tone",        {{"type", "number"},  {"description", "Distortion tone"}}},
                {"delay_ms",    {{"type", "number"},  {"description", "Delay time ms"}}},
                {"feedback",    {{"type", "number"},  {"description", "Delay/flanger feedback"}}},
                {"decay",       {{"type", "number"},  {"description", "Reverb decay time"}}},
                {"pre_delay",   {{"type", "number"},  {"description", "Reverb pre-delay ms"}}},
                {"damping",     {{"type", "number"},  {"description", "Reverb damping"}}},
                {"size",        {{"type", "number"},  {"description", "Reverb room size"}}},
                {"rate",        {{"type", "number"},  {"description", "Modulation rate Hz"}}},
                {"depth",       {{"type", "number"},  {"description", "Modulation depth"}}},
                {"stages",      {{"type", "integer"}, {"description", "Phaser stages"}}},
                {"threshold",   {{"type", "number"},  {"description", "Compressor threshold dB"}}},
                {"ratio",       {{"type", "number"},  {"description", "Compressor ratio"}}},
                {"attack",      {{"type", "number"},  {"description", "Compressor attack ms"}}},
                {"release",     {{"type", "number"},  {"description", "Compressor release ms"}}},
                {"bands",       {{"type", "array"},   {"description", "EQ bands array"}}}
            }},
            {"required", json::array({"profile_id", "effect_type"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto eid = session->next_effect_id++;
            auto effect = build_effect(params, eid);
            auto r = wf_add_effect(*p, std::move(effect));
            if (!r) return error_response("Failed to add effect");
            return {{"success", true}, {"effect_id", eid}};
        }
    );

    // =========================================================================
    // remove_effect
    // =========================================================================
    server.register_tool(
        "remove_effect",
        "Remove an effect from the insert chain",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"effect_id",  {{"type", "integer"}, {"description", "Effect ID to remove"}}}
            }},
            {"required", json::array({"profile_id", "effect_id"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto r = wf_remove_effect(*p, EffectId{params.at("effect_id").get<std::uint64_t>()});
            if (!r) return error_response("Effect not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // reorder_effects
    // =========================================================================
    server.register_tool(
        "reorder_effects",
        "Change effect order in the insert chain",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"effect_ids", {{"type", "array"}, {"items", {{"type", "integer"}}},
                                {"description", "New order of effect IDs"}}}
            }},
            {"required", json::array({"profile_id", "effect_ids"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            std::vector<EffectId> ids;
            for (auto id : params.at("effect_ids"))
                ids.push_back(EffectId{id.get<std::uint64_t>()});
            auto r = wf_reorder_effects(*p, ids);
            if (!r) return error_response("Reorder failed: id mismatch");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_parameter
    // =========================================================================
    server.register_tool(
        "set_parameter",
        "Set any numeric parameter by dot-separated path",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"path",       {{"type", "string"},  {"description", "Parameter path (e.g. source.filter.cutoff)"}}},
                {"value",      {{"type", "number"},  {"description", "New value"}}}
            }},
            {"required", json::array({"profile_id", "path", "value"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto r = wf_set_parameter(*p,
                params.at("path").get<std::string>(),
                static_cast<float>(params.at("value").get<double>()));
            if (!r) return error_response("Invalid parameter path: " + params.at("path").get<std::string>());
            return {{"success", true}};
        }
    );

    // =========================================================================
    // get_parameter
    // =========================================================================
    server.register_tool(
        "get_parameter",
        "Read a numeric parameter by dot-separated path",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"path",       {{"type", "string"},  {"description", "Parameter path"}}}
            }},
            {"required", json::array({"profile_id", "path"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto r = wf_get_parameter(*p, params.at("path").get<std::string>());
            if (!r) return error_response("Invalid parameter path");
            return {{"value", *r}};
        }
    );

    // =========================================================================
    // create_macro
    // =========================================================================
    server.register_tool(
        "create_macro",
        "Create a macro knob with mappings",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"index",      {{"type", "integer"}, {"description", "Macro index (0-7)"}}},
                {"name",       {{"type", "string"},  {"description", "Macro name"}}},
                {"value",      {{"type", "number"},  {"description", "Initial value 0-1"}}},
                {"mappings",   {{"type", "array"},   {"description", "Array of {target, min, max} objects"}}}
            }},
            {"required", json::array({"profile_id", "index", "name"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            MacroKnob macro;
            macro.index = static_cast<std::uint8_t>(params.at("index").get<int>());
            macro.name = params.at("name").get<std::string>();
            macro.value = static_cast<float>(params.value("value", 0.0));
            if (params.contains("mappings")) {
                for (const auto& m : params["mappings"]) {
                    MacroMapping mapping;
                    mapping.target = m.at("target").get<std::string>();
                    mapping.min = static_cast<float>(m.value("min", 0.0));
                    mapping.max = static_cast<float>(m.value("max", 1.0));
                    macro.mappings.push_back(mapping);
                }
            }
            auto r = wf_create_macro(*p, std::move(macro));
            if (!r) return error_response("Failed to create macro");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_macro
    // =========================================================================
    server.register_tool(
        "set_macro",
        "Set a macro knob value",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",  {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"macro_index", {{"type", "integer"}, {"description", "Macro index"}}},
                {"value",       {{"type", "number"},  {"description", "New value 0-1"}}}
            }},
            {"required", json::array({"profile_id", "macro_index", "value"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto r = wf_set_macro(*p,
                static_cast<std::uint8_t>(params.at("macro_index").get<int>()),
                static_cast<float>(params.at("value").get<double>()));
            if (!r) return error_response("Macro index not found");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // add_modulation
    // =========================================================================
    server.register_tool(
        "add_modulation",
        "Add a modulation routing",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",  {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"source_type", {{"type", "integer"}, {"description", "ModulationSourceType enum"}}},
                {"source_index",{{"type", "integer"}, {"description", "Source index (LFO/envelope/step seq)"}}},
                {"target",      {{"type", "string"},  {"description", "Target parameter path"}}},
                {"depth",       {{"type", "number"},  {"description", "Modulation depth -1 to 1"}}}
            }},
            {"required", json::array({"profile_id", "source_type", "target", "depth"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            ModulationRouting routing;
            auto src_type = checked_enum<ModulationSourceType>(
                params.at("source_type").get<int>(), ModulationSourceType_Max);
            if (!src_type) return error_response("source_type out of range (0-" +
                std::to_string(ModulationSourceType_Max) + ")");
            routing.source.type = *src_type;
            routing.source.index = static_cast<std::uint8_t>(params.value("source_index", 0));
            routing.target = params.at("target").get<std::string>();
            routing.depth = static_cast<float>(params.at("depth").get<double>());
            auto r = wf_add_modulation(*p, std::move(routing));
            if (!r) return error_response("Failed to add modulation");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // add_automation
    // =========================================================================
    server.register_tool(
        "add_automation",
        "Add timbral automation",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"path",       {{"type", "string"},  {"description", "Parameter path"}}},
                {"breakpoints",{{"type", "array"},   {"description", "Array of {bar, beat_num, beat_den, value}"}}},
                {"interpolation", {{"type", "integer"}, {"description", "AutomationInterpolation enum"}}}
            }},
            {"required", json::array({"profile_id", "path", "breakpoints"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            TimbreAutomation automation;
            automation.parameter_path = params.at("path").get<std::string>();
            auto interp = checked_enum<AutomationInterpolation>(
                params.value("interpolation", 1), AutomationInterpolation_Max);
            if (!interp) return error_response("interpolation out of range (0-" +
                std::to_string(AutomationInterpolation_Max) + ")");
            automation.interpolation = *interp;
            for (const auto& bp : params.at("breakpoints")) {
                AutomationBreakpoint b;
                b.time.bar = bp.at("bar").get<std::uint32_t>();
                b.time.beat = Beat{bp.value("beat_num", static_cast<std::int64_t>(0)),
                                   bp.value("beat_den", static_cast<std::int64_t>(1))};
                b.value = static_cast<float>(bp.at("value").get<double>());
                automation.breakpoints.push_back(b);
            }
            auto r = wf_add_automation(*p, std::move(automation));
            if (!r) return error_response("Failed to add automation (empty breakpoints?)");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // set_semantic_descriptors
    // =========================================================================
    server.register_tool(
        "set_semantic_descriptors",
        "Set or update semantic descriptors",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"brightness", {{"type", "number"},  {"description", "Brightness 0-1"}}},
                {"warmth",     {{"type", "number"},  {"description", "Warmth 0-1"}}},
                {"roughness",  {{"type", "number"},  {"description", "Roughness 0-1"}}},
                {"width",      {{"type", "number"},  {"description", "Width 0-1"}}},
                {"density",    {{"type", "number"},  {"description", "Density 0-1"}}},
                {"movement",   {{"type", "number"},  {"description", "Movement 0-1"}}},
                {"weight",     {{"type", "number"},  {"description", "Weight 0-1"}}},
                {"tags",       {{"type", "array"},   {"items", {{"type", "string"}}}, {"description", "Descriptor tags"}}}
            }},
            {"required", json::array({"profile_id"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            SemanticTimbreDescriptor d = p->semantic_descriptors;
            if (params.contains("brightness")) d.brightness = static_cast<float>(params["brightness"].get<double>());
            if (params.contains("warmth"))     d.warmth = static_cast<float>(params["warmth"].get<double>());
            if (params.contains("roughness"))  d.roughness = static_cast<float>(params["roughness"].get<double>());
            if (params.contains("width"))      d.width = static_cast<float>(params["width"].get<double>());
            if (params.contains("density"))    d.density = static_cast<float>(params["density"].get<double>());
            if (params.contains("movement"))   d.movement = static_cast<float>(params["movement"].get<double>());
            if (params.contains("weight"))     d.weight = static_cast<float>(params["weight"].get<double>());
            if (params.contains("tags"))       d.tags = params["tags"].get<std::vector<std::string>>();
            wf_set_semantic_descriptors(*p, d);
            return {{"success", true}};
        }
    );

    // =========================================================================
    // analyze_timbre
    // =========================================================================
    server.register_tool(
        "analyze_timbre",
        "Derive semantic descriptors from current parameters",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}}
            }},
            {"required", json::array({"profile_id"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto d = wf_analyze_timbre(*p);
            return {{"descriptors", semantic_to_json(d)}};
        }
    );

    // =========================================================================
    // search_presets
    // =========================================================================
    server.register_tool(
        "search_presets",
        "Search the preset library by descriptors, tags, or text",
        {
            {"type", "object"},
            {"properties", {
                {"name_contains",   {{"type", "string"},  {"description", "Substring match on preset name"}}},
                {"required_tags",   {{"type", "array"},   {"items", {{"type", "string"}}}, {"description", "Required tags"}}},
                {"min_brightness",  {{"type", "number"},  {"description", "Minimum brightness"}}},
                {"max_brightness",  {{"type", "number"},  {"description", "Maximum brightness"}}},
                {"min_warmth",      {{"type", "number"},  {"description", "Minimum warmth"}}},
                {"max_warmth",      {{"type", "number"},  {"description", "Maximum warmth"}}},
                {"max_results",     {{"type", "integer"}, {"description", "Maximum results (default 10)"}}}
            }},
            {"required", json::array()}
        },
        [session](const json& params) -> json {
            PresetSearchQuery q;
            if (params.contains("name_contains"))  q.name_contains = params["name_contains"].get<std::string>();
            if (params.contains("required_tags"))   q.required_tags = params["required_tags"].get<std::vector<std::string>>();
            if (params.contains("min_brightness"))  q.min_brightness = static_cast<float>(params["min_brightness"].get<double>());
            if (params.contains("max_brightness"))  q.max_brightness = static_cast<float>(params["max_brightness"].get<double>());
            if (params.contains("min_warmth"))      q.min_warmth = static_cast<float>(params["min_warmth"].get<double>());
            if (params.contains("max_warmth"))      q.max_warmth = static_cast<float>(params["max_warmth"].get<double>());
            q.max_results = static_cast<std::size_t>(params.value("max_results", 10));

            auto results = wf_search_presets(session->preset_library, q);
            json arr = json::array();
            for (const auto& p : results) {
                arr.push_back({
                    {"id", p.id.value},
                    {"name", p.name},
                    {"tags", p.tags},
                    {"brightness", p.semantic_descriptors.brightness},
                    {"warmth", p.semantic_descriptors.warmth}
                });
            }
            return {{"presets", arr}, {"count", results.size()}};
        }
    );

    // =========================================================================
    // save_preset
    // =========================================================================
    server.register_tool(
        "save_preset",
        "Save current state as a preset",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"name",       {{"type", "string"},  {"description", "Preset name"}}},
                {"tags",       {{"type", "array"},   {"items", {{"type", "string"}}}, {"description", "Preset tags"}}}
            }},
            {"required", json::array({"profile_id", "name"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto pid = session->next_preset_id++;
            auto preset = wf_save_preset(*p, TimbrePresetId{pid}, params.at("name").get<std::string>());
            if (params.contains("tags")) preset.tags = params["tags"].get<std::vector<std::string>>();
            session->preset_library.push_back(preset);
            return {{"success", true}, {"preset_id", pid}};
        }
    );

    // =========================================================================
    // load_preset
    // =========================================================================
    server.register_tool(
        "load_preset",
        "Apply a preset to a TimbreProfile",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"preset_id",  {{"type", "integer"}, {"description", "Preset ID to load"}}}
            }},
            {"required", json::array({"profile_id", "preset_id"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            auto preset_id = params.at("preset_id").get<std::uint64_t>();
            const TimbrePreset* preset = nullptr;
            for (const auto& pr : session->preset_library) {
                if (pr.id.value == preset_id) { preset = &pr; break; }
            }
            if (!preset) return error_response("Preset not found: " + std::to_string(preset_id));
            auto r = wf_load_preset(*p, *preset);
            if (!r) return error_response("Failed to load preset");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // morph_presets
    // =========================================================================
    server.register_tool(
        "morph_presets",
        "Set up a preset morph over a score time range",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",     {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"from_preset_id", {{"type", "integer"}, {"description", "Starting preset ID"}}},
                {"to_preset_id",   {{"type", "integer"}, {"description", "Ending preset ID"}}},
                {"start_bar",      {{"type", "integer"}, {"description", "Start bar number"}}},
                {"end_bar",        {{"type", "integer"}, {"description", "End bar number"}}},
                {"curve_type",     {{"type", "integer"}, {"description", "MappingCurveType enum (default: linear)"}}}
            }},
            {"required", json::array({"profile_id", "from_preset_id", "to_preset_id", "start_bar", "end_bar"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            PresetMorph morph;
            morph.from_preset = TimbrePresetId{params.at("from_preset_id").get<std::uint64_t>()};
            morph.to_preset = TimbrePresetId{params.at("to_preset_id").get<std::uint64_t>()};
            morph.start = ScoreTime{params.at("start_bar").get<std::uint32_t>(), Beat{0, 1}};
            morph.end = ScoreTime{params.at("end_bar").get<std::uint32_t>(), Beat{0, 1}};
            auto ct = checked_enum<MappingCurveType>(params.value("curve_type", 0), MappingCurveType_Max);
            if (ct) morph.curve.type = *ct;
            auto r = wf_morph_presets(*p, std::move(morph));
            if (!r) return error_response("Failed to add morph");
            return {{"success", true}};
        }
    );

    // =========================================================================
    // validate_timbre
    // =========================================================================
    server.register_tool(
        "validate_timbre",
        "Run validation on a TimbreProfile",
        {
            {"type", "object"},
            {"properties", {
                {"profile_id",    {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
                {"sample_rate",   {{"type", "number"},  {"description", "Sample rate Hz (default 44100)"}}}
            }},
            {"required", json::array({"profile_id"})}
        },
        [session](const json& params) -> json {
            auto* p = session->find(params.at("profile_id").get<std::uint64_t>());
            if (!p) return profile_not_found(params.at("profile_id").get<std::uint64_t>());
            float sr = static_cast<float>(params.value("sample_rate", 44100.0));
            auto diags = wf_validate(*p, sr);
            json arr = json::array();
            for (const auto& d : diags) arr.push_back(diagnostic_j(d));
            bool valid = true;
            for (const auto& d : diags) {
                if (d.severity == ValidationSeverity::Error) { valid = false; break; }
            }
            return {{"valid", valid}, {"diagnostics", arr}};
        }
    );
}

}  // namespace Sunny::Infrastructure
