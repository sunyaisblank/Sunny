#pragma once
#include <array>
#include <sunny/infrastructure/ableton/detail/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_song_settings.hpp>
namespace sunny::infrastructure::managed_song_detail {
[[nodiscard]] bool observation_closes(const nlohmann::json& preview,
                                      const nlohmann::json& observation);
inline constexpr std::string_view preview_method = "sunny_managed_preview_song_settings";
inline constexpr std::string_view apply_method = "sunny_managed_apply_song_settings";
inline constexpr std::array<std::string_view, 3> scalar_fields{
    "signature_numerator", "signature_denominator", "tempo"};
inline constexpr std::array<std::string_view, 16> flag_fields{
    "is_playing",
    "is_counting_in",
    "record_mode",
    "session_record",
    "session_automation_record",
    "arrangement_overdub",
    "overdub",
    "is_ableton_link_enabled",
    "is_ableton_link_start_stop_sync_enabled",
    "tempo_follower_enabled",
    "nudge_down",
    "nudge_up",
    "back_to_arranger",
    "re_enable_automation_enabled",
    "loop",
    "metronome"};
[[nodiscard]] bool snapshot_valid(const nlohmann::json& value);
[[nodiscard]] bool settings_valid(const nlohmann::json& value, bool actual = false);
[[nodiscard]] nlohmann::json changed_fields(const nlohmann::json& before,
                                            const nlohmann::json& desired);
[[nodiscard]] bool untouched(const nlohmann::json& before, const nlohmann::json& after);
} // namespace sunny::infrastructure::managed_song_detail
