/**
 * @file PTPS001A.cpp
 * @brief Pitch class set operations implementation
 *
 * Component: PTPS001A
 */

#include "PTPS001A.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Sunny::Core {

PitchClassSet pcs_transpose(const PitchClassSet& pcs, int n) {
    PitchClassSet result;
    result.reserve(pcs.size());
    for (auto pc : pcs) {
        result.insert(transpose(pc, n));
    }
    return result;
}

PitchClassSet pcs_invert(const PitchClassSet& pcs, int axis) {
    PitchClassSet result;
    result.reserve(pcs.size());
    for (auto pc : pcs) {
        result.insert(invert(pc, axis));
    }
    return result;
}

std::array<int, 6> pcs_interval_vector(const PitchClassSet& pcs) {
    std::array<int, 6> counts = {0, 0, 0, 0, 0, 0};

    std::vector<PitchClass> sorted(pcs.begin(), pcs.end());
    std::sort(sorted.begin(), sorted.end());

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        for (std::size_t j = i + 1; j < sorted.size(); ++j) {
            int ic = interval_class(sorted[j] - sorted[i]);
            if (ic >= 1 && ic <= 6) {
                counts[ic - 1]++;
            }
        }
    }

    return counts;
}

namespace {

// Helper: compute all rotations and find most compact
std::vector<PitchClass> find_normal_form_sorted(std::vector<PitchClass> sorted) {
    if (sorted.size() <= 1) return sorted;

    std::size_t n = sorted.size();
    std::vector<PitchClass> best = sorted;
    int best_span = 12;

    for (std::size_t r = 0; r < n; ++r) {
        // Rotate
        std::vector<PitchClass> rotated;
        rotated.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            int pc = (sorted[(r + i) % n] - sorted[r] + 12) % 12;
            rotated.push_back(static_cast<PitchClass>(pc));
        }

        int span = rotated.back();  // Already transposed to start at 0
        if (span < best_span) {
            best_span = span;
            best = rotated;
        } else if (span == best_span) {
            // Compare lexicographically
            if (rotated < best) {
                best = rotated;
            }
        }
    }

    return best;
}

}  // namespace

std::vector<PitchClass> pcs_normal_form(const PitchClassSet& pcs) {
    if (pcs.empty()) return {};

    std::vector<PitchClass> sorted(pcs.begin(), pcs.end());
    std::sort(sorted.begin(), sorted.end());

    return find_normal_form_sorted(sorted);
}

std::vector<PitchClass> pcs_prime_form(const PitchClassSet& pcs) {
    if (pcs.empty()) return {};

    // Get normal form
    auto normal = pcs_normal_form(pcs);

    // Get normal form of inversion
    auto inverted = pcs_invert(pcs, 0);
    auto inv_normal = pcs_normal_form(inverted);

    // Return the lexicographically smaller one
    return (normal <= inv_normal) ? normal : inv_normal;
}

bool pcs_t_equivalent(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;
    auto prime_a = pcs_normal_form(a);
    auto prime_b = pcs_normal_form(b);
    return prime_a == prime_b;
}

bool pcs_ti_equivalent(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;
    auto prime_a = pcs_prime_form(a);
    auto prime_b = pcs_prime_form(b);
    return prime_a == prime_b;
}

// =============================================================================
// Forte Number Catalogue
// =============================================================================

