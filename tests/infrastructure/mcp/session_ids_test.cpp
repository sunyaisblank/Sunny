/** Literal finite identity-domain checks through unbound MCP handlers. */
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <string_view>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/session_ids.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using json = nlohmann::json;

namespace {

constexpr auto maximum_id = std::numeric_limits<std::uint64_t>::max();

enum class Domain { Profile, TimbreEffect, Preset, Graph, Group, Aux, MixEffect, Reference };

struct AllocationCall {
    Domain domain;
    const char* tool;
    const char* returned_id;
    json arguments;
};

std::vector<AllocationCall> allocation_calls() {
    return {
        {Domain::Profile,
         "create_timbre_profile",
         "profile_id",
         {{"part_id", 1}, {"name", "Fresh"}}},
        {Domain::TimbreEffect,
         "add_effect",
         "effect_id",
         {{"profile_id", 1}, {"effect_type", "distortion"}}},
        {Domain::Preset, "save_preset", "preset_id", {{"profile_id", 1}, {"name", "Fresh"}}},
        {Domain::Graph, "create_mix_graph", "graph_id", {{"part_ids", {1}}}},
        {Domain::Group, "create_group_bus", "group_bus_id", {{"graph_id", 1}, {"name", "Fresh"}}},
        {Domain::Aux, "create_aux_bus", "aux_bus_id", {{"graph_id", 1}, {"name", "Fresh"}}},
        {Domain::MixEffect,
         "add_channel_effect",
         "effect_id",
         {{"graph_id", 1}, {"channel_id", 1}, {"effect_type", "compressor"}}},
        {Domain::MixEffect,
         "add_bus_effect",
         "effect_id",
         {{"graph_id", 1}, {"group_id", 1}, {"effect_type", "compressor"}}},
        {Domain::MixEffect,
         "add_aux_effect",
         "effect_id",
         {{"graph_id", 1}, {"aux_id", 1}, {"effect_type", "compressor"}}},
        {Domain::MixEffect,
         "add_master_effect",
         "effect_id",
         {{"graph_id", 1}, {"effect_type", "compressor"}}},
        {Domain::Reference,
         "create_reference_profile",
         "reference_id",
         {{"graph_id", 1}, {"name", "Fresh"}}},
    };
}

struct Fixture {
    McpServer server;
    McpSession session;

