/** Internal closed device supplement and immutable request/ACK joins. */
#pragma once

#include <nlohmann/json.hpp>
#include <string_view>

namespace sunny::infrastructure::managed_device_detail {

[[nodiscard]] bool device_identity_valid(const nlohmann::json& identity);
/** No device supplement is legacy-valid; provided supplements require both
 * exact device identity and its typed canonical SM1 fingerprint. */
[[nodiscard]] bool device_supplement_valid(const nlohmann::json& observation);
[[nodiscard]] bool device_request_valid(std::string_view method, const nlohmann::json& payload);
/** Caller additionally validates the ordinary before/after binding observations
 * through the existing managed manifest validator. This function validates the
 * complete device proof and joins it to the authoritative prepared request. */
[[nodiscard]] bool device_result_matches_request(std::string_view method,
                                                 const nlohmann::json& payload,
                                                 const nlohmann::json& result);
[[nodiscard]] bool device_preview_valid(const nlohmann::json& preview);
[[nodiscard]] bool device_adoption_result_matches_request(const nlohmann::json& payload,
                                                          const nlohmann::json& result);

} // namespace sunny::infrastructure::managed_device_detail
