/**
 * @file project_session.cpp
 * @brief Bound project authoring and request-atomic history
 */
#include "evidence_encoding.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/project_session.hpp>
#include <sunny/infrastructure/mcp/project_timeline.hpp>
#include <sunny/infrastructure/mcp/session_ids.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

json error(const std::string& message) {
    return {{"error", message}};
}

json project_info(const ProjectRecord& project) {
    return {{"score_id", project.score_id},
            {"timbre_profile_ids", project.profile_ids},
            {"mix_graph_id", project.mix_graph_id},
            {"revision", project.revision},
            {"can_undo", !project.undo_entries.empty()},
            {"can_redo", !project.redo_entries.empty()}};
}

std::optional<std::uint64_t> owner_of_profile(const McpSession& session, std::uint64_t id) {
    for (const auto& [score_id, project] : session.project->projects)
        if (std::ranges::find(project.profile_ids, id) != project.profile_ids.end())
            return score_id;
    return std::nullopt;
}

std::optional<std::uint64_t> owner_of_mix(const McpSession& session, std::uint64_t id) {
    for (const auto& [score_id, project] : session.project->projects)
        if (project.mix_graph_id == id) return score_id;
    return std::nullopt;
}

void append_preset_diagnostics(const McpSession& session,
                               const std::vector<const TimbreProfile*>& profiles,
                               std::vector<Diagnostic>& diagnostics) {
    std::set<std::uint64_t> preset_ids;
    for (const auto& preset : session.timbre->preset_library)
        if (preset.id.value == 0 || !preset_ids.insert(preset.id.value).second)
            diagnostics.push_back({ValidationSeverity::Error,
                                   "P3",
                                   "Shared preset identities must be positive and unique",
                                   std::nullopt,
                                   std::nullopt,
                                   ErrorCode::TimbreDuplicateId});
    for (const auto* profile : profiles) {
        if (!profile) continue;
        auto available = preset_ids;
        for (const auto& preset : profile->presets)
            available.insert(preset.id.value);
        for (const auto& morph : profile->preset_morphs)
            if (!available.contains(morph.from_preset.value) ||
                !available.contains(morph.to_preset.value))
                diagnostics.push_back({ValidationSeverity::Error,
                                       "P3",
                                       "Preset morph references a missing preset identity",
                                       std::nullopt,
                                       profile->part_id,
                                       ErrorCode::TimbreNotFound});
    }
}

std::vector<Diagnostic> project_diagnostics(const McpSession& session,
                                            const ProjectRecord& project) {
    const auto* score = session.score->find(project.score_id);
    const auto* mix = session.mix->find(project.mix_graph_id);
    if (!score || !mix)
        return {{ValidationSeverity::Error,
                 "P1",
                 "Project document is missing",
                 std::nullopt,
                 std::nullopt,
                 ErrorCode::ProjectMissingComponent}};
    std::vector<const TimbreProfile*> profiles;
    for (const auto id : project.profile_ids)
        profiles.push_back(session.timbre->find(id));
    auto diagnostics = validate_project({*score, profiles, *mix});
    append_preset_diagnostics(session, profiles, diagnostics);
    std::stable_sort(
        diagnostics.begin(), diagnostics.end(), [](const auto& left, const auto& right) {
            return static_cast<int>(left.severity) < static_cast<int>(right.severity);
        });
    return diagnostics;
}

