"""Strict production JSON and offline migration through native and compiled consumers."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "remote_script/Sunny/configuration.py"
SPEC = importlib.util.spec_from_file_location("sunny_configuration_standalone", SCRIPT)
configuration = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(configuration)


def document(workspace=None):
    """A literal two-role production document with distinct consumers."""
    return {
        "configuration_schema_version": 1,
        "client": {"transport": {"mode": "offline"}, "workspace": workspace},
        "native": {"bridge": {"bind_host": "127.0.0.2", "port": 49152}},
    }


def encoded(value):
    """Encode independently of the runtime parser."""
    return json.dumps(value).encode()


@pytest.mark.parametrize("host", ["example.test", "host.docker.internal", "127.0.0.1", "::1"])
@pytest.mark.parametrize("port", [1, 65535])
def test_both_roles_accept_typed_endpoints_and_spaced_paths(host, port):
    """Changing endpoints and absolute paths requires only external JSON data."""
    value = document({"path": "/data/musical work/workspace.json", "recovery": "backup"})
    value["client"]["transport"] = {"mode": "tcp", "host": host, "port": port}
    assert configuration.parse_document(encoded(value)) == value
    windows = document({"path": r"C:\User Library\workspace.json", "recovery": "none"})
    assert configuration.parse_document(encoded(windows)) == windows


@pytest.mark.parametrize(
    "raw",
    [
        b'{"configuration_schema_version":1,"configuration_schema_version":1,"client":{}}',
        b'{"configuration_schema_version":1,"native":{"bridge":{"bind_host":"127.0.0.1","port":1,"port":2}}}',
        b'{"configuration_schema_version":1,"client":{"transport":{"mode":"offline"},"workspace":null},"extra":0}',
        b'{"configuration_schema_version":2,"native":{"bridge":{"bind_host":"127.0.0.1","port":1}}}',
        b'{"configuration_schema_version":true}',
        b'{"configuration_schema_version":1.0,"client":{}}',
        b'{"configuration_schema_version":1}',
        b'{"configuration_schema_version":1,"native":{"bridge":{"bind_host":"127.0.0.1","port":NaN}}}',
        b"{" + b" " * 65536 + b"}",
        b'{"x":' + b"[" * 9 + b"0" + b"]" * 9 + b"}",
        b"\xff{}",
    ],
)
def test_bounded_closed_json_rejects_unsafe_inputs(raw):
    """Unknown, future, duplicate, excessive and ill-typed inputs fail explicitly."""
    with pytest.raises((ValueError, UnicodeError)):
        configuration.parse_document(raw)


@pytest.mark.parametrize("port", [True, 0, 65536, -1, 1.0, "9001", None])
def test_port_types_and_bounds_validate_unconsumed_role(port):
    """The native consumer cannot ignore a malformed client role."""
    value = document()
    value["client"]["transport"] = {"mode": "tcp", "host": "example.test", "port": port}
    with pytest.raises(ValueError):
        configuration.parse_document(encoded(value))


@pytest.mark.parametrize("host", ["0.0.0.0", "192.0.2.1", "localhost", "::1", "127.00.0.1"])
def test_native_json_never_exposes_non_ipv4_loopback(host):
    """The existing AF_INET server receives only explicit IPv4 loopback addresses."""
    value = document()
    value["native"]["bridge"]["bind_host"] = host
    with pytest.raises(ValueError, match="IPv4 loopback"):
        configuration.parse_document(encoded(value))


@pytest.mark.parametrize("key", configuration.LEGACY_KEYS)
def test_json_selector_never_merges_legacy_settings(tmp_path, key):
    """Even an unused legacy role variable makes selection ambiguous."""
    path = tmp_path / "selected.json"
    path.write_bytes(encoded(document()))
    with pytest.raises(ValueError, match="cannot be combined"):
        configuration.load_native_configuration({"SUNNY_CONFIG_PATH": str(path), key: "1"})


def test_native_migration_is_exclusive_and_does_not_touch_previous_files(tmp_path):
    """Migration and rollback use separate explicit files, without changing environment."""
    path = tmp_path / "new config with spaces.json"
    environment = {"SUNNY_BIND_HOST": "127.0.0.8", "SUNNY_TCP_PORT": "65432"}
    original = environment.copy()
    value = configuration.migrate_native_environment(path, environment)
    assert environment == original
    assert value == {
        "configuration_schema_version": 1,
        "native": {"bridge": {"bind_host": "127.0.0.8", "port": 65432}},
    }
    assert configuration.load_native_configuration({"SUNNY_CONFIG_PATH": str(path)}) == (
        "127.0.0.8",
        65432,
        "versioned_json",
    )
    previous = path.read_bytes()
    with pytest.raises(FileExistsError):
        configuration.migrate_native_environment(path, environment)
    assert path.read_bytes() == previous
    unsafe = tmp_path / "unsafe.json"
    with pytest.raises(ValueError, match="IPv4 loopback"):
        configuration.migrate_native_environment(unsafe, {"SUNNY_BIND_HOST": "0.0.0.0"})
    assert not unsafe.exists()
    # Legacy compatibility remains explicit; migration never silently rewrites its bind.
    assert configuration.load_native_configuration({"SUNNY_BIND_HOST": "0.0.0.0"}) == (
        "0.0.0.0",
        9001,
        "legacy_environment",
    )


def test_invalid_native_config_precedes_live_logger_registry_and_socket(tmp_path, monkeypatch):
    """Constructor failure occurs before any Live or durable/native setup action."""
    from Sunny import surface

    path = tmp_path / "invalid.json"
    value = document()
    value["native"]["bridge"]["port"] = True
    path.write_bytes(encoded(value))
    for key in configuration.LEGACY_KEYS:
        monkeypatch.delenv(key, raising=False)
    monkeypatch.setenv("SUNNY_CONFIG_PATH", str(path))

    def unexpected(*args, **kwargs):
        pytest.fail("configuration rejection happened after a startup effect")

    monkeypatch.setattr(surface.ControlSurface, "__init__", unexpected)
    monkeypatch.setattr(surface, "RemoteLog", unexpected)
    monkeypatch.setattr(surface, "ManagedRegistry", unexpected)
    monkeypatch.setattr(surface, "TcpServer", unexpected)
    with pytest.raises(ValueError, match="integer"):
        surface.SunnyControlSurface(None)


def clean_environment():
    """Isolate tests from the operator's Sunny runtime inputs."""
    return {
        key: value
        for key, value in os.environ.items()
        if key not in (*configuration.LEGACY_KEYS, "SUNNY_CONFIG_PATH")
    }


