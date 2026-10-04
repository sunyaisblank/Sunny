/**
 * @file ableton_project.hpp
 * @brief Guarded plan/apply and ordered compilation for a complete Sunny project
 *
 * The aggregate compiler is the external-alignment boundary between Sunny's
 * Score, Timbre, and Mix models and one Ableton Live Set. It derives every
 * downstream track target from the Score's authoritative Part ordering.
 */

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sunny/core/project/validation.hpp>
#include <sunny/infrastructure/ableton/deployment.hpp>
#include <sunny/infrastructure/formats/ableton_mix.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <sunny/infrastructure/formats/ableton_timbre.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

struct PartTimbreCompilationResult {
    sunny::core::PartId part_id;
    int track_index = 0;
    TimbreCompilationResult compilation;
};

enum class AbletonOutputDestination : std::uint8_t { Master, Group };

struct AbletonRoutingOptionEvidence {
    std::string display_name;
    std::string identifier;
};

/** Selected/available Live input routes without inferred source identity or neutrality. */
struct AbletonInputRoutingEvidence {
    std::optional<std::string> selected_type_display_name;
    std::optional<std::string> selected_type_identifier;
    std::optional<std::string> selected_channel_display_name;
    std::optional<std::string> selected_channel_identifier;
    std::vector<AbletonRoutingOptionEvidence> available_types;
    std::vector<AbletonRoutingOptionEvidence> available_channels;
    bool selected_input_observed = false;
    bool selected_type_available_verified = false;
    bool selected_channel_available_verified = false;
    bool selected_input_available_verified = false;
    /** Live route identifiers have not been mapped to a semantic external source identity. */
    bool input_source_identity_mapped = false;
    /** A Live-valid input selection does not prove that external input is neutral or suppressed. */
    bool external_input_neutrality_verified = false;
};

/** Requested Sunny edge plus selected/available Live routes, without inferred semantic identity. */
struct AbletonOutputRoutingEvidence {
    AbletonOutputDestination requested_destination = AbletonOutputDestination::Master;
    std::optional<sunny::core::GroupBusId> requested_group_id;
    /** Explicit caller admission connecting Sunny destination semantics to opaque Live symbols. */
    std::optional<AbletonOutputRouteBinding> requested_binding;
    std::optional<std::string> selected_type_display_name;
    std::optional<std::string> selected_type_identifier;
    std::optional<std::string> selected_channel_display_name;
    std::optional<std::string> selected_channel_identifier;
    std::vector<AbletonRoutingOptionEvidence> available_types;
    std::vector<AbletonRoutingOptionEvidence> available_channels;
    bool selected_output_observed = false;
    bool selected_type_available_verified = false;
    bool selected_channel_available_verified = false;
    bool selected_output_available_verified = false;
    /** Live route identifiers have not been mapped to Sunny Master/Group identities. */
    bool source_target_identity_mapped = false;
    /** Observation alone does not prove the requested Sunny output edge. */
    bool verified = false;
};

/** One exact ordered ClipSlot observation, including target-owned Stop Button state. */
struct AbletonClipSlotStateEvidence {
    std::uint32_t slot = 0;
    bool has_clip = false;
    bool has_stop_button = false;
    bool is_group_slot = false;
    bool controls_other_clips = false;
    bool is_playing = false;
    bool is_recording = false;
    bool is_triggered = false;
    std::uint8_t playing_status = 0;
    bool will_record_on_start = false;
};

