/**
 * @file SRTW001A.cpp
 * @brief Twelve-tone serialism implementation
 *
 * Component: SRTW001A
 */

#include "SRTW001A.h"

#include <algorithm>
#include <numeric>

namespace Sunny::Core {

Result<ToneRow> make_tone_row(std::span<const PitchClass> pcs) {
    if (pcs.size() != 12) {
        return std::unexpected(ErrorCode::InvalidToneRow);
    }

    std::array<bool, 12> seen{};
    ToneRow row{};

    for (std::size_t i = 0; i < 12; ++i) {
        if (pcs[i] >= 12) {
            return std::unexpected(ErrorCode::InvalidToneRow);
        }
        if (seen[pcs[i]]) {
            return std::unexpected(ErrorCode::InvalidToneRow);
        }
        seen[pcs[i]] = true;
        row.elements[i] = pcs[i];
    }

    return row;
}

ToneRow row_prime(const ToneRow& row, int n) {
    ToneRow result{};
    for (std::size_t i = 0; i < 12; ++i) {
        result.elements[i] = transpose(row.elements[i], n);
    }
    return result;
}

ToneRow row_inversion(const ToneRow& row, int n) {
    ToneRow result{};
    for (std::size_t i = 0; i < 12; ++i) {
        result.elements[i] = static_cast<PitchClass>(
            (n - static_cast<int>(row.elements[i]) + 24) % 12
        );
    }
    return result;
}

ToneRow row_retrograde(const ToneRow& row, int n) {
    ToneRow prime = row_prime(row, n);
    ToneRow result{};
    for (std::size_t i = 0; i < 12; ++i) {
        result.elements[i] = prime.elements[11 - i];
    }
    return result;
}

ToneRow row_retrograde_inversion(const ToneRow& row, int n) {
    ToneRow inv = row_inversion(row, n);
    ToneRow result{};
    for (std::size_t i = 0; i < 12; ++i) {
        result.elements[i] = inv.elements[11 - i];
    }
    return result;
}

ToneRow row_form(const ToneRow& row, RowFormType type, int n) {
    switch (type) {
        case RowFormType::Prime:                return row_prime(row, n);
        case RowFormType::Inversion:            return row_inversion(row, n);
        case RowFormType::Retrograde:           return row_retrograde(row, n);
        case RowFormType::RetrogradeInversion:  return row_retrograde_inversion(row, n);
    }
    return row;  // unreachable
}

ToneRowMatrix generate_matrix(const ToneRow& row) {
    ToneRowMatrix matrix{};

    // I_0: invert the row at level 0
    ToneRow i0 = row_inversion(row, 0);

    for (int i = 0; i < 12; ++i) {
        // Row i starts with I_0[i], so we need P_n where n = (I_0[i] - row[0] + 12) % 12
        int n = (static_cast<int>(i0.elements[i]) - static_cast<int>(row.elements[0]) + 12) % 12;
        ToneRow p = row_prime(row, n);
        matrix.cells[i] = p.elements;
    }

    return matrix;
}

ToneRow read_matrix_row(const ToneRowMatrix& matrix, int i) {
    ToneRow result{};
    result.elements = matrix.cells[i % 12];
    return result;
}

ToneRow read_matrix_column(const ToneRowMatrix& matrix, int j) {
    ToneRow result{};
    int col = j % 12;
    for (int i = 0; i < 12; ++i) {
        result.elements[i] = matrix.cells[i][col];
    }
    return result;
}

PitchClassSet hexachord(const ToneRow& row, bool first_half) {
    PitchClassSet result;
    int start = first_half ? 0 : 6;
    for (int i = start; i < start + 6; ++i) {
        result.insert(row.elements[i]);
    }
    return result;
}

PitchClassSet hexachord_complement(const PitchClassSet& hex) {
    PitchClassSet result;
    for (PitchClass pc = 0; pc < 12; ++pc) {
        if (hex.find(pc) == hex.end()) {
            result.insert(pc);
        }
    }
    return result;
}

namespace {

// Serial inversion I_n(x) = (n - x) mod 12  (differs from pcs_invert which uses 2n - x)
PitchClassSet serial_invert_set(const PitchClassSet& pcs, int n) {
    PitchClassSet result;
    for (auto pc : pcs) {
        result.insert(static_cast<PitchClass>((n - static_cast<int>(pc) + 24) % 12));
    }
    return result;
}

}  // namespace

CombinatorialityResult analyse_combinatoriality(const ToneRow& row) {
    CombinatorialityResult result{};
    result.p_combinatorial = false;
    result.i_combinatorial = false;
    result.ri_combinatorial = false;
    result.all_combinatorial = false;
    result.p_level = -1;
    result.i_level = -1;
    result.ri_level = -1;

    PitchClassSet h1 = hexachord(row, true);
    PitchClassSet complement = hexachord_complement(h1);

    // P-combinatoriality: T_n(H1) == complement for some n
    for (int n = 0; n < 12; ++n) {
        PitchClassSet transposed = pcs_transpose(h1, n);
        if (transposed == complement) {
            result.p_combinatorial = true;
            result.p_level = n;
            break;
        }
    }

    // I-combinatoriality: I_n(H1) == complement for some n
    // Using serial inversion: I_n(x) = (n - x) mod 12
    for (int n = 0; n < 12; ++n) {
        PitchClassSet inverted = serial_invert_set(h1, n);
        if (inverted == complement) {
            result.i_combinatorial = true;
            result.i_level = n;
            break;
        }
    }

    // RI-combinatoriality: I_n applied to the first hexachord of R_0
    // On the set level this reduces to checking I_n of the second hexachord == complement of second hexachord
    // But per spec: reverse P_0 first half elements, take as set, check I_n == complement
    PitchClassSet h2 = hexachord(row, false);
    PitchClassSet h2_complement = hexachord_complement(h2);
    for (int n = 0; n < 12; ++n) {
        PitchClassSet inv_h2 = serial_invert_set(h2, n);
        if (inv_h2 == h2_complement) {
            result.ri_combinatorial = true;
            result.ri_level = n;
            break;
        }
    }

    result.all_combinatorial =
        result.p_combinatorial && result.i_combinatorial && result.ri_combinatorial;

    return result;
}

}  // namespace Sunny::Core
