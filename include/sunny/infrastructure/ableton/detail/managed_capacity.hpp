#pragma once

#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace sunny::infrastructure::managed_detail {
inline constexpr std::size_t managed_response_limit = 16U * 1024U * 1024U;

/// Conservative default Python json.dumps wire bytes: ASCII escapes, spaces,
/// 32 bytes for any finite binary64 spelling, five for either Boolean. SM1's
/// typed fingerprint representation is deliberately a separate boundary.
inline std::optional<std::size_t> json_wire_bound(const nlohmann::json& value,
                                                  std::size_t depth = 0) {
    if (depth > 32) return std::nullopt;
    if (value.is_null()) return 4;
    if (value.is_boolean()) return 5;
    if (value.is_number_float())
        return std::isfinite(value.get<double>()) ? std::optional<std::size_t>{32} : std::nullopt;
    if (value.is_number_integer()) return value.dump().size();
    if (value.is_string()) {
        try {
            auto result = value.dump(-1, ' ', true).size();
            // Python escapes DEL too, whereas nlohmann's ASCII mode preserves it.
            for (const auto c : value.get_ref<const std::string&>())
                if (static_cast<unsigned char>(c) == 127) result += 5;
            return result;
        } catch (...) {
            return std::nullopt;
        }
    }
    if (!value.is_array() && !value.is_object()) return std::nullopt;
    std::size_t result = 2 + (value.empty() ? 0 : (value.size() - 1) * 2);
    const auto add = [&result](std::optional<std::size_t> amount) {
        if (!amount || result > std::numeric_limits<std::size_t>::max() - *amount) return false;
        result += *amount;
        return true;
    };
    if (value.is_array()) {
        for (const auto& child : value)
            if (!add(json_wire_bound(child, depth + 1))) return std::nullopt;
    } else {
        for (const auto& [name, child] : value.items())
            if (!add(json_wire_bound(nlohmann::json(name), depth + 1)) || !add(std::size_t{2}) ||
                !add(json_wire_bound(child, depth + 1)))
                return std::nullopt;
    }
    return result;
}

inline std::size_t note_array_bound(std::size_t count, bool with_ids) {
    nlohmann::json note{{"pitch", 127},
                        {"start_time", 0.0},
                        {"duration", 0.0},
                        {"velocity", 0.0},
                        {"mute", false},
                        {"probability", 0.0},
                        {"velocity_deviation", 0.0},
                        {"release_velocity", 0.0}};
    if (with_ids) note["note_id"] = INT32_MIN;
    return 2 + (count == 0 ? 0 : (count - 1) * 2) + count * *json_wire_bound(note);
}

inline bool operation_reservation_fits(const nlohmann::json& request, std::string_view name) {
    using json = nlohmann::json;
    json declined{{"document_token", request.at("document_token")},
                  {"operation_id", request.at("operation_id")},
                  {"request_fingerprint", std::string(64, 'f')},
                  {"request", request},
                  {"name", name},
                  {"outcome", "declined"},
                  {"native_mutation_started", false},
                  {"partial_binding_retained", false}};
    std::string maximum_error;
    for (std::size_t i = 0; i < 1024; ++i)
        maximum_error += "\xF4\x8F\xBF\xBF";
    declined["error"] = maximum_error;
    if (name == "sunny_managed_revise_note_population") {
        auto ids = json::array();
        for (std::size_t i = 0; i < request.at("additions").size(); ++i)
            ids.push_back(INT32_MIN);
        declined["progress"] = json{
            {"started_calls", {"remove_notes_by_id", "apply_note_modifications", "add_new_notes"}},
            {"returned_calls", {"remove_notes_by_id", "apply_note_modifications", "add_new_notes"}},
            {"returned_added_note_ids", ids}};
    }
    const auto size = json_wire_bound(json{{"success", true}, {"value", declined}});
    return size && *size <= managed_response_limit;
}

