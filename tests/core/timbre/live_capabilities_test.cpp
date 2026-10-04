/**
 * @file live_capabilities_test.cpp
 * @brief Literal native registry/preflight checks against synthetic observed descriptors
 *
 * Fixture scales, names and enum order do not qualify a Live host or unit map.
 */

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <set>
#include <sunny/core/timbre/live_capabilities.hpp>
#include <utility>

using namespace sunny::core;

namespace {

using Disposition = LiveNativePreflightDisposition;

LiveNativeParameterProbe
continuous(std::string name, double minimum = 0.0, double maximum = 1.0, double value = 0.5) {
    LiveNativeParameterProbe parameter;
    parameter.name = name;
    parameter.original_name = std::move(name);
    parameter.actual_device_parameter = true;
    parameter.minimum = minimum;
    parameter.maximum = maximum;
    parameter.value = value;
    parameter.is_quantized = false;
    parameter.is_enabled = true;
    parameter.state = 0;
    parameter.automation_state = 0;
    return parameter;
}

LiveNativeParameterProbe
enumeration(std::string name, std::vector<std::string> items, double value = 0.0) {
    auto parameter = continuous(std::move(name), 0.0, static_cast<double>(items.size() - 1), value);
    parameter.is_quantized = true;
    parameter.value_items = std::move(items);
    return parameter;
}

LiveNativeDeviceProbe utility() {
    LiveNativeDeviceProbe device;
    device.version = {12, 3, 5};
    device.class_name = "StereoGain";
    device.chain_index = 3;
    device.device_type = 2;
    device.is_active = true;
    device.can_have_chains = false;
    device.parameter_population_observed = true;
    device.parameters = {enumeration("Device On", {"Off", "On"}, 1.0),
                         continuous("Gain"),
                         continuous("Balance", -1.0, 1.0, 0.0),
                         continuous("Stereo Width", 0.0, 4.0, 1.0),
                         enumeration("Channel Mode", {"Left", "Stereo", "Right", "Swap"}, 1.0),
                         enumeration("Mono", {"On", "Off"}, 1.0),
                         enumeration("Mute", {"Off", "On"}),
                         enumeration("Left Inv", {"Off", "On"}),
                         enumeration("Right Inv", {"Off", "On"}),
                         enumeration("Bass Mono", {"Off", "On"}),
                         continuous("Bass Freq"),
                         enumeration("DC Filter", {"Off", "On"})};
    return device;
}

LiveNativeDeviceProbe eq8() {
    auto device = utility();
    device.class_name = "Eq8";
    device.chain_index = 2;
    device.parameters = {enumeration("Device On", {"Off", "On"}, 1.0)};
    for (int band = 1; band <= 8; ++band) {
        const auto prefix = std::to_string(band) + " ";
        device.parameters.push_back(enumeration(prefix + "Filter On A", {"Off", "On"}, 1.0));
        device.parameters.push_back(continuous(prefix + "Frequency A", 0.0, 1.0, 0.5));
        device.parameters.push_back(continuous(prefix + "Gain A", -1.0, 1.0, 0.0));
        device.parameters.push_back(continuous(prefix + "Resonance A", 0.0, 1.0, 0.5));
        device.parameters.push_back(enumeration(prefix + "Filter Type A",
                                                {"HighCut48",
                                                 "Bell",
                                                 "LowShelf",
                                                 "Notch",
                                                 "HighShelf",
                                                 "LowCut12",
                                                 "HighCut12",
                                                 "LowCut48"},
                                                1.0));
    }
    device.parameters.push_back(continuous("Output Gain", -1.0, 1.0, 0.0));
    device.integer_properties["global_mode"] = 0;
    return device;
}

LiveNativeParameterProbe& parameter(LiveNativeDeviceProbe& device, const std::string& original) {
    const auto found =
        std::find_if(device.parameters.begin(),
                     device.parameters.end(),
                     [&](const auto& candidate) { return candidate.original_name == original; });
    REQUIRE(found != device.parameters.end());
    return *found;
}

void expect_decline(const LiveNativeMappingPreflight& result, Disposition disposition) {
    CHECK(result.disposition == disposition);
    CHECK_FALSE(result.candidate);
    CHECK_FALSE(result.diagnostic.empty());
}

} // namespace

