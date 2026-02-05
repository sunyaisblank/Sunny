/**
 * @file TSBM001A.cpp
 * @brief Performance benchmarks for Sunny.Core operations
 *
 * Component: TSBM001A
 * Domain: TS (Test) | Category: BM (Benchmark)
 *
 * Benchmarks for critical hot paths:
 * - Pitch class operations (transpose, invert)
 * - Scale generation
 * - Euclidean rhythm (Bjorklund algorithm)
 * - Harmony analysis
 * - Voice leading
 * - Negative harmony
 *
 * Run with: ./Sunny.Test.Benchmark [benchmark]
 */

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "Pitch/PTPC001A.h"
#include "Pitch/PTPS001A.h"
#include "Scale/SCDF001A.h"
#include "Scale/SCGN001A.h"
#include "Rhythm/RHEU001A.h"
#include "Harmony/HRFN001A.h"
#include "Harmony/HRNG001A.h"
#include "VoiceLeading/VLNT001A.h"

using namespace Sunny::Core;

// =============================================================================
// Pitch Class Operations
// =============================================================================

TEST_CASE("TSBM001A: Pitch class transpose", "[benchmark][pitch]") {
    BENCHMARK("transpose single pitch class") {
        return transpose(PitchClass{0}, 7);
    };

    BENCHMARK("transpose 1000 pitch classes") {
        int sum = 0;
        for (int i = 0; i < 1000; ++i) {
            sum += static_cast<int>(transpose(static_cast<PitchClass>(i % 12), i % 12));
        }
        return sum;
    };
}

TEST_CASE("TSBM001A: Pitch class set operations", "[benchmark][pitch]") {
    PitchClassSet c_major = {0, 4, 7};

    BENCHMARK("pcs_transpose") {
        return pcs_transpose(c_major, 5);
    };

    BENCHMARK("pcs_invert") {
        return pcs_invert(c_major, 0);
    };

    BENCHMARK("pcs_interval_vector") {
        return pcs_interval_vector(c_major);
    };

    BENCHMARK("pcs_normal_form") {
        return pcs_normal_form(c_major);
    };

    BENCHMARK("pcs_prime_form") {
        return pcs_prime_form(c_major);
    };

    BENCHMARK("pcs_t_equivalent") {
        PitchClassSet f_major = {5, 9, 0};
        return pcs_t_equivalent(c_major, f_major);
    };
}

// =============================================================================
// Scale Generation
// =============================================================================

TEST_CASE("TSBM001A: Scale generation", "[benchmark][scale]") {
    BENCHMARK("generate C major scale") {
        return generate_scale_notes(0, SCALE_MAJOR, 4);
    };

    BENCHMARK("generate 2-octave range") {
        return generate_scale_range(0, SCALE_MAJOR, 4, 2);
    };

    BENCHMARK("quantize to scale") {
        return quantize_to_scale(61, 0, SCALE_MAJOR);
    };

    BENCHMARK("is_note_in_scale check") {
        return is_note_in_scale(64, 0, SCALE_MAJOR);
    };

    BENCHMARK("generate all 12 major scales") {
        int total = 0;
        for (int root = 0; root < 12; ++root) {
            auto result = generate_scale_notes(static_cast<PitchClass>(root), SCALE_MAJOR, 4);
            if (result) total += static_cast<int>(result->size());
        }
        return total;
    };
}

// =============================================================================
// Euclidean Rhythm
// =============================================================================

TEST_CASE("TSBM001A: Euclidean rhythm", "[benchmark][rhythm]") {
    BENCHMARK("E(3,8) - Tresillo") {
        return euclidean_rhythm(3, 8);
    };

    BENCHMARK("E(5,8) - Cinquillo") {
        return euclidean_rhythm(5, 8);
    };

    BENCHMARK("E(7,16) - Complex pattern") {
        return euclidean_rhythm(7, 16);
    };

    BENCHMARK("E(13,32) - Large pattern") {
        return euclidean_rhythm(13, 32);
    };

    BENCHMARK("Generate 100 different patterns") {
        int total = 0;
        for (int k = 1; k <= 10; ++k) {
            for (int n = k; n <= k + 9; ++n) {
                auto result = euclidean_rhythm(k, n);
                if (result) total += static_cast<int>(result->size());
            }
        }
        return total;
    };
}

// =============================================================================
// Harmony Analysis
// =============================================================================

