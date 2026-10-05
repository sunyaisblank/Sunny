#include <array>
#include <cmath>
#include <map>
#include <ranges>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_envelope_author.hpp>
#include <sunny/infrastructure/ableton/detail/managed_envelope_revision.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/detail/managed_geometry.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>
#include <sunny/infrastructure/ableton/detail/managed_routing.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_song_settings.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;

bool key(const json& value) {
    if (!value.is_string()) return false;
    const auto& s = value.get_ref<const std::string&>();
    return !s.empty() && s.size() <= 64 && std::ranges::all_of(s, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

bool fingerprint(const json& value) {
    if (!value.is_string()) return false;
    const auto& s = value.get_ref<const std::string&>();
    return s.size() == 64 && std::ranges::all_of(s, [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

bool context_valid(const ManagedBridgeContext& context) {
    return key(context.bridge_instance) && key(context.document_token);
}

const json* payload(const LomRequest& request) {
    return request.args.size() == 1 ? std::get_if<json>(&request.args[0]) : nullptr;
}

bool mutation(const LomRequest& request) {
    return request.type == LomRequestType::CallMethod && request.path.to_string() == "song" &&
           (request.property_or_method == "sunny_managed_create_clip" ||
            request.property_or_method == "sunny_managed_replace_clip" ||
            request.property_or_method == "sunny_managed_rebind" ||
            request.property_or_method == "sunny_managed_author_envelope" ||
            request.property_or_method == "sunny_managed_update_notes" ||
            request.property_or_method == "sunny_managed_revise_note_population" ||
            request.property_or_method == "sunny_managed_update_clip_geometry" ||
            request.property_or_method == "sunny_managed_adopt_clip" ||
            request.property_or_method == "sunny_managed_insert_device" ||
            request.property_or_method == "sunny_managed_update_device_parameters" ||
            request.property_or_method == "sunny_managed_update_device_modes" ||
            request.property_or_method == "sunny_managed_replace_envelope" ||
            request.property_or_method == "sunny_managed_adopt_devices" ||
            request.property_or_method == "sunny_managed_adopt_static_mixer" ||
            request.property_or_method == "sunny_managed_update_static_mixer" ||
            request.property_or_method == "sunny_managed_apply_routing" ||
            request.property_or_method == "sunny_managed_apply_song_settings");
}

json context_json(const ManagedBridgeContext& context) {
    return {{"bridge_instance", context.bridge_instance},
            {"document_token", context.document_token}};
}

Result<ManagedBridgeContext> parse_context(const json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("bridge_instance") ||
        !value.contains("document_token") || !key(value.at("bridge_instance")) ||
        !key(value.at("document_token")))
        return std::unexpected(ErrorCode::ProtocolError);
    return ManagedBridgeContext{value.at("bridge_instance").get<std::string>(),
                                value.at("document_token").get<std::string>()};
}

constexpr std::array outcome_names{"prepared",
                                   "not_sent",
                                   "acknowledged",
                                   "declined",
                                   "indeterminate",
                                   "unknown_operation",
                                   "unknown_epoch"};
constexpr std::array delivery_names{"not_sent", "sent_without_valid_response", "response_received"};

bool fields(const json& value, std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&value](auto name) { return value.contains(name); });
}

bool integer(const json& value, std::int64_t lower, std::int64_t upper) {
    if (!value.is_number_integer()) return false;
    if (value.is_number_unsigned() &&
        value.get<std::uint64_t>() > static_cast<std::uint64_t>(upper))
        return false;
    const auto number = value.get<std::int64_t>();
    return lower <= number && number <= upper;
}

bool finite(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool parameter_valid(const json& value) {
    return fields(value,
                  {"name",
                   "original_name",
                   "value",
                   "min",
                   "max",
                   "is_quantized",
                   "is_enabled",
                   "state",
                   "automation_state"}) &&
           value.at("name").is_string() && value.at("original_name").is_string() &&
           finite(value.at("value")) && finite(value.at("min")) && finite(value.at("max")) &&
           value.at("min").get<double>() <= value.at("value").get<double>() &&
           value.at("value").get<double>() <= value.at("max").get<double>() &&
           value.at("is_quantized").is_boolean() && value.at("is_enabled").is_boolean() &&
           integer(value.at("state"), 0, 2) && integer(value.at("automation_state"), 0, 2);
}

bool note_valid(const json& value) {
    if (!fields(value,
                {"pitch",
                 "start_time",
                 "duration",
                 "velocity",
                 "mute",
                 "probability",
                 "velocity_deviation",
                 "release_velocity"}) ||
        !integer(value.at("pitch"), 0, 127) || !value.at("mute").is_boolean())
        return false;
    return std::ranges::all_of(std::array{"start_time",
                                          "duration",
                                          "velocity",
                                          "probability",
                                          "velocity_deviation",
                                          "release_velocity"},
                               [&value](auto name) {
                                   // The Python MidiNote adapter normalizes these
                                   // six native fields to float before serialization.
                                   return value.at(name).is_number_float() &&
                                          finite(value.at(name));
                               });
}

bool manifest_valid(const json& manifest, bool& structural, bool& complete_coverage) {
    if (!fields(manifest,
                {"schema_version",
                 "track",
                 "clip",
                 "notes",
                 "mixer",
                 "routing",
                 "content_counts",
                 "devices_empty",
                 "other_session_clips_empty",
                 "entire_clip_population_observed",
                 "mpe_note_expression_state_observed",
                 "follow_actions_state_observed"}) ||
        !integer(manifest.at("schema_version"), 1, 1))
        return false;
    for (const auto* name : {"devices_empty",
                             "other_session_clips_empty",
                             "entire_clip_population_observed",
                             "mpe_note_expression_state_observed",
                             "follow_actions_state_observed"})
        if (!manifest.at(name).is_boolean()) return false;
    const auto& track = manifest.at("track");
    if (!fields(track,
                {"name",
                 "mute",
                 "solo",
                 "arm",
                 "implicit_arm",
                 "is_frozen",
                 "is_grouped",
                 "back_to_arranger",
                 "has_audio_input",
                 "has_midi_input",
                 "has_audio_output",
                 "has_midi_output"}) ||
        !track.at("name").is_string())
        return false;
    for (const auto& [name, value] : track.items())
        if (name != "name" && !value.is_boolean()) return false;
    const auto& clip = manifest.at("clip");
    if (!fields(clip,
                {"name",
                 "signature_numerator",
                 "signature_denominator",
                 "start_marker",
                 "end_marker",
                 "loop_start",
                 "loop_end",
                 "looping",
                 "muted",
                 "has_envelopes",
                 "has_groove",
                 "is_session_clip",
                 "is_arrangement_clip",
                 "is_midi_clip",
                 "is_audio_clip",
                 "is_playing",
                 "is_recording",
                 "is_overdubbing",
                 "is_triggered",
                 "will_record_on_start",
                 "launch_mode",
                 "launch_quantization",
                 "legato",
                 "velocity_amount"}) ||
        !clip.at("name").is_string() || !integer(clip.at("signature_numerator"), 1, 99) ||
        !integer(clip.at("signature_denominator"), 1, 16) ||
        !integer(clip.at("launch_mode"), 0, 3) || !integer(clip.at("launch_quantization"), 0, 14))
        return false;
    const auto denominator = clip.at("signature_denominator").get<int>();
    if ((denominator & (denominator - 1)) != 0) return false;
    for (const auto* name :
         {"start_marker", "end_marker", "loop_start", "loop_end", "velocity_amount"})
        if (!finite(clip.at(name))) return false;
    for (const auto* name : {"looping",
                             "muted",
                             "has_envelopes",
                             "has_groove",
                             "is_session_clip",
                             "is_arrangement_clip",
                             "is_midi_clip",
                             "is_audio_clip",
                             "is_playing",
                             "is_recording",
                             "is_overdubbing",
                             "is_triggered",
                             "will_record_on_start",
                             "legato"})
        if (!clip.at(name).is_boolean()) return false;
    if (!manifest.at("notes").is_array() || !std::ranges::all_of(manifest.at("notes"), note_valid))
        return false;
    const auto& mixer = manifest.at("mixer");
    if (!fields(mixer,
                {"panning_mode",
                 "crossfade_assign",
                 "volume",
                 "panning",
                 "track_activator",
                 "sends"}) ||
        !integer(mixer.at("panning_mode"), 0, 1) || !integer(mixer.at("crossfade_assign"), 0, 2) ||
        !parameter_valid(mixer.at("volume")) || !parameter_valid(mixer.at("panning")) ||
        !parameter_valid(mixer.at("track_activator")) || !mixer.at("sends").is_array() ||
        !std::ranges::all_of(mixer.at("sends"), parameter_valid))
        return false;
    const auto& routing = manifest.at("routing");
    if (!fields(routing,
                {"input_routing_type",
                 "input_routing_channel",
                 "output_routing_type",
                 "output_routing_channel"}))
        return false;
    for (const auto& route : routing)
        if (!fields(route, {"display_name", "identifier"}) ||
            !route.at("display_name").is_string() || !route.at("identifier").is_string())
            return false;
    const auto& counts = manifest.at("content_counts");
    if (!fields(counts, {"arrangement_clips", "take_lanes"})) return false;
    for (const auto& count : counts)
        if (!count.is_null() && !integer(count, 0, std::numeric_limits<std::int32_t>::max()))
            return false;
    structural =
        manifest.at("devices_empty") == true && manifest.at("other_session_clips_empty") == true &&
        manifest.at("entire_clip_population_observed") == true && track.at("is_grouped") == false &&
        track.at("is_frozen") == false && counts.at("arrangement_clips") == 0 &&
        counts.at("take_lanes") == 0 && clip.at("is_session_clip") == true &&
        clip.at("is_arrangement_clip") == false && clip.at("is_midi_clip") == true &&
        clip.at("is_audio_clip") == false && clip.at("looping") == false &&
        clip.at("start_marker").get<double>() == 0.0 && clip.at("end_marker").get<double>() > 0.0 &&
        clip.at("has_envelopes") == false && clip.at("has_groove") == false;
    complete_coverage = structural && manifest.at("mpe_note_expression_state_observed") == true &&
                        manifest.at("follow_actions_state_observed") == true;
    return true;
}

bool notes_match(const json& actual, const json& requested);
bool note_update_boundary(const json& manifest, const json& observation);

bool identity_evidence_valid(const json& identity, const json& digest) {
    if (!managed_detail::note_identity_valid(identity) || !fingerprint(digest)) return false;
    const auto actual = managed_detail::managed_digest(identity);
    return actual && digest == *actual;
}

bool observation_valid(const json& value) {
    if (!value.is_object() || !value.contains("manifest") ||
        !value.contains("content_fingerprint") || !fingerprint(value.at("content_fingerprint")) ||
        !value.contains("track_tag") || !value.at("track_tag").is_string() ||
        !value.contains("clip_tag") || !value.at("clip_tag").is_string() ||
        !value.contains("track_index") ||
        !integer(value.at("track_index"), 0, std::numeric_limits<std::int32_t>::max()) ||
        !value.contains("slot_index") ||
        !integer(value.at("slot_index"), 0, std::numeric_limits<std::int32_t>::max()) ||
        !value.contains("structural_boundary_complete") ||
        !value.at("structural_boundary_complete").is_boolean() ||
        !value.contains("content_boundary_complete") ||
        !value.at("content_boundary_complete").is_boolean() ||
        !value.contains("unavailable_reasons") || !value.at("unavailable_reasons").is_array() ||
        !std::ranges::all_of(value.at("unavailable_reasons"),
                             [](const auto& reason) { return reason.is_string(); }))
        return false;
    bool structural = false, complete_coverage = false;
    if (!manifest_valid(value.at("manifest"), structural, complete_coverage) ||
        (value.at("structural_boundary_complete") == true && !structural) ||
        (value.at("content_boundary_complete") == true &&
         (!complete_coverage || value.at("structural_boundary_complete") != true)))
        return false;
    for (const auto* name :
         {"observed_notes_match_request", "observed_clip_properties_match_request"})
        if (value.contains(name) && !value.at(name).is_boolean()) return false;
    if (value.contains("note_identity") || value.contains("note_identity_fingerprint")) {
        if (!value.contains("note_identity") || !value.contains("note_identity_fingerprint") ||
            !identity_evidence_valid(value.at("note_identity"),
                                     value.at("note_identity_fingerprint")) ||
            value.at("note_identity").at("entire_clip_population_observed") !=
                value.at("manifest").at("entire_clip_population_observed"))
            return false;
        json notes = json::array();
        for (const auto& note : value.at("note_identity").at("notes"))
            notes.push_back(managed_detail::semantic_note(note));
        if (!notes_match(notes, value.at("manifest").at("notes"))) return false;
    }
    if (!managed_device_detail::device_supplement_valid(value) ||
        !managed_detail::group_supplement_valid(value))
        return false;
    if (value.contains("device_identity") && value.at("manifest").at("devices_empty") !=
                                                 value.at("device_identity").at("cohort").empty())
        return false;
    const auto digest = managed_detail::managed_digest(value.at("manifest"));
    return digest && value.at("content_fingerprint") == *digest;
}

bool notes_match(const json& actual, const json& requested) {
    const auto population = [](const json& notes) {
        std::multiset<std::string> result;
        for (auto note : notes) {
            for (auto& [name, value] : note.items())
                if (name != "pitch" && name != "mute") value = value.get<double>();
            result.insert(*managed_detail::canonical_managed_bytes(note));
        }
        return result;
    };
    return population(actual) == population(requested);
}

bool result_matches_intent(const json& observation, const json& intent, std::string_view name) {
    if (name == managed_routing_detail::apply_method)
        return managed_routing_detail::result_matches_request(intent, observation);
    const auto project = intent.at("project_key").get<std::string>();
    const auto binding = intent.at("binding_key").get<std::string>();
    if (observation.at("track_tag") != "Sunny|" + project + "|" + binding + "|track" ||
        observation.at("clip_tag") != "Sunny|" + project + "|" + binding + "|clip")
        return false;
    if (name == "sunny_managed_adopt_clip")
        return managed_detail::adoption_acknowledgement_valid(intent, observation);
    if (name == "sunny_managed_apply_song_settings")
        return managed_song_detail::result_matches_request(intent, observation);
    if (name == managed_mixer_detail::adopt_method || name == managed_mixer_detail::update_method)
        return managed_mixer_detail::result_matches_request(name, intent, observation);
    if (name == "sunny_managed_replace_envelope")
        return observation.contains("envelope_replacement") &&
               observation.at("envelope_replacement").contains("before_observation") &&
               observation_valid(observation.at("envelope_replacement").at("before_observation")) &&
               managed_envelope_detail::result_matches_request(intent, observation);
    if (name == "sunny_managed_update_device_modes")
        return observation.contains("device_mode_update") &&
               observation.at("device_mode_update").contains("before_observation") &&
               observation_valid(observation.at("device_mode_update").at("before_observation")) &&
               managed_device_detail::device_mode_result_matches_request(intent, observation);
    if (name == "sunny_managed_adopt_devices")
        return managed_device_detail::device_adoption_result_matches_request(intent, observation);
    if (name == "sunny_managed_insert_device" || name == "sunny_managed_update_device_parameters") {
        return observation.contains("device_update") &&
               observation.at("device_update").contains("before_observation") &&
               observation_valid(observation.at("device_update").at("before_observation")) &&
               managed_device_detail::device_result_matches_request(name, intent, observation);
    }
    if (name == "sunny_managed_create_clip" || name == "sunny_managed_replace_clip") {
        if (!observation.contains("observed_notes_match_request") ||
            !observation.contains("observed_clip_properties_match_request"))
            return false;
        const auto& manifest = observation.at("manifest");
        const auto& clip = manifest.at("clip");
        const bool clip_matches =
            clip.at("end_marker").get<double>() == intent.at("clip_end").get<double>() &&
            clip.at("signature_numerator") == intent.at("signature_numerator") &&
            clip.at("signature_denominator") == intent.at("signature_denominator");
        return observation.at("observed_notes_match_request").get<bool>() ==
                   notes_match(manifest.at("notes"), intent.at("notes")) &&
               observation.at("observed_clip_properties_match_request").get<bool>() == clip_matches;
    }
    if (name == "sunny_managed_rebind")
        return observation.at("manifest") == intent.at("expected_manifest");
    if (name == "sunny_managed_update_clip_geometry") {
        if (!observation.contains("note_identity") || !observation.contains("clip_geometry_update"))
            return false;
        const auto& update = observation.at("clip_geometry_update");
        if (!fields(update,
                    {"before_manifest",
                     "before_note_identity",
                     "before_note_identity_fingerprint",
                     "before_device_identity_fingerprint",
                     "requested_geometry",
                     "submitted_properties",
                     "returned_properties",
                     "observed_geometry_matches_request",
                     "note_values_preserved",
                     "note_ids_preserved",
                     "note_cardinality_preserved",
                     "other_finite_properties_preserved",
                     "device_identity_preserved",
                     "loop_end_relationship"}) ||
            !identity_evidence_valid(update.at("before_note_identity"),
                                     update.at("before_note_identity_fingerprint")) ||
            (!update.at("before_device_identity_fingerprint").is_null() &&
             !fingerprint(update.at("before_device_identity_fingerprint"))) ||
            update.at("requested_geometry") != intent.at("geometry"))
            return false;
        bool structural = false, complete = false;
        if (!manifest_valid(update.at("before_manifest"), structural, complete) ||
            !note_update_boundary(update.at("before_manifest"), observation) ||
            managed_detail::managed_digest(update.at("before_manifest")) !=
                std::optional<std::string>{
                    intent.at("expected_content_fingerprint").get<std::string>()} ||
            !managed_detail::clip_geometry_note_admission(update.at("before_note_identity"),
                                                          intent.at("geometry")) ||
            observation.at("note_identity").at("entire_clip_population_observed") != true)
            return false;
        json before_notes = json::array();
        for (const auto& note : update.at("before_note_identity").at("notes"))
            before_notes.push_back(managed_detail::semantic_note(note));
        if (!notes_match(before_notes, update.at("before_manifest").at("notes"))) return false;
        const auto properties = managed_detail::changed_geometry_properties(
            update.at("before_manifest").at("clip"), intent.at("geometry"));
        if (properties.empty() || update.at("submitted_properties") != properties ||
            update.at("returned_properties") != properties)
            return false;
        const auto flags = managed_detail::clip_geometry_flags(
            update.at("before_manifest"),
            update.at("before_note_identity"),
            update.at("before_device_identity_fingerprint"),
            observation,
            intent.at("geometry"),
            std::ranges::find(properties, json("end_marker")) != properties.end());
        for (const auto& [name, expected] : flags.items())
            if (update.at(name) != expected) return false;
        return true;
    }
    if (name == "sunny_managed_revise_note_population") {
        if (!observation.contains("note_identity") ||
            !observation.contains("note_population_update"))
            return false;
        const auto& update = observation.at("note_population_update");
        if (!fields(update,
                    {"before_manifest",
                     "before_note_identity",
                     "before_note_identity_fingerprint",
                     "changes_submitted",
                     "deletions_submitted",
                     "additions_submitted",
                     "returned_added_note_ids",
                     "addition_associations",
                     "observed_changes_match_request",
                     "observed_deletions_absent",
                     "observed_additions_match_request",
                     "untouched_notes_preserved",
                     "retained_note_ids_preserved",
                     "observed_population_cardinality_match"}) ||
            !identity_evidence_valid(update.at("before_note_identity"),
                                     update.at("before_note_identity_fingerprint")))
            return false;
        for (const auto* category : {"changes", "deletions", "additions"}) {
            const auto count = std::string(category) + "_submitted";
            if (!integer(update.at(count), 0, 65536) ||
                update.at(count).get<std::size_t>() != intent.at(category).size())
                return false;
        }
        for (const auto* flag : {"observed_changes_match_request",
                                 "observed_deletions_absent",
                                 "observed_additions_match_request",
                                 "untouched_notes_preserved",
                                 "retained_note_ids_preserved",
                                 "observed_population_cardinality_match"})
            if (!update.at(flag).is_boolean()) return false;
        bool structural = false, complete = false;
        if (!manifest_valid(update.at("before_manifest"), structural, complete) ||
            !note_update_boundary(update.at("before_manifest"), observation) ||
            managed_detail::managed_digest(update.at("before_manifest")) !=
                std::optional<std::string>{
                    intent.at("expected_content_fingerprint").get<std::string>()} ||
            update.at("before_manifest").at("entire_clip_population_observed") != true ||
            observation.at("note_identity").at("entire_clip_population_observed") != true)
            return false;
        json before_values = json::array();
        for (const auto& note : update.at("before_note_identity").at("notes"))
            before_values.push_back(managed_detail::semantic_note(note));
        if (!notes_match(before_values, update.at("before_manifest").at("notes"))) return false;
        const auto proposed = managed_detail::proposed_population(
            update.at("before_note_identity"),
            intent,
            update.at("before_manifest").at("clip").at("end_marker").get<double>());
        if (!proposed) return false;
        const auto before = managed_detail::note_map(update.at("before_note_identity"));
        const auto after = managed_detail::note_map(observation.at("note_identity"));
        std::set<std::int32_t> changed, deleted, added;
        for (const auto& change : intent.at("changes"))
            changed.insert(change.at("note_id").get<std::int32_t>());
        for (const auto& deletion : intent.at("deletions"))
            deleted.insert(deletion.at("note_id").get<std::int32_t>());
        const auto& returned = update.at("returned_added_note_ids");
        if (!returned.is_array() || returned.size() != intent.at("additions").size()) return false;
        for (const auto& id : returned) {
            if (!managed_detail::note_integer(id, INT32_MIN, INT32_MAX) ||
                !added.insert(id.get<std::int32_t>()).second ||
                before.contains(id.get<std::int32_t>()))
                return false;
        }
        bool changes_match = true, deletions_absent = true, untouched = true, retained = true;
        for (const auto id : deleted)
            deletions_absent = deletions_absent && !after.contains(id);
        for (const auto& [id, note] : before) {
            if (deleted.contains(id)) continue;
            const auto found = after.find(id);
            retained = retained && found != after.end();
            const auto& expected = changed.contains(id) ? proposed->at(id) : note;
            const bool match =
                found != after.end() && managed_detail::managed_digest(found->second) ==
                                            managed_detail::managed_digest(expected);
            if (changed.contains(id))
                changes_match = changes_match && match;
            else
                untouched = untouched && match;
        }
        json associations = json::array();
        auto remaining = added;
        bool additions_match = true;
        // Index each actual returned-ID value once. Exact typed canonical bytes
        // close digest collisions without assuming native insertion order.
        std::map<std::string, std::map<std::string, std::set<std::int32_t>>> by_value;
        for (const auto id : remaining) {
            const auto found = after.find(id);
            if (found == after.end()) continue;
            const auto note = managed_detail::semantic_note(found->second);
            by_value[*managed_detail::managed_digest(note)]
                    [*managed_detail::canonical_managed_bytes(note)]
                        .insert(id);
        }
        for (const auto& addition : intent.at("additions")) {
            const auto note = managed_detail::semantic_note(addition.at("note"));
            auto digest = by_value.find(*managed_detail::managed_digest(note));
            if (digest == by_value.end()) {
                additions_match = false;
                continue;
            }
            auto value = digest->second.find(*managed_detail::canonical_managed_bytes(note));
            if (value == digest->second.end() || value->second.size() != 1) {
                additions_match = false;
                continue;
            }
            const auto id = *value->second.begin();
            value->second.clear();
            remaining.erase(id);
            associations.push_back(json{{"note_key", addition.at("note_key")}, {"note_id", id}});
        }
        additions_match = additions_match && remaining.empty();
        return update.at("addition_associations") == associations &&
               update.at("observed_changes_match_request").get<bool>() == changes_match &&
               update.at("observed_deletions_absent").get<bool>() == deletions_absent &&
               update.at("observed_additions_match_request").get<bool>() == additions_match &&
               update.at("untouched_notes_preserved").get<bool>() == untouched &&
               update.at("retained_note_ids_preserved").get<bool>() == retained &&
               update.at("observed_population_cardinality_match").get<bool>() ==
                   (after.size() == proposed->size() + added.size());
    }
    if (name == "sunny_managed_update_notes") {
        if (!observation.contains("note_identity") || !observation.contains("note_update"))
            return false;
        const auto& update = observation.at("note_update");
        if (!fields(update,
                    {"before_manifest",
                     "before_note_identity",
                     "before_note_identity_fingerprint",
                     "notes_submitted",
                     "observed_updates_match_request",
                     "untouched_notes_preserved",
                     "note_ids_preserved"}) ||
            !identity_evidence_valid(update.at("before_note_identity"),
                                     update.at("before_note_identity_fingerprint")) ||
            !integer(update.at("notes_submitted"), 1, 65536) ||
            update.at("notes_submitted").get<std::size_t>() != intent.at("changes").size())
            return false;
        for (const auto* flag :
             {"observed_updates_match_request", "untouched_notes_preserved", "note_ids_preserved"})
            if (!update.at(flag).is_boolean()) return false;
        bool before_structural = false, before_complete = false;
        if (!manifest_valid(update.at("before_manifest"), before_structural, before_complete) ||
            !note_update_boundary(update.at("before_manifest"), observation) ||
            managed_detail::managed_digest(update.at("before_manifest")) !=
                std::optional<std::string>{
                    intent.at("expected_content_fingerprint").get<std::string>()} ||
            update.at("before_manifest").at("entire_clip_population_observed") != true)
            return false;
        json before_notes = json::array();
        for (const auto& note : update.at("before_note_identity").at("notes"))
            before_notes.push_back(managed_detail::semantic_note(note));
        if (!notes_match(before_notes, update.at("before_manifest").at("notes"))) return false;
        const auto proposed = managed_detail::proposed_notes(
            update.at("before_note_identity"),
            intent.at("changes"),
            update.at("before_manifest").at("clip").at("end_marker").get<double>());
        if (!proposed ||
            observation.at("note_identity").at("entire_clip_population_observed") != true)
            return false;
        const auto before = managed_detail::note_map(update.at("before_note_identity"));
        const auto after = managed_detail::note_map(observation.at("note_identity"));
        std::set<std::int32_t> touched;
        for (const auto& change : intent.at("changes"))
            touched.insert(change.at("note_id").get<std::int32_t>());
        bool matched = true, untouched = true, ids = before.size() == after.size();
        for (const auto& [id, note] : before) {
            const auto found = after.find(id);
            if (found == after.end()) {
                ids = false;
            }
            if (touched.contains(id))
                matched = matched && found != after.end() &&
                          managed_detail::managed_digest(found->second) ==
                              managed_detail::managed_digest(proposed->at(id));
            else
                untouched = untouched && found != after.end() &&
                            managed_detail::managed_digest(found->second) ==
                                managed_detail::managed_digest(note);
        }
        return update.at("observed_updates_match_request").get<bool>() == matched &&
               update.at("untouched_notes_preserved").get<bool>() == untouched &&
               update.at("note_ids_preserved").get<bool>() == ids;
    }
    if (!observation.contains("acknowledgement")) return false;
    const auto& acknowledgement = observation.at("acknowledgement");
    if (!fields(acknowledgement, {"action", "steps_inserted", "parameter"}) ||
        acknowledgement.at("action") != "created" ||
        !integer(
            acknowledgement.at("steps_inserted"), 1, std::numeric_limits<std::int32_t>::max()) ||
        acknowledgement.at("steps_inserted").get<std::size_t>() !=
            intent.at("lane").at("points").size())
        return false;
    const auto& parameter = acknowledgement.at("parameter");
    if (!fields(parameter,
                {"matched_name",
                 "original_name",
                 "minimum",
                 "maximum",
                 "unit",
                 "state",
                 "automation_state"}) ||
        !parameter.at("matched_name").is_string() || !parameter.at("original_name").is_string() ||
        !finite(parameter.at("minimum")) || !finite(parameter.at("maximum")) ||
        parameter.at("minimum").get<double>() >= parameter.at("maximum").get<double>() ||
        parameter.at("unit") != "internal" || !integer(parameter.at("state"), 0, 0) ||
        !integer(parameter.at("automation_state"), 0, 1))
        return false;
    return std::ranges::all_of(intent.at("lane").at("points"), [&parameter](const auto& point) {
        const auto value = point.at("value").template get<double>();
        return parameter.at("minimum").get<double>() <= value &&
               value <= parameter.at("maximum").get<double>();
    }); // Write acknowledgement only; sampled readback remains independent.
}

Result<ManagedOperationReceipt> observe_journal(ManagedOperationReceipt result,
                                                const LomResponse& response) {
    auto malformed = [&result]() -> Result<ManagedOperationReceipt> {
        result.outcome = ManagedOperationOutcome::Indeterminate;
        result.journal.reset();
        result.error =
            "Malformed or foreign retained managed journal; mutation outcome is uncertain";
        return result;
    };
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value)) {
        result.journal.reset();
        result.error =
            response.error.value_or("Managed operation has no valid retained journal response");
        // A complete frame may have executed before an error/malformed reply.
        result.outcome = result.delivery == LomDeliveryState::NotSent
                             ? ManagedOperationOutcome::NotSent
                             : ManagedOperationOutcome::Indeterminate;
        return result;
    }
    const auto& journal = std::get<json>(*response.value);
    if (!journal.is_object() || !journal.contains("outcome") ||
        !journal.at("outcome").is_string()) {
        return malformed();
    }
    const auto outcome = journal.at("outcome").get<std::string>();
    if (outcome == "unknown_epoch") {
        if (!journal.contains("document_token") || !key(journal.at("document_token")) ||
            journal.at("document_token") == result.context.document_token)
            return malformed();
        result.outcome = ManagedOperationOutcome::UnknownEpoch;
        result.journal = journal;
        return result;
    }
    const auto* intent = payload(result.request);
    if (outcome == "unknown_operation") {
        if (!journal.contains("document_token") ||
            journal.at("document_token") != result.context.document_token || !intent ||
            !journal.contains("operation_id") ||
            journal.at("operation_id") != intent->at("operation_id"))
            return malformed();
        result.outcome = ManagedOperationOutcome::UnknownOperation;
        result.journal = journal;
        return result;
    }
    // Only this exact token, operation and original payload can reconcile this
    // receipt. JSON dumps preserve the bool/int distinction in persisted data.
    if (!intent || !journal.contains("document_token") ||
        journal.at("document_token") != result.context.document_token ||
        !journal.contains("operation_id") ||
        journal.at("operation_id") != intent->at("operation_id") || !journal.contains("name") ||
        journal.at("name") != result.request.property_or_method || !journal.contains("request") ||
        journal.at("request").dump() != intent->dump() ||
        !journal.contains("request_fingerprint") ||
        !fingerprint(journal.at("request_fingerprint")) ||
        !journal.contains("native_mutation_started") ||
        !journal.at("native_mutation_started").is_boolean())
        return malformed();
    const auto request_digest = managed_detail::managed_digest(
        json{{"name", result.request.property_or_method}, {"request", *intent}});
    if (!request_digest || journal.at("request_fingerprint") != *request_digest) return malformed();
    const bool song_settings =
        result.request.property_or_method == "sunny_managed_apply_song_settings";
    const bool routing = result.request.property_or_method == managed_routing_detail::apply_method;
    if (routing && !managed_routing_detail::partial_valid(*intent, journal)) return malformed();
    if (song_settings && !managed_song_detail::partial_valid(*intent, journal)) return malformed();
    const bool static_mixer =
        result.request.property_or_method == managed_mixer_detail::adopt_method ||
        result.request.property_or_method == managed_mixer_detail::update_method;
    if (result.request.property_or_method == managed_mixer_detail::update_method &&
        outcome != "acknowledged" &&
        (journal.contains("mixer_progress") || journal.at("native_mutation_started") == true) &&
        !managed_mixer_detail::partial_valid(*intent, journal))
        return malformed();
    if (outcome == "acknowledged") {
        const bool native_mutation =
            result.request.property_or_method != "sunny_managed_rebind" &&
            result.request.property_or_method != "sunny_managed_adopt_clip" &&
            result.request.property_or_method != "sunny_managed_adopt_devices";
        const bool native_start_valid =
            routing && journal.contains("result")
                ? managed_routing_detail::acknowledged_native_start_valid(
                      *intent,
                      journal.at("result"),
                      journal.at("native_mutation_started").get<bool>())
            : static_mixer && journal.contains("result")
                ? managed_mixer_detail::acknowledged_native_start_valid(
                      result.request.property_or_method,
                      *intent,
                      journal.at("result"),
                      journal.at("native_mutation_started").get<bool>())
            : song_settings && journal.contains("result")
                ? managed_song_detail::acknowledged_native_start_valid(
                      *intent,
                      journal.at("result"),
                      journal.at("native_mutation_started").get<bool>())
                : journal.at("native_mutation_started").get<bool>() == native_mutation;
        const bool group_only = routing && intent->at("intent").at("kind") == "adopt_group";
        if (!native_start_valid || !journal.contains("result") ||
            (!group_only && (!observation_valid(journal.at("result")) ||
                             !managed_detail::group_context_matches(
                                 journal.at("result"),
                                 result.context,
                                 intent->at("project_key").get<std::string>(),
                                 intent->at("binding_key").get<std::string>()))) ||
            !result_matches_intent(
                journal.at("result"), *intent, result.request.property_or_method))
            return malformed();
        if (result.request.property_or_method == managed_mixer_detail::update_method &&
            (!journal.contains("mixer_progress") ||
             !fields(journal.at("mixer_progress"), {"started_fields", "returned_fields"}) ||
             journal.at("mixer_progress").at("started_fields") !=
                 journal.at("result").at("mixer_update").at("started_fields") ||
             journal.at("mixer_progress").at("returned_fields") !=
                 journal.at("result").at("mixer_update").at("returned_fields")))
            return malformed();
        if (song_settings && (!journal.contains("song_settings_progress") ||
                              journal.at("song_settings_progress").at("started_fields") !=
                                  journal.at("result").at("song_settings").at("started_fields") ||
                              journal.at("song_settings_progress").at("returned_fields") !=
                                  journal.at("result").at("song_settings").at("returned_fields")))
            return malformed();
        if (result.request.property_or_method == "sunny_managed_replace_envelope" &&
            (!journal.contains("progress") || !managed_envelope_detail::progress_valid(
                                                  journal.at("result"), journal.at("progress"))))
            return malformed();
        if (result.request.property_or_method == "sunny_managed_revise_note_population") {
            json calls = json::array();
            if (!intent->at("deletions").empty()) calls.push_back("remove_notes_by_id");
            if (!intent->at("changes").empty()) calls.push_back("apply_note_modifications");
            if (!intent->at("additions").empty()) calls.push_back("add_new_notes");
            if (!journal.contains("progress") ||
                !fields(journal.at("progress"),
                        {"started_calls", "returned_calls", "returned_added_note_ids"}) ||
                journal.at("progress").at("started_calls") != calls ||
                journal.at("progress").at("returned_calls") != calls ||
                journal.at("progress").at("returned_added_note_ids") !=
                    journal.at("result").at("note_population_update").at("returned_added_note_ids"))
                return malformed();
        }
        if (result.request.property_or_method == "sunny_managed_update_clip_geometry") {
            const auto& properties =
                journal.at("result").at("clip_geometry_update").at("submitted_properties");
            if (!journal.contains("progress") ||
                !fields(journal.at("progress"), {"started_properties", "returned_properties"}) ||
                journal.at("progress").at("started_properties") != properties ||
                journal.at("progress").at("returned_properties") != properties)
                return malformed();
        }
        result.outcome = ManagedOperationOutcome::Acknowledged;
    } else if (outcome == "declined") {
        if (journal.at("native_mutation_started").get<bool>()) return malformed();
        result.outcome = ManagedOperationOutcome::Declined;
    } else if (outcome == "indeterminate" || outcome == "pending") {
        result.outcome = ManagedOperationOutcome::Indeterminate;
    } else
        return malformed();
    result.journal = journal;
    if (journal.contains("error")) {
        if (!journal.at("error").is_string()) return malformed();
        result.error = journal.at("error").get<std::string>();
    }
    return result;
}
} // namespace

