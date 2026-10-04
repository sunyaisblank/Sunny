#include "managed_device_fixture.hpp"
#include "managed_device_modes_fixture.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <iomanip>
#include <sstream>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/managed_device_recovery.hpp>

using namespace sunny::infrastructure;
using namespace sunny::core;
using nlohmann::json;

namespace {
const std::string ns = "0123456789abcdef0123456789abcdef";
const std::string project = "w" + ns + "_s7";
const ManagedBridgeContext context{"bridge_a", "document_a"};
using History = std::map<std::string, RealizationStoredAttempt>;
std::string token(unsigned number) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(32) << number;
    return output.str();
}
void hash(json& value) {
    if (value.is_array() || value.is_object())
        for (auto& item : value)
            hash(item);
    if (!value.is_object()) return;
    if (value.contains("manifest")) {
        const auto digest = managed_detail::managed_digest(value.at("manifest"));
        REQUIRE(digest);
        value["content_fingerprint"] = *digest;
    }
    if (value.contains("device_identity")) {
        const auto digest = managed_detail::managed_digest(value.at("device_identity"));
        REQUIRE(digest);
        value["device_identity_fingerprint"] = *digest;
    }
    for (const auto* supplement : {"device_update", "device_mode_update"})
        if (value.contains(supplement)) {
            auto& update = value[supplement];
            const auto& before = update.at("before_observation");
            update["before_device_identity"] = before.at("device_identity");
            update["before_device_identity_fingerprint"] = before.at("device_identity_fingerprint");
        }
}
void owning(json& value) {
    if (value.is_string()) {
        auto text = value.get<std::string>();
        if (text == "source" || text == "current_source") text = "source_a";
        if (text == "effect" || text == "current_effect") text = "mix_b_effect_c";
        for (const auto& [before, after] :
             std::array{std::pair{std::string{"project_a"}, project},
                        std::pair{std::string{"part_a"}, std::string{"part_9"}}}) {
            const auto at = text.find(before);
            if (at != std::string::npos) text.replace(at, before.size(), after);
        }
        value = text;
    } else if (value.is_object()) {
        for (auto& [name, item] : value.items())
            if (name != "role") owning(item);
    } else if (value.is_array())
        for (auto& item : value)
            owning(item);
}
json fixture(bool mode = false) {
    auto result = mode ? managed_device_modes_fixture() : managed_device_fixture();
    owning(result);
    hash(result);
    return result;
}
json compiled(json notes) {
    for (auto& note : notes)
        note["velocity"] = note.at("velocity").get<int>();
    return notes;
}
json base(json observed) {
    for (const auto* field : {"device_update",
                              "device_mode_update",
                              "device_adoption",
                              "observed_notes_match_request",
                              "observed_clip_properties_match_request"})
        observed.erase(field);
    return observed;
}
RealizationStoredAttempt record(std::string method, json payload, json observed, unsigned ordinal) {
    const auto id = token(ordinal);
    payload["operation_id"] = id;
    if (method == "sunny_managed_insert_device" ||
        method == "sunny_managed_update_device_parameters" ||
        method == "sunny_managed_update_device_modes") {
        const auto& before =
            observed
                .at(method == "sunny_managed_update_device_modes" ? "device_mode_update"
                                                                  : "device_update")
                .at("before_observation");
        payload["expected_content_fingerprint"] = before.at("content_fingerprint");
        payload["expected_device_identity_fingerprint"] = before.at("device_identity_fingerprint");
    }
    const auto request = LomProtocol::call_method(LomPaths::song(), method, {payload});
    const auto prepared = prepare_managed_operation(context, request);
    INFO(method);
    INFO(payload.dump());
    REQUIRE(prepared);
    const auto digest =
        managed_detail::managed_digest(json{{"name", method}, {"request", payload}});
    REQUIRE(digest);
    const json journal{{"document_token", context.document_token},
                       {"operation_id", id},
                       {"name", method},
                       {"request", payload},
                       {"request_fingerprint", *digest},
                       {"outcome", "acknowledged"},
                       {"native_mutation_started", method != "sunny_managed_adopt_devices"},
                       {"result", observed}};
    auto evidence = *prepared;
    evidence.delivery = LomDeliveryState::ResponseReceived;
    evidence.outcome = ManagedOperationOutcome::Acknowledged;
    evidence.journal = journal;
    const auto checked = managed_receipt_from_json(managed_receipt_to_json(evidence));
    REQUIRE(checked);
    const auto binding = managed_binding_receipt(*checked);
    REQUIRE(binding);
    RealizationAttemptIntent intent;
    intent.attempt_id = id;
    intent.score_id = ScoreId{7};
    intent.part_id = PartId{9};
    intent.project_revision = 12;
    const auto& manifest = observed.at("manifest");
    for (std::size_t index = 0; index < manifest.at("notes").size(); ++index)
        intent.desired_note_keys.push_back("e" + std::to_string(index + 1) + "_n0");
    intent.desired_projection = {
        {"clip_end", manifest.at("clip").at("end_marker")},
        {"signature_numerator", manifest.at("clip").at("signature_numerator")},
        {"signature_denominator", manifest.at("clip").at("signature_denominator")},
        {"notes", compiled(manifest.at("notes"))}};
    const auto identity =
        realization_note_identity(intent.desired_note_keys, intent.desired_projection);
    REQUIRE(identity);
    intent.desired_note_identity = *identity;
    intent.prepared = *prepared;
    return {std::move(intent), ordinal, {*checked}, {*binding}};
}
RealizationStoredAttempt created(const json& observed, unsigned ordinal = 1) {
    auto result = base(observed);
    result["observed_notes_match_request"] = true;
    result["observed_clip_properties_match_request"] = true;
    const auto& manifest = result.at("manifest");
    return record("sunny_managed_create_clip",
                  {{"document_token", context.document_token},
                   {"project_key", project},
                   {"binding_key", "part_9"},
                   {"clip_end", manifest.at("clip").at("end_marker")},
                   {"signature_numerator", manifest.at("clip").at("signature_numerator")},
                   {"signature_denominator", manifest.at("clip").at("signature_denominator")},
                   {"notes", compiled(manifest.at("notes"))}},
                  result,
                  ordinal);
}
void add(History& history, RealizationStoredAttempt attempt) {
    const auto id = attempt.intent.attempt_id;
    REQUIRE(history.emplace(id, std::move(attempt)).second);
}
History chain() {
    const auto f = fixture();
    History history;
    add(history, created(f.at("before")));
    add(history,
        record("sunny_managed_insert_device", f.at("source_request"), f.at("source_result"), 2));
    add(history,
        record("sunny_managed_insert_device", f.at("effect_request"), f.at("effect_result"), 3));
    return history;
}
ManagedDeviceRecoveryScope scope(unsigned through = 3) {
    return {ns, ScoreId{7}, PartId{9}, token(through), std::nullopt};
}
RealizationStoredAttempt adoption(json approved, json observed, unsigned ordinal) {
    hash(approved);
    const auto digest = managed_detail::managed_digest(approved);
    REQUIRE(digest);
    observed = base(observed);
    hash(observed);
    observed["device_adoption"] = {{"preview_token", "preview"},
                                   {"preview_fingerprint", *digest},
                                   {"authority_origin", "explicit_current_device_adoption"},
                                   {"native_mutation_started", false},
                                   {"current_values_match_approved_intent", true},
                                   {"native_knob_only", true},
                                   {"host_qualified", false},
                                   {"opaque_state_observed", false}};
    return record("sunny_managed_adopt_devices",
                  {{"document_token", context.document_token},
                   {"project_key", project},
                   {"binding_key", "part_9"},
                   {"preview_token", "preview"},
                   {"approved_preview", approved}},
                  observed,
                  ordinal);
}
RealizationStoredAttempt musical_ack(json before, json observed, unsigned ordinal) {
    before = base(before);
    observed = base(observed);
    observed["note_update"] = {
        {"before_manifest", before.at("manifest")},
        {"before_note_identity", before.at("note_identity")},
        {"before_note_identity_fingerprint", before.at("note_identity_fingerprint")},
        {"notes_submitted", 1},
        {"observed_updates_match_request", true},
        {"untouched_notes_preserved", true},
        {"note_ids_preserved", true}};
    auto expected = before.at("note_identity").at("notes")[0];
    const auto id = expected.at("note_id");
    expected.erase("note_id");
    return record("sunny_managed_update_notes",
                  {{"document_token", context.document_token},
                   {"project_key", project},
                   {"binding_key", "part_9"},
                   {"expected_content_fingerprint", before.at("content_fingerprint")},
                   {"changes",
                    json::array({json{{"note_id", id},
                                      {"expected", expected},
                                      {"updates", {{"velocity", expected.at("velocity")}}}}})}},
                  observed,
                  ordinal);
}
json eq_approved() {
    const auto plain = fixture();
    const auto mode = fixture(true);
    auto approved = plain.at("preview_response").at("preview");
    approved["binding_observation"] = base(mode.at("scale_result"));
    approved["devices"][0]["physical_intents"][0]["target"] = 200.0;
    auto& source = approved["resolutions"][0];
    source["intent"]["target"] = 200.0;
    source["candidate"]["target"] = 200.0;
    source["candidate"]["descriptor"]["value"] = 0.0;
    source["candidate"]["internal_value"] = 0.0;
    source["candidate"]["display"] = "200.00 Hz";
    source["candidate"]["display_value"] = 200.0;
    source["candidate"]["samples"].back()["internal_value"] = 0.0;
    source["candidate"]["samples"].back()["display"] = "200.00 Hz";
    source["candidate"]["samples"].back()["display_value"] = 200.0;
    source["current_readback"]["internal_value"] = 0.0;
    source["current_readback"]["display"] = "200.00 Hz";
    source["current_readback"]["display_value"] = 200.0;
    approved["devices"][1]["device"] = mode.at("scale_request").at("device");
    approved["devices"][1]["physical_intents"] = mode.at("scale_request").at("physical_intents");
    auto& resolution = approved["resolutions"][1];
    resolution["intent"] = mode.at("scale_request").at("physical_intents")[0];
    resolution["candidate"] =
        mode.at("scale_result").at("device_update").at("resolutions")[0].at("candidate");
    resolution["current_readback"] = mode.at("scale_result").at("device_update").at("readbacks")[0];
    resolution["current_readback"].erase("capability_id");
    const auto current = resolution.at("current_readback").at("internal_value");
    resolution["candidate"]["descriptor"]["value"] = current;
    resolution["candidate"]["modes"]["Scale"]["value"] = current;
    for (const auto* field : {"display", "display_value", "display_increment"})
        resolution["candidate"]["eq8_scale_display"][field] =
            resolution.at("current_readback").at(field);
    hash(approved);
    REQUIRE(managed_device_detail::device_preview_valid(approved));
    return approved;
}
} // namespace

