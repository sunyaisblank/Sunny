/** Internal finite native routing evidence; names/hashes never grant target
 * identity. */
#pragma once
#include <sunny/infrastructure/ableton/managed_routing.hpp>
namespace sunny::infrastructure::managed_routing_detail {
inline constexpr std::string_view candidates_method = "sunny_managed_routing_candidates";
inline constexpr std::string_view preview_method = "sunny_managed_preview_routing";
inline constexpr std::string_view group_preview_method = "sunny_managed_preview_group";
inline constexpr std::string_view apply_method = "sunny_managed_apply_routing";
[[nodiscard]] bool intent_valid(const nlohmann::json&);
[[nodiscard]] bool frame_valid(const nlohmann::json&);
[[nodiscard]] bool observed_state_preserved(const nlohmann::json& preview,
                                            const nlohmann::json& after);
} // namespace sunny::infrastructure::managed_routing_detail
