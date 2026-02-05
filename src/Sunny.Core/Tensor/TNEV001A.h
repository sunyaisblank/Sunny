/**
 * @file TNEV001A.h
 * @brief Note event and chord voicing structures
 *
 * Component: TNEV001A
 * Domain: TN (Tensor) | Category: EV (Event)
 *
 * Compound structures for musical events and voicings.
 */

#pragma once

#include "TNTP001A.h"
#include "TNBT001A.h"

#include <string>
#include <vector>

namespace Sunny::Core {

/**
 * @brief A single MIDI note event
 *
 * Represents a note with pitch, timing, and velocity.
 * Uses exact Beat type for timing (no floating-point).
 */
struct NoteEvent {
    MidiNote pitch;
    Beat start_time;
    Beat duration;
    Velocity velocity;
    bool muted = false;

    [[nodiscard]] constexpr Beat end_time() const noexcept {
        return start_time + duration;
    }

    [[nodiscard]] constexpr bool overlaps(const NoteEvent& other) const noexcept {
        return start_time < other.end_time() && end_time() > other.start_time;
    }
};

/**
 * @brief A chord voicing with metadata
 *
 * Represents a specific voicing of a chord with notes in ascending order.
 */
struct ChordVoicing {
    std::vector<MidiNote> notes;     ///< MIDI notes in ascending order
    PitchClass root;                  ///< Chord root pitch class
    std::string quality;              ///< Chord quality (major, minor, etc.)
    int inversion = 0;                ///< 0 = root position

    [[nodiscard]] bool empty() const noexcept { return notes.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return notes.size(); }

    /**
     * @brief Get bass note (lowest)
     */
    [[nodiscard]] MidiNote bass() const noexcept {
        return notes.empty() ? 0 : notes.front();
    }

    /**
     * @brief Get soprano note (highest)
     */
    [[nodiscard]] MidiNote soprano() const noexcept {
        return notes.empty() ? 0 : notes.back();
    }

    /**
     * @brief Get pitch class set of this voicing
     */
    [[nodiscard]] std::vector<PitchClass> pitch_classes() const {
        std::vector<PitchClass> pcs;
        pcs.reserve(notes.size());
        for (auto note : notes) {
            pcs.push_back(note % 12);
        }
        return pcs;
    }
};

/**
 * @brief Scale definition
 */
struct ScaleDefinition {
    std::string_view name;
    std::array<Interval, 12> intervals;  ///< Semitones from root (max 12)
    std::uint8_t note_count;              ///< Actual number of notes
    std::string_view description;

    [[nodiscard]] std::span<const Interval> get_intervals() const noexcept {
        return std::span{intervals.data(), note_count};
    }
};

}  // namespace Sunny::Core
