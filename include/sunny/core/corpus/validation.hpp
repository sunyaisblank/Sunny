/**
 * @file validation.hpp
 * @brief Corpus IR validation — C1–C17 rules
 *
 *
 * Validates ingested works, analysis completeness, and profile quality.
 *
 * Rules:
 *   C1–C5:   Ingestion validation
 *   C6–C9:   Analysis validation
 *   C10–C13: Profile validation
 *   C14:     Corpus graph consistency
 *   C15:     Derived aggregate freshness
 *   C16:     Ingestion-confidence numeric domains
 *   C17:     Analysis method, availability and exact passage provenance
 */

#pragma once

#include <sunny/core/corpus/document.hpp>

namespace sunny::core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Validate an ingested work (C1–C9, C16–C17).
 */
[[nodiscard]] std::vector<Diagnostic> validate_ingested_work(const IngestedWork& work);

/**
 * @brief Validate a composer profile (C10–C13).
 */
[[nodiscard]] std::vector<Diagnostic> validate_composer_profile(const ComposerProfile& profile);

/**
 * @brief Validate the entire corpus database.
 */
[[nodiscard]] std::vector<Diagnostic> validate_corpus(const CorpusDatabase& corpus);

/**
 * @brief Check whether a corpus has any errors (severity == Error).
 */
[[nodiscard]] bool is_corpus_valid(const CorpusDatabase& corpus);

/**
 * @brief Whether a diagnostic denies loading a persisted corpus.
 *
 * Only Error-severity structural rules gate loading: an embedded Score that
 * fails structural validation (C2), contradictory identities (C14), stale
 * derived aggregates (C15), out-of-domain confidence records (C16), and
 * contradictory analysis evidence/provenance (C17).
 * Ingestion and analysis quality rules (C1, C3-C13) describe the evidence
 * rather than the document's integrity, so a state produced by Sunny's own
 * tools always reloads; they remain diagnostics for validate_corpus.
 */
[[nodiscard]] bool blocks_corpus_load(const Diagnostic& diagnostic);

} // namespace sunny::core
