/**
 * @file VLSC001A.cpp
 * @brief Species Counterpoint Constraint Checker implementation
 *
 * Component: VLSC001A
 */

#include "VLSC001A.h"

#include <cmath>
#include <cstdlib>

namespace Sunny::Core {

namespace {

// Absolute interval in semitones
int abs_interval(MidiNote a, MidiNote b) {
    return std::abs(static_cast<int>(b) - static_cast<int>(a));
}

// Melodic interval (signed) from note a to note b in a single voice
int melodic_interval(MidiNote a, MidiNote b) {
    return static_cast<int>(b) - static_cast<int>(a);
}

// Detect parallel perfect consonances between two voice pairs
bool parallel_perfect(MidiNote cf1, MidiNote cp1, MidiNote cf2, MidiNote cp2) {
    int iv1 = abs_interval(cf1, cp1) % 12;
    int iv2 = abs_interval(cf2, cp2) % 12;
    // Both are the same perfect consonance (P5 or P8/P1)
    if (iv1 != iv2) return false;
    if (iv1 != 0 && iv1 != 7) return false;
    // Both voices must actually move
    if (cf1 == cf2 && cp1 == cp2) return false;
    // Same direction
    int cf_motion = melodic_interval(cf1, cf2);
    int cp_motion = melodic_interval(cp1, cp2);
    return (cf_motion > 0 && cp_motion > 0) || (cf_motion < 0 && cp_motion < 0);
}

// Direct (hidden) perfect consonances: both voices move in same direction
// to a perfect consonance, and the counterpoint does not move by step
bool direct_perfect(MidiNote cf1, MidiNote cp1, MidiNote cf2, MidiNote cp2) {
    int iv2 = abs_interval(cf2, cp2) % 12;
    if (iv2 != 0 && iv2 != 7) return false;
    int cf_motion = melodic_interval(cf1, cf2);
    int cp_motion = melodic_interval(cp1, cp2);
    bool same_dir = (cf_motion > 0 && cp_motion > 0) ||
                    (cf_motion < 0 && cp_motion < 0);
    if (!same_dir) return false;
    // Direct fifths/octaves are acceptable if counterpoint moves by step
    return !is_step(cp_motion);
}

void add_violation(std::vector<SpeciesViolation>& v, int measure, int beat,
                   const char* rule, const char* msg) {
    v.push_back({measure, beat, rule, msg});
}

}  // namespace

// =============================================================================
// First Species
// =============================================================================

SpeciesCheckResult check_first_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position
) {
    SpeciesCheckResult result{true, {}};
    auto& v = result.violations;

    if (cantus.size() != counterpoint.size() || cantus.size() < 2) {
        add_violation(v, 0, 0, "length", "Cantus and counterpoint must have equal length >= 2");
        result.valid = false;
        return result;
    }

    const auto n = cantus.size();

    // Determine upper/lower for interval computation
    auto upper = [&](std::size_t i) -> MidiNote {
        return position == CounterpointPosition::Above ? counterpoint[i] : cantus[i];
    };
    auto lower = [&](std::size_t i) -> MidiNote {
        return position == CounterpointPosition::Above ? cantus[i] : counterpoint[i];
    };

    // Rule: Begin on perfect consonance
    {
        int iv = abs_interval(lower(0), upper(0));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, 0, 0, "begin_consonance",
                "Must begin on a perfect consonance (P1, P5, P8)");
        }
    }

    // Rule: End on perfect consonance
    {
        int iv = abs_interval(lower(n - 1), upper(n - 1));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, static_cast<int>(n - 1), 0, "end_consonance",
                "Must end on a perfect consonance (P1, P5, P8)");
        }
    }

    // Rule: All intervals must be consonant
    for (std::size_t i = 0; i < n; ++i) {
        int iv = abs_interval(lower(i), upper(i));
        if (!is_consonant(iv)) {
            add_violation(v, static_cast<int>(i), 0, "consonance",
                "Vertical interval must be consonant");
        }
    }

    // Rule: No parallel P5 or P8
    for (std::size_t i = 1; i < n; ++i) {
        if (parallel_perfect(cantus[i - 1], counterpoint[i - 1],
                             cantus[i], counterpoint[i])) {
            int iv = abs_interval(cantus[i], counterpoint[i]) % 12;
            if (iv == 7) {
                add_violation(v, static_cast<int>(i), 0, "parallel_fifths",
                    "Parallel perfect fifths");
            } else {
                add_violation(v, static_cast<int>(i), 0, "parallel_octaves",
                    "Parallel perfect octaves/unisons");
            }
        }
    }

    // Rule: No direct (hidden) P5 or P8
    for (std::size_t i = 1; i < n; ++i) {
        if (direct_perfect(cantus[i - 1], counterpoint[i - 1],
                           cantus[i], counterpoint[i])) {
            add_violation(v, static_cast<int>(i), 0, "direct_perfect",
                "Direct (hidden) fifths or octaves");
        }
    }

    // Rule: No voice crossing
    for (std::size_t i = 0; i < n; ++i) {
        if (upper(i) < lower(i)) {
            add_violation(v, static_cast<int>(i), 0, "voice_crossing",
                "Voice crossing detected");
        }
    }

    // Rule: Approach final note by step
    if (n >= 2) {
        int final_motion = melodic_interval(counterpoint[n - 2], counterpoint[n - 1]);
        if (!is_step(final_motion)) {
            add_violation(v, static_cast<int>(n - 1), 0, "final_approach",
                "Final note must be approached by step");
        }
    }

    result.valid = v.empty();
    return result;
}

