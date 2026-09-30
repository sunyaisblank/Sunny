/**
 * @file core_tools.cpp
 * @brief MCP Tool Registration implementation
 *
 *
 * Maps MCP tool calls to Orchestrator and Core functions.
 */

#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/harmony/harmonic_function.hpp>
#include <sunny/core/harmony/negative_harmony.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/rhythm/euclidean.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/scale/generation.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>
#include <sunny/infrastructure/mcp/core_tools.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;

namespace {

/// Decline result for Ableton-mutating tools when no transport is connected
json offline_decline() {
    return {{"success", false},
            {"error",
             "Ableton is not connected. Set SUNNY_ABLETON_HOST (and optionally "
             "SUNNY_TCP_PORT) and ensure the Sunny Remote Script control surface "
             "is active in Live."}};
}

/// Merge an orchestrator result with the dispatch outcome
json delivery_result(const OrchestratorResult& result, const DispatchReport& report) {
    json out = {{"success", result.success && report.all_ok()},
                {"operation_id", result.operation_id},
                {"message", result.message},
                {"commands_sent", report.sent}};
    if (!report.all_ok()) {
        out["errors"] = report.errors;
    }
    return out;
}

json lom_value_json(const LomValue& value) {
    return std::visit([](const auto& item) -> json { return item; }, value);
}

json history_delivery_result(const char* action,
                             Orchestrator& orchestrator,
                             const DispatchReport& report) {
    json out = {{"success", report.all_ok()},
                {"action", action},
                {"commands_sent", report.sent},
                {"can_undo", orchestrator.can_undo()},
                {"can_redo", orchestrator.can_redo()}};
    if (!report.all_ok()) out["errors"] = report.errors;
    return out;
}

} // namespace