TEST_CASE("Saved native recovery folds empty-origin full chain without current IR targets",
          "[ableton][managed-device-recovery]") {
    const auto history = chain();
    const auto recovered = fold_managed_device_recovery(history, scope());
    REQUIRE(recovered);
    REQUIRE(recovered->selections.size() == 2);
    CHECK(recovered->device_history_attempt == token(3));
    CHECK(recovered->selections[0].device_key == "source_a");
    REQUIRE(recovered->selections[0].physical_intents.size() == 1);
    CHECK(recovered->selections[0].physical_intents[0].target == 1200.0);
    CHECK(recovered->selections[0].physical_intents[0].tolerance == 0.0);
    CHECK(recovered->selections[1].physical_intents[0].target == 0.0);
    CHECK(recovered->selections[1].chain_index == 1);
    CHECK(recovered->selections[1].device == ManagedNativeDevice::Utility);
    CHECK(recovered->contributing_attempt_ids == std::vector{token(1), token(2), token(3)});
    REQUIRE(recovered->residuals.size() == 2);
    CHECK_FALSE(recovered->residuals[1].observed_mode_guards.empty());
    CHECK(recovered->residuals[0].observed_modes.at("voice_mode").at("label") == "Poly");
}

TEST_CASE("Saved physical delta retains last exact target tolerance and untouched stages",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    auto f = fixture();
    auto result = f.at("source_result");
    result["device_update"]["inserted"] = false;
    result["device_update"]["output_role_transition"] = false;
    result["device_update"]["before_observation"] = base(f.at("effect_result"));
    result["device_identity"] = f.at("effect_result").at("device_identity");
    auto payload = f.at("source_request");
    payload["physical_intents"][0]["target"] = 2450.0;
    payload["physical_intents"][0]["tolerance"] = 0.01;
    auto& resolution = result["device_update"]["resolutions"][0];
    resolution["intent"] = payload.at("physical_intents")[0];
    auto& candidate = resolution["candidate"];
    candidate["target"] = 2450.0;
    candidate["display_tolerance"] = 0.01;
    candidate["internal_value"] = .75;
    candidate["descriptor"]["value"] = .5;
    candidate["display"] = "2450.00 Hz";
    candidate["display_value"] = 2450.0;
    candidate["samples"].back()["internal_value"] = .75;
    candidate["samples"].back()["display"] = "2450.00 Hz";
    candidate["samples"].back()["display_value"] = 2450.0;
    auto& readback = result["device_update"]["readbacks"][0];
    readback["internal_value"] = .75;
    readback["display"] = "2450.00 Hz";
    readback["display_value"] = 2450.0;
    result["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["value"] = .75;
    hash(result);
    add(history, record("sunny_managed_update_device_parameters", payload, result, 4));
    const auto recovered = fold_managed_device_recovery(history, scope(4));
    REQUIRE(recovered);
    CHECK(recovered->selections[0].physical_intents[0].target == 2450.0);
    CHECK(recovered->selections[0].physical_intents[0].tolerance == .01);
    CHECK(recovered->selections[1].physical_intents[0].target == 0.0);
    CHECK(recovered->device_history_attempt == token(4));
    const auto old = fold_managed_device_recovery(history, scope(3));
    REQUIRE(old);
    CHECK(old->selections[0].physical_intents[0].target == 1200.0);
}

TEST_CASE("Immutable terminal ACK survives a later changed-epoch query",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    auto unknown = history.at(token(3)).evidence.back();
    unknown.outcome = ManagedOperationOutcome::UnknownEpoch;
    unknown.journal = {{"outcome", "unknown_epoch"}, {"document_token", "document_new"}};
    REQUIRE(managed_receipt_from_json(managed_receipt_to_json(unknown)));
    history.at(token(3)).evidence.push_back(unknown);
    const auto recovered = fold_managed_device_recovery(history, scope());
    REQUIRE(recovered);
    CHECK(recovered->selections[0].physical_intents[0].target == 1200.0);
    auto stale = scope();
    stale.expected_device_history_attempt = token(2);
    const auto rejected = fold_managed_device_recovery(history, stale);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "StaleDeviceSnapshot");
}