    Fixture() {
        register_timbre_tools(server, session.timbre);
        register_mix_tools(server, session.mix);
        session.timbre->profiles.emplace(
            1, create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Existing"));
        session.timbre->next_profile_id = 2;
        auto graph = create_mix_graph(MixGraphId{1}, {PartId{1}});
        REQUIRE(create_group_bus(graph, GroupBusId{1}, "Existing group", {}));
        REQUIRE(create_aux_bus(graph, AuxBusId{1}, "Existing aux"));
        session.mix->graphs.emplace(1, std::move(graph));
        session.mix->next_graph_id = 2;
        session.mix->next_group_id = 2;
        session.mix->next_aux_id = 2;
    }

    std::uint64_t& counter(Domain domain) {
        switch (domain) {
        case Domain::Profile:
            return session.timbre->next_profile_id;
        case Domain::TimbreEffect:
            return session.timbre->next_effect_id;
        case Domain::Preset:
            return session.timbre->next_preset_id;
        case Domain::Graph:
            return session.mix->next_graph_id;
        case Domain::Group:
            return session.mix->next_group_id;
        case Domain::Aux:
            return session.mix->next_aux_id;
        case Domain::MixEffect:
            return session.mix->next_effect_id;
        case Domain::Reference:
            return session.mix->next_ref_id;
        }
        throw std::logic_error("unknown fixture domain");
    }

    json call(const char* name, const json& arguments) {
        auto response =
            server.process_request({{"jsonrpc", "2.0"},
                                    {"id", 1},
                                    {"method", "tools/call"},
                                    {"params", {{"name", name}, {"arguments", arguments}}}});
        INFO(response.dump());
        REQUIRE(response.contains("result"));
        return json::parse(response["result"]["content"][0]["text"].get<std::string>());
    }

    json state() const {
        json result{{"profiles", json::array()},
                    {"graphs", json::array()},
                    {"presets", json::array()},
                    {"counters",
                     {session.timbre->next_profile_id,
                      session.timbre->next_effect_id,
                      session.timbre->next_preset_id,
                      session.mix->next_graph_id,
                      session.mix->next_group_id,
                      session.mix->next_aux_id,
                      session.mix->next_effect_id,
                      session.mix->next_ref_id}}};
        for (const auto& [id, profile] : session.timbre->profiles)
            result["profiles"].push_back({{"key", id}, {"value", timbre_to_json(profile)}});
        for (const auto& [id, graph] : session.mix->graphs)
            result["graphs"].push_back({{"key", id}, {"value", mix_to_json(graph)}});
        for (const auto& preset : session.timbre->preset_library)
            result["presets"].push_back(timbre_preset_to_json(preset));
        return result;
    }

    void import_identity(Domain domain, std::uint64_t id) {
        switch (domain) {
        case Domain::Profile:
            session.timbre->profiles.emplace(
                id, create_timbre_profile(TimbreProfileId{id}, PartId{1}, "Imported"));
            break;
        case Domain::Preset:
            session.timbre->preset_library.push_back(
                save_preset(*session.timbre->find(1), TimbrePresetId{id}, "Imported"));
            break;
        case Domain::TimbreEffect: {
            auto profile = create_timbre_profile(TimbreProfileId{2}, PartId{1}, "Imported effects");
            REQUIRE(add_effect(profile, Effect{EffectId{id}, DistortionEffect{}, true, 0.5f}));
            session.timbre->profiles.emplace(2, std::move(profile));
            break;
        }
        case Domain::Graph:
            session.mix->graphs.emplace(id, create_mix_graph(MixGraphId{id}, {PartId{1}}));
            break;
        default: {
            auto graph = create_mix_graph(MixGraphId{2}, {PartId{1}});
            if (domain == Domain::Group)
                REQUIRE(create_group_bus(graph, GroupBusId{id}, "Imported", {}));
            if (domain == Domain::Aux) REQUIRE(create_aux_bus(graph, AuxBusId{id}, "Imported"));
            if (domain == Domain::MixEffect)
                REQUIRE(add_master_effect(graph,
                                          MixEffect{MixEffectId{id}, MixCompressor{}, true, {}}));
            if (domain == Domain::Reference) {
                ReferenceProfile reference{};
                reference.id = ReferenceProfileId{id};
                reference.name = "Imported";
                add_reference_profile(graph, std::move(reference));
            }
            session.mix->graphs.emplace(2, std::move(graph));
            break;
        }
        }
    }
};

} // namespace

TEST_CASE("Every scratch allocation handler rejects zero and exhausted counters atomically",
          "[mcp][identity][scratch][allocation]") {
    for (const auto& operation : allocation_calls()) {
        for (const auto invalid : {std::uint64_t{0}, maximum_id}) {
            INFO(operation.tool << " counter=" << invalid);
            Fixture fixture;
            fixture.counter(operation.domain) = invalid;
            const auto before = fixture.state();
            const auto result = fixture.call(operation.tool, operation.arguments);
            REQUIRE(result.contains("error"));
            CHECK(result.at("error_code") == static_cast<int>(ErrorCode::ArithmeticOverflow));
            CHECK(fixture.state() == before);
        }
    }
}

TEST_CASE("Scratch allocation uses the last safe global identity once then reports exhaustion",
          "[mcp][identity][scratch][allocation]") {
    for (const auto& operation : allocation_calls()) {
        INFO(operation.tool);
        Fixture fixture;
        fixture.counter(operation.domain) = maximum_id - 1;
        const auto result = fixture.call(operation.tool, operation.arguments);
        REQUIRE_FALSE(result.contains("error"));
        CHECK(result.at(operation.returned_id) == maximum_id - 1);
        CHECK(fixture.counter(operation.domain) == maximum_id);
        const auto before = fixture.state();
        CHECK(fixture.call(operation.tool, operation.arguments).contains("error"));
        CHECK(fixture.state() == before);
    }
}

TEST_CASE("Imported identities in any scratch document guard the complete global domain",
          "[mcp][identity][scratch][allocation][import]") {
    for (const auto& operation : allocation_calls()) {
        for (const auto observed : {std::uint64_t{73}, maximum_id}) {
            INFO(operation.tool << " represented=" << observed);
            Fixture fixture;
            fixture.import_identity(operation.domain, observed);
            fixture.counter(operation.domain) = 73; // occupied at73; stale beneath importedMAX
            const auto before = fixture.state();
            const auto result = fixture.call(operation.tool, operation.arguments);
            REQUIRE(result.contains("error"));
            CHECK(fixture.state() == before);
            CHECK(result.at("error_code") == static_cast<int>(observed == maximum_id
                                                                  ? ErrorCode::ArithmeticOverflow
                                                                  : ErrorCode::InvariantViolation));
        }
    }
}

TEST_CASE("Global high-water allocations preserve imported holes and permit local duplicates",
          "[mcp][identity][scratch][allocation][import]") {
    for (const auto& operation : allocation_calls()) {
        INFO(operation.tool);
        Fixture fixture;
        fixture.import_identity(operation.domain, 73);
        fixture.counter(operation.domain) = 70; // unoccupied hole below the imported maximum
        const auto before = fixture.state();
        CHECK(fixture.call(operation.tool, operation.arguments).contains("error"));
        CHECK(fixture.state() == before);
        fixture.counter(operation.domain) = 74;
        const auto result = fixture.call(operation.tool, operation.arguments);
        REQUIRE_FALSE(result.contains("error"));
        CHECK(result.at(operation.returned_id) == 74);
        CHECK(fixture.counter(operation.domain) == 75);
    }
    Fixture duplicate;
    duplicate.import_identity(Domain::MixEffect, 73);
    REQUIRE(add_master_effect(*duplicate.session.mix->find(1),
                              MixEffect{MixEffectId{73}, MixCompressor{}, true, {}}));
    duplicate.session.mix->next_effect_id = 74;
    CHECK(duplicate.call("add_master_effect", {{"graph_id", 1}, {"effect_type", "eq"}})
              .at("effect_id") == 74);
    // Embedded preset IDs are local; they do not consume the shared library counter.
    duplicate.session.timbre->find(1)->presets.push_back(
        save_preset(*duplicate.session.timbre->find(1), TimbrePresetId{maximum_id}, "Local"));
    CHECK(duplicate.call("save_preset", {{"profile_id", 1}, {"name", "Shared"}}).at("preset_id") ==
          1);
}

TEST_CASE("Imported effects are observed in every Mix chain before scratch allocation",
          "[mcp][identity][scratch][allocation][import]") {
    for (int chain = 0; chain < 4; ++chain) {
        Fixture fixture;
        auto graph = create_mix_graph(MixGraphId{2}, {PartId{1}});
        REQUIRE(create_group_bus(graph, GroupBusId{2}, "Imported", {}));
        REQUIRE(create_aux_bus(graph, AuxBusId{2}, "Imported"));
        const MixEffect effect{MixEffectId{91}, MixCompressor{}, true, {}};
        if (chain == 0) REQUIRE(add_channel_effect(graph, ChannelStripId{1}, effect));
        if (chain == 1) REQUIRE(add_bus_effect(graph, GroupBusId{2}, effect));
        if (chain == 2) REQUIRE(add_aux_effect(graph, AuxBusId{2}, effect));
        if (chain == 3) REQUIRE(add_master_effect(graph, effect));
        fixture.session.mix->graphs.emplace(2, std::move(graph));
        fixture.session.mix->next_effect_id = 91;
        const auto before = fixture.state();
        CHECK(fixture.call("add_master_effect", {{"graph_id", 1}, {"effect_type", "eq"}})
                  .contains("error"));
        CHECK(fixture.state() == before);
    }
}

TEST_CASE("Scratch operation failures consume no prepared identity or musical configuration",
          "[mcp][identity][scratch][allocation][atomicity]") {
    for (const auto& operation : allocation_calls()) {
        INFO(operation.tool);
        Fixture fixture;
        auto invalid = operation.arguments;
        if (operation.domain == Domain::Profile) invalid["part_id"] = -1;
        if (operation.domain == Domain::Graph) invalid["part_ids"] = {-1};
        if (operation.domain == Domain::TimbreEffect) invalid["mix"] = 2.0;
        if (operation.domain == Domain::MixEffect) invalid["ratio"] = 0.25;
        if (operation.domain == Domain::Group) invalid["member_channel_ids"] = {999};
        if (operation.domain == Domain::Aux) invalid["name"] = json::array();
        if (operation.domain == Domain::Preset) invalid["tags"] = {12};
        if (operation.domain == Domain::Reference)
            invalid["tonal_balance"] = {{{"frequency", "bad"}, {"level", 0}}};
        const auto next = fixture.counter(operation.domain);
        const auto before = fixture.state();
        CHECK(fixture.call(operation.tool, invalid).contains("error"));
        CHECK(fixture.state() == before);
        const auto result = fixture.call(operation.tool, operation.arguments);
        REQUIRE_FALSE(result.contains("error"));
        CHECK(result.at(operation.returned_id) == next);
        CHECK(fixture.counter(operation.domain) == next + 1);
    }
}

TEST_CASE("Prepared global identity batches admit the whole finite range or consume nothing",
          "[mcp][identity][scratch][allocation][batch]") {
    auto counter = maximum_id - 2;
    const auto rejected = mcp_detail::checked_session_id_batch(counter, 8, 3);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == ErrorCode::ArithmeticOverflow);
    CHECK(counter == maximum_id - 2);
    const auto accepted = mcp_detail::checked_session_id_batch(counter, 8, 2);
    REQUIRE(accepted);
    CHECK(accepted->first == maximum_id - 2);
    CHECK(accepted->count == 2);
    CHECK(accepted->next == maximum_id);
    CHECK(counter == maximum_id - 2); // preparation never publishes the marker
    counter = accepted->next;
    CHECK_FALSE(mcp_detail::checked_session_id_batch(counter, maximum_id - 1));
    CHECK_FALSE(mcp_detail::checked_session_id_batch(8, maximum_id, 2));
    CHECK_FALSE(mcp_detail::checked_session_id_batch(8, 8, 2));
    const auto literal = mcp_detail::checked_session_id_batch(9, 7, 3);
    REQUIRE(literal);
    CHECK(literal->first == 9);
    CHECK(literal->count == 3);
    CHECK(literal->next == 12);
}

