/**
 * @file SCRN001A.cpp
 * @brief Scale relationships implementation
 *
 * Component: SCRN001A
 */

#include "SCRN001A.h"

#include <algorithm>
#include <numeric>
#include <unordered_set>

namespace Sunny::Core {

namespace {

// Build the set of absolute pitch classes for a scale rooted at 'root'
std::unordered_set<PitchClass> build_pc_set(
    PitchClass root, std::span<const Interval> intervals
) {
    std::unordered_set<PitchClass> pcs;
    for (auto iv : intervals) {
        pcs.insert(transpose(root, iv));
    }
    return pcs;
}

// Determine quality of a triad built on scale degree 'degree'
// by stacking thirds within the scale
std::string triad_quality_on_degree(
    PitchClass root,
    std::span<const Interval> intervals,
    int degree
) {
    if (intervals.size() < 3) return "?";

    int n = static_cast<int>(intervals.size());
    PitchClass deg_root = transpose(root, intervals[degree]);
    PitchClass deg_third = transpose(root, intervals[(degree + 2) % n]);
    PitchClass deg_fifth = transpose(root, intervals[(degree + 4) % n]);

    int third_interval = (static_cast<int>(deg_third) - static_cast<int>(deg_root) + 12) % 12;
    int fifth_interval = (static_cast<int>(deg_fifth) - static_cast<int>(deg_root) + 12) % 12;

    if (third_interval == 4 && fifth_interval == 7) return "major";
    if (third_interval == 3 && fifth_interval == 7) return "minor";
    if (third_interval == 3 && fifth_interval == 6) return "dim";
    if (third_interval == 4 && fifth_interval == 8) return "aug";

    return "?";
}

}  // namespace

std::optional<ScaleDegreeName> scale_degree_name(
    int degree,
    std::span<const Interval> intervals
) {
    if (intervals.size() != 7) return std::nullopt;
    if (degree < 0 || degree > 6) return std::nullopt;

    switch (degree) {
        case 0: return ScaleDegreeName::Tonic;
        case 1: return ScaleDegreeName::Supertonic;
        case 2: return ScaleDegreeName::Mediant;
        case 3: return ScaleDegreeName::Subdominant;
        case 4: return ScaleDegreeName::Dominant;
        case 5: return ScaleDegreeName::Submediant;
        case 6:
            if (intervals[6] == 11) return ScaleDegreeName::LeadingTone;
            return ScaleDegreeName::Subtonic;
    }

    return std::nullopt;
}

ScaleRelationship classify_scale_relationship(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
) {
    auto pcs1 = build_pc_set(r1, s1);
    auto pcs2 = build_pc_set(r2, s2);

    if (r1 == r2 && pcs1 != pcs2) return ScaleRelationship::Parallel;
    if (r1 != r2 && pcs1 == pcs2) return ScaleRelationship::Relative;

    return ScaleRelationship::None;
}

int scale_common_tone_count(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
) {
    auto pcs1 = build_pc_set(r1, s1);
    auto pcs2 = build_pc_set(r2, s2);

    int count = 0;
    for (auto pc : pcs1) {
        if (pcs2.count(pc)) ++count;
    }
    return count;
}

bool scale_is_subset(
    PitchClass r1, std::span<const Interval> s1,
    PitchClass r2, std::span<const Interval> s2
) {
    auto pcs1 = build_pc_set(r1, s1);
    auto pcs2 = build_pc_set(r2, s2);

    for (auto pc : pcs1) {
        if (pcs2.find(pc) == pcs2.end()) return false;
    }
    return true;
}

std::vector<BorrowedChord> find_borrowed_chords(
    PitchClass root,
    std::span<const Interval> primary,
    std::span<const Interval> parallel,
    std::string_view parallel_name
) {
    std::vector<BorrowedChord> result;

    if (parallel.size() < 5) return result;

    // Build triads available in the primary scale
    struct TriadId {
        PitchClass root;
        std::string quality;
        bool operator==(const TriadId& o) const {
            return root == o.root && quality == o.quality;
        }
    };

    std::vector<TriadId> primary_triads;
    int pn = static_cast<int>(primary.size());
    for (int d = 0; d < pn; ++d) {
        PitchClass dr = transpose(root, primary[d]);
        std::string q = triad_quality_on_degree(root, primary, d);
        if (q != "?") {
            primary_triads.push_back({dr, q});
        }
    }

    // Generate triads from the parallel scale and check for borrowing
    int parN = static_cast<int>(parallel.size());
    for (int d = 0; d < parN; ++d) {
        PitchClass dr = transpose(root, parallel[d]);
        std::string q = triad_quality_on_degree(root, parallel, d);
        if (q == "?") continue;

        bool found_in_primary = false;
        for (auto& pt : primary_triads) {
            if (pt.root == dr && pt.quality == q) {
                found_in_primary = true;
                break;
            }
        }

        if (!found_in_primary) {
            result.push_back({dr, q, d, std::string(parallel_name)});
        }
    }

    return result;
}

Result<GeneratedScaleResult> generate_scale_from_generator(
    PitchClass root,
    int generator,
    int cardinality
) {
    if (cardinality <= 0 || cardinality > 12) {
        return std::unexpected(ErrorCode::InvalidGeneratedScale);
    }

    // Normalise generator to [0,11]
    int g = ((generator % 12) + 12) % 12;

    std::unordered_set<PitchClass> seen;
    std::vector<PitchClass> pcs;

    for (int j = 0; j < cardinality; ++j) {
        PitchClass pc = static_cast<PitchClass>((static_cast<int>(root) + j * g) % 12);
        if (!seen.insert(pc).second) {
            return std::unexpected(ErrorCode::InvalidGeneratedScale);
        }
        pcs.push_back(pc);
    }

    std::sort(pcs.begin(), pcs.end());

    bool deep = has_deep_scale_property(generator, cardinality);

    return GeneratedScaleResult{std::move(pcs), deep};
}

bool has_deep_scale_property(int generator, int cardinality) {
    int g = ((generator % 12) + 12) % 12;
    int d = std::gcd(g, 12);
    return d == 1 && cardinality <= 12;
}

}  // namespace Sunny::Core
