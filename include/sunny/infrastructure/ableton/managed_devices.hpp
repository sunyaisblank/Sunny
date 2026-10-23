/** Closed finite native-device requests on an existing retained Part binding. */
#pragma once

#include <span>
#include <sunny/infrastructure/ableton/managed_realization.hpp>
#include <sunny/infrastructure/ableton/native_units.hpp>

namespace sunny::infrastructure {

inline constexpr std::size_t SUNNY_MANAGED_MAX_DEVICES = 16;
inline constexpr std::size_t SUNNY_MANAGED_MAX_DEVICE_TARGETS = 32;
inline constexpr std::size_t SUNNY_MANAGED_MAX_DEVICE_PARAMETERS = 512;

enum class ManagedNativeDevice { Drift, Utility, EqEight };

struct ManagedDevicePhysicalIntent {
    std::string capability_id;
    double target = 0.0;
    double tolerance = 0.0;
};

/** Append one owned source/effect to a previously retained native chain.
 * Requires Drift first on an empty managed MIDI Track, then Utility/EQ Eight.
 * Every target is resolved from actual native descriptors after insertion and
 * before any parameter setter. Late failure retains the inserted native object;
 * it never deletes, retries, or claims edition availability/opaque DSP state. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_insert_request(const ManagedBridgeContext& context,
                                   const std::string& operation_id,
                                   const ManagedBindingReceipt& binding,
                                   const std::string& device_key,
                                   ManagedNativeDevice device,
                                   std::span<const ManagedDevicePhysicalIntent> physical_intents);

/** Revise only explicit physical targets on an actual retained logical device.
 * Request guards both the current Clip/Track baseline and separately hashed
 * owned Device/parameter cohort. No generic index or internal-value setter is
 * admitted. Native formatted readback must meet each declared display tolerance. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_update_request(const ManagedBridgeContext& context,
                                   const std::string& operation_id,
                                   const ManagedBindingReceipt& binding,
                                   const std::string& device_key,
                                   ManagedNativeDevice device,
                                   std::span<const ManagedDevicePhysicalIntent> physical_intents);

struct ManagedDeviceModeIntent {
    std::string capability_id;
    std::string label;
};
struct ManagedDevicePropertyIntent {
    std::string property;
    std::string label;
};
/** A separately fenced finite mode phase on an actual retained native effect.
 * Exact native enum labels are admitted before setters; EQ Stereo uses the
 * documented native global_mode property. Complete observed EQ edit/oversample
 * evidence is required for new mode authority. Physical resolution follows
 * this actual ACK rather than a speculative future-mode candidate. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_mode_request(const ManagedBridgeContext& context,
                                 const std::string& operation_id,
                                 const ManagedBindingReceipt& binding,
                                 const std::string& device_key,
                                 ManagedNativeDevice device,
                                 std::span<const ManagedDeviceModeIntent> enum_intents,
                                 std::span<const ManagedDevicePropertyIntent> property_intents);

struct ManagedDeviceAdoptionSelection {
    std::string device_key;
    std::uint32_t chain_index = 0;
    ManagedNativeDevice device = ManagedNativeDevice::Drift;
    std::vector<ManagedDevicePhysicalIntent> physical_intents;
    std::vector<ManagedDeviceModeIntent> enum_intents{};
    std::vector<ManagedDevicePropertyIntent> property_intents{};
    bool authored_bypass = false;
};

struct ManagedDeviceAdoptionPreview {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    std::string preview_token;
    std::string preview_fingerprint;
    /** Exact current native observation and explicit selections, never old
     * logical ownership. Physical targets already match current readback. */
    nlohmann::json approved_preview;
};

/** Read-only preview of the complete current finite Drift/Utility/EQ8 chain.
 * An explicitly empty chain grants prospective owned source insertion after
 * approval, with no parameter formatter calls or setters.
 * Existing Clip ownership is retained, but prior device keys never confer this
 * new authority. Unknown plugins/racks remain preserved without device writes. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_preview_request(const ManagedBridgeContext& context,
                                    const ManagedBindingReceipt& binding,
                                    std::span<const ManagedDeviceAdoptionSelection> selections);
[[nodiscard]] sunny::core::Result<ManagedDeviceAdoptionPreview> parse_managed_device_preview(
    const LomRequest& request, const ManagedBridgeContext& context, const nlohmann::json& response);

/** Fresh closed physical/mode readback of the complete current finite chain.
 * No preview token, grant, retained baseline, operation journal or native write
 * is created. The actual binding and formatter population must remain guarded. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_inspection_request(const ManagedBridgeContext& context,
                                       const ManagedBindingReceipt& binding,
                                       std::span<const ManagedDeviceAdoptionSelection> selections);
[[nodiscard]] sunny::core::Result<nlohmann::json> parse_managed_device_inspection(
    const LomRequest& request, const ManagedBridgeContext& context, const nlohmann::json& response);

/** A separately fenced explicit adoption of CURRENT objects, without native
 * setters or transfer of historical ownership. Grants finite controls and
 * AppendOwnedChain on these CURRENT handles. Grant requires exact native
 * handles/state and repeated formatter evidence still matching the preview. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_device_adoption_request(const ManagedBridgeContext& context,
                                     const std::string& operation_id,
                                     const ManagedDeviceAdoptionPreview& preview);

} // namespace sunny::infrastructure
