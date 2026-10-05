"""Executable opt-in final-host probes; default read-only, never runs on import."""

import argparse
import json
import re
import sys
import time
from decimal import Decimal
from pathlib import Path

sys.dont_write_bytecode = True

from common import (  # noqa: E402 - preserve the release before peer imports.
    Mcp,
    approved_origin,
    bridge_rpc,
    legacy_call,
    prime_native_session,
    record,
    require_primary_context,
    rpc,
    verify_selected_release,
)

ROOT = Path(__file__).resolve().parent


def load(path):
    """Read the explicitly selected machine configuration or probe arguments."""
    return json.loads(Path(path).read_text(encoding="utf-8"))


def dump(path, value):
    """Retain a complete finite observation as strict JSON."""
    Path(path).write_text(
        json.dumps(value, indent=2, ensure_ascii=True, allow_nan=False) + "\n", encoding="utf-8"
    )


def ok(client, tool, **arguments):
    """Refuse unsuccessful MCP outcomes and retain uncertainty without retry."""
    result = client.call(tool, **arguments)
    if result.get("success") is False or result.get("ok") is False or result.get("error"):
        raise RuntimeError(
            {
                "tool": tool,
                "result": result,
                "action": "STOP. Preserve journal and native state. "
                "Query original token; do not replay.",
            }
        )
    return result


def revision(client):
    """Read the actual current owning project revision."""
    return ok(client, "get_project_json", score_id=1)["project"]["revision"]


def diagnostic(config, log, request):
    """Require successful direct evidence from the retained Live callback thread."""
    result = rpc(config["diagnostic_host"], config["diagnostic_port"], request, log)
    if result.get("success") is not True:
        raise RuntimeError(result)
    record(
        log,
        "diagnostic_thread_check",
        callback_matches_constructor=result["dispatch"]["callback_thread"]
        == result["dispatch"]["constructor_thread"],
        callback_differs_from_socket=result["dispatch"]["callback_thread"]
        != result["dispatch"]["socket_thread"],
    )
    dispatch = result["dispatch"]
    if (
        dispatch["callback_thread"] != dispatch["constructor_thread"]
        or dispatch["callback_thread"] == dispatch["socket_thread"]
    ):
        raise RuntimeError("Actual scheduler dispatch did not execute on the retained UI thread")
    return result["result"]


def scratch(config, log, approved):
    """Verify explicit approval and exact stopped host document before authoring."""
    if not approved:
        raise RuntimeError("Mutating probe requires --scratch-approved after reviewing the runbook")
    verify_selected_release(config)
    result = diagnostic(config, log, {"op": "inventory", "track_names": []})
    name = result["set_name"].get("value", "")
    if name != config["scratch_set_name"] or not name.startswith("SUNNY_HOST_QUALIFICATION_"):
        raise RuntimeError("Exact configured scratch Set name does not match actual host")
    if any(
        result["song"][field].get("value") is not False
        for field in ("is_playing", "session_record", "record_mode")
    ):
        raise RuntimeError("Stop transport and recording before native authoring")
    if result.get("input_monitoring", {}).get("all_off") is not True:
        raise RuntimeError("Disable input monitoring on all scratch Tracks before native authoring")
    if (
        not config.get("host_edition")
        or not config.get("host_os")
        or not config.get("host_live_version")
    ):
        raise RuntimeError("Record exact edition/OS/version before native authoring")
    if result["version"] != config["host_live_version"]:
        raise RuntimeError("Actual Live version differs from configured qualification matrix row")
    if not isinstance(result.get("document_token"), str) or not result["document_token"]:
        raise RuntimeError("Scratch probe did not identify the observed native document")
    require_primary_context(config, result)
    return result


def owned(client, tool, part=1, **arguments):
    """Call the selected owning tool with the current project revision."""
    return ok(
        client,
        tool,
        score_id=1,
        part_id=part,
        expected_project_revision=revision(client),
        **arguments,
    )


def insert(client, part, bar, letter, duration, velocity, offset=(0, 1)):
    """Author one literal symbolic note in the selected qualification Part."""
    return ok(
        client,
        "score_insert_note",
        score_id=1,
        part_id=part,
        bar=bar,
        offset={"n": offset[0], "d": offset[1]},
        pitch={"letter": letter, "accidental": 0, "octave": 4 if part == 1 else 3},
        duration={"n": duration[0], "d": duration[1]},
        velocity=velocity,
    )


