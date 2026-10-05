"""Finite direct Python host observations; install only for final scratch-host review.

No eval, arbitrary paths, implicit retries, object deletion, parameter remapping,
Sunny registry imports or save automation. Port9002 is independent of Sunny9001.
"""

import json
import math
import os
import socket
import struct
import threading
import time
import uuid

import Live
from _Framework.ControlSurface import ControlSurface

NOTE_FIELDS = (
    "note_id",
    "pitch",
    "start_time",
    "duration",
    "velocity",
    "mute",
    "probability",
    "velocity_deviation",
    "release_velocity",
)
MAX_BYTES = 16 * 1024 * 1024
PREFIX = "SUNNY_HOST_QUALIFICATION_"
TRACK_PREFIX = "SUNNY_HOST_PROBE_"


def typename(value):
    """Record the native runtime type rather than infer it from JSON."""
    return type(value).__module__ + "." + type(value).__name__


def observed(value, limit=128):
    """Record native type independently from iterable acceptance or JSON conversion."""
    result = {"python_type": typename(value), "repr": repr(value)[:4096]}
    if value is None or isinstance(value, (bool, str, int)):
        result["value"] = value
    elif isinstance(value, float):
        result["value"] = value if math.isfinite(value) else str(value)
        result["hex"] = value.hex()
    else:
        try:
            result["length"] = len(value)
            result["items"] = [
                observed(item, 0) if limit else typename(item) for item in tuple(value)[:limit]
            ]
            result["entire_population_observed"] = len(value) <= limit
        except Exception as exc:
            result["sequence_error"] = repr(exc)
    return result


def field(obj, name):
    """Capture a property or its actual getter failure without a fallback value."""
    try:
        return observed(getattr(obj, name))
    except Exception as exc:
        return {"getter_error": repr(exc)}


def notes(value):
    """Record every returned native note field independently of Sunny receipts."""
    return {
        "container": observed(value, 0),
        "notes": [{key: field(note, key) for key in NOTE_FIELDS} for note in value],
    }


