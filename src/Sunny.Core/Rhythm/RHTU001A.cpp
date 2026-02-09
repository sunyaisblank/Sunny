/**
 * @file RHTU001A.cpp
 * @brief Tuplet implementation
 *
 * Component: RHTU001A
 */

#include "RHTU001A.h"

namespace Sunny::Core {

Result<Tuplet> make_tuplet(int actual, int normal, Beat base_duration) {
    if (actual < 1 || normal < 1) {
        return std::unexpected(ErrorCode::TupletInvalidRatio);
    }
    if (base_duration.numerator <= 0 || base_duration.denominator <= 0) {
        return std::unexpected(ErrorCode::TupletInvalidRatio);
    }

    return Tuplet{actual, normal, base_duration};
}

std::vector<NoteEvent> tuplet_to_events(
    const Tuplet& tuplet,
    Beat start,
    MidiNote pitch,
    Velocity velocity
) {
    std::vector<NoteEvent> events;
    events.reserve(tuplet.actual);

    Beat note_dur = tuplet_note_duration(tuplet);
    Beat current = start;

    for (int i = 0; i < tuplet.actual; ++i) {
        NoteEvent ev;
        ev.pitch = pitch;
        ev.start_time = current;
        ev.duration = note_dur;
        ev.velocity = velocity;
        ev.muted = false;
        events.push_back(ev);
        current = current + note_dur;
    }

    return events;
}

}  // namespace Sunny::Core