/// Necessary known-note byte admission before native creation. Unknown native
/// metadata is deliberately not invented; the bridge measures it after creating
/// the empty Track/Clip and repeats admission before note insertion.
inline bool creation_response_fits(const nlohmann::json& request, std::string_view name) {
    using json = nlohmann::json;
    if (!operation_reservation_fits(request, name)) return false;
    json result{{"manifest", {{"notes", json::array()}}},
                {"note_identity", {{"notes", json::array()}}},
                {"observed_notes_match_request", false},
                {"observed_clip_properties_match_request", false}};
    json journal{{"document_token", request.at("document_token")},
                 {"operation_id", request.at("operation_id")},
                 {"request_fingerprint", std::string(64, 'f')},
                 {"request", request},
                 {"name", name},
                 {"outcome", "acknowledged"},
                 {"native_mutation_started", true},
                 {"result", result}};
    const auto metadata = json_wire_bound(json{{"success", true}, {"value", journal}});
    if (!metadata) return false;
    const auto count = request.at("notes").size();
    const auto notes = note_array_bound(count, false) + note_array_bound(count, true) - 4;
    return *metadata <= managed_response_limit && notes <= managed_response_limit - *metadata;
}

/// Immutable before populations retain their observed values; prospective after
/// populations reserve all native IDs/finite numeric widths. Device/recovery
/// supplements remain included as finite metadata, not silently stripped.
inline bool note_response_fits(const nlohmann::json& request,
                               std::string_view name,
                               const nlohmann::json& before,
                               std::size_t after_count,
                               bool population = false) {
    using json = nlohmann::json;
    auto after = before;
    for (const auto* key :
         {"note_update", "note_population_update", "acknowledgement", "device_update", "adoption"})
        after.erase(key);
    after["manifest"]["notes"] = json::array();
    after["note_identity"]["notes"] = json::array();
    after["observed_notes_match_request"] = false;
    after["observed_clip_properties_match_request"] = false;
    json supplement{{"before_manifest", before.at("manifest")},
                    {"before_note_identity", before.at("note_identity")},
                    {"before_note_identity_fingerprint", before.at("note_identity_fingerprint")}};
    json journal{{"document_token", request.at("document_token")},
                 {"operation_id", request.at("operation_id")},
                 {"request_fingerprint", std::string(64, 'f')},
                 {"request", request},
                 {"name", name},
                 {"outcome", "acknowledged"},
                 {"native_mutation_started", true}};
    if (population) {
        const auto additions = request.at("additions").size();
        supplement["changes_submitted"] = request.at("changes").size();
        supplement["deletions_submitted"] = request.at("deletions").size();
        supplement["additions_submitted"] = additions;
        supplement["returned_added_note_ids"] = json::array();
        supplement["addition_associations"] = json::array();
        for (const auto& addition : request.at("additions")) {
            supplement["returned_added_note_ids"].push_back(INT32_MIN);
            supplement["addition_associations"].push_back(
                json{{"note_key", addition.at("note_key")}, {"note_id", INT32_MIN}});
        }
        for (const auto* flag : {"observed_changes_match_request",
                                 "observed_deletions_absent",
                                 "observed_additions_match_request",
                                 "untouched_notes_preserved",
                                 "retained_note_ids_preserved",
                                 "observed_population_cardinality_match"})
            supplement[flag] = false;
        after["note_population_update"] = supplement;
        journal["progress"] = json{
            {"started_calls", {"remove_notes_by_id", "apply_note_modifications", "add_new_notes"}},
            {"returned_calls", {"remove_notes_by_id", "apply_note_modifications", "add_new_notes"}},
            {"returned_added_note_ids", supplement.at("returned_added_note_ids")}};
    } else {
        supplement["notes_submitted"] = request.at("changes").size();
        supplement["observed_updates_match_request"] = false;
        supplement["untouched_notes_preserved"] = false;
        supplement["note_ids_preserved"] = false;
        after["note_update"] = supplement;
    }
    if (!operation_reservation_fits(request, name)) return false;
    journal["result"] = after;
    const auto metadata = json_wire_bound(json{{"success", true}, {"value", journal}});
    if (!metadata) return false;
    const auto populations =
        note_array_bound(after_count, false) + note_array_bound(after_count, true) - 4;
    return *metadata <= managed_response_limit && populations <= managed_response_limit - *metadata;
}
} // namespace sunny::infrastructure::managed_detail
