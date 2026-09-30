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
    "rect": [100, 100, 820, 570],
    "bglocked": 0,
    "openinpresentation": 0,
    "default_fontsize": 12,
    "default_fontface": 0,
    "default_fontname": "Arial",
    "gridonopen": 1,
    "gridsize": [
      15,
      15
    ],
    "gridsnaponopen": 1,
    "objectsnaponopen": 1,
    "statusbarvisible": 2,
    "toolbarvisible": 1,
    "boxes": [
      {
        "box": {
          "id": "in",
          "maxclass": "inlet",
          "patching_rect": [
            30,
            35,
            25,
            25
          ],
          "numinlets": 0,
          "numoutlets": 1
        }
      },
      {
        "box": {
          "id": "route",
          "maxclass": "newobj",
          "text": "route event_status transport_status",
          "patching_rect": [
            30,
            80,
            225,
            22
          ]
        }
      },
      {
        "box": {
          "id": "unpack",
          "maxclass": "newobj",
          "text": "unpack i i i i i i i i",
          "patching_rect": [
            30,
            125,
            210,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-command",
          "maxclass": "newobj",
          "text": "test.assert #1:command-observed",
          "patching_rect": [30, 175, 225, 22]
        }
      },
      {
        "box": {
          "id": "assert-scheduled",
          "maxclass": "newobj",
          "text": "test.assert #1:scheduled-observed",
          "patching_rect": [270, 175, 235, 22]
        }
      },
      {
        "box": {
          "id": "assert-fired",
          "maxclass": "newobj",
          "text": "test.assert #1:fired-observed",
          "patching_rect": [510, 175, 210, 22]
        }
      },
      {
        "box": {
          "id": "assert-cancelled",
          "maxclass": "newobj",
          "text": "test.assert #1:cancelled-observed",
          "patching_rect": [30, 215, 230, 22]
        }
      },
      {
        "box": {
          "id": "error-zero",
          "maxclass": "newobj",
          "text": "== 0",
          "patching_rect": [590, 255, 55, 22]
        }
      },
      {
        "box": {
          "id": "reserved-zero",
          "maxclass": "newobj",
          "text": "== 0",
          "patching_rect": [270, 255, 55, 22]
        }
      },
      {
        "box": {
          "id": "retained-zero",
          "maxclass": "newobj",
          "text": "== 0",
          "patching_rect": [430, 255, 55, 22]
        }
      },
      {
        "box": {
          "id": "assert-healthy",
          "maxclass": "newobj",
          "text": "test.assert #1:callbacks-healthy",
          "patching_rect": [30, 255, 230, 22]
        }
      },
      {
        "box": {
          "id": "assert-reserved",
          "maxclass": "newobj",
          "text": "test.assert #1:reservations-empty",
          "patching_rect": [270, 295, 230, 22]
        }
      },
      {
        "box": {
          "id": "assert-retained",
          "maxclass": "newobj",
          "text": "test.assert #1:retained-empty",
          "patching_rect": [430, 335, 210, 22]
        }
      },
      {
        "box": {
          "id": "assert-error",
          "maxclass": "newobj",
          "text": "test.assert #1:last-error-zero",
          "patching_rect": [590, 375, 205, 22]
        }
      },
      {
        "box": {
          "id": "transport-unpack",
          "maxclass": "newobj",
          "text": "unpack f f i s",
          "patching_rect": [30, 335, 125, 22]
        }
      },
      {
        "box": {
          "id": "position-valid",
          "maxclass": "newobj",
          "text": ">= 0.",
          "patching_rect": [30, 375, 55, 22]
        }
      },
      {
        "box": {
          "id": "resolution-valid",
          "maxclass": "newobj",
          "text": "> 0.",
          "patching_rect": [180, 375, 55, 22]
        }
      },
      {
        "box": {
          "id": "assert-position",
          "maxclass": "newobj",
          "text": "test.assert #1:transport-position-nonnegative",
          "patching_rect": [30, 415, 300, 22]
        }
      },
      {
        "box": {
          "id": "assert-resolution",
          "maxclass": "newobj",
          "text": "test.assert #1:transport-resolution-positive",
          "patching_rect": [180, 455, 290, 22]
        }
      },
      {
        "box": {
          "id": "assert-running",
          "maxclass": "newobj",
          "text": "test.assert #1:transport-running",
          "patching_rect": [360, 415, 230, 22]
        }
      },
      {
        "box": {
          "id": "comment",
          "maxclass": "comment",
          "text": "Closed event_status and public transport_status assertions for the callback-driven standalone transport sequence.",
          "patching_rect": [30, 505, 720, 22]
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": [
            "in",
            0
          ],
          "destination": [
            "route",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "route",
            0
          ],
          "destination": [
            "unpack",
            0
          ]
        }
      },
      {"patchline": {"source": ["route", 1], "destination": ["transport-unpack", 0]}},
      {"patchline": {"source": ["unpack", 0], "destination": ["assert-command", 0]}},
      {"patchline": {"source": ["unpack", 1], "destination": ["assert-scheduled", 0]}},
      {"patchline": {"source": ["unpack", 2], "destination": ["assert-fired", 0]}},
      {"patchline": {"source": ["unpack", 3], "destination": ["assert-cancelled", 0]}},
      {
        "patchline": {
          "source": [
            "unpack",
            4
          ],
          "destination": [
            "assert-healthy",
            0
          ]
        }
      },
      {"patchline": {"source": ["unpack", 5], "destination": ["reserved-zero", 0]}},
      {"patchline": {"source": ["reserved-zero", 0], "destination": ["assert-reserved", 0]}},
      {"patchline": {"source": ["unpack", 6], "destination": ["retained-zero", 0]}},
      {"patchline": {"source": ["retained-zero", 0], "destination": ["assert-retained", 0]}},
      {
        "patchline": {
          "source": [
            "unpack",
            7
          ],
          "destination": [
            "error-zero",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "error-zero",
            0
          ],
          "destination": [
            "assert-error",
            0
          ]
        }
      },
      {"patchline": {"source": ["transport-unpack", 0], "destination": ["position-valid", 0]}},
      {"patchline": {"source": ["position-valid", 0], "destination": ["assert-position", 0]}},
      {"patchline": {"source": ["transport-unpack", 1], "destination": ["resolution-valid", 0]}},
      {"patchline": {"source": ["resolution-valid", 0], "destination": ["assert-resolution", 0]}},
      {"patchline": {"source": ["transport-unpack", 2], "destination": ["assert-running", 0]}}
    ],
    "autosave": 0
  }
}