TEST_CASE("Whole chain explicit adoption seeds exact saved intents without empty creation",
          "[ableton][managed-device-recovery]") {
    const auto f = fixture();
    auto approved = f.at("preview_response").at("preview");
    History history;
    add(history, adoption(approved, f.at("adopt_result"), 1));
    const auto recovered = fold_managed_device_recovery(history, scope(1));
    REQUIRE(recovered);
    CHECK(recovered->contributing_attempt_ids == std::vector{token(1)});
    CHECK(recovered->selections[0].device_key == "source_a");
    CHECK(recovered->selections[0].physical_intents[0].target == 1200.0);
    CHECK(recovered->selections[1].physical_intents[0].target == 0.0);
}

TEST_CASE("Saved recovery rejects malformed scope projection keys and unexplained chain",
          "[ableton][managed-device-recovery]") {
    const auto original = chain();
    SECTION("wrong namespace") {
        auto selected = scope();
        selected.workspace_namespace = std::string(32, 'a');
        const auto rejected = fold_managed_device_recovery(original, selected);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().reason == "WrongScope");
    }
    SECTION("wrong Part") {
        auto selected = scope();
        selected.part_id = PartId{10};
        CHECK_FALSE(fold_managed_device_recovery(original, selected));
    }
    SECTION("tampered musical hash") {
        auto history = original;
        history.at(token(3)).intent.desired_note_keys[0] = "e88_n0";
        const auto rejected = fold_managed_device_recovery(history, scope());
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().reason == "InvalidMusicalSource");
    }
    SECTION("missing creation proof") {
        auto history = original;
        history.erase(token(1));
        for (auto& [id, attempt] : history) {
            (void)id;
            --attempt.dispatch_ordinal;
        }
        const auto rejected = fold_managed_device_recovery(history, scope());
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().reason == "MissingEmptyOriginOrAdoption");
    }
    SECTION("duplicate dispatch ordinal") {
        auto history = original;
        history.at(token(3)).dispatch_ordinal = 2;
        CHECK_FALSE(fold_managed_device_recovery(history, scope()));
    }
    SECTION("missing binding") {
        auto history = original;
        history.at(token(3)).bindings.clear();
        const auto rejected = fold_managed_device_recovery(history, scope());
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().reason == "IncompleteHistory");
    }
}

