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
    "rect": [80.0, 80.0, 720.0, 430.0],
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
          "text": "sunny.adsr~ — bounded vector-edge envelope",
          "patching_rect": [30.0, 20.0, 350.0, 20.0]
        }
      },
      {
        "box": {
          "id": "adsr",
          "maxclass": "newobj",
          "text": "sunny.adsr~",
          "patching_rect": [300.0, 210.0, 85.0, 22.0]
        }
      },
      {
        "box": {
          "id": "scope",
          "maxclass": "newobj",
          "text": "scope~",
          "patching_rect": [300.0, 275.0, 240.0, 95.0]
        }
      },
      {
        "box": {
          "id": "attack",
          "maxclass": "message",
          "text": "attack 0.01",
          "patching_rect": [30.0, 80.0, 80.0, 22.0]
        }
      },
      {
        "box": {
          "id": "decay",
          "maxclass": "message",
          "text": "decay 0.1",
          "patching_rect": [30.0, 120.0, 75.0, 22.0]
        }
      },
      {
        "box": {
          "id": "sustain",
          "maxclass": "message",
          "text": "sustain 0.7",
          "patching_rect": [30.0, 160.0, 80.0, 22.0]
        }
      },
      {
        "box": {
          "id": "release_time",
          "maxclass": "message",
          "text": "release_time 0.3",
          "patching_rect": [30.0, 200.0, 110.0, 22.0]
        }
      },
      {
        "box": {
          "id": "gate",
          "maxclass": "message",
          "text": "gate 1.",
          "patching_rect": [30.0, 240.0, 55.0, 22.0]
        }
      },
      {
        "box": {
          "id": "float_gate",
          "maxclass": "message",
          "text": "0.",
          "patching_rect": [95.0, 240.0, 35.0, 22.0]
        }
      },
      {
        "box": {
          "id": "trigger",
          "maxclass": "message",
          "text": "trigger",
          "patching_rect": [170.0, 80.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "release",
          "maxclass": "message",
          "text": "release",
          "patching_rect": [170.0, 120.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "reset",
          "maxclass": "message",
          "text": "reset",
          "patching_rect": [170.0, 160.0, 45.0, 22.0]
        }
      },
      {
        "box": {
          "id": "status",
          "maxclass": "message",
          "text": "status",
          "patching_rect": [170.0, 200.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "clear_error",
          "maxclass": "message",
          "text": "clear_error",
          "patching_rect": [230.0, 200.0, 70.0, 22.0]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["attack", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["decay", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["sustain", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["release_time", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["gate", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["float_gate", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["trigger", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["release", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["reset", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["status", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["clear_error", 0], "destination": ["adsr", 0]}},
      {"patchline": {"source": ["adsr", 0], "destination": ["scope", 0]}}
    ]
  }
}
