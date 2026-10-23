#pragma once
// Actual source-produced capture after explicit current Group-only adoption.
inline constexpr const char* MANAGED_GROUP_OBSERVATION = R"JSON({
  "track_index": 0,
  "slot_index": 0,
  "manifest": {
    "schema_version": 1,
    "track": {
      "name": "Sunny|project_a|part_a|track",
      "mute": false,
      "solo": false,
      "arm": false,
      "implicit_arm": false,
      "is_frozen": false,
      "is_grouped": true,
      "back_to_arranger": false,
      "has_audio_input": false,
      "has_midi_input": true,
      "has_audio_output": true,
      "has_midi_output": false
    },
    "clip": {
      "name": "Sunny|project_a|part_a|clip",
      "signature_numerator": 4,
      "signature_denominator": 4,
      "start_marker": 0.0,
      "end_marker": 4.0,
      "loop_start": 0.0,
      "loop_end": 4.0,
      "looping": false,
      "muted": false,
      "has_envelopes": false,
      "has_groove": false,
      "is_session_clip": true,
      "is_arrangement_clip": false,
      "is_midi_clip": true,
      "is_audio_clip": false,
      "is_playing": false,
      "is_recording": false,
      "is_overdubbing": false,
      "is_triggered": false,
      "will_record_on_start": false,
      "launch_mode": 0,
      "launch_quantization": 0,
      "legato": false,
      "velocity_amount": 0.0
    },
    "notes": [
      {
        "pitch": 60,
        "start_time": 0.0,
        "duration": 1.0,
        "velocity": 96.0,
        "mute": false,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0
      },
      {
        "pitch": 67,
        "start_time": 2.0,
        "duration": 0.5,
        "velocity": 72.0,
        "mute": true,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 32.0
      }
    ],
    "mixer": {
      "panning_mode": 0,
      "crossfade_assign": 1,
      "volume": {
        "name": "Track Volume",
        "original_name": "Track Volume",
        "value": 0.85,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "panning": {
        "name": "Track Panning",
        "original_name": "Track Panning",
        "value": 0.0,
        "min": -1.0,
        "max": 1.0,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "track_activator": {
        "name": "Speaker On",
        "original_name": "Speaker On",
        "value": 1.0,
        "min": 0.0,
        "max": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "sends": [
        {
          "name": "Send A",
          "original_name": "Send A",
          "value": 0.0,
          "min": 0.0,
          "max": 1.0,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        }
      ]
    },
    "routing": {
      "input_routing_type": {
        "display_name": "All Ins",
        "identifier": "0:All Ins"
      },
      "input_routing_channel": {
        "display_name": "All Channels",
        "identifier": "All Channels"
      },
      "output_routing_type": {
        "display_name": "Master",
        "identifier": "2:Master"
      },
      "output_routing_channel": {
        "display_name": "Track In",
        "identifier": "Track In"
      }
    },
    "content_counts": {
      "arrangement_clips": 0,
      "take_lanes": 0
    },
    "devices_empty": false,
    "other_session_clips_empty": true,
    "entire_clip_population_observed": true,
    "mpe_note_expression_state_observed": false,
    "follow_actions_state_observed": false
  },
  "content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
  "note_identity": {
    "entire_clip_population_observed": true,
    "notes": [
      {
        "note_id": 41,
        "pitch": 60,
        "start_time": 0.0,
        "duration": 1.0,
        "velocity": 96.0,
        "mute": false,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0
      },
      {
        "note_id": 99,
        "pitch": 67,
        "start_time": 2.0,
        "duration": 0.5,
        "velocity": 72.0,
        "mute": true,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 32.0
      }
    ]
  },
  "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
  "track_tag": "Sunny|project_a|part_a|track",
  "clip_tag": "Sunny|project_a|part_a|clip",
  "structural_boundary_complete": false,
  "content_boundary_complete": false,
  "unavailable_reasons": [
    "Native devices/racks are outside this initial complete manifest",
    "Grouped/frozen Track state is unsupported",
    "MpeExpressionUnavailable: per-note expression fields were not observed",
    "FollowActionsUnavailable: Follow Action settings were not observed"
  ],
  "group_authority": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "group_key": "group_strings",
    "approved_preview_fingerprint": "4999f3d91c78adc0b96998c05f74a24c1b53b9470b07f5f04682f89e50f6ea2c",
    "group_track_index": 1,
    "member_binding_keys": [
      "part_a"
    ],
    "member_track_indices": [
      0
    ],
    "selected_binding_key": "part_a",
    "selected_track_index": 0,
    "authority_origin": "explicit_current_group_adoption",
    "historical_identity_proven": false
  },
  "group_authority_fingerprint": "efe27876dc344ef457a29937de88dfb53c1d7b52a04bf4b26c321ae2c681d5f2"
})JSON";
