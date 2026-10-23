#pragma once
// Actual offline native-boundary Python journal, with typed SM1 digests.
inline constexpr const char* MANAGED_CLIP_GEOMETRY_FIXTURE = R"JSON({
  "before": {
    "track_index": 1,
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
        "is_grouped": false,
        "back_to_arranger": false,
        "has_audio_input": false,
        "has_midi_input": true,
        "has_audio_output": false,
        "has_midi_output": true
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
        "launch_quantization": 1,
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
          "display_name": "No Output",
          "identifier": "5:No Output"
        },
        "output_routing_channel": {
          "display_name": "",
          "identifier": ""
        }
      },
      "content_counts": {
        "arrangement_clips": 0,
        "take_lanes": 0
      },
      "devices_empty": true,
      "other_session_clips_empty": true,
      "entire_clip_population_observed": true,
      "mpe_note_expression_state_observed": false,
      "follow_actions_state_observed": false
    },
    "content_fingerprint": "3836eff9dbd64b6f0a61e1d6345bcc93001723b1e1a5bc844cdefe88bfa2dae3",
    "note_identity": {
      "entire_clip_population_observed": true,
      "notes": [
        {
          "note_id": 1,
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
          "note_id": 2,
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
    "note_identity_fingerprint": "8a5fc73380703250cf459592bdff2163d626b69e86905c2578d0f3eff4a47857",
    "track_tag": "Sunny|project_a|part_a|track",
    "clip_tag": "Sunny|project_a|part_a|clip",
    "structural_boundary_complete": true,
    "content_boundary_complete": false,
    "unavailable_reasons": [
      "MpeExpressionUnavailable: per-note expression fields were not observed",
      "FollowActionsUnavailable: Follow Action settings were not observed"
    ],
    "device_identity": {
      "schema_version": 1,
      "cohort": [],
      "opaque_state_observed": false
    },
    "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5",
    "observed_notes_match_request": true,
    "observed_clip_properties_match_request": true
  },
  "journal": {
    "document_token": "document_a",
    "operation_id": "geometry_a",
    "request_fingerprint": "e8893d2a3346e8a6cb05d53360df3d46b0e748183ab19d9b7ca9f64567972868",
    "request": {
      "document_token": "document_a",
      "operation_id": "geometry_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "3836eff9dbd64b6f0a61e1d6345bcc93001723b1e1a5bc844cdefe88bfa2dae3",
      "geometry": {
        "end_marker": 8.0,
        "signature_numerator": 3,
        "signature_denominator": 8
      }
    },
    "name": "sunny_managed_update_clip_geometry",
    "outcome": "acknowledged",
    "native_mutation_started": true,
    "progress": {
      "started_properties": [
        "end_marker",
        "signature_numerator",
        "signature_denominator"
      ],
      "returned_properties": [
        "end_marker",
        "signature_numerator",
        "signature_denominator"
      ]
    },
    "result": {
      "track_index": 1,
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
          "is_grouped": false,
          "back_to_arranger": false,
          "has_audio_input": false,
          "has_midi_input": true,
          "has_audio_output": false,
          "has_midi_output": true
        },
        "clip": {
          "name": "Sunny|project_a|part_a|clip",
          "signature_numerator": 3,
          "signature_denominator": 8,
          "start_marker": 0.0,
          "end_marker": 8.0,
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
          "launch_quantization": 1,
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
            "display_name": "No Output",
            "identifier": "5:No Output"
          },
          "output_routing_channel": {
            "display_name": "",
            "identifier": ""
          }
        },
        "content_counts": {
          "arrangement_clips": 0,
          "take_lanes": 0
        },
        "devices_empty": true,
        "other_session_clips_empty": true,
        "entire_clip_population_observed": true,
        "mpe_note_expression_state_observed": false,
        "follow_actions_state_observed": false
      },
      "content_fingerprint": "392a60197268f08f1c31e30b941e9f262eea3852d60da3557e4b1b2566c66bce",
      "note_identity": {
        "entire_clip_population_observed": true,
        "notes": [
          {
            "note_id": 1,
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
            "note_id": 2,
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
      "note_identity_fingerprint": "8a5fc73380703250cf459592bdff2163d626b69e86905c2578d0f3eff4a47857",
      "track_tag": "Sunny|project_a|part_a|track",
      "clip_tag": "Sunny|project_a|part_a|clip",
      "structural_boundary_complete": true,
      "content_boundary_complete": false,
      "unavailable_reasons": [
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "device_identity": {
        "schema_version": 1,
        "cohort": [],
        "opaque_state_observed": false
      },
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5",
      "observed_notes_match_request": true,
      "observed_clip_properties_match_request": false,
      "clip_geometry_update": {
        "before_manifest": {
          "schema_version": 1,
          "track": {
            "name": "Sunny|project_a|part_a|track",
            "mute": false,
            "solo": false,
            "arm": false,
            "implicit_arm": false,
            "is_frozen": false,
            "is_grouped": false,
            "back_to_arranger": false,
            "has_audio_input": false,
            "has_midi_input": true,
            "has_audio_output": false,
            "has_midi_output": true
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
            "launch_quantization": 1,
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
              "display_name": "No Output",
              "identifier": "5:No Output"
            },
            "output_routing_channel": {
              "display_name": "",
              "identifier": ""
            }
          },
          "content_counts": {
            "arrangement_clips": 0,
            "take_lanes": 0
          },
          "devices_empty": true,
          "other_session_clips_empty": true,
          "entire_clip_population_observed": true,
          "mpe_note_expression_state_observed": false,
          "follow_actions_state_observed": false
        },
        "before_note_identity": {
          "entire_clip_population_observed": true,
          "notes": [
            {
              "note_id": 1,
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
              "note_id": 2,
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
        "before_note_identity_fingerprint": "8a5fc73380703250cf459592bdff2163d626b69e86905c2578d0f3eff4a47857",
        "before_device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5",
        "requested_geometry": {
          "end_marker": 8.0,
          "signature_numerator": 3,
          "signature_denominator": 8
        },
        "submitted_properties": [
          "end_marker",
          "signature_numerator",
          "signature_denominator"
        ],
        "returned_properties": [
          "end_marker",
          "signature_numerator",
          "signature_denominator"
        ],
        "observed_geometry_matches_request": true,
        "note_values_preserved": true,
        "note_ids_preserved": true,
        "note_cardinality_preserved": true,
        "other_finite_properties_preserved": true,
        "device_identity_preserved": true,
        "loop_end_relationship": "unchanged_inactive_boundary"
      }
    }
  }
})JSON";
