/**
 * @file FMSL001A.h
 * @brief Scala tuning file (.scl) reader/writer
 *
 * Component: FMSL001A
 * Domain: FM (Format) | Category: SL (Scala)
 *
 * Reads and writes Scala tuning definition files. The .scl format defines
 * microtuning scales as a sequence of intervals (cents or ratios) from unison.
 *
 * Invariants:
 * - parse_scala ∘ write_scala = identity (round-trip)
 * - ratio_to_cents(2, 1) = 1200.0
 * - scala_to_cent_table only succeeds for 12-note tunings
 */

#pragma once

#include "../../Sunny.Core/Tensor/TNTP001A.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Infrastructure::Format {

/// A single interval in a Scala file
struct ScalaInterval {
    double cents;       ///< interval in cents from unison
    bool is_ratio;      ///< parsed as ratio (p/q) vs cents
    int ratio_num;      ///< numerator if ratio
    int ratio_den;      ///< denominator if ratio
};

/// Parsed Scala tuning definition
struct ScalaTuning {
    std::string description;
    std::vector<ScalaInterval> intervals;  ///< n intervals (including octave)
};

/// Convert frequency ratio to cents: 1200 * log2(num/den)
[[nodiscard]] double ratio_to_cents(int num, int den);

/// Parse .scl text into a ScalaTuning
[[nodiscard]] Sunny::Core::Result<ScalaTuning> parse_scala(std::string_view text);

/// Serialise ScalaTuning to .scl text
[[nodiscard]] std::string write_scala(const ScalaTuning& tuning);

/// Map a 12-note tuning to cent deviations from 12-TET
/// Returns FormatError if the tuning does not have exactly 12 intervals
[[nodiscard]] Sunny::Core::Result<std::array<double, 12>> scala_to_cent_table(
    const ScalaTuning& tuning);

}  // namespace Sunny::Infrastructure::Format
