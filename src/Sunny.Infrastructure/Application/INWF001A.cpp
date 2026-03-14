/**
 * @file INWF001A.cpp
 * @brief Infrastructure compilation workflow wrappers — implementation
 *
 * Component: INWF001A
 */

#include "INWF001A.h"

namespace Sunny::Infrastructure {

Sunny::Core::Result<Format::MusicXmlCompilationResult>
wf_compile_to_musicxml(const Sunny::Core::Score& score) {
    return Format::compile_score_to_musicxml(score);
}

Sunny::Core::Result<Format::LilyPondCompilationResult>
wf_compile_to_lilypond(const Sunny::Core::Score& score) {
    return Format::compile_score_to_lilypond(score);
}

Sunny::Core::Result<Format::AbletonCompilationResult>
wf_compile_to_ableton(const Sunny::Core::Score& score, LomTransport& transport) {
    return Format::compile_to_ableton(score, transport);
}

}  // namespace Sunny::Infrastructure
