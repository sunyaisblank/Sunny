/**
 * @file workflows.hpp
 * @brief Corpus IR workflow functions — agent-driven corpus management
 *
 *
 * Provides entry-point functions for agent-driven corpus operations per
 * Corpus Spec §6. These functions manage the corpus lifecycle from
 * ingestion through analysis to composition-time queries.
 *
 * Functions divide into four categories:
 *   Ingestion:   create_composer_profile, ingest_work, assign_work
 *   Lifecycle:   clear_work_period_assignment, remove_ingested_work
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

#include <sunny/core/corpus/document.hpp>
#include <sunny/core/corpus/validation.hpp>
#include <sunny/core/score/document.hpp>

namespace sunny::core {

// =============================================================================
// Profile Management
// =============================================================================

/**
 * @brief Create a new composer profile.
 */
[[nodiscard]] ComposerProfile create_composer_profile(ComposerProfileId id,
                                                      const std::string& name);

/**
 * @brief Create a new ingested work shell (metadata only, no analysis).
 */
[[nodiscard]] IngestedWork create_ingested_work(IngestedWorkId id, const WorkMetadata& metadata);

/**
 * @brief Assign a work to a composer profile.
 */
[[nodiscard]] Result<void> assign_work_to_composer(CorpusDatabase& corpus,
                                                   IngestedWorkId work_id,
                                                   ComposerProfileId composer_id);

/**
 * @brief Assign a work to a period within a composer's profile.
 */
[[nodiscard]] Result<void> assign_work_to_period(CorpusDatabase& corpus,
                                                 IngestedWorkId work_id,
                                                 ComposerProfileId composer_id,
                                                 const std::string& period_label);

/**
 * @brief Add a period profile to a composer.
 */
[[nodiscard]] Result<void> add_period_profile(CorpusDatabase& corpus,
                                              ComposerProfileId composer_id,
                                              const PeriodProfile& period);

/**
 * @brief Remove a work from any period while preserving composer ownership.
 */
[[nodiscard]] Result<void> clear_work_period_assignment(CorpusDatabase& corpus,
                                                        IngestedWorkId work_id);

/**
 * @brief Remove an ingested work and all composer/period reverse references.
 */
[[nodiscard]] Result<void> remove_ingested_work(CorpusDatabase& corpus, IngestedWorkId work_id);

// =============================================================================
// Analysis
// =============================================================================

/**
 * @brief Run analytical decomposition on an ingested work.
 *
 * Performs full analytical decomposition across all nine domains using the
 * supplied Score, or the work's embedded Score when the argument is null.
 * Returns InvalidMutation when neither source is available.
 */
[[nodiscard]] Result<void>
analyze_work(CorpusDatabase& corpus, IngestedWorkId work_id, const Score* score = nullptr);

/**
 * @brief Rebuild a composer's style profile from all their works.
 *
 * Aggregates per-work analyses into the composer and period StyleProfiles.
 * Normal lifecycle mutations perform this refresh automatically; this entry
 * point remains available to normalise explicitly assembled/imported state.
 */
[[nodiscard]] Result<void> rebuild_style_profile(CorpusDatabase& corpus,
                                                 ComposerProfileId composer_id);

/**
 * @brief Test a supplied profile against the deterministic aggregate of a membership.
 *
 * Signature patterns are excluded because they belong to the explicit detection
 * workflow rather than the synchronous statistical aggregate.
 */
[[nodiscard]] bool style_profile_is_fresh(const CorpusDatabase& corpus,
                                          const std::vector<IngestedWorkId>& work_ids,
                                          const StyleProfile& profile);

/**
 * @brief Detect statistically distinctive patterns for a composer.
 */
[[nodiscard]] Result<void> detect_signature_patterns(CorpusDatabase& corpus,
                                                     ComposerProfileId composer_id);

// =============================================================================
// Query
// =============================================================================

/**
 * @brief Get a composer's style profile.
 */
[[nodiscard]] Result<const StyleProfile*> query_style_profile(const CorpusDatabase& corpus,
                                                              ComposerProfileId composer_id);

/**
 * @brief Find examples matching analytical criteria.
 */
[[nodiscard]] std::vector<AnnotatedExample> find_examples(const CorpusDatabase& corpus,
                                                          ComposerProfileId composer_id,
                                                          const std::string& criterion);

/**
 * @brief Get progression examples from a composer's corpus.
 */
[[nodiscard]] std::vector<AnnotatedExample>
get_progression_examples(const CorpusDatabase& corpus,
                         ComposerProfileId composer_id,
                         const std::vector<std::string>& roman_numerals);

/**
 * @brief Get formal proportions template for a given form type.
 */
[[nodiscard]] Result<FormalStyleProfile> get_formal_template(const CorpusDatabase& corpus,
                                                             ComposerProfileId composer_id,
                                                             FormClassification form_type);

/**
 * @brief HowWouldXHandle query — find insights for a compositional situation.
 */
[[nodiscard]] Result<HowWouldXHandleResult> how_would_x_handle(const CorpusDatabase& corpus,
                                                               ComposerProfileId composer_id,
                                                               const std::string& situation);

// =============================================================================
// Comparison
// =============================================================================

/**
 * @brief Compare two composers across all analytical domains.
 */
[[nodiscard]] Result<StyleComparison> compare_composers(const CorpusDatabase& corpus,
                                                        ComposerProfileId composer_a,
                                                        ComposerProfileId composer_b);

/**
 * @brief Analyse a composer's stylistic evolution across periods.
 */
[[nodiscard]] Result<EvolutionaryAnalysis> analyze_evolution(const CorpusDatabase& corpus,
                                                             ComposerProfileId composer_id);

} // namespace sunny::core