def author(client, config):
    """Build and save the independent two-Part symbolic qualification portfolio."""
    ok(
        client,
        "score_create",
        title="SUNNY final host finite portfolio",
        total_bars=2,
        time_sig_num=4,
        time_sig_den=4,
        bpm=96,
        parts=[
            {"name": "Host phrase", "instrument_type": 47},
            {"name": "Host held", "instrument_type": 47},
        ],
    )
    ok(client, "create_project", score_id=1)
    insert(client, 1, 1, "C", (1, 12), 80)
    insert(client, 1, 1, "C", (1, 12), 80, (1, 12))
    insert(client, 1, 1, "E", (1, 12), 80, (2, 12))
    actual_score = ok(client, "score_get_json", score_id=1)
    first_voice = actual_score["parts"][0]["measures"][0]["voices"][0]["events"]
    attacks = [event for event in first_voice if event["type"] == "note_group"]
    if len(attacks) != 3:
        raise AssertionError("Expected actual three-note authored triplet before tying")
    ok(client, "score_set_tie", score_id=1, event_id=attacks[0]["id"], tied=True)
    insert(client, 1, 2, "A", (1, 4), 76)
    insert(client, 2, 1, "G", (1, 1), 72)
    controls = [
        ("source.filter.cutoff", "LP Freq", "drift.lp.frequency", 20.0, 20000.0),
        ("source.amplifier.stages[0].duration", "Env 1 Attack", "drift.env.1.attack", 0.0, 10000.0),
        ("source.amplifier.stages[1].duration", "Env 1 Decay", "drift.env.1.decay", 0.0, 10000.0),
        (
            "source.amplifier.stages[2].duration",
            "Env 1 Release",
            "drift.env.1.release",
            0.0,
            10000.0,
        ),
    ]
    sources = {}
    for part in (1, 2):
        ok(
            client,
            "set_sound_source",
            profile_id=part,
            source_type="subtractive",
            filter_cutoff=1200.0,
            attack=250.0,
            decay=250.0,
            sustain=0.4,
            release=1000.0,
        )
        for path, name, _, low, high in controls:
            ok(
                client,
                "map_timbre_parameter",
                profile_id=part,
                ir_path=path,
                device_index=0,
                parameter_name=name,
                device_name="Drift",
                source_min=low,
                source_max=high,
                target_min=0.0,
                target_max=1.0,
                value_property="display_value",
            )
        sources[str(part)] = [
            {
                "source_path": path,
                "capability_id": cap,
                "tolerance": config["source_tolerances"][cap],
            }
            for path, _, cap, _, _ in controls
        ]
        ok(client, "set_channel_level", graph_id=1, channel_id=part, level_db=-6.0)
        ok(client, "set_channel_pan", graph_id=1, channel_id=part, pan=-0.25 if part == 1 else 0.25)
    ok(client, "set_channel_input_trim", graph_id=1, channel_id=1, input_trim_db=-7.5)
    width = ok(
        client, "add_channel_effect", graph_id=1, channel_id=1, effect_type="stereo", width=1.25
    )["effect_id"]
    effects = [
        {"kind": "mix_input_trim", "authored_id": 1, "tolerance": config["effect_tolerances"]},
        {"kind": "mix_effect", "authored_id": width, "tolerance": config["effect_tolerances"]},
    ]
    eq = None
    if config["include_eq_eight"]:
        eq = ok(
            client,
            "add_channel_effect",
            graph_id=1,
            channel_id=1,
            effect_type="eq",
            bands=[{"frequency": 733.0, "gain": -5.0, "q": 1.75, "type": 0}],
        )["effect_id"]
        effects.append(
            {"kind": "mix_effect", "authored_id": eq, "tolerance": config["effect_tolerances"]}
        )
    aux = ok(client, "create_aux_bus", graph_id=1, name="Sunny host Return")["aux_bus_id"]
    ok(
        client,
        "set_channel_send",
        graph_id=1,
        channel_id=1,
        aux_id=aux,
        level_db=-24.0,
        pre_fader=False,
    )
    ok(
        client,
        "add_mix_automation",
        graph_id=1,
        target="channels[1].spatial.pan",
        interpolation=0,
        breakpoints=[
            {"bar": 1, "beat_num": 0, "beat_den": 1, "value": -0.5},
            {"bar": 1, "beat_num": 1, "beat_den": 2, "value": 0.5},
        ],
    )
    ok(client, "workspace_save", path=config["server_workspace"])
    return {"sources": sources, "effects": effects, "width": width, "eq": eq, "aux": aux}


def selection(client, state, initial):
    """Select the explicitly approved musical and physical domains for realization."""
    return {
        "score_id": 1,
        "expected_project_revision": revision(client),
        "parts": [
            {
                "part_id": 1,
                "source_selections": state["sources"]["1"],
                "effect_selections": state["effects"],
                "static_mixer": {
                    "domains": ["volume", "mute", "solo"],
                    "volume_tolerance_db": state["mixer_tolerance_db"],
                },
                "pan_lane": {"lane_index": 0, "mode": "absent" if initial else "replace"},
            },
            {
                "part_id": 2,
                "source_selections": state["sources"]["2"],
                "static_mixer": {
                    "domains": ["volume", "pan"],
                    "volume_tolerance_db": state["mixer_tolerance_db"],
                },
            },
        ],
        "routing": [
            {
                "part_id": 1,
                "kind": "create_return" if initial else "send_level",
                "aux_id": state["aux"],
                **({} if initial else {"send_tolerance_db": state["send_tolerance_db"]}),
            },
            *(
                [
                    {
                        "part_id": 1,
                        "kind": "send_level",
                        "aux_id": state["aux"],
                        "send_tolerance_db": state["send_tolerance_db"],
                    }
                ]
                if initial
                else []
            ),
        ],
        "song_settings_part_id": 1,
    }


