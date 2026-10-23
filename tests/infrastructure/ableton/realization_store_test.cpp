#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace sunny::infrastructure;
using namespace sunny::core;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {
const std::string ns = "0123456789abcdef0123456789abcdef";
const std::string token = "11111111111111111111111111111111";
const ManagedBridgeContext context{"bridge_store", "document_store"};
struct Directory {
    fs::path path;
    Directory() {
        static std::atomic<unsigned> nonce{};
        path = fs::temp_directory_path() /
               ("sunny-realization-store-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(nonce.fetch_add(1)));
        REQUIRE(fs::create_directory(path));
    }
    ~Directory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};
[[maybe_unused]] std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}
[[maybe_unused]] void write(const fs::path& path, const std::string& value) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    REQUIRE(file.good());
    file.write(value.data(), static_cast<std::streamsize>(value.size()));
    file.close();
    REQUIRE(file.good());
}
ManagedClipProjection projection() {
    ManagedClipProjection result;
    result.clip_end = 4.0;
    result.notes = json::array({{{"pitch", 60},
                                 {"start_time", 0.0},
                                 {"duration", 1.0},
                                 {"velocity", 96},
                                 {"mute", false},
                                 {"probability", 1.0},
                                 {"velocity_deviation", 0.0},
                                 {"release_velocity", 64.0}}});
    return result;
}
RealizationAttemptIntent intent(std::string id = token) {
    const auto notes = projection();
    auto request = make_managed_clip_request(context, id, "w" + ns + "_s7", "part_9", notes);
    REQUIRE(request);
    const auto prepared = prepare_managed_operation(context, *request);
    REQUIRE(prepared);
    RealizationAttemptIntent result;
    result.attempt_id = std::move(id);
    result.score_id = ScoreId{7};
    result.part_id = PartId{9};
    result.project_revision = 12;
    result.desired_note_keys = {"e42_n0"};
    result.desired_projection = {{"clip_end", notes.clip_end},
                                 {"signature_numerator", notes.signature_numerator},
                                 {"signature_denominator", notes.signature_denominator},
                                 {"notes", notes.notes}};
    const auto digest =
        realization_note_identity(result.desired_note_keys, result.desired_projection);
    REQUIRE(digest);
    result.desired_note_identity = *digest;
    result.prepared = *prepared;
    return result;
}
[[maybe_unused]] void no_temporaries(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory))
        CHECK_FALSE(entry.path().filename().string().starts_with(".ledger-tmp-"));
}
[[maybe_unused]] json literal_observation() {
    // Literal actual Python model acknowledgement, with typed SM1 SHA256.
    return json::parse(R"JSON({
  "track_index": 1,
  "slot_index": 0,
  "manifest": {
    "schema_version": 1,
    "track": {
      "name": "Sunny|project_a|part_a|track",
      "mute": false,
      "solo": false,
      "arm": false,
      "implicit_arm": false,
      "is_frozen": false,
      "is_grouped": false,
      "back_to_arranger": false,
      "has_audio_input": false,
      "has_midi_input": true,
      "has_audio_output": false,
      "has_midi_output": true
    },
    "clip": {
      "name": "Sunny|project_a|part_a|clip",
      "signature_numerator": 4,
      "signature_denominator": 4,
      "start_marker": 0.0,
      "end_marker": 4.0,
      "loop_start": 0.0,
      "loop_end": 4.0,
      "looping": false,
      "muted": false,
      "has_envelopes": false,
      "has_groove": false,
      "is_session_clip": true,
      "is_arrangement_clip": false,
      "is_midi_clip": true,
      "is_audio_clip": false,
      "is_playing": false,
      "is_recording": false,
      "is_overdubbing": false,
      "is_triggered": false,
      "will_record_on_start": false,
      "launch_mode": 0,
      "launch_quantization": 1,
      "legato": false,
      "velocity_amount": 0.0
    },
    "notes": [
      {
        "pitch": 60,
        "start_time": 0.0,
        "duration": 1.0,
        "velocity": 96.0,
        "mute": false,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0
      }
    ],
    "mixer": {
      "panning_mode": 0,
      "crossfade_assign": 1,
      "volume": {
        "name": "Track Volume",
        "original_name": "Track Volume",
        "value": 0.85,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "panning": {
        "name": "Track Panning",
        "original_name": "Track Panning",
        "value": 0.0,
        "min": -1.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "track_activator": {
        "name": "Speaker On",
        "original_name": "Speaker On",
        "value": 1.0,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "sends": [
        {
          "name": "Send A",
          "original_name": "Send A",
          "value": 0.0,
          "min": 0.0,
          "max": 1.0,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        }
      ]
    },
    "routing": {
      "input_routing_type": {
        "display_name": "All Ins",
        "identifier": "0:All Ins"
      },
      "input_routing_channel": {
        "display_name": "All Channels",
        "identifier": "All Channels"
      },
      "output_routing_type": {
        "display_name": "No Output",
        "identifier": "5:No Output"
      },
      "output_routing_channel": {
        "display_name": "",
        "identifier": ""
      }
    },
    "content_counts": {
      "arrangement_clips": 0,
      "take_lanes": 0
    },
    "devices_empty": true,
    "other_session_clips_empty": true,
    "entire_clip_population_observed": true,
    "mpe_note_expression_state_observed": false,
    "follow_actions_state_observed": false
  },
  "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
  "structural_boundary_complete": true,
  "content_boundary_complete": false,
  "unavailable_reasons": [
    "MpeExpressionUnavailable: per-note expression fields were not observed",
    "FollowActionsUnavailable: Follow Action settings were not observed"
  ],
  "track_tag": "Sunny|project_a|part_a|track",
  "clip_tag": "Sunny|project_a|part_a|clip",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true
})JSON");
}

[[maybe_unused]] json acknowledgement(const RealizationAttemptIntent& original) {
    auto actual = literal_observation();
    const auto& payload = std::get<json>(original.prepared.request.args[0]);
    const auto project = payload.at("project_key").get<std::string>();
    const auto binding = payload.at("binding_key").get<std::string>();
    actual["track_tag"] = "Sunny|" + project + "|" + binding + "|track";
    actual["clip_tag"] = "Sunny|" + project + "|" + binding + "|clip";
    actual["manifest"]["track"]["name"] = actual["track_tag"];
    actual["manifest"]["clip"]["name"] = actual["clip_tag"];
    actual["content_fingerprint"] = *managed_detail::managed_digest(actual.at("manifest"));
    return {{"document_token", context.document_token},
            {"operation_id", original.attempt_id},
            {"name", original.prepared.request.property_or_method},
            {"request", payload},
            {"request_fingerprint",
             *managed_detail::managed_digest(json{
                 {"name", original.prepared.request.property_or_method}, {"request", payload}})},
            {"outcome", "acknowledged"},
            {"native_mutation_started", true},
            {"result", actual}};
}
class Peer final : public LomTransport {
  public:
    std::vector<LomRequest> requests;
    std::vector<LomResponse> responses;
    std::optional<fs::path> mutation_marker;
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (mutation_marker && request.property_or_method == "sunny_managed_create_clip") {
            std::ofstream file(*mutation_marker);
            file << "one simulated native mutation\n";
        }
        if (responses.empty())
            return {false, std::nullopt, "Lost reply", LomDeliveryState::SentWithoutValidResponse};
        auto response = responses.front();
        responses.erase(responses.begin());
        return response;
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override { return {}; }
    bool is_connected() const override { return true; }
};
[[maybe_unused]] std::unique_ptr<RealizationStore> initialize(const Directory& directory) {
    auto result = RealizationStore::open(directory.path, ns, RealizationStoreMode::InitializeNew);
    INFO((result ? "initialized" : result.error().message));
    REQUIRE(result);
    return std::move(*result);
}
} // namespace

TEST_CASE("Realization semantic identity admits complete closed geometry and distinct canonical "
          "note keys",
          "[realization-store][managed]") {
    const auto original = intent();
    CHECK(original.desired_note_identity.size() == 64);
    CHECK(realization_note_identity({"e18446744073709551615_n18446744073709551615"},
                                    original.desired_projection));
    for (const auto* key :
         {"e0_n0", "e042_n0", "e42_n00", "e42_n-1", "e42_n1.5", "e18446744073709551616_n0"})
        CHECK_FALSE(realization_note_identity({key}, original.desired_projection));
    CHECK_FALSE(realization_note_identity({}, original.desired_projection));
    auto two = original.desired_projection;
    two["notes"].push_back(two["notes"][0]);
    CHECK_FALSE(realization_note_identity({"e42_n0", "e42_n0"}, two));
    CHECK(realization_note_identity({"e42_n0", "e42_n1"}, two));
    for (const auto& [field, value] : std::vector<std::pair<std::string, json>>{
             {"clip_end", 0.0}, {"signature_numerator", 4.5}, {"signature_denominator", 3}}) {
        auto invalid = original.desired_projection;
        invalid[field] = value;
        CHECK_FALSE(realization_note_identity(original.desired_note_keys, invalid));
    }
    auto invalid = original.desired_projection;
    invalid["notes"][0]["pitch"] = 128;
    CHECK_FALSE(realization_note_identity(original.desired_note_keys, invalid));
    invalid = original.desired_projection;
    invalid["notes"][0]["unknown"] = true;
    CHECK_FALSE(realization_note_identity(original.desired_note_keys, invalid));
    invalid = original.desired_projection;
    invalid["notes"][0]["duration"] = -1.0;
    CHECK_FALSE(realization_note_identity(original.desired_note_keys, invalid));
}

#ifdef _WIN32
TEST_CASE("Windows native realization initialization refuses unconfirmed durability",
          "[realization-store][managed]") {
    Directory directory;
    const auto result =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::InitializeNew);
    REQUIRE_FALSE(result);
    CHECK_FALSE(result.error().committed);
    CHECK_FALSE(fs::exists(directory.path / ns));
}
#else
TEST_CASE("Durable fresh fences return one consumable permit and restored fences remain query-only",
          "[realization-store][managed][durability]") {
    Directory directory;
    auto store = initialize(directory);
    CHECK(store->attempts().empty());
    const auto first = intent("ffffffffffffffffffffffffffffffff");
    auto fence = store->fence(first);
    REQUIRE(fence);
    CHECK(store->find(first.attempt_id)->dispatch_ordinal == 1);
    auto moved = std::move(*fence);
    CHECK_FALSE(fence->take_prepared());
    const auto prepared = moved.take_prepared();
    REQUIRE(prepared);
    CHECK(managed_receipt_to_json(*prepared) == managed_receipt_to_json(first.prepared));
    CHECK_FALSE(moved.take_prepared());
    const auto second = intent("00000000000000000000000000000000");
    REQUIRE(store->fence(second));
    CHECK(store->find(second.attempt_id)->dispatch_ordinal == 2);
    CHECK(store->attempts().begin()->second.dispatch_ordinal ==
          2); // Opaque token sort is not chronology.
    auto changed = first;
    changed.project_revision = 13;
    CHECK_FALSE(store->fence(changed));
    CHECK(store->find(first.attempt_id)->intent.project_revision == 12);
    const auto allocated = store->new_attempt_id();
    REQUIRE(allocated);
    CHECK(allocated->size() == 32);
    CHECK_FALSE(store->find(*allocated));
    const auto encoded = json::parse(read(store->directory() / "ledger.json"));
    CHECK(encoded["attempts"][0]["dispatch_state"] == "may_have_sent");
    CHECK(encoded["attempts"][0]["dispatch_ordinal"] == 2);
    no_temporaries(store->directory());
    store.reset();
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    CHECK((*restored)->find(first.attempt_id)->intent.prepared.explicit_retry_safe());
    // Core Prepared is retained as original intent; the store never reissues a permit for it.
    CHECK_FALSE((*restored)->fence(first));
    CHECK((*restored)->find(second.attempt_id)->dispatch_ordinal == 2);
}

