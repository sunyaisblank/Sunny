{
  "patcher": {
    "fileversion": 1,
    "appversion": {
      "major": 8,
      "minor": 6,
      "revision": 5,
      "architecture": "x64",
      "modernui": 1
    },
    "classnamespace": "box",
    "rect": [100, 100, 620, 300],
    "bglocked": 0,
    "openinpresentation": 0,
    "default_fontsize": 12,
    "default_fontface": 0,
    "default_fontname": "Arial",
    "gridonopen": 1,
    "gridsize": [15, 15],
    "gridsnaponopen": 1,
    "objectsnaponopen": 1,
    "statusbarvisible": 2,
    "toolbarvisible": 1,
    "boxes": [
      {
        "box": {
          "id": "in",
          "maxclass": "inlet",
          "patching_rect": [30, 35, 25, 25],
          "numinlets": 0,
          "numoutlets": 1
        }
      },
      {
        "box": {
          "id": "load",
          "maxclass": "newobj",
          "text": "loadbang",
          "patching_rect": [220, 35, 70, 22]
        }
      },
      {
        "box": {
          "id": "expected",
          "maxclass": "message",
          "text": "#1 #2 #3",
          "patching_rect": [220, 80, 85, 22]
        }
      },
      {
        "box": {
          "id": "compare",
          "maxclass": "newobj",
          "text": "zl compare",
          "patching_rect": [30, 125, 80, 22]
        }
      },
      {
        "box": {
          "id": "assert",
          "maxclass": "newobj",
          "text": "test.assert #4:list",
          "patching_rect": [30, 175, 175, 22]
        }
      },
      {
        "box": {
          "id": "comment",
          "maxclass": "comment",
          "text": "Exact three-atom sunny.events list assertion: #1 pitch, #2 attack-or-zero, #3 release, #4 label.",
          "patching_rect": [30, 225, 550, 22]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["load", 0], "destination": ["expected", 0]}},
      {"patchline": {"source": ["expected", 0], "destination": ["compare", 1]}},
      {"patchline": {"source": ["in", 0], "destination": ["compare", 0]}},
      {"patchline": {"source": ["compare", 0], "destination": ["assert", 0]}}
    ],
    "autosave": 0
  }
}
