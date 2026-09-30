/**
 * @file non_tertian.cpp
 * @brief Non-tertian chord construction
 *
 */

#include <algorithm>
#include <sunny/core/harmony/non_tertian.hpp>

namespace sunny::core {

Result<ChordVoicing>
chord_by_stacking(PitchClass root, Interval stack_interval, int count, int octave) {
    if (count < 2) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }
    if (stack_interval < 1 || stack_interval > 11) {
        return std::unexpected(ErrorCode::InvalidChordQuality);
    }

    auto base_midi = pitch_octave_to_midi(root, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    ChordVoicing voicing;
    voicing.root = root;
    voicing.inversion = 0;

    int midi = *base_midi;
    for (int i = 0; i < count; ++i) {
        if (auto note = MidiNote::from_int(midi)) {
            voicing.notes.push_back(*note);
        }
        midi += stack_interval;
    }

    return voicing;
}

Result<ChordVoicing> quartal_chord(PitchClass root, int count, int octave) {
    auto result = chord_by_stacking(root, 5, count, octave);
    if (result) {
        result->quality = "quartal";
    }
    return result;
}

Result<ChordVoicing> quintal_chord(PitchClass root, int count, int octave) {
    auto result = chord_by_stacking(root, 7, count, octave);
    if (result) {
        result->quality = "quintal";
    }
    return result;
}

Result<ChordVoicing> tone_cluster(PitchClass root, int width, int octave) {
    if (width < 1) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    auto base_midi = pitch_octave_to_midi(root, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    ChordVoicing voicing;
    voicing.root = root;
    voicing.quality = "cluster";
    voicing.inversion = 0;

    for (int i = 0; i <= width; ++i) {
        if (auto note = MidiNote::from_int(*base_midi + i)) {
            voicing.notes.push_back(*note);
        }
    }

    return voicing;
}

ChordVoicing polychord(const ChordVoicing& lower, const ChordVoicing& upper) {
    ChordVoicing result;
    result.root = lower.root;
    result.quality = "polychord";
    result.inversion = 0;

    // Merge notes from both chords
    result.notes.reserve(lower.notes.size() + upper.notes.size());
    result.notes.insert(result.notes.end(), lower.notes.begin(), lower.notes.end());
    result.notes.insert(result.notes.end(), upper.notes.begin(), upper.notes.end());

    // Sort ascending, remove duplicates
    std::sort(result.notes.begin(), result.notes.end());
    result.notes.erase(std::unique(result.notes.begin(), result.notes.end()), result.notes.end());

    return result;
}

} // namespace sunny::core
