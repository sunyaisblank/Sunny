/**
 * @file TSSC001A.cpp
 * @brief Unit tests for Species Counterpoint (§7.6)
 *
 * Component: TSSC001A
 * Tests: VLSC001A
 * Validates: Formal Spec §7.6 (Species 1–5)
 */

#include <catch2/catch_test_macros.hpp>

#include "VoiceLeading/VLSC001A.h"

using namespace Sunny::Core;

// =============================================================================
// Interval classification helpers
// =============================================================================

TEST_CASE("VLSC001A: perfect consonance classification", "[species][core]") {
    REQUIRE(is_perfect_consonance(0));    // P1
    REQUIRE(is_perfect_consonance(7));    // P5
    REQUIRE(is_perfect_consonance(12));   // P8
    REQUIRE(is_perfect_consonance(19));   // P5 + P8
    REQUIRE_FALSE(is_perfect_consonance(3));   // m3
    REQUIRE_FALSE(is_perfect_consonance(4));   // M3
    REQUIRE_FALSE(is_perfect_consonance(6));   // TT
}

TEST_CASE("VLSC001A: imperfect consonance classification", "[species][core]") {
    REQUIRE(is_imperfect_consonance(3));   // m3
    REQUIRE(is_imperfect_consonance(4));   // M3
    REQUIRE(is_imperfect_consonance(8));   // m6
    REQUIRE(is_imperfect_consonance(9));   // M6
    REQUIRE_FALSE(is_imperfect_consonance(0));  // P1
    REQUIRE_FALSE(is_imperfect_consonance(7));  // P5
    REQUIRE_FALSE(is_imperfect_consonance(6));  // TT
}

TEST_CASE("VLSC001A: is_consonant combines perfect and imperfect", "[species][core]") {
    REQUIRE(is_consonant(0));    // P1
    REQUIRE(is_consonant(3));    // m3
    REQUIRE(is_consonant(4));    // M3
    REQUIRE(is_consonant(7));    // P5
    REQUIRE(is_consonant(8));    // m6
    REQUIRE(is_consonant(9));    // M6
    REQUIRE(is_consonant(12));   // P8
    REQUIRE_FALSE(is_consonant(1));    // m2
    REQUIRE_FALSE(is_consonant(2));    // M2
    REQUIRE_FALSE(is_consonant(5));    // P4
    REQUIRE_FALSE(is_consonant(6));    // TT
    REQUIRE_FALSE(is_consonant(10));   // m7
    REQUIRE_FALSE(is_consonant(11));   // M7
}

TEST_CASE("VLSC001A: is_step identifies stepwise motion", "[species][core]") {
    REQUIRE(is_step(1));
    REQUIRE(is_step(2));
    REQUIRE(is_step(-1));
    REQUIRE(is_step(-2));
    REQUIRE_FALSE(is_step(0));
    REQUIRE_FALSE(is_step(3));
    REQUIRE_FALSE(is_step(-3));
}

// =============================================================================
// First Species
// =============================================================================

TEST_CASE("VLSC001A: first species valid counterpoint", "[species][core]") {
    // CF:  C4  D4  E4  D4  C4
    // CP:  C5  A4  C5  B4  C5
    // Intervals: P8, P5, m6, M6, P8 — all consonant
    // Begin=P8, end=P8. Approach: B4→C5 = +1 step ✓
    // No parallel fifths/octaves (voices move in contrary/oblique motion)
    std::vector<MidiNote> cf = {60, 62, 64, 62, 60};
    std::vector<MidiNote> cp = {72, 69, 72, 71, 72};

    auto result = check_first_species(cf, cp);
    REQUIRE(result.valid);
    REQUIRE(result.violations.empty());
}

TEST_CASE("VLSC001A: first species detects dissonance", "[species][core]") {
    // CF: C4 D4 C4
    // CP: C5 E4 C5
    // m1: E4(64) above D4(62) = 2 semitones (M2) → dissonant
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {72, 64, 72};

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "consonance" && viol.measure == 1) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("VLSC001A: first species detects parallel fifths", "[species][core]") {
    // CF: C4 D4 E4
    // CP: G4 A4 B4  → all P5, all same direction → parallel fifths
    std::vector<MidiNote> cf = {60, 62, 64};
    std::vector<MidiNote> cp = {67, 69, 71};

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found_parallel = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "parallel_fifths") found_parallel = true;
    }
    REQUIRE(found_parallel);
}

