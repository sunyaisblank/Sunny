/**
 * @file live_capabilities.hpp
 * @brief Finite source-candidate native Live parameter registry and pure preflight
 *
 * Matching observed descriptors produces an internal-domain candidate. It does
 * not prove ownership, edition availability, Python ABI, physical-unit
 * conversion, automation availability, or saved/reopened persistence.
 */

#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace sunny::core {

inline constexpr std::uint32_t LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION = 3;

struct LiveNativeVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t patch = 0;
    auto operator<=>(const LiveNativeVersion&) const = default;
};

struct LiveNativeRegistryProvenance {
    std::uint32_t registry_version = LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION;
    LiveNativeVersion public_lom_reference_version{12, 4, 5};
    /** Registry candidate scope is 12.3.x/12.4.x within this version type's domain.
     *  Patch coverage is descriptor guarded; the documentation patch is provenance,
     *  not a verified Python ABI boundary. Future minor versions require review. */
    LiveNativeVersion first_candidate_version{12, 3, 0};
    LiveNativeVersion last_candidate_version{12, 4, UINT16_MAX};
    std::string public_lom_reference;
    std::string official_manual_reference;
    std::string python_source_commit;
    std::vector<std::string> python_source_locations;
    /** No exact version/edition/OS has been qualified by this pure registry. */
    bool host_qualified = false;
};

enum class LiveNativePhysicalUnit : std::uint8_t {
    Decibels,
    Hertz,
    QualityFactor,
    Percent,
    StereoBalance,
    Milliseconds,
};

enum class LiveNativeParameterKind : std::uint8_t { Continuous, Quantized };
enum class LiveNativeModeRequirement : std::uint8_t {
    UtilityStereo,
    Eq8Stereo,
    Eq8StereoBandOn,
    DriftVoice,
    DriftFilter,
    OwnedEffectSetup,
};

struct LiveNativeParameterCapability {
    /** A registry identifier, not a Timbre/Mix path or a native object identity. */
    std::string id;
    std::string device_browser_name;
    std::string device_class_name;
    std::string parameter_original_name;
    LiveNativeParameterKind kind = LiveNativeParameterKind::Continuous;
    std::optional<std::uint32_t> expected_enum_items;
    std::optional<LiveNativePhysicalUnit> physical_unit;
    LiveNativeModeRequirement mode = LiveNativeModeRequirement::UtilityStereo;
    /** Native EQ Eight band number1..8;0 means no band. */
    std::uint8_t band = 0;
    std::string python_source_location;
};

/** A read-only observation of one actual member of a native Device.parameters collection. */
struct LiveNativeParameterProbe {
    std::optional<std::string> name;
    std::optional<std::string> original_name;
    std::optional<bool> actual_device_parameter;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> value;
    std::optional<bool> is_quantized;
    std::optional<bool> is_enabled;
    std::optional<std::uint8_t> state;
    std::optional<std::uint8_t> automation_state;
    std::optional<std::vector<std::string>> value_items;
};

struct LiveNativeDeviceProbe {
    LiveNativeVersion version;
    /** Edition is retained as observation, without claiming a licensing matrix. */
    std::optional<std::string> edition;
    std::optional<std::string> class_name;
    std::optional<std::uint32_t> chain_index;
    std::optional<std::uint8_t> device_type;
    std::optional<bool> is_active;
    std::optional<bool> can_have_chains;
    /** Required to establish unique names and rule out omitted aliases. */
    bool parameter_population_observed = false;
    std::vector<LiveNativeParameterProbe> parameters;
    /** Device-level native properties, e.g. Eq8Device.global_mode; absent stays unknown. */
    std::map<std::string, std::int32_t> integer_properties;
    /** Advertised native property labels, e.g. Drift voice_mode_list. */
    std::map<std::string, std::vector<std::string>> string_list_properties;
};

struct LiveNativeInternalValue {
    double value = 0.0;
};

struct LiveNativePhysicalValue {
    LiveNativePhysicalUnit unit = LiveNativePhysicalUnit::Decibels;
    double value = 0.0;
};

struct LiveNativeEnumChoice {
    /** Exact advertised label; no locale translation or fixed filter-type ordinal is inferred. */
    std::string label;
};

using LiveNativeParameterIntent =
    std::variant<LiveNativeInternalValue, LiveNativePhysicalValue, LiveNativeEnumChoice>;

enum class LiveNativePreflightDisposition : std::uint8_t {
    CandidateReady,
    UnknownRegistryCoverage,
    UnknownCapability,
    ObservationUnavailable,
    DeviceMismatch,
    WrongChainOrder,
    AmbiguousParameter,
    ParameterMismatch,
    InvalidDomain,
    InactiveParameter,
    ExistingAutomation,
    UnsupportedMode,
    UnsupportedUnitMapping,
    UnknownEnumChoice,
    InvalidIntent,
    ValueOutsideDomain,
};

enum class LiveNativeCandidateQualification : std::uint8_t { RuntimeDescriptorMatchedCandidate };
enum class LiveNativeValueMapping : std::uint8_t { InternalIdentity, AdvertisedEnumIndex };

struct LiveNativeParameterCandidate {
    std::uint32_t registry_version = LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION;
    std::string capability_id;
    std::string device_class_name;
    std::uint32_t device_chain_index = 0;
    std::uint32_t observed_parameter_index = 0;
    std::string parameter_original_name;
    double minimum = 0.0;
    double maximum = 0.0;
    double internal_value = 0.0;
    LiveNativeValueMapping mapping = LiveNativeValueMapping::InternalIdentity;
    LiveNativeCandidateQualification qualification =
        LiveNativeCandidateQualification::RuntimeDescriptorMatchedCandidate;
    bool host_qualified = false;
    /** Continuous candidate shape only; these do not claim runtime envelope availability. */
    bool continuous_internal_step_candidate = false;
    bool envelope_runtime_qualified = false;
    bool persistence_verified = false;
};

struct LiveNativeMappingPreflight {
    LiveNativePreflightDisposition disposition =
        LiveNativePreflightDisposition::ObservationUnavailable;
    std::optional<LiveNativeParameterCandidate> candidate;
    std::string diagnostic;
};

[[nodiscard]] const LiveNativeRegistryProvenance& live_native_registry_provenance();
[[nodiscard]] std::span<const LiveNativeParameterCapability> live_native_parameter_registry();

/**
 * Pure, read-only admission for a finite registry entry against actual descriptors.
 *
 * The caller must obtain probes from Live, retain their identity/provenance, and
 * recheck managed ownership and drift immediately before writing. Missing data
 * is unknown, never a default mode. This function performs no host operations.
 */
[[nodiscard]] LiveNativeMappingPreflight
preflight_live_native_parameter(const std::string& capability_id,
                                const LiveNativeParameterIntent& intent,
                                const LiveNativeDeviceProbe& observed,
                                std::uint32_t expected_chain_index);

} // namespace sunny::core