TEST_CASE("Unknown or partial append cannot become a successful saved effect",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    auto& failed = history.at(token(3));
    failed.evidence.clear();
    failed.bindings.clear();
    const auto rejected = fold_managed_device_recovery(history, scope());
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "UnresolvedHistoryGap");
    CHECK(rejected.error().attempt_id == token(3));
}

TEST_CASE("Later musical ACK retains exact cumulative Device snapshot and no new targets",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    const auto observed = fixture().at("effect_result");
    add(history, musical_ack(observed, observed, 4));
    const auto recovered = fold_managed_device_recovery(history, scope(4));
    REQUIRE(recovered);
    CHECK(recovered->device_history_attempt == token(4));
    CHECK(recovered->selections[0].physical_intents[0].target == 1200.0);
    CHECK(recovered->selections[1].physical_intents[0].target == 0.0);
    CHECK(recovered->contributing_attempt_ids.back() == token(4));
    auto changed = observed;
    changed["device_identity"]["cohort"][0]["parameters"][2]["descriptor"]["value"] = .125;
    hash(changed);
    history.erase(token(4));
    // The native note receipt itself is valid. The independent history join
    // must still reject the changed uncommanded Device control.
    add(history, musical_ack(observed, changed, 4));
    const auto rejected = fold_managed_device_recovery(history, scope(4));
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "UncommandedDeviceDrift");
}

