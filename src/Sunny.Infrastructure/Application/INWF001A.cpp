/**
 * @file INWF001A.cpp
 * @brief Infrastructure compilation workflow wrappers — implementation
 *
 * Component: INWF001A
 */

#include "INWF001A.h"

#include "Corpus/CIWF001A.h"

namespace Sunny::Infrastructure {

using namespace Sunny::Core;

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

Result<IngestedWorkId> wf_ingest_midi(
    CorpusDatabase& corpus,
    std::span<const std::uint8_t> data,
    IngestedWorkId id,
    ComposerProfileId composer_id,
    const Corpus::IngestionOptions& options
) {
    auto result = Corpus::ingest_midi(data, id, options);
    if (!result)
        return std::unexpected(result.error());

    corpus.works[id.value] = std::move(*result);

    auto assign = wf_assign_work_to_composer(corpus, id, composer_id);
    if (!assign)
        return std::unexpected(assign.error());

    return id;
}

Result<IngestedWorkId> wf_ingest_musicxml(
    CorpusDatabase& corpus,
    std::string_view xml,
    IngestedWorkId id,
    ComposerProfileId composer_id,
    const Corpus::IngestionOptions& options
) {
    auto result = Corpus::ingest_musicxml(xml, id, options);
    if (!result)
        return std::unexpected(result.error());

    corpus.works[id.value] = std::move(*result);

    auto assign = wf_assign_work_to_composer(corpus, id, composer_id);
    if (!assign)
        return std::unexpected(assign.error());

    return id;
}

}  // namespace Sunny::Infrastructure
