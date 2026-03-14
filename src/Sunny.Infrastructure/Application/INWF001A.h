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
#include "../Bridge/INTP001A.h"
#include "../Format/FMAL001A.h"
#include "../Corpus/CIIN001A.h"

namespace Sunny::Infrastructure {

[[nodiscard]] Sunny::Core::Result<Format::MusicXmlCompilationResult>
wf_compile_to_musicxml(const Sunny::Core::Score& score);

[[nodiscard]] Sunny::Core::Result<Format::LilyPondCompilationResult>
wf_compile_to_lilypond(const Sunny::Core::Score& score);

[[nodiscard]] Sunny::Core::Result<Format::AbletonCompilationResult>
wf_compile_to_ableton(const Sunny::Core::Score& score, LomTransport& transport);

// =============================================================================
// Corpus Ingestion Wrappers
// =============================================================================

[[nodiscard]] Sunny::Core::Result<Sunny::Core::IngestedWorkId> wf_ingest_midi(
    Sunny::Core::CorpusDatabase& corpus,
    std::span<const std::uint8_t> data,
    Sunny::Core::IngestedWorkId id,
    Sunny::Core::ComposerProfileId composer_id,
    const Corpus::IngestionOptions& options);

[[nodiscard]] Sunny::Core::Result<Sunny::Core::IngestedWorkId> wf_ingest_musicxml(
    Sunny::Core::CorpusDatabase& corpus,
    std::string_view xml,
    Sunny::Core::IngestedWorkId id,
    Sunny::Core::ComposerProfileId composer_id,
    const Corpus::IngestionOptions& options);

}  // namespace Sunny::Infrastructure
