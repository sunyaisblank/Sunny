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
      1600,
      1100
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
          "id": "title",
          "maxclass": "comment",
          "text": "Sunny standalone Max runtime and global-ITM transport test (Cycling '74 max-test). This does not claim Max for Live behavior.",
          "patching_rect": [
            30,
            20,
            760,
            22
          ]
        }
      },
      {
        "box": {
          "id": "load",
          "maxclass": "newobj",
          "text": "loadbang",
          "patching_rect": [
            30,
            65,
            70,
            22
          ]
        }
      },
      {
        "box": {
          "id": "init-order",
          "maxclass": "newobj",
          "text": "t b b b b b b b",
          "patching_rect": [
            30,
            105,
            125,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo-controls",
          "maxclass": "message",
          "text": "0., frequency 0., waveform saw, seed 42, reset, phase 0.25, clear_error",
          "patching_rect": [
            180,
            65,
            250,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr-controls",
          "maxclass": "message",
          "text": "1., attack 0., decay 0., sustain 0.5, release_time 0., gate 1., release, reset, clear_error, trigger",
          "patching_rect": [
            180,
            100,
            270,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold-controls",
          "maxclass": "message",
          "text": "0.1, value 0.25, reset, clear_error, value 0.25",
          "patching_rect": [
            180,
            135,
            90,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock-controls",
          "maxclass": "message",
          "text": "tempo 120., position 240, play, record, pause, stop, clear_error, position 480, pause",
          "patching_rect": [
            180,
            170,
            135,
            22
          ]
        }
      },
      {
        "box": {
          "id": "events-controls",
          "maxclass": "message",
          "text": "event 1000000000 70 90 11, note 1000001000 71 24 91 12, clear, clear_error, event_after 96 60 100 23, event_after 192 61 100 31, event_after 192 62 0 47, note_after 288 63 48 110 59",
          "patching_rect": [
            180,
            205,
            930,
            22
          ]
        }
      },
      {
        "box": {
          "id": "dsp-on",
          "maxclass": "message",
          "text": "1",
          "patching_rect": [
            180,
            240,
            35,
            22
          ]
        }
      },
      {
        "box": {
          "id": "dac",
          "maxclass": "newobj",
          "text": "dac~",
          "patching_rect": [
            230,
            240,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "delay",
          "maxclass": "newobj",
          "text": "delay 250",
          "patching_rect": [
            30,
            150,
            75,
            22
          ]
        }
      },
      {
        "box": {
          "id": "final-order",
          "maxclass": "newobj",
          "text": "t b b b b b",
          "patching_rect": [
            30,
            190,
            50,
            22
          ]
        }
      },
      {
        "box": {
          "id": "status",
          "maxclass": "message",
          "text": "status",
          "patching_rect": [
            105,
            190,
            55,
            22
          ]
        }
      },
      {
        "box": {
          "id": "console",
          "maxclass": "newobj",
          "text": "CheckConsoleClear sunny-runtime-smoke",
          "patching_rect": [
            30,
            245,
            245,
            22
          ]
        }
      },
      {
        "box": {
          "id": "terminate",
          "maxclass": "newobj",
          "text": "test.terminate",
          "patching_rect": [
            30,
            285,
            100,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo",
          "maxclass": "newobj",
          "text": "sunny.lfo~ 1.",
          "varname": "sunny_lfo_source",
          "patching_rect": [
            475,
            65,
            105,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo-sample",
          "maxclass": "newobj",
          "text": "test.sample~",
          "varname": "sunny_lfo_sink",
          "patching_rect": [
            475,
            105,
            90,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo-equals",
          "maxclass": "newobj",
          "text": "test.equals -0.5 @single_precision 1",
          "patching_rect": [
            475,
            145,
            230,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-lfo-output",
          "patching_rect": [
            475,
            185,
            195,
            22
          ]
        }
      },
      {
        "box": {
          "id": "lfo-status",
          "maxclass": "newobj",
          "text": "sunny.assert-host-status lfo",
          "patching_rect": [
            710,
            65,
            190,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr",
          "maxclass": "newobj",
          "text": "sunny.adsr~",
          "varname": "sunny_adsr_source",
          "patching_rect": [
            475,
            235,
            100,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr-sample",
          "maxclass": "newobj",
          "text": "test.sample~",
          "varname": "sunny_adsr_sink",
          "patching_rect": [
            475,
            275,
            90,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr-equals",
          "maxclass": "newobj",
          "text": "test.equals 0.5 @single_precision 1",
          "patching_rect": [
            475,
            315,
            225,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-adsr-output",
          "patching_rect": [
            475,
            355,
            200,
            22
          ]
        }
      },
      {
        "box": {
          "id": "adsr-status",
          "maxclass": "newobj",
          "text": "sunny.assert-host-status adsr",
          "patching_rect": [
            710,
            235,
            195,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold",
          "maxclass": "newobj",
          "text": "sunny.hold~",
          "varname": "sunny_hold_source",
          "patching_rect": [
            475,
            405,
            100,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold-sample",
          "maxclass": "newobj",
          "text": "test.sample~",
          "varname": "sunny_hold_sink",
          "patching_rect": [
            475,
            445,
            90,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold-equals",
          "maxclass": "newobj",
          "text": "test.equals 0.25 @single_precision 1",
          "patching_rect": [
            475,
            485,
            230,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-hold-output",
          "patching_rect": [
            475,
            525,
            200,
            22
          ]
        }
      },
      {
        "box": {
          "id": "hold-status",
          "maxclass": "newobj",
          "text": "sunny.assert-host-status hold",
          "patching_rect": [
            710,
            405,
            195,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock",
          "maxclass": "newobj",
          "text": "sunny.clock~ 480",
          "varname": "sunny_clock_source",
          "patching_rect": [
            30,
            360,
            120,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock-sample",
          "maxclass": "newobj",
          "text": "test.sample~",
          "varname": "sunny_clock_sink",
          "patching_rect": [
            30,
            400,
            90,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock-equals",
          "maxclass": "newobj",
          "text": "test.equals 1. @single_precision 1",
          "patching_rect": [
            30,
            440,
            220,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-clock-output",
          "patching_rect": [
            30,
            480,
            205,
            22
          ]
        }
      },
      {
        "box": {
          "id": "clock-status",
          "maxclass": "newobj",
          "text": "sunny.assert-host-status clock",
          "patching_rect": [
            260,
            360,
            205,
            22
          ]
        }
      },
      {
        "box": {
          "id": "events",
          "maxclass": "newobj",
          "text": "sunny.events 480",
          "patching_rect": [
            30,
            545,
            120,
            22
          ]
        }
      },
      {
        "box": {
          "id": "events-status",
          "maxclass": "newobj",
          "text": "sunny.assert-event-status events",
          "patching_rect": [
            260,
            545,
            210,
            22
          ]
        }
      },
      {
        "box": {
          "id": "event-order",
          "maxclass": "newobj",
          "text": "t l b",
          "patching_rect": [30, 585, 45, 22]
        }
      },
      {
        "box": {
          "id": "event-count",
          "maxclass": "newobj",
          "text": "counter 1 6",
          "patching_rect": [90, 585, 80, 22]
        }
      },
      {
        "box": {
          "id": "event-gate",
          "maxclass": "newobj",
          "text": "gate 6",
          "patching_rect": [30, 625, 55, 22]
        }
      },
      {
        "box": {
          "id": "assert-first",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 60 100 23 itm-first",
          "patching_rect": [30, 670, 275, 22]
        }
      },
      {
        "box": {
          "id": "assert-equal-off",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 62 0 47 itm-equal-off",
          "patching_rect": [30, 705, 285, 22]
        }
      },
      {
        "box": {
          "id": "assert-equal-on",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 61 100 31 itm-equal-on",
          "patching_rect": [30, 740, 290, 22]
        }
      },
      {
        "box": {
          "id": "assert-note-on",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 63 110 59 itm-note-on",
          "patching_rect": [30, 775, 285, 22]
        }
      },
      {
        "box": {
          "id": "fifth-order",
          "maxclass": "newobj",
          "text": "t b l",
          "patching_rect": [335, 625, 45, 22]
        }
      },
      {
        "box": {
          "id": "assert-note-off",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 63 0 59 itm-note-off",
          "patching_rect": [335, 670, 285, 22]
        }
      },
      {
        "box": {
          "id": "reassign-controls",
          "maxclass": "message",
          "text": "event_after 96 64 100 71, clear, event_after 192 65 101 73",
          "patching_rect": [335, 705, 355, 22]
        }
      },
      {
        "box": {
          "id": "sixth-order",
          "maxclass": "newobj",
          "text": "t b l",
          "patching_rect": [715, 625, 45, 22]
        }
      },
      {
        "box": {
          "id": "assert-replacement",
          "maxclass": "newobj",
          "text": "sunny.assert-event-list 65 101 73 itm-replacement",
          "patching_rect": [715, 670, 305, 22]
        }
      },
      {
        "box": {
          "id": "transport-order",
          "maxclass": "newobj",
          "text": "t b b b b b b",
          "patching_rect": [330, 285, 105, 22]
        }
      },
      {
        "box": {
          "id": "host-transport",
          "maxclass": "newobj",
          "text": "transport",
          "patching_rect": [330, 405, 70, 22]
        }
      },
      {
        "box": {
          "id": "transport-stop",
          "maxclass": "message",
          "text": "0",
          "patching_rect": [330, 325, 30, 22]
        }
      },
      {
        "box": {
          "id": "transport-tempo",
          "maxclass": "message",
          "text": "tempo 120.",
          "patching_rect": [370, 325, 75, 22]
        }
      },
      {
        "box": {
          "id": "transport-position",
          "maxclass": "message",
          "text": "0",
          "patching_rect": [455, 325, 30, 22]
        }
      },
      {
        "box": {
          "id": "transport-start",
          "maxclass": "message",
          "text": "1",
          "patching_rect": [495, 325, 30, 22]
        }
      },
      {
        "box": {
          "id": "watchdog",
          "maxclass": "newobj",
          "text": "delay 1500",
          "patching_rect": [535, 325, 80, 22]
        }
      },
      {
        "box": {
          "id": "watchdog-order",
          "maxclass": "newobj",
          "text": "t b b b",
          "patching_rect": [535, 365, 60, 22]
        }
      },
      {
        "box": {
          "id": "watchdog-stop",
          "maxclass": "message",
          "text": "stop",
          "patching_rect": [715, 740, 45, 22]
        }
      },
      {
        "box": {
          "id": "completion-fail",
          "maxclass": "message",
          "text": "0",
          "patching_rect": [605, 365, 30, 22]
        }
      },
      {
        "box": {
          "id": "completion-pass",
          "maxclass": "message",
          "text": "1",
          "patching_rect": [770, 740, 30, 22]
        }
      },
      {
        "box": {
          "id": "completion-assert",
          "maxclass": "newobj",
          "text": "test.assert events:transport-sequence-complete",
          "patching_rect": [605, 405, 310, 22]
        }
      },
      {
        "box": {
          "id": "events-final-order",
          "maxclass": "newobj",
          "text": "t b b b b b",
          "patching_rect": [715, 705, 90, 22]
        }
      },
      {
        "box": {
          "id": "events-final-status",
          "maxclass": "message",
          "text": "status, transport_status",
          "patching_rect": [815, 705, 150, 22]
        }
      },
      {
        "box": {
          "id": "lfo-sample-gate",
          "maxclass": "newobj",
          "text": "gate 2 1",
          "patching_rect": [1030, 50, 65, 22]
        }
      },
      {
        "box": {
          "id": "lfo-reconnect-order",
          "maxclass": "newobj",
          "text": "t b f",
          "patching_rect": [1110, 50, 45, 22]
        }
      },
      {
        "box": {
          "id": "lfo-reconnect-equals",
          "maxclass": "newobj",
          "text": "test.equals -0.5 @single_precision 1",
          "patching_rect": [1170, 50, 230, 22]
        }
      },
      {
        "box": {
          "id": "lfo-reconnect-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-lfo-reconnected-output",
          "patching_rect": [1170, 80, 270, 22]
        }
      },
      {
        "box": {
          "id": "adsr-sample-gate",
          "maxclass": "newobj",
          "text": "gate 2 1",
          "patching_rect": [1030, 120, 65, 22]
        }
      },
      {
        "box": {
          "id": "adsr-reconnect-order",
          "maxclass": "newobj",
          "text": "t b f",
          "patching_rect": [1110, 120, 45, 22]
        }
      },
      {
        "box": {
          "id": "adsr-reconnect-equals",
          "maxclass": "newobj",
          "text": "test.equals 0.5 @single_precision 1",
          "patching_rect": [1170, 120, 225, 22]
        }
      },
      {
        "box": {
          "id": "adsr-reconnect-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-adsr-reconnected-output",
          "patching_rect": [1170, 150, 280, 22]
        }
      },
      {
        "box": {
          "id": "hold-sample-gate",
          "maxclass": "newobj",
          "text": "gate 2 1",
          "patching_rect": [1030, 190, 65, 22]
        }
      },
      {
        "box": {
          "id": "hold-reconnect-order",
          "maxclass": "newobj",
          "text": "t b f",
          "patching_rect": [1110, 190, 45, 22]
        }
      },
      {
        "box": {
          "id": "hold-reconnect-equals",
          "maxclass": "newobj",
          "text": "test.equals 0.25 @single_precision 1",
          "patching_rect": [1170, 190, 230, 22]
        }
      },
      {
        "box": {
          "id": "hold-reconnect-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-hold-reconnected-output",
          "patching_rect": [1170, 220, 280, 22]
        }
      },
      {
        "box": {
          "id": "clock-sample-gate",
          "maxclass": "newobj",
          "text": "gate 2 1",
          "patching_rect": [1030, 260, 65, 22]
        }
      },
      {
        "box": {
          "id": "clock-reconnect-order",
          "maxclass": "newobj",
          "text": "t b f",
          "patching_rect": [1110, 260, 45, 22]
        }
      },
      {
        "box": {
          "id": "clock-reconnect-equals",
          "maxclass": "newobj",
          "text": "test.equals 1. @single_precision 1",
          "patching_rect": [1170, 260, 220, 22]
        }
      },
      {
        "box": {
          "id": "clock-reconnect-assert",
          "maxclass": "newobj",
          "text": "test.assert sunny-clock-reconnected-output",
          "patching_rect": [1170, 290, 285, 22]
        }
      },
      {
        "box": {
          "id": "lfo-lifecycle",
          "maxclass": "newobj",
          "text": "sunny.assert-host-lifecycle lfo",
          "patching_rect": [1030, 340, 220, 22]
        }
      },
      {
        "box": {
          "id": "adsr-lifecycle",
          "maxclass": "newobj",
          "text": "sunny.assert-host-lifecycle adsr",
          "patching_rect": [1270, 340, 225, 22]
        }
      },
      {
        "box": {
          "id": "hold-lifecycle",
          "maxclass": "newobj",
          "text": "sunny.assert-host-lifecycle hold",
          "patching_rect": [1030, 375, 225, 22]
        }
      },
      {
        "box": {
          "id": "clock-lifecycle",
          "maxclass": "newobj",
          "text": "sunny.assert-host-lifecycle clock",
          "patching_rect": [1270, 375, 230, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-baseline",
          "maxclass": "message",
          "text": "baseline",
          "patching_rect": [1030, 425, 65, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-disconnected",
          "maxclass": "message",
          "text": "disconnected",
          "patching_rect": [1110, 425, 90, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-reconnected",
          "maxclass": "message",
          "text": "reconnected",
          "patching_rect": [1215, 425, 90, 22]
        }
      },
      {
        "box": {
          "id": "patcher-script",
          "maxclass": "newobj",
          "text": "thispatcher",
          "patching_rect": [1030, 475, 80, 22]
        }
      },
      {
        "box": {
          "id": "disconnect-script",
          "maxclass": "message",
          "text": "script disconnect sunny_lfo_source 0 sunny_lfo_sink 0, script disconnect sunny_adsr_source 0 sunny_adsr_sink 0, script disconnect sunny_hold_source 0 sunny_hold_sink 0, script disconnect sunny_clock_source 0 sunny_clock_sink 0",
          "patching_rect": [1030, 510, 520, 42]
        }
      },
      {
        "box": {
          "id": "reconnect-script",
          "maxclass": "message",
          "text": "script connect sunny_lfo_source 0 sunny_lfo_sink 0, script connect sunny_adsr_source 0 sunny_adsr_sink 0, script connect sunny_hold_source 0 sunny_hold_sink 0, script connect sunny_clock_source 0 sunny_clock_sink 0",
          "patching_rect": [1030, 565, 520, 42]
        }
      },
      {
        "box": {
          "id": "disconnect-delay",
          "maxclass": "newobj",
          "text": "delay 120",
          "patching_rect": [1030, 625, 75, 22]
        }
      },
      {
        "box": {
          "id": "disconnected-order",
          "maxclass": "newobj",
          "text": "t b b b",
          "patching_rect": [1120, 625, 60, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-command-order",
          "maxclass": "newobj",
          "text": "t b b",
          "patching_rect": [1195, 625, 50, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-delay",
          "maxclass": "newobj",
          "text": "delay 120",
          "patching_rect": [1260, 625, 75, 22]
        }
      },
      {
        "box": {
          "id": "reconnected-order",
          "maxclass": "newobj",
          "text": "t b b b",
          "patching_rect": [1350, 625, 60, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-sample-order",
          "maxclass": "newobj",
          "text": "t b b",
          "patching_rect": [1425, 625, 50, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-sample-route",
          "maxclass": "message",
          "text": "2",
          "patching_rect": [1425, 660, 30, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-sample-bang",
          "maxclass": "button",
          "patching_rect": [1470, 660, 24, 24]
        }
      },
      {
        "box": {
          "id": "reconnect-count",
          "maxclass": "newobj",
          "text": "counter 1 4",
          "patching_rect": [1030, 710, 80, 22]
        }
      },
      {
        "box": {
          "id": "reconnect-complete",
          "maxclass": "newobj",
          "text": "sel 4",
          "patching_rect": [1125, 710, 45, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-success-order",
          "maxclass": "newobj",
          "text": "t b b b",
          "patching_rect": [1185, 710, 60, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-watchdog",
          "maxclass": "newobj",
          "text": "delay 2000",
          "patching_rect": [1030, 755, 85, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-watchdog-order",
          "maxclass": "newobj",
          "text": "t b b b",
          "patching_rect": [1125, 755, 60, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-watchdog-stop",
          "maxclass": "message",
          "text": "stop",
          "patching_rect": [1200, 755, 45, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-completion-fail",
          "maxclass": "message",
          "text": "0",
          "patching_rect": [1260, 755, 30, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-completion-pass",
          "maxclass": "message",
          "text": "1",
          "patching_rect": [1305, 755, 30, 22]
        }
      },
      {
        "box": {
          "id": "lifecycle-completion-assert",
          "maxclass": "newobj",
          "text": "test.assert lifecycle:sequence-complete",
          "patching_rect": [1030, 800, 260, 22]
        }
      },
      {
        "box": {
          "id": "boundary",
          "maxclass": "comment",
          "text": "Covered here: discovery/instantiation, public selectors, mono DSP facts, disconnected callback advance, reconnect callback/output continuity, deterministic output, global-ITM firing/order/reuse, release velocity, and standalone transport identity. Scheduler sample timing, MIDI receipt, render, active teardown, and Max for Live remain separate checks.",
          "patching_rect": [
            30,
            865,
            1080,
            42
          ]
        }
      }
    ],
    "lines": [
      {
        "patchline": {
          "source": [
            "load",
            0
          ],
          "destination": [
            "init-order",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            6
          ],
          "destination": [
            "lfo-controls",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "lfo-controls",
            0
          ],
          "destination": [
            "lfo",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            5
          ],
          "destination": [
            "adsr-controls",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "adsr-controls",
            0
          ],
          "destination": [
            "adsr",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            4
          ],
          "destination": [
            "hold-controls",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "hold-controls",
            0
          ],
          "destination": [
            "hold",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            3
          ],
          "destination": [
            "clock-controls",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "clock-controls",
            0
          ],
          "destination": [
            "clock",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "events-controls",
            0
          ],
          "destination": [
            "events",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            1
          ],
          "destination": [
            "dsp-on",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "dsp-on",
            0
          ],
          "destination": [
            "dac",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "init-order",
            0
          ],
          "destination": [
            "delay",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "delay",
            0
          ],
          "destination": [
            "final-order",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "final-order",
            4
          ],
          "destination": [
            "lifecycle-watchdog",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "final-order",
            3
          ],
          "destination": [
            "lifecycle-baseline",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "final-order",
            2
          ],
          "destination": [
            "status",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "status",
            0
          ],
          "destination": [
            "lfo",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "status",
            0
          ],
          "destination": [
            "adsr",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "status",
            0
          ],
          "destination": [
            "hold",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "status",
            0
          ],
          "destination": [
            "clock",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "final-order",
            1
          ],
          "destination": [
            "disconnect-script",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "final-order",
            0
          ],
          "destination": [
            "disconnect-delay",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "lifecycle-success-order",
            0
          ],
          "destination": [
            "transport-order",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "console",
            0
          ],
          "destination": [
            "terminate",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "lfo",
            0
          ],
          "destination": [
            "lfo-sample",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "lfo-sample",
            0
          ],
          "destination": [
            "lfo-sample-gate",
            0
          ]
        }
      },
      {"patchline": {"source": ["lfo-sample-gate", 0], "destination": ["lfo-equals", 0]}},
      {"patchline": {"source": ["lfo-sample-gate", 1], "destination": ["lfo-reconnect-order", 0]}},
      {"patchline": {"source": ["lfo-reconnect-order", 1], "destination": ["lfo-reconnect-equals", 0]}},
      {"patchline": {"source": ["lfo-reconnect-equals", 0], "destination": ["lfo-reconnect-assert", 0]}},
      {"patchline": {"source": ["lfo-reconnect-order", 0], "destination": ["reconnect-count", 0]}},
      {
        "patchline": {
          "source": [
            "lfo-equals",
            0
          ],
          "destination": [
            "lfo-assert",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "lfo",
            1
          ],
          "destination": [
            "lfo-status",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "adsr",
            0
          ],
          "destination": [
            "adsr-sample",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "adsr-sample",
            0
          ],
          "destination": [
            "adsr-sample-gate",
            0
          ]
        }
      },
      {"patchline": {"source": ["adsr-sample-gate", 0], "destination": ["adsr-equals", 0]}},
      {"patchline": {"source": ["adsr-sample-gate", 1], "destination": ["adsr-reconnect-order", 0]}},
      {"patchline": {"source": ["adsr-reconnect-order", 1], "destination": ["adsr-reconnect-equals", 0]}},
      {"patchline": {"source": ["adsr-reconnect-equals", 0], "destination": ["adsr-reconnect-assert", 0]}},
      {"patchline": {"source": ["adsr-reconnect-order", 0], "destination": ["reconnect-count", 0]}},
      {
        "patchline": {
          "source": [
            "adsr-equals",
            0
          ],
          "destination": [
            "adsr-assert",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "adsr",
            1
          ],
          "destination": [
            "adsr-status",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "hold",
            0
          ],
          "destination": [
            "hold-sample",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "hold-sample",
            0
          ],
          "destination": [
            "hold-sample-gate",
            0
          ]
        }
      },
      {"patchline": {"source": ["hold-sample-gate", 0], "destination": ["hold-equals", 0]}},
      {"patchline": {"source": ["hold-sample-gate", 1], "destination": ["hold-reconnect-order", 0]}},
      {"patchline": {"source": ["hold-reconnect-order", 1], "destination": ["hold-reconnect-equals", 0]}},
      {"patchline": {"source": ["hold-reconnect-equals", 0], "destination": ["hold-reconnect-assert", 0]}},
      {"patchline": {"source": ["hold-reconnect-order", 0], "destination": ["reconnect-count", 0]}},
      {
        "patchline": {
          "source": [
            "hold-equals",
            0
          ],
          "destination": [
            "hold-assert",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "hold",
            1
          ],
          "destination": [
            "hold-status",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "clock",
            0
          ],
          "destination": [
            "clock-sample",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "clock-sample",
            0
          ],
          "destination": [
            "clock-sample-gate",
            0
          ]
        }
      },
      {"patchline": {"source": ["clock-sample-gate", 0], "destination": ["clock-equals", 0]}},
      {"patchline": {"source": ["clock-sample-gate", 1], "destination": ["clock-reconnect-order", 0]}},
      {"patchline": {"source": ["clock-reconnect-order", 1], "destination": ["clock-reconnect-equals", 0]}},
      {"patchline": {"source": ["clock-reconnect-equals", 0], "destination": ["clock-reconnect-assert", 0]}},
      {"patchline": {"source": ["clock-reconnect-order", 0], "destination": ["reconnect-count", 0]}},
      {
        "patchline": {
          "source": [
            "clock-equals",
            0
          ],
          "destination": [
            "clock-assert",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "clock",
            1
          ],
          "destination": [
            "clock-status",
            0
          ]
        }
      },
      {
        "patchline": {
          "source": [
            "events",
            1
          ],
          "destination": [
            "events-status",
            0
          ]
        }
      },
      {"patchline": {"source": ["lifecycle-baseline", 0], "destination": ["lfo-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-baseline", 0], "destination": ["adsr-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-baseline", 0], "destination": ["hold-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-baseline", 0], "destination": ["clock-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-disconnected", 0], "destination": ["lfo-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-disconnected", 0], "destination": ["adsr-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-disconnected", 0], "destination": ["hold-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-disconnected", 0], "destination": ["clock-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-reconnected", 0], "destination": ["lfo-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-reconnected", 0], "destination": ["adsr-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-reconnected", 0], "destination": ["hold-lifecycle", 0]}},
      {"patchline": {"source": ["lifecycle-reconnected", 0], "destination": ["clock-lifecycle", 0]}},
      {"patchline": {"source": ["lfo", 1], "destination": ["lfo-lifecycle", 0]}},
      {"patchline": {"source": ["adsr", 1], "destination": ["adsr-lifecycle", 0]}},
      {"patchline": {"source": ["hold", 1], "destination": ["hold-lifecycle", 0]}},
      {"patchline": {"source": ["clock", 1], "destination": ["clock-lifecycle", 0]}},
      {"patchline": {"source": ["disconnect-script", 0], "destination": ["patcher-script", 0]}},
      {"patchline": {"source": ["disconnect-delay", 0], "destination": ["disconnected-order", 0]}},
      {"patchline": {"source": ["disconnected-order", 2], "destination": ["lifecycle-disconnected", 0]}},
      {"patchline": {"source": ["disconnected-order", 1], "destination": ["status", 0]}},
      {"patchline": {"source": ["disconnected-order", 0], "destination": ["reconnect-command-order", 0]}},
      {"patchline": {"source": ["reconnect-command-order", 1], "destination": ["reconnect-script", 0]}},
      {"patchline": {"source": ["reconnect-script", 0], "destination": ["patcher-script", 0]}},
      {"patchline": {"source": ["reconnect-command-order", 0], "destination": ["reconnect-delay", 0]}},
      {"patchline": {"source": ["reconnect-delay", 0], "destination": ["reconnected-order", 0]}},
      {"patchline": {"source": ["reconnected-order", 2], "destination": ["lifecycle-reconnected", 0]}},
      {"patchline": {"source": ["reconnected-order", 1], "destination": ["status", 0]}},
      {"patchline": {"source": ["reconnected-order", 0], "destination": ["reconnect-sample-order", 0]}},
      {"patchline": {"source": ["reconnect-sample-order", 1], "destination": ["reconnect-sample-route", 0]}},
      {"patchline": {"source": ["reconnect-sample-route", 0], "destination": ["lfo-sample-gate", 0]}},
      {"patchline": {"source": ["reconnect-sample-route", 0], "destination": ["adsr-sample-gate", 0]}},
      {"patchline": {"source": ["reconnect-sample-route", 0], "destination": ["hold-sample-gate", 0]}},
      {"patchline": {"source": ["reconnect-sample-route", 0], "destination": ["clock-sample-gate", 0]}},
      {"patchline": {"source": ["reconnect-sample-order", 0], "destination": ["reconnect-sample-bang", 0]}},
      {"patchline": {"source": ["reconnect-sample-bang", 0], "destination": ["lfo-sample", 0]}},
      {"patchline": {"source": ["reconnect-sample-bang", 0], "destination": ["adsr-sample", 0]}},
      {"patchline": {"source": ["reconnect-sample-bang", 0], "destination": ["hold-sample", 0]}},
      {"patchline": {"source": ["reconnect-sample-bang", 0], "destination": ["clock-sample", 0]}},
      {"patchline": {"source": ["reconnect-count", 0], "destination": ["reconnect-complete", 0]}},
      {"patchline": {"source": ["reconnect-complete", 0], "destination": ["lifecycle-success-order", 0]}},
      {"patchline": {"source": ["lifecycle-success-order", 2], "destination": ["lifecycle-watchdog-stop", 0]}},
      {"patchline": {"source": ["lifecycle-watchdog-stop", 0], "destination": ["lifecycle-watchdog", 0]}},
      {"patchline": {"source": ["lifecycle-success-order", 1], "destination": ["lifecycle-completion-pass", 0]}},
      {"patchline": {"source": ["lifecycle-completion-pass", 0], "destination": ["lifecycle-completion-assert", 0]}},
      {"patchline": {"source": ["lifecycle-watchdog", 0], "destination": ["lifecycle-watchdog-order", 0]}},
      {"patchline": {"source": ["lifecycle-watchdog-order", 2], "destination": ["lifecycle-completion-fail", 0]}},
      {"patchline": {"source": ["lifecycle-completion-fail", 0], "destination": ["lifecycle-completion-assert", 0]}},
      {"patchline": {"source": ["lifecycle-watchdog-order", 1], "destination": ["transport-stop", 0]}},
      {"patchline": {"source": ["lifecycle-watchdog-order", 0], "destination": ["console", 0]}},
      {"patchline": {"source": ["transport-order", 5], "destination": ["transport-stop", 0]}},
      {"patchline": {"source": ["transport-order", 4], "destination": ["transport-tempo", 0]}},
      {"patchline": {"source": ["transport-order", 3], "destination": ["transport-position", 0]}},
      {"patchline": {"source": ["transport-order", 2], "destination": ["events-controls", 0]}},
      {"patchline": {"source": ["transport-order", 1], "destination": ["transport-start", 0]}},
      {"patchline": {"source": ["transport-order", 0], "destination": ["watchdog", 0]}},
      {"patchline": {"source": ["transport-stop", 0], "destination": ["host-transport", 0]}},
      {"patchline": {"source": ["transport-tempo", 0], "destination": ["host-transport", 0]}},
      {"patchline": {"source": ["transport-position", 0], "destination": ["host-transport", 1]}},
      {"patchline": {"source": ["transport-start", 0], "destination": ["host-transport", 0]}},
      {"patchline": {"source": ["watchdog", 0], "destination": ["watchdog-order", 0]}},
      {"patchline": {"source": ["watchdog-order", 2], "destination": ["completion-fail", 0]}},
      {"patchline": {"source": ["watchdog-order", 1], "destination": ["transport-stop", 0]}},
      {"patchline": {"source": ["watchdog-order", 0], "destination": ["console", 0]}},
      {"patchline": {"source": ["completion-fail", 0], "destination": ["completion-assert", 0]}},
      {"patchline": {"source": ["completion-pass", 0], "destination": ["completion-assert", 0]}},
      {"patchline": {"source": ["watchdog-stop", 0], "destination": ["watchdog", 0]}},
      {"patchline": {"source": ["events", 0], "destination": ["event-order", 0]}},
      {"patchline": {"source": ["event-order", 1], "destination": ["event-count", 0]}},
      {"patchline": {"source": ["event-count", 0], "destination": ["event-gate", 0]}},
      {"patchline": {"source": ["event-order", 0], "destination": ["event-gate", 1]}},
      {"patchline": {"source": ["event-gate", 0], "destination": ["assert-first", 0]}},
      {"patchline": {"source": ["event-gate", 1], "destination": ["assert-equal-off", 0]}},
      {"patchline": {"source": ["event-gate", 2], "destination": ["assert-equal-on", 0]}},
      {"patchline": {"source": ["event-gate", 3], "destination": ["assert-note-on", 0]}},
      {"patchline": {"source": ["event-gate", 4], "destination": ["fifth-order", 0]}},
      {"patchline": {"source": ["fifth-order", 1], "destination": ["assert-note-off", 0]}},
      {"patchline": {"source": ["fifth-order", 0], "destination": ["reassign-controls", 0]}},
      {"patchline": {"source": ["reassign-controls", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["event-gate", 5], "destination": ["sixth-order", 0]}},
      {"patchline": {"source": ["sixth-order", 1], "destination": ["assert-replacement", 0]}},
      {"patchline": {"source": ["sixth-order", 0], "destination": ["events-final-order", 0]}},
      {"patchline": {"source": ["events-final-order", 4], "destination": ["events-final-status", 0]}},
      {"patchline": {"source": ["events-final-status", 0], "destination": ["events", 0]}},
      {"patchline": {"source": ["events-final-order", 3], "destination": ["transport-stop", 0]}},
      {"patchline": {"source": ["events-final-order", 2], "destination": ["watchdog-stop", 0]}},
      {"patchline": {"source": ["events-final-order", 1], "destination": ["completion-pass", 0]}},
      {"patchline": {"source": ["events-final-order", 0], "destination": ["console", 0]}}
    ],
    "autosave": 0
  }
}
