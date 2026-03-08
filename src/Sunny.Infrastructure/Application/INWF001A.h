/**
 * @file INWF001A.h
 * @brief Infrastructure compilation workflow wrappers
 *
 * Component: INWF001A
 * Domain: IN (Infrastructure) | Category: WF (Workflow)
 *
 * Bridges Score IR workflow tools to Infrastructure compilation targets
 * (MusicXML, LilyPond). These cannot live in SIWF001A (Core) because
 * they depend on FMMX002A and FMLY002A (Infrastructure).
 */

#pragma once

#include "../Format/FMMX002A.h"
#include "../Format/FMLY002A.h"

namespace Sunny::Infrastructure {

[[nodiscard]] Sunny::Core::Result<Format::MusicXmlCompilationResult>
wf_compile_to_musicxml(const Sunny::Core::Score& score);

[[nodiscard]] Sunny::Core::Result<Format::LilyPondCompilationResult>
wf_compile_to_lilypond(const Sunny::Core::Score& score);

}  // namespace Sunny::Infrastructure