def require_plan_origin(plan, config):
    """Check every native context against original approval before dispatching a plan."""
    expected = {"schema_version": 1, **approved_origin(config)}

    def check(context):
        if (
            not isinstance(context, dict)
            or type(context.get("schema_version")) is not int
            or context != expected
        ):
            raise RuntimeError("Plan native identity differs from original scratch approval")

    check(plan.get("native_context"))
    for entry in [*plan.get("parts", []), *plan.get("retirements", [])]:
        baseline = entry.get("native_baseline")
        if baseline is not None:
            check(baseline.get("context"))
        inspection = entry.get("current_static_mixer_inspection")
        if inspection is not None:
            check(inspection["inspection"].get("context"))
    for entry in plan.get("routing", []):
        inspection = entry.get("current_send_inspection")
        if inspection is not None:
            check(inspection["inspection"].get("context"))
    settings = plan.get("current_song_settings_inspection")
    if settings is not None:
        check(settings["inspection"].get("context"))


def apply(client, selected, config):
    """Apply one approved coordinator plan without replaying failed phases."""
    planned = ok(client, "project_realization_plan", **selected)
    if (
        planned.get("mutation_dispatched") is not False
        or planned.get("authority_granted") is not False
    ):
        raise AssertionError("Pure plan claimed dispatch or authority")
    require_plan_origin(planned["plan"], config)
    result = ok(
        client,
        "project_realization_apply",
        plan=planned["plan"],
        explicit_plan_approval=True,
        explicit_current_mixer_approval=True,
        explicit_set_wide_audible_approval=True,
        explicit_current_routing_approval=True,
        explicit_selected_envelope_replacement=True,
        allow_unsampled_selected_state_overwrite=True,
        explicit_mute_retirement=True,
        explicit_set_wide_approval=True,
    )
    if (
        result.get("selected_contract_completed") is not True
        or result.get("final_current_cohorts_verified") is not True
    ):
        raise AssertionError(result)
    if (
        result.get("host_qualified") is not False
        or result.get("dsp_equivalence_qualified") is not False
    ):
        raise AssertionError("A sampled finite run cannot claim host/DSP equivalence")
    return result


def save_binding_state(state, result):
    """Retain actual managed bindings for later independent observations."""
    state["phase_attempt_ids"] = result["phase_attempt_ids"]
    state["last_result"] = result
    for evidence in result["final_native_observations"]:
        guard = evidence["binding_guard"]
        part = str(evidence["part_id"])
        state.setdefault("bindings", {})[part] = guard
        state.setdefault("track_tags", {})[part] = (
            f"Sunny|{guard['project_key']}|{guard['binding_key']}|track"
        )


def direct_samples(config, log, state, revised=False):
    """Read the selected native envelope samples without parameter setters."""
    times = [0.0, 0.5, 1.0, 2.0, 7.0] if not revised else [0.0, 0.5, 1.0, 8.0, 11.0]
    result = diagnostic(
        config,
        log,
        {"op": "envelope_samples", "track_name": state["track_tags"]["1"], "times": times},
    )
    expected = [-0.5, -0.5, -0.5, 0.5, 0.5] if not revised else [-0.25, -0.25, 0.25, 0.25, 0.25]
    actual = [s["value"].get("value") for s in result["samples"]]
    if len(actual) != len(expected) or any(
        not isinstance(a, (int, float)) or abs(a - b) > 1.0e-6 for a, b in zip(actual, expected)
    ):
        raise AssertionError({"expected": expected, "actual": actual})
    return result


