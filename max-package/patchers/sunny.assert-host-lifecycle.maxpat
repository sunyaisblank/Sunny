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
    "rect": [100, 100, 760, 430],
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
          "id": "route",
          "maxclass": "newobj",
          "text": "route baseline disconnected reconnected host_status",
          "patching_rect": [30, 80, 330, 22]
        }
      },
      {
        "box": {
          "id": "phase-baseline",
          "maxclass": "message",
          "text": "1",
          "patching_rect": [30, 125, 30, 22]
        }
      },
      {
        "box": {
          "id": "phase-disconnected",
          "maxclass": "message",
          "text": "2",
          "patching_rect": [75, 125, 30, 22]
        }
      },
      {
        "box": {
          "id": "phase-reconnected",
          "maxclass": "message",
          "text": "3",
          "patching_rect": [120, 125, 30, 22]
        }
      },
      {
        "box": {
          "id": "unpack",
          "maxclass": "newobj",
          "text": "unpack i f i i i i i i",
          "patching_rect": [380, 125, 205, 22]
        }
      },
      {
        "box": {
          "id": "phase-gate",
          "maxclass": "newobj",
          "text": "gate 3",
          "patching_rect": [215, 170, 55, 22]
        }
      },
      {
        "box": {
          "id": "baseline-store",
          "maxclass": "newobj",
          "text": "int",
          "patching_rect": [30, 215, 40, 22]
        }
      },
      {
        "box": {
          "id": "disconnected-order",
          "maxclass": "newobj",
          "text": "t i i b",
          "patching_rect": [160, 215, 55, 22]
        }
      },
      {
        "box": {
          "id": "disconnected-store",
          "maxclass": "newobj",
          "text": "int",
          "patching_rect": [250, 255, 40, 22]
        }
      },
      {
        "box": {
          "id": "disconnected-compare",
          "maxclass": "newobj",
          "text": ">",
          "patching_rect": [160, 295, 40, 22]
        }
      },
      {
        "box": {
          "id": "assert-disconnected",
          "maxclass": "newobj",
          "text": "test.assert #1:disconnected-process-advanced",
          "patching_rect": [30, 340, 310, 22]
        }
      },
      {
        "box": {
          "id": "reconnected-order",
          "maxclass": "newobj",
          "text": "t i b",
          "patching_rect": [405, 215, 45, 22]
        }
      },
      {
        "box": {
          "id": "reconnected-compare",
          "maxclass": "newobj",
          "text": ">",
          "patching_rect": [405, 295, 40, 22]
        }
      },
      {
        "box": {
          "id": "assert-reconnected",
          "maxclass": "newobj",
          "text": "test.assert #1:reconnected-process-advanced",
          "patching_rect": [375, 340, 310, 22]
        }
      },
      {
        "box": {
          "id": "comment",
          "maxclass": "comment",
          "text": "Compare exact process_calls across baseline, disconnected, and reconnected DSP-chain snapshots.",
          "patching_rect": [30, 390, 650, 22]
        }
      }
    ],
    "lines": [
      {"patchline": {"source": ["in", 0], "destination": ["route", 0]}},
      {"patchline": {"source": ["route", 0], "destination": ["phase-baseline", 0]}},
      {"patchline": {"source": ["route", 1], "destination": ["phase-disconnected", 0]}},
      {"patchline": {"source": ["route", 2], "destination": ["phase-reconnected", 0]}},
      {"patchline": {"source": ["route", 3], "destination": ["unpack", 0]}},
      {"patchline": {"source": ["phase-baseline", 0], "destination": ["phase-gate", 0]}},
      {"patchline": {"source": ["phase-disconnected", 0], "destination": ["phase-gate", 0]}},
      {"patchline": {"source": ["phase-reconnected", 0], "destination": ["phase-gate", 0]}},
      {"patchline": {"source": ["unpack", 4], "destination": ["phase-gate", 1]}},
      {"patchline": {"source": ["phase-gate", 0], "destination": ["baseline-store", 1]}},
      {"patchline": {"source": ["phase-gate", 1], "destination": ["disconnected-order", 0]}},
      {"patchline": {"source": ["disconnected-order", 2], "destination": ["baseline-store", 0]}},
      {"patchline": {"source": ["baseline-store", 0], "destination": ["disconnected-compare", 1]}},
      {"patchline": {"source": ["disconnected-order", 1], "destination": ["disconnected-store", 1]}},
      {"patchline": {"source": ["disconnected-order", 0], "destination": ["disconnected-compare", 0]}},
      {"patchline": {"source": ["disconnected-compare", 0], "destination": ["assert-disconnected", 0]}},
      {"patchline": {"source": ["phase-gate", 2], "destination": ["reconnected-order", 0]}},
      {"patchline": {"source": ["reconnected-order", 1], "destination": ["disconnected-store", 0]}},
      {"patchline": {"source": ["disconnected-store", 0], "destination": ["reconnected-compare", 1]}},
      {"patchline": {"source": ["reconnected-order", 0], "destination": ["reconnected-compare", 0]}},
      {"patchline": {"source": ["reconnected-compare", 0], "destination": ["assert-reconnected", 0]}}
    ],
    "autosave": 0
  }
}
