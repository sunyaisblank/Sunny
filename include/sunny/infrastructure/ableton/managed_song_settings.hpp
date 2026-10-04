/** Explicit current Set-wide Song scalars. Finite Live12.4.x/12.3.x candidates. */
#pragma once
#include <expected>
#include <sunny/core/score/document.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {
struct ManagedSongSettings {
    double tempo = 120.0; // Quarter notes/minute, normalized by existing core semantics.
    int signature_numerator = 4;
    int signature_denominator = 4;
};
/** Single Immediate origin tempo/initial global meter only; returns precise unsupported cause. */
[[nodiscard]] std::expected<ManagedSongSettings, std::string>
plan_managed_song_settings(const sunny::core::Score& score);
[[nodiscard]] nlohmann::json managed_song_settings_to_json(const ManagedSongSettings& settings);
struct ManagedSongSettingsPreview {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    std::string preview_token;
    std::string preview_fingerprint;
    nlohmann::json approved_preview; // Small closed Song snapshot and Part guard hashes.
    nlohmann::json observation; // Actual full normal Part binding; no Song ownership inference.
};
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_song_settings_preview_request(const ManagedBridgeContext& context,
                                           const ManagedBindingReceipt& binding,
                                           const ManagedSongSettings& desired);
[[nodiscard]] sunny::core::Result<ManagedSongSettingsPreview> parse_managed_song_settings_preview(
    const LomRequest& request, const ManagedBridgeContext& context, const nlohmann::json& value);
/** Fresh current Set snapshot with existing idle/automation/Part guard checks;
 * no preview token, grant, journal or authority refresh. */
[[nodiscard]] sunny::core::Result<LomRequest> make_managed_song_settings_inspection_request(
    const ManagedBridgeContext&, const ManagedBindingReceipt&, const ManagedSongSettings& desired);
[[nodiscard]] sunny::core::Result<nlohmann::json> parse_managed_song_settings_inspection(
    const LomRequest&, const ManagedBridgeContext&, const nlohmann::json& actual);
/** Owning product must compare real Score projection and fence this exact intent before send. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_song_settings_request(const ManagedBridgeContext& context,
                                   const std::string& operation,
                                   const ManagedSongSettingsPreview& preview);
namespace managed_song_detail {
[[nodiscard]] bool request_valid(std::string_view method, const nlohmann::json& payload);
[[nodiscard]] bool preview_valid(const nlohmann::json& preview);
[[nodiscard]] bool result_matches_request(const nlohmann::json& payload,
                                          const nlohmann::json& result);
/** New-method-only no-op exception; validates full result before deciding mutation-start truth. */
[[nodiscard]] bool acknowledged_native_start_valid(const nlohmann::json& payload,
                                                   const nlohmann::json& result,
                                                   bool native_started);
[[nodiscard]] bool partial_valid(const nlohmann::json& payload, const nlohmann::json& journal);
} // namespace managed_song_detail
} // namespace sunny::infrastructure
