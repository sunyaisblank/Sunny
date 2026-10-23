/**
 * @file target_profile_test.cpp
 * @brief Ableton target-model derivation and bridge trust-boundary tests
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;

TEST_CASE("Ableton versions imply documented deployment capabilities",
          "[ableton][target-profile]") {
    auto live_10 = modeled_target_profile({10, 1, 43, "10.1.43"});
    CHECK(live_10.clip_note_insertion == CapabilityState::Unavailable);
    CHECK(live_10.native_device_insertion == CapabilityState::Unavailable);

    auto live_11 = modeled_target_profile({11, 3, 42, "11.3.42"});
    CHECK(live_11.clip_note_insertion == CapabilityState::Available);
    CHECK(live_11.native_device_insertion == CapabilityState::Unavailable);

    auto live_12_3 = modeled_target_profile({12, 3, 0, "12.3.0"});
    CHECK(live_12_3.clip_note_insertion == CapabilityState::Available);
    CHECK(live_12_3.native_device_insertion == CapabilityState::Available);
    CHECK(live_12_3.max_for_live == CapabilityState::Unknown);
}

TEST_CASE("target profile JSON round trips through strict validation",
          "[ableton][target-profile]") {
    const auto expected = modeled_target_profile({12, 3, 5, "12.3.5"});
    auto parsed = target_profile_from_json(target_profile_to_json(expected));

    REQUIRE(parsed.has_value());
    CHECK(parsed->live_version.version_string == "12.3.5");
    CHECK(parsed->native_device_insertion == CapabilityState::Available);
    CHECK(parsed->adapter_contract == "version_coupled_private");
    CHECK(parsed->adapter_source_sha256 == SUNNY_BRIDGE_SOURCE_SHA256);
}

TEST_CASE("target profile rejects a peer whose claims contradict its Live version",
          "[ableton][target-profile][trust-boundary]") {
    auto payload = target_profile_to_json(modeled_target_profile({11, 3, 42, "11.3.42"}));
    payload["capabilities"]["track_insert_device_native"] = "available";

    auto parsed = target_profile_from_json(payload);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error() == ErrorCode::ProtocolError);
}

TEST_CASE("in-memory target profile validation rejects transport-supplied contradictions",
          "[ableton][target-profile][trust-boundary]") {
    auto profile = modeled_target_profile({11, 3, 42, "11.3.42 custom transport"});
    REQUIRE(validate_target_profile(profile).has_value());

    profile.native_device_insertion = CapabilityState::Available;
    const auto invalid = validate_target_profile(profile);
    REQUIRE_FALSE(invalid.has_value());
    CHECK(invalid.error() == ErrorCode::ProtocolError);
}

TEST_CASE("Target profiles require the exact bundled source identity",
          "[ableton][target-profile][source-identity]") {
    const auto expected = modeled_target_profile({12, 4, 0, "12.4.0"});
    const auto original = target_profile_to_json(expected);
    REQUIRE(target_profile_from_json(original));
    for (const auto& identity : {nlohmann::json(nullptr),
                                 nlohmann::json(46),
                                 nlohmann::json("unknown"),
                                 nlohmann::json(std::string(64, '0'))}) {
        auto value = original;
        value["adapter"]["source_sha256"] = identity;
        CHECK_FALSE(target_profile_from_json(value));
    }
    auto missing = original;
    missing["adapter"].erase("source_sha256");
    CHECK_FALSE(target_profile_from_json(missing));
    auto in_memory = expected;
    in_memory.adapter_source_sha256 = std::string(64, '0');
    CHECK_FALSE(validate_target_profile(in_memory));
}

TEST_CASE("target profile rejects incompatible bridge identity and protocol",
          "[ableton][target-profile][trust-boundary]") {
    auto payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));

    payload["adapter"]["contract"] = "documented_public";
    CHECK_FALSE(target_profile_from_json(payload).has_value());

    payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));
    payload["bridge_protocol_version"] = SUNNY_BRIDGE_PROTOCOL_VERSION + 1;
    CHECK_FALSE(target_profile_from_json(payload).has_value());

    payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));
    payload["capabilities"]["max_for_live"] = "probably";
    CHECK_FALSE(target_profile_from_json(payload).has_value());

    payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));
    payload["live"]["version"]["string"] = "11.3.5";
    CHECK_FALSE(target_profile_from_json(payload).has_value());

    payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));
    payload["adapter_name"] = "Sunny Remote Script";
    CHECK_FALSE(target_profile_from_json(payload).has_value());

    payload = target_profile_to_json(modeled_target_profile({12, 3, 5, "12.3.5"}));
    payload["capabilities"]["future_capability"] = "unknown";
    CHECK_FALSE(target_profile_from_json(payload).has_value());
}