Result<ManagedClipProjection> managed_clip_projection(const CommandBuffer& recording,
                                                      int planned_track_index) {
    if (planned_track_index < 0) return std::unexpected(ErrorCode::ProtocolError);
    const auto slot = LomPaths::clip_slot(planned_track_index, 0).to_string();
    const auto clip = LomPaths::clip(planned_track_index, 0).to_string();
    ManagedClipProjection result;
    bool created = false, start = false, end = false, numerator = false, denominator = false;
    std::set<std::string> seen;
    for (const auto& entry : recording.entries()) {
        auto request = entry.request;
        // CommandBuffer::send_notes records the typed batch separately from
        // the request arguments. Normalize through the same closed protocol
        // used by the live transport before validating/projecting it.
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "add_new_notes" && request.args.empty()) {
            if (!LomProtocol::validate_notes(request.path, entry.notes))
                return std::unexpected(ErrorCode::ProtocolError);
            request = LomProtocol::add_new_notes(request.path, entry.notes);
        } else if (!entry.notes.empty())
            return std::unexpected(ErrorCode::ProtocolError);
        if (!LomProtocol::validate_request(request))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto path = request.path.to_string();
        if (path != clip && path != slot) {
            result.outside_clip_requests.push_back(request);
            continue;
        }
        const auto wire = json::parse(LomProtocol::serialize_request(request));
        const auto args = wire.value("args", json::array());
        const auto& name = request.property_or_method;
        if (path == slot) {
            if (created || request.type != LomRequestType::CallMethod || name != "create_clip")
                return std::unexpected(ErrorCode::ProtocolError);
            created = true;
            result.clip_end = args[0].get<double>();
        } else if (request.type == LomRequestType::CallMethod && name == "add_new_notes") {
            for (const auto& note : args[0].at("notes"))
                result.notes.push_back(note);
        } else if (request.type == LomRequestType::CallMethod &&
                   name == "sunny_clear_all_envelopes") {
            // The new managed Clip is observed independently. No blanket clear
            // is replayed on an existing user/managed Clip by this projection.
            result.outside_clip_requests.push_back(request);
        } else if (request.type == LomRequestType::SetProperty) {
            if (!seen.insert(name).second) return std::unexpected(ErrorCode::ProtocolError);
            const auto& value = args[0];
            if (name == "start_marker") {
                start = value == 0.0;
                if (!start) return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "end_marker") {
                end = true;
                if (value.get<double>() != result.clip_end)
                    return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "signature_numerator") {
                numerator = true;
                result.signature_numerator = value.get<int>();
            } else if (name == "signature_denominator") {
                denominator = true;
                result.signature_denominator = value.get<int>();
            } else if (name == "name") {
                result.outside_clip_requests.push_back(request);
            } else if (name == "looping" || name == "muted" || name == "legato") {
                if (value != false) return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "launch_mode") {
                if (value != 0) return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "launch_quantization") {
                if (value != 1) return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "velocity_amount") {
                if (value != 0.0) return std::unexpected(ErrorCode::ProtocolError);
            } else if (name == "groove") {
                if (!value.is_null()) return std::unexpected(ErrorCode::ProtocolError);
            } else
                return std::unexpected(ErrorCode::ProtocolError);
        } else
            return std::unexpected(ErrorCode::ProtocolError);
    }
    if (!created || !start || !end || !numerator || !denominator)
        return std::unexpected(ErrorCode::ProtocolError);
    const ManagedBridgeContext test_context{"projection", "projection"};
    const auto shape =
        make_managed_clip_request(test_context, "projection", "projection", "projection", result);
    if (!shape && shape.error() != ErrorCode::ManagedReplyCapacityExceeded)
        return std::unexpected(ErrorCode::ProtocolError);
    return result;
}

