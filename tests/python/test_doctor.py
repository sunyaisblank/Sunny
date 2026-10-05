"""Doctor contracts: literal malformed peers, finite cursors, and actual stdio/TCP."""

from __future__ import annotations

import copy
import importlib.util
import json
import stat
import subprocess
import sys
import time
from pathlib import Path

import pytest
from live_model import LiveSet
from Sunny import surface as surface_module
from Sunny.handler import LomHandler
from Sunny.surface import SunnyControlSurface
from test_live_end_to_end import _LiveMainThread, _sunny_mcp_binary

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sunny_doctor", ROOT / "tools" / "doctor.py")
assert SPEC and SPEC.loader
doctor = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(doctor)

EPOCH_A, EPOCH_B = "a" * 32, "b" * 32
PAGE = json.loads(
    '{"success":true,"entries":[{"sequence":1,"time":1700000000.0,"level":"INFO",'
    '"source":"sunny.test","message":"Private Set title; password=SECRET"}],'
    '"next_sequence":1,"latest_sequence":1,"oldest_sequence":1,"stream_id":"' + EPOCH_A + '",'
    '"reset":false,"truncated":false,"observed_at":1700000001.0,"has_more":false}'
)


@pytest.mark.parametrize(
    "literal",
    [
        '{"bridge_protocol_version":46,"name":"session_record","path":"song","type":"get"}',
        '{"bridge_protocol_version":46,"name":"record_mode","path":"song","type":"get"}',
    ],
)
def test_minimal_doctor_flags_are_literal_read_only_protocol(literal, monkeypatch):
    """Both peers admit these reads while reflection, arguments and mutations decline."""
    live = LiveSet((12, 4, 5)).install(monkeypatch)
    handler = LomHandler(live.surface)
    request = json.loads(literal)
    assert handler.handle(request) == {"success": True, "value": False}

    def forbidden():
        pytest.fail("An invalid flag request reached a Live object")

    monkeypatch.setattr(handler, "_get_song", forbidden)
    invalid = [
        {**request, "type": "set", "args": [False]},
        {**request, "type": "call"},
        {**request, "path": "song/tracks/0"},
        {**request, "args": [False]},
    ]
    for wire in invalid:
        response = handler.handle(wire)
        assert response["success"] is False
        assert "outside Sunny bridge protocol" in response["error"]


class Clock:
    """An advancing deadline witness independent of subprocess and protocol code."""

    def __init__(self, monkeypatch):
        """Replace only the clocks used during deterministic polling tests."""
        self.now = 0.0
        self.sleeps = []
        monkeypatch.setattr(doctor.time, "monotonic", lambda: self.now)
        monkeypatch.setattr(doctor.time, "time", lambda: 1700000000 + self.now)
        monkeypatch.setattr(doctor.time, "sleep", self.sleep)

    def sleep(self, duration):
        """Record every wait and advance the finite budget."""
        self.sleeps.append(duration)
        self.now += duration


class Pages:
    """External page source recording exactly the requested cursor and deadline."""

    def __init__(self, clock, pages, elapsed=0.01):
        """Supply literal replies without sharing the producer implementation."""
        self.clock, self.pages, self.elapsed = clock, pages, elapsed
        self.requests = []

    def call(self, name, arguments, deadline):
        """Return a boundary reply or a deliberate protocol failure."""
        assert name == "get_ableton_remote_log"
        assert self.clock.now < deadline
        self.requests.append((arguments, deadline))
        self.clock.now += self.elapsed
        page = self.pages[min(len(self.requests) - 1, len(self.pages) - 1)]
        if isinstance(page, Exception):
            raise page
        return copy.deepcopy(page)


