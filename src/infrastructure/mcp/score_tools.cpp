/**
 * @file score_tools.cpp
 * @brief MCP Tool Registration — Score IR implementation
 *
 *
 * Maps MCP tool calls to Score IR workflow functions,
 * query functions, and Infrastructure compilation
 * wrappers. Score state is held in a shared_ptr to
 * a session store captured by the tool handler lambdas.
 */

#include "evidence_encoding.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/queries.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/compilation_workflows.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

// =============================================================================
// Response helpers
// =============================================================================

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

Result<std::uint64_t> pending_score_id(const ScoreSession& session) {
    const auto id = session.next_score_id;
    if (id == 0 || session.scores.contains(id))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    return id;
}

void commit_score_id(ScoreSession& session, std::uint64_t id) {
    if (id != std::numeric_limits<std::uint64_t>::max()) session.next_score_id = id + 1;
}

// =============================================================================
// JSON → domain type parsing helpers
// =============================================================================

std::optional<ScoreTime> score_time_from_json(const json& j) {
    if (!j.is_object() || !j.contains("bar") || !j.contains("beat_n") || !j.contains("beat_d") ||
        !j["bar"].is_number_integer() || !j["beat_n"].is_number_integer() ||
        !j["beat_d"].is_number_integer())
        return std::nullopt;
    auto bar = detail::checked_integer<std::uint32_t>(j["bar"], "score-time bar");
    auto denominator = detail::checked_integer<int>(j["beat_d"], "beat denominator");
    if (bar == 0 || denominator <= 0) return std::nullopt;
    return ScoreTime{
        bar, Beat{detail::checked_integer<int>(j["beat_n"], "beat numerator"), denominator}};
}

std::optional<ScoreRegion> region_from_json(const json& j) {
    if (!j.is_object() || !j.contains("start_bar") || !j.contains("end_bar") ||
        !j["start_bar"].is_number_integer() || !j["end_bar"].is_number_integer())
        return std::nullopt;
    const auto start_bar =
        detail::checked_integer<std::uint32_t>(j["start_bar"], "region start bar");
    const auto end_bar = detail::checked_integer<std::uint32_t>(j["end_bar"], "region end bar");
    if (start_bar == 0 || end_bar == 0 || end_bar < start_bar) return std::nullopt;

    ScoreRegion r;
    r.start = {start_bar, Beat::zero()};
    r.end = {end_bar, Beat::zero()};
    if (j.contains("parts")) {
        if (!j["parts"].is_array()) return std::nullopt;
        for (const auto& p : j["parts"])
            if (!p.is_number_integer()) return std::nullopt;
        for (const auto& p : j["parts"])
            r.parts.push_back(PartId{detail::checked_integer<std::uint64_t>(p, "region part id")});
    }
    return r;
}

std::optional<SpelledPitch> spelled_pitch_from_json(const json& j) {
    if (!j.is_object() || !j.contains("letter") || !j["letter"].is_string() ||
        !j.contains("accidental") || !j["accidental"].is_number_integer() ||
        !j.contains("octave") || !j["octave"].is_number_integer())
        return std::nullopt;
    // Map letter string to index: C=0, D=1,..., B=6
    auto letter_str = j["letter"].get<std::string>();
    if (letter_str.size() != 1) return std::nullopt;
    uint8_t letter;
    switch (letter_str[0]) {
    case 'C':
    case 'c':
        letter = 0;
        break;
    case 'D':
    case 'd':
        letter = 1;
        break;
    case 'E':
    case 'e':
        letter = 2;
        break;
    case 'F':
    case 'f':
        letter = 3;
        break;
    case 'G':
    case 'g':
        letter = 4;
        break;
    case 'A':
    case 'a':
        letter = 5;
        break;
    case 'B':
    case 'b':
        letter = 6;
        break;
    default:
        return std::nullopt;
    }

    auto accidental = detail::checked_integer<int>(j["accidental"], "pitch accidental");
    auto octave = detail::checked_integer<int>(j["octave"], "pitch octave");
    if (accidental < std::numeric_limits<std::int8_t>::min() ||
        accidental > std::numeric_limits<std::int8_t>::max() ||
        octave < std::numeric_limits<std::int8_t>::min() ||
        octave > std::numeric_limits<std::int8_t>::max())
        return std::nullopt;
    return SpelledPitch{
        letter, static_cast<std::int8_t>(accidental), static_cast<std::int8_t>(octave)};
}

std::optional<Beat> beat_from_json(const json& j) {
    if (!j.is_object() || !j.contains("n") || !j["n"].is_number_integer() || !j.contains("d") ||
        !j["d"].is_number_integer())
        return std::nullopt;
    auto denominator = detail::checked_integer<int>(j["d"], "beat denominator");
    if (denominator <= 0) return std::nullopt;
    return Beat{detail::checked_integer<int>(j["n"], "beat numerator"), denominator};
}

std::optional<DiatonicInterval> diatonic_interval_from_json(const json& j) {
    if (!j.is_object() || !j.contains("chromatic") || !j["chromatic"].is_number_integer() ||
        !j.contains("diatonic") || !j["diatonic"].is_number_integer())
        return std::nullopt;
    return DiatonicInterval{detail::checked_integer<int>(j["chromatic"], "chromatic interval"),
                            detail::checked_integer<int>(j["diatonic"], "diatonic interval")};
}

// =============================================================================
// Domain type → JSON serialisation helpers
// =============================================================================

json score_time_j(const ScoreTime& t) {
    return {{"bar", t.bar}, {"beat_n", t.beat.numerator()}, {"beat_d", t.beat.denominator()}};
}

json spelled_pitch_j(const SpelledPitch& sp) {
    static constexpr const char* LETTERS[] = {"C", "D", "E", "F", "G", "A", "B"};
    return {// letter > 6 violates the SpelledPitch invariant; emit "?" rather than
            // aliasing the corrupt value onto a wrong letter name
            {"letter", sp.letter < 7 ? LETTERS[sp.letter] : "?"},
            {"accidental", sp.accidental},
            {"octave", sp.octave}};
}

json form_entry_j(const FormSummaryEntry& f) {
    return {{"label", f.label},
            {"start", score_time_j(f.start)},
            {"end", score_time_j(f.end)},
            {"key_root", spelled_pitch_j(f.key.root)},
            {"key_accidentals", f.key.accidentals},
            {"tempo_bpm", f.tempo_bpm}};
}