TEST_CASE("Realization evidence appends without rewriting intent or earlier acknowledgement",
          "[realization-store][managed][receipts]") {
    Directory directory;
    auto store = initialize(directory);
    const auto original = intent();
    auto fenced = store->fence(original);
    REQUIRE(fenced);
    Peer peer;
    const auto uncertain = execute_managed_operation(*fenced->take_prepared(), peer);
    REQUIRE(uncertain);
    REQUIRE(uncertain->outcome == ManagedOperationOutcome::Indeterminate);
    REQUIRE(store->append_evidence(token, *uncertain));
    peer.responses.push_back({true, LomValue{acknowledgement(original)}, std::nullopt});
    const auto acknowledged = reconcile_managed_operation(*uncertain, peer);
    REQUIRE(acknowledged);
    REQUIRE(acknowledged->outcome == ManagedOperationOutcome::Acknowledged);
    const auto binding = managed_binding_receipt(*acknowledged);
    REQUIRE(binding);
    REQUIRE(store->append_evidence(token, *acknowledged, *binding));
    REQUIRE(store->find(token)->evidence.size() == 2);
    CHECK(store->find(token)->bindings.size() == 1);
    CHECK(store->find(token)->intent.project_revision == 12);
    CHECK(store->find(token)->intent.desired_note_keys == std::vector<std::string>{"e42_n0"});
    CHECK(peer.requests.size() == 2);
    CHECK(peer.requests[0].property_or_method == "sunny_managed_create_clip");
    CHECK(peer.requests[1].property_or_method == "sunny_managed_operation");
    // Later query failure cannot remove earlier positive/partial evidence.
    peer.responses.push_back(
        {false, std::nullopt, "Later read-only query failed", LomDeliveryState::NotSent});
    const auto failed_query = reconcile_managed_operation(*acknowledged, peer);
    REQUIRE(failed_query);
    REQUIRE(store->append_evidence(token, *failed_query));
    CHECK(store->find(token)->evidence.size() == 3);
    CHECK(store->find(token)->evidence[1].journal == acknowledged->journal);
    const auto before = read(store->directory() / "ledger.json");
    auto foreign = *acknowledged;
    std::get<json>(foreign.request.args[0])["operation_id"] = "22222222222222222222222222222222";
    CHECK_FALSE(store->append_evidence(token, foreign));
    auto forged = *binding;
    forged.project_key = "foreign";
    CHECK_FALSE(store->append_evidence(token, *acknowledged, forged));
    CHECK(read(store->directory() / "ledger.json") == before);
    store.reset();
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    REQUIRE((*restored)->find(token));
    CHECK((*restored)->find(token)->evidence.size() == 3);
    CHECK(managed_binding_to_json((*restored)->find(token)->bindings[0]) ==
          managed_binding_to_json(*binding));
    CHECK(managed_receipt_to_json((*restored)->find(token)->intent.prepared) ==
          managed_receipt_to_json(original.prepared));
}