TEST_CASE("native registry is finite versioned provenance without host qualification",
          "[timbre][live-capabilities]") {
    const auto& provenance = live_native_registry_provenance();
    CHECK(provenance.registry_version == 3);
    CHECK(provenance.public_lom_reference_version == LiveNativeVersion{12, 4, 5});
    CHECK(provenance.first_candidate_version == LiveNativeVersion{12, 3, 0});
    CHECK(provenance.last_candidate_version == LiveNativeVersion{12, 4, 65535});
    CHECK(provenance.python_source_commit == "e83d5192f321b24eb9daab843ac49a2d95d862b1");
    CHECK_FALSE(provenance.host_qualified);
    const auto entries = live_native_parameter_registry();
    REQUIRE(entries.size() == 59);
    std::set<std::string> ids;
    for (const auto& entry : entries) {
        CHECK(ids.insert(entry.id).second);
        CHECK_FALSE(entry.python_source_location.empty());
        CHECK((entry.device_class_name == "StereoGain" || entry.device_class_name == "Eq8" ||
               entry.device_class_name == "Drift"));
        CHECK(entry.band <= 8);
    }
    CHECK(ids.contains("utility.balance"));
    CHECK(ids.contains("eq8.band.8.type"));
    CHECK(ids.contains("drift.lp.frequency"));
    CHECK(ids.contains("drift.env.1.release"));
    CHECK_FALSE(ids.contains("eq8.band.9.type"));
    CHECK_FALSE(ids.contains("autofilter.frequency"));
}

TEST_CASE("Drift registry requires actual instrument role and observed native voice properties",
          "[timbre][live-capabilities][drift]") {
    auto device = utility();
    device.class_name = "Drift";
    device.device_type = 1;
    device.parameters = {continuous("LP Freq", -4.0, 8.0, 1.0),
                         enumeration("LP Type", {"II", "I"}, 1.0),
                         continuous("Env 1 Attack", -4.0, 8.0, 1.0),
                         continuous("Env 1 Decay", -4.0, 8.0, 1.0),
                         continuous("Env 1 Release", -4.0, 8.0, 1.0)};
    device.integer_properties = {{"voice_mode_index", 1}, {"voice_count_index", 2}};
    device.string_list_properties = {{"voice_mode_list", {"Unison", "Poly", "Mono", "Stereo"}},
                                     {"voice_count_list", {"16", "8", "4", "2"}}};
    for (const auto* id :
         {"drift.lp.frequency", "drift.env.1.attack", "drift.env.1.decay", "drift.env.1.release"}) {
        const auto result =
            preflight_live_native_parameter(id, LiveNativeInternalValue{3.5}, device, 3);
        REQUIRE(result.candidate);
        CHECK(result.candidate->internal_value == 3.5);
        CHECK(result.candidate->device_class_name == "Drift");
        CHECK_FALSE(result.candidate->host_qualified);
    }
    expect_decline(preflight_live_native_parameter(
                       "drift.env.1.attack",
                       LiveNativePhysicalValue{LiveNativePhysicalUnit::Milliseconds, 250.0},
                       device,
                       3),
                   Disposition::UnsupportedUnitMapping);
    device.device_type = 2;
    expect_decline(preflight_live_native_parameter(
                       "drift.lp.frequency", LiveNativeInternalValue{3.5}, device, 3),
                   Disposition::DeviceMismatch);
    device.device_type = 1;
    device.integer_properties.erase("voice_mode_index");
    expect_decline(preflight_live_native_parameter(
                       "drift.env.1.attack", LiveNativeInternalValue{3.5}, device, 3),
                   Disposition::ObservationUnavailable);
    device.integer_properties["voice_mode_index"] = 4;
    expect_decline(preflight_live_native_parameter(
                       "drift.env.1.attack", LiveNativeInternalValue{3.5}, device, 3),
                   Disposition::InvalidDomain);
    device.integer_properties["voice_mode_index"] = 1;
    parameter(device, "LP Type").is_enabled = false;
    expect_decline(preflight_live_native_parameter(
                       "drift.lp.frequency", LiveNativeInternalValue{3.5}, device, 3),
                   Disposition::InactiveParameter);
    REQUIRE(preflight_live_native_parameter(
                "drift.env.1.attack", LiveNativeInternalValue{3.5}, device, 3)
                .candidate);
}

