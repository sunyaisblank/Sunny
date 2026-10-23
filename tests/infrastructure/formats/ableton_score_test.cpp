/**
 * @file ableton_score_test.cpp
 * @brief Unit tests for Ableton Live Compiler
 *
 *
 * Coverage: Track creation, clip creation, note injection, tempo,
 *           section markers, multi-part scores, pan assignment
 *
 * Uses CommandBuffer transport so tests run without Ableton.
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <map>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>

// Score IR workflow for test score creation
#include <sunny/core/score/workflows.hpp>

using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_test_score(std::uint32_t bars = 4, int time_sig_num = 4, int time_sig_den = 4) {
    ScoreSpec spec;
    spec.title = "Test Score";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4}; // C4
    spec.key_accidentals = 0;
    spec.time_sig_num = time_sig_num;
    spec.time_sig_den = time_sig_den;

    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.clef = Clef::Treble;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

Score make_two_part_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Two Part Score";
    spec.total_bars = bars;
    spec.bpm = 100.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition violin;
    violin.name = "Violin";
    violin.abbreviation = "Vln.";
    violin.instrument_type = InstrumentType::Violin;
    violin.clef = Clef::Treble;
    violin.rendering.midi_channel = 1;
    violin.rendering.pan = -0.5f;
    spec.parts.push_back(violin);

    PartDefinition cello;
    cello.name = "Cello";
    cello.abbreviation = "Vc.";
    cello.instrument_type = InstrumentType::Cello;
    cello.clef = Clef::Bass;
    cello.rendering.midi_channel = 2;
    cello.rendering.pan = 0.5f;
    spec.parts.push_back(cello);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

void insert_note(
    Score& score, std::size_t part_idx, SpelledPitch pitch, Beat offset, Beat duration) {
    auto& voice = score.parts[part_idx].measures[0].voices[0];
    Note note;
    note.pitch = pitch;
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = duration;

    // Replace events: note at offset, rest fills remainder
    voice.events.clear();
    const auto event_base = static_cast<std::uint64_t>(9000000 + part_idx * 10);
    if (offset != Beat::zero()) {
        RestEvent pre_rest{offset, true};
        voice.events.push_back(Event{EventId{event_base + 1}, Beat::zero(), pre_rest});
    }
    EventId note_id{event_base + 2};
    voice.events.push_back(Event{note_id, offset, ng});

    // Remaining rest to fill the bar (4/4 = Beat{1,1})
    Beat after_note = offset + duration;
    Beat bar_dur{1, 1}; // 4/4 bar in whole-note units
    if (after_note < bar_dur) {
        Beat rest_dur = bar_dur - after_note;
        RestEvent post_rest{rest_dur, true};
        voice.events.push_back(Event{EventId{event_base + 3}, after_note, post_rest});
    }
}

class FailingTransport : public LomTransport {
  public:
    LomResponse send(const LomRequest&) override {
        return {false, std::nullopt, "transport failure"};
    }
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override {
        return {false, std::nullopt, "transport failure"};
    }
    [[nodiscard]] bool is_connected() const override { return false; }
    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return std::optional<AbletonTargetProfile>{modeled_target_profile({12, 3, 0, "12.3.0"})};
    }
    [[nodiscard]] Result<std::optional<std::uint32_t>> scene_count() override {
        return std::optional<std::uint32_t>{1};
    }
};

class ScoreReadbackTransport final : public LomTransport {
  public:
    LomResponse send(const LomRequest& request) override {
        requests.push_back(request);
        if (request.type == LomRequestType::CallMethod &&
            (request.property_or_method == "get_notes_by_id" ||
             request.property_or_method == "get_notes_extended" ||
             request.property_or_method == "get_all_notes_extended")) {
            if (malformed_note_readback)
                return {true,
                        nlohmann::json{{"notes", nlohmann::json::array({{{"note_id", 100}}})}},
                        std::nullopt};
            nlohmann::json notes = nlohmann::json::array();
            for (std::size_t reverse = last_notes.size(); reverse > 0; --reverse) {
                const auto index = reverse - 1;
                const auto& note = last_notes[index];
                int pitch = static_cast<int>(note.pitch);
                if (diverge_note_readback && index == 0) ++pitch;
                const auto note_id = wrong_note_readback_ids && index == 0 ? last_ids[index] + 1000
                                                                           : last_ids[index];
                notes.push_back({{"note_id", note_id},
                                 {"pitch", pitch},
                                 {"start_time", note.start_time},
                                 {"duration", note.duration},
                                 {"velocity", static_cast<int>(note.velocity)},
                                 {"mute", note.muted},
                                 {"probability", note.probability},
                                 {"velocity_deviation", note.velocity_deviation},
                                 {"release_velocity", note.release_velocity}});
                if (integral_probability_readback && index == 0) notes.back()["probability"] = 1;
                if (diverge_probability_readback && index == 0) notes.back()["probability"] = 0.5;
            }
            if (extra_note_readback && request.property_or_method == "get_all_notes_extended") {
                notes.push_back({{"note_id", 9999},
                                 {"pitch", 72},
                                 {"start_time", 0.0},
                                 {"duration", 1.0},
                                 {"velocity", 100.0},
                                 {"mute", false},
                                 {"probability", 1.0},
                                 {"velocity_deviation", 0.0},
                                 {"release_velocity", 64.0}});
            }
            return {true, nlohmann::json{{"notes", std::move(notes)}}, std::nullopt};
        }
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "sunny_set_cue") {
            if (malformed_cue_evidence)
                return {true, nlohmann::json{{"action", "created"}}, std::nullopt};
            const auto time = std::get<double>(request.args.at(0));
            const auto& name = std::get<std::string>(request.args.at(1));
            const auto action = cue_calls++ == 0 ? "created" : "updated";
            return {true,
                    nlohmann::json{{"action", action},
                                   {"requested_time", time},
                                   {"observed_time", time},
                                   {"requested_name", name},
                                   {"observed_name", name}},
                    std::nullopt};
        }
        if (request.type == LomRequestType::CallMethod &&
            request.property_or_method == "sunny_clear_all_envelopes") {
            if (malformed_envelope_evidence)
                return {true, nlohmann::json{{"has_envelopes", 0}}, std::nullopt};
            return {true, nlohmann::json{{"has_envelopes", diverge_envelope_clear}}, std::nullopt};
        }
        if (request.type != LomRequestType::SetProperty) return {true, std::nullopt, std::nullopt};
        if (omit_property_evidence) return {true, std::nullopt, std::nullopt};

        const auto requested =
            std::visit([](const auto& value) { return nlohmann::json(value); }, request.args.at(0));
        nlohmann::json observed = requested;
        if (diverge_tempo && request.property_or_method == "tempo") observed = 119.0;
        if (malformed_tempo && request.property_or_method == "tempo") observed = "119.0";
        if (malformed_groove && request.property_or_method == "groove") observed = false;
        if (fractional_integer_observation && request.property_or_method == "signature_numerator")
            observed = 4.000001;
        nlohmann::json evidence = {{"property", request.property_or_method},
                                   {"requested", requested},
                                   {"observed", observed}};
        if (fractional_integer_echo && request.property_or_method == "signature_numerator")
            evidence["requested"] = 4.0;
        if (extra_property_evidence && request.property_or_method == "tempo")
            evidence["unexpected"] = true;
        return {true, std::move(evidence), std::nullopt};
    }

    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>& notes) override {
        last_notes = notes;
        if (malformed_note_evidence) return {true, std::vector<int>{41, 41}, std::nullopt};
        std::vector<int> ids;
        ids.reserve(notes.size());
        for (std::size_t index = 0; index < notes.size(); ++index)
            ids.push_back(static_cast<int>(100 + index));
        last_ids = ids;
        return {true, std::move(ids), std::nullopt};
    }

    [[nodiscard]] bool is_connected() const override { return true; }

    [[nodiscard]] Result<std::optional<AbletonTargetProfile>> target_profile() override {
        return std::optional<AbletonTargetProfile>{modeled_target_profile(version)};
    }

    [[nodiscard]] Result<std::optional<std::uint32_t>> scene_count() override {
        return std::optional<std::uint32_t>{1};
    }

    std::vector<LomRequest> requests;
    AbletonVersion version{12, 3, 0, "12.3.0"};
    bool omit_property_evidence = false;
    bool diverge_tempo = false;
    bool malformed_tempo = false;
    bool malformed_groove = false;
    bool extra_property_evidence = false;
    bool fractional_integer_observation = false;
    bool fractional_integer_echo = false;
    bool malformed_cue_evidence = false;
    bool malformed_note_evidence = false;
    bool malformed_note_readback = false;
    bool wrong_note_readback_ids = false;
    bool extra_note_readback = false;
    bool diverge_note_readback = false;
    bool integral_probability_readback = false;
    bool diverge_probability_readback = false;
    bool malformed_envelope_evidence = false;
    bool diverge_envelope_clear = false;
    std::size_t cue_calls = 0;
    std::vector<LomNoteData> last_notes;
    std::vector<int> last_ids;
};

} // namespace

TEST_CASE("Ableton preserves complete Score tuning as an explicit unsupported residual",
          "[ableton][score][tuning]") {
    auto score = make_test_score(1);
    score.tuning.name = "custom non-octave";
    score.tuning.cents_from_reference[60] = -901.25;
    CommandBuffer transport;

    auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->tuning_definitions_requested == 1);
    CHECK(result->tuning_definitions_written == 0);
    CHECK(result->requested_tuning == score.tuning);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("non-standard score tuning") != std::string::npos;
    }));
}

// =============================================================================
// Basic compilation
// =============================================================================

TEST_CASE("empty score compiles to tracks and clips", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    CHECK(result->tracks_created == 1);
    CHECK(result->clips_created == 1);
    CHECK(result->clip_envelope_clears_requested == 1);
    CHECK(result->clip_envelope_clears_executed == 0);
    CHECK(result->clip_envelope_clears_verified == 0);
    REQUIRE(result->clip_envelope_deployments.size() == 1);
    CHECK(result->clip_envelope_deployments[0].action == AbletonClipEnvelopeAction::RecordedOnly);
    CHECK_FALSE(result->clip_envelope_deployments[0].observed_has_envelopes.has_value());
    CHECK(result->key_signature_events_requested == 1);
    CHECK(result->key_signature_events_written == 0);
    CHECK(result->tempo_events_written >= 1);
    CHECK(result->property_writes == 23);
    CHECK(result->property_writes_verified == 0);
    REQUIRE(result->property_deployments.size() == 23);
    const auto arm =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [](const auto& deployment) {
                         return deployment.path == "song/tracks/0" && deployment.property == "arm";
                     });
    const auto implicit_arm = std::find_if(result->property_deployments.begin(),
                                           result->property_deployments.end(),
                                           [](const auto& deployment) {
                                               return deployment.path == "song/tracks/0" &&
                                                      deployment.property == "implicit_arm";
                                           });
    REQUIRE(arm != result->property_deployments.end());
    REQUIRE(implicit_arm != result->property_deployments.end());
    CHECK(arm->requested == false);
    CHECK(implicit_arm->requested == false);
    for (const auto& deployment : result->property_deployments) {
        CHECK_FALSE(deployment.observed.has_value());
        CHECK_FALSE(deployment.verified);
    }
}

TEST_CASE("clip envelope clear evidence is closed and divergence remains incomplete",
          "[ableton][compiler][clip-envelope][trust-boundary]") {
    auto score = make_test_score(4);
    ScoreReadbackTransport transport;
    transport.malformed_envelope_evidence = true;

    auto result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.malformed_envelope_evidence = false;
    transport.diverge_envelope_clear = true;
    result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->clip_envelope_clears_requested == 1);
    CHECK(result->clip_envelope_clears_executed == 1);
    CHECK(result->clip_envelope_clears_verified == 0);
    REQUIRE(result->clip_envelope_deployments.size() == 1);
    CHECK(result->clip_envelope_deployments[0].observed_has_envelopes == true);
    CHECK_FALSE(result->clip_envelope_deployments[0].verified);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("envelope clear") != std::string::npos;
    }));
}

TEST_CASE("score compiler creates scene zero when the Session has no clip slots",
          "[ableton][compiler][target-state]") {
    auto score = make_test_score(1);
    CommandBuffer buffer;
    buffer.set_scene_count(0);

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->scenes_created == 1);
    REQUIRE_FALSE(buffer.entries().empty());

    const auto scene =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "create_scene";
        });
    const auto track =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "create_midi_track";
        });
    REQUIRE(scene != buffer.entries().end());
    REQUIRE(track != buffer.entries().end());
    CHECK(scene < track);
    REQUIRE(scene->request.args.size() == 1);
    CHECK(std::get<int>(scene->request.args.front()) == 0);
    const auto count = buffer.scene_count();
    REQUIRE(count.has_value());
    REQUIRE(count->has_value());
    CHECK(**count == 1);
}

TEST_CASE("score compiler rejects initial meters outside Live's value domain before mutation",
          "[ableton][compiler][target-value]") {
    auto score = make_test_score(1);

    SECTION("numerator") {
        const auto signature = make_time_signature(100, 4);
        REQUIRE(signature.has_value());
        score.time_map.front().time_signature = *signature;
    }
    SECTION("denominator") {
        const auto signature = make_time_signature(4, 32);
        REQUIRE(signature.has_value());
        score.time_map.front().time_signature = *signature;
    }

    CommandBuffer buffer;
    const auto result = compile_to_ableton(score, buffer);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetValueUnrepresentable);
    CHECK(buffer.entries().empty());
}

TEST_CASE("score compiler explicitly installs the initial meter on every created clip",
          "[ableton][compiler][meter][readback]") {
    auto score = make_test_score(1, 7, 8);
    ScoreReadbackTransport transport;

    const auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->time_signature_events_written == 1);

    const auto clip_path = LomPaths::clip(0, 0).to_string();
    const auto clip_numerator = std::find_if(
        result->property_deployments.begin(),
        result->property_deployments.end(),
        [&](const auto& deployment) {
            return deployment.path == clip_path && deployment.property == "signature_numerator";
        });
    const auto clip_denominator = std::find_if(
        result->property_deployments.begin(),
        result->property_deployments.end(),
        [&](const auto& deployment) {
            return deployment.path == clip_path && deployment.property == "signature_denominator";
        });

    REQUIRE(clip_numerator != result->property_deployments.end());
    REQUIRE(clip_denominator != result->property_deployments.end());
    CHECK(clip_numerator->requested == 7);
    CHECK(clip_denominator->requested == 8);
    CHECK(clip_numerator->verified);
    CHECK(clip_denominator->verified);

    const auto scene_tempo =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [](const auto& deployment) {
                         return deployment.path == LomPaths::scene(0).to_string() &&
                                deployment.property == "tempo_enabled";
                     });
    const auto scene_meter =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [](const auto& deployment) {
                         return deployment.path == LomPaths::scene(0).to_string() &&
                                deployment.property == "time_signature_enabled";
                     });
    const auto scene_name =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [](const auto& deployment) {
                         return deployment.path == LomPaths::scene(0).to_string() &&
                                deployment.property == "name";
                     });
    REQUIRE(scene_name != result->property_deployments.end());
    REQUIRE(scene_tempo != result->property_deployments.end());
    REQUIRE(scene_meter != result->property_deployments.end());
    CHECK(scene_name->requested == "Test Score");
    CHECK(scene_tempo->requested == false);
    CHECK(scene_meter->requested == false);
    CHECK(scene_tempo->verified);
    CHECK(scene_meter->verified);

    const auto clip_start = std::find_if(result->property_deployments.begin(),
                                         result->property_deployments.end(),
                                         [&](const auto& deployment) {
                                             return deployment.path == clip_path &&
                                                    deployment.property == "start_marker";
                                         });
    const auto clip_end =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "end_marker";
                     });
    const auto clip_looping =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "looping";
                     });
    const auto clip_muted =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "muted";
                     });
    const auto clip_groove =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "groove";
                     });
    const auto clip_launch_mode = std::find_if(result->property_deployments.begin(),
                                               result->property_deployments.end(),
                                               [&](const auto& deployment) {
                                                   return deployment.path == clip_path &&
                                                          deployment.property == "launch_mode";
                                               });
    const auto clip_launch_quantization = std::find_if(
        result->property_deployments.begin(),
        result->property_deployments.end(),
        [&](const auto& deployment) {
            return deployment.path == clip_path && deployment.property == "launch_quantization";
        });
    const auto clip_legato =
        std::find_if(result->property_deployments.begin(),
                     result->property_deployments.end(),
                     [&](const auto& deployment) {
                         return deployment.path == clip_path && deployment.property == "legato";
                     });
    const auto clip_velocity_amount = std::find_if(
        result->property_deployments.begin(),
        result->property_deployments.end(),
        [&](const auto& deployment) {
            return deployment.path == clip_path && deployment.property == "velocity_amount";
        });
    REQUIRE(clip_start != result->property_deployments.end());
    REQUIRE(clip_end != result->property_deployments.end());
    REQUIRE(clip_looping != result->property_deployments.end());
    REQUIRE(clip_muted != result->property_deployments.end());
    REQUIRE(clip_groove != result->property_deployments.end());
    REQUIRE(clip_launch_mode != result->property_deployments.end());
    REQUIRE(clip_launch_quantization != result->property_deployments.end());
    REQUIRE(clip_legato != result->property_deployments.end());
    REQUIRE(clip_velocity_amount != result->property_deployments.end());
    CHECK(clip_start->requested == 0.0);
    CHECK(clip_end->requested == 3.5);
    CHECK(clip_looping->requested == false);
    CHECK(clip_muted->requested == false);
    CHECK(clip_groove->requested.is_null());
    CHECK(clip_launch_mode->requested == 0);
    CHECK(clip_launch_quantization->requested == 1);
    CHECK(clip_legato->requested == false);
    CHECK(clip_velocity_amount->requested == 0.0);
    CHECK(clip_start->verified);
    CHECK(clip_end->verified);
    CHECK(clip_looping->verified);
    CHECK(clip_muted->verified);
    CHECK(clip_groove->verified);
    CHECK(clip_launch_mode->verified);
    CHECK(clip_launch_quantization->verified);
    CHECK(clip_legato->verified);
    CHECK(clip_velocity_amount->verified);
}

TEST_CASE("live score property writes require and expose readback evidence",
          "[ableton][compiler][target-profile]") {
    auto score = make_test_score(4);
    ScoreReadbackTransport transport;

    auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->property_writes == 23);
    CHECK(result->property_writes_verified == 23);
    REQUIRE(result->property_deployments.size() == 23);
    for (const auto& deployment : result->property_deployments) {
        CHECK(deployment.verified);
        REQUIRE(deployment.observed.has_value());
        CHECK(*deployment.observed == deployment.requested);
    }
    CHECK(result->clip_envelope_clears_requested == 1);
    CHECK(result->clip_envelope_clears_executed == 1);
    CHECK(result->clip_envelope_clears_verified == 1);
    REQUIRE(result->clip_envelope_deployments.size() == 1);
    CHECK(result->clip_envelope_deployments[0].action == AbletonClipEnvelopeAction::Cleared);
    CHECK(result->clip_envelope_deployments[0].observed_has_envelopes == false);
    CHECK(result->clip_envelope_deployments[0].verified);
}

TEST_CASE("live score property writes reject empty or malformed readback",
          "[ableton][compiler][target-profile]") {
    auto score = make_test_score(4);
    ScoreReadbackTransport transport;
    transport.omit_property_evidence = true;

    auto result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.omit_property_evidence = false;
    transport.malformed_tempo = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.malformed_tempo = false;
    transport.extra_property_evidence = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.extra_property_evidence = false;
    transport.fractional_integer_observation = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.fractional_integer_observation = false;
    transport.fractional_integer_echo = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.fractional_integer_echo = false;
    transport.malformed_groove = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

TEST_CASE("live score readback divergence is explicit and unverified",
          "[ableton][compiler][target-profile]") {
    auto score = make_test_score(4);
    ScoreReadbackTransport transport;
    transport.diverge_tempo = true;

    auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->property_writes == 23);
    CHECK(result->property_writes_verified == 22);
    REQUIRE(result->warnings.size() == 2);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("tempo") != std::string::npos;
    }));
}

TEST_CASE("track creation sends create_midi_track", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    // Should have at least create_midi_track and create_clip
    bool found_create_track = false;
    bool found_create_clip = false;
    for (const auto* entry : calls) {
        if (entry->request.property_or_method == "create_midi_track") found_create_track = true;
        if (entry->request.property_or_method == "create_clip") found_create_clip = true;
    }
    CHECK(found_create_track);
    CHECK(found_create_clip);
}

TEST_CASE("track name is set from Part definition", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_name = false;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "name" &&
            entry->request.path.to_string().find("tracks") != std::string::npos) {
            REQUIRE(!entry->request.args.empty());
            auto* name = std::get_if<std::string>(&entry->request.args[0]);
            REQUIRE(name != nullptr);
            CHECK(*name == "Piano");
            found_name = true;
        }
    }
    CHECK(found_name);
}

// =============================================================================
// Note injection
// =============================================================================

TEST_CASE("notes are injected into clips", "[ableton][compiler]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->notes_requested == 1);
    CHECK(result->notes_written >= 1);

    // Verify notes were sent via send_notes
    bool found_notes = false;
    for (const auto& entry : buf.entries()) {
        if (entry.request.property_or_method == "add_new_notes" && !entry.notes.empty()) {
            found_notes = true;
            CHECK(entry.notes[0].pitch == 60); // C4 = MIDI 60
        }
    }
    CHECK(found_notes);
}

TEST_CASE("grace timing reaches Live as finite non-negative clip-note coordinates",
          "[ableton][compiler][grace]") {
    auto score = make_test_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    Note grace;
    grace.pitch = SpelledPitch{0, 0, 4};
    grace.grace = GraceType::Acciaccatura;
    events.push_back(Event{EventId{9101}, Beat::zero(), NoteGroup{{grace}, Beat{1, 16}}});
    events.push_back(Event{EventId{9102}, Beat{1, 16}, RestEvent{Beat{15, 16}, true}});

    CommandBuffer buffer;
    const auto result = compile_to_ableton(score, buffer, 480);
    REQUIRE(result);
    CHECK(result->notes_written == 1);

    const auto note_call =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "add_new_notes";
        });
    REQUIRE(note_call != buffer.entries().end());
    REQUIRE(note_call->notes.size() == 1);
    CHECK(note_call->notes[0].start_time == Catch::Approx(0.125));
    CHECK(note_call->notes[0].duration == Catch::Approx(0.125));
}

TEST_CASE("clip length covers articulation-extended sounding notes",
          "[ableton][compiler][duration]") {
    auto score = make_test_score(1);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 1});
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);
    group.notes[0].articulation = ArticulationType::Fermata;

    CommandBuffer buffer;
    const auto result = compile_to_ableton(score, buffer, 480);
    REQUIRE(result);
    REQUIRE(result->notes_written == 1);

    const auto create_clip =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "create_clip";
        });
    REQUIRE(create_clip != buffer.entries().end());
    REQUIRE(create_clip->request.args.size() == 1);
    CHECK(std::get<double>(create_clip->request.args[0]) == Catch::Approx(7.0));

    const auto note_call =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "add_new_notes";
        });
    REQUIRE(note_call != buffer.entries().end());
    REQUIRE(note_call->notes.size() == 1);
    CHECK(note_call->notes[0].duration == Catch::Approx(7.0));
}

TEST_CASE("Live 10 target preserves notes without issuing unsupported note calls",
          "[ableton][compiler][target-profile]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;
    buf.set_target_profile(modeled_target_profile({10, 1, 43, "10.1.43"}));

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->tracks_created == 1);
    CHECK(result->clips_created == 1);
    CHECK(result->notes_requested == 1);
    CHECK(result->notes_written == 0);
    CHECK(result->note_batches_requested == 1);
    CHECK(result->note_batches_executed == 0);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK(result->note_deployments[0].action == AbletonNoteAction::Unsupported);
    REQUIRE(result->note_deployments[0].requested_notes.size() == 1);
    CHECK(result->note_deployments[0].requested_notes[0].pitch == 60);
    CHECK(result->note_deployments[0].created_note_ids.empty());
    CHECK(result->note_deployments[0].observed_notes.empty());
    REQUIRE_FALSE(result->warnings.empty());
    CHECK(result->warnings.back().find("Live 11+") != std::string::npos);
    for (const auto& entry : buf.entries()) {
        CHECK(entry.request.property_or_method != "add_new_notes");
        CHECK(entry.request.property_or_method != "groove");
    }
    CHECK(result->property_writes == 18);
}

TEST_CASE("Score compilation declines an absent target profile before mutation",
          "[ableton][compiler][target-profile]") {
    auto score = make_test_score(4);
    CommandBuffer transport;
    transport.set_target_profile(std::nullopt);

    const auto result = compile_to_ableton(score, transport);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());

    auto contradictory = modeled_target_profile({10, 1, 43, "10.1.43"});
    contradictory.clip_note_insertion = CapabilityState::Available;
    transport.set_target_profile(std::move(contradictory));
    const auto contradicted = compile_to_ableton(score, transport);
    REQUIRE_FALSE(contradicted.has_value());
    CHECK(contradicted.error() == ErrorCode::ProtocolError);
    CHECK(transport.entries().empty());
}

// =============================================================================
// Multi-part score
// =============================================================================

TEST_CASE("two-part score creates two tracks", "[ableton][compiler]") {
    auto score = make_two_part_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->tracks_created == 2);
    CHECK(result->clips_created == 2);
}

TEST_CASE("staff topology remains notation layout within one Ableton Part track",
          "[ableton][compiler][staff][routing]") {
    auto score = make_test_score(1);
    auto& part = score.parts[0];
    part.definition.staff_count = 2;
    part.definition.staff_clefs = {Clef::Treble, Clef::Bass};
    REQUIRE(add_voice(score, 1, part.id, 1, 1));

    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    auto& lower = part.measures[0].voices[1];
    lower.events.clear();
    Note lower_note;
    lower_note.pitch = SpelledPitch{0, 0, 3};
    lower_note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup lower_group;
    lower_group.notes.push_back(lower_note);
    lower_group.duration = Beat{1, 4};
    lower.events.push_back(Event{EventId{9200001}, Beat::zero(), lower_group});
    lower.events.push_back(Event{EventId{9200002}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    CommandBuffer buffer;
    auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->tracks_created == 1);
    CHECK(result->clips_created == 1);
    CHECK(result->notes_written == 2);

    const auto note_calls = buffer.find_by_type(LomRequestType::CallMethod);
    const auto insertion =
        std::find_if(note_calls.begin(), note_calls.end(), [](const auto* entry) {
            return entry->request.property_or_method == "add_new_notes";
        });
    REQUIRE(insertion != note_calls.end());
    REQUIRE((*insertion)->notes.size() == 2);
    CHECK((*insertion)->notes[0].pitch == 60);
    CHECK((*insertion)->notes[1].pitch == 48);
}

TEST_CASE("parts sharing a MIDI channel retain separate Ableton notes",
          "[ableton][compiler][routing]") {
    auto score = make_two_part_score(1);
    score.parts[1].definition.rendering.midi_channel = 1;
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    insert_note(score, 1, SpelledPitch{2, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->notes_written == 2);

    std::map<std::string, std::vector<LomNoteData>> notes_by_clip;
    for (const auto& entry : buf.entries()) {
        if (entry.request.property_or_method == "add_new_notes")
            notes_by_clip[entry.request.path.to_string()] = entry.notes;
    }
    REQUIRE(notes_by_clip["song/tracks/0/clip_slots/0/clip"].size() == 1);
    REQUIRE(notes_by_clip["song/tracks/1/clip_slots/0/clip"].size() == 1);
    CHECK(notes_by_clip["song/tracks/0/clip_slots/0/clip"][0].pitch == 60);
    CHECK(notes_by_clip["song/tracks/1/clip_slots/0/clip"][0].pitch == 64);
}

TEST_CASE("Live 12 note deployment does not claim an unobserved tuning context",
          "[ableton][compiler][tuning]") {
    auto score = make_test_score(1);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->notes_requested == 1);
    CHECK(result->notes_written == 1);
    CHECK(result->note_batches_requested == 1);
    CHECK(result->note_batches_executed == 0);
    CHECK(result->note_ids_returned == 0);
    CHECK(result->note_batches_verified == 0);
    CHECK(result->notes_verified == 0);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK(result->note_deployments[0].action == AbletonNoteAction::RecordedOnly);
    CHECK_FALSE(result->note_deployments[0].cardinality_verified);
    CHECK_FALSE(result->note_deployments[0].properties_verified);
    REQUIRE(result->note_deployments[0].requested_notes.size() == 1);
    CHECK(result->note_deployments[0].requested_notes[0].pitch == 60);
    CHECK(result->note_deployments[0].observed_notes.empty());
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("audible pitch is unverified") != std::string::npos &&
               warning.find("active tuning") != std::string::npos;
    }));

    buffer.set_target_profile(modeled_target_profile({11, 3, 42, "11.3.42"}));
    const auto live_11 = compile_to_ableton(score, buffer);
    REQUIRE(live_11.has_value());
    CHECK(std::none_of(live_11->warnings.begin(), live_11->warnings.end(), [](const auto& warning) {
        return warning.find("audible pitch is unverified") != std::string::npos;
    }));
}

TEST_CASE("Live 11.0 note readback retains bounded evidence without a complete population verdict",
          "[ableton][compiler][note-evidence]") {
    auto score = make_test_score(1);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    ScoreReadbackTransport transport;
    transport.version = {11, 0, 0, "11.0.0"};
    const auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->notes_verified == 1);
    CHECK(result->note_batches_verified == 0);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK(result->note_deployments[0].properties_verified);
    CHECK_FALSE(result->note_deployments[0].entire_clip_population_observed);
    REQUIRE(result->note_deployments[0].observed_time_span.has_value());
    CHECK(*result->note_deployments[0].observed_time_span == 4.0);
    REQUIRE(
        std::none_of(transport.requests.begin(), transport.requests.end(), [](const auto& request) {
            return request.property_or_method == "get_all_notes_extended";
        }));
    const auto query =
        std::find_if(transport.requests.begin(), transport.requests.end(), [](const auto& request) {
            return request.property_or_method == "get_notes_extended";
        });
    REQUIRE(query != transport.requests.end());
    const auto& payload = std::get<nlohmann::json>(query->args.at(0));
    CHECK(payload.at("from_pitch") == 0);
    CHECK(payload.at("pitch_span") == 128);
    CHECK(payload.at("from_time") == 0.0);
    CHECK(payload.at("time_span") == 4.0);
    CHECK(LomProtocol::validate_request(*query));
    const auto newer = ableton_note_population_request(
        LomPaths::clip(0, 0), modeled_target_profile({11, 1, 0, "11.1.0"}), 4.0);
    CHECK(newer.property_or_method == "get_all_notes_extended");
    CHECK(std::get<nlohmann::json>(newer.args.at(0)).size() == 1);
}

TEST_CASE("live note insertion requires exact unique creation IDs",
          "[ableton][compiler][note-evidence]") {
    auto score = make_test_score(1);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    auto& chord = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes;
    chord.front().release_velocity = 23;
    auto second = chord.front();
    second.pitch = SpelledPitch{2, 0, 4};
    second.release_velocity = 91;
    chord.push_back(second);
    ScoreReadbackTransport transport;

    auto result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->note_batches_requested == 1);
    CHECK(result->note_batches_executed == 1);
    CHECK(result->note_ids_returned == 2);
    CHECK(result->note_batches_verified == 1);
    CHECK(result->notes_verified == 2);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK(result->note_deployments[0].action == AbletonNoteAction::Inserted);
    CHECK(result->note_deployments[0].created_note_ids == std::vector<int>{100, 101});
    CHECK(result->note_deployments[0].cardinality_verified);
    CHECK(result->note_deployments[0].properties_verified);
    CHECK(result->note_deployments[0].entire_clip_population_observed);
    CHECK_FALSE(result->note_deployments[0].observed_time_span.has_value());
    REQUIRE(result->note_deployments[0].requested_notes.size() == 2);
    CHECK(result->note_deployments[0].requested_notes[0].pitch == 60);
    CHECK(result->note_deployments[0].requested_notes[0].start_time == 0.0);
    CHECK(result->note_deployments[0].requested_notes[0].duration == 1.0);
    CHECK(result->note_deployments[0].requested_notes[0].velocity == 80.0);
    CHECK_FALSE(result->note_deployments[0].requested_notes[0].muted);
    CHECK(result->note_deployments[0].requested_notes[0].probability == 1.0);
    CHECK(result->note_deployments[0].requested_notes[0].velocity_deviation == 0.0);
    CHECK(result->note_deployments[0].requested_notes[0].release_velocity == 23.0);
    REQUIRE(result->note_deployments[0].observed_notes.size() == 2);
    CHECK(result->note_deployments[0].observed_notes[0].note_id == 101);
    CHECK(result->note_deployments[0].observed_notes[0].probability == 1.0);
    CHECK(result->note_deployments[0].observed_notes[0].velocity_deviation == 0.0);
    CHECK(result->note_deployments[0].observed_notes[0].release_velocity == 91.0);
    CHECK(result->note_deployments[0].observed_notes[1].note_id == 100);

    transport.malformed_note_evidence = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.malformed_note_evidence = false;
    transport.malformed_note_readback = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.malformed_note_readback = false;
    transport.wrong_note_readback_ids = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.wrong_note_readback_ids = false;
    transport.extra_note_readback = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.extra_note_readback = false;
    transport.integral_probability_readback = true;
    result = compile_to_ableton(score, transport);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);

    transport.integral_probability_readback = false;
    transport.diverge_probability_readback = true;
    result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->note_batches_verified == 0);
    CHECK(result->notes_verified == 1);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK_FALSE(result->note_deployments[0].properties_verified);

    transport.diverge_probability_readback = false;
    transport.diverge_note_readback = true;
    result = compile_to_ableton(score, transport);
    REQUIRE(result.has_value());
    CHECK(result->note_batches_verified == 0);
    CHECK(result->notes_verified == 1);
    REQUIRE(result->note_deployments.size() == 1);
    CHECK_FALSE(result->note_deployments[0].properties_verified);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("note readback") != std::string::npos;
    }));
}

TEST_CASE("pan is set from RenderingConfig", "[ableton][compiler]") {
    auto score = make_two_part_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    int pan_count = 0;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "value" &&
            entry->request.path.to_string().ends_with("mixer_device/panning")) {
            pan_count++;
        }
    }
    CHECK(pan_count == 2);
}

// =============================================================================
// Tempo
// =============================================================================

TEST_CASE("initial tempo is set", "[ableton][compiler]") {
    auto score = make_test_score(4);
    score.tempo_map.front().bpm = make_bpm(60);
    score.tempo_map.front().beat_unit = BeatUnit::Half;
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    auto sets = buf.find_by_type(LomRequestType::SetProperty);
    bool found_tempo = false;
    for (const auto* entry : sets) {
        if (entry->request.property_or_method == "tempo") {
            REQUIRE(!entry->request.args.empty());
            auto* val = std::get_if<double>(&entry->request.args[0]);
            REQUIRE(val != nullptr);
            CHECK(*val == Catch::Approx(120.0));
            found_tempo = true;
            break;
        }
    }
    CHECK(found_tempo);
    CHECK(r->tempo_events_requested == 1);
    CHECK(r->tempo_events_written == 1);
}

TEST_CASE("tempo automation remains explicit when Live exposes only the initial scalar",
          "[ableton][compiler][tempo-transition]") {
    auto score = make_test_score(4);
    TempoEvent target = score.tempo_map.front();
    target.position = ScoreTime{3, Beat::zero()};
    target.bpm = make_bpm(60);
    target.transition_type = TempoTransitionType::Linear;
    target.linear_duration = Beat{2, 1};
    score.tempo_map.push_back(target);
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->tempo_events_requested == 2);
    CHECK(result->tempo_events_written == 1);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("tempo-automation authoring") != std::string::npos;
    }));
}

TEST_CASE("meter automation retains requested versus written cardinality",
          "[ableton][compiler][meter]") {
    auto score = make_test_score(4);
    const auto three_four = make_time_signature(3, 4);
    REQUIRE(three_four.has_value());
    score.time_map.push_back(TimeSignatureEntry{3, *three_four});
    for (std::size_t measure_index = 2; measure_index < score.parts[0].measures.size();
         ++measure_index) {
        auto& event = score.parts[0].measures[measure_index].voices[0].events[0];
        std::get<RestEvent>(event.payload).duration = Beat{3, 4};
    }
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);

    REQUIRE(result.has_value());
    CHECK(result->time_signature_events_requested == 2);
    CHECK(result->time_signature_events_written == 1);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("time-signature events") != std::string::npos;
    }));
}

TEST_CASE("explicit grouped metre reports flat Live projection loss",
          "[ableton][compiler][meter][grouping]") {
    auto score = make_test_score(1, 5, 8);
    const auto additive = make_additive_time_signature({3, 2}, 8);
    REQUIRE(additive.has_value());
    score.time_map.front().time_signature = *additive;
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);

    REQUIRE(result.has_value());
    CHECK(result->time_signature_events_requested == 1);
    CHECK(result->time_signature_events_written == 1);
    CHECK(result->time_signature_groupings_requested == 1);
    CHECK(result->time_signature_groupings_written == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("time-signature grouping") != std::string::npos;
    }));
    const auto numerator_write =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "signature_numerator";
        });
    REQUIRE(numerator_write != buffer.entries().end());
    CHECK(std::get<int>(numerator_write->request.args.front()) == 5);
    const auto denominator_write =
        std::find_if(buffer.entries().begin(), buffer.entries().end(), [](const auto& entry) {
            return entry.request.property_or_method == "signature_denominator";
        });
    REQUIRE(denominator_write != buffer.entries().end());
    CHECK(std::get<int>(denominator_write->request.args.front()) == 8);
}

TEST_CASE("equal-duration local meter remains explicit at the Ableton boundary",
          "[ableton][compiler][meter]") {
    auto score = make_test_score(1);
    const auto two_two = make_time_signature(2, 2);
    REQUIRE(two_two.has_value());
    score.parts[0].measures[0].local_time = *two_two;
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);

    REQUIRE(result.has_value());
    CHECK(result->time_signature_events_requested == 2);
    CHECK(result->time_signature_events_written == 1);
    CHECK(result->midi_report.dropped_time_sig_events == 1);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("measure-local time signature") != std::string::npos;
    }));
}

TEST_CASE("unequal-duration local meter is rejected before Ableton target access",
          "[ableton][compiler][meter][target-value]") {
    auto score = make_test_score(1);
    const auto three_four = make_time_signature(3, 4);
    REQUIRE(three_four.has_value());
    score.parts[0].measures[0].local_time = *three_four;
    std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
        Beat{3, 4};
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetValueUnrepresentable);
    CHECK(buffer.entries().empty());
}

TEST_CASE("Live tempo range is rejected before target access", "[ableton][compiler][tempo]") {
    auto score = make_test_score(1);
    score.tempo_map.front().bpm = make_bpm(1000);
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::TargetValueUnrepresentable);
    CHECK(buffer.entries().empty());
}

// =============================================================================
// Section markers
// =============================================================================

TEST_CASE("section markers are created through the safe cue adapter", "[ableton][compiler]") {
    auto score = make_test_score(8);

    // Add two sections
    ScoreSection intro;
    intro.id = SectionId{1};
    intro.label = "Intro";
    intro.start = ScoreTime{1, Beat::zero()};
    intro.end = ScoreTime{5, Beat::zero()};
    ScoreSection phrase;
    phrase.id = SectionId{3};
    phrase.label = "Phrase";
    phrase.start = ScoreTime{2, Beat::zero()};
    phrase.end = ScoreTime{3, Beat::zero()};
    intro.children.push_back(phrase);
    score.section_map.push_back(intro);

    ScoreSection verse;
    verse.id = SectionId{2};
    verse.label = "Verse";
    verse.start = ScoreTime{5, Beat::zero()};
    verse.end = ScoreTime{9, Beat::zero()};
    score.section_map.push_back(verse);

    CommandBuffer buf;
    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    CHECK(result->section_nodes_total == 3);
    CHECK(result->section_nodes_projected == 2);
    CHECK(result->section_nodes_unprojected == 1);
    CHECK(result->markers_requested == 2);
    CHECK(result->markers_created == 0);
    CHECK(result->markers_updated == 0);
    CHECK(result->markers_verified == 0);
    REQUIRE(result->marker_deployments.size() == 2);
    CHECK(result->marker_deployments[0].action == AbletonCueAction::RecordedOnly);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("nested section node") != std::string::npos;
    }));

    auto calls = buf.find_by_type(LomRequestType::CallMethod);
    int cue_count = 0;
    for (const auto* entry : calls) {
        if (entry->request.property_or_method == "sunny_set_cue") cue_count++;
    }
    CHECK(cue_count == 2);

    ScoreReadbackTransport live;
    auto deployed = compile_to_ableton(score, live);
    REQUIRE(deployed.has_value());
    CHECK(deployed->markers_requested == 2);
    CHECK(deployed->markers_created == 1);
    CHECK(deployed->markers_updated == 1);
    CHECK(deployed->markers_verified == 2);
    REQUIRE(deployed->marker_deployments.size() == 2);
    CHECK(deployed->marker_deployments[0].action == AbletonCueAction::Created);
    CHECK(deployed->marker_deployments[1].action == AbletonCueAction::Updated);
    CHECK(deployed->marker_deployments[0].verified);
    CHECK(deployed->marker_deployments[1].verified);
}

TEST_CASE("section marker positions are preflighted before Ableton mutation",
          "[ableton][compiler][target-value]") {
    auto score = make_test_score(2);
    ScoreSection invalid;
    invalid.id = SectionId{1};
    invalid.label = "Outside bar";
    invalid.start = ScoreTime{1, Beat{1, 1}};
    invalid.end = ScoreTime{2, Beat::zero()};
    score.section_map.push_back(invalid);
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvariantViolation);
    CHECK(buffer.entries().empty());
}

TEST_CASE("live cue deployment requires exact adapter evidence", "[ableton][compiler][protocol]") {
    auto score = make_test_score(1);
    ScoreSection section;
    section.id = SectionId{1};
    section.label = "A";
    section.start = ScoreTime{1, Beat::zero()};
    section.end = ScoreTime{2, Beat::zero()};
    score.section_map.push_back(section);
    ScoreReadbackTransport live;
    live.malformed_cue_evidence = true;

    const auto result = compile_to_ableton(score, live);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ProtocolError);
}

// =============================================================================
// Instrument preset
// =============================================================================

TEST_CASE("unsupported instrument preset is reported without fictional LOM",
          "[ableton][compiler]") {
    auto score = make_test_score(4);
    score.parts[0].definition.rendering.instrument_preset = "Instruments/Piano/Grand Piano.adv";
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    CHECK(std::any_of(r->warnings.begin(), r->warnings.end(), [](const auto& warning) {
        return warning.find("preset") != std::string::npos;
    }));
    for (const auto& entry : buf.entries())
        CHECK(entry.request.property_or_method != "instrument_preset");
}

TEST_CASE("requested articulation controls and part directives are reported with evidence",
          "[ableton][compiler]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);
    group.notes[0].articulation = ArticulationType::Staccato;
    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::Keyswitch;
    mapping.keyswitch_pitch = SpelledPitch{0, 0, 1};
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = mapping;
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat::zero()}, ScoreTime{2, Beat::zero()}, DirectiveType::Mute, 0});
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());
    REQUIRE(result->warnings.size() == 4);
    CHECK(result->articulation_control_events_requested == 1);
    CHECK(result->articulation_control_events_written == 0);
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("articulation control event") != std::string::npos;
    }));
    CHECK(std::any_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("Part directives") != std::string::npos;
    }));
}

TEST_CASE("note-transform-only articulation mappings are deployed without false warnings",
          "[ableton][compiler][articulation]") {
    auto score = make_test_score(1);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);
    group.notes[0].articulation = ArticulationType::Staccato;
    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::NoteDurationScale;
    mapping.duration_scale = 0.75F;
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = mapping;
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result);
    CHECK(result->notes_written == 1);
    CHECK(result->articulation_control_events_requested == 0);
    CHECK(result->articulation_control_events_written == 0);
    CHECK(std::none_of(result->warnings.begin(), result->warnings.end(), [](const auto& warning) {
        return warning.find("articulation") != std::string::npos;
    }));
}

TEST_CASE("standard pedal CC requests remain explicit at the Ableton capability boundary",
          "[ableton][compiler][directive][direction][pedal]") {
    auto score = make_test_score(1);
    ScoreDirection down;
    down.type = DirectionType::PedalDown;
    ScoreDirection up;
    up.type = DirectionType::PedalUp;
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.push_back(Event{EventId{9300001}, Beat{1, 4}, down});
    events.push_back(Event{EventId{9300002}, Beat{3, 4}, up});
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::SustainingPedal, 0});
    CommandBuffer buffer;

    auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->articulation_control_events_requested == 2);
    CHECK(result->articulation_control_events_written == 0);
    CHECK(std::any_of(
        result->warnings.begin(), result->warnings.end(), [](const std::string& warning) {
            return warning.find("2 articulation control event") != std::string::npos;
        }));
    CHECK(std::any_of(
        result->warnings.begin(), result->warnings.end(), [](const std::string& warning) {
            return warning.find("Part directives") != std::string::npos;
        }));
}

TEST_CASE("invalid Ableton PPQ is declined", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf, 0);

    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::InvalidMidiPPQ);
    CHECK(buf.entries().empty());
}

// =============================================================================
// Compilation report
// =============================================================================

TEST_CASE("compilation result contains MIDI report", "[ableton][compiler]") {
    auto score = make_test_score(4);
    CommandBuffer buf;

    auto result = compile_to_ableton(score, buf);
    REQUIRE(result.has_value());

    // MIDI report should be present (even if no drops for a simple score)
    CHECK_FALSE(result->midi_report.has_drops());
    CHECK_FALSE(result->midi_report.has_residuals());
}

TEST_CASE("Ableton result retains non-native key-mode evidence from MIDI compilation",
          "[ableton][compiler][key]") {
    auto score = make_test_score(1);
    const auto dorian = find_scale("dorian");
    REQUIRE(dorian.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{1, 0, 4}, *dorian, 0};
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    REQUIRE(result->midi_report.diagnostics.size() == 1);
    CHECK(result->midi_report.diagnostics[0].message.find("major/minor mode") != std::string::npos);
    CHECK(result->midi_report.diagnostics[0].message.find("dorian") != std::string::npos);
    CHECK_FALSE(result->midi_report.has_drops());
    CHECK(result->midi_report.has_residuals());
}

TEST_CASE("Ableton result retains SMF key-signature domain loss from MIDI compilation",
          "[ableton][compiler][key]") {
    auto score = make_test_score(1);
    const auto major = find_scale("major");
    REQUIRE(major.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{0, 2, 4}, *major, 14};
    CommandBuffer buffer;

    const auto result = compile_to_ableton(score, buffer);
    REQUIRE(result.has_value());
    CHECK(result->midi_report.dropped_key_sig_events == 1);
    CHECK(result->midi_report.has_drops());
    REQUIRE(result->midi_report.diagnostics.size() == 1);
    CHECK(result->midi_report.diagnostics[0].message.find("14 fifths") != std::string::npos);
}

// =============================================================================
// Command count validation
// =============================================================================

TEST_CASE("single-part command sequence is well-ordered", "[ableton][compiler]") {
    auto score = make_test_score(4);
    insert_note(score, 0, SpelledPitch{0, 0, 4}, Beat::zero(), Beat{1, 4});
    CommandBuffer buf;

    auto r = compile_to_ableton(score, buf);
    REQUIRE(r.has_value());

    // Should have: set tempo, create track, set name, create clip, set clip name,
    // add notes — all in sequence
    CHECK(buf.size() >= 5);
}

// =============================================================================
// Transport failure propagation
// =============================================================================

TEST_CASE("transport failure propagates SendFailed", "[ableton][compiler]") {
    auto score = make_test_score(4);
    FailingTransport failing;

    auto result = compile_to_ableton(score, failing);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::SendFailed);
}
