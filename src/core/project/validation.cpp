/**
 * @file validation.cpp
 * @brief Cross-IR project validation implementation
 */

#include <algorithm>
#include <iterator>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/timbre/validation.hpp>

namespace sunny::core {

namespace {

void validate_audio_follower_source(const Score& score,
                                    const TimbreProfile& profile,
                                    const ModulationSource& source,
                                    const std::string& role,
                                    std::vector<Diagnostic>& diagnostics) {
    if (source.type != ModulationSourceType::AudioFollower) return;
    const bool exists = std::any_of(score.parts.begin(), score.parts.end(), [&](const Part& part) {
        return part.id == source.sidechain_part;
    });
    if (!exists) {
        diagnostics.push_back({ValidationSeverity::Error,
                               "P2",
                               role + " AudioFollower in TimbreProfile " +
                                   std::to_string(profile.id.value) + " references unknown Part " +
                                   std::to_string(source.sidechain_part.value),
                               std::nullopt,
                               profile.part_id,
                               ErrorCode::InvalidModSource});
    }
}

void validate_source_part_references(const Score& score,
                                     const TimbreProfile& profile,
                                     const SoundSourceData& source,
                                     const std::string& path,
                                     std::vector<Diagnostic>& diagnostics) {
    const auto* hybrid = std::get_if<HybridSource>(&source.data);
    if (!hybrid) return;
    if (hybrid->routing.type == LayerRoutingType::Crossfade)
        validate_audio_follower_source(
            score, profile, hybrid->routing.crossfade_source, path + ".crossfade", diagnostics);
    for (std::size_t index = 0; index < hybrid->layers.size(); ++index)
        if (hybrid->layers[index])
            validate_source_part_references(score,
                                            profile,
                                            *hybrid->layers[index],
                                            path + ".layers[" + std::to_string(index) + "]",
                                            diagnostics);
}

void validate_effect_part_references(const Score& score,
                                     const TimbreProfile& profile,
                                     std::vector<Diagnostic>& diagnostics) {
    for (const auto& effect : profile.insert_chain.effects) {
        const auto* compressor = std::get_if<CompressorEffect>(&effect.parameters);
        if (!compressor || !compressor->sidechain) continue;
        const auto part_id = compressor->sidechain->source;
        if (std::ranges::any_of(score.parts, [&](const Part& part) { return part.id == part_id; }))
            continue;
        diagnostics.push_back({ValidationSeverity::Error,
                               "P2",
                               "Compressor effect " + std::to_string(effect.id.value) +
                                   " in TimbreProfile " + std::to_string(profile.id.value) +
                                   " references unknown sidechain Part " +
                                   std::to_string(part_id.value),
                               std::nullopt,
                               profile.part_id,
                               ErrorCode::InvalidSidechain});
    }
}

} // namespace

std::vector<Diagnostic> validate_project(const ProjectView& project) {
    auto diagnostics = validate_score(project.score);

    const auto control_time = [&](ScoreTime position,
                                  const std::string& role,
                                  std::optional<PartId> part = std::nullopt) {
        // The final downbeat is a valid authored control endpoint. All other
        // controls must occupy an actual bar and exact in-metre offset.
        const bool final_downbeat =
            position.bar > 0 &&
            static_cast<std::uint64_t>(position.bar) ==
                static_cast<std::uint64_t>(project.score.metadata.total_bars) + 1 &&
            position.beat == Beat::zero();
        if (final_downbeat) return;
        if (position.bar == 0 || position.bar > project.score.metadata.total_bars ||
            !score_time_to_absolute_beat(position, project.score.time_map))
            diagnostics.push_back({ValidationSeverity::Error,
                                   "P4",
                                   role + " lies outside its owning Score's exact temporal domain",
                                   position,
                                   part,
                                   ErrorCode::ProjectValidationFailed});
    };

    for (const auto* profile : project.timbre_profiles) {
        if (profile == nullptr) {
            diagnostics.push_back({ValidationSeverity::Error,
                                   "P1",
                                   "Project contains a null TimbreProfile reference",
                                   std::nullopt,
                                   std::nullopt,
                                   ErrorCode::ProjectMissingComponent});
            continue;
        }
        auto profile_diagnostics = validate_timbre(*profile);
        diagnostics.insert(diagnostics.end(),
                           std::make_move_iterator(profile_diagnostics.begin()),
                           std::make_move_iterator(profile_diagnostics.end()));
        for (const auto& lane : profile->parameter_automation)
            for (const auto& point : lane.breakpoints)
                control_time(point.time, "Timbre automation", profile->part_id);
        for (const auto& morph : profile->preset_morphs) {
            control_time(morph.start, "Preset morph start", profile->part_id);
            control_time(morph.end, "Preset morph end", profile->part_id);
        }
    }

    auto timbre_correspondence =
        validate_timbre_correspondence(project.score, project.timbre_profiles);
    diagnostics.insert(diagnostics.end(),
                       std::make_move_iterator(timbre_correspondence.begin()),
                       std::make_move_iterator(timbre_correspondence.end()));

    for (const auto* profile : project.timbre_profiles) {
        if (profile == nullptr) continue;
        validate_source_part_references(
            project.score, *profile, profile->source, "source", diagnostics);
        validate_effect_part_references(project.score, *profile, diagnostics);
        for (const auto& routing : profile->modulation.routings) {
            validate_audio_follower_source(
                project.score, *profile, routing.source, "Primary", diagnostics);
            if (routing.via) {
                validate_audio_follower_source(
                    project.score, *profile, *routing.via, "Via", diagnostics);
            }
        }
    }

    auto mix_diagnostics = validate_mix(project.mix);
    for (const auto& lane : project.mix.automation)
        for (const auto& point : lane.breakpoints)
            control_time(point.time, "Mix automation");
    diagnostics.insert(diagnostics.end(),
                       std::make_move_iterator(mix_diagnostics.begin()),
                       std::make_move_iterator(mix_diagnostics.end()));

    auto mix_correspondence = validate_mix_correspondence(project.score, project.mix);
    diagnostics.insert(diagnostics.end(),
                       std::make_move_iterator(mix_correspondence.begin()),
                       std::make_move_iterator(mix_correspondence.end()));

    std::stable_sort(diagnostics.begin(), diagnostics.end(), [](const auto& lhs, const auto& rhs) {
        return static_cast<int>(lhs.severity) < static_cast<int>(rhs.severity);
    });
    return diagnostics;
}

bool is_project_compilable(const ProjectView& project) {
    const auto diagnostics = validate_project(project);
    return std::none_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

} // namespace sunny::core