TEST_CASE(
    "Reference measurement overflow rejects the whole candidate without consuming its identity",
    "[mcp][identity][scratch][reference][atomicity]") {
    for (const auto* field : {"integrated_lufs",
                              "true_peak",
                              "loudness_range",
                              "avg_correlation",
                              "avg_width",
                              "frequency",
                              "level"}) {
        for (const auto invalid : {1e300,
                                   -1e300,
                                   std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::quiet_NaN()}) {
            INFO(field << " value=" << invalid);
            Fixture fixture;
            fixture.session.mix->next_ref_id = maximum_id - 1;
            const auto before = fixture.state();
            json arguments{{"graph_id", 1}, {"name", "Rejected measurement"}};
            if (std::string_view(field) == "frequency" || std::string_view(field) == "level") {
                // Validate the entire curve before publishing even its first valid point.
                arguments["tonal_balance"] = {{{"frequency", 100.0}, {"level", -3.0}},
                                              {{"frequency", 1000.0}, {"level", 0.0}}};
                arguments["tonal_balance"][1][field] = invalid;
            } else {
                arguments[field] = invalid;
            }
            CHECK(fixture.call("create_reference_profile", arguments).contains("error"));
            CHECK(fixture.state() == before);
            const auto valid =
                fixture.call("create_reference_profile", {{"graph_id", 1}, {"name", "Valid"}});
            REQUIRE_FALSE(valid.contains("error"));
            CHECK(valid.at("reference_id") == maximum_id - 1);
            CHECK(fixture.session.mix->next_ref_id == maximum_id);
        }
    }
}