// =============================================================================
// Second Species
// =============================================================================

SpeciesCheckResult check_second_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position
) {
    SpeciesCheckResult result{true, {}};
    auto& v = result.violations;

    const auto n = cantus.size();
    if (counterpoint.size() != n * 2) {
        add_violation(v, 0, 0, "length",
            "Second species counterpoint must have 2 notes per cantus note");
        result.valid = false;
        return result;
    }
    if (n < 2) {
        add_violation(v, 0, 0, "length", "Cantus must have at least 2 notes");
        result.valid = false;
        return result;
    }

    auto upper = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cp : cf;
    };
    auto lower = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cf : cp;
    };

    // Strong beats (beat 0) must be consonant
    for (std::size_t i = 0; i < n; ++i) {
        MidiNote cp_strong = counterpoint[i * 2];
        int iv = abs_interval(lower(cantus[i], cp_strong), upper(cantus[i], cp_strong));
        if (!is_consonant(iv)) {
            add_violation(v, static_cast<int>(i), 0, "strong_consonance",
                "Strong beat must be consonant");
        }
    }

    // Weak beats: if dissonant, must be approached and left by step (passing tone)
    for (std::size_t i = 0; i < n; ++i) {
        MidiNote cp_weak = counterpoint[i * 2 + 1];
        int iv = abs_interval(lower(cantus[i], cp_weak), upper(cantus[i], cp_weak));
        if (!is_consonant(iv)) {
            // Must be a passing tone: stepwise approach from strong beat
            MidiNote cp_strong = counterpoint[i * 2];
            int approach = melodic_interval(cp_strong, cp_weak);
            if (!is_step(approach)) {
                add_violation(v, static_cast<int>(i), 1, "passing_tone",
                    "Dissonant weak beat must be approached by step");
            }
            // Must continue by step in the same direction
            if (i + 1 < n) {
                MidiNote cp_next = counterpoint[(i + 1) * 2];
                int continuation = melodic_interval(cp_weak, cp_next);
                if (!is_step(continuation)) {
                    add_violation(v, static_cast<int>(i), 1, "passing_tone",
                        "Dissonant weak beat must continue by step");
                }
            }
        }
    }

    // No parallel P5/P8 between successive strong beats
    for (std::size_t i = 1; i < n; ++i) {
        MidiNote cp_prev = counterpoint[(i - 1) * 2];
        MidiNote cp_curr = counterpoint[i * 2];
        if (parallel_perfect(cantus[i - 1], cp_prev, cantus[i], cp_curr)) {
            add_violation(v, static_cast<int>(i), 0, "parallel_perfect",
                "Parallel fifths or octaves between strong beats");
        }
    }

    // Begin on perfect consonance
    {
        MidiNote cp0 = counterpoint[0];
        int iv = abs_interval(lower(cantus[0], cp0), upper(cantus[0], cp0));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, 0, 0, "begin_consonance",
                "Must begin on a perfect consonance");
        }
    }

    // End on perfect consonance
    {
        // Last strong beat
        MidiNote cp_last = counterpoint[(n - 1) * 2];
        int iv = abs_interval(lower(cantus[n - 1], cp_last), upper(cantus[n - 1], cp_last));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, static_cast<int>(n - 1), 0, "end_consonance",
                "Must end on a perfect consonance");
        }
    }

    result.valid = v.empty();
    return result;
}