def recover(client, state, config):
    """Adopt explicitly observed current objects against retained musical history."""
    index = config.get("reopened_return_index")
    if type(index) is not int or index < 0:
        raise RuntimeError(
            "Select/review the actual current owned Return index "
            "in reopened_return_index before recovery"
        )
    for part in (1, 2):
        preview = owned(
            client,
            "project_realization_preview_adoption",
            part,
            selector={
                "track_tag": state["track_tags"][str(part)],
                "clip_tag": state["track_tags"][str(part)].removesuffix("track") + "clip",
            },
        )
        if preview["eligible_for_explicit_adoption"] is not True:
            raise RuntimeError(preview)
        adopted = owned(
            client,
            "project_realization_adopt",
            part,
            preview=preview["preview"],
            explicit_adoption=True,
        )
        if adopted["receipt"]["journal"]["native_mutation_started"] is not False:
            raise AssertionError("Clip adoption invoked native setters")
        arguments = {"selections": state["sources"][str(part)]}
        if part == 1:
            arguments["effect_selections"] = state["effects"]
        devices = owned(client, "project_realization_preview_device_adoption", part, **arguments)
        adopted = owned(
            client,
            "project_realization_adopt_devices",
            part,
            **arguments,
            preview=devices["preview"],
            explicit_adoption=True,
        )
        if adopted["receipt"]["journal"]["native_mutation_started"] is not False:
            raise AssertionError("Device adoption invoked native setters")
    returned = owned(
        client,
        "project_realization_preview_routing",
        1,
        kind="adopt_return",
        aux_id=state["aux"],
        return_index=index,
    )
    adopted = owned(
        client,
        "project_realization_apply_routing",
        1,
        kind="adopt_return",
        aux_id=state["aux"],
        return_index=index,
        preview=returned["preview"],
        explicit_current_routing_approval=True,
    )
    if adopted["receipt"]["journal"]["native_mutation_started"] is not False:
        raise AssertionError("Return adoption invoked native setters")
    for part in (1, 2):
        domains = ["volume", "mute", "solo"] if part == 1 else ["volume", "pan"]
        mixer = owned(
            client,
            "project_realization_preview_static_mixer",
            part,
            purpose="adopt",
            domains=domains,
            volume_tolerance_db=state["mixer_tolerance_db"],
        )
        adopted = owned(
            client,
            "project_realization_adopt_static_mixer",
            part,
            domains=domains,
            volume_tolerance_db=state["mixer_tolerance_db"],
            preview=mixer["preview"],
            explicit_current_mixer_approval=True,
            explicit_set_wide_audible_approval=True,
        )
        if adopted["receipt"]["journal"]["native_mutation_started"] is not False:
            raise AssertionError("Mixer adoption invoked native setters")


def independent_notes(config, log, state, revised, retained_ids=None):
    """Compare actual native notes and retained identities against literal music."""
    capture = diagnostic(
        config, log, {"op": "inventory", "track_names": list(state["track_tags"].values())}
    )
    expected = {
        "1": [
            (62 if revised else 60, 0.0, 2.0 / 3.0, 80.0),
            (66 if revised else 64, 2.0 / 3.0, 1.0 / 3.0, 80.0),
            (69, 4.0, 1.0, 76.0),
        ],
        "2": [(55, 0.0, 4.0, 72.0)],
    }
    if revised:
        expected["1"].append((62, 8.0, 1.0, 74.0))
    ids = {}
    for part in ("1", "2"):
        track = next(
            t for t in capture["tracks"] if t["name"]["value"] == state["track_tags"][part]
        )
        clip = next(c for c in track["clips"] if c["slot"] == 0)
        actual = clip["full_notes"]["notes"]
        ordered = sorted(actual, key=lambda n: (n["start_time"]["value"], n["pitch"]["value"]))
        if len(ordered) != len(expected[part]):
            raise AssertionError(
                "Actual native note cardinality differs from literal expected attacks"
            )
        ids[part] = []
        for note, (pitch, start, duration, velocity) in zip(ordered, expected[part]):
            values = {key: value["value"] for key, value in note.items()}
            if (
                values["pitch"] != pitch
                or values["velocity"] != velocity
                or values["mute"] is not False
            ):
                raise AssertionError(
                    {"actual": values, "literal_expected": (pitch, start, duration, velocity)}
                )
            if (
                abs(values["start_time"] - start) > 1.0e-6
                or abs(values["duration"] - duration) > 1.0e-6
            ):
                raise AssertionError(
                    "Actual time differs beyond explicit float32 "
                    "observation tolerance1e-6 quarter notes"
                )
            if (
                values["probability"] != 1.0
                or values["velocity_deviation"] != 0.0
                or values["release_velocity"] != 64.0
            ):
                raise AssertionError("Unrequested probabilistic/native note fields changed")
            ids[part].append(values["note_id"])
        if len(set(ids[part])) != len(ids[part]):
            raise AssertionError("Actual native note IDs are not unique")
        if retained_ids and ids[part][: len(retained_ids[part])] != retained_ids[part]:
            raise AssertionError("Native IDs changed for retained semantic attacks")
        if (
            clip["end_marker"]["value"] != (12.0 if revised else 8.0)
            or clip["looping"]["value"] is not False
        ):
            raise AssertionError(
                "Actual marker/looping geometry differs from literal expected projection"
            )
        if (clip["signature_numerator"]["value"], clip["signature_denominator"]["value"]) != (
            (2, 2) if revised else (4, 4)
        ):
            raise AssertionError("Actual native meter differs")
    record(
        log,
        "literal_native_notes_verified",
        note_ids=ids,
        revised=revised,
        time_tolerance_quarter_notes=1.0e-6,
        float_precision_warning_only=True,
    )
    return ids