TEST_CASE("Reference semantic boundaries are checked before float rounding and publication",
          "[mcp][identity][scratch][reference][atomicity]") {
    const std::vector<json> invalid_measurements{
        {{"loudness_range", -1.0}},
        {{"loudness_range", -std::numeric_limits<double>::denorm_min()}},
        {{"avg_correlation", -1.01}},
        {{"avg_correlation", std::nextafter(1.0, 2.0)}},
        {{"avg_width", -0.01}},
        {{"avg_width", std::nextafter(1.0, 2.0)}},
        {{"tonal_balance", {{{"frequency", 0.0}, {"level", 0.0}}}}},
        {{"tonal_balance", {{{"frequency", -1.0}, {"level", 0.0}}}}},
        // Finite positive JSON input becomes zero in f32 and cannot be a Hz point.
        {{"tonal_balance", {{{"frequency", 1e-300}, {"level", 0.0}}}}},
        {{"tonal_balance", {{{"frequency", 100.0}}}}},
        {{"tonal_balance", {{{"frequency", true}, {"level", 0.0}}}}},
    };
    for (const auto& measurement : invalid_measurements) {
        INFO(measurement.dump());
        Fixture fixture;
        const auto before = fixture.state();
        json arguments{{"graph_id", 1}, {"name", "Invalid boundary"}};
        arguments.update(measurement);
        CHECK(fixture.call("create_reference_profile", arguments).contains("error"));
        CHECK(fixture.state() == before);
        CHECK(fixture.session.mix->next_ref_id == 1);
    }

    Fixture fixture;
    const auto float_limit = static_cast<double>(std::numeric_limits<float>::max());
    const auto smallest_positive = static_cast<double>(std::numeric_limits<float>::denorm_min());
    const auto result = fixture.call("create_reference_profile",
                                     {{"graph_id", 1},
                                      {"name", "Finite endpoints"},
                                      {"integrated_lufs", -float_limit},
                                      {"true_peak", float_limit},
                                      {"loudness_range", float_limit},
                                      {"avg_correlation", -1.0},
                                      {"avg_width", 1.0},
                                      {"tonal_balance",
                                       {{{"frequency", smallest_positive}, {"level", -float_limit}},
                                        {{"frequency", 1.0}, {"level", float_limit}},
                                        {{"frequency", float_limit}, {"level", 0.0}}}}});
    REQUIRE_FALSE(result.contains("error"));
    CHECK(result.at("reference_id") == 1);
    CHECK(fixture.session.mix->next_ref_id == 2);
    const auto& references = fixture.session.mix->find(1)->reference_profiles;
    REQUIRE(references.size() == 1);
    CHECK(references[0].loudness_profile.integrated == -std::numeric_limits<float>::max());
    // An analysed reference is not a loudness target; signed measured dB values
    // do not inherit the target's true-peak <=0 constraint.
    CHECK(references[0].loudness_profile.true_peak == std::numeric_limits<float>::max());
    CHECK(references[0].dynamic_profile.loudness_range == std::numeric_limits<float>::max());
    CHECK(references[0].spatial_profile.average_correlation == -1.0f);
    CHECK(references[0].spatial_profile.average_width == 1.0f);
    REQUIRE(references[0].tonal_balance_curve.size() == 3);
    CHECK(references[0].tonal_balance_curve[0].first == std::numeric_limits<float>::denorm_min());
    CHECK(references[0].tonal_balance_curve[0].second == -std::numeric_limits<float>::max());
    CHECK(references[0].tonal_balance_curve[1].second == std::numeric_limits<float>::max());
    CHECK(references[0].tonal_balance_curve[2].first == std::numeric_limits<float>::max());
    CHECK(references[0].tonal_balance_curve[2].second == 0.0f);
    // The other closed spatial endpoints and zero LRA are admitted too.
    REQUIRE_FALSE(fixture
                      .call("create_reference_profile",
                            {{"graph_id", 1},
                             {"name", "Other endpoints"},
                             {"avg_correlation", 1.0},
                             {"avg_width", 0.0},
                             {"loudness_range", 0.0},
                             {"true_peak", 3.0}})
                      .contains("error"));
    CHECK(fixture.session.mix->next_ref_id == 3);
}