namespace {

// Encode a prime form as a 12-bit bitmask (bit i = pc i present)
uint16_t prime_form_to_bitmask(const std::vector<PitchClass>& pf) {
    uint16_t mask = 0;
    for (auto pc : pf) {
        mask |= static_cast<uint16_t>(1u << pc);
    }
    return mask;
}

// Forte catalogue: arrays of prime-form bitmasks, indexed by ordinal-1.
// Cardinality 2 (6 entries: 2-1 through 2-6)
constexpr std::array<uint16_t, 6> FORTE_C2 = {
    0x003, // 2-1: {0,1}
    0x005, // 2-2: {0,2}
    0x009, // 2-3: {0,3}
    0x011, // 2-4: {0,4}
    0x021, // 2-5: {0,5}
    0x041, // 2-6: {0,6}
};

// Cardinality 3 (12 entries: 3-1 through 3-12)
constexpr std::array<uint16_t, 12> FORTE_C3 = {
    0x007, // 3-1:  {0,1,2}
    0x00B, // 3-2:  {0,1,3}
    0x013, // 3-3:  {0,1,4}
    0x023, // 3-4:  {0,1,5}
    0x043, // 3-5:  {0,1,6}
    0x015, // 3-6:  {0,2,4}
    0x025, // 3-7:  {0,2,5}
    0x045, // 3-8:  {0,2,6}
    0x085, // 3-9:  {0,2,7}
    0x049, // 3-10: {0,3,6}
    0x089, // 3-11: {0,3,7}
    0x111, // 3-12: {0,4,8}
};

// Cardinality 4 (29 entries: 4-1 through 4-29)
constexpr std::array<uint16_t, 29> FORTE_C4 = {
    0x00F, // 4-1:  {0,1,2,3}
    0x017, // 4-2:  {0,1,2,4}
    0x01B, // 4-3:  {0,1,3,4}
    0x027, // 4-4:  {0,1,2,5}
    0x047, // 4-5:  {0,1,2,6}
    0x087, // 4-6:  {0,1,2,7}
    0x033, // 4-7:  {0,1,4,5}
    0x063, // 4-8:  {0,1,5,6}
    0x0C3, // 4-9:  {0,1,6,7}
    0x02D, // 4-10: {0,2,3,5}
    0x02B, // 4-11: {0,1,3,5}
    0x04D, // 4-12: {0,2,3,6}
    0x04B, // 4-13: {0,1,3,6}
    0x08D, // 4-14: {0,2,3,7}
    0x053, // 4-15: {0,1,4,6} Z
    0x0A3, // 4-16: {0,1,5,7}
    0x099, // 4-17: {0,3,4,7}
    0x093, // 4-18: {0,1,4,7}
    0x113, // 4-19: {0,1,4,8}
    0x123, // 4-20: {0,1,5,8}
    0x055, // 4-21: {0,2,4,6}
    0x095, // 4-22: {0,2,4,7}
    0x0A5, // 4-23: {0,2,5,7}
    0x115, // 4-24: {0,2,4,8}
    0x145, // 4-25: {0,2,6,8}
    0x129, // 4-26: {0,3,5,8}
    0x125, // 4-27: {0,2,5,8}
    0x249, // 4-28: {0,3,6,9}
    0x08B, // 4-29: {0,1,3,7} Z
};

// Cardinality 5 (38 entries: 5-1 through 5-38)
constexpr std::array<uint16_t, 38> FORTE_C5 = {
    0x01F, // 5-1:  {0,1,2,3,4}
    0x02F, // 5-2:  {0,1,2,3,5}
    0x037, // 5-3:  {0,1,2,4,5}
    0x04F, // 5-4:  {0,1,2,3,6}
    0x08F, // 5-5:  {0,1,2,3,7}
    0x067, // 5-6:  {0,1,2,5,6}
    0x0C7, // 5-7:  {0,1,2,6,7}
    0x05D, // 5-8:  {0,2,3,4,6}
    0x057, // 5-9:  {0,1,2,4,6}
    0x05B, // 5-10: {0,1,3,4,6}
    0x09D, // 5-11: {0,2,3,4,7}
    0x06B, // 5-12: {0,1,3,5,6} Z
    0x117, // 5-13: {0,1,2,4,8}
    0x0A7, // 5-14: {0,1,2,5,7}
    0x147, // 5-15: {0,1,2,6,8}
    0x09B, // 5-16: {0,1,3,4,7}
    0x11B, // 5-17: {0,1,3,4,8}
    0x0B3, // 5-18: {0,1,4,5,7}
    0x0CB, // 5-19: {0,1,3,6,7}
    0x163, // 5-20: {0,1,5,6,8}
    0x133, // 5-21: {0,1,4,5,8}
    0x193, // 5-22: {0,1,4,7,8}
    0x0AD, // 5-23: {0,2,3,5,7}
    0x0AB, // 5-24: {0,1,3,5,7}
    0x12D, // 5-25: {0,2,3,5,8}
    0x135, // 5-26: {0,2,4,5,8}
    0x12B, // 5-27: {0,1,3,5,8}
    0x14D, // 5-28: {0,2,3,6,8}
    0x14B, // 5-29: {0,1,3,6,8}
    0x153, // 5-30: {0,1,4,6,8}
    0x24B, // 5-31: {0,1,3,6,9}
    0x253, // 5-32: {0,1,4,6,9}
    0x155, // 5-33: {0,2,4,6,8}
    0x255, // 5-34: {0,2,4,6,9}
    0x295, // 5-35: {0,2,4,7,9}
    0x097, // 5-36: {0,1,2,4,7} Z
    0x139, // 5-37: {0,3,4,5,8}
    0x127, // 5-38: {0,1,2,5,8}
};

// Cardinality 6 (50 entries: 6-1 through 6-Z50)
// Hexachords are self-complementary in cardinality, so the
// complement derivation path does not apply. Full table required.
// Prime forms use the Rahn algorithm (consistent with pcs_prime_form).
// Z-related pairs: (Z3,Z36), (Z4,Z37), (Z6,Z38), (Z10,Z39),
//   (Z11,Z40), (Z12,Z41), (Z13,Z42), (Z17,Z43), (Z19,Z44),
//   (Z23,Z45), (Z24,Z46), (Z25,Z47), (Z26,Z48), (Z28,Z49), (Z29,Z50).
// Rahn–Forte disagreements: 6-Z29 uses {0,1,3,6,8,9} (Rahn),
//   6-31 uses {0,1,3,5,8,9} (Rahn).
constexpr std::array<uint16_t, 50> FORTE_C6 = {
    0x03F, // 6-1:   {0,1,2,3,4,5}     chromatic hexachord
    0x05F, // 6-2:   {0,1,2,3,4,6}
    0x06F, // 6-Z3:  {0,1,2,3,5,6}
    0x077, // 6-Z4:  {0,1,2,4,5,6}
    0x0CF, // 6-5:   {0,1,2,3,6,7}
    0x0E7, // 6-Z6:  {0,1,2,5,6,7}
    0x1C7, // 6-7:   {0,1,2,6,7,8}
    0x0BD, // 6-8:   {0,2,3,4,5,7}
    0x0AF, // 6-9:   {0,1,2,3,5,7}
    0x0BB, // 6-Z10: {0,1,3,4,5,7}
    0x0B7, // 6-Z11: {0,1,2,4,5,7}
    0x0D7, // 6-Z12: {0,1,2,4,6,7}
    0x0DB, // 6-Z13: {0,1,3,4,6,7}
    0x13B, // 6-14:  {0,1,3,4,5,8}
    0x137, // 6-15:  {0,1,2,4,5,8}
    0x173, // 6-16:  {0,1,4,5,6,8}
    0x197, // 6-Z17: {0,1,2,4,7,8}
    0x1A7, // 6-18:  {0,1,2,5,7,8}
    0x19B, // 6-Z19: {0,1,3,4,7,8}
    0x333, // 6-20:  {0,1,4,5,8,9}     hexatonic scale
    0x15D, // 6-21:  {0,2,3,4,6,8}
    0x157, // 6-22:  {0,1,2,4,6,8}
    0x16D, // 6-Z23: {0,2,3,5,6,8}
    0x15B, // 6-Z24: {0,1,3,4,6,8}
    0x16B, // 6-Z25: {0,1,3,5,6,8}
    0x1AB, // 6-Z26: {0,1,3,5,7,8}
    0x25B, // 6-27:  {0,1,3,4,6,9}
    0x26B, // 6-Z28: {0,1,3,5,6,9}
    0x34B, // 6-Z29: {0,1,3,6,8,9}     Rahn; Forte gives {0,2,3,6,7,9}
    0x2CB, // 6-30:  {0,1,3,6,7,9}     Petrushka chord
    0x32B, // 6-31:  {0,1,3,5,8,9}     Rahn; Forte gives {0,1,4,5,7,9}
    0x2B5, // 6-32:  {0,2,4,5,7,9}     major scale hexachord
    0x2AD, // 6-33:  {0,2,3,5,7,9}     Dorian hexachord
    0x2AB, // 6-34:  {0,1,3,5,7,9}     Mystic chord (Scriabin)
    0x555, // 6-35:  {0,2,4,6,8,10}    whole-tone scale
    0x09F, // 6-Z36: {0,1,2,3,4,7}     Z-mate of 6-Z3
    0x11F, // 6-Z37: {0,1,2,3,4,8}     Z-mate of 6-Z4
    0x18F, // 6-Z38: {0,1,2,3,7,8}     Z-mate of 6-Z6
    0x13D, // 6-Z39: {0,2,3,4,5,8}     Z-mate of 6-Z10
    0x12F, // 6-Z40: {0,1,2,3,5,8}     Z-mate of 6-Z11
    0x14F, // 6-Z41: {0,1,2,3,6,8}     Z-mate of 6-Z12
    0x24F, // 6-Z42: {0,1,2,3,6,9}     Z-mate of 6-Z13
    0x167, // 6-Z43: {0,1,2,5,6,8}     Z-mate of 6-Z17
    0x267, // 6-Z44: {0,1,2,5,6,9}     Z-mate of 6-Z19
    0x25D, // 6-Z45: {0,2,3,4,6,9}     Z-mate of 6-Z23
    0x257, // 6-Z46: {0,1,2,4,6,9}     Z-mate of 6-Z24
    0x297, // 6-Z47: {0,1,2,4,7,9}     Z-mate of 6-Z25
    0x2A7, // 6-Z48: {0,1,2,5,7,9}     Z-mate of 6-Z26
    0x29B, // 6-Z49: {0,1,3,4,7,9}     Z-mate of 6-Z28
    0x2D3, // 6-Z50: {0,1,4,6,7,9}     Z-mate of 6-Z29
};

// Check whether a Forte number carries the Z-designation.
// Z-related pairs share an interval class vector but differ in prime form.
bool is_z_related_ordinal(int card, int ordinal) {
    switch (card) {
        case 4: return ordinal == 15 || ordinal == 29;
        case 5: return ordinal == 12 || ordinal == 36
                    || ordinal == 17 || ordinal == 37
                    || ordinal == 18 || ordinal == 38;
        case 6: {
            // Z-related hexachords: Z3,Z36  Z4,Z37  Z6,Z38  Z10,Z39
            //   Z11,Z40  Z12,Z41  Z13,Z42  Z17,Z43  Z19,Z44
            //   Z23,Z45  Z24,Z46  Z25,Z47  Z26,Z48  Z28,Z49  Z29,Z50
            constexpr int z6[] = {3,4,6,10,11,12,13,17,19,23,24,25,26,28,29,
                                  36,37,38,39,40,41,42,43,44,45,46,47,48,49,50};
            for (int z : z6) {
                if (ordinal == z) return true;
            }
            return false;
        }
        default: return false;
    }
}

// Lookup in a single cardinality's table
std::optional<int> forte_lookup_in_table(uint16_t mask, std::span<const uint16_t> table) {
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i] == mask) {
            return static_cast<int>(i + 1);
        }
    }
    return std::nullopt;
}