TEST_CASE("Failed fence write grants no dispatch and retains old bytes before atomic replacement",
          "[realization-store][managed][fault]") {
    for (const auto phase : {RealizationStoreIoPhase::CreateTemporary,
                             RealizationStoreIoPhase::AfterPartialWrite,
                             RealizationStoreIoPhase::FileSync,
                             RealizationStoreIoPhase::Replace}) {
        INFO(static_cast<int>(phase));
        Directory directory;
        auto store = initialize(directory);
        const auto before = read(store->directory() / "ledger.json");
        const auto failed =
            store->fence(intent(), [phase](auto observed) { return observed == phase; });
        REQUIRE_FALSE(failed);
        CHECK_FALSE(failed.error().committed);
        CHECK_FALSE(failed.error().durability_confirmed);
        CHECK_FALSE(store->native_writes_available());
        CHECK_FALSE(store->find(token));
        CHECK(read(store->directory() / "ledger.json") == before);
        no_temporaries(store->directory());
        store.reset();
        auto restored =
            RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
        REQUIRE(restored);
        CHECK((*restored)->attempts().empty());
    }
}

TEST_CASE("Update intent retains the complete desired projection rather than treating a delta as "
          "all notes",
          "[realization-store][managed][receipts]") {
    Directory directory;
    auto store = initialize(directory);
    const auto created = intent();
    REQUIRE(store->fence(created));
    auto journal = acknowledgement(created);
    auto& observed = journal["result"];
    auto actual_note = observed["manifest"]["notes"][0];
    actual_note["note_id"] = 41;
    observed["note_identity"] = {{"entire_clip_population_observed", true},
                                 {"notes", json::array({actual_note})}};
    observed["note_identity_fingerprint"] =
        *managed_detail::managed_digest(observed.at("note_identity"));
    auto acknowledged = created.prepared;
    acknowledged.delivery = LomDeliveryState::ResponseReceived;
    acknowledged.outcome = ManagedOperationOutcome::Acknowledged;
    acknowledged.journal = journal;
    const auto binding = managed_binding_receipt(acknowledged);
    REQUIRE(binding);
    REQUIRE(store->append_evidence(token, acknowledged, *binding));
    const auto changes = json::array({{{"note_id", 41},
                                       {"expected", observed["manifest"]["notes"][0]},
                                       {"updates", {{"velocity", 72.0}}}}});
    const std::string update_token = "22222222222222222222222222222222";
    const auto request = make_managed_note_update_request(context, update_token, *binding, changes);
    REQUIRE(request);
    const auto prepared = prepare_managed_operation(context, *request);
    REQUIRE(prepared);
    auto updated = created;
    updated.attempt_id = update_token;
    updated.project_revision = 13;
    updated.prepared = *prepared;
    updated.desired_projection["notes"][0]["velocity"] = 72;
    const auto digest =
        realization_note_identity(updated.desired_note_keys, updated.desired_projection);
    REQUIRE(digest);
    updated.desired_note_identity = *digest;
    REQUIRE(store->fence(updated));
    CHECK(store->find(update_token)->dispatch_ordinal == 2);
    CHECK(store->find(token)->intent.desired_projection["notes"][0]["velocity"] == 96);
    store.reset();
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    CHECK((*restored)->find(update_token)->intent.desired_projection["notes"].size() == 1);
    CHECK((*restored)->find(update_token)->intent.desired_projection["notes"][0]["velocity"] == 72);
    CHECK((*restored)->find(update_token)->intent.desired_note_keys ==
          std::vector<std::string>{"e42_n0"});
    CHECK(
        (*restored)->find(token)->bindings[0].observation["note_identity"]["notes"][0]["note_id"] ==
        41);
}