def test_native_cli_standalone_migration_requires_no_live_import(tmp_path):
    """The exported module is executable using ordinary client Python only."""
    output = tmp_path / "native external config.json"
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--migrate-legacy", str(output)],
        env=clean_environment(),
        capture_output=True,
        text=True,
        timeout=5,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout)["configuration_schema_version"] == 1
    assert json.loads(output.read_text())["native"]["bridge"]["bind_host"] == "127.0.0.1"


@pytest.fixture
def binary():
    """Require an actual configuration-capable build, without qualifying stale binaries."""
    selected = Path(os.environ.get("SUNNY_MCP_BINARY", ROOT / ".bin/sunny-mcp"))
    if not selected.is_file():
        pytest.skip("new configuration-capable sunny-mcp has not been built")
    result = subprocess.run(
        [str(selected), "--configuration-contract"],
        env=clean_environment(),
        input="",
        capture_output=True,
        text=True,
        timeout=5,
        check=False,
    )
    if not result.stdout.strip():
        pytest.skip("binary predates runtime configuration implementation")
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout) == {
        "configuration_schema_version": 1,
        "configuration_contract": "versioned_json",
    }
    return selected


def run_binary(binary, arguments=(), environment=None):
    """Launch the real executable with bounded IO and no implicit native connection."""
    return subprocess.run(
        [str(binary), *arguments],
        env=clean_environment() | (environment or {}),
        input="",
        capture_output=True,
        text=True,
        timeout=8,
        check=False,
    )