/** Post-apply evidence for the tractable Track-level playback gate of one Part. */
struct AbletonTrackGateEvidence {
    sunny::core::PartId part_id;
    int track_index = 0;
    std::string requested_name;
    bool requested_mute = false;
    bool requested_solo = false;
    bool requested_has_audio_input = false;
    bool requested_has_midi_input = true;
    double requested_input_meter_level = 0.0;
    double requested_output_meter_level = 0.0;
    double requested_input_meter_left = 0.0;
    double requested_input_meter_right = 0.0;
    double requested_output_meter_left = 0.0;
    double requested_output_meter_right = 0.0;
    bool requested_is_frozen = false;
    bool requested_arm = false;
    bool requested_implicit_arm = false;
    bool requested_back_to_arranger = false;
    int requested_fired_slot_index = -1;
    int requested_playing_slot_index = -1;
    bool requested_clip_slot_is_group_slot = false;
    bool requested_clip_slot_controls_other_clips = false;
    bool requested_clip_slot_is_playing = false;
    bool requested_clip_slot_is_recording = false;
    bool requested_clip_slot_is_triggered = false;
    std::uint8_t requested_clip_slot_playing_status = 0;
    bool requested_clip_slot_will_record_on_start = false;
    /** Generated Part Tracks must contain no independently playable Arrangement Clips. */
    std::uint32_t requested_arrangement_clip_count = 0;
    /** Generated Part Tracks must have no Take Lane that could replace the main lane when
     * auditioned. */
    std::uint32_t requested_take_lane_count = 0;
    /** Null is the generated Part Track's required top-level membership. */
    std::optional<std::uint32_t> requested_group_track_index;
    int requested_crossfade_assign = 1;
    int requested_panning_mode = 0;
    double requested_track_activator = 1.0;
    bool expected_mixer_enabled = false;
    std::optional<std::string> observed_name;
    std::optional<bool> observed_has_audio_input;
    std::optional<bool> observed_has_midi_input;
    AbletonInputRoutingEvidence input_routing;
    std::optional<double> observed_input_meter_level;
    std::optional<double> observed_output_meter_level;
    std::optional<double> observed_input_meter_left;
    std::optional<double> observed_input_meter_right;
    std::optional<double> observed_output_meter_left;
    std::optional<double> observed_output_meter_right;
    std::optional<bool> observed_has_audio_output;
    std::optional<bool> observed_has_midi_output;
    AbletonOutputRoutingEvidence output_routing;
    std::optional<bool> observed_is_frozen;
    std::optional<bool> observed_arm;
    std::optional<bool> observed_implicit_arm;
    std::optional<bool> observed_back_to_arranger;
    std::optional<int> observed_fired_slot_index;
    std::optional<int> observed_playing_slot_index;
    std::optional<std::uint32_t> observed_arrangement_clip_count;
    std::optional<std::uint32_t> observed_take_lane_count;
    std::vector<AbletonClipSlotStateEvidence> observed_clip_slots;
    bool group_membership_observed = false;
    std::optional<std::uint32_t> observed_group_track_index;
    std::optional<int> observed_crossfade_assign;
    std::optional<int> observed_panning_mode;
    std::optional<bool> observed_mute;
    std::optional<bool> observed_solo;
    std::optional<bool> observed_muted_via_solo;
    std::optional<double> observed_track_activator;
    std::optional<double> observed_track_activator_minimum;
    std::optional<double> observed_track_activator_maximum;
    std::optional<bool> observed_track_activator_is_quantized;
    std::optional<bool> observed_track_activator_is_enabled;
    std::optional<std::uint8_t> observed_track_activator_state;
    std::optional<std::uint8_t> observed_track_activator_automation_state;
    bool input_classification_verified = false;
    bool meter_levels_observed = false;
    bool input_meter_hold_quiescent_verified = false;
    bool output_meter_hold_quiescent_verified = false;
    bool meter_hold_quiescence_verified = false;
    bool momentary_meter_levels_observed = false;
    bool input_stereo_momentary_quiescent_verified = false;
    bool output_stereo_momentary_quiescent_verified = false;
    bool meter_momentary_quiescence_verified = false;
    /** Both one-second hold peaks and one bounded stereo momentary observation are zero. */
    bool meter_quiescence_verified = false;
    /** Two one-second hold peaks do not establish continuous absence of external input. */
    bool continuous_input_silence_verified = false;
    /** A zero output hold peak is not a rendered-signal or acoustic silence proof. */
    bool continuous_output_silence_verified = false;
    bool unfrozen_verified = false;
    bool disarmed_verified = false;
    bool implicitly_disarmed_verified = false;
    /** No pending Session launch or stop action was observed on this Track. */
    bool session_launch_quiescent_verified = false;
    /** The Track is not overriding Arrangement playback with a Session Clip. */
    bool track_arrangement_playback_aligned_verified = false;
    bool clip_slot_states_observed = false;
    bool clip_slot_non_group_semantics_verified = false;
    bool clip_slot_playback_idle_verified = false;
    bool clip_slot_recording_quiescence_verified = false;
    bool clip_slot_launch_quiescence_verified = false;
    bool clip_slot_runtime_quiescence_verified = false;
    bool arrangement_clip_topology_observed = false;
    bool arrangement_content_absent_verified = false;
    bool take_lane_topology_observed = false;
    bool take_lanes_absent_verified = false;
    bool ungrouped_verified = false;
    bool crossfade_neutral_verified = false;
    bool stereo_panning_verified = false;
    bool track_activator_range_verified = false;
    bool track_activator_quantization_verified = false;
    bool track_activator_verified = false;
    /** Current public Track LOM omits the monitoring mode documented by the Live manual. */
    bool monitoring_state_observed = false;
    /** Without that state, Monitor-In clip suppression is not excluded by this adapter. */
    bool clip_output_not_suppressed_by_monitoring_verified = false;
    bool verified = false;
};