@pytest.mark.parametrize(
    "change",
    [
        {"observed_at": float("nan")},
        {"observed_at": 10**1000},
        {"observed_at": True},
        {"reset": 0},
        {"reset": True},
        {"truncated": True},
        {"has_more": True},
        {"next_sequence": 2},
        {"stream_id": "uppercase"},
        {"private_set": "unexpected content"},
        {"entries": [{**PAGE["entries"][0], "sequence": 2}]},
        {"entries": [{**PAGE["entries"][0], "time": True}]},
        {"entries": [{**PAGE["entries"][0], "time": 10**1000}]},
        {"entries": [{**PAGE["entries"][0], "message": "x" * 2001}]},
    ],
)
def test_log_page_rejects_literal_contradictions(change):
    """Malformed types and impossible cursor claims cannot become fresh evidence."""
    with pytest.raises(ValueError):
        doctor.validate_page({**PAGE, **change}, 0, None)


def test_legacy_page_cannot_supply_epoch_or_freshness():
    """The native seq-only compatibility path must not invent current metadata."""
    legacy = {
        "success": True,
        "entries": PAGE["entries"],
        "next_sequence": 1,
        "truncated": False,
        "stream_id": None,
        "reset": None,
        "observed_at": None,
        "oldest_sequence": None,
        "latest_sequence": None,
        "has_more": None,
        "cursor_metadata_available": False,
    }
    with pytest.raises(ValueError):
        doctor.validate_page(legacy, 0, None)


def test_poll_tracks_restart_gap_cursor_and_omits_messages(monkeypatch):
    """An epoch restart and ring loss remain explicit across later good reads."""
    clock = Clock(monkeypatch)
    reset = {**PAGE, "stream_id": EPOCH_B, "reset": True}
    gap = {
        **PAGE,
        "stream_id": EPOCH_B,
        "oldest_sequence": 3,
        "latest_sequence": 3,
        "next_sequence": 3,
        "truncated": True,
        "entries": [{**PAGE["entries"][0], "sequence": 3}],
    }
    peer = Pages(clock, [PAGE, reset, gap])
    result = doctor.poll_logs(peer, 0.15, 0.05, False)
    assert [request[0] for request in peer.requests] == [
        {"after_sequence": 0},
        {"after_sequence": 1, "stream_id": EPOCH_A},
        {"after_sequence": 1, "stream_id": EPOCH_B},
    ]
    assert result["cursor"] == {"after_sequence": 3, "stream_id": EPOCH_B}
    assert (result["resets"], result["gaps"], result["incomplete"], result["stale"]) == (
        1,
        1,
        True,
        False,
    )
    assert result["records_seen"] == 3
    assert "Private Set" not in json.dumps(result)
    assert "message" not in result["records"][0]


def test_poll_stops_at_deadline_with_undelivered_page(monkeypatch):
    """The budget prevents an extra call and undelivered entries remain incomplete."""
    clock = Clock(monkeypatch)
    pending = {**PAGE, "latest_sequence": 2, "has_more": True}
    peer = Pages(clock, [pending], elapsed=1.0)
    result = doctor.poll_logs(peer, 1.0, 0.05, False)
    assert len(peer.requests) == 1
    assert result["incomplete"] is True and result["stale"] is False
    assert clock.sleeps == []
    expired = doctor.poll_logs(peer, 1.0, 0.05, False)
    assert expired["requests"] == 0
    assert expired["stale"] is True and expired["incomplete"] is True


def test_poll_uses_finite_backoff_for_business_failures(monkeypatch):
    """Invalid pages retry read-only with unchanged cursors and bounded waits."""
    clock = Clock(monkeypatch)
    peer = Pages(clock, [{"success": False, "error": "Private Set token=SECRET"}])
    result = doctor.poll_logs(peer, 0.35, 0.05, True)
    assert result["requests"] == 3 and clock.now == pytest.approx(0.35)
    assert clock.sleeps[:2] == pytest.approx([0.1, 0.2])
    assert all(arguments == {"after_sequence": 0} for arguments, _ in peer.requests)
    assert result["stale"] and result["incomplete"]
    assert "Private Set" not in json.dumps(result) and "SECRET" not in json.dumps(result)


