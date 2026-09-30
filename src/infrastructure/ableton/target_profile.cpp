/**
 * @file target_profile.cpp
 * @brief Ableton target-profile derivation and trust-boundary validation
 */

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>
#include <sunny/infrastructure/ableton/target_profile.hpp>
#include <utility>

namespace sunny::infrastructure {

using sunny::core::ErrorCode;
using sunny::core::Result;

namespace {

bool exact_fields(const nlohmann::json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

CapabilityState capability_state_from_json(const nlohmann::json& value) {
    if (!value.is_string()) return CapabilityState::Unknown;
    const auto state = value.get<std::string>();
    if (state == "available") return CapabilityState::Available;
    if (state == "unavailable") return CapabilityState::Unavailable;
    return CapabilityState::Unknown;
}

bool valid_u16(const nlohmann::json& value) {
    return value.is_number_unsigned() &&
           value.get<std::uint64_t>() <= std::numeric_limits<std::uint16_t>::max();
}

bool valid_capability_state(const nlohmann::json& value) {
    if (!value.is_string()) return false;
    const auto state = value.get<std::string>();
    return state == "available" || state == "unavailable" || state == "unknown";
}

bool valid_capability_state(CapabilityState state) {
    switch (state) {
    case CapabilityState::Unavailable:
    case CapabilityState::Available:
    case CapabilityState::Unknown:
        return true;
    }
    return false;
}

bool has_exact_capability(const nlohmann::json& capabilities,
                          const char* name,
                          CapabilityState expected) {
    return capabilities.contains(name) &&
           capability_state_from_json(capabilities.at(name)) == expected;
}

} // namespace

bool AbletonVersion::at_least(std::uint16_t required_major,
                              std::uint16_t required_minor,
                              std::uint16_t required_bugfix) const {
    if (major != required_major) return major > required_major;
    if (minor != required_minor) return minor > required_minor;
    return bugfix >= required_bugfix;
}

std::string capability_state_name(CapabilityState state) {
    switch (state) {
    case CapabilityState::Unavailable:
        return "unavailable";
    case CapabilityState::Available:
        return "available";
    case CapabilityState::Unknown:
        return "unknown";
    }
    return "unknown";
}

AbletonTargetProfile modeled_target_profile(AbletonVersion version) {
    AbletonTargetProfile profile;
    profile.live_version = std::move(version);
    profile.clip_note_insertion = profile.live_version.at_least(11, 0)
                                      ? CapabilityState::Available
                                      : CapabilityState::Unavailable;
    profile.native_device_insertion = profile.live_version.at_least(12, 3)
                                          ? CapabilityState::Available
                                          : CapabilityState::Unavailable;
    return profile;
}

Result<void> validate_target_profile(const AbletonTargetProfile& profile) {
    const auto version_prefix = std::to_string(profile.live_version.major) + "." +
                                std::to_string(profile.live_version.minor) + "." +
                                std::to_string(profile.live_version.bugfix);
    const auto expected = modeled_target_profile(profile.live_version);
    if (!profile.live_version.version_string.starts_with(version_prefix) ||
        profile.bridge_protocol_version != SUNNY_BRIDGE_PROTOCOL_VERSION ||
        profile.adapter_name != "Sunny Remote Script" ||
        profile.adapter_runtime != "control_surface_python" ||
        profile.adapter_contract != "version_coupled_private" ||
        profile.clip_note_insertion != expected.clip_note_insertion ||
        profile.native_device_insertion != expected.native_device_insertion ||
        profile.automation_envelope_authoring != CapabilityState::Unavailable ||
        profile.group_track_creation != CapabilityState::Unavailable ||
        profile.arbitrary_browser_loading != CapabilityState::Unavailable ||
        profile.structural_snapshot != CapabilityState::Available ||
        !valid_capability_state(profile.max_for_live)) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
    return {};
}

Result<AbletonTargetProfile> target_profile_from_json(const nlohmann::json& value) {
    try {
        if (!exact_fields(value, {"bridge_protocol_version", "adapter", "live", "capabilities"}) ||
            !value.at("bridge_protocol_version").is_number_unsigned() ||
            value.at("bridge_protocol_version").get<std::uint64_t>() !=
                SUNNY_BRIDGE_PROTOCOL_VERSION ||
            !value.contains("adapter") || !value.at("adapter").is_object() ||
            !value.contains("live") || !value.at("live").is_object() ||
            !value.contains("capabilities") || !value.at("capabilities").is_object()) {
            return std::unexpected(ErrorCode::ProtocolError);
        }

        const auto& adapter = value.at("adapter");
        const auto& live = value.at("live");
        const auto& capabilities = value.at("capabilities");
        if (!exact_fields(adapter, {"name", "runtime", "contract"}) ||
            !adapter.at("name").is_string() || !adapter.at("runtime").is_string() ||
            !adapter.at("contract").is_string() || !exact_fields(live, {"version"})) {
            return std::unexpected(ErrorCode::ProtocolError);
        }

        const auto& version = live.at("version");
        if (!exact_fields(version, {"major", "minor", "bugfix", "string"}) ||
            !valid_u16(version.at("major")) || !valid_u16(version.at("minor")) ||
            !valid_u16(version.at("bugfix")) || !version.at("string").is_string() ||
            !exact_fields(capabilities,
                          {"clip_add_new_notes",
                           "track_insert_device_native",
                           "automation_envelope_authoring",
                           "group_track_creation",
                           "arbitrary_browser_loading",
                           "structural_snapshot",
                           "max_for_live"})) {
            return std::unexpected(ErrorCode::ProtocolError);
        }

        AbletonVersion live_version{
            version.at("major").get<std::uint16_t>(),
            version.at("minor").get<std::uint16_t>(),
            version.at("bugfix").get<std::uint16_t>(),
            version.at("string").get<std::string>(),
        };
        const auto version_prefix = std::to_string(live_version.major) + "." +
                                    std::to_string(live_version.minor) + "." +
                                    std::to_string(live_version.bugfix);
        if (!live_version.version_string.starts_with(version_prefix)) {
            return std::unexpected(ErrorCode::ProtocolError);
        }
        auto profile = modeled_target_profile(std::move(live_version));
        profile.adapter_name = adapter.at("name").get<std::string>();
        profile.adapter_runtime = adapter.at("runtime").get<std::string>();
        profile.adapter_contract = adapter.at("contract").get<std::string>();

        // The bridge is part of this repository.  Reject a peer that claims a
        // different host contract, or capability flags inconsistent with the
        // documented version thresholds, instead of trusting self-description.
        if (profile.adapter_name != "Sunny Remote Script" ||
            profile.adapter_runtime != "control_surface_python" ||
            profile.adapter_contract != "version_coupled_private" ||
            !has_exact_capability(
                capabilities, "clip_add_new_notes", profile.clip_note_insertion) ||
            !has_exact_capability(
                capabilities, "track_insert_device_native", profile.native_device_insertion) ||
            !has_exact_capability(
                capabilities, "automation_envelope_authoring", CapabilityState::Unavailable) ||
            !has_exact_capability(
                capabilities, "group_track_creation", CapabilityState::Unavailable) ||
            !has_exact_capability(
                capabilities, "arbitrary_browser_loading", CapabilityState::Unavailable) ||
            !has_exact_capability(
                capabilities, "structural_snapshot", CapabilityState::Available) ||
            !capabilities.contains("max_for_live") ||
            !valid_capability_state(capabilities.at("max_for_live"))) {
            return std::unexpected(ErrorCode::ProtocolError);
        }

        profile.max_for_live = capability_state_from_json(capabilities.at("max_for_live"));
        auto valid = validate_target_profile(profile);
        if (!valid) return std::unexpected(valid.error());
        return profile;
    } catch (const nlohmann::json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}

nlohmann::json target_profile_to_json(const AbletonTargetProfile& profile) {
    return {
        {"bridge_protocol_version", profile.bridge_protocol_version},
        {"adapter",
         {{"name", profile.adapter_name},
          {"runtime", profile.adapter_runtime},
          {"contract", profile.adapter_contract}}},
        {"live",
         {{"version",
           {{"major", profile.live_version.major},
            {"minor", profile.live_version.minor},
            {"bugfix", profile.live_version.bugfix},
            {"string", profile.live_version.version_string}}}}},
        {"capabilities",
         {{"clip_add_new_notes", capability_state_name(profile.clip_note_insertion)},
          {"track_insert_device_native", capability_state_name(profile.native_device_insertion)},
          {"automation_envelope_authoring",
           capability_state_name(profile.automation_envelope_authoring)},
          {"group_track_creation", capability_state_name(profile.group_track_creation)},
          {"arbitrary_browser_loading", capability_state_name(profile.arbitrary_browser_loading)},
          {"structural_snapshot", capability_state_name(profile.structural_snapshot)},
          {"max_for_live", capability_state_name(profile.max_for_live)}}}};
}

} // namespace sunny::infrastructure