// =============================================================================
// Third Species
// =============================================================================

SpeciesCheckResult check_third_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position
) {
    SpeciesCheckResult result{true, {}};
    auto& v = result.violations;

    const auto n = cantus.size();
    if (counterpoint.size() != n * 4) {
        add_violation(v, 0, 0, "length",
            "Third species counterpoint must have 4 notes per cantus note");
        result.valid = false;
        return result;
    }
    if (n < 2) {
        add_violation(v, 0, 0, "length", "Cantus must have at least 2 notes");
        result.valid = false;
        return result;
    }

    auto upper = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cp : cf;
    };
    auto lower = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cf : cp;
    };

    // First beat of each measure must be consonant
    for (std::size_t i = 0; i < n; ++i) {
        MidiNote cp_down = counterpoint[i * 4];
        int iv = abs_interval(lower(cantus[i], cp_down), upper(cantus[i], cp_down));
        if (!is_consonant(iv)) {
            add_violation(v, static_cast<int>(i), 0, "downbeat_consonance",
                "Downbeat must be consonant");
        }
    }

    // Weak beats: dissonance permitted only as passing or neighbour tones (by step)
    for (std::size_t i = 0; i < n; ++i) {
        for (int b = 1; b < 4; ++b) {
            std::size_t idx = i * 4 + static_cast<std::size_t>(b);
            MidiNote cp = counterpoint[idx];
            int iv = abs_interval(lower(cantus[i], cp), upper(cantus[i], cp));
            if (!is_consonant(iv)) {
                // Must be approached by step
                MidiNote cp_prev = counterpoint[idx - 1];
                int approach = melodic_interval(cp_prev, cp);
                if (!is_step(approach)) {
                    add_violation(v, static_cast<int>(i), b, "weak_step",
                        "Dissonant weak beat must be approached by step");
                }
                // Must be left by step
                if (idx + 1 < counterpoint.size()) {
                    MidiNote cp_next = counterpoint[idx + 1];
                    int departure = melodic_interval(cp, cp_next);
                    if (!is_step(departure)) {
                        add_violation(v, static_cast<int>(i), b, "weak_step",
                            "Dissonant weak beat must be left by step");
                    }
                }
            }
        }
    }

    // No parallel P5/P8 between successive downbeats
    for (std::size_t i = 1; i < n; ++i) {
        MidiNote cp_prev = counterpoint[(i - 1) * 4];
        MidiNote cp_curr = counterpoint[i * 4];
        if (parallel_perfect(cantus[i - 1], cp_prev, cantus[i], cp_curr)) {
            add_violation(v, static_cast<int>(i), 0, "parallel_perfect",
                "Parallel fifths or octaves between downbeats");
        }
    }

    // Begin on perfect consonance
    {
        MidiNote cp0 = counterpoint[0];
        int iv = abs_interval(lower(cantus[0], cp0), upper(cantus[0], cp0));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, 0, 0, "begin_consonance",
                "Must begin on a perfect consonance");
        }
    }

    // End on perfect consonance
    {
        MidiNote cp_last = counterpoint[(n - 1) * 4];
        int iv = abs_interval(lower(cantus[n - 1], cp_last), upper(cantus[n - 1], cp_last));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, static_cast<int>(n - 1), 0, "end_consonance",
                "Must end on a perfect consonance");
        }
    }

    result.valid = v.empty();
    return result;
}