TEST_CASE("VLSC001A: first species detects parallel octaves", "[species][core]") {
    // CF: C4 D4 E4
    // CP: C5 D5 E5  → all P8, same direction → parallel octaves
    std::vector<MidiNote> cf = {60, 62, 64};
    std::vector<MidiNote> cp = {72, 74, 76};

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found_parallel = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "parallel_octaves") found_parallel = true;
    }
    REQUIRE(found_parallel);
}

TEST_CASE("VLSC001A: first species requires perfect consonance at start and end", "[species][core]") {
    // Start on M3 (not perfect), end on m3 (not perfect)
    // CF: C4 D4 C4
    // CP: E4 F4 Eb4  → M3, m3, m3. Approach: F4→Eb4 = -1, step ✓
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {64, 65, 63};

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool begin_viol = false, end_viol = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "begin_consonance") begin_viol = true;
        if (viol.rule == "end_consonance") end_viol = true;
    }
    REQUIRE(begin_viol);
    REQUIRE(end_viol);
}

TEST_CASE("VLSC001A: first species detects voice crossing", "[species][core]") {
    // CP above but goes below CF
    // CF: C4  D4  C4
    // CP: G4  C4  G4  → m1: CP=C4=CF=D4? No, CP=60, CF=62 → CP < CF → crossing
    // Actually CP(C4=60) below CF(D4=62)
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {72, 60, 72};  // m1: 60 < 62 → crossing

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "voice_crossing") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("VLSC001A: first species final approach by step", "[species][core]") {
    // CF: C4 D4 C4
    // CP: C5 A4 C5  → final approach A4(69)→C5(72) = +3, not a step
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {72, 69, 72};

    auto result = check_first_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "final_approach") found = true;
    }
    REQUIRE(found);
}

// =============================================================================
// Second Species
// =============================================================================

TEST_CASE("VLSC001A: second species valid counterpoint", "[species][core]") {
    // CF:  C4    D4    C4
    // CP:  G4 E4  F4 A4  G4 E4
    // Strong beats: P5, m3, P5 — all consonant. Begin/end P5 ✓
    // Weak beats: M3, P5, M3 — all consonant
    // No parallel P5/P8 between strong beats (contrary motion)

    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {67, 64, 65, 69, 67, 64};

    auto result = check_second_species(cf, cp);
    REQUIRE(result.valid);
    REQUIRE(result.violations.empty());
}

TEST_CASE("VLSC001A: second species strong beat dissonance", "[species][core]") {
    // Strong beat dissonant: m2 interval
    // CF: C4 D4 C4
    // CP: Db4 ... (61 vs 60 = 1 = m2)
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {61, 64, 69, 67, 72, 71};

    auto result = check_second_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "strong_consonance") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("VLSC001A: second species dissonant passing tone requires step", "[species][core]") {
    // CF: C4  D4  C4
    // CP strong beats: G4 A4 G4 → P5, then 69-62=7=P5, then P5
    // Weak beat at m0: make it dissonant and reached by leap
    // G4(67) → F#4(66): F#4 vs C4 = 6 = TT → dissonant. 67→66 = -1 = step, OK for approach
    //   But 66→A4(69) = +3 = not step → violation

    // Actually let me make the approach a leap:
    // CP: G4 C5  A4 ... → G4(67) to C5(72) = +5 (leap), C5 vs C4 = 12 = P8 → consonant, no issue
    // Need weak beat dissonant AND approach by leap:
    // CP: G4 F4  A4 B4  G4 A4
    // Weak m0: F4(65) vs C4(60) = 5 = P4 → dissonant in 2-voice. Approach: G4→F4 = -2 (step)
    // Continuation: F4→A4 = +4 (not step) → violation

    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {67, 65, 69, 67, 67, 64};
    // m0: strong G4(67) vs C4=P5 ✓, weak F4(65) vs C4=5=P4 dissonant
    //   approach: 67→65=-2 step ✓, continuation: 65→69=+4 NOT step → violation
    // m1: strong A4(69) vs D4=7=P5 ✓, weak G4(67) vs D4=5=P4 dissonant
    //   approach: 69→67=-2 step ✓, continuation: 67→67=0 NOT step → violation
    // m2: strong G4(67) vs C4=7=P5, weak E4(64) vs C4=4=M3 ✓

    auto result = check_second_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "passing_tone") found = true;
    }
    REQUIRE(found);
}

// =============================================================================
// Third Species
// =============================================================================

