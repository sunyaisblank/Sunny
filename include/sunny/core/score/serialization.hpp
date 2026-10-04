/**
 * @file serialization.hpp
 * @brief Score IR serialisation — JSON round-trip
 *
 *
 * Provides JSON serialisation and deserialisation for the Score IR
 * document model. Uses nlohmann/json for the transport format.
 *
 * The serialisation is lossless and canonical: serialise(deserialise(json))
 * is a fixed point for canonical Score IR documents. Readers accept
 * equivalent non-canonical Beat fractions and writers emit lowest terms.
 *
 * Invariants:
 * - Round-trip preserves all fields exactly
 * - Deserialisation validates structural invariants on load
 * - Schema version is checked on load
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <sunny/core/score/document.hpp>

namespace sunny::core {

// =============================================================================
// Serialisation API
// =============================================================================

/// Current schema version for serialised Score IR documents
constexpr int SCORE_IR_SCHEMA_VERSION = 9;

/** @brief Serialise one articulation mapping in canonical tagged-union form. */
[[nodiscard]] nlohmann::json articulation_mapping_to_json(const ArticulationMapping& mapping);

/** @brief Parse and validate one bounded articulation mapping tree. */
[[nodiscard]] Result<ArticulationMapping>
articulation_mapping_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a Score to JSON
 *
 * @param score The score to serialise
 * @return JSON object
 */
[[nodiscard]] nlohmann::json score_to_json(const Score& score);

/**
 * @brief Deserialise a Score from JSON
 *
 * Validates schema version and structural invariants on load.
 *
 * @param json The JSON to deserialise
 * @return Score or error
 */
[[nodiscard]] Result<Score> score_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a Score to a JSON string
 *
 * @param score The score to serialise
 * @param indent Indentation level (-1 for compact)
 * @return JSON string
 */
[[nodiscard]] std::string score_to_json_string(const Score& score, int indent = 2);

/**
 * @brief Deserialise a Score from a JSON string
 *
 * @param json_str The JSON string
 * @return Score or error
 */
[[nodiscard]] Result<Score> score_from_json_string(const std::string& json_str);

} // namespace sunny::core