Result<ManagedBridgeContext> managed_bridge_context(LomTransport& transport) {
    if (transport.records_without_execution()) return std::unexpected(ErrorCode::ProtocolError);
    const auto response =
        transport.send(LomProtocol::call_method(LomPaths::song(), "sunny_managed_context"));
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ConnectionLost);
    auto value = std::get<json>(*response.value);
    if (!value.is_object() || value.size() != 3 || !value.contains("schema_version") ||
        !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1)
        return std::unexpected(ErrorCode::ProtocolError);
    value.erase("schema_version");
    return parse_context(value);
}

Result<LomRequest>
make_managed_clip_request(const ManagedBridgeContext& context,
                          const std::string& operation_id,
                          const std::string& project_key,
                          const std::string& binding_key,
                          const ManagedClipProjection& projection,
                          std::optional<std::string> expected_content_fingerprint) {
    if (!context_valid(context)) return std::unexpected(ErrorCode::ProtocolError);
    json payload{{"document_token", context.document_token},
                 {"operation_id", operation_id},
                 {"project_key", project_key},
                 {"binding_key", binding_key},
                 {"clip_end", projection.clip_end},
                 {"signature_numerator", projection.signature_numerator},
                 {"signature_denominator", projection.signature_denominator},
                 {"notes", projection.notes}};
    if (expected_content_fingerprint)
        payload["expected_content_fingerprint"] = *expected_content_fingerprint;
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        expected_content_fingerprint ? "sunny_managed_replace_clip" : "sunny_managed_create_clip",
        {payload});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::creation_response_fits(payload,
                                                expected_content_fingerprint
                                                    ? "sunny_managed_replace_clip"
                                                    : "sunny_managed_create_clip"))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return request;
}

