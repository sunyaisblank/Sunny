/**
 * @file CISZ001A.cpp
 * @brief Corpus IR serialisation — implementation
 *
 * Component: CISZ001A
 *
 * Tests: TSCI004A, TSCI009A
 */

#include "CISZ001A.h"
#include "CIVD001A.h"
#include "../Score/SISZ001A.h"
#include "../Score/SISZ002A.h"

namespace Sunny::Core {

using json = nlohmann::json;

namespace {

// =============================================================================
// Score Time — v1 back-compat reader
//
// Schema v1 stored ScoreTime flat as {"bar", "beat_n", "beat_d"}; v2 uses
// the shared nested scheme from SISZ002A. The write path is v2-only.
// =============================================================================

ScoreTime score_time_from_flat_v1(const json& j) {
    ScoreTime t;
    t.bar = j.value("bar", static_cast<std::uint32_t>(0));
    t.beat.numerator = j.value("beat_n", static_cast<std::int64_t>(0));
    t.beat.denominator = j.value("beat_d", static_cast<std::int64_t>(1));
    return t;
}

// =============================================================================
// Ingestion Confidence
// =============================================================================

json confidence_j(const IngestionConfidence& ic) {
    json j = {
        {"key_confidence", ic.key_confidence},
        {"metre_confidence", ic.metre_confidence},
        {"spelling_confidence", ic.spelling_confidence},
        {"voice_separation_confidence", ic.voice_separation_confidence},
        {"quantisation_residual", ic.quantisation_residual},
        {"source_format", ic.source_format}
    };
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
    ic.source_format = j.value("source_format", "");
    if (j.contains("manual_corrections")) {
        for (const auto& mc : j["manual_corrections"])
            ic.manual_corrections.push_back({
                mc.value("field", ""), mc.value("description", "")});
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
    ic.source_format = j.at("source_format").get<std::string>();
    for (const auto& mc : j.at("manual_corrections"))
        ic.manual_corrections.push_back({
            mc.at("field").get<std::string>(),
            mc.at("description").get<std::string>()});
    return ic;
}

// =============================================================================
// Work Metadata
// =============================================================================

json metadata_j(const WorkMetadata& m) {
    json j = {
        {"title", m.title},
        {"composer", m.composer.value},
        {"instrumentation", m.instrumentation},
        {"source_format", m.source_format},
        {"is_reduction", m.is_reduction},
        {"tags", m.tags}
    };
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
    if (j.contains("year_composed")) m.year_composed = j["year_composed"].get<std::uint16_t>();
    if (j.contains("period")) m.period = j["period"].get<std::string>();
    if (j.contains("genre")) m.genre = j["genre"].get<std::string>();
    if (j.contains("source_description")) m.source_description = j["source_description"].get<std::string>();
    if (j.contains("original_instrumentation")) m.original_instrumentation = j["original_instrumentation"].get<std::string>();
    if (j.contains("movement")) m.movement = j["movement"].get<std::string>();
}

WorkMetadata metadata_f_v1(const json& j) {
    WorkMetadata m;
    m.title = j.value("title", "");
    m.composer = ComposerRef{j.value("composer", static_cast<std::uint64_t>(0))};
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
    m.composer = ComposerRef{j.at("composer").get<std::uint64_t>()};
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
        for (const auto& o : p.occurrences) occ.push_back(score_time_to_json(o));
        progs.push_back({
            {"roman_numerals", p.roman_numerals},
            {"length", p.length},
            {"occurrences", occ},
            {"key_context", p.key_context}
        });
    }

    json mods = json::array();
    for (const auto& m : h.modulation_inventory) {
        json mj = {
            {"position", score_time_to_json(m.position)},
            {"from_key", m.from_key},
            {"to_key", m.to_key},
            {"technique", static_cast<int>(m.technique)}
        };
        if (m.pivot_chord) mj["pivot_chord"] = *m.pivot_chord;
        mods.push_back(mj);
    }

    json cads = json::array();
    for (const auto& c : h.cadence_inventory) {
        cads.push_back({
            {"position", score_time_to_json(c.position)},
            {"type", c.type},
            {"approach", c.approach},
            {"section_context", c.section_context},
            {"is_structural", c.is_structural}
        });
    }

    return {
        {"chord_vocabulary", h.chord_vocabulary},
        {"progression_inventory", progs},
        {"modulation_inventory", mods},
        {"harmonic_rhythm", {
            {"changes_per_bar", h.harmonic_rhythm.changes_per_bar},
            {"mean_rate", h.harmonic_rhythm.mean_rate},
            {"variance", h.harmonic_rhythm.variance},
            {"rate_by_section", h.harmonic_rhythm.rate_by_section}
        }},
        {"cadence_inventory", cads},
        {"tonal_plan", {
            {"key_area_count", h.tonal_plan.key_area_count},
            {"most_distant_key", h.tonal_plan.most_distant_key}
        }}
    };
}

HarmonicAnalysisRecord harmonic_analysis_f_v1(const json& j) {
    HarmonicAnalysisRecord h;
    if (j.contains("chord_vocabulary"))
        h.chord_vocabulary = j["chord_vocabulary"].get<std::map<std::string, std::uint32_t>>();

    if (j.contains("progression_inventory")) {
        for (const auto& pj : j["progression_inventory"]) {
            ProgressionPattern p;
            p.roman_numerals = pj.value("roman_numerals", std::vector<std::string>{});
            p.length = pj.value("length", static_cast<std::uint8_t>(0));
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
        h.tonal_plan.key_area_count = j["tonal_plan"].value("key_area_count", static_cast<std::uint32_t>(1));
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
        p.length = pj.at("length").get<std::uint8_t>();
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
        m.technique = static_cast<ModulationTechnique>(mj.at("technique").get<int>());
        if (mj.contains("pivot_chord")) m.pivot_chord = mj["pivot_chord"].get<std::string>();
        h.modulation_inventory.push_back(std::move(m));
    }

    const auto& hr = j.at("harmonic_rhythm");
    h.harmonic_rhythm.changes_per_bar = hr.at("changes_per_bar").get<std::vector<float>>();
    h.harmonic_rhythm.mean_rate = hr.at("mean_rate").get<float>();
    h.harmonic_rhythm.variance = hr.at("variance").get<float>();
    h.harmonic_rhythm.rate_by_section = hr.at("rate_by_section").get<std::map<std::string, float>>();

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
    h.tonal_plan.key_area_count = tp.at("key_area_count").get<std::uint32_t>();
    h.tonal_plan.most_distant_key = tp.at("most_distant_key").get<std::string>();

    return h;
}

// =============================================================================
// Formal Analysis
// =============================================================================

json formal_section_j(const FormalSection& s) {
    json subs = json::array();
    for (const auto& sub : s.subsections) subs.push_back(formal_section_j(sub));
    json j = {
        {"label", s.label},
        {"start_bar", s.start_bar},
        {"end_bar", s.end_bar},
        {"length_bars", s.length_bars},
        {"key", s.key},
        {"tempo", s.tempo},
        {"subsections", subs}
    };
    if (s.character) j["character"] = *s.character;
    return j;
}

FormalSection formal_section_f_v1(const json& j) {
    FormalSection s;
    s.label = j.value("label", "");
    s.start_bar = j.value("start_bar", static_cast<std::uint32_t>(0));
    s.end_bar = j.value("end_bar", static_cast<std::uint32_t>(0));
    s.length_bars = j.value("length_bars", static_cast<std::uint32_t>(0));
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
    s.start_bar = j.at("start_bar").get<std::uint32_t>();
    s.end_bar = j.at("end_bar").get<std::uint32_t>();
    s.length_bars = j.at("length_bars").get<std::uint32_t>();
    s.key = j.at("key").get<std::string>();
    s.tempo = j.at("tempo").get<float>();
    if (j.contains("character")) s.character = j["character"].get<std::string>();
    for (const auto& sub : j.at("subsections"))
        s.subsections.push_back(formal_section_f_v2(sub));
    return s;
}

json formal_analysis_j(const FormalAnalysisRecord& f) {
    json sections = json::array();
    for (const auto& s : f.section_plan) sections.push_back(formal_section_j(s));

    json proportions = json::array();
    for (const auto& p : f.proportions) {
        proportions.push_back({
            {"label", p.label},
            {"proportion", p.proportion},
            {"golden_ratio_proximity", p.golden_ratio_proximity}
        });
    }

    return {
        {"section_plan", sections},
        {"form_type", static_cast<int>(f.form_type)},
        {"total_duration_bars", f.total_duration_bars},
        {"proportions", proportions}
    };
}

FormalAnalysisRecord formal_analysis_f_v1(const json& j) {
    FormalAnalysisRecord f;
    if (j.contains("section_plan"))
        for (const auto& sj : j["section_plan"])
            f.section_plan.push_back(formal_section_f_v1(sj));
    f.form_type = static_cast<FormClassification>(j.value("form_type", 20));  // Other
    f.total_duration_bars = j.value("total_duration_bars", static_cast<std::uint32_t>(0));
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
    f.form_type = static_cast<FormClassification>(j.at("form_type").get<int>());
    f.total_duration_bars = j.at("total_duration_bars").get<std::uint32_t>();
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

json work_analysis_j(const WorkAnalysis& wa) {
    return {
        {"harmonic_analysis", harmonic_analysis_j(wa.harmonic_analysis)},
        {"formal_analysis", formal_analysis_j(wa.formal_analysis)},
        {"rhythmic_analysis", {
            {"syncopation_index", wa.rhythmic_analysis.syncopation_index},
            {"metrical_complexity", wa.rhythmic_analysis.metrical_complexity},
            {"rest_proportion", wa.rhythmic_analysis.rest_proportion}
        }},
        {"voice_leading_analysis", {
            {"parallel_fifths_count", wa.voice_leading_analysis.parallel_fifths_count},
            {"parallel_octaves_count", wa.voice_leading_analysis.parallel_octaves_count},
            {"contrary_motion_proportion", wa.voice_leading_analysis.contrary_motion_proportion},
            {"common_tone_retention_rate", wa.voice_leading_analysis.common_tone_retention_rate},
            {"average_voice_independence", wa.voice_leading_analysis.average_voice_independence}
        }},
        {"textural_analysis", {
            {"average_density", wa.textural_analysis.average_density}
        }},
        {"dynamic_analysis", {
            {"climax_position", wa.dynamic_analysis.climax_position},
            {"hairpin_count", wa.dynamic_analysis.hairpin_count}
        }},
        {"motivic_analysis", {
            {"thematic_density", wa.motivic_analysis.thematic_density},
            {"thematic_economy", wa.motivic_analysis.thematic_economy}
        }}
    };
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
        wa.voice_leading_analysis.parallel_fifths_count =
            vl.value("parallel_fifths_count", static_cast<std::uint32_t>(0));
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
    wa.voice_leading_analysis.parallel_fifths_count =
        vl.at("parallel_fifths_count").get<std::uint32_t>();
    wa.voice_leading_analysis.parallel_octaves_count =
        vl.at("parallel_octaves_count").get<std::uint32_t>();
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
    wa.dynamic_analysis.hairpin_count = da.at("hairpin_count").get<std::uint32_t>();

    const auto& ma = j.at("motivic_analysis");
    wa.motivic_analysis.thematic_density = ma.at("thematic_density").get<float>();
    wa.motivic_analysis.thematic_economy = ma.at("thematic_economy").get<float>();

    return wa;
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
        sigs.push_back({
            {"id", pat.id.value},
            {"description", pat.description},
            {"domain", static_cast<int>(pat.domain)},
            {"distinctiveness", pat.distinctiveness},
            {"examples", examples}
        });
    }

    return {
        {"harmonic_profile", {
            {"chord_vocabulary_size", sp.harmonic_profile.chord_vocabulary_size},
            {"chord_frequency", sp.harmonic_profile.chord_frequency},
            {"harmonic_rhythm_mean", sp.harmonic_profile.harmonic_rhythm_mean},
            {"modulation_frequency", sp.harmonic_profile.modulation_frequency},
            {"chromatic_density", sp.harmonic_profile.chromatic_density}
        }},
        {"melodic_profile", {
            {"conjunct_proportion", sp.melodic_profile.conjunct_proportion},
            {"chromaticism_rate", sp.melodic_profile.chromaticism_rate},
            {"average_phrase_length", sp.melodic_profile.average_phrase_length}
        }},
        {"rhythmic_profile", {
            {"syncopation_index", sp.rhythmic_profile.syncopation_index},
            {"metrical_complexity", sp.rhythmic_profile.metrical_complexity}
        }},
        {"formal_profile", {
            {"average_work_length", sp.formal_profile.average_work_length},
            {"climax_placement", sp.formal_profile.climax_placement}
        }},
        {"voice_leading_profile", {
            {"common_tone_retention", sp.voice_leading_profile.common_tone_retention},
            {"voice_independence_index", sp.voice_leading_profile.voice_independence_index}
        }},
        {"signature_patterns", sigs},
        {"sample_size", sp.sample_size},
        {"confidence", sp.confidence}
    };
}

StyleProfile style_profile_f_v1(const json& j) {
    StyleProfile sp;
    if (j.contains("harmonic_profile")) {
        const auto& hp = j["harmonic_profile"];
        sp.harmonic_profile.chord_vocabulary_size =
            hp.value("chord_vocabulary_size", static_cast<std::uint32_t>(0));
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
    sp.sample_size = j.value("sample_size", static_cast<std::uint32_t>(0));
    sp.confidence = j.value("confidence", 0.0f);
    return sp;
}

StyleProfile style_profile_f_v2(const json& j) {
    StyleProfile sp;

    const auto& hp = j.at("harmonic_profile");
    sp.harmonic_profile.chord_vocabulary_size =
        hp.at("chord_vocabulary_size").get<std::uint32_t>();
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
    sp.voice_leading_profile.common_tone_retention =
        vp.at("common_tone_retention").get<float>();
    sp.voice_leading_profile.voice_independence_index =
        vp.at("voice_independence_index").get<float>();

    for (const auto& sj : j.at("signature_patterns")) {
        SignaturePattern pat;
        pat.id = SignaturePatternId{sj.at("id").get<std::uint64_t>()};
        pat.description = sj.at("description").get<std::string>();
        pat.domain = static_cast<PatternDomain>(sj.at("domain").get<int>());
        pat.distinctiveness = sj.at("distinctiveness").get<float>();
        for (const auto& ej : sj.at("examples"))
            pat.examples.emplace_back(
                IngestedWorkId{ej.at("work_id").get<std::uint64_t>()},
                score_time_from_json(ej.at("position")));
        sp.signature_patterns.push_back(std::move(pat));
    }

    sp.sample_size = j.at("sample_size").get<std::uint32_t>();
    sp.confidence = j.at("confidence").get<float>();
    return sp;
}

// =============================================================================
// Period Profile
// =============================================================================

json period_profile_j(const PeriodProfile& pp) {
    json work_ids = json::array();
    for (const auto& w : pp.works) work_ids.push_back(w.value);
    return {
        {"label", pp.label},
        {"year_start", pp.year_start},
        {"year_end", pp.year_end},
        {"works", work_ids},
        {"profile", style_profile_j(pp.profile)}
    };
}

PeriodProfile period_profile_f_v1(const json& j) {
    PeriodProfile pp;
    pp.label = j.value("label", "");
    pp.year_start = j.value("year_start", static_cast<std::uint16_t>(0));
    pp.year_end = j.value("year_end", static_cast<std::uint16_t>(0));
    if (j.contains("works"))
        for (auto id : j["works"])
            pp.works.push_back(IngestedWorkId{id.get<std::uint64_t>()});
    if (j.contains("profile"))
        pp.profile = style_profile_f_v1(j["profile"]);
    return pp;
}

PeriodProfile period_profile_f_v2(const json& j) {
    PeriodProfile pp;
    pp.label = j.at("label").get<std::string>();
    pp.year_start = j.at("year_start").get<std::uint16_t>();
    pp.year_end = j.at("year_end").get<std::uint16_t>();
    for (auto id : j.at("works"))
        pp.works.push_back(IngestedWorkId{id.get<std::uint64_t>()});
    pp.profile = style_profile_f_v2(j.at("profile"));
    return pp;
}

// =============================================================================
// Schema version dispatch
// =============================================================================

// Reads "schema_version" via at(): a document without it is refused
// (json::out_of_range -> FormatError at the entry point).
int read_schema_version(const json& j) {
    return j.at("schema_version").get<int>();
}

bool version_out_of_range(int version) {
    return version < 1 || version > CORPUS_IR_SCHEMA_VERSION;
}

}  // anonymous namespace

// =============================================================================
// Public API
// =============================================================================

json composer_profile_to_json(const ComposerProfile& profile) {
    json work_ids = json::array();
    for (const auto& w : profile.works) work_ids.push_back(w.value);

    json periods = json::array();
    for (const auto& pp : profile.period_profiles)
        periods.push_back(period_profile_j(pp));

    json j = {
        {"schema_version", CORPUS_IR_SCHEMA_VERSION},
        {"id", profile.id.value},
        {"name", profile.name},
        {"works", work_ids},
        {"style_profile", style_profile_j(profile.style_profile)},
        {"period_profiles", periods},
        {"tags", profile.tags}
    };
    if (profile.birth_year) j["birth_year"] = *profile.birth_year;
    if (profile.death_year) j["death_year"] = *profile.death_year;
    if (profile.tradition) j["tradition"] = *profile.tradition;
    return j;
}

Result<ComposerProfile> composer_profile_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version))
            return std::unexpected(ErrorCode::FormatError);