// =============================================================================
// Fourth Species
// =============================================================================

SpeciesCheckResult check_fourth_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position
) {
    SpeciesCheckResult result{true, {}};
    auto& v = result.violations;

    const auto n = cantus.size();
    if (counterpoint.size() != n * 2) {
        add_violation(v, 0, 0, "length",
            "Fourth species counterpoint must have 2 notes per cantus note");
        result.valid = false;
        return result;
    }
    if (n < 2) {
        add_violation(v, 0, 0, "length", "Cantus must have at least 2 notes");
        result.valid = false;
        return result;
    }

    // Layout: counterpoint[i*2] = strong beat (tied/held from previous weak beat)
    //         counterpoint[i*2+1] = weak beat (preparation for next suspension)

    auto upper = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cp : cf;
    };
    auto lower = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cf : cp;
    };

    // First strong beat: consonant
    {
        MidiNote cp0 = counterpoint[0];
        int iv = abs_interval(lower(cantus[0], cp0), upper(cantus[0], cp0));
        if (!is_consonant(iv)) {
            add_violation(v, 0, 0, "first_consonance",
                "First strong beat must be consonant");
        }
    }

    // For each measure from 1 onward, the strong beat is a suspension (tied from previous weak beat)
    for (std::size_t i = 1; i < n; ++i) {
        MidiNote cp_strong = counterpoint[i * 2];
        MidiNote cp_prev_weak = counterpoint[(i - 1) * 2 + 1];

        // The strong beat should be the same pitch as the previous weak beat (tied note)
        if (cp_strong != cp_prev_weak) {
            add_violation(v, static_cast<int>(i), 0, "suspension_tie",
                "Strong beat must be tied from previous weak beat");
        }

        // If the suspension is dissonant, it must resolve down by step on the weak beat
        int iv = abs_interval(lower(cantus[i], cp_strong), upper(cantus[i], cp_strong));
        if (!is_consonant(iv)) {
            MidiNote cp_weak = counterpoint[i * 2 + 1];
            int resolution = melodic_interval(cp_strong, cp_weak);
            if (resolution >= 0 || !is_step(resolution)) {
                add_violation(v, static_cast<int>(i), 0, "suspension_resolution",
                    "Dissonant suspension must resolve down by step");
            }
        }
    }

    // Weak beats must be consonant (they are preparation notes)
    for (std::size_t i = 0; i < n; ++i) {
        MidiNote cp_weak = counterpoint[i * 2 + 1];
        int iv = abs_interval(lower(cantus[i], cp_weak), upper(cantus[i], cp_weak));
        if (!is_consonant(iv)) {
            add_violation(v, static_cast<int>(i), 1, "weak_consonance",
                "Weak beat (preparation) must be consonant");
        }
    }

    // Begin on perfect consonance
    {
        MidiNote cp0 = counterpoint[0];
        int iv = abs_interval(lower(cantus[0], cp0), upper(cantus[0], cp0));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, 0, 0, "begin_consonance",
                "Must begin on a perfect consonance");
        }
    }

    // End on perfect consonance (last strong beat)
    {
        MidiNote cp_last = counterpoint[(n - 1) * 2];
        int iv = abs_interval(lower(cantus[n - 1], cp_last), upper(cantus[n - 1], cp_last));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, static_cast<int>(n - 1), 0, "end_consonance",
                "Must end on a perfect consonance");
        }
    }

    result.valid = v.empty();
    return result;
}