def test_poll_never_overlaps_after_protocol_failure(monkeypatch):
    """An uncorrelated/late protocol response cannot be reused by another request."""
    clock = Clock(monkeypatch)
    failure = doctor.DiagnosticFailure("stdio", "response_timeout", "Fixed message.", "Retry.")
    peer = Pages(clock, [failure, PAGE])
    result = doctor.poll_logs(peer, 1.0, 0.05, True)
    assert len(peer.requests) == 1 and clock.sleeps == []
    assert result["stale"] and result["incomplete"]


def test_poll_retention_is_bounded_and_reports_omitted_records(monkeypatch):
    """Two full independent windows retain at most 1000 records without hiding loss."""
    clock = Clock(monkeypatch)
    first = {
        **PAGE,
        "entries": [{**PAGE["entries"][0], "sequence": sequence} for sequence in range(1, 1001)],
        "next_sequence": 1000,
        "latest_sequence": 1000,
    }
    second = {
        **PAGE,
        "entries": [{**PAGE["entries"][0], "sequence": sequence} for sequence in range(1001, 2001)],
        "oldest_sequence": 1001,
        "next_sequence": 2000,
        "latest_sequence": 2000,
    }
    peer = Pages(clock, [first, second], elapsed=0.1)
    result = doctor.poll_logs(peer, 0.25, 0.05, False)
    assert result["requests"] == 2 and result["records_seen"] == 2000
    assert len(result["records"]) == 1000 and result["records"][0]["sequence"] == 1001
    assert result["omitted_records"] == 1000 and result["incomplete"]
    assert result["cursor"]["after_sequence"] == 2000 and not result["stale"]


def test_poll_request_limit_is_a_finite_independent_bound(monkeypatch):
    """A busy peer cannot defeat the explicit request count cap."""
    clock = Clock(monkeypatch)
    empty = {**PAGE, "entries": [], "next_sequence": 0, "latest_sequence": 0}
    peer = Pages(clock, [empty])
    result = doctor.poll_logs(peer, 100, 0.05, False)
    assert result["requests"] == 1000
    assert clock.now < 100 and result["incomplete"] and not result["stale"]


def test_opted_in_log_messages_redact_known_credentials(monkeypatch):
    """Message collection is explicit and known secrets are removed before retention."""
    clock = Clock(monkeypatch)
    peer = Pages(clock, [PAGE])
    result = doctor.poll_logs(peer, 0.03, 0.05, True)
    assert result["records"][0]["message"] == "Private Set title; [REDACTED]"


@pytest.mark.parametrize(
    "text",
    [
        "password=SECRET",
        "api_key: SECRET",
        "Bearer SECRET",
        "Basic SECRET",
        "https://user:SECRET@localhost/example",
        '"password": "SECRET WITH SPACES"',
        "sk-SECRET123456",
        "ghp_SECRET123456",
    ],
)
def test_export_redacts_known_credential_patterns(text):
    """Literal credential examples are independently specified export obligations."""
    assert "SECRET" not in doctor.redact(text)


def test_export_is_exclusive_private_and_capped_including_newline(tmp_path):
    """The literal byte cap includes the written newline and prevents replacement."""
    path = tmp_path / "diagnosis.json"
    overhead = len(json.dumps({"text": ""}, indent=2).encode()) + 1
    doctor.export_report(path, {"text": "x" * (doctor.MAX_EXPORT_BYTES - overhead)})
    assert path.stat().st_size == doctor.MAX_EXPORT_BYTES
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    previous = path.read_bytes()
    with pytest.raises(FileExistsError):
        doctor.export_report(path, {"text": "replacement"})
    assert path.read_bytes() == previous
    too_large = tmp_path / "too_large.json"
    with pytest.raises(ValueError):
        doctor.export_report(too_large, {"text": "x" * (doctor.MAX_EXPORT_BYTES - overhead + 1)})
    assert not too_large.exists()