Result<ManagedOperationReceipt> prepare_managed_operation(const ManagedBridgeContext& context,
                                                          const LomRequest& request) {
    if (!context_valid(context) || !mutation(request) || !LomProtocol::validate_request(request))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto* value = payload(request);
    if (!value || value->at("document_token") != context.document_token)
        return std::unexpected(ErrorCode::ProtocolError);
    if (request.property_or_method == managed_routing_detail::apply_method &&
        value->at("approved_preview").at("context") != context_json(context))
        return std::unexpected(ErrorCode::ProtocolError);
    return ManagedOperationReceipt{context,
                                   request,
                                   LomDeliveryState::NotSent,
                                   ManagedOperationOutcome::Prepared,
                                   std::nullopt,
                                   std::nullopt};
}

Result<ManagedOperationReceipt> execute_managed_operation(const ManagedOperationReceipt& prepared,
                                                          LomTransport& transport) {
    if (!prepare_managed_operation(prepared.context, prepared.request) ||
        !prepared.explicit_retry_safe() || transport.records_without_execution())
        return std::unexpected(ErrorCode::ProtocolError);
    auto result = prepared;
    try {
        const auto response =
            transport.send(prepared.request); // ONE send, no retry or compensation.
        result.delivery = response.delivery;
        return observe_journal(result, response);
    } catch (const std::exception& error) {
        // A transport exception does not prove the complete frame stayed local.
        result.delivery = LomDeliveryState::SentWithoutValidResponse;
        result.outcome = ManagedOperationOutcome::Indeterminate;
        result.error = error.what();
        return result;
    } catch (...) {
        result.delivery = LomDeliveryState::SentWithoutValidResponse;
        result.outcome = ManagedOperationOutcome::Indeterminate;
        result.error = "Transport threw after managed send began; outcome is uncertain";
        return result;
    }
}