TEST_CASE("Saved Device delta must join full old cohort even when its own codec accepts it",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    auto f = fixture();
    auto result = f.at("update_result");
    result["device_update"]["before_observation"]["device_identity"]["cohort"][0]["parameters"][2]
          ["descriptor"]["value"] = .125;
    result["device_identity"]["cohort"][0]["parameters"][2]["descriptor"]["value"] = .125;
    hash(result);
    add(history,
        record("sunny_managed_update_device_parameters", f.at("update_request"), result, 4));
    const auto rejected = fold_managed_device_recovery(history, scope(4));
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "BrokenDeviceJoin");
}

TEST_CASE("EQ saved actual label modes remain observed guards and legacy unknown is unavailable",
          "[ableton][managed-device-recovery]") {
    auto approved = eq_approved();
    History history;
    add(history, adoption(approved, approved.at("binding_observation"), 1));
    const auto recovered = fold_managed_device_recovery(history, scope(1));
    REQUIRE(recovered);
    CHECK(recovered->selections[1].physical_intents[0].capability_id == "eq8.scale");
    CHECK(recovered->selections[1].physical_intents[0].target == 100.0);
    CHECK(recovered->selections[1].property_intents[0].label == "Stereo");
    CHECK(std::ranges::any_of(recovered->selections[1].enum_intents, [](const auto& mode) {
        return mode.capability_id == "eq8.band.1.type" && mode.label == "Bell";
    }));
    CHECK(recovered->residuals[1].observed_modes.at("edit_mode") == false);
    auto legacy = approved;
    legacy["binding_observation"]["device_identity"]["cohort"][1]["modes"].erase("edit_mode");
    legacy["binding_observation"]["device_identity"]["cohort"][1]["modes"].erase("oversample");
    history.clear();
    add(history, adoption(legacy, legacy.at("binding_observation"), 1));
    const auto rejected = fold_managed_device_recovery(history, scope(1));
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "IncompleteSavedModes");
}

