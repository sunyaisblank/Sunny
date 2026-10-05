"""Original approval survives observations, planning and one-shot gateway failures."""

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path

import pytest

KIT = Path(__file__).resolve().parents[2] / "tools/live_qualification"
BRIDGE = "b" * 32
DOCUMENT = "d" * 32
CONTEXT = {"schema_version": 1, "bridge_instance": BRIDGE, "document_token": DOCUMENT}


@pytest.fixture
def helpers(monkeypatch):
    """Import actual maintained helpers without executing their command lines."""
    monkeypatch.syspath_prepend(str(KIT))
    modules = []
    for name in ("common", "host_runner"):
        spec = importlib.util.spec_from_file_location(
            "qualification_origin_" + name, KIT / (name + ".py")
        )
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        modules.append(module)
    return modules


def config():
    """Literal externally retained approval; no native response supplies these tokens."""
    return {
        "scratch_bridge_instance": BRIDGE,
        "scratch_document_token": DOCUMENT,
        "expected_bridge_protocol_version": 47,
        "expected_bridge_source_sha256": "f" * 64,
    }


@pytest.mark.parametrize("value", [None, True, 7, "D" * 32, "d" * 31, "z" * 32])
def test_invalid_original_approval_cannot_be_inferred_from_a_native_observation(helpers, value):
    """Fresh matching-looking observations never fill absent/invalid operator approval."""
    common, _ = helpers
    selected = config()
    selected["scratch_document_token"] = value
    with pytest.raises(ValueError, match="approved original"):
        common.require_primary_context(selected, {"primary_context": CONTEXT})


@pytest.mark.parametrize("field", ["bridge_instance", "document_token", "schema_version", "extra"])
def test_probe_must_join_the_same_original_native_song(helpers, field):
    """Reject either epoch change, typed substitution or an open context envelope."""
    common, _ = helpers
    observed = {"primary_context": dict(CONTEXT), "primary_context_error": None}
    assert common.require_primary_context(config(), observed) == CONTEXT
    observed["primary_context"][field] = True if field == "schema_version" else "c" * 32
    with pytest.raises(RuntimeError, match="identity differs"):
        common.require_primary_context(config(), observed)


class Client:
    """Boundary witness records requested tools; it cannot grant real native authority."""

    def __init__(self, response):
        """Keep one literal boundary result and record every call."""
        self.response = response
        self.calls = []

    def call(self, tool, **arguments):
        """Return the retained witness without retries or native effects."""
        self.calls.append((tool, arguments))
        return self.response


def test_read_only_doctor_arms_only_the_original_approved_epoch(helpers):
    """Wrong B/D, source or typed schema prevents proceeding after the actual doctor call."""
    common, _ = helpers
    identity = {"protocol_version": 47, "source_sha256": "f" * 64}
    report = {
        "success": True,
        "read_only_ready": True,
        "session": dict(CONTEXT),
        "observed_bridge": dict(identity),
        "expected_bridge": dict(identity),
    }
    client = Client(report)
    assert common.prime_native_session(client, config()) == report
    assert client.calls == [("doctor_ableton", {})]
    for change in ("document_token", "bridge_instance", "schema_version", "source"):
        bad = copy.deepcopy(report)
        if change == "source":
            bad["observed_bridge"]["source_sha256"] = "0" * 64
        else:
            bad["session"][change] = 1.0 if change == "schema_version" else "c" * 32
        client = Client(bad)
        with pytest.raises(RuntimeError, match="originally approved session"):
            common.prime_native_session(client, config())
        assert client.calls == [("doctor_ableton", {})]


@pytest.mark.parametrize("nested", ["common", "baseline", "mixer", "send", "song", "retirement"])
def test_each_coordinator_context_is_checked_before_any_apply(helpers, nested):
    """New Part null baseline is legitimate; any retained cohort from another epoch is refused."""
    _, runner = helpers
    plan = {
        "native_context": dict(CONTEXT),
        "parts": [
            {"native_baseline": None},
            {
                "native_baseline": {"context": dict(CONTEXT)},
                "current_static_mixer_inspection": {"inspection": {"context": dict(CONTEXT)}},
            },
        ],
        "retirements": [{"native_baseline": {"context": dict(CONTEXT)}}],
        "routing": [{"current_send_inspection": {"inspection": {"context": dict(CONTEXT)}}}],
        "current_song_settings_inspection": {"inspection": {"context": dict(CONTEXT)}},
    }
    runner.require_plan_origin(plan, config())
    targets = {
        "common": plan["native_context"],
        "baseline": plan["parts"][1]["native_baseline"]["context"],
        "mixer": plan["parts"][1]["current_static_mixer_inspection"]["inspection"]["context"],
        "send": plan["routing"][0]["current_send_inspection"]["inspection"]["context"],
        "song": plan["current_song_settings_inspection"]["inspection"]["context"],
        "retirement": plan["retirements"][0]["native_baseline"]["context"],
    }
    targets[nested]["document_token"] = "c" * 32
    client = Client(
        {"success": True, "mutation_dispatched": False, "authority_granted": False, "plan": plan}
    )
    with pytest.raises(RuntimeError, match="original scratch approval"):
        runner.apply(client, {}, config())
    assert client.calls == [("project_realization_plan", {})]


def test_failed_gateway_preserves_stage_receipt_and_never_replays(helpers):
    """An unsent query after an earlier lost execute ACK remains uncertain original evidence."""
    common, _ = helpers
    receipt = {
        "stage": "query",
        "delivery": "not_sent",
        "outcome": "unknown",
        "operation_id": "e" * 32,
        "native_mutation_started": True,
    }
    failed = {"success": False, "receipt": receipt, "error": "ACK unavailable"}
    client = Client(failed)
    command = {"type": "call", "path": "song", "name": "sunny_set_cue", "args": [8.0, "literal"]}
    with pytest.raises(RuntimeError) as error:
        common.legacy_call(client, config(), command)
    assert error.value.args[0]["result"]["receipt"] == receipt
    assert client.calls == [
        (
            "legacy_ableton_request",
            {"command": command, "bridge_instance": BRIDGE, "document_token": DOCUMENT},
        )
    ]