def test_export_omits_oversized_records_without_losing_cursor_facts(tmp_path):
    """A deliberate limited export declares missing detail and preserves loss facts."""
    path = tmp_path / "diagnosis.json"
    report = {
        "success": True,
        "logs": {
            "cursor": {"after_sequence": 10, "stream_id": EPOCH_A},
            "resets": 1,
            "gaps": 2,
            "incomplete": False,
            "records": [{"message": "a" * 2000 + " password=SECRET"}] * 1000,
        },
    }
    doctor.export_report(path, report)
    exported = json.loads(path.read_bytes())
    assert path.stat().st_size <= doctor.MAX_EXPORT_BYTES
    assert exported["logs"]["records"] == []
    assert exported["logs"]["cursor"] == report["logs"]["cursor"]
    assert exported["logs"]["export_records_omitted"] == 1000
    assert exported["logs"]["incomplete"] and exported["success"] is False
    assert "SECRET" not in path.read_text()
    assert len(report["logs"]["records"]) == 1000  # The caller's evidence stays intact.


def test_actual_process_launch_failure_has_its_own_layer(tmp_path):
    """A missing executable is distinct from bridge/Live failure."""
    report = doctor.diagnose([str(tmp_path / "missing-command")], 1)
    assert report["success"] is False
    assert report["checks"][-1]["layer"] == "client_launch"
    assert report["checks"][-1]["code"] == "launch_failed"


def test_cli_explicit_export_is_reviewable_and_default_diagnosis_writes_nothing(tmp_path):
    """The actual CLI deliberately exports only its scoped result and returns failure."""
    command = [sys.executable, str(ROOT / "tools" / "doctor.py"), "--timeout", "1"]
    missing = str(tmp_path / "uninstalled-command")
    result = subprocess.run(
        [*command, "--", missing], cwd=tmp_path, capture_output=True, text=True, timeout=3
    )
    assert result.returncode == 1 and json.loads(result.stdout)["success"] is False
    assert list(tmp_path.iterdir()) == []
    path = tmp_path / "diagnosis.json"
    exported = subprocess.run(
        [*command, "--export", str(path), "--", missing],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=3,
    )
    assert exported.returncode == 1
    assert json.loads(path.read_text()) == json.loads(exported.stdout)
    assert path.stat().st_size <= doctor.MAX_EXPORT_BYTES
    assert stat.S_IMODE(path.stat().st_mode) == 0o600


@pytest.mark.parametrize(
    "program,code",
    [
        ('print("non-protocol banner", flush=True)', "protocol_invalid"),
        ('print(\'{"jsonrpc":"2.0","id":true,"result":{}}\', flush=True)', "protocol_invalid"),
        ("pass", "server_exited"),
        ("import time; time.sleep(10)", "response_timeout"),
    ],
)
def test_actual_stdio_rejects_literal_malformed_or_silent_process(program, code):
    """Actual pipe framing, correlation, EOF and deadline failures stay separate."""
    started = time.monotonic()
    report = doctor.diagnose([sys.executable, "-u", "-c", program], 0.15)
    assert report["checks"][-1]["layer"] == "stdio"
    assert report["checks"][-1]["code"] == code
    assert not report["success"] and not report["read_only_ready"]
    assert time.monotonic() - started < 2.0


def test_actual_stdio_discards_unscoped_doctor_payload():
    """A different MCP command cannot inject project content into a scoped export."""
    program = """import sys,json
for line in sys.stdin:
    request=json.loads(line)
    if "id" not in request: continue
    method=request["method"]
    if method=="initialize": result={"protocolVersion":"2025-11-25"}
    elif method=="ping": result={}
    elif method=="tools/list":
        result={"tools":[{"name":"doctor_ableton"},{"name":"get_ableton_remote_log"}]}
    else:
        result={"isError":True,"structuredContent":{"success":False,"private_set":"SECRET_PROJECT"}}
    print(json.dumps({"jsonrpc":"2.0","id":request["id"],"result":result}),flush=True)
"""
    report = doctor.diagnose([sys.executable, "-u", "-c", program], 2)
    assert report["tool_count"] == 2  # Measured, not a hardcoded release claim.
    assert report["checks"][-1]["code"] == "doctor_response_invalid"
    assert report["doctor"] is None and "SECRET_PROJECT" not in json.dumps(report)