TEST_CASE("Saved bypass omits physical requests but retains exact historical residual targets",
          "[ableton][managed-device-recovery]") {
    auto approved = eq_approved();
    const auto f = fixture(true);
    History history;
    add(history, adoption(approved, approved.at("binding_observation"), 1));
    add(history,
        record(
            "sunny_managed_update_device_modes", f.at("bypass_request"), f.at("bypass_result"), 2));
    const auto recovered = fold_managed_device_recovery(history, scope(2));
    REQUIRE(recovered);
    const auto& selected = recovered->selections[1];
    CHECK(selected.authored_bypass);
    CHECK(selected.physical_intents.empty());
    REQUIRE(selected.enum_intents.size() == 1);
    CHECK(selected.enum_intents[0].capability_id == "eq8.enabled");
    CHECK(selected.enum_intents[0].label == "Off");
    CHECK(selected.property_intents.empty());
    REQUIRE(recovered->residuals[1].bypassed_physical_intents.size() == 1);
    CHECK(recovered->residuals[1].bypassed_physical_intents[0].target == 100.0);
    CHECK_FALSE(recovered->residuals[1].bypassed_physical_intents_unknown);
}

TEST_CASE(
    "Active empty-target adoption is readable but cannot manufacture authored physical intent",
    "[ableton][managed-device-recovery]") {
    const auto f = fixture();
    auto approved = f.at("preview_response").at("preview");
    approved["devices"][1]["physical_intents"] = json::array();
    approved["resolutions"].erase(approved["resolutions"].begin() + 1);
    History history;
    add(history, adoption(approved, approved.at("binding_observation"), 1));
    const auto rejected = fold_managed_device_recovery(history, scope(1));
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "IncompletePhysicalIntent");
}

TEST_CASE("Saved explicit mode delta uses native labels instead of invented Off ordinal",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    const auto f = fixture();
    auto observed = base(f.at("effect_result"));
    const auto& row = observed.at("device_identity").at("cohort")[1].at("parameters")[5];
    REQUIRE(row.at("original_name") == "Mono");
    REQUIRE(row.at("descriptor").at("value") == 1.0);
    REQUIRE(row.at("descriptor").at("value_items")[1] == "Off");
    const json mode{{"capability_id", "utility.mono"}, {"label", "Off"}};
    observed["device_mode_update"] = {
        {"before_observation", base(observed)},
        {"before_device_identity", observed.at("device_identity")},
        {"before_device_identity_fingerprint", observed.at("device_identity_fingerprint")},
        {"device_key", "mix_b_effect_c"},
        {"admitted_modes",
         json::array({json{{"intent", mode},
                           {"parameter_index", 5},
                           {"parameter_original_name", "Mono"},
                           {"descriptor", row.at("descriptor")},
                           {"target_internal", 1.0}}})},
        {"admitted_properties", json::array()},
        {"readbacks",
         json::array({json{{"capability_id", "utility.mono"},
                           {"parameter_index", 5},
                           {"internal_value", 1.0},
                           {"label", "Off"}}})},
        {"property_readbacks", json::array()},
        {"clip_and_note_ids_preserved", true},
        {"native_knob_only", true},
        {"host_qualified", false},
        {"opaque_state_observed", false}};
    auto payload = f.at("update_request");
    payload.erase("physical_intents");
    payload["enum_intents"] = json::array({mode});
    payload["property_intents"] = json::array();
    add(history, record("sunny_managed_update_device_modes", payload, observed, 4));
    const auto recovered = fold_managed_device_recovery(history, scope(4));
    REQUIRE(recovered);
    CHECK(std::ranges::any_of(recovered->selections[1].enum_intents, [](const auto& saved) {
        return saved.capability_id == "utility.mono" && saved.label == "Off";
    }));
    CHECK_FALSE(
        std::ranges::any_of(recovered->residuals[1].observed_mode_guards, [](const auto& saved) {
            return saved.capability_id == "utility.mono";
        }));
}

