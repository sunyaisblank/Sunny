#pragma once
// Actual Python native-boundary preview and journal, with typed SM1 hashes.
inline constexpr const char* MANAGED_ENVELOPE_REVISION_FIXTURE = R"JSON({
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "lane": {
      "parameter": {
        "kind": "panning"
      },
      "interpolation": "step",
      "clip_end": 4.0,
      "points": [
        {
          "time": 0.0,
          "value": -0.5
        },
        {
          "time": 2.0,
          "value": 0.5
        }
      ]
    },
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
          "has_envelopes": true,
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
      "content_fingerprint": "f32dd3de474090975a6614663900045dd9901244d5bf0fb168ce40f3d438a1ac",
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
      "structural_boundary_complete": false,
      "content_boundary_complete": false,
      "unavailable_reasons": [
        "Complete native envelope breakpoint population is unavailable",
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "device_identity": {
        "schema_version": 1,
        "cohort": [],
        "opaque_state_observed": false
      },
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "selected_envelope": {
      "has_envelope": true,
      "parameter": {
        "matched_name": "Track Panning",
        "original_name": "Track Panning",
        "minimum": -1.0,
        "maximum": 1.0,
        "unit": "internal",
        "state": 0,
        "automation_state": 0
      },
      "samples": [
        {
          "time": 1.0,
          "value": 0.0
        },
        {
          "time": 3.0,
          "value": 0.0
        }
      ]
    },
    "scope": {
      "replacement_scope": "selected_parameter_envelope",
      "breakpoint_population_observed": false,
      "unsampled_state_preservation_proven": false,
      "other_parameter_envelopes_written": false,
      "historical_envelope_identity_proven": false,
      "same_parameter_modulation_preservation_proven": false,
      "host_qualified": false
    },
    "native_step_call_budget": 64,
    "preview_fingerprint": "4670c565be4a9382ebb2a0f602cbc5bcd28f601432a487ea84da60071a934e23"
  },
  "journal": {
    "document_token": "document_a",
    "operation_id": "replace_a",
    "request_fingerprint": "6c4e60ac821e978e10880c8deb2264c9dc29d3c665b0e4c786f168db6be3e234",
    "request": {
      "document_token": "document_a",
      "operation_id": "replace_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "4670c565be4a9382ebb2a0f602cbc5bcd28f601432a487ea84da60071a934e23",
      "explicit_selected_envelope_replacement": true,
      "allow_unsampled_selected_state_overwrite": true
    },
    "name": "sunny_managed_replace_envelope",
    "outcome": "acknowledged",
    "native_mutation_started": true,
    "progress": {
      "started_calls": [
        "clear_envelope",
        "create_automation_envelope",
        "insert_step",
        "insert_step"
      ],
      "returned_calls": [
        "clear_envelope",
        "create_automation_envelope",
        "insert_step",
        "insert_step"
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
          "signature_numerator": 4,
          "signature_denominator": 4,
          "start_marker": 0.0,
          "end_marker": 4.0,
          "loop_start": 0.0,
          "loop_end": 4.0,
          "looping": false,
          "muted": false,
          "has_envelopes": true,
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
      "content_fingerprint": "f32dd3de474090975a6614663900045dd9901244d5bf0fb168ce40f3d438a1ac",
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
      "structural_boundary_complete": false,
      "content_boundary_complete": false,
      "unavailable_reasons": [
        "Complete native envelope breakpoint population is unavailable",
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
      "observed_clip_properties_match_request": true,
      "envelope_replacement": {
        "before_observation": {
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
              "has_envelopes": true,
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
          "content_fingerprint": "f32dd3de474090975a6614663900045dd9901244d5bf0fb168ce40f3d438a1ac",
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
          "structural_boundary_complete": false,
          "content_boundary_complete": false,
          "unavailable_reasons": [
            "Complete native envelope breakpoint population is unavailable",
            "MpeExpressionUnavailable: per-note expression fields were not observed",
            "FollowActionsUnavailable: Follow Action settings were not observed"
          ],
          "device_identity": {
            "schema_version": 1,
            "cohort": [],
            "opaque_state_observed": false
          },
          "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
        },
        "preview_metadata": {
          "schema_version": 1,
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "lane": {
            "parameter": {
              "kind": "panning"
            },
            "interpolation": "step",
            "clip_end": 4.0,
            "points": [
              {
                "time": 0.0,
                "value": -0.5
              },
              {
                "time": 2.0,
                "value": 0.5
              }
            ]
          },
          "selected_envelope": {
            "has_envelope": true,
            "parameter": {
              "matched_name": "Track Panning",
              "original_name": "Track Panning",
              "minimum": -1.0,
              "maximum": 1.0,
              "unit": "internal",
              "state": 0,
              "automation_state": 0
            },
            "samples": [
              {
                "time": 1.0,
                "value": 0.0
              },
              {
                "time": 3.0,
                "value": 0.0
              }
            ]
          },
          "scope": {
            "replacement_scope": "selected_parameter_envelope",
            "breakpoint_population_observed": false,
            "unsampled_state_preservation_proven": false,
            "other_parameter_envelopes_written": false,
            "historical_envelope_identity_proven": false,
            "same_parameter_modulation_preservation_proven": false,
            "host_qualified": false
          },
          "native_step_call_budget": 64
        },
        "preview_fingerprint": "4670c565be4a9382ebb2a0f602cbc5bcd28f601432a487ea84da60071a934e23",
        "actual_samples": [
          {
            "time": 1.0,
            "value": -0.5
          },
          {
            "time": 3.0,
            "value": 0.5
          }
        ],
        "observed_step_samples_match_request": true,
        "note_ids_and_values_preserved": true,
        "other_finite_properties_preserved": true,
        "device_identity_preserved": true
      }
    }
  }
})JSON";
