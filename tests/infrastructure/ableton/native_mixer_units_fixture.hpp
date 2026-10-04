/** Literal offline native MixerDevice source candidate; not host qualification. */
#pragma once
#include <nlohmann/json.hpp>
inline nlohmann::json native_mixer_units_fixture() {
    return nlohmann::json::parse(R"MIXERFIX({
 "nonlinear": {
  "schema_version": 1,
  "unit": "Decibels",
  "target": -6.0,
  "display_tolerance": 0.0,
  "internal_value": 0.5,
  "display": "-6.00 dB",
  "display_value": -6.0,
  "display_increment": 0.01,
  "absolute_display_error": 0.0,
  "balance_full_scale": null,
  "descriptor": {
   "minimum": 0.0,
   "maximum": 1.0,
   "value": 0.375,
   "is_quantized": false,
   "is_enabled": true,
   "state": 0,
   "automation_state": 0,
   "default_value": 0.375
  },
  "formatter_calls": 21,
  "samples": [
   {
    "internal_value": 0.0,
    "display": "-40.00 dB",
    "phase": "grid",
    "display_value": -40.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.0625,
    "display": "-39.93 dB",
    "phase": "grid",
    "display_value": -39.93,
    "negative_infinity": false
   },
   {
    "internal_value": 0.125,
    "display": "-39.47 dB",
    "phase": "grid",
    "display_value": -39.47,
    "negative_infinity": false
   },
   {
    "internal_value": 0.1875,
    "display": "-38.21 dB",
    "phase": "grid",
    "display_value": -38.21,
    "negative_infinity": false
   },
   {
    "internal_value": 0.25,
    "display": "-35.75 dB",
    "phase": "grid",
    "display_value": -35.75,
    "negative_infinity": false
   },
   {
    "internal_value": 0.3125,
    "display": "-31.70 dB",
    "phase": "grid",
    "display_value": -31.7,
    "negative_infinity": false
   },
   {
    "internal_value": 0.375,
    "display": "-25.66 dB",
    "phase": "grid",
    "display_value": -25.66,
    "negative_infinity": false
   },
   {
    "internal_value": 0.4375,
    "display": "-17.22 dB",
    "phase": "grid",
    "display_value": -17.22,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5,
    "display": "-6.00 dB",
    "phase": "grid",
    "display_value": -6.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5625,
    "display": "8.41 dB",
    "phase": "grid",
    "display_value": 8.41,
    "negative_infinity": false
   },
   {
    "internal_value": 0.625,
    "display": "26.41 dB",
    "phase": "grid",
    "display_value": 26.41,
    "negative_infinity": false
   },
   {
    "internal_value": 0.6875,
    "display": "48.39 dB",
    "phase": "grid",
    "display_value": 48.39,
    "negative_infinity": false
   },
   {
    "internal_value": 0.75,
    "display": "74.75 dB",
    "phase": "grid",
    "display_value": 74.75,
    "negative_infinity": false
   },
   {
    "internal_value": 0.8125,
    "display": "105.89 dB",
    "phase": "grid",
    "display_value": 105.89,
    "negative_infinity": false
   },
   {
    "internal_value": 0.875,
    "display": "142.22 dB",
    "phase": "grid",
    "display_value": 142.22,
    "negative_infinity": false
   },
   {
    "internal_value": 0.9375,
    "display": "184.12 dB",
    "phase": "grid",
    "display_value": 184.12,
    "negative_infinity": false
   },
   {
    "internal_value": 1.0,
    "display": "232.00 dB",
    "phase": "grid",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.0,
    "display": "-40.00 dB",
    "phase": "repeat",
    "display_value": -40.0,
    "negative_infinity": false
   },
   {
    "internal_value": 1.0,
    "display": "232.00 dB",
    "phase": "repeat",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5,
    "display": "-6.00 dB",
    "phase": "repeat",
    "display_value": -6.0,
    "negative_infinity": false
   }
  ],
  "current_display": {
   "internal_value": 0.375,
   "display": "-25.66 dB",
   "display_value": -25.66,
   "display_increment": 0.01,
   "negative_infinity": false
  },
  "parameter_kind": "volume",
  "parameter_name": "Track Volume",
  "parameter_original_name": "Track Volume",
  "mixer_capture": {
   "panning_mode": 0,
   "crossfade_assign": 1,
   "parameters": [
    {
     "kind": "volume",
     "send_index": null,
     "name": "Track Volume",
     "original_name": "Track Volume",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.375,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.375
     }
    },
    {
     "kind": "panning",
     "send_index": null,
     "name": "Track Panning",
     "original_name": "Track Panning",
     "descriptor": {
      "minimum": -1.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    },
    {
     "kind": "track_activator",
     "send_index": null,
     "name": "Speaker On",
     "original_name": "Speaker On",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 1.0,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "Off",
       "On"
      ],
      "label": "On"
     }
    },
    {
     "kind": "send",
     "send_index": 0,
     "name": "Send A",
     "original_name": "Send A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    }
   ]
  },
  "track_context": {
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
  "device_cohort_count": 0,
  "qualification": "ObservedNativeMixerDisplayCandidate",
  "source_commit": "e83d5192f321b24eb9daab843ac49a2d95d862b1",
  "host_qualified": false,
  "native_knob_only": true,
  "coverage_limits": [
   "Tolerance compares displayed dB; no hidden rounding-error bound is inferred.",
   "Sampled monotonicity does not prove a complete fader transfer function.",
   "A finite volume candidate does not grant write or automation/envelope authority.",
   "Device opaque state and known owned parameter guards remain separately retained.",
   "Native formatted readback after any authorized write must be independently verified.",
   "Version, edition, operating system, loudness and persistence qualification remain separate."
  ]
 },
 "nonstandard": {
  "schema_version": 1,
  "unit": "Decibels",
  "target": -6.0,
  "display_tolerance": 0.0,
  "internal_value": 2.0,
  "display": "-6.00 dB",
  "display_value": -6.0,
  "display_increment": 0.01,
  "absolute_display_error": 0.0,
  "balance_full_scale": null,
  "descriptor": {
   "minimum": -2.0,
   "maximum": 6.0,
   "value": 1.0,
   "is_quantized": false,
   "is_enabled": true,
   "state": 0,
   "automation_state": 0,
   "default_value": 0.0
  },
  "formatter_calls": 21,
  "samples": [
   {
    "internal_value": -2.0,
    "display": "-40.00 dB",
    "phase": "grid",
    "display_value": -40.0,
    "negative_infinity": false
   },
   {
    "internal_value": -1.5,
    "display": "-39.93 dB",
    "phase": "grid",
    "display_value": -39.93,
    "negative_infinity": false
   },
   {
    "internal_value": -1.0,
    "display": "-39.47 dB",
    "phase": "grid",
    "display_value": -39.47,
    "negative_infinity": false
   },
   {
    "internal_value": -0.5,
    "display": "-38.21 dB",
    "phase": "grid",
    "display_value": -38.21,
    "negative_infinity": false
   },
   {
    "internal_value": 0.0,
    "display": "-35.75 dB",
    "phase": "grid",
    "display_value": -35.75,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5,
    "display": "-31.70 dB",
    "phase": "grid",
    "display_value": -31.7,
    "negative_infinity": false
   },
   {
    "internal_value": 1.0,
    "display": "-25.66 dB",
    "phase": "grid",
    "display_value": -25.66,
    "negative_infinity": false
   },
   {
    "internal_value": 1.5,
    "display": "-17.22 dB",
    "phase": "grid",
    "display_value": -17.22,
    "negative_infinity": false
   },
   {
    "internal_value": 2.0,
    "display": "-6.00 dB",
    "phase": "grid",
    "display_value": -6.0,
    "negative_infinity": false
   },
   {
    "internal_value": 2.5,
    "display": "8.41 dB",
    "phase": "grid",
    "display_value": 8.41,
    "negative_infinity": false
   },
   {
    "internal_value": 3.0,
    "display": "26.41 dB",
    "phase": "grid",
    "display_value": 26.41,
    "negative_infinity": false
   },
   {
    "internal_value": 3.5,
    "display": "48.39 dB",
    "phase": "grid",
    "display_value": 48.39,
    "negative_infinity": false
   },
   {
    "internal_value": 4.0,
    "display": "74.75 dB",
    "phase": "grid",
    "display_value": 74.75,
    "negative_infinity": false
   },
   {
    "internal_value": 4.5,
    "display": "105.89 dB",
    "phase": "grid",
    "display_value": 105.89,
    "negative_infinity": false
   },
   {
    "internal_value": 5.0,
    "display": "142.22 dB",
    "phase": "grid",
    "display_value": 142.22,
    "negative_infinity": false
   },
   {
    "internal_value": 5.5,
    "display": "184.12 dB",
    "phase": "grid",
    "display_value": 184.12,
    "negative_infinity": false
   },
   {
    "internal_value": 6.0,
    "display": "232.00 dB",
    "phase": "grid",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": -2.0,
    "display": "-40.00 dB",
    "phase": "repeat",
    "display_value": -40.0,
    "negative_infinity": false
   },
   {
    "internal_value": 6.0,
    "display": "232.00 dB",
    "phase": "repeat",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": 2.0,
    "display": "-6.00 dB",
    "phase": "repeat",
    "display_value": -6.0,
    "negative_infinity": false
   }
  ],
  "current_display": {
   "internal_value": 1.0,
   "display": "-25.66 dB",
   "display_value": -25.66,
   "display_increment": 0.01,
   "negative_infinity": false
  },
  "parameter_kind": "volume",
  "parameter_name": "Track Volume",
  "parameter_original_name": "Track Volume",
  "mixer_capture": {
   "panning_mode": 0,
   "crossfade_assign": 1,
   "parameters": [
    {
     "kind": "volume",
     "send_index": null,
     "name": "Track Volume",
     "original_name": "Track Volume",
     "descriptor": {
      "minimum": -2.0,
      "maximum": 6.0,
      "value": 1.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    },
    {
     "kind": "panning",
     "send_index": null,
     "name": "Track Panning",
     "original_name": "Track Panning",
     "descriptor": {
      "minimum": -1.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    },
    {
     "kind": "track_activator",
     "send_index": null,
     "name": "Speaker On",
     "original_name": "Speaker On",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 1.0,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "Off",
       "On"
      ],
      "label": "On"
     }
    },
    {
     "kind": "send",
     "send_index": 0,
     "name": "Send A",
     "original_name": "Send A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    }
   ]
  },
  "track_context": {
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
  "device_cohort_count": 0,
  "qualification": "ObservedNativeMixerDisplayCandidate",
  "source_commit": "e83d5192f321b24eb9daab843ac49a2d95d862b1",
  "host_qualified": false,
  "native_knob_only": true,
  "coverage_limits": [
   "Tolerance compares displayed dB; no hidden rounding-error bound is inferred.",
   "Sampled monotonicity does not prove a complete fader transfer function.",
   "A finite volume candidate does not grant write or automation/envelope authority.",
   "Device opaque state and known owned parameter guards remain separately retained.",
   "Native formatted readback after any authorized write must be independently verified.",
   "Version, edition, operating system, loudness and persistence qualification remain separate."
  ]
 },
 "infinity": {
  "schema_version": 1,
  "unit": "Decibels",
  "target": -6.0,
  "display_tolerance": 0.0,
  "internal_value": 0.5,
  "display": "-6.00 dB",
  "display_value": -6.0,
  "display_increment": 0.01,
  "absolute_display_error": 0.0,
  "balance_full_scale": null,
  "descriptor": {
   "minimum": 0.0,
   "maximum": 1.0,
   "value": 0.375,
   "is_quantized": false,
   "is_enabled": true,
   "state": 0,
   "automation_state": 0,
   "default_value": 0.375
  },
  "formatter_calls": 21,
  "samples": [
   {
    "internal_value": 0.0,
    "display": "-\u221e dB",
    "phase": "grid",
    "display_value": null,
    "negative_infinity": true
   },
   {
    "internal_value": 0.0625,
    "display": "-39.93 dB",
    "phase": "grid",
    "display_value": -39.93,
    "negative_infinity": false
   },
   {
    "internal_value": 0.125,
    "display": "-39.47 dB",
    "phase": "grid",
    "display_value": -39.47,
    "negative_infinity": false
   },
   {
    "internal_value": 0.1875,
    "display": "-38.21 dB",
    "phase": "grid",
    "display_value": -38.21,
    "negative_infinity": false
   },
   {
    "internal_value": 0.25,
    "display": "-35.75 dB",
    "phase": "grid",
    "display_value": -35.75,
    "negative_infinity": false
   },
   {
    "internal_value": 0.3125,
    "display": "-31.70 dB",
    "phase": "grid",
    "display_value": -31.7,
    "negative_infinity": false
   },
   {
    "internal_value": 0.375,
    "display": "-25.66 dB",
    "phase": "grid",
    "display_value": -25.66,
    "negative_infinity": false
   },
   {
    "internal_value": 0.4375,
    "display": "-17.22 dB",
    "phase": "grid",
    "display_value": -17.22,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5,
    "display": "-6.00 dB",
    "phase": "grid",
    "display_value": -6.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5625,
    "display": "8.41 dB",
    "phase": "grid",
    "display_value": 8.41,
    "negative_infinity": false
   },
   {
    "internal_value": 0.625,
    "display": "26.41 dB",
    "phase": "grid",
    "display_value": 26.41,
    "negative_infinity": false
   },
   {
    "internal_value": 0.6875,
    "display": "48.39 dB",
    "phase": "grid",
    "display_value": 48.39,
    "negative_infinity": false
   },
   {
    "internal_value": 0.75,
    "display": "74.75 dB",
    "phase": "grid",
    "display_value": 74.75,
    "negative_infinity": false
   },
   {
    "internal_value": 0.8125,
    "display": "105.89 dB",
    "phase": "grid",
    "display_value": 105.89,
    "negative_infinity": false
   },
   {
    "internal_value": 0.875,
    "display": "142.22 dB",
    "phase": "grid",
    "display_value": 142.22,
    "negative_infinity": false
   },
   {
    "internal_value": 0.9375,
    "display": "184.12 dB",
    "phase": "grid",
    "display_value": 184.12,
    "negative_infinity": false
   },
   {
    "internal_value": 1.0,
    "display": "232.00 dB",
    "phase": "grid",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.0,
    "display": "-\u221e dB",
    "phase": "repeat",
    "display_value": null,
    "negative_infinity": true
   },
   {
    "internal_value": 1.0,
    "display": "232.00 dB",
    "phase": "repeat",
    "display_value": 232.0,
    "negative_infinity": false
   },
   {
    "internal_value": 0.5,
    "display": "-6.00 dB",
    "phase": "repeat",
    "display_value": -6.0,
    "negative_infinity": false
   }
  ],
  "current_display": {
   "internal_value": 0.375,
   "display": "-25.66 dB",
   "display_value": -25.66,
   "display_increment": 0.01,
   "negative_infinity": false
  },
  "parameter_kind": "volume",
  "parameter_name": "Track Volume",
  "parameter_original_name": "Track Volume",
  "mixer_capture": {
   "panning_mode": 0,
   "crossfade_assign": 1,
   "parameters": [
    {
     "kind": "volume",
     "send_index": null,
     "name": "Track Volume",
     "original_name": "Track Volume",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.375,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.375
     }
    },
    {
     "kind": "panning",
     "send_index": null,
     "name": "Track Panning",
     "original_name": "Track Panning",
     "descriptor": {
      "minimum": -1.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    },
    {
     "kind": "track_activator",
     "send_index": null,
     "name": "Speaker On",
     "original_name": "Speaker On",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 1.0,
      "is_quantized": true,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "value_items": [
       "Off",
       "On"
      ],
      "label": "On"
     }
    },
    {
     "kind": "send",
     "send_index": 0,
     "name": "Send A",
     "original_name": "Send A",
     "descriptor": {
      "minimum": 0.0,
      "maximum": 1.0,
      "value": 0.0,
      "is_quantized": false,
      "is_enabled": true,
      "state": 0,
      "automation_state": 0,
      "default_value": 0.0
     }
    }
   ]
  },
  "track_context": {
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
  "device_cohort_count": 0,
  "qualification": "ObservedNativeMixerDisplayCandidate",
  "source_commit": "e83d5192f321b24eb9daab843ac49a2d95d862b1",
  "host_qualified": false,
  "native_knob_only": true,
  "coverage_limits": [
   "Tolerance compares displayed dB; no hidden rounding-error bound is inferred.",
   "Sampled monotonicity does not prove a complete fader transfer function.",
   "A finite volume candidate does not grant write or automation/envelope authority.",
   "Device opaque state and known owned parameter guards remain separately retained.",
   "Native formatted readback after any authorized write must be independently verified.",
   "Version, edition, operating system, loudness and persistence qualification remain separate."
  ]
 }
})MIXERFIX");
}
