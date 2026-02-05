/**
 * @file RDAP001A.h
 * @brief Arpeggiator pattern generation
 *
 * Component: RDAP001A
 * Domain: RD (Render) | Category: AP (Arpeggio)
 *
 * Generates arpeggio patterns from chord voicings:
 * - Up, Down, Up-Down, Random patterns
 * - Octave range control
 * - Gate time control
 * - Pattern locking
 */

#pragma once

#include "Tensor/TNTP001A.h"
#include "Tensor/TNEV001A.h"

#include <vector>
#include <random>

namespace Sunny::Render {

/// Arpeggio direction/pattern
enum class ArpDirection {
    Up,         ///< Lowest to highest
    Down,       ///< Highest to lowest
    UpDown,     ///< Up then down (exclusive)
    DownUp,     ///< Down then up (exclusive)
    Random,     ///< Random note selection
    Order       ///< In order of input
};

/**
 * @brief Arpeggiator
 *
 * Transforms a chord voicing into a sequence of individual notes.
 */
class Arpeggiator {
public:
    Arpeggiator() = default;

    // Configuration
    void set_direction(ArpDirection dir) { direction_ = dir; }
    void set_octave_range(int octaves) { octave_range_ = octaves; }
    void set_gate(double gate) { gate_ = gate; }  // 0.0-1.0

    // Input
    void set_notes(const std::vector<Core::MidiNote>& notes);
    void set_notes(const Core::ChordVoicing& voicing);
    void clear();

    // Pattern generation
    [[nodiscard]] std::vector<Core::MidiNote> generate_pattern() const;

    // Step-by-step access
    void reset();
    [[nodiscard]] Core::MidiNote next();
    [[nodiscard]] Core::MidiNote current() const;
    [[nodiscard]] std::size_t step() const { return current_step_; }
    [[nodiscard]] std::size_t pattern_length() const;

    // Configuration queries
    [[nodiscard]] ArpDirection direction() const { return direction_; }
    [[nodiscard]] int octave_range() const { return octave_range_; }
    [[nodiscard]] double gate() const { return gate_; }

private:
    ArpDirection direction_{ArpDirection::Up};
    int octave_range_{1};
    double gate_{0.5};

    std::vector<Core::MidiNote> input_notes_;
    mutable std::vector<Core::MidiNote> pattern_cache_;
    mutable bool pattern_dirty_{true};

    std::size_t current_step_{0};
    bool going_up_{true};  // For UpDown/DownUp

    mutable std::mt19937 rng_{std::random_device{}()};

    void rebuild_pattern() const;
};

/**
 * @brief Generate arpeggio note events
 *
 * @param voicing Input chord voicing
 * @param direction Arpeggio direction
 * @param step_duration Duration of each step
 * @param gate Gate time as fraction of step (0.0-1.0)
 * @param octaves Number of octaves to span
 * @return Vector of note events
 */
[[nodiscard]] std::vector<Core::NoteEvent> generate_arpeggio(
    const Core::ChordVoicing& voicing,
    ArpDirection direction,
    Core::Beat step_duration,
    double gate = 0.5,
    int octaves = 1
);

}  // namespace Sunny::Render
