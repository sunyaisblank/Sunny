/**
 * @file TISZ001A.h
 * @brief Timbre IR serialisation — JSON round-trip
 *
 * Component: TISZ001A
 * Domain: TI (Timbre IR) | Category: SZ (Serialisation)
 *
 * Provides JSON serialisation and deserialisation for the Timbre IR
 * document model. Uses nlohmann/json for the transport format.
 *
 * The serialisation is lossless: serialise(deserialise(json)) == json
 * for all valid TimbreProfile documents.
 *
 * Invariants:
 * - Round-trip preserves all fields exactly
 * - Deserialisation validates structural requirements on load
 * - Schema version is checked on load
 */

#pragma once

#include "TIDC001A.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Sunny::Core {

// =============================================================================
// Serialisation API
// =============================================================================

constexpr int TIMBRE_IR_SCHEMA_VERSION = 1;

/**
 * @brief Serialise a TimbreProfile to JSON
 */
[[nodiscard]] nlohmann::json timbre_to_json(const TimbreProfile& profile);

/**
 * @brief Deserialise a TimbreProfile from JSON
 */
[[nodiscard]] Result<TimbreProfile> timbre_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a TimbreProfile to a JSON string
 */
[[nodiscard]] std::string timbre_to_json_string(
    const TimbreProfile& profile,
    int indent = 2);

/**
 * @brief Deserialise a TimbreProfile from a JSON string
 */
[[nodiscard]] Result<TimbreProfile> timbre_from_json_string(
    const std::string& json_str);

}  // namespace Sunny::Core