void register_sunny_tools(McpServer& server,
                          Orchestrator& orchestrator,
                          BridgeDispatcher& dispatcher) {

    // =========================================================================
    // create_progression_clip
    // =========================================================================
    server.register_tool(
        "create_progression_clip",
        "Create a chord progression clip in Ableton Live with voice leading",
        {{"type", "object"},
         {"properties",
          {{"track_index", {{"type", "integer"}, {"description", "Track index"}}},
           {"slot_index", {{"type", "integer"}, {"description", "Clip slot index"}}},
           {"root", {{"type", "string"}, {"description", "Root note (e.g. C, F#, Bb)"}}},
           {"scale", {{"type", "string"}, {"description", "Scale name (e.g. major, minor)"}}},
           {"numerals",
            {{"type", "array"},
             {"items", {{"type", "string"}}},
             {"description", "Roman numerals (e.g. [\"I\", \"IV\", \"V\", \"I\"])"}}},
           {"octave", {{"type", "integer"}, {"description", "Base octave (default 4)"}}},
           {"duration_beats",
            {{"type", "number"}, {"description", "Total duration in beats (default 4.0)"}}}}},
         {"required", json::array({"track_index", "slot_index", "root", "scale", "numerals"})}},
        [&orchestrator, &dispatcher](const json& params) -> json {
            if (!dispatcher.online()) {
                return offline_decline();
            }
            auto result = orchestrator.create_progression_clip(
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                params.at("root").get<std::string>(),
                params.at("scale").get<std::string>(),
                params.at("numerals").get<std::vector<std::string>>(),
                sunny::core::detail::checked_integer_or<int>(params, "octave", 4, "octave"),
                params.value("duration_beats", 4.0));
            if (!result.success) {
                return {{"success", false},
                        {"operation_id", result.operation_id},
                        {"message", result.message}};
            }
            return delivery_result(result, dispatcher.dispatch(orchestrator.drain_messages()));
        });

    // =========================================================================
    // apply_euclidean_rhythm
    // =========================================================================
    server.register_tool(
        "apply_euclidean_rhythm",
        "Create a Euclidean rhythm pattern clip in Ableton Live",
        {{"type", "object"},
         {"properties",
          {{"track_index", {{"type", "integer"}, {"description", "Track index"}}},
           {"slot_index", {{"type", "integer"}, {"description", "Clip slot index"}}},
           {"pulses", {{"type", "integer"}, {"description", "Number of active pulses"}}},
           {"steps", {{"type", "integer"}, {"description", "Total steps in pattern"}}},
           {"pitch", {{"type", "integer"}, {"description", "MIDI note number (default 60)"}}},
           {"step_duration",
            {{"type", "number"},
             {"description", "Duration of each step in beats (default 0.25)"}}}}},
         {"required", json::array({"track_index", "slot_index", "pulses", "steps"})}},
        [&orchestrator, &dispatcher](const json& params) -> json {
            if (!dispatcher.online()) {
                return offline_decline();
            }
            int pitch_val =
                sunny::core::detail::checked_integer_or<int>(params, "pitch", 60, "MIDI pitch");
            auto pitch = sunny::core::MidiNote::from_int(pitch_val);
            if (!pitch) {
                return {{"error", "pitch must be 0-127, got " + std::to_string(pitch_val)}};
            }
            auto result = orchestrator.apply_euclidean_rhythm(
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                sunny::core::detail::checked_integer<int>(params.at("pulses"), "pulse count"),
                sunny::core::detail::checked_integer<int>(params.at("steps"), "step count"),
                *pitch,
                params.value("step_duration", 0.25));
            if (!result.success) {
                return {{"success", false},
                        {"operation_id", result.operation_id},
                        {"message", result.message}};
            }
            return delivery_result(result, dispatcher.dispatch(orchestrator.drain_messages()));
        });

    // =========================================================================
    // apply_arpeggio
    // =========================================================================
    server.register_tool(
        "apply_arpeggio",
        "Create an arpeggiated clip from chord numerals",
        {{"type", "object"},
         {"properties",
          {{"track_index", {{"type", "integer"}, {"description", "Track index"}}},
           {"slot_index", {{"type", "integer"}, {"description", "Clip slot index"}}},
           {"numerals",
            {{"type", "array"},
             {"items", {{"type", "string"}}},
             {"description", "Roman numerals for chord source"}}},
           {"direction",
            {{"type", "string"}, {"description", "up, down, updown, downup, random, order"}}},
           {"step_duration",
            {{"type", "number"}, {"description", "Step duration in beats (default 0.25)"}}}}},
         {"required", json::array({"track_index", "slot_index", "numerals", "direction"})}},
        [&orchestrator, &dispatcher](const json& params) -> json {
            if (!dispatcher.online()) {
                return offline_decline();
            }
            auto result = orchestrator.apply_arpeggio(
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                params.at("numerals").get<std::vector<std::string>>(),
                params.at("direction").get<std::string>(),
                params.value("step_duration", 0.25));
            if (!result.success) {
                return {{"success", false},
                        {"operation_id", result.operation_id},
                        {"message", result.message}};
            }
            return delivery_result(result, dispatcher.dispatch(orchestrator.drain_messages()));
        });

    // =========================================================================
    // get_scale_notes
    // =========================================================================
    server.register_tool(
        "get_scale_notes",
        "Get MIDI note numbers for a scale",
        {{"type", "object"},
         {"properties",
          {{"root", {{"type", "string"}, {"description", "Root note name (e.g. C, F#)"}}},
           {"scale", {{"type", "string"}, {"description", "Scale name"}}},
           {"octave", {{"type", "integer"}, {"description", "Base octave (default 4)"}}}}},
         {"required", json::array({"root", "scale"})}},
        [](const json& params) -> json {
            auto root_result =
                sunny::core::note_to_pitch_class(params.at("root").get<std::string>());
            if (!root_result) {
                return {{"error", "Invalid root note"}};
            }

            auto scale_def = sunny::core::find_scale(params.at("scale").get<std::string>());
            if (!scale_def) {
                return {{"error", "Unknown scale"}};
            }

            int octave =
                sunny::core::detail::checked_integer_or<int>(params, "octave", 4, "octave");
            auto notes =
                sunny::core::generate_scale_notes(*root_result, scale_def->intervals, octave);
            if (!notes) {
                return {{"error", "Scale generation failed"}};
            }

            json note_array = json::array();
            for (auto n : *notes) {
                note_array.push_back(static_cast<int>(n));
            }
            return {{"notes", note_array}};
        });

    // =========================================================================
    // analyze_harmony
    // =========================================================================
    server.register_tool(
        "analyze_harmony",
        "Analyze a chord's harmonic function in a key",
        {{"type", "object"},
         {"properties",
          {{"chord_notes",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Pitch classes of the chord (0-11)"}}},
           {"key_root", {{"type", "integer"}, {"description", "Key root pitch class (0-11)"}}},
           {"is_minor", {{"type", "boolean"}, {"description", "Whether the key is minor"}}}}},
         {"required", json::array({"chord_notes", "key_root"})}},
        [](const json& params) -> json {
            sunny::core::PitchClassSet pcs;
            for (const auto& pc : params.at("chord_notes")) {
                int val = sunny::core::detail::checked_integer<int>(pc, "pitch class");
                auto pc_val = sunny::core::PitchClass::from_int(val);
                if (!pc_val)
                    return {{"error", "Pitch class must be 0-11, got " + std::to_string(val)}};
                pcs.insert(*pc_val);
            }

            int key_root_val = sunny::core::detail::checked_integer<int>(params.at("key_root"),
                                                                         "key-root pitch class");
            auto key_root = sunny::core::PitchClass::from_int(key_root_val);
            if (!key_root)
                return {{"error", "key_root must be 0-11, got " + std::to_string(key_root_val)}};

            auto analysis = sunny::core::analyze_chord_function(
                pcs, *key_root, params.value("is_minor", false));

            return {{"root", static_cast<int>(analysis.root)},
                    {"quality", analysis.quality},
                    {"function", std::string(sunny::core::function_to_string(analysis.function))},
                    {"numeral", analysis.numeral},
                    {"degree", analysis.degree}};
        });

    // =========================================================================
    // generate_negative_harmony
    // =========================================================================
    server.register_tool(
        "generate_negative_harmony",
        "Transform a chord using negative harmony (axis inversion)",
        {{"type", "object"},
         {"properties",
          {{"chord_notes",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Pitch classes to transform (0-11)"}}},
           {"key_root", {{"type", "integer"}, {"description", "Key root pitch class (0-11)"}}}}},
         {"required", json::array({"chord_notes", "key_root"})}},
        [](const json& params) -> json {
            sunny::core::PitchClassSet pcs;
            for (const auto& pc : params.at("chord_notes")) {
                int val = sunny::core::detail::checked_integer<int>(pc, "pitch class");
                auto pc_val = sunny::core::PitchClass::from_int(val);
                if (!pc_val)
                    return {{"error", "Pitch class must be 0-11, got " + std::to_string(val)}};
                pcs.insert(*pc_val);
            }

            int key_root_val = sunny::core::detail::checked_integer<int>(params.at("key_root"),
                                                                         "key-root pitch class");
            auto key_root = sunny::core::PitchClass::from_int(key_root_val);
            if (!key_root)
                return {{"error", "key_root must be 0-11, got " + std::to_string(key_root_val)}};

            auto result = sunny::core::negative_harmony(pcs, *key_root);

            json notes = json::array();
            for (auto pc : result) {
                notes.push_back(static_cast<int>(pc));
            }
            return {{"notes", notes}};
        });

    // =========================================================================
    // voice_lead
    // =========================================================================
    server.register_tool(
        "voice_lead",
        "Compute optimal voice leading between two chords",
        {{"type", "object"},
         {"properties",
          {{"source_notes",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Current chord as MIDI notes"}}},
           {"target_pcs",
            {{"type", "array"},
             {"items", {{"type", "integer"}}},
             {"description", "Target chord as pitch classes (0-11)"}}},
           {"lock_bass", {{"type", "boolean"}, {"description", "Lock bass note to root"}}}}},
         {"required", json::array({"source_notes", "target_pcs"})}},
        [](const json& params) -> json {
            auto source_vec = params.at("source_notes").get<std::vector<int>>();
            auto target_vec = params.at("target_pcs").get<std::vector<int>>();

            std::vector<sunny::core::MidiNote> source;
            source.reserve(source_vec.size());
            for (auto n : source_vec) {
                auto note = sunny::core::MidiNote::from_int(n);
                if (!note) return {{"error", "MIDI note must be 0-127, got " + std::to_string(n)}};
                source.push_back(*note);
            }

            std::vector<sunny::core::PitchClass> target;
            target.reserve(target_vec.size());
            for (auto pc : target_vec) {
                auto pc_val = sunny::core::PitchClass::from_int(pc);
                if (!pc_val)
                    return {{"error", "Pitch class must be 0-11, got " + std::to_string(pc)}};
                target.push_back(*pc_val);
            }

            auto result = sunny::core::voice_lead_nearest_tone(
                source, target, params.value("lock_bass", false));
            if (!result) {
                return {{"error", "Voice leading failed"}};
            }

            json notes = json::array();
            for (auto n : result->voiced_notes) {
                notes.push_back(static_cast<int>(n));
            }
            return {{"notes", notes},
                    {"total_motion", result->total_motion},
                    {"parallel_fifths", result->has_parallel_fifths},
                    {"parallel_octaves", result->has_parallel_octaves}};
        });

    // =========================================================================
    // get_ableton_session_state
    // =========================================================================
    server.register_tool(
        "get_ableton_session_state",
        "Read transport, tempo, meter, and track counts from the live Ableton set",
        {{"type", "object"}, {"properties", json::object()}},
        [&orchestrator, &dispatcher](const json&) -> json {
            if (!dispatcher.online()) return offline_decline();

            json state = {{"success", true},
                          {"connected", true},
                          {"can_undo", orchestrator.can_undo()},
                          {"can_redo", orchestrator.can_redo()}};
            json errors = json::array();

            auto target = dispatcher.target_profile();
            if (!target) {
                errors.push_back("target_profile: incompatible or malformed bridge handshake");
            } else if (*target) {
                state["target_profile"] = target_profile_to_json(**target);
            } else {
                state["target_profile"] = nullptr;
            }

            const std::vector<std::pair<std::string, std::string>> properties = {
                {"tempo", "tempo"},
                {"signature_numerator", "signature_numerator"},
                {"signature_denominator", "signature_denominator"},
                {"is_playing", "is_playing"},
                {"current_song_time", "current_song_time"},
                {"track_count", "tracks"},
                {"return_track_count", "return_tracks"}};

            for (const auto& [output_name, property] : properties) {
                auto response =
                    dispatcher.request(LomProtocol::get_property(LomPaths::song(), property));
                if (!response.success || !response.value) {
                    errors.push_back(property + ": " + response.error.value_or("no value"));
                    continue;
                }

                auto value = lom_value_json(*response.value);
                if (output_name == "track_count" || output_name == "return_track_count") {
                    state[output_name] = value.is_array() ? value.size() : 0;
                } else {
                    state[output_name] = std::move(value);
                }
            }

            if (!errors.empty()) {
                state["success"] = false;
                state["errors"] = std::move(errors);
            }
            return state;
        });

    // =========================================================================
    // undo_ableton_operation / redo_ableton_operation
    // =========================================================================
    server.register_tool(
        "undo_ableton_operation",
        "Undo Sunny's most recent live Ableton clip operation",
        {{"type", "object"}, {"properties", json::object()}},
        [&orchestrator, &dispatcher](const json&) -> json {
            if (!dispatcher.online()) return offline_decline();
            if (!orchestrator.undo()) {
                return {{"success", false},
                        {"error", "No Sunny Ableton operation is available to undo"},
                        {"can_undo", false},
                        {"can_redo", orchestrator.can_redo()}};
            }
            return history_delivery_result(
                "undo", orchestrator, dispatcher.dispatch(orchestrator.drain_messages()));
        });

    server.register_tool(
        "redo_ableton_operation",
        "Redo Sunny's most recently undone live Ableton clip operation",
        {{"type", "object"}, {"properties", json::object()}},
        [&orchestrator, &dispatcher](const json&) -> json {
            if (!dispatcher.online()) return offline_decline();
            if (!orchestrator.redo()) {
                return {{"success", false},
                        {"error", "No Sunny Ableton operation is available to redo"},
                        {"can_undo", orchestrator.can_undo()},
                        {"can_redo", false}};
            }
            return history_delivery_result(
                "redo", orchestrator, dispatcher.dispatch(orchestrator.drain_messages()));
        });
}

} // namespace sunny::infrastructure
