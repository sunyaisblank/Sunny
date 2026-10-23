/** Durable may-have-sent fences. This module never invokes a native transport. */
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <fstream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sunny::infrastructure {
using nlohmann::json;
namespace fs = std::filesystem;

namespace {
constexpr const char* ledger_name = "ledger.json";
constexpr const char* temporary_prefix = ".ledger-tmp-";

[[noreturn]] void deny(const std::string& message) {
    throw std::runtime_error(message);
}
bool hex(const std::string& value, std::size_t size) {
    return value.size() == size && std::ranges::all_of(value, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
void exact(const json& value, std::initializer_list<const char*> fields, const std::string& path) {
    if (!value.is_object() || value.size() != fields.size()) deny(path + ": unexpected fields");
    for (const auto* field : fields)
        if (!value.contains(field)) deny(path + ": missing " + field);
}
std::uint64_t positive(const json& value, const std::string& path) {
    const auto result = sunny::core::detail::checked_integer<std::uint64_t>(value, path);
    if (!result) deny(path + ": identity/revision must be positive");
    return result;
}
std::string hexadecimal(std::uint64_t value) {
    std::array<char, 16> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    return {buffer.data(), result.ptr};
}
bool decimal(std::string_view value, bool allow_zero) {
    if (value.empty() || (value.size() > 1 && value.front() == '0')) return false;
    std::uint64_t parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
           (allow_zero || parsed != 0);
}
bool note_key(const std::string& key) {
    const auto split = key.find("_n");
    return key.size() <= 43 && key.starts_with('e') && split != std::string::npos &&
           decimal(std::string_view(key).substr(1, split - 1), false) &&
           decimal(std::string_view(key).substr(split + 2), true);
}

std::string desired_identity(const std::vector<std::string>& keys, const json& projection) {
    exact(projection,
          {"clip_end", "signature_numerator", "signature_denominator", "notes"},
          "desired_projection");
    if (!projection.at("clip_end").is_number() || !projection.at("notes").is_array())
        deny("desired_projection: invalid geometry/notes");
    ManagedClipProjection candidate;
    candidate.clip_end = projection.at("clip_end").get<double>();
    candidate.signature_numerator = sunny::core::detail::checked_integer<int>(
        projection.at("signature_numerator"), "desired_projection.signature_numerator");
    candidate.signature_denominator = sunny::core::detail::checked_integer<int>(
        projection.at("signature_denominator"), "desired_projection.signature_denominator");
    candidate.notes = projection.at("notes");
    const auto checked =
        make_managed_clip_request({"projection_validation", "projection_validation"},
                                  "projection_validation",
                                  "projection_validation",
                                  "projection_validation",
                                  candidate);
    if (!checked) deny("desired_projection: unsupported note or clip domain");
    if (keys.size() != candidate.notes.size()) deny("desired_note_keys: expected one key per note");
    std::set<std::string> distinct;
    for (const auto& key : keys)
        if (!note_key(key) || !distinct.insert(key).second)
            deny("desired_note_keys: canonical distinct e<event>_n<index> keys required");
    const auto digest =
        managed_detail::managed_digest(json{{"keys", keys}, {"projection", projection}});
    if (!digest) deny("desired_projection: typed canonical digest is unavailable");
    return *digest;
}

json intent_json(const RealizationAttemptIntent& intent) {
    return {{"attempt_id", intent.attempt_id},
            {"score_id", intent.score_id.value},
            {"part_id", intent.part_id.value},
            {"project_revision", intent.project_revision},
            {"desired_note_identity", intent.desired_note_identity},
            {"desired_note_keys", intent.desired_note_keys},
            {"desired_projection", intent.desired_projection},
            {"prepared", managed_receipt_to_json(intent.prepared)}};
}
void validate_intent(const RealizationAttemptIntent& intent, const std::string& ns) {
    if (!hex(intent.attempt_id, 32) || !intent.score_id.value || !intent.part_id.value ||
        !intent.project_revision)
        deny("intent: invalid attempt/Score/Part/revision identity");
    const auto prepared = managed_receipt_from_json(managed_receipt_to_json(intent.prepared));
    if (!prepared || prepared->outcome != ManagedOperationOutcome::Prepared ||
        prepared->delivery != LomDeliveryState::NotSent || prepared->journal || prepared->error)
        deny("intent: original receipt must be a closed Prepared unsent operation");
    const auto& payload = std::get<json>(prepared->request.args.at(0));
    if (payload.at("operation_id") != intent.attempt_id ||
        payload.at("project_key") != "w" + ns + "_s" + hexadecimal(intent.score_id.value) ||
        payload.at("binding_key") != "part_" + hexadecimal(intent.part_id.value))
        deny("intent: request identity does not close against workspace/Score/Part/attempt");
    if (!hex(intent.desired_note_identity, 64) ||
        desired_identity(intent.desired_note_keys, intent.desired_projection) !=
            intent.desired_note_identity)
        deny("intent: desired semantic identity does not match retained keys/projection");
    if (prepared->request.property_or_method == "sunny_managed_create_clip" ||
        prepared->request.property_or_method == "sunny_managed_replace_clip") {
        const json requested{{"clip_end", payload.at("clip_end")},
                             {"signature_numerator", payload.at("signature_numerator")},
                             {"signature_denominator", payload.at("signature_denominator")},
                             {"notes", payload.at("notes")}};
        if (requested.dump() != intent.desired_projection.dump())
            deny("intent: create/replace request differs from complete desired projection");
    }
}
void validate_evidence(const RealizationAttemptIntent& intent,
                       const ManagedOperationReceipt& evidence) {
    const auto encoded = managed_receipt_to_json(evidence);
    const auto checked = managed_receipt_from_json(encoded);
    if (!checked || managed_receipt_to_json(*checked).dump() != encoded.dump())
        deny("evidence: managed receipt closure failed");
    const auto original = managed_receipt_to_json(intent.prepared);
    if (encoded.at("context").dump() != original.at("context").dump() ||
        encoded.at("request").dump() != original.at("request").dump())
        deny("evidence: original context/request/operation intent cannot change");
}
void validate_binding(const RealizationStoredAttempt& attempt,
                      const ManagedBindingReceipt& binding) {
    const auto encoded = managed_binding_to_json(binding);
    if (!managed_binding_from_json(encoded)) deny("binding: managed binding closure failed");
    for (const auto& evidence : attempt.evidence) {
        const auto derived = managed_binding_receipt(evidence);
        if (derived && managed_binding_to_json(*derived).dump() == encoded.dump()) return;
    }
    deny("binding: must derive exactly from this attempt's retained acknowledgement");
}
void validate_terminal_history(const RealizationStoredAttempt& attempt) {
    std::optional<std::string> retained;
    for (const auto& receipt : attempt.evidence) {
        if (!receipt.journal || receipt.outcome == ManagedOperationOutcome::UnknownEpoch ||
            receipt.outcome == ManagedOperationOutcome::UnknownOperation)
            continue;
        const auto journal = receipt.journal->dump();
        if (retained && *retained != journal)
            deny("evidence: conflicting retained terminal native journals");
        if (receipt.outcome == ManagedOperationOutcome::Acknowledged ||
            receipt.outcome == ManagedOperationOutcome::Declined ||
            receipt.journal->at("outcome") == "indeterminate")
            retained = journal;
    }
}
json workflow_json(const LegacyStoredWorkflow& w) {
    json children = json::array();
    for (const auto& child : w.children) {
        json evidence = json::array();
        for (const auto& r : child.evidence)
            evidence.push_back(legacy_receipt_to_json(r));
        children.push_back({{"prepared", legacy_receipt_to_json(child.prepared)},
                            {"dispatch_state", "may_have_sent"},
                            {"evidence", evidence}});
    }
    return {{"workflow_id", w.workflow_id},
            {"recipe", legacy_recipe_to_json(w.recipe)},
            {"state", w.state},
            {"disposition", w.disposition ? json(*w.disposition) : json{}},
            {"children", children}};
}
bool child_acknowledged(const LegacyStoredChild& c) {
    return std::ranges::any_of(c.evidence, [](const auto& r) {
        return r.outcome == LegacyOperationOutcome::Acknowledged;
    });
}
bool child_no_effect(const LegacyStoredChild& c) {
    const bool definite = std::ranges::any_of(c.evidence, [](const auto& r) {
        return r.outcome == LegacyOperationOutcome::Declined ||
               (r.outcome == LegacyOperationOutcome::NotSent &&
                (r.stage == LegacyOperationStage::Prepare ||
                 r.stage == LegacyOperationStage::Execute));
    });
    const bool effect = std::ranges::any_of(c.evidence, [](const auto& r) {
        return r.outcome == LegacyOperationOutcome::Acknowledged ||
               r.outcome == LegacyOperationOutcome::Partial;
    });
    return definite && !effect;
}
void validate_workflow(const LegacyStoredWorkflow& w, const std::string& ns) {
    if (!hex(w.workflow_id, 32) || !legacy_recipe_from_json(legacy_recipe_to_json(w.recipe)) ||
        w.recipe.authority.workspace_namespace != ns ||
        w.children.size() > w.recipe.commands.size())
        deny("legacy workflow: immutable recipe/namespace mismatch");
    if (w.state != "unresolved" && w.state != "completed" && w.state != "not_applied" &&
        w.state != "disposed")
        deny("legacy workflow: unsupported state");
    if (w.state == "disposed") {
        if (!w.disposition || w.disposition->empty() || w.disposition->size() > 4096)
            deny("legacy workflow: explicit retained-state disposition required");
    } else if (w.disposition)
        deny("legacy workflow: unexpected disposition");
    unsigned revision = w.recipe.authority.graph_revision;
    for (std::size_t i = 0; i < w.children.size(); ++i) {
        const auto& c = w.children[i];
        const auto& r = c.prepared;
        if (!legacy_receipt_from_json(legacy_receipt_to_json(r)) ||
            r.outcome != LegacyOperationOutcome::Prepared ||
            r.intent.at("workflow_id") != w.workflow_id || r.intent.at("ordinal") != i + 1 ||
            r.intent.at("graph_revision") != revision ||
            managed_detail::managed_digest(r.intent.at("command")) !=
                managed_detail::managed_digest(w.recipe.commands.at(i).at("command")))
            deny("legacy child: original recipe/ordinal/revision mismatch");
        for (const auto& [key, value] : w.recipe.authority.scope.items())
            if (r.intent.at(key) != value) deny("legacy child: original planning scope changed");
        if (c.evidence.size() > REALIZATION_STORE_MAX_EVIDENCE)
            deny("legacy evidence capacity exhausted");
        std::optional<json> terminal;
        for (const auto& e : c.evidence) {
            if (!legacy_receipt_from_json(legacy_receipt_to_json(e)) ||
                managed_detail::managed_digest(e.intent) !=
                    managed_detail::managed_digest(r.intent) ||
                e.outcome == LegacyOperationOutcome::Prepared)
                deny("legacy evidence: immutable intent or closed codec mismatch");
            if (e.journal && (e.outcome == LegacyOperationOutcome::Acknowledged ||
                              e.outcome == LegacyOperationOutcome::Declined ||
                              e.outcome == LegacyOperationOutcome::Partial)) {
                if (terminal && *terminal != *e.journal)
                    deny("legacy evidence: conflicting terminal journal");
                terminal = e.journal;
            } else if (terminal && e.outcome == LegacyOperationOutcome::NativePrepared)
                deny("legacy preparation after terminal journal");
        }
        if (terminal && terminal->at("outcome") == "acknowledged")
            revision = terminal->at("result").at("graph_revision").get<unsigned>();
        else if (i + 1 < w.children.size())
            deny("legacy continuation requires acknowledged predecessor");
    }
    if (w.state == "completed" && (w.children.size() != w.recipe.commands.size() ||
                                   !std::ranges::all_of(w.children, child_acknowledged)))
        deny("legacy completion requires entire acknowledged recipe");
    if (w.state == "not_applied" && !std::ranges::all_of(w.children, child_no_effect))
        deny("legacy not_applied requires definitive zero native effects");
}
LegacyStoredWorkflow decode_workflow(const json& v, const std::string& ns) {
    exact(v, {"workflow_id", "recipe", "state", "disposition", "children"}, "legacy workflow");
    auto recipe = legacy_recipe_from_json(v.at("recipe"));
    if (!recipe) deny("legacy recipe codec rejected value");
    LegacyStoredWorkflow w;
    w.workflow_id = v.at("workflow_id").get<std::string>();
    w.recipe = *recipe;
    w.state = v.at("state").get<std::string>();
    if (!v.at("disposition").is_null()) w.disposition = v.at("disposition").get<std::string>();
    if (!v.at("children").is_array() || v.at("children").size() > 4096)
        deny("legacy child capacity exhausted");
    for (const auto& child : v.at("children")) {
        exact(child, {"prepared", "dispatch_state", "evidence"}, "legacy child");
        auto prepared = legacy_receipt_from_json(child.at("prepared"));
        if (!prepared || child.at("dispatch_state") != "may_have_sent" ||
            !child.at("evidence").is_array() ||
            child.at("evidence").size() > REALIZATION_STORE_MAX_EVIDENCE)
            deny("legacy child: invalid fence");
        LegacyStoredChild c{*prepared, {}};
        for (const auto& evidence : child.at("evidence")) {
            auto r = legacy_receipt_from_json(evidence);
            if (!r) deny("legacy evidence codec rejected value");
            c.evidence.push_back(*r);
        }
        w.children.push_back(std::move(c));
    }
    validate_workflow(w, ns);
    return w;
}

json ledger_json(const std::string& ns,
                 const std::map<std::string, RealizationStoredAttempt>& attempts,
                 const std::map<std::string, OrdinaryStoredAttempt>& ordinary,
                 const std::map<std::string, LegacyStoredWorkflow>& legacy) {
    json records = json::array();
    for (const auto& [id, attempt] : attempts) {
        static_cast<void>(id);
        json evidence = json::array(), bindings = json::array();
        for (const auto& receipt : attempt.evidence)
            evidence.push_back(managed_receipt_to_json(receipt));
        for (const auto& binding : attempt.bindings)
            bindings.push_back(managed_binding_to_json(binding));
        records.push_back({{"intent", intent_json(attempt.intent)},
                           {"dispatch_ordinal", attempt.dispatch_ordinal},
                           {"dispatch_state", "may_have_sent"},
                           {"evidence", evidence},
                           {"bindings", bindings}});
    }
    json ordinary_records = json::array();
    for (const auto& [id, attempt] : ordinary) {
        static_cast<void>(id);
        json evidence = json::array();
        for (const auto& receipt : attempt.evidence)
            evidence.push_back(ordinary_receipt_to_json(receipt));
        ordinary_records.push_back({{"prepared", ordinary_receipt_to_json(attempt.prepared)},
                                    {"dispatch_ordinal", attempt.dispatch_ordinal},
                                    {"dispatch_state", "may_have_sent"},
                                    {"evidence", evidence}});
    }
    json legacy_records = json::array();
    for (const auto& [id, w] : legacy) {
        static_cast<void>(id);
        legacy_records.push_back(workflow_json(w));
    }
    json document{{"format", "sunny-realization-ledger"},
                  {"schema_version", legacy.empty() ? 2 : REALIZATION_STORE_SCHEMA_VERSION},
                  {"workspace_namespace", ns},
                  {"attempts", records},
                  {"ordinary_attempts", ordinary_records}};
    if (!legacy.empty()) document["legacy_workflows"] = std::move(legacy_records);
    return document;
}

void validate_ordinary_history(const OrdinaryStoredAttempt& attempt) {
    const auto prepared = ordinary_receipt_from_json(ordinary_receipt_to_json(attempt.prepared));
    if (!prepared || prepared->outcome != OrdinaryClipOutcome::Prepared || prepared->journal ||
        prepared->error)
        deny("ordinary attempt: original unsent Prepared receipt required");
    std::optional<json> terminal;
    for (const auto& evidence : attempt.evidence) {
        if (!ordinary_receipt_from_json(ordinary_receipt_to_json(evidence)) ||
            managed_detail::managed_digest(evidence.intent) !=
                managed_detail::managed_digest(prepared->intent) ||
            evidence.outcome == OrdinaryClipOutcome::Prepared)
            deny("ordinary evidence: original immutable intent/epoch mismatch");
        if (evidence.journal && evidence.outcome != OrdinaryClipOutcome::NativePrepared) {
            if (terminal && *terminal != *evidence.journal)
                deny("ordinary evidence: conflicting retained terminal journal");
            terminal = *evidence.journal;
        } else if (terminal && evidence.outcome == OrdinaryClipOutcome::NativePrepared) {
            deny("ordinary evidence: native preparation cannot follow a terminal journal");
        }
    }
}
json parse(const std::string& bytes) {
    if (bytes.size() > REALIZATION_STORE_MAX_BYTES) deny("Realization ledger exceeds 64 MiB");
    std::vector<std::set<std::string>> objects;
    return json::parse(bytes, [&](int depth, json::parse_event_t event, json& value) {
        if (depth > 32) deny("Realization ledger exceeds depth 32");
        if (event == json::parse_event_t::object_start)
            objects.emplace_back();
        else if (event == json::parse_event_t::key) {
            if (objects.empty() || !objects.back().insert(value.get<std::string>()).second)
                deny("Realization ledger contains duplicate JSON fields");
        } else if (event == json::parse_event_t::object_end)
            objects.pop_back();
        return true;
    });
}
std::map<std::string, RealizationStoredAttempt>
decode(const std::string& bytes,
       const std::string& ns,
       std::map<std::string, OrdinaryStoredAttempt>* ordinary = nullptr,
       std::map<std::string, LegacyStoredWorkflow>* legacy = nullptr) {
    const auto document = parse(bytes);
    const auto schema =
        sunny::core::detail::checked_integer<int>(document.at("schema_version"), "schema_version");
    if (schema == 1)
        exact(document, {"format", "schema_version", "workspace_namespace", "attempts"}, "ledger");
    else if (schema == 2)
        exact(document,
              {"format", "schema_version", "workspace_namespace", "attempts", "ordinary_attempts"},
              "ledger");
    else if (schema == 3)
        exact(document,
              {"format",
               "schema_version",
               "workspace_namespace",
               "attempts",
               "ordinary_attempts",
               "legacy_workflows"},
              "ledger");
    else
        deny("Unsupported realization ledger schema; retain the older snapshot for software "
             "rollback");
    if (document.at("format") != "sunny-realization-ledger" ||
        sunny::core::detail::checked_integer<int>(document.at("schema_version"),
                                                  "schema_version") != schema ||
        document.at("workspace_namespace") != ns)
        deny("Realization ledger format/version/workspace namespace mismatch");
    const auto& records = document.at("attempts");
    if (!records.is_array() || records.size() > REALIZATION_STORE_MAX_ATTEMPTS)
        deny("Realization ledger attempt count exceeds admitted bound");
    std::map<std::string, RealizationStoredAttempt> result;
    std::string previous;
    std::set<std::uint64_t> ordinals;
    for (const auto& record : records) {
        exact(record,
              {"intent", "dispatch_ordinal", "dispatch_state", "evidence", "bindings"},
              "attempt");
        if (record.at("dispatch_state") != "may_have_sent")
            deny("attempt: dispatch fence is required");
        const auto& encoded = record.at("intent");
        exact(encoded,
              {"attempt_id",
               "score_id",
               "part_id",
               "project_revision",
               "desired_note_identity",
               "desired_note_keys",
               "desired_projection",
               "prepared"},
              "intent");
        RealizationStoredAttempt attempt;
        attempt.dispatch_ordinal = positive(record.at("dispatch_ordinal"), "dispatch_ordinal");
        if (attempt.dispatch_ordinal > records.size() ||
            !ordinals.insert(attempt.dispatch_ordinal).second)
            deny("dispatch ordinals must be unique and dense 1..attempt count");
        auto& intent = attempt.intent;
        intent.attempt_id = encoded.at("attempt_id").get<std::string>();
        intent.score_id = sunny::core::ScoreId{positive(encoded.at("score_id"), "score_id")};
        intent.part_id = sunny::core::PartId{positive(encoded.at("part_id"), "part_id")};
        intent.project_revision = positive(encoded.at("project_revision"), "project_revision");
        intent.desired_note_identity = encoded.at("desired_note_identity").get<std::string>();
        intent.desired_note_keys = encoded.at("desired_note_keys").get<std::vector<std::string>>();
        intent.desired_projection = encoded.at("desired_projection");
        const auto prepared = managed_receipt_from_json(encoded.at("prepared"));
        if (!prepared) deny("intent: managed receipt codec rejected original request");
        intent.prepared = *prepared;
        validate_intent(intent, ns);
        if (intent.attempt_id <= previous) deny("attempts must be sorted and unique by attempt_id");
        previous = intent.attempt_id;
        const auto& evidence = record.at("evidence");
        const auto& bindings = record.at("bindings");
        if (!evidence.is_array() || !bindings.is_array() ||
            evidence.size() > REALIZATION_STORE_MAX_EVIDENCE ||
            bindings.size() > REALIZATION_STORE_MAX_EVIDENCE)
            deny("attempt: evidence/binding count exceeds admitted bound");
        for (const auto& value : evidence) {
            const auto receipt = managed_receipt_from_json(value);
            if (!receipt) deny("evidence: managed receipt codec rejected result");
            validate_evidence(intent, *receipt);
            attempt.evidence.push_back(*receipt);
        }
        validate_terminal_history(attempt);
        std::set<std::string> distinct_bindings;
        for (const auto& value : bindings) {
            const auto binding = managed_binding_from_json(value);
            if (!binding || !distinct_bindings.insert(value.dump()).second)
                deny("binding: invalid or duplicate binding receipt");
            validate_binding(attempt, *binding);
            attempt.bindings.push_back(*binding);
        }
        result.emplace(intent.attempt_id, std::move(attempt));
    }
    std::map<std::string, OrdinaryStoredAttempt> ordinary_result;
    if (schema >= 2) {
        const auto& ordinary_records = document.at("ordinary_attempts");
        if (!ordinary_records.is_array() ||
            ordinary_records.size() + records.size() > REALIZATION_STORE_MAX_ATTEMPTS)
            deny("Realization ledger combined attempt capacity exhausted");
        std::set<std::uint64_t> ordinary_ordinals;
        std::string previous_ordinary;
        for (const auto& record : ordinary_records) {
            exact(record,
                  {"prepared", "dispatch_ordinal", "dispatch_state", "evidence"},
                  "ordinary attempt");
            const auto prepared = ordinary_receipt_from_json(record.at("prepared"));
            if (!prepared || record.at("dispatch_state") != "may_have_sent")
                deny("ordinary attempt: invalid receipt/fence");
            OrdinaryStoredAttempt attempt{
                *prepared, positive(record.at("dispatch_ordinal"), "ordinary ordinal"), {}};
            const auto id = prepared->intent.at("operation_id").get<std::string>();
            if (id <= previous_ordinary || result.contains(id) ||
                attempt.dispatch_ordinal > ordinary_records.size() ||
                !ordinary_ordinals.insert(attempt.dispatch_ordinal).second)
                deny("ordinary attempts: duplicate identity or non-dense ordinal");
            previous_ordinary = id;
            if (!record.at("evidence").is_array() ||
                record.at("evidence").size() > REALIZATION_STORE_MAX_EVIDENCE)
                deny("ordinary evidence capacity exhausted");
            for (const auto& value : record.at("evidence")) {
                const auto evidence = ordinary_receipt_from_json(value);
                if (!evidence) deny("ordinary evidence codec rejected result");
                attempt.evidence.push_back(*evidence);
            }
            validate_ordinary_history(attempt);
            ordinary_result.emplace(id, std::move(attempt));
        }
    }
    std::map<std::string, LegacyStoredWorkflow> legacy_result;
    if (schema == 3) {
        const auto& workflows = document.at("legacy_workflows");
        if (!workflows.is_array() || workflows.size() > 4096)
            deny("legacy workflow capacity exhausted");
        std::set<std::string> ids;
        for (const auto& [id, record] : result) {
            static_cast<void>(record);
            ids.insert(id);
        }
        for (const auto& [id, record] : ordinary_result) {
            static_cast<void>(record);
            ids.insert(id);
        }
        std::string previous_id;
        for (const auto& encoded : workflows) {
            auto w = decode_workflow(encoded, ns);
            if (w.workflow_id <= previous_id || !ids.insert(w.workflow_id).second)
                deny("legacy workflow identity collision/order");
            previous_id = w.workflow_id;
            for (const auto& child : w.children)
                if (!ids.insert(child.prepared.intent.at("operation_id").get<std::string>()).second)
                    deny("legacy operation identity collision");
            legacy_result.emplace(w.workflow_id, std::move(w));
        }
        if (ids.size() - legacy_result.size() > REALIZATION_STORE_MAX_ATTEMPTS)
            deny("combined ledger identity capacity exhausted");
    }
    if (legacy) *legacy = std::move(legacy_result);
    if (ordinary) *ordinary = std::move(ordinary_result);
    return result;
}
#ifndef _WIN32
std::string io_error(const std::string& operation) {
    return operation + ": " + std::generic_category().message(errno);
}
struct Descriptor {
    int value = -1;
    ~Descriptor() {
        if (value >= 0) close(value);
    }
};
bool synchronize(int fd) {
    int result;
    do {
        result = fsync(fd);
    } while (result == -1 && errno == EINTR);
    return result == 0;
}
#endif
} // namespace

struct RealizationStore::Impl {
    std::string ns;
    fs::path path;
    std::map<std::string, RealizationStoredAttempt> attempts;
    std::map<std::string, OrdinaryStoredAttempt> ordinary_attempts;
    std::map<std::string, LegacyStoredWorkflow> legacy_workflows;
    std::set<std::string> fresh_workflows;
    bool identity_exists(const std::string& id) const {
        if (attempts.contains(id) || ordinary_attempts.contains(id) ||
            legacy_workflows.contains(id))
            return true;
        for (const auto& [key, w] : legacy_workflows) {
            static_cast<void>(key);
            for (const auto& c : w.children)
                if (c.prepared.intent.at("operation_id") == id) return true;
        }
        return false;
    }
    std::size_t identity_count() const {
        auto count = attempts.size() + ordinary_attempts.size();
        for (const auto& [key, w] : legacy_workflows) {
            static_cast<void>(key);
            count += w.children.size();
        }
        return count;
    }
    std::string disk_bytes;
    std::optional<std::string> blocked;
#ifdef _WIN32
    HANDLE lock = INVALID_HANDLE_VALUE;
    ~Impl() {
        if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
    }
#else
    int directory_fd = -1;
    int lock_fd = -1;
    ~Impl() {
        if (lock_fd >= 0) close(lock_fd);
        if (directory_fd >= 0) close(directory_fd);
    }
#endif
    void validate_location() const {
#ifndef _WIN32
        struct stat held_directory {
        }, named_directory{}, held_lock{}, named_lock{};
        const auto same = [](const struct stat& held, const struct stat& named) {
            return held.st_dev == named.st_dev && held.st_ino == named.st_ino;
        };
        if (fstat(directory_fd, &held_directory) != 0 ||
            lstat(path.c_str(), &named_directory) != 0 || !S_ISDIR(named_directory.st_mode) ||
            !same(held_directory, named_directory))
            deny("Realization namespace directory identity changed; conflicting history");
        if (fstat(lock_fd, &held_lock) != 0 || !S_ISREG(held_lock.st_mode) ||
            fstatat(directory_fd, ".lock", &named_lock, AT_SYMLINK_NOFOLLOW) != 0 ||
            !S_ISREG(named_lock.st_mode) || !same(held_lock, named_lock))
            deny("Realization lock identity changed; conflicting history");
#endif
    }
    std::string read() const {
        validate_location();
        for (const auto& entry : fs::directory_iterator(path))
            if (entry.path().filename().string().starts_with(temporary_prefix))
                deny("Realization ledger has orphaned temporary history; native writes blocked");
#ifdef _WIN32
        std::ifstream file(path / ledger_name, std::ios::binary);
        if (!file) deny("Existing realization ledger is missing or unreadable");
        std::string result;
        std::array<char, 16384> buffer{};
        while (file) {
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            result.append(buffer.data(), static_cast<std::size_t>(file.gcount()));
            if (result.size() > REALIZATION_STORE_MAX_BYTES)
                deny("Realization ledger exceeds 64 MiB");
        }
        if (!file.eof() || file.bad()) deny("Realization ledger read failed");
#else
        Descriptor file{openat(directory_fd, ledger_name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
        if (file.value < 0) deny(io_error("Existing realization ledger is missing or unreadable"));
        struct stat information {};
        if (fstat(file.value, &information) != 0 || !S_ISREG(information.st_mode) ||
            information.st_size < 0 ||
            static_cast<std::uintmax_t>(information.st_size) > REALIZATION_STORE_MAX_BYTES)
            deny("Realization ledger must be a bounded regular file");
        std::string result;
        std::array<char, 16384> buffer{};
        while (true) {
            const auto size = ::read(file.value, buffer.data(), buffer.size());
            if (size < 0 && errno == EINTR) continue;
            if (size < 0) deny(io_error("Realization ledger read failed"));
            if (!size) break;
            result.append(buffer.data(), static_cast<std::size_t>(size));
            if (result.size() > REALIZATION_STORE_MAX_BYTES)
                deny("Realization ledger exceeds 64 MiB");
        }
#endif
        validate_location();
        return result;
    }
    void verify_unchanged() {
        try {
            const auto observed = read();
            if (observed != disk_bytes)
                deny("Realization ledger changed outside this writer; conflicting history");
        } catch (const std::exception& error) {
            blocked = error.what();
            throw;
        }
    }
    RealizationStoreResult<void>
    write(std::map<std::string, RealizationStoredAttempt> candidate,
          const RealizationStoreIoFault& fault,
          bool initialize = false,
          std::optional<std::map<std::string, OrdinaryStoredAttempt>> ordinary_candidate =
              std::nullopt,
          std::optional<std::map<std::string, LegacyStoredWorkflow>> legacy_candidate =
              std::nullopt) {
        bool committed = false;
        try {
            if (blocked) deny(*blocked);
#ifdef _WIN32
            static_cast<void>(candidate);
            static_cast<void>(fault);
            static_cast<void>(initialize);
            static_cast<void>(ordinary_candidate);
            static_cast<void>(legacy_candidate);
            deny("Native realization writes unsupported: confirmed directory durability is "
                 "unavailable on Windows");
#else
            validate_location();
            if (!initialize) verify_unchanged();
            auto candidate_ordinary =
                ordinary_candidate ? std::move(*ordinary_candidate) : ordinary_attempts;
            auto candidate_legacy =
                legacy_candidate ? std::move(*legacy_candidate) : legacy_workflows;
            auto encoded =
                ledger_json(ns, candidate, candidate_ordinary, candidate_legacy).dump(2) + "\n";
            // Validate the actual text/private candidate before touching disk.
            static_cast<void>(decode(encoded, ns));
            const auto inject = [&](RealizationStoreIoPhase phase) {
                if (fault && fault(phase)) deny("Injected realization ledger I/O failure");
            };
            inject(RealizationStoreIoPhase::CreateTemporary);
            static std::atomic<std::uint64_t> nonce{};
            std::string temporary;
            Descriptor file;
            for (unsigned tries = 0; tries < 64; ++tries) {
                temporary = std::string(temporary_prefix) + std::to_string(getpid()) + "-" +
                            std::to_string(nonce.fetch_add(1));
                file.value = openat(directory_fd,
                                    temporary.c_str(),
                                    O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                                    0600);
                if (file.value >= 0) break;
                if (errno != EEXIST) deny(io_error("Realization temporary creation failed"));
            }
            if (file.value < 0) deny("Realization temporary names exhausted");
            struct Cleanup {
                int fd;
                std::string name;
                ~Cleanup() {
                    if (!name.empty()) unlinkat(fd, name.c_str(), 0);
                }
            } cleanup{directory_fd, temporary};
            std::size_t offset = 0;
            bool partial_checked = false;
            while (offset < encoded.size()) {
                const auto remaining = encoded.size() - offset;
                const auto count =
                    !partial_checked ? std::min<std::size_t>(remaining, 17) : remaining;
                const auto written = ::write(file.value, encoded.data() + offset, count);
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) deny(io_error("Realization ledger write failed"));
                offset += static_cast<std::size_t>(written);
                if (!partial_checked) {
                    partial_checked = true;
                    inject(RealizationStoreIoPhase::AfterPartialWrite);
                }
            }
            inject(RealizationStoreIoPhase::FileSync);
            if (!synchronize(file.value)) deny(io_error("Realization file synchronization failed"));
            const int closed = std::exchange(file.value, -1);
            if (close(closed) != 0) deny(io_error("Realization temporary close failed"));
            inject(RealizationStoreIoPhase::Replace);
            if (renameat(directory_fd, temporary.c_str(), directory_fd, ledger_name) != 0)
                deny(io_error("Realization atomic replacement failed"));
            cleanup.name.clear();
            committed = true;
            attempts.swap(candidate);
            ordinary_attempts.swap(candidate_ordinary);
            legacy_workflows.swap(candidate_legacy);
            disk_bytes.swap(encoded);
            inject(RealizationStoreIoPhase::DirectorySync);
            if (!synchronize(directory_fd))
                deny(io_error("Realization directory synchronization failed"));
            // A replacement during file I/O also revokes this call's permit.
            validate_location();
            return {};
#endif
        } catch (const std::exception& error) {
            // Any uncertainty/conflict blocks further native writing through this handle.
            blocked = error.what();
            return std::unexpected(RealizationStoreError{error.what(), committed, false});
        }
    }
};

RealizationStoreResult<std::string> realization_note_identity(const std::vector<std::string>& keys,
                                                              const json& projection) {
    try {
        return desired_identity(keys, projection);
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
RealizationDispatchPermit::RealizationDispatchPermit(ManagedOperationReceipt prepared)
    : prepared_(std::move(prepared)) {}
RealizationDispatchPermit::RealizationDispatchPermit(RealizationDispatchPermit&& other) noexcept
    : prepared_(std::move(other.prepared_)) {
    other.prepared_.reset();
}
RealizationDispatchPermit&
RealizationDispatchPermit::operator=(RealizationDispatchPermit&& other) noexcept {
    if (this != &other) {
        prepared_ = std::move(other.prepared_);
        other.prepared_.reset();
    }
    return *this;
}
std::optional<ManagedOperationReceipt> RealizationDispatchPermit::take_prepared() noexcept {
    auto result = std::move(prepared_);
    prepared_.reset();
    return result;
}
RealizationStore::RealizationStore(std::unique_ptr<Impl> implementation)
    : impl_(std::move(implementation)) {}
RealizationStore::~RealizationStore() = default;

RealizationStoreResult<std::unique_ptr<RealizationStore>>
RealizationStore::open(const fs::path& base,
                       const std::string& ns,
                       RealizationStoreMode mode,
                       const RealizationStoreIoFault& fault) {
    bool committed = false;
    try {
#ifdef _WIN32
        static_cast<void>(fault);
#endif
        if (!hex(ns, 32)) deny("Workspace namespace must be canonical 32 lowercase hex");
        if (base.empty() ||
            base.native().find(fs::path::value_type{}) != fs::path::string_type::npos ||
            !fs::is_directory(base))
            deny("Realization base must be an existing durable directory");
        auto impl = std::make_unique<Impl>();
        impl->ns = ns;
        impl->path = base / ns;
        if (mode == RealizationStoreMode::InitializeNew) {
#ifdef _WIN32
            deny("Native realization initialization unsupported: confirmed directory durability is "
                 "unavailable on Windows");
#else
            if (fault && fault(RealizationStoreIoPhase::CreateDirectory))
                deny("Injected namespace creation failure");
            if (!fs::create_directory(impl->path))
                deny("InitializeNew refuses an existing namespace directory");
#endif
        }
        if (!fs::is_directory(fs::symlink_status(impl->path)))
            deny("Existing realization namespace directory is missing or redirected");
#ifdef _WIN32
        impl->lock = CreateFileW((impl->path / ".lock").c_str(),
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,
                                 nullptr,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 nullptr);
        if (impl->lock == INVALID_HANDLE_VALUE)
            deny("Realization namespace is locked or its lock history is missing");
        impl->disk_bytes = impl->read();
        impl->attempts =
            decode(impl->disk_bytes, ns, &impl->ordinary_attempts, &impl->legacy_workflows);
        impl->blocked = "Native realization writes unsupported: confirmed directory durability is "
                        "unavailable on Windows";
#else
        impl->directory_fd =
            ::open(impl->path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (impl->directory_fd < 0) deny(io_error("Realization namespace open failed"));
        const auto lock_flags =
            O_RDWR | O_CLOEXEC | O_NOFOLLOW |
            (mode == RealizationStoreMode::InitializeNew ? O_CREAT | O_EXCL : 0);
        impl->lock_fd = openat(impl->directory_fd, ".lock", lock_flags, 0600);
        if (impl->lock_fd < 0) deny(io_error("Realization lock history is missing or unavailable"));
        if (flock(impl->lock_fd, LOCK_EX | LOCK_NB) != 0)
            deny(io_error("Realization namespace already has a writer"));
        if (mode == RealizationStoreMode::InitializeNew) {
            if (!synchronize(impl->lock_fd))
                deny(io_error("Realization lock synchronization failed"));
            const auto initialized = impl->write({}, fault, true);
            if (!initialized) return std::unexpected(initialized.error());
            committed = true;
        } else {
            impl->disk_bytes = impl->read();
            impl->attempts =
                decode(impl->disk_bytes, ns, &impl->ordinary_attempts, &impl->legacy_workflows);
        }
        // A warm reopen must not bypass a prior namespace-parent sync failure.
        if (fault && fault(RealizationStoreIoPhase::ParentDirectorySync))
            deny("Injected namespace parent synchronization failure");
        Descriptor parent{::open(base.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
        if (parent.value < 0 || !synchronize(parent.value))
            deny(io_error("Realization namespace parent synchronization failed"));
#endif
        return std::unique_ptr<RealizationStore>(new RealizationStore(std::move(impl)));
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what(), committed, false});
    }
}
const std::string& RealizationStore::workspace_namespace() const noexcept {
    return impl_->ns;
}
const fs::path& RealizationStore::directory() const noexcept {
    return impl_->path;
}
const std::map<std::string, RealizationStoredAttempt>& RealizationStore::attempts() const noexcept {
    return impl_->attempts;
}
const RealizationStoredAttempt* RealizationStore::find(const std::string& id) const noexcept {
    const auto found = impl_->attempts.find(id);
    return found == impl_->attempts.end() ? nullptr : &found->second;
}
const std::map<std::string, OrdinaryStoredAttempt>&
RealizationStore::ordinary_attempts() const noexcept {
    return impl_->ordinary_attempts;
}
const OrdinaryStoredAttempt* RealizationStore::find_ordinary(const std::string& id) const noexcept {
    const auto found = impl_->ordinary_attempts.find(id);
    return found == impl_->ordinary_attempts.end() ? nullptr : &found->second;
}
bool RealizationStore::native_writes_available() const noexcept {
    return !impl_->blocked && !has_unresolved_legacy_workflow();
}
const std::optional<std::string>& RealizationStore::blocked_reason() const noexcept {
    return impl_->blocked;
}
RealizationStoreResult<std::string> RealizationStore::new_attempt_id() const {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        std::random_device entropy;
        constexpr std::string_view digits = "0123456789abcdef";
        for (unsigned retry = 0; retry < 64; ++retry) {
            std::string value;
            value.reserve(32);
            for (unsigned i = 0; i < 16; ++i) {
                const auto byte = static_cast<unsigned>(entropy()) & 255U;
                value.push_back(digits[byte >> 4]);
                value.push_back(digits[byte & 15]);
            }
            if (!impl_->identity_exists(value)) return value;
        }
        deny("Opaque realization attempt-ID collisions exhausted retry bound");
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
RealizationStoreResult<RealizationDispatchPermit>
RealizationStore::fence(const RealizationAttemptIntent& intent,
                        const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        if (has_unresolved_legacy_workflow())
            deny("Unresolved native workflow blocks fresh musical writes");
        if (impl_->identity_exists(intent.attempt_id))
            deny("Realization attempt already exists; disk-restored fences are query-only");
        if (impl_->identity_count() >= REALIZATION_STORE_MAX_ATTEMPTS)
            deny("Realization attempt capacity exhausted");
        validate_intent(intent, impl_->ns);
        auto candidate = impl_->attempts;
        candidate.emplace(intent.attempt_id,
                          RealizationStoredAttempt{intent, impl_->attempts.size() + 1, {}, {}});
        // Prepare the one-shot value before any file publication; allocation can fail here.
        RealizationDispatchPermit permit(intent.prepared);
        const auto written = impl_->write(std::move(candidate), fault);
        if (!written) return std::unexpected(written.error());
        return permit;
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
RealizationStoreResult<void>
RealizationStore::append_evidence(const std::string& id,
                                  const ManagedOperationReceipt& evidence,
                                  const std::optional<ManagedBindingReceipt>& binding,
                                  const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        const auto found = impl_->attempts.find(id);
        if (found == impl_->attempts.end()) deny("Realization attempt does not exist");
        validate_evidence(found->second.intent, evidence);
        auto candidate = impl_->attempts;
        auto& attempt = candidate.at(id);
        const auto encoded = managed_receipt_to_json(evidence).dump();
        bool changed = false;
        if (attempt.evidence.empty() ||
            managed_receipt_to_json(attempt.evidence.back()).dump() != encoded) {
            if (attempt.evidence.size() >= REALIZATION_STORE_MAX_EVIDENCE)
                deny("Realization evidence capacity exhausted");
            attempt.evidence.push_back(evidence);
            try {
                validate_terminal_history(attempt);
            } catch (const std::exception& error) {
                impl_->blocked = error.what();
                throw;
            }
            changed = true;
        }
        if (binding) {
            validate_binding(attempt, *binding);
            const auto binding_bytes = managed_binding_to_json(*binding).dump();
            const bool known = std::ranges::any_of(attempt.bindings, [&](const auto& existing) {
                return managed_binding_to_json(existing).dump() == binding_bytes;
            });
            if (!known) {
                if (attempt.bindings.size() >= REALIZATION_STORE_MAX_EVIDENCE)
                    deny("Realization binding capacity exhausted");
                attempt.bindings.push_back(*binding);
                changed = true;
            }
        }
        if (!changed) {
            impl_->verify_unchanged();
            return {};
        }
        return impl_->write(std::move(candidate), fault);
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
OrdinaryDispatchPermit::OrdinaryDispatchPermit(OrdinaryClipReceipt receipt)
    : prepared_(std::move(receipt)) {}
OrdinaryDispatchPermit::OrdinaryDispatchPermit(OrdinaryDispatchPermit&& other) noexcept
    : prepared_(std::move(other.prepared_)) {
    other.prepared_.reset();
}
OrdinaryDispatchPermit& OrdinaryDispatchPermit::operator=(OrdinaryDispatchPermit&& other) noexcept {
    if (this != &other) {
        prepared_ = std::move(other.prepared_);
        other.prepared_.reset();
    }
    return *this;
}
std::optional<OrdinaryClipReceipt> OrdinaryDispatchPermit::take_prepared() noexcept {
    auto result = std::move(prepared_);
    prepared_.reset();
    return result;
}
RealizationStoreResult<OrdinaryDispatchPermit>
RealizationStore::fence_ordinary(const OrdinaryClipReceipt& prepared,
                                 const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        const auto id = prepared.intent.at("operation_id").get<std::string>();
        if (has_unresolved_legacy_workflow())
            deny("Unresolved native workflow blocks fresh musical writes");
        if (impl_->identity_exists(id))
            deny("Ordinary attempt already fenced; restored records are query-only");
        if (impl_->identity_count() >= REALIZATION_STORE_MAX_ATTEMPTS)
            deny("Realization attempt capacity exhausted");
        OrdinaryStoredAttempt attempt{prepared, impl_->ordinary_attempts.size() + 1, {}};
        validate_ordinary_history(attempt);
        auto candidate = impl_->ordinary_attempts;
        candidate.emplace(id, std::move(attempt));
        OrdinaryDispatchPermit permit(prepared);
        const auto written = impl_->write(impl_->attempts, fault, false, std::move(candidate));
        if (!written) return std::unexpected(written.error());
        return permit;
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
RealizationStoreResult<void>
RealizationStore::append_ordinary_evidence(const std::string& id,
                                           const OrdinaryClipReceipt& evidence,
                                           const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        if (!impl_->ordinary_attempts.contains(id)) deny("Ordinary attempt does not exist");
        auto candidate = impl_->ordinary_attempts;
        auto& attempt = candidate.at(id);
        if (!attempt.evidence.empty() && ordinary_receipt_to_json(attempt.evidence.back()) ==
                                             ordinary_receipt_to_json(evidence)) {
            impl_->verify_unchanged();
            return {};
        }
        if (attempt.evidence.size() >= REALIZATION_STORE_MAX_EVIDENCE)
            deny("Ordinary evidence capacity exhausted");
        attempt.evidence.push_back(evidence);
        try {
            validate_ordinary_history(attempt);
        } catch (const std::exception& error) {
            impl_->blocked = error.what();
            throw;
        }
        return impl_->write(impl_->attempts, fault, false, std::move(candidate));
    } catch (const std::exception& error) {
        return std::unexpected(RealizationStoreError{error.what()});
    }
}
const std::map<std::string, LegacyStoredWorkflow>&
RealizationStore::legacy_workflows() const noexcept {
    return impl_->legacy_workflows;
}
bool RealizationStore::has_unresolved_legacy_workflow() const noexcept {
    return std::ranges::any_of(impl_->legacy_workflows, [](const auto& entry) {
        return entry.second.state == "unresolved";
    });
}
RealizationStoreResult<void>
RealizationStore::fence_legacy_workflow(const std::string& id,
                                        const LegacyWorkflowRecipe& recipe,
                                        const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        if (has_unresolved_legacy_workflow())
            deny("An unresolved native workflow blocks fresh musical writes; inspect "
                 "original-token history");
        if (impl_->identity_exists(id) || !hex(id, 32) ||
            impl_->legacy_workflows.size() >= REALIZATION_STORE_MAX_ATTEMPTS)
            deny("Legacy workflow identity/capacity unavailable");
        LegacyStoredWorkflow w;
        w.workflow_id = id;
        w.recipe = recipe;
        validate_workflow(w, impl_->ns);
        auto candidate = impl_->legacy_workflows;
        candidate.emplace(id, std::move(w));
        // Allocate the in-process capability before durable publication. Reopens never fill it.
        impl_->fresh_workflows.insert(id);
        const auto written =
            impl_->write(impl_->attempts, fault, false, std::nullopt, std::move(candidate));
        if (!written) {
            impl_->fresh_workflows.erase(id);
            return std::unexpected(written.error());
        }
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(RealizationStoreError{e.what()});
    }
}
RealizationStoreResult<LegacyDispatchPermit> RealizationStore::fence_legacy_child(
    const std::string& id, const LegacyOperationReceipt& r, const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        if (!impl_->fresh_workflows.contains(id) || !impl_->legacy_workflows.contains(id))
            deny("Restored workflows are query-only and cannot issue dispatch permits");
        const auto operation = r.intent.at("operation_id").get<std::string>();
        if (impl_->identity_exists(operation) ||
            impl_->identity_count() >= REALIZATION_STORE_MAX_ATTEMPTS)
            deny("Legacy child identity/capacity unavailable");
        auto candidate = impl_->legacy_workflows;
        auto& w = candidate.at(id);
        if (w.state != "unresolved") deny("Finalized workflow cannot dispatch");
        w.children.push_back({r, {}});
        validate_workflow(w, impl_->ns);
        LegacyDispatchPermit permit(r);
        const auto written =
            impl_->write(impl_->attempts, fault, false, std::nullopt, std::move(candidate));
        if (!written) return std::unexpected(written.error());
        return permit;
    } catch (const std::exception& e) {
        return std::unexpected(RealizationStoreError{e.what()});
    }
}
RealizationStoreResult<void> RealizationStore::append_legacy_evidence(
    const std::string& id, const LegacyOperationReceipt& r, const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        auto candidate = impl_->legacy_workflows;
        auto& w = candidate.at(id);
        const auto ordinal = r.intent.at("ordinal").get<unsigned>();
        if (!ordinal || ordinal > w.children.size()) deny("Original legacy child is absent");
        auto& evidence = w.children.at(ordinal - 1).evidence;
        if (!evidence.empty() &&
            legacy_receipt_to_json(evidence.back()) == legacy_receipt_to_json(r)) {
            impl_->verify_unchanged();
            return {};
        }
        evidence.push_back(r);
        validate_workflow(w, impl_->ns);
        return impl_->write(impl_->attempts, fault, false, std::nullopt, std::move(candidate));
    } catch (const std::exception& e) {
        return std::unexpected(RealizationStoreError{e.what()});
    }
}
RealizationStoreResult<void> RealizationStore::finalize_legacy_workflow(
    const std::string& id, bool completed, const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        auto candidate = impl_->legacy_workflows;
        auto& w = candidate.at(id);
        if (w.state != "unresolved") deny("Workflow already finalized");
        if (completed)
            w.state = "completed";
        else if (std::ranges::all_of(w.children, child_no_effect))
            w.state = "not_applied";
        // Uncertain/known partial prefixes remain unresolved, even if all observed children acked.
        validate_workflow(w, impl_->ns);
        auto result =
            impl_->write(impl_->attempts, fault, false, std::nullopt, std::move(candidate));
        impl_->fresh_workflows.erase(id);
        return result;
    } catch (const std::exception& e) {
        return std::unexpected(RealizationStoreError{e.what()});
    }
}
RealizationStoreResult<void> RealizationStore::dispose_legacy_workflow(
    const std::string& id, const std::string& retained, const RealizationStoreIoFault& fault) {
    try {
        if (impl_->blocked) deny(*impl_->blocked);
        auto candidate = impl_->legacy_workflows;
        auto& w = candidate.at(id);
        if (w.state != "unresolved")
            deny("Only unresolved original workflows accept explicit disposition");
        w.state = "disposed";
        w.disposition = retained;
        validate_workflow(w, impl_->ns);
        auto result =
            impl_->write(impl_->attempts, fault, false, std::nullopt, std::move(candidate));
        impl_->fresh_workflows.erase(id);
        return result;
    } catch (const std::exception& e) {
        return std::unexpected(RealizationStoreError{e.what()});
    }
}

} // namespace sunny::infrastructure