class HostProbe(ControlSurface):
    """Observe finite native capabilities with separately gated scratch mutations."""

    def __init__(self, c_instance):
        """Initialize the single process or native probe without granting mutation authority."""
        super().__init__(c_instance)
        self._constructor_thread = threading.get_ident()
        self._stop = threading.Event()
        self._retained = {}
        self._mutations = []
        self._snapshots = {}
        self._document_song = None
        self._document_token = None
        self._socket_errors = []
        self._socket_errors_lock = threading.Lock()
        self._server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind(
            (
                os.environ.get("SUNNY_PROBE_BIND_HOST", "127.0.0.1"),
                int(os.environ.get("SUNNY_PROBE_PORT", "9002")),
            )
        )
        self._server.listen(1)
        self._server.settimeout(1)
        self._worker = threading.Thread(
            target=self._serve, name="SunnyHostProbeSocket", daemon=True
        )
        self._worker.start()
        self.log_message(
            "SunnyHostProbe ready; separate diagnostic surface, no host qualification yet"
        )

    def _exact(self, connection, size):
        out = bytearray()
        while len(out) < size:
            part = connection.recv(size - len(out))
            if not part:
                raise EOFError("Incomplete diagnostic frame")
            out.extend(part)
        return bytes(out)

    def _serve(self):
        while not self._stop.is_set():
            try:
                connection, _ = self._server.accept()
            except TimeoutError:
                continue
            except OSError:
                return
            with connection:
                connection.settimeout(35)
                try:
                    size = struct.unpack(">I", self._exact(connection, 4))[0]
                    if not 0 < size <= 65536:
                        raise ValueError("Diagnostic request limit64KiB")
                    request = json.loads(self._exact(connection, size))
                    done = threading.Event()
                    response = {}
                    socket_thread = threading.get_ident()
                    started = time.monotonic()
                    phase = {"started": False, "cancelled": False}
                    lock = threading.Lock()

                    def callback():
                        with lock:
                            if phase["cancelled"]:
                                return
                            phase["started"] = True
                        try:
                            if threading.get_ident() != self._constructor_thread:
                                raise RuntimeError(
                                    "Wrong scheduled callback thread; no Live API accessed"
                                )
                            result = self._dispatch(request)
                            response.update(success=True, result=result)
                        except Exception as exc:
                            response.update(success=False, error=repr(exc))
                        finally:
                            response["dispatch"] = {
                                "constructor_thread": self._constructor_thread,
                                "socket_thread": socket_thread,
                                "callback_thread": threading.get_ident(),
                                "delay_requested": 0,
                                "seconds": time.monotonic() - started,
                            }
                            done.set()

                    self.schedule_message(0, callback)
                    if not done.wait(30):
                        with lock:
                            if not phase["started"]:
                                phase["cancelled"] = True
                                response.update(
                                    success=False,
                                    error="Diagnostic callback cancelled before start",
                                )
                        if phase["started"]:
                            done.wait()  # no timeout pretending a started mutation was cancelled
                    body = json.dumps(response, allow_nan=False).encode("utf-8")
                    if len(body) > MAX_BYTES:
                        body = json.dumps(
                            {"success": False, "error": "Diagnostic evidence exceeds16MiB"}
                        ).encode()
                    connection.sendall(struct.pack(">I", len(body)) + body)
                except Exception as exc:
                    with self._socket_errors_lock:
                        self._socket_errors.append(repr(exc)[:1024])
                        self._socket_errors[:] = self._socket_errors[-100:]

    def _observe_document(self, song):
        """Identify the actual native Song; names alone never establish identity."""
        previous = self._document_song
        if previous is None or not (song is previous or song == previous):
            self._document_song = song
            self._document_token = uuid.uuid4().hex
        return self._document_token

    def _scratch(self, request):
        song = self.song()
        current_token = self._observe_document(song)
        if (
            request.get("scratch_approved") is not True
            or not isinstance(request.get("expected_set_name"), str)
            or request["expected_set_name"] != song.name
            or not song.name.startswith(PREFIX)
            or request.get("expected_document_token") != current_token
        ):
            raise RuntimeError(
                "Native mutation requires approval for the exact observed scratch document"
            )
        if song.is_playing or song.session_record or song.record_mode:
            raise RuntimeError("Stop transport and recording before diagnostic mutations")
        if self._monitoring_observation(song).get("all_off") is not True:
            raise RuntimeError("Disable input monitoring on all scratch Tracks before mutations")
        return song

    @staticmethod
    def _monitoring_observation(song):
        """Read the native OFF enum; absent/unreadable monitoring is not a safe result."""
        try:
            # Source observation: pinned Live Remote Scripts Push2/routing.py
            # refers to this native enum. Exact-host binding remains unqualified.
            off = Live.Track.Track.monitoring_states.OFF
            values = [track.current_monitoring_state for track in song.tracks]
            return {
                "off_state": observed(off),
                "track_states": [observed(value) for value in values],
                "all_off": all(value == off for value in values),
            }
        except Exception as exc:
            return {"getter_error": repr(exc)}

    def _owned_probe_clip(self, request):
        song = self._scratch(request)
        matches = [track for track in song.tracks if track.name == request["track_name"]]
        if len(matches) != 1 or not matches[0].name.startswith(TRACK_PREFIX):
            raise RuntimeError("Select one dedicated diagnostic Track by exact probe tag")
        clip = matches[0].clip_slots[0].clip
        if clip is None or not clip.is_midi_clip:
            raise RuntimeError("Expected diagnostic MIDI Clip in Session slot0")
        return matches[0], clip

    def _parameter(self, track, original_name, device_index=None):
        if device_index is None:
            return getattr(track.mixer_device, original_name)
        device = tuple(track.devices)[device_index]
        matches = [p for p in device.parameters if p.original_name == original_name]
        if len(matches) != 1:
            raise RuntimeError("Original parameter name must be unique in actual Device")
        return matches[0]

    def _routing_rows(self, values):
        rows = []
        for route in values:
            row = {
                "python_type": typename(route),
                "hash": observed(hash(route)),
                "category": field(route, "category"),
                "display_name": field(route, "display_name"),
                "attached_object": field(route, "attached_object"),
            }
            try:
                attached = route.attached_object
                row["attached_native_target"] = {
                    "python_type": typename(attached),
                    "name": field(attached, "name"),
                    "is_actual_main": attached == self.song().master_track,
                    "track_indices": [i for i, t in enumerate(self.song().tracks) if t == attached],
                    "return_indices": [
                        i for i, t in enumerate(self.song().return_tracks) if t == attached
                    ],
                }
            except Exception as exc:
                row["attachment_error"] = repr(exc)
            rows.append(row)
        return rows

    def _routing_property_rows(self, track, name):
        try:
            return self._routing_rows(getattr(track, name))
        except Exception as exc:
            return {"getter_error": repr(exc)}

    def _inventory(self, selected_names):
        song = self.song()
        app = Live.Application.get_application()
        result = {
            "version": app.get_version_string(),
            "python_runtime": __import__("sys").version,
            "set_name": field(song, "name"),
            "set_file": field(song, "file_path"),
            "document_token": self._observe_document(song),
            "input_monitoring": self._monitoring_observation(song),
            "song": {
                key: field(song, key)
                for key in (
                    "scale_intervals",
                    "scenes",
                    "cue_points",
                    "tracks",
                    "return_tracks",
                    "is_playing",
                    "session_record",
                    "record_mode",
                    "current_song_time",
                    "tempo",
                    "signature_numerator",
                    "signature_denominator",
                )
            },
            "scene0": {
                key: field(song.scenes[0], key)
                for key in ("tempo", "tempo_enabled", "time_signature_enabled")
            },
            "tracks": [],
            "mutation_log": list(self._mutations),
            "socket_errors": list(self._socket_errors),
        }
        try:
            tempo_parameter = song.master_track.mixer_device.song_tempo
            result["main_song_tempo_parameter"] = {
                "python_type": typename(tempo_parameter),
                **{
                    key: field(tempo_parameter, key)
                    for key in (
                        "name",
                        "original_name",
                        "value",
                        "min",
                        "max",
                        "is_enabled",
                        "state",
                        "automation_state",
                    )
                },
            }
        except Exception as exc:
            result["main_song_tempo_parameter"] = {"getter_error": repr(exc)}
        for index, track in enumerate(song.tracks):
            if selected_names and track.name not in selected_names:
                continue
            row = {
                "index": index,
                "name": field(track, "name"),
                "mute": field(track, "mute"),
                "solo": field(track, "solo"),
                "current_monitoring_state": field(track, "current_monitoring_state"),
                "containers": {
                    key: field(track, key) for key in ("devices", "arrangement_clips", "take_lanes")
                },
                "mixer_in_devices": any(d == track.mixer_device for d in track.devices),
                "routing": {
                    key: field(track, key)
                    for key in (
                        "available_output_routing_types",
                        "available_output_routing_channels",
                        "output_routing_type",
                        "output_routing_channel",
                    )
                },
                "devices": [],
                "mixer": {},
                "clips": [],
                "routing_native_rows": {
                    "types": self._routing_property_rows(track, "available_output_routing_types"),
                    "channels": self._routing_property_rows(
                        track, "available_output_routing_channels"
                    ),
                },
            }
            for key in ("volume", "panning"):
                parameter = getattr(track.mixer_device, key)
                row["mixer"][key] = {
                    name: field(parameter, name)
                    for name in (
                        "name",
                        "original_name",
                        "value",
                        "min",
                        "max",
                        "display_value",
                        "automation_state",
                        "state",
                    )
                }
                row["mixer"][key]["actual_str_for_value"] = self._format(parameter)
            row["mixer"]["sends"] = [
                {
                    "display_value": field(p, "display_value"),
                    "value": field(p, "value"),
                    "actual_str_for_value": self._format(p),
                }
                for p in track.mixer_device.sends
            ]
            for device in track.devices:
                native = {
                    key: field(device, key)
                    for key in (
                        "class_name",
                        "class_display_name",
                        "name",
                        "type",
                        "is_active",
                        "global_mode",
                        "edit_mode",
                        "oversample",
                        "voice_mode",
                        "voice_modes",
                        "voice_count",
                        "voice_counts",
                        "mode",
                    )
                }
                native["is_mixer"] = device == track.mixer_device
                try:
                    parameters = device.parameters
                    native["parameters_type"] = typename(parameters)
                    native["parameters"] = [
                        {
                            **{
                                key: field(p, key)
                                for key in (
                                    "name",
                                    "original_name",
                                    "min",
                                    "max",
                                    "value",
                                    "is_quantized",
                                    "is_enabled",
                                    "state",
                                    "automation_state",
                                    "display_value",
                                )
                            },
                            **(
                                {"value_items": field(p, "value_items")}
                                if p.is_quantized
                                else {"default_value": field(p, "default_value")}
                            ),
                            "actual_str_for_value": self._format(p),
                        }
                        for p in parameters
                    ]
                except Exception as exc:
                    native["parameters_error"] = repr(exc)
                row["devices"].append(native)
            for slot_index, slot in enumerate(track.clip_slots):
                if slot.clip is None:
                    continue
                clip = slot.clip
                summary = {
                    "slot": slot_index,
                    **{
                        key: field(clip, key)
                        for key in (
                            "name",
                            "start_marker",
                            "end_marker",
                            "loop_start",
                            "loop_end",
                            "looping",
                            "has_envelopes",
                            "has_groove",
                            "groove",
                            "signature_numerator",
                            "signature_denominator",
                        )
                    },
                }
                if clip.is_midi_clip:
                    try:
                        summary["full_notes"] = notes(clip.get_all_notes_extended())
                    except Exception as exc:
                        summary["full_notes_error"] = repr(exc)
                row["clips"].append(summary)
            result["tracks"].append(row)
        return result

    def _format(self, parameter):
        try:
            return observed(parameter.str_for_value(parameter.value))
        except Exception as exc:
            return {"formatter_error": repr(exc)}

    def _dispatch(self, request):
        if threading.get_ident() != self._constructor_thread:
            raise RuntimeError("Wrong callback thread; no Live API accessed")
        name = request["op"]
        if name == "inventory":
            return self._inventory(request.get("track_names", []))
        if name == "note_shapes":
            song = self._scratch(request)
            tag = TRACK_PREFIX + "NOTES_" + request["run_id"]
            if any(track.name == tag for track in song.tracks):
                raise RuntimeError("Existing probe tag; do not recreate or replay")
            if not song.scenes:
                raise RuntimeError("Scene0 must already exist")
            before = tuple(song.tracks)
            song.create_midi_track(-1)
            created = [track for track in song.tracks if track not in before]
            if len(created) != 1:
                raise RuntimeError("Unknown create outcome; inspect manually, do not replay")
            track = created[0]
            track.name = tag
            track.current_monitoring_state = Live.Track.Track.monitoring_states.OFF
            track.clip_slots[0].create_clip(4.0)
            clip = track.clip_slots[0].clip
            clip.name = tag
            clip.looping = False
            clip.start_marker = 0.0
            clip.end_marker = 4.0
            requested = [
                (0, 0.0, 0.25),
                (60, 1.0 / 3.0, 1.0 / 3.0),
                (127, 3.75, 1.0),
                (62, 4.0, 0.25),
                (64, 8.0, 0.25),
            ]
            specifications = tuple(
                Live.Clip.MidiNoteSpecification(
                    pitch=p,
                    start_time=t,
                    duration=d,
                    velocity=96.0,
                    mute=False,
                    probability=1.0,
                    velocity_deviation=0.0,
                    release_velocity=64.0,
                )
                for p, t, d in requested
            )
            returned = clip.add_new_notes(specifications)
            self._mutations.append({"op": name, "track_name": tag, "requested": requested})
            result = {
                "track_name": tag,
                "requested": requested,
                "specifications_type": typename(specifications),
                "insertion_return": observed(returned),
                "ranged": notes(
                    clip.get_notes_extended(
                        from_pitch=0, pitch_span=128, from_time=0.0, time_span=4.0
                    )
                ),
                "entire_clip_population_observed_by_range": False,
            }
            if returned is not None:
                try:
                    result["by_returned_ids"] = notes(clip.get_notes_by_id(tuple(returned)))
                except Exception as exc:
                    result["by_returned_ids_error"] = repr(exc)
            try:
                result["all"] = notes(clip.get_all_notes_extended())
            except Exception as exc:
                result["all_error"] = repr(exc)
            self._retained[tag] = (track, clip)
            return result
        if name in ("retain_selected", "compare_retained"):
            selected = []
            for tag in request["track_names"]:
                matches = [t for t in self.song().tracks if t.name == tag]
                if len(matches) != 1:
                    raise RuntimeError("Independent selected tag must resolve uniquely")
                track = matches[0]
                clip = track.clip_slots[0].clip
                devices = tuple(track.devices)
                parameters = tuple(tuple(d.parameters) for d in devices if d != track.mixer_device)
                selected.append(
                    (
                        track,
                        clip,
                        devices,
                        parameters,
                        track.mixer_device.volume,
                        track.mixer_device.panning,
                        tuple(track.mixer_device.sends),
                    )
                )
            if name == "retain_selected":
                if request["key"] in self._snapshots:
                    raise RuntimeError("Existing independent baseline; never refresh silently")
                self._snapshots[request["key"]] = tuple(selected)
                return {"baseline_retained": True, "authority_granted": False}
            before = self._snapshots[request["key"]]

            def same(a, b):
                return a is b or a == b

            def same_seq(a, b):
                return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))

            proof = []
            for old, current in zip(before, selected):
                proof.append(
                    {
                        "track_same": same(old[0], current[0]),
                        "clip_same": same(old[1], current[1]),
                        "device_cohort_same": same_seq(old[2], current[2]),
                        "parameter_cohort_same": len(old[3]) == len(current[3])
                        and all(same_seq(x, y) for x, y in zip(old[3], current[3])),
                        "volume_same": same(old[4], current[4]),
                        "pan_same": same(old[5], current[5]),
                        "send_cohort_same": same_seq(old[6], current[6]),
                    }
                )
            return {
                "same_selected_count": len(before) == len(selected),
                "proof": proof,
                "authority_granted": False,
            }
        if name == "scene_flags_same_value_set":
            song = self._scratch(request)
            matches = [s for s in song.scenes if s.name == "SUNNY_HOST_PROBE_SCENE"]
            if len(matches) != 1:
                raise RuntimeError("Manually create one diagnostic Scene with exact name first")
            scene = matches[0]
            result = {}
            for key in ("tempo_enabled", "time_signature_enabled"):
                result[key] = {"before": field(scene, key)}
                try:
                    current = getattr(scene, key)
                    self._mutations.append({"op": name, "property": key, "started": True})
                    setattr(scene, key, current)
                    result[key]["after"] = field(scene, key)
                except Exception as exc:
                    result[key]["setter_error"] = repr(exc)
            return result
        if name == "envelope_samples":
            song = self.song()
            matches = [t for t in song.tracks if t.name == request["track_name"]]
            if len(matches) != 1:
                raise RuntimeError("Current actual tagged Track must be unique")
            track = matches[0]
            clip = track.clip_slots[request.get("slot", 0)].clip
            parameter = track.mixer_device.panning
            envelope = clip.automation_envelope(parameter)
            valid = Live.Base.liveobj_valid(envelope)
            return {
                "track_name": track.name,
                "clip_name": clip.name,
                "parameter_type": typename(parameter),
                "envelope_type": typename(envelope),
                "envelope_valid": valid,
                "samples": [
                    {"time": t, "value": observed(envelope.value_at_time(t))}
                    for t in request["times"]
                ]
                if valid
                else [],
                "no_native_setters": True,
                "entire_breakpoint_population_observed": False,
                "same_parameter_modulation_preserved": "unobserved",
            }
        if name == "clear_groove":
            _, clip = self._owned_probe_clip(request)
            before = {"groove": field(clip, "groove"), "has_groove": field(clip, "has_groove")}
            clip.groove = None
            self._mutations.append({"op": name, "track_name": request["track_name"]})
            return {
                "before": before,
                "after": {"groove": field(clip, "groove"), "has_groove": field(clip, "has_groove")},
            }
        if name == "display_same_value_set":
            track, _ = self._owned_probe_clip(request)
            parameter = self._parameter(track, "volume")
            before = field(parameter, "display_value")
            value = parameter.display_value  # actual getter type, no fabricated unit conversion
            parameter.display_value = value
            self._mutations.append({"op": name, "track_name": track.name})
            return {"before": before, "after": field(parameter, "display_value")}
        raise ValueError("Unknown diagnostic operation; no eval or generic native mutation")

    def disconnect(self):
        """Stop the optional qualification socket and release the control surface."""
        self._stop.set()
        self._server.close()
        super().disconnect()
