/**
 * @file analysis.hpp
 * @brief Corpus IR analytical decomposition — Score to WorkAnalysis
 *
 *
 * Decomposes a Score IR document into a WorkAnalysis covering nine
 * analytical domains. Each domain is a pure function Score → Record,
 * composing existing theory engine components.
 *
 * Precondition:  Score passes structural validation (no Error-level diagnostics)
 * Postcondition: WorkAnalysis fields are populated with data derived from
 *                the score's note content, key map, tempo map, and section map.
 *
 * Symbolic timing uses the Score's whole-note allocations (including tuplets
 * and allocated grace notes), with each individual duration tie folded once.
 * Melodic lanes are (PartId, voice_index); at a simultaneous onset the highest
 * newly attacked MIDI note is selected. This is an explicit melodic heuristic.
 * Motifs cover contiguous, nonoverlapping 3–8 note windows with identical
 * directed semitone intervals and exact rational duration ratios. Occurrences
 * in the same lane cannot overlap; separate lanes remain independent.
 * No human thematic importance or performance timing is inferred. Full analysis
 * reports methods and unavailable fields in its per-domain evidence map.
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

#include <sunny/core/corpus/types.hpp>
#include <sunny/core/score/document.hpp>

namespace sunny::core {

/**
 * @brief Perform full analytical decomposition on a Score.
 *
 * @param score Validated Score IR document
 * @return Populated WorkAnalysis
 */
[[nodiscard]] Result<WorkAnalysis> analyze_score(const Score& score);

/**
 * @brief Analyze only the harmonic domain.
 */
[[nodiscard]] Result<HarmonicAnalysisRecord> analyze_harmonic(const Score& score);

/**
 * @brief Analyze only the melodic domain.
 */
[[nodiscard]] Result<MelodicAnalysisRecord> analyze_melodic(const Score& score);

/**
 * @brief Analyze only the rhythmic domain.
 */
[[nodiscard]] Result<RhythmicAnalysisRecord> analyze_rhythmic(const Score& score);

/**
 * @brief Analyze only the formal domain.
 */
[[nodiscard]] Result<FormalAnalysisRecord> analyze_formal(const Score& score);

/**
 * @brief Analyze only the voice-leading domain.
 */
[[nodiscard]] Result<VoiceLeadingAnalysisRecord> analyze_voice_leading(const Score& score);

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
[[nodiscard]] std::optional<OrchestrationAnalysisRecord> analyze_orchestration(const Score& score);

/**
 * @brief Analyze only the motivic domain.
 */
[[nodiscard]] Result<MotivicAnalysisRecord> analyze_motivic(const Score& score);

} // namespace sunny::core
