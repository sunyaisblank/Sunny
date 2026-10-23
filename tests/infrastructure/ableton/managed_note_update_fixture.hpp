// Literal actual Python source-contract acknowledgement, not a host qualification.
// SM1 hashes were emitted by Python hashlib; numeric JSON types are intentional.
#pragma once
#include <string_view>
inline constexpr std::string_view managed_note_update_fixture = R"JSON({
  "request": {
    "document_token": "document_a",
    "operation_id": "update_a",
    "project_key": "project_a",
    "binding_key": "part_a",
    "expected_content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
    "changes": [
      {
        "note_id": 1,
        "expected": {
          "pitch": 60,
          "start_time": 0.0,
          "duration": 1.0,
          "velocity": 96.0,
          "mute": false,
          "probability": 1.0,
          "velocity_deviation": 0.0,
          "release_velocity": 64.0
        },
        "updates": {
          "pitch": 61,
          "start_time": 0.25,
          "duration": 1.5,
          "velocity": 80.5,
          "mute": true,
          "release_velocity": 45.25
        }
      }
    ]
  },
  "journal": {
    "document_token": "document_a",
    "operation_id": "update_a",
    "request_fingerprint": "f8b4641ff7afd9f757c5431631c4f58f21d087e8a4c4ee1bc5cc8122a4c70983",
    "request": {
      "document_token": "document_a",
      "operation_id": "update_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
      "changes": [
        {
          "note_id": 1,
          "expected": {
            "pitch": 60,
            "start_time": 0.0,
            "duration": 1.0,
            "velocity": 96.0,
            "mute": false,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0
          },
          "updates": {
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": true,
            "release_velocity": 45.25
          }
        }
      ]
    },
    "name": "sunny_managed_update_notes",
    "outcome": "acknowledged",
    "native_mutation_started": true,
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
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": true,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 45.25
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
      "content_fingerprint": "06d4c3ea6f6055b78dc5f2a0a25c428b6cb872d0f97d066244db8c7941b5ae20",
      "note_identity": {
        "entire_clip_population_observed": true,
        "notes": [
          {
            "note_id": 1,
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": true,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 45.25
          }
        ]
      },
      "note_identity_fingerprint": "61b73f065613bed3bb210f16f53e2c3ed6b02dd81f599324ad1d723516ed5c87",
      "track_tag": "Sunny|project_a|part_a|track",
      "clip_tag": "Sunny|project_a|part_a|clip",
      "structural_boundary_complete": true,
      "content_boundary_complete": false,
      "unavailable_reasons": [
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "observed_notes_match_request": false,
      "observed_clip_properties_match_request": true,
      "note_update": {
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
            }
          ]
        },
        "before_note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
        "notes_submitted": 1,
        "observed_updates_match_request": true,
        "untouched_notes_preserved": true,
        "note_ids_preserved": true
      }
    }
  },
  "observation": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "outcome": "observed",
    "ownership_retained": true,
    "observation": {
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
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": true,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 45.25
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
      "content_fingerprint": "06d4c3ea6f6055b78dc5f2a0a25c428b6cb872d0f97d066244db8c7941b5ae20",
      "note_identity": {
        "entire_clip_population_observed": true,
        "notes": [
          {
            "note_id": 1,
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": true,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 45.25
          }
        ]
      },
      "note_identity_fingerprint": "61b73f065613bed3bb210f16f53e2c3ed6b02dd81f599324ad1d723516ed5c87",
      "track_tag": "Sunny|project_a|part_a|track",
      "clip_tag": "Sunny|project_a|part_a|clip",
      "structural_boundary_complete": true,
      "content_boundary_complete": false,
      "unavailable_reasons": [
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ]
    }
  }
})JSON";
