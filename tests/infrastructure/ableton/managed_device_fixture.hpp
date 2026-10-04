/** Actual offline native-provider ACKs; quantized defaults throw, no host qualification. */
#pragma once
#include <nlohmann/json.hpp>

inline nlohmann::json managed_device_fixture() {
    return nlohmann::json::parse(R"fixture({
 "source_request": {
  "document_token": "document_a",
  "operation_id": "source",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
  "expected_device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5",
  "device_key": "source",
  "device": {
   "browser_name": "Drift",
   "class_name": "Drift",
   "type": 1,
   "role": "source",
   "insertion_policy": "AppendOwnedChain"
  },
  "physical_intents": [
   {
    "capability_id": "drift.lp.frequency",
    "target": 1200.0,
    "tolerance": 0.0
   }
  ]
 },
 "source_result": {
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
        "value": 0.5,
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
    }
   ],
   "opaque_state_observed": false
  },
  "device_identity_fingerprint": "339ce51ffcd8e18b347d690f7ab9535954402b7b27828138e047fbcc91acf42f",
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
    "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
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
   "before_device_identity": {
    "schema_version": 1,
    "cohort": [],
    "opaque_state_observed": false
   },
   "before_device_identity_fingerprint": "f5b3bc3e201756712c75bc0d5f34d2ea7c280311e1644356fdb9744d06f7d6b5",
   "inserted": true,
   "device_key": "source",
   "resolutions": [
    {
     "intent": {
      "capability_id": "drift.lp.frequency",
      "target": 1200.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "Drift",
      "parameter_original_name": "LP Freq",
      "parameter_index": 1,
      "unit": "Hertz",
      "target": 1200.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "1200.00 Hz",
      "display_value": 1200.0,
      "display_increment": 0.01,
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
       },
       "LP Type": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "II",
         "I"
        ],
        "label": "I"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "LP Freq",
        "original_name": "LP Freq"
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack"
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay"
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release"
       },
       {
        "name": "LP Type",
        "original_name": "LP Type"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "grid",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "215.62 Hz",
        "phase": "grid",
        "display_value": 215.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "262.50 Hz",
        "phase": "grid",
        "display_value": 262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "340.62 Hz",
        "phase": "grid",
        "display_value": 340.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "450.00 Hz",
        "phase": "grid",
        "display_value": 450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "590.62 Hz",
        "phase": "grid",
        "display_value": 590.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "762.50 Hz",
        "phase": "grid",
        "display_value": 762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "965.62 Hz",
        "phase": "grid",
        "display_value": 965.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "grid",
        "display_value": 1200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1465.62 Hz",
        "phase": "grid",
        "display_value": 1465.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "1762.50 Hz",
        "phase": "grid",
        "display_value": 1762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "2090.62 Hz",
        "phase": "grid",
        "display_value": 2090.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "2450.00 Hz",
        "phase": "grid",
        "display_value": 2450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "2840.62 Hz",
        "phase": "grid",
        "display_value": 2840.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "3262.50 Hz",
        "phase": "grid",
        "display_value": 3262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "3715.62 Hz",
        "phase": "grid",
        "display_value": 3715.62,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "grid",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "repeat",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "repeat",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "repeat",
        "display_value": 1200.0,
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
     "capability_id": "drift.lp.frequency",
     "internal_value": 0.5,
     "display": "1200.00 Hz",
     "display_value": 1200.0,
     "display_increment": 0.01,
     "absolute_display_error": 0.0,
     "matches_intent": true,
     "formatter_calls": 1
    }
   ],
   "clip_and_note_ids_preserved": true,
   "output_role_transition": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "effect_request": {
  "document_token": "document_a",
  "operation_id": "effect",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "expected_device_identity_fingerprint": "339ce51ffcd8e18b347d690f7ab9535954402b7b27828138e047fbcc91acf42f",
  "device_key": "effect",
  "device": {
   "browser_name": "Utility",
   "class_name": "StereoGain",
   "type": 2,
   "role": "effect",
   "insertion_policy": "AppendOwnedChain"
  },
  "physical_intents": [
   {
    "capability_id": "utility.gain",
    "target": 0.0,
    "tolerance": 0.0
   }
  ]
 },
 "effect_result": {
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
        "value": 0.5,
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
     "browser_name": "Utility",
     "class_name": "StereoGain",
     "class_display_name": "Utility",
     "name": "Utility",
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
       "name": "Gain",
       "original_name": "Gain",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.5,
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
  "device_identity_fingerprint": "7980a95282286a29db8bb0aadbab1c321b08b1b08acffef48a1a4c03dbe23033",
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
          "value": 0.5,
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
      }
     ],
     "opaque_state_observed": false
    },
    "device_identity_fingerprint": "339ce51ffcd8e18b347d690f7ab9535954402b7b27828138e047fbcc91acf42f"
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
         "value": 0.5,
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
     }
    ],
    "opaque_state_observed": false
   },
   "before_device_identity_fingerprint": "339ce51ffcd8e18b347d690f7ab9535954402b7b27828138e047fbcc91acf42f",
   "inserted": true,
   "device_key": "effect",
   "resolutions": [
    {
     "intent": {
      "capability_id": "utility.gain",
      "target": 0.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "StereoGain",
      "parameter_original_name": "Gain",
      "parameter_index": 1,
      "unit": "Decibels",
      "target": 0.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
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
      "modes": {
       "Channel Mode": {
        "minimum": 0.0,
        "maximum": 3.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Right",
         "Stereo",
         "Swap",
         "Left"
        ],
        "label": "Stereo"
       },
       "Mono": {
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
       },
       "Mute": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Off",
         "On"
        ],
        "label": "Off"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "Gain",
        "original_name": "Gain"
       },
       {
        "name": "Balance",
        "original_name": "Balance"
       },
       {
        "name": "Stereo Width",
        "original_name": "Stereo Width"
       },
       {
        "name": "Channel Mode",
        "original_name": "Channel Mode"
       },
       {
        "name": "Mono",
        "original_name": "Mono"
       },
       {
        "name": "Mute",
        "original_name": "Mute"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "grid",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "-10.50 dB",
        "phase": "grid",
        "display_value": -10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "-9.00 dB",
        "phase": "grid",
        "display_value": -9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "-7.50 dB",
        "phase": "grid",
        "display_value": -7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "-6.00 dB",
        "phase": "grid",
        "display_value": -6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "-4.50 dB",
        "phase": "grid",
        "display_value": -4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "-3.00 dB",
        "phase": "grid",
        "display_value": -3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "-1.50 dB",
        "phase": "grid",
        "display_value": -1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "grid",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1.50 dB",
        "phase": "grid",
        "display_value": 1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "3.00 dB",
        "phase": "grid",
        "display_value": 3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "4.50 dB",
        "phase": "grid",
        "display_value": 4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "6.00 dB",
        "phase": "grid",
        "display_value": 6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "7.50 dB",
        "phase": "grid",
        "display_value": 7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "9.00 dB",
        "phase": "grid",
        "display_value": 9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "10.50 dB",
        "phase": "grid",
        "display_value": 10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "grid",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "repeat",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "repeat",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "repeat",
        "display_value": 0.0,
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
     "capability_id": "utility.gain",
     "internal_value": 0.5,
     "display": "0.00 dB",
     "display_value": 0.0,
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
 "update_request": {
  "document_token": "document_a",
  "operation_id": "gain_update",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "9d1aa37fbfd7ce3cd993330d402bb268b19121f5113c9f96a241133c96f84fde",
  "expected_device_identity_fingerprint": "7980a95282286a29db8bb0aadbab1c321b08b1b08acffef48a1a4c03dbe23033",
  "device_key": "effect",
  "device": {
   "browser_name": "Utility",
   "class_name": "StereoGain",
   "type": 2,
   "role": "effect",
   "insertion_policy": "AppendOwnedChain"
  },
  "physical_intents": [
   {
    "capability_id": "utility.gain",
    "target": 0.0,
    "tolerance": 0.0
   }
  ]
 },
 "update_result": {
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
        "value": 0.5,
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
     "browser_name": "Utility",
     "class_name": "StereoGain",
     "class_display_name": "Utility",
     "name": "Utility",
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
       "name": "Gain",
       "original_name": "Gain",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.5,
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
  "device_identity_fingerprint": "7980a95282286a29db8bb0aadbab1c321b08b1b08acffef48a1a4c03dbe23033",
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
          "value": 0.5,
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
       "browser_name": "Utility",
       "class_name": "StereoGain",
       "class_display_name": "Utility",
       "name": "Utility",
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
         "name": "Gain",
         "original_name": "Gain",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.5,
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
    "device_identity_fingerprint": "7980a95282286a29db8bb0aadbab1c321b08b1b08acffef48a1a4c03dbe23033"
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
         "value": 0.5,
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
      "browser_name": "Utility",
      "class_name": "StereoGain",
      "class_display_name": "Utility",
      "name": "Utility",
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
        "name": "Gain",
        "original_name": "Gain",
        "descriptor": {
         "minimum": 0.0,
         "maximum": 1.0,
         "value": 0.5,
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
   "before_device_identity_fingerprint": "7980a95282286a29db8bb0aadbab1c321b08b1b08acffef48a1a4c03dbe23033",
   "inserted": false,
   "device_key": "effect",
   "resolutions": [
    {
     "intent": {
      "capability_id": "utility.gain",
      "target": 0.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "StereoGain",
      "parameter_original_name": "Gain",
      "parameter_index": 1,
      "unit": "Decibels",
      "target": 0.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.5,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.0
      },
      "modes": {
       "Channel Mode": {
        "minimum": 0.0,
        "maximum": 3.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Right",
         "Stereo",
         "Swap",
         "Left"
        ],
        "label": "Stereo"
       },
       "Mono": {
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
       },
       "Mute": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Off",
         "On"
        ],
        "label": "Off"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "Gain",
        "original_name": "Gain"
       },
       {
        "name": "Balance",
        "original_name": "Balance"
       },
       {
        "name": "Stereo Width",
        "original_name": "Stereo Width"
       },
       {
        "name": "Channel Mode",
        "original_name": "Channel Mode"
       },
       {
        "name": "Mono",
        "original_name": "Mono"
       },
       {
        "name": "Mute",
        "original_name": "Mute"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "grid",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "-10.50 dB",
        "phase": "grid",
        "display_value": -10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "-9.00 dB",
        "phase": "grid",
        "display_value": -9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "-7.50 dB",
        "phase": "grid",
        "display_value": -7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "-6.00 dB",
        "phase": "grid",
        "display_value": -6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "-4.50 dB",
        "phase": "grid",
        "display_value": -4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "-3.00 dB",
        "phase": "grid",
        "display_value": -3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "-1.50 dB",
        "phase": "grid",
        "display_value": -1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "grid",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1.50 dB",
        "phase": "grid",
        "display_value": 1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "3.00 dB",
        "phase": "grid",
        "display_value": 3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "4.50 dB",
        "phase": "grid",
        "display_value": 4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "6.00 dB",
        "phase": "grid",
        "display_value": 6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "7.50 dB",
        "phase": "grid",
        "display_value": 7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "9.00 dB",
        "phase": "grid",
        "display_value": 9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "10.50 dB",
        "phase": "grid",
        "display_value": 10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "grid",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "repeat",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "repeat",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "repeat",
        "display_value": 0.0,
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
     "capability_id": "utility.gain",
     "internal_value": 0.5,
     "display": "0.00 dB",
     "display_value": 0.0,
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
  "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
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
 "preview_request": {
  "document_token": "document_a",
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
    "physical_intents": [
     {
      "capability_id": "drift.lp.frequency",
      "target": 1200.0,
      "tolerance": 0.0
     }
    ]
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
    "physical_intents": [
     {
      "capability_id": "utility.gain",
      "target": 0.0,
      "tolerance": 0.0
     }
    ]
   }
  ]
 },
 "preview_response": {
  "outcome": "previewed",
  "document_token": "document_a",
  "project_key": "project_a",
  "binding_key": "part_a",
  "preview_token": "2f3253e29f15425fa479995165b6876b",
  "preview_fingerprint": "4127061657c076514398f66446a982b51e9191cd95d9d9fe6ea25d726ff96f84",
  "preview": {
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
          "value": 0.5,
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
         "name": "Gain",
         "original_name": "Gain",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.5,
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
    "device_identity_fingerprint": "69fe47bd09f4fc7be9d57b35c5f934d0ffadf357c90285ab31bb99d103143e9b"
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
     "physical_intents": [
      {
       "capability_id": "drift.lp.frequency",
       "target": 1200.0,
       "tolerance": 0.0
      }
     ]
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
     "physical_intents": [
      {
       "capability_id": "utility.gain",
       "target": 0.0,
       "tolerance": 0.0
      }
     ]
    }
   ],
   "resolutions": [
    {
     "device_key": "current_source",
     "intent": {
      "capability_id": "drift.lp.frequency",
      "target": 1200.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "Drift",
      "parameter_original_name": "LP Freq",
      "parameter_index": 1,
      "unit": "Hertz",
      "target": 1200.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "1200.00 Hz",
      "display_value": 1200.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.5,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.0
      },
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
       },
       "LP Type": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "II",
         "I"
        ],
        "label": "I"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "LP Freq",
        "original_name": "LP Freq"
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack"
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay"
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release"
       },
       {
        "name": "LP Type",
        "original_name": "LP Type"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "grid",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "215.62 Hz",
        "phase": "grid",
        "display_value": 215.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "262.50 Hz",
        "phase": "grid",
        "display_value": 262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "340.62 Hz",
        "phase": "grid",
        "display_value": 340.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "450.00 Hz",
        "phase": "grid",
        "display_value": 450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "590.62 Hz",
        "phase": "grid",
        "display_value": 590.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "762.50 Hz",
        "phase": "grid",
        "display_value": 762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "965.62 Hz",
        "phase": "grid",
        "display_value": 965.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "grid",
        "display_value": 1200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1465.62 Hz",
        "phase": "grid",
        "display_value": 1465.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "1762.50 Hz",
        "phase": "grid",
        "display_value": 1762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "2090.62 Hz",
        "phase": "grid",
        "display_value": 2090.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "2450.00 Hz",
        "phase": "grid",
        "display_value": 2450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "2840.62 Hz",
        "phase": "grid",
        "display_value": 2840.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "3262.50 Hz",
        "phase": "grid",
        "display_value": 3262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "3715.62 Hz",
        "phase": "grid",
        "display_value": 3715.62,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "grid",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "repeat",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "repeat",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "repeat",
        "display_value": 1200.0,
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
     },
     "current_readback": {
      "internal_value": 0.5,
      "display": "1200.00 Hz",
      "display_value": 1200.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "matches_intent": true,
      "formatter_calls": 1
     }
    },
    {
     "device_key": "current_effect",
     "intent": {
      "capability_id": "utility.gain",
      "target": 0.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "StereoGain",
      "parameter_original_name": "Gain",
      "parameter_index": 1,
      "unit": "Decibels",
      "target": 0.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.5,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.0
      },
      "modes": {
       "Channel Mode": {
        "minimum": 0.0,
        "maximum": 3.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Right",
         "Stereo",
         "Swap",
         "Left"
        ],
        "label": "Stereo"
       },
       "Mono": {
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
       },
       "Mute": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Off",
         "On"
        ],
        "label": "Off"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "Gain",
        "original_name": "Gain"
       },
       {
        "name": "Balance",
        "original_name": "Balance"
       },
       {
        "name": "Stereo Width",
        "original_name": "Stereo Width"
       },
       {
        "name": "Channel Mode",
        "original_name": "Channel Mode"
       },
       {
        "name": "Mono",
        "original_name": "Mono"
       },
       {
        "name": "Mute",
        "original_name": "Mute"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "grid",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "-10.50 dB",
        "phase": "grid",
        "display_value": -10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "-9.00 dB",
        "phase": "grid",
        "display_value": -9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "-7.50 dB",
        "phase": "grid",
        "display_value": -7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "-6.00 dB",
        "phase": "grid",
        "display_value": -6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "-4.50 dB",
        "phase": "grid",
        "display_value": -4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "-3.00 dB",
        "phase": "grid",
        "display_value": -3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "-1.50 dB",
        "phase": "grid",
        "display_value": -1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "grid",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1.50 dB",
        "phase": "grid",
        "display_value": 1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "3.00 dB",
        "phase": "grid",
        "display_value": 3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "4.50 dB",
        "phase": "grid",
        "display_value": 4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "6.00 dB",
        "phase": "grid",
        "display_value": 6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "7.50 dB",
        "phase": "grid",
        "display_value": 7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "9.00 dB",
        "phase": "grid",
        "display_value": 9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "10.50 dB",
        "phase": "grid",
        "display_value": 10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "grid",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "repeat",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "repeat",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "repeat",
        "display_value": 0.0,
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
     },
     "current_readback": {
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "matches_intent": true,
      "formatter_calls": 1
     }
    }
   ],
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "adopt_request": {
  "document_token": "document_a",
  "project_key": "project_a",
  "binding_key": "part_a",
  "operation_id": "adopt_devices",
  "preview_token": "2f3253e29f15425fa479995165b6876b",
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
          "value": 0.5,
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
         "name": "Gain",
         "original_name": "Gain",
         "descriptor": {
          "minimum": 0.0,
          "maximum": 1.0,
          "value": 0.5,
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
    "device_identity_fingerprint": "69fe47bd09f4fc7be9d57b35c5f934d0ffadf357c90285ab31bb99d103143e9b"
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
     "physical_intents": [
      {
       "capability_id": "drift.lp.frequency",
       "target": 1200.0,
       "tolerance": 0.0
      }
     ]
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
     "physical_intents": [
      {
       "capability_id": "utility.gain",
       "target": 0.0,
       "tolerance": 0.0
      }
     ]
    }
   ],
   "resolutions": [
    {
     "device_key": "current_source",
     "intent": {
      "capability_id": "drift.lp.frequency",
      "target": 1200.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "Drift",
      "parameter_original_name": "LP Freq",
      "parameter_index": 1,
      "unit": "Hertz",
      "target": 1200.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "1200.00 Hz",
      "display_value": 1200.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.5,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.0
      },
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
       },
       "LP Type": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "II",
         "I"
        ],
        "label": "I"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "LP Freq",
        "original_name": "LP Freq"
       },
       {
        "name": "Env 1 Attack",
        "original_name": "Env 1 Attack"
       },
       {
        "name": "Env 1 Decay",
        "original_name": "Env 1 Decay"
       },
       {
        "name": "Env 1 Release",
        "original_name": "Env 1 Release"
       },
       {
        "name": "LP Type",
        "original_name": "LP Type"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "grid",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "215.62 Hz",
        "phase": "grid",
        "display_value": 215.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "262.50 Hz",
        "phase": "grid",
        "display_value": 262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "340.62 Hz",
        "phase": "grid",
        "display_value": 340.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "450.00 Hz",
        "phase": "grid",
        "display_value": 450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "590.62 Hz",
        "phase": "grid",
        "display_value": 590.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "762.50 Hz",
        "phase": "grid",
        "display_value": 762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "965.62 Hz",
        "phase": "grid",
        "display_value": 965.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "grid",
        "display_value": 1200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1465.62 Hz",
        "phase": "grid",
        "display_value": 1465.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "1762.50 Hz",
        "phase": "grid",
        "display_value": 1762.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "2090.62 Hz",
        "phase": "grid",
        "display_value": 2090.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "2450.00 Hz",
        "phase": "grid",
        "display_value": 2450.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "2840.62 Hz",
        "phase": "grid",
        "display_value": 2840.62,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "3262.50 Hz",
        "phase": "grid",
        "display_value": 3262.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "3715.62 Hz",
        "phase": "grid",
        "display_value": 3715.62,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "grid",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "200.00 Hz",
        "phase": "repeat",
        "display_value": 200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "4200.00 Hz",
        "phase": "repeat",
        "display_value": 4200.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "1200.00 Hz",
        "phase": "repeat",
        "display_value": 1200.0,
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
     },
     "current_readback": {
      "internal_value": 0.5,
      "display": "1200.00 Hz",
      "display_value": 1200.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "matches_intent": true,
      "formatter_calls": 1
     }
    },
    {
     "device_key": "current_effect",
     "intent": {
      "capability_id": "utility.gain",
      "target": 0.0,
      "tolerance": 0.0
     },
     "candidate": {
      "schema_version": 1,
      "device_class_name": "StereoGain",
      "parameter_original_name": "Gain",
      "parameter_index": 1,
      "unit": "Decibels",
      "target": 0.0,
      "display_tolerance": 0.0,
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "balance_full_scale": null,
      "descriptor": {
       "minimum": 0.0,
       "maximum": 1.0,
       "value": 0.5,
       "is_quantized": false,
       "is_enabled": true,
       "state": 0,
       "automation_state": 0,
       "default_value": 0.0
      },
      "modes": {
       "Channel Mode": {
        "minimum": 0.0,
        "maximum": 3.0,
        "value": 1.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Right",
         "Stereo",
         "Swap",
         "Left"
        ],
        "label": "Stereo"
       },
       "Mono": {
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
       },
       "Mute": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.0,
        "is_quantized": true,
        "is_enabled": true,
        "state": 0,
        "automation_state": 0,
        "value_items": [
         "Off",
         "On"
        ],
        "label": "Off"
       }
      },
      "eq8_scale_display": null,
      "population": [
       {
        "name": "Device On",
        "original_name": "Device On"
       },
       {
        "name": "Gain",
        "original_name": "Gain"
       },
       {
        "name": "Balance",
        "original_name": "Balance"
       },
       {
        "name": "Stereo Width",
        "original_name": "Stereo Width"
       },
       {
        "name": "Channel Mode",
        "original_name": "Channel Mode"
       },
       {
        "name": "Mono",
        "original_name": "Mono"
       },
       {
        "name": "Mute",
        "original_name": "Mute"
       }
      ],
      "formatter_calls": 20,
      "samples": [
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "grid",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0625,
        "display": "-10.50 dB",
        "phase": "grid",
        "display_value": -10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.125,
        "display": "-9.00 dB",
        "phase": "grid",
        "display_value": -9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.1875,
        "display": "-7.50 dB",
        "phase": "grid",
        "display_value": -7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.25,
        "display": "-6.00 dB",
        "phase": "grid",
        "display_value": -6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.3125,
        "display": "-4.50 dB",
        "phase": "grid",
        "display_value": -4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.375,
        "display": "-3.00 dB",
        "phase": "grid",
        "display_value": -3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.4375,
        "display": "-1.50 dB",
        "phase": "grid",
        "display_value": -1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "grid",
        "display_value": 0.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5625,
        "display": "1.50 dB",
        "phase": "grid",
        "display_value": 1.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.625,
        "display": "3.00 dB",
        "phase": "grid",
        "display_value": 3.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.6875,
        "display": "4.50 dB",
        "phase": "grid",
        "display_value": 4.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.75,
        "display": "6.00 dB",
        "phase": "grid",
        "display_value": 6.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.8125,
        "display": "7.50 dB",
        "phase": "grid",
        "display_value": 7.5,
        "negative_infinity": false
       },
       {
        "internal_value": 0.875,
        "display": "9.00 dB",
        "phase": "grid",
        "display_value": 9.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.9375,
        "display": "10.50 dB",
        "phase": "grid",
        "display_value": 10.5,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "grid",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.0,
        "display": "-12.00 dB",
        "phase": "repeat",
        "display_value": -12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 1.0,
        "display": "12.00 dB",
        "phase": "repeat",
        "display_value": 12.0,
        "negative_infinity": false
       },
       {
        "internal_value": 0.5,
        "display": "0.00 dB",
        "phase": "repeat",
        "display_value": 0.0,
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
     },
     "current_readback": {
      "internal_value": 0.5,
      "display": "0.00 dB",
      "display_value": 0.0,
      "display_increment": 0.01,
      "absolute_display_error": 0.0,
      "matches_intent": true,
      "formatter_calls": 1
     }
    }
   ],
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "adopt_result": {
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
        "value": 0.5,
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
       "name": "Gain",
       "original_name": "Gain",
       "descriptor": {
        "minimum": 0.0,
        "maximum": 1.0,
        "value": 0.5,
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
  "device_identity_fingerprint": "69fe47bd09f4fc7be9d57b35c5f934d0ffadf357c90285ab31bb99d103143e9b",
  "observed_notes_match_request": true,
  "observed_clip_properties_match_request": true,
  "device_adoption": {
   "preview_token": "2f3253e29f15425fa479995165b6876b",
   "preview_fingerprint": "4127061657c076514398f66446a982b51e9191cd95d9d9fe6ea25d726ff96f84",
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "current_values_match_approved_intent": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "empty_preview_request": {
  "document_token": "document_a",
  "project_key": "project_a",
  "binding_key": "part_a",
  "expected_content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
  "devices": []
 },
 "empty_preview_response": {
  "outcome": "previewed",
  "document_token": "document_a",
  "project_key": "project_a",
  "binding_key": "part_a",
  "preview_token": "1c7aef6dffe44e24b151709907f1d2e5",
  "preview_fingerprint": "57189ccda73fc4f3111048fd187366774a3abfe0c81fe5ea6005e9003cd29762",
  "preview": {
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
    "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
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
   "devices": [],
   "resolutions": [],
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "empty_adopt_request": {
  "document_token": "document_a",
  "project_key": "project_a",
  "binding_key": "part_a",
  "operation_id": "adopt_empty",
  "preview_token": "1c7aef6dffe44e24b151709907f1d2e5",
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
    "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
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
   "devices": [],
   "resolutions": [],
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 },
 "empty_adopt_result": {
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
  "content_fingerprint": "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622",
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
  "observed_clip_properties_match_request": true,
  "device_adoption": {
   "preview_token": "1c7aef6dffe44e24b151709907f1d2e5",
   "preview_fingerprint": "57189ccda73fc4f3111048fd187366774a3abfe0c81fe5ea6005e9003cd29762",
   "authority_origin": "explicit_current_device_adoption",
   "native_mutation_started": false,
   "current_values_match_approved_intent": true,
   "native_knob_only": true,
   "host_qualified": false,
   "opaque_state_observed": false
  }
 }
})fixture");
}