TEST_CASE("VLSC001A: third species valid counterpoint", "[species][core]") {
    // CF:  C4              D4                C4
    // CP:  C5 A4 G4 E4     A4 B4 A4 F#4     C5 B4 C5 G4
    // Downbeats: P8, P5, P8 — all consonant. Begin/end P8 ✓
    // m2 beat 1: B4(71) vs C4 = M7 (dissonant) — valid neighbour tone
    //   approached by step (C5→B4=-1) ✓, left by step (B4→C5=+1) ✓
    // No parallel P5/P8 between downbeats (contrary motion)

    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {
        72, 69, 67, 64,
        69, 71, 69, 66,
        72, 71, 72, 67
    };

    auto result = check_third_species(cf, cp);
    REQUIRE(result.valid);
    REQUIRE(result.violations.empty());
}

TEST_CASE("VLSC001A: third species dissonant downbeat violation", "[species][core]") {
    // Downbeat dissonance: m2 interval
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {
        72, 69, 67, 64,
        63, 66, 69, 66,    // Downbeat: Eb4(63) vs D4(62) = 1 = m2 → dissonant
        72, 71, 72, 67
    };

    auto result = check_third_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "downbeat_consonance" && viol.measure == 1) found = true;
    }
    REQUIRE(found);
}

// =============================================================================
// Fourth Species
// =============================================================================

TEST_CASE("VLSC001A: fourth species valid suspension chain", "[species][core]") {
    // CF:  D4  C4  D4
    // Layout: cp[i*2] = strong beat, cp[i*2+1] = weak beat
    //
    // m0: strong=A4(69) vs D4(62)=7=P5 ✓ (consonant start)
    //     weak=G4(67)   vs D4(62)=5=P4→dissonant? No, must be consonant preparation.
    //     Actually 67-62=5=P4 is dissonant. Let me use different notes.
    //     weak=A4(69)   vs D4(62)=7=P5 ✓ consonant, and tied to next strong beat
    //
    // m1: strong=A4(69) vs C4(60)=9=M6 consonant (tied from m0 weak). Not a dissonant suspension.
    //     weak=G4(67)   vs C4(60)=7=P5 ✓ consonant
    //
    // m2: strong=G4(67) vs D4(62)=5=P4 → dissonant suspension!
    //     Tied from m1 weak (G4=67=G4 ✓). Resolves: weak=F#4(66), 67→66=-1 step down ✓
    //     F#4(66) vs D4(62)=4=M3 consonant ✓
    //
    // Begin: A4 vs D4 = P5 ✓. End strong: G4 vs D4 = P4... not perfect consonance (P4 is dissonant in 2-voice).
    // Hmm. Let me adjust m2 end. Actually I need more measures.

    // Let me do a clean 4-measure example:
    // CF:  C4    D4    E4    C4
    // m0: strong C5(72) weak B4(71)  → C5 vs C4=P8 ✓, B4 vs C4=11=M7 dissonant → bad weak beat

    // OK let me just do tied consonances (no actual dissonant suspensions):
    // CF:  C4    D4    C4
    // m0: strong=G4(67) weak=A4(69)  → 7=P5 ✓, 9=M6 ✓
    // m1: strong=A4(69) weak=G4(67)  → 69 tied from 69 ✓. A4vsD4=7=P5 consonant. G4vsD4=5=P4 dissonant → bad

    // Use cleaner notes:
    // CF:  C4    D4    C4
    // m0: strong=G4(67) weak=F4(65)   → 7=P5 ✓, 5=P4 dissonant. No good.

    // The issue is P4 being dissonant. Let me use thirds/sixths.
    // CF:  C4(60)  D4(62)  C4(60)
    // m0: strong=E4(64)  weak=F4(65)   → 4=M3 ✓, 5=P4 dissonant. Argh.

    // CF:  E4(64)  D4(62)  E4(64)
    // m0: strong=C5(72) weak=A4(69)  → 72-64=8=m6 ✓, 69-64=5=P4 dissonant.

    // P4 above is consistently dissonant. Use larger intervals.
    // CF:  C4(60) D4(62) C4(60)
    // m0: strong=C5(72) weak=B4(71)  → P8 ✓, 71-60=11=M7 dissonant → bad prep
    // m0: strong=C5(72) weak=A4(69)  → P8 ✓, 69-60=9=M6 ✓ consonant
    // m1: strong=A4(69) weak=G4(67)  → tied 69=69 ✓, 69-62=7=P5 ✓ consonant, 67-62=5=P4 dissonant → bad
    // m1: strong=A4(69) weak=F#4(66)  → 69-62=7=P5 ✓, 66-62=4=M3 ✓
    // m2: strong=F#4(66) weak=E4(64)  → tied 66=66 ✓, 66-60=6=TT → dissonant suspension!
    //   resolves: 66→64=-2 step down ✓, 64-60=4=M3 consonant ✓
    //   But end strong beat is dissonant (TT). Need P consonance at end.

    // This is getting complex. Let me do all-consonant fourth species (no dissonant suspensions):
    // CF:  C4(60) D4(62) C4(60)
    // m0: strong=G4(67) weak=A4(69)  → P5 ✓, M6 ✓
    // m1: strong=A4(69) weak=E4(64)  → tied 69=69 ✓, A4vsD4=P5 ✓, E4vsD4=2=M2 dissonant → bad
    // m1: strong=A4(69) weak=A4(69)  → P5 ✓, P5 ✓. Tied same note, OK.
    // m2: strong=A4(69) weak=G4(67)  → tied 69=69 ✓, 69-60=9=M6 ✓, 67-60=7=P5 ✓
    // Begin P5 ✓, End m2 strong: M6 → not perfect. Need P consonance.

    // Actually let me just make end strong perfect:
    // CF: C4(60) E4(64) C4(60)
    // m0: strong=G4(67) weak=G4(67)  → P5 ✓, P5 ✓
    // m1: strong=G4(67) weak=G4(67)  → tied ✓, 67-64=3=m3 ✓, m3 ✓
    // m2: strong=G4(67) weak=E4(64)  → tied ✓, 67-60=7=P5 ✓, 64-60=4=M3 ✓
    // Begin=P5 ✓, End strong=P5 ✓. All consonant. Valid!

    std::vector<MidiNote> cf = {60, 64, 60};
    std::vector<MidiNote> cp = {67, 67, 67, 67, 67, 64};

    auto result = check_fourth_species(cf, cp);
    REQUIRE(result.valid);
    REQUIRE(result.violations.empty());
}