TEST_CASE("Utility preflight preserves actual internal values and localized public names",
          "[timbre][live-capabilities]") {
    auto device = utility();
    parameter(device, "Gain").name = "Verstärkung";
    auto result =
        preflight_live_native_parameter("utility.gain", LiveNativeInternalValue{0.73}, device, 3);
    REQUIRE(result.candidate);
    CHECK(result.disposition == Disposition::CandidateReady);
    CHECK(result.candidate->parameter_original_name == "Gain");
    CHECK(result.candidate->observed_parameter_index == 1);
    CHECK(result.candidate->device_chain_index == 3);
    CHECK(result.candidate->internal_value == 0.73);
    CHECK(result.candidate->minimum == 0.0);
    CHECK(result.candidate->maximum == 1.0);
    CHECK(result.candidate->mapping == LiveNativeValueMapping::InternalIdentity);
    CHECK(result.candidate->qualification ==
          LiveNativeCandidateQualification::RuntimeDescriptorMatchedCandidate);
    CHECK(result.candidate->continuous_internal_step_candidate);
    CHECK_FALSE(result.candidate->host_qualified);
    CHECK_FALSE(result.candidate->envelope_runtime_qualified);
    CHECK_FALSE(result.candidate->persistence_verified);
    auto balance = preflight_live_native_parameter(
        "utility.balance", LiveNativeInternalValue{-0.4}, device, 3);
    REQUIRE(balance.candidate);
    CHECK(balance.candidate->internal_value == -0.4);
    CHECK(balance.candidate->parameter_original_name == "Balance");
    auto width =
        preflight_live_native_parameter("utility.width", LiveNativeInternalValue{1.7}, device, 3);
    REQUIRE(width.candidate);
    CHECK(width.candidate->internal_value == 1.7);
    CHECK(width.candidate->parameter_original_name == "Stereo Width");
}

TEST_CASE("physical GUI units and semantic aliases do not establish internal conversion",
          "[timbre][live-capabilities]") {
    auto device = utility();
    for (const auto& [id, intent] :
         std::array{std::pair{"utility.gain",
                              LiveNativePhysicalValue{LiveNativePhysicalUnit::Decibels, -6.0}},
                    std::pair{"utility.balance",
                              LiveNativePhysicalValue{LiveNativePhysicalUnit::StereoBalance, -0.4}},
                    std::pair{"utility.width",
                              LiveNativePhysicalValue{LiveNativePhysicalUnit::Percent, 170.0}}})
        expect_decline(preflight_live_native_parameter(id, intent, device, 3),
                       Disposition::UnsupportedUnitMapping);
    auto equalizer = eq8();
    expect_decline(preflight_live_native_parameter(
                       "eq8.band.1.frequency",
                       LiveNativePhysicalValue{LiveNativePhysicalUnit::Hertz, 1000.0},
                       equalizer,
                       2),
                   Disposition::UnsupportedUnitMapping);
    expect_decline(preflight_live_native_parameter(
                       "eq8.band.1.q",
                       LiveNativePhysicalValue{LiveNativePhysicalUnit::QualityFactor, 1.0},
                       equalizer,
                       2),
                   Disposition::UnsupportedUnitMapping);
    expect_decline(
        preflight_live_native_parameter("utility.gain", LiveNativeEnumChoice{"Off"}, device, 3),
        Disposition::InvalidIntent);
    expect_decline(preflight_live_native_parameter(
                       "utility.gain",
                       LiveNativePhysicalValue{static_cast<LiveNativePhysicalUnit>(255), 0.0},
                       device,
                       3),
                   Disposition::InvalidIntent);
}

