/**
 * @file compilation_workflows.cpp
 * @brief Infrastructure compilation workflow wrappers — implementation
 *
 */

#include <sunny/core/corpus/workflows.hpp>
#include <sunny/infrastructure/compilation_workflows.hpp>

namespace sunny::infrastructure {

using namespace sunny::core;

sunny::core::Result<sunny::infrastructure::formats::MusicXmlCompilationResult>
compile_to_musicxml(const sunny::core::Score& score) {
    return sunny::infrastructure::formats::compile_score_to_musicxml(score);
}

sunny::core::Result<sunny::infrastructure::formats::LilyPondCompilationResult>
compile_to_lilypond(const sunny::core::Score& score) {
    return sunny::infrastructure::formats::compile_score_to_lilypond(score);
}

Result<IngestedWorkId> ingest_midi(CorpusDatabase& corpus,
                                   std::span<const std::uint8_t> data,
                                   IngestedWorkId id,
                                   ComposerProfileId composer_id,
                                   const sunny::infrastructure::corpus::IngestionOptions& options) {
    if (!corpus.composers.contains(composer_id.value))
        return std::unexpected(ErrorCode::CorpusNotFound);
    if (corpus.works.contains(id.value)) return std::unexpected(ErrorCode::CorpusDuplicateId);

    auto result = sunny::infrastructure::corpus::ingest_midi(data, id, options);
    if (!result) return std::unexpected(result.error());

    corpus.works.emplace(id.value, std::move(*result));

    auto assign = assign_work_to_composer(corpus, id, composer_id);
    if (!assign) {
        corpus.works.erase(id.value);
        return std::unexpected(assign.error());
    }

    return id;
}

Result<IngestedWorkId>
ingest_musicxml(CorpusDatabase& corpus,
                std::string_view xml,
                IngestedWorkId id,
                ComposerProfileId composer_id,
                const sunny::infrastructure::corpus::IngestionOptions& options) {
    if (!corpus.composers.contains(composer_id.value))
        return std::unexpected(ErrorCode::CorpusNotFound);
    if (corpus.works.contains(id.value)) return std::unexpected(ErrorCode::CorpusDuplicateId);

    auto result = sunny::infrastructure::corpus::ingest_musicxml(xml, id, options);
    if (!result) return std::unexpected(result.error());

    corpus.works.emplace(id.value, std::move(*result));

    auto assign = assign_work_to_composer(corpus, id, composer_id);
    if (!assign) {
        corpus.works.erase(id.value);
        return std::unexpected(assign.error());
    }

    return id;
}

} // namespace sunny::infrastructure
