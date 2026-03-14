/**
 * @file CIDC001A.h
 * @brief Corpus IR document model — IngestedWork, ComposerProfile, CorpusDatabase
 *
 * Component: CIDC001A
 * Domain: CI (Corpus IR) | Category: DC (Document)
 *
 * The document model holds the complete corpus state: ingested works,
 * per-composer profiles with aggregated style analyses, and the indices
 * needed for composition-time queries.
 *
 * Hierarchy:
 *   CorpusDatabase
 *     └── ComposerProfile[]
 *           ├── IngestedWork[]
 *           │     ├── WorkMetadata
 *           │     ├── IngestionConfidence
 *           │     └── WorkAnalysis
 *           ├── StyleProfile (aggregated)
 *           └── PeriodProfile[]
 *                 ├── IngestedWork refs
 *                 └── StyleProfile (aggregated)
 *
 * Invariants:
 * - Every IngestedWork belongs to exactly one ComposerProfile
 * - Period assignments are non-overlapping
 * - StyleProfiles are derivable from underlying WorkAnalysis records
 */

#pragma once

#include "CITP001A.h"

namespace Sunny::Core {

// =============================================================================
// §1.5 IngestedWork
// =============================================================================

struct IngestedWork {
    IngestedWorkId id{};
    WorkMetadata metadata;
    IngestionConfidence ingestion_confidence;
    WorkAnalysis analysis;
    bool analysis_complete = false;
};

// =============================================================================
// §3.3 PeriodProfile
// =============================================================================

struct PeriodProfile {
    std::string label;
    std::uint16_t year_start = 0;
    std::uint16_t year_end = 0;
    std::vector<IngestedWorkId> works;
    StyleProfile profile;
};

// =============================================================================
// §3.2 ComposerProfile
// =============================================================================

struct ComposerProfile {
    ComposerProfileId id{};
    std::string name;
    std::optional<std::uint16_t> birth_year;
    std::optional<std::uint16_t> death_year;
    std::optional<std::pair<std::uint16_t, std::uint16_t>> active_period;
    std::optional<std::string> tradition;
    std::vector<IngestedWorkId> works;
    StyleProfile style_profile;
    std::vector<PeriodProfile> period_profiles;
    std::vector<std::string> tags;
};

// =============================================================================
// §4.2 EvolutionaryAnalysis
// =============================================================================

struct EvolutionaryAnalysis {
    ComposerProfileId composer{};
    std::vector<PeriodProfile> periods;
    std::vector<EvolutionaryTrend> trends;
};

// =============================================================================
// CorpusDatabase (root object)
// =============================================================================

struct CorpusDatabase {
    std::map<std::uint64_t, ComposerProfile> composers;
    std::map<std::uint64_t, IngestedWork> works;
};

}  // namespace Sunny::Core