TEST_CASE("TSBM001A: Harmony analysis", "[benchmark][harmony]") {
    PitchClassSet c_major_chord = {0, 4, 7};
    PitchClassSet dm7 = {2, 5, 9, 0};
    PitchClassSet g7 = {7, 11, 2, 5};

    BENCHMARK("analyze C major chord") {
        return analyze_chord_function(c_major_chord, 0);
    };

    BENCHMARK("analyze Dm7 chord") {
        return analyze_chord_function(dm7, 0);
    };

    BENCHMARK("analyze ii-V-I progression") {
        auto ii = analyze_chord_function(dm7, 0);
        auto V = analyze_chord_function(g7, 0);
        auto I = analyze_chord_function(c_major_chord, 0);
        return ii.degree + V.degree + I.degree;
    };

    BENCHMARK("analyze 72 triads (all roots x qualities)") {
        int total_degrees = 0;
        for (int root = 0; root < 12; ++root) {
            for (int third : {3, 4}) {
                for (int fifth : {6, 7, 8}) {
                    PitchClassSet chord = {
                        static_cast<PitchClass>(root),
                        static_cast<PitchClass>((root + third) % 12),
                        static_cast<PitchClass>((root + fifth) % 12)
                    };
                    auto result = analyze_chord_function(chord, 0);
                    total_degrees += result.degree;
                }
            }
        }
        return total_degrees;
    };
}

// =============================================================================
// Negative Harmony
// =============================================================================

TEST_CASE("TSBM001A: Negative harmony", "[benchmark][harmony]") {
    PitchClassSet c_major_chord = {0, 4, 7};
    PitchClassSet cmaj7 = {0, 4, 7, 11};

    BENCHMARK("negative harmony axis calculation") {
        return negative_harmony_axis(0);
    };

    BENCHMARK("negative harmony - triad") {
        return negative_harmony(c_major_chord, 0);
    };

    BENCHMARK("negative harmony - seventh chord") {
        return negative_harmony(cmaj7, 0);
    };

    BENCHMARK("negative harmony - all 12 keys") {
        int total = 0;
        for (int key = 0; key < 12; ++key) {
            auto result = negative_harmony(c_major_chord, static_cast<PitchClass>(key));
            total += static_cast<int>(result.size());
        }
        return total;
    };

    BENCHMARK("involution verification (f(f(x)) = x)") {
        auto neg1 = negative_harmony(c_major_chord, 0);
        auto neg2 = negative_harmony(neg1, 0);
        return neg2 == c_major_chord;
    };
}

// =============================================================================
// Voice Leading
// =============================================================================

TEST_CASE("TSBM001A: Voice leading", "[benchmark][voiceleading]") {
    std::vector<MidiNote> source = {60, 64, 67};
    std::vector<PitchClass> target_pcs = {5, 9, 0};  // F major

    BENCHMARK("voice lead nearest tone - 3 voices") {
        return voice_lead_nearest_tone(source, target_pcs);
    };

    BENCHMARK("voice lead nearest tone - 4 voices") {
        std::vector<MidiNote> source4 = {60, 64, 67, 71};
        std::vector<PitchClass> target4 = {2, 5, 9, 0};  // Dm7
        return voice_lead_nearest_tone(source4, target4);
    };

    BENCHMARK("generate close voicing") {
        std::vector<PitchClass> pcs = {0, 4, 7};
        return generate_close_voicing(pcs, 4);
    };

    BENCHMARK("generate drop-2 voicing") {
        std::vector<MidiNote> close = {60, 64, 67, 71};
        return generate_drop2_voicing(close);
    };
}

// =============================================================================
// Combined Operations (Real-world scenarios)
// =============================================================================

TEST_CASE("TSBM001A: Combined operations", "[benchmark][integration]") {
    BENCHMARK("generate scale + quantize melody") {
        auto scale = generate_scale_notes(0, SCALE_MAJOR, 4);
        if (!scale) return 0;

        int quantized = 0;
        for (int note = 60; note <= 72; ++note) {
            quantized += quantize_to_scale(note, 0, SCALE_MAJOR);
        }
        return quantized;
    };

    BENCHMARK("analyze progression + negative harmony") {
        std::vector<PitchClassSet> chords = {
            {2, 5, 9},
            {7, 11, 2},
            {0, 4, 7},
        };

        int total = 0;
        for (const auto& chord : chords) {
            auto analysis = analyze_chord_function(chord, 0);
            auto neg = negative_harmony(chord, 0);
            total += static_cast<int>(neg.size());
            total += analysis.degree;
        }
        return total;
    };

    BENCHMARK("euclidean + voice leading pattern") {
        auto rhythm = euclidean_rhythm(5, 8);
        if (!rhythm) return 0;

        std::vector<MidiNote> current = {60, 64, 67};
        std::vector<PitchClass> next_pcs = {2, 5, 9};  // Dm

        int steps = 0;
        for (bool hit : *rhythm) {
            if (hit) {
                auto voiced = voice_lead_nearest_tone(current, next_pcs);
                if (voiced) {
                    current = voiced->voiced_notes;
                    ++steps;
                }
            }
        }
        return steps;
    };
}
