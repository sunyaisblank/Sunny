/**
 * @file scheduled_event.cpp
 * @brief Shared tick-scheduled MIDI event validation
 */

#include <limits>
#include <sunny/render/scheduled_event.hpp>

namespace sunny::render {

bool scheduled_event_precedes(const ScheduledEvent& lhs,
                              std::uint64_t lhs_sequence,
                              const ScheduledEvent& rhs,
                              std::uint64_t rhs_sequence) noexcept {
    if (lhs.tick != rhs.tick) return lhs.tick < rhs.tick;
    const bool lhs_is_note_off = lhs.event.velocity == 0;
    const bool rhs_is_note_off = rhs.event.velocity == 0;
    if (lhs_is_note_off != rhs_is_note_off) return lhs_is_note_off;
    return lhs_sequence < rhs_sequence;
}

sunny::core::Result<ScheduledEvent> normalise_scheduled_event(const ScheduledEvent& event,
                                                              std::int64_t current_tick,
                                                              std::int64_t ppq) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    if (current_tick < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (event.tick < 0 || event.tick < current_tick)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (event.event.velocity > 127) return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    if (event.event.release_velocity > 127)
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);

    auto normalized = event;
    normalized.event.start_time = sunny::core::Beat::normalise(event.tick, 4 * ppq);
    return normalized;
}

sunny::core::Result<ScheduledNotePair>
make_scheduled_note_pair(std::int64_t tick,
                         std::int64_t current_tick,
                         std::int64_t ppq,
                         sunny::core::MidiNote pitch,
                         sunny::core::Beat duration,
                         sunny::core::Velocity velocity,
                         std::uint8_t release_velocity) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    if (current_tick < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (tick < 0 || tick < current_tick)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (!sunny::core::is_valid_velocity(static_cast<int>(velocity)))
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    if (release_velocity > 127) return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    if (duration.numerator() <= 0)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);

    auto tick_duration = sunny::core::checked_mul(duration, sunny::core::Beat{4 * ppq, 1});
    if (!tick_duration) return std::unexpected(tick_duration.error());
    if (tick_duration->denominator() != 1)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    if (tick > std::numeric_limits<std::int64_t>::max() - tick_duration->numerator())
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    sunny::core::NoteEvent note_on;
    note_on.pitch = pitch;
    note_on.start_time = sunny::core::Beat::normalise(tick, 4 * ppq);
    note_on.duration = duration.reduce();
    note_on.velocity = velocity;
    note_on.release_velocity = release_velocity;

    auto note_off = note_on;
    note_off.velocity = 0;
    const auto note_off_tick = tick + tick_duration->numerator();
    note_off.start_time = sunny::core::Beat::normalise(note_off_tick, 4 * ppq);

    return ScheduledNotePair{
        .note_on = ScheduledEvent{tick, note_on},
        .note_off = ScheduledEvent{note_off_tick, note_off},
    };
}

sunny::core::Result<ScheduledNotePair>
make_scheduled_note_pair_ticks(std::int64_t tick,
                               std::int64_t current_tick,
                               std::int64_t ppq,
                               sunny::core::MidiNote pitch,
                               std::int64_t duration_ticks,
                               sunny::core::Velocity velocity,
                               std::uint8_t release_velocity) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    if (current_tick < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (tick < 0 || tick < current_tick)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (!sunny::core::is_valid_velocity(static_cast<int>(velocity)))
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    if (release_velocity > 127) return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    if (duration_ticks <= 0)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    if (tick > std::numeric_limits<std::int64_t>::max() - duration_ticks)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    sunny::core::NoteEvent note_on;
    note_on.pitch = pitch;
    note_on.start_time = sunny::core::Beat::normalise(tick, 4 * ppq);
    note_on.duration = sunny::core::Beat::normalise(duration_ticks, 4 * ppq);
    note_on.velocity = velocity;
    note_on.release_velocity = release_velocity;

    auto note_off = note_on;
    note_off.velocity = 0;
    const auto note_off_tick = tick + duration_ticks;
    note_off.start_time = sunny::core::Beat::normalise(note_off_tick, 4 * ppq);

    return ScheduledNotePair{
        .note_on = ScheduledEvent{tick, note_on},
        .note_off = ScheduledEvent{note_off_tick, note_off},
    };
}

} // namespace sunny::render