/** Post-apply audibility evidence for one generated Aux Return Track. */
struct AbletonReturnTrackGateEvidence {
    sunny::core::AuxBusId aux_bus_id;
    int track_index = 0;
    std::string requested_name;
    bool requested_mute = false;
    bool requested_solo = false;
    int requested_crossfade_assign = 1;
    int requested_panning_mode = 0;
    double requested_track_activator = 1.0;
    bool expected_mixer_enabled = true;
    std::optional<std::string> observed_name;
    std::optional<bool> observed_mute;
    std::optional<bool> observed_solo;
    std::optional<bool> observed_muted_via_solo;
    AbletonOutputRoutingEvidence output_routing;
    std::optional<int> observed_crossfade_assign;
    std::optional<int> observed_panning_mode;
    std::optional<double> observed_track_activator;
    std::optional<double> observed_track_activator_minimum;
    std::optional<double> observed_track_activator_maximum;
    std::optional<bool> observed_track_activator_is_quantized;
    std::optional<bool> observed_track_activator_is_enabled;
    std::optional<std::uint8_t> observed_track_activator_state;
    std::optional<std::uint8_t> observed_track_activator_automation_state;
    bool crossfade_neutral_verified = false;
    bool stereo_panning_verified = false;
    bool track_activator_range_verified = false;
    bool track_activator_quantization_verified = false;
    bool track_activator_verified = false;
    bool verified = false;
};

/** Final gain/spatial evidence for the project-wide Main Track output stage. */
struct AbletonMasterTrackGateEvidence {
    double requested_track_activator = 1.0;
    int requested_panning_mode = 0;
    double requested_pan = 0.0;
    std::optional<double> observed_track_activator;
    std::optional<double> observed_track_activator_minimum;
    std::optional<double> observed_track_activator_maximum;
    std::optional<bool> observed_track_activator_is_quantized;
    std::optional<bool> observed_track_activator_is_enabled;
    std::optional<std::uint8_t> observed_track_activator_state;
    std::optional<std::uint8_t> observed_track_activator_automation_state;
    std::optional<int> observed_panning_mode;
    std::optional<double> observed_pan;
    std::optional<double> observed_pan_minimum;
    std::optional<double> observed_pan_maximum;
    std::optional<bool> observed_pan_is_quantized;
    std::optional<bool> observed_pan_is_enabled;
    std::optional<std::uint8_t> observed_pan_state;
    std::optional<std::uint8_t> observed_pan_automation_state;
    bool track_activator_range_verified = false;
    bool track_activator_quantization_verified = false;
    bool track_activator_verified = false;
    bool stereo_panning_verified = false;
    bool pan_range_verified = false;
    bool pan_quantization_verified = false;
    bool neutral_pan_verified = false;
    bool verified = false;
};

