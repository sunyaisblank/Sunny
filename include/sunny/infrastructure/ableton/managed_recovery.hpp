/** Explicit current-object adoption; preview and saved receipts confer no
 * authority. */
#pragma once
#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {

inline constexpr int SUNNY_MANAGED_ADOPTION_SCHEMA_VERSION = 1;
inline constexpr std::size_t SUNNY_MANAGED_ADOPTION_MAX_PREVIEWS = 256;

struct ManagedAdoptionPreview {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    std::string preview_token;
    std::string preview_fingerprint;
    // Closed observed preview, including actual names/indices/full note IDs.
    // This value carries no mutation authority or historical native identity.
    nlohmann::json evidence;
};

/** Selector is closed {track_index,slot_index} or {track_tag,clip_tag}.
 * This initial subset preserves actual Sunny names and never renames native
 * objects. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_adoption_preview_request(const ManagedBridgeContext& context,
                                      const std::string& project_key,
                                      const std::string& binding_key,
                                      const nlohmann::json& selector);

[[nodiscard]] sunny::core::Result<ManagedAdoptionPreview>
preview_managed_adoption(const ManagedBridgeContext& context,
                         const std::string& project_key,
                         const std::string& binding_key,
                         const nlohmann::json& selector,
                         LomTransport& transport);

[[nodiscard]] sunny::core::Result<ManagedAdoptionPreview>
managed_adoption_preview_from_json(const nlohmann::json& value);

/** Explicit approval of this exact preview. Product must first check its owning
 * Score projection, persist the may-have-sent fence, and consume its one send
 * permit. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_adoption_request(const ManagedBridgeContext& context,
                              const std::string& operation_id,
                              const ManagedAdoptionPreview& preview);

namespace managed_detail {
// Small shared hooks for LomProtocol and managed journal
// classification/closure.
[[nodiscard]] bool adoption_preview_request_valid(const nlohmann::json& value);
[[nodiscard]] bool adoption_request_valid(const nlohmann::json& value);
[[nodiscard]] bool adoption_acknowledgement_valid(const nlohmann::json& intent,
                                                  const nlohmann::json& result);
} // namespace managed_detail
} // namespace sunny::infrastructure
