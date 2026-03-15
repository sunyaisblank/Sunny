/**
 * @file MCPT005A.cpp
 * @brief MCP Tool Registration — Score IR implementation
 *
 * Component: MCPT005A
 *
 * Maps MCP tool calls to Score IR workflow functions (SIWF001A),
 * query functions (SIQR001A), and Infrastructure compilation
 * wrappers (INWF001A). Score state is held in a shared_ptr to
 * a session store captured by the tool handler lambdas.
 */

#include "MCPT005A.h"

#include "Score/SIWF001A.h"
#include "Score/SIQR001A.h"
#include "Score/SISZ001A.h"
#include "../Application/INWF001A.h"

#include <map>
#include <memory>

namespace Sunny::Infrastructure {

using json = nlohmann::json;
using namespace Sunny::Core;

namespace {

// =============================================================================
// Session store
// =============================================================================

struct ScoreSession {
    std::map<std::uint64_t, Score> scores;
    std::map<std::uint64_t, UndoStack> undo_stacks;
    std::uint64_t next_score_id = 1;
    std::uint64_t next_part_id = 1;

    Score* find(std::uint64_t id) {
        auto it = scores.find(id);
        return it != scores.end() ? &it->second : nullptr;
    }

    UndoStack* undo_for(std::uint64_t id) {
        return &undo_stacks[id];
    }
};

// =============================================================================
// Response helpers
// =============================================================================

json error_response(const std::string& msg) {
    return {{"error", msg}};
}

json diagnostic_j(const Diagnostic& d) {
    json j = {
        {"rule", d.rule},
        {"severity", d.severity == ValidationSeverity::Error ? "error" :
                     d.severity == ValidationSeverity::Warning ? "warning" : "info"},
        {"message", d.message},
        {"error_code", d.error_code}
    };
    if (d.location) {
        j["location"] = {
            {"bar", d.location->bar},
            {"beat_n", d.location->beat.numerator},
            {"beat_d", d.location->beat.denominator}
        };
    }
    if (d.part) {
        j["part_id"] = d.part->value;
    }
    return j;
}

json compilation_report_j(const CompilationReport& r) {
    json diags = json::array();
    for (const auto& d : r.diagnostics) {
        json dj = {{"message", d.message}};
        if (d.location) {
            dj["location"] = {
                {"bar", d.location->bar},
                {"beat_n", d.location->beat.numerator},
                {"beat_d", d.location->beat.denominator}
            };
        }
        if (d.part) dj["part_id"] = d.part->value;
        diags.push_back(std::move(dj));
    }
    return {
        {"dropped_notes", r.dropped_notes},
        {"dropped_tempo_events", r.dropped_tempo_events},
        {"dropped_time_sig_events", r.dropped_time_sig_events},
        {"diagnostics", diags}
    };
}

// =============================================================================
// JSON → domain type parsing helpers
// =============================================================================

ScoreTime score_time_from_json(const json& j) {
    return {
        j.value("bar", std::uint32_t{1}),
        Beat{j.value("beat_n", 0), j.value("beat_d", 1)}
    };
}

ScoreRegion region_from_json(const json& j) {
    ScoreRegion r;
    r.start = {j.value("start_bar", std::uint32_t{1}), Beat::zero()};
    r.end = {j.value("end_bar", std::uint32_t{1}), Beat::zero()};
    if (j.contains("parts") && j["parts"].is_array()) {
        for (const auto& p : j["parts"])
            r.parts.push_back(PartId{p.get<std::uint64_t>()});
    }
    return r;
}

SpelledPitch spelled_pitch_from_json(const json& j) {
    // Map letter string to index: C=0, D=1, ..., B=6
    auto letter_str = j.value("letter", "C");
    uint8_t letter = 0;
    if (!letter_str.empty()) {
        switch (letter_str[0]) {
            case 'C': case 'c': letter = 0; break;
            case 'D': case 'd': letter = 1; break;
            case 'E': case 'e': letter = 2; break;
            case 'F': case 'f': letter = 3; break;
            case 'G': case 'g': letter = 4; break;
            case 'A': case 'a': letter = 5; break;
            case 'B': case 'b': letter = 6; break;
        }
    }
    return SpelledPitch{
        letter,
        j.value("accidental", std::int8_t{0}),
        j.value("octave", std::int8_t{4})
    };
}

Beat beat_from_json(const json& j) {
    return Beat{j.value("n", 0), j.value("d", 1)};
}

DiatonicInterval diatonic_interval_from_json(const json& j) {
    return DiatonicInterval{
        j.value("chromatic", 0),
        j.value("diatonic", 0)
    };
}

// =============================================================================
// Domain type → JSON serialisation helpers
// =============================================================================

json score_time_j(const ScoreTime& t) {
    return {
        {"bar", t.bar},
        {"beat_n", t.beat.numerator},
        {"beat_d", t.beat.denominator}
    };
}

json spelled_pitch_j(const SpelledPitch& sp) {
    static constexpr const char* LETTERS[] = {"C","D","E","F","G","A","B"};
    return {
        {"letter", LETTERS[sp.letter % 7]},
        {"accidental", sp.accidental},
        {"octave", sp.octave}
    };
}

json form_entry_j(const FormSummaryEntry& f) {
    return {
        {"label", f.label},
        {"start", score_time_j(f.start)},
        {"end", score_time_j(f.end)},
        {"key_root", spelled_pitch_j(f.key.root)},
        {"key_accidentals", f.key.accidentals},
        {"tempo_bpm", f.tempo_bpm}
    };
}

json harmonic_annotation_j(const HarmonicAnnotation& ha) {
    json j = {
        {"position", score_time_j(ha.position)},
        {"duration", {{"n", ha.duration.numerator}, {"d", ha.duration.denominator}}}
    };
    if (!ha.roman_numeral.empty()) j["roman_numeral"] = ha.roman_numeral;
    if (ha.confidence > 0.0) j["confidence"] = ha.confidence;
    // Chord voicing
    json notes = json::array();
    for (const auto& n : ha.chord.notes)
        notes.push_back(static_cast<int>(n));
    j["chord"] = {
        {"root", static_cast<int>(ha.chord.root)},
        {"quality", ha.chord.quality},
        {"notes", notes}
    };
    return j;
}

json motif_occurrence_j(const MotifOccurrence& m) {
    return {
        {"part_id", m.part_id.value},
        {"position", score_time_j(m.position)}
    };
}

/// Extract score_id from params, look up in session, return pointer or null
Score* lookup_score(const std::shared_ptr<ScoreSession>& session,
                    const json& params, json& err) {
    if (!params.contains("score_id")) {
        err = error_response("score_id is required");
        return nullptr;
    }
    auto id = params["score_id"].get<std::uint64_t>();
    auto* s = session->find(id);
    if (!s) {
        err = error_response("score not found: " + std::to_string(id));
        return nullptr;
    }
    return s;
}

/// Enum parse helpers

template<typename E>
std::optional<E> checked_enum(int val, int max_val) {
    if (val < 0 || val > max_val) return std::nullopt;
    return static_cast<E>(val);
}

json mutation_result_j(const MutationResult& mr) {
    json j = {{"ok", true}};
    if (!mr.diagnostics.empty()) {
        json diags = json::array();
        for (const auto& d : mr.diagnostics)
            diags.push_back(diagnostic_j(d));
        j["diagnostics"] = diags;
    }
    return j;
}

}  // anonymous namespace


void register_score_tools(McpServer& server) {
    auto session = std::make_shared<ScoreSession>();

    // =========================================================================
    // Composition Tools
    // =========================================================================

    server.register_tool(
        "score_create",
        "Create a new score from a specification",
        {{"title", "string"}, {"total_bars", "integer"},
         {"bpm", "number"}, {"key_root", "object {letter, accidental, octave}"},
         {"minor", "boolean (optional)"}, {"key_accidentals", "integer (optional)"},
         {"time_sig_num", "integer (optional)"}, {"time_sig_den", "integer (optional)"},
         {"parts", "array of {name, abbreviation, instrument_type (integer), clef (integer, optional)} (optional)"}},
        [session](const json& params) -> json {
            ScoreSpec spec;
            spec.title = params.value("title", "Untitled");
            spec.total_bars = params.value("total_bars", std::uint32_t{16});
            spec.bpm = params.value("bpm", 120.0);

            if (params.contains("key_root"))
                spec.key_root = spelled_pitch_from_json(params["key_root"]);
            else
                spec.key_root = SpelledPitch{0, 0, 4};  // C4

            spec.minor = params.value("minor", false);
            spec.key_accidentals = params.value("key_accidentals", std::int8_t{0});
            spec.time_sig_num = params.value("time_sig_num", 4);
            spec.time_sig_den = params.value("time_sig_den", 4);

            if (params.contains("parts") && params["parts"].is_array()) {
                for (const auto& pj : params["parts"]) {
                    PartDefinition pd;
                    pd.name = pj.value("name", "Part");
                    pd.abbreviation = pj.value("abbreviation", "Pt.");
                    if (pj.contains("instrument_type")) {
                        auto it = checked_enum<InstrumentType>(
                            pj["instrument_type"].get<int>(), 80);
                        if (it) pd.instrument_type = *it;
                        else pd.instrument_type = InstrumentType::Piano;
                    } else {
                        pd.instrument_type = InstrumentType::Piano;
                    }
                    if (pj.contains("clef")) {
                        auto cl = checked_enum<Clef>(pj["clef"].get<int>(), 5);
                        if (cl) pd.clef = *cl;
                    }
                    spec.parts.push_back(std::move(pd));
                }
            }

            auto result = create_score(spec);
            if (!result) return error_response("create_score failed: error " +
                                                std::to_string(static_cast<int>(result.error())));

            auto id = session->next_score_id++;
            session->scores[id] = std::move(*result);

            return {{"score_id", id}, {"title", spec.title},
                    {"bars", spec.total_bars},
                    {"parts", session->scores[id].parts.size()}};
        }
    );

    server.register_tool(
        "score_set_formal_plan",
        "Replace the section map with a formal plan",
        {{"score_id", "integer"}, {"sections", "array of {label, start_bar, end_bar, function (integer, optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("sections") || !params["sections"].is_array())
                return error_response("sections array is required");

            std::vector<SectionDefinition> sections;
            for (const auto& sj : params["sections"]) {
                SectionDefinition sd;
                sd.label = sj.value("label", "");
                sd.start_bar = sj.value("start_bar", std::uint32_t{1});
                sd.end_bar = sj.value("end_bar", std::uint32_t{1});
                if (sj.contains("function")) {
                    auto ff = checked_enum<FormFunction>(sj["function"].get<int>(), 6);
                    if (ff) sd.function = *ff;
                }
                sections.push_back(std::move(sd));
            }

            auto result = set_formal_plan(*score, std::move(sections),
                                          session->undo_for(sid));
            if (!result) return error_response("set_formal_plan failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_add_part",
        "Add a new part to the score",
        {{"score_id", "integer"}, {"name", "string"}, {"abbreviation", "string (optional)"},
         {"instrument_type", "integer (optional)"}, {"clef", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            PartDefinition pd;
            pd.name = params.value("name", "Part");
            pd.abbreviation = params.value("abbreviation", pd.name);
            if (params.contains("instrument_type")) {
                auto it = checked_enum<InstrumentType>(
                    params["instrument_type"].get<int>(), 80);
                if (it) pd.instrument_type = *it;
                else pd.instrument_type = InstrumentType::Piano;
            } else {
                pd.instrument_type = InstrumentType::Piano;
            }
            if (params.contains("clef")) {
                auto cl = checked_enum<Clef>(params["clef"].get<int>(), 5);
                if (cl) pd.clef = *cl;
            }

            auto result = wf_add_part(*score, std::move(pd),
                                      session->undo_for(sid));
            if (!result) return error_response("wf_add_part failed");

            auto part_id = score->parts.back().id.value;
            json r = mutation_result_j(*result);
            r["part_id"] = part_id;
            return r;
        }
    );

    server.register_tool(
        "score_set_section_harmony",
        "Write harmonic annotations (chord progression) into a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional array of integer)}"},
         {"chords", "array of {position: {bar, beat_n, beat_d}, root: {letter, accidental, octave}, quality: string, bass: {letter, accidental, octave} (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("chords") || !params["chords"].is_array())
                return error_response("chords array is required");

            auto region = region_from_json(params["region"]);

            std::vector<ChordSymbolEntry> chords;
            for (const auto& cj : params["chords"]) {
                ChordSymbolEntry ce;
                if (cj.contains("position"))
                    ce.position = score_time_from_json(cj["position"]);
                if (cj.contains("root"))
                    ce.root = spelled_pitch_from_json(cj["root"]);
                ce.quality = cj.value("quality", "major");
                if (cj.contains("bass"))
                    ce.bass = spelled_pitch_from_json(cj["bass"]);
                chords.push_back(std::move(ce));
            }

            auto result = set_section_harmony(*score, region, std::move(chords),
                                              session->undo_for(sid));
            if (!result) return error_response("set_section_harmony failed");
            return mutation_result_j(*result);
        }
    );

    // =========================================================================
    // Arrangement Tools
    // =========================================================================

    server.register_tool(
        "score_write_melody",
        "Write a melody line into a voice within a part",
        {{"score_id", "integer"}, {"part_id", "integer"}, {"voice_index", "integer (optional, default 0)"},
         {"melody", "array of {position: {bar, beat_n, beat_d}, pitch: {letter, accidental, octave}, duration: {n, d}, dynamic (integer, optional), articulation (integer, optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("melody") || !params["melody"].is_array())
                return error_response("melody array is required");

            PartId part_id{params["part_id"].get<std::uint64_t>()};
            auto voice_index = params.value("voice_index", std::uint8_t{0});

            std::vector<MelodyEntry> melody;
            for (const auto& mj : params["melody"]) {
                MelodyEntry me;
                if (mj.contains("position"))
                    me.position = score_time_from_json(mj["position"]);
                if (mj.contains("pitch"))
                    me.pitch = spelled_pitch_from_json(mj["pitch"]);
                if (mj.contains("duration"))
                    me.duration = beat_from_json(mj["duration"]);
                if (mj.contains("dynamic")) {
                    auto dl = checked_enum<DynamicLevel>(mj["dynamic"].get<int>(), 13);
                    if (dl) me.dynamic = *dl;
                }
                if (mj.contains("articulation")) {
                    auto at = checked_enum<ArticulationType>(mj["articulation"].get<int>(), 24);
                    if (at) me.articulation = *at;
                }
                melody.push_back(std::move(me));
            }

            auto result = write_melody(*score, part_id, voice_index, melody,
                                       session->undo_for(sid));
            if (!result) return error_response("write_melody failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_write_harmony",
        "Distribute harmonic voicings across target parts",
        {{"score_id", "integer"},
         {"target_parts", "array of integer (part IDs, bottom to top)"},
         {"chords", "array of {position: {bar, beat_n, beat_d}, voicing: [{letter, accidental, octave}], duration: {n, d}}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("target_parts") || !params["target_parts"].is_array())
                return error_response("target_parts array is required");
            if (!params.contains("chords") || !params["chords"].is_array())
                return error_response("chords array is required");

            std::vector<PartId> target_parts;
            for (const auto& p : params["target_parts"])
                target_parts.push_back(PartId{p.get<std::uint64_t>()});

            std::vector<HarmonyEntry> chords;
            for (const auto& cj : params["chords"]) {
                HarmonyEntry he;
                if (cj.contains("position"))
                    he.position = score_time_from_json(cj["position"]);
                if (cj.contains("voicing") && cj["voicing"].is_array()) {
                    for (const auto& vj : cj["voicing"])
                        he.voicing.push_back(spelled_pitch_from_json(vj));
                }
                if (cj.contains("duration"))
                    he.duration = beat_from_json(cj["duration"]);
                chords.push_back(std::move(he));
            }

            auto result = write_harmony(*score, target_parts, chords,
                                        session->undo_for(sid));
            if (!result) return error_response("write_harmony failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_reorchestrate",
        "Copy note events from source to target part within a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"source_part", "integer"}, {"target_part", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("source_part")) return error_response("source_part is required");
            if (!params.contains("target_part")) return error_response("target_part is required");

            auto region = region_from_json(params["region"]);
            PartId source{params["source_part"].get<std::uint64_t>()};
            PartId target{params["target_part"].get<std::uint64_t>()};

            auto result = wf_reorchestrate(*score, region, source, target,
                                           session->undo_for(sid));
            if (!result) return error_response("wf_reorchestrate failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_double_part",
        "Double a part at a semitone interval into another part",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"source_part", "integer"}, {"target_part", "integer"},
         {"interval", "integer (semitones)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("source_part")) return error_response("source_part is required");
            if (!params.contains("target_part")) return error_response("target_part is required");

            auto region = region_from_json(params["region"]);
            PartId source{params["source_part"].get<std::uint64_t>()};
            PartId target{params["target_part"].get<std::uint64_t>()};
            auto interval = params.value("interval", std::int8_t{0});

            auto result = wf_double_part(*score, region, source, target, interval,
                                         session->undo_for(sid));
            if (!result) return error_response("wf_double_part failed");
            return mutation_result_j(*result);
        }
    );

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
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("level")) return error_response("level is required");

            auto region = region_from_json(params["region"]);
            auto dl = checked_enum<DynamicLevel>(params["level"].get<int>(), 13);
            if (!dl) return error_response("invalid dynamic level");

            auto result = wf_set_dynamics(*score, region, *dl,
                                          session->undo_for(sid));
            if (!result) return error_response("wf_set_dynamics failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_set_articulation",
        "Set the articulation for all notes in a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"},
         {"articulation", "integer (ArticulationType enum: 0=Staccato, 1=Staccatissimo, ...)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("region")) return error_response("region is required");
            if (!params.contains("articulation")) return error_response("articulation is required");

            auto region = region_from_json(params["region"]);
            auto at = checked_enum<ArticulationType>(params["articulation"].get<int>(), 24);
            if (!at) return error_response("invalid articulation type");

            auto result = wf_set_articulation(*score, region, *at,
                                              session->undo_for(sid));
            if (!result) return error_response("wf_set_articulation failed");
            return mutation_result_j(*result);
        }
    );

    // =========================================================================
    // Detail Tools
    // =========================================================================

    server.register_tool(
        "score_insert_note",
        "Insert a single note at a specific location",
        {{"score_id", "integer"}, {"part_id", "integer"}, {"bar", "integer (1-indexed)"},
         {"voice", "integer (optional, default 0)"},
         {"offset", "object {n, d} (beat offset within bar)"},
         {"pitch", "object {letter, accidental, octave}"},
         {"duration", "object {n, d}"},
         {"velocity", "integer (0-127, optional)"},
         {"articulation", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("part_id")) return error_response("part_id is required");
            if (!params.contains("bar")) return error_response("bar is required");
            if (!params.contains("pitch")) return error_response("pitch is required");
            if (!params.contains("duration")) return error_response("duration is required");

            PartId part_id{params["part_id"].get<std::uint64_t>()};
            auto bar = params["bar"].get<std::uint32_t>();
            auto voice = params.value("voice", std::uint8_t{0});

            Beat offset = Beat::zero();
            if (params.contains("offset"))
                offset = beat_from_json(params["offset"]);

            auto pitch = spelled_pitch_from_json(params["pitch"]);
            auto duration = beat_from_json(params["duration"]);

            Note note;
            note.pitch = pitch;
            if (params.contains("velocity")) {
                VelocityValue vv;
                vv.value = static_cast<std::uint8_t>(params["velocity"].get<int>());
                note.velocity = vv;
            }
            if (params.contains("articulation")) {
                auto at = checked_enum<ArticulationType>(params["articulation"].get<int>(), 24);
                if (at) note.articulation = *at;
            }

            auto result = wf_insert_note(*score, part_id, bar, voice, offset,
                                         note, duration, session->undo_for(sid));
            if (!result) return error_response("wf_insert_note failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_modify_note",
        "Modify properties of an existing note within a NoteGroup",
        {{"score_id", "integer"}, {"event_id", "integer"},
         {"note_index", "integer (optional, default 0)"},
         {"pitch", "object {letter, accidental, octave} (optional)"},
         {"duration", "object {n, d} (optional)"},
         {"velocity", "integer (optional)"},
         {"articulation", "integer (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("event_id")) return error_response("event_id is required");

            EventId event_id{params["event_id"].get<std::uint64_t>()};
            auto note_index = params.value("note_index", std::uint8_t{0});

            std::optional<SpelledPitch> pitch;
            if (params.contains("pitch"))
                pitch = spelled_pitch_from_json(params["pitch"]);

            std::optional<Beat> duration;
            if (params.contains("duration"))
                duration = beat_from_json(params["duration"]);

            std::optional<VelocityValue> velocity;
            if (params.contains("velocity")) {
                VelocityValue vv;
                vv.value = static_cast<std::uint8_t>(params["velocity"].get<int>());
                velocity = vv;
            }

            std::optional<ArticulationType> articulation;
            if (params.contains("articulation")) {
                auto at = checked_enum<ArticulationType>(params["articulation"].get<int>(), 24);
                if (at) articulation = *at;
            }

            auto result = wf_modify_note(*score, event_id, note_index,
                                         pitch, duration, velocity, articulation,
                                         session->undo_for(sid));
            if (!result) return error_response("wf_modify_note failed");
            return mutation_result_j(*result);
        }
    );

    server.register_tool(
        "score_delete_event",
        "Delete an event, replacing it with a rest of equal duration",
        {{"score_id", "integer"}, {"event_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("event_id")) return error_response("event_id is required");
            EventId event_id{params["event_id"].get<std::uint64_t>()};

            auto result = wf_delete_event(*score, event_id, session->undo_for(sid));
            if (!result) return error_response("wf_delete_event failed");
            return mutation_result_j(*result);
        }
    );

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
            auto sid = params["score_id"].get<std::uint64_t>();

            if (!params.contains("interval")) return error_response("interval is required");
            auto interval = diatonic_interval_from_json(params["interval"]);

            std::variant<EventId, ScoreRegion> target;
            if (params.contains("event_id")) {
                target = EventId{params["event_id"].get<std::uint64_t>()};
            } else if (params.contains("region")) {
                target = region_from_json(params["region"]);
            } else {
                return error_response("either event_id or region is required");
            }

            auto result = wf_transpose(*score, target, interval,
                                        session->undo_for(sid));
            if (!result) return error_response("wf_transpose failed");
            return mutation_result_j(*result);
        }
    );

    // =========================================================================
    // Analysis Tools (read-only)
    // =========================================================================

    server.register_tool(
        "score_analyze_harmony",
        "Analyse harmonic content within a region of the score",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            if (!params.contains("region")) return error_response("region is required");
            auto region = region_from_json(params["region"]);

            auto annotations = wf_analyze_harmony(*score, region);
            json result = json::array();
            for (const auto& a : annotations)
                result.push_back(harmonic_annotation_j(a));
            return {{"annotations", result}};
        }
    );

    server.register_tool(
        "score_get_orchestration",
        "Retrieve orchestration annotations (part-role pairs) for a region",
        {{"score_id", "integer"},
         {"region", "object {start_bar, end_bar, parts (optional)}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            if (!params.contains("region")) return error_response("region is required");
            auto region = region_from_json(params["region"]);

            auto orch = wf_get_orchestration(*score, region);
            json result = json::array();
            for (const auto& [pid, role] : orch)
                result.push_back({{"part_id", pid.value}, {"role", static_cast<int>(role)}});
            return {{"orchestration", result}};
        }
    );

    server.register_tool(
        "score_get_reduction",
        "Produce a reduced view of the score (piano, short, skeleton)",
        {{"score_id", "integer"}, {"view_type", "string (piano|short|skeleton)"},
         {"region", "object {start_bar, end_bar, parts (optional)} (optional)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto view_type = params.value("view_type", "piano");

            std::optional<ScoreRegion> region;
            if (params.contains("region"))
                region = region_from_json(params["region"]);

            auto reduced = wf_get_reduction(*score, view_type, region);

            // Store the reduction as a new score in the session
            auto id = session->next_score_id++;
            session->scores[id] = std::move(reduced);

            return {{"score_id", id}, {"view_type", view_type},
                    {"parts", session->scores[id].parts.size()}};
        }
    );

    server.register_tool(
        "score_validate",
        "Run all validation rules on the score",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto diagnostics = wf_validate_score(*score);
            bool compilable = is_compilable(*score);

            json diags = json::array();
            for (const auto& d : diagnostics)
                diags.push_back(diagnostic_j(d));

            return {{"valid", diagnostics.empty()}, {"compilable", compilable},
                    {"diagnostics", diags}};
        }
    );

    server.register_tool(
        "score_get_form_summary",
        "Produce a condensed form summary of the score's sections",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto entries = wf_get_form_summary(*score);
            json result = json::array();
            for (const auto& e : entries)
                result.push_back(form_entry_j(e));
            return {{"sections", result}};
        }
    );

    // =========================================================================
    // Serialisation
    // =========================================================================

    server.register_tool(
        "score_get_json",
        "Serialise the score to JSON",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            return score_to_json(*score);
        }
    );

    // =========================================================================
    // Compilation
    // =========================================================================

    server.register_tool(
        "score_compile_to_midi",
        "Compile the score to MIDI event data",
        {{"score_id", "integer"}, {"ppq", "integer (optional, default 480)"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto ppq = params.value("ppq", 480);
            auto result = wf_compile_to_midi(*score, ppq);
            if (!result) return error_response("compile_to_midi failed: score not compilable");

            json notes = json::array();
            for (const auto& n : result->midi.notes) {
                notes.push_back({
                    {"tick", n.tick}, {"duration_ticks", n.duration_ticks},
                    {"channel", n.channel}, {"note", n.note}, {"velocity", n.velocity}
                });
            }

            json tempos = json::array();
            for (const auto& t : result->midi.tempos)
                tempos.push_back({{"tick", t.tick}, {"microseconds_per_beat", t.microseconds_per_beat}});

            json time_sigs = json::array();
            for (const auto& ts : result->midi.time_signatures)
                time_sigs.push_back({{"tick", ts.tick}, {"numerator", ts.numerator},
                                     {"denominator", ts.denominator}});

            json key_sigs = json::array();
            for (const auto& ks : result->midi.key_signatures)
                key_sigs.push_back({{"tick", ks.tick}, {"accidentals", ks.accidentals},
                                    {"minor", ks.minor}});

            return {
                {"ppq", result->midi.ppq},
                {"notes", notes}, {"tempos", tempos},
                {"time_signatures", time_sigs}, {"key_signatures", key_sigs},
                {"report", compilation_report_j(result->report)}
            };
        }
    );

    server.register_tool(
        "score_compile_to_musicxml",
        "Compile the score to MusicXML",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto result = wf_compile_to_musicxml(*score);
            if (!result) return error_response("compile_to_musicxml failed");

            return {
                {"xml", result->xml},
                {"report", compilation_report_j(result->report)}
            };
        }
    );

    server.register_tool(
        "score_compile_to_lilypond",
        "Compile the score to LilyPond notation",
        {{"score_id", "integer"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            auto result = wf_compile_to_lilypond(*score);
            if (!result) return error_response("compile_to_lilypond failed");

            return {
                {"ly", result->ly},
                {"report", compilation_report_j(result->report)}
            };
        }
    );

    // =========================================================================
    // Query Tools
    // =========================================================================

    server.register_tool(
        "score_query_harmony_at",
        "Find the harmonic annotation active at a given time",
        {{"score_id", "integer"}, {"time", "object {bar, beat_n, beat_d}"}},
        [session](const json& params) -> json {
            json err;
            auto* score = lookup_score(session, params, err);
            if (!score) return err;

            if (!params.contains("time")) return error_response("time is required");
            auto time = score_time_from_json(params["time"]);

            auto ha = query_harmony_at(*score, time);
            if (!ha) return {{"found", false}};
            json r = harmonic_annotation_j(*ha);
            r["found"] = true;
            return r;
        }
    );

    server.register_tool(
        "score_find_motif",
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
            if (!params.contains("region")) return error_response("region is required");

            std::vector<PitchClass> pattern;
            for (const auto& p : params["pattern"])
                pattern.push_back(static_cast<PitchClass>(p.get<int>()));

            auto region = region_from_json(params["region"]);
            auto occurrences = query_find_motif(*score, pattern, region);

            json result = json::array();
            for (const auto& o : occurrences)
                result.push_back(motif_occurrence_j(o));
            return {{"occurrences", result}, {"count", occurrences.size()}};
        }
    );
}

}  // namespace Sunny::Infrastructure