/** Final-snapshot evidence for one device insertion and its exact containing chain. */
struct AbletonDevicePostconditionEvidence {
    std::string device_path;
    std::string requested_name;
    std::uint32_t requested_index = 0;
    std::uint8_t requested_type = 0;
    std::uint32_t expected_chain_size = 0;
    std::optional<std::uint32_t> observed_chain_size;
    std::optional<std::string> observed_name;
    std::optional<std::string> observed_class_display_name;
    std::optional<std::string> observed_class_name;
    std::optional<std::uint8_t> observed_type;
    std::optional<bool> observed_active;
    std::optional<bool> observed_can_have_chains;
    std::optional<std::uint32_t> observed_latency_in_samples;
    std::optional<double> observed_latency_in_ms;
    bool reported_latency_observed = false;
    /** True only when Device.can_have_chains proves this is not a Device Rack. */
    bool flat_device_verified = false;
    /** Public per-device reports are not a complete compensated output-path latency model. */
    bool render_path_latency_fully_observed = false;
    bool verified = false;
};

/** Final-snapshot evidence for the structural state of one generated Session Clip. */
struct AbletonClipPostconditionEvidence {
    sunny::core::PartId part_id;
    int track_index = 0;
    std::uint32_t slot_index = 0;
    std::vector<std::uint32_t> requested_occupied_slot_indices{0};
    std::string requested_name;
    bool requested_is_audio_clip = false;
    bool requested_is_midi_clip = true;
    bool requested_is_arrangement_clip = false;
    /** The explicit Session/Take-Lane identity pair is modeled from Live 11 onward. */
    bool location_identity_available = false;
    bool requested_is_session_clip = true;
    bool requested_is_take_lane_clip = false;
    double requested_length = 0.0;
    int requested_signature_numerator = 4;
    int requested_signature_denominator = 4;
    double requested_start_marker = 0.0;
    double requested_end_marker = 0.0;
    double requested_end_time = 0.0;
    bool requested_looping = false;
    bool requested_muted = false;
    bool requested_has_envelopes = false;
    bool requested_is_playing = false;
    bool requested_is_recording = false;
    bool requested_is_overdubbing = false;
    bool requested_is_triggered = false;
    bool requested_will_record_on_start = false;
    bool groove_state_available = false;
    bool requested_has_groove = false;
    bool launch_state_available = false;
    int requested_launch_mode = 0;
    int requested_launch_quantization = 1;
    bool requested_legato = false;
    double requested_velocity_amount = 0.0;
    std::vector<std::uint32_t> observed_occupied_slot_indices;
    /** The generated Track contains this Clip and no other occupied Session slot. */
    bool exact_slot_occupancy_verified = false;
    std::optional<std::string> observed_name;
    std::optional<bool> observed_is_audio_clip;
    std::optional<bool> observed_is_midi_clip;
    std::optional<bool> observed_is_arrangement_clip;
    std::optional<bool> observed_is_session_clip;
    std::optional<bool> observed_is_take_lane_clip;
    /** Public media and location predicates form the requested Session MIDI identity. */
    bool clip_identity_verified = false;
    std::optional<double> observed_length;
    std::optional<int> observed_signature_numerator;
    std::optional<int> observed_signature_denominator;
    std::optional<double> observed_start_marker;
    std::optional<double> observed_end_marker;
    std::optional<double> observed_end_time;
    /** For an unlooped Session Clip, public end_time equals the requested End Marker. */
    bool playback_end_verified = false;
    std::optional<bool> observed_looping;
    std::optional<bool> observed_muted;
    std::optional<bool> observed_has_envelopes;
    bool envelope_absence_verified = false;
    std::optional<bool> observed_is_playing;
    std::optional<bool> observed_is_recording;
    std::optional<bool> observed_is_overdubbing;
    std::optional<bool> observed_is_triggered;
    std::optional<bool> observed_will_record_on_start;
    /** Exact public runtime predicates were all present in the sequential snapshot. */
    bool runtime_state_observed = false;
    /** No current recording, overdub, or queued record start was observed. */
    bool recording_quiescence_verified = false;
    /** No current playback or queued launch was observed at the snapshot point. */
    bool playback_idle_verified = false;
    std::optional<bool> observed_has_groove;
    bool groove_absence_verified = false;
    std::optional<int> observed_launch_mode;
    std::optional<int> observed_launch_quantization;
    std::optional<bool> observed_legato;
    std::optional<double> observed_velocity_amount;
    bool launch_behavior_verified = false;
    /** Follow Actions are not exposed by the public Clip LOM. */
    bool follow_actions_observed = false;
    /** Structural markers/loop and launch scalars do not prove a one-shot execution. */
    bool one_shot_playback_verified = false;
    /** Public Clip LOM exposes no MIDI bank/sub-bank/program launch state. */
    bool midi_bank_program_state_observed = false;
    /** A generated Clip is not proven unable to emit a launch-time Program Change. */
    bool program_change_suppression_verified = false;
    /** Public Clip note dictionaries expose no per-note MPE expression state. */
    bool mpe_note_expression_state_observed = false;
    /** Exact ordinary-note fields do not prove neutral pitch-bend/slide/pressure curves. */
    bool mpe_note_expression_neutrality_verified = false;
    /** Structural beat positions plus groove absence do not measure rendered device/output time. */
    bool audible_timing_verified = false;
    /** Groove absence does not constrain launch velocity, devices, or rendered amplitude. */
    bool audible_velocity_verified = false;
    bool verified = false;
};

