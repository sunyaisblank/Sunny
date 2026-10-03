/**
 * @file compilation_workflows.hpp
 * @brief Infrastructure compilation workflow wrappers
 *
 *
 * Bridges Score IR workflow tools to Infrastructure compilation targets
 * (MusicXML, LilyPond) and corpus ingestion. These cannot live in
 * sunny::core because they depend on format adapters owned by
 * sunny::infrastructure. Live deployment goes through the guarded
 * project path in formats/ableton_project.hpp.
 */

#pragma once

#include <sunny/infrastructure/corpus/ingestion.hpp>
#include <sunny/infrastructure/formats/score_to_lilypond.hpp>
#include <sunny/infrastructure/formats/score_to_musicxml.hpp>

namespace sunny::infrastructure {

[[nodiscard]] sunny::core::Result<sunny::infrastructure::formats::MusicXmlCompilationResult>
compile_to_musicxml(const sunny::core::Score& score);

[[nodiscard]] sunny::core::Result<sunny::infrastructure::formats::LilyPondCompilationResult>
compile_to_lilypond(const sunny::core::Score& score);

// =============================================================================
// Corpus Ingestion Wrappers
// =============================================================================

[[nodiscard]] sunny::core::Result<sunny::core::IngestedWorkId>
ingest_midi(sunny::core::CorpusDatabase& corpus,
            std::span<const std::uint8_t> data,
            sunny::core::IngestedWorkId id,
            sunny::core::ComposerProfileId composer_id,
            const sunny::infrastructure::corpus::IngestionOptions& options);

[[nodiscard]] sunny::core::Result<sunny::core::IngestedWorkId>
ingest_musicxml(sunny::core::CorpusDatabase& corpus,
                std::string_view xml,
                sunny::core::IngestedWorkId id,
                sunny::core::ComposerProfileId composer_id,
                const sunny::infrastructure::corpus::IngestionOptions& options);

} // namespace sunny::infrastructure