def independent_physical(config, log, state):
    """Compare actual native controls against the selected physical expectations."""
    capture = diagnostic(
        config, log, {"op": "inventory", "track_names": list(state["track_tags"].values())}
    )

    if capture["song"]["tempo"].get("value") != 96.0:
        raise AssertionError("Actual Set tempo differs from literal96 quarter-note BPM")
    actual_meter = (
        capture["song"]["signature_numerator"].get("value"),
        capture["song"]["signature_denominator"].get("value"),
    )
    if actual_meter != ((2, 2) if state.get("revised", False) else (4, 4)):
        raise AssertionError("Actual Song global meter differs from literal selected setting")
    if capture["song"]["is_playing"].get("value") is not False:
        raise AssertionError("Actual managed qualification transport must remain stopped")

    def parsed(text, unit):
        # Independent decimal comparison of literal actual display; no normalized map.
        match = re.fullmatch(
            r"([+-]?(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+))\s*(Hz|kHz|ms|s|dB|%)?", text.strip()
        )
        if not match:
            raise AssertionError("Unreviewed actual native display grammar: " + repr(text))
        number = Decimal(match.group(1))
        suffix = match.group(2)
        expected = {
            "Hz": ("Hz", "kHz"),
            "ms": ("ms", "s"),
            "dB": ("dB",),
            "%": ("%",),
            "Q": (None,),
        }
        if suffix not in expected[unit]:
            raise AssertionError("Wrong literal native physical unit")
        return number * 1000 if suffix in ("kHz", "s") else number

    def parameter(device, name):
        matches = [p for p in device["parameters"] if p["original_name"]["value"] == name]
        if len(matches) != 1:
            raise AssertionError("Actual native original-name parameter is not unique")
        return matches[0]

    def check(device, name, target, tolerance, unit):
        p = parameter(device, name)
        text = p["actual_str_for_value"]["value"]
        observed = parsed(text, unit)
        if abs(observed - Decimal(str(target))) > Decimal(str(tolerance)):
            raise AssertionError(
                {"control": name, "actual_display": text, "target": target, "tolerance": tolerance}
            )
        record(
            log,
            "independent_actual_physical",
            class_name=device["class_name"]["value"],
            original_name=name,
            display=text,
            actual_internal=p["value"]["value"],
            target=target,
            tolerance=tolerance,
            unit=unit,
        )

    def enum(device, name, label):
        p = parameter(device, name)
        items = [item["value"] for item in p["value_items"]["items"]]
        index = p["value"]["value"]
        if index != int(index) or not 0 <= int(index) < len(items) or items[int(index)] != label:
            raise AssertionError(
                {"actual_labels": items, "actual_value": index, "required_label": label}
            )

    for part in ("1", "2"):
        track = next(
            t for t in capture["tracks"] if t["name"]["value"] == state["track_tags"][part]
        )
        if track["mute"].get("value") is not False or track["solo"].get("value") is not False:
            raise AssertionError(
                "Actual selected/default retained Track mute/solo differ from literal false/false"
            )
        devices = [d for d in track["devices"] if not d["is_mixer"]]
        classes = [d["class_name"]["value"] for d in devices]
        expected = (
            ["Drift"]
            if part == "2"
            else ["Drift", "StereoGain", "StereoGain"] + (["Eq8"] if state["eq"] else [])
        )
        if classes != expected:
            raise AssertionError("Actual native class/order differs from literal signal chain")
        for name, target, cap, unit in (
            ("LP Freq", 1200.0, "drift.lp.frequency", "Hz"),
            ("Env 1 Attack", 250.0, "drift.env.1.attack", "ms"),
            ("Env 1 Decay", 250.0, "drift.env.1.decay", "ms"),
            ("Env 1 Release", 1000.0, "drift.env.1.release", "ms"),
        ):
            check(devices[0], name, target, config["source_tolerances"][cap], unit)
        mixer = track["mixer"]["volume"]
        display = mixer["actual_str_for_value"]["value"]
        if abs(parsed(display, "dB") - Decimal("-6")) > Decimal(str(config["mixer_tolerance_db"])):
            raise AssertionError("Actual output fader differs from independently authored -6dB")
        if part == "2" and track["mixer"]["panning"]["value"]["value"] != 0.25:
            raise AssertionError("Actual Part2 static stereo pan coordinate differs")
        if part == "1":
            for device, gain, width in ((devices[1], -7.5, 100.0), (devices[2], 0.0, 125.0)):
                check(device, "Gain", gain, config["effect_tolerances"]["decibels"], "dB")
                check(device, "Stereo Width", width, config["effect_tolerances"]["percent"], "%")
                enum(device, "Device On", "On")
                enum(device, "Channel Mode", "Stereo")
                for name in ("Mono", "Mute", "Left Inv", "Right Inv", "Bass Mono", "DC Filter"):
                    enum(device, name, "Off")
            if state["eq"]:
                eq = devices[3]
                if eq["global_mode"]["value"] != 0:
                    raise AssertionError("Actual EQ Stereo native global_mode must be0")
                enum(eq, "Device On", "On")
                enum(eq, "Adaptive Q", "Off")
                enum(eq, "1 Filter Type A", "Bell")
                for band in range(1, 9):
                    enum(eq, str(band) + " Filter On A", "On" if band == 1 else "Off")
                for name, target, tolerance, unit in (
                    ("Scale", 100.0, config["effect_tolerances"]["percent"], "%"),
                    ("Output Gain", 0.0, config["effect_tolerances"]["decibels"], "dB"),
                    ("1 Frequency A", 733.0, config["effect_tolerances"]["hertz"], "Hz"),
                    ("1 Gain A", -5.0, config["effect_tolerances"]["decibels"], "dB"),
                    ("1 Resonance A", 1.75, config["effect_tolerances"]["quality_factor"], "Q"),
                ):
                    check(eq, name, target, tolerance, unit)
            send = track["mixer"]["sends"][-1]["actual_str_for_value"]["value"]
            if abs(parsed(send, "dB") - Decimal("-24")) > Decimal(str(config["send_tolerance_db"])):
                raise AssertionError("Actual newly appended Return send differs from -24dB")
    return capture


