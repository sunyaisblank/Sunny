#pragma once
#include <nlohmann/json.hpp>
namespace recovery_test_fixture {
inline nlohmann::json preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "context": {
    "bridge_instance": "bridge_a",
    "document_token": "document_a"
  },
  "project_key": "project_a",
  "binding_key": "part_a",
  "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "selector": {
    "track_index": 0,
    "slot_index": 0
  },
  "set_info": {
    "file_path": null,
    "name": null
  },
  "observation": {
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
    "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
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
    "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
  },
  "authority_origin": "none",
  "historical_identity_proven": false,
  "allowed_domains": [
    "existing_note_updates",
    "note_population_updates",
    "absent_mixer_step_lanes"
  ],
  "preserved_unknown_domains": [
    "mpe",
    "follow_actions",
    "existing_envelopes",
    "devices"
  ],
  "preview_fingerprint": "075133e8c2da6c6a9d3cc983d000e479ab3b1e39c791e03db9127d9892fdecc3"
})JSON");
}
inline nlohmann::json acknowledgement() {
    return nlohmann::json::parse(R"JSON({
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
  "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
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
  "adoption": {
    "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "preview_fingerprint": "075133e8c2da6c6a9d3cc983d000e479ab3b1e39c791e03db9127d9892fdecc3",
    "authority_origin": "explicit_adoption",
    "historical_identity_proven": false,
    "allowed_domains": [
      "existing_note_updates",
      "note_population_updates",
      "absent_mixer_step_lanes"
    ],
    "preserved_unknown_domains": [
      "mpe",
      "follow_actions",
      "existing_envelopes",
      "devices"
    ],
    "approved_note_ids": [
      41,
      99
    ],
    "devices_preserved": true,
    "preview_metadata": {
      "schema_version": 1,
      "outcome": "previewed",
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "selector": {
        "track_index": 0,
        "slot_index": 0
      },
      "set_info": {
        "file_path": null,
        "name": null
      },
      "authority_origin": "none",
      "historical_identity_proven": false,
      "allowed_domains": [
        "existing_note_updates",
        "note_population_updates",
        "absent_mixer_step_lanes"
      ],
      "preserved_unknown_domains": [
        "mpe",
        "follow_actions",
        "existing_envelopes",
        "devices"
      ]
    }
  }
})JSON");
}
inline nlohmann::json owned_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "context": {
    "bridge_instance": "bridge_a",
    "document_token": "document_a"
  },
  "project_key": "project_a",
  "binding_key": "part_a",
  "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
  "selector": {
    "track_index": 0,
    "slot_index": 0
  },
  "set_info": {
    "file_path": null,
    "name": null
  },
  "observation": {
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
        "is_grouped": false,
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
          "velocity": 12.0,
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
    "content_fingerprint": "70129c0a18c50fdfa1d4250bd3210bcb787dbbf5d3be5f21cec6818a0cb1d2a4",
    "note_identity": {
      "entire_clip_population_observed": true,
      "notes": [
        {
          "note_id": 41,
          "pitch": 60,
          "start_time": 0.0,
          "duration": 1.0,
          "velocity": 12.0,
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
    "note_identity_fingerprint": "e95d2757fcd43c0c68076abc64e983421a43f45243bbe97165f1da3d0c6d3fa2",
    "track_tag": "Sunny|project_a|part_a|track",
    "clip_tag": "Sunny|project_a|part_a|clip",
    "structural_boundary_complete": false,
    "content_boundary_complete": false,
    "unavailable_reasons": [
      "Native devices/racks are outside this initial complete manifest",
      "MpeExpressionUnavailable: per-note expression fields were not observed",
      "FollowActionsUnavailable: Follow Action settings were not observed"
    ],
    "device_identity": {
      "schema_version": 1,
      "cohort": [
        {
          "device_key": "source",
          "browser_name": "Drift",
          "class_name": "Drift",
          "class_display_name": "Drift",
          "name": "Drift",
          "type": 1,
          "role": "source",
          "is_active": true,
          "can_have_chains": false,
          "parameters": [
            {
              "name": "Device On",
              "original_name": "Device On",
              "descriptor": {
                "minimum": 0.0,
                "maximum": 1.0,
                "value": 1.0,
                "default_value": null,
                "is_quantized": true,
                "is_enabled": true,
                "state": 0,
                "automation_state": 0,
                "value_items": [
                  "Off",
                  "On"
                ]
              }
            },
            {
              "name": "LP Freq",
              "original_name": "LP Freq",
              "descriptor": {
                "minimum": 0.0,
                "maximum": 1.0,
                "value": 0.0,
                "default_value": 0.0,
                "is_quantized": false,
                "is_enabled": true,
                "state": 0,
                "automation_state": 0,
                "value_items": []
              }
            }
          ],
          "modes": {
            "voice_mode": {
              "index": 1,
              "value_items": [
                "Mono",
                "Poly",
                "Unison",
                "Stereo"
              ],
              "label": "Poly"
            },
            "voice_count": {
              "index": 2,
              "value_items": [
                "1",
                "2",
                "8",
                "32"
              ],
              "label": "8"
            }
          }
        }
      ],
      "opaque_state_observed": false
    },
    "device_identity_fingerprint": "600d2c412d98397480551b9a896c35dd2c8f85dde5ea011d248b6c4df5388470"
  },
  "authority_origin": "none",
  "historical_identity_proven": false,
  "allowed_domains": [
    "existing_note_updates",
    "note_population_updates",
    "absent_mixer_step_lanes"
  ],
  "preserved_unknown_domains": [
    "mpe",
    "follow_actions",
    "existing_envelopes",
    "devices"
  ],
  "preview_fingerprint": "314813954dac7716a3f62ebddc077cdcbdd16d98217e79120cb7dbe5c5587918"
})JSON");
}
inline nlohmann::json owned_acknowledgement() {
    return nlohmann::json::parse(R"JSON({
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
      "is_grouped": false,
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
        "velocity": 12.0,
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
  "content_fingerprint": "70129c0a18c50fdfa1d4250bd3210bcb787dbbf5d3be5f21cec6818a0cb1d2a4",
  "note_identity": {
    "entire_clip_population_observed": true,
    "notes": [
      {
        "note_id": 41,
        "pitch": 60,
        "start_time": 0.0,
        "duration": 1.0,
        "velocity": 12.0,
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
  "note_identity_fingerprint": "e95d2757fcd43c0c68076abc64e983421a43f45243bbe97165f1da3d0c6d3fa2",
  "track_tag": "Sunny|project_a|part_a|track",
  "clip_tag": "Sunny|project_a|part_a|clip",
  "structural_boundary_complete": false,
  "content_boundary_complete": false,
  "unavailable_reasons": [
    "Native devices/racks are outside this initial complete manifest",
    "MpeExpressionUnavailable: per-note expression fields were not observed",
    "FollowActionsUnavailable: Follow Action settings were not observed"
  ],
  "device_identity": {
    "schema_version": 1,
    "cohort": [
      {
        "device_key": "source",
        "browser_name": "Drift",
        "class_name": "Drift",
        "class_display_name": "Drift",
        "name": "Drift",
        "type": 1,
        "role": "source",
        "is_active": true,
        "can_have_chains": false,
        "parameters": [
          {
            "name": "Device On",
            "original_name": "Device On",
            "descriptor": {
              "minimum": 0.0,
              "maximum": 1.0,
              "value": 1.0,
              "default_value": null,
              "is_quantized": true,
              "is_enabled": true,
              "state": 0,
              "automation_state": 0,
              "value_items": [
                "Off",
                "On"
              ]
            }
          },
          {
            "name": "LP Freq",
            "original_name": "LP Freq",
            "descriptor": {
              "minimum": 0.0,
              "maximum": 1.0,
              "value": 0.0,
              "default_value": 0.0,
              "is_quantized": false,
              "is_enabled": true,
              "state": 0,
              "automation_state": 0,
              "value_items": []
            }
          }
        ],
        "modes": {
          "voice_mode": {
            "index": 1,
            "value_items": [
              "Mono",
              "Poly",
              "Unison",
              "Stereo"
            ],
            "label": "Poly"
          },
          "voice_count": {
            "index": 2,
            "value_items": [
              "1",
              "2",
              "8",
              "32"
            ],
            "label": "8"
          }
        }
      }
    ],
    "opaque_state_observed": false
  },
  "device_identity_fingerprint": "600d2c412d98397480551b9a896c35dd2c8f85dde5ea011d248b6c4df5388470",
  "adoption": {
    "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "preview_fingerprint": "314813954dac7716a3f62ebddc077cdcbdd16d98217e79120cb7dbe5c5587918",
    "authority_origin": "explicit_adoption",
    "historical_identity_proven": false,
    "allowed_domains": [
      "existing_note_updates",
      "note_population_updates",
      "absent_mixer_step_lanes"
    ],
    "preserved_unknown_domains": [
      "mpe",
      "follow_actions",
      "existing_envelopes",
      "devices"
    ],
    "approved_note_ids": [
      41,
      99
    ],
    "devices_preserved": true,
    "preview_metadata": {
      "schema_version": 1,
      "outcome": "previewed",
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "selector": {
        "track_index": 0,
        "slot_index": 0
      },
      "set_info": {
        "file_path": null,
        "name": null
      },
      "authority_origin": "none",
      "historical_identity_proven": false,
      "allowed_domains": [
        "existing_note_updates",
        "note_population_updates",
        "absent_mixer_step_lanes"
      ],
      "preserved_unknown_domains": [
        "mpe",
        "follow_actions",
        "existing_envelopes",
        "devices"
      ]
    }
  }
})JSON");
}
} // namespace recovery_test_fixture
