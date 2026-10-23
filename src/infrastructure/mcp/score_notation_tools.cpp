/** @file score_notation_tools.cpp
 *  @brief Typed exact musical authoring at the shared request/history boundary.
 */
#include "evidence_encoding.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/infrastructure/mcp/score_notation_tools.hpp>

namespace sunny::infrastructure {
namespace {

using json = nlohmann::json;
using namespace sunny::core;

json fraction_schema() {
    return {{"type", "object"},
            {"properties", {{"n", {{"type", "integer"}}}, {"d", {{"type", "integer"}}}}},
            {"required", {"n", "d"}}};
}

json time_schema() {
    return {{"type", "object"},
            {"properties",
             {{"bar", {{"type", "integer"}}},
              {"beat_n", {{"type", "integer"}}},
              {"beat_d", {{"type", "integer"}}}}},
            {"required", {"bar", "beat_n", "beat_d"}}};
}

json region_schema() {
    return {{"type", "object"},
            {"description", "Half-open exact ScoreTime span; omitted parts selects all Parts"},
            {"properties",
             {{"start", time_schema()},
              {"end", time_schema()},
              {"parts", {{"type", "array"}, {"items", {{"type", "integer"}}}}}}},
            {"required", {"start", "end"}}};
}

json object_schema(json properties, json required) {
    return {{"type", "object"},
            {"properties", std::move(properties)},
            {"required", std::move(required)}};
}

Beat parse_fraction(const json& value) {
    const auto numerator =
        detail::checked_integer<std::int64_t>(value.at("n"), "rational numerator");
    const auto denominator =
        detail::checked_integer<std::int64_t>(value.at("d"), "rational denominator");
    if (denominator <= 0) throw std::invalid_argument("rational denominator must be positive");
    auto result = Beat::from_ratio(numerator, denominator);
    if (!result) throw std::invalid_argument("rational value exceeds exact Beat arithmetic");
    return *result;
}

ScoreTime parse_time(const json& value) {
    const auto bar = detail::checked_integer<std::uint32_t>(value.at("bar"), "ScoreTime bar");
    const auto numerator =
        detail::checked_integer<std::int64_t>(value.at("beat_n"), "ScoreTime beat numerator");
    const auto denominator =
        detail::checked_integer<std::int64_t>(value.at("beat_d"), "ScoreTime beat denominator");
    if (bar == 0 || numerator < 0 || denominator <= 0)
        throw std::invalid_argument("ScoreTime needs a positive bar and nonnegative exact beat");
    auto beat = Beat::from_ratio(numerator, denominator);
    if (!beat) throw std::invalid_argument("ScoreTime exceeds exact Beat arithmetic");
    return {bar, *beat};
}

ScoreRegion parse_region(const json& value) {
    ScoreRegion result{parse_time(value.at("start")), parse_time(value.at("end")), {}};
    if (value.contains("parts"))
        for (const auto& part : value.at("parts"))
            result.parts.push_back(PartId{detail::checked_integer<std::uint64_t>(part, "PartId")});
    return result;
}

constexpr std::array<std::string_view, 8> unit_names{"whole",
                                                     "half",
                                                     "dotted_half",
                                                     "quarter",
                                                     "dotted_quarter",
                                                     "eighth",
                                                     "dotted_eighth",
                                                     "sixteenth"};

BeatUnit parse_unit(const json& value) {
    const auto name = value.get<std::string>();
    for (std::size_t index = 0; index < unit_names.size(); ++index)
        if (name == unit_names[index]) return static_cast<BeatUnit>(index);
    throw std::invalid_argument("unsupported tempo beat_unit");
}

json unit_schema() {
    json names = json::array();
    for (const auto name : unit_names)
        names.push_back(name);
    return {{"type", "string"}, {"enum", std::move(names)}};
}

TempoEvent parse_tempo(const json& value) {
    const auto bpm = parse_fraction(value.at("bpm"));
    auto rate = PositiveRational::from_ratio(bpm.numerator(), bpm.denominator());
    if (!rate) throw std::invalid_argument("tempo bpm must be a positive exact rational");
    TempoEvent result{};
    result.position = parse_time(value.at("position"));
    result.bpm = *rate;
    result.beat_unit = parse_unit(value.at("beat_unit"));
    result.old_unit = BeatUnit::Quarter;
    result.new_unit = BeatUnit::Quarter;
    const auto transition = value.at("transition").get<std::string>();
    if (transition == "immediate")
        result.transition_type = TempoTransitionType::Immediate;
    else if (transition == "linear") {
        result.transition_type = TempoTransitionType::Linear;
        if (!value.contains("linear_duration"))
            throw std::invalid_argument("incoming linear tempo needs explicit linear_duration");
    } else if (transition == "metric_modulation") {
        result.transition_type = TempoTransitionType::MetricModulation;
        if (!value.contains("old_unit") || !value.contains("new_unit"))
            throw std::invalid_argument("metric modulation needs explicit old_unit and new_unit");
    } else
        throw std::invalid_argument("unsupported incoming tempo transition");
    if (value.contains("linear_duration"))
        result.linear_duration = parse_fraction(value.at("linear_duration"));
    if (value.contains("old_unit")) result.old_unit = parse_unit(value.at("old_unit"));
    if (value.contains("new_unit")) result.new_unit = parse_unit(value.at("new_unit"));
    return result;
}

std::pair<std::uint64_t, Score*> lookup(const std::shared_ptr<ScoreSession>& session,
                                        const json& params) {
    const auto id = detail::checked_integer<std::uint64_t>(params.at("score_id"), "score_id");
    auto* score = session->find(id);
    if (!score) throw std::invalid_argument("score not found: " + std::to_string(id));
    return {id, score};
}

json failed(ErrorCode error) {
    return {{"error", "Score mutation failed: error " + std::to_string(static_cast<int>(error))},
            {"error_code", static_cast<int>(error)}};
}

json completed(const MutationResult& mutation, const Score& score) {
    json result{{"ok", true}, {"version", score.version}};
    if (!mutation.diagnostics.empty())
        result["diagnostics"] = mcp_detail::encode_diagnostics(mutation.diagnostics);
    return result;
}

SpelledPitch parse_pitch(const json& value) {
    const auto letter = value.at("letter").get<std::string>();
    constexpr std::string_view letters = "CDEFGAB";
    const auto index = letter.size() == 1 ? letters.find(letter[0]) : std::string_view::npos;
    if (index == std::string_view::npos)
        throw std::invalid_argument("axis letter must be C,D,E,F,G,A or B");
    return {static_cast<std::uint8_t>(index),
            detail::checked_integer<std::int8_t>(value.at("accidental"), "axis accidental"),
            detail::checked_integer<std::int8_t>(value.at("octave"), "axis octave")};
}

} // namespace

void register_score_notation_tools(McpServer& server, std::shared_ptr<ScoreSession> session) {
    if (!session) throw std::invalid_argument("notation tools require the shared ScoreSession");
    auto scope = server.registration_scope(McpDocumentDomain::Score);
    const json tempo_schema = object_schema(
        {{"position", time_schema()},
         {"bpm", fraction_schema()},
         {"beat_unit", unit_schema()},
         {"transition",
          {{"type", "string"}, {"enum", {"immediate", "linear", "metric_modulation"}}}},
         {"linear_duration", fraction_schema()},
         {"old_unit", unit_schema()},
         {"new_unit", unit_schema()}},
        {"position", "bpm", "beat_unit", "transition"});
    server.register_tool("score_set_tempo_map",
                         "Replace the exact ordered tempo map; incoming Linear duration and "
                         "MetricModulation rates must be coherent",
                         object_schema({{"score_id", {{"type", "integer"}}},
                                        {"entries", {{"type", "array"}, {"items", tempo_schema}}}},
                                       {"score_id", "entries"}),
                         [session](const json& params) -> json {
                             auto [id, score] = lookup(session, params);
                             TempoMap tempos;
                             for (const auto& value : params.at("entries"))
                                 tempos.push_back(parse_tempo(value));
                             auto result =
                                 set_tempo_map(*score, std::move(tempos), session->undo_for(id));
                             if (!result) return failed(result.error());
                             auto response = completed(*result, *score);
                             response["tempo_events"] = score->tempo_map.size();
                             return response;
                         });
    server.register_tool(
        "score_create_tuplet_group",
        "Annotate distinct contiguous already-scaled note/rest events in one voice/measure; "
        "durations are preserved and nested edits rejected",
        object_schema(
            {{"score_id", {{"type", "integer"}}},
             {"event_ids", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
             {"actual", {{"type", "integer"}}},
             {"normal", {{"type", "integer"}}},
             {"normal_type", fraction_schema()},
             {"scaled_allocation", fraction_schema()}},
            {"score_id", "event_ids", "actual", "normal", "normal_type", "scaled_allocation"}),
        [session](const json& params) -> json {
            auto [id, score] = lookup(session, params);
            std::vector<EventId> members;
            for (const auto& value : params.at("event_ids"))
                members.push_back(
                    EventId{detail::checked_integer<std::uint64_t>(value, "EventId")});
            const auto actual =
                detail::checked_integer<std::uint8_t>(params.at("actual"), "tuplet actual count");
            const auto normal =
                detail::checked_integer<std::uint8_t>(params.at("normal"), "tuplet normal count");
            const auto normal_type = parse_fraction(params.at("normal_type"));
            const auto allocation = parse_fraction(params.at("scaled_allocation"));
            auto result = create_tuplet_group(
                *score, members, actual, normal, normal_type, allocation, session->undo_for(id));
            if (!result) return failed(result.error());
            return {{"ok", true}, {"version", score->version}, {"tuplet_id", result->value}};
        });
    server.register_tool(
        "score_remove_tuplet_group",
        "Remove a standalone tuplet annotation without changing already-scaled durations; its "
        "identity remains reserved",
        object_schema({{"score_id", {{"type", "integer"}}}, {"tuplet_id", {{"type", "integer"}}}},
                      {"score_id", "tuplet_id"}),
        [session](const json& params) -> json {
            auto [id, score] = lookup(session, params);
            const auto tuplet = TupletId{
                detail::checked_integer<std::uint64_t>(params.at("tuplet_id"), "TupletId")};
            auto result = remove_tuplet_group(*score, tuplet, session->undo_for(id));
            return result ? completed(*result, *score) : failed(result.error());
        });

    enum class RegionEdit { Delete, Copy, Move, Retrograde, Invert, Augment, Diminish };
    for (const auto operation : {RegionEdit::Delete,
                                 RegionEdit::Copy,
                                 RegionEdit::Move,
                                 RegionEdit::Retrograde,
                                 RegionEdit::Invert,
                                 RegionEdit::Augment,
                                 RegionEdit::Diminish}) {
        std::string name, description;
        switch (operation) {
        case RegionEdit::Delete:
            name = "score_delete_region";
            description =
                "Replace selected measured content with rests and repair paired endpoints";
            break;
        case RegionEdit::Copy:
            name = "score_copy_region";
            description = "Copy selected notes to an exact destination, preserving fully selected "
                          "paired notation";
            break;
        case RegionEdit::Move:
            name = "score_move_region";
            description = "Move selected notes atomically to an exact destination";
            break;
        case RegionEdit::Retrograde:
            name = "score_retrograde_region";
            description = "Reverse selected note positions and ordering within an exact region";
            break;
        case RegionEdit::Invert:
            name = "score_invert_region";
            description = "Mirror selected concert pitches about an exact spelled-pitch axis";
            break;
        case RegionEdit::Augment:
            name = "score_augment_region";
            description = "Multiply selected note durations by an exact positive factor; conflicts "
                          "reject atomically";
            break;
        case RegionEdit::Diminish:
            name = "score_diminish_region";
            description = "Divide selected note durations by an exact positive factor; conflicts "
                          "reject atomically";
            break;
        }
        json properties{{"score_id", {{"type", "integer"}}}, {"region", region_schema()}};
        json required{"score_id", "region"};
        if (operation == RegionEdit::Copy || operation == RegionEdit::Move) {
            properties["destination"] = time_schema();
            required.push_back("destination");
        } else if (operation == RegionEdit::Invert) {
            properties["axis"] = object_schema(
                {{"letter", {{"type", "string"}, {"enum", {"C", "D", "E", "F", "G", "A", "B"}}}},
                 {"accidental", {{"type", "integer"}}},
                 {"octave", {{"type", "integer"}}}},
                {"letter", "accidental", "octave"});
            required.push_back("axis");
        } else if (operation == RegionEdit::Augment || operation == RegionEdit::Diminish) {
            properties["factor"] = fraction_schema();
            required.push_back("factor");
        }
        server.register_tool(
            name,
            description,
            object_schema(std::move(properties), std::move(required)),
            [session, operation](const json& params) -> json {
                auto [id, score] = lookup(session, params);
                const auto region = parse_region(params.at("region"));
                // Parse all operation-specific payloads before acquiring history.
                std::optional<ScoreTime> destination;
                std::optional<SpelledPitch> axis;
                std::optional<Beat> factor;
                if (operation == RegionEdit::Copy || operation == RegionEdit::Move)
                    destination = parse_time(params.at("destination"));
                if (operation == RegionEdit::Invert) axis = parse_pitch(params.at("axis"));
                if (operation == RegionEdit::Augment || operation == RegionEdit::Diminish)
                    factor = parse_fraction(params.at("factor"));
                auto* history = session->undo_for(id);
                Result<MutationResult> result = std::unexpected(ErrorCode::InvalidMutation);
                switch (operation) {
                case RegionEdit::Delete:
                    result = delete_region(*score, region, history);
                    break;
                case RegionEdit::Copy:
                    result = copy_region(*score, region, *destination, history);
                    break;
                case RegionEdit::Move:
                    result = move_region(*score, region, *destination, history);
                    break;
                case RegionEdit::Retrograde:
                    result = retrograde_region(*score, region, history);
                    break;
                case RegionEdit::Invert:
                    result = invert_region(*score, region, *axis, history);
                    break;
                case RegionEdit::Augment:
                    result = augment_region(*score, region, *factor, history);
                    break;
                case RegionEdit::Diminish:
                    result = diminute_region(*score, region, *factor, history);
                    break;
                }
                return result ? completed(*result, *score) : failed(result.error());
            });
    }
}

} // namespace sunny::infrastructure
