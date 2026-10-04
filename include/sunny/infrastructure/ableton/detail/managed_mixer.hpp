#pragma once
#include <sunny/infrastructure/ableton/managed_mixer.hpp>
namespace sunny::infrastructure::managed_mixer_detail {
inline constexpr std::string_view preview_method = "sunny_managed_preview_static_mixer";
inline constexpr std::string_view inspection_method = "sunny_managed_inspect_static_mixer";
inline constexpr std::string_view adopt_method = "sunny_managed_adopt_static_mixer";
inline constexpr std::string_view update_method = "sunny_managed_update_static_mixer";
[[nodiscard]] bool desired_valid(const nlohmann::json&, const nlohmann::json& domains);
[[nodiscard]] bool snapshot_valid(const nlohmann::json&,
                                  const nlohmann::json& metadata,
                                  bool require_envelope_absence = true);
[[nodiscard]] bool preview_valid(const nlohmann::json&);
[[nodiscard]] bool request_valid(std::string_view, const nlohmann::json&);
[[nodiscard]] bool
result_matches_request(std::string_view, const nlohmann::json&, const nlohmann::json&);
[[nodiscard]] bool acknowledged_native_start_valid(std::string_view,
                                                   const nlohmann::json&,
                                                   const nlohmann::json&,
                                                   bool);
[[nodiscard]] bool partial_valid(const nlohmann::json&, const nlohmann::json& journal);
[[nodiscard]] bool untouched(const nlohmann::json& before,
                             const nlohmann::json& after,
                             const nlohmann::json& returned_fields);
} // namespace sunny::infrastructure::managed_mixer_detail