def compare_saved_native(saved, current):
    """Independent exact finite persistence; native IDs/types may change on reopen."""

    def normalize(capture):
        result = {}
        for track in capture["tracks"]:
            clips = []
            for clip in track["clips"]:
                values = {
                    key: clip[key].get("value")
                    for key in (
                        "name",
                        "start_marker",
                        "end_marker",
                        "loop_start",
                        "loop_end",
                        "looping",
                        "has_envelopes",
                        "has_groove",
                        "signature_numerator",
                        "signature_denominator",
                    )
                }
                values["notes"] = sorted(
                    [
                        tuple(
                            n[key]["value"]
                            for key in (
                                "pitch",
                                "start_time",
                                "duration",
                                "velocity",
                                "mute",
                                "probability",
                                "velocity_deviation",
                                "release_velocity",
                            )
                        )
                        for n in clip["full_notes"]["notes"]
                    ]
                )
                clips.append(values)
            devices = []
            for device in track["devices"]:
                devices.append(
                    {
                        "class": device["class_name"].get("value"),
                        "parameters": [
                            (p["original_name"].get("value"), p["value"].get("value"))
                            for p in device.get("parameters", [])
                        ],
                        "modes": {
                            key: device[key].get("value")
                            for key in (
                                "global_mode",
                                "edit_mode",
                                "oversample",
                                "voice_mode",
                                "voice_count",
                            )
                        },
                    }
                )
            result[track["name"]["value"]] = {
                "mute": track["mute"].get("value"),
                "solo": track["solo"].get("value"),
                "clips": clips,
                "devices": devices,
                "volume": track["mixer"]["volume"]["value"].get("value"),
                "pan": track["mixer"]["panning"]["value"].get("value"),
                "sends": [s["value"].get("value") for s in track["mixer"]["sends"]],
            }
        result["__song__"] = {
            key: capture["song"][key].get("value")
            for key in ("tempo", "signature_numerator", "signature_denominator", "is_playing")
        }
        return result

    if normalize(saved) != normalize(current):
        raise AssertionError(
            "Saved native finite controls/notes/Clip geometry changed after actual host reopen"
        )


