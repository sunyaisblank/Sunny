/**
 * @file CIVD001A.h
 * @brief Corpus IR validation — C1–C13 rules
 *
 * Component: CIVD001A
 * Domain: CI (Corpus IR) | Category: VD (Validation)
 *
 * Validates ingested works, analysis completeness, and profile quality.
 *
 * Rules:
 *   C1–C5:   Ingestion validation
 *   C6–C9:   Analysis validation
 *   C10–C13: Profile validation
 */

#pragma once

#include "CIDC001A.h"

namespace Sunny::Core {

// =============================================================================
// Validation API
// =============================================================================

/**
 * @brief Validate an ingested work (C1–C5, C6–C9).
 */
[[nodiscard]] std::vector<Diagnostic> validate_ingested_work(
    const IngestedWork& work);

/**
 * @brief Validate a composer profile (C10–C13).
 */
[[nodiscard]] std::vector<Diagnostic> validate_composer_profile(
    const ComposerProfile& profile);

/**
 * @brief Validate the entire corpus database.
 */
[[nodiscard]] std::vector<Diagnostic> validate_corpus(
    const CorpusDatabase& corpus);

/**
 * @brief Check whether a corpus has any errors (severity == Error).
 */
[[nodiscard]] bool is_corpus_valid(const CorpusDatabase& corpus);

}  // namespace Sunny::Core
