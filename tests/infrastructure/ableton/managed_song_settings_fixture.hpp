#pragma once
#include <nlohmann/json.hpp>
namespace song_settings_test_fixture {
inline nlohmann::json normal_result() {
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
  "song_settings": {
    "approved_preview": {
      "schema_version": 1,
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "00000000000000000000000000000001",
      "desired": {
        "tempo": 92.5,
        "signature_numerator": 3,
        "signature_denominator": 8
      },
      "before": {
        "settings": {
          "signature_numerator": 4,
          "signature_denominator": 4,
          "tempo": 120.0
        },
        "current_song_time": 0.0,
        "loop_start": 0.0,
        "loop_length": 8.0,
        "flags": {
          "is_playing": false,
          "is_counting_in": false,
          "record_mode": false,
          "session_record": false,
          "session_automation_record": false,
          "arrangement_overdub": false,
          "overdub": false,
          "is_ableton_link_enabled": false,
          "is_ableton_link_start_stop_sync_enabled": false,
          "tempo_follower_enabled": false,
          "nudge_down": false,
          "nudge_up": false,
          "back_to_arranger": false,
          "re_enable_automation_enabled": false,
          "loop": false,
          "metronome": false
        },
        "tempo_parameter": {
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        },
        "tracks": [
          {
            "index": 0,
            "name": "Sunny|project_a|part_a|track",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          },
          {
            "index": 1,
            "name": "User strings",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Foreign audio idea",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          }
        ],
        "return_tracks": [
          {
            "index": 0,
            "name": "A-Return"
          }
        ],
        "master_track": {
          "name": "Main"
        },
        "scenes": [
          {
            "name": "User scene",
            "is_triggered": false,
            "tempo_enabled": true,
            "tempo": 150.0,
            "time_signature_enabled": true,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8
          }
        ],
        "cue_points": [
          {
            "name": "User cue",
            "time": 4.0
          }
        ]
      },
      "binding_guard": {
        "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
        "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
      },
      "scope": {
        "set_wide": true,
        "all_tracks_affected": true,
        "historical_song_identity_proven": false,
        "scene_overrides_preserved": true
      },
      "unavailable_domains": [
        "arrangement_tempo_envelope_population",
        "arrangement_time_signature_markers",
        "unrelated_track_internal_content"
      ]
    },
    "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "after": {
      "settings": {
        "signature_numerator": 3,
        "signature_denominator": 8,
        "tempo": 92.5
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "started_fields": [
      "signature_numerator",
      "signature_denominator",
      "tempo"
    ],
    "returned_fields": [
      "signature_numerator",
      "signature_denominator",
      "tempo"
    ],
    "authority_scope": "explicit_current_set_settings",
    "historical_song_identity_proven": false,
    "desired_settings_match": true,
    "observed_untouched_state_preserved": true,
    "clip_and_note_ids_preserved": true
  }
})JSON");
}
inline nlohmann::json normal_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "preview_token": "00000000000000000000000000000001",
  "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "00000000000000000000000000000001",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "binding_guard": {
      "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
      "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "scope": {
      "set_wide": true,
      "all_tracks_affected": true,
      "historical_song_identity_proven": false,
      "scene_overrides_preserved": true
    },
    "unavailable_domains": [
      "arrangement_tempo_envelope_population",
      "arrangement_time_signature_markers",
      "unrelated_track_internal_content"
    ]
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
  }
})JSON");
}
inline nlohmann::json noop_result() {
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
  "song_settings": {
    "approved_preview": {
      "schema_version": 1,
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "00000000000000000000000000000001",
      "desired": {
        "tempo": 120,
        "signature_numerator": 4,
        "signature_denominator": 4
      },
      "before": {
        "settings": {
          "signature_numerator": 4,
          "signature_denominator": 4,
          "tempo": 120.0
        },
        "current_song_time": 0.0,
        "loop_start": 0.0,
        "loop_length": 8.0,
        "flags": {
          "is_playing": false,
          "is_counting_in": false,
          "record_mode": false,
          "session_record": false,
          "session_automation_record": false,
          "arrangement_overdub": false,
          "overdub": false,
          "is_ableton_link_enabled": false,
          "is_ableton_link_start_stop_sync_enabled": false,
          "tempo_follower_enabled": false,
          "nudge_down": false,
          "nudge_up": false,
          "back_to_arranger": false,
          "re_enable_automation_enabled": false,
          "loop": false,
          "metronome": false
        },
        "tempo_parameter": {
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        },
        "tracks": [
          {
            "index": 0,
            "name": "Sunny|project_a|part_a|track",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          },
          {
            "index": 1,
            "name": "User strings",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Foreign audio idea",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          }
        ],
        "return_tracks": [
          {
            "index": 0,
            "name": "A-Return"
          }
        ],
        "master_track": {
          "name": "Main"
        },
        "scenes": [
          {
            "name": "User scene",
            "is_triggered": false,
            "tempo_enabled": true,
            "tempo": 150.0,
            "time_signature_enabled": true,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8
          }
        ],
        "cue_points": [
          {
            "name": "User cue",
            "time": 4.0
          }
        ]
      },
      "binding_guard": {
        "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
        "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
      },
      "scope": {
        "set_wide": true,
        "all_tracks_affected": true,
        "historical_song_identity_proven": false,
        "scene_overrides_preserved": true
      },
      "unavailable_domains": [
        "arrangement_tempo_envelope_population",
        "arrangement_time_signature_markers",
        "unrelated_track_internal_content"
      ]
    },
    "preview_fingerprint": "6d0e1b21bb901639814a124a2441d171a93c781563432104e97d68981282778c",
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "after": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "desired": {
      "tempo": 120,
      "signature_numerator": 4,
      "signature_denominator": 4
    },
    "started_fields": [],
    "returned_fields": [],
    "authority_scope": "explicit_current_set_settings",
    "historical_song_identity_proven": false,
    "desired_settings_match": true,
    "observed_untouched_state_preserved": true,
    "clip_and_note_ids_preserved": true
  }
})JSON");
}
inline nlohmann::json noop_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "preview_token": "00000000000000000000000000000001",
  "preview_fingerprint": "6d0e1b21bb901639814a124a2441d171a93c781563432104e97d68981282778c",
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "00000000000000000000000000000001",
    "desired": {
      "tempo": 120,
      "signature_numerator": 4,
      "signature_denominator": 4
    },
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "binding_guard": {
      "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
      "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "scope": {
      "set_wide": true,
      "all_tracks_affected": true,
      "historical_song_identity_proven": false,
      "scene_overrides_preserved": true
    },
    "unavailable_domains": [
      "arrangement_tempo_envelope_population",
      "arrangement_time_signature_markers",
      "unrelated_track_internal_content"
    ]
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
  }
})JSON");
}
inline nlohmann::json partial_journal() {
    return nlohmann::json::parse(R"JSON({
  "document_token": "document_a",
  "operation_id": "song_a",
  "name": "sunny_managed_apply_song_settings",
  "request": {
    "document_token": "document_a",
    "operation_id": "song_a",
    "project_key": "project_a",
    "binding_key": "part_a",
    "expected_content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
    "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "preview_token": "00000000000000000000000000000001",
    "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
    "approved_preview": {
      "schema_version": 1,
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "00000000000000000000000000000001",
      "desired": {
        "tempo": 92.5,
        "signature_numerator": 3,
        "signature_denominator": 8
      },
      "before": {
        "settings": {
          "signature_numerator": 4,
          "signature_denominator": 4,
          "tempo": 120.0
        },
        "current_song_time": 0.0,
        "loop_start": 0.0,
        "loop_length": 8.0,
        "flags": {
          "is_playing": false,
          "is_counting_in": false,
          "record_mode": false,
          "session_record": false,
          "session_automation_record": false,
          "arrangement_overdub": false,
          "overdub": false,
          "is_ableton_link_enabled": false,
          "is_ableton_link_start_stop_sync_enabled": false,
          "tempo_follower_enabled": false,
          "nudge_down": false,
          "nudge_up": false,
          "back_to_arranger": false,
          "re_enable_automation_enabled": false,
          "loop": false,
          "metronome": false
        },
        "tempo_parameter": {
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        },
        "tracks": [
          {
            "index": 0,
            "name": "Sunny|project_a|part_a|track",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          },
          {
            "index": 1,
            "name": "User strings",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Foreign audio idea",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          }
        ],
        "return_tracks": [
          {
            "index": 0,
            "name": "A-Return"
          }
        ],
        "master_track": {
          "name": "Main"
        },
        "scenes": [
          {
            "name": "User scene",
            "is_triggered": false,
            "tempo_enabled": true,
            "tempo": 150.0,
            "time_signature_enabled": true,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8
          }
        ],
        "cue_points": [
          {
            "name": "User cue",
            "time": 4.0
          }
        ]
      },
      "binding_guard": {
        "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
        "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
      },
      "scope": {
        "set_wide": true,
        "all_tracks_affected": true,
        "historical_song_identity_proven": false,
        "scene_overrides_preserved": true
      },
      "unavailable_domains": [
        "arrangement_tempo_envelope_population",
        "arrangement_time_signature_markers",
        "unrelated_track_internal_content"
      ]
    },
    "explicit_set_wide_approval": true
  },
  "request_fingerprint": "452ee286577edac023d52bdcd58d0d83c01600ff6f7ee9353ea797fb26d71127",
  "outcome": "pending",
  "native_mutation_started": true,
  "song_settings_progress": {
    "started_fields": [
      "signature_numerator",
      "signature_denominator"
    ],
    "returned_fields": [
      "signature_numerator"
    ]
  },
  "song_settings_partial": {
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "after": {
      "settings": {
        "signature_numerator": 3,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "started_fields": [
      "signature_numerator",
      "signature_denominator"
    ],
    "returned_fields": [
      "signature_numerator"
    ]
  }
})JSON");
}
inline nlohmann::json partial_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "preview_token": "00000000000000000000000000000001",
  "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "00000000000000000000000000000001",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "binding_guard": {
      "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
      "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "scope": {
      "set_wide": true,
      "all_tracks_affected": true,
      "historical_song_identity_proven": false,
      "scene_overrides_preserved": true
    },
    "unavailable_domains": [
      "arrangement_tempo_envelope_population",
      "arrangement_time_signature_markers",
      "unrelated_track_internal_content"
    ]
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
  }
})JSON");
}
inline nlohmann::json clamped_result() {
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
  "song_settings": {
    "approved_preview": {
      "schema_version": 1,
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "00000000000000000000000000000001",
      "desired": {
        "tempo": 92.5,
        "signature_numerator": 3,
        "signature_denominator": 8
      },
      "before": {
        "settings": {
          "signature_numerator": 4,
          "signature_denominator": 4,
          "tempo": 120.0
        },
        "current_song_time": 0.0,
        "loop_start": 0.0,
        "loop_length": 8.0,
        "flags": {
          "is_playing": false,
          "is_counting_in": false,
          "record_mode": false,
          "session_record": false,
          "session_automation_record": false,
          "arrangement_overdub": false,
          "overdub": false,
          "is_ableton_link_enabled": false,
          "is_ableton_link_start_stop_sync_enabled": false,
          "tempo_follower_enabled": false,
          "nudge_down": false,
          "nudge_up": false,
          "back_to_arranger": false,
          "re_enable_automation_enabled": false,
          "loop": false,
          "metronome": false
        },
        "tempo_parameter": {
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        },
        "tracks": [
          {
            "index": 0,
            "name": "Sunny|project_a|part_a|track",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          },
          {
            "index": 1,
            "name": "User strings",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Foreign audio idea",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          }
        ],
        "return_tracks": [
          {
            "index": 0,
            "name": "A-Return"
          }
        ],
        "master_track": {
          "name": "Main"
        },
        "scenes": [
          {
            "name": "User scene",
            "is_triggered": false,
            "tempo_enabled": true,
            "tempo": 150.0,
            "time_signature_enabled": true,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8
          }
        ],
        "cue_points": [
          {
            "name": "User cue",
            "time": 4.0
          }
        ]
      },
      "binding_guard": {
        "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
        "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
      },
      "scope": {
        "set_wide": true,
        "all_tracks_affected": true,
        "historical_song_identity_proven": false,
        "scene_overrides_preserved": true
      },
      "unavailable_domains": [
        "arrangement_tempo_envelope_population",
        "arrangement_time_signature_markers",
        "unrelated_track_internal_content"
      ]
    },
    "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "after": {
      "settings": {
        "signature_numerator": 3,
        "signature_denominator": 8,
        "tempo": 90.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "started_fields": [
      "signature_numerator",
      "signature_denominator",
      "tempo"
    ],
    "returned_fields": [
      "signature_numerator",
      "signature_denominator",
      "tempo"
    ],
    "authority_scope": "explicit_current_set_settings",
    "historical_song_identity_proven": false,
    "desired_settings_match": false,
    "observed_untouched_state_preserved": true,
    "clip_and_note_ids_preserved": true
  }
})JSON");
}
inline nlohmann::json clamped_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "preview_token": "00000000000000000000000000000001",
  "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "00000000000000000000000000000001",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "binding_guard": {
      "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
      "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "scope": {
      "set_wide": true,
      "all_tracks_affected": true,
      "historical_song_identity_proven": false,
      "scene_overrides_preserved": true
    },
    "unavailable_domains": [
      "arrangement_tempo_envelope_population",
      "arrangement_time_signature_markers",
      "unrelated_track_internal_content"
    ]
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
  }
})JSON");
}
inline nlohmann::json early_clamped_journal() {
    return nlohmann::json::parse(R"JSON({
  "document_token": "document_a",
  "operation_id": "song_a",
  "name": "sunny_managed_apply_song_settings",
  "request": {
    "document_token": "document_a",
    "operation_id": "song_a",
    "project_key": "project_a",
    "binding_key": "part_a",
    "expected_content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
    "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "preview_token": "00000000000000000000000000000001",
    "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
    "approved_preview": {
      "schema_version": 1,
      "context": {
        "bridge_instance": "bridge_a",
        "document_token": "document_a"
      },
      "project_key": "project_a",
      "binding_key": "part_a",
      "preview_token": "00000000000000000000000000000001",
      "desired": {
        "tempo": 92.5,
        "signature_numerator": 3,
        "signature_denominator": 8
      },
      "before": {
        "settings": {
          "signature_numerator": 4,
          "signature_denominator": 4,
          "tempo": 120.0
        },
        "current_song_time": 0.0,
        "loop_start": 0.0,
        "loop_length": 8.0,
        "flags": {
          "is_playing": false,
          "is_counting_in": false,
          "record_mode": false,
          "session_record": false,
          "session_automation_record": false,
          "arrangement_overdub": false,
          "overdub": false,
          "is_ableton_link_enabled": false,
          "is_ableton_link_start_stop_sync_enabled": false,
          "tempo_follower_enabled": false,
          "nudge_down": false,
          "nudge_up": false,
          "back_to_arranger": false,
          "re_enable_automation_enabled": false,
          "loop": false,
          "metronome": false
        },
        "tempo_parameter": {
          "is_enabled": true,
          "state": 0,
          "automation_state": 0
        },
        "tracks": [
          {
            "index": 0,
            "name": "Sunny|project_a|part_a|track",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          },
          {
            "index": 1,
            "name": "User strings",
            "clip_slots": [
              {
                "index": 0,
                "has_clip": true,
                "clip": {
                  "name": "Foreign audio idea",
                  "is_playing": false,
                  "is_recording": false,
                  "is_overdubbing": false,
                  "is_triggered": false,
                  "will_record_on_start": false
                }
              }
            ]
          }
        ],
        "return_tracks": [
          {
            "index": 0,
            "name": "A-Return"
          }
        ],
        "master_track": {
          "name": "Main"
        },
        "scenes": [
          {
            "name": "User scene",
            "is_triggered": false,
            "tempo_enabled": true,
            "tempo": 150.0,
            "time_signature_enabled": true,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8
          }
        ],
        "cue_points": [
          {
            "name": "User cue",
            "time": 4.0
          }
        ]
      },
      "binding_guard": {
        "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
        "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
      },
      "scope": {
        "set_wide": true,
        "all_tracks_affected": true,
        "historical_song_identity_proven": false,
        "scene_overrides_preserved": true
      },
      "unavailable_domains": [
        "arrangement_tempo_envelope_population",
        "arrangement_time_signature_markers",
        "unrelated_track_internal_content"
      ]
    },
    "explicit_set_wide_approval": true
  },
  "request_fingerprint": "452ee286577edac023d52bdcd58d0d83c01600ff6f7ee9353ea797fb26d71127",
  "outcome": "pending",
  "native_mutation_started": true,
  "song_settings_progress": {
    "started_fields": [
      "signature_numerator"
    ],
    "returned_fields": [
      "signature_numerator"
    ]
  },
  "song_settings_partial": {
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "after": {
      "settings": {
        "signature_numerator": 2,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "started_fields": [
      "signature_numerator"
    ],
    "returned_fields": [
      "signature_numerator"
    ]
  }
})JSON");
}
inline nlohmann::json early_clamped_preview() {
    return nlohmann::json::parse(R"JSON({
  "schema_version": 1,
  "outcome": "previewed",
  "preview_token": "00000000000000000000000000000001",
  "preview_fingerprint": "886056ab789517064c2d18b341f81e465b99583b5478243586e51700780298ff",
  "preview": {
    "schema_version": 1,
    "context": {
      "bridge_instance": "bridge_a",
      "document_token": "document_a"
    },
    "project_key": "project_a",
    "binding_key": "part_a",
    "preview_token": "00000000000000000000000000000001",
    "desired": {
      "tempo": 92.5,
      "signature_numerator": 3,
      "signature_denominator": 8
    },
    "before": {
      "settings": {
        "signature_numerator": 4,
        "signature_denominator": 4,
        "tempo": 120.0
      },
      "current_song_time": 0.0,
      "loop_start": 0.0,
      "loop_length": 8.0,
      "flags": {
        "is_playing": false,
        "is_counting_in": false,
        "record_mode": false,
        "session_record": false,
        "session_automation_record": false,
        "arrangement_overdub": false,
        "overdub": false,
        "is_ableton_link_enabled": false,
        "is_ableton_link_start_stop_sync_enabled": false,
        "tempo_follower_enabled": false,
        "nudge_down": false,
        "nudge_up": false,
        "back_to_arranger": false,
        "re_enable_automation_enabled": false,
        "loop": false,
        "metronome": false
      },
      "tempo_parameter": {
        "is_enabled": true,
        "state": 0,
        "automation_state": 0
      },
      "tracks": [
        {
          "index": 0,
          "name": "Sunny|project_a|part_a|track",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Sunny|project_a|part_a|clip",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        },
        {
          "index": 1,
          "name": "User strings",
          "clip_slots": [
            {
              "index": 0,
              "has_clip": true,
              "clip": {
                "name": "Foreign audio idea",
                "is_playing": false,
                "is_recording": false,
                "is_overdubbing": false,
                "is_triggered": false,
                "will_record_on_start": false
              }
            }
          ]
        }
      ],
      "return_tracks": [
        {
          "index": 0,
          "name": "A-Return"
        }
      ],
      "master_track": {
        "name": "Main"
      },
      "scenes": [
        {
          "name": "User scene",
          "is_triggered": false,
          "tempo_enabled": true,
          "tempo": 150.0,
          "time_signature_enabled": true,
          "time_signature_numerator": 7,
          "time_signature_denominator": 8
        }
      ],
      "cue_points": [
        {
          "name": "User cue",
          "time": 4.0
        }
      ]
    },
    "binding_guard": {
      "content_fingerprint": "89798824caa96c62e26e10189eae25d325396d8fd8d1ac490058ff40dbd295bb",
      "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
    },
    "scope": {
      "set_wide": true,
      "all_tracks_affected": true,
      "historical_song_identity_proven": false,
      "scene_overrides_preserved": true
    },
    "unavailable_domains": [
      "arrangement_tempo_envelope_population",
      "arrangement_time_signature_markers",
      "unrelated_track_internal_content"
    ]
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
  }
})JSON");
}
} // namespace song_settings_test_fixture