Result<ManagedOperationReceipt>
reconcile_managed_operation(const ManagedOperationReceipt& receipt,
                            LomTransport& transport,
                            std::optional<ManagedOperationReceipt>* query_evidence) {
    if (query_evidence) query_evidence->reset();
    if (!prepare_managed_operation(receipt.context, receipt.request) ||
        transport.records_without_execution())
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& value = *payload(receipt.request);
    try {
        const auto response = transport.send(
            LomProtocol::call_method(LomPaths::song(),
                                     "sunny_managed_operation",
                                     {json{{"document_token", receipt.context.document_token},
                                           {"operation_id", value.at("operation_id")}}}));
        if (!response.success) {
            // Read-only query failure must not change original mutation delivery to
            // NotSent, which would incorrectly authorize a retry of that mutation.
            auto result = receipt;
            result.error = response.error.value_or("Managed reconciliation query failed");
            return result;
        }
        auto observed = observe_journal(receipt, response);
        // A retained terminal receipt can differ from the successful current
        // query. A failed query supplies no newly observed native evidence.
        if (observed && query_evidence) *query_evidence = *observed;
        if (observed &&
            (receipt.outcome == ManagedOperationOutcome::Acknowledged ||
             receipt.outcome == ManagedOperationOutcome::Declined) &&
            (observed->outcome != receipt.outcome || !observed->journal ||
             observed->journal != receipt.journal)) {
            // A later read failure or changed epoch cannot erase a valid historical
            // acknowledgement/decline. It also cannot authorize mutation replay.
            auto retained = receipt;
            retained.error = observed->error.value_or(
                "Managed reconciliation no longer returned the retained terminal journal");
            return retained;
        }
        return observed;
    } catch (const std::exception& error) {
        auto result = receipt;
        result.error = error.what();
        return result;
    } catch (...) {
        auto result = receipt;
        result.error = "Read-only managed reconciliation threw; original delivery is retained";
        return result;
    }
}

