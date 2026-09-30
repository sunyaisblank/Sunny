/**
 * @file validation.cpp
 * @brief Cross-IR project validation implementation
 */

#include <algorithm>
#include <iterator>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/timbre/validation.hpp>

namespace sunny::core {

namespace {

void validate_audio_follower_source(const Score& score,
                                    const TimbreProfile& profile,
                                    const ModulationSource& source,
                                    const char* role,
                                    std::vector<Diagnostic>& diagnostics) {
    if (source.type != ModulationSourceType::AudioFollower || source.sidechain_part.value == 0)
        return;
    const bool exists = std::any_of(score.parts.begin(), score.parts.end(), [&](const Part& part) {
        return part.id == source.sidechain_part;
    });
    if (!exists) {
        diagnostics.push_back({ValidationSeverity::Error,
                               "P2",
                               std::string(role) + " AudioFollower in TimbreProfile " +
                                   std::to_string(profile.id.value) + " references unknown Part " +
                                   std::to_string(source.sidechain_part.value),
                               std::nullopt,
                               profile.part_id,
                               ErrorCode::InvalidModSource});
    }
}

} // namespace

std::vector<Diagnostic> validate_project(const ProjectView& project) {
    auto diagnostics = validate_score(project.score);

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
    }

    auto timbre_correspondence =
        validate_timbre_correspondence(project.score, project.timbre_profiles);
    diagnostics.insert(diagnostics.end(),
                       std::make_move_iterator(timbre_correspondence.begin()),
                       std::make_move_iterator(timbre_correspondence.end()));

    for (const auto* profile : project.timbre_profiles) {
        if (profile == nullptr) continue;
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