TEST_CASE("preflight rejects unknown version scope identity and order",
          "[timbre][live-capabilities]") {
    auto device = utility();
    const auto check = [&](Disposition disposition) {
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       disposition);
    };
    SECTION("legacy and future coverage is unknown without modifying existing profiles") {
        for (const auto version : {LiveNativeVersion{11, 0, 0},
                                   LiveNativeVersion{11, 3, 0},
                                   LiveNativeVersion{12, 2, 65535},
                                   LiveNativeVersion{12, 5, 0},
                                   LiveNativeVersion{13, 0, 0}}) {
            device.version = version;
            check(Disposition::UnknownRegistryCoverage);
        }
    }
    SECTION("all representable patches of reviewed minor versions remain candidates") {
        for (const auto version : {LiveNativeVersion{12, 3, 0},
                                   LiveNativeVersion{12, 3, 65535},
                                   LiveNativeVersion{12, 4, 6},
                                   LiveNativeVersion{12, 4, 65535}}) {
            device.version = version;
            const auto result = preflight_live_native_parameter(
                "utility.gain", LiveNativeInternalValue{0.4}, device, 3);
            REQUIRE(result.candidate);
            CHECK(result.disposition == Disposition::CandidateReady);
            CHECK_FALSE(result.candidate->host_qualified);
        }
    }
    SECTION("device class") {
        device.class_name = "PluginDevice";
        check(Disposition::DeviceMismatch);
    }
    SECTION("chain order") {
        device.chain_index = 4;
        check(Disposition::WrongChainOrder);
    }
    SECTION("missing order") {
        device.chain_index.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("MIDI effect") {
        device.device_type = 4;
        check(Disposition::DeviceMismatch);
    }
    SECTION("inactive device") {
        device.is_active = false;
        check(Disposition::DeviceMismatch);
    }
    SECTION("Rack") {
        device.can_have_chains = true;
        check(Disposition::DeviceMismatch);
    }
    SECTION("incomplete population") {
        device.parameter_population_observed = false;
        check(Disposition::ObservationUnavailable);
    }
    SECTION("missing original names anywhere cannot rule out aliases") {
        device.parameters.back().original_name.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("unknown entry") {
        expect_decline(preflight_live_native_parameter(
                           "eq8.band.9.gain", LiveNativeInternalValue{0.0}, device, 3),
                       Disposition::UnknownCapability);
    }
}

TEST_CASE("original names resolve uniquely and refuse legacy or public aliases",
          "[timbre][live-capabilities]") {
    auto device = utility();
    SECTION("same original name twice") {
        device.parameters.push_back(continuous("Gain"));
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       Disposition::AmbiguousParameter);
    }
    SECTION("another public name collides with the bridge selector") {
        auto alias = continuous("Other Gain");
        alias.name = "Gain";
        device.parameters.push_back(alias);
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       Disposition::AmbiguousParameter);
    }
    SECTION("public name alone cannot masquerade as original identity") {
        parameter(device, "Gain").original_name = "Gain (Legacy)";
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       Disposition::ParameterMismatch);
    }
    SECTION("a decorated proxy is not a native DeviceParameter") {
        parameter(device, "Gain").actual_device_parameter = false;
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       Disposition::ParameterMismatch);
    }
}

