/**
 * @file serialization.cpp
 * @brief Corpus IR serialisation — implementation
 *
 *
 */

#include <cmath>
#include <optional>
#include <sunny/core/corpus/serialization.hpp>
#include <sunny/core/corpus/validation.hpp>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/serialization_primitives.hpp>
#include <type_traits>

namespace sunny::core {

using json = nlohmann::json;

namespace {

template <typename EnumT>
EnumT check_enum(const json& encoded, EnumT maximum, const char* name, const json& context) {
    const auto value = detail::checked_integer<int>(encoded, name);
    if (value < 0 || value > static_cast<int>(maximum))
        throw json::other_error::create(604, std::string(name) + " out of range", &context);
    return static_cast<EnumT>(value);
}

// =============================================================================
// Score Time — v1 back-compat reader
//
// Schema v1 stored ScoreTime flat as {"bar", "beat_n", "beat_d"}; v2-v4 use
// the shared nested Score IR scheme. The write path is v4-only.
// =============================================================================

ScoreTime score_time_from_flat_v1(const json& j) {
    ScoreTime t;
    t.bar = detail::checked_integer_or<std::uint32_t>(j, "bar", 0, "score-time bar");
    const auto numerator =
        detail::checked_integer_or<std::int64_t>(j, "beat_n", 0, "beat numerator");
    const auto denominator =
        detail::checked_integer_or<std::int64_t>(j, "beat_d", 1, "beat denominator");
    if (denominator <= 0)
        throw json::other_error::create(604, "beat denominator must be positive", &j);
    t.beat = Beat::normalise(numerator, denominator);
    return t;
}

// =============================================================================
// Ingestion Confidence
// =============================================================================

json confidence_j(const IngestionConfidence& ic) {
    json j = {{"key_confidence", ic.key_confidence},
              {"metre_confidence", ic.metre_confidence},
              {"spelling_confidence", ic.spelling_confidence},
              {"voice_separation_confidence", ic.voice_separation_confidence},
              {"quantisation_residual", ic.quantisation_residual},
              {"duration_quantisation_residual", ic.duration_quantisation_residual},
              {"source_format", ic.source_format}};
    json corrections = json::array();
    for (const auto& mc : ic.manual_corrections)
        corrections.push_back({{"field", mc.field}, {"description", mc.description}});
    j["manual_corrections"] = corrections;
    return j;
}

IngestionConfidence confidence_f_v1(const json& j) {
    IngestionConfidence ic;
    ic.key_confidence = j.value("key_confidence", 1.0f);
    ic.metre_confidence = j.value("metre_confidence", 1.0f);
    ic.spelling_confidence = j.value("spelling_confidence", 1.0f);
    ic.voice_separation_confidence = j.value("voice_separation_confidence", 1.0f);
    ic.quantisation_residual = j.value("quantisation_residual", 0.0f);
    ic.duration_quantisation_residual = 0.0f;
    ic.source_format = j.value("source_format", "");
    if (j.contains("manual_corrections")) {
        for (const auto& mc : j["manual_corrections"])
            ic.manual_corrections.push_back({mc.value("field", ""), mc.value("description", "")});
    }
    return ic;
}

IngestionConfidence confidence_f_v2(const json& j) {
    IngestionConfidence ic;
    ic.key_confidence = j.at("key_confidence").get<float>();
    ic.metre_confidence = j.at("metre_confidence").get<float>();
    ic.spelling_confidence = j.at("spelling_confidence").get<float>();
    ic.voice_separation_confidence = j.at("voice_separation_confidence").get<float>();
    ic.quantisation_residual = j.at("quantisation_residual").get<float>();
    ic.duration_quantisation_residual = 0.0f;
    ic.source_format = j.at("source_format").get<std::string>();
    for (const auto& mc : j.at("manual_corrections"))
        ic.manual_corrections.push_back(
            {mc.at("field").get<std::string>(), mc.at("description").get<std::string>()});
    return ic;
}

float finite_confidence_value(const json& j,
                              const char* field,
                              float minimum,
                              std::optional<float> maximum = std::nullopt) {
    const auto value = j.at(field).get<float>();
    if (!std::isfinite(value) || value < minimum || (maximum && value > *maximum))
        throw json::other_error::create(
            604, std::string(field) + " is outside its finite domain", &j);
    return value;
}

IngestionConfidence confidence_f_v4(const json& j) {
    IngestionConfidence ic;
    ic.key_confidence = finite_confidence_value(j, "key_confidence", 0.0f, 1.0f);
    ic.metre_confidence = finite_confidence_value(j, "metre_confidence", 0.0f, 1.0f);
    ic.spelling_confidence = finite_confidence_value(j, "spelling_confidence", 0.0f, 1.0f);
    ic.voice_separation_confidence =
        finite_confidence_value(j, "voice_separation_confidence", 0.0f, 1.0f);
    ic.quantisation_residual = finite_confidence_value(j, "quantisation_residual", 0.0f);
    ic.duration_quantisation_residual =
        finite_confidence_value(j, "duration_quantisation_residual", 0.0f);
    ic.source_format = j.at("source_format").get<std::string>();
    for (const auto& mc : j.at("manual_corrections"))
        ic.manual_corrections.push_back(
            {mc.at("field").get<std::string>(), mc.at("description").get<std::string>()});
    return ic;
}

// =============================================================================
// Work Metadata
// =============================================================================

json metadata_j(const WorkMetadata& m) {
    json j = {{"title", m.title},
              {"composer", m.composer.value},
              {"instrumentation", m.instrumentation},
              {"source_format", m.source_format},
              {"is_reduction", m.is_reduction},
              {"tags", m.tags}};
    if (m.opus) j["opus"] = *m.opus;
    if (m.year_composed) j["year_composed"] = *m.year_composed;
    if (m.period) j["period"] = *m.period;
    if (m.genre) j["genre"] = *m.genre;
    if (m.source_description) j["source_description"] = *m.source_description;
    if (m.original_instrumentation) j["original_instrumentation"] = *m.original_instrumentation;
    if (m.movement) j["movement"] = *m.movement;
    return j;
}

// Optional metadata fields; identical in v1 and v2.
void metadata_optionals_f(const json& j, WorkMetadata& m) {
    if (j.contains("opus")) m.opus = j["opus"].get<std::string>();
    if (j.contains("year_composed"))
        m.year_composed =
            detail::checked_integer<std::uint16_t>(j["year_composed"], "composition year");
    if (j.contains("period")) m.period = j["period"].get<std::string>();
    if (j.contains("genre")) m.genre = j["genre"].get<std::string>();
    if (j.contains("source_description"))
        m.source_description = j["source_description"].get<std::string>();
    if (j.contains("original_instrumentation"))
        m.original_instrumentation = j["original_instrumentation"].get<std::string>();
    if (j.contains("movement")) m.movement = j["movement"].get<std::string>();
}

WorkMetadata metadata_f_v1(const json& j) {
    WorkMetadata m;
    m.title = j.value("title", "");
    m.composer =
        ComposerRef{detail::checked_integer_or<std::uint64_t>(j, "composer", 0, "composer id")};
    m.instrumentation = j.value("instrumentation", "");
    m.source_format = j.value("source_format", "");
    m.is_reduction = j.value("is_reduction", false);
    if (j.contains("tags")) m.tags = j["tags"].get<std::vector<std::string>>();
    metadata_optionals_f(j, m);
    return m;
}

WorkMetadata metadata_f_v2(const json& j) {
    WorkMetadata m;
    m.title = j.at("title").get<std::string>();
    m.composer =
        ComposerRef{detail::checked_integer<std::uint64_t>(j.at("composer"), "composer id")};
    m.instrumentation = j.at("instrumentation").get<std::string>();
    m.source_format = j.at("source_format").get<std::string>();
    m.is_reduction = j.at("is_reduction").get<bool>();
    m.tags = j.at("tags").get<std::vector<std::string>>();
    metadata_optionals_f(j, m);
    return m;
}

// =============================================================================
// Harmonic Analysis
// =============================================================================

json harmonic_analysis_j(const HarmonicAnalysisRecord& h) {
    json progs = json::array();
    for (const auto& p : h.progression_inventory) {
        json occ = json::array();
        for (const auto& o : p.occurrences)
            occ.push_back(score_time_to_json(o));
        progs.push_back({{"roman_numerals", p.roman_numerals},
                         {"length", p.length},
                         {"occurrences", occ},
                         {"key_context", p.key_context}});
    }

    json mods = json::array();
    for (const auto& m : h.modulation_inventory) {
        json mj = {{"position", score_time_to_json(m.position)},
                   {"from_key", m.from_key},
                   {"to_key", m.to_key},
                   {"technique", static_cast<int>(m.technique)}};
        if (m.pivot_chord) mj["pivot_chord"] = *m.pivot_chord;
        mods.push_back(mj);
    }

    json cads = json::array();
    for (const auto& c : h.cadence_inventory) {
        cads.push_back({{"position", score_time_to_json(c.position)},
                        {"type", c.type},
                        {"approach", c.approach},
                        {"section_context", c.section_context},
                        {"is_structural", c.is_structural}});
    }

    return {{"chord_vocabulary", h.chord_vocabulary},
            {"progression_inventory", progs},
            {"modulation_inventory", mods},
            {"harmonic_rhythm",
             {{"changes_per_bar", h.harmonic_rhythm.changes_per_bar},
              {"mean_rate", h.harmonic_rhythm.mean_rate},
              {"variance", h.harmonic_rhythm.variance},
              {"rate_by_section", h.harmonic_rhythm.rate_by_section}}},
            {"cadence_inventory", cads},
            {"tonal_plan",
             {{"key_area_count", h.tonal_plan.key_area_count},
              {"most_distant_key", h.tonal_plan.most_distant_key}}}};
}

HarmonicAnalysisRecord harmonic_analysis_f_v1(const json& j) {
    HarmonicAnalysisRecord h;
    if (j.contains("chord_vocabulary"))
        h.chord_vocabulary = j["chord_vocabulary"].get<std::map<std::string, std::uint32_t>>();

    if (j.contains("progression_inventory")) {
        for (const auto& pj : j["progression_inventory"]) {
            ProgressionPattern p;
            p.roman_numerals = pj.value("roman_numerals", std::vector<std::string>{});
            p.length =
                detail::checked_integer_or<std::uint8_t>(pj, "length", 0, "progression length");
            if (pj.contains("occurrences"))
                for (const auto& oj : pj["occurrences"])
                    p.occurrences.push_back(score_time_from_flat_v1(oj));
            p.key_context = pj.value("key_context", "");
            h.progression_inventory.push_back(std::move(p));
        }
    }

    if (j.contains("harmonic_rhythm")) {
        const auto& hr = j["harmonic_rhythm"];
        if (hr.contains("changes_per_bar"))
            h.harmonic_rhythm.changes_per_bar = hr["changes_per_bar"].get<std::vector<float>>();
        h.harmonic_rhythm.mean_rate = hr.value("mean_rate", 0.0f);
        h.harmonic_rhythm.variance = hr.value("variance", 0.0f);
    }

    if (j.contains("tonal_plan")) {
        h.tonal_plan.key_area_count = detail::checked_integer_or<std::uint32_t>(
            j["tonal_plan"], "key_area_count", 1, "key-area count");
        h.tonal_plan.most_distant_key = j["tonal_plan"].value("most_distant_key", "");
    }

    return h;
}

HarmonicAnalysisRecord harmonic_analysis_f_v2(const json& j) {
    HarmonicAnalysisRecord h;
    h.chord_vocabulary = j.at("chord_vocabulary").get<std::map<std::string, std::uint32_t>>();

    for (const auto& pj : j.at("progression_inventory")) {
        ProgressionPattern p;
        p.roman_numerals = pj.at("roman_numerals").get<std::vector<std::string>>();
        p.length = detail::checked_integer<std::uint8_t>(pj.at("length"), "progression length");
        for (const auto& oj : pj.at("occurrences"))
            p.occurrences.push_back(score_time_from_json(oj));
        p.key_context = pj.at("key_context").get<std::string>();
        h.progression_inventory.push_back(std::move(p));
    }

    for (const auto& mj : j.at("modulation_inventory")) {
        ModulationEvent m;
        m.position = score_time_from_json(mj.at("position"));
        m.from_key = mj.at("from_key").get<std::string>();
        m.to_key = mj.at("to_key").get<std::string>();
        m.technique =
            check_enum(mj.at("technique"), ModulationTechnique::Abrupt, "ModulationTechnique", mj);
        if (mj.contains("pivot_chord")) m.pivot_chord = mj["pivot_chord"].get<std::string>();
        h.modulation_inventory.push_back(std::move(m));
    }

    const auto& hr = j.at("harmonic_rhythm");
    h.harmonic_rhythm.changes_per_bar = hr.at("changes_per_bar").get<std::vector<float>>();
    h.harmonic_rhythm.mean_rate = hr.at("mean_rate").get<float>();
    h.harmonic_rhythm.variance = hr.at("variance").get<float>();
    h.harmonic_rhythm.rate_by_section =
        hr.at("rate_by_section").get<std::map<std::string, float>>();

    for (const auto& cj : j.at("cadence_inventory")) {
        CadenceEvent c;
        c.position = score_time_from_json(cj.at("position"));
        c.type = cj.at("type").get<std::string>();
        c.approach = cj.at("approach").get<std::vector<std::string>>();
        c.section_context = cj.at("section_context").get<std::string>();
        c.is_structural = cj.at("is_structural").get<bool>();
        h.cadence_inventory.push_back(std::move(c));
    }

    const auto& tp = j.at("tonal_plan");
    h.tonal_plan.key_area_count =
        detail::checked_integer<std::uint32_t>(tp.at("key_area_count"), "key-area count");
    h.tonal_plan.most_distant_key = tp.at("most_distant_key").get<std::string>();

    return h;
}

// =============================================================================
// Formal Analysis
// =============================================================================

json formal_section_j(const FormalSection& s) {
    json subs = json::array();
    for (const auto& sub : s.subsections)
        subs.push_back(formal_section_j(sub));
    json j = {{"label", s.label},
              {"start_bar", s.start_bar},
              {"end_bar", s.end_bar},
              {"length_bars", s.length_bars},
              {"key", s.key},
              {"tempo", s.tempo},
              {"subsections", subs}};
    if (s.character) j["character"] = *s.character;
    return j;
}

FormalSection formal_section_f_v1(const json& j) {
    FormalSection s;
    s.label = j.value("label", "");
    s.start_bar = detail::checked_integer_or<std::uint32_t>(j, "start_bar", 0, "section start bar");
    s.end_bar = detail::checked_integer_or<std::uint32_t>(j, "end_bar", 0, "section end bar");
    s.length_bars =
        detail::checked_integer_or<std::uint32_t>(j, "length_bars", 0, "section length");
    s.key = j.value("key", "");
    s.tempo = j.value("tempo", 120.0f);
    if (j.contains("character")) s.character = j["character"].get<std::string>();
    if (j.contains("subsections"))
        for (const auto& sub : j["subsections"])
            s.subsections.push_back(formal_section_f_v1(sub));
    return s;
}

FormalSection formal_section_f_v2(const json& j) {
    FormalSection s;
    s.label = j.at("label").get<std::string>();
    s.start_bar = detail::checked_integer<std::uint32_t>(j.at("start_bar"), "section start bar");
    s.end_bar = detail::checked_integer<std::uint32_t>(j.at("end_bar"), "section end bar");
    s.length_bars = detail::checked_integer<std::uint32_t>(j.at("length_bars"), "section length");
    s.key = j.at("key").get<std::string>();
    s.tempo = j.at("tempo").get<float>();
    if (j.contains("character")) s.character = j["character"].get<std::string>();
    for (const auto& sub : j.at("subsections"))
        s.subsections.push_back(formal_section_f_v2(sub));
    return s;
}

json formal_analysis_j(const FormalAnalysisRecord& f) {
    json sections = json::array();
    for (const auto& s : f.section_plan)
        sections.push_back(formal_section_j(s));

    json proportions = json::array();
    for (const auto& p : f.proportions) {
        proportions.push_back({{"label", p.label},
                               {"proportion", p.proportion},
                               {"golden_ratio_proximity", p.golden_ratio_proximity}});
    }

    return {{"section_plan", sections},
            {"form_type", static_cast<int>(f.form_type)},
            {"total_duration_bars", f.total_duration_bars},
            {"proportions", proportions}};
}

FormalAnalysisRecord formal_analysis_f_v1(const json& j) {
    FormalAnalysisRecord f;
    if (j.contains("section_plan"))
        for (const auto& sj : j["section_plan"])
            f.section_plan.push_back(formal_section_f_v1(sj));
    f.form_type =
        j.contains("form_type")
            ? check_enum(j.at("form_type"), FormClassification::Other, "FormClassification", j)
            : FormClassification::Other;
    f.total_duration_bars = detail::checked_integer_or<std::uint32_t>(
        j, "total_duration_bars", 0, "total duration in bars");
    if (j.contains("proportions")) {
        for (const auto& pj : j["proportions"]) {
            SectionProportion sp;
            sp.label = pj.value("label", "");
            sp.proportion = pj.value("proportion", 0.0f);
            sp.golden_ratio_proximity = pj.value("golden_ratio_proximity", 0.0f);
            f.proportions.push_back(sp);
        }
    }
    return f;
}

FormalAnalysisRecord formal_analysis_f_v2(const json& j) {
    FormalAnalysisRecord f;
    for (const auto& sj : j.at("section_plan"))
        f.section_plan.push_back(formal_section_f_v2(sj));
    f.form_type = check_enum(j.at("form_type"), FormClassification::Other, "FormClassification", j);
    f.total_duration_bars = detail::checked_integer<std::uint32_t>(j.at("total_duration_bars"),
                                                                   "total duration in bars");
    for (const auto& pj : j.at("proportions")) {
        SectionProportion sp;
        sp.label = pj.at("label").get<std::string>();
        sp.proportion = pj.at("proportion").get<float>();
        sp.golden_ratio_proximity = pj.at("golden_ratio_proximity").get<float>();
        f.proportions.push_back(sp);
    }
    return f;
}

// =============================================================================
// Work Analysis (aggregate)
// =============================================================================

[[maybe_unused]] json work_analysis_j(const WorkAnalysis& wa) {
    return {
        {"harmonic_analysis", harmonic_analysis_j(wa.harmonic_analysis)},
        {"formal_analysis", formal_analysis_j(wa.formal_analysis)},
        {"rhythmic_analysis",
         {{"syncopation_index", wa.rhythmic_analysis.syncopation_index},
          {"metrical_complexity", wa.rhythmic_analysis.metrical_complexity},
          {"rest_proportion", wa.rhythmic_analysis.rest_proportion}}},
        {"voice_leading_analysis",
         {{"parallel_fifths_count", wa.voice_leading_analysis.parallel_fifths_count},
          {"parallel_octaves_count", wa.voice_leading_analysis.parallel_octaves_count},
          {"contrary_motion_proportion", wa.voice_leading_analysis.contrary_motion_proportion},
          {"common_tone_retention_rate", wa.voice_leading_analysis.common_tone_retention_rate},
          {"average_voice_independence", wa.voice_leading_analysis.average_voice_independence}}},
        {"textural_analysis", {{"average_density", wa.textural_analysis.average_density}}},
        {"dynamic_analysis",
         {{"climax_position", wa.dynamic_analysis.climax_position},
          {"hairpin_count", wa.dynamic_analysis.hairpin_count}}},
        {"motivic_analysis",
         {{"thematic_density", wa.motivic_analysis.thematic_density},
          {"thematic_economy", wa.motivic_analysis.thematic_economy}}}};
}

WorkAnalysis work_analysis_f_v1(const json& j) {
    WorkAnalysis wa;
    if (j.contains("harmonic_analysis"))
        wa.harmonic_analysis = harmonic_analysis_f_v1(j["harmonic_analysis"]);
    if (j.contains("formal_analysis"))
        wa.formal_analysis = formal_analysis_f_v1(j["formal_analysis"]);
    if (j.contains("rhythmic_analysis")) {
        wa.rhythmic_analysis.syncopation_index =
            j["rhythmic_analysis"].value("syncopation_index", 0.0f);
        wa.rhythmic_analysis.metrical_complexity =
            j["rhythmic_analysis"].value("metrical_complexity", 0.0f);
    }
    if (j.contains("voice_leading_analysis")) {
        const auto& vl = j["voice_leading_analysis"];
        wa.voice_leading_analysis.parallel_fifths_count = detail::checked_integer_or<std::uint32_t>(
            vl, "parallel_fifths_count", 0, "parallel-fifths count");
        wa.voice_leading_analysis.common_tone_retention_rate =
            vl.value("common_tone_retention_rate", 0.0f);
        wa.voice_leading_analysis.average_voice_independence =
            vl.value("average_voice_independence", 0.0f);
    }
    if (j.contains("motivic_analysis")) {
        wa.motivic_analysis.thematic_density =
            j["motivic_analysis"].value("thematic_density", 0.0f);
        wa.motivic_analysis.thematic_economy =
            j["motivic_analysis"].value("thematic_economy", 0.0f);
    }
    return wa;
}

WorkAnalysis work_analysis_f_v2(const json& j) {
    WorkAnalysis wa;
    wa.harmonic_analysis = harmonic_analysis_f_v2(j.at("harmonic_analysis"));
    wa.formal_analysis = formal_analysis_f_v2(j.at("formal_analysis"));

    const auto& ra = j.at("rhythmic_analysis");
    wa.rhythmic_analysis.syncopation_index = ra.at("syncopation_index").get<float>();
    wa.rhythmic_analysis.metrical_complexity = ra.at("metrical_complexity").get<float>();
    wa.rhythmic_analysis.rest_proportion = ra.at("rest_proportion").get<float>();

    const auto& vl = j.at("voice_leading_analysis");
    wa.voice_leading_analysis.parallel_fifths_count = detail::checked_integer<std::uint32_t>(
        vl.at("parallel_fifths_count"), "parallel-fifths count");
    wa.voice_leading_analysis.parallel_octaves_count = detail::checked_integer<std::uint32_t>(
        vl.at("parallel_octaves_count"), "parallel-octaves count");
    wa.voice_leading_analysis.contrary_motion_proportion =
        vl.at("contrary_motion_proportion").get<float>();
    wa.voice_leading_analysis.common_tone_retention_rate =
        vl.at("common_tone_retention_rate").get<float>();
    wa.voice_leading_analysis.average_voice_independence =
        vl.at("average_voice_independence").get<float>();

    wa.textural_analysis.average_density =
        j.at("textural_analysis").at("average_density").get<float>();

    const auto& da = j.at("dynamic_analysis");
    wa.dynamic_analysis.climax_position = da.at("climax_position").get<float>();
    wa.dynamic_analysis.hairpin_count =
        detail::checked_integer<std::uint32_t>(da.at("hairpin_count"), "hairpin count");

    const auto& ma = j.at("motivic_analysis");
    wa.motivic_analysis.thematic_density = ma.at("thematic_density").get<float>();
    wa.motivic_analysis.thematic_economy = ma.at("thematic_economy").get<float>();

    return wa;
}

// =============================================================================
// Work Analysis — field-complete v3 projection
// =============================================================================

template <typename Key, typename Value> json numeric_map_j(const std::map<Key, Value>& values) {
    json result = json::array();
    for (const auto& [key, value] : values)
        result.push_back({{"key", static_cast<std::int64_t>(key)}, {"value", value}});
    return result;
}

template <typename Key, typename Value>
std::map<Key, Value> numeric_map_f(const json& j, const char* key_name) {
    std::map<Key, Value> result;
    for (const auto& entry : j) {
        const auto key = detail::checked_integer<Key>(entry.at("key"), key_name);
        const auto [_, inserted] = result.emplace(key, entry.at("value").get<Value>());
        if (!inserted)
            throw json::other_error::create(604, std::string("duplicate ") + key_name, &entry);
    }
    return result;
}

json signed_byte_vector_j(const std::vector<std::int8_t>& values) {
    json result = json::array();
    for (const auto value : values)
        result.push_back(static_cast<int>(value));
    return result;
}

std::vector<std::int8_t> signed_byte_vector_f(const json& j, const char* name) {
    std::vector<std::int8_t> result;
    for (const auto& value : j)
        result.push_back(detail::checked_integer<std::int8_t>(value, name));
    return result;
}

json tonal_plan_j_v3(const TonalPlan& plan) {
    json sequence = json::array();
    for (const auto& [key, relationship, start_bar] : plan.key_sequence)
        sequence.push_back(
            {{"key", key}, {"relationship", relationship}, {"start_bar", start_bar}});

    json result = {{"key_sequence", sequence},
                   {"key_area_count", plan.key_area_count},
                   {"most_distant_key", plan.most_distant_key}};
    if (plan.tonic_return_bar) result["tonic_return_bar"] = *plan.tonic_return_bar;
    return result;
}

TonalPlan tonal_plan_f_v3(const json& j) {
    TonalPlan plan;
    for (const auto& entry : j.at("key_sequence"))
        plan.key_sequence.emplace_back(
            entry.at("key").get<std::string>(),
            entry.at("relationship").get<std::string>(),
            detail::checked_integer<std::uint32_t>(entry.at("start_bar"), "key-area start bar"));
    plan.key_area_count =
        detail::checked_integer<std::uint32_t>(j.at("key_area_count"), "key-area count");
    plan.most_distant_key = j.at("most_distant_key").get<std::string>();
    if (j.contains("tonic_return_bar"))
        plan.tonic_return_bar =
            detail::checked_integer<std::uint32_t>(j.at("tonic_return_bar"), "tonic-return bar");
    return plan;
}

json thematic_unit_j_v3(const ThematicUnit& unit) {
    json occurrences = json::array();
    for (const auto& occurrence : unit.occurrences) {
        occurrences.push_back({{"position", score_time_to_json(occurrence.position)},
                               {"part_id", occurrence.part_id.value},
                               {"transformation", static_cast<int>(occurrence.transformation)},
                               {"key", occurrence.key}});
    }
    return {{"id", unit.id.value},
            {"label", unit.label},
            {"intervals", signed_byte_vector_j(unit.intervals)},
            {"rhythm", unit.rhythm},
            {"contour", signed_byte_vector_j(unit.contour)},
            {"occurrences", occurrences}};
}

ThematicUnit thematic_unit_f_v3(const json& j) {
    ThematicUnit unit;
    unit.id =
        ThematicUnitId{detail::checked_integer<std::uint64_t>(j.at("id"), "thematic-unit id")};
    unit.label = j.at("label").get<std::string>();
    unit.intervals = signed_byte_vector_f(j.at("intervals"), "thematic interval");
    unit.rhythm = j.at("rhythm").get<std::vector<float>>();
    unit.contour = signed_byte_vector_f(j.at("contour"), "thematic contour direction");
    for (const auto& encoded : j.at("occurrences")) {
        ThematicOccurrence occurrence;
        occurrence.position = score_time_from_json(encoded.at("position"));
        occurrence.part_id = PartId{
            detail::checked_integer<std::uint64_t>(encoded.at("part_id"), "thematic part id")};
        occurrence.transformation = check_enum(encoded.at("transformation"),
                                               ThematicTransformation::Developed,
                                               "ThematicTransformation",
                                               encoded);
        occurrence.key = encoded.at("key").get<std::string>();
        unit.occurrences.push_back(std::move(occurrence));
    }
    return unit;
}

json harmonic_analysis_j_v3(const HarmonicAnalysisRecord& h) {
    json progressions = json::array();
    for (const auto& progression : h.progression_inventory) {
        json occurrences = json::array();
        for (const auto& occurrence : progression.occurrences)
            occurrences.push_back(score_time_to_json(occurrence));
        progressions.push_back({{"roman_numerals", progression.roman_numerals},
                                {"length", progression.length},
                                {"occurrences", occurrences},
                                {"key_context", progression.key_context}});
    }

    json modulations = json::array();
    for (const auto& modulation : h.modulation_inventory) {
        json encoded = {{"position", score_time_to_json(modulation.position)},
                        {"from_key", modulation.from_key},
                        {"to_key", modulation.to_key},
                        {"technique", static_cast<int>(modulation.technique)}};
        if (modulation.pivot_chord) encoded["pivot_chord"] = *modulation.pivot_chord;
        modulations.push_back(std::move(encoded));
    }

    json cadences = json::array();
    for (const auto& cadence : h.cadence_inventory)
        cadences.push_back({{"position", score_time_to_json(cadence.position)},
                            {"type", cadence.type},
                            {"approach", cadence.approach},
                            {"section_context", cadence.section_context},
                            {"is_structural", cadence.is_structural}});

    json chromatic = json::array();
    for (const auto& event : h.chromatic_techniques)
        chromatic.push_back({{"position", score_time_to_json(event.position)},
                             {"type", event.type},
                             {"description", event.description}});

    return {{"chord_vocabulary", h.chord_vocabulary},
            {"progression_inventory", progressions},
            {"modulation_inventory", modulations},
            {"harmonic_rhythm",
             {{"changes_per_bar", h.harmonic_rhythm.changes_per_bar},
              {"mean_rate", h.harmonic_rhythm.mean_rate},
              {"variance", h.harmonic_rhythm.variance},
              {"rate_by_section", h.harmonic_rhythm.rate_by_section}}},
            {"cadence_inventory", cadences},
            {"chromatic_techniques", chromatic},
            {"tonicisation_frequency", numeric_map_j(h.tonicisation_frequency)},
            {"tonal_plan", tonal_plan_j_v3(h.tonal_plan)}};
}

HarmonicAnalysisRecord harmonic_analysis_f_v3(const json& j) {
    HarmonicAnalysisRecord h;
    h.chord_vocabulary = j.at("chord_vocabulary").get<std::map<std::string, std::uint32_t>>();
    for (const auto& encoded : j.at("progression_inventory")) {
        ProgressionPattern progression;
        progression.roman_numerals = encoded.at("roman_numerals").get<std::vector<std::string>>();
        progression.length =
            detail::checked_integer<std::uint8_t>(encoded.at("length"), "progression length");
        for (const auto& occurrence : encoded.at("occurrences"))
            progression.occurrences.push_back(score_time_from_json(occurrence));
        progression.key_context = encoded.at("key_context").get<std::string>();
        h.progression_inventory.push_back(std::move(progression));
    }
    for (const auto& encoded : j.at("modulation_inventory")) {
        ModulationEvent modulation;
        modulation.position = score_time_from_json(encoded.at("position"));
        modulation.from_key = encoded.at("from_key").get<std::string>();
        modulation.to_key = encoded.at("to_key").get<std::string>();
        modulation.technique = check_enum(
            encoded.at("technique"), ModulationTechnique::Abrupt, "ModulationTechnique", encoded);
        if (encoded.contains("pivot_chord"))
            modulation.pivot_chord = encoded.at("pivot_chord").get<std::string>();
        h.modulation_inventory.push_back(std::move(modulation));
    }
    const auto& rhythm = j.at("harmonic_rhythm");
    h.harmonic_rhythm.changes_per_bar = rhythm.at("changes_per_bar").get<std::vector<float>>();
    h.harmonic_rhythm.mean_rate = rhythm.at("mean_rate").get<float>();
    h.harmonic_rhythm.variance = rhythm.at("variance").get<float>();
    h.harmonic_rhythm.rate_by_section =
        rhythm.at("rate_by_section").get<std::map<std::string, float>>();
    for (const auto& encoded : j.at("cadence_inventory")) {
        CadenceEvent cadence;
        cadence.position = score_time_from_json(encoded.at("position"));
        cadence.type = encoded.at("type").get<std::string>();
        cadence.approach = encoded.at("approach").get<std::vector<std::string>>();
        cadence.section_context = encoded.at("section_context").get<std::string>();
        cadence.is_structural = encoded.at("is_structural").get<bool>();
        h.cadence_inventory.push_back(std::move(cadence));
    }
    for (const auto& encoded : j.at("chromatic_techniques")) {
        h.chromatic_techniques.push_back({score_time_from_json(encoded.at("position")),
                                          encoded.at("type").get<std::string>(),
                                          encoded.at("description").get<std::string>()});
    }
    h.tonicisation_frequency = numeric_map_f<std::uint8_t, std::uint32_t>(
        j.at("tonicisation_frequency"), "tonicisation scale degree");
    h.tonal_plan = tonal_plan_f_v3(j.at("tonal_plan"));
    return h;
}

json melodic_analysis_j_v3(const MelodicAnalysisRecord& m) {
    json voices = json::array();
    for (const auto& voice : m.per_voice_analysis) {
        json contours = json::array();
        for (const auto& contour : voice.contour_inventory)
            contours.push_back({{"start", score_time_to_json(contour.start)},
                                {"end", score_time_to_json(contour.end)},
                                {"shape", static_cast<int>(contour.shape)},
                                {"pitch_range", contour.pitch_range},
                                {"duration_beats", contour.duration_beats},
                                {"peak_position", contour.peak_position},
                                {"nadir_position", contour.nadir_position}});
        voices.push_back(
            {{"part_id", voice.part_id.value},
             {"note_count", voice.note_count},
             {"range_low", voice.range_low},
             {"range_high", voice.range_high},
             {"tessitura_low", voice.tessitura_low},
             {"tessitura_high", voice.tessitura_high},
             {"interval_distribution", numeric_map_j(voice.interval_distribution)},
             {"contour_inventory", contours},
             {"scale_degree_distribution", numeric_map_j(voice.scale_degree_distribution)},
             {"leap_resolution_rate", voice.leap_resolution_rate},
             {"conjunct_proportion", voice.conjunct_proportion},
             {"longest_ascending_run", voice.longest_ascending_run},
             {"longest_descending_run", voice.longest_descending_run},
             {"chromaticism_rate", voice.chromaticism_rate}});
    }
    json thematic = json::array();
    for (const auto& unit : m.thematic_material)
        thematic.push_back(thematic_unit_j_v3(unit));
    return {{"per_voice_analysis", voices},
            {"primary_melody_voice", m.primary_melody_voice.value},
            {"thematic_material", thematic}};
}

MelodicAnalysisRecord melodic_analysis_f_v3(const json& j) {
    MelodicAnalysisRecord m;
    for (const auto& encoded : j.at("per_voice_analysis")) {
        VoiceMelodicAnalysis voice;
        voice.part_id = PartId{
            detail::checked_integer<std::uint64_t>(encoded.at("part_id"), "melodic part id")};
        voice.note_count =
            detail::checked_integer<std::uint32_t>(encoded.at("note_count"), "melodic note count");
        voice.range_low =
            detail::checked_integer<std::int8_t>(encoded.at("range_low"), "voice range low");
        voice.range_high =
            detail::checked_integer<std::int8_t>(encoded.at("range_high"), "voice range high");
        voice.tessitura_low = detail::checked_integer<std::int8_t>(encoded.at("tessitura_low"),
                                                                   "voice tessitura low");
        voice.tessitura_high = detail::checked_integer<std::int8_t>(encoded.at("tessitura_high"),
                                                                    "voice tessitura high");
        voice.interval_distribution = numeric_map_f<std::int8_t, std::uint32_t>(
            encoded.at("interval_distribution"), "melodic interval");
        for (const auto& contour_j : encoded.at("contour_inventory")) {
            ContourSegment contour;
            contour.start = score_time_from_json(contour_j.at("start"));
            contour.end = score_time_from_json(contour_j.at("end"));
            contour.shape =
                check_enum(contour_j.at("shape"), ContourShape::Complex, "ContourShape", contour_j);
            contour.pitch_range = detail::checked_integer<std::int8_t>(contour_j.at("pitch_range"),
                                                                       "contour pitch range");
            contour.duration_beats = contour_j.at("duration_beats").get<float>();
            contour.peak_position = contour_j.at("peak_position").get<float>();
            contour.nadir_position = contour_j.at("nadir_position").get<float>();
            voice.contour_inventory.push_back(std::move(contour));
        }
        voice.scale_degree_distribution = numeric_map_f<std::uint8_t, std::uint32_t>(
            encoded.at("scale_degree_distribution"), "scale degree");
        voice.leap_resolution_rate = encoded.at("leap_resolution_rate").get<float>();
        voice.conjunct_proportion = encoded.at("conjunct_proportion").get<float>();
        voice.longest_ascending_run = detail::checked_integer<std::uint8_t>(
            encoded.at("longest_ascending_run"), "longest ascending run");
        voice.longest_descending_run = detail::checked_integer<std::uint8_t>(
            encoded.at("longest_descending_run"), "longest descending run");
        voice.chromaticism_rate = encoded.at("chromaticism_rate").get<float>();
        m.per_voice_analysis.push_back(std::move(voice));
    }
    m.primary_melody_voice = PartId{detail::checked_integer<std::uint64_t>(
        j.at("primary_melody_voice"), "primary melody part id")};
    for (const auto& encoded : j.at("thematic_material"))
        m.thematic_material.push_back(thematic_unit_f_v3(encoded));
    return m;
}

json rhythmic_analysis_j_v3(const RhythmicAnalysisRecord& r) {
    json motifs = json::array();
    for (const auto& motif : r.rhythmic_motifs) {
        json positions = json::array();
        for (const auto& position : motif.positions)
            positions.push_back(score_time_to_json(position));
        motifs.push_back({{"durations", motif.durations},
                          {"occurrences", motif.occurrences},
                          {"positions", positions}});
    }
    json tempo = json::array();
    for (const auto& [position, value] : r.tempo_profile)
        tempo.push_back({{"position", score_time_to_json(position)}, {"tempo", value}});
    return {{"duration_distribution", r.duration_distribution},
            {"metre_distribution", r.metre_distribution},
            {"onset_density", r.onset_density},
            {"syncopation_index", r.syncopation_index},
            {"rhythmic_motifs", motifs},
            {"tempo_profile", tempo},
            {"rubato_degree", r.rubato_degree},
            {"metrical_complexity", r.metrical_complexity},
            {"rest_proportion", r.rest_proportion},
            {"note_density_by_section", r.note_density_by_section}};
}

RhythmicAnalysisRecord rhythmic_analysis_f_v3(const json& j) {
    RhythmicAnalysisRecord r;
    r.duration_distribution =
        j.at("duration_distribution").get<std::map<std::string, std::uint32_t>>();
    r.metre_distribution = j.at("metre_distribution").get<std::map<std::string, std::uint32_t>>();
    r.onset_density = j.at("onset_density").get<std::vector<float>>();
    r.syncopation_index = j.at("syncopation_index").get<float>();
    for (const auto& encoded : j.at("rhythmic_motifs")) {
        RhythmicMotif motif;
        motif.durations = encoded.at("durations").get<std::vector<float>>();
        motif.occurrences = detail::checked_integer<std::uint32_t>(
            encoded.at("occurrences"), "rhythmic-motif occurrence count");
        for (const auto& position : encoded.at("positions"))
            motif.positions.push_back(score_time_from_json(position));
        r.rhythmic_motifs.push_back(std::move(motif));
    }
    for (const auto& encoded : j.at("tempo_profile"))
        r.tempo_profile.emplace_back(score_time_from_json(encoded.at("position")),
                                     encoded.at("tempo").get<float>());
    r.rubato_degree = j.at("rubato_degree").get<float>();
    r.metrical_complexity = j.at("metrical_complexity").get<float>();
    r.rest_proportion = j.at("rest_proportion").get<float>();
    r.note_density_by_section = j.at("note_density_by_section").get<std::map<std::string, float>>();
    return r;
}

json formal_analysis_j_v3(const FormalAnalysisRecord& f) {
    json sections = json::array();
    for (const auto& section : f.section_plan)
        sections.push_back(formal_section_j(section));
    json proportions = json::array();
    for (const auto& proportion : f.proportions)
        proportions.push_back({{"label", proportion.label},
                               {"proportion", proportion.proportion},
                               {"golden_ratio_proximity", proportion.golden_ratio_proximity}});
    json assignments = json::object();
    for (const auto& [label, ids] : f.thematic_assignment) {
        json encoded_ids = json::array();
        for (const auto id : ids)
            encoded_ids.push_back(id.value);
        assignments[label] = std::move(encoded_ids);
    }
    json result = {{"section_plan", sections},
                   {"form_type", static_cast<int>(f.form_type)},
                   {"total_duration_bars", f.total_duration_bars},
                   {"proportions", proportions},
                   {"tonal_plan", tonal_plan_j_v3(f.tonal_plan)},
                   {"thematic_assignment", assignments}};
    if (f.symmetry_analysis)
        result["symmetry_analysis"] = {{"is_arch", f.symmetry_analysis->is_arch},
                                       {"is_palindrome", f.symmetry_analysis->is_palindrome},
                                       {"symmetry_index", f.symmetry_analysis->symmetry_index},
                                       {"description", f.symmetry_analysis->description}};
    return result;
}

FormalAnalysisRecord formal_analysis_f_v3(const json& j) {
    FormalAnalysisRecord f;
    for (const auto& section : j.at("section_plan"))
        f.section_plan.push_back(formal_section_f_v2(section));
    f.form_type = check_enum(j.at("form_type"), FormClassification::Other, "FormClassification", j);
    f.total_duration_bars = detail::checked_integer<std::uint32_t>(j.at("total_duration_bars"),
                                                                   "total duration in bars");
    for (const auto& encoded : j.at("proportions"))
        f.proportions.push_back({encoded.at("label").get<std::string>(),
                                 encoded.at("proportion").get<float>(),
                                 encoded.at("golden_ratio_proximity").get<float>()});
    f.tonal_plan = tonal_plan_f_v3(j.at("tonal_plan"));
    for (const auto& [label, encoded_ids] : j.at("thematic_assignment").items()) {
        auto& ids = f.thematic_assignment[label];
        for (const auto& encoded_id : encoded_ids)
            ids.push_back(ThematicUnitId{
                detail::checked_integer<std::uint64_t>(encoded_id, "thematic assignment id")});
    }
    if (j.contains("symmetry_analysis")) {
        const auto& encoded = j.at("symmetry_analysis");
        f.symmetry_analysis = SymmetryAnalysis{encoded.at("is_arch").get<bool>(),
                                               encoded.at("is_palindrome").get<bool>(),
                                               encoded.at("symmetry_index").get<float>(),
                                               encoded.at("description").get<std::string>()};
    }
    return f;
}

json voice_leading_analysis_j_v3(const VoiceLeadingAnalysisRecord& v) {
    json resolutions = json::array();
    for (const auto& resolution : v.resolution_patterns)
        resolutions.push_back({{"tendency_tone", resolution.tendency_tone},
                               {"resolution", resolution.resolution},
                               {"frequency", resolution.frequency},
                               {"proportion_resolved", resolution.proportion_resolved}});
    return {{"parallel_fifths_count", v.parallel_fifths_count},
            {"parallel_octaves_count", v.parallel_octaves_count},
            {"contrary_motion_proportion", v.contrary_motion_proportion},
            {"oblique_motion_proportion", v.oblique_motion_proportion},
            {"similar_motion_proportion", v.similar_motion_proportion},
            {"parallel_motion_proportion", v.parallel_motion_proportion},
            {"voice_crossing_count", v.voice_crossing_count},
            {"average_voice_independence", v.average_voice_independence},
            {"common_tone_retention_rate", v.common_tone_retention_rate},
            {"resolution_patterns", resolutions},
            {"spacing_distribution", numeric_map_j(v.spacing_distribution)}};
}

VoiceLeadingAnalysisRecord voice_leading_analysis_f_v3(const json& j) {
    VoiceLeadingAnalysisRecord v;
    v.parallel_fifths_count = detail::checked_integer<std::uint32_t>(j.at("parallel_fifths_count"),
                                                                     "parallel-fifths count");
    v.parallel_octaves_count = detail::checked_integer<std::uint32_t>(
        j.at("parallel_octaves_count"), "parallel-octaves count");
    v.contrary_motion_proportion = j.at("contrary_motion_proportion").get<float>();
    v.oblique_motion_proportion = j.at("oblique_motion_proportion").get<float>();
    v.similar_motion_proportion = j.at("similar_motion_proportion").get<float>();
    v.parallel_motion_proportion = j.at("parallel_motion_proportion").get<float>();
    v.voice_crossing_count = detail::checked_integer<std::uint32_t>(j.at("voice_crossing_count"),
                                                                    "voice-crossing count");
    v.average_voice_independence = j.at("average_voice_independence").get<float>();
    v.common_tone_retention_rate = j.at("common_tone_retention_rate").get<float>();
    for (const auto& encoded : j.at("resolution_patterns"))
        v.resolution_patterns.push_back({encoded.at("tendency_tone").get<std::string>(),
                                         encoded.at("resolution").get<std::string>(),
                                         detail::checked_integer<std::uint32_t>(
                                             encoded.at("frequency"), "resolution frequency"),
                                         encoded.at("proportion_resolved").get<float>()});
    v.spacing_distribution =
        numeric_map_f<std::int8_t, std::uint32_t>(j.at("spacing_distribution"), "voice spacing");
    return v;
}

json textural_analysis_j_v3(const TexturalAnalysisRecord& t) {
    json density = json::array();
    for (const auto& [position, value] : t.density_curve)
        density.push_back({{"position", score_time_to_json(position)}, {"density", value}});
    return {{"density_curve", density},
            {"average_density", t.average_density},
            {"density_by_section", t.density_by_section},
            {"average_register_span", t.average_register_span},
            {"spacing_profile",
             {{"close_proportion", t.spacing_profile.close_proportion},
              {"open_proportion", t.spacing_profile.open_proportion},
              {"gap_distribution", numeric_map_j(t.spacing_profile.gap_distribution)}}},
            {"texture_type_proportions", t.texture_type_proportions}};
}

TexturalAnalysisRecord textural_analysis_f_v3(const json& j) {
    TexturalAnalysisRecord t;
    for (const auto& encoded : j.at("density_curve"))
        t.density_curve.emplace_back(
            score_time_from_json(encoded.at("position")),
            detail::checked_integer<std::uint8_t>(encoded.at("density"), "texture density"));
    t.average_density = j.at("average_density").get<float>();
    t.density_by_section = j.at("density_by_section").get<std::map<std::string, float>>();
    t.average_register_span = j.at("average_register_span").get<float>();
    const auto& spacing = j.at("spacing_profile");
    t.spacing_profile.close_proportion = spacing.at("close_proportion").get<float>();
    t.spacing_profile.open_proportion = spacing.at("open_proportion").get<float>();
    t.spacing_profile.gap_distribution =
        numeric_map_f<std::int8_t, std::uint32_t>(spacing.at("gap_distribution"), "textural gap");
    t.texture_type_proportions =
        j.at("texture_type_proportions").get<std::map<std::string, float>>();
    return t;
}

json dynamic_analysis_j_v3(const DynamicAnalysisRecord& d) {
    json shape = json::array();
    for (const auto& [position, value] : d.dynamic_shape)
        shape.push_back({{"position", score_time_to_json(position)}, {"value", value}});
    json by_section = json::object();
    for (const auto& [section, range] : d.dynamic_by_section)
        by_section[section] = {{"low", range.first}, {"high", range.second}};
    return {{"dynamic_range_low", d.dynamic_range_low},
            {"dynamic_range_high", d.dynamic_range_high},
            {"dynamic_distribution", d.dynamic_distribution},
            {"hairpin_count", d.hairpin_count},
            {"dynamic_change_rate", d.dynamic_change_rate},
            {"dynamic_shape", shape},
            {"climax_position", d.climax_position},
            {"subito_dynamics_count", d.subito_dynamics_count},
            {"dynamic_by_section", by_section}};
}

DynamicAnalysisRecord dynamic_analysis_f_v3(const json& j) {
    DynamicAnalysisRecord d;
    d.dynamic_range_low = j.at("dynamic_range_low").get<std::string>();
    d.dynamic_range_high = j.at("dynamic_range_high").get<std::string>();
    d.dynamic_distribution =
        j.at("dynamic_distribution").get<std::map<std::string, std::uint32_t>>();
    d.hairpin_count =
        detail::checked_integer<std::uint32_t>(j.at("hairpin_count"), "hairpin count");
    d.dynamic_change_rate = j.at("dynamic_change_rate").get<float>();
    for (const auto& encoded : j.at("dynamic_shape"))
        d.dynamic_shape.emplace_back(score_time_from_json(encoded.at("position")),
                                     encoded.at("value").get<float>());
    d.climax_position = j.at("climax_position").get<float>();
    d.subito_dynamics_count = detail::checked_integer<std::uint32_t>(j.at("subito_dynamics_count"),
                                                                     "subito-dynamics count");
    for (const auto& [section, encoded] : j.at("dynamic_by_section").items())
        d.dynamic_by_section.emplace(
            section,
            std::pair{encoded.at("low").get<std::string>(), encoded.at("high").get<std::string>()});
    return d;
}

json instrument_combination_j_v3(const InstrumentCombination& combination) {
    json result = {{"instruments", combination.instruments},
                   {"frequency", combination.frequency},
                   {"typical_context", combination.typical_context}};
    if (combination.interval_relationship)
        result["interval_relationship"] = *combination.interval_relationship;
    return result;
}

InstrumentCombination instrument_combination_f_v3(const json& j) {
    InstrumentCombination combination;
    combination.instruments = j.at("instruments").get<std::vector<std::string>>();
    combination.frequency = detail::checked_integer<std::uint32_t>(
        j.at("frequency"), "instrument-combination frequency");
    combination.typical_context = j.at("typical_context").get<std::string>();
    if (j.contains("interval_relationship"))
        combination.interval_relationship = detail::checked_integer<std::int8_t>(
            j.at("interval_relationship"), "instrument interval relationship");
    return combination;
}

json doubling_pattern_j_v3(const DoublingPattern& doubling) {
    return {{"source_instrument", doubling.source_instrument},
            {"doubling_instrument", doubling.doubling_instrument},
            {"interval", doubling.interval},
            {"frequency", doubling.frequency}};
}

DoublingPattern doubling_pattern_f_v3(const json& j) {
    return {j.at("source_instrument").get<std::string>(),
            j.at("doubling_instrument").get<std::string>(),
            detail::checked_integer<std::int8_t>(j.at("interval"), "doubling interval"),
            detail::checked_integer<std::uint32_t>(j.at("frequency"), "doubling frequency")};
}

json orchestration_analysis_j_v3(const OrchestrationAnalysisRecord& o) {
    json combinations = json::array();
    for (const auto& combination : o.instrument_combinations)
        combinations.push_back(instrument_combination_j_v3(combination));
    json doublings = json::array();
    for (const auto& doubling : o.doubling_patterns)
        doublings.push_back(doubling_pattern_j_v3(doubling));
    json crescendos = json::array();
    for (const auto& crescendo : o.orchestral_crescendo_patterns) {
        json entries = json::array();
        for (const auto& [instrument, position] : crescendo.instrument_entry_order)
            entries.push_back(
                {{"instrument", instrument}, {"position", score_time_to_json(position)}});
        crescendos.push_back({{"start", score_time_to_json(crescendo.start)},
                              {"end", score_time_to_json(crescendo.end)},
                              {"instrument_entry_order", entries},
                              {"register_expansion", crescendo.register_expansion}});
    }
    return {{"instrument_usage", o.instrument_usage},
            {"instrument_combinations", combinations},
            {"doubling_patterns", doublings},
            {"melody_carrier_distribution", o.melody_carrier_distribution},
            {"orchestral_crescendo_patterns", crescendos},
            {"density_orchestration_correlation", o.density_orchestration_correlation}};
}

OrchestrationAnalysisRecord orchestration_analysis_f_v3(const json& j) {
    OrchestrationAnalysisRecord o;
    o.instrument_usage = j.at("instrument_usage").get<std::map<std::string, float>>();
    for (const auto& encoded : j.at("instrument_combinations"))
        o.instrument_combinations.push_back(instrument_combination_f_v3(encoded));
    for (const auto& encoded : j.at("doubling_patterns"))
        o.doubling_patterns.push_back(doubling_pattern_f_v3(encoded));
    o.melody_carrier_distribution =
        j.at("melody_carrier_distribution").get<std::map<std::string, float>>();
    for (const auto& encoded : j.at("orchestral_crescendo_patterns")) {
        OrchestraCrescendoPattern crescendo;
        crescendo.start = score_time_from_json(encoded.at("start"));
        crescendo.end = score_time_from_json(encoded.at("end"));
        for (const auto& entry : encoded.at("instrument_entry_order"))
            crescendo.instrument_entry_order.emplace_back(
                entry.at("instrument").get<std::string>(),
                score_time_from_json(entry.at("position")));
        crescendo.register_expansion = encoded.at("register_expansion").get<bool>();
        o.orchestral_crescendo_patterns.push_back(std::move(crescendo));
    }
    o.density_orchestration_correlation = j.at("density_orchestration_correlation").get<float>();
    return o;
}

json motivic_analysis_j_v3(const MotivicAnalysisRecord& m) {
    json units = json::array();
    for (const auto& unit : m.thematic_units)
        units.push_back(thematic_unit_j_v3(unit));
    json transformations = json::array();
    for (const auto& event : m.transformation_inventory)
        transformations.push_back({{"source_theme", event.source_theme.value},
                                   {"position", score_time_to_json(event.position)},
                                   {"transformation", static_cast<int>(event.transformation)},
                                   {"context", event.context}});
    json techniques = json::array();
    for (const auto technique : m.developmental_techniques)
        techniques.push_back(static_cast<int>(technique));
    return {{"thematic_units", units},
            {"transformation_inventory", transformations},
            {"developmental_techniques", techniques},
            {"thematic_density", m.thematic_density},
            {"thematic_economy", m.thematic_economy}};
}

MotivicAnalysisRecord motivic_analysis_f_v3(const json& j) {
    MotivicAnalysisRecord m;
    for (const auto& encoded : j.at("thematic_units"))
        m.thematic_units.push_back(thematic_unit_f_v3(encoded));
    for (const auto& encoded : j.at("transformation_inventory")) {
        TransformationEvent event;
        event.source_theme = ThematicUnitId{detail::checked_integer<std::uint64_t>(
            encoded.at("source_theme"), "transformation source-theme id")};
        event.position = score_time_from_json(encoded.at("position"));
        event.transformation = check_enum(encoded.at("transformation"),
                                          ThematicTransformation::Developed,
                                          "ThematicTransformation",
                                          encoded);
        event.context = encoded.at("context").get<std::string>();
        m.transformation_inventory.push_back(std::move(event));
    }
    for (const auto& encoded : j.at("developmental_techniques"))
        m.developmental_techniques.push_back(
            check_enum(encoded, DevelopmentalTechnique::Timbral, "DevelopmentalTechnique", j));
    m.thematic_density = j.at("thematic_density").get<float>();
    m.thematic_economy = j.at("thematic_economy").get<float>();
    return m;
}

json work_analysis_j_v3(const WorkAnalysis& analysis) {
    json result = {
        {"harmonic_analysis", harmonic_analysis_j_v3(analysis.harmonic_analysis)},
        {"melodic_analysis", melodic_analysis_j_v3(analysis.melodic_analysis)},
        {"rhythmic_analysis", rhythmic_analysis_j_v3(analysis.rhythmic_analysis)},
        {"formal_analysis", formal_analysis_j_v3(analysis.formal_analysis)},
        {"voice_leading_analysis", voice_leading_analysis_j_v3(analysis.voice_leading_analysis)},
        {"textural_analysis", textural_analysis_j_v3(analysis.textural_analysis)},
        {"dynamic_analysis", dynamic_analysis_j_v3(analysis.dynamic_analysis)},
        {"motivic_analysis", motivic_analysis_j_v3(analysis.motivic_analysis)}};
    if (analysis.orchestration_analysis)
        result["orchestration_analysis"] =
            orchestration_analysis_j_v3(*analysis.orchestration_analysis);
    return result;
}

WorkAnalysis work_analysis_f_v3(const json& j) {
    WorkAnalysis analysis;
    analysis.harmonic_analysis = harmonic_analysis_f_v3(j.at("harmonic_analysis"));
    analysis.melodic_analysis = melodic_analysis_f_v3(j.at("melodic_analysis"));
    analysis.rhythmic_analysis = rhythmic_analysis_f_v3(j.at("rhythmic_analysis"));
    analysis.formal_analysis = formal_analysis_f_v3(j.at("formal_analysis"));
    analysis.voice_leading_analysis = voice_leading_analysis_f_v3(j.at("voice_leading_analysis"));
    analysis.textural_analysis = textural_analysis_f_v3(j.at("textural_analysis"));
    analysis.dynamic_analysis = dynamic_analysis_f_v3(j.at("dynamic_analysis"));
    if (j.contains("orchestration_analysis"))
        analysis.orchestration_analysis =
            orchestration_analysis_f_v3(j.at("orchestration_analysis"));
    analysis.motivic_analysis = motivic_analysis_f_v3(j.at("motivic_analysis"));
    return analysis;
}

// =============================================================================
// Style Profile
// =============================================================================

json style_profile_j(const StyleProfile& sp) {
    json sigs = json::array();
    for (const auto& pat : sp.signature_patterns) {
        json examples = json::array();
        for (const auto& [wid, pos] : pat.examples)
            examples.push_back({{"work_id", wid.value}, {"position", score_time_to_json(pos)}});
        sigs.push_back({{"id", pat.id.value},
                        {"description", pat.description},
                        {"domain", static_cast<int>(pat.domain)},
                        {"distinctiveness", pat.distinctiveness},
                        {"examples", examples}});
    }

    return {{"harmonic_profile",
             {{"chord_vocabulary_size", sp.harmonic_profile.chord_vocabulary_size},
              {"chord_frequency", sp.harmonic_profile.chord_frequency},
              {"harmonic_rhythm_mean", sp.harmonic_profile.harmonic_rhythm_mean},
              {"modulation_frequency", sp.harmonic_profile.modulation_frequency},
              {"chromatic_density", sp.harmonic_profile.chromatic_density}}},
            {"melodic_profile",
             {{"conjunct_proportion", sp.melodic_profile.conjunct_proportion},
              {"chromaticism_rate", sp.melodic_profile.chromaticism_rate},
              {"average_phrase_length", sp.melodic_profile.average_phrase_length}}},
            {"rhythmic_profile",
             {{"syncopation_index", sp.rhythmic_profile.syncopation_index},
              {"metrical_complexity", sp.rhythmic_profile.metrical_complexity}}},
            {"formal_profile",
             {{"average_work_length", sp.formal_profile.average_work_length},
              {"climax_placement", sp.formal_profile.climax_placement}}},
            {"voice_leading_profile",
             {{"common_tone_retention", sp.voice_leading_profile.common_tone_retention},
              {"voice_independence_index", sp.voice_leading_profile.voice_independence_index}}},
            {"signature_patterns", sigs},
            {"sample_size", sp.sample_size},
            {"confidence", sp.confidence}};
}

StyleProfile style_profile_f_v1(const json& j) {
    StyleProfile sp;
    if (j.contains("harmonic_profile")) {
        const auto& hp = j["harmonic_profile"];
        sp.harmonic_profile.chord_vocabulary_size = detail::checked_integer_or<std::uint32_t>(
            hp, "chord_vocabulary_size", 0, "chord vocabulary size");
        if (hp.contains("chord_frequency"))
            sp.harmonic_profile.chord_frequency =
                hp["chord_frequency"].get<std::map<std::string, float>>();
        sp.harmonic_profile.harmonic_rhythm_mean = hp.value("harmonic_rhythm_mean", 0.0f);
        sp.harmonic_profile.modulation_frequency = hp.value("modulation_frequency", 0.0f);
        sp.harmonic_profile.chromatic_density = hp.value("chromatic_density", 0.0f);
    }
    if (j.contains("melodic_profile")) {
        sp.melodic_profile.conjunct_proportion =
            j["melodic_profile"].value("conjunct_proportion", 0.0f);
        sp.melodic_profile.chromaticism_rate =
            j["melodic_profile"].value("chromaticism_rate", 0.0f);
    }
    if (j.contains("rhythmic_profile")) {
        sp.rhythmic_profile.syncopation_index =
            j["rhythmic_profile"].value("syncopation_index", 0.0f);
        sp.rhythmic_profile.metrical_complexity =
            j["rhythmic_profile"].value("metrical_complexity", 0.0f);
    }
    if (j.contains("formal_profile")) {
        sp.formal_profile.average_work_length =
            j["formal_profile"].value("average_work_length", 0.0f);
    }
    if (j.contains("voice_leading_profile")) {
        sp.voice_leading_profile.common_tone_retention =
            j["voice_leading_profile"].value("common_tone_retention", 0.0f);
        sp.voice_leading_profile.voice_independence_index =
            j["voice_leading_profile"].value("voice_independence_index", 0.0f);
    }
    sp.sample_size =
        detail::checked_integer_or<std::uint32_t>(j, "sample_size", 0, "style sample size");
    sp.confidence = j.value("confidence", 0.0f);
    return sp;
}

StyleProfile style_profile_f_v2(const json& j) {
    StyleProfile sp;

    const auto& hp = j.at("harmonic_profile");
    sp.harmonic_profile.chord_vocabulary_size = detail::checked_integer<std::uint32_t>(
        hp.at("chord_vocabulary_size"), "chord vocabulary size");
    sp.harmonic_profile.chord_frequency =
        hp.at("chord_frequency").get<std::map<std::string, float>>();
    sp.harmonic_profile.harmonic_rhythm_mean = hp.at("harmonic_rhythm_mean").get<float>();
    sp.harmonic_profile.modulation_frequency = hp.at("modulation_frequency").get<float>();
    sp.harmonic_profile.chromatic_density = hp.at("chromatic_density").get<float>();

    const auto& mp = j.at("melodic_profile");
    sp.melodic_profile.conjunct_proportion = mp.at("conjunct_proportion").get<float>();
    sp.melodic_profile.chromaticism_rate = mp.at("chromaticism_rate").get<float>();
    sp.melodic_profile.average_phrase_length = mp.at("average_phrase_length").get<float>();

    const auto& rp = j.at("rhythmic_profile");
    sp.rhythmic_profile.syncopation_index = rp.at("syncopation_index").get<float>();
    sp.rhythmic_profile.metrical_complexity = rp.at("metrical_complexity").get<float>();

    const auto& fp = j.at("formal_profile");
    sp.formal_profile.average_work_length = fp.at("average_work_length").get<float>();
    sp.formal_profile.climax_placement = fp.at("climax_placement").get<float>();

    const auto& vp = j.at("voice_leading_profile");
    sp.voice_leading_profile.common_tone_retention = vp.at("common_tone_retention").get<float>();
    sp.voice_leading_profile.voice_independence_index =
        vp.at("voice_independence_index").get<float>();

    for (const auto& sj : j.at("signature_patterns")) {
        SignaturePattern pat;
        pat.id = SignaturePatternId{
            detail::checked_integer<std::uint64_t>(sj.at("id"), "signature pattern id")};
        pat.description = sj.at("description").get<std::string>();
        pat.domain = check_enum(sj.at("domain"), PatternDomain::Dynamic, "PatternDomain", sj);
        pat.distinctiveness = sj.at("distinctiveness").get<float>();
        for (const auto& ej : sj.at("examples"))
            pat.examples.emplace_back(IngestedWorkId{detail::checked_integer<std::uint64_t>(
                                          ej.at("work_id"), "example work id")},
                                      score_time_from_json(ej.at("position")));
        sp.signature_patterns.push_back(std::move(pat));
    }

    sp.sample_size =
        detail::checked_integer<std::uint32_t>(j.at("sample_size"), "style sample size");
    sp.confidence = j.at("confidence").get<float>();
    return sp;
}

// =============================================================================
// Style Profile — field-complete v3 projection
// =============================================================================

json ranked_progression_j_v3(const RankedProgression& progression) {
    return {{"progression", progression.progression},
            {"frequency", progression.frequency},
            {"rank", progression.rank},
            {"contexts", progression.contexts}};
}

RankedProgression ranked_progression_f_v3(const json& j) {
    return {j.at("progression").get<std::vector<std::string>>(),
            j.at("frequency").get<float>(),
            detail::checked_integer<std::uint32_t>(j.at("rank"), "progression rank"),
            j.at("contexts").get<std::vector<std::string>>()};
}

json pattern_data_j_v3(const PatternData& data) {
    return std::visit(
        [](const auto& value) -> json {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::vector<std::string>>)
                return {{"kind", "string_sequence"}, {"value", value}};
            else if constexpr (std::is_same_v<T, std::vector<std::int8_t>>)
                return {{"kind", "interval_sequence"}, {"value", signed_byte_vector_j(value)}};
            else if constexpr (std::is_same_v<T, std::vector<float>>)
                return {{"kind", "float_sequence"}, {"value", value}};
            else
                return {{"kind", "text"}, {"value", value}};
        },
        data);
}

PatternData pattern_data_f_v3(const json& j) {
    const auto kind = j.at("kind").get<std::string>();
    if (kind == "string_sequence") return j.at("value").get<std::vector<std::string>>();
    if (kind == "interval_sequence")
        return signed_byte_vector_f(j.at("value"), "signature-pattern interval");
    if (kind == "float_sequence") return j.at("value").get<std::vector<float>>();
    if (kind == "text") return j.at("value").get<std::string>();
    throw json::other_error::create(604, "unknown signature-pattern data kind", &j);
}

json signature_pattern_j_v3(const SignaturePattern& pattern) {
    json examples = json::array();
    for (const auto& [work_id, position] : pattern.examples)
        examples.push_back(
            {{"work_id", work_id.value}, {"position", score_time_to_json(position)}});
    return {{"id", pattern.id.value},
            {"description", pattern.description},
            {"domain", static_cast<int>(pattern.domain)},
            {"pattern_data", pattern_data_j_v3(pattern.pattern_data)},
            {"distinctiveness", pattern.distinctiveness},
            {"examples", examples}};
}

SignaturePattern signature_pattern_f_v3(const json& j) {
    SignaturePattern pattern;
    pattern.id = SignaturePatternId{
        detail::checked_integer<std::uint64_t>(j.at("id"), "signature pattern id")};
    pattern.description = j.at("description").get<std::string>();
    pattern.domain = check_enum(j.at("domain"), PatternDomain::Dynamic, "PatternDomain", j);
    pattern.pattern_data = pattern_data_f_v3(j.at("pattern_data"));
    pattern.distinctiveness = j.at("distinctiveness").get<float>();
    for (const auto& encoded : j.at("examples"))
        pattern.examples.emplace_back(IngestedWorkId{detail::checked_integer<std::uint64_t>(
                                          encoded.at("work_id"), "example work id")},
                                      score_time_from_json(encoded.at("position")));
    return pattern;
}

json style_profile_j_v3(const StyleProfile& style) {
    json progressions = json::array();
    for (const auto& progression : style.harmonic_profile.preferred_progressions)
        progressions.push_back(ranked_progression_j_v3(progression));

    json combinations = json::array();
    json doublings = json::array();
    if (style.orchestration_profile) {
        for (const auto& combination : style.orchestration_profile->signature_combinations)
            combinations.push_back(instrument_combination_j_v3(combination));
        for (const auto& doubling : style.orchestration_profile->doubling_preferences)
            doublings.push_back(doubling_pattern_j_v3(doubling));
    }

    json patterns = json::array();
    for (const auto& pattern : style.signature_patterns)
        patterns.push_back(signature_pattern_j_v3(pattern));

    const auto& harmonic = style.harmonic_profile;
    const auto& melodic = style.melodic_profile;
    const auto& rhythmic = style.rhythmic_profile;
    const auto& formal = style.formal_profile;
    const auto& voice = style.voice_leading_profile;
    const auto& textural = style.textural_profile;
    const auto& dynamic = style.dynamic_profile;
    const auto& motivic = style.motivic_profile;

    json formal_j = {{"preferred_forms", numeric_map_j(formal.preferred_forms)},
                     {"average_work_length", formal.average_work_length},
                     {"section_proportions", formal.section_proportions},
                     {"introduction_frequency", formal.introduction_frequency},
                     {"coda_frequency", formal.coda_frequency},
                     {"climax_placement", formal.climax_placement},
                     {"golden_ratio_adherence", formal.golden_ratio_adherence},
                     {"transition_technique", formal.transition_technique}};
    if (formal.exposition_recapitulation_ratio)
        formal_j["exposition_recapitulation_ratio"] = *formal.exposition_recapitulation_ratio;
    if (formal.development_proportion)
        formal_j["development_proportion"] = *formal.development_proportion;

    json result = {
        {"harmonic_profile",
         {{"chord_vocabulary_size", harmonic.chord_vocabulary_size},
          {"chord_frequency", harmonic.chord_frequency},
          {"preferred_progressions", progressions},
          {"modulation_frequency", harmonic.modulation_frequency},
          {"modulation_technique_preference",
           numeric_map_j(harmonic.modulation_technique_preference)},
          {"preferred_key_relationships", harmonic.preferred_key_relationships},
          {"preferred_key_signatures", harmonic.preferred_key_signatures},
          {"chromatic_density", harmonic.chromatic_density},
          {"secondary_dominant_frequency", harmonic.secondary_dominant_frequency},
          {"augmented_sixth_frequency", harmonic.augmented_sixth_frequency},
          {"neapolitan_frequency", harmonic.neapolitan_frequency},
          {"harmonic_rhythm_mean", harmonic.harmonic_rhythm_mean},
          {"harmonic_rhythm_variance", harmonic.harmonic_rhythm_variance},
          {"cadence_type_distribution", numeric_map_j(harmonic.cadence_type_distribution)},
          {"deceptive_cadence_frequency", harmonic.deceptive_cadence_frequency},
          {"tonal_ambiguity_index", harmonic.tonal_ambiguity_index}}},
        {"melodic_profile",
         {{"interval_distribution", numeric_map_j(melodic.interval_distribution)},
          {"preferred_intervals", signed_byte_vector_j(melodic.preferred_intervals)},
          {"conjunct_proportion", melodic.conjunct_proportion},
          {"average_phrase_length", melodic.average_phrase_length},
          {"phrase_length_variance", melodic.phrase_length_variance},
          {"contour_preferences", numeric_map_j(melodic.contour_preferences)},
          {"typical_range", melodic.typical_range},
          {"chromaticism_rate", melodic.chromaticism_rate},
          {"scale_degree_emphasis", numeric_map_j(melodic.scale_degree_emphasis)},
          {"ornament_density", melodic.ornament_density},
          {"sequence_frequency", melodic.sequence_frequency},
          {"leitmotif_usage", melodic.leitmotif_usage}}},
        {"rhythmic_profile",
         {{"duration_distribution", rhythmic.duration_distribution},
          {"preferred_durations", rhythmic.preferred_durations},
          {"preferred_metres", rhythmic.preferred_metres},
          {"syncopation_index", rhythmic.syncopation_index},
          {"rhythmic_variety", rhythmic.rhythmic_variety},
          {"metrical_complexity", rhythmic.metrical_complexity},
          {"tempo_mean", rhythmic.tempo_mean},
          {"tempo_stddev", rhythmic.tempo_stddev},
          {"rubato_tendency", rhythmic.rubato_tendency},
          {"rhythmic_motif_consistency", rhythmic.rhythmic_motif_consistency}}},
        {"formal_profile", formal_j},
        {"voice_leading_profile",
         {{"parallel_fifths_tolerance", voice.parallel_fifths_tolerance},
          {"parallel_octaves_tolerance", voice.parallel_octaves_tolerance},
          {"preferred_motion_type", voice.preferred_motion_type},
          {"voice_independence_index", voice.voice_independence_index},
          {"common_tone_retention", voice.common_tone_retention},
          {"leading_tone_resolution_rate", voice.leading_tone_resolution_rate},
          {"seventh_resolution_rate", voice.seventh_resolution_rate},
          {"spacing_preference", voice.spacing_preference},
          {"voice_crossing_tolerance", voice.voice_crossing_tolerance}}},
        {"textural_profile",
         {{"average_density", textural.average_density},
          {"density_range_low", textural.density_range_low},
          {"density_range_high", textural.density_range_high},
          {"texture_type_distribution", textural.texture_type_distribution},
          {"register_span_preference", textural.register_span_preference},
          {"density_dynamic_correlation", textural.density_dynamic_correlation}}},
        {"dynamic_profile",
         {{"dynamic_range_low", dynamic.dynamic_range_low},
          {"dynamic_range_high", dynamic.dynamic_range_high},
          {"most_frequent_dynamic", dynamic.most_frequent_dynamic},
          {"dynamic_change_rate", dynamic.dynamic_change_rate},
          {"subito_frequency", dynamic.subito_frequency},
          {"climax_dynamic", dynamic.climax_dynamic},
          {"dynamic_arc_shape", static_cast<int>(dynamic.dynamic_arc_shape)}}},
        {"motivic_profile",
         {{"thematic_economy", motivic.thematic_economy},
          {"preferred_transformations", numeric_map_j(motivic.preferred_transformations)},
          {"development_density", motivic.development_density},
          {"fragmentation_frequency", motivic.fragmentation_frequency},
          {"sequence_frequency", motivic.sequence_frequency},
          {"cross_movement_thematic_links", motivic.cross_movement_thematic_links}}},
        {"signature_patterns", patterns},
        {"sample_size", style.sample_size},
        {"confidence", style.confidence}};

    if (style.orchestration_profile) {
        const auto& orchestration = *style.orchestration_profile;
        result["orchestration_profile"] = {
            {"preferred_instruments", orchestration.preferred_instruments},
            {"signature_combinations", combinations},
            {"doubling_preferences", doublings},
            {"melody_assignment_preference", orchestration.melody_assignment_preference},
            {"tutti_proportion", orchestration.tutti_proportion},
            {"solo_proportion", orchestration.solo_proportion},
            {"build_up_technique", orchestration.build_up_technique},
            {"colour_signature", orchestration.colour_signature}};
    }
    return result;
}

StyleProfile style_profile_f_v3(const json& j) {
    StyleProfile style;
    const auto& harmonic = j.at("harmonic_profile");
    style.harmonic_profile.chord_vocabulary_size = detail::checked_integer<std::uint32_t>(
        harmonic.at("chord_vocabulary_size"), "chord vocabulary size");
    style.harmonic_profile.chord_frequency =
        harmonic.at("chord_frequency").get<std::map<std::string, float>>();
    for (const auto& encoded : harmonic.at("preferred_progressions"))
        style.harmonic_profile.preferred_progressions.push_back(ranked_progression_f_v3(encoded));
    style.harmonic_profile.modulation_frequency = harmonic.at("modulation_frequency").get<float>();
    style.harmonic_profile.modulation_technique_preference = numeric_map_f<std::uint8_t, float>(
        harmonic.at("modulation_technique_preference"), "modulation-technique code");
    style.harmonic_profile.preferred_key_relationships =
        harmonic.at("preferred_key_relationships").get<std::map<std::string, float>>();
    style.harmonic_profile.preferred_key_signatures =
        harmonic.at("preferred_key_signatures").get<std::map<std::string, std::uint32_t>>();
    style.harmonic_profile.chromatic_density = harmonic.at("chromatic_density").get<float>();
    style.harmonic_profile.secondary_dominant_frequency =
        harmonic.at("secondary_dominant_frequency").get<float>();
    style.harmonic_profile.augmented_sixth_frequency =
        harmonic.at("augmented_sixth_frequency").get<float>();
    style.harmonic_profile.neapolitan_frequency = harmonic.at("neapolitan_frequency").get<float>();
    style.harmonic_profile.harmonic_rhythm_mean = harmonic.at("harmonic_rhythm_mean").get<float>();
    style.harmonic_profile.harmonic_rhythm_variance =
        harmonic.at("harmonic_rhythm_variance").get<float>();
    style.harmonic_profile.cadence_type_distribution = numeric_map_f<std::uint8_t, float>(
        harmonic.at("cadence_type_distribution"), "cadence-type code");
    style.harmonic_profile.deceptive_cadence_frequency =
        harmonic.at("deceptive_cadence_frequency").get<float>();
    style.harmonic_profile.tonal_ambiguity_index =
        harmonic.at("tonal_ambiguity_index").get<float>();

    const auto& melodic = j.at("melodic_profile");
    style.melodic_profile.interval_distribution = numeric_map_f<std::int8_t, float>(
        melodic.at("interval_distribution"), "melodic-style interval");
    style.melodic_profile.preferred_intervals =
        signed_byte_vector_f(melodic.at("preferred_intervals"), "preferred melodic interval");
    style.melodic_profile.conjunct_proportion = melodic.at("conjunct_proportion").get<float>();
    style.melodic_profile.average_phrase_length = melodic.at("average_phrase_length").get<float>();
    style.melodic_profile.phrase_length_variance =
        melodic.at("phrase_length_variance").get<float>();
    style.melodic_profile.contour_preferences =
        numeric_map_f<std::uint8_t, float>(melodic.at("contour_preferences"), "contour-shape code");
    style.melodic_profile.typical_range = melodic.at("typical_range").get<float>();
    style.melodic_profile.chromaticism_rate = melodic.at("chromaticism_rate").get<float>();
    style.melodic_profile.scale_degree_emphasis = numeric_map_f<std::uint8_t, float>(
        melodic.at("scale_degree_emphasis"), "emphasised scale degree");
    style.melodic_profile.ornament_density = melodic.at("ornament_density").get<float>();
    style.melodic_profile.sequence_frequency = melodic.at("sequence_frequency").get<float>();
    style.melodic_profile.leitmotif_usage = melodic.at("leitmotif_usage").get<bool>();

    const auto& rhythmic = j.at("rhythmic_profile");
    style.rhythmic_profile.duration_distribution =
        rhythmic.at("duration_distribution").get<std::map<std::string, float>>();
    style.rhythmic_profile.preferred_durations =
        rhythmic.at("preferred_durations").get<std::vector<std::string>>();
    style.rhythmic_profile.preferred_metres =
        rhythmic.at("preferred_metres").get<std::map<std::string, std::uint32_t>>();
    style.rhythmic_profile.syncopation_index = rhythmic.at("syncopation_index").get<float>();
    style.rhythmic_profile.rhythmic_variety = rhythmic.at("rhythmic_variety").get<float>();
    style.rhythmic_profile.metrical_complexity = rhythmic.at("metrical_complexity").get<float>();
    style.rhythmic_profile.tempo_mean = rhythmic.at("tempo_mean").get<float>();
    style.rhythmic_profile.tempo_stddev = rhythmic.at("tempo_stddev").get<float>();
    style.rhythmic_profile.rubato_tendency = rhythmic.at("rubato_tendency").get<float>();
    style.rhythmic_profile.rhythmic_motif_consistency =
        rhythmic.at("rhythmic_motif_consistency").get<float>();

    const auto& formal = j.at("formal_profile");
    style.formal_profile.preferred_forms = numeric_map_f<std::uint8_t, std::uint32_t>(
        formal.at("preferred_forms"), "form-classification code");
    style.formal_profile.average_work_length = formal.at("average_work_length").get<float>();
    style.formal_profile.section_proportions =
        formal.at("section_proportions").get<std::map<std::string, float>>();
    if (formal.contains("exposition_recapitulation_ratio"))
        style.formal_profile.exposition_recapitulation_ratio =
            formal.at("exposition_recapitulation_ratio").get<float>();
    if (formal.contains("development_proportion"))
        style.formal_profile.development_proportion =
            formal.at("development_proportion").get<float>();
    style.formal_profile.introduction_frequency = formal.at("introduction_frequency").get<float>();
    style.formal_profile.coda_frequency = formal.at("coda_frequency").get<float>();
    style.formal_profile.climax_placement = formal.at("climax_placement").get<float>();
    style.formal_profile.golden_ratio_adherence = formal.at("golden_ratio_adherence").get<float>();
    style.formal_profile.transition_technique =
        formal.at("transition_technique").get<std::vector<std::string>>();

    const auto& voice = j.at("voice_leading_profile");
    style.voice_leading_profile.parallel_fifths_tolerance =
        voice.at("parallel_fifths_tolerance").get<float>();
    style.voice_leading_profile.parallel_octaves_tolerance =
        voice.at("parallel_octaves_tolerance").get<float>();
    style.voice_leading_profile.preferred_motion_type =
        voice.at("preferred_motion_type").get<std::string>();
    style.voice_leading_profile.voice_independence_index =
        voice.at("voice_independence_index").get<float>();
    style.voice_leading_profile.common_tone_retention =
        voice.at("common_tone_retention").get<float>();
    style.voice_leading_profile.leading_tone_resolution_rate =
        voice.at("leading_tone_resolution_rate").get<float>();
    style.voice_leading_profile.seventh_resolution_rate =
        voice.at("seventh_resolution_rate").get<float>();
    style.voice_leading_profile.spacing_preference =
        voice.at("spacing_preference").get<std::string>();
    style.voice_leading_profile.voice_crossing_tolerance =
        voice.at("voice_crossing_tolerance").get<float>();

    const auto& textural = j.at("textural_profile");
    style.textural_profile.average_density = textural.at("average_density").get<float>();
    style.textural_profile.density_range_low = textural.at("density_range_low").get<float>();
    style.textural_profile.density_range_high = textural.at("density_range_high").get<float>();
    style.textural_profile.texture_type_distribution =
        textural.at("texture_type_distribution").get<std::map<std::string, float>>();
    style.textural_profile.register_span_preference =
        textural.at("register_span_preference").get<float>();
    style.textural_profile.density_dynamic_correlation =
        textural.at("density_dynamic_correlation").get<float>();

    const auto& dynamic = j.at("dynamic_profile");
    style.dynamic_profile.dynamic_range_low = dynamic.at("dynamic_range_low").get<std::string>();
    style.dynamic_profile.dynamic_range_high = dynamic.at("dynamic_range_high").get<std::string>();
    style.dynamic_profile.most_frequent_dynamic =
        dynamic.at("most_frequent_dynamic").get<std::string>();
    style.dynamic_profile.dynamic_change_rate = dynamic.at("dynamic_change_rate").get<float>();
    style.dynamic_profile.subito_frequency = dynamic.at("subito_frequency").get<float>();
    style.dynamic_profile.climax_dynamic = dynamic.at("climax_dynamic").get<std::string>();
    style.dynamic_profile.dynamic_arc_shape =
        check_enum(dynamic.at("dynamic_arc_shape"), ContourShape::Complex, "ContourShape", dynamic);

    if (j.contains("orchestration_profile")) {
        OrchestrationStyleProfile orchestration;
        const auto& encoded = j.at("orchestration_profile");
        orchestration.preferred_instruments =
            encoded.at("preferred_instruments").get<std::map<std::string, float>>();
        for (const auto& combination : encoded.at("signature_combinations"))
            orchestration.signature_combinations.push_back(
                instrument_combination_f_v3(combination));
        for (const auto& doubling : encoded.at("doubling_preferences"))
            orchestration.doubling_preferences.push_back(doubling_pattern_f_v3(doubling));
        orchestration.melody_assignment_preference =
            encoded.at("melody_assignment_preference").get<std::map<std::string, float>>();
        orchestration.tutti_proportion = encoded.at("tutti_proportion").get<float>();
        orchestration.solo_proportion = encoded.at("solo_proportion").get<float>();
        orchestration.build_up_technique =
            encoded.at("build_up_technique").get<std::vector<std::string>>();
        orchestration.colour_signature =
            encoded.at("colour_signature").get<std::vector<std::string>>();
        style.orchestration_profile = std::move(orchestration);
    }

    const auto& motivic = j.at("motivic_profile");
    style.motivic_profile.thematic_economy = motivic.at("thematic_economy").get<float>();
    style.motivic_profile.preferred_transformations = numeric_map_f<std::uint8_t, float>(
        motivic.at("preferred_transformations"), "thematic-transformation code");
    style.motivic_profile.development_density = motivic.at("development_density").get<float>();
    style.motivic_profile.fragmentation_frequency =
        motivic.at("fragmentation_frequency").get<float>();
    style.motivic_profile.sequence_frequency = motivic.at("sequence_frequency").get<float>();
    style.motivic_profile.cross_movement_thematic_links =
        motivic.at("cross_movement_thematic_links").get<bool>();

    for (const auto& encoded : j.at("signature_patterns"))
        style.signature_patterns.push_back(signature_pattern_f_v3(encoded));
    style.sample_size =
        detail::checked_integer<std::uint32_t>(j.at("sample_size"), "style sample size");
    style.confidence = j.at("confidence").get<float>();
    return style;
}

// =============================================================================
// Period Profile
// =============================================================================

[[maybe_unused]] json period_profile_j(const PeriodProfile& pp) {
    json work_ids = json::array();
    for (const auto& w : pp.works)
        work_ids.push_back(w.value);
    return {{"label", pp.label},
            {"year_start", pp.year_start},
            {"year_end", pp.year_end},
            {"works", work_ids},
            {"profile", style_profile_j(pp.profile)}};
}

PeriodProfile period_profile_f_v1(const json& j) {
    PeriodProfile pp;
    pp.label = j.value("label", "");
    pp.year_start =
        detail::checked_integer_or<std::uint16_t>(j, "year_start", 0, "period start year");
    pp.year_end = detail::checked_integer_or<std::uint16_t>(j, "year_end", 0, "period end year");
    if (j.contains("works"))
        for (const auto& id : j["works"])
            pp.works.push_back(
                IngestedWorkId{detail::checked_integer<std::uint64_t>(id, "period work id")});
    if (j.contains("profile")) pp.profile = style_profile_f_v1(j["profile"]);
    return pp;
}

PeriodProfile period_profile_f_v2(const json& j) {
    PeriodProfile pp;
    pp.label = j.at("label").get<std::string>();
    pp.year_start = detail::checked_integer<std::uint16_t>(j.at("year_start"), "period start year");
    pp.year_end = detail::checked_integer<std::uint16_t>(j.at("year_end"), "period end year");
    for (const auto& id : j.at("works"))
        pp.works.push_back(
            IngestedWorkId{detail::checked_integer<std::uint64_t>(id, "period work id")});
    pp.profile = style_profile_f_v2(j.at("profile"));
    return pp;
}

json period_profile_j_v3(const PeriodProfile& period) {
    json work_ids = json::array();
    for (const auto work_id : period.works)
        work_ids.push_back(work_id.value);
    return {{"label", period.label},
            {"year_start", period.year_start},
            {"year_end", period.year_end},
            {"works", work_ids},
            {"profile", style_profile_j_v3(period.profile)}};
}

PeriodProfile period_profile_f_v3(const json& j) {
    PeriodProfile period;
    period.label = j.at("label").get<std::string>();
    period.year_start =
        detail::checked_integer<std::uint16_t>(j.at("year_start"), "period start year");
    period.year_end = detail::checked_integer<std::uint16_t>(j.at("year_end"), "period end year");
    for (const auto& encoded : j.at("works"))
        period.works.push_back(
            IngestedWorkId{detail::checked_integer<std::uint64_t>(encoded, "period work id")});
    period.profile = style_profile_f_v3(j.at("profile"));
    return period;
}

// =============================================================================
// Schema version dispatch
// =============================================================================

// Reads "schema_version" via at(): a document without it is refused
// (json::out_of_range -> FormatError at the entry point).
int read_schema_version(const json& j) {
    return detail::checked_integer<int>(j.at("schema_version"), "Corpus schema version");
}

bool version_out_of_range(int version) {
    return version < 1 || version > CORPUS_IR_SCHEMA_VERSION;
}

} // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