json managed_receipt_to_json(const ManagedOperationReceipt& receipt) {
    return {{"schema_version", SUNNY_MANAGED_RECEIPT_SCHEMA_VERSION},
            {"context", context_json(receipt.context)},
            {"request", json::parse(LomProtocol::serialize_request(receipt.request))},
            {"delivery", delivery_names.at(static_cast<std::size_t>(receipt.delivery))},
            {"outcome", outcome_names.at(static_cast<std::size_t>(receipt.outcome))},
            {"journal", receipt.journal ? *receipt.journal : json(nullptr)},
            {"error", receipt.error ? json(*receipt.error) : json(nullptr)}};
}

Result<ManagedOperationReceipt> managed_receipt_from_json(const json& value) {
    if (!value.is_object() || value.size() != 7 || !value.contains("schema_version") ||
        !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1 ||
        !value.contains("context") || !value.contains("request") || !value.contains("delivery") ||
        !value.at("delivery").is_string() || !value.contains("outcome") ||
        !value.at("outcome").is_string() || !value.contains("journal") ||
        !value.contains("error") ||
        !(value.at("journal").is_null() || value.at("journal").is_object()) ||
        !(value.at("error").is_null() || value.at("error").is_string()))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto context = parse_context(value.at("context"));
    const auto request = LomProtocol::deserialize_request(value.at("request"));
    if (!context || !request) return std::unexpected(ErrorCode::ProtocolError);
    auto prepared = prepare_managed_operation(*context, *request);
    if (!prepared) return prepared;
    const auto delivery =
        std::ranges::find(delivery_names, value.at("delivery").get<std::string>());
    const auto outcome = std::ranges::find(outcome_names, value.at("outcome").get<std::string>());
    if (delivery == delivery_names.end() || outcome == outcome_names.end())
        return std::unexpected(ErrorCode::ProtocolError);
    prepared->delivery = static_cast<LomDeliveryState>(delivery - delivery_names.begin());
    prepared->outcome = static_cast<ManagedOperationOutcome>(outcome - outcome_names.begin());
    if (!value.at("journal").is_null()) prepared->journal = value.at("journal");
    if (!value.at("error").is_null()) prepared->error = value.at("error").get<std::string>();
    // An unsafe receipt cannot become safe just by changing an outcome label.
    if ((prepared->outcome == ManagedOperationOutcome::Prepared ||
         prepared->outcome == ManagedOperationOutcome::NotSent) &&
        (prepared->delivery != LomDeliveryState::NotSent || prepared->journal))
        return std::unexpected(ErrorCode::ProtocolError);
    if (prepared->journal) {
        auto checked = observe_journal(
            *prepared, LomResponse{true, LomValue{*prepared->journal}, std::nullopt});
        if (!checked || !checked->journal ||
            checked->journal->dump() != prepared->journal->dump() ||
            checked->outcome != prepared->outcome)
            return std::unexpected(ErrorCode::ProtocolError);
    } else if (prepared->outcome != ManagedOperationOutcome::Prepared &&
               prepared->outcome != ManagedOperationOutcome::NotSent &&
               prepared->outcome != ManagedOperationOutcome::Indeterminate)
        return std::unexpected(ErrorCode::ProtocolError);
    return prepared;
}

Result<ManagedBindingReceipt>
managed_binding_receipt(const ManagedOperationReceipt& acknowledgement) {
    if (acknowledgement.outcome != ManagedOperationOutcome::Acknowledged ||
        !acknowledgement.journal)
        return std::unexpected(ErrorCode::ProtocolError);
    auto checked = observe_journal(
        acknowledgement, LomResponse{true, LomValue{*acknowledgement.journal}, std::nullopt});
    if (!checked || checked->outcome != ManagedOperationOutcome::Acknowledged)
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& request = *payload(acknowledgement.request);
    if (acknowledgement.request.property_or_method == managed_routing_detail::apply_method &&
        request.at("intent").at("kind") == "adopt_group")
        return std::unexpected(ErrorCode::ProtocolError);
    ManagedBindingReceipt binding{acknowledgement.context,
                                  request.at("project_key").get<std::string>(),
                                  request.at("binding_key").get<std::string>(),
                                  acknowledgement.journal->at("result")};
    return managed_binding_from_json(managed_binding_to_json(binding));
}

json managed_binding_to_json(const ManagedBindingReceipt& binding) {
    return {{"schema_version", SUNNY_MANAGED_RECEIPT_SCHEMA_VERSION},
            {"context", context_json(binding.context)},
            {"project_key", binding.project_key},
            {"binding_key", binding.binding_key},
            {"observation", binding.observation}};
}

