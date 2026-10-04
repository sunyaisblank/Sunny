"""Literal native-device oracles through actual retained Clip bindings; no host claim."""

from __future__ import annotations

import copy
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import Device, DeviceParameter, DeviceType, LiveSet, MidiNoteVector
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.managed import ManagedRegistry, _digest
from Sunny.managed_devices import ManagedDevices, valid_managed_device_request


class Parameter(DeviceParameter):
    """Writable native fixture whose display curve is independently specified."""

    def __init__(self, name: str, oracle: Any = None, **kwargs: Any) -> None:
        """Keep exact observed domain and independently chosen display function."""
        super().__init__(name, **kwargs)
        self.oracle = oracle
        self.calls: list[float] = []
        self.writes: list[float] = []
        self.fail = False
        self.clamp = False
        self.callback: Any = None

    def str_for_value(self, value: float) -> str:
        """Format only the literal oracle; record every genuine formatter call."""
        self.calls.append(value)
        if self.callback:
            self.callback(value)
        return self.oracle(value)

    def _write(self, value: float) -> None:
        self.writes.append(value)
        if self.fail:
            raise RuntimeError("literal setter failure")
        super()._write(0.25 if self.clamp else value)


def enum(name: str, labels: tuple[str, ...], index: int) -> Parameter:
    """Advertise actual enum labels with deliberately nonstandard order."""
    return Parameter(name, maximum=len(labels) - 1, value=index, value_items=labels)


def device(browser: str) -> Device:
    """Create a native class with explicit nonlinear Hz/ms and Utility controls."""
    if browser == "Drift":
        result = Device("Drift", "Drift", DeviceType.instrument)
        parameters = [
            enum("Device On", ("Off", "On"), 1),
            Parameter("LP Freq", lambda v: f"{200 + 4000 * v * v:.2f} Hz"),
            Parameter("Env 1 Attack", lambda v: f"{1000 * v * v:.2f} ms"),
            Parameter("Env 1 Decay", lambda v: f"{v * v:.4f} s"),
            Parameter("Env 1 Release", lambda v: f"{4 * v * v:.4f} s"),
            enum("LP Type", ("II", "I"), 1),
        ]
        result.voice_mode_index = 1
        result.voice_mode_list = ("Mono", "Poly", "Unison", "Stereo")
        result.voice_count_index = 2
        result.voice_count_list = ("1", "2", "8", "32")
    elif browser == "Utility":
        result = Device("Utility", "StereoGain", DeviceType.audio_effect)
        parameters = [
            enum("Device On", ("Off", "On"), 1),
            Parameter("Gain", lambda v: f"{24 * v - 12:.2f} dB"),
            Parameter(
                "Balance",
                lambda v: "C"
                if v == 0.5
                else f"{abs(2 * v - 1) * 50:.2f}{'L' if v < 0.5 else 'R'}",
            ),
            Parameter("Stereo Width", lambda v: f"{200 * v:.2f} %"),
            enum("Channel Mode", ("Right", "Stereo", "Swap", "Left"), 1),
            enum("Mono", ("On", "Off"), 1),
            enum("Mute", ("Off", "On"), 0),
        ]
    else:
        raise RuntimeError("literal unavailable edition device")
    result._parameters = tuple(parameters)
    for parameter in parameters:
        parameter._canonical_parent = result
    return result