TEST_CASE("Provably NotSent later intent contributes no physical delta or invented completion",
          "[ableton][managed-device-recovery]") {
    auto history = chain();
    const auto f = fixture();
    auto later = record(
        "sunny_managed_update_device_parameters", f.at("update_request"), f.at("update_result"), 4);
    std::get<json>(later.intent.prepared.request.args[0])["physical_intents"][0]["target"] = -7.5;
    later.evidence = {later.intent.prepared};
    later.evidence[0].outcome = ManagedOperationOutcome::NotSent;
    later.bindings.clear();
    add(history, later);
    const auto recovered = fold_managed_device_recovery(history, scope(4));
    REQUIRE(recovered);
    CHECK(recovered->device_history_attempt == token(3));
    CHECK(recovered->contributing_attempt_ids == std::vector{token(1), token(2), token(3)});
    CHECK(recovered->selections[1].physical_intents[0].target == 0.0);
}

TEST_CASE("Declined and partially started operations retain distinct recovery semantics",
          "[ableton][managed-device-recovery]") {
    const auto f = fixture();
    auto history = chain();
    auto later = record(
        "sunny_managed_update_device_parameters", f.at("update_request"), f.at("update_result"), 4);
    later.bindings.clear();
    auto& receipt = later.evidence[0];
    receipt.journal->erase("result");
    SECTION("explicit no-setter decline") {
        receipt.outcome = ManagedOperationOutcome::Declined;
        (*receipt.journal)["outcome"] = "declined";
        (*receipt.journal)["native_mutation_started"] = false;
        REQUIRE(managed_receipt_from_json(managed_receipt_to_json(receipt)));
        add(history, later);
        const auto recovered = fold_managed_device_recovery(history, scope(4));
        REQUIRE(recovered);
        CHECK(recovered->device_history_attempt == token(3));
        CHECK(recovered->contributing_attempt_ids.size() == 3);
    }
    SECTION("setter began, no terminal ACK") {
        receipt.outcome = ManagedOperationOutcome::Indeterminate;
        (*receipt.journal)["outcome"] = "indeterminate";
        (*receipt.journal)["native_mutation_started"] = true;
        REQUIRE(managed_receipt_from_json(managed_receipt_to_json(receipt)));
        add(history, later);
        const auto rejected = fold_managed_device_recovery(history, scope(4));
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().reason == "UnresolvedHistoryGap");
    }
}

TEST_CASE("Typed native receipts cannot synthesize a canonical owning source ID",
          "[ableton][managed-device-recovery]") {
    auto f = fixture();
    const auto replace = [&](const auto& self, json& value) -> void {
        if (value.is_string() && value == "source_a")
            value = "source_0";
        else if (value.is_object() || value.is_array())
            for (auto& item : value)
                self(self, item);
    };
    replace(replace, f);
    hash(f);
    History history;
    add(history, created(f.at("before")));
    add(history,
        record("sunny_managed_insert_device", f.at("source_request"), f.at("source_result"), 2));
    const auto rejected = fold_managed_device_recovery(history, scope(2));
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().reason == "InvalidDeviceKey");
}

TEST_CASE("Bypass-only adoption keeps missing historical physical intent explicitly unknown",
          "[ableton][managed-device-recovery]") {
    const auto f = fixture(true);
    auto approved = f.at("bypass_preview");
    const auto source = eq_approved();
    approved["devices"][0] = source.at("devices")[0];
    approved["resolutions"] = json::array({source.at("resolutions")[0]});
    History history;
    add(history, adoption(approved, approved.at("binding_observation"), 1));
    const auto recovered = fold_managed_device_recovery(history, scope(1));
    REQUIRE(recovered);
    CHECK(recovered->selections[1].authored_bypass);
    CHECK(recovered->selections[1].physical_intents.empty());
    CHECK(recovered->residuals[1].bypassed_physical_intents.empty());
    CHECK(recovered->residuals[1].bypassed_physical_intents_unknown);
}
