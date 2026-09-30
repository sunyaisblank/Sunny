/**
 * @file scala.hpp
 * @brief Scala tuning file (.scl) reader/writer
 *
 *
 * Reads and writes Scala tuning definition files. The .scl format defines
 * microtuning scales as a sequence of intervals (cents or ratios) from unison.
 *
 * Invariants:
 * - parse_scala ∘ write_scala = identity (round-trip)
 * - ratio_to_cents(2, 1) = 1200.0
 * - scala_to_cent_table only succeeds for 12-note tunings with a 1200-cent period
 */

#pragma once

#include <string>
#include <string_view>
#include <sunny/core/score/tuning.hpp>
#include <sunny/core/tuning/historical_temperament.hpp>
#include <sunny/core/types/music_types.hpp>
#include <utility>
#include <vector>

namespace sunny::infrastructure::formats {

/// A single interval in a Scala file
struct ScalaInterval {
    double cents = 0.0;        ///< interval in cents from unison
    bool is_ratio = false;     ///< parsed as ratio (p/q) vs cents
    int ratio_num = 0;         ///< numerator if ratio
    int ratio_den = 0;         ///< denominator if ratio
    std::string trailing_text; ///< optional degree annotation after horizontal whitespace

    ScalaInterval() = default;
    ScalaInterval(
        double cents_value, bool ratio, int numerator, int denominator, std::string annotation = {})
        : cents(cents_value), is_ratio(ratio), ratio_num(numerator), ratio_den(denominator),
          trailing_text(std::move(annotation)) {}
};

/// Parsed Scala tuning definition
struct ScalaTuning {
    std::string description;
    std::vector<ScalaInterval> intervals; ///< n intervals (including the formal period)
};

/// Convert a positive frequency ratio to cents: 1200 * log2(num/den)
[[nodiscard]] sunny::core::Result<double> ratio_to_cents(int num, int den);

/// Parse .scl text into a ScalaTuning
[[nodiscard]] sunny::core::Result<ScalaTuning> parse_scala(std::string_view text);

/// Serialise a valid ScalaTuning to .scl text
[[nodiscard]] sunny::core::Result<std::string> write_scala(const ScalaTuning& tuning);

/// Map a 12-note, octave-periodic tuning to cent deviations from 12-TET
/// Returns FormatError unless there are exactly 12 intervals and the last is 1200 cents
[[nodiscard]] sunny::core::Result<sunny::core::TuningTable>
scala_to_cent_table(const ScalaTuning& tuning);

/**
 * Expand a non-empty Scala scale over Score's complete MIDI-note domain.
 * The reference key is Scala degree zero; each adjacent MIDI index advances
 * one degree, and the final Scala interval is the repeating formal period.
 */
[[nodiscard]] sunny::core::Result<sunny::core::ScoreTuning> scala_to_score_tuning(
    const ScalaTuning& tuning,
    std::uint8_t reference_note = sunny::core::SCORE_TUNING_DEFAULT_REFERENCE_NOTE,
    double reference_frequency_hz = sunny::core::SCORE_TUNING_DEFAULT_REFERENCE_HZ);

} // namespace sunny::infrastructure::formats
