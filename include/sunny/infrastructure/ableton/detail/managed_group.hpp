/** Finite current Group evidence. Public evidence is never a native grant. */
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure::managed_detail {

inline constexpr std::size_t maximum_group_members = 256;

inline bool group_fields(const nlohmann::json& value,
                         std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&value](auto name) { return value.contains(name); });
}

inline bool group_key(const nlohmann::json& value) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return !text.empty() && text.size() <= 64 && std::ranges::all_of(text, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

inline bool group_hex(const nlohmann::json& value) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return text.size() == 64 && std::ranges::all_of(text, [](unsigned char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

inline bool group_index(const nlohmann::json& value) {
    if (!value.is_number_integer()) return false;
    if (value.is_number_unsigned())
        return value.get<std::uint64_t>() <=
               static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    const auto index = value.get<std::int64_t>();
    return 0 <= index && index <= std::numeric_limits<std::int32_t>::max();
}

// A missing pair is valid read-only evidence, including an unapproved grouped
// Track. A present pair must be internally closed and cannot survive ungrouping.
inline bool group_supplement_valid(const nlohmann::json& observation) {
    try {
        if (!observation.is_object()) return false;
        const bool body = observation.contains("group_authority");
        const bool hash = observation.contains("group_authority_fingerprint");
        if (!body && !hash) return true;
        if (!body || !hash || !group_hex(observation.at("group_authority_fingerprint")) ||
            observation.at("manifest").at("track").at("is_grouped") != true ||
            observation.at("structural_boundary_complete") != false ||
            observation.at("content_boundary_complete") != false ||
            !group_index(observation.at("track_index")))
            return false;
        const auto& value = observation.at("group_authority");
        if (!group_fields(value,
                          {"schema_version",
                           "context",
                           "project_key",
                           "group_key",
                           "approved_preview_fingerprint",
                           "group_track_index",
                           "member_binding_keys",
                           "member_track_indices",
                           "selected_binding_key",
                           "selected_track_index",
                           "authority_origin",
                           "historical_identity_proven"}) ||
            !value.at("schema_version").is_number_integer() || value.at("schema_version") != 1 ||
            !group_fields(value.at("context"), {"bridge_instance", "document_token"}) ||
            !group_key(value.at("context").at("bridge_instance")) ||
            !group_key(value.at("context").at("document_token")) ||
            !group_key(value.at("project_key")) || !group_key(value.at("group_key")) ||
            !group_key(value.at("selected_binding_key")) ||
            !group_hex(value.at("approved_preview_fingerprint")) ||
            !group_index(value.at("group_track_index")) ||
            !group_index(value.at("selected_track_index")) ||
            value.at("selected_track_index") != observation.at("track_index") ||
            value.at("authority_origin") != "explicit_current_group_adoption" ||
            value.at("historical_identity_proven") != false)
            return false;
        const auto& keys = value.at("member_binding_keys");
        const auto& indices = value.at("member_track_indices");
        if (!keys.is_array() || !indices.is_array() || keys.empty() ||
            keys.size() > maximum_group_members || keys.size() != indices.size())
            return false;
        std::set<std::string> unique_keys;
        std::set<std::int32_t> unique_indices;
        bool selected = false;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (!group_key(keys[i]) || !group_index(indices[i]) ||
                !unique_keys.insert(keys[i].get<std::string>()).second ||
                !unique_indices.insert(indices[i].get<std::int32_t>()).second ||
                indices[i] == value.at("group_track_index"))
                return false;
            if (keys[i] == value.at("selected_binding_key")) {
                if (indices[i] != value.at("selected_track_index")) return false;
                selected = true;
            }
        }
        const auto project = value.at("project_key").get<std::string>();
        const auto binding = value.at("selected_binding_key").get<std::string>();
        const auto digest = managed_digest(value);
        return selected &&
               observation.at("track_tag") == "Sunny|" + project + "|" + binding + "|track" &&
               observation.at("clip_tag") == "Sunny|" + project + "|" + binding + "|clip" &&
               digest && observation.at("group_authority_fingerprint") == *digest;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

inline bool group_current_proof(const nlohmann::json& observation) {
    return observation.is_object() && observation.contains("group_authority") &&
           group_supplement_valid(observation);
}

// Close evidence to the outer receipt even when merely reading it. This checks
// no native objects and cannot replace the producer's private Group verifier.
inline bool group_context_matches(const nlohmann::json& observation,
                                  const ManagedBridgeContext& context,
                                  const std::string& project,
                                  const std::string& binding) {
    if (!group_supplement_valid(observation)) return false;
    if (!observation.contains("group_authority")) return true;
    const auto& proof = observation.at("group_authority");
    return proof.at("context") == nlohmann::json{{"bridge_instance", context.bridge_instance},
                                                 {"document_token", context.document_token}} &&
           proof.at("project_key") == project && proof.at("selected_binding_key") == binding;
}

inline bool group_touched_boundary(const nlohmann::json& observation,
                                   const ManagedBridgeContext& context,
                                   const std::string& project,
                                   const std::string& binding) {
    try {
        if (!group_context_matches(observation, context, project, binding)) return false;
        const auto& grouped = observation.at("manifest").at("track").at("is_grouped");
        return grouped.is_boolean() && (grouped == false || group_current_proof(observation));
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

} // namespace sunny::infrastructure::managed_detail
