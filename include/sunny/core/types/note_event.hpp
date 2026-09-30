/**
 * @file note_event.hpp
 * @brief Note event and chord voicing structures
 *
 *
 * Compound structures for musical events and voicings.
 */

#pragma once

#include <string>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <vector>

namespace sunny::core {

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
    std::uint8_t release_velocity = 64; ///< Note Off intensity [0, 127]

    [[nodiscard]] constexpr Beat end_time() const noexcept { return start_time + duration; }

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
    std::vector<MidiNote> notes; ///< MIDI notes in ascending order
    PitchClass root;             ///< Chord root pitch class
    std::string quality;         ///< Chord quality (major, minor, etc.)
    int inversion = 0;           ///< 0 = root position

    [[nodiscard]] bool empty() const noexcept { return notes.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return notes.size(); }

    /**
     * @brief Get bass note (lowest)
     */
    [[nodiscard]] MidiNote bass() const noexcept { return notes.empty() ? MidiNote{} : notes[0]; }

    /**
     * @brief Get soprano note (highest)
     */
    [[nodiscard]] MidiNote soprano() const noexcept {
        return notes.empty() ? MidiNote{} : notes[notes.size() - 1];
    }

    /**
     * @brief Get pitch class set of this voicing
     */
    [[nodiscard]] std::vector<PitchClass> pitch_classes() const {
        std::vector<PitchClass> pcs;
        pcs.reserve(notes.size());
        for (auto note : notes) {
            pcs.push_back(PitchClass::wrapped(note));
        }
        return pcs;
    }
};

/**
 * @brief Scale definition
 *
 * Formal Spec §15.1.7: Stores intervals from root and provides
 * derived interval_pattern (successive step sizes).
 */
struct ScaleDefinition {
    // Owned strings are required here because ScaleDefinition is persisted in
    // Score documents.  A string_view supplied by a caller could otherwise
    // dangle before validation, serialization, or target compilation.
    std::string name;
    std::array<Interval, 12> intervals{}; ///< Semitones from root (max 12)
    std::uint8_t note_count{0};           ///< Actual number of notes
    std::string description;

    [[nodiscard]] std::span<const Interval> get_intervals() const noexcept {
        return std::span{intervals.data(), note_count};
    }

    /**
     * @brief Successive step sizes (interval pattern)
     *
     * Formal Spec §4.1: For a scale with intervals {0, i1, i2, ..., in-1},
     * the interval pattern is (i1-0, i2-i1, ..., 12-in-1).
     * Sum of pattern elements always equals 12 (for 12-TET scales).
     *
     * @return Vector of successive intervals; empty if note_count == 0
     */
    [[nodiscard]] std::vector<Interval> step_pattern() const {
        if (note_count == 0) return {};
        std::vector<Interval> steps;
        steps.reserve(note_count);
        for (std::uint8_t i = 1; i < note_count; ++i) {
            steps.push_back(intervals[i] - intervals[i - 1]);
        }
        // Final step wrapping back to octave
        steps.push_back(static_cast<Interval>(12 - intervals[note_count - 1]));
        return steps;
    }
};

} // namespace sunny::core