@pytest.fixture
def native_boundary(monkeypatch):
    """Expose the real Sunny TCP bridge with only the external Live provider modelled."""
    live = LiveSet((12, 4, 5)).install(monkeypatch)
    monkeypatch.setattr(surface_module, "_server_configuration", lambda: ("127.0.0.1", 0))
    main_thread = _LiveMainThread()
    surface = SunnyControlSurface(object())
    # The test-only fallback framework lacks the native ControlSurface.song binding.
    # Supply that external host boundary; dispatcher, wire, handler and registry stay real.
    monkeypatch.setattr(surface, "song", live.surface.song, raising=False)
    surface.schedule_message = main_thread.schedule_message
    deadline = time.monotonic() + 5
    while not surface._server.is_running:
        assert time.monotonic() < deadline
        time.sleep(0.01)
    requests = []
    surface._doctor_wire_responses = []
    send_frame = surface._server._send_frame

    def record_frame(client, data):
        surface._doctor_wire_responses.append(data)
        return send_frame(client, data)

    monkeypatch.setattr(surface._server, "_send_frame", record_frame)
    handle = surface._handler.handle

    def record(request):
        requests.append(copy.deepcopy(request))
        return handle(request)

    monkeypatch.setattr(surface._handler, "handle", record)
    monkeypatch.setenv("SUNNY_ABLETON_HOST", "127.0.0.1")
    monkeypatch.setenv("SUNNY_TCP_PORT", str(surface._server.bound_port))
    try:
        yield live, surface, requests
    finally:
        surface.disconnect()
        main_thread.stop()


def test_real_stdio_tcp_doctor_polls_without_snapshot_or_mutation(native_boundary, monkeypatch):
    """An actual binary, sockets, handler and scheduled native reads prove the path."""
    live, surface, requests = native_boundary
    live.song._flags["is_playing"] = True
    before = copy.deepcopy(live.song._flags)

    def no_snapshot(*args, **kwargs):
        pytest.fail("Doctor must not collect a full Set snapshot")

    monkeypatch.setattr(surface._handler, "_target_snapshot", no_snapshot)
    report = doctor.diagnose([str(_sunny_mcp_binary())], 5, poll_seconds=0.15, interval=0.05)
    assert report["success"] and report["read_only_ready"], json.dumps(report, indent=2)
    assert report["doctor"]["request_id"] == report["request_id"]
    assert report["doctor"]["native_state"] == {
        "is_playing": True,
        "session_record": False,
        "record_mode": False,
    }
    assert report["doctor"]["capabilities"]["reported"]["max_for_live"] == "unknown"
    assert report["doctor"]["capabilities"]["live_version"] == {
        "major": 12,
        "minor": 4,
        "bugfix": 5,
    }
    assert report["logs"]["requests"] >= 2 and not report["logs"]["stale"]
    assert report["logs"]["cursor"]["stream_id"] == surface._remote_log._stream_id
    assert all("message" not in record for record in report["logs"]["records"])
    assert [request["name"] for request in requests[:7]] == [
        "sunny_get_target_profile",
        "sunny_get_target_profile",
        "sunny_managed_context",
        "is_playing",
        "session_record",
        "record_mode",
        "sunny_managed_context",
    ]  # The second profile is the existing TCP source-pairing handshake.
    assert [json.loads(frame) for frame in surface._doctor_wire_responses[3:6]] == [
        {"success": True, "value": True, "bridge_protocol_version": 46},
        {"success": True, "value": False, "bridge_protocol_version": 46},
        {"success": True, "value": False, "bridge_protocol_version": 46},
    ]
    assert all(request["name"] == "sunny_get_remote_log" for request in requests[7:])
    assert all(
        request["type"] in ("get", "call") and request["path"] == "song" for request in requests
    )
    assert live.song._flags == before and list(live.song.tracks) == []


