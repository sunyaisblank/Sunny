/**
 * @file MCPT001A.cpp
 * @brief MCP Tool Registration implementation
 *
 * Component: MCPT001A
 *
 * Maps MCP tool calls to Orchestrator and Core functions.
 */

#include "MCPT001A.h"

#include "Pitch/PTPC001A.h"
#include "Scale/SCDF001A.h"
#include "Scale/SCGN001A.h"
#include "Harmony/HRFN001A.h"
#include "Harmony/HRNG001A.h"
#include "Harmony/HRRN001A.h"
#include "VoiceLeading/VLNT001A.h"
#include "Rhythm/RHEU001A.h"

namespace Sunny::Infrastructure {

using json = nlohmann::json;

void register_sunny_tools(McpServer& server, Orchestrator& orchestrator) {

    // =========================================================================
    // create_progression_clip
    // =========================================================================
    server.register_tool(
        "create_progression_clip",
        "Create a chord progression clip in Ableton Live with voice leading",
        {
            {"type", "object"},
            {"properties", {
                {"track_index",    {{"type", "integer"}, {"description", "Track index"}}},
                {"slot_index",     {{"type", "integer"}, {"description", "Clip slot index"}}},
                {"root",           {{"type", "string"},  {"description", "Root note (e.g. C, F#, Bb)"}}},
                {"scale",          {{"type", "string"},  {"description", "Scale name (e.g. major, minor)"}}},
                {"numerals",       {{"type", "array"},   {"items", {{"type", "string"}}},
                                    {"description", "Roman numerals (e.g. [\"I\", \"IV\", \"V\", \"I\"])"}}},
                {"octave",         {{"type", "integer"}, {"description", "Base octave (default 4)"}}},
                {"duration_beats", {{"type", "number"},  {"description", "Total duration in beats (default 4.0)"}}}
            }},
            {"required", json::array({"track_index", "slot_index", "root", "scale", "numerals"})}
        },
        [&orchestrator](const json& params) -> json {
            auto result = orchestrator.create_progression_clip(
                params.at("track_index").get<int>(),
                params.at("slot_index").get<int>(),
                params.at("root").get<std::string>(),
                params.at("scale").get<std::string>(),
                params.at("numerals").get<std::vector<std::string>>(),
                params.value("octave", 4),
                params.value("duration_beats", 4.0)
            );
            return {
                {"success", result.success},
                {"operation_id", result.operation_id},
                {"message", result.message}
            };
        }
    );

    // =========================================================================
    // apply_euclidean_rhythm
    // =========================================================================
    server.register_tool(
        "apply_euclidean_rhythm",
        "Create a Euclidean rhythm pattern clip in Ableton Live",
        {
            {"type", "object"},
            {"properties", {
                {"track_index",   {{"type", "integer"}, {"description", "Track index"}}},
                {"slot_index",    {{"type", "integer"}, {"description", "Clip slot index"}}},
                {"pulses",        {{"type", "integer"}, {"description", "Number of active pulses"}}},
                {"steps",         {{"type", "integer"}, {"description", "Total steps in pattern"}}},
                {"pitch",         {{"type", "integer"}, {"description", "MIDI note number (default 60)"}}},
                {"step_duration", {{"type", "number"},  {"description", "Duration of each step in beats (default 0.25)"}}}
            }},
            {"required", json::array({"track_index", "slot_index", "pulses", "steps"})}
        },
        [&orchestrator](const json& params) -> json {
            auto result = orchestrator.apply_euclidean_rhythm(
                params.at("track_index").get<int>(),
                params.at("slot_index").get<int>(),
                params.at("pulses").get<int>(),
                params.at("steps").get<int>(),
                static_cast<Core::MidiNote>(params.value("pitch", 60)),
                params.value("step_duration", 0.25)
            );
            return {
                {"success", result.success},
                {"operation_id", result.operation_id},
                {"message", result.message}
            };
        }
    );

    // =========================================================================
    // apply_arpeggio
    // =========================================================================
    server.register_tool(
        "apply_arpeggio",
        "Create an arpeggiated clip from chord numerals",
        {
            {"type", "object"},
            {"properties", {
                {"track_index",   {{"type", "integer"}, {"description", "Track index"}}},
                {"slot_index",    {{"type", "integer"}, {"description", "Clip slot index"}}},
                {"numerals",      {{"type", "array"},   {"items", {{"type", "string"}}},
                                   {"description", "Roman numerals for chord source"}}},
                {"direction",     {{"type", "string"},  {"description", "up, down, updown, downup, random, order"}}},
                {"step_duration", {{"type", "number"},  {"description", "Step duration in beats (default 0.25)"}}}
            }},
            {"required", json::array({"track_index", "slot_index", "numerals", "direction"})}
        },
        [&orchestrator](const json& params) -> json {
            auto result = orchestrator.apply_arpeggio(
                params.at("track_index").get<int>(),
                params.at("slot_index").get<int>(),
                params.at("numerals").get<std::vector<std::string>>(),
                params.at("direction").get<std::string>(),
                params.value("step_duration", 0.25)
            );
            return {
                {"success", result.success},
                {"operation_id", result.operation_id},
                {"message", result.message}
            };
        }
    );

    // =========================================================================
    // get_scale_notes
    // =========================================================================
    server.register_tool(
        "get_scale_notes",
        "Get MIDI note numbers for a scale",
        {
            {"type", "object"},
            {"properties", {
                {"root",   {{"type", "string"},  {"description", "Root note name (e.g. C, F#)"}}},
                {"scale",  {{"type", "string"},  {"description", "Scale name"}}},
                {"octave", {{"type", "integer"}, {"description", "Base octave (default 4)"}}}
            }},
            {"required", json::array({"root", "scale"})}
        },
        [](const json& params) -> json {
            auto root_result = Core::note_to_pitch_class(
                params.at("root").get<std::string>());
            if (!root_result) {
                return {{"error", "Invalid root note"}};
            }

            auto scale_def = Core::find_scale(
                params.at("scale").get<std::string>());
            if (!scale_def) {
                return {{"error", "Unknown scale"}};
            }

            int octave = params.value("octave", 4);
            auto notes = Core::generate_scale_notes(
                *root_result, scale_def->intervals, octave);
            if (!notes) {
                return {{"error", "Scale generation failed"}};
            }

            json note_array = json::array();
            for (auto n : *notes) {
                note_array.push_back(static_cast<int>(n));
            }
            return {{"notes", note_array}};
        }
    );

    // =========================================================================
    // analyze_harmony
    // =========================================================================
    server.register_tool(
        "analyze_harmony",
        "Analyze a chord's harmonic function in a key",
        {
            {"type", "object"},
            {"properties", {
                {"chord_notes", {{"type", "array"}, {"items", {{"type", "integer"}}},
                                 {"description", "Pitch classes of the chord (0-11)"}}},
                {"key_root",    {{"type", "integer"}, {"description", "Key root pitch class (0-11)"}}},
                {"is_minor",    {{"type", "boolean"}, {"description", "Whether the key is minor"}}}
            }},
            {"required", json::array({"chord_notes", "key_root"})}
        },
        [](const json& params) -> json {
            Core::PitchClassSet pcs;
            for (auto pc : params.at("chord_notes")) {
                pcs.insert(static_cast<Core::PitchClass>(pc.get<int>()));
            }

            auto analysis = Core::analyze_chord_function(
                pcs,
                static_cast<Core::PitchClass>(params.at("key_root").get<int>()),
                params.value("is_minor", false)
            );

            return {
                {"root", static_cast<int>(analysis.root)},
                {"quality", analysis.quality},
                {"function", std::string(Core::function_to_string(analysis.function))},
                {"numeral", analysis.numeral},
                {"degree", analysis.degree}
            };
        }
    );

    // =========================================================================
    // generate_negative_harmony
    // =========================================================================
    server.register_tool(
        "generate_negative_harmony",
        "Transform a chord using negative harmony (axis inversion)",
        {
            {"type", "object"},
            {"properties", {
                {"chord_notes", {{"type", "array"}, {"items", {{"type", "integer"}}},
                                 {"description", "Pitch classes to transform (0-11)"}}},
                {"key_root",    {{"type", "integer"}, {"description", "Key root pitch class (0-11)"}}}
            }},
            {"required", json::array({"chord_notes", "key_root"})}
        },
        [](const json& params) -> json {
            Core::PitchClassSet pcs;
            for (auto pc : params.at("chord_notes")) {
                pcs.insert(static_cast<Core::PitchClass>(pc.get<int>()));
            }

            auto result = Core::negative_harmony(
                pcs,
                static_cast<Core::PitchClass>(params.at("key_root").get<int>())
            );

            json notes = json::array();
            for (auto pc : result) {
                notes.push_back(static_cast<int>(pc));
            }
            return {{"notes", notes}};
        }
    );

    // =========================================================================
    // voice_lead
    // =========================================================================
    server.register_tool(
        "voice_lead",
        "Compute optimal voice leading between two chords",
        {
            {"type", "object"},
            {"properties", {
                {"source_notes",  {{"type", "array"}, {"items", {{"type", "integer"}}},
                                   {"description", "Current chord as MIDI notes"}}},
                {"target_pcs",    {{"type", "array"}, {"items", {{"type", "integer"}}},
                                   {"description", "Target chord as pitch classes (0-11)"}}},
                {"lock_bass",     {{"type", "boolean"}, {"description", "Lock bass note to root"}}}
            }},
            {"required", json::array({"source_notes", "target_pcs"})}
        },
        [](const json& params) -> json {
            auto source_vec = params.at("source_notes").get<std::vector<int>>();
            auto target_vec = params.at("target_pcs").get<std::vector<int>>();

            std::vector<Core::MidiNote> source;
            source.reserve(source_vec.size());
            for (auto n : source_vec) {
                source.push_back(static_cast<Core::MidiNote>(n));
            }

            std::vector<Core::PitchClass> target;
            target.reserve(target_vec.size());
            for (auto pc : target_vec) {
                target.push_back(static_cast<Core::PitchClass>(pc));
            }

            auto result = Core::voice_lead_nearest_tone(
                source, target, params.value("lock_bass", false));
            if (!result) {
                return {{"error", "Voice leading failed"}};
            }

            json notes = json::array();
            for (auto n : result->voiced_notes) {
                notes.push_back(static_cast<int>(n));
            }
            return {
                {"notes", notes},
                {"total_motion", result->total_motion},
                {"parallel_fifths", result->has_parallel_fifths},
                {"parallel_octaves", result->has_parallel_octaves}
            };
        }
    );
}

}  // namespace Sunny::Infrastructure