/** Final note evidence for one requested nonempty Part batch, with explicit query coverage. */
struct AbletonNotePostconditionEvidence {
    sunny::core::PartId part_id;
    int track_index = 0;
    std::uint32_t slot_index = 0;
    AbletonNoteAction deployment_action = AbletonNoteAction::RecordedOnly;
    std::vector<AbletonNoteDeployment::RequestedNote> requested_notes;
    std::vector<int> requested_note_ids;
    std::vector<AbletonNoteDeployment::ObservedNote> observed_notes;
    bool identity_verified = false;
    bool properties_verified = false;
    bool entire_clip_population_observed = false;
    std::optional<double> observed_time_span;
    bool verified = false;
};

/** Final-snapshot evidence for global Song state and the generated Session row. */
struct AbletonSongPostconditionEvidence {
    double requested_tempo = 120.0;
    int requested_signature_numerator = 4;
    int requested_signature_denominator = 4;
    std::string requested_scene_name;
    /** Every Scene is expected not to advertise a pending launch at observation time. */
    bool requested_scene_triggered = false;
    bool requested_scene_tempo_enabled = false;
    bool requested_scene_signature_enabled = false;
    bool requested_is_counting_in = false;
    bool requested_arrangement_overdub = false;
    bool requested_overdub = false;
    bool requested_record_mode = false;
    bool requested_session_record = false;
    bool requested_session_automation_record = false;
    bool requested_is_ableton_link_enabled = false;
    bool requested_is_ableton_link_start_stop_sync_enabled = false;
    bool requested_tempo_follower_enabled = false;
    bool requested_nudge_down = false;
    bool requested_nudge_up = false;
    bool requested_back_to_arranger = false;
    bool requested_re_enable_automation_enabled = false;
    bool requested_arrangement_loop = false;
    bool requested_metronome = false;
    std::optional<double> observed_tempo;
    std::optional<int> observed_signature_numerator;
    std::optional<int> observed_signature_denominator;
    std::optional<std::string> observed_scene_name;
    std::vector<bool> observed_scene_triggered_states;
    bool scene_trigger_states_observed = false;
    /** The complete Scene vector was observed and every is_triggered value was false. */
    bool all_scene_launches_quiescent_verified = false;
    std::optional<bool> observed_scene_tempo_enabled;
    std::optional<bool> observed_scene_signature_enabled;
    std::optional<bool> observed_is_playing;
    std::optional<bool> observed_is_counting_in;
    std::optional<bool> observed_arrangement_overdub;
    std::optional<bool> observed_overdub;
    std::optional<bool> observed_record_mode;
    std::optional<bool> observed_session_record;
    std::optional<bool> observed_session_automation_record;
    std::optional<bool> observed_is_ableton_link_enabled;
    std::optional<bool> observed_is_ableton_link_start_stop_sync_enabled;
    std::optional<bool> observed_tempo_follower_enabled;
    std::optional<bool> observed_nudge_down;
    std::optional<bool> observed_nudge_up;
    std::optional<bool> observed_back_to_arranger;
    std::optional<bool> observed_re_enable_automation_enabled;
    std::optional<bool> observed_arrangement_loop;
    std::optional<bool> observed_metronome;
    /** Song transport may run during a structurally valid deployment. */
    bool transport_stopped_verified = false;
    /** No count-in, record, overdub, or automation-record arm was observed. */
    bool recording_modes_quiescent_verified = false;
    /** The public Link, Tempo Follower, and tempo-nudge controls were all inactive. */
    bool public_tempo_controls_quiescent_verified = false;
    /** Incoming MIDI Clock/MTC enablement has no public Song LOM property. */
    bool external_midi_sync_state_observed = false;
    /** Song.tempo exposes a scalar, not its Arrangement automation envelope. */
    bool tempo_automation_state_observed = false;
    /** Sequential scalar/control reads cannot prove tempo persistence. */
    bool tempo_stability_verified = false;
    /** Session playback did not supersede stored Arrangement playback. */
    bool arrangement_playback_aligned_verified = false;
    /** No automated control advertised a current manual override. */
    bool automation_overrides_quiescent_verified = false;
    /** Arrangement playback was not configured to repeat a loop brace. */
    bool arrangement_loop_disabled_verified = false;
    /** Live's metronome was disabled at the observation point. */
    bool metronome_disabled_verified = false;
    std::optional<int> observed_scale_root_note;
    std::optional<std::string> observed_scale_name;
    std::vector<int> observed_scale_intervals;
    std::optional<bool> observed_scale_mode;
    std::optional<std::string> observed_tuning_name;
    std::optional<double> observed_pseudo_octave_in_cents;
    /** Exact target dictionaries retained without interpreting undocumented member schemas. */
    std::optional<nlohmann::json> observed_tuning_lowest_note;
    std::optional<nlohmann::json> observed_tuning_highest_note;
    std::optional<nlohmann::json> observed_tuning_reference_pitch;
    std::optional<nlohmann::json> observed_tuning_note_tunings;
    bool tuning_dictionary_payloads_observed = false;
    /** Opaque retention does not map the target dictionaries to ScoreTuning semantics. */
    bool tuning_definition_fully_observed = false;
    /** The current public Track LOM has no Bypass Tuning property. */
    bool per_track_tuning_bypass_observed = false;
    /** Device/MPE support requires runtime behavior beyond structural state. */
    bool instrument_tuning_support_observed = false;
    bool audible_pitch_verified = false;
    bool verified = false;
};

