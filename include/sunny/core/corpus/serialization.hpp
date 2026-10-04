/**
 * @file serialization.hpp
 * @brief Corpus IR serialisation — JSON round-trip
 *
 *
 * Provides JSON serialisation and deserialisation for the Corpus IR
 * document model. Supports per-work, per-composer, and full-corpus
 * serialisation.
 *
 * Schema versions:
 * - v1: ScoreTime stored flat as {"bar", "beat_n", "beat_d"}; the reader
 *   fabricated defaults for missing fields. v1 documents still load via
 *   the lenient back-compat path (no validate-on-load).
 * - v2: ScoreTime uses the shared nested Score IR scheme
 *   ({"bar", "beat": {"num", "den"}}); the reader refuses documents with
 *   missing required fields (ErrorCode::FormatError) and, for the corpus
 *   database, blocks loading on Error-severity validation diagnostics
 *   (ErrorCode::ValidationOnLoadFailed).
 * - v3: the writer and strict reader project every authoritative
 *   WorkAnalysis, StyleProfile, SignaturePattern, metadata, confidence, and
 *   composer lifecycle field then known.
 * - v4: IngestionConfidence separately persists RMS onset and
 *   duration quantisation error. v1-v3 migrate the historically absent
 *   duration residual to zero; v4 requires both fields.
 * - v5 (current): persists analytical method/availability evidence and exact
 *   thematic voice/span provenance. v1-v4 retain supplied values but migrate
 *   absent evidence and voice/end identity as unqualified/unknown.
 *
 * Invariants:
 * - Round-trip preserves all fields exactly
 * - Schema version is checked on load; accepted range is [1, 5]
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <sunny/core/corpus/document.hpp>

namespace sunny::core {

constexpr int CORPUS_IR_SCHEMA_VERSION = 5;

/** Serialise the current embedded analysis record, without its enclosing Score. */
[[nodiscard]] nlohmann::json work_analysis_to_json(const WorkAnalysis& analysis);

/**
 * @brief Serialise a ComposerProfile to JSON.
 */
[[nodiscard]] nlohmann::json composer_profile_to_json(const ComposerProfile& profile);

/**
 * @brief Deserialise a ComposerProfile from JSON.
 */
[[nodiscard]] Result<ComposerProfile> composer_profile_from_json(const nlohmann::json& json);

/**
 * @brief Serialise an IngestedWork to JSON.
 */
[[nodiscard]] nlohmann::json ingested_work_to_json(const IngestedWork& work);

/**
 * @brief Deserialise an IngestedWork from JSON.
 */
[[nodiscard]] Result<IngestedWork> ingested_work_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a CorpusDatabase to JSON.
 */
[[nodiscard]] nlohmann::json corpus_to_json(const CorpusDatabase& corpus);

/**
 * @brief Deserialise a CorpusDatabase from JSON.
 */
[[nodiscard]] Result<CorpusDatabase> corpus_from_json(const nlohmann::json& json);

/**
 * @brief Serialise a CorpusDatabase to a JSON string.
 */
[[nodiscard]] std::string corpus_to_json_string(const CorpusDatabase& corpus, int indent = 2);

/**
 * @brief Deserialise a CorpusDatabase from a JSON string.
 */
[[nodiscard]] Result<CorpusDatabase> corpus_from_json_string(const std::string& json_str);

} // namespace sunny::core
