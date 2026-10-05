#include <array>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/detail/native_unit_decimal.hpp>
namespace sunny::infrastructure {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
namespace managed_mixer_detail {
using namespace managed_detail;
namespace {
constexpr std::array<std::string_view, 4> domains{"volume", "pan", "mute", "solo"};
constexpr std::array<std::string_view, 7> song_flags{"is_playing",
                                                     "is_counting_in",
                                                     "record_mode",
                                                     "session_record",
                                                     "session_automation_record",
                                                     "arrangement_overdub",
                                                     "overdub"};
json unavailable() {
    return json::array({"audio_dsp_equivalence",
                        "programme_loudness",
                        "pan_law",
                        "depth_elevation_width",
                        "other_static_mixer_roles",
                        "routing_returns_sends_groups_master",
                        "unknown_envelope_population",
                        "unknown_device_opaque_state",
                        "historical_native_identity"});
}
bool finite(const json& v, bool actual = false) {
    return v.is_number() && (!actual || v.is_number_float()) && std::isfinite(v.get<double>());
}
bool mask(const json& value, bool empty = false) {
    if (!value.is_array() || (!empty && value.empty()) || value.size() > 4) return false;
    auto expected = json::array();
    for (auto name : domains)
        if (std::ranges::find(value, json(name)) != value.end()) expected.push_back(name);
    return value == expected;
}
bool binding_valid(const json& v, const json& metadata) {
    auto closed = v;
    closed.erase("device_identity");
    closed.erase("device_identity_fingerprint");
    closed.erase("group_authority");
    closed.erase("group_authority_fingerprint");
    if (!recovery_fields(closed,
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
    auto parsed = managed_binding_from_json({{"schema_version", 1},
                                             {"context", metadata.at("context")},
                                             {"project_key", metadata.at("project_key")},
                                             {"binding_key", metadata.at("binding_key")},
                                             {"observation", v}});
    if (!parsed ||
        !group_touched_boundary(v,
                                {metadata.at("context").at("bridge_instance"),
                                 metadata.at("context").at("document_token")},
                                metadata.at("project_key"),
                                metadata.at("binding_key")) ||
        v.at("note_identity").at("entire_clip_population_observed") != true)
        return false;
    const auto& track = v.at("manifest").at("track");
    const auto& clip = v.at("manifest").at("clip");
    for (auto n : {"arm", "implicit_arm", "is_frozen"})
        if (track.at(n) != false) return false;
    for (auto n :
         {"is_playing", "is_recording", "is_overdubbing", "is_triggered", "will_record_on_start"})
        if (clip.at(n) != false) return false;
    return true;
}
bool matches_before(const json& p, const json& request) {
    const auto& b = p.at("before").at("binding_observation");
    return p.at("context").at("document_token") == request.at("document_token") &&
           p.at("project_key") == request.at("project_key") &&
           p.at("binding_key") == request.at("binding_key") &&
           p.at("purpose") == request.at("purpose") &&
           p.at("selected_domains") == request.at("selected_domains") &&
           managed_digest(p.at("desired")) == managed_digest(request.at("desired")) &&
           b.at("content_fingerprint") == request.at("expected_content_fingerprint") &&
           b.at("note_identity_fingerprint") == request.at("expected_note_identity_fingerprint");
}
json changed(const json& p) {
    auto result = json::array();
    const auto& d = p.at("desired");
    const auto& m = p.at("before").at("binding_observation").at("manifest");
    for (auto n : domains)
        if (d.contains(n)) {
            bool change = false;
            if (n == "volume")
                change = m.at("mixer").at("volume").at("value") !=
                         p.at("candidates").at("volume").at("internal_value");
            else if (n == "pan")
                change = m.at("mixer").at("panning").at("value") != d.at(n);
            else
                change = m.at("track").at(n) != d.at(n);
            if (change) result.push_back(n);
        }
    return result;
}
bool readback_valid(const json& v, const json& p, const json& after) {
    const auto& d = p.at("desired");
    const auto& m = after.at("binding_observation").at("manifest");
    if (!v.is_object() || v.size() != d.size()) return false;
    for (auto n : domains)
        if (d.contains(n)) {
            if (!v.contains(n)) return false;
            const auto& item = v.at(n);
            if (n == "volume") {
                if (!recovery_fields(item,
                                     {"internal_value",
                                      "display",
                                      "display_value",
                                      "display_increment",
                                      "negative_infinity",
                                      "matches"}) ||
                    !finite(item.at("internal_value"), true) ||
                    item.at("internal_value") != m.at("mixer").at("volume").at("value") ||
                    !recovery_text(item.at("display"), 80) ||
                    item.at("negative_infinity") != false || item.at("matches") != true ||
                    !finite(item.at("display_value"), true) ||
                    !finite(item.at("display_increment"), true))
                    return false;
                auto reading = native_unit_detail::display_reading(
                    item.at("display").get<std::string>(),
                    sunny::core::LiveNativePhysicalUnit::Decibels,
                    true);
                if (!reading || reading->negative_infinity) return false;
                auto proof = native_unit_detail::decimal_display_comparison(
                    *reading, d.at("volume").at("target"), d.at("volume").at("tolerance"), 1.0, 28);
                if (!proof || !proof->within_tolerance ||
                    item.at("display_value") != proof->display_value || !proof->display_increment ||
                    item.at("display_increment") != *proof->display_increment)
                    return false;
            } else if (n == "pan") {
                if (!recovery_fields(item, {"internal_value", "matches"}) ||
                    !finite(item.at("internal_value"), true) ||
                    item.at("internal_value") != d.at(n) ||
                    item.at("internal_value") != m.at("mixer").at("panning").at("value") ||
                    item.at("matches") != true)
                    return false;
            } else {
                if (!(n == "mute"
                          ? recovery_fields(item, {"value", "matches", "track_activator_value"})
                          : recovery_fields(item, {"value", "matches"})) ||
                    !item.at("value").is_boolean() || item.at("value") != d.at(n) ||
                    item.at("value") != m.at("track").at(n) || item.at("matches") != true)
                    return false;
                if (n == "mute" &&
                    (!finite(item.at("track_activator_value"), true) ||
                     item.at("track_activator_value") != (d.at(n) == true ? 0.0 : 1.0) ||
                     item.at("track_activator_value") !=
                         m.at("mixer").at("track_activator").at("value")))
                    return false;
            }
        }
    return true;
}
} // namespace
bool desired_valid(const json& v, const json& selected) {
    try {
        if (!mask(selected) || !v.is_object() || v.size() != selected.size()) return false;
        for (const auto& name : selected) {
            if (!v.contains(name.get<std::string>())) return false;
            const auto& item = v.at(name.get<std::string>());
            if (name == "volume") {
                if (!recovery_fields(item, {"target", "tolerance"}) || !finite(item.at("target")) ||
                    !finite(item.at("tolerance")) || item.at("tolerance").get<double>() < 0)
                    return false;
            } else if (name == "pan") {
                if (!finite(item) || item.get<double>() < -1 || item.get<double>() > 1)
                    return false;
            } else if (!item.is_boolean())
                return false;
        }
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
bool snapshot_valid(const json& v, const json& metadata, bool require_envelope_absence) {
    try {
        if (!recovery_fields(v,
                             {"binding_observation",
                              "selected_envelopes",
                              "mixer_capture",
                              "track_context",
                              "solo_cohort",
                              "song_flags",
                              "version"}) ||
            !binding_valid(v.at("binding_observation"), metadata) ||
            !native_mixer_detail::capture_valid(v.at("mixer_capture")))
            return false;
        const auto& version = v.at("version");
        if (!version.is_array() || version.size() != 3 || !note_integer(version[0], 12, 12) ||
            !note_integer(version[1], 3, 4) || !note_integer(version[2], 0, INT32_MAX))
            return false;
        const auto& envelopes = v.at("selected_envelopes");
        std::size_t expected_envelopes = 0;
        if (!envelopes.is_object()) return false;
        for (auto n : {"volume", "pan", "mute"})
            if (metadata.at("desired").contains(n)) {
                ++expected_envelopes;
                if (!envelopes.contains(n) || !envelopes.at(n).is_boolean() ||
                    (require_envelope_absence && envelopes.at(n) != false))
                    return false;
            }
        if (envelopes.size() != expected_envelopes) return false;
        const auto& flags = v.at("song_flags");
        if (!flags.is_object() || flags.size() != song_flags.size()) return false;
        for (auto n : song_flags)
            if (!flags.contains(n) || !flags.at(n).is_boolean() || flags.at(n) != false)
                return false;
        const auto& obs = v.at("binding_observation");
        const auto& manifest = obs.at("manifest");
        auto track = manifest.at("track");
        track.erase("name");
        if (managed_digest(track) != managed_digest(v.at("track_context"))) return false;
        const auto& capture = v.at("mixer_capture");
        const auto& native = manifest.at("mixer");
        if (capture.at("panning_mode") != native.at("panning_mode") ||
            capture.at("crossfade_assign") != native.at("crossfade_assign"))
            return false;
        const auto& members = capture.at("parameters");
        if (members.size() != native.at("sends").size() + 3) return false;
        for (std::size_t i = 0; i < members.size(); ++i) {
            const auto& member = members[i];
            const auto& d = member.at("descriptor");
            const auto& actual =
                i < 3 ? native.at(std::array{"volume", "panning", "track_activator"}[i])
                      : native.at("sends")[i - 3];
            if (member.at("name") != actual.at("name") ||
                member.at("original_name") != actual.at("original_name"))
                return false;
            for (auto n : {"value", "is_quantized", "is_enabled", "state", "automation_state"})
                if (d.at(n) != actual.at(n)) return false;
            if (d.at("minimum") != actual.at("min") || d.at("maximum") != actual.at("max"))
                return false;
        }
        const auto& cohort = v.at("solo_cohort");
        if (!cohort.is_array() || cohort.empty() || cohort.size() > 4096) return false;
        bool returns = false;
        std::size_t index = 0, tracks = 0;
        for (const auto& entry : cohort) {
            if (!recovery_fields(entry,
                                 {"kind", "index", "name", "mute", "solo", "muted_via_solo"}) ||
                !recovery_text(entry.at("name"), 1024, true) ||
                !note_integer(entry.at("index"), 0, 4095))
                return false;
            if (entry.at("kind") == "return" && !returns) {
                returns = true;
                index = 0;
            }
            if (entry.at("kind") != (returns ? "return" : "track") || entry.at("index") != index++)
                return false;
            if (!returns) ++tracks;
            for (auto n : {"mute", "solo", "muted_via_solo"})
                if (!entry.at(n).is_boolean()) return false;
        }
        const auto selected = obs.at("track_index").get<std::size_t>();
        if (selected >= tracks) return false;
        for (auto n : {"name", "mute", "solo"})
            if (cohort[selected].at(n) != manifest.at("track").at(n)) return false;
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
bool readonly_body_valid(const json& p, bool inspection) {
    try {
        auto common = p;
        if (inspection) {
            if (!common.is_object() || !common.contains("authority_origin") ||
                !common.contains("native_mutation_started") ||
                common.at("authority_origin") != "none" ||
                common.at("native_mutation_started") != false || common.contains("preview_token"))
                return false;
            common.erase("authority_origin");
            common.erase("native_mutation_started");
        } else {
            if (!common.contains("preview_token") || !recovery_hex(common.at("preview_token"), 32))
                return false;
            common.erase("preview_token");
        }
        if (!recovery_fields(common,
                             {"schema_version",
                              "context",
                              "project_key",
                              "binding_key",
                              "purpose",
                              "selected_domains",
                              "desired",
                              "before",
                              "candidates",
                              "current_authority_domains",
                              "scope",
                              "unavailable_domains"}) ||
            !note_integer(p.at("schema_version"), 1, 1) || !recovery_context(p.at("context")) ||
            !recovery_key(p.at("project_key")) || !recovery_key(p.at("binding_key")) ||
            (inspection ? p.at("purpose") != "inspect"
                        : (p.at("purpose") != "adopt" && p.at("purpose") != "update")) ||
            !desired_valid(p.at("desired"), p.at("selected_domains")) ||
            !mask(p.at("current_authority_domains"), true) || !snapshot_valid(p.at("before"), p))
            return false;
        if (p.at("purpose") == "update")
            for (const auto& n : p.at("selected_domains"))
                if (std::ranges::find(p.at("current_authority_domains"), n) ==
                    p.at("current_authority_domains").end())
                    return false;
        const auto& d = p.at("desired");
        const auto& before = p.at("before");
        const auto& capture = before.at("mixer_capture");
        const auto& params = capture.at("parameters");
        const auto& candidates = p.at("candidates");
        if (!candidates.is_object() || candidates.size() != (d.contains("volume") ? 1u : 0u))
            return false;
        if (d.contains("volume")) {
            if (!candidates.contains("volume")) return false;
            const auto& v = candidates.at("volume");
            const auto& intent = d.at("volume");
            if (!parse_native_mixer_display_candidate(
                    "volume", intent.at("target"), intent.at("tolerance"), v) ||
                managed_digest(v.at("mixer_capture")) != managed_digest(capture) ||
                managed_digest(v.at("track_context")) != managed_digest(before.at("track_context")))
                return false;
        }
        if (d.contains("volume") || d.contains("pan"))
            if (before.at("track_context").at("has_audio_output") != true ||
                before.at("track_context").at("has_midi_output") != false)
                return false;
        for (std::size_t i = 0; i < 3; ++i)
            if (d.contains(domains[i])) {
                const auto& descriptor = params[i].at("descriptor");
                if (descriptor.at("is_enabled") != true || descriptor.at("state") != 0 ||
                    descriptor.at("automation_state") != 0 ||
                    descriptor.at("is_quantized") != (i == 2))
                    return false;
            }
        if (d.contains("pan") &&
            (capture.at("panning_mode") != 0 || params[1].at("descriptor").at("minimum") != -1.0 ||
             params[1].at("descriptor").at("maximum") != 1.0))
            return false;
        if (d.contains("mute")) {
            const auto& a = params[2].at("descriptor");
            if (a.at("minimum") != 0.0 || a.at("maximum") != 1.0 ||
                a.at("value") != (before.at("track_context").at("mute") == true ? 0.0 : 1.0))
                return false;
        }
        return p.at("scope") == json{{"set_wide_audible_effect", d.contains("solo")},
                                     {"historical_identity_proven", false},
                                     {"native_knob_only", true},
                                     {"host_qualified", false}} &&
               p.at("unavailable_domains") == unavailable();
    } catch (const json::exception&) {
        return false;
    }
}
bool preview_valid(const json& p) {
    return readonly_body_valid(p, false);
}
bool request_valid(std::string_view method, const json& v) {
    try {
        const std::initializer_list<std::string_view> common = {
            "document_token",
            "project_key",
            "binding_key",
            "expected_content_fingerprint",
            "expected_note_identity_fingerprint",
            "purpose",
            "selected_domains",
            "desired"};
        if (method == preview_method || method == inspection_method) {
            if (!recovery_fields(v, common)) return false;
        } else if (method == adopt_method || method == update_method) {
            if (!recovery_fields(v,
                                 {"document_token",
                                  "project_key",
                                  "binding_key",
                                  "expected_content_fingerprint",
                                  "expected_note_identity_fingerprint",
                                  "purpose",
                                  "selected_domains",
                                  "desired",
                                  "operation_id",
                                  "preview_token",
                                  "preview_fingerprint",
                                  "approved_preview",
                                  "explicit_current_mixer_approval",
                                  "explicit_set_wide_audible_approval"}) ||
                !recovery_key(v.at("operation_id")) || !recovery_hex(v.at("preview_token"), 32) ||
                !recovery_hex(v.at("preview_fingerprint"), 64) ||
                v.at("explicit_current_mixer_approval") != true ||
                !v.at("explicit_set_wide_audible_approval").is_boolean() ||
                (v.at("desired").contains("solo") &&
                 v.at("explicit_set_wide_audible_approval") != true) ||
                v.at("purpose") != (method == adopt_method ? "adopt" : "update") ||
                !preview_valid(v.at("approved_preview")) ||
                !matches_before(v.at("approved_preview"), v) ||
                v.at("preview_token") != v.at("approved_preview").at("preview_token") ||
                managed_digest(v.at("approved_preview")) !=
                    std::optional{v.at("preview_fingerprint").get<std::string>()})
                return false;
        } else
            return false;
        return recovery_key(v.at("document_token")) && recovery_key(v.at("project_key")) &&
               recovery_key(v.at("binding_key")) &&
               recovery_hex(v.at("expected_content_fingerprint"), 64) &&
               recovery_hex(v.at("expected_note_identity_fingerprint"), 64) &&
               (method == inspection_method
                    ? v.at("purpose") == "inspect"
                    : (v.at("purpose") == "adopt" || v.at("purpose") == "update")) &&
               desired_valid(v.at("desired"), v.at("selected_domains"));
    } catch (const json::exception&) {
        return false;
    }
}
bool untouched(const json& before, const json& after, const json& returned) {
    try {
        if (!mask(returned, true)) return false;
        auto left = before, right = after;
        for (auto* value : {&left, &right}) {
            auto& observation = value->at("binding_observation");
            observation.erase("content_fingerprint");
            auto& m = observation.at("manifest");
            for (const auto& name : returned) {
                const auto n = name.get<std::string>();
                if (n == "volume" || n == "pan") {
                    const auto role = n == "volume" ? "volume" : "panning";
                    const auto i = n == "volume" ? 0 : 1;
                    m.at("mixer").at(role).erase("value");
                    value->at("mixer_capture").at("parameters")[i].at("descriptor").erase("value");
                } else {
                    m.at("track").erase(n);
                    value->at("track_context").erase(n);
                    value->at("solo_cohort")[observation.at("track_index").get<std::size_t>()]
                        .erase(n);
                    if (n == "mute") {
                        m.at("mixer").at("track_activator").erase("value");
                        auto& a = value->at("mixer_capture").at("parameters")[2].at("descriptor");
                        a.erase("value");
                        a.erase("label");
                    } else
                        for (auto& entry : value->at("solo_cohort"))
                            entry.erase("muted_via_solo");
                }
            }
        }
        return managed_digest(left) == managed_digest(right);
    } catch (const json::exception&) {
        return false;
    }
}
bool result_matches_request(std::string_view method, const json& request, const json& result) {
    try {
        if (!request_valid(method, request) || !result.is_object()) return false;
        const auto& p = request.at("approved_preview");
        auto base = result;
        base.erase("mixer_adoption");
        base.erase("mixer_update");
        base.erase("observed_notes_match_request");
        base.erase("observed_clip_properties_match_request");
        if (!binding_valid(base, p)) return false;
        if (method == adopt_method) {
            if (!result.contains("mixer_adoption") || result.contains("mixer_update")) return false;
            const auto& item = result.at("mixer_adoption");
            return recovery_fields(item,
                                   {"approved_preview",
                                    "preview_fingerprint",
                                    "authority_origin",
                                    "granted_domains",
                                    "native_mutation_started",
                                    "historical_identity_proven"}) &&
                   managed_digest(item.at("approved_preview")) == managed_digest(p) &&
                   item.at("preview_fingerprint") == request.at("preview_fingerprint") &&
                   item.at("authority_origin") == "explicit_current_mixer_adoption" &&
                   item.at("granted_domains") == request.at("selected_domains") &&
                   item.at("native_mutation_started") == false &&
                   item.at("historical_identity_proven") == false &&
                   managed_digest(base) == managed_digest(p.at("before").at("binding_observation"));
        }
        if (!result.contains("mixer_update") || result.contains("mixer_adoption")) return false;
        const auto& item = result.at("mixer_update");
        if (!recovery_fields(item,
                             {"approved_preview",
                              "preview_fingerprint",
                              "before",
                              "after",
                              "desired",
                              "readback",
                              "started_fields",
                              "returned_fields",
                              "observed_untouched_state_preserved",
                              "clip_and_note_ids_preserved",
                              "authority_scope",
                              "native_knob_only",
                              "host_qualified",
                              "set_wide_audible_effect",
                              "unavailable_domains"}) ||
            managed_digest(item.at("approved_preview")) != managed_digest(p) ||
            item.at("preview_fingerprint") != request.at("preview_fingerprint") ||
            managed_digest(item.at("before")) != managed_digest(p.at("before")) ||
            managed_digest(item.at("desired")) != managed_digest(p.at("desired")) ||
            !snapshot_valid(item.at("after"), p) || item.at("started_fields") != changed(p) ||
            item.at("returned_fields") != changed(p) ||
            !untouched(item.at("before"), item.at("after"), item.at("returned_fields")) ||
            !readback_valid(item.at("readback"), p, item.at("after")) ||
            managed_digest(base) != managed_digest(item.at("after").at("binding_observation")))
            return false;
        return item.at("observed_untouched_state_preserved") == true &&
               item.at("clip_and_note_ids_preserved") == true &&
               item.at("authority_scope") == "retained_selected_static_mixer_domains" &&
               item.at("native_knob_only") == true && item.at("host_qualified") == false &&
               item.at("set_wide_audible_effect") == p.at("desired").contains("solo") &&
               item.at("unavailable_domains") == unavailable();
    } catch (const json::exception&) {
        return false;
    }
}
bool acknowledged_native_start_valid(std::string_view method,
                                     const json& request,
                                     const json& result,
                                     bool started) {
    return result_matches_request(method, request, result) &&
           (method == adopt_method ? !started
                                   : started == !changed(request.at("approved_preview")).empty());
}
bool partial_valid(const json& request, const json& journal) {
    try {
        if (!request_valid(update_method, request) || !journal.contains("mixer_progress"))
            return false;
        const auto& progress = journal.at("mixer_progress");
        if (!recovery_fields(progress, {"started_fields", "returned_fields"}) ||
            !mask(progress.at("started_fields"), true) ||
            !mask(progress.at("returned_fields"), true))
            return false;
        const auto expected = changed(request.at("approved_preview"));
        const auto& started = progress.at("started_fields");
        const auto& returned = progress.at("returned_fields");
        if (started.size() > expected.size() || returned.size() > started.size() ||
            started.size() > returned.size() + 1)
            return false;
        for (std::size_t i = 0; i < started.size(); ++i)
            if (started[i] != expected[i]) return false;
        for (std::size_t i = 0; i < returned.size(); ++i)
            if (returned[i] != started[i]) return false;
        if (journal.contains("mixer_partial")) {
            const auto& evidence = journal.at("mixer_partial");
            if (!recovery_fields(evidence,
                                 {"before",
                                  "after",
                                  "observed_after_available",
                                  "desired",
                                  "started_fields",
                                  "returned_fields"}) ||
                !evidence.at("observed_after_available").is_boolean() ||
                managed_digest(evidence.at("before")) !=
                    managed_digest(request.at("approved_preview").at("before")) ||
                managed_digest(evidence.at("desired")) != managed_digest(request.at("desired")) ||
                evidence.at("started_fields") != started ||
                evidence.at("returned_fields") != returned)
                return false;
            if (evidence.at("observed_after_available") == false) {
                if (!evidence.at("after").is_null()) return false;
            } else if (!snapshot_valid(evidence.at("after"), request.at("approved_preview"), false))
                return false;
        } else if (!started.empty())
            return false;
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
} // namespace managed_mixer_detail
json managed_static_mixer_desired_to_json(const ManagedStaticMixerDesired& value) {
    json d = json::object();
    if (value.volume)
        d["volume"] = {{"target", value.volume->target}, {"tolerance", value.volume->tolerance}};
    if (value.pan) d["pan"] = *value.pan;
    if (value.mute) d["mute"] = *value.mute;
    if (value.solo) d["solo"] = *value.solo;
    return d;
}
namespace {
Result<LomRequest> make_mixer_readonly_request(const ManagedBridgeContext& context,
                                               const ManagedBindingReceipt& binding,
                                               const ManagedStaticMixerDesired& desired,
                                               std::string_view purpose,
                                               std::string_view method) {
    using namespace managed_detail;
    if (context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !managed_binding_from_json(managed_binding_to_json(binding)) ||
        !group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto d = managed_static_mixer_desired_to_json(desired);
    auto selected = json::array();
    for (auto n : {"volume", "pan", "mute", "solo"})
        if (d.contains(n)) selected.push_back(n);
    json request{
        {"document_token", context.document_token},
        {"project_key", binding.project_key},
        {"binding_key", binding.binding_key},
        {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
        {"expected_note_identity_fingerprint", binding.observation.at("note_identity_fingerprint")},
        {"purpose", purpose},
        {"selected_domains", selected},
        {"desired", d}};
    if (!managed_mixer_detail::request_valid(method, request))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(LomPaths::song(), std::string(method), {request});
}
} // namespace
Result<LomRequest>
make_managed_static_mixer_preview_request(const ManagedBridgeContext& context,
                                          const ManagedBindingReceipt& binding,
                                          const ManagedStaticMixerDesired& desired,
                                          bool adoption) {
    return make_mixer_readonly_request(context,
                                       binding,
                                       desired,
                                       adoption ? "adopt" : "update",
                                       managed_mixer_detail::preview_method);
}
Result<LomRequest>
make_managed_static_mixer_inspection_request(const ManagedBridgeContext& context,
                                             const ManagedBindingReceipt& binding,
                                             const ManagedStaticMixerDesired& desired) {
    return make_mixer_readonly_request(
        context, binding, desired, "inspect", managed_mixer_detail::inspection_method);
}
Result<json> parse_managed_static_mixer_inspection(const LomRequest& request,
                                                   const ManagedBridgeContext& context,
                                                   const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != managed_mixer_detail::inspection_method ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args[0]) ||
            !recovery_fields(value, {"schema_version", "outcome", "inspection"}) ||
            !note_integer(value.at("schema_version"), 1, 1) || value.at("outcome") != "observed" ||
            !managed_mixer_detail::readonly_body_valid(value.at("inspection"), true))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& p = value.at("inspection");
        const auto& payload = std::get<json>(request.args[0]);
        if (!managed_mixer_detail::request_valid(managed_mixer_detail::inspection_method,
                                                 payload) ||
            p.at("context") != json{{"bridge_instance", context.bridge_instance},
                                    {"document_token", context.document_token}} ||
            !managed_mixer_detail::matches_before(p, payload))
            return std::unexpected(ErrorCode::ProtocolError);
        return value;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<ManagedStaticMixerPreview> parse_managed_static_mixer_preview(
    const LomRequest& request, const ManagedBridgeContext& context, const json& value) {
    using namespace managed_detail;
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != managed_mixer_detail::preview_method ||
            request.args.size() != 1 || !std::holds_alternative<json>(request.args[0]) ||
            !recovery_fields(
                value,
                {"schema_version", "outcome", "preview_token", "preview_fingerprint", "preview"}) ||
            !note_integer(value.at("schema_version"), 1, 1) || value.at("outcome") != "previewed" ||
            !managed_mixer_detail::preview_valid(value.at("preview")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& p = value.at("preview");
        const auto& payload = std::get<json>(request.args[0]);
        if (!managed_mixer_detail::request_valid(managed_mixer_detail::preview_method, payload) ||
            p.at("context") != json{{"bridge_instance", context.bridge_instance},
                                    {"document_token", context.document_token}} ||
            !managed_mixer_detail::matches_before(p, payload) ||
            value.at("preview_token") != p.at("preview_token") ||
            !recovery_hex(value.at("preview_fingerprint"), 64) ||
            managed_digest(p) != std::optional{value.at("preview_fingerprint").get<std::string>()})
            return std::unexpected(ErrorCode::ProtocolError);
        return ManagedStaticMixerPreview{context,
                                         p.at("project_key"),
                                         p.at("binding_key"),
                                         value.at("preview_token"),
                                         value.at("preview_fingerprint"),
                                         p,
                                         p.at("before").at("binding_observation")};
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<LomRequest> make_managed_static_mixer_request(const ManagedBridgeContext& context,
                                                     const std::string& operation,
                                                     const ManagedStaticMixerPreview& preview,
                                                     bool setwide) {
    using namespace managed_detail;
    try {
        const auto& p = preview.approved_preview;
        if (!managed_mixer_detail::preview_valid(p) ||
            context.bridge_instance != preview.context.bridge_instance ||
            context.document_token != preview.context.document_token ||
            p.at("context") != json{{"bridge_instance", context.bridge_instance},
                                    {"document_token", context.document_token}} ||
            p.at("project_key") != preview.project_key ||
            p.at("binding_key") != preview.binding_key ||
            p.at("preview_token") != preview.preview_token ||
            managed_digest(p) != std::optional{preview.preview_fingerprint} ||
            managed_digest(preview.observation) !=
                managed_digest(p.at("before").at("binding_observation")))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto method = p.at("purpose") == "adopt" ? managed_mixer_detail::adopt_method
                                                       : managed_mixer_detail::update_method;
        json payload{
            {"document_token", context.document_token},
            {"project_key", preview.project_key},
            {"binding_key", preview.binding_key},
            {"expected_content_fingerprint", preview.observation.at("content_fingerprint")},
            {"expected_note_identity_fingerprint",
             preview.observation.at("note_identity_fingerprint")},
            {"purpose", p.at("purpose")},
            {"selected_domains", p.at("selected_domains")},
            {"desired", p.at("desired")},
            {"operation_id", operation},
            {"preview_token", preview.preview_token},
            {"preview_fingerprint", preview.preview_fingerprint},
            {"approved_preview", p},
            {"explicit_current_mixer_approval", true},
            {"explicit_set_wide_audible_approval", setwide}};
        if (!managed_mixer_detail::request_valid(method, payload))
            return std::unexpected(ErrorCode::ProtocolError);
        if (!operation_reservation_fits(payload, method))
            return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
        // Request/full before-after populations known now: reserve a conservative
        // full ACK/partial.
        auto future = p.at("before");
        auto anticipated = preview.observation;
        anticipated["observed_notes_match_request"] = false;
        anticipated["observed_clip_properties_match_request"] = false;
        anticipated["mixer_update"] = {
            {"approved_preview", p},
            {"before", future},
            {"after", future},
            {"desired", p.at("desired")},
            {"readback", json::object()},
            {"started_fields", p.at("selected_domains")},
            {"returned_fields", p.at("selected_domains")},
            {"unavailable_domains", p.at("unavailable_domains")},
            {"authority_scope", "retained_selected_static_mixer_domains"},
            {"preview_fingerprint", preview.preview_fingerprint},
            {"observed_untouched_state_preserved", true},
            {"clip_and_note_ids_preserved", true},
            {"native_knob_only", true},
            {"host_qualified", false},
            {"set_wide_audible_effect", setwide}};
        for (const auto& n : p.at("selected_domains"))
            anticipated["mixer_update"]["readback"][n.get<std::string>()] = {
                {"internal_value", 0.0},
                {"display", std::string(80, '\t')},
                {"display_value", 0.0},
                {"display_increment", 0.0},
                {"negative_infinity", false},
                {"matches", true},
                {"value", false},
                {"track_activator_value", 0.0}};
        const json journal{{"document_token", context.document_token},
                           {"operation_id", operation},
                           {"name", method},
                           {"request", payload},
                           {"request_fingerprint", std::string(64, 'f')},
                           {"outcome", "acknowledged"},
                           {"native_mutation_started", true},
                           {"mixer_progress",
                            {{"started_fields", p.at("selected_domains")},
                             {"returned_fields", p.at("selected_domains")}}},
                           {"result", anticipated}};
        auto bound = json_wire_bound(json{{"success", true}, {"value", journal}});
        auto partial = journal;
        partial.erase("result");
        partial["outcome"] = "indeterminate";
        std::string worst_error;
        for (int i = 0; i < 1024; ++i)
            worst_error += "\xf4\x8f\xbf\xbf";
        partial["error"] = std::move(worst_error);
        partial["mixer_partial"] = {{"before", future},
                                    {"after", future},
                                    {"observed_after_available", true},
                                    {"desired", p.at("desired")},
                                    {"started_fields", p.at("selected_domains")},
                                    {"returned_fields", p.at("selected_domains")}};
        auto partial_bound = json_wire_bound(json{{"success", true}, {"value", partial}});
        if (!bound || *bound > managed_response_limit || !partial_bound ||
            *partial_bound > managed_response_limit)
            return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
        return LomProtocol::call_method(LomPaths::song(), std::string(method), {payload});
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
} // namespace sunny::infrastructure
