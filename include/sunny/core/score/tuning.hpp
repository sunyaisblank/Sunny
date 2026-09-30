/**
 * @file tuning.hpp
 * @brief Complete sounding-pitch mapping for the Score IR MIDI domain
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sunny/core/types/music_types.hpp>

namespace sunny::core {

inline constexpr std::size_t SCORE_TUNING_NOTE_COUNT = 128;
inline constexpr std::uint8_t SCORE_TUNING_DEFAULT_REFERENCE_NOTE = 69;
inline constexpr double SCORE_TUNING_DEFAULT_REFERENCE_HZ = 440.0;

[[nodiscard]] constexpr std::array<double, SCORE_TUNING_NOTE_COUNT> equal_temperament_score_cents(
    std::uint8_t reference_note = SCORE_TUNING_DEFAULT_REFERENCE_NOTE) noexcept {
    std::array<double, SCORE_TUNING_NOTE_COUNT> result{};
    for (std::size_t note = 0; note < result.size(); ++note) {
        result[note] = (static_cast<double>(note) - static_cast<double>(reference_note)) * 100.0;
    }
    return result;
}

/**
 * @brief Explicit pitch function over the admitted MIDI/Live performance domain
 *
 * For MIDI note index m, the sounding frequency is
 * reference_frequency_hz * 2^(cents_from_reference[m] / 1200).
 * The reference entry is canonically zero. The table need not be monotone or
 * octave-periodic; those are musical policies rather than representation limits.
 */
struct ScoreTuning {
    std::string name = "12-TET";
    std::uint8_t reference_midi_note = SCORE_TUNING_DEFAULT_REFERENCE_NOTE;
    double reference_frequency_hz = SCORE_TUNING_DEFAULT_REFERENCE_HZ;
    std::array<double, SCORE_TUNING_NOTE_COUNT> cents_from_reference =
        equal_temperament_score_cents();

    friend bool operator==(const ScoreTuning&, const ScoreTuning&) = default;
};

/** Validate the complete tuning and every frequency derivable from it. */
[[nodiscard]] Result<void> validate_score_tuning(const ScoreTuning& tuning) noexcept;

/** Project one MIDI-domain note index through a validated Score tuning. */
[[nodiscard]] Result<double> score_tuned_frequency(const ScoreTuning& tuning,
                                                   std::uint8_t midi_note) noexcept;

/** True only for the canonical 12-TET/A4=440 MIDI baseline. */
[[nodiscard]] bool is_standard_midi_tuning(const ScoreTuning& tuning) noexcept;

/**
 * Expand a 12 pitch-class cent-deviation table into the complete Score domain.
 * Deviations are normalized so the selected reference note remains exactly at
 * reference_frequency_hz.
 */
[[nodiscard]] Result<ScoreTuning> score_tuning_from_pitch_class_deviations(
    std::string name,
    const std::array<double, 12>& deviations,
    std::uint8_t reference_note = SCORE_TUNING_DEFAULT_REFERENCE_NOTE,
    double reference_frequency_hz = SCORE_TUNING_DEFAULT_REFERENCE_HZ);

} // namespace sunny::core
