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
          "text": "sunny.lfo~ — bounded vector-edge LFO",
          "patching_rect": [30.0, 20.0, 330.0, 20.0]
        }
      },
      {
        "box": {
          "id": "lfo",
          "maxclass": "newobj",
          "text": "sunny.lfo~ 1.",
          "patching_rect": [300.0, 210.0, 90.0, 22.0]
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
          "id": "frequency",
          "maxclass": "message",
          "text": "frequency 2.",
          "patching_rect": [30.0, 80.0, 85.0, 22.0]
        }
      },
      {
        "box": {
          "id": "triangle",
          "maxclass": "message",
          "text": "waveform triangle",
          "patching_rect": [30.0, 120.0, 120.0, 22.0]
        }
      },
      {
        "box": {
          "id": "random",
          "maxclass": "message",
          "text": "waveform random",
          "patching_rect": [30.0, 160.0, 110.0, 22.0]
        }
      },
      {
        "box": {
          "id": "float_frequency",
          "maxclass": "message",
          "text": "1.5",
          "patching_rect": [30.0, 200.0, 45.0, 22.0]
        }
      },
      {
        "box": {
          "id": "phase",
          "maxclass": "message",
          "text": "phase 0.25",
          "patching_rect": [170.0, 80.0, 75.0, 22.0]
        }
      },
      {
        "box": {
          "id": "seed",
          "maxclass": "message",
          "text": "seed 42",
          "patching_rect": [170.0, 120.0, 60.0, 22.0]
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
      {"patchline": {"source": ["frequency", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["triangle", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["random", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["float_frequency", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["phase", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["seed", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["reset", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["status", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["clear_error", 0], "destination": ["lfo", 0]}},
      {"patchline": {"source": ["lfo", 0], "destination": ["scope", 0]}}
    ]
  }
}