// =============================================================================
// Fifth Species (Florid)
// =============================================================================

SpeciesCheckResult check_fifth_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    std::span<const int> notes_per_measure,
    CounterpointPosition position
) {
    SpeciesCheckResult result{true, {}};
    auto& v = result.violations;

    const auto n = cantus.size();
    if (notes_per_measure.size() != n) {
        add_violation(v, 0, 0, "length",
            "notes_per_measure must match cantus length");
        result.valid = false;
        return result;
    }

    // Verify total counterpoint notes
    std::size_t total = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (notes_per_measure[i] < 1 || notes_per_measure[i] > 4) {
            add_violation(v, static_cast<int>(i), 0, "notes_per_measure",
                "Each measure must have 1-4 counterpoint notes");
            result.valid = false;
            return result;
        }
        total += static_cast<std::size_t>(notes_per_measure[i]);
    }
    if (counterpoint.size() != total) {
        add_violation(v, 0, 0, "length",
            "Counterpoint length must match sum of notes_per_measure");
        result.valid = false;
        return result;
    }
    if (n < 2) {
        add_violation(v, 0, 0, "length", "Cantus must have at least 2 notes");
        result.valid = false;
        return result;
    }

    auto upper = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cp : cf;
    };
    auto lower = [&](MidiNote cf, MidiNote cp) -> MidiNote {
        return position == CounterpointPosition::Above ? cf : cp;
    };

    // Walk through measures
    std::size_t cp_idx = 0;
    for (std::size_t i = 0; i < n; ++i) {
        int npm = notes_per_measure[i];

        // First note of each measure must be consonant
        MidiNote cp_first = counterpoint[cp_idx];
        int iv = abs_interval(lower(cantus[i], cp_first), upper(cantus[i], cp_first));
        if (!is_consonant(iv)) {
            add_violation(v, static_cast<int>(i), 0, "downbeat_consonance",
                "Downbeat must be consonant in florid counterpoint");
        }

        // Weak beats: dissonance by step only
        for (int b = 1; b < npm; ++b) {
            std::size_t idx = cp_idx + static_cast<std::size_t>(b);
            MidiNote cp = counterpoint[idx];
            int beat_iv = abs_interval(lower(cantus[i], cp), upper(cantus[i], cp));
            if (!is_consonant(beat_iv)) {
                MidiNote cp_prev = counterpoint[idx - 1];
                int approach = melodic_interval(cp_prev, cp);
                if (!is_step(approach)) {
                    add_violation(v, static_cast<int>(i), b, "weak_step",
                        "Dissonant weak beat must be approached by step");
                }
                if (idx + 1 < counterpoint.size()) {
                    MidiNote cp_next = counterpoint[idx + 1];
                    int departure = melodic_interval(cp, cp_next);
                    if (!is_step(departure)) {
                        add_violation(v, static_cast<int>(i), b, "weak_step",
                            "Dissonant weak beat must be left by step");
                    }
                }
            }
        }

        cp_idx += static_cast<std::size_t>(npm);
    }

    // Begin on perfect consonance
    {
        int iv = abs_interval(lower(cantus[0], counterpoint[0]),
                              upper(cantus[0], counterpoint[0]));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, 0, 0, "begin_consonance",
                "Must begin on a perfect consonance");
        }
    }

    // End on perfect consonance
    {
        // Last downbeat
        std::size_t last_start = counterpoint.size();
        for (std::size_t i = n; i > 0; --i) {
            last_start -= static_cast<std::size_t>(notes_per_measure[i - 1]);
            if (i == n) break;
        }
        MidiNote cp_last = counterpoint[last_start];
        int iv = abs_interval(lower(cantus[n - 1], cp_last),
                              upper(cantus[n - 1], cp_last));
        if (!is_perfect_consonance(iv)) {
            add_violation(v, static_cast<int>(n - 1), 0, "end_consonance",
                "Must end on a perfect consonance");
        }
    }

    result.valid = v.empty();
    return result;
}

