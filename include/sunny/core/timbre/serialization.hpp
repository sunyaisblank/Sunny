/**
 * @file serialization.hpp
 * @brief Timbre IR serialisation — JSON round-trip
 *
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

#include <nlohmann/json.hpp>
#include <string>
#include <sunny/core/timbre/document.hpp>

namespace sunny::core {

// =============================================================================
// Serialisation API
// =============================================================================

// v3 adds the previously missing modulation-envelope collection. Readers
// migrate v1/v2 matrices to an empty collection without inventing sources.
constexpr int TIMBRE_IR_SCHEMA_VERSION = 3;

/** Reuse the existing profile-preset representation for the shared preset library. */
[[nodiscard]] nlohmann::json timbre_preset_to_json(const TimbrePreset& preset);
[[nodiscard]] Result<TimbrePreset> timbre_preset_from_json(const nlohmann::json& json);

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
[[nodiscard]] std::string timbre_to_json_string(const TimbreProfile& profile, int indent = 2);

/**
 * @brief Deserialise a TimbreProfile from a JSON string
 */
[[nodiscard]] Result<TimbreProfile> timbre_from_json_string(const std::string& json_str);

} // namespace sunny::core
