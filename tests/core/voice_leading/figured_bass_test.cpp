/**
 * @file figured_bass_test.cpp
 * @brief Figured Bass Realisation unit tests
 *
 * Validates: Formal Spec §7.7
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/voice_leading/figured_bass.hpp>
#include <sunny/core/voice_leading/voice_leading.hpp>

using namespace sunny::core;

// =============================================================================
// figured_bass_intervals shorthand lookup
// =============================================================================

TEST_CASE("root position intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("");
    REQUIRE(ivs.size() == 2);
    REQUIRE(ivs[0] == 3);
    REQUIRE(ivs[1] == 5);

    REQUIRE(figured_bass_intervals("5/3") == ivs);
    REQUIRE(figured_bass_intervals("53") == ivs);
}

TEST_CASE("first inversion intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("6");
    REQUIRE(ivs == std::vector<int>{3, 6});
    REQUIRE(figured_bass_intervals("6/3") == ivs);
}

TEST_CASE("second inversion intervals", "[figured-bass][core]") {
    auto ivs = figured_bass_intervals("6/4");
    REQUIRE(ivs == std::vector<int>{4, 6});
}

TEST_CASE("seventh chord intervals", "[figured-bass][core]") {
    REQUIRE(figured_bass_intervals("7") == std::vector<int>{3, 5, 7});
    REQUIRE(figured_bass_intervals("6/5") == std::vector<int>{3, 5, 6});
    REQUIRE(figured_bass_intervals("4/3") == std::vector<int>{3, 4, 6});
    REQUIRE(figured_bass_intervals("4/2") == std::vector<int>{2, 4, 6});
    REQUIRE(figured_bass_intervals("2") == std::vector<int>{2, 4, 6});
}

TEST_CASE("unknown shorthand returns empty", "[figured-bass][core]") {
    REQUIRE(figured_bass_intervals("xyz").empty());
}

// =============================================================================
// parse_figured_bass
// =============================================================================

TEST_CASE("parse standard shorthands", "[figured-bass][core]") {
    auto result = parse_figured_bass("7");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 3);
    REQUIRE(result->figures[0].interval == 3);
    REQUIRE(result->figures[1].interval == 5);
    REQUIRE(result->figures[2].interval == 7);
}

TEST_CASE("parse with accidentals", "[figured-bass][core]") {
    auto result = parse_figured_bass("#6");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 1);
    REQUIRE(result->figures[0].interval == 6);
    REQUIRE(result->figures[0].accidental == FigureAccidental::Sharp);
}

TEST_CASE("parse compound figures", "[figured-bass][core]") {
    auto result = parse_figured_bass("#6/b3");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 2);
    REQUIRE(result->figures[0].interval == 6);
    REQUIRE(result->figures[0].accidental == FigureAccidental::Sharp);
    REQUIRE(result->figures[1].interval == 3);
    REQUIRE(result->figures[1].accidental == FigureAccidental::Flat);
}

TEST_CASE("parse empty defaults to root position", "[figured-bass][core]") {
    auto result = parse_figured_bass("");
    REQUIRE(result.has_value());
    REQUIRE(result->figures.size() == 2);
}

// =============================================================================
// realise_figured_bass
// =============================================================================

TEST_CASE("root position C in C major", "[figured-bass][core]") {
    // Bass = C3 (48), figures = 5/3 → E, G above
    auto symbol = parse_figured_bass("5/3");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->bass == 48);
    REQUIRE(result->upper.size() == 2);

    // Upper voices should be E and G (pitch classes 4 and 7)
    REQUIRE(pitch_class(result->upper[0]) == 4); // E
    REQUIRE(pitch_class(result->upper[1]) == 7); // G

    // All above bass
    for (auto note : result->upper) {
        REQUIRE(note > 48);
    }
}

TEST_CASE("first inversion C in C major", "[figured-bass][core]") {
    // Bass = E3 (52), figures = 6 → G, C above (3rd + 6th above E)
    auto symbol = parse_figured_bass("6");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(52, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());

    // 3rd above E diatonically in C major = G (pc 7)
    // 6th above E diatonically in C major = C (pc 0)
    // Sorted ascending: C4 (60) < G4 (67)
    REQUIRE(pitch_class(result->upper[0]) == 0); // C
    REQUIRE(pitch_class(result->upper[1]) == 7); // G
}

TEST_CASE("seventh chord realisation", "[figured-bass][core]") {
    // Bass = G3 (55), figures = 7 → B, D, F above
    auto symbol = parse_figured_bass("7");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(55, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->upper.size() == 3);

    // 3rd above G = B (11), 5th above G = D (2), 7th above G = F (5)
    std::vector<PitchClass> upper_pcs;
    for (auto n : result->upper) {
        upper_pcs.push_back(pitch_class(n));
    }
    // Check pitch classes present (order may vary due to sorting)
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 11) == 1); // B
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 2) == 1);  // D
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 5) == 1);  // F
}

TEST_CASE("all_notes includes bass", "[figured-bass][core]") {
    auto symbol = parse_figured_bass("");
    auto result = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(result->all_notes.front() == 48);
    REQUIRE(result->all_notes.size() == result->upper.size() + 1);
}

TEST_CASE("all_notes sorted ascending", "[figured-bass][core]") {
    auto symbol = parse_figured_bass("7");
    auto result = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    for (std::size_t i = 1; i < result->all_notes.size(); ++i) {
        REQUIRE(result->all_notes[i] >= result->all_notes[i - 1]);
    }
}

TEST_CASE("accidental sharp raises pitch class", "[figured-bass][core]") {
    // Bass = C3 (48), figure = #3 → should be E# = F (pc 5) instead of E (pc 4)
    FiguredBassSymbol symbol;
    symbol.figures.push_back({3, FigureAccidental::Sharp});

    auto result = realise_figured_bass(48, symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(pitch_class(result->upper[0]) == 5); // F (E raised by semitone)
}

TEST_CASE("accidental flat lowers pitch class", "[figured-bass][core]") {
    // Bass = C3 (48), figure = b3 → should be Eb (pc 3) instead of E (pc 4)
    FiguredBassSymbol symbol;
    symbol.figures.push_back({3, FigureAccidental::Flat});

    auto result = realise_figured_bass(48, symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    REQUIRE(pitch_class(result->upper[0]) == 3); // Eb
}

TEST_CASE("invalid bass note is unrepresentable", "[figured-bass][core]") {
    // The old runtime rejection (bass 200 → error) has moved to the type
    // boundary: an out-of-range bass cannot be constructed at all, so
    // realise_figured_bass can no longer receive one.
    auto bass = MidiNote::from_int(200);
    REQUIRE_FALSE(bass.has_value());
    REQUIRE(bass.error() == ErrorCode::InvalidMidiNote);
}

TEST_CASE("second inversion 6/4", "[figured-bass][core]") {
    // Bass = G3 (55), figures = 6/4 → C, E above (4th + 6th above G)
    auto symbol = parse_figured_bass("6/4");
    REQUIRE(symbol.has_value());

    auto result = realise_figured_bass(55, *symbol, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());

    // 4th above G diatonically = C (pc 0)
    // 6th above G diatonically = E (pc 4)
    std::vector<PitchClass> upper_pcs;
    for (auto n : result->upper) {
        upper_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 0) == 1); // C
    REQUIRE(std::count(upper_pcs.begin(), upper_pcs.end(), 4) == 1); // E
}

// =============================================================================
// realise_figured_bass_sequence
// =============================================================================

TEST_CASE("single-event sequence matches direct realisation", "[figured-bass][sequence][core]") {
    auto symbol = parse_figured_bass("5/3");
    REQUIRE(symbol.has_value());

    FiguredBassEvent event{48, *symbol}; // C3, root position

    auto seq =
        realise_figured_bass_sequence(std::span<const FiguredBassEvent>(&event, 1), 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 1);

    auto direct = realise_figured_bass(48, *symbol, 0, SCALE_MAJOR);
    REQUIRE(direct.has_value());

    // Should produce same bass and pitch classes
    REQUIRE(seq->realisations[0].bass == direct->bass);
    REQUIRE(seq->realisations[0].upper.size() == direct->upper.size());
    for (std::size_t i = 0; i < direct->upper.size(); ++i) {
        REQUIRE(pitch_class(seq->realisations[0].upper[i]) == pitch_class(direct->upper[i]));
    }
}

TEST_CASE("two-chord I-V sequence in C major", "[figured-bass][sequence][core]") {
    // I: bass C3 (48), root position → E, G
    // V: bass G2 (43), root position → B, D
    auto sym_root = parse_figured_bass("5/3");
    REQUIRE(sym_root.has_value());

    std::vector<FiguredBassEvent> events = {
        {48, *sym_root}, // I
        {43, *sym_root}, // V
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 2);

    // First chord: bass C3, upper voices E and G
    REQUIRE(seq->realisations[0].bass == 48);

    // Second chord: bass G2, upper voices should contain B and D pitch classes
    REQUIRE(seq->realisations[1].bass == 43);
    std::vector<PitchClass> v_pcs;
    for (auto n : seq->realisations[1].upper) {
        v_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(v_pcs.begin(), v_pcs.end(), 11) == 1); // B
    REQUIRE(std::count(v_pcs.begin(), v_pcs.end(), 2) == 1);  // D
}

TEST_CASE("four-chord I-IV-V-I sequence", "[figured-bass][sequence][core]") {
    auto sym = parse_figured_bass("5/3");
    REQUIRE(sym.has_value());

    // I(C3) → IV(F2) → V(G2) → I(C3)
    std::vector<FiguredBassEvent> events = {
        {48, *sym}, // I:  C3
        {41, *sym}, // IV: F2
        {43, *sym}, // V:  G2
        {48, *sym}, // I:  C3
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 4);

    // Verify bass notes
    REQUIRE(seq->realisations[0].bass == 48);
    REQUIRE(seq->realisations[1].bass == 41);
    REQUIRE(seq->realisations[2].bass == 43);
    REQUIRE(seq->realisations[3].bass == 48);

    // Verify each realisation has correct number of upper voices
    for (auto& r : seq->realisations) {
        REQUIRE(r.upper.size() == 2);
        REQUIRE(r.all_notes.size() == 3);
        REQUIRE(r.all_notes.front() == r.bass);
    }

    // Verify pitch classes of final I chord
    std::vector<PitchClass> final_pcs;
    for (auto n : seq->realisations[3].upper) {
        final_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(final_pcs.begin(), final_pcs.end(), 4) == 1); // E
    REQUIRE(std::count(final_pcs.begin(), final_pcs.end(), 7) == 1); // G
}

TEST_CASE("sequence with inversions I-V65-I6", "[figured-bass][sequence][core]") {
    auto sym_root = parse_figured_bass("5/3");
    auto sym_65 = parse_figured_bass("6/5");
    auto sym_6 = parse_figured_bass("6");
    REQUIRE(sym_root.has_value());
    REQUIRE(sym_65.has_value());
    REQUIRE(sym_6.has_value());

    // I(C3) → V6/5(B2) → I6(E3)
    std::vector<FiguredBassEvent> events = {
        {48, *sym_root}, // I:    C3, 5/3
        {47, *sym_65},   // V6/5: B2, 6/5
        {52, *sym_6},    // I6:   E3, 6
    };

    auto seq = realise_figured_bass_sequence(events, 0, SCALE_MAJOR);
    REQUIRE(seq.has_value());
    REQUIRE(seq->realisations.size() == 3);

    // V6/5 on B: 3rd=D, 5th=F, 6th=G → pitch classes 2, 5, 7
    std::vector<PitchClass> v65_pcs;
    for (auto n : seq->realisations[1].upper) {
        v65_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 2) == 1); // D
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 5) == 1); // F
    REQUIRE(std::count(v65_pcs.begin(), v65_pcs.end(), 7) == 1); // G

    // I6 on E: 3rd=G, 6th=C → pitch classes 7, 0
    std::vector<PitchClass> i6_pcs;
    for (auto n : seq->realisations[2].upper) {
        i6_pcs.push_back(pitch_class(n));
    }
    REQUIRE(std::count(i6_pcs.begin(), i6_pcs.end(), 7) == 1); // G
    REQUIRE(std::count(i6_pcs.begin(), i6_pcs.end(), 0) == 1); // C
}

// =============================================================================
// diatonic_above edge cases (audit RC-F remediation)
// =============================================================================

TEST_CASE("figured bass rejects zero generic intervals", "[figured-bass][core]") {
    // generic_interval < 1 is out of domain (1 = unison, 2 = 2nd, etc.).
    // Invalid arithmetic is rejected instead of fabricating a key-root tone.
    FiguredBassSymbol sym;
    sym.figures.push_back({0, FigureAccidental::Natural});

    auto result = realise_figured_bass(48, sym, 0, SCALE_MAJOR);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::VoiceLeadingFailed);
}

TEST_CASE("diatonic_above negative modulo safety", "[figured-bass][core]") {
    // Test with a large generic interval to exercise the modulo path.
    // interval=8 on a 7-note scale wraps around: (bass_degree + 7) % 7.
    // This should produce the same pitch class as interval=1 (unison).
    FiguredBassSymbol sym;
    sym.figures.push_back({8, FigureAccidental::Natural});

    auto result = realise_figured_bass(48, sym, 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    // 8th above C in C major wraps to C (one octave up diatonically)
    REQUIRE(pitch_class(result->upper[0]) == 0);
}

TEST_CASE("figured bass preserves chromatic letter intervals explicitly",
          "[figured-bass][core][theory-domain]") {
    const auto symbol = parse_figured_bass("5/3");
    REQUIRE(symbol.has_value());
    const SpelledPitch c{0, 0, 4};
    const SpelledPitch bb{6, -1, 3};
    const SpelledPitch as{5, 1, 3};
    auto ambiguous = realise_figured_bass(58, *symbol, 0, SCALE_MAJOR);
    REQUIRE_FALSE(ambiguous.has_value());
    REQUIRE(ambiguous.error() == ErrorCode::InvalidSpelledPitch);
    auto flat = realise_figured_bass(bb, *symbol, c, SCALE_MAJOR);
    auto sharp = realise_figured_bass(as, *symbol, c, SCALE_MAJOR);
    REQUIRE(flat.has_value());
    REQUIRE(sharp.has_value());
    REQUIRE(flat->upper == std::vector<MidiNote>{62, 65});  // D4/F4 above Bb3
    REQUIRE(sharp->upper == std::vector<MidiNote>{60, 64}); // C4/E4 above A#3
    auto altered = parse_figured_bass("#3/b5");
    REQUIRE(realise_figured_bass(bb, *altered, c, SCALE_MAJOR)->upper ==
            std::vector<MidiNote>{63, 64});

    // Written Cb3 and key Cb major require Eb/Gb, despite a B2 MIDI bass.
    const SpelledPitch cb{0, -1, 3};
    auto cb_major = realise_figured_bass(cb, *symbol, cb, SCALE_MAJOR);
    REQUIRE(cb_major->bass == 47);
    REQUIRE(cb_major->upper == std::vector<MidiNote>{51, 54});
    const SpelledPitch bs{6, 1, 3}, fs{3, 1, 4};
    REQUIRE(realise_figured_bass(bs, *symbol, fs, SCALE_MAJOR, 4)->upper ==
            std::vector<MidiNote>{63, 66}); // D#4/F#4 above B#3
}

TEST_CASE("figured bass rejects malformed figures and finite-domain violations",
          "[figured-bass][core][theory-domain]") {
    for (std::string_view text : {"3/", "/3", "3//5", "30", "#", "b", "10", "999", "3#5"}) {
        INFO(text);
        REQUIRE_FALSE(parse_figured_bass(text).has_value());
    }
    for (int interval :
         {-1, 0, 10, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        FiguredBassSymbol invalid{{{interval, FigureAccidental::Natural}}};
        REQUIRE(realise_figured_bass(48, invalid, 0, SCALE_MAJOR).error() ==
                ErrorCode::VoiceLeadingFailed);
    }
    const auto symbol = parse_figured_bass("5/3");
    for (int octave : {-2, 10, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        REQUIRE(realise_figured_bass(48, *symbol, 0, SCALE_MAJOR, octave).error() ==
                ErrorCode::InvalidOctave);
    }
    FiguredBassSymbol invalid_accidental{{{3, static_cast<FigureAccidental>(255)}}};
    REQUIRE_FALSE(realise_figured_bass(48, invalid_accidental, 0, SCALE_MAJOR).has_value());
    REQUIRE_FALSE(realise_figured_bass(48, FiguredBassSymbol{}, 0, SCALE_MAJOR).has_value());
    FiguredBassSymbol many;
    many.figures.assign(MAX_FIGURED_BASS_VOICES, {3, FigureAccidental::Natural});
    REQUIRE(realise_figured_bass(48, many, 0, SCALE_MAJOR)->upper.size() ==
            MAX_FIGURED_BASS_VOICES);
    many.figures.push_back({5, FigureAccidental::Natural});
    REQUIRE_FALSE(realise_figured_bass(48, many, 0, SCALE_MAJOR).has_value());

    const std::array<Interval, 7> duplicate{0, 2, 4, 5, 7, 9, 9};
    const std::array<Interval, 7> wide{0, 2, 4, 5, 7, 9, 12};
    REQUIRE(realise_figured_bass(48, *symbol, 0, duplicate).error() == ErrorCode::InvalidScaleName);
    REQUIRE(realise_figured_bass(48, *symbol, 0, wide).error() == ErrorCode::InvalidScaleName);
    REQUIRE(realise_figured_bass(SpelledPitch{7, 0, 3}, *symbol, SpelledPitch{0, 0, 4}, SCALE_MAJOR)
                .error() == ErrorCode::InvalidLetterName);
}

TEST_CASE("figured bass compound intervals and highest MIDI registers are checked",
          "[figured-bass][core][theory-domain]") {
    FiguredBassSymbol ninth{{{9, FigureAccidental::Natural}}};
    REQUIRE(realise_figured_bass(48, ninth, 0, SCALE_MAJOR, 0)->upper ==
            std::vector<MidiNote>{62}); // D4, never D3 for a ninth above C3.
    const auto triad = parse_figured_bass("5/3");
    auto top = realise_figured_bass(120, *triad, 0, SCALE_MAJOR);
    REQUIRE(top.has_value());
    REQUIRE(top->upper == std::vector<MidiNote>{124, 127});
    FiguredBassSymbol third{{{3, FigureAccidental::Natural}}};
    REQUIRE(realise_figured_bass(124, third, 0, SCALE_MAJOR)->upper == std::vector<MidiNote>{127});
    REQUIRE_FALSE(realise_figured_bass(125, third, 0, SCALE_MAJOR).has_value());
    FiguredBassSymbol unison{{{1, FigureAccidental::Natural}}};
    REQUIRE_FALSE(realise_figured_bass(127, unison, 0, SCALE_MAJOR).has_value());
    // All admitted MIDI basses either give a bounded, correctly ordered
    // realisation or a domain error; none can emit an upper note below bass.
    for (int bass = 0; bass <= 127; ++bass) {
        auto note = MidiNote::from_int(bass);
        auto result = realise_figured_bass(*note, *triad, 0, SCALE_MAJOR);
        if (result) {
            REQUIRE(result->upper.size() == 2);
            REQUIRE(std::is_sorted(result->all_notes.begin(), result->all_notes.end()));
            for (auto upper : result->upper)
                REQUIRE(upper > *note);
        } else {
            REQUIRE((result.error() == ErrorCode::InvalidSpelledPitch ||
                     result.error() == ErrorCode::InvalidMidiNote));
        }
    }
}

TEST_CASE("figured bass sequence respects raised bass and independently minimal motion",
          "[figured-bass][sequence][core][theory-domain]") {
    const auto triad = parse_figured_bass("5/3");
    const std::array<FiguredBassEvent, 2> jump{{{36, *triad}, {84, *triad}}};
    auto raised = realise_figured_bass_sequence(jump, 0, SCALE_MAJOR);
    REQUIRE(raised.has_value());
    REQUIRE(raised->realisations[0].upper == std::vector<MidiNote>{52, 55});
    REQUIRE(raised->realisations[1].upper == std::vector<MidiNote>{88, 91});

    const std::array<FiguredBassEvent, 2> cadence{{{48, *triad}, {43, *triad}}};
    auto led = realise_figured_bass_sequence(cadence, 0, SCALE_MAJOR);
    REQUIRE(led.has_value());
    const auto& source = led->realisations[0].upper;
    const auto& target = led->realisations[1].upper;
    // Independent exhaustive two-voice oracle: B>=47, D>=50, both
    // above G2, sorted, exactly one of each required pitch class.
    int minimum_motion = 1000;
    for (int low = 44; low <= 127; ++low) {
        for (int high = low; high <= 127; ++high) {
            if (!((low % 12 == 11 && high % 12 == 2 && low >= 47 && high >= 50) ||
                  (low % 12 == 2 && high % 12 == 11 && low >= 50 && high >= 47)))
                continue;
            minimum_motion = std::min(minimum_motion,
                                      std::abs(static_cast<int>(source[0]) - low) +
                                          std::abs(static_cast<int>(source[1]) - high));
        }
    }
    REQUIRE(minimum_motion == 6);
    REQUIRE(std::abs(static_cast<int>(source[0]) - static_cast<int>(target[0])) +
                std::abs(static_cast<int>(source[1]) - static_cast<int>(target[1])) ==
            minimum_motion);
    REQUIRE(target == std::vector<MidiNote>{62, 71});

    const std::array<SpelledFiguredBassEvent, 2> chromatic{
        {{SpelledPitch{0, 0, 3}, *triad}, {SpelledPitch{6, -1, 3}, *triad}}};
    auto spelled = realise_figured_bass_sequence(chromatic, SpelledPitch{0, 0, 4}, SCALE_MAJOR);
    REQUIRE(spelled.has_value());
    REQUIRE(spelled->realisations[1].upper == std::vector<MidiNote>{62, 65});
}