TEST_CASE("Pending native journals may settle once and conflicting terminal histories block writes",
          "[realization-store][managed][receipts]") {
    Directory directory;
    auto store = initialize(directory);
    const auto original = intent();
    auto fenced = store->fence(original);
    REQUIRE(fenced);
    auto pending = acknowledgement(original);
    pending["outcome"] = "pending";
    pending["native_mutation_started"] = false;
    pending.erase("result");
    Peer peer;
    peer.responses.push_back({true, LomValue{pending}, std::nullopt});
    const auto running = execute_managed_operation(*fenced->take_prepared(), peer);
    REQUIRE(running);
    REQUIRE(running->outcome == ManagedOperationOutcome::Indeterminate);
    REQUIRE(store->append_evidence(token, *running));
    peer.responses.push_back({true, LomValue{acknowledgement(original)}, std::nullopt});
    const auto finished = reconcile_managed_operation(*running, peer);
    REQUIRE(finished);
    REQUIRE(finished->outcome == ManagedOperationOutcome::Acknowledged);
    REQUIRE(store->append_evidence(token, *finished));
    const auto path = store->directory() / "ledger.json";
    const auto before = read(path);
    REQUIRE(store->append_evidence(token, *finished));
    CHECK(read(path) == before); // Repeated identical evidence consumes no capacity/write.

    auto conflicting = *finished;
    auto& result = conflicting.journal->at("result");
    result["manifest"]["mixer"]["volume"]["value"] = 0.5;
    result["content_fingerprint"] = *managed_detail::managed_digest(result.at("manifest"));
    // This is individually closed evidence for the same original request, but
    // one retained operation cannot acquire a different terminal native result.
    REQUIRE(managed_receipt_from_json(managed_receipt_to_json(conflicting)));
    CHECK_FALSE(store->append_evidence(token, conflicting));
    CHECK_FALSE(store->native_writes_available());
    CHECK(read(path) == before);
    CHECK(store->find(token)->evidence.size() == 2);
    store.reset();

    auto malformed = json::parse(before);
    malformed["attempts"][0]["evidence"].push_back(managed_receipt_to_json(conflicting));
    write(path, malformed.dump());
    const auto rejected =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().message.find("conflicting") != std::string::npos);
}

