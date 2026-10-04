/**
 * @file target_profile.hpp
 * @brief Explicit Ableton Live target assumptions and capabilities
 *
 * A transport connection proves reachability, not that the connected Live
 * version can execute every compiler operation.  This model records the
 * externally observed host version, the version-coupled bridge contract, and
 * capability states used to preflight deployment.
 */

#pragma once

#include <cstdint>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <sunny/core/types/music_types.hpp>
#include <sunny/infrastructure/ableton/bridge_contract.hpp>

namespace sunny::infrastructure {

enum class CapabilityState : std::uint8_t { Unavailable, Available, Unknown };

struct AbletonVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t bugfix = 0;
    std::string version_string;

    [[nodiscard]] bool at_least(std::uint16_t required_major,
                                std::uint16_t required_minor,
                                std::uint16_t required_bugfix = 0) const;
};

struct AbletonTargetProfile {
    AbletonVersion live_version;
    std::uint32_t bridge_protocol_version = SUNNY_BRIDGE_PROTOCOL_VERSION;
    std::string adapter_name = "Sunny Remote Script";
    std::string adapter_runtime = "control_surface_python";
    std::string adapter_contract = "version_coupled_private";

    CapabilityState clip_note_insertion = CapabilityState::Unknown;
    CapabilityState native_device_insertion = CapabilityState::Unknown;
    CapabilityState automation_envelope_authoring = CapabilityState::Unavailable;
    CapabilityState group_track_creation = CapabilityState::Unavailable;
    CapabilityState arbitrary_browser_loading = CapabilityState::Unavailable;
    CapabilityState max_for_live = CapabilityState::Unknown;
    CapabilityState structural_snapshot = CapabilityState::Available;
};

/** Model version floors from the Max LOM; Python runtime and licensing need host evidence. */
[[nodiscard]] AbletonTargetProfile modeled_target_profile(AbletonVersion version);

/** Validate a profile supplied by any transport, including non-wire implementations. */
[[nodiscard]] sunny::core::Result<void>
validate_target_profile(const AbletonTargetProfile& profile);

/** Parse and adversarially validate a bridge target-profile payload. */
[[nodiscard]] sunny::core::Result<AbletonTargetProfile>
target_profile_from_json(const nlohmann::json& value);

/** Stable JSON representation used by MCP results and tests. */
[[nodiscard]] nlohmann::json target_profile_to_json(const AbletonTargetProfile& profile);

[[nodiscard]] std::string capability_state_name(CapabilityState state);

} // namespace sunny::infrastructure
