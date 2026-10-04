/** Whole-workspace codec, atomic publication, and checked durable file replacement. */
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/validation.hpp>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/validation.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/workspace_state.hpp>
#include <system_error>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

std::string new_workspace_namespace() {
    std::random_device entropy;
    constexpr char hexadecimal[] = "0123456789abcdef";
    std::string identity;
    identity.reserve(32);
    for (int word = 0; word < 4; ++word) {
        const auto value = static_cast<std::uint32_t>(entropy());
        for (int shift = 28; shift >= 0; shift -= 4)
            identity.push_back(hexadecimal[(value >> shift) & 15U]);
    }
    return identity;
}

namespace {

[[noreturn]] void deny(const std::string& message) {
    throw std::runtime_error(message);
}

struct WorkspaceView {
    const ScoreSession& score;
    const TimbreSession& timbre;
    const MixSession& mix;
    const CorpusSession& corpus;
    const ProjectSession& project;
    std::uint64_t next_plan_id;
    std::uint64_t observed_plan_id;
    const WorkspaceNamespaceHistory& namespace_history;
    const NativeWorkspaceMetadata& native_realization;
};

WorkspaceView view(const WorkspaceState& state) {
    return {state.score,
            state.timbre,
            state.mix,
            state.corpus,
            state.project,
            state.next_plan_id,
            0,
            state.namespace_history,
            state.native_realization};
}
WorkspaceView view(const McpSession& session, const WorkspaceNamespaceHistory& history) {
    return {*session.score,
            *session.timbre,
            *session.mix,
            *session.corpus,
            *session.project,
            session.deployment->next_plan_id,
            session.deployment->plans.empty() ? 0 : session.deployment->plans.rbegin()->first,
            history,
            session.realization->metadata};
}
WorkspaceView view(const McpSession& session) {
    return view(session, *session.namespace_history);
}

void merge_identities(ScoreIdentityReservations& destination,
                      const ScoreIdentityReservations& source) {
    destination.events.insert(source.events.begin(), source.events.end());
    destination.parts.insert(source.parts.begin(), source.parts.end());
    destination.sections.insert(source.sections.begin(), source.sections.end());
    destination.tuplets.insert(source.tuplets.begin(), source.tuplets.end());
    destination.beams.insert(source.beams.begin(), source.beams.end());
}

void merge_history(WorkspaceNamespaceHistory& destination,
                   const WorkspaceNamespaceHistory& source) {
    for (const auto& [id, history] : source.scores) {
        auto& target = destination.scores[id];
        merge_identities(target.identities, history.identities);
        target.version_floor = std::max(target.version_floor, history.version_floor);
    }
    for (const auto& [id, channels] : source.graph_channels)
        destination.graph_channels[id].insert(channels.begin(), channels.end());
    for (const auto& [id, revision] : source.project_revision_floors) {
        auto& target = destination.project_revision_floors[id];
        target = std::max(target, revision);
    }
}

void collect_active_history(WorkspaceNamespaceHistory& history, const WorkspaceView& state) {
    for (const auto& [id, score] : state.score.scores) {
        auto& target = history.scores[id];
        merge_identities(target.identities, collect_score_identities(score));
        target.version_floor = std::max(target.version_floor, score.version);
    }
    for (const auto& [id, graph] : state.mix.graphs) {
        auto& channels = history.graph_channels[id];
        for (const auto& channel : graph.channels)
            channels.insert(channel.id.value);
    }
    for (const auto& [id, project] : state.project.projects) {
        auto& revision = history.project_revision_floors[id];
        revision = std::max(revision, project.revision);
        auto& channels = history.graph_channels[project.mix_graph_id];
        channels.insert(project.reserved_channel_ids.begin(), project.reserved_channel_ids.end());
    }
}

bool includes_identities(const ScoreIdentityReservations& history,
                         const ScoreIdentityReservations& represented) {
    const auto includes = [](const auto& all, const auto& required) {
        return std::includes(all.begin(), all.end(), required.begin(), required.end());
    };
    return includes(history.events, represented.events) &&
           includes(history.parts, represented.parts) &&
           includes(history.sections, represented.sections) &&
           includes(history.tuplets, represented.tuplets) &&
           includes(history.beams, represented.beams);
}

void exact_fields(const json& value,
                  std::initializer_list<const char*> names,
                  const std::string& path) {
    if (!value.is_object()) deny(path + " must be an object");
    std::set<std::string> expected;
    for (const auto* name : names) {
        expected.insert(name);
        if (!value.contains(name)) deny(path + " is missing '" + name + "'");
    }
    for (const auto& [key, field] : value.items()) {
        static_cast<void>(field);
        if (!expected.contains(key)) deny(path + " has unsupported field '" + key + "'");
    }
}

std::uint64_t positive_id(const json& value, const std::string& path) {
    const auto id = detail::checked_integer<std::uint64_t>(value, path);
    if (id == 0) deny(path + " must be positive");
    return id;
}

void validate_diagnostics(const std::vector<Diagnostic>& diagnostics,
                          const std::string& path,
                          bool corpus = false) {
    for (const auto& diagnostic : diagnostics)
        if (corpus ? blocks_corpus_load(diagnostic)
                   : diagnostic.severity == ValidationSeverity::Error)
            deny(path + ": " + diagnostic.rule + ": " + diagnostic.message);
}

void validate_preset(const TimbrePreset& preset, const std::string& path) {
    if (preset.id.value == 0) deny(path + ": preset identity must be positive");
    auto probe =
        create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Preset descriptor validation");
    if (!set_semantic_descriptors(probe, preset.semantic_descriptors))
        deny(path + ": invalid semantic descriptor");
    for (const auto& [parameter, value] : preset.parameter_state)
        if (parameter.empty() || !std::isfinite(value))
            deny(path + ": parameter state requires a nonempty path and finite value");
}

void counter(std::uint64_t next, std::uint64_t observed, const std::string& name) {
    if (next == 0) deny("counters." + name + " must be positive");
    if (observed == std::numeric_limits<std::uint64_t>::max())
        deny("counters." + name + ": identity space exhausted by represented UINT64_MAX");
    if (next <= observed)
        deny("counters." + name + " must exceed every represented allocated identity");
}

void validate(const WorkspaceView& state) {
    const auto& native = state.native_realization;
    if (native.workspace_namespace.size() != 32 ||
        !std::all_of(
            native.workspace_namespace.begin(), native.workspace_namespace.end(), [](char value) {
                return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
            }))
        deny("native_realization.workspace_namespace must be 32 lowercase hexadecimal digits");
    if (native.history_base_directory &&
        (native.history_base_directory->empty() || native.history_base_directory->size() > 4096 ||
         native.history_base_directory->find('\0') != std::string::npos))
        deny("native_realization.history_base_directory must be a nonempty bounded path");
    std::uint64_t score_max = 0, profile_max = 0, timbre_effect_max = 0, preset_max = 0;
    std::uint64_t graph_max = 0, group_max = 0, aux_max = 0, mix_effect_max = 0, reference_max = 0;
    for (const auto& [id, history] : state.namespace_history.scores) {
        if (id == 0 || history.version_floor == 0)
            deny("namespace_history.scores: identities and version floors must be positive");
        score_max = std::max(score_max, id);
        const auto positive = [](const auto& ids) {
            return std::ranges::all_of(ids, [](const auto value) { return value.value != 0; });
        };
        if (!positive(history.identities.events) || !positive(history.identities.parts) ||
            !positive(history.identities.sections) || !positive(history.identities.tuplets) ||
            !positive(history.identities.beams))
            deny("namespace_history.scores: typed reservations must be positive");
    }
    for (const auto& [id, channels] : state.namespace_history.graph_channels) {
        if (id == 0 || channels.contains(0))
            deny("namespace_history.mix_graphs: graph and Channel identities must be positive");
        graph_max = std::max(graph_max, id);
    }
    for (const auto& [id, revision] : state.namespace_history.project_revision_floors) {
        if (id == 0 || revision == 0)
            deny("namespace_history.projects: identities and revision floors must be positive");
        score_max = std::max(score_max, id);
    }
    for (const auto& [id, score] : state.score.scores) {
        if (id == 0 || id != score.id.value)
            deny("scores: store key disagrees with Score identity");
        score_max = std::max(score_max, id);
        const auto history = state.namespace_history.scores.find(id);
        const auto represented = collect_score_identities(score);
        if (history == state.namespace_history.scores.end() ||
            history->second.version_floor != score.version ||
            !includes_identities(history->second.identities, represented) ||
            !includes_identities(represented, history->second.identities))
            deny("namespace_history.scores: missing reservations or inconsistent active version");
        validate_diagnostics(validate_score(score), "scores[" + std::to_string(id) + "]");
    }
    std::set<std::uint64_t> library_ids;
    for (const auto& preset : state.timbre.preset_library) {
        validate_preset(preset, "preset_library[" + std::to_string(preset.id.value) + "]");
        if (!library_ids.insert(preset.id.value).second)
            deny("preset_library: duplicate preset identity");
        preset_max = std::max(preset_max, preset.id.value);
    }
    for (const auto& [id, profile] : state.timbre.profiles) {
        const auto path = "timbre_profiles[" + std::to_string(id) + "]";
        if (id == 0 || id != profile.id.value)
            deny(path + ": store key disagrees with profile identity");
        profile_max = std::max(profile_max, id);
        validate_diagnostics(validate_timbre(profile), path);
        for (const auto& effect : profile.insert_chain.effects)
            timbre_effect_max = std::max(timbre_effect_max, effect.id.value);
        std::set<std::uint64_t> local_ids;
        for (const auto& preset : profile.presets) {
            validate_preset(preset, path + ".presets");
            if (!local_ids.insert(preset.id.value).second)
                deny(path + ": duplicate local preset identity");
        }
    }
    for (const auto& [id, graph] : state.mix.graphs) {
        const auto path = "mix_graphs[" + std::to_string(id) + "]";
        if (id == 0 || id != graph.id.value)
            deny(path + ": store key disagrees with graph identity");
        graph_max = std::max(graph_max, id);
        const auto history = state.namespace_history.graph_channels.find(id);
        if (history == state.namespace_history.graph_channels.end())
            deny("namespace_history.mix_graphs: missing active graph");
        for (const auto& channel : graph.channels)
            if (!history->second.contains(channel.id.value))
                deny("namespace_history.mix_graphs: missing represented Channel identity");
        validate_diagnostics(validate_mix(graph), path);
        const auto observe_chain = [&](const MixEffectChain& chain) {
            for (const auto& effect : chain.effects)
                mix_effect_max = std::max(mix_effect_max, effect.id.value);
        };
        for (const auto& channel : graph.channels)
            observe_chain(channel.insert_chain);
        for (const auto& group : graph.group_buses) {
            group_max = std::max(group_max, group.id.value);
            observe_chain(group.insert_chain);
        }
        for (const auto& aux : graph.aux_buses) {
            aux_max = std::max(aux_max, aux.id.value);
            observe_chain(aux.effect_chain);
        }
        observe_chain(graph.master_bus.insert_chain);
        for (const auto& reference : graph.reference_profiles)
            reference_max = std::max(reference_max, reference.id.value);
    }
    validate_diagnostics(validate_corpus(state.corpus.corpus), "corpus", true);
    std::uint64_t composer_max = 0, work_max = 0;
    for (const auto& [id, composer] : state.corpus.corpus.composers) {
        if (id == 0 || id != composer.id.value)
            deny("corpus.composers: store key disagrees with identity");
        composer_max = std::max(composer_max, id);
    }
    for (const auto& [id, work] : state.corpus.corpus.works) {
        if (id == 0 || id != work.id.value) deny("corpus.works: store key disagrees with identity");
        work_max = std::max(work_max, id);
    }
    std::set<std::uint64_t> owned_profiles, owned_graphs;
    for (const auto& [id, project] : state.project.projects) {
        const auto path = "projects[" + std::to_string(id) + "]";
        if (id == 0 || project.score_id != id)
            deny(path + ": store key disagrees with Score binding");
        if (project.revision == 0) deny(path + ": revision must be positive");
        const auto revision = state.namespace_history.project_revision_floors.find(id);
        if (revision == state.namespace_history.project_revision_floors.end() ||
            revision->second != project.revision)
            deny("namespace_history.projects: missing or inconsistent active revision");
        const auto score = state.score.scores.find(id);
        const auto graph = state.mix.graphs.find(project.mix_graph_id);
        if (score == state.score.scores.end() || graph == state.mix.graphs.end())
            deny(path + ": missing component");
        if (!owned_graphs.insert(project.mix_graph_id).second)
            deny(path + ": Mix graph has more than one owner");
        std::vector<const TimbreProfile*> profiles;
        for (const auto profile_id : project.profile_ids) {
            const auto profile = state.timbre.profiles.find(profile_id);
            if (profile == state.timbre.profiles.end()) deny(path + ": missing Timbre profile");
            if (!owned_profiles.insert(profile_id).second)
                deny(path + ": Timbre profile has more than one owner");
            profiles.push_back(&profile->second);
            auto available = library_ids;
            for (const auto& preset : profile->second.presets)
                available.insert(preset.id.value);
            for (const auto& morph : profile->second.preset_morphs)
                if (!available.contains(morph.from_preset.value) ||
                    !available.contains(morph.to_preset.value))
                    deny(path + ": P3: preset morph references missing preset identity");
        }
        if (project.reserved_channel_ids.contains(0))
            deny(path + ": Channel reservations must be positive");
        for (const auto& channel : graph->second.channels)
            if (!project.reserved_channel_ids.contains(channel.id.value))
                deny(path + ": Channel reservations omit a represented identity");
        const auto& channels = state.namespace_history.graph_channels.at(project.mix_graph_id);
        if (!std::includes(channels.begin(),
                           channels.end(),
                           project.reserved_channel_ids.begin(),
                           project.reserved_channel_ids.end()) ||
            !std::includes(project.reserved_channel_ids.begin(),
                           project.reserved_channel_ids.end(),
                           channels.begin(),
                           channels.end()))
            deny("namespace_history.mix_graphs: missing retired project Channel reservation");
        validate_diagnostics(validate_project({score->second, profiles, graph->second}), path);
    }
    counter(state.score.next_score_id, score_max, "next_score_id");
    counter(state.timbre.next_profile_id, profile_max, "next_profile_id");
    counter(state.timbre.next_effect_id, timbre_effect_max, "next_timbre_effect_id");
    counter(state.timbre.next_preset_id, preset_max, "next_preset_id");
    counter(state.mix.next_graph_id, graph_max, "next_graph_id");
    counter(state.mix.next_group_id, group_max, "next_group_id");
    counter(state.mix.next_aux_id, aux_max, "next_aux_id");
    counter(state.mix.next_effect_id, mix_effect_max, "next_mix_effect_id");
    counter(state.mix.next_ref_id, reference_max, "next_ref_id");
    counter(state.corpus.next_composer_id, composer_max, "next_composer_id");
    counter(state.corpus.next_work_id, work_max, "next_work_id");
    counter(state.next_plan_id, state.observed_plan_id, "next_plan_id");
}

json encode_history(const WorkspaceNamespaceHistory& history) {
    json encoded = {
        {"scores", json::array()}, {"mix_graphs", json::array()}, {"projects", json::array()}};
    const auto ids = [](const auto& values) {
        json result = json::array();
        for (const auto value : values)
            result.push_back(value.value);
        return result;
    };
    for (const auto& [id, record] : history.scores)
        encoded["scores"].push_back({{"score_id", id},
                                     {"version_floor", record.version_floor},
                                     {"identities",
                                      {{"events", ids(record.identities.events)},
                                       {"parts", ids(record.identities.parts)},
                                       {"sections", ids(record.identities.sections)},
                                       {"tuplets", ids(record.identities.tuplets)},
                                       {"beams", ids(record.identities.beams)}}}});
    for (const auto& [id, channels] : history.graph_channels)
        encoded["mix_graphs"].push_back({{"mix_graph_id", id}, {"channel_ids", channels}});
    for (const auto& [id, revision] : history.project_revision_floors)
        encoded["projects"].push_back({{"score_id", id}, {"revision_floor", revision}});
    return encoded;
}

json encode(const WorkspaceView& state) {
    json document = {{"format", "sunny-workspace"},
                     {"version", WORKSPACE_SCHEMA_VERSION},
                     {"scores", json::array()},
                     {"timbre_profiles", json::array()},
                     {"mix_graphs", json::array()},
                     {"preset_library", json::array()},
                     {"corpus", corpus_to_json(state.corpus.corpus)},
                     {"projects", json::array()},
                     {"namespace_history", encode_history(state.namespace_history)},
                     {"native_realization",
                      {{"workspace_namespace", state.native_realization.workspace_namespace},
                       {"history_base_directory",
                        state.native_realization.history_base_directory
                            ? json(*state.native_realization.history_base_directory)
                            : json(nullptr)}}},
                     {"counters",
                      {{"next_score_id", state.score.next_score_id},
                       {"next_profile_id", state.timbre.next_profile_id},
                       {"next_timbre_effect_id", state.timbre.next_effect_id},
                       {"next_preset_id", state.timbre.next_preset_id},
                       {"next_graph_id", state.mix.next_graph_id},
                       {"next_group_id", state.mix.next_group_id},
                       {"next_aux_id", state.mix.next_aux_id},
                       {"next_mix_effect_id", state.mix.next_effect_id},
                       {"next_ref_id", state.mix.next_ref_id},
                       {"next_composer_id", state.corpus.next_composer_id},
                       {"next_work_id", state.corpus.next_work_id},
                       {"next_plan_id", state.next_plan_id}}}};
    for (const auto& [id, score] : state.score.scores)
        document["scores"].push_back({{"id", id}, {"document", score_to_json(score)}});
    for (const auto& [id, profile] : state.timbre.profiles)
        document["timbre_profiles"].push_back({{"id", id}, {"document", timbre_to_json(profile)}});
    for (const auto& [id, graph] : state.mix.graphs)
        document["mix_graphs"].push_back({{"id", id}, {"document", mix_to_json(graph)}});
    for (const auto& preset : state.timbre.preset_library)
        document["preset_library"].push_back(timbre_preset_to_json(preset));
    for (const auto& [id, project] : state.project.projects)
        document["projects"].push_back({{"score_id", id},
                                        {"mix_graph_id", project.mix_graph_id},
                                        {"timbre_profile_ids", project.profile_ids},
                                        {"revision", project.revision},
                                        {"history_capacity", project.history_capacity},
                                        {"reserved_channel_ids", project.reserved_channel_ids}});
    return document;
}

template <class Document> Document decoded(Result<Document> result, const std::string& path) {
    if (!result)
        deny(path + ": child codec rejected document (error " +
             std::to_string(static_cast<int>(result.error())) + ")");
    return std::move(*result);
}

WorkspaceNamespaceHistory decode_history(const json& encoded) {
    exact_fields(encoded, {"scores", "mix_graphs", "projects"}, "namespace_history");
    WorkspaceNamespaceHistory history;
    const auto array = [&](const char* name) -> const json& {
        const auto& result = encoded.at(name);
        if (!result.is_array())
            deny(std::string("namespace_history.") + name + " must be an array");
        return result;
    };
    const auto read_ids = [](const json& encoded_ids, auto& destination, const std::string& path) {
        if (!encoded_ids.is_array()) deny(path + " must be an array");
        std::uint64_t previous = 0;
        for (const auto& encoded_id : encoded_ids) {
            const auto id = positive_id(encoded_id, path);
            if (id <= previous) deny(path + " must be sorted and unique");
            using Id = typename std::remove_reference_t<decltype(destination)>::value_type;
            destination.insert(Id{id});
            previous = id;
        }
    };
    std::uint64_t previous = 0;
    for (const auto& record : array("scores")) {
        exact_fields(
            record, {"score_id", "version_floor", "identities"}, "namespace_history.scores");
        const auto id = positive_id(record.at("score_id"), "namespace_history.scores.score_id");
        if (id <= previous) deny("namespace_history.scores must be sorted and unique");
        previous = id;
        ScoreNamespaceHistory item;
        item.version_floor =
            positive_id(record.at("version_floor"), "namespace_history.scores.version_floor");
        const auto& identities = record.at("identities");
        exact_fields(identities,
                     {"events", "parts", "sections", "tuplets", "beams"},
                     "namespace_history.scores.identities");
        read_ids(identities.at("events"), item.identities.events, "namespace_history.events");
        read_ids(identities.at("parts"), item.identities.parts, "namespace_history.parts");
        read_ids(identities.at("sections"), item.identities.sections, "namespace_history.sections");
        read_ids(identities.at("tuplets"), item.identities.tuplets, "namespace_history.tuplets");
        read_ids(identities.at("beams"), item.identities.beams, "namespace_history.beams");
        history.scores.emplace(id, std::move(item));
    }
    previous = 0;
    for (const auto& record : array("mix_graphs")) {
        exact_fields(record, {"mix_graph_id", "channel_ids"}, "namespace_history.mix_graphs");
        const auto id =
            positive_id(record.at("mix_graph_id"), "namespace_history.mix_graphs.mix_graph_id");
        if (id <= previous) deny("namespace_history.mix_graphs must be sorted and unique");
        previous = id;
        const auto& channels = record.at("channel_ids");
        if (!channels.is_array()) deny("namespace_history.channel_ids must be an array");
        std::uint64_t previous_channel = 0;
        for (const auto& encoded_id : channels) {
            const auto channel = positive_id(encoded_id, "namespace_history.channel_ids");
            if (channel <= previous_channel)
                deny("namespace_history.channel_ids must be sorted and unique");
            history.graph_channels[id].insert(channel);
            previous_channel = channel;
        }
        history.graph_channels.try_emplace(id);
    }
    previous = 0;
    for (const auto& record : array("projects")) {
        exact_fields(record, {"score_id", "revision_floor"}, "namespace_history.projects");
        const auto id = positive_id(record.at("score_id"), "namespace_history.projects.score_id");
        if (id <= previous) deny("namespace_history.projects must be sorted and unique");
        previous = id;
        history.project_revision_floors.emplace(
            id,
            positive_id(record.at("revision_floor"), "namespace_history.projects.revision_floor"));
    }
    return history;
}

WorkspaceState decode(const json& document) {
    const auto version = detail::checked_integer<int>(document.at("version"), "workspace version");
    if (version != 1 && version != WORKSPACE_SCHEMA_VERSION)
        deny("workspace: unsupported envelope version");
    auto canonical = document;
    if (version == 1) {
        if (canonical.contains("native_realization"))
            deny("workspace version 1 cannot contain native realization metadata");
        canonical["native_realization"] = {{"workspace_namespace", new_workspace_namespace()},
                                           {"history_base_directory", nullptr}};
    }
    exact_fields(canonical,
                 {"format",
                  "version",
                  "scores",
                  "timbre_profiles",
                  "mix_graphs",
                  "preset_library",
                  "corpus",
                  "projects",
                  "namespace_history",
                  "native_realization",
                  "counters"},
                 "workspace");
    if (document.at("format") != "sunny-workspace") deny("workspace: unsupported format");
    WorkspaceState state;
    const auto& native = canonical.at("native_realization");
    exact_fields(native, {"workspace_namespace", "history_base_directory"}, "native_realization");
    state.native_realization.workspace_namespace =
        native.at("workspace_namespace").get<std::string>();
    if (!native.at("history_base_directory").is_null())
        state.native_realization.history_base_directory =
            native.at("history_base_directory").get<std::string>();
    state.native_namespace_is_new = version == 1;
    state.namespace_history = decode_history(document.at("namespace_history"));
    const auto read_documents = [&](const char* key, auto& destination, auto reader) {
        const auto& records = document.at(key);
        if (!records.is_array()) deny(std::string(key) + " must be an array");
        for (const auto& record : records) {
            exact_fields(record, {"id", "document"}, key);
            const auto id = positive_id(record.at("id"), std::string(key) + ".id");
            auto child = decoded(reader(record.at("document")),
                                 std::string(key) + "[" + std::to_string(id) + "]");
            if (!destination.emplace(id, std::move(child)).second)
                deny(std::string(key) + ": duplicate store identity");
        }
    };
    read_documents("scores", state.score.scores, score_from_json);
    read_documents("timbre_profiles", state.timbre.profiles, timbre_from_json);
    read_documents("mix_graphs", state.mix.graphs, mix_from_json);
    if (!document.at("preset_library").is_array()) deny("preset_library must be an array");
    for (const auto& preset : document.at("preset_library"))
        state.timbre.preset_library.push_back(
            decoded(timbre_preset_from_json(preset), "preset_library"));
    state.corpus.corpus = decoded(corpus_from_json(document.at("corpus")), "corpus");
    if (!document.at("projects").is_array()) deny("projects must be an array");
    for (const auto& record : document.at("projects")) {
        exact_fields(record,
                     {"score_id",
                      "mix_graph_id",
                      "timbre_profile_ids",
                      "revision",
                      "history_capacity",
                      "reserved_channel_ids"},
                     "projects");
        ProjectRecord project;
        project.score_id = positive_id(record.at("score_id"), "projects.score_id");
        project.mix_graph_id = positive_id(record.at("mix_graph_id"), "projects.mix_graph_id");
        project.revision = positive_id(record.at("revision"), "projects.revision");
        project.history_capacity = detail::checked_integer<std::size_t>(
            record.at("history_capacity"), "projects.history_capacity");
        if (!record.at("timbre_profile_ids").is_array() ||
            !record.at("reserved_channel_ids").is_array())
            deny("projects: identities and reservations must be arrays");
        for (const auto& id : record.at("timbre_profile_ids"))
            project.profile_ids.push_back(positive_id(id, "projects.timbre_profile_ids"));
        std::uint64_t previous = 0;
        for (const auto& item : record.at("reserved_channel_ids")) {
            const auto id = positive_id(item, "projects.reserved_channel_ids");
            if (id <= previous) deny("projects: Channel reservations must be sorted and unique");
            project.reserved_channel_ids.insert(id);
            previous = id;
        }
        const auto id = project.score_id;
        if (!state.project.projects.emplace(id, std::move(project)).second)
            deny("projects: duplicate Score binding");
    }
    const auto& counters = document.at("counters");
    exact_fields(counters,
                 {"next_score_id",
                  "next_profile_id",
                  "next_timbre_effect_id",
                  "next_preset_id",
                  "next_graph_id",
                  "next_group_id",
                  "next_aux_id",
                  "next_mix_effect_id",
                  "next_ref_id",
                  "next_composer_id",
                  "next_work_id",
                  "next_plan_id"},
                 "counters");
    const auto next = [&](const char* name) {
        return positive_id(counters.at(name), std::string("counters.") + name);
    };
    state.score.next_score_id = next("next_score_id");
    state.timbre.next_profile_id = next("next_profile_id");
    state.timbre.next_effect_id = next("next_timbre_effect_id");
    state.timbre.next_preset_id = next("next_preset_id");
    state.mix.next_graph_id = next("next_graph_id");
    state.mix.next_group_id = next("next_group_id");
    state.mix.next_aux_id = next("next_aux_id");
    state.mix.next_effect_id = next("next_mix_effect_id");
    state.mix.next_ref_id = next("next_ref_id");
    state.corpus.next_composer_id = next("next_composer_id");
    state.corpus.next_work_id = next("next_work_id");
    state.next_plan_id = next("next_plan_id");
    // The outer history is explicit evidence even when a supported legacy child
    // schema cannot represent retired identities. Require coverage before union.
    for (auto& [id, score] : state.score.scores) {
        const auto history = state.namespace_history.scores.find(id);
        if (history == state.namespace_history.scores.end()) continue;
        if (!includes_identities(history->second.identities, collect_score_identities(score)))
            deny("namespace_history.scores: reservations omit represented typed identities");
        retain_score_identities(score, history->second.identities);
    }
    for (auto& [id, project] : state.project.projects) {
        static_cast<void>(id);
        const auto channels = state.namespace_history.graph_channels.find(project.mix_graph_id);
        if (channels == state.namespace_history.graph_channels.end()) continue;
        if (!std::includes(channels->second.begin(),
                           channels->second.end(),
                           project.reserved_channel_ids.begin(),
                           project.reserved_channel_ids.end()))
            deny("namespace_history.mix_graphs: reservations omit retired project Channels");
        const auto graph = state.mix.graphs.find(project.mix_graph_id);
        if (graph != state.mix.graphs.end())
            for (const auto& channel : graph->second.channels)
                if (!project.reserved_channel_ids.contains(channel.id.value))
                    deny("projects: Channel reservations omit a represented identity");
        project.reserved_channel_ids.insert(channels->second.begin(), channels->second.end());
    }
    validate(view(state));
    return state;
}

json summary(const WorkspaceState& state) {
    std::vector<std::uint64_t> scores;
    for (const auto& [id, score] : state.score.scores) {
        static_cast<void>(score);
        scores.push_back(id);
    }
    return {{"schema_version", WORKSPACE_SCHEMA_VERSION},
            {"score_ids", scores},
            {"project_count", state.project.projects.size()},
            {"timbre_profile_count", state.timbre.profiles.size()},
            {"mix_graph_count", state.mix.graphs.size()},
            {"preset_count", state.timbre.preset_library.size()},
            {"composer_count", state.corpus.corpus.composers.size()},
            {"work_count", state.corpus.corpus.works.size()}};
}

void preserve_counters(WorkspaceState& state, const WorkspaceView& current) noexcept {
    state.score.next_score_id = std::max(state.score.next_score_id, current.score.next_score_id);
    state.timbre.next_profile_id =
        std::max(state.timbre.next_profile_id, current.timbre.next_profile_id);
    state.timbre.next_effect_id =
        std::max(state.timbre.next_effect_id, current.timbre.next_effect_id);
    state.timbre.next_preset_id =
        std::max(state.timbre.next_preset_id, current.timbre.next_preset_id);
    state.mix.next_graph_id = std::max(state.mix.next_graph_id, current.mix.next_graph_id);
    state.mix.next_group_id = std::max(state.mix.next_group_id, current.mix.next_group_id);
    state.mix.next_aux_id = std::max(state.mix.next_aux_id, current.mix.next_aux_id);
    state.mix.next_effect_id = std::max(state.mix.next_effect_id, current.mix.next_effect_id);
    state.mix.next_ref_id = std::max(state.mix.next_ref_id, current.mix.next_ref_id);
    state.corpus.next_composer_id =
        std::max(state.corpus.next_composer_id, current.corpus.next_composer_id);
    state.corpus.next_work_id = std::max(state.corpus.next_work_id, current.corpus.next_work_id);
    state.next_plan_id = std::max(state.next_plan_id, current.next_plan_id);
}

void retain_observed_local_identities(WorkspaceState& state,
                                      const McpSession& session,
                                      const std::set<std::uint64_t>* published_scores = nullptr,
                                      const std::set<std::uint64_t>* published_projects = nullptr) {
    auto current = *session.namespace_history;
    collect_active_history(current, view(session));
    merge_history(state.namespace_history, current);
    std::set<std::uint64_t> changed_scores;
    for (auto& [id, score] : state.score.scores) {
        auto& history = state.namespace_history.scores.at(id);
        const auto previous = collect_score_identities(score);
        const bool reservations_changed = !includes_identities(previous, history.identities);
        retain_score_identities(score, history.identities);
        const bool republished =
            (!published_scores || published_scores->contains(id)) && current.scores.contains(id);
        if (republished || reservations_changed || history.version_floor > score.version) {
            const auto version = std::max(score.version, history.version_floor);
            if (version == std::numeric_limits<std::uint64_t>::max())
                deny("Same-ID Score publication version exhausted");
            score.version = version + 1;
            changed_scores.insert(id);
        }
    }
    for (auto& [id, project] : state.project.projects) {
        const auto& channels = state.namespace_history.graph_channels.at(project.mix_graph_id);
        const bool channels_changed = !std::includes(project.reserved_channel_ids.begin(),
                                                     project.reserved_channel_ids.end(),
                                                     channels.begin(),
                                                     channels.end());
        project.reserved_channel_ids.insert(channels.begin(), channels.end());
        const auto floor = state.namespace_history.project_revision_floors.at(id);
        const bool republished = (!published_projects || published_projects->contains(id)) &&
                                 current.project_revision_floors.contains(id);
        if (republished || changed_scores.contains(id) || channels_changed ||
            floor > project.revision) {
            const auto revision = std::max(project.revision, floor);
            if (revision == std::numeric_limits<std::uint64_t>::max())
                deny("Same-ID project publication revision exhausted");
            project.revision = revision + 1;
        }
    }
    collect_active_history(state.namespace_history, view(state));
    validate(view(state));
}

void publish(const McpSession& session, WorkspaceState& state) noexcept {
    const bool same_native_namespace = session.realization->metadata.workspace_namespace ==
                                       state.native_realization.workspace_namespace;
    session.realization->metadata.workspace_namespace.swap(
        state.native_realization.workspace_namespace);
    session.realization->metadata.history_base_directory.swap(
        state.native_realization.history_base_directory);
    if (!same_native_namespace) {
        session.realization->namespace_is_new = state.native_namespace_is_new;
        session.realization->namespace_saved_durably = !state.native_namespace_is_new;
        session.realization->store.reset();
        session.realization->history_error.reset();
    }
    session.score->scores.swap(state.score.scores);
    session.score->undo_stacks.swap(state.score.undo_stacks);
    session.score->project_transaction_score.reset();
    session.timbre->profiles.swap(state.timbre.profiles);
    session.timbre->preset_library.swap(state.timbre.preset_library);
    session.mix->graphs.swap(state.mix.graphs);
    session.corpus->corpus.composers.swap(state.corpus.corpus.composers);
    session.corpus->corpus.works.swap(state.corpus.corpus.works);
    session.project->projects.swap(state.project.projects);
    session.namespace_history->scores.swap(state.namespace_history.scores);
    session.namespace_history->graph_channels.swap(state.namespace_history.graph_channels);
    session.namespace_history->project_revision_floors.swap(
        state.namespace_history.project_revision_floors);
    session.score->next_score_id = state.score.next_score_id;
    session.timbre->next_profile_id = state.timbre.next_profile_id;
    session.timbre->next_effect_id = state.timbre.next_effect_id;
    session.timbre->next_preset_id = state.timbre.next_preset_id;
    session.mix->next_graph_id = state.mix.next_graph_id;
    session.mix->next_group_id = state.mix.next_group_id;
    session.mix->next_aux_id = state.mix.next_aux_id;
    session.mix->next_effect_id = state.mix.next_effect_id;
    session.mix->next_ref_id = state.mix.next_ref_id;
    session.corpus->next_composer_id = state.corpus.next_composer_id;
    session.corpus->next_work_id = state.corpus.next_work_id;
    session.deployment->plans.clear();
    session.deployment->next_plan_id = state.next_plan_id;
}

WorkspaceResult<std::string> read_bytes(const std::filesystem::path& path) {
    if (path.empty() || path.native().find(std::filesystem::path::value_type{}) !=
                            std::filesystem::path::string_type::npos)
        return std::unexpected(
            WorkspaceError{"Workspace path must be nonempty and contain no NUL"});
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::unexpected(WorkspaceError{"Cannot open workspace file: " + path.string()});
    std::string bytes;
    std::array<char, 16384> buffer;
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        bytes.append(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }
    if (!file.eof() || file.bad())
        return std::unexpected(WorkspaceError{"Failed reading workspace file: " + path.string()});
    return bytes;
}

json parse_bytes(const std::string& bytes) {
    std::vector<std::set<std::string>> keys;
    return json::parse(bytes, [&](int, json::parse_event_t event, json& value) {
        if (event == json::parse_event_t::object_start)
            keys.emplace_back();
        else if (event == json::parse_event_t::key &&
                 !keys.back().insert(value.get<std::string>()).second)
            deny("Workspace JSON has duplicate field '" + value.get<std::string>() + "'");
        else if (event == json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    });
}

struct FileReplacement {
    bool committed = false;
    bool durable = false;
    std::string error;
};

std::string system_failure(const std::string& operation) {
#ifdef _WIN32
    const auto code = static_cast<int>(GetLastError());
    return operation + ": " + std::system_category().message(code);
#else
    const auto code = errno;
    return operation + ": " + std::generic_category().message(code);
#endif
}

class TemporaryFile {
  public:
    std::filesystem::path path;
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
    ~TemporaryFile() {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if (descriptor != -1) close(descriptor);
#endif
        if (!path.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
};

std::optional<std::string>
confirm_loaded_native_namespace(const std::filesystem::path& path,
                                const NativeWorkspaceMetadata& expected) {
#ifdef _WIN32
    static_cast<void>(path);
    static_cast<void>(expected);
    return "Native namespace directory durability requires the primary POSIX Docker server";
#else
    TemporaryFile file, directory;
    file.descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (file.descriptor == -1) return system_failure("Cannot open native namespace workspace");
    struct stat held {};
    if (fstat(file.descriptor, &held) != 0)
        return system_failure("Cannot inspect native namespace workspace");
    if (!S_ISREG(held.st_mode)) return "Native namespace workspace must be a regular file";
    auto parent = path.parent_path();
    if (parent.empty()) parent = ".";
    directory.descriptor = open(parent.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (directory.descriptor == -1)
        return system_failure("Cannot open native namespace workspace directory");
    const auto namespace_matches = [&]() {
        if (lseek(file.descriptor, 0, SEEK_SET) == -1) return false;
        std::string bytes;
        std::array<char, 16384> buffer{};
        while (true) {
            const auto count = read(file.descriptor, buffer.data(), buffer.size());
            if (count == -1 && errno == EINTR) continue;
            if (count < 0) return false;
            if (count == 0) break;
            bytes.append(buffer.data(), static_cast<std::size_t>(count));
        }
        try {
            const auto document = parse_bytes(bytes);
            if (document.at("format") != "sunny-workspace" ||
                document.at("version") != WORKSPACE_SCHEMA_VERSION)
                return false;
            const auto& native = document.at("native_realization");
            if (!native.is_object() || native.size() != 2 ||
                native.at("workspace_namespace") != expected.workspace_namespace)
                return false;
            const auto& base = native.at("history_base_directory");
            // A same-namespace older snapshot may omit the operational location;
            // the already retained current location is preserved by publication.
            return base.is_null() ||
                   (expected.history_base_directory && base == *expected.history_base_directory);
        } catch (const std::exception&) {
            return false;
        }
    };
    if (!namespace_matches()) return "Native workspace namespace changed after parsing";
    const auto synchronize = [](int descriptor) {
        int result;
        do {
            result = fsync(descriptor);
        } while (result == -1 && errno == EINTR);
        return result == 0;
    };
    if (!synchronize(file.descriptor))
        return system_failure("Native namespace workspace synchronization failed");
    if (!synchronize(directory.descriptor))
        return system_failure("Native namespace workspace directory synchronization failed");
    struct stat named {};
    if (fstatat(directory.descriptor, path.filename().c_str(), &named, AT_SYMLINK_NOFOLLOW) != 0)
        return system_failure("Cannot recheck native namespace workspace");
    if (!S_ISREG(named.st_mode) || named.st_dev != held.st_dev || named.st_ino != held.st_ino)
        return "Native namespace workspace changed during synchronization";
    if (!namespace_matches()) return "Native workspace namespace changed during synchronization";
    return std::nullopt;
#endif
}

FileReplacement replace_file(const std::filesystem::path& destination,
                             const std::string& bytes,
                             WorkspaceFileRole role,
                             const WorkspaceIoFault& fault) {
    FileReplacement result;
    try {
        const auto injected = [&](WorkspaceIoPhase phase) {
            if (!fault || !fault(role, phase)) return false;
            result.error = "Injected workspace I/O failure at phase " +
                           std::to_string(static_cast<int>(phase));
            return true;
        };
        if (destination.empty() || destination.filename().empty() ||
            destination.native().find(std::filesystem::path::value_type{}) !=
                std::filesystem::path::string_type::npos) {
            result.error = "Workspace destination must name a file";
            return result;
        }
        auto directory = destination.parent_path();
        if (directory.empty()) directory = ".";
        if (injected(WorkspaceIoPhase::CreateTemporary)) return result;
        TemporaryFile temporary;
        static std::atomic<std::uint64_t> sequence{0};
#ifdef _WIN32
        const auto process_id = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
        const auto process_id = static_cast<std::uint64_t>(getpid());
#endif
        bool created = false;
        for (int attempt = 0; attempt < 128 && !created; ++attempt) {
            temporary.path = directory / ("." + destination.filename().string() + ".sunny-tmp-" +
                                          std::to_string(process_id) + "-" +
                                          std::to_string(sequence.fetch_add(1)));
#ifdef _WIN32
            temporary.handle = CreateFileW(temporary.path.c_str(),
                                           GENERIC_WRITE,
                                           0,
                                           nullptr,
                                           CREATE_NEW,
                                           FILE_ATTRIBUTE_NORMAL,
                                           nullptr);
            created = temporary.handle != INVALID_HANDLE_VALUE;
            if (!created && GetLastError() != ERROR_FILE_EXISTS &&
                GetLastError() != ERROR_ALREADY_EXISTS) {
#else
            temporary.descriptor =
                open(temporary.path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            created = temporary.descriptor != -1;
            if (!created && errno != EEXIST) {
#endif
                result.error = system_failure("Cannot create exclusive workspace temporary file");
                // An uncreated path may belong to another writer; never remove it.
                temporary.path.clear();
                return result;
            }
            if (!created) temporary.path.clear();
        }
        if (!created) {
            result.error = "Exclusive workspace temporary-name retries exhausted";
            return result;
        }
        const auto write_range = [&](std::size_t end, std::size_t& offset) {
            while (offset < end) {
                const auto count = std::min<std::size_t>(end - offset, 65536);
#ifdef _WIN32
                DWORD written = 0;
                if (!WriteFile(temporary.handle,
                               bytes.data() + offset,
                               static_cast<DWORD>(count),
                               &written,
                               nullptr)) {
                    result.error = system_failure("Workspace write failed");
                    return false;
                }
                const auto consumed = static_cast<std::size_t>(written);
#else
                const auto written = write(temporary.descriptor, bytes.data() + offset, count);
                if (written < 0 && errno == EINTR) continue;
                if (written < 0) {
                    result.error = system_failure("Workspace write failed");
                    return false;
                }
                const auto consumed = static_cast<std::size_t>(written);
#endif
                if (consumed == 0) {
                    result.error = "Workspace write made no progress";
                    return false;
                }
                offset += consumed;
            }
            return true;
        };
        std::size_t offset = 0;
        if (!write_range((bytes.size() + 1) / 2, offset) ||
            injected(WorkspaceIoPhase::AfterPartialWrite) || !write_range(bytes.size(), offset))
            return result;
        if (injected(WorkspaceIoPhase::FileSync)) return result;
#ifdef _WIN32
        if (!FlushFileBuffers(temporary.handle)) {
            result.error = system_failure("Workspace file synchronization failed");
            return result;
        }
        const auto handle = temporary.handle;
        temporary.handle = INVALID_HANDLE_VALUE;
        if (!CloseHandle(handle)) {
#else
        int synced;
        do {
            synced = fsync(temporary.descriptor);
        } while (synced == -1 && errno == EINTR);
        if (synced != 0) {
            result.error = system_failure("Workspace file synchronization failed");
            return result;
        }
        const auto descriptor = temporary.descriptor;
        temporary.descriptor = -1;
        if (close(descriptor) != 0) {
#endif
            result.error = system_failure("Workspace temporary close failed");
            return result;
        }
        if (injected(WorkspaceIoPhase::Replace)) return result;
#ifdef _WIN32
        if (!MoveFileExW(temporary.path.c_str(),
                         destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
        if (rename(temporary.path.c_str(), destination.c_str()) != 0) {
#endif
            result.error = system_failure("Atomic workspace replacement failed");
            return result;
        }
        temporary.path.clear();
        result.committed = true;
        if (injected(WorkspaceIoPhase::DirectorySync)) return result;
#ifdef _WIN32
            // MoveFileEx write-through does not expose the POSIX directory fsync proof.
            // The caller reports durability_confirmed=false, distinct from an I/O error.
#else
        int flags = O_RDONLY | O_CLOEXEC;
#ifdef O_DIRECTORY
        flags |= O_DIRECTORY;
#endif
        const auto directory_descriptor = open(directory.c_str(), flags);
        if (directory_descriptor == -1) {
            result.error = system_failure("Workspace directory open failed after replacement");
            return result;
        }
        do {
            synced = fsync(directory_descriptor);
        } while (synced == -1 && errno == EINTR);
        const auto sync_errno = errno;
        const auto closed = close(directory_descriptor);
        if (synced != 0) {
            result.error = "Workspace directory synchronization failed after replacement: " +
                           std::generic_category().message(sync_errno);
            return result;
        }
        if (closed != 0) {
            result.error = system_failure("Workspace directory close failed after synchronization");
            return result;
        }
        result.durable = true;
#endif
        return result;
    } catch (const std::exception& exception) {
        result.error = exception.what();
        return result;
    }
}

} // namespace

WorkspaceResult<json> workspace_to_json(const McpSession& session) {
    try {
        auto history = *session.namespace_history;
        collect_active_history(history, view(session));
        const auto prepared = view(session, history);
        validate(prepared);
        auto document = encode(prepared);
        // Verify the real textual round trip; never silently publish codec loss.
        auto restored = decode(parse_bytes(document.dump()));
        if (encode(view(restored)) != document) deny("Workspace child codec is not lossless");
        return document;
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{exception.what()});
    }
}

WorkspaceResult<WorkspaceState> workspace_from_json(const json& document) {
    try {
        return decode(document);
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{exception.what()});
    }
}

WorkspaceResult<WorkspaceState> read_workspace(const std::filesystem::path& path) {
    auto bytes = read_bytes(path);
    if (!bytes) return std::unexpected(bytes.error());
    try {
        return decode(parse_bytes(*bytes));
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{path.string() + ": " + exception.what()});
    }
}

WorkspaceResult<json> open_workspace(const McpSession& session, const std::filesystem::path& path) {
    try {
        auto state = read_workspace(path);
        if (!state) return std::unexpected(state.error());
        std::optional<std::string> source_path = std::filesystem::absolute(path).string();
        if (state->native_realization.workspace_namespace ==
                session.realization->metadata.workspace_namespace &&
            session.realization->metadata.history_base_directory) {
            if (state->native_realization.history_base_directory &&
                state->native_realization.history_base_directory !=
                    session.realization->metadata.history_base_directory)
                deny("Native namespace has conflicting history locations");
            state->native_realization.history_base_directory =
                session.realization->metadata.history_base_directory;
        }
        preserve_counters(*state, view(session));
        retain_observed_local_identities(*state, session);
        auto response = summary(*state);
        response["success"] = true;
        response["operation"] = "replace";
        response["source"] = path.string();
        auto native_error =
            state->native_namespace_is_new
                ? std::optional<std::string>{"Save the migrated native namespace first"}
                : confirm_loaded_native_namespace(path, state->native_realization);
        response["native_namespace_durability_confirmed"] = !native_error;
        if (native_error) response["native_history_error"] = *native_error;
        static_cast<void>(response.dump());
        publish(session, *state);
        session.realization->namespace_saved_durably = !native_error;
        if (native_error) session.realization->history_error.swap(native_error);
        session.realization->workspace_path.swap(source_path);
        return response;
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{exception.what()});
    }
}

WorkspaceResult<json> import_workspace(const McpSession& session,
                                       const std::filesystem::path& path) {
    try {
        auto incoming = read_workspace(path);
        if (!incoming) return std::unexpected(incoming.error());
        auto current = workspace_to_json(session);
        if (!current) return std::unexpected(current.error());
        auto state = decode(*current);
        std::set<std::uint64_t> published_scores, published_projects;
        for (const auto& [id, score] : incoming->score.scores) {
            static_cast<void>(score);
            published_scores.insert(id);
        }
        for (const auto& [id, project] : incoming->project.projects) {
            static_cast<void>(project);
            published_projects.insert(id);
        }
        merge_history(state.namespace_history, incoming->namespace_history);
        const auto merge = [&](auto& destination, auto& source, const char* name) {
            for (const auto& [id, item] : source) {
                static_cast<void>(item);
                if (destination.contains(id))
                    deny(std::string("Import identity collision in ") + name + ": " +
                         std::to_string(id));
            }
            destination.merge(source);
        };
        merge(state.score.scores, incoming->score.scores, "scores");
        merge(state.timbre.profiles, incoming->timbre.profiles, "timbre_profiles");
        merge(state.mix.graphs, incoming->mix.graphs, "mix_graphs");
        merge(state.project.projects, incoming->project.projects, "projects");
        merge(state.corpus.corpus.composers, incoming->corpus.corpus.composers, "corpus.composers");
        merge(state.corpus.corpus.works, incoming->corpus.corpus.works, "corpus.works");
        std::set<std::uint64_t> presets;
        for (const auto& preset : state.timbre.preset_library)
            presets.insert(preset.id.value);
        for (auto& preset : incoming->timbre.preset_library) {
            if (!presets.insert(preset.id.value).second)
                deny("Import identity collision in preset_library: " +
                     std::to_string(preset.id.value));
            state.timbre.preset_library.push_back(std::move(preset));
        }
        // Merge allocator high-water values without treating local effect/Part IDs as store keys.
        preserve_counters(state, view(*incoming));
        preserve_counters(state, view(session));
        retain_observed_local_identities(state, session, &published_scores, &published_projects);
        auto response = summary(state);
        response["success"] = true;
        response["operation"] = "import";
        response["source"] = path.string();
        static_cast<void>(response.dump());
        publish(session, state);
        return response;
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{exception.what()});
    }
}

WorkspaceResult<json>
recover_workspace_backup(const McpSession& session, const std::filesystem::path& path, bool apply) {
    auto backup = path;
    backup += ".bak";
    auto state = read_workspace(backup);
    if (!state) return std::unexpected(state.error());
    try {
        std::optional<std::string> source_path = std::filesystem::absolute(path).string();
        if (state->native_realization.workspace_namespace ==
                session.realization->metadata.workspace_namespace &&
            session.realization->metadata.history_base_directory) {
            if (state->native_realization.history_base_directory &&
                state->native_realization.history_base_directory !=
                    session.realization->metadata.history_base_directory)
                deny("Native namespace has conflicting history locations");
            state->native_realization.history_base_directory =
                session.realization->metadata.history_base_directory;
        }
        preserve_counters(*state, view(session));
        retain_observed_local_identities(*state, session);
        auto response = summary(*state);
        response["success"] = true;
        response["source"] = backup.string();
        response["preview"] = !apply;
        response["file_repaired"] = false;
        std::optional<std::string> native_error;
        if (apply) {
            native_error =
                state->native_namespace_is_new
                    ? std::optional<std::string>{"Save the migrated native namespace first"}
                    : confirm_loaded_native_namespace(backup, state->native_realization);
            response["native_namespace_durability_confirmed"] = !native_error;
            if (native_error) response["native_history_error"] = *native_error;
        }
        static_cast<void>(response.dump());
        if (apply) {
            publish(session, *state);
            session.realization->namespace_saved_durably = !native_error;
            if (native_error) session.realization->history_error.swap(native_error);
            session.realization->workspace_path.swap(source_path);
        }
        return response;
    } catch (const std::exception& exception) {
        return std::unexpected(WorkspaceError{exception.what()});
    }
}

WorkspaceSaveResult save_workspace(const McpSession& session,
                                   const std::filesystem::path& path,
                                   const WorkspaceIoFault& fault,
                                   const NativeWorkspaceMetadata* publication) {
    WorkspaceSaveResult result;
    try {
        auto encoded = workspace_to_json(session);
        if (!encoded) {
            result.error = encoded.error().message;
            return result;
        }
        if (publication) {
            (*encoded)["native_realization"] = {
                {"workspace_namespace", publication->workspace_namespace},
                {"history_base_directory",
                 publication->history_base_directory ? json(*publication->history_base_directory)
                                                     : json(nullptr)}};
            static_cast<void>(decode(*encoded));
        }
        const auto bytes = encoded->dump(2) + "\n";
        std::error_code exists_error;
        const bool exists = std::filesystem::exists(path, exists_error);
        if (exists_error) {
            result.error = "Cannot inspect old workspace file: " + exists_error.message();
            return result;
        }
        result.backup_status = "No previous main file; existing backup retained";
        if (exists) {
            auto previous = read_bytes(path);
            if (!previous) {
                result.error = previous.error().message;
                return result;
            }
            bool valid_previous = false;
            std::string backup_bytes = *previous;
            try {
                const auto previous_document = parse_bytes(*previous);
                auto previous_state = decode(previous_document);
                if (previous_document.at("version") == 1) {
                    // Legacy backups cannot invent a fresh native namespace after
                    // recovery once the migrated main has dispatched native work.
                    auto migrated_backup = encode(view(previous_state));
                    migrated_backup["native_realization"] = encoded->at("native_realization");
                    static_cast<void>(decode(migrated_backup));
                    backup_bytes = migrated_backup.dump(2) + "\n";
                }
                valid_previous = true;
            } catch (const std::exception& exception) {
                result.backup_status =
                    std::string("Invalid previous main file; backup retained: ") + exception.what();
            }
            if (valid_previous) {
                auto backup = path;
                backup += ".bak";
                const auto saved =
                    replace_file(backup, backup_bytes, WorkspaceFileRole::Backup, fault);
                result.backup_updated = saved.committed;
                result.backup_status = saved.durable ? "Previous valid main file saved durably"
                                                     : "Backup durability unknown";
                if (!saved.error.empty()) {
                    result.error = "Backup update failed; main file retained: " + saved.error;
                    return result;
                }
            }
        }
        const auto saved = replace_file(path, bytes, WorkspaceFileRole::Main, fault);
        result.committed = saved.committed;
        result.durability_confirmed = saved.durable;
        result.error = saved.error;
        result.success = saved.committed && saved.error.empty();
        return result;
    } catch (const std::exception& exception) {
        result.error = exception.what();
        return result;
    }
}

} // namespace sunny::infrastructure
