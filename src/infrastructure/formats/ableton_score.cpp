/**
 * @file ableton_score.cpp
 * @brief Ableton Live Compiler — Score IR to LOM commands
 *
 *
 * Compiles Score IR through two phases:
 *   Phase 1: compile_to_midi() handles velocity resolution,
 *            articulation, temporal conversion — all musical logic.
 *   Phase 2: this file translates CompiledMidi into LOM commands
 *            that create the Ableton session structure.
 *
 * The separation means phase 2 contains no musical decision-making;
 * it is a structural mapping from MIDI data to DAW operations.
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sunny/core/score/time.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <tuple>

namespace sunny::infrastructure::formats {

using namespace sunny::core;

namespace {

bool is_live_time_signature(const TimeSignature& signature) {
    if (signature.denominator() != 1 && signature.denominator() != 2 &&
        signature.denominator() != 4 && signature.denominator() != 8 &&
        signature.denominator() != 16)
        return false;

    std::int64_t numerator = 0;
    for (const int group : signature.groups()) {
        if (group <= 0 || numerator > 99 - static_cast<std::int64_t>(group)) return false;
        numerator += group;
    }
    return numerator >= 1 && numerator <= 99;
}

/// Compute the total score length in ticks for clip duration
std::int64_t score_length_ticks(const Score& score, int ppq) {
    // Sum measure durations
    Beat total = Beat::zero();
    for (const auto& ts_entry : score.time_map) {
        // Each time signature entry governs from its bar until the next entry
        Beat measure_dur = ts_entry.time_signature.measure_duration();
        std::uint32_t next_bar = score.metadata.total_bars + 1;
        for (const auto& other : score.time_map) {
            if (other.bar > ts_entry.bar && other.bar < next_bar) {
                next_bar = other.bar;
            }
        }
        std::uint32_t bar_count = next_bar - ts_entry.bar;
        // multiply measure_dur by bar_count
        total = total + Beat{measure_dur.numerator() * static_cast<int64_t>(bar_count),
                             measure_dur.denominator()};
    }
    return absolute_beat_to_tick(total, ppq);
}

/// Convert tick to Ableton beat position (Ableton uses quarter notes)
double tick_to_ableton_beats(std::int64_t tick, int ppq) {
    return static_cast<double>(tick) / static_cast<double>(ppq);
}

/// Convert MidiNoteData to LomNoteData; refuses a note byte outside
/// [0, 127] rather than smuggling it past the MidiNote invariant.
Result<LomNoteData> to_lom_note(const MidiNoteData& n, int ppq) {
    auto pitch = sunny::core::MidiNote::from_int(n.note);
    if (!pitch) {
        return std::unexpected(ErrorCode::InvalidMidiNote);
    }
    LomNoteData data;
    data.pitch = *pitch;
    data.start_time = tick_to_ableton_beats(n.tick, ppq);
    data.duration = tick_to_ableton_beats(n.duration_ticks, ppq);
    data.velocity = n.velocity;
    data.muted = false;
    data.release_velocity = static_cast<double>(n.release_velocity);
    return data;
}

std::optional<int> json_int(const nlohmann::json& value) {
    if (value.is_number_unsigned()) {
        const auto candidate = value.get<std::uint64_t>();
        if (candidate <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            return static_cast<int>(candidate);
        return std::nullopt;
    }
    if (!value.is_number_integer()) return std::nullopt;
    const auto candidate = value.get<std::int64_t>();
    if (candidate < std::numeric_limits<int>::min() || candidate > std::numeric_limits<int>::max())
        return std::nullopt;
    return static_cast<int>(candidate);
}

Result<void> record_property_write(const LomPath& path,
                                   const std::string& property,
                                   const LomValue& value,
                                   LomTransport& transport,
                                   AbletonCompilationResult& result) {
    auto deployment = set_property_with_readback(path, property, value, transport);
    if (!deployment) return std::unexpected(deployment.error());
    result.property_writes++;
    if (deployment->verified) {
        result.property_writes_verified++;
    } else if (deployment->observed) {
        result.warnings.push_back("Live readback for '" + path.to_string() + "/" + property +
                                  "' differed from the requested value");
    }
    result.property_deployments.push_back(std::move(*deployment));
    return {};
}

Result<AbletonCueDeployment> cue_deployment_from_response(double requested_time,
                                                          const std::string& requested_name,
                                                          const LomResponse& response,
                                                          bool recorded_only) {
    AbletonCueDeployment deployment;
    deployment.requested_time = requested_time;
    deployment.requested_name = requested_name;
    if (recorded_only) return deployment;

    if (!response.value) return std::unexpected(ErrorCode::ProtocolError);
    const auto* evidence = std::get_if<nlohmann::json>(&*response.value);
    if (evidence == nullptr || !evidence->is_object() || evidence->size() != 5 ||
        !evidence->contains("action") || !evidence->at("action").is_string() ||
        !evidence->contains("requested_time") || !evidence->at("requested_time").is_number() ||
        !std::isfinite(evidence->at("requested_time").get<double>()) ||
        !evidence->contains("observed_time") || !evidence->at("observed_time").is_number() ||
        !std::isfinite(evidence->at("observed_time").get<double>()) ||
        evidence->at("observed_time").get<double>() < 0.0 ||
        !evidence->contains("requested_name") || !evidence->at("requested_name").is_string() ||
        !evidence->contains("observed_name") || !evidence->at("observed_name").is_string() ||
        evidence->at("requested_time").get<double>() != requested_time ||
        evidence->at("requested_name").get<std::string>() != requested_name)
        return std::unexpected(ErrorCode::ProtocolError);

    const auto action = evidence->at("action").get<std::string>();
    if (action == "created")
        deployment.action = AbletonCueAction::Created;
    else if (action == "updated")
        deployment.action = AbletonCueAction::Updated;
    else
        return std::unexpected(ErrorCode::ProtocolError);

    deployment.observed_time = evidence->at("observed_time").get<double>();
    deployment.observed_name = evidence->at("observed_name").get<std::string>();
    deployment.verified = std::abs(*deployment.observed_time - requested_time) < 1.0e-7 &&
                          *deployment.observed_name == requested_name;
    return deployment;
}

Result<AbletonClipEnvelopeDeployment> clip_envelope_deployment_from_response(
    PartId part_id, int track_index, const LomResponse& response, bool recorded_only) {
    AbletonClipEnvelopeDeployment deployment;
    deployment.part_id = part_id;
    deployment.track_index = track_index;
    if (recorded_only) return deployment;

    if (!response.value) return std::unexpected(ErrorCode::ProtocolError);
    const auto* evidence = std::get_if<nlohmann::json>(&*response.value);
    if (evidence == nullptr || !evidence->is_object() || evidence->size() != 1 ||
        !evidence->contains("has_envelopes") || !evidence->at("has_envelopes").is_boolean())
        return std::unexpected(ErrorCode::ProtocolError);

    deployment.observed_has_envelopes = evidence->at("has_envelopes").get<bool>();
    deployment.action = AbletonClipEnvelopeAction::Cleared;
    deployment.verified = !*deployment.observed_has_envelopes;
    return deployment;
}

AbletonNoteDeployment
note_deployment_intent(PartId part_id, int track_index, const std::vector<LomNoteData>& requested) {
    AbletonNoteDeployment deployment;
    deployment.part_id = part_id;
    deployment.track_index = track_index;
    deployment.notes_requested = static_cast<std::uint64_t>(requested.size());
    deployment.requested_notes.reserve(requested.size());
    for (const auto& note : requested) {
        deployment.requested_notes.push_back({static_cast<int>(note.pitch),
                                              note.start_time,
                                              note.duration,
                                              static_cast<double>(static_cast<int>(note.velocity)),
                                              note.muted,
                                              note.probability,
                                              note.velocity_deviation,
                                              note.release_velocity});
    }
    return deployment;
}

Result<AbletonNoteDeployment>
note_deployment_from_response(PartId part_id,
                              int track_index,
                              const std::vector<LomNoteData>& requested,
                              const LomResponse& response,
                              bool recorded_only) {
    auto deployment = note_deployment_intent(part_id, track_index, requested);
    if (recorded_only) return deployment;

    if (!response.value) return std::unexpected(ErrorCode::ProtocolError);
    const auto* ids = std::get_if<std::vector<int>>(&*response.value);
    if (ids == nullptr || ids->size() != requested.size())
        return std::unexpected(ErrorCode::ProtocolError);
    const std::set<int> unique_ids(ids->begin(), ids->end());
    if (unique_ids.size() != ids->size()) return std::unexpected(ErrorCode::ProtocolError);

    deployment.created_note_ids = *ids;
    deployment.action = AbletonNoteAction::Inserted;
    deployment.cardinality_verified = true;
    return deployment;
}

Result<std::vector<AbletonNoteDeployment::ObservedNote>>
parse_note_readback_impl(const LomResponse& response) {
    if (!response.value) return std::unexpected(ErrorCode::ProtocolError);
    const auto* evidence = std::get_if<nlohmann::json>(&*response.value);
    if (evidence == nullptr || !evidence->is_object() || evidence->size() != 1 ||
        !evidence->contains("notes") || !evidence->at("notes").is_array())
        return std::unexpected(ErrorCode::ProtocolError);

    std::set<int> observed_ids;
    std::vector<AbletonNoteDeployment::ObservedNote> result;
    result.reserve(evidence->at("notes").size());
    for (const auto& note : evidence->at("notes")) {
        if (!note.is_object() || note.size() != 9 || !note.contains("note_id") ||
            !note.contains("pitch") || !note.contains("start_time") ||
            !note.at("start_time").is_number() || !note.contains("duration") ||
            !note.at("duration").is_number() || !note.contains("velocity") ||
            !note.at("velocity").is_number() || !note.contains("mute") ||
            !note.at("mute").is_boolean() || !note.contains("probability") ||
            !note.at("probability").is_number_float() || !note.contains("velocity_deviation") ||
            !note.at("velocity_deviation").is_number_float() ||
            !note.contains("release_velocity") || !note.at("release_velocity").is_number_float())
            return std::unexpected(ErrorCode::ProtocolError);

        const auto note_id = json_int(note.at("note_id"));
        const auto pitch = json_int(note.at("pitch"));
        const double start_time = note.at("start_time").get<double>();
        const double duration = note.at("duration").get<double>();
        const double velocity = note.at("velocity").get<double>();
        const double probability = note.at("probability").get<double>();
        const double velocity_deviation = note.at("velocity_deviation").get<double>();
        const double release_velocity = note.at("release_velocity").get<double>();
        if (!note_id || !pitch || *pitch < 0 || *pitch > 127 || !std::isfinite(start_time) ||
            start_time < 0.0 || !std::isfinite(duration) || duration <= 0.0 ||
            !std::isfinite(velocity) || velocity < 0.0 || velocity > 127.0 ||
            !std::isfinite(probability) || probability < 0.0 || probability > 1.0 ||
            !std::isfinite(velocity_deviation) || velocity_deviation < -127.0 ||
            velocity_deviation > 127.0 || !std::isfinite(release_velocity) ||
            release_velocity < 0.0 || release_velocity > 127.0 ||
            !observed_ids.insert(*note_id).second)
            return std::unexpected(ErrorCode::ProtocolError);

        const bool muted = note.at("mute").get<bool>();
        result.push_back({*note_id,
                          *pitch,
                          start_time,
                          duration,
                          velocity,
                          muted,
                          probability,
                          velocity_deviation,
                          release_velocity});
    }
    return result;
}

Result<std::uint64_t> apply_note_readback(AbletonNoteDeployment& deployment,
                                          const LomResponse& response) {
    auto observed = parse_note_readback_impl(response);
    if (!observed || observed->size() != deployment.requested_notes.size())
        return std::unexpected(ErrorCode::ProtocolError);

    const std::set<int> created_ids(deployment.created_note_ids.begin(),
                                    deployment.created_note_ids.end());
    std::set<int> observed_ids;
    using NoteState = std::tuple<int, double, double, double, bool, double, double, double>;
    std::multiset<NoteState> requested_states;
    for (const auto& note : deployment.requested_notes)
        requested_states.emplace(note.pitch,
                                 note.start_time,
                                 note.duration,
                                 note.velocity,
                                 note.muted,
                                 note.probability,
                                 note.velocity_deviation,
                                 note.release_velocity);

    std::uint64_t matching_notes = 0;
    for (const auto& note : *observed) {
        observed_ids.insert(note.note_id);
        const auto requested = requested_states.find({note.pitch,
                                                      note.start_time,
                                                      note.duration,
                                                      note.velocity,
                                                      note.muted,
                                                      note.probability,
                                                      note.velocity_deviation,
                                                      note.release_velocity});
        if (requested != requested_states.end()) {
            requested_states.erase(requested);
            ++matching_notes;
        }
    }
    if (observed_ids != created_ids) return std::unexpected(ErrorCode::ProtocolError);

    deployment.observed_notes = std::move(*observed);
    deployment.properties_verified = requested_states.empty();
    return matching_notes;
}

auto observed_note_states(const AbletonNoteDeployment& deployment) {
    using State = std::tuple<int, int, double, double, double, bool, double, double, double>;
    std::multiset<State> states;
    for (const auto& note : deployment.observed_notes) {
        states.emplace(note.note_id,
                       note.pitch,
                       note.start_time,
                       note.duration,
                       note.velocity,
                       note.muted,
                       note.probability,
                       note.velocity_deviation,
                       note.release_velocity);
    }
    return states;
}

} // anonymous namespace

Result<std::vector<AbletonNoteDeployment::ObservedNote>>
parse_ableton_note_readback(const LomResponse& response) {
    return parse_note_readback_impl(response);
}

LomRequest ableton_note_population_request(const LomPath& clip_path,
                                           const AbletonTargetProfile& profile,
                                           double clip_end) {
    nlohmann::json query = {{"return",
                             {"note_id",
                              "pitch",
                              "start_time",
                              "duration",
                              "velocity",
                              "mute",
                              "probability",
                              "velocity_deviation",
                              "release_velocity"}}};
    // Current LOM (Live 12.4.5) dates full-population access to 11.1, and
    // ranged access to 11.0. Neither fact alone verifies the Python runtime.
    if (profile.live_version.at_least(11, 1))
        return LomProtocol::call_method(clip_path, "get_all_notes_extended", {query});
    query["from_pitch"] = 0;
    query["pitch_span"] = 128;
    query["from_time"] = 0.0;
    query["time_span"] = clip_end;
    return LomProtocol::call_method(clip_path, "get_notes_extended", {query});
}

Result<AbletonCompilationResult>
compile_to_ableton(const Score& score, LomTransport& transport, int ppq) {
    AbletonCompilationResult result;
    result.requested_tuning = score.tuning;

    if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max()) {
        return std::unexpected(ErrorCode::InvalidMidiPPQ);
    }
    if (!score.parts.empty() &&
        score.parts.size() - 1 > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
    }
    if (!score.time_map.empty() && !is_live_time_signature(score.time_map.front().time_signature))
        return std::unexpected(ErrorCode::TargetValueUnrepresentable);
    if (!score.tempo_map.empty()) {
        const double initial_quarter_bpm = effective_quarter_bpm(score.tempo_map.front());
        if (!std::isfinite(initial_quarter_bpm) || initial_quarter_bpm < 20.0 ||
            initial_quarter_bpm > 999.0)
            return std::unexpected(ErrorCode::TargetValueUnrepresentable);
    }

    // Phase 1: MIDI compilation (all musical logic)
    auto midi_result = compile_to_midi(score, ppq);
    if (!midi_result) return std::unexpected(midi_result.error());
    auto& compiled = midi_result->midi;
    result.midi_report = midi_result->report;
    result.notes_requested = static_cast<std::uint64_t>(compiled.notes.size());
    result.time_signature_events_requested = static_cast<std::uint64_t>(score.time_map.size());
    for (const auto& entry : score.time_map) {
        if (!has_distinct_meter_grouping(entry.time_signature)) continue;
        if (result.time_signature_groupings_requested == std::numeric_limits<std::uint64_t>::max())
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        result.time_signature_groupings_requested++;
    }
    result.key_signature_events_requested = static_cast<std::uint64_t>(score.key_map.size());
    result.tuning_definitions_requested = 1;
    if (!is_standard_midi_tuning(score.tuning))
        result.warnings.push_back(
            "1 non-standard score tuning definition was preserved but not written: Live exposes "
            "the active TuningSystem, but its documented note_tunings dictionary does not specify "
            "a closed member schema that Sunny can safely author and verify");
    std::uint64_t local_time_signatures = 0;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (!measure.local_time) continue;
            const TimeSignatureEntry* global = nullptr;
            for (const auto& entry : score.time_map) {
                if (entry.bar > measure.bar_number) break;
                global = &entry;
            }
            if (global == nullptr) return std::unexpected(ErrorCode::InvalidTimeSignature);
            if (*measure.local_time == global->time_signature) continue;
            if (result.time_signature_events_requested == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            result.time_signature_events_requested++;
            local_time_signatures++;
            if (has_distinct_meter_grouping(*measure.local_time)) {
                if (result.time_signature_groupings_requested ==
                    std::numeric_limits<std::uint64_t>::max())
                    return std::unexpected(ErrorCode::ArithmeticOverflow);
                result.time_signature_groupings_requested++;
            }
        }
    }
    if (local_time_signatures != 0)
        result.warnings.push_back(
            std::to_string(local_time_signatures) +
            " measure-local time signature(s) were preserved but not written: Live exposes one "
            "global Song meter state");
    if (result.time_signature_groupings_requested != 0)
        result.warnings.push_back(
            std::to_string(result.time_signature_groupings_requested) +
            " explicit time-signature grouping(s) were preserved but not written: Live exposes "
            "only flat signature numerator/denominator scalars");
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            if (!measure.local_key) continue;
            const ScoreTime measure_start{measure.bar_number, Beat::zero()};
            const KeySignatureEntry* global = nullptr;
            for (const auto& entry : score.key_map) {
                if (entry.position > measure_start) break;
                global = &entry;
            }
            if (global != nullptr && *measure.local_key == global->key) continue;
            if (result.key_signature_events_requested == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::ArithmeticOverflow);
            result.key_signature_events_requested++;
        }
    }
    if (result.key_signature_events_requested != 0)
        result.warnings.push_back(
            std::to_string(result.key_signature_events_requested) +
            " global/local key signature intent(s) were preserved but not written: Sunny does not "
            "yet preflight Live's current tuning and scale-name registry");

    // Resolve every section position before target access. A malformed or
    // unrepresentable SectionMap must not produce a partially mutated Live Set
    // or disappear as a silently skipped cue request.
    std::vector<std::pair<double, std::string>> cues;
    cues.reserve(score.section_map.size());
    std::vector<const ScoreSection*> pending_sections;
    for (const auto& section : score.section_map) {
        auto tick = score_time_to_tick(section.start, score.time_map, ppq);
        if (!tick) return std::unexpected(tick.error());
        cues.emplace_back(tick_to_ableton_beats(*tick, ppq), section.label);
        pending_sections.push_back(&section);
    }
    while (!pending_sections.empty()) {
        const auto* section = pending_sections.back();
        pending_sections.pop_back();
        result.section_nodes_total++;
        for (const auto& child : section->children)
            pending_sections.push_back(&child);
    }
    result.section_nodes_projected = static_cast<std::uint64_t>(cues.size());
    result.section_nodes_unprojected = result.section_nodes_total - result.section_nodes_projected;
    if (result.section_nodes_unprojected != 0)
        result.warnings.push_back(
            std::to_string(result.section_nodes_unprojected) +
            " nested section node(s) were preserved but not projected: Live CuePoints are a flat "
            "time-keyed structure and cannot retain SectionMap hierarchy or coincident labels");

    auto target_profile = transport.target_profile();
    if (!target_profile) return std::unexpected(target_profile.error());
    if (!*target_profile) return std::unexpected(ErrorCode::ProtocolError);
    auto valid_target_profile = validate_target_profile(**target_profile);
    if (!valid_target_profile) return std::unexpected(valid_target_profile.error());
    result.target_profile = std::move(**target_profile);
    if (result.notes_requested != 0 && result.target_profile.live_version.at_least(12, 0))
        result.warnings.push_back(
            std::to_string(result.notes_requested) +
            " note index/indices will be written, but audible pitch is unverified: Live 12 can "
            "reinterpret existing MIDI note indices through the Set's active tuning; project "
            "snapshot schema 29 retains version-available scale and exact opaque TuningSystem "
            "state but cannot map its dictionary semantics, per-track bypass, or device/MPE "
            "support");

    auto scene_count = transport.scene_count();
    if (!scene_count || !*scene_count) return std::unexpected(ErrorCode::ProtocolError);
    if (**scene_count == 0) {
        auto response =
            transport.send(LomProtocol::call_method(LomPaths::song(), "create_scene", {0}));
        if (!response.success) return std::unexpected(ErrorCode::SendFailed);
        result.scenes_created++;
    }

    const bool can_insert_notes =
        result.target_profile.clip_note_insertion == CapabilityState::Available;

    // Compute score duration for clip lengths
    std::int64_t total_ticks = score_length_ticks(score, ppq);
    for (const auto& note : compiled.notes) {
        if (note.tick < 0 || note.duration_ticks < 1 ||
            note.tick > std::numeric_limits<std::int64_t>::max() - note.duration_ticks)
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        total_ticks = std::max(total_ticks, note.tick + note.duration_ticks);
    }
    double clip_length_beats = tick_to_ableton_beats(total_ticks, ppq);
    if (clip_length_beats <= 0.0) clip_length_beats = 4.0; // minimum 1 bar

    // Phase 2: Group notes by source Part. MIDI channels need not be unique,
    // while the Ableton target creates one track per Part.
    std::map<std::uint64_t, std::vector<const MidiNoteData*>> notes_by_part;
    for (const auto& note : compiled.notes) {
        notes_by_part[note.part_id.value].push_back(&note);
    }
    std::map<std::uint64_t, std::uint64_t> articulation_controls_by_part;
    for (const auto& event : compiled.keyswitches)
        ++articulation_controls_by_part[event.part_id.value];
    for (const auto& event : compiled.control_changes)
        ++articulation_controls_by_part[event.part_id.value];
    for (const auto& event : compiled.program_changes)
        ++articulation_controls_by_part[event.part_id.value];
    result.articulation_control_events_requested =
        static_cast<std::uint64_t>(compiled.keyswitches.size()) +
        static_cast<std::uint64_t>(compiled.control_changes.size()) +
        static_cast<std::uint64_t>(compiled.program_changes.size());

    // Build track index: Part index → source Part and track metadata.
    struct TrackInfo {
        int track_index;
        std::string name;
        PartId part_id;
        const RenderingConfig* rendering;
        const std::vector<PartDirective>* directives;
    };
    std::vector<TrackInfo> tracks;
    for (std::size_t i = 0; i < score.parts.size(); ++i) {
        const auto& part = score.parts[i];
        TrackInfo ti;
        ti.track_index = static_cast<int>(i);
        ti.name = part.definition.name;
        ti.part_id = part.id;
        ti.rendering = &part.definition.rendering;
        ti.directives = &part.part_directives;
        tracks.push_back(ti);
    }

    // Step 1: Set tempo
    if (!score.tempo_map.empty()) {
        const auto& first_tempo = score.tempo_map[0];
        result.tempo_events_requested = static_cast<std::uint64_t>(score.tempo_map.size());
        double bpm = effective_quarter_bpm(first_tempo);
        auto property_result =
            record_property_write(LomPaths::song(), "tempo", bpm, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        result.tempo_events_written++;

        if (score.tempo_map.size() > 1) {
            result.warnings.push_back(
                "Additional tempo events and transitions were preserved in Score IR but not "
                "written: the public Live Object Model exposes the current Song.tempo scalar, "
                "not tempo-automation authoring");
        }
    }

    // Song and each created Clip expose writable signatures, but not
    // time-signature automation authoring. Apply the initial signature to the
    // Song now and to every Clip after creation; create_clip(length) does not
    // document a signature-inheritance guarantee.
    if (!score.time_map.empty()) {
        const auto& signature = score.time_map.front().time_signature;
        auto property_result = record_property_write(
            LomPaths::song(), "signature_numerator", signature.numerator(), transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(LomPaths::song(),
                                                "signature_denominator",
                                                static_cast<int>(signature.denominator()),
                                                transport,
                                                result);
        if (!property_result) return std::unexpected(property_result.error());
        result.time_signature_events_written++;
        if (score.time_map.size() > 1) {
            result.warnings.push_back(
                "Additional time-signature events were not written: the public Live Object Model "
                "does not provide time-signature automation authoring");
        }
    }

    // Every generated clip occupies Scene 0. Give the full-score row the
    // document title, then disable launch overrides after installing Song
    // state so firing it preserves the Score tempo and meter.
    auto scene_property_result =
        record_property_write(LomPaths::scene(0), "name", score.metadata.title, transport, result);
    if (!scene_property_result) return std::unexpected(scene_property_result.error());
    scene_property_result =
        record_property_write(LomPaths::scene(0), "tempo_enabled", false, transport, result);
    if (!scene_property_result) return std::unexpected(scene_property_result.error());
    scene_property_result = record_property_write(
        LomPaths::scene(0), "time_signature_enabled", false, transport, result);
    if (!scene_property_result) return std::unexpected(scene_property_result.error());

    // Step 2: Create tracks and clips. The scene-count preflight above has
    // created scene 0 when necessary, so clip_slots/0 is never a host-default
    // assumption.
    for (const auto& ti : tracks) {
        // Create MIDI track
        auto resp = transport.send(
            LomProtocol::call_method(LomPaths::song(), "create_midi_track", {ti.track_index}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        result.tracks_created++;

        // Set track name
        auto property_result = record_property_write(
            LomPaths::track(ti.track_index), "name", ti.name, transport, result);
        if (!property_result) return std::unexpected(property_result.error());

        // A generated Score Track is a playback target, not a live-input or
        // recording target. Clear both public arm states before installing its
        // mixer and Clip state; monitoring mode remains a separate boundary.
        property_result =
            record_property_write(LomPaths::track(ti.track_index), "arm", false, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(
            LomPaths::track(ti.track_index), "implicit_arm", false, transport, result);
        if (!property_result) return std::unexpected(property_result.error());

        // Remove the Track from the Main crossfader's A/B gain stage. This is
        // an exact public MixerDevice enum, independent of Track mute/solo.
        property_result =
            record_property_write(LomPaths::track(ti.track_index).child("mixer_device"),
                                  "crossfade_assign",
                                  1,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());

        // RenderingConfig.pan addresses the single Stereo Pan parameter. Split
        // Stereo uses independent left/right parameters and is a different model.
        property_result =
            record_property_write(LomPaths::track(ti.track_index).child("mixer_device"),
                                  "panning_mode",
                                  0,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());

        // Arbitrary browser/preset loading is not part of the public LOM. Keep
        // the request visible instead of assigning a fictional Track property.
        if (ti.rendering->instrument_preset) {
            result.warnings.push_back("Instrument preset for track '" + ti.name +
                                      "' was not loaded: arbitrary preset paths are not writable "
                                      "through the public Live Object Model");
        }
        if (ti.rendering->group) {
            result.warnings.push_back("Group assignment for track '" + ti.name +
                                      "' was not applied: the public Live Object Model cannot "
                                      "create or assign Group Tracks");
        }
        if (const auto controls = articulation_controls_by_part.find(ti.part_id.value);
            controls != articulation_controls_by_part.end()) {
            result.warnings.push_back(
                std::to_string(controls->second) + " articulation control event(s) for track '" +
                ti.name +
                "' were not written: the public Live clip-note API cannot guarantee ordered "
                "keyswitch, CC, or program-change deployment");
        }
        if (!ti.directives->empty()) {
            result.warnings.push_back("Part directives for track '" + ti.name +
                                      "' were preserved but not deployed to Ableton");
        }

        // Set pan if specified
        if (ti.rendering->pan) {
            auto pan_parameter =
                LomPaths::track(ti.track_index).child("mixer_device").child("panning");
            property_result = record_property_write(
                pan_parameter, "value", static_cast<double>(*ti.rendering->pan), transport, result);
            if (!property_result) return std::unexpected(property_result.error());
        }

        // Create clip in slot 0 spanning full score
        resp = transport.send(LomProtocol::call_method(
            LomPaths::clip_slot(ti.track_index, 0), "create_clip", {clip_length_beats}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        result.clips_created++;

        // Set clip name
        property_result = record_property_write(
            LomPaths::clip(ti.track_index, 0), "name", ti.name, transport, result);
        if (!property_result) return std::unexpected(property_result.error());

        // The Score supplies a finite active Clip, while create_clip(length) does not
        // establish marker, loop, launch, or groove state. Install the tractable
        // structural/launch scalars explicitly and retain readback evidence. Follow
        // Actions remain outside the public Clip LOM, so these writes do not prove a
        // one-shot playback interval. A Live 11+ groove can move note timing and alter
        // velocity non-destructively, so clear the object association too.
        property_result = record_property_write(
            LomPaths::clip(ti.track_index, 0), "start_marker", 0.0, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(
            LomPaths::clip(ti.track_index, 0), "end_marker", clip_length_beats, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(
            LomPaths::clip(ti.track_index, 0), "looping", false, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(
            LomPaths::clip(ti.track_index, 0), "muted", false, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        if (can_insert_notes) {
            property_result = record_property_write(
                LomPaths::clip(ti.track_index, 0), "launch_mode", 0, transport, result);
            if (!property_result) return std::unexpected(property_result.error());
            property_result = record_property_write(
                LomPaths::clip(ti.track_index, 0), "launch_quantization", 1, transport, result);
            if (!property_result) return std::unexpected(property_result.error());
            property_result = record_property_write(
                LomPaths::clip(ti.track_index, 0), "legato", false, transport, result);
            if (!property_result) return std::unexpected(property_result.error());
            property_result = record_property_write(
                LomPaths::clip(ti.track_index, 0), "velocity_amount", 0.0, transport, result);
            if (!property_result) return std::unexpected(property_result.error());
            property_result = record_property_write(LomPaths::clip(ti.track_index, 0),
                                                    "groove",
                                                    nlohmann::json(nullptr),
                                                    transport,
                                                    result);
            if (!property_result) return std::unexpected(property_result.error());
        }

        if (!score.time_map.empty()) {
            const auto& signature = score.time_map.front().time_signature;
            property_result = record_property_write(LomPaths::clip(ti.track_index, 0),
                                                    "signature_numerator",
                                                    signature.numerator(),
                                                    transport,
                                                    result);
            if (!property_result) return std::unexpected(property_result.error());
            property_result = record_property_write(LomPaths::clip(ti.track_index, 0),
                                                    "signature_denominator",
                                                    static_cast<int>(signature.denominator()),
                                                    transport,
                                                    result);
            if (!property_result) return std::unexpected(property_result.error());
        }

        result.clip_envelope_clears_requested++;
        resp = transport.send(LomProtocol::call_method(LomPaths::clip(ti.track_index, 0),
                                                       "sunny_clear_all_envelopes"));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        auto envelope_deployment = clip_envelope_deployment_from_response(
            ti.part_id, ti.track_index, resp, transport.records_without_execution());
        if (!envelope_deployment) return std::unexpected(envelope_deployment.error());
        if (envelope_deployment->action == AbletonClipEnvelopeAction::Cleared)
            result.clip_envelope_clears_executed++;
        if (envelope_deployment->verified)
            result.clip_envelope_clears_verified++;
        else if (envelope_deployment->observed_has_envelopes)
            result.warnings.push_back("Live Clip envelope clear for track '" + ti.name +
                                      "' left envelope state present");
        result.clip_envelope_deployments.push_back(std::move(*envelope_deployment));

        // Inject notes for this channel
        auto part_it = notes_by_part.find(ti.part_id.value);
        if (part_it != notes_by_part.end()) {
            result.note_batches_requested++;
            std::vector<LomNoteData> lom_notes;
            lom_notes.reserve(part_it->second.size());
            for (const auto* n : part_it->second) {
                auto lom_note = to_lom_note(*n, ppq);
                if (!lom_note) return std::unexpected(lom_note.error());
                lom_notes.push_back(*lom_note);
            }
            if (!can_insert_notes) {
                auto deployment = note_deployment_intent(ti.part_id, ti.track_index, lom_notes);
                deployment.action = AbletonNoteAction::Unsupported;
                result.note_deployments.push_back(std::move(deployment));
                result.warnings.push_back(
                    "Notes for track '" + ti.name +
                    "' were not inserted: Clip.add_new_notes requires Live 11+");
                continue;
            }
            resp = transport.send_notes(LomPaths::clip(ti.track_index, 0), lom_notes);
            if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
            auto deployment = note_deployment_from_response(
                ti.part_id, ti.track_index, lom_notes, resp, transport.records_without_execution());
            if (!deployment) return std::unexpected(deployment.error());
            if (deployment->action == AbletonNoteAction::Inserted) {
                result.note_batches_executed++;
                result.note_ids_returned +=
                    static_cast<std::uint64_t>(deployment->created_note_ids.size());

                const nlohmann::json query = {{"note_ids", deployment->created_note_ids},
                                              {"return",
                                               nlohmann::json::array({"note_id",
                                                                      "pitch",
                                                                      "start_time",
                                                                      "duration",
                                                                      "velocity",
                                                                      "mute",
                                                                      "probability",
                                                                      "velocity_deviation",
                                                                      "release_velocity"})}};
                resp = transport.send(LomProtocol::call_method(
                    LomPaths::clip(ti.track_index, 0), "get_notes_by_id", {query}));
                if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
                const auto id_verified_notes = apply_note_readback(*deployment, resp);
                if (!id_verified_notes) return std::unexpected(id_verified_notes.error());
                const auto id_observed_states = observed_note_states(*deployment);

                resp = transport.send(ableton_note_population_request(
                    LomPaths::clip(ti.track_index, 0), result.target_profile, clip_length_beats));
                if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
                const auto verified_notes = apply_note_readback(*deployment, resp);
                if (!verified_notes) return std::unexpected(verified_notes.error());
                if (observed_note_states(*deployment) != id_observed_states)
                    return std::unexpected(ErrorCode::ProtocolError);
                deployment->entire_clip_population_observed =
                    result.target_profile.live_version.at_least(11, 1);
                if (!deployment->entire_clip_population_observed) {
                    deployment->observed_time_span = clip_length_beats;
                    result.warnings.push_back(
                        "Live 11.0 note readback covers all pitches within [0, " +
                        std::to_string(clip_length_beats) +
                        ") quarter-note beats; notes outside this interval remain unobserved");
                }
                result.notes_verified += *verified_notes;
                if (deployment->properties_verified &&
                    deployment->entire_clip_population_observed) {
                    result.note_batches_verified++;
                } else if (!deployment->properties_verified) {
                    result.warnings.push_back(
                        "Live note readback for track '" + ti.name +
                        "' differed from the requested deterministic nine-field note state");
                }
            }
            result.note_deployments.push_back(std::move(*deployment));
            result.notes_written += static_cast<std::uint64_t>(lom_notes.size());
        }
    }

    // Step 3: Section markers from SectionMap
    for (const auto& [beat_pos, label] : cues) {
        result.markers_requested++;
        auto resp = transport.send(
            LomProtocol::call_method(LomPaths::song(), "sunny_set_cue", {beat_pos, label}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        auto deployment = cue_deployment_from_response(
            beat_pos, label, resp, transport.records_without_execution());
        if (!deployment) return std::unexpected(deployment.error());
        if (deployment->action == AbletonCueAction::Created)
            result.markers_created++;
        else if (deployment->action == AbletonCueAction::Updated)
            result.markers_updated++;
        if (deployment->verified)
            result.markers_verified++;
        else if (deployment->observed_time || deployment->observed_name)
            result.warnings.push_back("Live cue readback differed from section marker '" + label +
                                      "'");
        result.marker_deployments.push_back(std::move(*deployment));
    }

    return result;
}

} // namespace sunny::infrastructure::formats
