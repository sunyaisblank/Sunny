/**
 * @file time.hpp
 * @brief Score IR temporal system — ScoreTime/AbsoluteBeat/RealTime/TickTime
 *
 *
 * Provides conversion functions between the four temporal coordinate
 * systems defined in the Score IR specification (SS-IR §5):
 *
 *   ScoreTime  →  AbsoluteBeat  →  RealTime  →  TickTime
 *   (bar,beat)    (cumulative)     (seconds)    (MIDI ticks)
 *
 * Key algorithms:
 * - AbsoluteBeat: sum of measure durations + beat offset
 * - RealTime: integral of 240/quarter-BPM(t) over the whole-note domain
 *   - Linear tempo: closed-form Δt = 240·d·ln(B₂/B₁)/(B₂−B₁)
 * - TickTime: AbsoluteBeat × 4 × PPQ with Bresenham rounding
 *
 * Invariants:
 * - ScoreTime → AbsoluteBeat is monotonically increasing
 * - AbsoluteBeat → TickTime is monotonically increasing
 * - TickTime rounding error ≤ 0.5 ticks per bar boundary
 */

#pragma once

#include <cmath>
#include <limits>
#include <sunny/core/score/document.hpp>

namespace sunny::core {

// =============================================================================
// Constants
// =============================================================================

/// Default MIDI pulses per quarter note
constexpr int DEFAULT_PPQ = 480;

// =============================================================================
// AbsoluteBeat (SS-IR §5.2)
// =============================================================================

/**
 * @brief Compute cumulative beat position from score start
 *
 * AbsoluteBeat(ScoreTime(b, β)) = Σ_{i=1}^{b-1} duration(measure_i) + β
 *
 * @param time Target score time
 * @param time_map Time signature map (must have entry at bar 1)
 * @return Cumulative beat offset from score start, or error
 */
[[nodiscard]] Result<Beat> score_time_to_absolute_beat(ScoreTime time,
                                                       const TimeSignatureMap& time_map);

/**
 * @brief Inverse: find ScoreTime from a cumulative beat position
 *
 * @param absolute Cumulative beat offset
 * @param time_map Time signature map
 * @param total_bars Total bars in the score
 * @return ScoreTime, or error if absolute exceeds score duration
 */
[[nodiscard]] Result<ScoreTime> absolute_beat_to_score_time(Beat absolute,
                                                            const TimeSignatureMap& time_map,
                                                            std::uint32_t total_bars);

// =============================================================================
// RealTime (SS-IR §5.3)
// =============================================================================

/**
 * @brief Convert AbsoluteBeat to clock time in seconds
 *
 * Integrates 240/Bq(t) over the whole-note AbsoluteBeat domain.
 * For linear tempo transitions uses the closed-form:
 *   Δt = 240·d·ln(B₂/B₁) / (B₂ − B₁)
 *
 * @param absolute Target beat position
 * @param tempo_map S24-coherent tempo map beginning exactly at score origin;
 *                  Linear is an incoming transition owned by its destination
 * @param time_map Time signature map for beat-unit normalisation
 * @return Clock time in seconds from score start
 */
[[nodiscard]] Result<double> absolute_beat_to_real_time(Beat absolute,
                                                        const TempoMap& tempo_map,
                                                        const TimeSignatureMap& time_map);

/**
 * @brief Convert ScoreTime directly to RealTime
 *
 * Composes score_time_to_absolute_beat then absolute_beat_to_real_time.
 */
[[nodiscard]] Result<double> score_time_to_real_time(ScoreTime time,
                                                     const TempoMap& tempo_map,
                                                     const TimeSignatureMap& time_map);

/**
 * @brief Resolve the exact instantaneous effective quarter-note tempo
 *
 * Linear interpolation is evaluated in exact rational Beat space. The
 * returned PositiveRational always uses quarter notes as its beat unit.
 */
[[nodiscard]] Result<PositiveRational> effective_quarter_tempo_at(ScoreTime time,
                                                                  const TempoMap& tempo_map,
                                                                  const TimeSignatureMap& time_map);

// =============================================================================
// TickTime (SS-IR §5.4)
// =============================================================================

namespace detail {

/**
 * Multiply a signed value by an unsigned value, divide, and round without
 * forming an overflowing intermediate product.
 */
[[nodiscard]] constexpr std::int64_t saturating_rounded_product_quotient(
    std::int64_t value, std::uint64_t multiplier, std::uint64_t divisor) noexcept {
    assert(divisor != 0);
    if (divisor == 0 || value == 0 || multiplier == 0) return 0;

    const bool negative = value < 0;
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const auto limit = maximum + static_cast<std::uint64_t>(negative);
    const auto saturated = [negative] {
        return negative ? std::numeric_limits<std::int64_t>::min()
                        : std::numeric_limits<std::int64_t>::max();
    };

    const auto value_magnitude = magnitude(value);
    const auto add_quotient = value_magnitude / divisor;
    const auto add_remainder = value_magnitude % divisor;
    std::uint64_t quotient = 0;
    std::uint64_t remainder = 0;

    for (int bit = 63; bit >= 0; --bit) {
        if (quotient > limit - quotient) return saturated();
        quotient += quotient;

        if (remainder >= divisor - remainder) {
            remainder -= divisor - remainder;
            if (quotient == limit) return saturated();
            ++quotient;
        } else {
            remainder += remainder;
        }

        if ((multiplier & (std::uint64_t{1} << bit)) == 0) continue;
        if (add_quotient > limit - quotient) return saturated();
        quotient += add_quotient;

        if (add_remainder >= divisor - remainder) {
            remainder = add_remainder - (divisor - remainder);
            if (quotient == limit) return saturated();
            ++quotient;
        } else {
            remainder += add_remainder;
        }
    }

    if (remainder >= (divisor + 1) / 2) {
        if (quotient == limit) return saturated();
        ++quotient;
    }
    return from_signed_magnitude(quotient, negative);
}

} // namespace detail

/**
 * @brief Convert AbsoluteBeat to MIDI tick position
 *
 * tick = AbsoluteBeat × 4 × PPQ (Beat is measured in whole notes)
 * Uses Bresenham-style rounding: cumulative error never exceeds
 * 0.5 ticks over any bar boundary.
 *
 * @param absolute Beat position
 * @param ppq Pulses per quarter note (default 480)
 * @return Tick position (exact integer)
 */
[[nodiscard]] constexpr std::int64_t absolute_beat_to_tick(Beat absolute,
                                                           int ppq = DEFAULT_PPQ) noexcept {
    assert(ppq > 0);
    if (ppq <= 0) return 0;

    return detail::saturating_rounded_product_quotient(
        absolute.numerator(),
        static_cast<std::uint64_t>(ppq) * 4,
        static_cast<std::uint64_t>(absolute.denominator()));
}

/**
 * @brief Convert MIDI tick to AbsoluteBeat
 *
 * @param tick Tick position
 * @param ppq Pulses per quarter note
 * @return AbsoluteBeat (exact rational)
 */
[[nodiscard]] constexpr Beat tick_to_absolute_beat(std::int64_t tick,
                                                   int ppq = DEFAULT_PPQ) noexcept {
    // Inverse of above: absolute = tick / (4 * ppq)
    return Beat::normalise(tick, 4 * static_cast<std::int64_t>(ppq));
}

/**
 * @brief Convert ScoreTime directly to tick
 *
 * @param time Score time
 * @param time_map Time signature map
 * @param ppq Pulses per quarter note
 * @return Tick position, or error
 */
[[nodiscard]] Result<std::int64_t>
score_time_to_tick(ScoreTime time, const TimeSignatureMap& time_map, int ppq = DEFAULT_PPQ);

// =============================================================================
// Beat-unit duration helpers (SS-IR §2.3)
// =============================================================================

/**
 * @brief Duration of a BeatUnit in quarter notes
 *
 * Returns the duration as a Beat value where Beat{1,1} = whole note.
 */
[[nodiscard]] constexpr Beat beat_unit_duration(BeatUnit unit) noexcept {
    switch (unit) {
    case BeatUnit::Whole:
        return Beat{1, 1};
    case BeatUnit::Half:
        return Beat{1, 2};
    case BeatUnit::DottedHalf:
        return Beat{3, 4};
    case BeatUnit::Quarter:
        return Beat{1, 4};
    case BeatUnit::DottedQuarter:
        return Beat{3, 8};
    case BeatUnit::Eighth:
        return Beat{1, 8};
    case BeatUnit::DottedEighth:
        return Beat{3, 16};
    case BeatUnit::Sixteenth:
        return Beat{1, 16};
    }
    return Beat{1, 4};
}

/**
 * @brief Effective BPM in quarter notes per minute
 *
 * Normalises a tempo event's BPM to quarter-note basis by accounting
 * for the beat unit. E.g., dotted-quarter = 120 → quarter = 180.
 */
[[nodiscard]] constexpr double effective_quarter_bpm(const TempoEvent& event) noexcept {
    Beat unit_dur = beat_unit_duration(event.beat_unit);
    // BPM is in beats per minute where beat = beat_unit
    // Quarter notes per minute = BPM * (beat_unit_duration / quarter_duration)
    // = BPM * (unit_dur / Beat{1,4}) = BPM * unit_dur * 4
    double bpm = event.bpm.to_float();
    double unit_in_quarters = unit_dur.to_float() * 4.0;
    return bpm * unit_in_quarters;
}

// =============================================================================
// Bresenham Tick Quantisation (SS-IR §5.4)
// =============================================================================

/**
 * @brief Quantise a sequence of beat durations to ticks with evenly
 *        distributed rounding error (Bresenham-style)
 *
 * For a sequence of beat values, returns the tick representation where
 * cumulative rounding error never exceeds 0.5 ticks. Each output tick
 * value is the quantised duration of the corresponding input beat.
 *
 * @param beats Sequence of durations to quantise
 * @param ppq Pulses per quarter note
 * @return Quantised tick durations
 */
[[nodiscard]] inline std::vector<std::int64_t> bresenham_quantise(std::span<const Beat> beats,
                                                                  int ppq = DEFAULT_PPQ) {
    std::vector<std::int64_t> ticks;
    ticks.reserve(beats.size());

    std::int64_t cumulative_ticks = 0;
    Beat cumulative_beats = Beat::zero();

    for (const auto& b : beats) {
        cumulative_beats = cumulative_beats + b;
        std::int64_t target_tick = absolute_beat_to_tick(cumulative_beats, ppq);
        std::int64_t this_tick = target_tick - cumulative_ticks;
        ticks.push_back(this_tick);
        cumulative_ticks = target_tick;
    }

    return ticks;
}

} // namespace sunny::core
