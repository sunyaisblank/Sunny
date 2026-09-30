/**
 * @file tuning.cpp
 * @brief Score IR sounding-pitch mapping
 */

#include <cmath>
#include <sunny/core/score/tuning.hpp>
#include <utility>

namespace sunny::core {

Result<double> score_tuned_frequency(const ScoreTuning& tuning, std::uint8_t midi_note) noexcept {
    if (midi_note >= SCORE_TUNING_NOTE_COUNT ||
        tuning.reference_midi_note >= SCORE_TUNING_NOTE_COUNT ||
        !std::isfinite(tuning.reference_frequency_hz) || tuning.reference_frequency_hz <= 0.0 ||
        !std::isfinite(tuning.cents_from_reference[midi_note]))
        return std::unexpected(ErrorCode::InvalidFrequency);

    const double exponent = tuning.cents_from_reference[midi_note] / 1200.0;
    const double frequency = tuning.reference_frequency_hz * std::exp2(exponent);
    if (!std::isfinite(exponent) || !std::isfinite(frequency) || frequency <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    return frequency;
}

Result<void> validate_score_tuning(const ScoreTuning& tuning) noexcept {
    if (tuning.reference_midi_note >= SCORE_TUNING_NOTE_COUNT ||
        !std::isfinite(tuning.reference_frequency_hz) || tuning.reference_frequency_hz <= 0.0 ||
        tuning.cents_from_reference[tuning.reference_midi_note] != 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);

    for (std::size_t note = 0; note < SCORE_TUNING_NOTE_COUNT; ++note) {
        if (!score_tuned_frequency(tuning, static_cast<std::uint8_t>(note)))
            return std::unexpected(ErrorCode::InvalidFrequency);
    }
    return {};
}

bool is_standard_midi_tuning(const ScoreTuning& tuning) noexcept {
    return tuning.reference_midi_note == SCORE_TUNING_DEFAULT_REFERENCE_NOTE &&
           tuning.reference_frequency_hz == SCORE_TUNING_DEFAULT_REFERENCE_HZ &&
           tuning.cents_from_reference == equal_temperament_score_cents();
}

Result<ScoreTuning>
score_tuning_from_pitch_class_deviations(std::string name,
                                         const std::array<double, 12>& deviations,
                                         std::uint8_t reference_note,
                                         double reference_frequency_hz) {
    if (reference_note >= SCORE_TUNING_NOTE_COUNT || !std::isfinite(reference_frequency_hz) ||
        reference_frequency_hz <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    for (const double deviation : deviations)
        if (!std::isfinite(deviation)) return std::unexpected(ErrorCode::InvalidFrequency);

    ScoreTuning result;
    result.name = std::move(name);
    result.reference_midi_note = reference_note;
    result.reference_frequency_hz = reference_frequency_hz;
    const double reference_deviation = deviations[reference_note % deviations.size()];
    for (std::size_t note = 0; note < SCORE_TUNING_NOTE_COUNT; ++note) {
        result.cents_from_reference[note] =
            (static_cast<double>(note) - static_cast<double>(reference_note)) * 100.0 +
            deviations[note % deviations.size()] - reference_deviation;
    }
    if (auto valid = validate_score_tuning(result); !valid) return std::unexpected(valid.error());
    return result;
}

} // namespace sunny::core