Result<ManagedBindingReceipt> managed_binding_from_json(const json& value) {
    if (!value.is_object() || value.size() != 5 || !value.contains("schema_version") ||
        !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1 ||
        !value.contains("context") || !value.contains("project_key") ||
        !key(value.at("project_key")) || !value.contains("binding_key") ||
        !key(value.at("binding_key")) || !value.contains("observation") ||
        !observation_valid(value.at("observation")))
        return std::unexpected(ErrorCode::ProtocolError);
    auto context = parse_context(value.at("context"));
    if (!context) return std::unexpected(ErrorCode::ProtocolError);
    const auto project = value.at("project_key").get<std::string>(),
               binding = value.at("binding_key").get<std::string>();
    if (!managed_detail::group_context_matches(
            value.at("observation"), *context, project, binding) ||
        value.at("observation").at("track_tag") != "Sunny|" + project + "|" + binding + "|track" ||
        value.at("observation").at("clip_tag") != "Sunny|" + project + "|" + binding + "|clip")
        return std::unexpected(ErrorCode::ProtocolError);
    return ManagedBindingReceipt{*context, project, binding, value.at("observation")};
}

Result<LomRequest> make_managed_rebind_request(const ManagedBridgeContext& new_context,
                                               const std::string& operation_id,
                                               const ManagedBindingReceipt& persisted) {
    if (!context_valid(new_context) ||
        !managed_binding_from_json(managed_binding_to_json(persisted)) ||
        !persisted.observation.at("content_boundary_complete").get<bool>())
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& manifest = persisted.observation.at("manifest");
    if (!manifest.contains("entire_clip_population_observed") ||
        manifest.at("entire_clip_population_observed") != true || !manifest.contains("clip") ||
        !manifest.contains("mpe_note_expression_state_observed") ||
        manifest.at("mpe_note_expression_state_observed") != true ||
        !manifest.contains("follow_actions_state_observed") ||
        manifest.at("follow_actions_state_observed") != true || !manifest.at("clip").is_object() ||
        !manifest.at("clip").contains("has_envelopes") ||
        manifest.at("clip").at("has_envelopes") != false)
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(LomPaths::song(),
                                            "sunny_managed_rebind",
                                            {json{{"document_token", new_context.document_token},
                                                  {"operation_id", operation_id},
                                                  {"project_key", persisted.project_key},
                                                  {"binding_key", persisted.binding_key},
                                                  {"expected_manifest", manifest}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    return request;
}

Result<LomRequest> make_managed_envelope_request(const ManagedBridgeContext& context,
                                                 const std::string& operation_id,
                                                 const ManagedBindingReceipt& binding,
                                                 const json& lane) {
    if (!context_valid(context) || !managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.document_token != binding.context.document_token ||
        context.bridge_instance != binding.context.bridge_instance ||
        !binding.observation.contains("note_identity") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key) ||
        !note_update_boundary(binding.observation.at("manifest"), binding.observation))
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_author_envelope",
        {json{{"document_token", context.document_token},
              {"operation_id", operation_id},
              {"project_key", binding.project_key},
              {"binding_key", binding.binding_key},
              {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
              {"lane", lane}}});
    if (!LomProtocol::validate_request(request) ||
        lane.at("clip_end").get<double>() !=
            binding.observation.at("manifest").at("clip").at("end_marker").get<double>())
        return std::unexpected(ErrorCode::ProtocolError);
    try {
        if (!managed_detail::envelope_author_response_fits(std::get<json>(request.args[0]),
                                                           binding.observation))
            return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
    return request;
}

namespace {
Result<ManagedBindingObservation> parse_observation(const ManagedBridgeContext& expected,
                                                    const std::string& project,
                                                    const std::string& binding,
                                                    const json& raw) {
    if (!raw.is_object() || !raw.contains("schema_version") ||
        !integer(raw.at("schema_version"), 1, 1) || !raw.contains("context") ||
        !raw.contains("project_key") || raw.at("project_key") != project ||
        !raw.contains("binding_key") || raw.at("binding_key") != binding ||
        !raw.contains("outcome") || !raw.at("outcome").is_string() ||
        !raw.contains("ownership_retained") || !raw.at("ownership_retained").is_boolean())
        return std::unexpected(ErrorCode::ProtocolError);
    const auto actual = parse_context(raw.at("context"));
    if (!actual) return std::unexpected(ErrorCode::ProtocolError);
    const bool same = actual->bridge_instance == expected.bridge_instance &&
                      actual->document_token == expected.document_token;
    ManagedObservationOutcome outcome;
    const auto& name = raw.at("outcome");
    if (name == "unknown_epoch") {
        if (same ||
            !fields(raw,
                    {"schema_version",
                     "context",
                     "project_key",
                     "binding_key",
                     "outcome",
                     "ownership_retained"}) ||
            raw.at("ownership_retained") != false)
            return std::unexpected(ErrorCode::ProtocolError);
        outcome = ManagedObservationOutcome::UnknownEpoch;
    } else if (!same)
        return std::unexpected(ErrorCode::ProtocolError);
    else if (name == "recovery_unavailable") {
        if (!fields(raw,
                    {"schema_version",
                     "context",
                     "project_key",
                     "binding_key",
                     "outcome",
                     "ownership_retained"}) ||
            raw.at("ownership_retained") != false)
            return std::unexpected(ErrorCode::ProtocolError);
        outcome = ManagedObservationOutcome::RecoveryUnavailable;
    } else if (name == "partial_binding") {
        if (!fields(raw,
                    {"schema_version",
                     "context",
                     "project_key",
                     "binding_key",
                     "outcome",
                     "ownership_retained",
                     "native_handles_retained",
                     "known_track_index",
                     "recovery_available"}) ||
            raw.at("ownership_retained") != false || raw.at("native_handles_retained") != true ||
            raw.at("recovery_available") != false ||
            (!raw.at("known_track_index").is_null() &&
             !integer(raw.at("known_track_index"), 0, INT32_MAX)))
            return std::unexpected(ErrorCode::ProtocolError);
        outcome = ManagedObservationOutcome::PartialBinding;
    } else if (name == "observed") {
        if (!fields(raw,
                    {"schema_version",
                     "context",
                     "project_key",
                     "binding_key",
                     "outcome",
                     "ownership_retained",
                     "observation"}) ||
            raw.at("ownership_retained") != true || !observation_valid(raw.at("observation")) ||
            !managed_detail::group_context_matches(
                raw.at("observation"), *actual, project, binding) ||
            !raw.at("observation").contains("note_identity") ||
            raw.at("observation").at("track_tag") !=
                "Sunny|" + project + "|" + binding + "|track" ||
            raw.at("observation").at("clip_tag") != "Sunny|" + project + "|" + binding + "|clip")
            return std::unexpected(ErrorCode::ProtocolError);
        outcome = ManagedObservationOutcome::Observed;
    } else
        return std::unexpected(ErrorCode::ProtocolError);
    return ManagedBindingObservation{*actual, project, binding, outcome, raw};
}

bool note_update_boundary(const json& manifest, const json& observation) {
    const auto& track = manifest.at("track");
    const auto& clip = manifest.at("clip");
    if (manifest.at("entire_clip_population_observed") != true || track.at("arm") != false ||
        track.at("implicit_arm") != false || track.at("is_frozen") != false ||
        track.at("is_grouped") != observation.at("manifest").at("track").at("is_grouped") ||
        (track.at("is_grouped") != false && !managed_detail::group_current_proof(observation)) ||
        clip.at("is_session_clip") != true || clip.at("is_arrangement_clip") != false ||
        clip.at("is_midi_clip") != true || clip.at("is_audio_clip") != false ||
        clip.at("looping") != false || clip.at("start_marker").get<double>() != 0.0 ||
        clip.at("end_marker").get<double>() <= 0.0)
        return false;
    for (const auto* state :
         {"is_playing", "is_recording", "is_overdubbing", "is_triggered", "will_record_on_start"})
        if (clip.at(state) != false) return false;
    return true;
}
} // namespace

Result<ManagedBindingObservation> observe_managed_binding(const ManagedBridgeContext& expected,
                                                          const std::string& project,
                                                          const std::string& binding,
                                                          LomTransport& transport) {
    if (!context_valid(expected) || !key(project) || !key(binding))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto request = LomProtocol::call_method(LomPaths::song(),
                                                  "sunny_managed_observe",
                                                  {json{{"document_token", expected.document_token},
                                                        {"project_key", project},
                                                        {"binding_key", binding}}});
    const auto response = transport.send(request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ProtocolError);
    return parse_observation(expected, project, binding, std::get<json>(*response.value));
}

Result<ManagedBindingReceipt>
managed_observed_binding(const ManagedBindingObservation& observation) {
    const auto checked = parse_observation(observation.context,
                                           observation.project_key,
                                           observation.binding_key,
                                           observation.evidence);
    if (!checked || checked->outcome != ManagedObservationOutcome::Observed ||
        observation.outcome != ManagedObservationOutcome::Observed)
        return std::unexpected(ErrorCode::ProtocolError);
    return managed_binding_from_json(
        managed_binding_to_json({observation.context,
                                 observation.project_key,
                                 observation.binding_key,
                                 observation.evidence.at("observation")}));
}

Result<ManagedEnvelopeObservation> sample_managed_envelope(const ManagedBridgeContext& expected,
                                                           const std::string& project,
                                                           const std::string& binding,
                                                           const json& parameter,
                                                           const std::vector<double>& times,
                                                           LomTransport& transport) {
    if (!context_valid(expected) || !key(project) || !key(binding))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto request = LomProtocol::call_method(LomPaths::song(),
                                                  "sunny_managed_sample_envelope",
                                                  {json{{"document_token", expected.document_token},
                                                        {"project_key", project},
                                                        {"binding_key", binding},
                                                        {"parameter", parameter},
                                                        {"sample_times", times}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    const auto response = transport.send(request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto raw = std::get<json>(*response.value);
    auto wrapper = raw;
    if (wrapper.is_object()) wrapper.erase("envelope");
    const auto observed = parse_observation(expected, project, binding, wrapper);
    if (!observed) return std::unexpected(ErrorCode::ProtocolError);
    if (observed->outcome != ManagedObservationOutcome::Observed) {
        if (raw != wrapper) return std::unexpected(ErrorCode::ProtocolError);
        return ManagedEnvelopeObservation{*observed, raw};
    }
    if (!raw.contains("envelope") || !wrapper.at("observation").contains("note_identity"))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& envelope = raw.at("envelope");
    if (!fields(envelope, {"has_envelope", "parameter", "samples"}) ||
        !envelope.at("has_envelope").is_boolean() || !envelope.at("samples").is_array())
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& domain = envelope.at("parameter");
    if (!fields(domain,
                {"matched_name",
                 "original_name",
                 "minimum",
                 "maximum",
                 "unit",
                 "state",
                 "automation_state"}) ||
        !domain.at("matched_name").is_string() || !domain.at("original_name").is_string() ||
        !finite(domain.at("minimum")) || !finite(domain.at("maximum")) ||
        domain.at("minimum").get<double>() >= domain.at("maximum").get<double>() ||
        domain.at("unit") != "internal" || !integer(domain.at("state"), 0, 0) ||
        !integer(domain.at("automation_state"), 0, 2))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& manifest = wrapper.at("observation").at("manifest");
    if (envelope.at("has_envelope") == true && manifest.at("clip").at("has_envelopes") != true)
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& mixer = manifest.at("mixer");
    const auto kind = parameter.at("kind").get<std::string>();
    const json* actual = nullptr;
    if (kind == "send") {
        const auto index = parameter.at("send_index").get<std::size_t>();
        if (index >= mixer.at("sends").size()) return std::unexpected(ErrorCode::ProtocolError);
        actual = &mixer.at("sends")[index];
    } else
        actual = &mixer.at(kind);
    if (domain.at("matched_name") != actual->at("name") ||
        domain.at("original_name") != actual->at("original_name") ||
        domain.at("minimum").get<double>() != actual->at("min").get<double>() ||
        domain.at("maximum").get<double>() != actual->at("max").get<double>() ||
        domain.at("state") != actual->at("state") ||
        domain.at("automation_state") != actual->at("automation_state") ||
        actual->at("is_quantized") != false || actual->at("is_enabled") != true)
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& samples = envelope.at("samples");
    if (samples.size() != (envelope.at("has_envelope").get<bool>() ? times.size() : 0U))
        return std::unexpected(ErrorCode::ProtocolError);
    const double end = manifest.at("clip").at("end_marker").get<double>();
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        if (!fields(sample, {"time", "value"}) || !finite(sample.at("time")) ||
            sample.at("time").get<double>() != times[i] || times[i] >= end ||
            !finite(sample.at("value")) ||
            sample.at("value").get<double>() < domain.at("minimum").get<double>() ||
            sample.at("value").get<double>() > domain.at("maximum").get<double>())
            return std::unexpected(ErrorCode::ProtocolError);
    }
    // Marker admission also applies when no lane exists and no values returned.
    if (std::ranges::any_of(times, [end](double time) { return time >= end; }))
        return std::unexpected(ErrorCode::ProtocolError);
    return ManagedEnvelopeObservation{*observed, raw};
}

Result<LomRequest> make_managed_note_update_request(const ManagedBridgeContext& context,
                                                    const std::string& operation_id,
                                                    const ManagedBindingReceipt& binding,
                                                    const json& changes) {
    if (!context_valid(context) || !managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.document_token != binding.context.document_token ||
        context.bridge_instance != binding.context.bridge_instance ||
        !binding.observation.contains("note_identity") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key) ||
        !note_update_boundary(binding.observation.at("manifest"), binding.observation) ||
        !managed_detail::proposed_notes(
            binding.observation.at("note_identity"),
            changes,
            binding.observation.at("manifest").at("clip").at("end_marker").get<double>()))
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_update_notes",
        {json{{"document_token", context.document_token},
              {"operation_id", operation_id},
              {"project_key", binding.project_key},
              {"binding_key", binding.binding_key},
              {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
              {"changes", changes}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::note_response_fits(
            *payload(request),
            request.property_or_method,
            binding.observation,
            binding.observation.at("note_identity").at("notes").size()))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return request;
}

Result<LomRequest> make_managed_note_population_request(const ManagedBridgeContext& context,
                                                        const std::string& operation_id,
                                                        const ManagedBindingReceipt& binding,
                                                        const json& changes,
                                                        const json& deletions,
                                                        const json& additions) {
    if (!context_valid(context) || !managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.document_token != binding.context.document_token ||
        context.bridge_instance != binding.context.bridge_instance ||
        !binding.observation.contains("note_identity") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key) ||
        !note_update_boundary(binding.observation.at("manifest"), binding.observation))
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_revise_note_population",
        {json{{"document_token", context.document_token},
              {"operation_id", operation_id},
              {"project_key", binding.project_key},
              {"binding_key", binding.binding_key},
              {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
              {"changes", changes},
              {"deletions", deletions},
              {"additions", additions}}});
    const auto proposed = managed_detail::proposed_population(
        binding.observation.at("note_identity"),
        *payload(request),
        binding.observation.at("manifest").at("clip").at("end_marker").get<double>());
    if (!LomProtocol::validate_request(request) || !proposed)
        return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::note_response_fits(*payload(request),
                                            request.property_or_method,
                                            binding.observation,
                                            proposed->size() + additions.size(),
                                            true))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return request;
}
Result<LomRequest> make_managed_clip_geometry_request(const ManagedBridgeContext& context,
                                                      const std::string& operation_id,
                                                      const ManagedBindingReceipt& binding,
                                                      double end_marker,
                                                      int signature_numerator,
                                                      int signature_denominator) {
    const json geometry{{"end_marker", end_marker},
                        {"signature_numerator", signature_numerator},
                        {"signature_denominator", signature_denominator}};
    if (!context_valid(context) || !managed_binding_from_json(managed_binding_to_json(binding)) ||
        context.document_token != binding.context.document_token ||
        context.bridge_instance != binding.context.bridge_instance ||
        !binding.observation.contains("note_identity") ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key) ||
        !note_update_boundary(binding.observation.at("manifest"), binding.observation) ||
        !managed_detail::clip_geometry_note_admission(binding.observation.at("note_identity"),
                                                      geometry) ||
        managed_detail::changed_geometry_properties(binding.observation.at("manifest").at("clip"),
                                                    geometry)
            .empty())
        return std::unexpected(ErrorCode::ProtocolError);
    auto request = LomProtocol::call_method(
        LomPaths::song(),
        "sunny_managed_update_clip_geometry",
        {json{{"document_token", context.document_token},
              {"operation_id", operation_id},
              {"project_key", binding.project_key},
              {"binding_key", binding.binding_key},
              {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
              {"geometry", geometry}}});
    if (!LomProtocol::validate_request(request)) return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::clip_geometry_response_fits(*payload(request), binding.observation))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return request;
}
} // namespace sunny::infrastructure
