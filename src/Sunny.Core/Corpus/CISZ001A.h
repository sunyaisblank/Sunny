/**
 * @file CISZ001A.h
 * @brief Corpus IR serialisation — JSON round-trip
 *
 * Component: CISZ001A
 * Domain: CI (Corpus IR) | Category: SZ (Serialisation)
 *
 * Provides JSON serialisation and deserialisation for the Corpus IR
 * document model. Supports per-work, per-composer, and full-corpus
 * serialisation.
 *
 * Invariants:
 * - Round-trip preserves all fields exactly
 * - Schema version is checked on load
 */

#pragma once

#include "CIDC001A.h"

#include <nlohmann/json.hpp>
#include <string>

namespace Sunny::Core {

constexpr int CORPUS_IR_SCHEMA_VERSION = 1;

/**
 * @brief Serialise a ComposerProfile to JSON.
 */
[[nodiscard]] nlohmann::json composer_profile_to_json(
    const ComposerProfile& profile);

/**
 * @brief Deserialise a ComposerProfile from JSON.
 */
[[nodiscard]] Result<ComposerProfile> composer_profile_from_json(
    const nlohmann::json& json);

/**
 * @brief Serialise an IngestedWork to JSON.
 */
[[nodiscard]] nlohmann::json ingested_work_to_json(
    const IngestedWork& work);

/**
 * @brief Deserialise an IngestedWork from JSON.
 */
[[nodiscard]] Result<IngestedWork> ingested_work_from_json(
    const nlohmann::json& json);

/**
 * @brief Serialise a CorpusDatabase to JSON.
 */
[[nodiscard]] nlohmann::json corpus_to_json(
    const CorpusDatabase& corpus);

/**
 * @brief Deserialise a CorpusDatabase from JSON.
 */
[[nodiscard]] Result<CorpusDatabase> corpus_from_json(
    const nlohmann::json& json);

/**
 * @brief Serialise a CorpusDatabase to a JSON string.
 */
[[nodiscard]] std::string corpus_to_json_string(
    const CorpusDatabase& corpus,
    int indent = 2);

/**
 * @brief Deserialise a CorpusDatabase from a JSON string.
 */
[[nodiscard]] Result<CorpusDatabase> corpus_from_json_string(
    const std::string& json_str);

}  // namespace Sunny::Core