json composer_profile_to_json(const ComposerProfile& profile) {
    json work_ids = json::array();
    for (const auto& w : profile.works)
        work_ids.push_back(w.value);

    json periods = json::array();
    for (const auto& pp : profile.period_profiles)
        periods.push_back(period_profile_j_v3(pp));

    json j = {{"schema_version", CORPUS_IR_SCHEMA_VERSION},
              {"id", profile.id.value},
              {"name", profile.name},
              {"works", work_ids},
              {"style_profile", style_profile_j_v3(profile.style_profile)},
              {"period_profiles", periods},
              {"tags", profile.tags}};
    if (profile.birth_year) j["birth_year"] = *profile.birth_year;
    if (profile.death_year) j["death_year"] = *profile.death_year;
    if (profile.active_period)
        j["active_period"] = {{"start", profile.active_period->first},
                              {"end", profile.active_period->second}};
    if (profile.tradition) j["tradition"] = *profile.tradition;
    return j;
}

Result<ComposerProfile> composer_profile_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version)) return std::unexpected(ErrorCode::FormatError);

        ComposerProfile profile;
        profile.id = ComposerProfileId{
            detail::checked_integer<std::uint64_t>(j.at("id"), "composer profile id")};
        profile.name = j.at("name").get<std::string>();
        if (j.contains("birth_year"))
            profile.birth_year =
                detail::checked_integer<std::uint16_t>(j["birth_year"], "composer birth year");
        if (j.contains("death_year"))
            profile.death_year =
                detail::checked_integer<std::uint16_t>(j["death_year"], "composer death year");
        if (j.contains("tradition")) profile.tradition = j["tradition"].get<std::string>();

        if (version == 1) {
            if (j.contains("works"))
                for (const auto& id : j["works"])
                    profile.works.push_back(IngestedWorkId{
                        detail::checked_integer<std::uint64_t>(id, "composer work id")});
            if (j.contains("style_profile"))
                profile.style_profile = style_profile_f_v1(j["style_profile"]);
            if (j.contains("period_profiles"))
                for (const auto& pp : j["period_profiles"])
                    profile.period_profiles.push_back(period_profile_f_v1(pp));
            if (j.contains("tags")) profile.tags = j["tags"].get<std::vector<std::string>>();
        } else if (version == 2) {
            for (const auto& id : j.at("works"))
                profile.works.push_back(
                    IngestedWorkId{detail::checked_integer<std::uint64_t>(id, "composer work id")});
            profile.style_profile = style_profile_f_v2(j.at("style_profile"));
            for (const auto& pp : j.at("period_profiles"))
                profile.period_profiles.push_back(period_profile_f_v2(pp));
            profile.tags = j.at("tags").get<std::vector<std::string>>();
        } else {
            if (j.contains("active_period")) {
                const auto& active = j.at("active_period");
                profile.active_period =
                    std::pair{detail::checked_integer<std::uint16_t>(active.at("start"),
                                                                     "active-period start year"),
                              detail::checked_integer<std::uint16_t>(active.at("end"),
                                                                     "active-period end year")};
            }
            for (const auto& id : j.at("works"))
                profile.works.push_back(
                    IngestedWorkId{detail::checked_integer<std::uint64_t>(id, "composer work id")});
            profile.style_profile = style_profile_f_v3(j.at("style_profile"));
            for (const auto& pp : j.at("period_profiles"))
                profile.period_profiles.push_back(period_profile_f_v3(pp));
            profile.tags = j.at("tags").get<std::vector<std::string>>();
        }
        return profile;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