TEST_CASE("VLSC001A: fourth species dissonant suspension resolves correctly", "[species][core]") {
    // M2 suspension (9→8 pattern) resolving down by step.
    // CF:  A3(57)  G3(55)  G3(55)
    // m0: strong=A4(69) weak=A4(69)  → 12=P8 ✓
    // m1: strong=A4(69) weak=G4(67)  → tied ✓, 69-55=14%12=2=M2 dissonant suspension
    //     resolves: 69→67=-2 step down ✓, 67-55=12=P8 ✓ consonant
    // m2: strong=G4(67) weak=G4(67)  → tied ✓, 67-55=12=P8 consonant
    // Begin P8 ✓, End strong P8 ✓

    std::vector<MidiNote> cf = {57, 55, 55};
    std::vector<MidiNote> cp = {69, 69, 69, 67, 67, 67};
    // m0: strong=A4(69) weak=A4(69) → 69-57=12=P8 ✓, P8 ✓
    // m1: strong=A4(69) weak=G4(67) → tied ✓, 69-55=14→14%12=2=M2 dissonant!
    //     resolves: 69→67=-2 step down ✓
    //     weak: 67-55=12→12%12=0=P1 → is_perfect_consonance(12)=true ✓
    // m2: strong=G4(67) weak=G4(67) → tied ✓, 67-55=12=P8 consonant, P8 ✓
    // Begin P8 ✓, End strong P8 ✓

    auto result = check_fourth_species(cf, cp);
    REQUIRE(result.valid);
}

TEST_CASE("VLSC001A: fourth species suspension must resolve down", "[species][core]") {
    // Dissonant suspension that resolves UP → violation
    // CF: D4(62)  E4(64)  D4(62)
    // m0: strong=A4(69) weak=A4(69) → P5 ✓
    // m1: strong=A4(69)→tied ✓, 69-64=5=P4 dissonant suspension
    //     weak=B4(71) → resolves UP 69→71=+2 → VIOLATION
    //     71-64=7=P5 consonant
    // m2: strong=B4(71)→tied ✓, 71-62=9=M6 consonant
    //     weak=A4(69), 69-62=7=P5 ✓
    // Begin P5 ✓, End M6 → not perfect → also violation

    std::vector<MidiNote> cf = {62, 64, 62};
    std::vector<MidiNote> cp = {69, 69, 69, 71, 71, 69};

    auto result = check_fourth_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "suspension_resolution") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("VLSC001A: fourth species tie requirement", "[species][core]") {
    // Strong beat NOT tied from previous weak beat
    // CF: C4 D4 C4
    // m0: strong=G4(67) weak=A4(69) → P5 ✓, M6 ✓
    // m1: strong=G4(67)≠A4(69) → tie violation
    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {67, 69, 67, 69, 67, 64};
    // m1 strong=67, m0 weak=69 → not tied

    auto result = check_fourth_species(cf, cp);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "suspension_tie") found = true;
    }
    REQUIRE(found);
}

