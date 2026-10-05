/**
 * @file core_tools.cpp
 * @brief MCP Tool Registration implementation
 *
 *
 * Maps MCP tool calls to Orchestrator and Core functions.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
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

/// Decline result for Ableton tools when no transport is connected
json offline_decline(const BridgeDispatcher& dispatcher) {
    return {{"success", false}, {"error", dispatcher.offline_reason()}};
}

/// Report what a Live operation, undo, or redo did, and the history it left.
json operation_result(const OrchestratorResult& result, const Orchestrator& orchestrator) {
    json out = {{"success", result.success()},
                {"outcome", std::string(to_string(result.outcome))},
                {"message", result.message},
                {"commands_sent", result.commands_sent},
                {"can_undo", orchestrator.can_undo()},
                {"can_redo", orchestrator.can_redo()}};
    if (!result.operation_id.empty()) out["operation_id"] = result.operation_id;
    if (result.outcome == OperationOutcome::NotAttempted) out["error"] = result.message;
    if (!result.errors.empty()) out["errors"] = result.errors;
    return out;
}

json history_result(const char* action,
                    const OrchestratorResult& result,
                    const Orchestrator& orchestrator) {
    auto out = operation_result(result, orchestrator);
    out["action"] = action;
    return out;
}

json lom_value_json(const LomValue& value) {
    return std::visit([](const auto& item) -> json { return item; }, value);
}

bool log_stream_id(const json& value) {
    return value.is_string() && value.get_ref<const std::string&>().size() == 32 &&
           std::ranges::all_of(value.get_ref<const std::string&>(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

bool log_integer(const json& value, std::int64_t minimum, std::int64_t maximum) {
    return value.is_number_integer() && value >= minimum && value <= maximum;
}

bool log_timestamp(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>()) && value >= 0;
}

bool remote_log_entry(const json& entry, std::int64_t expected, std::int64_t latest) {
    if (!entry.is_object() || entry.size() != 5 || !entry.contains("sequence") ||
        !log_integer(entry.at("sequence"), 1, std::numeric_limits<std::int32_t>::max()) ||
        entry.at("sequence") != expected || expected > latest || !entry.contains("time") ||
        !log_timestamp(entry.at("time")))
        return false;
    for (const auto* key : {"level", "source", "message"}) {
        if (!entry.contains(key) || !entry.at(key).is_string() ||
            entry.at(key).get_ref<const std::string&>().size() >
                (std::string_view(key) == "message" ? 8000 : 512))
            return false;
    }
    return true;
}

bool remote_log_response_size(const json& page) {
    const json envelope = {{"success", true},
                           {"value", page},
                           {"bridge_protocol_version", std::numeric_limits<std::uint32_t>::max()}};
    return envelope.dump(-1, ' ', true).size() <= 16 * 1024 * 1024;
}

/// Old bridges remain readable for mismatch diagnosis; continuity is unproven.
bool legacy_remote_log_page(const json& page, int after) {
    if (!page.is_object() || page.size() != 3 || !page.contains("entries") ||
        !page.at("entries").is_array() || page.at("entries").size() > 1000 ||
        !page.contains("truncated") || !page.at("truncated").is_boolean() ||
        !page.contains("next_sequence") ||
        !log_integer(page.at("next_sequence"), 0, std::numeric_limits<std::int32_t>::max()))
        return false;
    const auto latest = page.at("next_sequence").get<std::int64_t>();
    const auto& entries = page.at("entries");
    if (entries.empty())
        return latest <= after && page.at("truncated") == false && remote_log_response_size(page);
    if (!entries.front().is_object() || !entries.front().contains("sequence") ||
        !log_integer(entries.front().at("sequence"), 1, latest))
        return false;
    auto expected = entries.front().at("sequence").get<std::int64_t>();
    if (expected <= after || page.at("truncated") != (expected > std::int64_t(after) + 1))
        return false;
    for (const auto& entry : entries)
        if (!remote_log_entry(entry, expected++, latest)) return false;
    return expected - 1 == latest && remote_log_response_size(page);
}

/// Validate the cursor against the request, not merely the response's field types.
bool remote_log_page(const json& page, int after, const std::optional<std::string>& stream) {
    constexpr std::int64_t maximum = std::numeric_limits<std::int32_t>::max();
    if (!page.is_object() || page.size() != 9 || !page.contains("entries") ||
        !page.at("entries").is_array() || !page.contains("stream_id") ||
        !log_stream_id(page.at("stream_id")) || !page.contains("observed_at") ||
        !log_timestamp(page.at("observed_at")))
        return false;
    for (const auto* key : {"truncated", "reset", "has_more"})
        if (!page.contains(key) || !page.at(key).is_boolean()) return false;
    for (const auto* key : {"next_sequence", "latest_sequence"})
        if (!page.contains(key) || !log_integer(page.at(key), 0, maximum)) return false;
    if (!page.contains("oldest_sequence") ||
        !log_integer(page.at("oldest_sequence"), 1, maximum + 1))
        return false;
    const auto latest = page.at("latest_sequence").get<std::int64_t>();
    const auto oldest = page.at("oldest_sequence").get<std::int64_t>();
    const auto next = page.at("next_sequence").get<std::int64_t>();
    const bool reset =
        (stream && *stream != page.at("stream_id").get_ref<const std::string&>()) || after > latest;
    const std::int64_t effective_after = reset ? 0 : after;
    if (page.at("reset") != reset || oldest > latest + 1 ||
        page.at("truncated") != (oldest > effective_after + 1) || page.at("entries").size() > 1000)
        return false;
    auto expected = std::max<std::int64_t>(effective_after + 1, oldest);
    for (const auto& entry : page.at("entries"))
        if (!remote_log_entry(entry, expected++, latest)) return false;
    const auto& entries = page.at("entries");
    const auto delivered = entries.empty() ? effective_after : expected - 1;
    if (next != delivered || page.at("has_more") != (next < latest) ||
        (entries.empty() && next < latest))
        return false; // A bounded record always fits: an empty page must make no progress owed.
    return remote_log_response_size(page);
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
                return offline_decline(dispatcher);
            }
            auto result = orchestrator.create_progression_clip(
                dispatcher,
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                params.at("root").get<std::string>(),
                params.at("scale").get<std::string>(),
                params.at("numerals").get<std::vector<std::string>>(),
                sunny::core::detail::checked_integer_or<int>(params, "octave", 4, "octave"),
                params.value("duration_beats", 4.0));
            return operation_result(result, orchestrator);
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
                return offline_decline(dispatcher);
            }
            int pitch_val =
                sunny::core::detail::checked_integer_or<int>(params, "pitch", 60, "MIDI pitch");
            auto pitch = sunny::core::MidiNote::from_int(pitch_val);
            if (!pitch) {
                return {{"error", "pitch must be 0-127, got " + std::to_string(pitch_val)}};
            }
            auto result = orchestrator.apply_euclidean_rhythm(
                dispatcher,
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                sunny::core::detail::checked_integer<int>(params.at("pulses"), "pulse count"),
                sunny::core::detail::checked_integer<int>(params.at("steps"), "step count"),
                *pitch,
                params.value("step_duration", 0.25));
            return operation_result(result, orchestrator);
        });

    // =========================================================================
    // apply_arpeggio
    // =========================================================================
    server.register_tool(
        "apply_arpeggio",
        "Create a clip arpeggiating each chord numeral in turn in the given key",
        {{"type", "object"},
         {"properties",
          {{"track_index", {{"type", "integer"}, {"description", "Track index"}}},
           {"slot_index", {{"type", "integer"}, {"description", "Clip slot index"}}},
           {"root", {{"type", "string"}, {"description", "Key root note (e.g. C, F#, Bb)"}}},
           {"scale", {{"type", "string"}, {"description", "Scale name (e.g. major, minor)"}}},
           {"numerals",
            {{"type", "array"},
             {"items", {{"type", "string"}}},
             {"description", "Roman numerals for chord source"}}},
           {"direction",
            {{"type", "string"}, {"description", "up, down, updown, downup, random, order"}}},
           {"step_duration",
            {{"type", "number"}, {"description", "Step duration in beats (default 0.25)"}}}}},
         {"required",
          json::array({"track_index", "slot_index", "root", "scale", "numerals", "direction"})}},
        [&orchestrator, &dispatcher](const json& params) -> json {
            if (!dispatcher.online()) {
                return offline_decline(dispatcher);
            }
            auto result = orchestrator.apply_arpeggio(
                dispatcher,
                sunny::core::detail::checked_integer<int>(params.at("track_index"), "track index"),
                sunny::core::detail::checked_integer<int>(params.at("slot_index"), "slot index"),
                params.at("root").get<std::string>(),
                params.at("scale").get<std::string>(),
                params.at("numerals").get<std::vector<std::string>>(),
                params.at("direction").get<std::string>(),
                params.value("step_duration", 0.25));
            return operation_result(result, orchestrator);
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
                sunny::core::generate_scale_notes(*root_result, scale_def->get_intervals(), octave);
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
            if (!dispatcher.online()) return offline_decline(dispatcher);

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
                {"track_count", "sunny_get_track_count"},
                {"return_track_count", "sunny_get_return_track_count"}};

            for (const auto& [output_name, property] : properties) {
                // The bridge exposes collection sizes only as closed count calls;
                // the collections themselves are private host objects.
                const bool count = property.starts_with("sunny_get_");
                auto response = dispatcher.request(
                    count ? LomProtocol::call_method(LomPaths::song(), property, {})
                          : LomProtocol::get_property(LomPaths::song(), property));
                if (!response.success || !response.value) {
                    errors.push_back(property + ": " + response.error.value_or("no value"));
                    continue;
                }

                auto value = lom_value_json(*response.value);
                if (count && (!value.is_number_integer() || value.get<std::int64_t>() < 0)) {
                    errors.push_back(property + ": malformed count");
                    continue;
                }
                state[output_name] = std::move(value);
            }

            if (!errors.empty()) {
                state["success"] = false;
                state["errors"] = std::move(errors);
            }
            return state;
        });

    // =========================================================================
    // get_ableton_remote_log
    // =========================================================================
    // The Remote Script's own records (requests, refusals, exceptions) live on
    // the Ableton machine; this reads them over the bridge so a client on any
    // machine can see what happened inside Live.
    server.register_tool(
        "get_ableton_remote_log",
        "Read recent records from the Sunny Remote Script inside Ableton Live: every request "
        "with its outcome, refusals and errors. Pass after_sequence (the next_sequence of a "
        "previous call) and its stream_id to receive newer records. reset signals a changed "
        "stream or ahead cursor; truncated signals discarded unseen history. has_more means "
        "another bounded page is available. observed_at is a fresh log observation, not Live "
        "readiness. A legacy bridge read exposes unavailable cursor metadata as null.",
        {{"type", "object"},
         {"additionalProperties", false},
         {"properties",
          {{"after_sequence",
            {{"type", "integer"},
             {"minimum", 0},
             {"maximum", std::numeric_limits<std::int32_t>::max()},
             {"description", "integer (optional, default 0 for retained records)"}}},
           {"stream_id",
            {{"type", "string"},
             {"pattern", "^[0-9a-f]{32}$"},
             {"description", "string (optional, stream_id from the previous page)"}}}}}},
        [&dispatcher](const json& params) -> json {
            const auto after = sunny::core::detail::checked_integer_or<int>(
                params, "after_sequence", 0, "after_sequence");
            if (after < 0 ||
                (params.contains("stream_id") && !log_stream_id(params.at("stream_id"))))
                return {{"success", false}, {"error", "Invalid remote log cursor"}};
            if (!dispatcher.online()) return offline_decline(dispatcher);
            std::optional<std::string> stream;
            std::vector<LomValue> args{after};
            if (params.contains("stream_id")) {
                stream = params.at("stream_id").get<std::string>();
                args.emplace_back(*stream);
            }
            auto response = dispatcher.request(
                LomProtocol::call_method(LomPaths::song(), "sunny_get_remote_log", args));
            if (!response.success || !response.value)
                return {{"success", false},
                        {"error", response.error.value_or("Remote log unavailable")}};
            auto log = lom_value_json(*response.value);
            if (!remote_log_page(log, after, stream)) {
                if (stream || !legacy_remote_log_page(log, after))
                    return {{"success", false}, {"error", "Malformed remote log response"}};
                // Diagnostic bypass must still read an older mismatched bridge.
                // Null is unavailable evidence, never a manufactured reset or
                // fresh timestamp. It cannot qualify the new cursor contract.
                for (const auto* key : {"stream_id",
                                        "reset",
                                        "observed_at",
                                        "oldest_sequence",
                                        "latest_sequence",
                                        "has_more"})
                    log[key] = nullptr;
                log["cursor_metadata_available"] = false;
            }
            log["success"] = true;
            return log;
        });

    // =========================================================================
    // undo_ableton_operation / redo_ableton_operation
    // =========================================================================
    server.register_tool("undo_ableton_operation",
                         "Undo Sunny's most recent live Ableton clip operation",
                         {{"type", "object"}, {"properties", json::object()}},
                         [&orchestrator, &dispatcher](const json&) -> json {
                             if (!dispatcher.online()) return offline_decline(dispatcher);
                             return history_result(
                                 "undo", orchestrator.undo(dispatcher), orchestrator);
                         });

    server.register_tool("redo_ableton_operation",
                         "Redo Sunny's most recently undone live Ableton clip operation",
                         {{"type", "object"}, {"properties", json::object()}},
                         [&orchestrator, &dispatcher](const json&) -> json {
                             if (!dispatcher.online()) return offline_decline(dispatcher);
                             return history_result(
                                 "redo", orchestrator.redo(dispatcher), orchestrator);
                         });
}

} // namespace sunny::infrastructure
