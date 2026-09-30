/**
 * @file target_snapshot.hpp
 * @brief Read-only structural evidence for guarded Ableton deployment
 */

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>

namespace sunny::infrastructure {

/**
 * A main-thread observation of the selected Live Set state on which a deployment
 * plan depends. `song_state` deliberately excludes playhead and UI selection
 * state, but includes Song/Clip runtime predicates, current tempo/signature, Scene launch
 * overrides, Song scale state, the tractable identity/pseudo-octave subset of global tuning,
 * tracks, version-coupled Arrangement Clip and Take Lane counts, exact occupied Session Clip slots,
 * Track Session/Arrangement indices, conditional hold and channel-resolved momentary meters, Clip
 * structural/launch/envelope/groove/runtime state, device identities and
 * reported latencies, bounded mixer-parameter value/domain/operability state, Return Track
 * mute/solo gates, master devices, and cue points. The tuning
 * dictionaries, per-track bypass, instrument support, delay-compensation configuration, Track
 * Delay, and complete render-path latency remain explicit residuals.
 */
struct AbletonTargetSnapshot {
    std::uint32_t schema_version = SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION;
    AbletonTargetProfile target_profile;
    nlohmann::json song_state = nlohmann::json::object();
};

/** Parse and strictly validate a snapshot supplied by the Live peer. */
[[nodiscard]] sunny::core::Result<AbletonTargetSnapshot>
target_snapshot_from_json(const nlohmann::json& value);

/** Stable representation used for exact plan precondition comparison. */
[[nodiscard]] nlohmann::json target_snapshot_to_json(const AbletonTargetSnapshot& snapshot);

/**
 * Structural equality, including the observed target profile. Volatile track
 * meter levels are excluded: they change with playback, not with structure.
 */
[[nodiscard]] bool equivalent_target_snapshot(const AbletonTargetSnapshot& lhs,
                                              const AbletonTargetSnapshot& rhs);

} // namespace sunny::infrastructure