json ingested_work_to_json(const IngestedWork& work) {
    json j = {{"schema_version", CORPUS_IR_SCHEMA_VERSION},
              {"id", work.id.value},
              {"metadata", metadata_j(work.metadata)},
              {"ingestion_confidence", confidence_j(work.ingestion_confidence)},
              {"analysis", work_analysis_j_v3(work.analysis)},
              {"analysis_complete", work.analysis_complete}};
    if (work.score) j["score"] = score_to_json(*work.score);
    return j;
}

Result<IngestedWork> ingested_work_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version)) return std::unexpected(ErrorCode::FormatError);

        IngestedWork work;
        work.id =
            IngestedWorkId{detail::checked_integer<std::uint64_t>(j.at("id"), "ingested work id")};

        if (version == 1) {
            work.metadata = metadata_f_v1(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v1(j.at("ingestion_confidence"));
            if (j.contains("analysis")) work.analysis = work_analysis_f_v1(j["analysis"]);
            work.analysis_complete = j.value("analysis_complete", false);
            if (j.contains("score")) {
                // Lenient v1 behaviour: an unreadable embedded score is dropped.
                auto score_result = score_from_json(j["score"]);
                if (score_result) work.score = std::move(*score_result);
            }
        } else if (version == 2) {
            work.metadata = metadata_f_v2(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v2(j.at("ingestion_confidence"));
            work.analysis = work_analysis_f_v2(j.at("analysis"));
            work.analysis_complete = j.at("analysis_complete").get<bool>();
            if (j.contains("score")) {
                auto score_result = score_from_json(j["score"]);
                if (!score_result) return std::unexpected(score_result.error());
                work.score = std::move(*score_result);
            }
        } else if (version == 3) {
            work.metadata = metadata_f_v2(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v2(j.at("ingestion_confidence"));
            work.analysis = work_analysis_f_v3(j.at("analysis"));
            work.analysis_complete = j.at("analysis_complete").get<bool>();
            if (j.contains("score")) {
                auto score_result = score_from_json(j["score"]);
                if (!score_result) return std::unexpected(score_result.error());
                work.score = std::move(*score_result);
            }
        } else {
            work.metadata = metadata_f_v2(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v4(j.at("ingestion_confidence"));
            work.analysis = work_analysis_f_v3(j.at("analysis"));
            work.analysis_complete = j.at("analysis_complete").get<bool>();
            if (j.contains("score")) {
                auto score_result = score_from_json(j["score"]);
                if (!score_result) return std::unexpected(score_result.error());
                work.score = std::move(*score_result);
            }
        }
        return work;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

json corpus_to_json(const CorpusDatabase& corpus) {
    json composers = json::object();
    for (const auto& [id, profile] : corpus.composers)
        composers[std::to_string(id)] = composer_profile_to_json(profile);

    json works = json::object();
    for (const auto& [id, work] : corpus.works)
        works[std::to_string(id)] = ingested_work_to_json(work);

    return {
        {"schema_version", CORPUS_IR_SCHEMA_VERSION}, {"composers", composers}, {"works", works}};
}

Result<CorpusDatabase> corpus_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version)) return std::unexpected(ErrorCode::FormatError);

        // Nested composer and work objects carry their own schema_version,
        // so the per-object entry points dispatch v1-v4 for themselves.
        CorpusDatabase corpus;
        if (version == 1) {
            if (j.contains("composers")) {
                for (const auto& [key, val] : j["composers"].items()) {
                    auto profile = composer_profile_from_json(val);
                    if (!profile) return std::unexpected(profile.error());
                    const auto id = detail::checked_decimal_integer<std::uint64_t>(
                        key, "composer map key", val);
                    corpus.composers[id] = std::move(*profile);
                }
            }
            if (j.contains("works")) {
                for (const auto& [key, val] : j["works"].items()) {
                    auto work = ingested_work_from_json(val);
                    if (!work) return std::unexpected(work.error());
                    const auto id =
                        detail::checked_decimal_integer<std::uint64_t>(key, "work map key", val);
                    corpus.works[id] = std::move(*work);
                }
            }
            // v1 loads stay lenient: no validate-on-load, old files keep loading.
            return corpus;
        }

        for (const auto& [key, val] : j.at("composers").items()) {
            auto profile = composer_profile_from_json(val);
            if (!profile) return std::unexpected(profile.error());
            const auto id =
                detail::checked_decimal_integer<std::uint64_t>(key, "composer map key", val);
            corpus.composers[id] = std::move(*profile);
        }
        for (const auto& [key, val] : j.at("works").items()) {
            auto work = ingested_work_from_json(val);
            if (!work) return std::unexpected(work.error());
            const auto id =
                detail::checked_decimal_integer<std::uint64_t>(key, "work map key", val);
            corpus.works[id] = std::move(*work);
        }

        // Validate-on-load (v2-v4): Error-severity diagnostics block loading.
        for (const auto& diag : validate_corpus(corpus)) {
            // Schema v2 persisted caller-supplied partial aggregates and had no
            // freshness invariant. Preserve that historical migration contract;
            // v3+ and direct validation enforce C15.
            if (version == 2 && diag.rule == "C15") continue;
            if (diag.severity == ValidationSeverity::Error)
                return std::unexpected(ErrorCode::ValidationOnLoadFailed);
        }
        return corpus;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

std::string corpus_to_json_string(const CorpusDatabase& corpus, int indent) {
    return corpus_to_json(corpus).dump(indent);
}

Result<CorpusDatabase> corpus_from_json_string(const std::string& json_str) {
    try {
        auto j = json::parse(json_str);
        return corpus_from_json(j);
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

} // namespace sunny::core