TEST_CASE(
    "Post-replacement sync failure exposes a committed query-only fence with unknown durability",
    "[realization-store][managed][fault]") {
    Directory directory;
    auto store = initialize(directory);
    const auto original = intent();
    const auto failed = store->fence(
        original, [](auto phase) { return phase == RealizationStoreIoPhase::DirectorySync; });
    REQUIRE_FALSE(failed);
    CHECK(failed.error().committed);
    CHECK_FALSE(failed.error().durability_confirmed);
    CHECK_FALSE(store->native_writes_available());
    REQUIRE(store->find(token));
    CHECK(store->find(token)->evidence.empty());
    no_temporaries(store->directory());
    store.reset();
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    REQUIRE((*restored)->find(token));
    CHECK_FALSE((*restored)->fence(original));
}

TEST_CASE("Lost reply plus failed evidence save retains the durable fence rather than restoring "
          "retry permission",
          "[realization-store][managed][fault]") {
    Directory directory;
    auto store = initialize(directory);
    const auto original = intent();
    auto fenced = store->fence(original);
    REQUIRE(fenced);
    Peer peer;
    const auto outcome = execute_managed_operation(*fenced->take_prepared(), peer);
    REQUIRE(outcome);
    REQUIRE(peer.requests.size() == 1);
    const auto before = read(store->directory() / "ledger.json");
    const auto failed = store->append_evidence(token, *outcome, std::nullopt, [](auto phase) {
        return phase == RealizationStoreIoPhase::FileSync;
    });
    REQUIRE_FALSE(failed);
    CHECK_FALSE(failed.error().committed);
    CHECK(read(store->directory() / "ledger.json") == before);
    store.reset();
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    REQUIRE((*restored)->find(token));
    CHECK((*restored)->find(token)->evidence.empty());
    CHECK_FALSE((*restored)->fence(original));
    CHECK(peer.requests.size() == 1);
}

