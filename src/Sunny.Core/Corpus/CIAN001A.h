/**
 * @file CIAN001A.h
 * @brief Corpus IR analytical decomposition — Score to WorkAnalysis
 *
 * Component: CIAN001A
 * Domain: CI (Corpus IR) | Category: AN (Analysis)
 *
 * Decomposes a Score IR document into a WorkAnalysis covering nine
 * analytical domains. Each domain is a pure function Score → Record,
 * composing existing theory engine components.
 *
 * Precondition:  Score passes structural validation (no Error-level diagnostics)
 * Postcondition: WorkAnalysis fields are populated with data derived from
 *                the score's note content, key map, tempo map, and section map.
 *
 * Domains:
 *   1. Harmonic   — chord vocabulary, progressions, cadences, modulations
 *   2. Melodic    — intervals, contour, range, chromaticism per voice
 *   3. Rhythmic   — durations, onset density, syncopation, complexity
 *   4. Formal     — sections, form classification, proportions
 *   5. Voice-leading — motion types, parallels, independence
 *   6. Textural   — density, register span, spacing
 *   7. Dynamic    — range, distribution, shape
 *   8. Orchestration — instrument usage, doublings (multi-part only)
 *   9. Motivic    — thematic material, transformations (structural)
 */

#pragma once

#include "CITP001A.h"
#include "../Score/SIDC001A.h"

namespace Sunny::Core {

/**
 * @brief Perform full analytical decomposition on a Score.
 *
 * @param score Validated Score IR document
 * @return Populated WorkAnalysis
 */
[[nodiscard]] WorkAnalysis analyze_score(const Score& score);

/**
 * @brief Analyze only the harmonic domain.
 */
[[nodiscard]] HarmonicAnalysisRecord analyze_harmonic(const Score& score);

/**
 * @brief Analyze only the melodic domain.
 */
[[nodiscard]] MelodicAnalysisRecord analyze_melodic(const Score& score);

/**
 * @brief Analyze only the rhythmic domain.
 */
[[nodiscard]] RhythmicAnalysisRecord analyze_rhythmic(const Score& score);

/**
 * @brief Analyze only the formal domain.
 */
[[nodiscard]] FormalAnalysisRecord analyze_formal(const Score& score);

/**
 * @brief Analyze only the voice-leading domain.
 */
[[nodiscard]] VoiceLeadingAnalysisRecord analyze_voice_leading(const Score& score);

/**
 * @brief Analyze only the textural domain.
 */
[[nodiscard]] TexturalAnalysisRecord analyze_textural(const Score& score);

/**
 * @brief Analyze only the dynamic domain.
 */
[[nodiscard]] DynamicAnalysisRecord analyze_dynamic(const Score& score);

/**
 * @brief Analyze only the orchestration domain.
 *
 * Returns nullopt for single-part scores.
 */
[[nodiscard]] std::optional<OrchestrationAnalysisRecord>
analyze_orchestration(const Score& score);

/**
 * @brief Analyze only the motivic domain.
 */
[[nodiscard]] MotivicAnalysisRecord analyze_motivic(const Score& score);

}  // namespace Sunny::Core