def test_actual_client_validate_returns_data_and_does_not_merge_environment(binary, tmp_path):
    """Launcher validation retains typed endpoint/workspace data and makes no locks."""
    path = tmp_path / "selected config.json"
    workspace = tmp_path / "work with spaces" / "saved.json"
    value = document({"path": str(workspace), "recovery": "none"})
    value["client"]["transport"] = {"mode": "tcp", "host": "host.docker.internal", "port": 49153}
    path.write_bytes(encoded(value))
    result = run_binary(binary, ("--validate-config", str(path)), {"SUNNY_TCP_PORT": "invalid"})
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout) == value
    assert not workspace.parent.exists()
    assert result.stderr == ""


@pytest.mark.parametrize("fault", ["port", "schema", "duplicate", "conflict", "unused_native"])
def test_actual_invalid_client_refuses_before_workspace_lock(binary, tmp_path, fault):
    """Complete validation precedes a writer lease even when the workspace is selected."""
    directory = tmp_path / "musical work"
    directory.mkdir()
    value = document({"path": str(directory / "saved.json"), "recovery": "none"})
    if fault == "port":
        value["client"]["transport"] = {"mode": "tcp", "host": "127.0.0.1", "port": True}
    elif fault == "schema":
        value["configuration_schema_version"] = 2
    elif fault == "unused_native":
        value["native"]["bridge"]["bind_host"] = "0.0.0.0"
    path = tmp_path / "bad.json"
    body = encoded(value)
    if fault == "duplicate":
        body = body.replace(
            b'"configuration_schema_version": 1',
            b'"configuration_schema_version": 1,"configuration_schema_version":1',
        )
    path.write_bytes(body)
    environment = {"SUNNY_CONFIG_PATH": str(path)}
    if fault == "conflict":
        environment["SUNNY_BIND_HOST"] = "127.0.0.1"
    result = run_binary(binary, environment=environment)
    assert result.returncode != 0 and result.stdout == "", result
    assert "fatal error" in result.stderr
    assert list(directory.iterdir()) == []


def test_actual_client_migration_preserves_relative_legacy_path_without_workspace_effect(
    binary, tmp_path
):
    """Migration captures an absolute path and writes only a new external config."""
    output = tmp_path / "client config with spaces.json"
    legacy = {
        "SUNNY_WORKSPACE_PATH": "legacy project with spaces.json",
        "SUNNY_WORKSPACE_RECOVERY": "backup",
        "SUNNY_ABLETON_HOST": "example.test",
        "SUNNY_TCP_PORT": "65535",
    }
    result = run_binary(binary, ("--migrate-config", str(output)), legacy)
    assert result.returncode == 0, result.stderr
    value = json.loads(output.read_text())
    assert value["client"]["workspace"] == {
        "path": str(Path.cwd() / legacy["SUNNY_WORKSPACE_PATH"]),
        "recovery": "backup",
    }
    assert value["client"]["transport"] == {"mode": "tcp", "host": "example.test", "port": 65535}
    before = output.read_bytes()
    refused = run_binary(binary, ("--migrate-config", str(output)), legacy)
    assert refused.returncode != 0 and output.read_bytes() == before
    assert sorted(path.name for path in tmp_path.iterdir()) == [output.name]