TEST_CASE("Lifetime process lock excludes a second writer and initialization never replaces "
          "missing history",
          "[realization-store][managed][lock]") {
    Directory directory;
    auto store = initialize(directory);
    CHECK_FALSE(RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting));
    const auto child = fork();
    REQUIRE(child >= 0);
    if (child == 0) {
        const auto second =
            RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
        _exit(second ? 2 : 0);
    }
    int status = 0;
    REQUIRE(waitpid(child, &status, 0) == child);
    REQUIRE(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
    store.reset();
    auto reopened = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    reopened->reset();
    REQUIRE(fs::remove(directory.path / ns / "ledger.json"));
    const auto missing =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(missing);
    CHECK_FALSE(fs::exists(directory.path / ns / "ledger.json"));
    CHECK_FALSE(RealizationStore::open(directory.path, ns, RealizationStoreMode::InitializeNew));
    CHECK_FALSE(
        RealizationStore::open(directory.path, "ABCDEF", RealizationStoreMode::InitializeNew));
    CHECK_FALSE(RealizationStore::open(
        directory.path, "../../outside", RealizationStoreMode::InitializeNew));
}

TEST_CASE("Replacement namespace or lock inodes exclude the old handle even when another process "
          "locks identical history",
          "[realization-store][managed][lock]") {
    for (const bool replace_directory : {false, true}) {
        for (const bool repeat_evidence : {false, true}) {
            CAPTURE(replace_directory, repeat_evidence);
            Directory directory;
            auto store = initialize(directory);
            const auto original = intent();
            Peer peer;
            std::optional<ManagedOperationReceipt> outcome;
            if (repeat_evidence) {
                auto fenced = store->fence(original);
                REQUIRE(fenced);
                const auto executed = execute_managed_operation(*fenced->take_prepared(), peer);
                REQUIRE(executed);
                outcome = *executed;
                REQUIRE(store->append_evidence(token, *outcome));
            }
            const auto path = directory.path / ns;
            const auto before = read(path / "ledger.json");
            if (replace_directory) {
                const auto displaced = directory.path / "displaced";
                fs::rename(path, displaced);
                REQUIRE(fs::create_directory(path));
                REQUIRE(fs::copy_file(displaced / "ledger.json", path / "ledger.json"));
                REQUIRE(fs::copy_file(displaced / ".lock", path / ".lock"));
            } else {
                fs::rename(path / ".lock", path / ".old-lock");
                write(path / ".lock", "");
            }
            // A different process can acquire the new inode while this handle still owns
            // the original lock. Equal ledger bytes do not make those locks exclusive.
            const auto child = fork();
            REQUIRE(child >= 0);
            if (child == 0) {
                const auto replacement =
                    RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
                _exit(replacement ? 0 : 2);
            }
            int status = 0;
            REQUIRE(waitpid(child, &status, 0) == child);
            REQUIRE(WIFEXITED(status));
            REQUIRE(WEXITSTATUS(status) == 0);
            const auto check_error = [&](const RealizationStoreError& error) {
                CHECK_FALSE(error.committed);
                CHECK(error.message.find(replace_directory
                                             ? "directory identity changed"
                                             : "lock identity changed") != std::string::npos);
            };
            if (repeat_evidence) {
                const auto failed = store->append_evidence(token, *outcome);
                REQUIRE_FALSE(failed);
                check_error(failed.error());
                REQUIRE(store->find(token));
                CHECK(store->find(token)->evidence.size() == 1);
            } else {
                const auto failed = store->fence(original);
                REQUIRE_FALSE(failed);
                check_error(failed.error());
                CHECK(store->attempts().empty());
            }
            CHECK_FALSE(store->native_writes_available());
            CHECK(read(path / "ledger.json") == before);
            no_temporaries(path);
            store.reset();
            const auto replacement =
                RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
            REQUIRE(replacement);
            CHECK((*replacement)->attempts().size() == (repeat_evidence ? 1 : 0));
        }
    }
}

TEST_CASE("Replacing a namespace during publication revokes the current dispatch permit",
          "[realization-store][managed][lock][fault]") {
    Directory directory;
    auto store = initialize(directory);
    const auto path = directory.path / ns;
    const auto displaced = directory.path / "displaced";
    const auto before = read(path / "ledger.json");
    bool replaced = false;
    const auto failed = store->fence(intent(), [&](auto phase) {
        if (phase == RealizationStoreIoPhase::DirectorySync) {
            fs::rename(path, displaced);
            REQUIRE(fs::create_directory(path));
            write(path / "ledger.json", before);
            REQUIRE(fs::copy_file(displaced / ".lock", path / ".lock"));
            replaced = true;
        }
        return false;
    });
    REQUIRE(replaced);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().committed);
    CHECK_FALSE(failed.error().durability_confirmed);
    CHECK(failed.error().message.find("directory identity changed") != std::string::npos);
    CHECK_FALSE(store->native_writes_available());
    CHECK(read(path / "ledger.json") == before);
    const auto displaced_history = json::parse(read(displaced / "ledger.json"));
    REQUIRE(displaced_history["attempts"].size() == 1);
    CHECK(displaced_history["attempts"][0]["dispatch_state"] == "may_have_sent");
    no_temporaries(path);
    no_temporaries(displaced);
}

