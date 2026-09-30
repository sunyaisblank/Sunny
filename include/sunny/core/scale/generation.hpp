/**
 * @file generation.hpp
 * @brief Scale generation
 *
 *
 * Generate MIDI notes from scale definitions.
 *
 * Invariants:
 * - result[0] % 12 == root_pc
 * - len(result) == len(intervals)
 * - All results in [0, 127]
 */

#pragma once

#include <span>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/types/music_types.hpp>
#include <vector>

namespace sunny::core {

/**
 * @brief Generate MIDI notes for a scale
 *
 * @param root_pc Root pitch class [0, 11]
 * @param intervals Scale intervals from root
 * @param octave Base octave
 * @return List of MIDI note numbers
 */
[[nodiscard]] Result<std::vector<MidiNote>>
generate_scale_notes(PitchClass root_pc, std::span<const Interval> intervals, int octave);

/**
 * @brief Generate scale notes spanning multiple octaves
 *
 * @param root_pc Root pitch class
 * @param intervals Scale intervals
 * @param start_octave Starting octave
 * @param octave_count Number of octaves
 * @return List of MIDI note numbers
 */
[[nodiscard]] Result<std::vector<MidiNote>> generate_scale_range(
    PitchClass root_pc, std::span<const Interval> intervals, int start_octave, int octave_count);

/**
 * @brief Check if a MIDI note is in a scale
 *
 * @param note MIDI note to check
 * @param root_pc Scale root pitch class
 * @param intervals Scale intervals
 * @return true if note's pitch class is in the scale
 */
[[nodiscard]] bool
is_note_in_scale(MidiNote note, PitchClass root_pc, std::span<const Interval> intervals);

/**
 * @brief Quantize a MIDI note to the nearest scale degree
 *
 * @param note MIDI note to quantize
 * @param root_pc Scale root pitch class
 * @param intervals Scale intervals
 * @return Quantized MIDI note
 */
[[nodiscard]] MidiNote
quantize_to_scale(MidiNote note, PitchClass root_pc, std::span<const Interval> intervals);

} // namespace sunny::core