struct AbletonCuePostconditionEvidence {
    double requested_time = 0.0;
    std::string requested_name;
    std::optional<double> observed_time;
    std::optional<std::string> observed_name;
    bool verified = false;
};

struct AbletonMixerPostconditionEvidence {
    std::string parameter_path;
    std::string value_property;
    double requested_value = 0.0;
    bool requested_is_quantized = false;
    std::optional<double> observed_value;
    std::optional<double> observed_minimum;
    std::optional<double> observed_maximum;
    std::optional<bool> observed_is_quantized;
    std::optional<double> observed_default_value;
    std::optional<std::vector<std::string>> observed_value_items;
    std::optional<std::uint8_t> observed_state;
    std::optional<std::uint8_t> observed_automation_state;
    std::optional<bool> observed_is_enabled;
    /** Null for display_value, whose numeric domain is not the internal min/max interval. */
    std::optional<bool> range_verified;
    bool quantization_verified = false;
    bool verified = false;
};

enum class AbletonParameterOrigin : std::uint8_t { Timbre, Mix };

[[nodiscard]] const char* ableton_parameter_origin_name(AbletonParameterOrigin origin);

/** Final selective observation for one mapped native DeviceParameter. */
struct AbletonParameterPostconditionEvidence {
    AbletonParameterOrigin origin = AbletonParameterOrigin::Timbre;
    std::optional<sunny::core::PartId> part_id;
    std::optional<int> track_index;
    std::string source_path;
    std::optional<sunny::core::MixEffectId> effect_id;
    std::string device_path;
    std::string requested_name;
    sunny::core::DeviceParameterValueProperty value_property =
        sunny::core::DeviceParameterValueProperty::InternalValue;
    float requested_value = 0.0f;
    float range_min = 0.0f;
    float range_max = 1.0f;
    AbletonParameterAction deployment_action = AbletonParameterAction::NotApplied;
    std::optional<std::string> matched_name;
    std::optional<std::string> original_name;
    std::optional<float> observed_value;
    std::optional<float> observed_minimum;
    std::optional<float> observed_maximum;
    std::optional<bool> observed_is_quantized;
    std::optional<float> observed_default_value;
    std::optional<std::vector<std::string>> observed_value_items;
    std::optional<bool> observed_is_enabled;
    std::optional<std::uint8_t> observed_state;
    std::optional<std::uint8_t> observed_automation_state;
    bool identity_verified = false;
    bool value_verified = false;
    /** Null for display_value, whose mapping bounds are not Live's internal min/max. */
    std::optional<bool> range_verified;
    bool enabled_verified = false;
    bool state_verified = false;
    bool automation_verified = false;
    bool verified = false;
};