// =============================================================================
// Fifth Species (Florid)
// =============================================================================

TEST_CASE("VLSC001A: fifth species valid mixed rhythm", "[species][core]") {
    // CF: C4(60)  D4(62)  C4(60)
    // notes_per_measure: 1, 2, 1
    // CP: C5(72)   A4(69) B4(71)   C5(72)
    // m0: 1 note, C5 vs C4 = P8 ✓
    // m1: 2 notes, A4 vs D4 = 7 = P5 ✓ (downbeat consonant)
    //     B4 vs D4 = 9 = M6 ✓ (weak beat consonant)
    // m2: 1 note, C5 vs C4 = P8 ✓
    // Begin P8 ✓, End P8 ✓

    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {72, 69, 71, 72};
    std::vector<int> npm = {1, 2, 1};

    auto result = check_fifth_species(cf, cp, npm);
    REQUIRE(result.valid);
    REQUIRE(result.violations.empty());
}

TEST_CASE("VLSC001A: fifth species dissonant weak beat must be stepwise", "[species][core]") {
    // CF: C4(60)  D4(62)  C4(60)
    // notes_per_measure: 1, 4, 1
    // m1 counterpoint: A4(69) C5(72) Bb4(70) F#4(66)
    //   Downbeat: A4 vs D4 = 7 = P5 ✓
    //   Beat 1: C5 vs D4 = 10 = m7 → dissonant, approach: 69→72=+3 NOT step → violation
    //   Beat 2: Bb4 vs D4 = 8 = m6 ✓
    //   Beat 3: F#4 vs D4 = 4 = M3 ✓

    std::vector<MidiNote> cf = {60, 62, 60};
    std::vector<MidiNote> cp = {72, 69, 72, 70, 66, 72};
    std::vector<int> npm = {1, 4, 1};

    auto result = check_fifth_species(cf, cp, npm);
    REQUIRE_FALSE(result.valid);

    bool found = false;
    for (auto& viol : result.violations) {
        if (viol.rule == "weak_step") found = true;
    }
    REQUIRE(found);
}

// =============================================================================
// Dispatch
// =============================================================================

TEST_CASE("VLSC001A: check_species dispatches correctly", "[species][core]") {
    std::vector<MidiNote> cf = {60, 64, 60};
    std::vector<MidiNote> cp = {67, 67, 67, 67, 67, 64};

    auto result = check_species(Species::Fourth, cf, cp);
    REQUIRE(result.has_value());
    REQUIRE(result->valid);
}

TEST_CASE("VLSC001A: check_species length mismatch returns error", "[species][core]") {
    std::vector<MidiNote> cf = {60, 62, 64};
    std::vector<MidiNote> cp = {67, 69};  // Wrong length for first species

    auto result = check_species(Species::First, cf, cp);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
}

TEST_CASE("VLSC001A: check_species fifth returns error (needs notes_per_measure)", "[species][core]") {
    std::vector<MidiNote> cf = {60, 62};
    std::vector<MidiNote> cp = {72, 69, 72};

    auto result = check_species(Species::Fifth, cf, cp);
    REQUIRE_FALSE(result.has_value());
}

// =============================================================================
// Counterpoint Below
// =============================================================================

TEST_CASE("VLSC001A: first species counterpoint below", "[species][core]") {
    // CF: C5(72)  D5(74)  C5(72)
    // CP: C4(60)  E4(64)  B3(59)  (below)
    // upper=CF, lower=CP
    // Intervals: 72-60=12=P8 ✓, 74-64=10=m7→dissonant!
    // Fix: CP: C4(60) G3(55) C4(60)
    // 72-60=12=P8, 74-55=19=7mod12=P5, 72-60=12=P8
    // Begin P8 ✓, End P8 ✓
    // Final approach: 55→60=+5 not step → violation
    // CP: C4(60) B3(59) C4(60) → 72-60=P8, 74-59=15=3mod12=m3 ✓, 72-60=P8
    // Begin P8 ✓, End P8 ✓. Approach: 59→60=+1 step ✓
    // Parallel: m0→m1: CF 72→74(+2), CP 60→59(-1) different dir ✓
    // m1→m2: CF 74→72(-2), CP 59→60(+1) different dir ✓. No crossing: CF always above ✓

    std::vector<MidiNote> cf = {72, 74, 72};
    std::vector<MidiNote> cp = {60, 59, 60};

    auto result = check_first_species(cf, cp, CounterpointPosition::Below);
    REQUIRE(result.valid);
}
