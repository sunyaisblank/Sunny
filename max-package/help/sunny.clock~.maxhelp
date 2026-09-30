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
    "rect": [80.0, 80.0, 760.0, 470.0],
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
          "text": "sunny.clock~ — local quarter-note position signal (not Live transport)",
          "patching_rect": [30.0, 20.0, 470.0, 20.0]
        }
      },
      {
        "box": {
          "id": "clock",
          "maxclass": "newobj",
          "text": "sunny.clock~ 480",
          "patching_rect": [340.0, 235.0, 115.0, 22.0]
        }
      },
      {
        "box": {
          "id": "snapshot",
          "maxclass": "newobj",
          "text": "snapshot~ 50",
          "patching_rect": [340.0, 290.0, 85.0, 22.0]
        }
      },
      {
        "box": {
          "id": "position_number",
          "maxclass": "flonum",
          "patching_rect": [340.0, 345.0, 90.0, 22.0]
        }
      },
      {
        "box": {
          "id": "tempo",
          "maxclass": "message",
          "text": "tempo 120.",
          "patching_rect": [30.0, 80.0, 75.0, 22.0]
        }
      },
      {
        "box": {
          "id": "position",
          "maxclass": "message",
          "text": "position 960",
          "patching_rect": [30.0, 120.0, 82.0, 22.0]
        }
      },
      {
        "box": {
          "id": "play",
          "maxclass": "message",
          "text": "play",
          "patching_rect": [140.0, 80.0, 40.0, 22.0]
        }
      },
      {
        "box": {
          "id": "record",
          "maxclass": "message",
          "text": "record",
          "patching_rect": [190.0, 80.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "pause",
          "maxclass": "message",
          "text": "pause",
          "patching_rect": [140.0, 120.0, 45.0, 22.0]
        }
      },
      {
        "box": {
          "id": "stop",
          "maxclass": "message",
          "text": "stop",
          "patching_rect": [195.0, 120.0, 40.0, 22.0]
        }
      },
      {
        "box": {
          "id": "status",
          "maxclass": "message",
          "text": "status",
          "patching_rect": [30.0, 180.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "clear_error",
          "maxclass": "message",
          "text": "clear_error",
          "patching_rect": [90.0, 180.0, 70.0, 22.0]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["tempo", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["position", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["play", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["record", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["pause", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["stop", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["status", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["clear_error", 0], "destination": ["clock", 0]}},
      {"patchline": {"source": ["clock", 0], "destination": ["snapshot", 0]}},
      {"patchline": {"source": ["snapshot", 0], "destination": ["position_number", 0]}}
    ]
  }
}
