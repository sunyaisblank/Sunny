/**
 * @file written_duration.hpp
 * @brief Projection of a sounding duration onto written note values
 *
 * Gould (Behind Bars) requires every written note to carry a note value, dots
 * and, where needed, a tuplet ratio. A Score duration is an exact rational;
 * this projection finds the written values that notate it. A dyadic value
 * with no single glyph (5/16) becomes tied written values (a quarter tied to
 * a sixteenth). A non-dyadic value outside any TupletContext receives the
 * conventional tuplet ratio its odd denominator implies (1/12 is an eighth
 * under 3:2). When no written form exists the projection fails and the
 * exporter reports a residual rather than emitting a mislabelled glyph.
 *
 * Both notation exporters share this one projection so that MusicXML and
 * LilyPond agree on every written rhythm.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <sunny/core/types/beat.hpp>
#include <vector>

namespace sunny::core {

/// One written glyph: base value 2^exponent whole notes plus dots.
struct WrittenNoteValue {
    int exponent; ///< 1 = breve, 0 = whole, -2 = quarter, -10 = 1024th
    int dots;     ///< 0..2

    /// Written length in whole notes: 2^exponent * (2 - 2^-dots).
    [[nodiscard]] Beat length() const {
        const std::int64_t dotted_numerator = (std::int64_t{1} << (dots + 1)) - 1;
        const std::int64_t dotted_denominator = std::int64_t{1} << dots;
        if (exponent >= 0)
            return Beat{dotted_numerator * (std::int64_t{1} << exponent), dotted_denominator};
        return Beat{dotted_numerator, dotted_denominator * (std::int64_t{1} << -exponent)};
    }
};

/// One tied segment: the written glyph and the sounding time it occupies.
struct WrittenPiece {
    WrittenNoteValue value;
    Beat sounding;
};

/**
 * @brief Written form of one sounding duration
 *
 * `pieces` are tied in order and their sounding durations sum exactly to the
 * source duration. `inferred_actual:inferred_normal` is an additional tuplet
 * ratio implied by the duration itself (1:1 when none), which applies on top
 * of any enclosing TupletContext ratio.
 */
struct WrittenDuration {
    std::vector<WrittenPiece> pieces;
    std::int64_t inferred_actual = 1;
    std::int64_t inferred_normal = 1;
};

/// Shortest and longest written base values admitted: 1024th to breve.
inline constexpr int MIN_WRITTEN_EXPONENT = -10;
inline constexpr int MAX_WRITTEN_EXPONENT = 1;
/// More tied pieces than this is not a readable written form.
inline constexpr std::size_t MAX_WRITTEN_PIECES = 4;

namespace detail {

[[nodiscard]] constexpr bool is_power_of_two(std::int64_t value) noexcept {
    return value > 0 && (value & (value - 1)) == 0;
}

} // namespace detail

/**
 * @brief Project a sounding duration onto tied written values
 *
 * @param sounding Positive sounding duration in whole notes
 * @param context_ratio Cumulative actual/normal ratio of enclosing
 *        TupletContexts (1 outside any tuplet)
 * @param min_exponent Shortest base value the target can write
 * @return The written form, or std::nullopt when none exists within the
 *         admitted glyph range and piece count
 *
 * Written length W = sounding x context_ratio. When W is dyadic it is split
 * greedily into the longest writable values (at most two dots). Outside a
 * tuplet, a non-dyadic W with odd denominator factor m >= 3 is written under
 * m : 2^floor(log2 m), the conventional ratio (3:2, 5:4, 7:4, 9:8, ...).
 */
[[nodiscard]] inline std::optional<WrittenDuration> project_written_duration(
    Beat sounding, Beat context_ratio = Beat::one(), int min_exponent = MIN_WRITTEN_EXPONENT) {
    if (sounding <= Beat::zero() || context_ratio <= Beat::zero()) return std::nullopt;
    auto written = checked_mul(sounding, context_ratio);
    if (!written) return std::nullopt;

    WrittenDuration result;
    std::int64_t denominator = written->denominator();
    std::int64_t odd = denominator;
    while (odd % 2 == 0)
        odd /= 2;
    if (odd != 1) {
        if (context_ratio != Beat::one() || odd > 255) return std::nullopt;
        std::int64_t normal = 1;
        while (normal * 2 < odd)
            normal *= 2;
        result.inferred_actual = odd;
        result.inferred_normal = normal;
        written = checked_mul(*written, Beat{odd, normal});
        if (!written) return std::nullopt;
    }

    // Written-to-sounding factor for every piece.
    auto scale = checked_mul(context_ratio, Beat{result.inferred_actual, result.inferred_normal});
    if (!scale) return std::nullopt;

    Beat remaining = *written;
    while (remaining > Beat::zero()) {
        if (result.pieces.size() == MAX_WRITTEN_PIECES) return std::nullopt;
        std::optional<WrittenNoteValue> chosen;
        for (int exponent = MAX_WRITTEN_EXPONENT; exponent >= min_exponent && !chosen; --exponent) {
            for (int dots = 2; dots >= 0; --dots) {
                const WrittenNoteValue candidate{exponent, dots};
                // A dotted value must keep its smallest dot within the range.
                if (exponent - dots < min_exponent) continue;
                if (candidate.length() <= remaining) {
                    chosen = candidate;
                    break;
                }
            }
        }
        if (!chosen) return std::nullopt;
        const Beat length = chosen->length();
        auto sounding_piece = checked_div(length, *scale);
        if (!sounding_piece) return std::nullopt;
        result.pieces.push_back(WrittenPiece{*chosen, *sounding_piece});
        remaining = remaining - length;
    }
    return result;
}

} // namespace sunny::core
