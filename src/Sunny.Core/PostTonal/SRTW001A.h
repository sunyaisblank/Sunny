/**
 * @file SRTW001A.h
 * @brief Twelve-tone serialism (tone rows, matrix, combinatoriality)
 *
 * Component: SRTW001A
 * Domain: SR (Serialism) | Category: TW (Tone Row)
 *
 * Operations on twelve-tone rows: prime, inversion, retrograde,
 * retrograde-inversion, matrix generation, hexachord analysis,
 * and combinatoriality detection.
 *
 * Invariants:
 * - Every ToneRow is a permutation of {0..11}
 * - row_prime(row, 0) == row
 * - row_retrograde(row_retrograde(row, 0), 0) == row
 * - hexachord(row, true) ∪ hexachord(row, false) == {0..11}
 * - R-combinatoriality always holds for any row
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"
#include "../Pitch/PTPS001A.h"

#include <array>
#include <span>

namespace Sunny::Core {

// =============================================================================
// Types
// =============================================================================

/**
 * @brief A twelve-tone row (permutation of all 12 pitch classes)
 */
struct ToneRow {
    std::array<PitchClass, 12> elements;

    bool operator==(const ToneRow&) const = default;
};

/**
 * @brief Row form type for dispatch
 */
enum class RowFormType { Prime, Inversion, Retrograde, RetrogradeInversion };

/**
 * @brief 12x12 tone row matrix
 *
 * Rows are prime forms; columns are inversion forms.
 */
struct ToneRowMatrix {
    std::array<std::array<PitchClass, 12>, 12> cells;
};

/**
 * @brief Result of combinatoriality analysis
 */
struct CombinatorialityResult {
    bool p_combinatorial;
    bool i_combinatorial;
    bool ri_combinatorial;
    bool all_combinatorial;
    int p_level;   ///< Transposition level for P-comb (-1 if none)
    int i_level;   ///< Inversion level for I-comb (-1 if none)
    int ri_level;  ///< RI level (-1 if none)
};

// =============================================================================
// Row Construction
// =============================================================================

/**
 * @brief Construct a tone row from pitch classes
 *
 * @param pcs Exactly 12 pitch classes, all distinct
 * @return ToneRow or InvalidToneRow
 */
[[nodiscard]] Result<ToneRow> make_tone_row(std::span<const PitchClass> pcs);

// =============================================================================
// Row Forms
// =============================================================================

/**
 * @brief P_n: transpose the row by n semitones
 */
[[nodiscard]] ToneRow row_prime(const ToneRow& row, int n);

/**
 * @brief I_n: invert each element as (n - element) mod 12
 */
[[nodiscard]] ToneRow row_inversion(const ToneRow& row, int n);

/**
 * @brief R_n: reverse of P_n
 */
[[nodiscard]] ToneRow row_retrograde(const ToneRow& row, int n);

/**
 * @brief RI_n: reverse of I_n
 */
[[nodiscard]] ToneRow row_retrograde_inversion(const ToneRow& row, int n);

/**
 * @brief Dispatch to a specific row form
 */
[[nodiscard]] ToneRow row_form(const ToneRow& row, RowFormType type, int n);

// =============================================================================
// Matrix
// =============================================================================

/**
 * @brief Generate the 12x12 tone row matrix
 *
 * Row i = P_n where n makes the first element equal to I_0[i].
 * Column j = I_n reading down.
 */
[[nodiscard]] ToneRowMatrix generate_matrix(const ToneRow& row);

/**
 * @brief Read row i from the matrix (a prime form)
 */
[[nodiscard]] ToneRow read_matrix_row(const ToneRowMatrix& matrix, int i);

/**
 * @brief Read column j from the matrix (an inversion form)
 */
[[nodiscard]] ToneRow read_matrix_column(const ToneRowMatrix& matrix, int j);

// =============================================================================
// Hexachord Analysis
// =============================================================================

/**
 * @brief Extract first or second hexachord (6 elements)
 *
 * @param row The tone row
 * @param first_half true = elements 0-5, false = elements 6-11
 * @return PitchClassSet containing the 6 pitch classes
 */
[[nodiscard]] PitchClassSet hexachord(const ToneRow& row, bool first_half);

/**
 * @brief Complement of a hexachord (the 6 PCs not in the set)
 */
[[nodiscard]] PitchClassSet hexachord_complement(const PitchClassSet& hex);

/**
 * @brief Analyse combinatoriality properties of a tone row
 */
[[nodiscard]] CombinatorialityResult analyse_combinatoriality(const ToneRow& row);

}  // namespace Sunny::Core
