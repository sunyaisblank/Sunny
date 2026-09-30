/**
 * @file score_document.cpp
 * @brief Versioned Score document ownership
 */

#include <array>
#include <limits>
#include <sunny/core/score/score_document.hpp>

namespace sunny::core {

std::optional<SpelledPitch> derive_chord_numeral_root(const ChordNumeral& numeral) noexcept {
    if (numeral.root < 1 || numeral.root > 7 || numeral.key.fifths < -7 || numeral.key.fifths > 7 ||
        static_cast<std::uint8_t>(numeral.key.mode) >
            static_cast<std::uint8_t>(ChordNumeralMode::HarmonicMinor))
        return std::nullopt;

    constexpr std::array<std::array<int, 7>, 5> scale_intervals{{
        {{0, 2, 4, 5, 7, 9, 11}},
        {{0, 2, 3, 5, 7, 8, 10}},
        {{0, 2, 3, 5, 7, 8, 10}},
        {{0, 2, 3, 5, 7, 9, 11}},
        {{0, 2, 3, 5, 7, 8, 11}},
    }};
    const auto mode = static_cast<std::size_t>(numeral.key.mode);
    const int tonic_fifths = numeral.key.fifths + (mode == 0 ? 0 : 3);
    const auto tonic = from_line_of_fifths(tonic_fifths, 4);
    const auto degree_index = static_cast<std::size_t>(numeral.root - 1);
    const auto expected_letter = static_cast<std::uint8_t>((tonic.letter + degree_index) % 7);
    const int natural_target =
        static_cast<int>(nat(expected_letter)) +
        (static_cast<std::size_t>(tonic.letter) + degree_index >= 7 ? 12 : 0);
    const int expected_accidental = static_cast<int>(nat(tonic.letter)) + tonic.accidental +
                                    scale_intervals[mode][degree_index] + numeral.alteration -
                                    natural_target;
    if (expected_accidental < std::numeric_limits<std::int8_t>::min() ||
        expected_accidental > std::numeric_limits<std::int8_t>::max())
        return std::nullopt;
    return SpelledPitch{expected_letter, static_cast<std::int8_t>(expected_accidental), 4};
}

Result<ScoreDocument> ScoreDocument::create(Score initial) {
    if (!is_compilable(initial)) return std::unexpected(ErrorCode::InvariantViolation);

    Snapshot snapshot = std::make_shared<const Score>(std::move(initial));
    return ScoreDocument{std::make_shared<State>(std::move(snapshot))};
}

ScoreDocument::Snapshot ScoreDocument::snapshot() const {
    std::shared_lock lock(state_->snapshot_mutex);
    return state_->current;
}

std::uint64_t ScoreDocument::version() const {
    return snapshot()->version;
}

} // namespace sunny::core
