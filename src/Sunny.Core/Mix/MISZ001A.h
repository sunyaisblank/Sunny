/**
 * @file MISZ001A.h
 * @brief Mix IR serialisation — JSON round-trip
 *
 * Component: MISZ001A
 * Domain: MI (Mix IR) | Category: SZ (Serialisation)
 *
 * Provides JSON serialisation and deserialisation for the Mix IR document
 * model. Uses nlohmann/json for the transport format.
 *
 * The serialisation is lossless: serialise(deserialise(json)) == json
 * for all valid MixGraph documents.
 *
 * Invariants:
 * - Round-trip preserves all fields exactly
 * - Deserialisation validates structural requirements on load
 * - Schema version is checked on load
 */

#pragma once

#include "MIDC001A.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Sunny::Core {

// =============================================================================
// Serialisation API
// =============================================================================

constexpr int MIX_IR_SCHEMA_VERSION = 1;

/**
 * @brief Serialise a MixGraph to JSON
 */
[[nodiscard]] nlohmann::json mix_to_json(const MixGraph& graph);

/**
 * @brief Deserialise a MixGraph from JSON
 */
[[nodiscard]] Result<MixGraph> mix_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a MixGraph to a JSON string
 */
[[nodiscard]] std::string mix_to_json_string(
    const MixGraph& graph,
    int indent = 2);

/**
 * @brief Deserialise a MixGraph from a JSON string
 */
[[nodiscard]] Result<MixGraph> mix_from_json_string(
    const std::string& json_str);

}  // namespace Sunny::Core
