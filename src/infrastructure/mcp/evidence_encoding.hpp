/**
 * @file evidence_encoding.hpp
 * @brief Private MCP JSON authority for validation and Ableton deployment evidence
 *
 * This header is intentionally private to the Infrastructure target. Standalone
 * domain tools and aggregate Project tools must expose one identical schema for
 * the same evidence type.
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/tuning.hpp>
#include <sunny/core/score/types.hpp>
#include <sunny/infrastructure/formats/ableton_mix.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <sunny/infrastructure/formats/ableton_timbre.hpp>
#include <vector>

namespace sunny::infrastructure::mcp_detail {

[[nodiscard]] nlohmann::json encode_diagnostic(const sunny::core::Diagnostic& diagnostic);

[[nodiscard]] nlohmann::json
encode_diagnostics(const std::vector<sunny::core::Diagnostic>& diagnostics);

[[nodiscard]] nlohmann::json
encode_compilation_report(const sunny::core::CompilationReport& report);

[[nodiscard]] nlohmann::json encode_score_tuning(const sunny::core::ScoreTuning& tuning);

[[nodiscard]] nlohmann::json encode_property_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonPropertyDeployment>& deployments);

[[nodiscard]] nlohmann::json encode_cue_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonCueDeployment>& deployments);

[[nodiscard]] nlohmann::json encode_clip_envelope_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonClipEnvelopeDeployment>& deployments);

[[nodiscard]] nlohmann::json encode_note_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonNoteDeployment>& deployments);

[[nodiscard]] nlohmann::json encode_timbre_parameter_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonParameterDeployment>& deployments);

[[nodiscard]] nlohmann::json encode_device_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonDeviceInsertionDeployment>&
        deployments);

[[nodiscard]] nlohmann::json encode_return_track_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ReturnTrackDeployment>&
        deployments);

[[nodiscard]] nlohmann::json encode_master_track_deployment(
    const std::optional<
        sunny::infrastructure::formats::MixCompilationResult::MasterTrackDeployment>& deployment);

[[nodiscard]] nlohmann::json encode_mix_parameter_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ParameterDeployment>&
        deployments);

[[nodiscard]] nlohmann::json encode_mix_parameter_coverage(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ParameterCoverage>&
        coverage);

[[nodiscard]] nlohmann::json output_routing_bindings_schema();

[[nodiscard]] bool parse_output_routing_bindings(
    const nlohmann::json& params,
    sunny::infrastructure::formats::AbletonOutputRoutingBindings& bindings,
    std::string& error);

[[nodiscard]] nlohmann::json encode_output_routing_bindings(
    const sunny::infrastructure::formats::AbletonOutputRoutingBindings& bindings);

[[nodiscard]] nlohmann::json encode_output_route_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::OutputRouteDeployment>&
        deployments);

} // namespace sunny::infrastructure::mcp_detail