def test_actual_json_offline_save_restart_and_explicit_backup(binary, tmp_path, monkeypatch):
    """Production JSON preserves the existing saved-workspace and recovery semantics."""
    from test_live_end_to_end import _McpClient

    for key in configuration.LEGACY_KEYS:
        monkeypatch.delenv(key, raising=False)
    directory = tmp_path / "changed project path with spaces"
    directory.mkdir()
    workspace = directory / "saved.json"
    selected = tmp_path / "client external.json"
    value = document({"path": str(workspace), "recovery": "none"})
    selected.write_bytes(encoded(value))
    monkeypatch.setenv("SUNNY_CONFIG_PATH", str(selected))
    client = _McpClient(None, 0, host=None, command=[str(binary)])
    try:
        authored = client.call(
            "score_create",
            title="Literal durable JSON config",
            total_bars=1,
            parts=[{"name": "Piano", "instrument_type": 47}],
        )
        assert "error" not in authored, authored
        saved = client.call("workspace_save", path=str(workspace))
        assert saved["success"] and saved["durability_confirmed"], saved
        saved = client.call("workspace_save", path=str(workspace))
        assert saved["success"], saved
    finally:
        client.close()
    bytes_before = workspace.read_bytes()
    restarted = run_binary(binary, environment={"SUNNY_CONFIG_PATH": str(selected)})
    assert restarted.returncode == 0 and "restored authored workspace" in restarted.stderr
    workspace.write_bytes(b"corrupted main preserved")
    value["client"]["workspace"]["recovery"] = "backup"
    selected.write_bytes(encoded(value))
    recovered = run_binary(binary, environment={"SUNNY_CONFIG_PATH": str(selected)})
    assert recovered.returncode == 0 and "restored explicit backup" in recovered.stderr
    assert workspace.read_bytes() == b"corrupted main preserved"
    assert workspace.with_name(workspace.name + ".bak").read_bytes() == bytes_before


@pytest.mark.parametrize("host", ["127.0.0.1", "127.0.0.2"])
def test_actual_changed_json_endpoint_pairs_client_and_native_fixture(
    binary, tmp_path, monkeypatch, host
):
    """Both real peers consume external data; only an isolated read-only model is contacted."""
    import socket
    import time

    from live_model import LiveSet
    from Sunny import surface as surface_module
    from test_live_end_to_end import _LiveMainThread, _McpClient

    for key in configuration.LEGACY_KEYS:
        monkeypatch.delenv(key, raising=False)
    LiveSet((12, 4, 0)).install(monkeypatch)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reservation:
        reservation.bind((host, 0))
        port = reservation.getsockname()[1]
    value = document()
    value["client"]["transport"] = {"mode": "tcp", "host": host, "port": port}
    value["native"]["bridge"] = {"bind_host": host, "port": port}
    selected = tmp_path / "paired external config.json"
    selected.write_bytes(encoded(value))
    monkeypatch.setenv("SUNNY_CONFIG_PATH", str(selected))
    main_thread = _LiveMainThread()
    surface = main_thread.call(lambda: surface_module.SunnyControlSurface(object()))
    surface.schedule_message = main_thread.schedule_message
    client = None
    try:
        deadline = time.monotonic() + 5
        while not surface._server.is_running and time.monotonic() < deadline:
            time.sleep(0.01)
        assert surface._server.is_running and surface._server.bound_port == port
        client = _McpClient(None, 0, host=None, command=[str(binary)])
        state = client.call("get_ableton_session_state")
        assert state.get("success") is True, state
    finally:
        if client is not None:
            client.close()
        surface.disconnect()
        main_thread.stop()


@pytest.mark.parametrize("fault", ["relative", "control", "host", "unknown", "mode", "recovery"])
def test_selected_file_validation_uses_same_closed_contract_in_both_consumers(
    binary, tmp_path, fault
):
    """Independent production parsers refuse the same malformed role data."""
    value = document({"path": str(tmp_path / "workspace.json"), "recovery": "none"})
    if fault == "relative":
        value["client"]["workspace"]["path"] = "relative workspace.json"
    elif fault == "control":
        value["client"]["workspace"]["path"] += "\u0001"
    elif fault == "host":
        value["client"]["transport"] = {"mode": "tcp", "host": "bad host", "port": 9001}
    elif fault == "unknown":
        value["native"]["bridge"]["ssh_secret"] = "unused but unsafe"
    elif fault == "mode":
        value["client"]["transport"] = {"mode": "http", "host": "example.test", "port": 9001}
    else:
        value["client"]["workspace"]["recovery"] = "automatic"
    path = tmp_path / "malformed.json"
    path.write_bytes(encoded(value))
    with pytest.raises(ValueError):
        configuration.load_native_configuration({"SUNNY_CONFIG_PATH": str(path)})
    result = run_binary(binary, ("--validate-config", str(path)))
    assert result.returncode != 0 and result.stdout == "", result
    assert sorted(item.name for item in tmp_path.iterdir()) == [path.name]
