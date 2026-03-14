/**
 * @file MCPT004A.cpp
 * @brief MCP Tool Registration — Corpus IR implementation
 *
 * Component: MCPT004A
 *
 * Maps MCP tool calls to Corpus IR workflow functions (CIWF001A).
 * Corpus state is held in a shared_ptr to a session store captured
 * by the tool handler lambdas.
 */

#include "MCPT004A.h"

#include "Corpus/CIWF001A.h"
#include "Corpus/CISZ001A.h"
#include "../Corpus/CIIN001A.h"
#include "../Application/INWF001A.h"

#include <map>
#include <memory>

namespace Sunny::Infrastructure {

using json = nlohmann::json;
using namespace Sunny::Core;

namespace {

/// Session-scoped corpus store shared across all tool handlers
struct CorpusSession {
    CorpusDatabase corpus;
    std::uint64_t next_composer_id = 1;
    std::uint64_t next_work_id = 1;
};

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json ok_response() {
    return {{"ok", true}};
}

/// Serialise a validation diagnostic to JSON
json diagnostic_j(const Diagnostic& d) {
    return {
        {"rule", d.rule},
        {"severity", d.severity == ValidationSeverity::Error ? "error" :
                     d.severity == ValidationSeverity::Warning ? "warning" : "info"},
        {"message", d.message}
    };
}

/// Serialise an AnnotatedExample to JSON
json example_j(const AnnotatedExample& e) {
    json j = {
        {"relevance_score", e.relevance_score},
        {"analysis_summary", e.analysis_summary},
        {"formal_context", e.formal_context}
    };
    j["work_id"] = e.work_id.value;
    if (!e.harmonic_reduction.empty()) {
        j["harmonic_reduction"] = e.harmonic_reduction;
    }
    return j;
}

/// Serialise a StyleComparison to JSON
json comparison_j(const StyleComparison& sc) {
    return {
        {"composer_a", sc.composer_a.value},
        {"composer_b", sc.composer_b.value},
        {"harmonic_divergence", sc.harmonic_divergence},
        {"melodic_divergence", sc.melodic_divergence},
        {"rhythmic_divergence", sc.rhythmic_divergence},
        {"formal_divergence", sc.formal_divergence},
        {"most_similar_dimensions", sc.most_similar_dimensions},
        {"most_divergent_dimensions", sc.most_divergent_dimensions}
    };
}

/// Serialise an EvolutionaryAnalysis to JSON
json evolution_j(const EvolutionaryAnalysis& ea) {
    json trends = json::array();
    for (const auto& t : ea.trends) {
        trends.push_back({
            {"dimension", t.dimension},
            {"direction", static_cast<int>(t.direction)},
            {"early_value", t.early_value},
            {"late_value", t.late_value},
            {"description", t.description}
        });
    }
    return {
        {"composer", ea.composer.value},
        {"period_count", ea.periods.size()},
        {"trends", trends}
    };
}

/// Convert FormClassification from integer
std::optional<FormClassification> form_from_int(int val) {
    if (val < 0 || val > 20) return std::nullopt;
    return static_cast<FormClassification>(val);
}

/// Decode a base64 string to raw bytes
std::vector<std::uint8_t> decode_base64(const std::string& input) {
    static constexpr unsigned char table[256] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64,65,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
    };
    std::vector<std::uint8_t> out;
    out.reserve(input.size() * 3 / 4);
    std::uint32_t val = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (table[c] >= 64) continue;
        val = (val << 6) | table[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

}  // anonymous namespace


void register_corpus_tools(McpServer& server) {
    auto session = std::make_shared<CorpusSession>();

    // =========================================================================
    // Profile Management
    // =========================================================================

    server.register_tool(
        "create_composer_profile",
        "Create a new composer profile in the corpus",
        {{"name", "string"}, {"birth_year", "integer (optional)"}, {"death_year", "integer (optional)"},
         {"tradition", "string (optional)"}},
        [session](const json& params) -> json {
            auto name = params.value("name", "");
            if (name.empty()) return error_response("name is required");

            auto id = ComposerProfileId{session->next_composer_id++};
            auto profile = wf_create_composer_profile(id, name);

            if (params.contains("birth_year"))
                profile.birth_year = params["birth_year"].get<int>();
            if (params.contains("death_year"))
                profile.death_year = params["death_year"].get<int>();
            if (params.contains("tradition"))
                profile.tradition = params["tradition"].get<std::string>();

            session->corpus.composers[id.value] = profile;

            return {{"composer_id", id.value}, {"name", name}};
        });

    server.register_tool(
        "create_ingested_work",
        "Create a new ingested work entry in the corpus",
        {{"title", "string"}, {"source_format", "string"},
         {"composer_id", "integer (optional)"}, {"opus", "string (optional)"},
         {"year_composed", "integer (optional)"}, {"instrumentation", "string (optional)"}},
        [session](const json& params) -> json {
            auto title = params.value("title", "");
            auto source_format = params.value("source_format", "");
            if (title.empty()) return error_response("title is required");
            if (source_format.empty()) return error_response("source_format is required");

            WorkMetadata meta;
            meta.title = title;
            meta.source_format = source_format;
            if (params.contains("opus"))
                meta.opus = params["opus"].get<std::string>();
            if (params.contains("year_composed"))
                meta.year_composed = params["year_composed"].get<int>();
            if (params.contains("instrumentation"))
                meta.instrumentation = params["instrumentation"].get<std::string>();

            auto id = IngestedWorkId{session->next_work_id++};
            auto work = wf_create_ingested_work(id, meta);

            if (params.contains("composer_id")) {
                auto cid = ComposerProfileId{params["composer_id"].get<std::uint64_t>()};
                meta.composer = ComposerRef{cid.value};
                work.metadata.composer = ComposerRef{cid.value};
            }

            session->corpus.works[id.value] = work;

            // Auto-assign to composer if provided
            if (params.contains("composer_id")) {
                auto cid = ComposerProfileId{params["composer_id"].get<std::uint64_t>()};
                auto r = wf_assign_work_to_composer(session->corpus, id, cid);
                if (!r) return error_response("failed to assign to composer");
            }

            return {{"work_id", id.value}, {"title", title}};
        });

    server.register_tool(
        "assign_work_to_composer",
        "Associate an ingested work with a composer profile",
        {{"work_id", "integer"}, {"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{params.value("work_id", std::uint64_t{0})};
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};

            auto r = wf_assign_work_to_composer(session->corpus, wid, cid);
            if (!r) return error_response("assignment failed: work or composer not found");
            return ok_response();
        });

    server.register_tool(
        "assign_work_to_period",
        "Assign a work to a compositional period within a composer's profile",
        {{"work_id", "integer"}, {"composer_id", "integer"}, {"period_label", "string"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{params.value("work_id", std::uint64_t{0})};
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto label = params.value("period_label", "");
            if (label.empty()) return error_response("period_label is required");

            auto r = wf_assign_work_to_period(session->corpus, wid, cid, label);
            if (!r) return error_response("assignment failed: work, composer, or period not found");
            return ok_response();
        });

    server.register_tool(
        "add_period_profile",
        "Add a period profile to a composer",
        {{"composer_id", "integer"}, {"label", "string"},
         {"year_start", "integer"}, {"year_end", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto label = params.value("label", "");
            if (label.empty()) return error_response("label is required");

            PeriodProfile pp;
            pp.label = label;
            pp.year_start = params.value("year_start", 0);
            pp.year_end = params.value("year_end", 0);

            auto r = wf_add_period_profile(session->corpus, cid, pp);
            if (!r) return error_response("composer not found");
            return ok_response();
        });

    server.register_tool(
        "set_work_metadata",
        "Update metadata for an ingested work",
        {{"work_id", "integer"}, {"title", "string (optional)"},
         {"opus", "string (optional)"}, {"year_composed", "integer (optional)"},
         {"instrumentation", "string (optional)"}, {"period", "string (optional)"},
         {"genre", "string (optional)"}},
        [session](const json& params) -> json {
            auto wid = params.value("work_id", std::uint64_t{0});
            auto it = session->corpus.works.find(wid);
            if (it == session->corpus.works.end())
                return error_response("work not found: " + std::to_string(wid));

            auto& m = it->second.metadata;
            if (params.contains("title")) m.title = params["title"].get<std::string>();
            if (params.contains("opus")) m.opus = params["opus"].get<std::string>();
            if (params.contains("year_composed")) m.year_composed = params["year_composed"].get<int>();
            if (params.contains("instrumentation")) m.instrumentation = params["instrumentation"].get<std::string>();
            if (params.contains("period")) m.period = params["period"].get<std::string>();
            if (params.contains("genre")) m.genre = params["genre"].get<std::string>();

            return ok_response();
        });

    // =========================================================================
    // Analysis
    // =========================================================================

    server.register_tool(
        "analyze_work",
        "Run analytical decomposition on an ingested work",
        {{"work_id", "integer"}},
        [session](const json& params) -> json {
            auto wid = IngestedWorkId{params.value("work_id", std::uint64_t{0})};
            // Pass embedded Score pointer when available
            auto wit = session->corpus.works.find(wid.value);
            if (wit == session->corpus.works.end())
                return error_response("analysis failed: work not found");
            const Score* sp = wit->second.score ? &*wit->second.score : nullptr;
            auto r = wf_analyze_work(session->corpus, wid, sp);
            if (!r) return error_response("analysis failed");
            return {{"work_id", wid.value}, {"analysis_complete", true}};
        });

    server.register_tool(
        "rebuild_style_profile",
        "Recompute a composer's style profile from all their analysed works",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto r = wf_rebuild_style_profile(session->corpus, cid);
            if (!r) return error_response("rebuild failed: composer not found or no analysed works");

            auto it = session->corpus.composers.find(cid.value);
            return {
                {"composer_id", cid.value},
                {"sample_size", it->second.style_profile.sample_size},
                {"confidence", it->second.style_profile.confidence}
            };
        });

    server.register_tool(
        "detect_signature_patterns",
        "Identify statistically distinctive patterns for a composer",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto r = wf_detect_signature_patterns(session->corpus, cid);
            if (!r) return error_response("detection failed: composer not found");

            auto it = session->corpus.composers.find(cid.value);
            return {
                {"composer_id", cid.value},
                {"pattern_count", it->second.style_profile.signature_patterns.size()}
            };
        });

    // =========================================================================
    // Comparison
    // =========================================================================

    server.register_tool(
        "compare_composers",
        "Generate a style comparison between two composer profiles",
        {{"composer_a", "integer"}, {"composer_b", "integer"}},
        [session](const json& params) -> json {
            auto a = ComposerProfileId{params.value("composer_a", std::uint64_t{0})};
            auto b = ComposerProfileId{params.value("composer_b", std::uint64_t{0})};

            auto r = wf_compare_composers(session->corpus, a, b);
            if (!r) return error_response("comparison failed: one or both composers not found");
            return comparison_j(*r);
        });

    server.register_tool(
        "analyze_evolution",
        "Analyse a composer's stylistic evolution across periods",
        {{"composer_id", "integer"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto r = wf_analyze_evolution(session->corpus, cid);
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
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto r = wf_query_style_profile(session->corpus, cid);
            if (!r) return error_response("query failed: composer not found");

            const auto* sp = *r;
            return {
                {"composer_id", cid.value},
                {"sample_size", sp->sample_size},
                {"confidence", sp->confidence},
                {"harmonic_vocabulary_size", sp->harmonic_profile.chord_vocabulary_size},
                {"signature_pattern_count", sp->signature_patterns.size()}
            };
        });

    server.register_tool(
        "find_examples",
        "Find passages matching analytical criteria in a composer's corpus",
        {{"composer_id", "integer"}, {"criterion", "string"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto criterion = params.value("criterion", "");
            if (criterion.empty()) return error_response("criterion is required");

            auto examples = wf_find_examples(session->corpus, cid, criterion);
            json arr = json::array();
            for (const auto& e : examples) arr.push_back(example_j(e));
            return {{"examples", arr}, {"count", examples.size()}};
        });

    server.register_tool(
        "get_progression_examples",
        "Find examples of a specific chord progression in a composer's corpus",
        {{"composer_id", "integer"}, {"roman_numerals", "array of strings"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            if (!params.contains("roman_numerals") || !params["roman_numerals"].is_array())
                return error_response("roman_numerals array is required");

            std::vector<std::string> rn;
            for (const auto& v : params["roman_numerals"])
                rn.push_back(v.get<std::string>());

            auto examples = wf_get_progression_examples(session->corpus, cid, rn);
            json arr = json::array();
            for (const auto& e : examples) arr.push_back(example_j(e));
            return {{"examples", arr}, {"count", examples.size()}};
        });

    server.register_tool(
        "get_formal_template",
        "Get typical formal proportions for a form type from a composer",
        {{"composer_id", "integer"}, {"form_type", "integer (FormClassification enum)"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto ft_int = params.value("form_type", -1);
            auto ft = form_from_int(ft_int);
            if (!ft) return error_response("invalid form_type: " + std::to_string(ft_int));

            auto r = wf_get_formal_template(session->corpus, cid, *ft);
            if (!r) return error_response("query failed: composer not found");

            const auto& fp = *r;
            json j = {
                {"composer_id", cid.value},
                {"preferred_form_count", fp.preferred_forms.size()},
                {"average_work_length", fp.average_work_length},
                {"climax_placement", fp.climax_placement}
            };
            if (fp.development_proportion)
                j["development_proportion"] = *fp.development_proportion;
            return j;
        });

    server.register_tool(
        "query_how_would_x_handle",
        "Get insights for a described compositional situation from a composer's corpus",
        {{"composer_id", "integer"}, {"situation", "string"}},
        [session](const json& params) -> json {
            auto cid = ComposerProfileId{params.value("composer_id", std::uint64_t{0})};
            auto situation = params.value("situation", "");
            if (situation.empty()) return error_response("situation is required");

            auto r = wf_how_would_x_handle(session->corpus, cid, situation);
            if (!r) return error_response("query failed: composer not found");

            const auto& result = *r;
            json examples = json::array();
            for (const auto& e : result.relevant_examples)
                examples.push_back(example_j(e));

            json tendencies = json::array();
            for (const auto& t : result.statistical_tendencies) {
                tendencies.push_back({
                    {"domain", t.domain},
                    {"observation", t.observation},
                    {"confidence", t.confidence},
                    {"supporting_examples_count", t.supporting_examples_count}
                });
            }

            json patterns = json::array();
            for (const auto& p : result.signature_patterns) {
                patterns.push_back({
                    {"id", p.id.value},
                    {"description", p.description},
                    {"domain", static_cast<int>(p.domain)},
                    {"distinctiveness", p.distinctiveness}
                });
            }

            return {
                {"relevant_examples", examples},
                {"statistical_tendencies", tendencies},
                {"signature_patterns", patterns}
            };
        });

    // =========================================================================
    // Corpus-Level
    // =========================================================================

    server.register_tool(
        "validate_corpus",
        "Validate the entire corpus and return diagnostics",
        {},
        [session](const json& /*params*/) -> json {
            auto diags = wf_validate_corpus(session->corpus);
            json arr = json::array();
            for (const auto& d : diags) arr.push_back(diagnostic_j(d));

            bool valid = true;
            for (const auto& d : diags)
                if (d.severity == ValidationSeverity::Error) { valid = false; break; }

            return {{"valid", valid}, {"diagnostics", arr}, {"count", diags.size()}};
        });

    server.register_tool(
        "get_corpus_json",
        "Get the full corpus database as JSON",
        {},
        [session](const json& /*params*/) -> json {
            return corpus_to_json(session->corpus);
        });

    // =========================================================================
    // Ingestion
    // =========================================================================

    server.register_tool(
        "ingest_midi",
        "Ingest a MIDI file into the corpus (base64-encoded binary)",
        {{"midi_base64", "string"}, {"title", "string"}, {"composer_id", "integer"},
         {"instrumentation", "string (optional)"}, {"quantise_grid", "integer (optional)"}},
        [session](const json& params) -> json {
            auto b64 = params.value("midi_base64", "");
            auto title = params.value("title", "");
            auto cid_val = params.value("composer_id", std::uint64_t{0});
            if (b64.empty()) return error_response("midi_base64 is required");
            if (title.empty()) return error_response("title is required");
            if (cid_val == 0) return error_response("composer_id is required");

            auto data = decode_base64(b64);
            if (data.empty()) return error_response("invalid base64 data");

            Corpus::IngestionOptions opts;
            opts.title = title;
            if (params.contains("instrumentation"))
                opts.instrumentation = params["instrumentation"].get<std::string>();
            if (params.contains("quantise_grid"))
                opts.quantise_grid = params["quantise_grid"].get<int>();

            auto wid = IngestedWorkId{session->next_work_id++};
            auto cid = ComposerProfileId{cid_val};

            auto r = wf_ingest_midi(session->corpus,
                std::span<const std::uint8_t>(data), wid, cid, opts);
            if (!r) return error_response("MIDI ingestion failed");

            return {{"work_id", wid.value}, {"title", title}, {"analysis_complete", true}};
        });

    server.register_tool(
        "ingest_musicxml",
        "Ingest a MusicXML document into the corpus",
        {{"musicxml", "string"}, {"title", "string"}, {"composer_id", "integer"},
         {"instrumentation", "string (optional)"}},
        [session](const json& params) -> json {
            auto xml = params.value("musicxml", "");
            auto title = params.value("title", "");
            auto cid_val = params.value("composer_id", std::uint64_t{0});
            if (xml.empty()) return error_response("musicxml is required");
            if (title.empty()) return error_response("title is required");
            if (cid_val == 0) return error_response("composer_id is required");

            Corpus::IngestionOptions opts;
            opts.title = title;
            if (params.contains("instrumentation"))
                opts.instrumentation = params["instrumentation"].get<std::string>();

            auto wid = IngestedWorkId{session->next_work_id++};
            auto cid = ComposerProfileId{cid_val};

            auto r = wf_ingest_musicxml(session->corpus, xml, wid, cid, opts);
            if (!r) return error_response("MusicXML ingestion failed");

            return {{"work_id", wid.value}, {"title", title}, {"analysis_complete", true}};
        });

    server.register_tool(
        "ingest_batch",
        "Ingest multiple works into the corpus",
        {{"works", "array of {title, data/musicxml, composer_id, format}"}},
        [session](const json& params) -> json {
            if (!params.contains("works") || !params["works"].is_array())
                return error_response("works array is required");

            json results = json::array();
            for (const auto& entry : params["works"]) {
                auto title = entry.value("title", "");
                auto cid_val = entry.value("composer_id", std::uint64_t{0});
                auto format = entry.value("format", "");
                if (title.empty() || cid_val == 0 || format.empty()) {
                    results.push_back({{"title", title}, {"error", "missing required fields"}});
                    continue;
                }

                Corpus::IngestionOptions opts;
                opts.title = title;
                if (entry.contains("instrumentation"))
                    opts.instrumentation = entry["instrumentation"].get<std::string>();

                auto wid = IngestedWorkId{session->next_work_id++};
                auto cid = ComposerProfileId{cid_val};

                if (format == "midi") {
                    auto b64 = entry.value("data", "");
                    auto data = decode_base64(b64);
                    auto r = wf_ingest_midi(session->corpus,
                        std::span<const std::uint8_t>(data), wid, cid, opts);
                    if (r)
                        results.push_back({{"work_id", wid.value}, {"title", title}, {"ok", true}});
                    else
                        results.push_back({{"title", title}, {"error", "MIDI ingestion failed"}});
                } else if (format == "musicxml") {
                    auto xml = entry.value("musicxml", "");
                    auto r = wf_ingest_musicxml(session->corpus, xml, wid, cid, opts);
                    if (r)
                        results.push_back({{"work_id", wid.value}, {"title", title}, {"ok", true}});
                    else
                        results.push_back({{"title", title}, {"error", "MusicXML ingestion failed"}});
                } else {
                    results.push_back({{"title", title}, {"error", "unknown format: " + format}});
                }
            }

            return {{"results", results}, {"count", results.size()}};
        });
}

}  // namespace Sunny::Infrastructure