def test_real_stdio_tcp_mismatch_declines_readiness(native_boundary, monkeypatch):
    """An actual different bridge identity is reported before session or state reads."""
    _, surface, requests = native_boundary
    profile = surface._handler._target_profile()
    profile["adapter"]["source_sha256"] = "0" * 64
    monkeypatch.setattr(surface._handler, "_target_profile", lambda: profile)
    report = doctor.diagnose([str(_sunny_mcp_binary())], 5)
    assert not report["success"] and not report["read_only_ready"]
    assert report["doctor"]["checks"][-1]["code"] == "bridge_mismatch"
    assert report["doctor"]["session"] is None
    assert [request["name"] for request in requests] == ["sunny_get_target_profile"]


def test_real_doctor_does_not_reuse_cached_readiness_after_disconnect(native_boundary):
    """The same launched server becomes unready when its native bridge disappears."""
    _, surface, _ = native_boundary
    client = doctor.StdioClient([str(_sunny_mcp_binary())], time.monotonic() + 5)
    try:
        client.request(
            "initialize",
            {
                "protocolVersion": "2025-11-25",
                "capabilities": {},
                "clientInfo": {"name": "literal-doctor-witness", "version": "1"},
            },
        )
        healthy = client.call("doctor_ableton", {"request_id": "healthy"})
        assert healthy["read_only_ready"], json.dumps(healthy, indent=2)
        surface.disconnect()
        fresh = client.call("doctor_ableton", {"request_id": "disconnected"})
        doctor.validate_doctor(fresh, "disconnected")
        assert not fresh["read_only_ready"] and fresh["session"] is None
        assert fresh["observed_bridge"] is None and fresh["observed_at"] > healthy["observed_at"]
        assert fresh["checks"][-1]["code"] == "bridge_unreachable"
    finally:
        client.close()


def test_real_doctor_unconfigured_host_keeps_launch_and_native_layers_separate(monkeypatch):
    """Protocol startup succeeds even when native host configuration is absent."""
    monkeypatch.delenv("SUNNY_ABLETON_HOST", raising=False)
    monkeypatch.delenv("SUNNY_TCP_PORT", raising=False)
    report = doctor.diagnose([str(_sunny_mcp_binary())], 5)
    assert all(check["status"] == "pass" for check in report["checks"])
    assert not report["success"] and not report["read_only_ready"]
    assert report["doctor"]["checks"][-1]["code"] == "bridge_unreachable"
    assert "SUNNY_ABLETON_HOST" in report["doctor"]["connection_failure"]
    assert report["doctor"]["session"] is None


def test_real_stdio_tcp_deadline_stops_diagnosis_when_live_does_not_schedule(native_boundary):
    """A native scheduler stall expires the local diagnosis without claiming readiness."""
    _, surface, requests = native_boundary
    surface.schedule_message = lambda delay, callback: None
    started = time.monotonic()
    report = doctor.diagnose([str(_sunny_mcp_binary())], 0.15, poll_seconds=0.1)
    assert not report["success"] and not report["read_only_ready"]
    assert report["doctor"] is None and report["logs"] is None
    assert report["checks"][-1]["code"] == "response_timeout"
    assert report["checks"][-1]["layer"] == "stdio"
    assert time.monotonic() - started < 2.0
    assert requests == []  # No queued native read began before the client deadline.


def test_expired_stdio_deadline_sends_no_request():
    """Deadline expiry prevents a queued extra poll even before writing protocol bytes."""
    client = doctor.StdioClient([sys.executable, "-u", "-c", "import time; time.sleep(10)"], 0)
    try:
        with pytest.raises(doctor.DiagnosticFailure) as failure:
            client.request("ping", {})
        assert failure.value.check["code"] == "response_timeout"
        assert client._next_id == 0
    finally:
        client.close()
