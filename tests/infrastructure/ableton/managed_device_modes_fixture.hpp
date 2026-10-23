/** Actual offline native provider; reordered labels, nonlinear Scale, no host claim. */
#pragma once
#include <nlohmann/json.hpp>
#include <string>
inline nlohmann::json managed_device_modes_fixture() {
    return nlohmann::json::parse((std::string{} + R"MODEFIXTURE({
 "mode_request": {
  "document_token": "document_a",
  "operation_id": "mode3",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "expected_device_identity_fingerprint": "c8407ec4be655f249a4b17c390943fb6a0e16a56f42da797a3bc147da304caaf",
  "device_key": "effect",
  "device": {
   "browser_name": "EQ Eight",
   "class_name": "Eq8",
   "type": 2,
   "role": "effect",
   "insertion_policy": "AppendOwnedChain"
  },
  "enum_intents": [
   {
    "capability_id": "eq8.adaptive_q",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.1.type",
    "label": "Bell"
   },
   {
    "capability_id": "eq8.band.2.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.3.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.4.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.5.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.6.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.7.enabled",
    "label": "Off"
   },
   {
    "capability_id": "eq8.band.8.enabled",
    "label": "Off"
   }
  ],
  "property_intents": [
   {
    "property": "global_mode",
    "label": "Stereo"
   }
  ]
 },
 "mode_result": {
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
  "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
    }
   ]
  },
  "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
      },
      {
       "name": "Env 1 Attack",
       "original_name": "Env 1 Attack",
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
      },
      {
       "name": "Env 1 Decay",
       "original_name": "Env 1 Decay",
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
      },
      {
       "name": "Env 1 Release",
       "original_name": "Env 1 Release",
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
      },
      {
       "name": "LP Type",
       "original_name": "LP Type",
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
         "II",
         "I"
        ]
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
    },
    {
     "device_key": "effect",
     "browser_name": "EQ Eight",
     "class_name": "Eq8",
     "class_display_name": "EQ Eight",
     "name": "EQ Eight",
     "type": 2,
     "role": "effect",
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
       "name": "Adaptive Q",
       "original_name": "Adaptive Q",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "Scale",
       "original_name": "Scale",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.25,
        "default_value": 0.25,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": []
       }
      },
      {
       "name": "Output Gain",
       "original_name": "Output Gain",
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
      },
      {
       "name": "1 Filter On A",
       "original_name": "1 Filter On A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "1 Filter Type A",
       "original_name": "1 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 3.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "1 Frequency A",
       "original_name": "1 Frequency A",
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
      },
      {
       "name": "1 Gain A",
       "original_name": "1 Gain A",
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
      },
      {
       "name": "1 Resonance A",
       "original_name": "1 Resonance A",
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
      },
      {
       "name": "2 Filter On A",
       "original_name": "2 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "2 Filter Type A",
       "original_name": "2 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "2 Frequency A",
       "original_name": "2 Frequency A",
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
      },
      {
       "name": "2 Gain A",
       "original_name": "2 Gain A",
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
      },
      {
       "name": "2 Resonance A",
       "original_name": "2 Resonance A",
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
      },
      {
       "name": "3 Filter On A",
       "original_name": "3 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "3 Filter Type A",
       "original_name": "3 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "3 Frequency A",
       "original_name": "3 Frequency A",
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
      },
      {
       "name": "3 Gain A",
       "original_name": "3 Gain A",
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
      },
      {
       "name": "3 Resonance A",
       "original_name": "3 Resonance A",
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
      },
      {
       "name": "4 Filter On A",
       "original_name": "4 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "4 Filter Type A",
       "original_name": "4 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "4 Frequency A",
       "original_name": "4 Frequency A",
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
      },
      {
       "name": "4 Gain A",
       "original_name": "4 Gain A",
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
      },
      {
       "name": "4 Resonance A",
       "original_name": "4 Resonance A",
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
      },
      {
       "name": "5 Filter On A",
       "original_name": "5 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "5 Filter Type A",
       "original_name": "5 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "5 Frequency A",
       "original_name": "5 Frequency A",
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
      },
      {
       "name": "5 Gain A",
       "original_name": "5 Gain A",
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
      },
      {
       "name": "5 Resonance A",
       "original_name": "5 Resonance A",
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
      },
      {
       "name": "6 Filter On A",
       "original_name": "6 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "6 Filter Type A",
       "original_name": "6 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "6 Frequency A",
       "original_name": "6 Frequency A",
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
      },
      {
       "name": "6 Gain A",
       "original_name": "6 Gain A",
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
      },
      {
       "name": "6 Resonance A",
       "original_name": "6 Resonance A",
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
      },
      {
       "name": "7 Filter On A",
       "original_name": "7 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "7 Filter Type A",
       "original_name": "7 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "7 Frequency A",
       "original_name": "7 Frequency A",
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
      },
      {
       "name": "7 Gain A",
       "original_name": "7 Gain A",
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
      },
      {
       "name": "7 Resonance A",
       "original_name": "7 Resonance A",
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
      },
      {
       "name": "8 Filter On A",
       "original_name": "8 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "8 Filter Type A",
       "original_name": "8 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "8 Frequency A",
       "original_name": "8 Frequency A",
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
      },
      {
       "name": "8 Gain A",
       "original_name": "8 Gain A",
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
      },
      {
       "name": "8 Resonance A",
       "original_name": "8 Resonance A",
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
      "global_mode": 0,
      "edit_mode": false,
      "oversample": false
     }
    }
   ],
   "opaque_state_observed": false
  },
  "device_identity_fingerprint": "b10fdb0ca5cec5c2cfccfb5805ee6400ad029df54b915ec94c5b46722e58e0c4",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true,
  "device_mode_update": {
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
       )MODEFIXTURE" + R"MODEFIXTURE("max": 1.0,
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
    "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
      }
     ]
    },
    "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
        },
        {
         "name": "Env 1 Attack",
         "original_name": "Env 1 Attack",
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
        },
        {
         "name": "Env 1 Decay",
         "original_name": "Env 1 Decay",
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
        },
        {
         "name": "Env 1 Release",
         "original_name": "Env 1 Release",
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
        },
        {
         "name": "LP Type",
         "original_name": "LP Type",
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
           "II",
           "I"
          ]
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
      },
      {
       "device_key": "effect",
       "browser_name": "EQ Eight",
       "class_name": "Eq8",
       "class_display_name": "EQ Eight",
       "name": "EQ Eight",
       "type": 2,
       "role": "effect",
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
         "name": "Adaptive Q",
         "original_name": "Adaptive Q",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "Scale",
         "original_name": "Scale",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.25,
          "default_value": 0.25,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": []
         }
        },
        {
         "name": "Output Gain",
         "original_name": "Output Gain",
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
        },
        {
         "name": "1 Filter On A",
         "original_name": "1 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "1 Filter Type A",
         "original_name": "1 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "1 Frequency A",
         "original_name": "1 Frequency A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": 0.0,
          "is_quantized": false,
          "is_enabled": true,
          "state": 1,
          "automation_state": 0,
          "value_items": []
         }
        },
        {
         "name": "1 Gain A",
         "original_name": "1 Gain A",
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
        },
        {
         "name": "1 Resonance A",
         "original_name": "1 Resonance A",
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
        },
        {
         "name": "2 Filter On A",
         "original_name": "2 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "2 Filter Type A",
         "original_name": "2 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "2 Frequency A",
         "original_name": "2 Frequency A",
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
        },
        {
         "name": "2 Gain A",
         "original_name": "2 Gain A",
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
        },
        {
         "name": "2 Resonance A",
         "original_name": "2 Resonance A",
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
        },
        {
         "name": "3 Filter On A",
         "original_name": "3 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "3 Filter Type A",
         "original_name": "3 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "3 Frequency A",
         "original_name": "3 Frequency A",
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
        },
        {
         "name": "3 Gain A",
         "original_name": "3 Gain A",
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
        },
        {
         "name": "3 Resonance A",
         "original_name": "3 Resonance A",
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
        },
        {
         "name": "4 Filter On A",
         "original_name": "4 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "4 Filter Type A",
         "original_name": "4 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "4 Frequency A",
         "original_name": "4 Frequency A",
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
        },
        {
         "name": "4 Gain A",
         "original_name": "4 Gain A",
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
        },
        {
         "name": "4 Resonance A",
         "original_name": "4 Resonance A",
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
        },
        {
         "name": "5 Filter On A",
         "original_name": "5 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "5 Filter Type A",
         "original_name": "5 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "5 Frequency A",
         "original_name": "5 Frequency A",
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
        },
        {
         "name": "5 Gain A",
         "original_name": "5 Gain A",
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
        },
        {
         "name": "5 Resonance A",
         "original_name": "5 Resonance A",
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
        },
        {
         "name": "6 Filter On A",
         "original_name": "6 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "6 Filter Type A",
         "original_name": "6 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "6 Frequency A",
         "original_name": "6 Frequency A",
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
        },
        {
         "name": "6 Gain A",
         "original_name": "6 Gain A",
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
        },
        {
         "name": "6 Resonance A",
         "original_name": "6 Resonance A",
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
        },
        {
         "name": "7 Filter On A",
         "original_name": "7 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "7 Filter Type A",
         "original_name": "7 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "7 Frequency A",
         "original_name": "7 Frequency A",
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
        },
        {
         "name": "7 Gain A",
         "original_name": "7 Gain A",
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
        },
        {
         "name": "7 Resonance A",
         "original_name": "7 Resonance A",
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
        },
        {
         "name": "8 Filter On A",
         "original_name": "8 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "8 Filter Type A",
         "original_name": "8 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "8 Frequency A",
         "original_name": "8 Frequency A",
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
        },
        {
         "name": "8 Gain A",
         "original_name": "8 Gain A",
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
        },
        {
         "name": "8 Resonance A",
         "original_name": "8 Resonance A",
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
        "global_mode": 1,
        "edit_mode": false,
        "oversample": false
       }
      }
     ],
     "opaque_state_observed": false
    },
    "device_identity_fingerprint": "c8407ec4be655f249a4b17c390943fb6a0e16a56f42da797a3bc147da304caaf"
   },
   "before_device_identity": {
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
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack",
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
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay",
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
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release",
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
       },
       {
        "name": "LP Type",
        "original_name": "LP Type",
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
    )MODEFIXTURE" + R"MODEFIXTURE(      "II",
          "I"
         ]
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
     },
     {
      "device_key": "effect",
      "browser_name": "EQ Eight",
      "class_name": "Eq8",
      "class_display_name": "EQ Eight",
      "name": "EQ Eight",
      "type": 2,
      "role": "effect",
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
        "name": "Adaptive Q",
        "original_name": "Adaptive Q",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "Scale",
        "original_name": "Scale",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.25,
         "default_value": 0.25,
         "is_quantized": false,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": []
        }
       },
       {
        "name": "Output Gain",
        "original_name": "Output Gain",
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
       },
       {
        "name": "1 Filter On A",
        "original_name": "1 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "1 Filter Type A",
        "original_name": "1 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "1 Frequency A",
        "original_name": "1 Frequency A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": 0.0,
         "is_quantized": false,
         "is_enabled": true,
         "state": 1,
         "automation_state": 0,
         "value_items": []
        }
       },
       {
        "name": "1 Gain A",
        "original_name": "1 Gain A",
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
       },
       {
        "name": "1 Resonance A",
        "original_name": "1 Resonance A",
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
       },
       {
        "name": "2 Filter On A",
        "original_name": "2 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "2 Filter Type A",
        "original_name": "2 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "2 Frequency A",
        "original_name": "2 Frequency A",
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
       },
       {
        "name": "2 Gain A",
        "original_name": "2 Gain A",
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
       },
       {
        "name": "2 Resonance A",
        "original_name": "2 Resonance A",
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
       },
       {
        "name": "3 Filter On A",
        "original_name": "3 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "3 Filter Type A",
        "original_name": "3 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "3 Frequency A",
        "original_name": "3 Frequency A",
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
       },
       {
        "name": "3 Gain A",
        "original_name": "3 Gain A",
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
       },
       {
        "name": "3 Resonance A",
        "original_name": "3 Resonance A",
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
       },
       {
        "name": "4 Filter On A",
        "original_name": "4 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "4 Filter Type A",
        "original_name": "4 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "4 Frequency A",
        "original_name": "4 Frequency A",
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
       },
       {
        "name": "4 Gain A",
        "original_name": "4 Gain A",
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
       },
       {
        "name": "4 Resonance A",
        "original_name": "4 Resonance A",
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
       },
       {
        "name": "5 Filter On A",
        "original_name": "5 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "5 Filter Type A",
        "original_name": "5 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "5 Frequency A",
        "original_name": "5 Frequency A",
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
       },
       {
        "name": "5 Gain A",
        "original_name": "5 Gain A",
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
       },
       {
        "name": "5 Resonance A",
        "original_name": "5 Resonance A",
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
       },
       {
        "name": "6 Filter On A",
        "original_name": "6 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "6 Filter Type A",
        "original_name": "6 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "6 Frequency A",
        "original_name": "6 Frequency A",
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
       },
       {
        "name": "6 Gain A",
        "original_name": "6 Gain A",
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
       },
       {
        "name": "6 Resonance A",
        "original_name": "6 Resonance A",
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
       },
       {
        "name": "7 Filter On A",
        "original_name": "7 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "7 Filter Type A",
        "original_name": "7 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "7 Frequency A",
        "original_name": "7 Frequency A",
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
       },
       {
        "name": "7 Gain A",
        "original_name": "7 Gain A",
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
       },
       {
        "name": "7 Resonance A",
        "original_name": "7 Resonance A",
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
       },
       {
        "name": "8 Filter On A",
        "original_name": "8 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "8 Filter Type A",
        "original_name": "8 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "8 Frequency A",
        "original_name": "8 Frequency A",
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
       },
       {
        "name": "8 Gain A",
        "original_name": "8 Gain A",
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
       },
       {
        "name": "8 Resonance A",
        "original_name": "8 Resonance A",
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
       "global_mode": 1,
       "edit_mode": false,
       "oversample": false
      }
     }
    ],
    "opaque_state_observed": false
   },
   "before_device_identity_fingerprint": "c8407ec4be655f249a4b17c390943fb6a0e16a56f42da797a3bc147da304caaf",
   "device_key": "effect",
   "admitted_modes": [
    {
     "intent": {
      "capability_id": "eq8.adaptive_q",
      "label": "Off"
     },
     "parameter_index": 1,
     "parameter_original_name": "Adaptive Q",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.1.type",
      "label": "Bell"
     },
     "parameter_index": 5,
     "parameter_original_name": "1 Filter Type A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 7.0,
      "value": 5.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "High Shelf",
       "Notch",
       "Low Cut12",
       "Bell",
       "Low Cut48",
       "Low Shelf",
       "High Cut12",
       "High Cut48"
      ]
     },
     "target_internal": 3.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.2.enabled",
      "label": "Off"
     },
     "parameter_index": 9,
     "parameter_original_name": "2 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.3.enabled",
      "label": "Off"
     },
     "parameter_index": 14,
     "parameter_original_name": "3 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.4.enabled",
      "label": "Off"
     },
     "parameter_index": 19,
     "parameter_original_name": "4 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.5.enabled",
      "label": "Off"
     },
     "parameter_index": 24,
     "parameter_original_name": "5 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.6.enabled",
      "label": "Off"
     },
     "parameter_index": 29,
     "parameter_original_name": "6 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.7.enabled",
      "label": "Off"
     },
     "parameter_index": 34,
     "parameter_original_name": "7 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    },
    {
     "intent": {
      "capability_id": "eq8.band.8.enabled",
      "label": "Off"
     },
     "parameter_index": 39,
     "parameter_original_name": "8 Filter On A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "default_value": null,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "On",
       "Off"
      ]
     },
     "target_internal": 1.0
    }
   ],
   "admitted_properties": [
    {
     "intent": {
      "property": "global_mode",
      "label": "Stereo"
     },
     "before": 1,
     "target": 0
    }
   ],
   "readbacks": [
    {
     "capability_id": "eq8.adaptive_q",
     "parameter_index": 1,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.1.type",
     "parameter_index": 5,
     "internal_value": 3.0,
     "label": "Bell"
    },
    {
     "capability_id": "eq8.band.2.enabled",
     "parameter_index": 9,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.3.enabled",
     "parameter_index": 14,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.4.enabled",
     "parameter_index": 19,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.5.enabled",
     "parameter_index": 24,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.6.enabled",
     "parameter_index": 29,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.7.enabled",
     "parameter_index": 34,
     "internal_value": 1.0,
     "label": "Off"
    },
    {
     "capability_id": "eq8.band.8.enabled",
     "parameter_index": 39,
     "internal_value": 1.0,
     "label": "Off"
    }
   ],
   "property_readbacks": [
    {
     "property": "global_mode",
     "value": 0
    }
   ],
   "clip_and_note_ids_preserved": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "scale_request": {
  "document_token": "document_a",
  "operation_id": "scale",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "expected_device_identity_fingerprint": "b10fdb0ca5cec5c2cfccfb5805ee6400ad029df54b915ec94c5b46722e58e0c4",
  "device_key": "effect",
  "device": {
   "browser_name": "EQ Eight",
   "class_name": "Eq8",
   "type": 2,
   "role": "effect",
   "insertion_policy": "AppendOwnedChain"
  },
  "physical_intents": [
   {
    "capability_id": "eq8.scale",
    "target": 100.0,
    "tolerance": 0.0
   }
  ]
 },
 "scale_result": {
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
     "n)MODEFIXTURE" + R"MODEFIXTURE(ame": "Speaker On",
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
  "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
    }
   ]
  },
  "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
      },
      {
       "name": "Env 1 Attack",
       "original_name": "Env 1 Attack",
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
      },
      {
       "name": "Env 1 Decay",
       "original_name": "Env 1 Decay",
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
      },
      {
       "name": "Env 1 Release",
       "original_name": "Env 1 Release",
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
      },
      {
       "name": "LP Type",
       "original_name": "LP Type",
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
         "II",
         "I"
        ]
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
    },
    {
     "device_key": "effect",
     "browser_name": "EQ Eight",
     "class_name": "Eq8",
     "class_display_name": "EQ Eight",
     "name": "EQ Eight",
     "type": 2,
     "role": "effect",
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
       "name": "Adaptive Q",
       "original_name": "Adaptive Q",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "Scale",
       "original_name": "Scale",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.70709228515625,
        "default_value": 0.25,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": []
       }
      },
      {
       "name": "Output Gain",
       "original_name": "Output Gain",
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
      },
      {
       "name": "1 Filter On A",
       "original_name": "1 Filter On A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "1 Filter Type A",
       "original_name": "1 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 3.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "1 Frequency A",
       "original_name": "1 Frequency A",
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
      },
      {
       "name": "1 Gain A",
       "original_name": "1 Gain A",
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
      },
      {
       "name": "1 Resonance A",
       "original_name": "1 Resonance A",
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
      },
      {
       "name": "2 Filter On A",
       "original_name": "2 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "2 Filter Type A",
       "original_name": "2 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "2 Frequency A",
       "original_name": "2 Frequency A",
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
      },
      {
       "name": "2 Gain A",
       "original_name": "2 Gain A",
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
      },
      {
       "name": "2 Resonance A",
       "original_name": "2 Resonance A",
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
      },
      {
       "name": "3 Filter On A",
       "original_name": "3 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "3 Filter Type A",
       "original_name": "3 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "3 Frequency A",
       "original_name": "3 Frequency A",
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
      },
      {
       "name": "3 Gain A",
       "original_name": "3 Gain A",
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
      },
      {
       "name": "3 Resonance A",
       "original_name": "3 Resonance A",
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
      },
      {
       "name": "4 Filter On A",
       "original_name": "4 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "4 Filter Type A",
       "original_name": "4 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "4 Frequency A",
       "original_name": "4 Frequency A",
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
      },
      {
       "name": "4 Gain A",
       "original_name": "4 Gain A",
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
      },
      {
       "name": "4 Resonance A",
       "original_name": "4 Resonance A",
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
      },
      {
       "name": "5 Filter On A",
       "original_name": "5 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "5 Filter Type A",
       "original_name": "5 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "5 Frequency A",
       "original_name": "5 Frequency A",
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
      },
      {
       "name": "5 Gain A",
       "original_name": "5 Gain A",
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
      },
      {
       "name": "5 Resonance A",
       "original_name": "5 Resonance A",
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
      },
      {
       "name": "6 Filter On A",
       "original_name": "6 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "6 Filter Type A",
       "original_name": "6 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "6 Frequency A",
       "original_name": "6 Frequency A",
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
      },
      {
       "name": "6 Gain A",
       "original_name": "6 Gain A",
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
      },
      {
       "name": "6 Resonance A",
       "original_name": "6 Resonance A",
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
      },
      {
       "name": "7 Filter On A",
       "original_name": "7 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "7 Filter Type A",
       "original_name": "7 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "7 Frequency A",
       "original_name": "7 Frequency A",
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
      },
      {
       "name": "7 Gain A",
       "original_name": "7 Gain A",
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
      },
      {
       "name": "7 Resonance A",
       "original_name": "7 Resonance A",
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
      },
      {
       "name": "8 Filter On A",
       "original_name": "8 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "8 Filter Type A",
       "original_name": "8 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "8 Frequency A",
       "original_name": "8 Frequency A",
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
      },
      {
       "name": "8 Gain A",
       "original_name": "8 Gain A",
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
      },
      {
       "name": "8 Resonance A",
       "original_name": "8 Resonance A",
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
      "global_mode": 0,
      "edit_mode": false,
      "oversample": false
     }
    }
   ],
   "opaque_state_observed": false
  },
  "device_identity_fingerprint": "020ac0912085f91904eadc84532fe408ea9cd66a96491b469f1beb99eca0a92a",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true,
  "device_update": {
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
    "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
      }
     ]
    },
    "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
          ")MODEFIXTURE" + R"MODEFIXTURE(value": 1.0,
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
        },
        {
         "name": "Env 1 Attack",
         "original_name": "Env 1 Attack",
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
        },
        {
         "name": "Env 1 Decay",
         "original_name": "Env 1 Decay",
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
        },
        {
         "name": "Env 1 Release",
         "original_name": "Env 1 Release",
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
        },
        {
         "name": "LP Type",
         "original_name": "LP Type",
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
           "II",
           "I"
          ]
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
      },
      {
       "device_key": "effect",
       "browser_name": "EQ Eight",
       "class_name": "Eq8",
       "class_display_name": "EQ Eight",
       "name": "EQ Eight",
       "type": 2,
       "role": "effect",
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
         "name": "Adaptive Q",
         "original_name": "Adaptive Q",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "Scale",
         "original_name": "Scale",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.25,
          "default_value": 0.25,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": []
         }
        },
        {
         "name": "Output Gain",
         "original_name": "Output Gain",
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
        },
        {
         "name": "1 Filter On A",
         "original_name": "1 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "1 Filter Type A",
         "original_name": "1 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 3.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "1 Frequency A",
         "original_name": "1 Frequency A",
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
        },
        {
         "name": "1 Gain A",
         "original_name": "1 Gain A",
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
        },
        {
         "name": "1 Resonance A",
         "original_name": "1 Resonance A",
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
        },
        {
         "name": "2 Filter On A",
         "original_name": "2 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "2 Filter Type A",
         "original_name": "2 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "2 Frequency A",
         "original_name": "2 Frequency A",
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
        },
        {
         "name": "2 Gain A",
         "original_name": "2 Gain A",
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
        },
        {
         "name": "2 Resonance A",
         "original_name": "2 Resonance A",
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
        },
        {
         "name": "3 Filter On A",
         "original_name": "3 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "3 Filter Type A",
         "original_name": "3 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "3 Frequency A",
         "original_name": "3 Frequency A",
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
        },
        {
         "name": "3 Gain A",
         "original_name": "3 Gain A",
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
        },
        {
         "name": "3 Resonance A",
         "original_name": "3 Resonance A",
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
        },
        {
         "name": "4 Filter On A",
         "original_name": "4 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "4 Filter Type A",
         "original_name": "4 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "4 Frequency A",
         "original_name": "4 Frequency A",
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
        },
        {
         "name": "4 Gain A",
         "original_name": "4 Gain A",
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
        },
        {
         "name": "4 Resonance A",
         "original_name": "4 Resonance A",
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
        },
        {
         "name": "5 Filter On A",
         "original_name": "5 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "5 Filter Type A",
         "original_name": "5 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "5 Frequency A",
         "original_name": "5 Frequency A",
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
        },
        {
         "name": "5 Gain A",
         "original_name": "5 Gain A",
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
        },
        {
         "name": "5 Resonance A",
         "original_name": "5 Resonance A",
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
        },
        {
         "name": "6 Filter On A",
         "original_name": "6 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "6 Filter Type A",
         "original_name": "6 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "6 Frequency A",
         "original_name": "6 Frequency A",
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
        },
        {
         "name": "6 Gain A",
         "original_name": "6 Gain A",
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
        },
        {
         "name": "6 Resonance A",
         "original_name": "6 Resonance A",
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
        },
        {
         "name": "7 Filter On A",
         "original_name": "7 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "7 Filter Type A",
         "original_name": "7 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "7 Frequency A",
         "original_name": "7 Frequency A",
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
        },
        {
         "name": "7 Gain A",
         "original_name": "7 Gain A",
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
        },
        {
         "name": "7 Resonance A",
         "original_name": "7 Resonance A",
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
        },
        {
         "name": "8 Filter On A",
         "original_name": "8 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "8 Filter Type A",
         "original_name": "8 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "8 Frequency A",
         "original_name": "8 Frequency A",
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
        },
        {
         "name": "8 Gain A",
         "original_name": "8 Gain A",
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
        },
        {
         "name": "8 Resonance A",
         "original_name": "8 Resonance A",
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
        "global_mode": 0,
        "edit_mode": false,
        "oversample": false
       }
      }
     ],
     "opaque_state_observed": false
    },
    "device_identity_fingerprint": "b10fdb0ca5cec5c2cfccfb5805ee6400ad029df54b915ec94c5b46722e58e0c4"
   },
   "before_device_identity": {
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
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack",
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
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay",
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
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release",
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
       },
       {
        "name": "LP Type",
        "original_name": "LP Type",
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
          "II",
          "I"
         ]
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
     },
     {
      "device_key": "effect",
      "browser_name": "EQ Eight",
      "class_name": "Eq8",
      "class_display_name": "EQ Eight",
      "name": "EQ Eight",
      "type": 2,
      "role": "effect",
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
        "name": "Adaptive Q",
        "original_name": "Adaptive Q",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "Scale",
        "original_name": "Scale",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.25,
         "default_value": 0.25,
         "is_quantized": false,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": []
        }
       },
       {
        "name": "Output Gain",
        "original_name": "Output Gain",
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
       },
       {
        "name": "1 Filter On A",
        "original_name": "1 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "1 Filter Type A",
        "original_name": "1 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 3.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
         )MODEFIXTURE" + R"MODEFIXTURE( "High Cut48"
         ]
        }
       },
       {
        "name": "1 Frequency A",
        "original_name": "1 Frequency A",
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
       },
       {
        "name": "1 Gain A",
        "original_name": "1 Gain A",
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
       },
       {
        "name": "1 Resonance A",
        "original_name": "1 Resonance A",
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
       },
       {
        "name": "2 Filter On A",
        "original_name": "2 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "2 Filter Type A",
        "original_name": "2 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "2 Frequency A",
        "original_name": "2 Frequency A",
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
       },
       {
        "name": "2 Gain A",
        "original_name": "2 Gain A",
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
       },
       {
        "name": "2 Resonance A",
        "original_name": "2 Resonance A",
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
       },
       {
        "name": "3 Filter On A",
        "original_name": "3 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "3 Filter Type A",
        "original_name": "3 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "3 Frequency A",
        "original_name": "3 Frequency A",
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
       },
       {
        "name": "3 Gain A",
        "original_name": "3 Gain A",
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
       },
       {
        "name": "3 Resonance A",
        "original_name": "3 Resonance A",
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
       },
       {
        "name": "4 Filter On A",
        "original_name": "4 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "4 Filter Type A",
        "original_name": "4 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "4 Frequency A",
        "original_name": "4 Frequency A",
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
       },
       {
        "name": "4 Gain A",
        "original_name": "4 Gain A",
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
       },
       {
        "name": "4 Resonance A",
        "original_name": "4 Resonance A",
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
       },
       {
        "name": "5 Filter On A",
        "original_name": "5 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "5 Filter Type A",
        "original_name": "5 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "5 Frequency A",
        "original_name": "5 Frequency A",
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
       },
       {
        "name": "5 Gain A",
        "original_name": "5 Gain A",
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
       },
       {
        "name": "5 Resonance A",
        "original_name": "5 Resonance A",
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
       },
       {
        "name": "6 Filter On A",
        "original_name": "6 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "6 Filter Type A",
        "original_name": "6 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "6 Frequency A",
        "original_name": "6 Frequency A",
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
       },
       {
        "name": "6 Gain A",
        "original_name": "6 Gain A",
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
       },
       {
        "name": "6 Resonance A",
        "original_name": "6 Resonance A",
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
       },
       {
        "name": "7 Filter On A",
        "original_name": "7 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "7 Filter Type A",
        "original_name": "7 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "7 Frequency A",
        "original_name": "7 Frequency A",
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
       },
       {
        "name": "7 Gain A",
        "original_name": "7 Gain A",
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
       },
       {
        "name": "7 Resonance A",
        "original_name": "7 Resonance A",
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
       },
       {
        "name": "8 Filter On A",
        "original_name": "8 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "8 Filter Type A",
        "original_name": "8 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "8 Frequency A",
        "original_name": "8 Frequency A",
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
       },
       {
        "name": "8 Gain A",
        "original_name": "8 Gain A",
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
       },
       {
        "name": "8 Resonance A",
        "original_name": "8 Resonance A",
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
       "global_mode": 0,
       "edit_mode": false,
       "oversample": false
      }
     }
    ],
    "opaque_state_observed": false
   },
   "before_device_identity_fingerprint": "b10fdb0ca5cec5c2cfccfb5805ee6400ad029df54b915ec94c5b46722e58e0c4",
   "inserted": false,
   "device_key": "effect",
   "resolutions": [
    {
     "intent": {
      "capability_id": "eq8.scale",
      "target": 100.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "Eq8",
      "parameter_original_name": "Scale",
      "parameter_index": 2,
      "unit": "Percent",
      "target": 100.0,
      "display_tolerance": 0.0,
      "internal_value": 0.70709228515625,
      "display": "100.00 %",
      "display_value": 100.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.25,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.25
      },
      "modes": {
       "global_mode": 0,
       "Scale": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.25,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "default_value": 0.25
       },
       "Adaptive Q": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "On",
         "Off"
        ],
        "label": "Off"
       }
      },
      "eq8_scale_display": {
       "display": "12.50 %",
       "display_value": 12.5,
       "display_increment": 0.01
      },
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "Adaptive Q",
        "original_name": "Adaptive Q"
       },
       {
        "name": "Scale",
        "original_name": "Scale"
       },
       {
        "name": "Output Gain",
        "original_name": "Output Gain"
       },
       {
        "name": "1 Filter On A",
        "original_name": "1 Filter On A"
       },
       {
        "name": "1 Filter Type A",
        "original_name": "1 Filter Type A"
       },
       {
        "name": "1 Frequency A",
        "original_name": "1 Frequency A"
       },
       {
        "name": "1 Gain A",
        "original_name": "1 Gain A"
       },
       {
        "name": "1 Resonance A",
        "original_name": "1 Resonance A"
       },
       {
        "name": "2 Filter On A",
        "original_name": "2 Filter On A"
       },
       {
        "name": "2 Filter Type A",
        "original_name": "2 Filter Type A"
       },
       {
        "name": "2 Frequency A",
        "original_name": "2 Frequency A"
       },
       {
        "name": "2 Gain A",
        "original_name": "2 Gain A"
       },
       {
        "name": "2 Resonance A",
        "original_name": "2 Resonance A"
       },
       {
        "name": "3 Filter On A",
        "original_name": "3 Filter On A"
       },
       {
        "name": "3 Filter Type A",
        "original_name": "3 Filter Type A"
       },
       {
        "name": "3 Frequency A",
        "original_name": "3 Frequency A"
       },
       {
        "name": "3 Gain A",
        "original_name": "3 Gain A"
       },
       {
        "name": "3 Resonance A",
        "original_name": "3 Resonance A"
       },
       {
        "name": "4 Filter On A",
        "original_name": "4 Filter On A"
       },
       {
        "name": "4 Filter Type A",
        "original_name": "4 Filter Type A"
       },
       {
        "name": "4 Frequency A",
        "original_name": "4 Frequency A"
       },
       {
        "name": "4 Gain A",
        "original_name": "4 Gain A"
       },
       {
        "name": "4 Resonance A",
        "original_name": "4 Resonance A"
       },
       {
        "name": "5 Filter On A",
        "original_name": "5 Filter On A"
       },
       {
        "name": "5 Filter Type A",
        "original_name": "5 Filter Type A"
       },
       {
        "name": "5 Frequency A",
        "original_name": "5 Frequency A"
       },
       {
        "name": "5 Gain A",
        "original_name": "5 Gain A"
       },
       {
        "name": "5 Resonance A",
        "original_name": "5 Resonance A"
       },
       {
        "name": "6 Filter On A",
        "original_name": "6 Filter On A"
       },
       {
        "name": "6 Filter Type A",
        "original_name": "6 Filter Type A"
       },
       {
        "name": "6 Frequency A",
        "original_name": "6 Frequency A"
       },
       {
        "name": "6 Gain A",
        "original_name": "6 Gain A"
       },
       {
        "name": "6 Resonance A",
        "original_name": "6 Resonance A"
       },
       {
        "name": "7 Filter On A",
        "original_name": "7 Filter On A"
       },
       {
        "name": "7 Filter Type A",
        "original_name": "7 Filter Type A"
       },
       {
        "name": "7 Frequency A",
        "original_name": "7 Frequency A"
       },
       {
        "name": "7 Gain A",
        "original_name": "7 Gain A"
       },
       {
        "name": "7 Resonance A",
        "original_name": "7 Resonance A"
       },
       {
        "name": "8 Filter On A",
        "original_name": "8 Filter On A"
       },
       {
        "name": "8 Filter Type A",
        "original_name": "8 Filter Type A"
       },
       {
        "name": "8 Frequency A",
        "original_name": "8 Frequency A"
       },
       {
        "name": "8 Gain A",
        "original_name": "8 Gain A"
       },
       {
        "name": "8 Resonance A",
        "original_name": "8 Resonance A"
       }
      ],
      "formatter_calls": 31,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "0.00 %",
        "phase": "grid",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "0.78 %",
        "phase": "grid",
        "display_value": 0.78,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "3.12 %",
        "phase": "grid",
        "display_value": 3.12,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "7.03 %",
        "phase": "grid",
        "display_value": 7.03,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "12.50 %",
        "phase": "grid",
        "display_value": 12.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "19.53 %",
        "phase": "grid",
        "display_value": 19.53,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "28.12 %",
        "phase": "grid",
        "display_value": 28.12,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "38.28 %",
        "phase": "grid",
        "display_value": 38.28,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "50.00 %",
        "phase": "grid",
        "display_value": 50.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "63.28 %",
        "phase": "grid",
        "display_value": 63.28,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "78.12 %",
        "phase": "grid",
        "display_value": 78.12,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "94.53 %",
        "phase": "grid",
        "display_value": 94.53,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "112.50 %",
        "phase": "grid",
        "display_value": 112.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "132.03 %",
        "phase": "grid",
        "display_value": 132.03,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "153.12 %",
        "phase": "grid",
        "display_value": 153.12,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "175.78 %",
        "phase": "grid",
        "display_value": 175.78,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "200.00 %",
        "phase": "grid",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.71875,
        "display": "103.32 %",
        "phase": "search",
        "display_value": 103.32,
        "negative_infinity": false
       },
       {
        "internal_value": 0.703125,
        "display": "98.88 %",
        "phase": "search",
        "display_value": 98.88,
        "negative_infinity": false
       },
       {
        "internal_value": 0.7109375,
        "display": "101.09 %",
        "phase": "search",
        "display_value": 101.09,
        "negative_infinity": false
       },
       {
        "internal_value": 0.70703125,
        "display": "99.98 %",
        "phase": "search",
        "display_value": 99.98,
        "negative_infinity": false
       },
       {
        "internal_value": 0.708984375,
        "display": "100.53 %",
        "phase": "search",
        "display_value": 100.53,
        "negative_infinity": false
       },
       {
        "internal_value": 0.7080078125,
        "display": "100.26 %",
        "phase": "search",
        "display_value": 100.26,
        "negative_infinity": false
       },
       {
        "internal_value": 0.70751953125,
        "display": "100.12 %",
        "phase": "search",
        "display_value": 100.12,
        "negative_infinity": false
       },
       {
        "internal_value": 0.707275390625,
        "display": "100.05 %",
        "phase": "search",
        "display_value": 100.05,
        "negative_infinity": false
       },
       {
        "internal_value": 0.7071533203125,
        "display": "100.01 %",
        "phase": "search",
        "display_value": 100.01,
        "negative_infinity": false
       },
       {
        "internal_value": 0.70709228515625,
        "display": "100.00 %",
        "phase": "search",
        "display_value": 100.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "0.00 %",
        "phase": "repeat",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "200.00 %",
        "phase": "repeat",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.70709228515625,
        "display": "100.00 %",
        "phase": "repeat",
        "display_value": 100.0,
        "negative_infinity": false
       }
      ],
      "qualification": "ObservedNativeDisplayCandidate",
      "source_commit": "e83d5192f321b24eb9daab843ac49a2d95d862b1",
      "host_qualified": false,
      "native_knob_only": true,
      "coverage_limits": [
       "Tolerance compares displayed numbers; no hidden physical rounding-error bound is inferred.",
       "Sampled monotonicity does not prove a global transfer function or search completeness.",
       "Balance is a native displayed coordinate, not an arbitrary pan law or physical angle.",
       "EQ Eight Scale and Adaptive Q are observed couplings, not a literal DSP response claim.",
       "Drift voice and LP Type modes do not qualify envelope shape, routing or modulation.",
       "Native readback after an authorized write and reopen must be independently verified.",
       "Version, edition, operating system, envelope and persistence qualification remain separate."
      ]
     }
    }
   ],
   "readbacks": [
    {
     "capa)MODEFIXTURE" + R"MODEFIXTURE(bility_id": "eq8.scale",
     "internal_value": 0.70709228515625,
     "display": "100.00 %",
     "display_value": 100.0,
     "display_increment": 0.01,
     "absolute_display_error": 0.0,
     "matches_intent": true,
     "formatter_calls": 1
    }
   ],
   "clip_and_note_ids_preserved": true,
   "output_role_transition": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "bypass_request": {
  "document_token": "document_a",
  "operation_id": "mode5",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "expected_device_identity_fingerprint": "020ac0912085f91904eadc84532fe408ea9cd66a96491b469f1beb99eca0a92a",
  "device_key": "effect",
  "device": {
   "browser_name": "EQ Eight",
   "class_name": "Eq8",
   "type": 2,
   "role": "effect",
   "insertion_policy": "AppendOwnedChain"
  },
  "enum_intents": [
   {
    "capability_id": "eq8.enabled",
    "label": "Off"
   }
  ],
  "property_intents": []
 },
 "bypass_result": {
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
  "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
    }
   ]
  },
  "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
      },
      {
       "name": "Env 1 Attack",
       "original_name": "Env 1 Attack",
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
      },
      {
       "name": "Env 1 Decay",
       "original_name": "Env 1 Decay",
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
      },
      {
       "name": "Env 1 Release",
       "original_name": "Env 1 Release",
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
      },
      {
       "name": "LP Type",
       "original_name": "LP Type",
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
         "II",
         "I"
        ]
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
    },
    {
     "device_key": "effect",
     "browser_name": "EQ Eight",
     "class_name": "Eq8",
     "class_display_name": "EQ Eight",
     "name": "EQ Eight",
     "type": 2,
     "role": "effect",
     "is_active": false,
     "can_have_chains": false,
     "parameters": [
      {
       "name": "Device On",
       "original_name": "Device On",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
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
       "name": "Adaptive Q",
       "original_name": "Adaptive Q",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "Scale",
       "original_name": "Scale",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.70709228515625,
        "default_value": 0.25,
        "is_quantized": false,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": []
       }
      },
      {
       "name": "Output Gain",
       "original_name": "Output Gain",
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
      },
      {
       "name": "1 Filter On A",
       "original_name": "1 Filter On A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "1 Filter Type A",
       "original_name": "1 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 3.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "1 Frequency A",
       "original_name": "1 Frequency A",
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
      },
      {
       "name": "1 Gain A",
       "original_name": "1 Gain A",
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
      },
      {
       "name": "1 Resonance A",
       "original_name": "1 Resonance A",
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
      },
      {
       "name": "2 Filter On A",
       "original_name": "2 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "2 Filter Type A",
       "original_name": "2 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "2 Frequency A",
       "original_name": "2 Frequency A",
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
      },
      {
       "name": "2 Gain A",
       "original_name": "2 Gain A",
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
      },
      {
       "name": "2 Resonance A",
       "original_name": "2 Resonance A",
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
      },
      {
       "name": "3 Filter On A",
       "original_name": "3 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "3 Filter Type A",
       "original_name": "3 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "3 Frequency A",
       "original_name": "3 Frequency A",
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
      },
      {
       "name": "3 Gain A",
       "original_name": "3 Gain A",
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
      },
      {
       "name": "3 Resonance A",
       "original_name": "3 Resonance A",
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
      },
      {
       "name": "4 Filter On A",
       "original_name": "4 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "4 Filter Type A",
       "original_name": "4 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "4 Frequency A",
       "original_name": "4 Frequency A",
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
      },
      {
       "name": "4 Gain A",
       "original_name": "4 Gain A",
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
      },
      {
       "name": "4 Resonance A",
       "original_name": "4 Resonance A",
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
      },
      {
       "name": "5 Filter On A",
       "original_name": "5 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "5 Filter Type A",
       "original_name": "5 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "5 Frequency A",
       "original_name": "5 Frequency A",
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
      },
      {
       "name": "5 Gain A",
       "original_name": "5 Gain A",
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
      },
      {
       "name": "5 Resonance A",
       "original_name": "5 Resonance A",
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
      },
      {
       "name": "6 Filter On A",
       "original_name": "6 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "6 Filter Type A",
       "original_name": "6 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "6 Frequency A",
       "original_name": "6 Frequency A",
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
      },
      {
       "name": "6 Gain A",
       "original_name": "6 Gain A",
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
      },
      {
       "name": "6 Resonance A",
       "original_name": "6 Resonance A",
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
      },
      {
       "name": "7 Filter On A",
       "original_name": "7 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "7 Filter Type A",
       "original_name": "7 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "7 Frequency A",
       "original_name": "7 Frequency A",
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
      },
      {
       "name": "7 Gain A",
       "original_name": "7 Gain A",
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
      },
      {
       "name": "7 Resonance A",
       "original_name": "7 Resonance A",
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
      },
      {
       "name": "8 Filter On A",
       "original_name": "8 Filter On A",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "8 Filter Type A",
       "original_name": "8 Filter Type A",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 7.0,
        "value": 5.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "High Shelf",
         "Notch",
         "Low Cut12",
         "Bell",
         "Low Cut48",
         "Low Shelf",
         "High Cut12",
         "High Cut48"
        ]
       }
      },
      {
       "name": "8 Frequency A",
       "original_name": "8 Frequency A",
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
      },
      {
       "name": "8 Gain A",
       "original_name": "8 Gain A",
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
      },
      {
       "name": "8 Resonance A",
       "original_name": "8 Resonance A",
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
      "global_mode": 0,
      "edit_mode": false,
      "oversample": false
     }
    }
   ],
   "opaque_state_observed": false
  },
  "device_identity_fingerprint": "ec82ea2a55fc5277f214c569165fda86991a7aedbd32cc32a35d737a1f8fd11c",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true,
  "device_mode_update": {
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
       "original_name": "T)MODEFIXTURE" +
                                  R"MODEFIXTURE(rack Panning",
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
    "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
      }
     ]
    },
    "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
        },
        {
         "name": "Env 1 Attack",
         "original_name": "Env 1 Attack",
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
        },
        {
         "name": "Env 1 Decay",
         "original_name": "Env 1 Decay",
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
        },
        {
         "name": "Env 1 Release",
         "original_name": "Env 1 Release",
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
        },
        {
         "name": "LP Type",
         "original_name": "LP Type",
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
           "II",
           "I"
          ]
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
      },
      {
       "device_key": "effect",
       "browser_name": "EQ Eight",
       "class_name": "Eq8",
       "class_display_name": "EQ Eight",
       "name": "EQ Eight",
       "type": 2,
       "role": "effect",
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
         "name": "Adaptive Q",
         "original_name": "Adaptive Q",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "Scale",
         "original_name": "Scale",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.70709228515625,
          "default_value": 0.25,
          "is_quantized": false,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": []
         }
        },
        {
         "name": "Output Gain",
         "original_name": "Output Gain",
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
        },
        {
         "name": "1 Filter On A",
         "original_name": "1 Filter On A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "1 Filter Type A",
         "original_name": "1 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 3.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "1 Frequency A",
         "original_name": "1 Frequency A",
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
        },
        {
         "name": "1 Gain A",
         "original_name": "1 Gain A",
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
        },
        {
         "name": "1 Resonance A",
         "original_name": "1 Resonance A",
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
        },
        {
         "name": "2 Filter On A",
         "original_name": "2 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "2 Filter Type A",
         "original_name": "2 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "2 Frequency A",
         "original_name": "2 Frequency A",
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
        },
        {
         "name": "2 Gain A",
         "original_name": "2 Gain A",
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
        },
        {
         "name": "2 Resonance A",
         "original_name": "2 Resonance A",
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
        },
        {
         "name": "3 Filter On A",
         "original_name": "3 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "3 Filter Type A",
         "original_name": "3 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "3 Frequency A",
         "original_name": "3 Frequency A",
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
        },
        {
         "name": "3 Gain A",
         "original_name": "3 Gain A",
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
        },
        {
         "name": "3 Resonance A",
         "original_name": "3 Resonance A",
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
        },
        {
         "name": "4 Filter On A",
         "original_name": "4 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "4 Filter Type A",
         "original_name": "4 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "4 Frequency A",
         "original_name": "4 Frequency A",
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
        },
        {
         "name": "4 Gain A",
         "original_name": "4 Gain A",
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
        },
        {
         "name": "4 Resonance A",
         "original_name": "4 Resonance A",
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
        },
        {
         "name": "5 Filter On A",
         "original_name": "5 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "5 Filter Type A",
         "original_name": "5 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "5 Frequency A",
         "original_name": "5 Frequency A",
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
        },
        {
         "name": "5 Gain A",
         "original_name": "5 Gain A",
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
        },
        {
         "name": "5 Resonance A",
         "original_name": "5 Resonance A",
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
        },
        {
         "name": "6 Filter On A",
         "original_name": "6 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "6 Filter Type A",
         "original_name": "6 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "6 Frequency A",
         "original_name": "6 Frequency A",
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
        },
        {
         "name": "6 Gain A",
         "original_name": "6 Gain A",
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
        },
        {
         "name": "6 Resonance A",
         "original_name": "6 Resonance A",
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
        },
        {
         "name": "7 Filter On A",
         "original_name": "7 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "7 Filter Type A",
         "original_name": "7 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "7 Frequency A",
         "original_name": "7 Frequency A",
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
        },
        {
         "name": "7 Gain A",
         "original_name": "7 Gain A",
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
        },
        {
         "name": "7 Resonance A",
         "original_name": "7 Resonance A",
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
        },
        {
         "name": "8 Filter On A",
         "original_name": "8 Filter On A",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "8 Filter Type A",
         "original_name": "8 Filter Type A",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 7.0,
          "value": 5.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "High Shelf",
           "Notch",
           "Low Cut12",
           "Bell",
           "Low Cut48",
           "Low Shelf",
           "High Cut12",
           "High Cut48"
          ]
         }
        },
        {
         "name": "8 Frequency A",
         "original_name": "8 Frequency A",
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
        },
        {
         "name": "8 Gain A",
         "original_name": "8 Gain A",
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
        },
        {
         "name": "8 Resonance A",
         "original_name": "8 Resonance A",
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
        "global_mode": 0,
        "edit_mode": false,
        "oversample": false
       }
      }
     ],
     "opaque_state_observed": false
    },
    "device_identity_fingerprint": "020ac0912085f91904eadc84532fe408ea9cd66a96491b469f1beb99eca0a92a"
   },
   "before_device_identity": {
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
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack",
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
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay",
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
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release",
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
       },
       {
        "name": "LP Type",
        "original_name": "LP Type",
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
          "II",
          "I"
         ]
        }
       }
      ],
      "modes": {
       "voice_mode": {
        "index": 1,
        "value_items": [
         "Mono",
         "Poly",
       )MODEFIXTURE" + R"MODEFIXTURE(  "Unison",
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
     },
     {
      "device_key": "effect",
      "browser_name": "EQ Eight",
      "class_name": "Eq8",
      "class_display_name": "EQ Eight",
      "name": "EQ Eight",
      "type": 2,
      "role": "effect",
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
        "name": "Adaptive Q",
        "original_name": "Adaptive Q",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "Scale",
        "original_name": "Scale",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.70709228515625,
         "default_value": 0.25,
         "is_quantized": false,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": []
        }
       },
       {
        "name": "Output Gain",
        "original_name": "Output Gain",
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
       },
       {
        "name": "1 Filter On A",
        "original_name": "1 Filter On A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "1 Filter Type A",
        "original_name": "1 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 3.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "1 Frequency A",
        "original_name": "1 Frequency A",
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
       },
       {
        "name": "1 Gain A",
        "original_name": "1 Gain A",
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
       },
       {
        "name": "1 Resonance A",
        "original_name": "1 Resonance A",
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
       },
       {
        "name": "2 Filter On A",
        "original_name": "2 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "2 Filter Type A",
        "original_name": "2 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "2 Frequency A",
        "original_name": "2 Frequency A",
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
       },
       {
        "name": "2 Gain A",
        "original_name": "2 Gain A",
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
       },
       {
        "name": "2 Resonance A",
        "original_name": "2 Resonance A",
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
       },
       {
        "name": "3 Filter On A",
        "original_name": "3 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "3 Filter Type A",
        "original_name": "3 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "3 Frequency A",
        "original_name": "3 Frequency A",
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
       },
       {
        "name": "3 Gain A",
        "original_name": "3 Gain A",
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
       },
       {
        "name": "3 Resonance A",
        "original_name": "3 Resonance A",
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
       },
       {
        "name": "4 Filter On A",
        "original_name": "4 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "4 Filter Type A",
        "original_name": "4 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "4 Frequency A",
        "original_name": "4 Frequency A",
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
       },
       {
        "name": "4 Gain A",
        "original_name": "4 Gain A",
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
       },
       {
        "name": "4 Resonance A",
        "original_name": "4 Resonance A",
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
       },
       {
        "name": "5 Filter On A",
        "original_name": "5 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "5 Filter Type A",
        "original_name": "5 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "5 Frequency A",
        "original_name": "5 Frequency A",
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
       },
       {
        "name": "5 Gain A",
        "original_name": "5 Gain A",
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
       },
       {
        "name": "5 Resonance A",
        "original_name": "5 Resonance A",
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
       },
       {
        "name": "6 Filter On A",
        "original_name": "6 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "6 Filter Type A",
        "original_name": "6 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "6 Frequency A",
        "original_name": "6 Frequency A",
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
       },
       {
        "name": "6 Gain A",
        "original_name": "6 Gain A",
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
       },
       {
        "name": "6 Resonance A",
        "original_name": "6 Resonance A",
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
       },
       {
        "name": "7 Filter On A",
        "original_name": "7 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "7 Filter Type A",
        "original_name": "7 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "7 Frequency A",
        "original_name": "7 Frequency A",
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
       },
       {
        "name": "7 Gain A",
        "original_name": "7 Gain A",
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
       },
       {
        "name": "7 Resonance A",
        "original_name": "7 Resonance A",
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
       },
       {
        "name": "8 Filter On A",
        "original_name": "8 Filter On A",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "8 Filter Type A",
        "original_name": "8 Filter Type A",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 7.0,
         "value": 5.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "High Shelf",
          "Notch",
          "Low Cut12",
          "Bell",
          "Low Cut48",
          "Low Shelf",
          "High Cut12",
          "High Cut48"
         ]
        }
       },
       {
        "name": "8 Frequency A",
        "original_name": "8 Frequency A",
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
       },
       {
        "name": "8 Gain A",
        "original_name": "8 Gain A",
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
       },
       {
        "name": "8 Resonance A",
        "original_name": "8 Resonance A",
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
       "global_mode": 0,
       "edit_mode": false,
       "oversample": false
      }
     }
    ],
    "opaque_state_observed": false
   },
   "before_device_identity_fingerprint": "020ac0912085f91904eadc84532fe408ea9cd66a96491b469f1beb99eca0a92a",
   "device_key": "effect",
   "admitted_modes": [
    {
     "intent": {
      "capability_id": "eq8.enabled",
      "label": "Off"
     },
     "parameter_index": 0,
     "parameter_original_name": "Device On",
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
     },
     "target_internal": 0.0
    }
   ],
   "admitted_properties": [],
   "readbacks": [
    {
     "capability_id": "eq8.enabled",
     "parameter_index": 0,
     "internal_value": 0.0,
     "label": "Off"
    }
   ],
   "property_readbacks": [],
   "clip_and_note_ids_preserved": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "bypass_preview_request": {
  "document_token": "6ecf3677129d4d94a362f7e09147450b",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "devices": [
   {
    "device_key": "current_source",
    "chain_index": 0,
    "device": {
     "browser_name": "Drift",
     "class_name": "Drift",
     "type": 1,
     "role": "source",
     "insertion_policy": "AppendOwnedChain"
    },
    "physical_intents": []
   },
   {
    "device_key": "current_effect",
    "chain_index": 1,
    "device": {
     "browser_name": "Utility",
     "class_name": "StereoGain",
     "type": 2,
     "role": "effect",
     "insertion_policy": "AppendOwnedChain"
    },
    "physical_intents": [],
    "enum_intents": [
     {
      "capability_id": "utility.enabled",
      "label": "Off"
     }
    ],
    "property_intents": [],
    "authored_bypass": true
   }
  ]
 },
 "bypass_preview": {
  "schema_version": 1,
  "binding_observation": {
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
   "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
     }
    ]
   },
   "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
      "device_key": "current_source",
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
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack",
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
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay",
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
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release",
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
       },
       {
        "name": "LP Type",
        "original_name": "LP Type",
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
          "II",
          "I"
         ]
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
       "voic)MODEFIXTURE" + R"MODEFIXTURE(e_count": {
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
     },
     {
      "device_key": "current_effect",
      "browser_name": "Utility",
      "class_name": "StereoGain",
      "class_display_name": "Utility",
      "name": "Utility",
      "type": 2,
      "role": "effect",
      "is_active": false,
      "can_have_chains": false,
      "parameters": [
       {
        "name": "Device On",
        "original_name": "Device On",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
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
        "name": "Gain",
        "original_name": "Gain",
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
       },
       {
        "name": "Balance",
        "original_name": "Balance",
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
       },
       {
        "name": "Stereo Width",
        "original_name": "Stereo Width",
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
       },
       {
        "name": "Channel Mode",
        "original_name": "Channel Mode",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 3.0,
         "value": 1.0,
         "default_value": null,
         "is_quantized": true,
         "is_enabled": true,
         "state": 0,
         "automation_state": 0,
         "value_items": [
          "Right",
          "Stereo",
          "Swap",
          "Left"
         ]
        }
       },
       {
        "name": "Mono",
        "original_name": "Mono",
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
          "On",
          "Off"
         ]
        }
       },
       {
        "name": "Mute",
        "original_name": "Mute",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.0,
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
       }
      ],
      "modes": {}
     }
    ],
    "opaque_state_observed": false
   },
   "device_identity_fingerprint": "6514d6d6608bc3e82e718ace161b5092039102586428cd2492a3c6f784132497"
  },
  "devices": [
   {
    "device_key": "current_source",
    "chain_index": 0,
    "device": {
     "browser_name": "Drift",
     "class_name": "Drift",
     "type": 1,
     "role": "source",
     "insertion_policy": "AppendOwnedChain"
    },
    "physical_intents": []
   },
   {
    "device_key": "current_effect",
    "chain_index": 1,
    "device": {
     "browser_name": "Utility",
     "class_name": "StereoGain",
     "type": 2,
     "role": "effect",
     "insertion_policy": "AppendOwnedChain"
    },
    "physical_intents": [],
    "enum_intents": [
     {
      "capability_id": "utility.enabled",
      "label": "Off"
     }
    ],
    "property_intents": [],
    "authored_bypass": true
   }
  ],
  "resolutions": [],
  "authority_origin": "explicit_current_device_adoption",
  "native_mutation_started": false,
  "native_knob_only": true,
  "host_qualified": false,
  "opaque_state_observed": false
 },
 "bypass_adoption_request": {
  "document_token": "6ecf3677129d4d94a362f7e09147450b",
  "project_key": "project_a",
  "binding_key": "part_a",
  "operation_id": "adopt_devices",
  "preview_token": "ceaa7ca18e224eefa0e88cf5dd09619d",
  "approved_preview": {
   "schema_version": 1,
   "binding_observation": {
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
    "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
      }
     ]
    },
    "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
       "device_key": "current_source",
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
        },
        {
         "name": "Env 1 Attack",
         "original_name": "Env 1 Attack",
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
        },
        {
         "name": "Env 1 Decay",
         "original_name": "Env 1 Decay",
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
        },
        {
         "name": "Env 1 Release",
         "original_name": "Env 1 Release",
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
        },
        {
         "name": "LP Type",
         "original_name": "LP Type",
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
           "II",
           "I"
          ]
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
      },
      {
       "device_key": "current_effect",
       "browser_name": "Utility",
       "class_name": "StereoGain",
       "class_display_name": "Utility",
       "name": "Utility",
       "type": 2,
       "role": "effect",
       "is_active": false,
       "can_have_chains": false,
       "parameters": [
        {
         "name": "Device On",
         "original_name": "Device On",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
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
         "name": "Gain",
         "original_name": "Gain",
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
        },
        {
         "name": "Balance",
         "original_name": "Balance",
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
        },
        {
         "name": "Stereo Width",
         "original_name": "Stereo Width",
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
        },
        {
         "name": "Channel Mode",
         "original_name": "Channel Mode",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 3.0,
          "value": 1.0,
          "default_value": null,
          "is_quantized": true,
          "is_enabled": true,
          "state": 0,
          "automation_state": 0,
          "value_items": [
           "Right",
           "Stereo",
           "Swap",
           "Left"
          ]
         }
        },
        {
         "name": "Mono",
         "original_name": "Mono",
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
           "On",
           "Off"
          ]
         }
        },
        {
         "name": "Mute",
         "original_name": "Mute",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.0,
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
        }
       ],
       "modes": {}
      }
     ],
     "opaque_state_observed": false
    },
    "device_identity_fingerprint": "6514d6d6608bc3e82e718ace161b5092039102586428cd2492a3c6f784132497"
   },
   "devices": [
    {
     "device_key": "current_source",
     "chain_index": 0,
     "device": {
      "browser_name": "Drift",
      "class_name": "Drift",
      "type": 1,
      "role": "source",
      "insertion_policy": "AppendOwnedChain"
     },
     "physical_intents": []
    },
    {
     "device_key": "current_effect",
     "chain_index": 1,
     "device": {
      "browser_name": "Utility",
      "class_name": "StereoGain",
      "type": 2,
      "role": "effect",
      "insertion_policy": "AppendOwnedChain"
     },
     "physical_intents": [],
     "enum_intents": [
      {
       "capability_id": "utility.enabled",
       "label": "Off"
      }
     ],
     "property_intents": [],
     "authored_bypass": true
    }
   ],
   "resolutions": [],
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "bypass_adoption_result": {
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
  "content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
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
    }
   ]
  },
  "note_identity_fingerprint": "f69c3ef4ce93b997267f5f27e8d2bd07545f83568525e4153304da8a1f2bb4c4",
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
     "device_key": "current_source",
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
      },
      {
       "name": "Env 1 Attack",
       "original_name": "Env 1 Attack",
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
      },
      {
       "name": "Env 1 Decay",
       "original_name": "Env 1 Decay",
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
      },
      {
       "name": "Env 1 Release",
       "original_name": "Env 1 Release",
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
      },
      {
       "name": "LP Type",
       "original_name": "LP Type",
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
         "II",
         "I"
        ]
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
    },
    {
     "device_key": "current_effect",
     "browser_name": "Utility",
     "class_name": "StereoGain",
     "class_display_name": "Utility",
     "name": "Utility",
     "type": 2,
     "role": "effect",
     "is_active": false,
     "can_have_chains": false,
     "parameters": [
      {
       "name": "Device On",
       "original_name": "Device On",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
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
       "name": "Gain",
       "original_name": "Gain",
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
      },
      {
       "name": "Balance",
       "original_name": "Balance",
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
      },
      {
       "name": "Stereo Width",
       "original_name": "Stereo Width",
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
      },
      {
       "name": "Channel Mode",
       "original_name": "Channel Mode",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 3.0,
        "value": 1.0,
        "default_value": null,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Right",
         "Stereo",
         "Swap",
         "Left"
        ]
       }
      },
      {
       "name": "Mono",
       "original_name": "Mono",
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
         "On",
         "Off"
        ]
       }
      },
      {
       "name": "Mute",
       "original_name": "Mute",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
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
      }
     ],
     "modes": {}
    }
   ],
   "opaque_state_observed": false
  },
  "device_identity_fingerprint": "6514d6d6608bc3e82e718ace161b5092039102586428cd2492a3c6f784132497",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true,
  "device_adoption": {
   "preview_token": "ceaa7ca18e224eefa0e88cf5dd09619d",
   "preview_fingerprint": "f9f2d7003afc92811b37d32b5fb8a8f2d9a8d309c9b09723654a76305c27680f",
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "current_values_match_approved_intent": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 }
})MODEFIXTURE"));
}
