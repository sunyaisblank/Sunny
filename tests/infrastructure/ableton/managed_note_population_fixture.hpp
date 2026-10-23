// Actual offline Python source-contract response; no host qualification.
#pragma once
#include <string_view>
inline constexpr std::string_view managed_note_population_fixture = R"JSON({
  "request": {
    "document_token": "document_a",
    "operation_id": "population_a",
    "project_key": "project_a",
    "binding_key": "part_a",
    "expected_content_fingerprint": "3836eff9dbd64b6f0a61e1d6345bcc93001723b1e1a5bc844cdefe88bfa2dae3",
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
          "pitch": 62,
          "velocity": 96.0
        }
      }
    ],
    "deletions": [
      {
        "note_id": 2,
        "expected": {
          "pitch": 67,
          "start_time": 2.0,
          "duration": 0.5,
          "velocity": 72.0,
          "mute": true,
          "probability": 1.0,
          "velocity_deviation": 0.0,
          "release_velocity": 32.0
        }
      }
    ],
    "additions": [
      {
        "note_key": "e3_n0",
        "note": {
          "pitch": 64,
          "start_time": 2.0,
          "duration": 0.5,
          "velocity": 88.0,
          "mute": false,
          "probability": 1.0,
          "velocity_deviation": 0.0,
          "release_velocity": 64.0
        }
      }
    ]
  },
  "journal": {
    "document_token": "document_a",
    "operation_id": "population_a",
    "request_fingerprint": "72e7310be47b01eefce9c56083500d14e133cbfec5990a766195809b81da712e",
    "request": {
      "document_token": "document_a",
      "operation_id": "population_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "3836eff9dbd64b6f0a61e1d6345bcc93001723b1e1a5bc844cdefe88bfa2dae3",
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
            "pitch": 62,
            "velocity": 96.0
          }
        }
      ],
      "deletions": [
        {
          "note_id": 2,
          "expected": {
            "pitch": 67,
            "start_time": 2.0,
            "duration": 0.5,
            "velocity": 72.0,
            "mute": true,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 32.0
          }
        }
      ],
      "additions": [
        {
          "note_key": "e3_n0",
          "note": {
            "pitch": 64,
            "start_time": 2.0,
            "duration": 0.5,
            "velocity": 88.0,
            "mute": false,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0
          }
        }
      ]
    },
    "name": "sunny_managed_revise_note_population",
    "outcome": "acknowledged",
    "native_mutation_started": true,
    "progress": {
      "started_calls": [
        "remove_notes_by_id",
        "apply_note_modifications",
        "add_new_notes"
      ],
      "returned_calls": [
        "remove_notes_by_id",
        "apply_note_modifications",
        "add_new_notes"
      ],
      "returned_added_note_ids": [
        3
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
            "pitch": 62,
            "start_time": 0.0,
            "duration": 1.0,
            "velocity": 96.0,
            "mute": false,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0
          },
          {
            "pitch": 64,
            "start_time": 2.0,
            "duration": 0.5,
            "velocity": 88.0,
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
      "content_fingerprint": "e71e0329eb6d33049f482e50a37e339d5cb64131b4ecb07b44b1084a1870b4ef",
      "note_identity": {
        "entire_clip_population_observed": true,
        "notes": [
          {
            "note_id": 1,
            "pitch": 62,
            "start_time": 0.0,
            "duration": 1.0,
            "velocity": 96.0,
            "mute": false,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0
          },
          {
            "note_id": 3,
            "pitch": 64,
            "start_time": 2.0,
            "duration": 0.5,
            "velocity": 88.0,
            "mute": false,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0
          }
        ]
      },
      "note_identity_fingerprint": "2dfee4804c8c5153260c94575e96328acedf18ba695f897ce8044c0242c8aa2e",
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
      "note_population_update": {
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
        "changes_submitted": 1,
        "deletions_submitted": 1,
        "additions_submitted": 1,
        "returned_added_note_ids": [
          3
        ],
        "addition_associations": [
          {
            "note_key": "e3_n0",
            "note_id": 3
          }
        ],
        "observed_changes_match_request": true,
        "observed_deletions_absent": true,
        "observed_additions_match_request": true,
        "untouched_notes_preserved": true,
        "retained_note_ids_preserved": true,
        "observed_population_cardinality_match": true
      }
    }
  }
})JSON";