json harmonic_annotation_j(const HarmonicAnnotation& ha) {
    json j = {{"position", score_time_j(ha.position)},
              {"duration", {{"n", ha.duration.numerator()}, {"d", ha.duration.denominator()}}}};
    if (!ha.roman_numeral.empty()) j["roman_numeral"] = ha.roman_numeral;
    if (ha.confidence > 0.0) j["confidence"] = ha.confidence;
    // Chord voicing
    json notes = json::array();
    for (const auto& n : ha.chord.notes)
        notes.push_back(static_cast<int>(n));
    j["chord"] = {
        {"root", static_cast<int>(ha.chord.root)}, {"quality", ha.chord.quality}, {"notes", notes}};
    return j;
}

json motif_occurrence_j(const MotifOccurrence& m) {
    return {{"part_id", m.part_id.value}, {"position", score_time_j(m.position)}};
}

/// Extract score_id from params, look up in session, return pointer or null
Score* lookup_score(const std::shared_ptr<ScoreSession>& session, const json& params, json& err) {
    if (!params.contains("score_id")) {
        err = error_response("score_id is required");
        return nullptr;
    }
    auto id = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");
    auto* s = session->find(id);
    if (!s) {
        err = error_response("score not found: " + std::to_string(id));
        return nullptr;
    }
    return s;
}

/// Enum parse helpers

template <typename E> std::optional<E> checked_enum(const json& encoded, int max_val) {
    const auto val = detail::checked_integer<int>(encoded, "enum value");
    if (val < 0 || val > max_val) return std::nullopt;
    return static_cast<E>(val);
}

std::optional<ChordSymbolEvent> chord_symbol_from_json(const json& j) {
    if (!j.contains("root") || !j.contains("quality") || !j["quality"].is_string())
        return std::nullopt;
    auto root = spelled_pitch_from_json(j["root"]);
    if (!root) return std::nullopt;

    ChordSymbolEvent symbol;
    symbol.root = *root;
    symbol.quality = j["quality"].get<std::string>();
    if (j.contains("bass")) {
        auto bass = spelled_pitch_from_json(j["bass"]);
        if (!bass) return std::nullopt;
        symbol.bass = *bass;
    }
    if (j.contains("roman")) {
        if (!j["roman"].is_string()) return std::nullopt;
        symbol.roman = j["roman"].get<std::string>();
    }
    if (j.contains("extensions")) {
        if (!j["extensions"].is_array()) return std::nullopt;
        for (const auto& extension : j["extensions"]) {
            if (!extension.is_string()) return std::nullopt;
            symbol.extensions.push_back(extension.get<std::string>());
        }
    }
    if (j.contains("numeral")) {
        const auto& numeral = j["numeral"];
        if (!numeral.is_object() || !numeral.contains("root") || !numeral.contains("key"))
            return std::nullopt;
        const auto& key = numeral["key"];
        if (!key.is_object() || !key.contains("fifths") || !key.contains("mode"))
            return std::nullopt;
        auto mode = checked_enum<ChordNumeralMode>(key["mode"], 4);
        if (!mode) return std::nullopt;
        symbol.numeral = ChordNumeral{
            detail::checked_integer<std::uint8_t>(numeral["root"], "chord numeral root"),
            detail::checked_integer_or<std::int8_t>(
                numeral, "alteration", 0, "chord numeral alteration"),
            ChordNumeralKey{
                detail::checked_integer<std::int8_t>(key["fifths"], "chord numeral key fifths"),
                *mode}};
    }
    if (j.contains("inversion")) {
        symbol.inversion =
            detail::checked_integer<std::uint16_t>(j["inversion"], "chord inversion");
    }
    if (j.contains("degrees")) {
        if (!j["degrees"].is_array()) return std::nullopt;
        for (const auto& encoded : j["degrees"]) {
            if (!encoded.is_object() || !encoded.contains("value") || !encoded.contains("type"))
                return std::nullopt;
            auto type = checked_enum<ChordDegreeType>(encoded["type"], 2);
            if (!type) return std::nullopt;
            symbol.degrees.push_back(ChordDegree{
                detail::checked_integer<std::uint16_t>(encoded["value"], "chord degree value"),
                detail::checked_integer_or<std::int8_t>(
                    encoded, "alteration", 0, "chord degree alteration"),
                *type});
        }
    }
    return symbol;
}

json mutation_result_j(const MutationResult& mr) {
    json j = {{"ok", true}};
    if (!mr.diagnostics.empty()) {
        json diags = json::array();
        for (const auto& d : mr.diagnostics)
            diags.push_back(mcp_detail::encode_diagnostic(d));
        j["diagnostics"] = diags;
    }
    return j;
}

Result<ScoreTuning> parse_score_tuning(const json& value) {
    if (!value.is_object() || value.size() != 4 || !value.contains("name") ||
        !value["name"].is_string() || !value.contains("reference_midi_note") ||
        !value.contains("reference_frequency_hz") || !value["reference_frequency_hz"].is_number() ||
        !value.contains("cents_from_reference") || !value["cents_from_reference"].is_array() ||
        value["cents_from_reference"].size() != SCORE_TUNING_NOTE_COUNT)
        return std::unexpected(ErrorCode::FormatError);

    ScoreTuning tuning;
    tuning.name = value["name"].get<std::string>();
    tuning.reference_midi_note = detail::checked_integer<std::uint8_t>(
        value["reference_midi_note"], "tuning reference MIDI note");
    tuning.reference_frequency_hz = value["reference_frequency_hz"].get<double>();
    for (std::size_t note = 0; note < SCORE_TUNING_NOTE_COUNT; ++note) {
        if (!value["cents_from_reference"][note].is_number())
            return std::unexpected(ErrorCode::FormatError);
        tuning.cents_from_reference[note] = value["cents_from_reference"][note].get<double>();
    }
    if (auto valid = validate_score_tuning(tuning); !valid) return std::unexpected(valid.error());
    return tuning;
}

} // anonymous namespace

