/** Pure reconstruction of finite saved Device intent; grants no native authority. */
#pragma once

#include <sunny/infrastructure/ableton/managed_devices.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>

namespace sunny::infrastructure {

struct ManagedDeviceRecoveryScope {
    std::string workspace_namespace;
    sunny::core::ScoreId score_id;
    sunny::core::PartId part_id;
    /// Retained musical history ceiling; never a caller-supplied current IR projection.
    std::string through_attempt_id;
    /// Adoption repeats the precise snapshot returned by preview.
    std::optional<std::string> expected_device_history_attempt;
};

struct ManagedDeviceRecoveryError {
    std::string reason;
    std::string diagnostic;
    std::optional<std::string> attempt_id;
};

struct ManagedDeviceRecoveryResidual {
    std::string device_key;
    /// Historical targets deliberately omitted from an inactive effect preview.
    std::vector<ManagedDevicePhysicalIntent> bypassed_physical_intents;
    bool bypassed_physical_intents_unknown = false;
    /// Saved unique labels used as read-only guards, never synthesized defaults.
    std::vector<ManagedDeviceModeIntent> observed_mode_guards;
    /// Drift voice properties / EQ edit and oversample observations. These are
    /// residual evidence, not new property intent or a claim of DSP identity.
    nlohmann::json observed_modes;
    bool opaque_state_observed = false;
};

struct ManagedDeviceRecoveryPlan {
    std::string device_history_attempt;
    ManagedBridgeContext context;
    std::string device_identity_fingerprint;
    std::vector<ManagedDeviceAdoptionSelection> selections;
    std::vector<std::string> contributing_attempt_ids;
    std::vector<ManagedDeviceRecoveryResidual> residuals;
};

/// Validate typed immutable same-namespace/Score/Part history, retain the latest
/// full saved cohort through the selected musical source, and fold successful
/// physical/mode intent with exact Device joins. No current IR, Live calls,
/// filesystem writes, normalized-value conversion, or new Store schema.
/// Existing current-object preview/adoption must still verify actual native
/// objects and formatted values before any fresh authority is granted.
[[nodiscard]] std::expected<ManagedDeviceRecoveryPlan, ManagedDeviceRecoveryError>
fold_managed_device_recovery(const std::map<std::string, RealizationStoredAttempt>& history,
                             const ManagedDeviceRecoveryScope& scope);

} // namespace sunny::infrastructure
