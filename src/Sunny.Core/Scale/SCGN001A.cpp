/**
 * @file SCGN001A.cpp
 * @brief Scale generation implementation
 *
 * Component: SCGN001A
 */

#include "SCGN001A.h"
#include "../Pitch/PTMN001A.h"

#include <algorithm>
#include <cmath>

namespace Sunny::Core {

Result<std::vector<MidiNote>> generate_scale_notes(
    PitchClass root_pc,
    std::span<const Interval> intervals,
    int octave
) {
    if (!is_valid_pitch_class(root_pc)) {
        return std::unexpected(ErrorCode::InvalidPitchClass);
    }
    if (octave < Constants::OCTAVE_MIN || octave > Constants::OCTAVE_MAX) {
        return std::unexpected(ErrorCode::InvalidOctave);
    }
    if (intervals.empty()) {
        return std::unexpected(ErrorCode::ScaleGenerationFailed);
    }

    auto base_midi = pitch_octave_to_midi(root_pc, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ScaleGenerationFailed);
    }

    std::vector<MidiNote> notes;
    notes.reserve(intervals.size());

    for (Interval interval : intervals) {
        int midi = *base_midi + interval;
        if (midi < 0 || midi > 127) {
            // Skip notes outside MIDI range
            continue;
        }
        notes.push_back(static_cast<MidiNote>(midi));
    }

    return notes;
}

Result<std::vector<MidiNote>> generate_scale_range(
    PitchClass root_pc,
    std::span<const Interval> intervals,
    int start_octave,
    int octave_count
) {
    if (!is_valid_pitch_class(root_pc)) {
        return std::unexpected(ErrorCode::InvalidPitchClass);
    }
    if (intervals.empty() || octave_count < 1) {
        return std::unexpected(ErrorCode::ScaleGenerationFailed);
    }

    std::vector<MidiNote> notes;
    notes.reserve(intervals.size() * octave_count + 1);

    for (int oct = 0; oct < octave_count; ++oct) {
        int octave = start_octave + oct;
        auto base_midi = pitch_octave_to_midi(root_pc, octave);
        if (!base_midi) continue;

        for (Interval interval : intervals) {
            int midi = *base_midi + interval;
            if (midi >= 0 && midi <= 127) {
                notes.push_back(static_cast<MidiNote>(midi));
            }
        }
    }

    return notes;
}

bool is_note_in_scale(
    MidiNote note,
    PitchClass root_pc,
    std::span<const Interval> intervals
) {
    PitchClass note_pc = pitch_class(note);

    for (Interval interval : intervals) {
        PitchClass scale_pc = transpose(root_pc, interval);
        if (note_pc == scale_pc) {
            return true;
        }
    }

    return false;
}

MidiNote quantize_to_scale(
    MidiNote note,
    PitchClass root_pc,
    std::span<const Interval> intervals
) {
    if (intervals.empty()) {
        return note;
    }

    // If already in scale, return unchanged
    if (is_note_in_scale(note, root_pc, intervals)) {
        return note;
    }

    PitchClass note_pc = pitch_class(note);

    // Find closest scale degree
    int best_distance = 12;
    PitchClass best_pc = root_pc;

    for (Interval interval : intervals) {
        PitchClass scale_pc = transpose(root_pc, interval);

        // Distance in pitch class space (minimum of up or down)
        int dist_up = (scale_pc - note_pc + 12) % 12;
        int dist_down = (note_pc - scale_pc + 12) % 12;
        int dist = std::min(dist_up, dist_down);

        if (dist < best_distance) {
            best_distance = dist;
            best_pc = scale_pc;
        }
    }

    // Find closest MIDI note with that pitch class
    return closest_pitch_class_midi(note, best_pc);
}

}  // namespace Sunny::Core
