/**
 * @file PTPC001A.cpp
 * @brief Pitch class operations implementation
 *
 * Component: PTPC001A
 */

#include "PTPC001A.h"

namespace Sunny::Core {

Result<PitchClass> note_to_pitch_class(std::string_view name) noexcept {
    if (name.empty()) {
        return std::unexpected(ErrorCode::InvalidNoteName);
    }

    // Parse base note
    int base_pc;
    switch (name[0]) {
        case 'C': case 'c': base_pc = 0; break;
        case 'D': case 'd': base_pc = 2; break;
        case 'E': case 'e': base_pc = 4; break;
        case 'F': case 'f': base_pc = 5; break;
        case 'G': case 'g': base_pc = 7; break;
        case 'A': case 'a': base_pc = 9; break;
        case 'B': case 'b': base_pc = 11; break;
        default:
            return std::unexpected(ErrorCode::InvalidNoteName);
    }

    // Parse accidentals
    int modifier = 0;
    for (std::size_t i = 1; i < name.size(); ++i) {
        switch (name[i]) {
            case '#': modifier += 1; break;
            case 'b': modifier -= 1; break;
            case 'x': modifier += 2; break;  // Double sharp
            default:
                // Ignore octave numbers
                if (name[i] >= '0' && name[i] <= '9') continue;
                return std::unexpected(ErrorCode::InvalidNoteName);
        }
    }

    int pc = (base_pc + modifier % 12 + 12) % 12;
    return static_cast<PitchClass>(pc);
}

}  // namespace Sunny::Core