// =============================================================================
// First Species Solver (backtracking)
// =============================================================================

namespace {

bool solve_bt(
    std::span<const MidiNote> cantus,
    std::vector<MidiNote>& cp,
    std::size_t idx,
    CounterpointPosition position,
    MidiNote lo,
    MidiNote hi
) {
    const auto n = cantus.size();
    const bool is_last = (idx == n - 1);

    auto upper = [&](std::size_t i) -> MidiNote {
        return position == CounterpointPosition::Above ? cp[i] : cantus[i];
    };
    auto lower = [&](std::size_t i) -> MidiNote {
        return position == CounterpointPosition::Above ? cantus[i] : cp[i];
    };

    for (MidiNote pitch = lo; pitch <= hi; ++pitch) {
        cp[idx] = pitch;

        // Voice crossing check
        if (position == CounterpointPosition::Above) {
            if (pitch < cantus[idx]) continue;
        } else {
            if (pitch > cantus[idx]) continue;
        }

        int iv = abs_interval(lower(idx), upper(idx));

        // First and last notes must be perfect consonances
        if (idx == 0 && !is_perfect_consonance(iv)) continue;
        if (is_last && !is_perfect_consonance(iv)) continue;

        // All notes must be consonant
        if (!is_consonant(iv)) continue;

        if (idx > 0) {
            // No parallel P5/P8
            if (parallel_perfect(cantus[idx - 1], cp[idx - 1],
                                 cantus[idx], cp[idx])) continue;

            // No direct (hidden) P5/P8
            if (direct_perfect(cantus[idx - 1], cp[idx - 1],
                               cantus[idx], cp[idx])) continue;
        }

        // Last note must be approached by step
        if (is_last && idx > 0) {
            int motion = melodic_interval(cp[idx - 1], cp[idx]);
            if (!is_step(motion)) continue;
        }

        // If we've placed all notes, we have a solution
        if (is_last) return true;

        // Recurse
        if (solve_bt(cantus, cp, idx + 1, position, lo, hi)) return true;
    }

    return false;
}

}  // namespace (solver helpers)

Result<CounterpointSolution> solve_first_species(
    std::span<const MidiNote> cantus,
    CounterpointPosition position,
    MidiNote pitch_low,
    MidiNote pitch_high
) {
    if (cantus.size() < 2) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }
    if (pitch_low > pitch_high) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::vector<MidiNote> cp(cantus.size(), 0);

    if (!solve_bt(cantus, cp, 0, position, pitch_low, pitch_high)) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    // Post-condition: validate with check_first_species
    auto check = check_first_species(cantus, cp, position);

    CounterpointSolution sol;
    sol.counterpoint = std::move(cp);
    sol.valid = check.valid;
    return sol;
}

// =============================================================================
// Dispatch
// =============================================================================

Result<SpeciesCheckResult> check_species(
    Species species,
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position
) {
    switch (species) {
        case Species::First:
            if (counterpoint.size() != cantus.size())
                return std::unexpected(ErrorCode::VoiceLeadingFailed);
            return check_first_species(cantus, counterpoint, position);

        case Species::Second:
            if (counterpoint.size() != cantus.size() * 2)
                return std::unexpected(ErrorCode::VoiceLeadingFailed);
            return check_second_species(cantus, counterpoint, position);

        case Species::Third:
            if (counterpoint.size() != cantus.size() * 4)
                return std::unexpected(ErrorCode::VoiceLeadingFailed);
            return check_third_species(cantus, counterpoint, position);

        case Species::Fourth:
            if (counterpoint.size() != cantus.size() * 2)
                return std::unexpected(ErrorCode::VoiceLeadingFailed);
            return check_fourth_species(cantus, counterpoint, position);

        case Species::Fifth:
            // Fifth species requires notes_per_measure; cannot dispatch generically
            return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    return std::unexpected(ErrorCode::VoiceLeadingFailed);
}

}  // namespace Sunny::Core
