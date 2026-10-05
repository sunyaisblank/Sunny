/**
 * @file score_document.cpp
 * @brief Versioned Score document ownership
 */

#include <array>
#include <limits>
#include <sunny/core/score/score_document.hpp>

namespace sunny::core {

namespace {

void include_sections(ScoreIdentityReservations& ids, const SectionMap& sections) {
    for (const auto& section : sections) {
        ids.sections.insert(section.id);
        include_sections(ids, section.children);
    }
}

void include_tuplet(ScoreIdentityReservations& ids, const TupletContext& context) {
    ids.tuplets.insert(context.id);
    if (context.nested_in) ids.tuplets.insert(*context.nested_in);
}

} // namespace

ScoreIdentityReservations collect_score_identities(const Score& score) {
    auto ids = score.identity_reservations;
    include_sections(ids, score.section_map);
    for (const auto& part : score.parts) {
        ids.parts.insert(part.id);
        for (const auto& measure : part.measures) {
            for (const auto& voice : measure.voices) {
                for (const auto& beam : voice.beam_groups) {
                    ids.beams.insert(beam.id);
                    ids.events.insert(beam.event_ids.begin(), beam.event_ids.end());
                }
                for (const auto& event : voice.events) {
                    ids.events.insert(event.id);
                    if (const auto* group = event.as_note_group()) {
                        if (group->tuplet_context) include_tuplet(ids, *group->tuplet_context);
                        if (group->beam_group) ids.beams.insert(*group->beam_group);
                    } else if (const auto* rest = event.as_rest()) {
                        if (rest->tuplet_context) include_tuplet(ids, *rest->tuplet_context);
                    }
                }
            }
        }
    }
    return ids;
}

void retain_score_identities(Score& score, const ScoreIdentityReservations& observed) {
    auto combined = collect_score_identities(score);
    combined.events.insert(observed.events.begin(), observed.events.end());
    combined.parts.insert(observed.parts.begin(), observed.parts.end());
    combined.sections.insert(observed.sections.begin(), observed.sections.end());
    combined.tuplets.insert(observed.tuplets.begin(), observed.tuplets.end());
    combined.beams.insert(observed.beams.begin(), observed.beams.end());
    score.identity_reservations = std::move(combined);
}

void retain_score_identities(Score& restored, const Score& current) {
    retain_score_identities(restored, collect_score_identities(current));
}

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
    retain_score_identities(initial, ScoreIdentityReservations{});

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
