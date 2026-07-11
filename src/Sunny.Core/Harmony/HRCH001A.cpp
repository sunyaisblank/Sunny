/**
 * @file HRCH001A.cpp
 * @brief Chromatic harmony implementation
 *
 * Component: HRCH001A
 */

#include "HRCH001A.h"
#include "HRRN001A.h"
#include "../Pitch/PTMN001A.h"

#include <algorithm>

namespace Sunny::Core {

Result<ChordVoicing> neapolitan_sixth(
    PitchClass key_root,
    bool /*is_minor*/,
    int octave
) {
    // bII: root is one semitone above the tonic
    PitchClass nea_root = transpose(key_root, 1);

    // Generate bII major triad
    auto chord = generate_chord(nea_root, "major", octave);
    if (!chord.has_value()) {
        return std::unexpected(chord.error());
    }

    // First inversion: move the root up an octave; the factory refuses a
    // raise past MIDI 127, matching the old guard.
    chord->inversion = 1;
    if (chord->notes.size() >= 3) {
        if (auto raised = MidiNote::from_int(chord->notes[0] + 12)) {
            chord->notes[0] = *raised;
            std::sort(chord->notes.begin(), chord->notes.end());
        }
    }

    return chord;
}

Result<ChordVoicing> augmented_sixth(
    AugSixthType type,
    PitchClass key_root,
    bool is_minor,
    int octave
) {
    // In minor: b6 = key_root + 8 (Ab in C minor)
    // In major: b6 = key_root + 8 (borrowed from parallel minor)
    PitchClass b6 = transpose(key_root, 8);
    PitchClass tonic = key_root;
    PitchClass sharp4 = transpose(key_root, 6);  // #4 = tritone above root

    auto base_midi = pitch_octave_to_midi(b6, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    ChordVoicing voicing;
    voicing.root = b6;
    voicing.inversion = 0;

    // b6 is the bass
    int bass = *base_midi;
    // Ensure tonic and #4 are above the bass
    int tonic_midi = bass + ((static_cast<int>(tonic) - static_cast<int>(b6) + 12) % 12);
    if (tonic_midi <= bass) tonic_midi += 12;
    int sharp4_midi = bass + ((static_cast<int>(sharp4) - static_cast<int>(b6) + 12) % 12);
    if (sharp4_midi <= bass) sharp4_midi += 12;

    // Store upper voices through the validated factory; an upper voice
    // pushed past MIDI 127 (octave at the top of the range) is refused
    // where it previously truncated silently.
    auto push_note = [&voicing](int midi_val) -> bool {
        auto note = MidiNote::from_int(midi_val);
        if (!note) {
            return false;
        }
        voicing.notes.push_back(*note);
        return true;
    };

    switch (type) {
        case AugSixthType::Italian: {
            voicing.quality = "It+6";
            if (!push_note(bass) || !push_note(tonic_midi) ||
                !push_note(sharp4_midi)) {
                return std::unexpected(ErrorCode::ChordGenerationFailed);
            }
            break;
        }
        case AugSixthType::French: {
            voicing.quality = "Fr+6";
            PitchClass scale_2 = transpose(key_root, 2);  // D in C
            int d_midi = bass + ((static_cast<int>(scale_2) - static_cast<int>(b6) + 12) % 12);
            if (d_midi <= bass) d_midi += 12;
            if (!push_note(bass) || !push_note(tonic_midi) ||
                !push_note(d_midi) || !push_note(sharp4_midi)) {
                return std::unexpected(ErrorCode::ChordGenerationFailed);
            }
            break;
        }
        case AugSixthType::German: {
            voicing.quality = "Ger+6";
            // b3 in minor = key_root + 3 (Eb in C minor)
            // b3 in major = key_root + 3 (borrowed from parallel minor)
            PitchClass b3 = transpose(key_root, 3);
            int b3_midi = bass + ((static_cast<int>(b3) - static_cast<int>(b6) + 12) % 12);
            if (b3_midi <= bass) b3_midi += 12;
            if (!push_note(bass) || !push_note(tonic_midi) ||
                !push_note(b3_midi) || !push_note(sharp4_midi)) {
                return std::unexpected(ErrorCode::ChordGenerationFailed);
            }
            break;
        }
    }

    std::sort(voicing.notes.begin(), voicing.notes.end());
    return voicing;
}

Result<ChordVoicing> tritone_substitution(
    PitchClass chord_root,
    int octave
) {
    // New root is a tritone (6 semitones) away
    PitchClass new_root = transpose(chord_root, 6);
    return generate_chord(new_root, "7", octave);
}

Result<ChordVoicing> common_tone_diminished(
    PitchClass target_root,
    int octave
) {
    // CT dim root is one semitone above the target root
    PitchClass ct_root = transpose(target_root, 1);
    return generate_chord(ct_root, "dim7", octave);
}

}  // namespace Sunny::Core