        ComposerProfile profile;
        profile.id = ComposerProfileId{j.at("id").get<std::uint64_t>()};
        profile.name = j.at("name").get<std::string>();
        if (j.contains("birth_year")) profile.birth_year = j["birth_year"].get<std::uint16_t>();
        if (j.contains("death_year")) profile.death_year = j["death_year"].get<std::uint16_t>();
        if (j.contains("tradition")) profile.tradition = j["tradition"].get<std::string>();

        if (version == 1) {
            if (j.contains("works"))
                for (auto id : j["works"])
                    profile.works.push_back(IngestedWorkId{id.get<std::uint64_t>()});
            if (j.contains("style_profile"))
                profile.style_profile = style_profile_f_v1(j["style_profile"]);
            if (j.contains("period_profiles"))
                for (const auto& pp : j["period_profiles"])
                    profile.period_profiles.push_back(period_profile_f_v1(pp));
            if (j.contains("tags"))
                profile.tags = j["tags"].get<std::vector<std::string>>();
        } else {
            for (auto id : j.at("works"))
                profile.works.push_back(IngestedWorkId{id.get<std::uint64_t>()});
            profile.style_profile = style_profile_f_v2(j.at("style_profile"));
            for (const auto& pp : j.at("period_profiles"))
                profile.period_profiles.push_back(period_profile_f_v2(pp));
            profile.tags = j.at("tags").get<std::vector<std::string>>();
        }
        return profile;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

json ingested_work_to_json(const IngestedWork& work) {
    json j = {
        {"schema_version", CORPUS_IR_SCHEMA_VERSION},
        {"id", work.id.value},
        {"metadata", metadata_j(work.metadata)},
        {"ingestion_confidence", confidence_j(work.ingestion_confidence)},
        {"analysis", work_analysis_j(work.analysis)},
        {"analysis_complete", work.analysis_complete}
    };
    if (work.score)
        j["score"] = score_to_json(*work.score);
    return j;
}

Result<IngestedWork> ingested_work_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version))
            return std::unexpected(ErrorCode::FormatError);

        IngestedWork work;
        work.id = IngestedWorkId{j.at("id").get<std::uint64_t>()};

        if (version == 1) {
            work.metadata = metadata_f_v1(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v1(j.at("ingestion_confidence"));
            if (j.contains("analysis"))
                work.analysis = work_analysis_f_v1(j["analysis"]);
            work.analysis_complete = j.value("analysis_complete", false);
            if (j.contains("score")) {
                // Lenient v1 behaviour: an unreadable embedded score is dropped.
                auto score_result = score_from_json(j["score"]);
                if (score_result)
                    work.score = std::move(*score_result);
            }
        } else {
            work.metadata = metadata_f_v2(j.at("metadata"));
            work.ingestion_confidence = confidence_f_v2(j.at("ingestion_confidence"));
            work.analysis = work_analysis_f_v2(j.at("analysis"));
            work.analysis_complete = j.at("analysis_complete").get<bool>();
            if (j.contains("score")) {
                auto score_result = score_from_json(j["score"]);
                if (!score_result)
                    return std::unexpected(score_result.error());
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
        {"schema_version", CORPUS_IR_SCHEMA_VERSION},
        {"composers", composers},
        {"works", works}
    };
}

Result<CorpusDatabase> corpus_from_json(const json& j) {
    try {
        const int version = read_schema_version(j);
        if (version_out_of_range(version))
            return std::unexpected(ErrorCode::FormatError);

        // Nested composer and work objects carry their own schema_version,
        // so the per-object entry points dispatch v1/v2 for themselves.
        CorpusDatabase corpus;
        if (version == 1) {
            if (j.contains("composers")) {
                for (const auto& [key, val] : j["composers"].items()) {
                    auto profile = composer_profile_from_json(val);
                    if (!profile) return std::unexpected(profile.error());
                    corpus.composers[std::stoull(key)] = std::move(*profile);
                }
            }
            if (j.contains("works")) {
                for (const auto& [key, val] : j["works"].items()) {
                    auto work = ingested_work_from_json(val);
                    if (!work) return std::unexpected(work.error());
                    corpus.works[std::stoull(key)] = std::move(*work);
                }
            }
            // v1 loads stay lenient: no validate-on-load, old files keep loading.
            return corpus;
        }

        for (const auto& [key, val] : j.at("composers").items()) {
            auto profile = composer_profile_from_json(val);
            if (!profile) return std::unexpected(profile.error());
            corpus.composers[std::stoull(key)] = std::move(*profile);
        }
        for (const auto& [key, val] : j.at("works").items()) {
            auto work = ingested_work_from_json(val);
            if (!work) return std::unexpected(work.error());
            corpus.works[std::stoull(key)] = std::move(*work);
        }

        // Validate-on-load (v2 only): Error-severity diagnostics block loading.
        for (const auto& diag : validate_corpus(corpus)) {
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

}  // namespace Sunny::Core
