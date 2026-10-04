/**
 * @file timbre_tools.cpp
 * @brief MCP Tool Registration — Timbre IR implementation
 *
 *
 * Maps MCP tool calls to Timbre IR workflow functions.
 * Profile state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "evidence_encoding.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

/// Validate that an integer is within a contiguous enum range [0, max].
/// Returns the cast enum on success, or nullopt on out-of-range.
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

// Enum max values (last enumerator as int)
constexpr int WaveformType_Max = 6;            // Custom
constexpr int PhysicalModelCategory_Max = 7;   // Bowed
constexpr int ExciterType_Max = 4;             // Strike
constexpr int EQBandType_Max = 4;              // HighCut
constexpr int ModulationSourceType_Max = 13;   // MacroKnob
constexpr int AutomationInterpolation_Max = 3; // Exponential
constexpr int MappingCurveType_Max = 5;        // Custom
constexpr int EnvelopeCurve_Max = 4;           // Step
constexpr int LoopMode_Max = 2;                // OneShot

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json profile_not_found(std::uint64_t id) {
    return error_response("Profile not found: " + std::to_string(id));
}

/// Build a SoundSourceData from JSON parameters without inventing defaults
/// for unknown discriminators or enum values.
std::optional<SoundSourceData> build_source(const json& params) {
    SoundSourceData src;
    auto type = params.at("source_type").get<std::string>();

    if (type == "subtractive") {
        SubtractiveSynth sub;
        const auto osc_count = detail::checked_integer_or<std::size_t>(
            params, "oscillator_count", 1, "oscillator count");
        if (osc_count < 1 || osc_count > 1024) return std::nullopt;
        for (std::size_t i = 0; i < osc_count; ++i) {
            Oscillator osc;
            auto wf = checked_enum_or<WaveformType>(params, "waveform", 0, WaveformType_Max);
            if (!wf) return std::nullopt;
            osc.waveform.type = *wf;
            sub.oscillators.push_back(osc);
        }
        sub.amplifier.stages = {
            {static_cast<float>(params.value("attack", 5.0)), 1.0f, EnvelopeCurve::Linear},
            {static_cast<float>(params.value("decay", 100.0)),
             static_cast<float>(params.value("sustain", 0.7)),
             EnvelopeCurve::Exponential},
            {static_cast<float>(params.value("release", 200.0)), 0.0f, EnvelopeCurve::Exponential}};
        sub.filter.cutoff = static_cast<float>(params.value("filter_cutoff", 1000.0));
        sub.filter.resonance = static_cast<float>(params.value("filter_resonance", 0.0));
        src.data = std::move(sub);
    } else if (type == "fm") {
        FMSynth fm;
        const auto op_count = detail::checked_integer_or<std::size_t>(
            params, "operator_count", 4, "FM operator count");
        if (op_count < 1 || op_count > 1024) return std::nullopt;
        for (std::size_t i = 0; i < op_count; ++i) {
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
        wt.amplifier.stages = {{5.0f, 1.0f, EnvelopeCurve::Linear},
                               {100.0f, 0.7f, EnvelopeCurve::Exponential},
                               {200.0f, 0.0f, EnvelopeCurve::Exponential}};
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
        const auto count = detail::checked_integer_or<std::uint16_t>(
            params, "partial_count", 8, "additive partial count");
        if (count < 1 || count > 1024) return std::nullopt;
        add.partial_count = count;
        for (std::uint16_t i = 0; i < count; ++i) {
            PartialDefinition p;
            p.ratio = static_cast<float>(i + 1);
            p.amplitude = 1.0f / static_cast<float>(i + 1);
            add.partials.push_back(p);
        }
        add.global_envelope.stages = {{20.0f, 1.0f, EnvelopeCurve::Linear},
                                      {100.0f, 0.7f, EnvelopeCurve::Exponential},
                                      {200.0f, 0.0f, EnvelopeCurve::Exponential}};
        src.data = std::move(add);
    } else if (type == "physical_model") {
        PhysicalModelSource pm;
        auto cat = checked_enum_or<PhysicalModelCategory>(
            params, "model_category", 0, PhysicalModelCategory_Max);
        if (!cat) return std::nullopt;
        pm.model.category = *cat;
        auto exc = checked_enum_or<ExciterType>(params, "exciter_type", 4, ExciterType_Max);
        if (!exc) return std::nullopt;
        pm.exciter.type = *exc;
        pm.brightness = static_cast<float>(params.value("brightness", 0.5));
        src.data = pm;
    } else if (type == "sampler") {
        SamplerSource sam;
        sam.library = params.value("library", "");
        sam.preset = params.value("preset", "");
        src.data = std::move(sam);
    } else {
        return std::nullopt;
    }

    return src;
}

/// Build an Effect from JSON parameters
std::optional<Effect> build_effect(const json& params, std::uint64_t effect_id) {
    Effect effect;
    effect.id = EffectId{effect_id};
    effect.enabled = params.value("enabled", true);
    effect.mix = static_cast<float>(params.value("mix", 1.0));

    auto type = params.at("effect_type").get<std::string>();

    if (type == "distortion") {
        DistortionEffect e;
        e.drive = static_cast<float>(params.value("drive", 0.5));
        e.tone = static_cast<float>(params.value("tone", 0.5));
        effect.parameters = e;
    } else if (type == "delay") {
        DelayEffect e;
        e.delay_time.ms = static_cast<float>(params.value("delay_ms", 500.0));
        e.feedback = static_cast<float>(params.value("feedback", 0.3));
        effect.parameters = e;
    } else if (type == "reverb") {
        ReverbEffect e;
        e.decay_time = static_cast<float>(params.value("decay", 1.5));
        e.pre_delay = static_cast<float>(params.value("pre_delay", 20.0));
        e.damping = static_cast<float>(params.value("damping", 0.5));
        e.size = static_cast<float>(params.value("size", 0.5));
        effect.parameters = e;
    } else if (type == "chorus") {
        ChorusEffect e;
        e.rate = static_cast<float>(params.value("rate", 1.0));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        effect.parameters = e;
    } else if (type == "phaser") {
        PhaserEffect e;
        e.rate = static_cast<float>(params.value("rate", 0.5));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        const auto stages =
            detail::checked_integer_or<std::uint8_t>(params, "stages", 4, "phaser stages");
        if (stages < 2 || stages > 24) return std::nullopt;
        e.stages = stages;
        effect.parameters = e;
    } else if (type == "flanger") {
        FlangerEffect e;
        e.rate = static_cast<float>(params.value("rate", 0.3));
        e.depth = static_cast<float>(params.value("depth", 0.5));
        e.feedback = static_cast<float>(params.value("feedback", 0.5));
        effect.parameters = e;
    } else if (type == "eq") {
        EQEffect e;
        if (params.contains("bands")) {
            for (const auto& b : params["bands"]) {
                EQBand band;
                band.frequency = static_cast<float>(b.value("frequency", 1000.0));
                band.gain = static_cast<float>(b.value("gain", 0.0));
                band.q = static_cast<float>(b.value("q", 1.0));
                auto bt = checked_enum_or<EQBandType>(b, "type", 0, EQBandType_Max);
                if (!bt) return std::nullopt;
                band.band_type = *bt;
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
        effect.parameters = e;
    } else {
        return std::nullopt;
    }

    return effect;
}

/// Serialise semantic descriptors to JSON
json semantic_to_json(const SemanticTimbreDescriptor& d) {
    return {{"brightness", d.brightness},
            {"warmth", d.warmth},
            {"roughness", d.roughness},
            {"attack_character", static_cast<int>(d.attack_character)},
            {"sustain_character", static_cast<int>(d.sustain_character)},
            {"width", d.width},
            {"density", d.density},
            {"movement", d.movement},
            {"weight", d.weight},
            {"tags", d.tags},
            {"derivation", static_cast<int>(d.derivation)}};
}

} // anonymous namespace

void register_timbre_tools(McpServer& server, std::shared_ptr<TimbreSession> session) {
    if (!session) session = std::make_shared<TimbreSession>();

    // =========================================================================
    // create_timbre_profile
    // =========================================================================
    server.register_tool(
        "create_timbre_profile",
        "Create a new TimbreProfile for a Score IR Part",
        {{"type", "object"},
         {"properties",
          {{"part_id", {{"type", "integer"}, {"description", "Score IR Part ID to bind to"}}},
           {"name", {{"type", "string"}, {"description", "Profile name"}}}}},
         {"required", json::array({"part_id", "name"})}},
        [session](const json& params) -> json {
            const auto part_id =
                detail::checked_integer<std::uint64_t>(params.at("part_id"), "part id");
            const auto name = params.at("name").get<std::string>();
            auto id = session->next_profile_id;
            auto p = create_timbre_profile(TimbreProfileId{id}, PartId{part_id}, name);
            session->profiles.emplace(id, std::move(p));
            ++session->next_profile_id;
            return {{"profile_id", id}, {"success", true}};
        });

    // =========================================================================
    // set_sound_source
    // =========================================================================
    server.register_tool(
        "set_sound_source",
        "Set or change the sound source type and parameters",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"source_type",
            {{"type", "string"},
             {"description", "subtractive|fm|wavetable|granular|additive|physical_model|sampler"}}},
           {"oscillator_count",
            {{"type", "integer"}, {"description", "Number of oscillators (subtractive)"}}},
           {"waveform", {{"type", "integer"}, {"description", "WaveformType enum value"}}},
           {"filter_cutoff", {{"type", "number"}, {"description", "Filter cutoff Hz"}}},
           {"filter_resonance", {{"type", "number"}, {"description", "Filter resonance 0-1"}}},
           {"attack", {{"type", "number"}, {"description", "Envelope attack ms"}}},
           {"decay", {{"type", "number"}, {"description", "Envelope decay ms"}}},
           {"sustain", {{"type", "number"}, {"description", "Envelope sustain level 0-1"}}},
           {"release", {{"type", "number"}, {"description", "Envelope release ms"}}},
           {"operator_count", {{"type", "integer"}, {"description", "Number of FM operators"}}},
           {"feedback", {{"type", "number"}, {"description", "FM feedback amount 0-1"}}},
           {"wavetable", {{"type", "string"}, {"description", "Wavetable reference name"}}},
           {"position", {{"type", "number"}, {"description", "Wavetable/granular position 0-1"}}},
           {"grain_size", {{"type", "number"}, {"description", "Grain size ms"}}},
           {"grain_density", {{"type", "number"}, {"description", "Grains per second"}}},
           {"partial_count", {{"type", "integer"}, {"description", "Number of additive partials"}}},
           {"model_category", {{"type", "integer"}, {"description", "PhysicalModelCategory enum"}}},
           {"exciter_type", {{"type", "integer"}, {"description", "ExciterType enum"}}},
           {"brightness", {{"type", "number"}, {"description", "Physical model brightness"}}},
           {"library", {{"type", "string"}, {"description", "Sampler library name"}}},
           {"preset", {{"type", "string"}, {"description", "Sampler preset name"}}},
           {"audio_source",
            {{"type", "string"}, {"description", "Audio source path (granular)"}}}}},
         {"required", json::array({"profile_id", "source_type"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto src = build_source(params);
            if (!src) return error_response("Unknown or invalid sound source configuration");
            auto r = set_sound_source(*p, std::move(*src));
            if (!r) return error_response("Failed to set sound source");
            return {{"success", true}};
        });

    // =========================================================================
    // add_effect
    // =========================================================================
    server.register_tool(
        "add_effect",
        "Add an effect to the insert chain",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"effect_type",
            {{"type", "string"},
             {"description", "distortion|delay|reverb|chorus|phaser|flanger|eq|compressor"}}},
           {"enabled", {{"type", "boolean"}, {"description", "Effect enabled state"}}},
           {"mix", {{"type", "number"}, {"description", "Dry/wet mix 0-1"}}},
           {"drive", {{"type", "number"}, {"description", "Distortion drive"}}},
           {"tone", {{"type", "number"}, {"description", "Distortion tone"}}},
           {"delay_ms", {{"type", "number"}, {"description", "Delay time ms"}}},
           {"feedback", {{"type", "number"}, {"description", "Delay/flanger feedback"}}},
           {"decay", {{"type", "number"}, {"description", "Reverb decay time"}}},
           {"pre_delay", {{"type", "number"}, {"description", "Reverb pre-delay ms"}}},
           {"damping", {{"type", "number"}, {"description", "Reverb damping"}}},
           {"size", {{"type", "number"}, {"description", "Reverb room size"}}},
           {"rate", {{"type", "number"}, {"description", "Modulation rate Hz"}}},
           {"depth", {{"type", "number"}, {"description", "Modulation depth"}}},
           {"stages", {{"type", "integer"}, {"description", "Phaser stages"}}},
           {"threshold", {{"type", "number"}, {"description", "Compressor threshold dB"}}},
           {"ratio", {{"type", "number"}, {"description", "Compressor ratio"}}},
           {"attack", {{"type", "number"}, {"description", "Compressor attack ms"}}},
           {"release", {{"type", "number"}, {"description", "Compressor release ms"}}},
           {"bands", {{"type", "array"}, {"description", "EQ bands array"}}}}},
         {"required", json::array({"profile_id", "effect_type"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto eid = session->next_effect_id;
            auto effect = build_effect(params, eid);
            if (!effect) return error_response("Unknown or invalid timbre effect configuration");
            auto r = add_effect(*p, std::move(*effect));
            if (!r) return error_response("Failed to add effect");
            ++session->next_effect_id;
            return {{"success", true}, {"effect_id", eid}};
        });

    // =========================================================================
    // remove_effect
    // =========================================================================
    server.register_tool(
        "remove_effect",
        "Remove an effect from the insert chain",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"effect_id", {{"type", "integer"}, {"description", "Effect ID to remove"}}}}},
         {"required", json::array({"profile_id", "effect_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto r = remove_effect(*p,
                                   EffectId{detail::checked_integer<std::uint64_t>(
                                       params.at("effect_id"), "effect id")});
            if (!r) {
                if (r.error() == ErrorCode::TimbreNotFound)
                    return error_response("Effect not found");
                return error_response(
                    "Effect is referenced by a modulation, macro, automation or rendering "
                    "mapping; remove those references first");
            }
            return {{"success", true}};
        });

    // =========================================================================
    // reorder_effects
    // =========================================================================
    server.register_tool(
        "reorder_effects",
        "Change effect order in the insert chain",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"effect_ids",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "New order of effect IDs"}}}}},
         {"required", json::array({"profile_id", "effect_ids"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            std::vector<EffectId> ids;
            for (const auto& id : params.at("effect_ids"))
                ids.push_back(EffectId{detail::checked_integer<std::uint64_t>(id, "effect id")});
            auto r = reorder_effects(*p, ids);
            if (!r) return error_response("Reorder failed: id mismatch");
            return {{"success", true}};
        });

    // =========================================================================
    // set_parameter
    // =========================================================================
    server.register_tool(
        "set_parameter",
        "Set any numeric parameter by dot-separated path",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"path",
            {{"type", "string"}, {"description", "Parameter path (e.g. source.filter.cutoff)"}}},
           {"value", {{"type", "number"}, {"description", "New value"}}}}},
         {"required", json::array({"profile_id", "path", "value"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto r = set_parameter(*p,
                                   params.at("path").get<std::string>(),
                                   static_cast<float>(params.at("value").get<double>()));
            if (!r)
                return error_response("Invalid parameter path: " +
                                      params.at("path").get<std::string>());
            return {{"success", true}};
        });

    // =========================================================================
    // get_parameter
    // =========================================================================
    server.register_tool(
        "get_parameter",
        "Read a numeric parameter by dot-separated path",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"path", {{"type", "string"}, {"description", "Parameter path"}}}}},
         {"required", json::array({"profile_id", "path"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto r = get_parameter(*p, params.at("path").get<std::string>());
            if (!r) return error_response("Invalid parameter path");
            return {{"value", *r}};
        });

    // =========================================================================
    // create_modulation_lfo
    // =========================================================================
    server.register_tool(
        "create_modulation_lfo",
        "Create an owned LFO modulation source",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}}},
           {"waveform", {{"type", "integer"}}},
           {"synced", {{"type", "boolean"}}},
           {"rate_hz", {{"type", "number"}}},
           {"division_num", {{"type", "integer"}}},
           {"division_den", {{"type", "integer"}}},
           {"phase", {{"type", "number"}}},
           {"symmetry", {{"type", "number"}}},
           {"retrigger", {{"type", "boolean"}}},
           {"fade_in", {{"type", "number"}}},
           {"delay", {{"type", "number"}}},
           {"custom_points", {{"type", "array"}}}}},
         {"required", json::array({"profile_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* profile = session->find(profile_id);
            if (!profile) return profile_not_found(profile_id);

            LFO lfo;
            auto waveform =
                checked_enum_or<LFOWaveformType>(params, "waveform", 0, WaveformType_Max);
            if (!waveform) return error_response("waveform out of range");
            lfo.waveform = *waveform;
            lfo.rate.synced = params.value("synced", false);
            lfo.rate.hz = static_cast<float>(params.value("rate_hz", 1.0));
            const auto division =
                Beat::from_ratio(detail::checked_integer_or<std::int64_t>(
                                     params, "division_num", 1, "LFO division numerator"),
                                 detail::checked_integer_or<std::int64_t>(
                                     params, "division_den", 4, "LFO division denominator"));
            if (!division) return error_response("invalid LFO division");
            lfo.rate.division = *division;
            lfo.phase = static_cast<float>(params.value("phase", 0.0));
            lfo.symmetry = static_cast<float>(params.value("symmetry", 0.5));
            lfo.retrigger = params.value("retrigger", true);
            lfo.fade_in = static_cast<float>(params.value("fade_in", 0.0));
            lfo.delay = static_cast<float>(params.value("delay", 0.0));
            if (params.contains("custom_points")) {
                for (const auto& point : params.at("custom_points")) {
                    lfo.custom_points.emplace_back(static_cast<float>(point.at("x").get<double>()),
                                                   static_cast<float>(point.at("y").get<double>()));
                }
            }
            auto index = create_lfo(*profile, std::move(lfo));
            if (!index) return error_response("invalid LFO definition");
            return {{"success", true}, {"source_index", *index}};
        });

    // =========================================================================
    // create_modulation_envelope
    // =========================================================================
    server.register_tool(
        "create_modulation_envelope",
        "Create an owned envelope modulation source",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}}},
           {"stages", {{"type", "array"}}},
           {"velocity_sensitivity", {{"type", "number"}}},
           {"key_tracking", {{"type", "number"}}},
           {"loop", {{"type", "object"}}}}},
         {"required", json::array({"profile_id", "stages"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* profile = session->find(profile_id);
            if (!profile) return profile_not_found(profile_id);

            Envelope envelope;
            for (const auto& encoded : params.at("stages")) {
                auto curve = checked_enum_or<EnvelopeCurve>(encoded, "curve", 0, EnvelopeCurve_Max);
                if (!curve) return error_response("envelope curve out of range");
                envelope.stages.push_back(
                    {static_cast<float>(encoded.at("duration").get<double>()),
                     static_cast<float>(encoded.at("target_level").get<double>()),
                     *curve,
                     static_cast<float>(encoded.value("curvature", 1.0))});
            }
            envelope.velocity_sensitivity =
                static_cast<float>(params.value("velocity_sensitivity", 0.0));
            envelope.key_tracking = static_cast<float>(params.value("key_tracking", 0.0));
            if (params.contains("loop")) {
                const auto& loop = params.at("loop");
                envelope.loop = EnvelopeLoop{detail::checked_integer<std::uint8_t>(
                                                 loop.at("start_stage"), "envelope loop start"),
                                             detail::checked_integer<std::uint8_t>(
                                                 loop.at("end_stage"), "envelope loop end"),
                                             detail::checked_integer_or<std::uint8_t>(
                                                 loop, "count", 0, "envelope loop count")};
            }
            auto index = create_modulation_envelope(*profile, std::move(envelope));
            if (!index) return error_response("invalid modulation envelope definition");
            return {{"success", true}, {"source_index", *index}};
        });

    // =========================================================================
    // create_step_sequencer
    // =========================================================================
    server.register_tool(
        "create_step_sequencer",
        "Create an owned step-sequencer modulation source",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}}},
           {"steps", {{"type", "array"}}},
           {"duration_num", {{"type", "integer"}}},
           {"duration_den", {{"type", "integer"}}},
           {"smooth", {{"type", "number"}}},
           {"loop_mode", {{"type", "integer"}}}}},
         {"required", json::array({"profile_id", "steps"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* profile = session->find(profile_id);
            if (!profile) return profile_not_found(profile_id);

            StepSequencer sequencer;
            for (const auto& step : params.at("steps"))
                sequencer.steps.push_back(static_cast<float>(step.get<double>()));
            const auto duration =
                Beat::from_ratio(detail::checked_integer_or<std::int64_t>(
                                     params, "duration_num", 1, "step duration numerator"),
                                 detail::checked_integer_or<std::int64_t>(
                                     params, "duration_den", 16, "step duration denominator"));
            if (!duration) return error_response("invalid step duration");
            sequencer.step_duration = *duration;
            sequencer.smooth = static_cast<float>(params.value("smooth", 0.0));
            auto loop_mode = checked_enum_or<LoopMode>(params, "loop_mode", 0, LoopMode_Max);
            if (!loop_mode) return error_response("loop_mode out of range");
            sequencer.loop_mode = *loop_mode;
            auto index = create_step_sequencer(*profile, std::move(sequencer));
            if (!index) return error_response("invalid step sequencer definition");
            return {{"success", true}, {"source_index", *index}};
        });

    // =========================================================================
    // create_macro
    // =========================================================================
    server.register_tool(
        "create_macro",
        "Create a macro knob with mappings",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"index", {{"type", "integer"}, {"description", "Macro index (0-7)"}}},
           {"name", {{"type", "string"}, {"description", "Macro name"}}},
           {"value", {{"type", "number"}, {"description", "Initial value 0-1"}}},
           {"mappings",
            {{"type", "array"}, {"description", "Array of {target, min, max} objects"}}}}},
         {"required", json::array({"profile_id", "index", "name"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            MacroKnob macro;
            macro.index = detail::checked_integer<std::uint8_t>(params.at("index"), "macro index");
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
            auto r = create_macro(*p, std::move(macro));
            if (!r) return error_response("Failed to create macro");
            return {{"success", true}};
        });

    // =========================================================================
    // set_macro
    // =========================================================================
    server.register_tool(
        "set_macro",
        "Set a macro knob value",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"macro_index", {{"type", "integer"}, {"description", "Macro index"}}},
           {"value", {{"type", "number"}, {"description", "New value 0-1"}}}}},
         {"required", json::array({"profile_id", "macro_index", "value"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto r = set_macro(
                *p,
                detail::checked_integer<std::uint8_t>(params.at("macro_index"), "macro index"),
                static_cast<float>(params.at("value").get<double>()));
            if (!r) return error_response("Macro index not found");
            return {{"success", true}};
        });

    // =========================================================================
    // add_modulation
    // =========================================================================
    server.register_tool(
        "add_modulation",
        "Add a modulation routing",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"source_type", {{"type", "integer"}, {"description", "ModulationSourceType enum"}}},
           {"source_index",
            {{"type", "integer"}, {"description", "Source index (LFO/envelope/step seq)"}}},
           {"source_cc", {{"type", "integer"}, {"description", "CC source number"}}},
           {"source_sidechain_part",
            {{"type", "integer"}, {"description", "AudioFollower PartId"}}},
           {"via_type", {{"type", "integer"}, {"description", "Optional via source type"}}},
           {"via_index", {{"type", "integer"}}},
           {"via_cc", {{"type", "integer"}}},
           {"via_sidechain_part", {{"type", "integer"}}},
           {"target", {{"type", "string"}, {"description", "Target parameter path"}}},
           {"depth", {{"type", "number"}, {"description", "Modulation depth -1 to 1"}}}}},
         {"required", json::array({"profile_id", "source_type", "target", "depth"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            ModulationRouting routing;
            auto src_type = checked_enum<ModulationSourceType>(params.at("source_type"),
                                                               ModulationSourceType_Max);
            if (!src_type)
                return error_response("source_type out of range (0-" +
                                      std::to_string(ModulationSourceType_Max) + ")");
            routing.source.type = *src_type;
            routing.source.index = detail::checked_integer_or<std::uint8_t>(
                params, "source_index", 0, "modulation source index");
            routing.source.cc_number = detail::checked_integer_or<std::uint8_t>(
                params, "source_cc", 0, "modulation source CC");
            routing.source.sidechain_part.value = detail::checked_integer_or<std::uint64_t>(
                params, "source_sidechain_part", 0, "modulation source sidechain PartId");
            if (params.contains("via_type")) {
                auto via_type = checked_enum<ModulationSourceType>(params.at("via_type"),
                                                                   ModulationSourceType_Max);
                if (!via_type) return error_response("via_type out of range");
                ModulationSource via;
                via.type = *via_type;
                via.index = detail::checked_integer_or<std::uint8_t>(
                    params, "via_index", 0, "via source index");
                via.cc_number =
                    detail::checked_integer_or<std::uint8_t>(params, "via_cc", 0, "via source CC");
                via.sidechain_part.value = detail::checked_integer_or<std::uint64_t>(
                    params, "via_sidechain_part", 0, "via source sidechain PartId");
                routing.via = via;
            } else if (params.contains("via_index") || params.contains("via_cc") ||
                       params.contains("via_sidechain_part")) {
                return error_response("via_type is required when via payload is present");
            }
            routing.target = params.at("target").get<std::string>();
            routing.depth = static_cast<float>(params.at("depth").get<double>());
            auto r = add_modulation(*p, std::move(routing));
            if (!r) return error_response("Failed to add modulation");
            return {{"success", true}};
        });

    // =========================================================================
    // add_automation
    // =========================================================================
    server.register_tool(
        "add_automation",
        "Add timbral automation",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"path", {{"type", "string"}, {"description", "Parameter path"}}},
           {"breakpoints",
            {{"type", "array"}, {"description", "Array of {bar, beat_num, beat_den, value}"}}},
           {"interpolation",
            {{"type", "integer"}, {"description", "AutomationInterpolation enum"}}}}},
         {"required", json::array({"profile_id", "path", "breakpoints"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            TimbreAutomation automation;
            automation.parameter_path = params.at("path").get<std::string>();
            auto interp = checked_enum_or<AutomationInterpolation>(
                params, "interpolation", 1, AutomationInterpolation_Max);
            if (!interp)
                return error_response("interpolation out of range (0-" +
                                      std::to_string(AutomationInterpolation_Max) + ")");
            automation.interpolation = *interp;
            for (const auto& bp : params.at("breakpoints")) {
                AutomationBreakpoint b;
                b.time.bar = detail::checked_integer<std::uint32_t>(bp.at("bar"), "automation bar");
                auto beat = Beat::from_ratio(detail::checked_integer_or<std::int64_t>(
                                                 bp, "beat_num", 0, "automation beat numerator"),
                                             detail::checked_integer_or<std::int64_t>(
                                                 bp, "beat_den", 1, "automation beat denominator"));
                if (!beat) return error_response("invalid automation beat");
                b.time.beat = *beat;
                b.value = static_cast<float>(bp.at("value").get<double>());
                automation.breakpoints.push_back(b);
            }
            auto r = add_automation(*p, std::move(automation));
            if (!r) return error_response("Failed to add invalid automation lane");
            return {{"success", true}};
        });

    // =========================================================================
    // set_semantic_descriptors
    // =========================================================================
    server.register_tool(
        "set_semantic_descriptors",
        "Set or update semantic descriptors",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"brightness", {{"type", "number"}, {"description", "Brightness 0-1"}}},
           {"warmth", {{"type", "number"}, {"description", "Warmth 0-1"}}},
           {"roughness", {{"type", "number"}, {"description", "Roughness 0-1"}}},
           {"width", {{"type", "number"}, {"description", "Width 0-1"}}},
           {"density", {{"type", "number"}, {"description", "Density 0-1"}}},
           {"movement", {{"type", "number"}, {"description", "Movement 0-1"}}},
           {"weight", {{"type", "number"}, {"description", "Weight 0-1"}}},
           {"tags",
            {{"type", "array"},
             {"items", {{"type", "string"}}},
             {"description", "Descriptor tags"}}}}},
         {"required", json::array({"profile_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            SemanticTimbreDescriptor d = p->semantic_descriptors;
            if (params.contains("brightness"))
                d.brightness = static_cast<float>(params["brightness"].get<double>());
            if (params.contains("warmth"))
                d.warmth = static_cast<float>(params["warmth"].get<double>());
            if (params.contains("roughness"))
                d.roughness = static_cast<float>(params["roughness"].get<double>());
            if (params.contains("width"))
                d.width = static_cast<float>(params["width"].get<double>());
            if (params.contains("density"))
                d.density = static_cast<float>(params["density"].get<double>());
            if (params.contains("movement"))
                d.movement = static_cast<float>(params["movement"].get<double>());
            if (params.contains("weight"))
                d.weight = static_cast<float>(params["weight"].get<double>());
            if (params.contains("tags")) d.tags = params["tags"].get<std::vector<std::string>>();
            if (!set_semantic_descriptors(*p, d))
                return error_response("Semantic descriptor values must be finite and in [0, 1]");
            return {{"success", true}};
        });

    // =========================================================================
    // analyze_timbre
    // =========================================================================
    server.register_tool(
        "analyze_timbre",
        "Derive semantic descriptors from current parameters",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}}}},
         {"required", json::array({"profile_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto d = analyze_timbre(*p);
            return {{"descriptors", semantic_to_json(d)}};
        });

    // =========================================================================
    // search_presets
    // =========================================================================
    server.register_tool(
        "search_presets",
        "Search the preset library by descriptors, tags, or text",
        {{"type", "object"},
         {"properties",
          {{"name_contains",
            {{"type", "string"}, {"description", "Substring match on preset name"}}},
           {"required_tags",
            {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Required tags"}}},
           {"min_brightness", {{"type", "number"}, {"description", "Minimum brightness"}}},
           {"max_brightness", {{"type", "number"}, {"description", "Maximum brightness"}}},
           {"min_warmth", {{"type", "number"}, {"description", "Minimum warmth"}}},
           {"max_warmth", {{"type", "number"}, {"description", "Maximum warmth"}}},
           {"max_results",
            {{"type", "integer"}, {"description", "Maximum results (default 10)"}}}}},
         {"required", json::array()}},
        [session](const json& params) -> json {
            PresetSearchQuery q;
            if (params.contains("name_contains"))
                q.name_contains = params["name_contains"].get<std::string>();
            if (params.contains("required_tags"))
                q.required_tags = params["required_tags"].get<std::vector<std::string>>();
            if (params.contains("min_brightness"))
                q.min_brightness = static_cast<float>(params["min_brightness"].get<double>());
            if (params.contains("max_brightness"))
                q.max_brightness = static_cast<float>(params["max_brightness"].get<double>());
            if (params.contains("min_warmth"))
                q.min_warmth = static_cast<float>(params["min_warmth"].get<double>());
            if (params.contains("max_warmth"))
                q.max_warmth = static_cast<float>(params["max_warmth"].get<double>());
            q.max_results = detail::checked_integer_or<std::size_t>(
                params, "max_results", 10, "maximum result count");

            auto results = search_presets(session->preset_library, q);
            json arr = json::array();
            for (const auto& p : results) {
                arr.push_back({{"id", p.id.value},
                               {"name", p.name},
                               {"tags", p.tags},
                               {"brightness", p.semantic_descriptors.brightness},
                               {"warmth", p.semantic_descriptors.warmth}});
            }
            return {{"presets", arr}, {"count", results.size()}};
        });

    // =========================================================================
    // save_preset
    // =========================================================================
    server.register_tool(
        "save_preset",
        "Save current state as a preset",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"name", {{"type", "string"}, {"description", "Preset name"}}},
           {"tags",
            {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Preset tags"}}}}},
         {"required", json::array({"profile_id", "name"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            const auto name = params.at("name").get<std::string>();
            std::vector<std::string> tags;
            if (params.contains("tags")) tags = params["tags"].get<std::vector<std::string>>();
            auto pid = session->next_preset_id;
            auto preset = save_preset(*p, TimbrePresetId{pid}, name);
            preset.tags = std::move(tags);
            session->preset_library.push_back(preset);
            ++session->next_preset_id;
            return {{"success", true}, {"preset_id", pid}};
        });

    // =========================================================================
    // load_preset
    // =========================================================================
    server.register_tool(
        "load_preset",
        "Apply a preset to a TimbreProfile",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"preset_id", {{"type", "integer"}, {"description", "Preset ID to load"}}}}},
         {"required", json::array({"profile_id", "preset_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            auto preset_id =
                detail::checked_integer<std::uint64_t>(params.at("preset_id"), "preset id");
            const TimbrePreset* preset = nullptr;
            for (const auto& pr : session->preset_library) {
                if (pr.id.value == preset_id) {
                    preset = &pr;
                    break;
                }
            }
            if (!preset) return error_response("Preset not found: " + std::to_string(preset_id));
            auto r = load_preset(*p, *preset);
            if (!r)
                return error_response(
                    "Failed to load preset: a parameter path does not resolve or validate in "
                    "this profile; the profile is unchanged");
            return {{"success", true}};
        });

    // =========================================================================
    // morph_presets
    // =========================================================================
    server.register_tool(
        "morph_presets",
        "Set up a preset morph over a score time range",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"from_preset_id", {{"type", "integer"}, {"description", "Starting preset ID"}}},
           {"to_preset_id", {{"type", "integer"}, {"description", "Ending preset ID"}}},
           {"start_bar", {{"type", "integer"}, {"description", "Start bar number"}}},
           {"end_bar", {{"type", "integer"}, {"description", "End bar number"}}},
           {"curve_type",
            {{"type", "integer"}, {"description", "MappingCurveType enum (default: linear)"}}}}},
         {"required",
          json::array({"profile_id", "from_preset_id", "to_preset_id", "start_bar", "end_bar"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            PresetMorph morph;
            morph.from_preset = TimbrePresetId{detail::checked_integer<std::uint64_t>(
                params.at("from_preset_id"), "source preset id")};
            morph.to_preset = TimbrePresetId{detail::checked_integer<std::uint64_t>(
                params.at("to_preset_id"), "destination preset id")};
            morph.start = ScoreTime{
                detail::checked_integer<std::uint32_t>(params.at("start_bar"), "morph start bar"),
                Beat{0, 1}};
            morph.end = ScoreTime{
                detail::checked_integer<std::uint32_t>(params.at("end_bar"), "morph end bar"),
                Beat{0, 1}};
            auto ct =
                checked_enum_or<MappingCurveType>(params, "curve_type", 0, MappingCurveType_Max);
            if (!ct) return error_response("curve_type out of range");
            morph.curve.type = *ct;
            auto r = morph_presets(*p, std::move(morph), session->preset_library);
            if (!r) {
                if (r.error() == ErrorCode::TimbreNotFound)
                    return error_response("Morph preset not found in the preset library");
                return error_response("Morph must start at or after bar 1 and end after it starts");
            }
            return {{"success", true}};
        });

    // =========================================================================
    // map_timbre_parameter
    // =========================================================================
    server.register_tool(
        "map_timbre_parameter",
        "Map a numeric Timbre IR path into an exact Ableton DeviceParameter domain",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}}},
           {"ir_path", {{"type", "string"}}},
           {"device_index", {{"type", "integer"}, {"minimum", 0}, {"maximum", 2147483647}}},
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
           {"value_property", {{"type", "string"}, {"enum", {"value", "display_value"}}}},
           {"device_name", {{"type", "string"}}}}},
         {"required",
          json::array({"profile_id",
                       "ir_path",
                       "device_index",
                       "parameter_name",
                       "source_min",
                       "source_max",
                       "target_min",
                       "target_max"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* profile = session->find(profile_id);
            if (!profile) return profile_not_found(profile_id);

            DeviceParameter mapping;
            mapping.device_index =
                detail::checked_integer<std::uint32_t>(params.at("device_index"), "device index");
            mapping.parameter_name = params.at("parameter_name").get<std::string>();
            mapping.source_min = params.at("source_min").get<float>();
            mapping.source_max = params.at("source_max").get<float>();
            mapping.range_min = params.at("target_min").get<float>();
            mapping.range_max = params.at("target_max").get<float>();
            auto curve =
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

            const auto path = params.at("ir_path").get<std::string>();
            if (path.empty() || mapping.parameter_name.empty())
                return error_response("ir_path and parameter_name must be non-empty");
            auto source_value = get_parameter(*profile, path);
            if (!source_value) return error_response("ir_path does not resolve to a numeric value");
            auto target_value = map_device_parameter_value(*source_value, mapping);
            if (!target_value)
                return error_response("mapping domains, curve, or current IR value are not valid");

            std::size_t requested_device_count = 1;
            if (!profile->insert_chain.bypass_all) {
                requested_device_count += static_cast<std::size_t>(
                    std::count_if(profile->insert_chain.effects.begin(),
                                  profile->insert_chain.effects.end(),
                                  [](const Effect& effect) { return effect.enabled; }));
            }
            if (mapping.device_index >= requested_device_count)
                return error_response("device_index is outside the requested device chain");
            for (const auto& [existing_path, existing] : profile->rendering.parameter_map) {
                if (existing_path != path && existing.device_index == mapping.device_index &&
                    existing.parameter_name == mapping.parameter_name)
                    return error_response(
                        "another IR path already maps to the same target parameter");
            }

            if (params.contains("device_name")) {
                const auto device_name = params.at("device_name").get<std::string>();
                if (device_name.empty()) return error_response("device_name must be non-empty");
                profile->rendering.device_type.tag = DeviceTypeTag::NativeAbleton;
                profile->rendering.device_type.device_name = device_name;
            }
            profile->rendering.parameter_map[path] = std::move(mapping);
            return {{"success", true},
                    {"source_value", *source_value},
                    {"target_value", *target_value}};
        });

    server.register_tool("get_timbre_json",
                         "Inspect the complete TimbreProfile, effect IDs, modulation, automation, "
                         "presets and mappings",
                         {{"profile_id", "integer"}},
                         [session](const json& params) -> json {
                             const auto id = detail::checked_integer<std::uint64_t>(
                                 params.at("profile_id"), "profile id");
                             const auto* profile = session->find(id);
                             if (!profile) return profile_not_found(id);
                             return timbre_to_json(*profile);
                         });

    server.register_tool(
        "replace_timbre_effect",
        "Replace an effect at its stable ID using a complete add_effect configuration; omitted "
        "fields use add_effect defaults; references are retained and validated",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}}},
           {"effect_id", {{"type", "integer"}}},
           {"configuration",
            {{"type", "object"},
             {"properties", {{"effect_type", {{"type", "string"}}}}},
             {"required", {"effect_type"}}}}}},
         {"required", {"profile_id", "effect_id", "configuration"}}},
        [session](const json& params) -> json {
            const auto id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* profile = session->find(id);
            if (!profile) return profile_not_found(id);
            const auto effect_id =
                detail::checked_integer<std::uint64_t>(params.at("effect_id"), "effect id");
            auto effect = build_effect(params.at("configuration"), effect_id);
            if (!effect || !replace_effect(*profile, std::move(*effect)))
                return error_response(
                    "Effect replacement has invalid parameters or retained references; inspect the "
                    "profile and remove incompatible references first");
            return {{"success", true}};
        });

    for (const auto& kind : {std::string{"automation"},
                             std::string{"modulation"},
                             std::string{"macro"},
                             std::string{"parameter_mapping"}}) {
        json properties = {{"profile_id", {{"type", "integer"}}}};
        const auto field = kind == "parameter_mapping" ? "path" : "index";
        properties[field] = {{"type", kind == "parameter_mapping" ? "string" : "integer"}};
        server.register_tool(
            "remove_timbre_" + kind,
            "Remove one " + kind +
                "; index is zero-based for lanes/routings and the stable source index for a macro; "
                "referenced macros are refused",
            {{"type", "object"}, {"properties", properties}, {"required", {"profile_id", field}}},
            [session, kind](const json& params) -> json {
                const auto id =
                    detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
                auto* profile = session->find(id);
                if (!profile) return profile_not_found(id);
                Result<void> outcome;
                if (kind == "parameter_mapping")
                    outcome =
                        remove_parameter_mapping(*profile, params.at("path").get<std::string>());
                else if (kind == "macro")
                    outcome = remove_macro(
                        *profile,
                        detail::checked_integer<std::uint8_t>(params.at("index"), "macro index"));
                else {
                    const auto index = detail::checked_integer<std::size_t>(params.at("index"),
                                                                            "collection index");
                    outcome = kind == "automation" ? remove_automation(*profile, index)
                                                   : remove_modulation(*profile, index);
                }
                if (!outcome) return error_response("Entry does not exist or still has references");
                return {{"success", true}};
            });
    }

    // =========================================================================
    // validate_timbre
    // =========================================================================
    server.register_tool(
        "validate_timbre",
        "Run validation on a TimbreProfile",
        {{"type", "object"},
         {"properties",
          {{"profile_id", {{"type", "integer"}, {"description", "TimbreProfile ID"}}},
           {"sample_rate",
            {{"type", "number"}, {"description", "Sample rate Hz (default 44100)"}}}}},
         {"required", json::array({"profile_id"})}},
        [session](const json& params) -> json {
            const auto profile_id =
                detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id");
            auto* p = session->find(profile_id);
            if (!p) return profile_not_found(profile_id);
            float sr = static_cast<float>(params.value("sample_rate", 44100.0));
            auto diags = validate(*p, sr);
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
}

} // namespace sunny::infrastructure