struct AbletonProjectPostconditionEvidence {
    bool observed = false;
    std::uint64_t song_states_requested = 0;
    std::uint64_t song_states_verified = 0;
    std::optional<AbletonSongPostconditionEvidence> song_state;
    std::uint64_t track_gates_requested = 0;
    std::uint64_t track_gates_verified = 0;
    std::vector<AbletonTrackGateEvidence> track_gates;
    std::uint64_t return_track_gates_requested = 0;
    std::uint64_t return_track_gates_verified = 0;
    std::vector<AbletonReturnTrackGateEvidence> return_track_gates;
    std::uint64_t master_track_gates_requested = 0;
    std::uint64_t master_track_gates_verified = 0;
    std::optional<AbletonMasterTrackGateEvidence> master_track_gate;
    std::uint64_t devices_requested = 0;
    std::uint64_t devices_verified = 0;
    std::vector<AbletonDevicePostconditionEvidence> devices;
    std::uint64_t clips_requested = 0;
    std::uint64_t clips_verified = 0;
    std::vector<AbletonClipPostconditionEvidence> clips;
    std::uint64_t note_batches_requested = 0;
    std::uint64_t note_batches_verified = 0;
    std::uint64_t notes_requested = 0;
    std::uint64_t notes_verified = 0;
    std::vector<AbletonNotePostconditionEvidence> note_batches;
    std::uint64_t cues_requested = 0;
    std::uint64_t cues_verified = 0;
    std::vector<AbletonCuePostconditionEvidence> cues;
    std::uint64_t mixer_properties_requested = 0;
    std::uint64_t mixer_properties_verified = 0;
    std::vector<AbletonMixerPostconditionEvidence> mixer_properties;
    std::uint64_t device_parameters_requested = 0;
    std::uint64_t device_parameters_verified = 0;
    std::vector<AbletonParameterPostconditionEvidence> device_parameters;
    std::vector<std::string> warnings;
};

struct AbletonProjectCompilationResult {
    AbletonPartTrackMap part_tracks;
    AbletonOutputRoutingBindings output_routing_bindings;
    std::vector<sunny::core::Diagnostic> diagnostics;
    AbletonCompilationResult score;
    std::vector<PartTimbreCompilationResult> timbre;
    MixCompilationResult mix;
    AbletonProjectPostconditionEvidence postconditions;
};