TEST_CASE("Strict realization codec blocks corrupt foreign duplicate and incomplete histories",
          "[realization-store][managed][codec]") {
    Directory directory;
    auto store = initialize(directory);
    const auto original = intent();
    REQUIRE(store->fence(original));
    const auto path = store->directory() / "ledger.json";
    const auto valid = json::parse(read(path));
    store.reset();
    std::vector<json> invalid;
    auto value = valid;
    value["workspace_namespace"] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    invalid.push_back(value);
    value = valid;
    value["schema_version"] = 4;
    invalid.push_back(value);
    value = valid;
    value["unknown"] = 1;
    invalid.push_back(value);
    value = valid;
    value["attempts"].push_back(value["attempts"][0]);
    invalid.push_back(value);
    for (const auto ordinal : {0, 2}) {
        value = valid;
        value["attempts"][0]["dispatch_ordinal"] = ordinal;
        invalid.push_back(value);
    }
    value = valid;
    value["attempts"][0]["dispatch_state"] = "prepared";
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["project_revision"] = 0;
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["part_id"] = 9.5;
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["desired_note_identity"] = std::string(64, '0');
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["desired_note_keys"] = {"e42_n00"};
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["desired_projection"]["notes"][0]["velocity"] = 95;
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["intent"]["prepared"]["request"]["args"][0]["binding_key"] = "part_a";
    invalid.push_back(value);
    value = valid;
    value["attempts"][0]["evidence"].push_back(managed_receipt_to_json(original.prepared));
    value["attempts"][0]["evidence"][0]["outcome"] = "acknowledged";
    invalid.push_back(value);
    for (const auto& malformed : invalid) {
        write(path, malformed.dump());
        const auto rejected =
            RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
        REQUIRE_FALSE(rejected);
        CHECK(read(path) == malformed.dump());
    }
    write(path,
          "{\"format\":\"sunny-realization-ledger\",\"format\":\"sunny-realization-ledger\"}");
    const auto duplicate =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().message.find("duplicate") != std::string::npos);
    write(path, valid.dump().substr(0, valid.dump().size() / 2));
    CHECK_FALSE(RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting));
    write(path, valid.dump());
    write(directory.path / ns / ".ledger-tmp-orphan", "half written history");
    const auto torn =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(torn);
    CHECK(torn.error().message.find("orphaned") != std::string::npos);
}

TEST_CASE("Realization codec bounds bytes depth attempts and evidence before granting writes",
          "[realization-store][managed][codec]") {
    Directory directory;
    auto store = initialize(directory);
    REQUIRE(store->fence(intent()));
    const auto path = store->directory() / "ledger.json";
    const auto valid = json::parse(read(path));
    store.reset();
    auto deep = json(0);
    for (unsigned i = 0; i < 40; ++i)
        deep = json::array({deep});
    auto value = valid;
    value["deep"] = deep;
    write(path, value.dump());
    auto result = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(result);
    CHECK(result.error().message.find("depth") != std::string::npos);
    value = valid;
    value["attempts"][0]["evidence"] = json::array();
    for (unsigned i = 0; i <= REALIZATION_STORE_MAX_EVIDENCE; ++i)
        value["attempts"][0]["evidence"].push_back(managed_receipt_to_json(intent().prepared));
    write(path, value.dump());
    result = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(result);
    CHECK(result.error().message.find("bound") != std::string::npos);
    value = valid;
    value["attempts"] = json::array();
    for (unsigned i = 0; i <= REALIZATION_STORE_MAX_ATTEMPTS; ++i)
        value["attempts"].push_back(nullptr);
    write(path, value.dump());
    result = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(result);
    CHECK(result.error().message.find("bound") != std::string::npos);
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file.good());
        file.seekp(static_cast<std::streamoff>(REALIZATION_STORE_MAX_BYTES));
        file.put('x');
        file.close();
        REQUIRE(file.good());
    }
    result = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE_FALSE(result);
    CHECK(result.error().message.find("bounded") != std::string::npos);
}

TEST_CASE("External ledger replacement conflicts poison the existing writer before another fence",
          "[realization-store][managed][codec]") {
    Directory directory;
    auto store = initialize(directory);
    REQUIRE(store->fence(intent()));
    const auto path = store->directory() / "ledger.json";
    auto changed = json::parse(read(path));
    changed["attempts"] = json::array();
    write(path, changed.dump());
    CHECK_FALSE(store->fence(intent("22222222222222222222222222222222")));
    CHECK_FALSE(store->native_writes_available());
    REQUIRE(store->blocked_reason());
    CHECK(store->blocked_reason()->find("conflicting") != std::string::npos);
    CHECK(store->find(token));
    CHECK(json::parse(read(path))["attempts"].empty());
}

