/**
 * @file CIWF001A.h
 * @brief Corpus IR workflow functions — agent-driven corpus management
 *
 * Component: CIWF001A
 * Domain: CI (Corpus IR) | Category: WF (Workflow)
 *
 * Provides entry-point functions for agent-driven corpus operations per
 * Corpus Spec §6. These functions manage the corpus lifecycle from
 * ingestion through analysis to composition-time queries.
 *
 * Functions divide into four categories:
 *   Ingestion:   create_composer_profile, ingest_work, assign_work
 *   Analysis:    analyze_work, rebuild_style_profile, detect_signatures
 *   Query:       query_style_profile, find_examples, how_would_x_handle
 *   Comparison:  compare_composers, analyze_evolution
 *
 * Invariants:
 * - Ingestion produces valid Score IR documents
 * - Analysis is idempotent for the same input
 * - Queries are pure (no mutation of corpus state)
 */

#pragma once

#include "CIDC001A.h"
#include "CIVD001A.h"

namespace Sunny::Core {

// =============================================================================
// Profile Management
// =============================================================================

/**
 * @brief Create a new composer profile.
 */
[[nodiscard]] ComposerProfile wf_create_composer_profile(
    ComposerProfileId id,
    const std::string& name);

/**
 * @brief Create a new ingested work shell (metadata only, no analysis).
 */
[[nodiscard]] IngestedWork wf_create_ingested_work(
    IngestedWorkId id,
    const WorkMetadata& metadata);

/**
 * @brief Assign a work to a composer profile.
 */
[[nodiscard]] Result<void> wf_assign_work_to_composer(
    CorpusDatabase& corpus,
    IngestedWorkId work_id,
    ComposerProfileId composer_id);

/**
 * @brief Assign a work to a period within a composer's profile.
 */
[[nodiscard]] Result<void> wf_assign_work_to_period(
    CorpusDatabase& corpus,
    IngestedWorkId work_id,
    ComposerProfileId composer_id,
    const std::string& period_label);

/**
 * @brief Add a period profile to a composer.
 */
[[nodiscard]] Result<void> wf_add_period_profile(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const PeriodProfile& period);

// =============================================================================
// Analysis
// =============================================================================

/**
 * @brief Run analytical decomposition on an ingested work.
 *
 * Produces a WorkAnalysis covering all nine analytical domains.
 * This is a placeholder that initialises the analysis structure;
 * full heuristic analysis requires the theory engine integration
 * which is beyond the scope of the document model layer.
 */
[[nodiscard]] Result<void> wf_analyze_work(
    CorpusDatabase& corpus,
    IngestedWorkId work_id);

/**
 * @brief Rebuild a composer's style profile from all their works.
 *
 * Aggregates per-work analyses into the StyleProfile.
 */
[[nodiscard]] Result<void> wf_rebuild_style_profile(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id);

/**
 * @brief Detect statistically distinctive patterns for a composer.
 */
[[nodiscard]] Result<void> wf_detect_signature_patterns(
    CorpusDatabase& corpus,
    ComposerProfileId composer_id);

// =============================================================================
// Query
// =============================================================================

/**
 * @brief Get a composer's style profile.
 */
[[nodiscard]] Result<const StyleProfile*> wf_query_style_profile(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id);

/**
 * @brief Find examples matching analytical criteria.
 */
[[nodiscard]] std::vector<AnnotatedExample> wf_find_examples(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::string& criterion);

/**
 * @brief Get progression examples from a composer's corpus.
 */
[[nodiscard]] std::vector<AnnotatedExample> wf_get_progression_examples(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::vector<std::string>& roman_numerals);

/**
 * @brief Get formal proportions template for a given form type.
 */
[[nodiscard]] Result<FormalStyleProfile> wf_get_formal_template(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    FormClassification form_type);

/**
 * @brief HowWouldXHandle query — find insights for a compositional situation.
 */
[[nodiscard]] Result<HowWouldXHandleResult> wf_how_would_x_handle(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id,
    const std::string& situation);

// =============================================================================
// Comparison
// =============================================================================

/**
 * @brief Compare two composers across all analytical domains.
 */
[[nodiscard]] Result<StyleComparison> wf_compare_composers(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_a,
    ComposerProfileId composer_b);

/**
 * @brief Analyse a composer's stylistic evolution across periods.
 */
[[nodiscard]] Result<EvolutionaryAnalysis> wf_analyze_evolution(
    const CorpusDatabase& corpus,
    ComposerProfileId composer_id);

// =============================================================================
// Corpus-Level
// =============================================================================

/**
 * @brief Validate the entire corpus.
 */
[[nodiscard]] std::vector<Diagnostic> wf_validate_corpus(
    const CorpusDatabase& corpus);

}  // namespace Sunny::Core