/** Move-only preconditions and exact dry-run sequence with one-shot state. */
struct AbletonProjectDeploymentPlan {
    AbletonProjectDeploymentPlan() = default;
    AbletonProjectDeploymentPlan(const AbletonProjectDeploymentPlan&) = delete;
    AbletonProjectDeploymentPlan& operator=(const AbletonProjectDeploymentPlan&) = delete;
    AbletonProjectDeploymentPlan(AbletonProjectDeploymentPlan&&) noexcept = default;
    AbletonProjectDeploymentPlan& operator=(AbletonProjectDeploymentPlan&&) noexcept = default;

    int ppq = 480;
    nlohmann::json project_state;
    nlohmann::json output_routing_binding_state;
    AbletonTargetSnapshot target_snapshot;
    AbletonPartTrackMap part_tracks;
    AbletonOutputRoutingBindings output_routing_bindings;
    std::vector<sunny::core::Diagnostic> diagnostics;
    AbletonProjectCompilationResult preview;
    std::vector<AbletonPlannedMutation> mutations;
    bool consumed = false;
};

enum class AbletonProjectDeploymentStatus : std::uint8_t {
    Completed,
    ProjectChanged,
    TargetChanged,
    PlanDiverged,
    PlanConsumed,
    ApplyFailed,
    PostSnapshotUnavailable,
};

/** Value result retained even when a non-atomic apply fails. */
struct AbletonProjectDeploymentAttempt {
    AbletonProjectDeploymentStatus status = AbletonProjectDeploymentStatus::ApplyFailed;
    std::optional<sunny::core::ErrorCode> error;
    std::optional<AbletonProjectCompilationResult> compilation;
    std::optional<AbletonTargetSnapshot> target_before;
    std::optional<AbletonTargetSnapshot> target_after;
    std::vector<AbletonMutationJournalEntry> mutation_journal;

    [[nodiscard]] bool target_may_be_partially_modified() const;
};

/** Validate, snapshot the target, and dry-run the exact mutation sequence. */
[[nodiscard]] sunny::core::Result<AbletonProjectDeploymentPlan> plan_project_to_ableton(
    const sunny::core::ProjectView& project, LomTransport& transport, int ppq = 480);

[[nodiscard]] sunny::core::Result<AbletonProjectDeploymentPlan>
plan_project_to_ableton(const sunny::core::ProjectView& project,
                        const AbletonOutputRoutingBindings& output_routing_bindings,
                        LomTransport& transport,
                        int ppq = 480);

/** Evaluate exact Track gate/output evidence from a validated post-apply snapshot. */
[[nodiscard]] sunny::core::Result<AbletonProjectPostconditionEvidence>
evaluate_project_ableton_postconditions(
    const sunny::core::ProjectView& project,
    const AbletonPartTrackMap& part_tracks,
    const AbletonTargetSnapshot& snapshot,
    const AbletonProjectCompilationResult* compilation = nullptr);

/** Apply a one-shot plan only while its project and target preconditions hold. */
[[nodiscard]] AbletonProjectDeploymentAttempt
apply_project_ableton_plan(AbletonProjectDeploymentPlan& plan,
                           const sunny::core::ProjectView& project,
                           LomTransport& transport);

/**
 * @brief Compile one fully-correspondent project into a single Live Set
 *
 * Convenience plan+apply operation. All internal and cross-IR validation and
 * an exact command dry-run complete before the first target mutation. The
 * richer plan/apply API must be used when partial-failure evidence is needed.
 */
[[nodiscard]] sunny::core::Result<AbletonProjectCompilationResult> compile_project_to_ableton(
    const sunny::core::ProjectView& project, LomTransport& transport, int ppq = 480);

[[nodiscard]] sunny::core::Result<AbletonProjectCompilationResult>
compile_project_to_ableton(const sunny::core::ProjectView& project,
                           const AbletonOutputRoutingBindings& output_routing_bindings,
                           LomTransport& transport,
                           int ppq = 480);

} // namespace sunny::infrastructure::formats