// Get complement of a PCS (all 12 pitch classes not in the set)
PitchClassSet pcs_complement(const PitchClassSet& pcs) {
    PitchClassSet result;
    for (int pc = 0; pc < 12; ++pc) {
        if (pcs.find(static_cast<PitchClass>(pc)) == pcs.end()) {
            result.insert(static_cast<PitchClass>(pc));
        }
    }
    return result;
}

}  // namespace

std::optional<std::string> forte_number(const PitchClassSet& pcs) {
    std::size_t card = pcs.size();

    // Trivial cases
    if (card == 0) return "0-1";
    if (card == 1) return "1-1";
    if (card == 11) return "11-1";
    if (card == 12) return "12-1";

    // For cardinalities 7-10, use complement (same Forte ordinal)
    if (card >= 7 && card <= 10) {
        auto comp = pcs_complement(pcs);
        auto comp_forte = forte_number(comp);
        if (!comp_forte) return std::nullopt;

        // Replace cardinality prefix
        auto dash = comp_forte->find('-');
        if (dash == std::string::npos) return std::nullopt;
        return std::to_string(card) + comp_forte->substr(dash);
    }

    // Compute prime form and convert to bitmask
    auto pf = pcs_prime_form(pcs);
    uint16_t mask = prime_form_to_bitmask(pf);

    std::optional<int> ordinal;
    switch (card) {
        case 2:  ordinal = forte_lookup_in_table(mask, FORTE_C2); break;
        case 3:  ordinal = forte_lookup_in_table(mask, FORTE_C3); break;
        case 4:  ordinal = forte_lookup_in_table(mask, FORTE_C4); break;
        case 5:  ordinal = forte_lookup_in_table(mask, FORTE_C5); break;
        case 6:  ordinal = forte_lookup_in_table(mask, FORTE_C6); break;
        default: return std::nullopt;
    }

    if (!ordinal) return std::nullopt;
    std::string prefix = is_z_related_ordinal(static_cast<int>(card), *ordinal) ? "Z" : "";
    return std::to_string(card) + "-" + prefix + std::to_string(*ordinal);
}