void register_score_tools(McpServer& server,
                          LomTransport* transport,
                          std::shared_ptr<ScoreSession> session) {
    if (!session) session = std::make_shared<ScoreSession>();

    // =========================================================================
    // Composition Tools
    // =========================================================================

    server.register_tool(
        "score_create",
        "Create a new score from a specification",
        {{"title", "string (optional, default Untitled)"},
         {"total_bars", "integer (optional, default 16)"},
         {"bpm", "number (optional, default 120)"},
         {"key_root", "object {letter, accidental, octave} (optional, default C4)"},
         {"minor", "boolean (optional)"},
         {"key_accidentals", "integer (optional)"},
         {"time_sig_num", "integer (optional)"},
         {"time_sig_den", "integer (optional)"},
         {"tuning",
          "object {name, reference_midi_note, reference_frequency_hz, "
          "cents_from_reference[128]} (optional, exact fields)"},
         {"parts",
          "array of {name, abbreviation, instrument_type (integer), clef (integer, optional)} "
          "(optional)"}},
        [session](const json& params) -> json {
            ScoreSpec spec;
            spec.title = params.value("title", "Untitled");
            spec.total_bars = detail::checked_integer_or<std::uint32_t>(
                params, "total_bars", 16, "total bar count");
            spec.bpm = params.value("bpm", 120.0);

            if (params.contains("key_root")) {
                auto key_root = spelled_pitch_from_json(params["key_root"]);
                if (!key_root) return error_response("invalid key_root");
                spec.key_root = *key_root;
            } else
                spec.key_root = SpelledPitch{0, 0, 4}; // C4

            spec.minor = params.value("minor", false);
            if (params.contains("key_accidentals"))
                spec.key_accidentals = detail::checked_integer<std::int8_t>(
                    params.at("key_accidentals"), "key accidental count");
            spec.time_sig_num =
                detail::checked_integer_or<int>(params, "time_sig_num", 4, "time numerator");
            spec.time_sig_den =
                detail::checked_integer_or<int>(params, "time_sig_den", 4, "time denominator");
            if (params.contains("tuning")) {
                auto tuning = parse_score_tuning(params["tuning"]);
                if (!tuning) return error_response("invalid score tuning");
                spec.tuning = std::move(*tuning);
            }

            if (params.contains("parts") && params["parts"].is_array()) {
                for (const auto& pj : params["parts"]) {
                    if (!pj.is_object() || !pj.contains("name") || !pj["name"].is_string() ||
                        !pj.contains("instrument_type") ||
                        !pj["instrument_type"].is_number_integer())
                        return error_response("each part requires name and instrument_type");
                    PartDefinition pd;
                    pd.name = pj["name"].get<std::string>();
                    pd.abbreviation = pj.value("abbreviation", "Pt.");
                    auto it = checked_enum<InstrumentType>(pj["instrument_type"], 80);
                    if (!it) return error_response("invalid instrument_type");
                    pd.instrument_type = *it;
                    if (pj.contains("clef")) {
                        auto cl = checked_enum<Clef>(pj["clef"], 5);
                        if (!cl) return error_response("invalid clef");
                        pd.clef = *cl;
                    }
                    spec.parts.push_back(std::move(pd));
                }
            }

            spec.id = ScoreId{session->next_score_id};
            auto result = create_score(spec);
            if (!result)
                return error_response("create_score failed: error " +
                                      std::to_string(static_cast<int>(result.error())));

            auto pending_id = pending_score_id(*session);
            if (!pending_id) return error_response("score identity domain exhausted");
            const auto id = *pending_id;
            session->scores[id] = std::move(*result);
            commit_score_id(*session, id);

            json part_ids = json::array();
            for (const auto& part : session->scores[id].parts)
                part_ids.push_back(part.id.value);

            return {{"score_id", id},
                    {"title", spec.title},
                    {"bars", spec.total_bars},
                    {"parts", session->scores[id].parts.size()},
                    {"part_ids", std::move(part_ids)}};
        });

    server.register_tool(
        "score_set_tuning",
        "Replace the complete 128-note sounding-pitch function",
        {{"score_id", "integer"},
         {"name", "string"},
         {"reference_midi_note", "integer in [0, 127]"},
         {"reference_frequency_hz", "number (finite and positive)"},
         {"cents_from_reference", "array of exactly 128 finite numbers; reference entry is 0"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            const auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");
            if (!params.contains("name") || !params["name"].is_string() ||
                !params.contains("reference_midi_note") ||
                !params.contains("reference_frequency_hz") ||
                !params["reference_frequency_hz"].is_number() ||
                !params.contains("cents_from_reference") ||
                !params["cents_from_reference"].is_array() ||
                params["cents_from_reference"].size() != SCORE_TUNING_NOTE_COUNT)
                return error_response(
                    "tuning requires name, reference_midi_note, reference_frequency_hz, and "
                    "exactly 128 cent entries");

            auto tuning =
                parse_score_tuning({{"name", params["name"]},
                                    {"reference_midi_note", params["reference_midi_note"]},
                                    {"reference_frequency_hz", params["reference_frequency_hz"]},
                                    {"cents_from_reference", params["cents_from_reference"]}});
            if (!tuning) return error_response("invalid score tuning");
            auto result = set_score_tuning(*score, std::move(*tuning), session->undo_for(sid));
            if (!result) {
                if (result.error() == ErrorCode::InvalidFrequency)
                    return error_response("invalid score tuning");
                if (result.error() == ErrorCode::ArithmeticOverflow)
                    return error_response("score version domain exhausted");
                return error_response("set_score_tuning failed");
            }
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_set_formal_plan",
        "Replace the section map with a formal plan",
        {{"score_id", "integer"},
         {"sections", "array of {label, start_bar, end_bar, function (integer, optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("sections") || !params["sections"].is_array())
                return error_response("sections array is required");

            std::vector<SectionDefinition> sections;
            for (const auto& sj : params["sections"]) {
                if (!sj.is_object() || !sj.contains("label") || !sj["label"].is_string() ||
                    !sj.contains("start_bar") || !sj["start_bar"].is_number_integer() ||
                    !sj.contains("end_bar") || !sj["end_bar"].is_number_integer())
                    return error_response("each section requires label, start_bar, and end_bar");
                SectionDefinition sd;
                sd.label = sj["label"].get<std::string>();
                sd.start_bar =
                    detail::checked_integer<std::uint32_t>(sj["start_bar"], "section start bar");
                sd.end_bar =
                    detail::checked_integer<std::uint32_t>(sj["end_bar"], "section end bar");
                if (sj.contains("function")) {
                    auto ff = checked_enum<FormFunction>(sj["function"], 6);
                    if (!ff) return error_response("invalid form function");
                    sd.function = ff;
                }
                sections.push_back(std::move(sd));
            }

            auto result = set_formal_plan(*score, sections, session->undo_for(sid));
            if (!result) return error_response("set_formal_plan failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_add_part",
        "Add a new part to the score",
        {{"score_id", "integer"},
         {"name", "string"},
         {"abbreviation", "string (optional)"},
         {"instrument_type", "integer (optional)"},
         {"clef", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            PartDefinition pd;
            pd.name = params.value("name", "Part");
            pd.abbreviation = params.value("abbreviation", pd.name);
            if (params.contains("instrument_type")) {
                auto it = checked_enum<InstrumentType>(params["instrument_type"], 80);
                if (!it) return error_response("invalid instrument_type");
                pd.instrument_type = *it;
            } else {
                pd.instrument_type = InstrumentType::Piano;
            }
            if (params.contains("clef")) {
                auto cl = checked_enum<Clef>(params["clef"], 5);
                if (!cl) return error_response("invalid clef");
                pd.clef = *cl;
            }

            auto result = add_part(*score, std::move(pd), session->undo_for(sid));
            if (!result) return error_response("add_part failed");

            auto part_id = score->parts.back().id.value;
            json r = mutation_result_j(*result);
            r["part_id"] = part_id;
            return r;
        });

    server.register_tool(
        "score_set_section_harmony",
        "Write harmonic annotations (chord progression) into a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional array of integer)}"},
         {"chords",
          "array of {position: {bar, beat_n, beat_d}, root: {letter, accidental, octave}, quality: "
          "string, bass: {letter, accidental, octave} (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("chords") || !params["chords"].is_array())
                return error_response("chords array is required");

            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");

            std::vector<ChordSymbolEntry> chords;
            for (const auto& cj : params["chords"]) {
                if (!cj.contains("position") || !cj.contains("root"))
                    return error_response("each chord requires position and root");
                ChordSymbolEntry ce;
                auto position = score_time_from_json(cj["position"]);
                auto root = spelled_pitch_from_json(cj["root"]);
                if (!position || !root) return error_response("invalid chord position or root");
                ce.position = *position;
                ce.root = *root;
                if (!cj.contains("quality") || !cj["quality"].is_string())
                    return error_response("each chord requires quality");
                ce.quality = cj["quality"].get<std::string>();
                if (cj.contains("bass")) {
                    auto bass = spelled_pitch_from_json(cj["bass"]);
                    if (!bass) return error_response("invalid chord bass");
                    ce.bass = *bass;
                }
                chords.push_back(std::move(ce));
            }

            auto result =
                set_section_harmony(*score, *region, std::move(chords), session->undo_for(sid));
            if (!result) return error_response("set_section_harmony failed");
            return mutation_result_j(*result);
        });

    // =========================================================================
    // Arrangement Tools
    // =========================================================================

    server.register_tool(
        "score_write_melody",
        "Write a melody line into a voice within a part",
        {{"score_id", "integer"},
         {"part_id", "integer"},
         {"voice_index", "integer (optional, default 0)"},
         {"melody",
          "array of {position: {bar, beat_n, beat_d}, pitch: {letter, accidental, octave}, "
          "duration: {n, d}, dynamic (integer, optional), articulation (integer, optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("melody") || !params["melody"].is_array())
                return error_response("melody array is required");

            PartId part_id{detail::checked_integer<std::uint64_t>(params["part_id"], "part id")};
            auto voice_index =
                detail::checked_integer_or<std::uint8_t>(params, "voice_index", 0, "voice index");

            std::vector<MelodyEntry> melody;
            for (const auto& mj : params["melody"]) {
                if (!mj.contains("position") || !mj.contains("pitch") || !mj.contains("duration"))
                    return error_response(
                        "each melody entry requires position, pitch, and duration");
                MelodyEntry me;
                auto position = score_time_from_json(mj["position"]);
                auto pitch = spelled_pitch_from_json(mj["pitch"]);
                auto duration = beat_from_json(mj["duration"]);
                if (!position || !pitch || !duration)
                    return error_response("invalid melody position, pitch, or duration");
                me.position = *position;
                me.pitch = *pitch;
                me.duration = *duration;
                if (mj.contains("dynamic")) {
                    auto dl = checked_enum<DynamicLevel>(mj["dynamic"], 13);
                    if (!dl) return error_response("invalid dynamic level");
                    me.dynamic = dl;
                }
                if (mj.contains("articulation")) {
                    auto at = checked_enum<ArticulationType>(mj["articulation"], 24);
                    if (!at) return error_response("invalid articulation type");
                    me.articulation = at;
                }
                melody.push_back(me);
            }

            auto result =
                write_melody(*score, part_id, voice_index, melody, session->undo_for(sid));
            if (!result) return error_response("write_melody failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_write_harmony",
        "Distribute harmonic voicings across target parts",
        {{"score_id", "integer"},
         {"target_parts", "array of integer (part IDs, bottom to top)"},
         {"chords",
          "array of {position: {bar, beat_n, beat_d}, voicing: [{letter, accidental, octave}], "
          "duration: {n, d}}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("target_parts") || !params["target_parts"].is_array())
                return error_response("target_parts array is required");
            if (!params.contains("chords") || !params["chords"].is_array())
                return error_response("chords array is required");

            std::vector<PartId> target_parts;
            for (const auto& p : params["target_parts"])
                target_parts.push_back(
                    PartId{detail::checked_integer<std::uint64_t>(p, "target part id")});

            std::vector<HarmonyEntry> chords;
            for (const auto& cj : params["chords"]) {
                if (!cj.contains("position") || !cj.contains("voicing") ||
                    !cj["voicing"].is_array() || !cj.contains("duration"))
                    return error_response(
                        "each harmony entry requires position, voicing, and duration");
                HarmonyEntry he;
                auto position = score_time_from_json(cj["position"]);
                auto duration = beat_from_json(cj["duration"]);
                if (!position || !duration)
                    return error_response("invalid harmony position or duration");
                he.position = *position;
                he.duration = *duration;
                for (const auto& vj : cj["voicing"]) {
                    auto pitch = spelled_pitch_from_json(vj);
                    if (!pitch) return error_response("invalid harmony voicing pitch");
                    he.voicing.push_back(*pitch);
                }
                chords.push_back(std::move(he));
            }

            auto result = write_harmony(*score, target_parts, chords, session->undo_for(sid));
            if (!result) return error_response("write_harmony failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_reorchestrate",
        "Copy note events from source to target part within a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"source_part", "integer"},
         {"target_part", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("source_part")) return error_response("source_part is required");
            if (!params.contains("target_part")) return error_response("target_part is required");

            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");
            PartId source{
                detail::checked_integer<std::uint64_t>(params["source_part"], "source part id")};
            PartId target{
                detail::checked_integer<std::uint64_t>(params["target_part"], "target part id")};

            auto result = reorchestrate(*score, *region, source, target, session->undo_for(sid));
            if (!result) return error_response("reorchestrate failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_double_part",
        "Double a part at a semitone interval into another part",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"source_part", "integer"},
         {"target_part", "integer"},
         {"interval", "integer (semitones)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("source_part")) return error_response("source_part is required");
            if (!params.contains("target_part")) return error_response("target_part is required");

            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");
            PartId source{
                detail::checked_integer<std::uint64_t>(params["source_part"], "source part id")};
            PartId target{
                detail::checked_integer<std::uint64_t>(params["target_part"], "target part id")};
            auto interval =
                detail::checked_integer_or<std::int8_t>(params, "interval", 0, "doubling interval");

            auto result =
                double_part(*score, *region, source, target, interval, session->undo_for(sid));
            if (!result) return error_response("double_part failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_set_dynamics",
        "Set the dynamic level for all note events in a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"level", "integer (DynamicLevel enum: 0=pppp..9=ffff, 10=fp, 11=sfz, 12=sfp, 13=rfz)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("level")) return error_response("level is required");

            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");
            auto dl = checked_enum<DynamicLevel>(params["level"], 13);
            if (!dl) return error_response("invalid dynamic level");

            auto result = set_dynamics(*score, *region, *dl, session->undo_for(sid));
            if (!result) return error_response("set_dynamics failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_set_articulation",
        "Set the articulation for all notes in a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"articulation", "integer (ArticulationType enum: 0=Staccato, 1=Staccatissimo,...)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("articulation")) return error_response("articulation is required");

            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");
            auto at = checked_enum<ArticulationType>(params["articulation"], 24);
            if (!at) return error_response("invalid articulation type");

            auto result = set_articulation(*score, *region, *at, session->undo_for(sid));
            if (!result) return error_response("set_articulation failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_set_articulation_mapping",
        "Set or remove a validated MIDI rendering mapping for one part articulation",
        {{"score_id", "integer"},
         {"part_id", "integer"},
         {"articulation", "integer (ArticulationType enum: 0..24)"},
         {"mapping",
          "canonical articulation mapping object, or null to remove; types: 0=keyswitch, "
          "1=CC, 2=velocity layer, 3=duration scale, 4=program change, 5=combined"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            const auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");
            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("articulation")) return error_response("articulation is required");
            if (!params.contains("mapping")) return error_response("mapping is required");

            const PartId part_id{
                detail::checked_integer<std::uint64_t>(params["part_id"], "part id")};
            const auto articulation = checked_enum<ArticulationType>(params["articulation"], 24);
            if (!articulation) return error_response("invalid articulation type");

            std::optional<ArticulationMapping> mapping;
            if (!params["mapping"].is_null()) {
                auto parsed = articulation_mapping_from_json(params["mapping"]);
                if (!parsed) return error_response("invalid articulation mapping");
                mapping = std::move(*parsed);
            }
            auto result = set_articulation_mapping(
                *score, part_id, *articulation, std::move(mapping), session->undo_for(sid));
            if (!result) {
                if (result.error() == ErrorCode::InvalidRenderingConfig)
                    return error_response("invalid articulation mapping");
                return error_response("set_articulation_mapping failed");
            }
            return mutation_result_j(*result);
        });

    // =========================================================================
    // Detail Tools
    // =========================================================================

    server.register_tool(
        "score_insert_note",
        "Insert a single note at a specific location",
        {{"score_id", "integer"},
         {"part_id", "integer"},
         {"bar", "integer (1-indexed)"},
         {"voice", "integer (optional, default 0)"},
         {"offset", "object {n, d} (optional, default zero beat offset within bar)"},
         {"pitch", "object {letter, accidental, octave}"},
         {"duration", "object {n, d}"},
         {"velocity", "integer (0-127, optional)"},
         {"release_velocity", "integer (0-127, optional, default 64)"},
         {"articulation", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("bar")) return error_response("bar is required");
            if (!params.contains("pitch")) return error_response("pitch is required");
            if (!params.contains("duration")) return error_response("duration is required");

            PartId part_id{detail::checked_integer<std::uint64_t>(params["part_id"], "part id")};
            auto bar = detail::checked_integer<std::uint32_t>(params["bar"], "bar number");
            auto voice =
                detail::checked_integer_or<std::uint8_t>(params, "voice", 0, "voice index");

            Beat offset = Beat::zero();
            if (params.contains("offset")) {
                auto parsed_offset = beat_from_json(params["offset"]);
                if (!parsed_offset) return error_response("invalid offset");
                offset = *parsed_offset;
            }

            auto pitch = spelled_pitch_from_json(params["pitch"]);
            auto duration = beat_from_json(params["duration"]);
            if (!pitch || !duration) return error_response("invalid pitch or duration");

            Note note;
            note.pitch = *pitch;
            if (params.contains("velocity")) {
                auto raw_velocity =
                    detail::checked_integer<int>(params["velocity"], "note velocity");
                if (raw_velocity < 0 || raw_velocity > 127)
                    return error_response("velocity must be between 0 and 127");
                VelocityValue vv;
                vv.value = static_cast<std::uint8_t>(raw_velocity);
                note.velocity = vv;
            }
            if (params.contains("release_velocity")) {
                const auto value =
                    detail::checked_integer<int>(params["release_velocity"], "release velocity");
                if (value < 0 || value > 127)
                    return error_response("release_velocity must be between 0 and 127");
                note.release_velocity = static_cast<std::uint8_t>(value);
            }
            if (params.contains("articulation")) {
                auto at = checked_enum<ArticulationType>(params["articulation"], 24);
                if (!at) return error_response("invalid articulation type");
                note.articulation = at;
            }

            auto result = insert_note(
                *score, part_id, bar, voice, offset, note, *duration, session->undo_for(sid));
            if (!result) return error_response("insert_note failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_insert_chord_symbol",
        "Insert one typed harmonic symbol at a specific voice location",
        {{"score_id", "integer"},
         {"part_id", "integer"},
         {"bar", "integer (1-indexed)"},
         {"voice", "integer (optional, default 0)"},
         {"offset", "object {n, d} (optional, default zero beat offset within bar)"},
         {"root", "object {letter, accidental, octave}"},
         {"quality", "string"},
         {"bass", "object {letter, accidental, octave} (optional)"},
         {"roman", "string (optional display spelling)"},
         {"extensions", "array of string (optional display residuals)"},
         {"numeral",
          "object {root: 1..7, alteration: integer (optional), key: {fifths: -7..7, mode: "
          "0..4}} (optional)"},
         {"inversion", "integer (optional, zero is root position)"},
         {"degrees",
          "array of {value: positive integer, alteration: integer (optional), type: 0..2} "
          "(optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            const auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("bar")) return error_response("bar is required");
            if (!params.contains("root")) return error_response("root is required");
            if (!params.contains("quality")) return error_response("quality is required");

            const PartId part_id{
                detail::checked_integer<std::uint64_t>(params["part_id"], "part id")};
            const auto bar = detail::checked_integer<std::uint32_t>(params["bar"], "bar number");
            const auto voice =
                detail::checked_integer_or<std::uint8_t>(params, "voice", 0, "voice index");
            Beat offset = Beat::zero();
            if (params.contains("offset")) {
                auto parsed = beat_from_json(params["offset"]);
                if (!parsed) return error_response("invalid offset");
                offset = *parsed;
            }

            auto symbol = chord_symbol_from_json(params);
            if (!symbol) return error_response("invalid chord symbol payload");
            auto result = insert_chord_symbol(
                *score, part_id, bar, voice, offset, std::move(*symbol), session->undo_for(sid));
            if (!result) return error_response("insert_chord_symbol failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_modify_note",
        "Modify properties of an existing note within a NoteGroup",
        {{"score_id", "integer"},
         {"event_id", "integer"},
         {"note_index", "integer (optional, default 0)"},
         {"pitch", "object {letter, accidental, octave} (optional)"},
         {"duration", "object {n, d} (optional)"},
         {"velocity", "integer (optional)"},
         {"release_velocity", "integer (0-127, optional)"},
         {"articulation", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("event_id")) return error_response("event_id is required");

            EventId event_id{
                detail::checked_integer<std::uint64_t>(params["event_id"], "event id")};
            auto note_index =
                detail::checked_integer_or<std::uint8_t>(params, "note_index", 0, "note index");

            std::optional<SpelledPitch> pitch;
            if (params.contains("pitch")) {
                pitch = spelled_pitch_from_json(params["pitch"]);
                if (!pitch) return error_response("invalid pitch");
            }

            std::optional<Beat> duration;
            if (params.contains("duration")) {
                duration = beat_from_json(params["duration"]);
                if (!duration) return error_response("invalid duration");
            }

            std::optional<VelocityValue> velocity;
            if (params.contains("velocity")) {
                auto raw_velocity =
                    detail::checked_integer<int>(params["velocity"], "note velocity");
                if (raw_velocity < 0 || raw_velocity > 127)
                    return error_response("velocity must be between 0 and 127");
                VelocityValue vv;
                vv.value = static_cast<std::uint8_t>(raw_velocity);
                velocity = vv;
            }

            std::optional<std::uint8_t> release_velocity;
            if (params.contains("release_velocity")) {
                const auto value =
                    detail::checked_integer<int>(params["release_velocity"], "release velocity");
                if (value < 0 || value > 127)
                    return error_response("release_velocity must be between 0 and 127");
                release_velocity = static_cast<std::uint8_t>(value);
            }

            std::optional<ArticulationType> articulation;
            if (params.contains("articulation")) {
                auto at = checked_enum<ArticulationType>(params["articulation"], 24);
                if (!at) return error_response("invalid articulation type");
                articulation = at;
            }

            auto result = modify_note(*score,
                                      event_id,
                                      note_index,
                                      pitch,
                                      duration,
                                      velocity,
                                      release_velocity,
                                      articulation,
                                      session->undo_for(sid));
            if (!result) return error_response("modify_note failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_delete_event",
        "Delete an event, replacing it with a rest of equal duration",
        {{"score_id", "integer"}, {"event_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("event_id")) return error_response("event_id is required");
            EventId event_id{
                detail::checked_integer<std::uint64_t>(params["event_id"], "event id")};

            auto result = delete_event(*score, event_id, session->undo_for(sid));
            if (!result) return error_response("delete_event failed");
            return mutation_result_j(*result);
        });

    server.register_tool(
        "score_transpose",
        "Transpose a single event or an entire region by a diatonic interval",
        {{"score_id", "integer"},
         {"event_id", "integer (provide this OR region, not both)"},
         {"region", "object {start_bar, end_bar, parts (optional)} (provide this OR event_id)"},
         {"interval", "object {chromatic, diatonic}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = detail::checked_integer<std::uint64_t>(params["score_id"], "score id");

            if (!params.contains("interval")) return error_response("interval is required");
            auto interval = diatonic_interval_from_json(params["interval"]);
            if (!interval) return error_response("invalid interval");

            std::variant<EventId, ScoreRegion> target;
            if (params.contains("event_id")) {
                target =
                    EventId{detail::checked_integer<std::uint64_t>(params["event_id"], "event id")};
            } else if (params.contains("region")) {
                auto region = region_from_json(params["region"]);
                if (!region) return error_response("invalid region");
                target = *region;
            } else {
                return error_response("either event_id or region is required");
            }

            auto result = transpose(*score, target, *interval, session->undo_for(sid));
            if (!result) return error_response("transpose failed");
            return mutation_result_j(*result);
        });

    // =========================================================================
    // Analysis Tools (read-only)
    // =========================================================================

    server.register_tool(
        "score_analyze_harmony",
        "Analyse harmonic content within a region of the score",
        {{"score_id", "integer"}, {"region", "object {start_bar, end_bar, parts (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            if (!params.contains("region")) return error_response("region is required");
            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");

            auto annotations = analyze_harmony(*score, *region);
            json result = json::array();
            for (const auto& a : annotations)
                result.push_back(harmonic_annotation_j(a));
            return {{"annotations", result}};
        });

    server.register_tool(
        "score_get_orchestration",
        "Retrieve orchestration annotations (part-role pairs) for a region",
        {{"score_id", "integer"}, {"region", "object {start_bar, end_bar, parts (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            if (!params.contains("region")) return error_response("region is required");
            auto region = region_from_json(params["region"]);
            if (!region) return error_response("invalid region");

            auto orch = get_orchestration(*score, *region);
            json result = json::array();
            for (const auto& [pid, role] : orch)
                result.push_back({{"part_id", pid.value}, {"role", static_cast<int>(role)}});
            return {{"orchestration", result}};
        });

    server.register_tool(
        "score_get_reduction",
        "Produce a reduced view of the score (piano, short, skeleton)",
        {{"score_id", "integer"},
         {"view_type", "string (piano|short|skeleton; optional, default piano)"},
         {"region", "object {start_bar, end_bar, parts (optional)} (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto view_type = params.value("view_type", "piano");

            std::optional<ScoreRegion> region;
            if (params.contains("region")) {
                auto parsed = region_from_json(params["region"]);
                if (!parsed) return error_response("invalid region");
                region = *parsed;
            }

            auto reduced =
                get_reduction(*score, ScoreId{session->next_score_id}, view_type, region);
            if (!reduced) return error_response("get_reduction failed: invalid view or region");

            // Store the reduction as a new score in the session
            auto pending_id = pending_score_id(*session);
            if (!pending_id) return error_response("score identity domain exhausted");
            const auto id = *pending_id;
            session->scores[id] = std::move(*reduced);
            commit_score_id(*session, id);

            return {{"score_id", id},
                    {"view_type", view_type},
                    {"parts", session->scores[id].parts.size()}};
        });

    server.register_tool(
        "score_validate",
        "Run all validation rules on the score",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto diagnostics = validate_score(*score);
            const bool structurally_compilable = is_compilable(*score);
            const auto rendering = validate_rendering(*score);
            const bool midi_compilable =
                structurally_compilable &&
                std::none_of(rendering.begin(), rendering.end(), [](const Diagnostic& diagnostic) {
                    return diagnostic.severity == ValidationSeverity::Error;
                });

            json diags = json::array();
            for (const auto& d : diagnostics)
                diags.push_back(mcp_detail::encode_diagnostic(d));

            return {{"valid", diagnostics.empty()},
                    {"compilable", structurally_compilable},
                    {"structurally_compilable", structurally_compilable},
                    {"midi_compilable", midi_compilable},
                    {"diagnostics", diags}};
        });

    server.register_tool("score_get_form_summary",
                         "Produce a condensed form summary of the score's sections",
                         {{"score_id", "integer"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             auto entries = get_form_summary(*score);
                             json result = json::array();
                             for (const auto& e : entries)
                                 result.push_back(form_entry_j(e));
                             return {{"sections", result}};
                         });

    // =========================================================================
    // Serialisation
    // =========================================================================

    server.register_tool("score_get_json",
                         "Serialise the score to JSON",
                         {{"score_id", "integer"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             return score_to_json(*score);
                         });

    // =========================================================================
    // Compilation
    // =========================================================================

    server.register_tool(
        "score_compile_to_midi",
        "Compile the score to MIDI event data",
        {{"score_id", "integer"}, {"ppq", "integer (optional, 1..65535; default 480)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            const auto ppq = detail::checked_integer_or<int>(params, "ppq", 480, "MIDI PPQ");
            auto result = compile_to_midi(*score, ppq);
            if (!result) {
                if (result.error() == ErrorCode::InvalidMidiPPQ)
                    return error_response("ppq must be between 1 and 65535");
                return error_response("compile_to_midi failed: score not compilable");
            }

            json notes = json::array();
            for (const auto& n : result->midi.notes) {
                notes.push_back({{"tick", n.tick},
                                 {"duration_ticks", n.duration_ticks},
                                 {"part_id", n.part_id.value},
                                 {"channel", n.channel},
                                 {"note", n.note},
                                 {"velocity", n.velocity}});
            }

            json keyswitches = json::array();
            for (const auto& k : result->midi.keyswitches) {
                keyswitches.push_back({{"tick", k.tick},
                                       {"duration_ticks", k.duration_ticks},
                                       {"part_id", k.part_id.value},
                                       {"channel", k.channel},
                                       {"note", k.note},
                                       {"velocity", k.velocity}});
            }

            json control_changes = json::array();
            for (const auto& cc : result->midi.control_changes) {
                control_changes.push_back({{"tick", cc.tick},
                                           {"part_id", cc.part_id.value},
                                           {"channel", cc.channel},
                                           {"controller", cc.controller},
                                           {"value", cc.value}});
            }

            json program_changes = json::array();
            for (const auto& program : result->midi.program_changes) {
                program_changes.push_back({{"tick", program.tick},
                                           {"part_id", program.part_id.value},
                                           {"channel", program.channel},
                                           {"program", program.program}});
            }

            json tempos = json::array();
            for (const auto& t : result->midi.tempos)
                tempos.push_back(
                    {{"tick", t.tick}, {"microseconds_per_beat", t.microseconds_per_beat}});

            json time_sigs = json::array();
            for (const auto& ts : result->midi.time_signatures)
                time_sigs.push_back({{"tick", ts.tick},
                                     {"numerator", ts.numerator},
                                     {"denominator", ts.denominator},
                                     {"clocks_per_metronome_click", ts.clocks_per_metronome_click},
                                     {"notated_32nds_per_quarter", ts.notated_32nds_per_quarter}});

            json key_sigs = json::array();
            for (const auto& ks : result->midi.key_signatures)
                key_sigs.push_back(
                    {{"tick", ks.tick}, {"accidentals", ks.accidentals}, {"minor", ks.minor}});

            return {{"ppq", result->midi.ppq},
                    {"notes", notes},
                    {"keyswitches", keyswitches},
                    {"control_changes", control_changes},
                    {"program_changes", program_changes},
                    {"tempos", tempos},
                    {"time_signatures", time_sigs},
                    {"key_signatures", key_sigs},
                    {"report", mcp_detail::encode_compilation_report(result->report)}};
        });

    server.register_tool("score_compile_to_musicxml",
                         "Compile the score to MusicXML",
                         {{"score_id", "integer"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             auto result = compile_to_musicxml(*score);
                             if (!result) return error_response("compile_to_musicxml failed");

                             return {
                                 {"xml", result->xml},
                                 {"report", mcp_detail::encode_compilation_report(result->report)}};
                         });

    server.register_tool("score_compile_to_lilypond",
                         "Compile the score to LilyPond notation",
                         {{"score_id", "integer"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             auto result = compile_to_lilypond(*score);
                             if (!result) return error_response("compile_to_lilypond failed");

                             return {
                                 {"ly", result->ly},
                                 {"report", mcp_detail::encode_compilation_report(result->report)}};
                         });

    server.register_tool(
        "score_compile_to_ableton",
        "Compile the Score IR into supported Ableton Live operations",
        {{"score_id", "integer"}, {"ppq", "integer (optional, default 480)"}},
        [session, transport](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            const auto ppq = detail::checked_integer_or<int>(params, "ppq", 480, "MIDI PPQ");
            if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max())
                return error_response("ppq must be between 1 and 65535");
            if (transport == nullptr || !transport->ensure_connected()) {
                return {{"success", false},
                        {"connected", false},
                        {"error", "Ableton transport unavailable"}};
            }

            auto result = formats::compile_to_ableton(*score, *transport, ppq);
            if (!result) {
                return {{"success", false},
                        {"connected", transport->is_connected()},
                        {"error_code", static_cast<int>(result.error())},
                        {"error", "Ableton score compilation failed"}};
            }

            return {
                {"success", true},
                {"connected", true},
                {"complete",
                 result->warnings.empty() && !result->midi_report.has_residuals() &&
                     result->tuning_definitions_written == result->tuning_definitions_requested},
                {"scenes_created", result->scenes_created},
                {"tracks_created", result->tracks_created},
                {"clips_created", result->clips_created},
                {"clip_envelope_clears_requested", result->clip_envelope_clears_requested},
                {"clip_envelope_clears_executed", result->clip_envelope_clears_executed},
                {"clip_envelope_clears_verified", result->clip_envelope_clears_verified},
                {"clip_envelope_deployments",
                 mcp_detail::encode_clip_envelope_deployments(result->clip_envelope_deployments)},
                {"notes_requested", result->notes_requested},
                {"notes_written", result->notes_written},
                {"note_batches_requested", result->note_batches_requested},
                {"note_batches_executed", result->note_batches_executed},
                {"note_ids_returned", result->note_ids_returned},
                {"note_batches_verified", result->note_batches_verified},
                {"notes_verified", result->notes_verified},
                {"note_deployments", mcp_detail::encode_note_deployments(result->note_deployments)},
                {"articulation_control_events_requested",
                 result->articulation_control_events_requested},
                {"articulation_control_events_written",
                 result->articulation_control_events_written},
                {"tempo_events_requested", result->tempo_events_requested},
                {"tempo_events_written", result->tempo_events_written},
                {"time_signature_events_requested", result->time_signature_events_requested},
                {"time_signature_events_written", result->time_signature_events_written},
                {"time_signature_groupings_requested", result->time_signature_groupings_requested},
                {"time_signature_groupings_written", result->time_signature_groupings_written},
                {"key_signature_events_requested", result->key_signature_events_requested},
                {"key_signature_events_written", result->key_signature_events_written},
                {"tuning_definitions_requested", result->tuning_definitions_requested},
                {"tuning_definitions_written", result->tuning_definitions_written},
                {"requested_tuning", mcp_detail::encode_score_tuning(result->requested_tuning)},
                {"section_nodes_total", result->section_nodes_total},
                {"section_nodes_projected", result->section_nodes_projected},
                {"section_nodes_unprojected", result->section_nodes_unprojected},
                {"markers_requested", result->markers_requested},
                {"markers_created", result->markers_created},
                {"markers_updated", result->markers_updated},
                {"markers_verified", result->markers_verified},
                {"marker_deployments",
                 mcp_detail::encode_cue_deployments(result->marker_deployments)},
                {"property_writes", result->property_writes},
                {"property_writes_verified", result->property_writes_verified},
                {"property_deployments",
                 mcp_detail::encode_property_deployments(result->property_deployments)},
                {"target_profile", target_profile_to_json(result->target_profile)},
                {"report", mcp_detail::encode_compilation_report(result->midi_report)},
                {"warnings", result->warnings}};
        });

    // =========================================================================
    // Query Tools
    // =========================================================================

    server.register_tool("score_query_harmony_at",
                         "Find the harmonic annotation active at a given time",
                         {{"score_id", "integer"}, {"time", "object {bar, beat_n, beat_d}"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             if (!params.contains("time"))
                                 return error_response("time is required");
                             auto time = score_time_from_json(params["time"]);
                             if (!time) return error_response("invalid score time");

                             auto ha = query_harmony_at(*score, *time);
                             if (!ha) return {{"found", false}};
                             json r = harmonic_annotation_j(*ha);
                             r["found"] = true;
                             return r;
                         });

    server.register_tool("score_find_motif",
                         "Find occurrences of a pitch-class motif pattern within a region",
                         {{"score_id", "integer"},
                          {"pattern", "array of integer (pitch classes 0-11)"},
                          {"region", "object {start_bar, end_bar, parts (optional)}"}},
                         [session](const json& params) -> json {
                             json err;
                             auto* score = lookup_score(session, params, err);
                             if (!score) return err;

                             if (!params.contains("pattern") || !params["pattern"].is_array())
                                 return error_response("pattern array is required");
                             if (!params.contains("region"))
                                 return error_response("region is required");

                             std::vector<PitchClass> pattern;
                             for (const auto& p : params["pattern"]) {
                                 int val = detail::checked_integer<int>(p, "motif pitch class");
                                 auto pc = PitchClass::from_int(val);
                                 if (!pc)
                                     return error_response(
                                         "pattern pitch class must be 0-11, got " +
                                         std::to_string(val));
                                 pattern.push_back(*pc);
                             }

                             auto region = region_from_json(params["region"]);
                             if (!region) return error_response("invalid region");
                             auto occurrences = query_find_motif(*score, pattern, *region);

                             json result = json::array();
                             for (const auto& o : occurrences)
                                 result.push_back(motif_occurrence_j(o));
                             return {{"occurrences", result}, {"count", occurrences.size()}};
                         });
}

} // namespace sunny::infrastructure