TEST_CASE("actual parameter observations reject missing invalid inactive or automated domains",
          "[timbre][live-capabilities]") {
    auto device = utility();
    auto& target = parameter(device, "Gain");
    const auto check = [&](Disposition disposition) {
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{0.4}, device, 3),
                       disposition);
    };
    SECTION("NaN bound") {
        target.minimum = std::numeric_limits<double>::quiet_NaN();
        check(Disposition::InvalidDomain);
    }
    SECTION("infinite bound") {
        target.maximum = std::numeric_limits<double>::infinity();
        check(Disposition::InvalidDomain);
    }
    SECTION("inverted bounds") {
        target.minimum = 2.0;
        check(Disposition::InvalidDomain);
    }
    SECTION("constant bounds") {
        target.maximum = 0.0;
        check(Disposition::InvalidDomain);
    }
    SECTION("nonfinite observed value") {
        target.value = std::numeric_limits<double>::infinity();
        check(Disposition::InvalidDomain);
    }
    SECTION("observed value outside bounds") {
        target.value = -0.1;
        check(Disposition::InvalidDomain);
    }
    SECTION("missing minimum") {
        target.minimum.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("missing enabled") {
        target.is_enabled.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("disabled") {
        target.is_enabled = false;
        check(Disposition::InactiveParameter);
    }
    SECTION("irrelevant") {
        target.state = 1;
        check(Disposition::InactiveParameter);
    }
    SECTION("inactive") {
        target.state = 2;
        check(Disposition::InactiveParameter);
    }
    SECTION("invalid state") {
        target.state = 3;
        check(Disposition::InvalidDomain);
    }
    SECTION("automation active") {
        target.automation_state = 1;
        check(Disposition::ExistingAutomation);
    }
    SECTION("automation overridden") {
        target.automation_state = 2;
        check(Disposition::ExistingAutomation);
    }
    SECTION("invalid automation") {
        target.automation_state = 3;
        check(Disposition::InvalidDomain);
    }
    SECTION("unexpected quantization") {
        target.is_quantized = true;
        check(Disposition::InvalidDomain);
    }
    SECTION("requested outside bounds") {
        expect_decline(preflight_live_native_parameter(
                           "utility.gain", LiveNativeInternalValue{1.1}, device, 3),
                       Disposition::ValueOutsideDomain);
    }
    SECTION("nonfinite intent") {
        expect_decline(preflight_live_native_parameter(
                           "utility.gain",
                           LiveNativeInternalValue{std::numeric_limits<double>::quiet_NaN()},
                           device,
                           3),
                       Disposition::InvalidIntent);
    }
}

TEST_CASE("Utility modes require explicitly matched native choices and width availability",
          "[timbre][live-capabilities]") {
    auto device = utility();
    const auto check = [&](Disposition disposition) {
        expect_decline(preflight_live_native_parameter(
                           "utility.width", LiveNativeInternalValue{1.7}, device, 3),
                       disposition);
    };
    SECTION("missing Mono does not mean off") {
        device.parameters.erase(device.parameters.begin() + 5);
        check(Disposition::ObservationUnavailable);
    }
    SECTION("missing Channel Mode") {
        parameter(device, "Channel Mode").value.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("Left mode") {
        parameter(device, "Channel Mode").value = 0.0;
        check(Disposition::UnsupportedMode);
    }
    SECTION("Mono on despite reversed enum labels") {
        parameter(device, "Mono").value = 0.0;
        check(Disposition::UnsupportedMode);
    }
    SECTION("Mute on") {
        parameter(device, "Mute").value = 1.0;
        check(Disposition::UnsupportedMode);
    }
    SECTION("unmapped localized enum mode") {
        parameter(device, "Channel Mode").value_items =
            std::vector<std::string>{"Links", "Stéréo", "Rechts", "Tausch"};
        check(Disposition::UnsupportedMode);
    }
    SECTION("Mid/Side substitute") {
        parameter(device, "Stereo Width").name = "Mid/Side Balance";
        parameter(device, "Stereo Width").original_name = "Mid/Side Balance";
        check(Disposition::ParameterMismatch);
    }
    SECTION("active Mid/Side alongside Width is ambiguous mode") {
        device.parameters.push_back(continuous("Mid/Side Balance"));
        check(Disposition::UnsupportedMode);
    }
    SECTION("unobserved alternate mode state") {
        auto alternate = continuous("Mid/Side Balance");
        alternate.state.reset();
        device.parameters.push_back(alternate);
        check(Disposition::ObservationUnavailable);
    }
}

TEST_CASE("EQ Eight candidates bind finite bands and actual native enum order",
          "[timbre][live-capabilities]") {
    auto device = eq8();
    auto frequency = preflight_live_native_parameter(
        "eq8.band.8.frequency", LiveNativeInternalValue{0.75}, device, 2);
    REQUIRE(frequency.candidate);
    CHECK(frequency.candidate->parameter_original_name == "8 Frequency A");
    CHECK(frequency.candidate->internal_value == 0.75);
    auto type =
        preflight_live_native_parameter("eq8.band.1.type", LiveNativeEnumChoice{"Bell"}, device, 2);
    REQUIRE(type.candidate);
    CHECK(type.candidate->parameter_original_name == "1 Filter Type A");
    CHECK(type.candidate->internal_value == 1.0);
    CHECK(type.candidate->mapping == LiveNativeValueMapping::AdvertisedEnumIndex);
    CHECK_FALSE(type.candidate->continuous_internal_step_candidate);
    auto& items = *parameter(device, "1 Filter Type A").value_items;
    std::swap(items[1], items[6]);
    auto moved =
        preflight_live_native_parameter("eq8.band.1.type", LiveNativeEnumChoice{"Bell"}, device, 2);
    REQUIRE(moved.candidate);
    CHECK(moved.candidate->internal_value == 6.0);
    expect_decline(
        preflight_live_native_parameter("eq8.band.1.type", LiveNativeEnumChoice{"Peak"}, device, 2),
        Disposition::UnknownEnumChoice);
    auto enabled = preflight_live_native_parameter(
        "eq8.band.1.enabled", LiveNativeEnumChoice{"Off"}, device, 2);
    REQUIRE(enabled.candidate);
    CHECK(enabled.candidate->internal_value == 0.0);
    CHECK_FALSE(enabled.candidate->continuous_internal_step_candidate);
}

TEST_CASE("EQ native mode activation and quantized shapes cannot be assumed",
          "[timbre][live-capabilities]") {
    auto device = eq8();
    const auto check = [&](Disposition disposition) {
        expect_decline(preflight_live_native_parameter(
                           "eq8.band.1.type", LiveNativeEnumChoice{"Bell"}, device, 2),
                       disposition);
    };
    auto& target = parameter(device, "1 Filter Type A");
    SECTION("missing native global mode") {
        device.integer_properties.clear();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("Left/Right") {
        device.integer_properties["global_mode"] = 1;
        check(Disposition::UnsupportedMode);
    }
    SECTION("Mid/Side") {
        device.integer_properties["global_mode"] = 2;
        check(Disposition::UnsupportedMode);
    }
    SECTION("band off") {
        parameter(device, "1 Filter On A").value = 0.0;
        check(Disposition::UnsupportedMode);
    }
    SECTION("missing band activation") {
        parameter(device, "1 Filter On A").value_items.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("missing enum vector") {
        target.value_items.reset();
        check(Disposition::ObservationUnavailable);
    }
    SECTION("unexpected enum count") {
        target.value_items->push_back("New Mode");
        target.maximum = 8.0;
        check(Disposition::InvalidDomain);
    }
    SECTION("duplicate label") {
        (*target.value_items)[0] = "Bell";
        check(Disposition::InvalidDomain);
    }
    SECTION("empty label") {
        (*target.value_items)[0] = "";
        check(Disposition::InvalidDomain);
    }
    SECTION("nonzero minimum") {
        target.minimum = 1.0;
        target.maximum = 8.0;
        check(Disposition::InvalidDomain);
    }
    SECTION("fractional enum value") {
        target.value = 1.25;
        check(Disposition::InvalidDomain);
    }
    SECTION("unexpected index maximum") {
        target.maximum = 8.0;
        check(Disposition::InvalidDomain);
    }
    SECTION("fractional internal request") {
        expect_decline(preflight_live_native_parameter(
                           "eq8.band.1.type", LiveNativeInternalValue{1.25}, device, 2),
                       Disposition::ValueOutsideDomain);
    }
}