TEST_CASE("Process loss after a durable fence and simulated mutation restores query-only history",
          "[realization-store][managed][crash]") {
    Directory directory;
    auto store = initialize(directory);
    store.reset();
    const auto original = intent();
    const auto marker = directory.path / "simulated-mutation";
    const auto child = fork();
    REQUIRE(child >= 0);
    if (child == 0) {
        auto opened =
            RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
        if (!opened) _exit(2);
        auto fenced = (*opened)->fence(original);
        if (!fenced) _exit(3);
        auto prepared = fenced->take_prepared();
        if (!prepared) _exit(4);
        Peer peer;
        peer.mutation_marker = marker;
        const auto outcome = execute_managed_operation(*prepared, peer);
        if (!outcome || outcome->outcome != ManagedOperationOutcome::Indeterminate) _exit(5);
        _exit(0); // No receipt save and no C++ destructors; OS releases the process lock.
    }
    int status = 0;
    REQUIRE(waitpid(child, &status, 0) == child);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
    CHECK(read(marker) == "one simulated native mutation\n");
    auto restored = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(restored);
    const auto* record = (*restored)->find(token);
    REQUIRE(record);
    CHECK(record->evidence.empty());
    CHECK(record->intent.prepared.outcome == ManagedOperationOutcome::Prepared);
    CHECK_FALSE((*restored)->fence(original));
    Peer query;
    query.responses.push_back({true, LomValue{acknowledgement(original)}, std::nullopt});
    const auto reconciled = reconcile_managed_operation(record->intent.prepared, query);
    REQUIRE(reconciled);
    REQUIRE(query.requests.size() == 1);
    CHECK(query.requests[0].property_or_method == "sunny_managed_operation");
    REQUIRE((*restored)->append_evidence(token, *reconciled));
    CHECK(read(marker) == "one simulated native mutation\n");
}

TEST_CASE("New namespace parent synchronization failure never grants a store or native send",
          "[realization-store][managed][fault]") {
    Directory directory;
    const auto failed = RealizationStore::open(
        directory.path, ns, RealizationStoreMode::InitializeNew, [](auto phase) {
            return phase == RealizationStoreIoPhase::ParentDirectorySync;
        });
    REQUIRE_FALSE(failed);
    CHECK(failed.error().committed);
    CHECK_FALSE(failed.error().durability_confirmed);
    CHECK_FALSE(RealizationStore::open(directory.path, ns, RealizationStoreMode::InitializeNew));
    const auto failed_reopen = RealizationStore::open(
        directory.path, ns, RealizationStoreMode::OpenExisting, [](auto phase) {
            return phase == RealizationStoreIoPhase::ParentDirectorySync;
        });
    REQUIRE_FALSE(failed_reopen);
    CHECK_FALSE(failed_reopen.error().committed);
    const auto existing =
        RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(existing);
    CHECK((*existing)->attempts().empty());
}

TEST_CASE("Schema1 migration preserves complete existing managed records and separate ordinals",
          "[realization-store][ordinary][migration]") {
    Directory directory;
    auto store = initialize(directory);
    REQUIRE(store->fence(intent()));
    const auto path = store->directory() / "ledger.json";
    auto legacy = json::parse(read(path));
    legacy["schema_version"] = 1;
    legacy.erase("ordinary_attempts");
    const auto original_bytes = legacy.dump(2) + "\n";
    store.reset();
    write(path, original_bytes);
    auto reopened = RealizationStore::open(directory.path, ns, RealizationStoreMode::OpenExisting);
    REQUIRE(reopened);
    CHECK(read(path) == original_bytes);
    REQUIRE((*reopened)->find(token));
    CHECK((*reopened)->ordinary_attempts().empty());
    auto prepared = prepare_ordinary_clip(
        {std::string(32, 'b'), std::string(32, 'c')},
        std::string(32, '2'),
        "create",
        {{"track_index", 0}, {"slot_index", 0}, {"clip_end", 4.0}, {"notes", json::array()}});
    REQUIRE(prepared);
    REQUIRE((*reopened)->fence_ordinary(*prepared));
    const auto migrated = json::parse(read(path));
    CHECK(migrated.at("schema_version") == 2);
    CHECK(migrated.at("attempts").dump() == legacy.at("attempts").dump());
    CHECK((*reopened)->find(token)->dispatch_ordinal == 1);
    CHECK((*reopened)->find_ordinary(std::string(32, '2'))->dispatch_ordinal == 1);
}
#endif
