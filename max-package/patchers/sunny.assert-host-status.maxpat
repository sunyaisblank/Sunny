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
    "rect": [
      100,
      100,
      820,
      570
    ],
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
          "id": "once",
          "maxclass": "newobj",
          "text": "onebang",
          "patching_rect": [75, 35, 65, 22]
        }
      },
      {
        "box": {
          "id": "route",
          "maxclass": "newobj",
          "text": "route host_status",
          "patching_rect": [
            30,
            80,
            120,
            22
          ]
        }
      },
      {
        "box": {
          "id": "unpack",
          "maxclass": "newobj",
          "text": "unpack i f i i i i i i",
          "patching_rect": [
            30,
            125,
            190,
            22
          ]
        }
      },
      {
        "box": {
          "id": "sr-positive",
          "maxclass": "newobj",
          "text": "> 0.",
          "patching_rect": [
            160,
            175,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "max-positive",
          "maxclass": "newobj",
          "text": "> 0",
          "patching_rect": [
            245,
            175,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "frames-positive",
          "maxclass": "newobj",
          "text": "> 0",
          "patching_rect": [
            415,
            175,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "error-zero",
          "maxclass": "newobj",
          "text": "== 0",
          "patching_rect": [
            585,
            175,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-configured",
          "maxclass": "newobj",
          "text": "test.assert #1:configured",
          "patching_rect": [
            30,
            225,
            190,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-sr",
          "maxclass": "newobj",
          "text": "test.assert #1:sample-rate-positive",
          "patching_rect": [
            160,
            260,
            235,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-max",
          "maxclass": "newobj",
          "text": "test.assert #1:maximum-frames-positive",
          "patching_rect": [
            245,
            295,
            250,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-process",
          "maxclass": "newobj",
          "text": "test.assert #1:process-observed",
          "patching_rect": [
            330,
            330,
            220,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-frames",
          "maxclass": "newobj",
          "text": "test.assert #1:last-frame-positive",
          "patching_rect": [
            415,
            365,
            220,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-healthy",
          "maxclass": "newobj",
          "text": "test.assert #1:process-healthy",
          "patching_rect": [
            500,
            400,
            210,
            22
          ]
        }
      },
      {
        "box": {
          "id": "assert-error",
          "maxclass": "newobj",
          "text": "test.assert #1:last-error-zero",
          "patching_rect": [
            585,
            435,
            200,
            22
          ]
        }
      },
      {
        "box": {
          "id": "comment",
          "maxclass": "comment",
          "text": "One-shot closed host_status tuple validator used only by Sunny .maxtest patchers.",
          "patching_rect": [
            30,
            490,
            520,
            22
          ]
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
            "once",
            0
          ]
        }
      },
      {"patchline": {"source": ["once", 0], "destination": ["route", 0]}},
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
      {
        "patchline": {
          "source": [
            "unpack",
            0
          ],
          "destination": [
            "assert-configured",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "unpack",
            1
          ],
          "destination": [
            "sr-positive",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "sr-positive",
            0
          ],
          "destination": [
            "assert-sr",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "unpack",
            2
          ],
          "destination": [
            "max-positive",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "max-positive",
            0
          ],
          "destination": [
            "assert-max",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "unpack",
            3
          ],
          "destination": [
            "assert-process",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "unpack",
            5
          ],
          "destination": [
            "frames-positive",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "frames-positive",
            0
          ],
          "destination": [
            "assert-frames",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "unpack",
            6
          ],
          "destination": [
            "assert-healthy",
            0
          ]
        }
      },
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
      }
    ],
    "autosave": 0
  }
}
