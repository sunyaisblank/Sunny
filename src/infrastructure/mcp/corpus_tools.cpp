/**
 * @file corpus_tools.cpp
 * @brief MCP Tool Registration — Corpus IR implementation
 *
 *
 * Maps MCP tool calls to Corpus IR workflow functions.
 * Corpus state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "evidence_encoding.hpp"

#include <array>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/serialization_primitives.hpp>
#include <sunny/infrastructure/compilation_workflows.hpp>
#include <sunny/infrastructure/corpus/ingestion.hpp>
#include <sunny/infrastructure/mcp/corpus_tools.hpp>
#include <sunny/infrastructure/mcp/session_ids.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json corpus_allocation_error(const std::string& kind, ErrorCode code) {
    return {{"error",
             kind + (code == ErrorCode::ArithmeticOverflow
                         ? " identity counter is exhausted or invalid"
                         : " identity counter does not exceed existing identities")},
            {"error_code", static_cast<int>(code)}};
}

json ingestion_error_response(const std::string& format, ErrorCode code) {
    std::string message;
    switch (code) {
    case ErrorCode::InvalidMidiFile:
        message = "Invalid MIDI file";
        break;
    case ErrorCode::InvalidMusicXml:
        message = "Invalid MusicXML document";
        break;
    case ErrorCode::InvalidMidiPPQ:
        message = "Invalid MIDI pulse resolution";
        break;
    case ErrorCode::InvalidMidiTempo:
        message = "Invalid MIDI tempo";
        break;
    case ErrorCode::InvalidMidiTimeSig:
        message = "Invalid MIDI time signature";
        break;
    case ErrorCode::InvalidTimeSignature:
        message = "Invalid time signature";
        break;
    case ErrorCode::InvalidBeat:
        message = "Invalid beat value";
        break;
    case ErrorCode::ArithmeticOverflow:
        message = "Exact musical arithmetic overflow";
        break;
    case ErrorCode::DocumentStructure:
        message = "Invalid Score document structure";
        break;
    case ErrorCode::MeasureCountMismatch:
        message = "Score measure counts disagree";
        break;
    case ErrorCode::InvalidOffset:
        message = "Invalid event offset";
        break;
    case ErrorCode::OverlappingEvents:
        message = "Overlapping events in one Score voice";
        break;
    case ErrorCode::TieMismatch:
        message = "Invalid Score tie chain";
        break;
    case ErrorCode::MeasureFillError:
        message = "Score voice does not fill its measure";
        break;
    case ErrorCode::TupletSpanError:
        message = "Invalid Score tuplet span";
        break;
    case ErrorCode::InvalidScoreTime:
        message = "Invalid Score time";
        break;
    case ErrorCode::InvalidMutation:
        message = "Input cannot be represented by the core model";
        break;
    case ErrorCode::ScoreValidationFailed:
        message = "Ingested Score failed structural validation";
        break;
    case ErrorCode::IngestionFailed:
        message = "Source parsing or Score projection failed";
        break;
    case ErrorCode::AnalysisFailed:
        message = "Corpus analysis failed";
        break;
    case ErrorCode::CorpusInvalidParameter:
        message = "Invalid corpus ingestion parameter";
        break;
    case ErrorCode::CorpusNotFound:
        message = "Corpus work or composer not found";
        break;
    case ErrorCode::CorpusDuplicateId:
        message = "Corpus identity already exists";
        break;
    default:
        message = "Core ingestion or analysis rejected the input";
        break;
    }
    return {{"error", format + " ingestion failed"},
            {"error_code", static_cast<int>(code)},
            {"message", std::move(message)}};
}

json ok_response() {
    return {{"ok", true}};
}

constexpr std::array<const char*, 9> analysis_domains = {"harmonic",
                                                         "melodic",
                                                         "rhythmic",
                                                         "formal",
                                                         "voice_leading",
                                                         "textural",
                                                         "dynamic",
                                                         "orchestration",
                                                         "motivic"};

json analysis_availability(const IngestedWork& work) {
    json result = json::object();
    for (const auto* domain : analysis_domains) {
        const auto found = work.analysis.evidence.find(domain);
        const bool qualified = work.analysis_complete && found != work.analysis.evidence.end() &&
                               (found->second.kind == AnalysisEvidenceKind::ExactSymbolic ||
                                found->second.kind == AnalysisEvidenceKind::Heuristic);
        const bool unavailable = found != work.analysis.evidence.end() &&
                                 found->second.kind == AnalysisEvidenceKind::Unavailable;
        result[domain] = {{"available", qualified},
                          {"status",
                           qualified     ? "computed"
                           : unavailable ? "unavailable"
                                         : "unqualified"},
                          {"unavailable_fields",
                           found == work.analysis.evidence.end()
                               ? std::vector<std::string>{}
                               : found->second.unavailable_fields}};
    }
    return result;
}

json profile_availability(const CorpusDatabase& corpus, const ComposerProfile& composer) {
    json result = json::object();
    for (const auto* domain : analysis_domains) {
        std::uint64_t computed = 0, unavailable = 0, unqualified = 0;
        std::set<std::string> methods;
        std::set<std::uint64_t> seen;
        for (const auto id : composer.works) {
            if (!seen.insert(id.value).second) continue;
            const auto found = corpus.works.find(id.value);
            if (found == corpus.works.end() || !found->second.analysis_complete) continue;
            const auto evidence = found->second.analysis.evidence.find(domain);
            if (evidence == found->second.analysis.evidence.end() ||
                evidence->second.kind == AnalysisEvidenceKind::Unqualified) {
                ++unqualified;
            } else if (evidence->second.kind == AnalysisEvidenceKind::Unavailable) {
                ++unavailable;
            } else {
                ++computed;
                methods.insert(evidence->second.method);
            }
        }
        result[domain] = {{"computed_works", computed},
                          {"unavailable_works", unavailable},
                          {"unqualified_works", unqualified},
                          {"methods", methods},
                          {"fully_qualified", computed > 0 && unqualified == 0},
                          {"fields", json::object()}};
    }
    for (const auto field : style_profile_fields()) {
        std::uint64_t computed = 0, unavailable = 0, unqualified = 0;
        std::set<std::string> methods;
        std::set<std::uint64_t> seen;
        for (const auto id : composer.works) {
            if (!seen.insert(id.value).second) continue;
            const auto found = corpus.works.find(id.value);
            if (found == corpus.works.end() || !found->second.analysis_complete) continue;
            const auto kind =
                style_profile_field_evidence(found->second.analysis, field.domain, field.name);
            if (kind == AnalysisEvidenceKind::Unavailable)
                ++unavailable;
            else if (kind == AnalysisEvidenceKind::Unqualified)
                ++unqualified;
            else
                ++computed;
            if (kind != AnalysisEvidenceKind::Unavailable) {
                for (const auto source : style_profile_field_sources(field.domain, field.name)) {
                    const auto evidence =
                        found->second.analysis.evidence.find(std::string{source.domain});
                    if (evidence != found->second.analysis.evidence.end() &&
                        !evidence->second.method.empty())
                        methods.insert(evidence->second.method);
                }
            }
        }
        json sources = json::array();
        for (const auto source : style_profile_field_sources(field.domain, field.name))
            sources.push_back({{"domain", source.domain}, {"path", source.path}});
        result[std::string{field.domain}]["fields"][std::string{field.name}] = {
            {"computed_works", computed},
            {"unavailable_works", unavailable},
            {"unqualified_works", unqualified},
            {"contributing_works", computed + unqualified},
            {"available", computed + unqualified > 0},
            {"status",
             computed > 0      ? (unqualified > 0 ? "mixed" : "computed")
             : unqualified > 0 ? "unqualified"
                               : "unavailable"},
            {"fully_qualified", computed > 0 && unqualified == 0},
            {"methods", methods},
            {"source_fields", std::move(sources)}};
    }
    return result;
}

/// Serialise an AnnotatedExample to JSON
json example_j(const AnnotatedExample& e) {
    json j = {{"relevance_score", e.relevance_score},
              {"analysis_summary", e.analysis_summary},
              {"formal_context", e.formal_context}};
    j["work_id"] = e.work_id.value;
    j["region_start"] = score_time_to_json(e.region_start);
    j["region_end"] = score_time_to_json(e.region_end);
    if (!e.harmonic_reduction.empty()) {
        j["harmonic_reduction"] = e.harmonic_reduction;
    }
    return j;
}

/// Serialise a StyleComparison to JSON
json comparison_j(const StyleComparison& sc) {
    return {{"composer_a", sc.composer_a.value},
            {"composer_b", sc.composer_b.value},
            {"harmonic_divergence", sc.harmonic_divergence},
            {"melodic_divergence", sc.melodic_divergence},
            {"rhythmic_divergence", sc.rhythmic_divergence},
            {"formal_divergence", sc.formal_divergence},
            {"most_similar_dimensions", sc.most_similar_dimensions},
            {"most_divergent_dimensions", sc.most_divergent_dimensions}};
}

/// Serialise an EvolutionaryAnalysis to JSON
json evolution_j(const EvolutionaryAnalysis& ea) {
    json trends = json::array();
    for (const auto& t : ea.trends) {
        trends.push_back({{"dimension", t.dimension},
                          {"direction", static_cast<int>(t.direction)},
                          {"early_value", t.early_value},
                          {"late_value", t.late_value},
                          {"description", t.description}});
    }
    return {
        {"composer", ea.composer.value}, {"period_count", ea.periods.size()}, {"trends", trends}};
}

/// Convert FormClassification from integer
std::optional<FormClassification> form_from_int(int val) {
    if (val < 0 || val > 20) return std::nullopt;
    return static_cast<FormClassification>(val);
}

bool read_optional_year(const json& params,
                        std::string_view key,
                        std::optional<std::uint16_t>& output,
                        std::string& error) {
    if (!params.contains(key)) return true;

    const auto& value = params.at(key);
    std::uint64_t year = 0;
    if (value.is_number_unsigned()) {
        year = value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        const auto signed_year = value.get<std::int64_t>();
        if (signed_year < 0) {
            error = std::string(key) + " must be between 0 and 65535";
            return false;
        }
        year = static_cast<std::uint64_t>(signed_year);
    } else {
        error = std::string(key) + " must be an integer";
        return false;
    }

    if (year > std::numeric_limits<std::uint16_t>::max()) {
        error = std::string(key) + " must be between 0 and 65535";
        return false;
    }
    output = static_cast<std::uint16_t>(year);
    return true;
}

bool read_required_year(const json& params,
                        std::string_view key,
                        std::uint16_t& output,
                        std::string& error) {
    if (!params.contains(key)) {
        error = std::string(key) + " is required";
        return false;
    }
    std::optional<std::uint16_t> parsed;
    if (!read_optional_year(params, key, parsed, error)) return false;
    output = *parsed;
    return true;
}

/// Strictly decode a base64 string to raw bytes.
std::optional<std::vector<std::uint8_t>> decode_base64(const std::string& input) {
    static constexpr unsigned char table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
        64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 65, 64, 64, 64, 0,
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};
    std::string encoded;
    encoded.reserve(input.size());
    for (unsigned char c : input) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (table[c] == 64) return std::nullopt;
        encoded.push_back(static_cast<char>(c));
    }
    if (encoded.empty() || encoded.size() % 4 != 0) return std::nullopt;

    std::vector<std::uint8_t> out;
    out.reserve(encoded.size() / 4 * 3);
    for (std::size_t offset = 0; offset < encoded.size(); offset += 4) {
        const bool final = offset + 4 == encoded.size();
        const unsigned char c0 = static_cast<unsigned char>(encoded[offset]);
        const unsigned char c1 = static_cast<unsigned char>(encoded[offset + 1]);
        const unsigned char c2 = static_cast<unsigned char>(encoded[offset + 2]);
        const unsigned char c3 = static_cast<unsigned char>(encoded[offset + 3]);
        if (table[c0] >= 64 || table[c1] >= 64) return std::nullopt;

        const bool pad2 = c2 == '=';
        const bool pad3 = c3 == '=';
        if ((pad2 || pad3) && !final) return std::nullopt;
        if (pad2 && !pad3) return std::nullopt;
        if (!pad2 && table[c2] >= 64) return std::nullopt;
        if (!pad3 && table[c3] >= 64) return std::nullopt;

        const auto v0 = table[c0];
        const auto v1 = table[c1];
        const auto v2 = pad2 ? 0 : table[c2];
        const auto v3 = pad3 ? 0 : table[c3];
        if (pad2 && (v1 & 0x0F) != 0) return std::nullopt;
        if (pad3 && !pad2 && (v2 & 0x03) != 0) return std::nullopt;

        const std::uint32_t value =
            (static_cast<std::uint32_t>(v0) << 18) | (static_cast<std::uint32_t>(v1) << 12) |
            (static_cast<std::uint32_t>(v2) << 6) | static_cast<std::uint32_t>(v3);
        out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
        if (!pad2) out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
        if (!pad3) out.push_back(static_cast<std::uint8_t>(value & 0xFF));
        if (pad2 || pad3) {
            if (!final) return std::nullopt;
        }
    }
    return out;
}

} // anonymous namespace

void register_corpus_tools(McpServer& server, std::shared_ptr<CorpusSession> session) {
    if (!session) session = std::make_shared<CorpusSession>();

    // =========================================================================
    // Profile Management
    // =========================================================================

    server.register_tool(
        "create_composer_profile",
        "Create a new composer profile in the corpus",
        {{"name", "string"},
         {"birth_year", "integer 0..65535 (optional)"},
         {"death_year", "integer 0..65535 (optional)"},
         {"tradition", "string (optional)"}},
        [session](const json& params) -> json {
            auto name = params.value("name", "");
            if (name.empty()) return error_response("name is required");

            std::optional<std::uint16_t> birth_year;
            std::optional<std::uint16_t> death_year;
            std::string year_error;
            if (!read_optional_year(params, "birth_year", birth_year, year_error) ||
                !read_optional_year(params, "death_year", death_year, year_error))
                return error_response(year_error);
            if (birth_year && death_year && *birth_year > *death_year)
                return error_response("birth_year must not exceed death_year");

            const auto allocation = mcp_detail::checked_session_id_batch(
                session->next_composer_id,
                mcp_detail::maximum_session_store_id(session->corpus.composers));
            if (!allocation) return corpus_allocation_error("Composer", allocation.error());
            auto id = ComposerProfileId{allocation->first};
            auto profile = create_composer_profile(id, name);

            profile.birth_year = birth_year;
            profile.death_year = death_year;
            if (params.contains("tradition"))
                profile.tradition = params["tradition"].get<std::string>();

            json response = {{"composer_id", id.value}, {"name", name}};
            (void)response.dump();
            (void)composer_profile_to_json(profile).dump();
            const auto [position, inserted] =
                session->corpus.composers.emplace(id.value, std::move(profile));
            (void)position;
            if (!inserted)
                return corpus_allocation_error("Composer", ErrorCode::InvariantViolation);
            session->next_composer_id = allocation->next;
            return response;
        });

    server.register_tool(
        "create_ingested_work",
        "Create a new ingested work entry in the corpus",
        {{"title", "string"},
         {"source_format", "string"},
         {"composer_id", "integer (optional)"},
         {"opus", "string (optional)"},
         {"year_composed", "integer 0..65535 (optional)"},
         {"instrumentation", "string (optional)"}},
        [session](const json& params) -> json {
            auto title = params.value("title", "");
            auto source_format = params.value("source_format", "");
            if (title.empty()) return error_response("title is required");
            if (source_format.empty()) return error_response("source_format is required");

            std::optional<std::uint16_t> year_composed;
            std::string year_error;
            if (!read_optional_year(params, "year_composed", year_composed, year_error))
                return error_response(year_error);

            std::optional<ComposerProfileId> composer_id;
            if (params.contains("composer_id")) {
                auto cid = ComposerProfileId{
                    detail::checked_integer<std::uint64_t>(params["composer_id"], "composer id")};
                if (!session->corpus.composers.contains(cid.value))
                    return error_response("composer not found");
                composer_id = cid;
            }

            WorkMetadata meta;
            meta.title = title;
            meta.source_format = source_format;
            if (params.contains("opus")) meta.opus = params["opus"].get<std::string>();
            meta.year_composed = year_composed;
            if (params.contains("instrumentation"))
                meta.instrumentation = params["instrumentation"].get<std::string>();

            const auto allocation = mcp_detail::checked_session_id_batch(
                session->next_work_id, mcp_detail::maximum_session_store_id(session->corpus.works));
            if (!allocation) return corpus_allocation_error("Work", allocation.error());
            auto id = IngestedWorkId{allocation->first};
            auto work = create_ingested_work(id, meta);
            auto candidate = session->corpus;
            if (!candidate.works.emplace(id.value, std::move(work)).second)
                return corpus_allocation_error("Work", ErrorCode::InvariantViolation);

            // Auto-assign to composer if provided
            if (composer_id) {
                auto r = assign_work_to_composer(candidate, id, *composer_id);
                if (!r) return error_response("failed to assign to composer");
            }
            json response = {{"work_id", id.value}, {"title", title}};
            (void)response.dump();
            (void)corpus_to_json(candidate).dump();
            std::swap(session->corpus, candidate);
            session->next_work_id = allocation->next;
            return response;
        });

    server.register_tool(
        "assign_work_to_composer",
        "Associate an ingested work with a composer profile",
        {{"work_id", "integer"}, {"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{
                detail::checked_integer_or<std::uint64_t>(params, "work_id", 0, "work id")};
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};

            auto r = assign_work_to_composer(session->corpus, wid, cid);
            if (!r) return error_response("assignment failed: work or composer not found");
            return ok_response();
        });

    server.register_tool(
        "remove_ingested_work",
        "Remove an ingested work and all composer/period references",
        {{"work_id", "integer"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{
                detail::checked_integer_or<std::uint64_t>(params, "work_id", 0, "work id")};
            auto r = remove_ingested_work(session->corpus, wid);
            if (!r) return error_response("work not found: " + std::to_string(wid.value));
            return ok_response();
        });

    server.register_tool(
        "assign_work_to_period",
        "Assign a work to a compositional period within a composer's profile",
        {{"work_id", "integer"}, {"composer_id", "integer"}, {"period_label", "string"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{
                detail::checked_integer_or<std::uint64_t>(params, "work_id", 0, "work id")};
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            auto label = params.value("period_label", "");
            if (label.empty()) return error_response("period_label is required");

            auto r = assign_work_to_period(session->corpus, wid, cid, label);
            if (!r) return error_response("assignment failed: work, composer, or period not found");
            return ok_response();
        });

    server.register_tool(
        "add_period_profile",
        "Add a period profile to a composer",
        {{"composer_id", "integer"},
         {"label", "string"},
         {"year_start", "integer 0..65535"},
         {"year_end", "integer 0..65535"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            auto label = params.value("label", "");
            if (label.empty()) return error_response("label is required");

            std::uint16_t year_start = 0;
            std::uint16_t year_end = 0;
            std::string year_error;
            if (!read_required_year(params, "year_start", year_start, year_error) ||
                !read_required_year(params, "year_end", year_end, year_error))
                return error_response(year_error);
            if (year_start > year_end) return error_response("year_start must not exceed year_end");

            PeriodProfile pp;
            pp.label = label;
            pp.year_start = year_start;
            pp.year_end = year_end;

            auto r = add_period_profile(session->corpus, cid, pp);
            if (!r) {
                if (r.error() == ErrorCode::CorpusNotFound)
                    return error_response("composer not found");
                if (r.error() == ErrorCode::CorpusDuplicateId)
                    return error_response("period label already exists");
                return error_response("period range is invalid or overlaps an existing period");
            }
            return ok_response();
        });

    server.register_tool(
        "set_work_metadata",
        "Transactionally update work metadata; period names an owned profile and an empty value "
        "clears membership",
        {{"work_id", "integer"},
         {"title", "string (optional)"},
         {"opus", "string (optional)"},
         {"year_composed", "integer 0..65535 (optional)"},
         {"instrumentation", "string (optional)"},
         {"period", "string (optional)"},
         {"genre", "string (optional)"}},
        [session](const json& params) -> json {
            auto wid = detail::checked_integer_or<std::uint64_t>(params, "work_id", 0, "work id");
            if (!session->corpus.works.contains(wid))
                return error_response("work not found: " + std::to_string(wid));

            // Stage the whole mutation so an invalid period never commits the
            // otherwise-valid metadata fields supplied in the same call.
            auto candidate = session->corpus;
            auto& work = candidate.works.at(wid);
            auto m = work.metadata;
            std::optional<std::uint16_t> year_composed;
            std::string year_error;
            if (!read_optional_year(params, "year_composed", year_composed, year_error))
                return error_response(year_error);
            if (params.contains("title")) m.title = params["title"].get<std::string>();
            if (params.contains("opus")) m.opus = params["opus"].get<std::string>();
            if (params.contains("year_composed")) m.year_composed = year_composed;
            if (params.contains("instrumentation"))
                m.instrumentation = params["instrumentation"].get<std::string>();
            if (params.contains("genre")) m.genre = params["genre"].get<std::string>();

            work.metadata = std::move(m);
            if (params.contains("period")) {
                const auto period = params["period"].get<std::string>();
                Result<void> period_result;
                if (period.empty()) {
                    period_result = clear_work_period_assignment(candidate, IngestedWorkId{wid});
                } else if (work.metadata.composer.value == 0) {
                    return error_response("period assignment requires a composer owner");
                } else {
                    period_result = assign_work_to_period(
                        candidate, IngestedWorkId{wid}, work.metadata.composer, period);
                }
                if (!period_result) {
                    if (period_result.error() == ErrorCode::CorpusNotFound)
                        return error_response("period assignment failed: period not found");
                    return error_response(
                        "period assignment failed: work ownership is inconsistent");
                }
            }

            session->corpus = std::move(candidate);

            return ok_response();
        });

    // =========================================================================
    // Analysis
    // =========================================================================

    server.register_tool(
        "get_work_analysis",
        "Inspect one work's complete analysis, exact thematic passages, methods and limitations",
        {{"work_id", "integer"}},
        [session](const json& params) -> json {
            const auto id = detail::checked_integer<std::uint64_t>(params.at("work_id"), "work id");
            const auto found = session->corpus.works.find(id);
            if (found == session->corpus.works.end()) return error_response("work not found");
            return {{"work_id", id},
                    {"analysis_complete", found->second.analysis_complete},
                    {"schema_version", CORPUS_IR_SCHEMA_VERSION},
                    {"availability", analysis_availability(found->second)},
                    {"analysis", work_analysis_to_json(found->second.analysis)}};
        });

    server.register_tool("analyze_work",
                         "Run analytical decomposition on an ingested work",
                         {{"work_id", "integer"}},
                         [session](const json& params) -> json {
                             auto wid = IngestedWorkId{detail::checked_integer_or<std::uint64_t>(
                                 params, "work_id", 0, "work id")};
                             // Pass embedded Score pointer when available
                             auto wit = session->corpus.works.find(wid.value);
                             if (wit == session->corpus.works.end())
                                 return error_response("analysis failed: work not found");
                             const Score* sp = wit->second.score ? &*wit->second.score : nullptr;
                             auto r = analyze_work(session->corpus, wid, sp);
                             if (!r) return error_response("analysis failed");
                             return {{"work_id", wid.value}, {"analysis_complete", true}};
                         });

    server.register_tool("rebuild_style_profile",
                         "Recompute a composer's style profile from all their analysed works",
                         {{"composer_id", "integer"}},
                         [session](const json& params) -> json {
                             auto cid = ComposerProfileId{detail::checked_integer_or<std::uint64_t>(
                                 params, "composer_id", 0, "composer id")};
                             auto r = rebuild_style_profile(session->corpus, cid);
                             if (!r) return error_response("rebuild failed: composer not found");

                             auto it = session->corpus.composers.find(cid.value);
                             return {{"composer_id", cid.value},
                                     {"sample_size", it->second.style_profile.sample_size},
                                     {"confidence", it->second.style_profile.confidence}};
                         });

    server.register_tool(
        "detect_signature_patterns",
        "Identify statistically distinctive patterns for a composer",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            SignatureDetectionEvidence evidence;
            auto r = detect_signature_patterns(session->corpus, cid, &evidence);
            if (!r) return error_response("detection failed: composer not found");

            auto it = session->corpus.composers.find(cid.value);
            json target_works = json::array();
            json baseline_works = json::array();
            for (const auto id : evidence.target_works)
                target_works.push_back(id.value);
            for (const auto id : evidence.baseline_works)
                baseline_works.push_back(id.value);
            return {{"composer_id", cid.value},
                    {"pattern_count", it->second.style_profile.signature_patterns.size()},
                    {"available", evidence.available},
                    {"method", "observed_harmonic_bigram_pooled_proportion_z"},
                    {"interpretation",
                     "descriptive contrast; overlapping windows; no calibrated significance"},
                    {"target_windows", evidence.target_windows},
                    {"baseline_windows", evidence.baseline_windows},
                    {"target_works", target_works},
                    {"baseline_works", baseline_works},
                    {"unavailable_reason",
                     evidence.available ? json(nullptr)
                                        : json("Analysed harmonic bigram observations are required "
                                               "for the target and at least one other composer")}};
        });

    // =========================================================================
    // Comparison
    // =========================================================================

    server.register_tool("compare_composers",
                         "Generate a style comparison between two composer profiles",
                         {{"composer_a", "integer"}, {"composer_b", "integer"}},
                         [session](const json& params) -> json {
                             auto a = ComposerProfileId{detail::checked_integer_or<std::uint64_t>(
                                 params, "composer_a", 0, "first composer id")};
                             auto b = ComposerProfileId{detail::checked_integer_or<std::uint64_t>(
                                 params, "composer_b", 0, "second composer id")};

                             auto r = compare_composers(session->corpus, a, b);
                             if (!r)
                                 return error_response(
                                     "comparison failed: one or both composers not found");
                             return comparison_j(*r);
                         });

    server.register_tool(
        "analyze_evolution",
        "Analyse a composer's stylistic evolution across periods",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            auto r = analyze_evolution(session->corpus, cid);
            if (!r) return error_response("evolution analysis failed: composer not found");
            return evolution_j(*r);
        });

    // =========================================================================
    // Queries
    // =========================================================================

    server.register_tool(
        "query_style_profile",
        "Get a composer's complete style profile",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            auto r = query_style_profile(session->corpus, cid);
            if (!r) return error_response("query failed: composer not found");

            const auto* sp = *r;
            return {
                {"composer_id", cid.value},
                {"sample_size", sp->sample_size},
                {"confidence", sp->confidence},
                {"confidence_interpretation",
                 "sample-size heuristic, not calibrated analytical accuracy"},
                {"profile",
                 composer_profile_to_json(session->corpus.composers.at(cid.value))
                     .at("style_profile")},
                {"availability",
                 profile_availability(session->corpus, session->corpus.composers.at(cid.value))},
                {"harmonic_vocabulary_size", sp->harmonic_profile.chord_vocabulary_size},
                {"signature_pattern_count", sp->signature_patterns.size()}};
        });

    server.register_tool("find_examples",
                         "Find passages matching analytical criteria in a composer's corpus",
                         {{"composer_id", "integer"}, {"criterion", "string"}},
                         [session](const json& params) -> json {
                             auto cid = ComposerProfileId{detail::checked_integer_or<std::uint64_t>(
                                 params, "composer_id", 0, "composer id")};
                             auto criterion = params.value("criterion", "");
                             if (criterion.empty()) return error_response("criterion is required");

                             auto examples = find_examples(session->corpus, cid, criterion);
                             json arr = json::array();
                             for (const auto& e : examples)
                                 arr.push_back(example_j(e));
                             return {{"examples", arr},
                                     {"count", examples.size()},
                                     {"method", "all_significant_tokens_in_annotated_section"},
                                     {"relevance_interpretation",
                                      "lexical match coverage, not calibrated semantic relevance"}};
                         });

    server.register_tool(
        "get_progression_examples",
        "Find examples of a specific chord progression in a composer's corpus",
        {{"composer_id", "integer"}, {"roman_numerals", "array of strings"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            if (!params.contains("roman_numerals") || !params["roman_numerals"].is_array())
                return error_response("roman_numerals array is required");

            std::vector<std::string> rn;
            for (const auto& v : params["roman_numerals"])
                rn.push_back(v.get<std::string>());

            auto examples = get_progression_examples(session->corpus, cid, rn);
            json arr = json::array();
            for (const auto& e : examples)
                arr.push_back(example_j(e));
            return {{"examples", arr}, {"count", examples.size()}};
        });

    server.register_tool(
        "get_formal_template",
        "Get typical formal proportions for a form type from a composer",
        {{"composer_id", "integer"}, {"form_type", "integer (FormClassification enum)"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            const auto ft_int =
                detail::checked_integer_or<int>(params, "form_type", -1, "form type");
            auto ft = form_from_int(ft_int);
            if (!ft) return error_response("invalid form_type: " + std::to_string(ft_int));

            auto r = get_formal_template(session->corpus, cid, *ft);
            if (!r) return error_response("query failed: composer not found");

            const auto& fp = *r;
            json j = {{"composer_id", cid.value},
                      {"preferred_form_count", fp.preferred_forms.size()},
                      {"average_work_length", fp.average_work_length},
                      {"climax_placement", fp.climax_placement}};
            if (fp.development_proportion) j["development_proportion"] = *fp.development_proportion;
            return j;
        });

    server.register_tool(
        "query_how_would_x_handle",
        "Get insights for a described compositional situation from a composer's corpus",
        {{"composer_id", "integer"}, {"situation", "string"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id")};
            auto situation = params.value("situation", "");
            if (situation.empty()) return error_response("situation is required");

            auto r = how_would_x_handle(session->corpus, cid, situation);
            if (!r) return error_response("query failed: composer not found");

            const auto& result = *r;
            json examples = json::array();
            for (const auto& e : result.relevant_examples)
                examples.push_back(example_j(e));

            json tendencies = json::array();
            for (const auto& t : result.statistical_tendencies) {
                tendencies.push_back({{"domain", t.domain},
                                      {"observation", t.observation},
                                      {"confidence", t.confidence},
                                      {"supporting_examples_count", t.supporting_examples_count}});
            }

            json patterns = json::array();
            for (const auto& p : result.signature_patterns) {
                patterns.push_back({{"id", p.id.value},
                                    {"description", p.description},
                                    {"domain", static_cast<int>(p.domain)},
                                    {"distinctiveness", p.distinctiveness}});
            }

            return {{"relevant_examples", examples},
                    {"statistical_tendencies", tendencies},
                    {"signature_patterns", patterns},
                    {"available", !result.relevant_examples.empty()},
                    {"statistics_available", !result.statistical_tendencies.empty()},
                    {"statistics_unavailable_reason",
                     !result.statistical_tendencies.empty()
                         ? json(nullptr)
                         : json("No complete finite nonnegative per-bar observations support the "
                                "matching passages")},
                    {"method", "annotated_section_lexical_match_and_matched_bar_aggregation"},
                    {"confidence_interpretation",
                     "arithmetic aggregation of supplied observations; not calibrated musical or "
                     "statistical confidence"},
                    {"unavailable_reason",
                     !result.relevant_examples.empty()
                         ? json(nullptr)
                         : json("No annotated section matches all significant query tokens; no "
                                "contextual inference is available")}};
        });

    // =========================================================================
    // Corpus-Level
    // =========================================================================

    server.register_tool("validate_corpus",
                         "Validate the entire corpus and return diagnostics",
                         {},
                         [session](const json& /*params*/) -> json {
                             auto diags = validate_corpus(session->corpus);
                             json arr = json::array();
                             for (const auto& d : diags)
                                 arr.push_back(mcp_detail::encode_diagnostic(d));

                             bool valid = true;
                             bool loadable = true;
                             for (const auto& d : diags) {
                                 if (d.severity == ValidationSeverity::Error) valid = false;
                                 if (blocks_corpus_load(d)) loadable = false;
                             }

                             // valid: no Error diagnostic of any kind. loadable:
                             // no structural error, so the saved corpus reloads.
                             return {{"valid", valid},
                                     {"loadable", loadable},
                                     {"diagnostics", arr},
                                     {"count", diags.size()}};
                         });

    server.register_tool(
        "get_corpus_json",
        "Get the full corpus database as JSON",
        {},
        [session](const json& /*params*/) -> json { return corpus_to_json(session->corpus); });

    // =========================================================================
    // Ingestion
    // =========================================================================

    server.register_tool(
        "ingest_midi",
        "Ingest a MIDI file into the corpus (base64-encoded binary)",
        {{"midi_base64", "string"},
         {"title", "string"},
         {"composer_id", "integer"},
         {"instrumentation", "string (optional)"},
         {"quantise_grid", "integer divisions per whole note, 1..65535 (optional)"}},
        [session](const json& params) -> json {
            auto b64 = params.value("midi_base64", "");
            auto title = params.value("title", "");
            auto cid_val =
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id");
            if (b64.empty()) return error_response("midi_base64 is required");
            if (title.empty()) return error_response("title is required");
            if (cid_val == 0) return error_response("composer_id is required");
            if (!session->corpus.composers.contains(cid_val))
                return error_response("composer not found");

            auto data = decode_base64(b64);
            if (!data || data->empty()) return error_response("invalid base64 data");

            sunny::infrastructure::corpus::IngestionOptions opts;
            opts.title = title;
            if (params.contains("instrumentation"))
                opts.instrumentation = params["instrumentation"].get<std::string>();
            if (params.contains("quantise_grid"))
                opts.quantise_grid =
                    detail::checked_integer<int>(params["quantise_grid"], "quantisation grid");
            if (opts.quantise_grid <= 0 || opts.quantise_grid > 65535)
                return error_response("quantise_grid must be between 1 and 65535");

            const auto allocation = mcp_detail::checked_session_id_batch(
                session->next_work_id, mcp_detail::maximum_session_store_id(session->corpus.works));
            if (!allocation) return corpus_allocation_error("Work", allocation.error());
            auto wid = IngestedWorkId{allocation->first};
            auto cid = ComposerProfileId{cid_val};
            auto candidate = session->corpus;

            auto r = ingest_midi(candidate, std::span<const std::uint8_t>(*data), wid, cid, opts);
            if (!r) return ingestion_error_response("MIDI", r.error());
            json response = {{"work_id", wid.value}, {"title", title}, {"analysis_complete", true}};
            (void)response.dump();
            (void)corpus_to_json(candidate).dump();
            std::swap(session->corpus, candidate);
            session->next_work_id = allocation->next;
            return response;
        });

    server.register_tool(
        "ingest_musicxml",
        "Ingest a MusicXML document into the corpus",
        {{"musicxml", "string"},
         {"title", "string"},
         {"composer_id", "integer"},
         {"instrumentation", "string (optional)"}},
        [session](const json& params) -> json {
            auto xml = params.value("musicxml", "");
            auto title = params.value("title", "");
            auto cid_val =
                detail::checked_integer_or<std::uint64_t>(params, "composer_id", 0, "composer id");
            if (xml.empty()) return error_response("musicxml is required");
            if (title.empty()) return error_response("title is required");
            if (cid_val == 0) return error_response("composer_id is required");
            if (!session->corpus.composers.contains(cid_val))
                return error_response("composer not found");

            sunny::infrastructure::corpus::IngestionOptions opts;
            opts.title = title;
            if (params.contains("instrumentation"))
                opts.instrumentation = params["instrumentation"].get<std::string>();

            const auto allocation = mcp_detail::checked_session_id_batch(
                session->next_work_id, mcp_detail::maximum_session_store_id(session->corpus.works));
            if (!allocation) return corpus_allocation_error("Work", allocation.error());
            auto wid = IngestedWorkId{allocation->first};
            auto cid = ComposerProfileId{cid_val};
            auto candidate = session->corpus;

            auto r = ingest_musicxml(candidate, xml, wid, cid, opts);
            if (!r) return ingestion_error_response("MusicXML", r.error());
            json response = {{"work_id", wid.value}, {"title", title}, {"analysis_complete", true}};
            (void)response.dump();
            (void)corpus_to_json(candidate).dump();
            std::swap(session->corpus, candidate);
            session->next_work_id = allocation->next;
            return response;
        });

    server.register_tool(
        "ingest_batch",
        "Ingest multiple works into the corpus",
        {{"works",
          "array of {title, data/musicxml, composer_id, format, quantise_grid?}; quantise_grid is "
          "integer divisions per whole note, 1..65535"}},
        [session](const json& params) -> json {
            if (!params.contains("works") || !params["works"].is_array())
                return error_response("works array is required");

            json results = json::array();
            for (const auto& entry : params["works"]) {
                if (!entry.is_object()) {
                    results.push_back({{"title", ""}, {"error", "work entry must be an object"}});
                    continue;
                }
                auto title = entry.value("title", "");
                auto cid_val = detail::checked_integer_or<std::uint64_t>(
                    entry, "composer_id", 0, "composer id");
                auto format = entry.value("format", "");
                if (title.empty() || cid_val == 0 || format.empty()) {
                    results.push_back({{"title", title}, {"error", "missing required fields"}});
                    continue;
                }
                if (!session->corpus.composers.contains(cid_val)) {
                    results.push_back({{"title", title}, {"error", "composer not found"}});
                    continue;
                }
                if (format != "midi" && format != "musicxml") {
                    results.push_back({{"title", title}, {"error", "unknown format: " + format}});
                    continue;
                }

                sunny::infrastructure::corpus::IngestionOptions opts;
                opts.title = title;
                if (entry.contains("instrumentation"))
                    opts.instrumentation = entry["instrumentation"].get<std::string>();
                if (entry.contains("quantise_grid"))
                    opts.quantise_grid =
                        detail::checked_integer<int>(entry["quantise_grid"], "quantisation grid");
                if (opts.quantise_grid <= 0 || opts.quantise_grid > 65535) {
                    results.push_back(
                        {{"title", title}, {"error", "quantise_grid must be between 1 and 65535"}});
                    continue;
                }

                const auto allocation = mcp_detail::checked_session_id_batch(
                    session->next_work_id,
                    mcp_detail::maximum_session_store_id(session->corpus.works));
                if (!allocation) {
                    auto failure = corpus_allocation_error("Work", allocation.error());
                    failure["title"] = title;
                    results.push_back(std::move(failure));
                    continue;
                }
                auto wid = IngestedWorkId{allocation->first};
                auto cid = ComposerProfileId{cid_val};
                auto candidate = session->corpus;

                if (format == "midi") {
                    auto b64 = entry.value("data", "");
                    auto data = decode_base64(b64);
                    if (!data || data->empty()) {
                        results.push_back({{"title", title}, {"error", "invalid base64 data"}});
                        continue;
                    }
                    auto r = ingest_midi(
                        candidate, std::span<const std::uint8_t>(*data), wid, cid, opts);
                    if (r) {
                        json response = {{"work_id", wid.value}, {"title", title}, {"ok", true}};
                        (void)response.dump();
                        (void)corpus_to_json(candidate).dump();
                        results.push_back(std::move(response));
                        std::swap(session->corpus, candidate);
                        session->next_work_id = allocation->next;
                    } else {
                        auto failure = ingestion_error_response("MIDI", r.error());
                        failure["title"] = title;
                        results.push_back(std::move(failure));
                    }
                } else if (format == "musicxml") {
                    auto xml = entry.value("musicxml", "");
                    if (xml.empty()) {
                        results.push_back({{"title", title}, {"error", "musicxml is required"}});
                        continue;
                    }
                    auto r = ingest_musicxml(candidate, xml, wid, cid, opts);
                    if (r) {
                        json response = {{"work_id", wid.value}, {"title", title}, {"ok", true}};
                        (void)response.dump();
                        (void)corpus_to_json(candidate).dump();
                        results.push_back(std::move(response));
                        std::swap(session->corpus, candidate);
                        session->next_work_id = allocation->next;
                    } else {
                        auto failure = ingestion_error_response("MusicXML", r.error());
                        failure["title"] = title;
                        results.push_back(std::move(failure));
                    }
                }
            }

            return {{"results", results}, {"count", results.size()}};
        });
}

} // namespace sunny::infrastructure
