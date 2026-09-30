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
    "rect": [80.0, 80.0, 620.0, 360.0],
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
          "text": "sunny.hold~ — bounded vector-edge held value",
          "patching_rect": [30.0, 20.0, 360.0, 20.0]
        }
      },
      {
        "box": {
          "id": "hold",
          "maxclass": "newobj",
          "text": "sunny.hold~",
          "patching_rect": [280.0, 160.0, 85.0, 22.0]
        }
      },
      {
        "box": {
          "id": "scope",
          "maxclass": "newobj",
          "text": "scope~",
          "patching_rect": [280.0, 220.0, 240.0, 95.0]
        }
      },
      {
        "box": {
          "id": "positive",
          "maxclass": "message",
          "text": "value 0.75",
          "patching_rect": [30.0, 80.0, 70.0, 22.0]
        }
      },
      {
        "box": {
          "id": "negative",
          "maxclass": "message",
          "text": "-0.25",
          "patching_rect": [30.0, 120.0, 45.0, 22.0]
        }
      },
      {
        "box": {
          "id": "reset",
          "maxclass": "message",
          "text": "reset",
          "patching_rect": [120.0, 80.0, 45.0, 22.0]
        }
      },
      {
        "box": {
          "id": "status",
          "maxclass": "message",
          "text": "status",
          "patching_rect": [120.0, 120.0, 50.0, 22.0]
        }
      },
      {
        "box": {
          "id": "clear_error",
          "maxclass": "message",
          "text": "clear_error",
          "patching_rect": [180.0, 120.0, 70.0, 22.0]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["positive", 0], "destination": ["hold", 0]}},
      {"patchline": {"source": ["negative", 0], "destination": ["hold", 0]}},
      {"patchline": {"source": ["reset", 0], "destination": ["hold", 0]}},
      {"patchline": {"source": ["status", 0], "destination": ["hold", 0]}},
      {"patchline": {"source": ["clear_error", 0], "destination": ["hold", 0]}},
      {"patchline": {"source": ["hold", 0], "destination": ["scope", 0]}}
    ]
  }
}