bool has_error(const std::vector<Diagnostic>& diagnostics) {
    return std::ranges::any_of(diagnostics, [](const auto& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

json invalid_project(const std::vector<Diagnostic>& diagnostics) {
    json encoded = json::array();
    for (const auto& diagnostic : diagnostics)
        encoded.push_back(mcp_detail::encode_diagnostic(diagnostic));
    return {{"error", "Bound project mutation violates project invariants"},
            {"diagnostics", std::move(encoded)}};
}

bool failed(const json& result) {
    return result.is_object() &&
           (result.contains("error") ||
            (result.contains("success") && result["success"].is_boolean() &&
             !result["success"].get<bool>()) ||
            (result.contains("ok") && result["ok"].is_boolean() && !result["ok"].get<bool>()));
}

struct ParsedProject {
    Score score;
    std::map<std::uint64_t, TimbreProfile> profiles;
    MixGraph mix;
    std::vector<TimbrePreset> presets;
};

Result<ParsedProject> parse_snapshot(const ProjectSnapshot& snapshot) {
    auto score = score_from_json(snapshot.documents.at("score"));
    auto mix = mix_from_json(snapshot.documents.at("mix"));
    if (!score) return std::unexpected(score.error());
    if (!mix) return std::unexpected(mix.error());
    ParsedProject parsed;
    parsed.score = std::move(*score);
    parsed.mix = std::move(*mix);
    for (const auto& encoded : snapshot.documents.at("timbre_profiles")) {
        auto profile = timbre_from_json(encoded);
        if (!profile) return std::unexpected(profile.error());
        const auto id = profile->id.value;
        if (!parsed.profiles.emplace(id, std::move(*profile)).second)
            return std::unexpected(ErrorCode::TimbreDuplicateId);
    }
    for (const auto& encoded : snapshot.documents.at("preset_library")) {
        auto preset = timbre_preset_from_json(encoded);
        if (!preset) return std::unexpected(preset.error());
        parsed.presets.push_back(std::move(*preset));
    }
    return parsed;
}

struct AllocationState {
    std::array<std::uint64_t, 9> counters;
    explicit AllocationState(const McpSession& session)
        : counters{session.score->next_score_id,
                   session.timbre->next_profile_id,
                   session.timbre->next_effect_id,
                   session.timbre->next_preset_id,
                   session.mix->next_graph_id,
                   session.mix->next_group_id,
                   session.mix->next_aux_id,
                   session.mix->next_effect_id,
                   session.mix->next_ref_id} {}

    void restore(const McpSession& session) const noexcept {
        session.score->next_score_id = counters[0];
        session.timbre->next_profile_id = counters[1];
        session.timbre->next_effect_id = counters[2];
        session.timbre->next_preset_id = counters[3];
        session.mix->next_graph_id = counters[4];
        session.mix->next_group_id = counters[5];
        session.mix->next_aux_id = counters[6];
        session.mix->next_effect_id = counters[7];
        session.mix->next_ref_id = counters[8];
    }
    bool advanced_without_wrap(const McpSession& session) const {
        const AllocationState after(session);
        for (std::size_t index = 0; index < counters.size(); ++index)
            if (after.counters[index] < counters[index]) return false;
        return true;
    }
};

/**
 * Stage private codec-created candidates in the existing identity-stable slots.
 * These slots are observable only through this server's serialized request
 * boundary. The original values remain owned here until validation and history
 * preparation finish; rollback uses swaps/node insertion without allocation.
 */
class RequestStage {
  public:
    RequestStage(const McpSession& session, const ProjectRecord& project, ParsedProject candidates)
        : session_(session), project_(project), originals_(std::move(candidates)),
          allocations_(session), previous_active_(session.score->project_transaction_score) {
        for (const auto& [id, profile] : session_.timbre->profiles) {
            static_cast<void>(profile);
            global_profile_ids_.insert(id);
        }
        // All allocation above occurs before any canonical slot changes.
        std::swap(*session_.score->find(project_.score_id), originals_.score);
        std::swap(*session_.mix->find(project_.mix_graph_id), originals_.mix);
        for (auto& [id, profile] : originals_.profiles)
            std::swap(*session_.timbre->find(id), profile);
        std::swap(session_.timbre->preset_library, originals_.presets);
        session_.score->project_transaction_score = project_.score_id;
    }
    RequestStage(const RequestStage&) = delete;
    RequestStage& operator=(const RequestStage&) = delete;
    ~RequestStage() noexcept {
        session_.score->project_transaction_score = previous_active_;
        if (committed_) return;
        std::swap(*session_.score->find(project_.score_id), originals_.score);
        std::swap(*session_.mix->find(project_.mix_graph_id), originals_.mix);
        std::swap(session_.timbre->preset_library, originals_.presets);
        std::erase_if(session_.timbre->profiles, [&](const auto& entry) {
            return !global_profile_ids_.contains(entry.first);
        });
        while (!originals_.profiles.empty()) {
            auto node = originals_.profiles.extract(originals_.profiles.begin());
            const auto found = session_.timbre->profiles.find(node.key());
            if (found == session_.timbre->profiles.end())
                session_.timbre->profiles.insert(std::move(node));
            else
                std::swap(found->second, node.mapped());
        }
        allocations_.restore(session_);
    }
    void commit() noexcept { committed_ = true; }
    bool counters_valid() const { return allocations_.advanced_without_wrap(session_); }

  private:
    const McpSession& session_;
    const ProjectRecord& project_;
    ParsedProject originals_;
    AllocationState allocations_;
    std::optional<std::uint64_t> previous_active_;
    std::set<std::uint64_t> global_profile_ids_;
    bool committed_ = false;
};

Result<std::uint64_t> reserve_fresh(std::set<std::uint64_t>& reservations) {
    std::uint64_t candidate = 1;
    while (reservations.contains(candidate)) {
        if (candidate == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        ++candidate;
    }
    reservations.insert(candidate);
    return candidate;
}

Result<std::uint64_t> pending_profile(const McpSession& session) {
    auto batch = mcp_detail::checked_session_id_batch(
        session.timbre->next_profile_id,
        mcp_detail::maximum_session_store_id(session.timbre->profiles));
    if (!batch) return std::unexpected(batch.error());
    return batch->first;
}

bool source_references_part(const SoundSourceData& source, PartId removed) {
    const auto* hybrid = std::get_if<HybridSource>(&source.data);
    if (!hybrid) return false;
    const auto& crossfade = hybrid->routing.crossfade_source;
    if (hybrid->routing.type == LayerRoutingType::Crossfade &&
        crossfade.type == ModulationSourceType::AudioFollower &&
        crossfade.sidechain_part == removed)
        return true;
    return std::ranges::any_of(hybrid->layers, [&](const auto& layer) {
        return layer && source_references_part(*layer, removed);
    });
}

bool profile_references_part(const TimbreProfile& profile, PartId removed) {
    if (source_references_part(profile.source, removed)) return true;
    for (const auto& effect : profile.insert_chain.effects) {
        const auto* compressor = std::get_if<CompressorEffect>(&effect.parameters);
        if (compressor && compressor->sidechain && compressor->sidechain->source == removed)
            return true;
    }
    for (const auto& routing : profile.modulation.routings) {
        const auto references = [&](const ModulationSource& source) {
            return source.type == ModulationSourceType::AudioFollower &&
                   source.sidechain_part == removed;
        };
        if (references(routing.source) || (routing.via && references(*routing.via))) return true;
    }
    return false;
}

std::optional<std::string> close_part_bindings(const McpSession& session,
                                               const ProjectSnapshot& before,
                                               ProjectRecord& project,
                                               const std::string& name,
                                               json& result) {
    auto& score = *session.score->find(project.score_id);
    auto& mix = *session.mix->find(project.mix_graph_id);
    std::set<std::uint64_t> before_parts;
    for (const auto& part : before.documents.at("score").at("parts"))
        before_parts.insert(part.at("id").get<std::uint64_t>());
    std::set<std::uint64_t> after_parts;
    for (const auto& part : score.parts)
        after_parts.insert(part.id.value);

    if (before_parts != after_parts && name != "score_add_part" && name != "score_remove_part")
        return "This tool has no admitted project Part lifecycle";

    for (auto& part : score.parts) {
        if (before_parts.contains(part.id.value)) continue;
        // The core allocator reserves observed/retired typed Score identities.
        // Keep its fresh Part identity unchanged, including every typed edge.
        result["part_id"] = part.id.value;
        auto profile_id = pending_profile(session);
        if (!profile_id) return "Timbre identity domain exhausted";
        auto profile =
            create_timbre_profile(TimbreProfileId{*profile_id}, part.id, part.definition.name);
        session.timbre->profiles.emplace(*profile_id, std::move(profile));
        ++session.timbre->next_profile_id;
        project.profile_ids.push_back(*profile_id);
        auto channel_id = reserve_fresh(project.reserved_channel_ids);
        if (!channel_id) return "Channel identity domain exhausted";
        ChannelStrip channel;
        channel.id = ChannelStripId{*channel_id};
        channel.part_id = part.id;
        mix.channels.push_back(std::move(channel));
        result["timbre_profile_id"] = *profile_id;
        result["channel_id"] = *channel_id;
    }

    // Recompute after Parts following the admitted fresh-Part identity change.
    after_parts.clear();
    for (const auto& part : score.parts)
        after_parts.insert(part.id.value);
    for (const auto removed : before_parts) {
        if (after_parts.contains(removed)) continue;
        for (const auto id : project.profile_ids) {
            const auto* profile = session.timbre->find(id);
            if (profile && profile->part_id.value != removed &&
                profile_references_part(*profile, PartId{removed}))
                return "Retained Timbre references the removed Part; remove those references first";
        }
        std::set<std::uint64_t> removed_effect_ids;
        for (const auto& channel : mix.channels)
            if (channel.part_id.value == removed)
                for (const auto& effect : channel.insert_chain.effects)
                    removed_effect_ids.insert(effect.id.value);
        for (const auto& channel : mix.channels)
            if (channel.part_id.value != removed && channel.intent)
                for (const auto& rationale : channel.intent->processing_rationale)
                    if (removed_effect_ids.contains(rationale.effect_id.value))
                        return "Retained processing rationale references removed channel "
                               "processing";
        std::erase_if(project.profile_ids, [&](std::uint64_t id) {
            const auto* profile = session.timbre->find(id);
            if (!profile || profile->part_id.value != removed) return false;
            session.timbre->profiles.erase(id);
            return true;
        });
        for (const auto& channel : mix.channels)
            if (channel.part_id.value == removed)
                for (auto& group : mix.group_buses)
                    std::erase(group.member_channels, channel.id);
        std::erase_if(mix.channels,
                      [&](const auto& channel) { return channel.part_id.value == removed; });
    }

    if (name != "score_reorder_parts" && before_parts == after_parts) return std::nullopt;
    // Author order changes only order. Paths use stable Part/Channel identities.
    std::vector<std::uint64_t> ordered_profiles;
    std::vector<ChannelStrip> ordered_channels;
    for (const auto& part : score.parts) {
        for (const auto id : project.profile_ids)
            if (session.timbre->find(id)->part_id == part.id) ordered_profiles.push_back(id);
        for (auto& channel : mix.channels)
            if (channel.part_id == part.id) ordered_channels.push_back(std::move(channel));
    }
    project.profile_ids = std::move(ordered_profiles);
    mix.channels = std::move(ordered_channels);
    return std::nullopt;
}

std::map<std::uint64_t, json> preset_map(const ProjectSnapshot& snapshot) {
    std::map<std::uint64_t, json> presets;
    for (const auto& preset : snapshot.documents.at("preset_library"))
        presets.emplace(preset.at("id").get<std::uint64_t>(), preset);
    return presets;
}

std::vector<std::uint64_t> changed_presets(const ProjectSnapshot& before,
                                           const ProjectSnapshot& after) {
    const auto old_presets = preset_map(before);
    const auto new_presets = preset_map(after);
    std::set<std::uint64_t> changed;
    for (const auto& [id, preset] : old_presets) {
        const auto found = new_presets.find(id);
        if (found == new_presets.end() || found->second != preset) changed.insert(id);
    }
    for (const auto& [id, preset] : new_presets) {
        const auto found = old_presets.find(id);
        if (found == old_presets.end() || found->second != preset) changed.insert(id);
    }
    return {changed.begin(), changed.end()};
}

json history_step(const McpSession& session, std::uint64_t score_id, bool forward) {
    auto& live = session.project->projects.at(score_id);
    const auto& entries = forward ? live.redo_entries : live.undo_entries;
    if (entries.empty()) return error(forward ? "nothing to redo" : "nothing to undo");
    if (live.revision == std::numeric_limits<std::uint64_t>::max() ||
        session.score->find(score_id)->version == std::numeric_limits<std::uint64_t>::max())
        return error("project version domain exhausted");
    const auto entry = entries.back();
    const auto& target = forward ? entry.after : entry.before;
    auto current = snapshot_project(session, live);
    if (!current) return error("Cannot snapshot bound project");
    auto candidates = parse_snapshot(*current);
    auto restored = parse_snapshot(*target);
    if (!candidates || !restored) return error("Cannot prepare project history snapshot");
    for (const auto id : target->profile_ids) {
        if (session.timbre->profiles.contains(id) &&
            std::ranges::find(live.profile_ids, id) == live.profile_ids.end())
            return error("Restored profile identity is already in use");
    }

    auto desired_presets = session.timbre->preset_library;
    const auto target_presets = preset_map(*target);
    const auto expected_presets = preset_map(*(forward ? entry.before : entry.after));
    std::map<std::uint64_t, json> current_presets;
    for (const auto& preset : desired_presets)
        current_presets.emplace(preset.id.value, timbre_preset_to_json(preset));
    for (const auto id : entry.changed_preset_ids) {
        const auto expected = expected_presets.find(id);
        const auto actual = current_presets.find(id);
        if ((expected == expected_presets.end()) != (actual == current_presets.end()) ||
            (expected != expected_presets.end() && expected->second != actual->second))
            return error("Shared preset changed after the project history entry");
        const auto found = target_presets.find(id);
        if (found == target_presets.end()) {
            // A different project's/scratch document's retained morph is an
            // incoming reference and prevents removing this shared library item.
            for (const auto& [profile_id, profile] : session.timbre->profiles) {
                if (std::ranges::find(live.profile_ids, profile_id) != live.profile_ids.end())
                    continue;
                if (std::ranges::any_of(profile.presets,
                                        [&](const auto& preset) { return preset.id.value == id; }))
                    continue;
                for (const auto& morph : profile.preset_morphs)
                    if (morph.from_preset.value == id || morph.to_preset.value == id)
                        return error(
                            "Retained preset morph references a preset this undo would remove");
            }
        }
        std::erase_if(desired_presets, [&](const auto& preset) { return preset.id.value == id; });
        if (found != target_presets.end()) {
            auto preset = timbre_preset_from_json(found->second);
            if (!preset) return error("Cannot restore preset library entry");
            // Library IDs are allocated monotonically. Insert at the original
            // identity order while retaining independently created entries.
            const auto position = std::ranges::find_if(
                desired_presets, [&](const auto& item) { return item.id.value > id; });
            desired_presets.insert(position, std::move(*preset));
        }
    }
    retain_score_identities(restored->score, *session.score->find(score_id));
    restored->score.version = session.score->find(score_id)->version + 1;
    ProjectRecord next = live;
    next.profile_ids = target->profile_ids;
    ++next.revision;
    auto& source_entries = forward ? next.redo_entries : next.undo_entries;
    auto& destination_entries = forward ? next.undo_entries : next.redo_entries;
    source_entries.pop_back();
    destination_entries.push_back(entry);
    json response = {
        {"ok", true}, {"version", restored->score.version}, {"project", project_info(next)}};
    response["can_undo"] = !next.undo_entries.empty();
    response["can_redo"] = !next.redo_entries.empty();

    RequestStage stage(session, live, std::move(*candidates));
    std::swap(*session.score->find(score_id), restored->score);
    std::swap(*session.mix->find(live.mix_graph_id), restored->mix);
    for (const auto id : live.profile_ids)
        if (std::ranges::find(target->profile_ids, id) == target->profile_ids.end())
            session.timbre->profiles.erase(id);
    while (!restored->profiles.empty()) {
        auto node = restored->profiles.extract(restored->profiles.begin());
        const auto found = session.timbre->profiles.find(node.key());
        if (found == session.timbre->profiles.end())
            session.timbre->profiles.insert(std::move(node));
        else
            std::swap(found->second, node.mapped());
    }
    session.timbre->preset_library = std::move(desired_presets);
    const auto diagnostics = project_diagnostics(session, next);
    if (has_error(diagnostics)) return invalid_project(diagnostics);
    // Encoding failures must happen while originals are still available.
    // In-memory snapshots preserve the child codecs' existing scalar values.
    auto prepared = snapshot_project(session, next);
    if (!prepared) return error("Cannot snapshot restored project");
    static_cast<void>(prepared->documents.dump());
    static_cast<void>(response.dump());
    live = std::move(next);
    stage.commit();
    return response;
}

json execute_tool(const McpSession& session,
                  McpDocumentDomain domain,
                  const std::string& name,
                  const json& params,
                  const McpToolHandler& handler) {
    std::optional<std::uint64_t> owner;
    if (domain == McpDocumentDomain::Score && params.contains("score_id")) {
        const auto id = detail::checked_integer<std::uint64_t>(params.at("score_id"), "score id");
        if (session.project->projects.contains(id)) owner = id;
    } else if (domain == McpDocumentDomain::Timbre && params.contains("profile_id")) {
        owner = owner_of_profile(
            session, detail::checked_integer<std::uint64_t>(params.at("profile_id"), "profile id"));
    } else if (domain == McpDocumentDomain::Mix && params.contains("graph_id")) {
        owner = owner_of_mix(
            session, detail::checked_integer<std::uint64_t>(params.at("graph_id"), "graph id"));
    }
    if (!owner) return handler(params);
    if (name == "score_undo" || name == "score_redo")
        return history_step(session, *owner, name == "score_redo");
    auto& live = session.project->projects.at(*owner);
    if (live.revision == std::numeric_limits<std::uint64_t>::max())
        return error("project version domain exhausted");
    const auto initial_diagnostics = project_diagnostics(session, live);
    if (has_error(initial_diagnostics)) return invalid_project(initial_diagnostics);
    auto captured = snapshot_project(session, live);
    if (!captured) return error("Cannot snapshot bound project");
    auto before = std::make_shared<const ProjectSnapshot>(std::move(*captured));
    auto candidates = parse_snapshot(*before);
    if (!candidates) return error("Cannot prepare bound project mutation");
    ProjectRecord next = live;
    RequestStage stage(session, live, std::move(*candidates));
    auto result = handler(params);
    if (failed(result)) return result;
    auto relocation = relocate_project_controls(session, next, name, params);
    if (!relocation.empty()) result["control_relocation"] = std::move(relocation);
    if (auto failure = close_part_bindings(session, *before, next, name, result))
        return error(*failure);
    const auto diagnostics = project_diagnostics(session, next);
    if (has_error(diagnostics)) return invalid_project(diagnostics);
    if (!stage.counters_valid()) return error("identity domain exhausted");
    auto after_value = snapshot_project(session, next);
    if (!after_value) return error("Cannot snapshot completed project mutation");
    auto after = std::make_shared<const ProjectSnapshot>(std::move(*after_value));
    if (before->documents != after->documents || before->profile_ids != after->profile_ids) {
        ++next.revision;
        ProjectHistoryEntry entry{before, after, name, changed_presets(*before, *after)};
        next.redo_entries.clear();
        if (next.history_capacity != 0) {
            next.undo_entries.push_back(std::move(entry));
            if (next.undo_entries.size() > next.history_capacity)
                next.undo_entries.erase(next.undo_entries.begin(),
                                        next.undo_entries.begin() +
                                            static_cast<std::ptrdiff_t>(next.undo_entries.size() -
                                                                        next.history_capacity));
        } else {
            next.undo_entries.clear();
        }
        result["project"] = project_info(next);
    }
    static_cast<void>(after->documents.dump());
    static_cast<void>(result.dump());
    live = std::move(next);
    stage.commit();
    return result;
}

Result<std::uint64_t> next_project_revision(const McpSession& session, std::uint64_t score_id) {
    const auto observed = session.namespace_history->project_revision_floors.find(score_id);
    if (observed == session.namespace_history->project_revision_floors.end()) return 1;
    if (observed->second == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    return observed->second + 1;
}

void retain_channel_namespace(const McpSession& session, ProjectRecord& project) {
    const auto observed = session.namespace_history->graph_channels.find(project.mix_graph_id);
    if (observed != session.namespace_history->graph_channels.end())
        project.reserved_channel_ids.insert(observed->second.begin(), observed->second.end());
}

json bind_project(const McpSession& session,
                  std::uint64_t score_id,
                  std::vector<std::uint64_t> profiles,
                  std::uint64_t mix_id) {
    if (session.project->projects.contains(score_id))
        return error("Score already belongs to a bound project");
    if (!session.score->find(score_id) || !session.mix->find(mix_id))
        return error("Project component not found");
    if (owner_of_mix(session, mix_id)) return error("Mix graph already belongs to a bound project");
    std::set<std::uint64_t> seen;
    for (const auto id : profiles) {
        if (!seen.insert(id).second) return error("Duplicate Timbre profile identity");
        if (!session.timbre->find(id)) return error("Timbre profile not found");
        if (owner_of_profile(session, id))
            return error("Timbre profile already belongs to a bound project");
    }
    ProjectRecord project;
    const auto revision = next_project_revision(session, score_id);
    if (!revision) return error("project version domain exhausted");
    project.revision = *revision;
    project.score_id = score_id;
    project.mix_graph_id = mix_id;
    project.profile_ids = std::move(profiles);
    const auto diagnostics = project_diagnostics(session, project);
    if (has_error(diagnostics)) return invalid_project(diagnostics);
    std::vector<std::uint64_t> ordered;
    for (const auto& part : session.score->find(score_id)->parts) {
        for (const auto id : project.profile_ids)
            if (session.timbre->find(id)->part_id == part.id) ordered.push_back(id);
    }
    for (const auto& channel : session.mix->find(mix_id)->channels)
        project.reserved_channel_ids.insert(channel.id.value);
    retain_channel_namespace(session, project);
    project.profile_ids = std::move(ordered);
    auto response = project_info(project);
    response["success"] = true;
    auto prepared = snapshot_project(session, project);
    if (!prepared) return error("Cannot snapshot project components");
    static_cast<void>(prepared->documents.dump());
    static_cast<void>(response.dump());
    session.project->projects.emplace(score_id, std::move(project));
    // Score-only history cannot be replayed after sibling documents are bound.
    session.score->undo_stacks.erase(score_id);
    return response;
}

} // namespace

Result<ProjectSnapshot> snapshot_project(const McpSession& session, const ProjectRecord& project) {
    const auto* score = session.score->find(project.score_id);
    const auto* mix = session.mix->find(project.mix_graph_id);
    if (!score || !mix) return std::unexpected(ErrorCode::ProjectMissingComponent);
    ProjectSnapshot snapshot;
    snapshot.score_id = project.score_id;
    snapshot.mix_graph_id = project.mix_graph_id;
    snapshot.profile_ids = project.profile_ids;
    snapshot.documents = {{"score", score_to_json(*score)},
                          {"mix", mix_to_json(*mix)},
                          {"timbre_profiles", json::array()},
                          {"preset_library", json::array()}};
    for (const auto id : project.profile_ids) {
        const auto* profile = session.timbre->find(id);
        if (!profile) return std::unexpected(ErrorCode::ProjectMissingComponent);
        snapshot.documents["timbre_profiles"].push_back(timbre_to_json(*profile));
    }
    for (const auto& preset : session.timbre->preset_library)
        snapshot.documents["preset_library"].push_back(timbre_preset_to_json(preset));
    return snapshot;
}

void register_project_authoring_tools(McpServer& server, const McpSession& session) {
    server.set_tool_executor([session](McpDocumentDomain domain,
                                       const std::string& name,
                                       const json& params,
                                       const McpToolHandler& handler) {
        return execute_tool(session, domain, name, params, handler);
    });
    server.register_tool(
        "bind_project",
        "Bind existing Score, Timbre and Mix identities to one authoring history",
        {{"score_id", "integer"}, {"timbre_profile_ids", "array"}, {"mix_graph_id", "integer"}},
        [session](const json& params) {
            std::vector<std::uint64_t> ids;
            for (const auto& encoded : params.at("timbre_profile_ids"))
                ids.push_back(detail::checked_integer<std::uint64_t>(encoded, "profile id"));
            return bind_project(
                session,
                detail::checked_integer<std::uint64_t>(params.at("score_id"), "score id"),
                std::move(ids),
                detail::checked_integer<std::uint64_t>(params.at("mix_graph_id"), "mix graph id"));
        });
    server.register_tool(
        "create_project",
        "Create default Timbre and Mix siblings for an existing Score, then bind them",
        {{"score_id", "integer"}},
        [session](const json& params) {
            const auto id =
                detail::checked_integer<std::uint64_t>(params.at("score_id"), "score id");
            const auto* score = session.score->find(id);
            if (!score) return error("Score not found");
            if (session.project->projects.contains(id))
                return error("Score already belongs to a bound project");
            std::map<std::uint64_t, TimbreProfile> profiles;
            std::vector<std::uint64_t> profile_ids;
            std::vector<PartId> part_ids;
            auto profile_batch = mcp_detail::checked_session_id_batch(
                session.timbre->next_profile_id,
                mcp_detail::maximum_session_store_id(session.timbre->profiles),
                score->parts.size());
            if (!profile_batch)
                return json{{"error", "Timbre identity allocation rejected"},
                            {"error_code", static_cast<int>(profile_batch.error())}};
            auto graph_batch = mcp_detail::checked_session_id_batch(
                session.mix->next_graph_id,
                mcp_detail::maximum_session_store_id(session.mix->graphs));
            if (!graph_batch)
                return json{{"error", "Mix identity allocation rejected"},
                            {"error_code", static_cast<int>(graph_batch.error())}};
            auto next_profile_id = profile_batch->first;
            for (const auto& part : score->parts) {
                profiles.emplace(next_profile_id,
                                 create_timbre_profile(TimbreProfileId{next_profile_id},
                                                       part.id,
                                                       part.definition.name));
                profile_ids.push_back(next_profile_id++);
                part_ids.push_back(part.id);
            }
            const auto graph_id = graph_batch->first;
            std::map<std::uint64_t, MixGraph> graphs;
            graphs.emplace(graph_id, create_mix_graph(MixGraphId{graph_id}, part_ids));
            std::vector<const TimbreProfile*> views;
            for (const auto& [profile_id, profile] : profiles) {
                static_cast<void>(profile_id);
                views.push_back(&profile);
            }
            auto diagnostics = validate_project({*score, views, graphs.begin()->second});
            append_preset_diagnostics(session, views, diagnostics);
            if (has_error(diagnostics)) return invalid_project(diagnostics);
            ProjectRecord project;
            const auto revision = next_project_revision(session, id);
            if (!revision) return error("project version domain exhausted");
            project.revision = *revision;
            project.score_id = id;
            project.mix_graph_id = graph_id;
            project.profile_ids = std::move(profile_ids);
            for (const auto& channel : graphs.begin()->second.channels)
                project.reserved_channel_ids.insert(channel.id.value);
            retain_channel_namespace(session, project);
            // Prepare the binding node and response before inserting any canonical document.
            std::map<std::uint64_t, ProjectRecord> binding;
            auto response = project_info(project);
            response["success"] = true;
            static_cast<void>(score_to_json(*score).dump());
            static_cast<void>(mix_to_json(graphs.begin()->second).dump());
            for (const auto& [profile_id, profile] : profiles) {
                static_cast<void>(profile_id);
                static_cast<void>(timbre_to_json(profile).dump());
            }
            static_cast<void>(response.dump());
            binding.emplace(id, std::move(project));
            session.timbre->profiles.merge(profiles);
            session.mix->graphs.merge(graphs);
            session.project->projects.merge(binding);
            session.timbre->next_profile_id = profile_batch->next;
            session.mix->next_graph_id = graph_batch->next;
            session.score->undo_stacks.erase(id);
            return response;
        });
    server.register_tool("get_project_json",
                         "Inspect the bound project using unchanged child document schemas",
                         {{"score_id", "integer"}},
                         [session](const json& params) {
                             const auto id = detail::checked_integer<std::uint64_t>(
                                 params.at("score_id"), "score id");
                             const auto found = session.project->projects.find(id);
                             if (found == session.project->projects.end())
                                 return error("Bound project not found");
                             auto snapshot = snapshot_project(session, found->second);
                             if (!snapshot) return error("Project component not found");
                             return json{{"success", true},
                                         {"project", project_info(found->second)},
                                         {"documents", std::move(snapshot->documents)}};
                         });
}

} // namespace sunny::infrastructure
