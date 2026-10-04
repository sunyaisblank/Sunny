/** Internal closed preview/explicit-adoption evidence contract. */
#pragma once

#include <algorithm>
#include <limits>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>

namespace sunny::infrastructure::managed_detail {

inline bool recovery_fields(const nlohmann::json& value,
                            std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&value](auto name) { return value.contains(name); });
}

inline bool recovery_key(const nlohmann::json& value) {
    if (!value.is_string()) return false;
    const auto& s = value.get_ref<const std::string&>();
    return !s.empty() && s.size() <= 64 && std::ranges::all_of(s, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

inline bool recovery_hex(const nlohmann::json& value, std::size_t length) {
    if (!value.is_string()) return false;
    const auto& s = value.get_ref<const std::string&>();
    return s.size() == length && std::ranges::all_of(s, [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

inline bool recovery_text(const nlohmann::json& value, std::size_t limit, bool empty = false) {
    return value.is_string() && (empty || !value.get_ref<const std::string&>().empty()) &&
           value.get_ref<const std::string&>().size() <= limit && canonical_managed_bytes(value);
}

inline bool recovery_context(const nlohmann::json& value) {
    return recovery_fields(value, {"bridge_instance", "document_token"}) &&
           recovery_key(value.at("bridge_instance")) && recovery_key(value.at("document_token"));
}

inline bool recovery_selector(const nlohmann::json& value) {
    if (recovery_fields(value, {"track_index", "slot_index"}))
        return note_integer(value.at("track_index"), 0, INT32_MAX) &&
               note_integer(value.at("slot_index"), 0, INT32_MAX);
    return recovery_fields(value, {"track_tag", "clip_tag"}) &&
           recovery_text(value.at("track_tag"), 1024) && recovery_text(value.at("clip_tag"), 1024);
}

inline nlohmann::json recovery_domains() {
    return nlohmann::json::array({"existing_note_updates",
                                  "note_population_updates",
                                  "clip_geometry_updates",
                                  "absent_mixer_step_lanes"});
}

// Exact historical grants remain readable; accepting an earlier grant never
// adds a domain to the approved immutable preview or its acknowledgement.
inline bool recovery_domains_valid(const nlohmann::json& value) {
    return value == nlohmann::json::array({"existing_note_updates", "absent_mixer_step_lanes"}) ||
           value == nlohmann::json::array({"existing_note_updates",
                                           "note_population_updates",
                                           "absent_mixer_step_lanes"}) ||
           value == recovery_domains();
}

inline nlohmann::json recovery_preserved_domains() {
    return nlohmann::json::array({"mpe", "follow_actions", "existing_envelopes", "devices"});
}

inline bool recovery_metadata(const nlohmann::json& value) {
    if (!recovery_fields(value,
                         {"schema_version",
                          "outcome",
                          "context",
                          "project_key",
                          "binding_key",
                          "preview_token",
                          "selector",
                          "set_info",
                          "authority_origin",
                          "historical_identity_proven",
                          "allowed_domains",
                          "preserved_unknown_domains"}) ||
        !note_integer(value.at("schema_version"),
                      SUNNY_MANAGED_ADOPTION_SCHEMA_VERSION,
                      SUNNY_MANAGED_ADOPTION_SCHEMA_VERSION) ||
        value.at("outcome") != "previewed" || !recovery_context(value.at("context")) ||
        !recovery_key(value.at("project_key")) || !recovery_key(value.at("binding_key")) ||
        !recovery_hex(value.at("preview_token"), 32) || !recovery_selector(value.at("selector")) ||
        value.at("authority_origin") != "none" || value.at("historical_identity_proven") != false ||
        !recovery_domains_valid(value.at("allowed_domains")) ||
        value.at("preserved_unknown_domains") != recovery_preserved_domains())
        return false;
    const auto& set = value.at("set_info");
    return recovery_fields(set, {"file_path", "name"}) &&
           (set.at("file_path").is_null() || recovery_text(set.at("file_path"), 4096, true)) &&
           (set.at("name").is_null() || recovery_text(set.at("name"), 1024, true));
}

inline bool recovery_observation(const nlohmann::json& metadata,
                                 const nlohmann::json& observation) {
    if (!observation.is_object()) return false;
    auto base = observation;
    base.erase("device_identity");
    base.erase("device_identity_fingerprint");
    base.erase("group_authority");
    base.erase("group_authority_fingerprint");
    if (!recovery_fields(base,
                         {"track_index",
                          "slot_index",
                          "manifest",
                          "content_fingerprint",
                          "note_identity",
                          "note_identity_fingerprint",
                          "track_tag",
                          "clip_tag",
                          "structural_boundary_complete",
                          "content_boundary_complete",
                          "unavailable_reasons"}))
        return false;
    if (!managed_device_detail::device_supplement_valid(observation)) return false;
    const auto checked =
        managed_binding_from_json({{"schema_version", SUNNY_MANAGED_RECEIPT_SCHEMA_VERSION},
                                   {"context", metadata.at("context")},
                                   {"project_key", metadata.at("project_key")},
                                   {"binding_key", metadata.at("binding_key")},
                                   {"observation", observation}});
    if (!checked || !group_touched_boundary(observation,
                                            {metadata.at("context").at("bridge_instance"),
                                             metadata.at("context").at("document_token")},
                                            metadata.at("project_key"),
                                            metadata.at("binding_key")))
        return false;
    const auto& identity = observation.at("note_identity");
    if (identity.at("entire_clip_population_observed") != true ||
        identity.at("notes").size() > 65536 || observation.at("content_boundary_complete") != false)
        return false;
    const auto& manifest = observation.at("manifest");
    if (observation.contains("device_identity") &&
        manifest.at("devices_empty") != observation.at("device_identity").at("cohort").empty())
        return false;
    const auto& track = manifest.at("track");
    const auto& clip = manifest.at("clip");
    if (track.at("name") != observation.at("track_tag") ||
        clip.at("name") != observation.at("clip_tag") ||
        manifest.at("mpe_note_expression_state_observed") != false ||
        manifest.at("follow_actions_state_observed") != false || track.at("arm") != false ||
        track.at("implicit_arm") != false || track.at("is_frozen") != false ||
        clip.at("is_session_clip") != true || clip.at("is_arrangement_clip") != false ||
        clip.at("is_midi_clip") != true || clip.at("is_audio_clip") != false ||
        clip.at("looping") != false || clip.at("start_marker").get<double>() != 0.0 ||
        clip.at("end_marker").get<double>() <= 0.0)
        return false;
    for (const auto* name :
         {"is_playing", "is_recording", "is_overdubbing", "is_triggered", "will_record_on_start"})
        if (clip.at(name) != false) return false;
    const auto& selector = metadata.at("selector");
    if (selector.contains("track_index"))
        return selector.at("track_index") == observation.at("track_index") &&
               selector.at("slot_index") == observation.at("slot_index");
    return selector.at("track_tag") == observation.at("track_tag") &&
           selector.at("clip_tag") == observation.at("clip_tag");
}

} // namespace sunny::infrastructure::managed_detail
