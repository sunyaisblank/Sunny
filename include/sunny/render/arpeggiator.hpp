/**
 * @file arpeggiator.hpp
 * @brief Arpeggiator pattern generation
 *
 *
 * Generates arpeggio patterns from chord voicings:
 * - Up, Down, Up-Down, Random patterns
 * - Octave range control
 * - Gate time control
 * - Pattern locking
 */

#pragma once

#include <cstdint>
#include <random>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::render {

/// Arpeggio direction/pattern
enum class ArpDirection {
    Up,     ///< Lowest to highest
    Down,   ///< Highest to lowest
    UpDown, ///< Up then down (exclusive)
    DownUp, ///< Down then up (exclusive)
    Random, ///< Random note selection
    Order   ///< In order of input
};

/**
 * @brief Arpeggiator
 *
 * Transforms a chord voicing into a sequence of individual notes.
 *
 * Instances are exclusively owned and provide no internal synchronisation. Pattern construction
 * allocates and is not an audio-thread operation.
 */
class Arpeggiator {
  public:
    Arpeggiator() = default;

    // Configuration
    sunny::core::VoidResult set_direction(ArpDirection dir);
    sunny::core::VoidResult set_octave_range(int octaves);
    sunny::core::VoidResult set_gate(double gate);
    void set_seed(std::uint32_t seed);

    // Input
    void set_notes(const std::vector<sunny::core::MidiNote>& notes);
    void set_notes(const sunny::core::ChordVoicing& voicing);
    void clear();

    // Pattern generation
    [[nodiscard]] sunny::core::Result<std::vector<sunny::core::MidiNote>> generate_pattern() const;

    // Step-by-step access
    void reset();
    [[nodiscard]] sunny::core::Result<sunny::core::MidiNote> next();
    [[nodiscard]] sunny::core::Result<sunny::core::MidiNote> current() const;
    [[nodiscard]] std::size_t step() const { return current_step_; }
    [[nodiscard]] sunny::core::Result<std::size_t> pattern_length() const;

    // Configuration queries
    [[nodiscard]] ArpDirection direction() const { return direction_; }
    [[nodiscard]] int octave_range() const { return octave_range_; }
    [[nodiscard]] double gate() const { return gate_; }

  private:
    ArpDirection direction_{ArpDirection::Up};
    int octave_range_{1};
    double gate_{0.5};

    std::vector<sunny::core::MidiNote> input_notes_;
    mutable std::vector<sunny::core::MidiNote> pattern_cache_;
    mutable bool pattern_dirty_{true};

    std::size_t current_step_{0};
    std::uint32_t seed_{0};
    mutable std::mt19937 rng_{seed_};

    [[nodiscard]] sunny::core::VoidResult rebuild_pattern() const;
    void invalidate_pattern();
};

/**
 * @brief Generate arpeggio note events
 *
 * @param voicing Input chord voicing
 * @param direction Arpeggio direction
 * @param step_duration Duration of each step
 * @param gate Gate time as fraction of step (0.0, 1.0]
 * @param octaves Number of octaves to span
 * @return Vector of note events
 */
[[nodiscard]] sunny::core::Result<std::vector<sunny::core::NoteEvent>>
generate_arpeggio(const sunny::core::ChordVoicing& voicing,
                  ArpDirection direction,
                  sunny::core::Beat step_duration,
                  double gate = 0.5,
                  int octaves = 1);

} // namespace sunny::render
