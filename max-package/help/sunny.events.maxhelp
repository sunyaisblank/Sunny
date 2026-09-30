{
  "patcher": {
    "fileversion": 1,
    "appversion": {
      "major": 8,
      "minor": 2,
      "revision": 0,
      "architecture": "x64",
      "modernui": 1
    },
    "rect": [80.0, 80.0, 890.0, 570.0],
    "bglocked": 0,
    "openinpresentation": 0,
    "default_fontsize": 12.0,
    "default_fontface": 0,
    "default_fontname": "Arial",
    "gridonopen": 1,
    "gridsize": [15.0, 15.0],
    "gridsnaponopen": 1,
    "boxes": [
      {
        "box": {
          "id": "title",
          "maxclass": "comment",
          "text": "sunny.events — bounded one-shot events on the unnamed global ITM transport",
          "patching_rect": [30.0, 20.0, 560.0, 20.0]
        }
      },
      {
        "box": {
          "id": "conditions",
          "maxclass": "comment",
          "linecount": 3,
          "text": "In Max for Live, the unnamed global transport synchronizes to Live. Sample-accurate delivery additionally requires Overdrive, Scheduler in Audio Interrupt, a high-priority command that arrives before its target, and a downstream object that supports sample-accurate events.",
          "patching_rect": [30.0, 50.0, 810.0, 52.0]
        }
      },
      {
        "box": {
          "id": "event",
          "maxclass": "message",
          "text": "event 960 60 100 23",
          "patching_rect": [30.0, 135.0, 120.0, 22.0]
        }
      },
      {
        "box": {
          "id": "note",
          "maxclass": "message",
          "text": "note 1440 64 240 100 91",
          "patching_rect": [30.0, 175.0, 155.0, 22.0]
        }
      },
      {
        "box": {
          "id": "event_after",
          "maxclass": "message",
          "text": "event_after 240 67 96 31",
          "patching_rect": [30.0, 105.0, 165.0, 22.0]
        }
      },
      {
        "box": {
          "id": "note_after",
          "maxclass": "message",
          "text": "note_after 480 69 240 104 47",
          "patching_rect": [205.0, 105.0, 205.0, 22.0]
        }
      },
      {
        "box": {
          "id": "clear",
          "maxclass": "message",
          "text": "clear",
          "patching_rect": [30.0, 230.0, 42.0, 22.0]
        }
      },
      {
        "box": {
          "id": "status",
          "maxclass": "message",
          "text": "status",
          "patching_rect": [82.0, 230.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "clear_error",
          "maxclass": "message",
          "text": "clear_error",
          "patching_rect": [142.0, 230.0, 70.0, 22.0]
        }
      },
      {
        "box": {
          "id": "transport_status",
          "maxclass": "message",
          "text": "transport_status",
          "patching_rect": [222.0, 230.0, 105.0, 22.0]
        }
      },
      {
        "box": {
          "id": "events",
          "maxclass": "newobj",
          "text": "sunny.events 480",
          "patching_rect": [315.0, 175.0, 112.0, 22.0]
        }
      },
      {
        "box": {
          "id": "event_order",
          "maxclass": "newobj",
          "text": "t l l l",
          "patching_rect": [315.0, 225.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "pitch",
          "maxclass": "newobj",
          "text": "zl nth 1",
          "patching_rect": [250.0, 270.0, 58.0, 22.0]
        }
      },
      {
        "box": {
          "id": "attack",
          "maxclass": "newobj",
          "text": "zl nth 2",
          "patching_rect": [330.0, 270.0, 58.0, 22.0]
        }
      },
      {
        "box": {
          "id": "release",
          "maxclass": "newobj",
          "text": "zl nth 3",
          "patching_rect": [410.0, 270.0, 58.0, 22.0]
        }
      },
      {
        "box": {
          "id": "attack_order",
          "maxclass": "newobj",
          "text": "t i i",
          "patching_rect": [330.0, 310.0, 38.0, 22.0]
        }
      },
      {
        "box": {
          "id": "note_flag",
          "maxclass": "newobj",
          "text": "> 0",
          "patching_rect": [390.0, 340.0, 32.0, 22.0]
        }
      },
      {
        "box": {
          "id": "selected_velocity",
          "maxclass": "newobj",
          "text": "if $i1 > 0 then $i1 else $i2",
          "patching_rect": [330.0, 380.0, 190.0, 22.0]
        }
      },
      {
        "box": {
          "id": "xnoteout",
          "maxclass": "newobj",
          "text": "xnoteout 1",
          "patching_rect": [250.0, 430.0, 72.0, 22.0]
        }
      },
      {
        "box": {
          "id": "transport",
          "maxclass": "newobj",
          "text": "transport",
          "patching_rect": [555.0, 175.0, 65.0, 22.0]
        }
      },
      {
        "box": {
          "id": "transport_hint",
          "maxclass": "comment",
          "linecount": 4,
          "text": "Use the global transport to inspect or control the demonstration timeline. sunny.events itself never mutates transport state or resolution. transport_status reports the public sampled position, resolution, running state, and SDK name.",
          "patching_rect": [555.0, 210.0, 275.0, 67.0]
        }
      },
      {
        "box": {
          "id": "capacity",
          "maxclass": "comment",
          "linecount": 4,
          "text": "Output is pitch, attack-or-zero velocity, release velocity. Relative delay 0 selects the first Sunny tick strictly after the sampled host tick; queue latency can still make it stale. Admission is bounded to 64 pending commands and 256 reserved event cells; equal-tick note-offs precede note-ons. Status proves SDK observation/action invocation, not host delivery.",
          "patching_rect": [30.0, 475.0, 520.0, 67.0]
        }
      },
      {
        "box": {
          "id": "midi_hint",
          "maxclass": "comment",
          "linecount": 4,
          "text": "The ordered adapter feeds xnoteout channel 1: note-on selects attack velocity; note-off selects release velocity and an explicit off flag. No midiout or port is selected, so this demonstrates formatting rather than MIDI delivery.",
          "patching_rect": [555.0, 320.0, 285.0, 67.0]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["event", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["note", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["event_after", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["note_after", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["clear", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["status", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["clear_error", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["transport_status", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["events", 0], "destination": ["event_order", 0]}},
      {"patchline": {"source": ["event_order", 0], "destination": ["pitch", 0]}},
      {"patchline": {"source": ["event_order", 1], "destination": ["attack", 0]}},
      {"patchline": {"source": ["event_order", 2], "destination": ["release", 0]}},
      {"patchline": {"source": ["release", 0], "destination": ["selected_velocity", 1]}},
      {"patchline": {"source": ["attack", 0], "destination": ["attack_order", 0]}},
      {"patchline": {"source": ["attack_order", 1], "destination": ["note_flag", 0]}},
      {"patchline": {"source": ["note_flag", 0], "destination": ["xnoteout", 2]}},
      {"patchline": {"source": ["attack_order", 0], "destination": ["selected_velocity", 0]}},
      {"patchline": {"source": ["selected_velocity", 0], "destination": ["xnoteout", 1]}},
      {"patchline": {"source": ["pitch", 0], "destination": ["xnoteout", 0]}}
    ]
  }
}