bool pcs_z_related(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;

    // Same interval class vector
    auto iv_a = pcs_interval_vector(a);
    auto iv_b = pcs_interval_vector(b);
    if (iv_a != iv_b) return false;

    // Different prime forms (not TI-equivalent)
    return !pcs_ti_equivalent(a, b);
}

// =============================================================================
// Similarity Relations
// =============================================================================

bool similarity_R0(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;

    auto iv_a = pcs_interval_vector(a);
    auto iv_b = pcs_interval_vector(b);

    // All 6 entries must differ
    for (int i = 0; i < 6; ++i) {
        if (iv_a[i] == iv_b[i]) return false;
    }
    return true;
}

bool similarity_R1(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;

    auto iv_a = pcs_interval_vector(a);
    auto iv_b = pcs_interval_vector(b);

    int diff_count = 0;
    for (int i = 0; i < 6; ++i) {
        int diff = std::abs(iv_a[i] - iv_b[i]);
        if (diff != 0) {
            ++diff_count;
            if (diff != 1) return false;  // Must differ by exactly 1
        }
    }
    return diff_count == 1;
}

bool similarity_R2(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;

    auto iv_a = pcs_interval_vector(a);
    auto iv_b = pcs_interval_vector(b);

    int diff_count = 0;
    for (int i = 0; i < 6; ++i) {
        if (iv_a[i] != iv_b[i]) {
            ++diff_count;
        }
    }
    return diff_count == 1;
}

bool similarity_Rp(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size() || a.size() < 2) return false;

    // Check if A and B share n-1 elements under some transposition.
    // For each T_n, count how many elements of T_n(B) are in A.
    std::size_t target = a.size() - 1;

    for (int n = 0; n < 12; ++n) {
        auto t_b = pcs_transpose(b, n);
        std::size_t common = 0;
        for (auto pc : t_b) {
            if (a.count(pc)) ++common;
        }
        if (common >= target) return true;
    }
    return false;
}

}  // namespace Sunny::Core