class Registry(ManagedRegistry):
    """Connect only the agreed narrow helper hooks to the actual existing registry."""

    def __init__(self, surface: Any) -> None:
        """Attach the tested helper without changing the production dispatcher."""
        super().__init__(surface)
        self.devices = ManagedDevices(self)

    def _create(
        self,
        song: Any,
        binding: tuple[str, str],
        request: dict[str, Any],
        operation: dict[str, Any],
    ) -> dict[str, Any]:
        """The actual creator alone grants initial source insertion authority."""
        result: dict[str, Any] = super()._create(song, binding, request, operation)
        self.devices.retain_created_track_authority(self._bindings[binding])
        return result

    def _capture(self, record: dict[str, Any]) -> dict[str, Any]:
        return {**super()._capture(record), **self.devices.capture(record)}

    def _require_in_place_guard(self, record: dict[str, Any], fingerprint: str) -> dict[str, Any]:
        self.devices.verify_retained_chain(record)
        return super()._require_in_place_guard(record, fingerprint)

    def _seal(
        self, record: dict[str, Any], authorized_device_change: bool = False
    ) -> dict[str, Any]:
        observed = self._capture(record)
        self.devices.seal(record, observed, authorized_device_change)
        return super()._seal(record)


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    """Create a real managed Clip then expose an independent native insertion provider."""
    live = LiveSet(midi_tracks=1, return_tracks=1, python_envelope_api=True).install(monkeypatch)
    registry = Registry(live.surface)
    handler = LomHandler(
        live.surface, managed_registry=registry, envelope_authorizer=registry.authorize_envelope
    )
    registry.attach_handler(handler)
    context = registry.dispatch("sunny_managed_context", [])
    request = {
        "document_token": context["document_token"],
        "operation_id": "create",
        "project_key": "project_a",
        "binding_key": "part_a",
        "clip_end": 4.0,
        "signature_numerator": 4,
        "signature_denominator": 4,
        "notes": [
            {
                "pitch": 60,
                "start_time": 0.0,
                "duration": 1.0,
                "velocity": 96,
                "mute": False,
                "probability": 1.0,
                "velocity_deviation": 0.0,
                "release_velocity": 64.0,
            }
        ],
    }
    response = handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_managed_create_clip",
            "args": [request],
        }
    )
    assert response["success"] and response["value"]["outcome"] == "acknowledged"
    record = registry._bindings[("project_a", "part_a")]
    inserted: list[str] = []

    def insert(browser: str, index: int) -> None:
        inserted.append(browser)
        created = device(browser)
        created._canonical_parent = record["track"]
        record["track"]._devices.insert(index, created)

    monkeypatch.setattr(record["track"], "insert_device", insert)
    return SimpleNamespace(registry=registry, record=record, context=context, inserted=inserted)


def request(
    target: Any, key: str, browser: str = "Drift", intents: list[dict[str, Any]] | None = None
) -> dict[str, Any]:
    """Build one exact finite operation from its current observed independent binding."""
    observed = target.registry._capture(target.record)
    return {
        "document_token": target.context["document_token"],
        "operation_id": key,
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": observed["content_fingerprint"],
        "expected_device_identity_fingerprint": observed["device_identity_fingerprint"],
        "device_key": key,
        "device": {
            "browser_name": browser,
            "class_name": {"Drift": "Drift", "Utility": "StereoGain", "EQ Eight": "Eq8"}[browser],
            "type": 1 if browser == "Drift" else 2,
            "role": "source" if browser == "Drift" else "effect",
            "insertion_policy": "AppendOwnedChain",
        },
        "physical_intents": intents or [],
    }


def apply(target: Any, payload: dict[str, Any], insert: bool = True) -> dict[str, Any]:
    """Exercise helper only inside an explicitly retained pending journal entry."""
    name = "sunny_managed_insert_device" if insert else "sunny_managed_update_device_parameters"
    operation = {
        "name": name,
        "request": copy.deepcopy(payload),
        "outcome": "pending",
        "native_mutation_started": False,
        "operation_id": payload["operation_id"],
        "document_token": payload["document_token"],
        "request_fingerprint": _digest({"name": name, "request": payload}),
    }
    target.registry._operations[payload["operation_id"]] = operation
    result = target.registry.devices.apply(name, target.record, payload, operation)
    operation.update({"outcome": "acknowledged", "result": result})
    return result