def main():
    """Run only the explicitly selected command-line qualification operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        required=True,
        help="External machine/approval JSON, outside the immutable release",
    )
    parser.add_argument("--scratch-approved", action="store_true")
    parser.add_argument("--arguments", help="JSON file for one explicit diagnostic/bridge/MCP call")
    parser.add_argument("--tool")
    parser.add_argument("--attempt")
    parser.add_argument("--repeat", type=int, default=10)
    parser.add_argument(
        "action",
        choices=[
            "inventory",
            "diagnostic",
            "bridge",
            "mcp",
            "author",
            "revise",
            "reconnect",
            "reopen-readback",
            "recover",
            "reconcile",
            "snapshot-timing",
            "resume",
        ],
    )
    args = parser.parse_args()
    config = load(args.config)
    output = Path(config["evidence_directory"])
    output.mkdir(parents=True, exist_ok=True)
    log = output / (time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) + "-" + args.action + ".jsonl")
    record(log, "run_metadata", configuration=config, actual_host_qualification="pending")
    state_path = output / "state.json"
    if args.action in ("inventory", "diagnostic"):
        request = load(args.arguments) if args.arguments else {"op": "inventory", "track_names": []}
        if request["op"] in (
            "note_shapes",
            "clear_groove",
            "display_same_value_set",
            "scene_flags_same_value_set",
        ):
            observed = scratch(config, log, args.scratch_approved)
            # Port 9002 has effects. Prove the paired primary script before
            # issuing its independent diagnostic setter, then close cleanly.
            with Mcp(config, log) as client:
                prime_native_session(client, config)
            request["scratch_approved"] = True
            request["expected_set_name"] = config["scratch_set_name"]
            request["expected_document_token"] = observed["document_token"]
            request["expected_bridge_instance"] = config["scratch_bridge_instance"]
            request["expected_native_document_token"] = config["scratch_document_token"]
        dump(output / (log.stem + ".json"), diagnostic(config, log, request))
        return
    if args.action in ("bridge", "snapshot-timing"):
        if args.action == "bridge":
            request = load(args.arguments)
            if request.get("type") != "get" and request.get("name") not in (
                "sunny_get_target_profile",
                "sunny_get_target_snapshot",
                "sunny_managed_context",
                "sunny_managed_operation",
                "sunny_ordinary_operation",
                "sunny_legacy_operation",
                "get_all_notes_extended",
                "get_notes_extended",
                "get_notes_by_id",
                "sunny_get_step_envelope",
                "sunny_get_remote_log",
            ):
                scratch(config, log, args.scratch_approved)
                if (
                    request.get("bridge_protocol_version")
                    != config["expected_bridge_protocol_version"]
                ):
                    raise ValueError("Command protocol differs from the paired release")
                if set(request) != {"bridge_protocol_version", "type", "path", "name", "args"}:
                    raise ValueError("Select one closed type/path/name/args command")
                with Mcp(config, log) as client:
                    prime_native_session(client, config)
                    if Path(config["host_workspace"]).exists():
                        ok(client, "workspace_open", path=config["server_workspace"])
                    ok(client, "workspace_save", path=config["server_workspace"])
                    command = {key: request[key] for key in ("type", "path", "name", "args")}
                    result = legacy_call(client, config, command)
                    record(log, "legacy_gateway_complete_receipt", result=result)
            else:
                bridge_rpc(config, request, log)
        else:
            if not 1 <= args.repeat <= 100:
                raise ValueError("Bounded timing repetitions1..100")
            for _ in range(args.repeat):
                response = bridge_rpc(
                    config,
                    {
                        "bridge_protocol_version": config["expected_bridge_protocol_version"],
                        "type": "call",
                        "path": "song",
                        "name": "sunny_get_target_snapshot",
                        "args": [],
                    },
                    log,
                    config.get("snapshot_observation_timeout", 35.0),
                )
                if (
                    response.get("success") is not True
                    or response.get("bridge_protocol_version")
                    != config["expected_bridge_protocol_version"]
                    or response["value"]["schema_version"]
                    != config["expected_target_snapshot_schema_version"]
                ):
                    raise RuntimeError("Actual snapshot unavailable or wrong protocol/schema")
                if (
                    response["value"]["target_profile"]["adapter"]["source_sha256"]
                    != config["expected_bridge_source_sha256"]
                ):
                    raise RuntimeError("Actual snapshot originated from different source bytes")
        return
    if args.action not in ("reconnect", "reopen-readback", "reconcile"):
        scratch(config, log, args.scratch_approved)
    with Mcp(config, log) as client:
        if args.action not in ("reconnect", "reopen-readback", "reconcile"):
            prime_native_session(client, config)
        schemas = client.request("tools/list", {})
        record(log, "tool_schemas", tools=schemas)
        session = ok(client, "get_ableton_session_state")
        profile = session["target_profile"]
        if len(schemas["tools"]) != config["expected_mcp_tool_count"]:
            raise RuntimeError("MCP tool inventory differs from pinned artifact")
        if (
            profile["bridge_protocol_version"] != config["expected_bridge_protocol_version"]
            or profile["adapter"]["source_sha256"] != config["expected_bridge_source_sha256"]
        ):
            raise RuntimeError("Installed bridge identity differs from pinned artifact")
        if args.action == "mcp":
            ok(client, args.tool, **load(args.arguments))
            return
        if args.action == "reconcile":
            if not args.attempt:
                raise ValueError("Select the original retained attempt token explicitly")
            ok(client, "project_realization_reconcile", attempt_id=args.attempt)
            return
        if args.action == "author":
            if state_path.exists() or Path(config["host_workspace"]).exists():
                raise RuntimeError("Existing qualification workspace/state; do not recreate")
            state = author(client, config)
            state.update(
                mixer_tolerance_db=config["mixer_tolerance_db"],
                send_tolerance_db=config["send_tolerance_db"],
            )
            dump(state_path, state)  # owning selections saved before any native dispatch
            result = apply(client, selection(client, state, True), config)
            save_binding_state(state, result)
            state["initial_native_ids"] = independent_notes(config, log, state, revised=False)
            independent_physical(config, log, state)
            state["initial_native_capture"] = diagnostic(
                config, log, {"op": "inventory", "track_names": list(state["track_tags"].values())}
            )
            diagnostic(
                config,
                log,
                {
                    "op": "retain_selected",
                    "key": "initial",
                    "track_names": list(state["track_tags"].values()),
                },
            )
            state["revised"] = False
            dump(state_path, state)
            direct_samples(config, log, state)
        else:
            state = load(state_path)
            if args.action == "resume":
                # Only after explicit original-token reconciliation; IR is already final.
                result = apply(
                    client,
                    selection(client, state, not state.get("revision_authored", False)),
                    config,
                )
                save_binding_state(state, result)
                state["revised"] = bool(state.get("revision_authored", False))
                actual_ids = independent_notes(
                    config, log, state, state["revised"], state.get("initial_native_ids")
                )
                actual_capture = independent_physical(config, log, state)
                if state["revised"]:
                    state["final_native_ids"] = actual_ids
                    state["saved_native_capture"] = actual_capture
                else:
                    state["initial_native_ids"] = actual_ids
                    state["initial_native_capture"] = actual_capture
                    diagnostic(
                        config,
                        log,
                        {
                            "op": "retain_selected",
                            "key": "initial",
                            "track_names": list(state["track_tags"].values()),
                        },
                    )
                dump(state_path, state)
                direct_samples(config, log, state, state["revised"])
            elif args.action == "revise":
                if state.get("revised"):
                    raise RuntimeError("Revision already applied; no automatic replay")
                ok(client, "score_insert_measures", score_id=1, after_bar=2, count=1)
                ok(
                    client,
                    "score_transpose",
                    score_id=1,
                    region={"start_bar": 1, "end_bar": 1, "parts": [1]},
                    interval={"chromatic": 2, "diatonic": 1},
                )
                insert(client, 1, 3, "D", (1, 4), 74)
                ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[2], denominator=2)
                ok(client, "remove_mix_automation", graph_id=1, index=0)
                ok(
                    client,
                    "add_mix_automation",
                    graph_id=1,
                    target="channels[1].spatial.pan",
                    interpolation=0,
                    breakpoints=[
                        {"bar": 1, "beat_num": 0, "beat_den": 1, "value": -0.25},
                        {"bar": 1, "beat_num": 1, "beat_den": 4, "value": 0.25},
                    ],
                )
                state["revision_authored"] = True
                ok(client, "workspace_save", path=config["server_workspace"])
                dump(state_path, state)
                result = apply(client, selection(client, state, False), config)
                save_binding_state(state, result)
                state["revised"] = True
                state["final_native_ids"] = independent_notes(
                    config, log, state, True, state["initial_native_ids"]
                )
                proof = diagnostic(
                    config,
                    log,
                    {
                        "op": "compare_retained",
                        "key": "initial",
                        "track_names": list(state["track_tags"].values()),
                    },
                )
                if not proof["same_selected_count"] or not all(
                    all(row.values()) for row in proof["proof"]
                ):
                    raise AssertionError(
                        "Native object/parameter identity changed across ordinary revision"
                    )
                independent_physical(config, log, state)
                state["saved_native_capture"] = diagnostic(
                    config,
                    log,
                    {"op": "inventory", "track_names": list(state["track_tags"].values())},
                )
                dump(state_path, state)
                direct_samples(config, log, state, True)
            elif args.action == "recover":
                recover(client, state, config)
            else:
                for token in state["phase_attempt_ids"]:
                    result = ok(client, "project_realization_inspect", attempt_id=token)
                    record(log, "historical_attempt_readonly", token=token, evidence=result)
                diagnostic(
                    config,
                    log,
                    {"op": "inventory", "track_names": list(state["track_tags"].values())},
                )
                independent_notes(
                    config,
                    log,
                    state,
                    state.get("revised", False),
                    state.get("final_native_ids", state.get("initial_native_ids"))
                    if args.action == "reconnect"
                    else None,
                )
                direct_samples(config, log, state, state.get("revised", False))
                independent_physical(config, log, state)
                if args.action == "reopen-readback":
                    saved = state.get("saved_native_capture", state.get("initial_native_capture"))
                    current = diagnostic(
                        config,
                        log,
                        {"op": "inventory", "track_names": list(state["track_tags"].values())},
                    )
                    compare_saved_native(saved, current)
                    record(
                        log,
                        "save_reopen_finite_native_values_equal",
                        result=True,
                        note_ids_may_change=True,
                        complete_envelope_population_observed=False,
                        mpe_and_follow_actions_observed=False,
                    )
                # No plan/apply, adoption or native setter in readback-only modes.
        ok(client, "workspace_save", path=config["server_workspace"]) if args.action in (
            "author",
            "revise",
            "recover",
            "resume",
        ) else None
        ok(client, "get_ableton_remote_log", after_sequence=0)
    print(str(log))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1) from exc
