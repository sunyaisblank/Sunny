// Literal JSON emitted by staged ManagedRouting on the independent native model.
#pragma once
#include <nlohmann/json.hpp>
#include <string>
inline nlohmann::json managed_routing_fixture() {
    return nlohmann::json::parse(
        (std::string{} + R"SUNNYROUTE({
  "create": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "create_return",
        "aux_key": "aux_room"
      }
    },
    "preview": {
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "52ba4de7a6e3ea0dbc5449f79d9cf30871093595d6659690910adc1327f49bdf",
      "preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "1434510153566565381",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "Sunny|project_a|aux_room|return"
        },
        "scope": {
          "affects_send_population": true,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
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
        "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
          "MpeExpressionUnavailable: per-note expression fields were not observed",
          "FollowActionsUnavailable: Follow Action settings were not observed"
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "create_return",
        "aux_key": "aux_room"
      },
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "52ba4de7a6e3ea0dbc5449f79d9cf30871093595d6659690910adc1327f49bdf",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                   )SUNNYROUTE" +
         R"SUNNYROUTE( "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "1434510153566565381",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "Sunny|project_a|aux_room|return"
        },
        "scope": {
          "affects_send_population": true,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
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
            },
            {
              "name": "Send B",
              "original_name": "Send B",
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
      "content_fingerprint": "8804f9504251a1e37f651cbd49d156e23e4a5a64fca8c86990671f2cf5753904",
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
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "routing": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "create_return",
            "aux_key": "aux_room"
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "1434510153566565381",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
                  "has_audio_output": true,
                  "has_midi_output": false
                },
                "clip": {
                  "name": "Sunny|project_a|part_a|clip",
            )SUNNYROUTE" +
         R"SUNNYROUTE(      "signature_numerator": 4,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "Sunny|project_a|aux_room|return"
          },
          "scope": {
            "affects_send_population": true,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
              }
            ],
            "return_tracks": [
              {
                "index": 0,
                "name": "A-Return"
              },
              {
                "index": 1,
                "name": "Sunny|project_a|aux_room|return"
              }
            ],
            "master_track": {
              "name": "Main"
            },
            "scenes": [
              {
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                  },
                  {
                    "name": "Send B",
                    "original_name": "Send B",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "1434510153566565381",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  },
                  {
                    "display_name": "Sunny|project_a|aux_room|return",
                    "identifier": "-2501242815813853499",
                    "attached_target": {
                      "kind": "return",
                      "index": 1
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
                    "value": 0.0,
                    "min": 0.0,
                    "max": 1.0,
                    "is_quantized": false,
                    "is_enabled": true,
                    "state": 0,
                    "automation_state": 0
                  },
                  {
                    "name": "New return send",
                    "original_name": "New return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "Sunny|project_a|aux_room|return",
                    "identifier": "-2501242815813853499",
                    "attached_target": {
                      "kind": "return",
                      "index": 1
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 1,
              "name": "Sunny|project_a|aux_room|return",
              "mute": false,
              "solo": false,
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
                    "name": "New return send",
                    "original_name": "New return send",
                    "value": 0.0,
                    "min": 0.0,
                    "max": 1.0,
                    "is_quantized": false,
                    "is_enabled": true,
                    "state": 0,
                    "automation_state": 0
                  },
                  {
                    "name": "New return send",
                    "original_name": "New return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "1434510153566565381",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [
            "create_return_track",
            "return_name"
          ],
          "returned": [
            "create_return_track",
            "return_name"
          ]
        },
        "desired_match": true,
        "untouched_observed_state_preserved": true,
        "affected_observations": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
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
                    },
                    {
                      "name": "Send B",
                      "original_name": "Send B",
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
              "content_fingerprint": "8804f9504251a1e37f651cbd49d156e23e4a5a64fca8c86990671f2cf5753904",
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
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed"
              ]
            }
          }
        ],
        "authority_origin": "explicit_current_routing",
        "tap_policy_observed": false,
        "logical_send_complete": false,
        "send_readback": null
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
        "expected_note_identity_fingerpr)SUNNYROUTE" +
         R"SUNNYROUTE(int": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "preview_fingerprint": "52ba4de7a6e3ea0dbc5449f79d9cf30871093595d6659690910adc1327f49bdf",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "create_return",
            "aux_key": "aux_room"
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "1434510153566565381",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "Sunny|project_a|aux_room|return"
          },
          "scope": {
            "affects_send_population": true,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "2feb34d0874bbc85a349122451b2756d8eae87e05920242d2b29353705feb08e",
      "outcome": "pending",
      "native_mutation_started": true,
      "routing_progress": {
        "started": [
          "create_return_track",
          "return_name"
        ],
        "returned": [
          "create_return_track",
          "return_name"
        ]
      },
      "partial_return_index": 1
    }
  },
  "adopt": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "adopt_return",
        "aux_key": "aux_room",
        "return_index": 0
      }
    },
    "preview": {
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "e25c42a60dd00bce93f34eaee6b3f58b33db428209290507b828cf5b8258e4d0",
      "preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "adopt_return",
          "aux_key": "aux_room",
          "return_index": 0
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-2746699956865951664",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": ")SUNNYROUTE" +
         R"SUNNYROUTE(b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "A-Return"
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
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
        "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
          "MpeExpressionUnavailable: per-note expression fields were not observed",
          "FollowActionsUnavailable: Follow Action settings were not observed"
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "adopt_return",
        "aux_key": "aux_room",
        "return_index": 0
      },
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "e25c42a60dd00bce93f34eaee6b3f58b33db428209290507b828cf5b8258e4d0",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "adopt_return",
          "aux_key": "aux_room",
          "return_index": 0
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-2746699956865951664",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "A-Return"
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
      "track_index)SUNNYROUTE" +
         R"SUNNYROUTE(": 0,
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
      "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "routing": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "adopt_return",
            "aux_key": "aux_room",
            "return_index": 0
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-2746699956865951664",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "A-Return"
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-2746699956865951664",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
      )SUNNYROUTE" +
         R"SUNNYROUTE(        "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [],
          "returned": []
        },
        "desired_match": true,
        "untouched_observed_state_preserved": true,
        "affected_observations": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
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
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed"
              ]
            }
          }
        ],
        "authority_origin": "explicit_current_adoption",
        "tap_policy_observed": false,
        "logical_send_complete": false,
        "send_readback": null
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
        "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "adopt_return",
          "aux_key": "aux_room",
          "return_index": 0
        },
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "preview_fingerprint": "e25c42a60dd00bce93f34eaee6b3f58b33db428209290507b828cf5b8258e4d0",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "adopt_return",
            "aux_key": "aux_room",
            "return_index": 0
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-2746699956865951664",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "A-Return"
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identit)SUNNYROUTE" +
         R"SUNNYROUTE(y_proven": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "68379488658e2ad137eae2552d303d4f926544fe65002cb727e94f46e6c234da",
      "outcome": "pending",
      "native_mutation_started": false,
      "routing_progress": {
        "started": [],
        "returned": []
      }
    }
  },
  "output": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "output_type",
        "destination": "main",
        "group_key": null,
        "route_identifier": "-8217667638299348433"
      }
    },
    "preview": {
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "1d4b7ebbabfb5d0758885f64d78f6c40f1d3e018b67e7d09b05563d90b6e5ae5",
      "preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "output_type",
          "destination": "main",
          "group_key": null,
          "route_identifier": "-8217667638299348433"
        },
        "binding_guard": {
          "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "A-Return",
                  "identifier": "-3935431777095434482",
                  "attached_target": {
                    "kind": "return",
                    "index": 0
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "1887946717441805270",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-3935431777095434482",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "1887946717441805270",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
                  "display_name": "A-Return",
                  "identifier": "3:A-Return"
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "route": {
            "display_name": "Master",
            "identifier": "-8217667638299348433",
            "attached_target": {
              "kind": "main",
              "index": null
            }
          }
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
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
              "display_name": "A-Return",
              "identifier": "3:A-Return"
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
        "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
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
          "MpeExpressionUnavailable: per-note expression fields were not observed",
          "FollowActionsUnavailable: Follow Action settings were not observed"
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "output_type",
        "destination": "main",
        "group_key": null,
        "route_identifier": "-8217667638299348433"
      },
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "1d4b7ebbabfb5d0758885f64d78f6c40f1d3e018b67e7d09b05563d90b6e5ae5",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "output_type",
          "destination": "main",
          "group_key": null,
          "route_identifier": "-8217667638299348433"
        },
        "binding_guard": {
          "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
         )SUNNYROUTE" +
         R"SUNNYROUTE(         "max": 1.0,
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
                "output_type": {
                  "display_name": "A-Return",
                  "identifier": "-3935431777095434482",
                  "attached_target": {
                    "kind": "return",
                    "index": 0
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "1887946717441805270",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-3935431777095434482",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "1887946717441805270",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
                  "display_name": "A-Return",
                  "identifier": "3:A-Return"
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "route": {
            "display_name": "Master",
            "identifier": "-8217667638299348433",
            "attached_target": {
              "kind": "main",
              "index": null
            }
          }
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
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
      "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "routing": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "output_type",
            "destination": "main",
            "group_key": null,
            "route_identifier": "-8217667638299348433"
          },
          "binding_guard": {
            "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "A-Return",
                    "identifier": "-3935431777095434482",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "1887946717441805270",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-3935431777095434482",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "1887946717441805270",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint":)SUNNYROUTE" +
         R"SUNNYROUTE( null
              },
              "track_index": 0,
              "slot_index": 0,
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
                    "display_name": "A-Return",
                    "identifier": "3:A-Return"
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "route": {
              "display_name": "Master",
              "identifier": "-8217667638299348433",
              "attached_target": {
                "kind": "main",
                "index": null
              }
            }
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-3935431777095434482",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [
            "output_routing_type"
          ],
          "returned": [
            "output_routing_type"
          ]
        },
        "desired_match": true,
        "untouched_observed_state_preserved": true,
        "affected_observations": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
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
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed"
              ]
            }
          }
        ],
        "authority_origin": "explicit_current_routing",
        "tap_policy_observed": false,
        "logical_send_complete": false,
        "send_readback": null
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
        "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "output_type",
          "destination": "main",
          "group_key": null,
          "route_identifier": "-8217667638299348433"
        },
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "preview_fingerprint": "1d4b7ebbabfb5d0758885f64d78f6c40f1d3e018b67e7d09b05563d90b6e5ae5",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "output_type",
            "destination": "main",
            "group_key": null,
            "route_identifier": "-8217667638299348433"
          },
          "binding_guard": {
            "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
 )SUNNYROUTE" +
         R"SUNNYROUTE(                   "original_name": "Speaker On",
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
                  "output_type": {
                    "display_name": "A-Return",
                    "identifier": "-3935431777095434482",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "1887946717441805270",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-3935431777095434482",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "1887946717441805270",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "afb2a082ee03a4179fa612119f64f9032629883ddf9d6bacc685f01b7a3c7d18",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
                    "display_name": "A-Return",
                    "identifier": "3:A-Return"
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "route": {
              "display_name": "Master",
              "identifier": "-8217667638299348433",
              "attached_target": {
                "kind": "main",
                "index": null
              }
            }
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "f9901f4a043a520364afd75e229dda7b02f603381efe144221f4525b482fbd60",
      "outcome": "pending",
      "native_mutation_started": true,
      "routing_progress": {
        "started": [
          "output_routing_type"
        ],
        "returned": [
          "output_routing_type"
        ]
      }
    }
  },
  "send": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": false
      }
    },
    "preview": {
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "ce64cd6029a9efd8162c8207a4e79b96dca8298c62ae55b6ca47e41a69ca74c9",
      "preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "send_level",
          "aux_key": "aux_room",
          "level_db": -24.0,
          "tolerance_db": 0.0,
          "requested_pre_fader": false
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-5522318779816046603",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
           )SUNNYROUTE" +
         R"SUNNYROUTE(     },
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_index": 0,
          "return_tag": "A-Return",
          "candidate": {
            "schema_version": 1,
            "unit": "Decibels",
            "target": -24.0,
            "display_tolerance": 0.0,
            "internal_value": 0.25,
            "display": "-24.000000000000 dB",
            "display_value": -24.0,
            "display_increment": 1e-12,
            "absolute_display_error": 0.0,
            "balance_full_scale": null,
            "descriptor": {
              "minimum": 0.0,
              "maximum": 1.0,
              "value": 0.0,
              "is_quantized": false,
              "is_enabled": true,
              "state": 0,
              "automation_state": 0,
              "default_value": 0.0
            },
            "formatter_calls": 20,
            "samples": [
              {
                "internal_value": 0.0,
                "display": "-48.000000000000 dB",
                "phase": "grid",
                "display_value": -48.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.0625,
                "display": "-36.000000000000 dB",
                "phase": "grid",
                "display_value": -36.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.125,
                "display": "-31.029437251523 dB",
                "phase": "grid",
                "display_value": -31.029437251523,
                "negative_infinity": false
              },
              {
                "internal_value": 0.1875,
                "display": "-27.215390309173 dB",
                "phase": "grid",
                "display_value": -27.215390309173,
                "negative_infinity": false
              },
              {
                "internal_value": 0.25,
                "display": "-24.000000000000 dB",
                "phase": "grid",
                "display_value": -24.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.3125,
                "display": "-21.167184270003 dB",
                "phase": "grid",
                "display_value": -21.167184270003,
                "negative_infinity": false
              },
              {
                "internal_value": 0.375,
                "display": "-18.606123086602 dB",
                "phase": "grid",
                "display_value": -18.606123086602,
                "negative_infinity": false
              },
              {
                "internal_value": 0.4375,
                "display": "-16.250984267225 dB",
                "phase": "grid",
                "display_value": -16.250984267225,
                "negative_infinity": false
              },
              {
                "internal_value": 0.5,
                "display": "-14.058874503046 dB",
                "phase": "grid",
                "display_value": -14.058874503046,
                "negative_infinity": false
              },
              {
                "internal_value": 0.5625,
                "display": "-12.000000000000 dB",
                "phase": "grid",
                "display_value": -12.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.625,
                "display": "-10.052668077979 dB",
                "phase": "grid",
                "display_value": -10.052668077979,
                "negative_infinity": false
              },
              {
                "internal_value": 0.6875,
                "display": "-8.200502515735 dB",
                "phase": "grid",
                "display_value": -8.200502515735,
                "negative_infinity": false
              },
              {
                "internal_value": 0.75,
                "display": "-6.430780618347 dB",
                "phase": "grid",
                "display_value": -6.430780618347,
                "negative_infinity": false
              },
              {
                "internal_value": 0.8125,
                "display": "-4.733384694432 dB",
                "phase": "grid",
                "display_value": -4.733384694432,
                "negative_infinity": false
              },
              {
                "internal_value": 0.875,
                "display": "-3.100111358713 dB",
                "phase": "grid",
                "display_value": -3.100111358713,
                "negative_infinity": false
              },
              {
                "internal_value": 0.9375,
                "display": "-1.524199845511 dB",
                "phase": "grid",
                "display_value": -1.524199845511,
                "negative_infinity": false
              },
              {
                "internal_value": 1.0,
                "display": "0.000000000000 dB",
                "phase": "grid",
                "display_value": 0.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.0,
                "display": "-48.000000000000 dB",
                "phase": "repeat",
                "display_value": -48.0,
                "negative_infinity": false
              },
              {
                "internal_value": 1.0,
                "display": "0.000000000000 dB",
                "phase": "repeat",
                "display_value": 0.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.25,
                "display": "-24.000000000000 dB",
                "phase": "repeat",
                "display_value": -24.0,
                "negative_infinity": false
              }
            ],
            "_supplemental_observation": null
          },
          "tap_policy_observed": false,
          "logical_send_complete": false
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
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
        "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
          "MpeExpressionUnavailable: per-note expression fields were not observed",
          "FollowActionsUnavailable: Follow Action settings were not observed"
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": false
      },
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "ce64cd6029a9efd8162c8207a4e79b96dca8298c62ae55b6ca47e41a69ca74c9",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "send_level",
          "aux_key": "aux_room",
          "level_db": -24.0,
          "tolerance_db": 0.0,
          "requested_pre_fader": false
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-5522318779816046603",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
              "routin)SUNNYROUTE" +
         R"SUNNYROUTE(g": {
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_index": 0,
          "return_tag": "A-Return",
          "candidate": {
            "schema_version": 1,
            "unit": "Decibels",
            "target": -24.0,
            "display_tolerance": 0.0,
            "internal_value": 0.25,
            "display": "-24.000000000000 dB",
            "display_value": -24.0,
            "display_increment": 1e-12,
            "absolute_display_error": 0.0,
            "balance_full_scale": null,
            "descriptor": {
              "minimum": 0.0,
              "maximum": 1.0,
              "value": 0.0,
              "is_quantized": false,
              "is_enabled": true,
              "state": 0,
              "automation_state": 0,
              "default_value": 0.0
            },
            "formatter_calls": 20,
            "samples": [
              {
                "internal_value": 0.0,
                "display": "-48.000000000000 dB",
                "phase": "grid",
                "display_value": -48.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.0625,
                "display": "-36.000000000000 dB",
                "phase": "grid",
                "display_value": -36.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.125,
                "display": "-31.029437251523 dB",
                "phase": "grid",
                "display_value": -31.029437251523,
                "negative_infinity": false
              },
              {
                "internal_value": 0.1875,
                "display": "-27.215390309173 dB",
                "phase": "grid",
                "display_value": -27.215390309173,
                "negative_infinity": false
              },
              {
                "internal_value": 0.25,
                "display": "-24.000000000000 dB",
                "phase": "grid",
                "display_value": -24.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.3125,
                "display": "-21.167184270003 dB",
                "phase": "grid",
                "display_value": -21.167184270003,
                "negative_infinity": false
              },
              {
                "internal_value": 0.375,
                "display": "-18.606123086602 dB",
                "phase": "grid",
                "display_value": -18.606123086602,
                "negative_infinity": false
              },
              {
                "internal_value": 0.4375,
                "display": "-16.250984267225 dB",
                "phase": "grid",
                "display_value": -16.250984267225,
                "negative_infinity": false
              },
              {
                "internal_value": 0.5,
                "display": "-14.058874503046 dB",
                "phase": "grid",
                "display_value": -14.058874503046,
                "negative_infinity": false
              },
              {
                "internal_value": 0.5625,
                "display": "-12.000000000000 dB",
                "phase": "grid",
                "display_value": -12.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.625,
                "display": "-10.052668077979 dB",
                "phase": "grid",
                "display_value": -10.052668077979,
                "negative_infinity": false
              },
              {
                "internal_value": 0.6875,
                "display": "-8.200502515735 dB",
                "phase": "grid",
                "display_value": -8.200502515735,
                "negative_infinity": false
              },
              {
                "internal_value": 0.75,
                "display": "-6.430780618347 dB",
                "phase": "grid",
                "display_value": -6.430780618347,
                "negative_infinity": false
              },
              {
                "internal_value": 0.8125,
                "display": "-4.733384694432 dB",
                "phase": "grid",
                "display_value": -4.733384694432,
                "negative_infinity": false
              },
              {
                "internal_value": 0.875,
                "display": "-3.100111358713 dB",
                "phase": "grid",
                "display_value": -3.100111358713,
                "negative_infinity": false
              },
              {
                "internal_value": 0.9375,
                "display": "-1.524199845511 dB",
                "phase": "grid",
                "display_value": -1.524199845511,
                "negative_infinity": false
              },
              {
                "internal_value": 1.0,
                "display": "0.000000000000 dB",
                "phase": "grid",
                "display_value": 0.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.0,
                "display": "-48.000000000000 dB",
                "phase": "repeat",
                "display_value": -48.0,
                "negative_infinity": false
              },
              {
                "internal_value": 1.0,
                "display": "0.000000000000 dB",
                "phase": "repeat",
                "display_value": 0.0,
                "negative_infinity": false
              },
              {
                "internal_value": 0.25,
                "display": "-24.000000000000 dB",
                "phase": "repeat",
                "display_value": -24.0,
                "negative_infinity": false
              }
            ],
            "_supplemental_observation": null
          },
          "tap_policy_observed": false,
          "logical_send_complete": false
        },
        "scope": {
          "affects_send_population": false,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
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
              "value": 0.25,
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
      "content_fingerprint": "c77e051ca692b59c0ff3bc28101e73437b149a8efee3c1762da8810c625d72a6",
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
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "routing": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "send_level",
            "aux_key": "aux_room",
            "level_db": -24.0,
            "tolerance_db": 0.0,
            "requested_pre_fader": false
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-5522318779816046603",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
                )SUNNYROUTE" +
         R"SUNNYROUTE(    }
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_index": 0,
            "return_tag": "A-Return",
            "candidate": {
              "schema_version": 1,
              "unit": "Decibels",
              "target": -24.0,
              "display_tolerance": 0.0,
              "internal_value": 0.25,
              "display": "-24.000000000000 dB",
              "display_value": -24.0,
              "display_increment": 1e-12,
              "absolute_display_error": 0.0,
              "balance_full_scale": null,
              "descriptor": {
                "minimum": 0.0,
                "maximum": 1.0,
                "value": 0.0,
                "is_quantized": false,
                "is_enabled": true,
                "state": 0,
                "automation_state": 0,
                "default_value": 0.0
              },
              "formatter_calls": 20,
              "samples": [
                {
                  "internal_value": 0.0,
                  "display": "-48.000000000000 dB",
                  "phase": "grid",
                  "display_value": -48.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.0625,
                  "display": "-36.000000000000 dB",
                  "phase": "grid",
                  "display_value": -36.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.125,
                  "display": "-31.029437251523 dB",
                  "phase": "grid",
                  "display_value": -31.029437251523,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.1875,
                  "display": "-27.215390309173 dB",
                  "phase": "grid",
                  "display_value": -27.215390309173,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.25,
                  "display": "-24.000000000000 dB",
                  "phase": "grid",
                  "display_value": -24.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.3125,
                  "display": "-21.167184270003 dB",
                  "phase": "grid",
                  "display_value": -21.167184270003,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.375,
                  "display": "-18.606123086602 dB",
                  "phase": "grid",
                  "display_value": -18.606123086602,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.4375,
                  "display": "-16.250984267225 dB",
                  "phase": "grid",
                  "display_value": -16.250984267225,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.5,
                  "display": "-14.058874503046 dB",
                  "phase": "grid",
                  "display_value": -14.058874503046,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.5625,
                  "display": "-12.000000000000 dB",
                  "phase": "grid",
                  "display_value": -12.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.625,
                  "display": "-10.052668077979 dB",
                  "phase": "grid",
                  "display_value": -10.052668077979,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.6875,
                  "display": "-8.200502515735 dB",
                  "phase": "grid",
                  "display_value": -8.200502515735,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.75,
                  "display": "-6.430780618347 dB",
                  "phase": "grid",
                  "display_value": -6.430780618347,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.8125,
                  "display": "-4.733384694432 dB",
                  "phase": "grid",
                  "display_value": -4.733384694432,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.875,
                  "display": "-3.100111358713 dB",
                  "phase": "grid",
                  "display_value": -3.100111358713,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.9375,
                  "display": "-1.524199845511 dB",
                  "phase": "grid",
                  "display_value": -1.524199845511,
                  "negative_infinity": false
                },
                {
                  "internal_value": 1.0,
                  "display": "0.000000000000 dB",
                  "phase": "grid",
                  "display_value": 0.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.0,
                  "display": "-48.000000000000 dB",
                  "phase": "repeat",
                  "display_value": -48.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 1.0,
                  "display": "0.000000000000 dB",
                  "phase": "repeat",
                  "display_value": 0.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.25,
                  "display": "-24.000000000000 dB",
                  "phase": "repeat",
                  "display_value": -24.0,
                  "negative_infinity": false
                }
              ],
              "_supplemental_observation": null
            },
            "tap_policy_observed": false,
            "logical_send_complete": false
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                    "value": 0.25,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-5522318779816046603",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [
            "send_value"
          ],
          "returned": [
            "send_value"
          ]
        },
        "desired_match": true,
        "untouched_observed_state_preserved": true,
        "affected_observations": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
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
                      "value": 0.25,
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
              "content_fingerprint": "c77e051ca692b59c0ff3bc28101e73437b149a8efee3c1762da8810c625d72a6",
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
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed"
              ]
            }
          }
        ],
        "authority_origin": "explicit_current_routing",
        "tap_policy_observed": false,
        "logical_send_complete": false,
        "send_readback": {
          "internal_value": 0.25,
          "display": "-24.000000000000 dB",
          "display_value": -24.0,
          "absolute_display_error": 0.0,
          "negative_infinity": false,
          "matches_intent": true
        }
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
        "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "send_level",
          "aux_key": "aux_room",
          "level_db": -24.0,
          "tolerance_db": 0.0,
          "requested_pre_fader": false
        },
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "preview_fingerprint": "ce64cd6029a9efd8162c8207a4e79b96dca8298c62ae55b6ca47e41a69ca74c9",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "send_level",
            "aux_key": "aux_room",
            "level_db": -24.0,
            "tolerance_db": 0.0,
            "requested_pre_fader": false
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                        )SUNNYROUTE" +
         R"SUNNYROUTE("will_record_on_start": false
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-5522318779816046603",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_index": 0,
            "return_tag": "A-Return",
            "candidate": {
              "schema_version": 1,
              "unit": "Decibels",
              "target": -24.0,
              "display_tolerance": 0.0,
              "internal_value": 0.25,
              "display": "-24.000000000000 dB",
              "display_value": -24.0,
              "display_increment": 1e-12,
              "absolute_display_error": 0.0,
              "balance_full_scale": null,
              "descriptor": {
                "minimum": 0.0,
                "maximum": 1.0,
                "value": 0.0,
                "is_quantized": false,
                "is_enabled": true,
                "state": 0,
                "automation_state": 0,
                "default_value": 0.0
              },
              "formatter_calls": 20,
              "samples": [
                {
                  "internal_value": 0.0,
                  "display": "-48.000000000000 dB",
                  "phase": "grid",
                  "display_value": -48.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.0625,
                  "display": "-36.000000000000 dB",
                  "phase": "grid",
                  "display_value": -36.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.125,
                  "display": "-31.029437251523 dB",
                  "phase": "grid",
                  "display_value": -31.029437251523,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.1875,
                  "display": "-27.215390309173 dB",
                  "phase": "grid",
                  "display_value": -27.215390309173,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.25,
                  "display": "-24.000000000000 dB",
                  "phase": "grid",
                  "display_value": -24.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.3125,
                  "display": "-21.167184270003 dB",
                  "phase": "grid",
                  "display_value": -21.167184270003,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.375,
                  "display": "-18.606123086602 dB",
                  "phase": "grid",
                  "display_value": -18.606123086602,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.4375,
                  "display": "-16.250984267225 dB",
                  "phase": "grid",
                  "display_value": -16.250984267225,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.5,
                  "display": "-14.058874503046 dB",
                  "phase": "grid",
                  "display_value": -14.058874503046,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.5625,
                  "display": "-12.000000000000 dB",
                  "phase": "grid",
                  "display_value": -12.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.625,
                  "display": "-10.052668077979 dB",
                  "phase": "grid",
                  "display_value": -10.052668077979,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.6875,
                  "display": "-8.200502515735 dB",
                  "phase": "grid",
                  "display_value": -8.200502515735,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.75,
                  "display": "-6.430780618347 dB",
                  "phase": "grid",
                  "display_value": -6.430780618347,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.8125,
                  "display": "-4.733384694432 dB",
                  "phase": "grid",
                  "display_value": -4.733384694432,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.875,
                  "display": "-3.100111358713 dB",
                  "phase": "grid",
                  "display_value": -3.100111358713,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.9375,
                  "display": "-1.524199845511 dB",
                  "phase": "grid",
                  "display_value": -1.524199845511,
                  "negative_infinity": false
                },
                {
                  "internal_value": 1.0,
                  "display": "0.000000000000 dB",
                  "phase": "grid",
                  "display_value": 0.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.0,
                  "display": "-48.000000000000 dB",
                  "phase": "repeat",
                  "display_value": -48.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 1.0,
                  "display": "0.000000000000 dB",
                  "phase": "repeat",
                  "display_value": 0.0,
                  "negative_infinity": false
                },
                {
                  "internal_value": 0.25,
                  "display": "-24.000000000000 dB",
                  "phase": "repeat",
                  "display_value": -24.0,
                  "negative_infinity": false
                }
              ],
              "_supplemental_observation": null
            },
            "tap_policy_observed": false,
            "logical_send_complete": false
          },
          "scope": {
            "affects_send_population": false,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "b6af859a5fdd4148ba24aeb31160719092171621564bffc19b30868f656adc7a",
      "outcome": "pending",
      "native_mutation_started": true,
      "routing_progress": {
        "started": [
          "send_value"
        ],
        "returned": [
          "send_value"
        ]
      }
    }
  },
  "group": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "intent": {
        "kind": "adopt_group",
        "group_key": "group_strings",
        "track_index": 1,
        "member_binding_keys": [
          "part_a"
        ]
      },
      "selector": {
        "track_index": 0,
        "slot_index": 0
      }
    },
    "preview": {
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "30f78bda2f01714cd8353f17848501b801345ec2418ccb42eb4d29d5e07c518e",
      "preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "group_only": true,
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "adopt_group",
          "group_key": "group_strings",
          "track_index": 1,
          "member_binding_keys": [
            "part_a"
          ]
        },
        "selector": {
          "track_index": 0,
          "slot_index": 0
        },
        "binding_guard": {
          "content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "Current strings",
                "clip_slots": []
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
           )SUNNYROUTE" +
         R"SUNNYROUTE(         "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Current strings",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "selected": {
          "group_name": "Current strings",
          "member_count": 1,
          "members": [
            {
              "binding_key": "part_a",
              "track_index": 0,
              "name": "Sunny|project_a|part_a|track"
            }
          ]
        },
        "scope": {
          "group_membership_mutated": false,
          "historical_identity_proven": false,
          "part_authority_granted": false,
          "device_authority_granted": false
        }
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
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "adopt_group",
        "group_key": "group_strings",
        "track_index": 1,
        "member_binding_keys": [
          "part_a"
        ]
      },
      "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "preview_fingerprint": "30f78bda2f01714cd8353f17848501b801345ec2418ccb42eb4d29d5e07c518e",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "group_only": true,
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "adopt_group",
          "group_key": "group_strings",
          "track_index": 1,
          "member_binding_keys": [
            "part_a"
          ]
        },
        "selector": {
          "track_index": 0,
          "slot_index": 0
        },
        "binding_guard": {
          "content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "Current strings",
                "clip_slots": []
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Current strings",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "selected": {
          "group_name": "Current strings",
          "member_count": 1,
          "members": [
            {
              "binding_key": "part_a",
              "track_index": 0,
              "name": "Sunny|project_a|part_a|track"
            }
          ]
        },
        "scope": {
          ")SUNNYROUTE" +
         R"SUNNYROUTE(group_membership_mutated": false,
          "historical_identity_proven": false,
          "part_authority_granted": false,
          "device_authority_granted": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
      "group_adoption": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "group_only": true,
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "adopt_group",
            "group_key": "group_strings",
            "track_index": 1,
            "member_binding_keys": [
              "part_a"
            ]
          },
          "selector": {
            "track_index": 0,
            "slot_index": 0
          },
          "binding_guard": {
            "content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "Current strings",
                  "clip_slots": []
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "9076475209768904591",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "track",
                "index": 1,
                "name": "Current strings",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "9076475209768904591",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "selected": {
            "group_name": "Current strings",
            "member_count": 1,
            "members": [
              {
                "binding_key": "part_a",
                "track_index": 0,
                "name": "Sunny|project_a|part_a|track"
              }
            ]
          },
          "scope": {
            "group_membership_mutated": false,
            "historical_identity_proven": false,
            "part_authority_granted": false,
            "device_authority_granted": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "Current strings",
                "clip_slots": []
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Current strings",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "9076475209768904591",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [],
          "returned": []
        },
        "current_group_and_members_match": true,
        "authority_or)SUNNYROUTE" +
         R"SUNNYROUTE(igin": "explicit_current_group_adoption",
        "part_authority_granted": false,
        "device_authority_granted": false,
        "historical_identity_proven": false
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
        "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "adopt_group",
          "group_key": "group_strings",
          "track_index": 1,
          "member_binding_keys": [
            "part_a"
          ]
        },
        "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "preview_fingerprint": "30f78bda2f01714cd8353f17848501b801345ec2418ccb42eb4d29d5e07c518e",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "group_only": true,
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "adopt_group",
            "group_key": "group_strings",
            "track_index": 1,
            "member_binding_keys": [
              "part_a"
            ]
          },
          "selector": {
            "track_index": 0,
            "slot_index": 0
          },
          "binding_guard": {
            "content_fingerprint": "7c380ffd8b9a3f00ea4c043e0302cd9f7e570936c48e447fe8fa9c3ec6d577e8",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "Current strings",
                  "clip_slots": []
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "9076475209768904591",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "track",
                "index": 1,
                "name": "Current strings",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "9076475209768904591",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "selected": {
            "group_name": "Current strings",
            "member_count": 1,
            "members": [
              {
                "binding_key": "part_a",
                "track_index": 0,
                "name": "Sunny|project_a|part_a|track"
              }
            ]
          },
          "scope": {
            "group_membership_mutated": false,
            "historical_identity_proven": false,
            "part_authority_granted": false,
            "device_authority_granted": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "25ad54b847be677c0bb9c63e9aabac786ab1a38eac95b54cb8174e0a43a56f74",
      "outcome": "pending",
      "native_mutation_started": false,
      "routing_progress": {
        "started": [],
        "returned": []
      }
    },
    "group_observation": {
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
        "approved_preview_fingerprint": "30f78bda2f01714cd8353f17848501b801345ec2418ccb42eb4d29d5e07c518e",
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
      "group_authority_fingerprint": "b1d57b5f792c46d3159c4e30d0b42fdfe58e0635337451901bee4655d9518f10"
    }
  },
  "foreign": {
    "request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "create_return",
        "aux_key": "aux_room"
      }
    },
    "preview": {
      "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "preview_fingerprint": "24acea29d7e50e9b12c3b236706ec8f336bd9fa16f00bacc113c341690dd1438",
      "preview": {
        "schema_version": 1,
        "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
            "tempo_parameter": null,
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
                "name": "Sunny|project_b|part_b|track",
                "clip_slots": [
                  {
                    "index": 0,
                    "has_clip": true,
                    "clip": {
                      "name": "Sunny|project_b|part_b|clip",
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                  "automation_s)SUNNYROUTE" +
         R"SUNNYROUTE(tate": 0
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-1274450301245013985",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Sunny|project_b|part_b|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "No Output",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "output_channel": {
                  "display_name": "",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "No Output",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          },
          {
            "project_key": "project_b",
            "binding_key": "part_b",
            "guard": {
              "content_fingerprint": "960811d3406e438a4fdc69b6a7c68357069552d575059ec7570715ffe3e22cc3",
              "note_identity_fingerprint": "ae9dda475282a3acb53e495abdf4d406ef7437b1a9d1c467f06c45621321b990",
              "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
            },
            "track_index": 1,
            "slot_index": 0,
            "before_manifest": {
              "schema_version": 1,
              "track": {
                "name": "Sunny|project_b|part_b|track",
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
                "name": "Sunny|project_b|part_b|clip",
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
                  "pitch": 72,
                  "start_time": 1.0,
                  "duration": 1.0,
                  "velocity": 55.0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "Sunny|project_a|aux_room|return"
        },
        "scope": {
          "affects_send_population": true,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
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
        "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
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
          "MpeExpressionUnavailable: per-note expression fields were not observed",
          "FollowActionsUnavailable: Follow Action settings were not observed"
        ]
      },
      "native_mutation_started": false,
      "authority_origin": "none"
    },
    "applied_request": {
      "document_token": "document_a",
      "project_key": "project_a",
      "binding_key": "part_a",
      "operation_id": "route_a",
      "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
      "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
      "intent": {
        "kind": "create_return",
        "aux_key": "aux_room"
      },
      "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "preview_fingerprint": "24acea29d7e50e9b12c3b236706ec8f336bd9fa16f00bacc113c341690dd1438",
      "approved_preview": {
        "schema_version": 1,
        "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "context": {
          "bridge_instance": "bridge_a",
          "document_token": "document_a"
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "binding_guard": {
          "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
          "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
          "device_identity_fingerprint": null
        },
        "before": {
          "song": {
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
              "metronome": fa)SUNNYROUTE" +
         R"SUNNYROUTE(lse
            },
            "tempo_parameter": null,
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
                "name": "Sunny|project_b|part_b|track",
                "clip_slots": [
                  {
                    "index": 0,
                    "has_clip": true,
                    "clip": {
                      "name": "Sunny|project_b|part_b|clip",
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
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-1274450301245013985",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Sunny|project_b|part_b|track",
              "mute": false,
              "solo": false,
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
                "output_type": {
                  "display_name": "No Output",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "output_channel": {
                  "display_name": "",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "No Output",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "affected_bindings": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
            "guard": {
              "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
              "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
              "device_identity_fingerprint": null
            },
            "track_index": 0,
            "slot_index": 0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          },
          {
            "project_key": "project_b",
            "binding_key": "part_b",
            "guard": {
              "content_fingerprint": "960811d3406e438a4fdc69b6a7c68357069552d575059ec7570715ffe3e22cc3",
              "note_identity_fingerprint": "ae9dda475282a3acb53e495abdf4d406ef7437b1a9d1c467f06c45621321b990",
              "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
            },
            "track_index": 1,
            "slot_index": 0,
            "before_manifest": {
              "schema_version": 1,
              "track": {
                "name": "Sunny|project_b|part_b|track",
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
                "name": "Sunny|project_b|part_b|clip",
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
                  "pitch": 72,
                  "start_time": 1.0,
                  "duration": 1.0,
                  "velocity": 55.0,
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
            "before_group_authority": null,
            "before_group_authority_fingerprint": null
          }
        ],
        "selected": {
          "return_tag": "Sunny|project_a|aux_room|return"
        },
        "scope": {
          "affects_send_population": true,
          "tap_policy_observed": false,
          "group_membership_mutated": false,
          "historical_identity_proven": false
        }
      },
      "explicit_current_routing_approval": true
    },
    "result": {
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
            },
            {
              "name": "Send B",
              "original_name": "Send B",
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
      "content_fingerprint": "8804f9504251a1e37f651cbd49d156e23e4a5a64fca8c86990671f2cf5753904",
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
            "r)SUNNYROUTE" +
         R"SUNNYROUTE(elease_velocity": 32.0
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
        "MpeExpressionUnavailable: per-note expression fields were not observed",
        "FollowActionsUnavailable: Follow Action settings were not observed"
      ],
      "routing": {
        "schema_version": 1,
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "create_return",
            "aux_key": "aux_room"
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "Sunny|project_b|part_b|track",
                  "clip_slots": [
                    {
                      "index": 0,
                      "has_clip": true,
                      "clip": {
                        "name": "Sunny|project_b|part_b|clip",
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-1274450301245013985",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "track",
                "index": 1,
                "name": "Sunny|project_b|part_b|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "No Output",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  },
                  "output_channel": {
                    "display_name": "",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "No Output",
                      "identifier": "5403226549407146486",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "",
                      "identifier": "5403226549407146486",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            },
            {
              "project_key": "project_b",
              "binding_key": "part_b",
              "guard": {
                "content_fingerprint": "960811d3406e438a4fdc69b6a7c68357069552d575059ec7570715ffe3e22cc3",
                "note_identity_fingerprint": "ae9dda475282a3acb53e495abdf4d406ef7437b1a9d1c467f06c45621321b990",
                "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
              },
              "track_index": 1,
              "slot_index": 0,
              "before_manifest": {
                "schema_version": 1,
                "track": {
                  "name": "Sunny|project_b|part_b|track",
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
                  "name": "Sunny|project_b|part_b|clip",
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
                    "pitch": 72,
                    "start_time": 1.0,
                    "duration": 1.0,
                    "velocity": 55.0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "Sunny|project_a|aux_room|return"
          },
          "scope": {
            "affects_send_population": true,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "after": {
          "song": {
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
            "tempo_parameter": null,
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
           )SUNNYROUTE" +
         R"SUNNYROUTE(         }
                  }
                ]
              },
              {
                "index": 1,
                "name": "Sunny|project_b|part_b|track",
                "clip_slots": [
                  {
                    "index": 0,
                    "has_clip": true,
                    "clip": {
                      "name": "Sunny|project_b|part_b|clip",
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
              },
              {
                "index": 1,
                "name": "Sunny|project_a|aux_room|return"
              }
            ],
            "master_track": {
              "name": "Main"
            },
            "scenes": [
              {
                "name": "",
                "is_triggered": false,
                "tempo_enabled": false,
                "tempo": -1.0,
                "time_signature_enabled": false,
                "time_signature_numerator": -1,
                "time_signature_denominator": -1
              }
            ],
            "cue_points": []
          },
          "mixers": [
            {
              "kind": "track",
              "index": 0,
              "name": "Sunny|project_a|part_a|track",
              "mute": false,
              "solo": false,
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
                  },
                  {
                    "name": "Send B",
                    "original_name": "Send B",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-1274450301245013985",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  },
                  {
                    "display_name": "Sunny|project_a|aux_room|return",
                    "identifier": "-9055740950311956551",
                    "attached_target": {
                      "kind": "return",
                      "index": 1
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "track",
              "index": 1,
              "name": "Sunny|project_b|part_b|track",
              "mute": false,
              "solo": false,
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
                  },
                  {
                    "name": "Send B",
                    "original_name": "Send B",
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
                "output_type": {
                  "display_name": "No Output",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "output_channel": {
                  "display_name": "",
                  "identifier": "5403226549407146486",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "No Output",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 0,
              "name": "A-Return",
              "mute": false,
              "solo": false,
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
                    "name": "Existing return send",
                    "original_name": "Existing return send",
                    "value": 0.0,
                    "min": 0.0,
                    "max": 1.0,
                    "is_quantized": false,
                    "is_enabled": true,
                    "state": 0,
                    "automation_state": 0
                  },
                  {
                    "name": "New return send",
                    "original_name": "New return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "Sunny|project_a|aux_room|return",
                    "identifier": "-9055740950311956551",
                    "attached_target": {
                      "kind": "return",
                      "index": 1
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            },
            {
              "kind": "return",
              "index": 1,
              "name": "Sunny|project_a|aux_room|return",
              "mute": false,
              "solo": false,
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
                    "name": "New return send",
                    "original_name": "New return send",
                    "value": 0.0,
                    "min": 0.0,
                    "max": 1.0,
                    "is_quantized": false,
                    "is_enabled": true,
                    "state": 0,
                    "automation_state": 0
                  },
                  {
                    "name": "New return send",
                    "original_name": "New return send",
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
                "output_type": {
                  "display_name": "Master",
                  "identifier": "-8217667638299348433",
                  "attached_target": {
                    "kind": "main",
                    "index": null
                  }
                },
                "output_channel": {
                  "display_name": "Track In",
                  "identifier": "-1006922350989620874",
                  "attached_target": null
                },
                "available_types": [
                  {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  {
                    "display_name": "Ext. Out",
                    "identifier": "-7156951377484576444",
                    "attached_target": null
                  },
                  {
                    "display_name": "A-Return",
                    "identifier": "-1274450301245013985",
                    "attached_target": {
                      "kind": "return",
                      "index": 0
                    }
                  }
                ],
                "available_channels": [
                  {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  }
                ]
              }
            }
          ]
        },
        "progress": {
          "started": [
            "create_return_track",
            "return_name"
          ],
          "returned": [
            "create_return_track",
            "return_name"
          ]
        },
        "desired_match": true,
        "untouched_observed_state_preserved": true,
        "affected_observations": [
          {
            "project_key": "project_a",
            "binding_key": "part_a",
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
                    },
                    {
                      "name": "Send B",
                      "original_name": "Send B",
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
              "content_fingerprint": "8804f9504251a1e37f651cbd49d156e23e4a5a64fca8c86990671f2cf5753904",
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
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed"
              ]
            }
          },
          {
            "project_key": "project_b",
            "binding_key": "part_b",
            "observation": {
              "track_index": 1,
              "slot_index": 0,
              "manifest": {
                "schema_version": 1,
                "track": {
                  "name": "Sunny|project_b|part_b|track",
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
                  "name": "Sunny|project_b|part_b|clip",
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
                    "pitch": 72,
                    "start_time": 1.0,
                    "duration": 1.0,
                    "velocity": 55.0,
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
                    )SUNNYROUTE" +
         R"SUNNYROUTE("state": 0,
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
                    },
                    {
                      "name": "Send B",
                      "original_name": "Send B",
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
              "content_fingerprint": "f1e3d3b2d925d786ef2596a423bf4e7eb74bf5398793fe0e6ec44a05a900fdf5",
              "note_identity": {
                "entire_clip_population_observed": true,
                "notes": [
                  {
                    "note_id": 501,
                    "pitch": 72,
                    "start_time": 1.0,
                    "duration": 1.0,
                    "velocity": 55.0,
                    "mute": false,
                    "probability": 1.0,
                    "velocity_deviation": 0.0,
                    "release_velocity": 64.0
                  }
                ]
              },
              "note_identity_fingerprint": "ae9dda475282a3acb53e495abdf4d406ef7437b1a9d1c467f06c45621321b990",
              "track_tag": "Sunny|project_b|part_b|track",
              "clip_tag": "Sunny|project_b|part_b|clip",
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
          }
        ],
        "authority_origin": "explicit_current_routing",
        "tap_policy_observed": false,
        "logical_send_complete": false,
        "send_readback": null
      }
    },
    "operation": {
      "document_token": "document_a",
      "operation_id": "route_a",
      "name": "sunny_managed_apply_routing",
      "request": {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
        "expected_note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
        "intent": {
          "kind": "create_return",
          "aux_key": "aux_room"
        },
        "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "preview_fingerprint": "24acea29d7e50e9b12c3b236706ec8f336bd9fa16f00bacc113c341690dd1438",
        "approved_preview": {
          "schema_version": 1,
          "preview_token": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
          "context": {
            "bridge_instance": "bridge_a",
            "document_token": "document_a"
          },
          "project_key": "project_a",
          "binding_key": "part_a",
          "intent": {
            "kind": "create_return",
            "aux_key": "aux_room"
          },
          "binding_guard": {
            "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
            "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
            "device_identity_fingerprint": null
          },
          "before": {
            "song": {
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
              "tempo_parameter": null,
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
                  "name": "Sunny|project_b|part_b|track",
                  "clip_slots": [
                    {
                      "index": 0,
                      "has_clip": true,
                      "clip": {
                        "name": "Sunny|project_b|part_b|clip",
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
                  "name": "",
                  "is_triggered": false,
                  "tempo_enabled": false,
                  "tempo": -1.0,
                  "time_signature_enabled": false,
                  "time_signature_numerator": -1,
                  "time_signature_denominator": -1
                }
              ],
              "cue_points": []
            },
            "mixers": [
              {
                "kind": "track",
                "index": 0,
                "name": "Sunny|project_a|part_a|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    },
                    {
                      "display_name": "A-Return",
                      "identifier": "-1274450301245013985",
                      "attached_target": {
                        "kind": "return",
                        "index": 0
                      }
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "track",
                "index": 1,
                "name": "Sunny|project_b|part_b|track",
                "mute": false,
                "solo": false,
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
                  "output_type": {
                    "display_name": "No Output",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  },
                  "output_channel": {
                    "display_name": "",
                    "identifier": "5403226549407146486",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "No Output",
                      "identifier": "5403226549407146486",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "",
                      "identifier": "5403226549407146486",
                      "attached_target": null
                    }
                  ]
                }
              },
              {
                "kind": "return",
                "index": 0,
                "name": "A-Return",
                "mute": false,
                "solo": false,
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
                      "name": "Existing return send",
                      "original_name": "Existing return send",
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
                  "output_type": {
                    "display_name": "Master",
                    "identifier": "-8217667638299348433",
                    "attached_target": {
                      "kind": "main",
                      "index": null
                    }
                  },
                  "output_channel": {
                    "display_name": "Track In",
                    "identifier": "-1006922350989620874",
                    "attached_target": null
                  },
                  "available_types": [
                    {
                      "display_name": "Master",
                      "identifier": "-8217667638299348433",
                      "attached_target": {
                        "kind": "main",
                        "index": null
                      }
                    },
                    {
                      "display_name": "Ext. Out",
                      "identifier": "-7156951377484576444",
                      "attached_target": null
                    }
                  ],
                  "available_channels": [
                    {
                      "display_name": "Track In",
                      "identifier": "-1006922350989620874",
                      "attached_target": null
                    }
                  ]
                }
              }
            ]
          },
          "affected_bindings": [
            {
              "project_key": "project_a",
              "binding_key": "part_a",
              "guard": {
                "content_fingerprint": "5c66e358ee8816b7cc68f7687a40b188480e95c049e68afe06b85f3c49ff693c",
                "note_identity_fingerprint": "b6b624648b2842d3df6f78f9658830b3dc023369c89598481e2148bfcaa02eef",
                "device_identity_fingerprint": null
              },
              "track_index": 0,
              "slot_index": 0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            },
            {
              "project_key": "project_b",
              "binding_key": "part_b",
              "guard": {
                "content_fingerprint": "960811d3406e438a4fdc69b6a7c68357069552d575059ec7570715ffe3e22cc3",
                "note_identity_fingerprint": "ae9dda475282a3acb53e495abdf4d406ef7437b1a9d1c467f06c45621321b990",
                "device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5"
              },
              "track_index": 1,
              "slot_index": 0,
              "before_manifest": {
                "schema_version": 1,
                "track": {
                  "name": "Sunny|project_b|part_b|track",
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
                  "name": "Sunny|project_b|part_b|clip",
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
                    "pitch": 72,
                    "start_time": 1.0,
                    "duration": 1.0,
                    "velocity": 55.0,
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
        )SUNNYROUTE" +
         R"SUNNYROUTE(            "min": 0.0,
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
              "before_group_authority": null,
              "before_group_authority_fingerprint": null
            }
          ],
          "selected": {
            "return_tag": "Sunny|project_a|aux_room|return"
          },
          "scope": {
            "affects_send_population": true,
            "tap_policy_observed": false,
            "group_membership_mutated": false,
            "historical_identity_proven": false
          }
        },
        "explicit_current_routing_approval": true
      },
      "request_fingerprint": "bd377aec1132bf63818f1eab5f73ec29fdd6540fb5a4afc8e825e20bc71ddad1",
      "outcome": "pending",
      "native_mutation_started": true,
      "routing_progress": {
        "started": [
          "create_return_track",
          "return_name"
        ],
        "returned": [
          "create_return_track",
          "return_name"
        ]
      },
      "partial_return_index": 1
    }
  }
})SUNNYROUTE"));
}