def test_drift_batch_uses_literal_nonlinear_readbacks_and_preserves_notes(target: Any) -> None:
    """Actual submitted0.5 gives1200Hz/250ms; all four controls resolve before setters."""
    before = target.registry._capture(target.record)
    payload = request(
        target,
        "source",
        intents=[
            {"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0},
            {"capability_id": "drift.env.1.attack", "target": 250.0, "tolerance": 0.0},
            {"capability_id": "drift.env.1.decay", "target": 250.0, "tolerance": 0.0},
            {"capability_id": "drift.env.1.release", "target": 1000.0, "tolerance": 0.0},
        ],
    )
    result = apply(target, payload)
    assert target.inserted == ["Drift"]
    assert result["manifest"]["track"]["has_audio_output"] is True
    assert result["note_identity"] == before["note_identity"]
    assert result["manifest"]["clip"] == before["manifest"]["clip"]
    assert [item["display_value"] for item in result["device_update"]["readbacks"]] == [
        1200,
        250,
        250,
        1000,
    ]
    assert [item["internal_value"] for item in result["device_update"]["readbacks"]] == [0.5] * 4
    assert result["device_identity"]["cohort"][0]["modes"]["voice_mode"]["label"] == "Poly"
    assert result["device_update"]["opaque_state_observed"] is False
    target.registry.devices.verify_retained_chain(target.record)


def test_late_candidate_decline_preserves_inserted_handle_and_no_parameter_setters(
    target: Any,
) -> None:
    """The second locale-invalid native formatter cannot leave the first target written."""
    original = target.record["track"].insert_device

    def insert(browser: str, index: int) -> None:
        original(browser, index)
        target.record["track"]._devices[-1]._parameters[2].oracle = lambda _: "1,5 ms"

    target.record["track"].insert_device = insert
    payload = request(
        target,
        "source",
        intents=[
            {"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0},
            {"capability_id": "drift.env.1.attack", "target": 250.0, "tolerance": 0.0},
        ],
    )
    with pytest.raises(RuntimeError, match="locale"):
        apply(target, payload)
    state = target.record["_managed_devices"]
    assert state["handles"] == tuple(target.record["track"]._devices)
    assert state["entries"][0]["device_key"] == "source"
    assert not any(parameter.writes for parameter in state["handles"][0].parameters)
    assert target.registry._operations["source"]["native_mutation_started"] is True
    with pytest.raises(RuntimeError, match="drift"):
        target.registry.devices.verify_retained_chain(target.record)


def test_insert_effect_requires_owned_drift_and_exact_native_device_key(target: Any) -> None:
    """Append policy refuses effects first and refuses another source without native calls."""
    with pytest.raises(RuntimeError, match="source first"):
        apply(target, request(target, "invalid_effect_before_source", "Utility"))
    assert target.inserted == []
    apply(target, request(target, "source"))
    with pytest.raises(RuntimeError, match="source first"):
        apply(target, request(target, "second_source"))
    effect = apply(
        target,
        request(
            target,
            "effect",
            "Utility",
            [{"capability_id": "utility.gain", "target": 0.0, "tolerance": 0.0}],
        ),
    )
    assert target.inserted == ["Drift", "Utility"]
    assert effect["device_update"]["readbacks"][0]["internal_value"] == 0.5
    assert [item["device_key"] for item in effect["device_identity"]["cohort"]] == [
        "source",
        "effect",
    ]


def test_identical_replacement_and_unselected_value_drift_decline_before_write(target: Any) -> None:
    """Matching serializable class/values cannot replace actual native Device ownership."""
    apply(target, request(target, "source"))
    payload = request(
        target,
        "source",
        intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
    )
    payload["operation_id"] = "update"
    existing = target.record["track"]._devices[0]
    replacement = device("Drift")
    replacement._canonical_parent = target.record["track"]
    target.record["track"]._devices[0] = replacement
    with pytest.raises(RuntimeError, match="cohort"):
        apply(target, payload, False)
    assert not replacement.parameters[1].writes
    target.record["track"]._devices[0] = existing
    existing.parameters[3]._value = 0.125
    with pytest.raises(RuntimeError, match="state drift"):
        apply(target, payload, False)
    assert not existing.parameters[1].writes


def test_partial_setter_failure_and_native_clamp_remain_unsealed(target: Any) -> None:
    """No compensation erases the first accepted native write or the uncertain second write."""
    apply(target, request(target, "source"))
    native = target.record["track"]._devices[0]
    native.parameters[2].fail = True
    payload = request(
        target,
        "source",
        intents=[
            {"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0},
            {"capability_id": "drift.env.1.attack", "target": 250.0, "tolerance": 0.0},
        ],
    )
    payload["operation_id"] = "update"
    with pytest.raises(RuntimeError, match="setter failure"):
        apply(target, payload, False)
    assert native.parameters[1].value == 0.5
    assert native.parameters[2].value == 0.0
    assert native.parameters[1].writes == [0.5]
    assert native.parameters[2].writes == [0.5]
    with pytest.raises(RuntimeError, match="state drift"):
        target.registry.devices.verify_retained_chain(target.record)


def test_device_baseline_survives_later_note_ack_and_detects_mode_drift(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A native note ACK preserves the source key/guard; Mono mode changes decline later writes."""
    apply(target, request(target, "source"))
    before = target.registry._capture(target.record)
    note = before["note_identity"]["notes"][0]
    clip = target.record["clip"]

    def select(ids: Any) -> MidiNoteVector:
        return MidiNoteVector(copy.deepcopy(clip._notes[i]) for i in ids)

    def revise(notes: Any) -> None:
        for selected in notes:
            for field in (
                "pitch",
                "start_time",
                "duration",
                "velocity",
                "mute",
                "probability",
                "velocity_deviation",
                "release_velocity",
            ):
                setattr(clip._notes[selected.note_id], field, getattr(selected, field))

    monkeypatch.setattr(clip, "get_notes_by_id", select)
    monkeypatch.setattr(clip, "apply_note_modifications", revise, raising=False)
    update = {
        "document_token": target.context["document_token"],
        "operation_id": "notes",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": before["content_fingerprint"],
        "changes": [{"note_id": note["note_id"], "expected": note, "updates": {"velocity": 72.0}}],
    }
    result = target.registry.dispatch("sunny_managed_update_notes", [update])
    assert result["outcome"] == "acknowledged", result
    assert result["result"]["device_identity"] == before["device_identity"]
    assert result["result"]["note_identity"]["notes"][0]["note_id"] == note["note_id"]
    assert result["result"]["note_identity"]["notes"][0]["velocity"] == 72.0
    target.record["track"]._devices[0].voice_mode_index = 0
    with pytest.raises(RuntimeError, match="device state drift"):
        target.registry.devices.verify_retained_chain(target.record)


def test_closed_request_rejects_unregistered_class_and_caller_descriptors(target: Any) -> None:
    """Authority never comes from arbitrary names, numeric internal values or descriptors."""
    payload = request(target, "source")
    assert valid_managed_device_request("sunny_managed_insert_device", [payload])
    for changed in ({"unknown": True}, {"internal_value": 0.5}):
        invalid = copy.deepcopy(payload)
        invalid.update(changed)
        assert not valid_managed_device_request("sunny_managed_insert_device", [invalid])
    invalid = copy.deepcopy(payload)
    invalid["device"]["class_name"] = "Operator"
    assert not valid_managed_device_request("sunny_managed_insert_device", [invalid])


def test_reply_capacity_declines_before_insertion_for_actual_large_note_population(
    target: Any,
) -> None:
    """A count-valid native Clip can make the full before/after ACK exceed the frame."""
    clip = target.record["clip"]
    original = copy.deepcopy(next(iter(clip._notes.values())))
    for identifier in range(2, 26001):
        note = copy.deepcopy(original)
        note.note_id = identifier
        clip._notes[identifier] = note
    target.registry._seal(target.record)
    payload = request(target, "source")
    with pytest.raises(RuntimeError, match="ReplyCapacityUnavailable"):
        apply(target, payload)
    assert target.inserted == []
    assert target.registry._operations["source"]["native_mutation_started"] is False


def test_preserve_only_adoption_never_grants_device_keys_and_guards_replacement(
    target: Any,
) -> None:
    """An explicitly adopted Clip may preserve unknown devices without authorizing them."""
    current = device("Drift")
    current._canonical_parent = target.record["track"]
    target.record["track"]._devices.append(current)
    target.record.pop("_managed_devices")
    target.record["adopted_device_cohort"] = (current,)
    target.registry.devices.verify_retained_chain(target.record)
    assert target.registry.devices.capture(target.record) == {}
    replacement = device("Drift")
    replacement._canonical_parent = target.record["track"]
    target.record["track"]._devices[0] = replacement
    with pytest.raises(RuntimeError, match="cohort"):
        target.registry.devices.verify_retained_chain(target.record)


def test_clamped_native_readback_does_not_seal_failed_physical_intent(target: Any) -> None:
    """An actual native value readback matters even when a setter returned normally."""
    apply(target, request(target, "source"))
    native = target.record["track"]._devices[0]
    native.parameters[1].clamp = True
    payload = request(
        target,
        "source",
        intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
    )
    payload["operation_id"] = "clamped"
    with pytest.raises(RuntimeError, match="formatted readback"):
        apply(target, payload, False)
    assert native.parameters[1].value == 0.25
    assert native.parameters[1].oracle(0.25) == "450.00 Hz"
    with pytest.raises(RuntimeError, match="state drift"):
        target.registry.devices.verify_retained_chain(target.record)


def preview_request(target: Any, cutoff: float = 1200.0) -> dict[str, Any]:
    """Explicitly select current known objects with fresh logical keys."""
    return {
        "document_token": target.context["document_token"],
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": target.registry._capture(target.record)[
            "content_fingerprint"
        ],
        "devices": [
            {
                "device_key": "current_source",
                "chain_index": 0,
                "device": {
                    "browser_name": "Drift",
                    "class_name": "Drift",
                    "type": 1,
                    "role": "source",
                    "insertion_policy": "AppendOwnedChain",
                },
                "physical_intents": [
                    {"capability_id": "drift.lp.frequency", "target": cutoff, "tolerance": 0.0}
                ],
            }
        ],
    }


def adopt_request(target: Any, preview: dict[str, Any]) -> dict[str, Any]:
    """Fenced adoption approves the exact current observation, never old receipts."""
    return {
        "document_token": target.context["document_token"],
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "adopt_devices",
        "preview_token": preview["preview_token"],
        "approved_preview": preview["preview"],
    }


def test_explicit_current_device_adoption_from_preserve_only_chain_then_revision(
    target: Any,
) -> None:
    """Saved current handles receive new authority; no old key or insertion is reused."""
    apply(
        target,
        request(
            target,
            "source",
            intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
        ),
    )
    native = target.record["track"]._devices[0]
    target.record.pop("_managed_devices")
    target.record["adopted_device_cohort"] = (native,)
    calls_before = len(native.parameters[1].writes)
    preview = target.registry.devices.preview(target.record, preview_request(target))
    assert "_managed_devices" not in target.record
    assert preview["preview"]["native_mutation_started"] is False
    assert preview["preview"]["resolutions"][0]["current_readback"]["display"] == "1200.00 Hz"
    approved = adopt_request(target, preview)
    operation = {
        "name": "sunny_managed_adopt_devices",
        "request": approved,
        "outcome": "pending",
        "native_mutation_started": False,
    }
    result = target.registry.devices.adopt(target.record, approved, operation)
    assert operation["native_mutation_started"] is False
    assert len(native.parameters[1].writes) == calls_before
    assert target.inserted == ["Drift"]
    assert result["device_adoption"]["authority_origin"] == "explicit_current_device_adoption"
    assert target.record["_managed_devices"]["entries"][0]["device_key"] == "current_source"
    update = request(
        target,
        "current_source",
        intents=[{"capability_id": "drift.lp.frequency", "target": 2450.0, "tolerance": 0.0}],
    )
    update["operation_id"] = "revise_current"
    revised = apply(target, update, False)
    assert revised["device_update"]["readbacks"][0]["display"] == "2450.00 Hz"
    assert revised["device_update"]["readbacks"][0]["internal_value"] == 0.75
    assert target.inserted == ["Drift"]


def test_device_preview_refuses_authored_mismatch_and_adoption_rechecks_actual_identity(
    target: Any,
) -> None:
    """A solver candidate at.5 does not mean current0 already sounds at1200Hz."""
    apply(target, request(target, "source"))
    native = target.record["track"]._devices[0]
    with pytest.raises(RuntimeError, match="AuthoredNativeMismatch"):
        target.registry.devices.preview(target.record, preview_request(target))
    assert not native.parameters[1].writes
    preview = target.registry.devices.preview(target.record, preview_request(target, 200.0))
    approved = adopt_request(target, preview)
    replacement = device("Drift")
    replacement._canonical_parent = target.record["track"]
    target.record["track"]._devices[0] = replacement
    with pytest.raises(RuntimeError, match="cohort"):
        target.registry.devices.adopt(
            target.record,
            approved,
            {"request": approved, "outcome": "pending", "native_mutation_started": False},
        )
    assert not replacement.parameters[1].writes
    assert target.record["_managed_devices"]["entries"][0]["device_key"] == "source"


def test_explicit_device_refresh_requires_matching_revised_intent_and_repeated_formatter(
    target: Any,
) -> None:
    """A changed native value may be explicitly adopted only through current-value agreement."""
    apply(target, request(target, "source"))
    native = target.record["track"]._devices[0]
    native.parameters[1]._value = 0.5
    with pytest.raises(RuntimeError, match="state drift"):
        target.registry.devices.verify_retained_chain(target.record)
    preview = target.registry.devices.preview(target.record, preview_request(target, 1200.0))
    approved = adopt_request(target, preview)
    native.parameters[1].oracle = lambda _: "999.00 Hz"
    with pytest.raises(RuntimeError, match="formatted device"):
        target.registry.devices.adopt(
            target.record,
            approved,
            {"request": approved, "outcome": "pending", "native_mutation_started": False},
        )
    assert target.record["_managed_devices"]["entries"][0]["device_key"] == "source"
    native.parameters[1].oracle = lambda v: f"{200 + 4000 * v * v:.2f} Hz"
    approved["operation_id"] = "adopt_after_formatter_restored"
    result = target.registry.devices.adopt(
        target.record,
        approved,
        {"request": approved, "outcome": "pending", "native_mutation_started": False},
    )
    assert result["device_adoption"]["current_values_match_approved_intent"] is True
    target.registry.devices.verify_retained_chain(target.record)


def test_current_device_preview_declines_unknown_rack_without_setters(target: Any) -> None:
    """Known metadata cannot grant opaque racks any finite device parameter authority."""
    apply(target, request(target, "source"))
    native = target.record["track"]._devices[0]
    native._can_have_chains = True
    with pytest.raises(RuntimeError, match="flat state"):
        target.registry.devices.preview(target.record, preview_request(target, 200.0))
    assert not any(parameter.writes for parameter in native.parameters)


def test_broad_display_tolerance_never_admits_negative_native_width(target: Any) -> None:
    """Tolerance affects agreement but never changes percent's nonnegative domain."""
    apply(target, request(target, "source"))
    apply(target, request(target, "effect", "Utility"))
    width = target.record["track"]._devices[1].parameters[3]

    def change_formatter_after_write(_: float) -> None:
        if width.writes:
            width.oracle = lambda _: "-1.00 %"

    width.callback = change_formatter_after_write
    payload = request(
        target,
        "effect",
        "Utility",
        [{"capability_id": "utility.width", "target": 100.0, "tolerance": 250.0}],
    )
    payload["operation_id"] = "invalid_width_after_setter"
    with pytest.raises(RuntimeError, match="physical unit domain"):
        apply(target, payload, False)
    assert len(width.writes) == 1
    assert target.registry._operations[payload["operation_id"]]["native_mutation_started"] is True


def test_empty_preserve_only_clip_note_ack_never_grants_device_insertion(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The exact two-domain adoption record stays preserve-only after an actual note ACK."""
    target.record.pop("_managed_devices")
    target.record.update(
        {
            "authority_origin": "explicit_adoption",
            "allowed_domains": ("existing_note_updates", "absent_mixer_step_lanes"),
            "adopted_device_cohort": (),
        }
    )
    before = target.registry._capture(target.record)
    note = before["note_identity"]["notes"][0]
    clip = target.record["clip"]
    monkeypatch.setattr(
        clip,
        "get_notes_by_id",
        lambda ids: MidiNoteVector(copy.deepcopy(clip._notes[i]) for i in ids),
    )

    def change_velocity(notes: Any) -> None:
        for selected in notes:
            clip._notes[selected.note_id].velocity = selected.velocity

    monkeypatch.setattr(clip, "apply_note_modifications", change_velocity, raising=False)
    updated = target.registry.dispatch(
        "sunny_managed_update_notes",
        [
            {
                "document_token": target.context["document_token"],
                "operation_id": "adopted_note_revision",
                "project_key": "project_a",
                "binding_key": "part_a",
                "expected_content_fingerprint": before["content_fingerprint"],
                "changes": [
                    {"note_id": note["note_id"], "expected": note, "updates": {"velocity": 72.0}}
                ],
            }
        ],
    )
    assert updated["outcome"] == "acknowledged", updated
    assert updated["result"]["note_identity"]["notes"][0]["velocity"] == 72.0
    assert target.record["_managed_devices"]["entries"] == []
    with pytest.raises(RuntimeError, match="DeviceAuthorityUnavailable"):
        apply(target, request(target, "source"))
    assert target.inserted == []
    assert target.registry._operations["source"]["native_mutation_started"] is False


def test_explicit_empty_device_adoption_grants_source_insertion_without_native_calls(
    target: Any,
) -> None:
    """Empty capture metadata grants nothing; explicit current-track approval grants append."""
    target.record.pop("_managed_devices")
    target.record.update(
        {
            "authority_origin": "explicit_adoption",
            "allowed_domains": ("existing_note_updates", "absent_mixer_step_lanes"),
            "adopted_device_cohort": (),
        }
    )
    # An ordinary ACK may retain empty observation metadata but confers no grant.
    target.registry._seal(target.record)
    with pytest.raises(RuntimeError, match="DeviceAuthorityUnavailable"):
        apply(target, request(target, "declined_before_device_grant"))
    declaration = preview_request(target)
    declaration["devices"] = []
    preview = target.registry.devices.preview(target.record, declaration)
    assert preview["preview"]["resolutions"] == []
    assert target.inserted == []
    approved = adopt_request(target, preview)
    pending = {
        "name": "sunny_managed_adopt_devices",
        "request": approved,
        "outcome": "pending",
        "native_mutation_started": False,
    }
    adopted = target.registry.devices.adopt(target.record, approved, pending)
    assert pending["native_mutation_started"] is False
    assert adopted["device_identity"]["cohort"] == []
    assert target.record["_managed_devices"]["authority_track"] is target.record["track"]
    assert target.inserted == []
    inserted = apply(
        target,
        request(
            target,
            "source",
            intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
        ),
    )
    assert inserted["device_update"]["readbacks"][0]["display"] == "1200.00 Hz"
    assert target.inserted == ["Drift"]


def test_explicit_empty_device_adoption_rechecks_current_empty_chain(target: Any) -> None:
    """A new unowned native Device after preview cannot receive the empty-chain grant."""
    declaration = preview_request(target)
    declaration["devices"] = []
    preview = target.registry.devices.preview(target.record, declaration)
    approved = adopt_request(target, preview)
    native = device("Drift")
    native._canonical_parent = target.record["track"]
    target.record["track"]._devices.append(native)
    pending = {"request": approved, "outcome": "pending", "native_mutation_started": False}
    with pytest.raises(RuntimeError, match="cohort"):
        target.registry.devices.adopt(target.record, approved, pending)
    assert pending["native_mutation_started"] is False
    assert target.inserted == []
    assert not any(parameter.writes for parameter in native.parameters)


def test_known_current_device_adoption_grants_effect_append_on_actual_track(target: Any) -> None:
    """Current known-chain approval grants revisions and its declared append policy."""
    apply(
        target,
        request(
            target,
            "source",
            intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
        ),
    )
    target.record.pop("_managed_devices")
    target.record["adopted_device_cohort"] = tuple(target.record["track"]._devices)
    preview = target.registry.devices.preview(target.record, preview_request(target))
    approved = adopt_request(target, preview)
    target.registry.devices.adopt(
        target.record,
        approved,
        {"request": approved, "outcome": "pending", "native_mutation_started": False},
    )
    result = apply(
        target,
        request(
            target,
            "effect",
            "Utility",
            [{"capability_id": "utility.gain", "target": 0.0, "tolerance": 0.0}],
        ),
    )
    assert target.inserted == ["Drift", "Utility"]
    assert result["device_update"]["readbacks"][0]["display"] == "0.00 dB"


def test_quantized_native_defaults_are_never_read(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Quantized defaults are unavailable; native captures retain explicit null."""
    reads: list[str] = []

    def default(parameter: Any) -> float:
        if parameter.is_quantized:
            reads.append(parameter.original_name)
            raise RuntimeError("literal quantized default unavailable")
        return parameter._default

    monkeypatch.setattr(Parameter, "default_value", property(default))
    source = apply(
        target,
        request(
            target,
            "source",
            intents=[{"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}],
        ),
    )
    effect = apply(
        target,
        request(
            target,
            "effect",
            "Utility",
            [{"capability_id": "utility.gain", "target": 0.0, "tolerance": 0.0}],
        ),
    )
    assert reads == []
    assert (
        source["device_identity"]["cohort"][0]["parameters"][0]["descriptor"]["default_value"]
        is None
    )
    assert (
        source["device_identity"]["cohort"][0]["parameters"][1]["descriptor"]["default_value"]
        == 0.0
    )
    assert effect["device_update"]["readbacks"][0]["display"] == "0.00 dB"
    for member in effect["device_identity"]["cohort"]:
        for parameter in member["parameters"]:
            if parameter["descriptor"]["is_quantized"]:
                assert parameter["descriptor"]["default_value"] is None


def test_continuous_native_default_remains_required(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """An unavailable continuous default is an observation failure before any setter."""

    def default(parameter: Any) -> float:
        if not parameter.is_quantized:
            raise RuntimeError("literal continuous default unavailable")
        raise AssertionError("quantized default must never be read")

    monkeypatch.setattr(Parameter, "default_value", property(default))
    with pytest.raises(RuntimeError, match="continuous default unavailable"):
        apply(
            target,
            request(
                target,
                "source",
                intents=[
                    {"capability_id": "drift.lp.frequency", "target": 1200.0, "tolerance": 0.0}
                ],
            ),
        )
    assert target.inserted == ["Drift"]
    assert target.registry._operations["source"]["native_mutation_started"] is True
    assert not any(parameter.writes for parameter in target.record["track"]._devices[0].parameters)
