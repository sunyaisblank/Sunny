"""Literal Docker grammar for the image-owned final developer qualification kit."""

from __future__ import annotations

import copy
import importlib.util
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

IMAGE = "sha256:" + "a" * 64
OTHER_IMAGE = "sha256:" + "b" * 64
KIT = Path(__file__).resolve().parents[2] / "tools/live_qualification/common.py"


def load_fault_proxy(monkeypatch, helper):
    """Test maintained operator code with the selected release's authenticated helper."""
    monkeypatch.setitem(sys.modules, "common", helper)
    spec = importlib.util.spec_from_file_location(
        "qualification_fault_proxy_test", KIT.with_name("fault_proxy.py")
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def fault_proxy(monkeypatch, common):
    """Load the real proxy without granting any child or native authority."""
    return load_fault_proxy(monkeypatch, common)


@pytest.fixture
def common():
    """Load the real maintained helper without a child or daemon operation."""
    specification = importlib.util.spec_from_file_location("qualification_launch_test", KIT)
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


def configuration():
    """Use literal production argv with one external JSON file and one data directory."""
    return {
        "release_directory": "/verified/offline/release",
        "image_local_immutable_id": IMAGE,
        "host_mount_directory": "/caller/qualification data",
        "server_mount_prefix": "/data",
        "mcp_command": [
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--mount",
            "type=bind,source=/caller/production.json,target=/run/sunny/configuration.json,readonly",
            "--mount",
            "type=bind,source=/caller/qualification data,target=/data",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            IMAGE,
        ],
    }


@pytest.mark.parametrize("spelling", ["literal", "equals", "interactive", "windows_cli"])
def test_correct_final_profile_identifies_actual_image(common, spelling):
    """Useful canonical spellings select the exact immutable image, never a trailing command."""
    config = configuration()
    command = config["mcp_command"]
    if spelling == "equals":
        for index in (9, 7, 5):
            command[index : index + 2] = [command[index] + "=" + command[index + 1]]
    elif spelling == "interactive":
        command[2] = "--interactive"
        command[4:5] = ["--pull", "never"]
    elif spelling == "windows_cli":
        command[0] = "/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe"
    assert common.qualification_command(config, IMAGE) is command


def test_bounded_optional_identity_and_bridge_route_options(common):
    """The route does not require host networking, executable overlays or extra environment."""
    config = configuration()
    config["mcp_command"][2:2] = [
        "--name",
        "sunny-final-1",
        "--label",
        "qualification.run=literal-1",
        "--platform=linux/amd64",
        "--network=bridge",
        "--add-host=host.docker.internal:host-gateway",
    ]
    assert common.qualification_command(config, IMAGE) == config["mcp_command"]


@pytest.mark.parametrize("selector", [None, "", False, []])
def test_missing_release_cannot_fall_through_to_an_unverified_child(
    common, monkeypatch, tmp_path, selector
):
    """Reconnect/readback/reconcile launches obey the same complete-release boundary."""
    config = {"mcp_command": ["python3", "-c", "pass"]}
    if selector is not None:
        config["release_directory"] = selector
    monkeypatch.setattr(
        subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Unverified child launched")
    )
    with pytest.raises(ValueError, match="complete verified release"):
        common.Mcp(config, tmp_path / "observations.jsonl")


@pytest.mark.parametrize(
    "fault",
    [
        "different_image",
        "wrong_image_trailing_verified_id",
        "trailing_command",
        "entrypoint",
        "binary_overlay",
        "operator_overlay",
        "root_overlay",
        "data_parent_overlay",
        "legacy_env",
        "env_file",
        "unselected_config",
        "missing_config_readonly",
        "readonly_false",
        "readonly_data",
        "different_data",
        "duplicate_data",
        "duplicate_config",
        "duplicate_mount_field",
        "mount_alias",
        "unknown_mount_field",
        "volume_shorthand",
        "missing_mount_value",
        "host_network",
        "custom_network",
        "user",
        "workdir",
        "platform",
        "add_host",
        "tty",
        "detach",
        "pull_latest",
        "missing_pull",
        "missing_rm",
        "missing_interactive",
        "duplicate_env",
        "duplicate_option",
        "duplicate_label",
        "reserved_label",
        "missing_image",
        "boolean_argument",
    ],
)
def test_unpaired_or_ambiguous_profile_declines_before_any_child_or_socket(
    common, monkeypatch, tmp_path, fault
):
    """The real Mcp boundary fails closed before process, daemon or native endpoint access."""
    config = configuration()
    command = config["mcp_command"]
    if fault == "different_image":
        command[-1] = OTHER_IMAGE
    elif fault == "wrong_image_trailing_verified_id":
        command[-1:] = [OTHER_IMAGE, IMAGE]
    elif fault == "trailing_command":
        command.append("another-server")
    elif fault == "entrypoint":
        command[2:2] = ["--entrypoint=/tmp/other-server"]
    elif fault.endswith("overlay"):
        target = {
            "binary_overlay": "/usr/local/bin/sunny-mcp",
            "operator_overlay": "/opt/sunny/operator",
            "root_overlay": "/",
            "data_parent_overlay": "/run",
        }[fault]
        command[2:2] = ["--mount", "type=bind,source=/caller/older,target=" + target]
    elif fault == "legacy_env":
        command[2:2] = ["--env", "SUNNY_ABLETON_HOST=other-host"]
    elif fault == "env_file":
        command[2:2] = ["--env-file=/caller/other.env"]
    elif fault == "unselected_config":
        command[10] = "SUNNY_CONFIG_PATH=/caller/other.json"
    elif fault == "missing_config_readonly":
        command[6] = command[6].removesuffix(",readonly")
    elif fault == "readonly_false":
        command[6] += "=false"
    elif fault == "readonly_data":
        command[8] += ",readonly"
    elif fault == "different_data":
        command[8] = command[8].replace("/caller/qualification data", "/caller/other")
    elif fault == "duplicate_data":
        command[2:2] = command[7:9]
    elif fault == "duplicate_config":
        command[2:2] = command[5:7]
    elif fault == "duplicate_mount_field":
        command[8] += ",target=/data"
    elif fault == "mount_alias":
        command[8] = command[8].replace("source=", "src=")
    elif fault == "unknown_mount_field":
        command[8] += ",bind-propagation=rshared"
    elif fault == "volume_shorthand":
        command[2:2] = ["-v", "/caller/older:/usr/local/bin"]
    elif fault == "missing_mount_value":
        command[-1:] = ["--mount"]
    elif fault == "host_network":
        command[2:2] = ["--network=host"]
    elif fault == "custom_network":
        command[2:2] = ["--network=other-network"]
    elif fault == "user":
        command[2:2] = ["--user=0"]
    elif fault == "workdir":
        command[2:2] = ["--workdir=/caller"]
    elif fault == "platform":
        command[2:2] = ["--platform=linux/arm64"]
    elif fault == "add_host":
        command[2:2] = ["--add-host=host.docker.internal:192.0.2.1"]
    elif fault == "tty":
        command[2:2] = ["-t"]
    elif fault == "detach":
        command[2:2] = ["--detach"]
    elif fault == "pull_latest":
        command[4] = "--pull=always"
    elif fault == "missing_pull":
        del command[4]
    elif fault == "missing_rm":
        del command[3]
    elif fault == "missing_interactive":
        del command[2]
    elif fault == "duplicate_env":
        command[2:2] = ["-e", "SUNNY_CONFIG_PATH=/run/sunny/configuration.json"]
    elif fault == "duplicate_option":
        command[2:2] = ["--platform=linux/amd64", "--platform=linux/amd64"]
    elif fault == "duplicate_label":
        command[2:2] = ["--label=qualification.run=1", "--label=qualification.run=2"]
    elif fault == "reserved_label":
        command[2:2] = ["--label=com.sunny.doctor.owner=caller"]
    elif fault == "missing_image":
        command.pop()
    elif fault == "boolean_argument":
        command[3] = True
    monkeypatch.setattr(
        common, "verify_selected_release", lambda ignored: {"image": {"local_immutable_id": IMAGE}}
    )
    monkeypatch.setattr(
        subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Unpaired launch started a child")
    )
    monkeypatch.setattr(
        socket, "socket", lambda *args, **kwargs: pytest.fail("Unpaired launch reached a socket")
    )
    with pytest.raises(ValueError):
        common.Mcp(copy.deepcopy(config), tmp_path / "no-effect.jsonl")
    assert not (tmp_path / "no-effect.jsonl").exists()


@pytest.mark.parametrize(
    "host,port",
    [
        ("0.0.0.0", 9003),
        ("192.0.2.1", 9003),
        ("localhost", 9003),
        ("::1", 9003),
        ("127.0.0.1", 0),
        ("127.0.0.1", 65536),
    ],
)
def test_fault_proxy_refuses_plain_tcp_exposure_before_configuration_or_listener(
    tmp_path, host, port
):
    """The finite effect-forwarding instrument shares the supported loopback boundary."""
    log = tmp_path / "no-effects.jsonl"
    result = subprocess.run(
        [
            sys.executable,
            str(KIT.with_name("fault_proxy.py")),
            "--config",
            str(tmp_path / "configuration-does-not-exist.json"),
            "--listen-host",
            host,
            "--listen-port",
            str(port),
            "--drop-method",
            "sunny_managed_create_clip",
            "--log",
            str(log),
        ],
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert result.returncode == 2, result.stderr
    assert "requires a numeric IPv4 loopback and port 1..65535" in result.stderr
    assert "FileNotFoundError" not in result.stderr
    assert not log.exists()


@pytest.mark.parametrize("selector", [None, "", False, []])
def test_fault_observer_authenticates_release_before_listener_or_child(
    fault_proxy, monkeypatch, tmp_path, selector
):
    """The instrument cannot fall through to an unverified ledger child."""
    config = configuration()
    config["release_directory"] = selector
    monkeypatch.setattr(subprocess, "Popen", lambda *a, **k: pytest.fail("Unverified child"))
    monkeypatch.setattr(socket, "socket", lambda *a, **k: pytest.fail("Unverified listener"))
    with pytest.raises(ValueError, match="complete verified release"):
        fault_proxy.LedgerObserver(config, tmp_path / "evidence.jsonl")


@pytest.mark.parametrize("fault", ["wrong_image", "user", "entrypoint", "readonly_data"])
def test_fault_observer_admits_exact_profile_before_daemon_or_child(
    fault_proxy, monkeypatch, tmp_path, fault
):
    """Pure launch-grammar tests precede cleanup delegation and Docker operations."""
    config = configuration()
    if fault == "wrong_image":
        config["mcp_command"][-1] = OTHER_IMAGE
    elif fault == "user":
        config["mcp_command"][2:2] = ["--user=0"]
    elif fault == "entrypoint":
        config["mcp_command"][2:2] = ["--entrypoint=other"]
    else:
        config["mcp_command"][8] += ",readonly"
    monkeypatch.setattr(fault_proxy, "verify_selected_release", lambda _: {})
    monkeypatch.setattr(subprocess, "Popen", lambda *a, **k: pytest.fail("Unpaired child"))
    with pytest.raises(ValueError):
        fault_proxy.LedgerObserver(config, tmp_path / "evidence.jsonl")


MANAGED_FAMILIES = (
    "sunny_managed_create_clip",
    "sunny_managed_replace_clip",
    "sunny_managed_rebind",
    "sunny_managed_update_notes",
    "sunny_managed_revise_note_population",
    "sunny_managed_update_clip_geometry",
    "sunny_managed_insert_device",
    "sunny_managed_update_device_parameters",
    "sunny_managed_update_device_modes",
    "sunny_managed_adopt_devices",
    "sunny_managed_adopt_clip",
    "sunny_managed_author_envelope",
    "sunny_managed_replace_envelope",
    "sunny_managed_adopt_static_mixer",
    "sunny_managed_update_static_mixer",
    "sunny_managed_apply_routing",
    "sunny_managed_apply_song_settings",
)


def fence_fixture(name="sunny_managed_create_clip"):
    """Literal disk/wire correlation fixture, without claiming a native prepare grant."""
    request = {
        "bridge_protocol_version": 47,
        "type": "call",
        "path": "song",
        "name": name,
        "args": [{"operation_id": "1" * 32, "document_token": "2" * 32}],
    }
    entry = {
        "intent": {"attempt_id": "1" * 32, "prepared": {"request": copy.deepcopy(request)}},
        "dispatch_state": "may_have_sent",
        "evidence": [],
    }
    return request, entry


@pytest.mark.parametrize("name", MANAGED_FAMILIES)
def test_fault_fence_preserves_all_seventeen_families_and_exact_payload(fault_proxy, name):
    """An exact fence succeeds; same token plus changed native payload declines."""
    assert fault_proxy.MANAGED_MUTATIONS == frozenset(MANAGED_FAMILIES)
    request, entry = fence_fixture(name)
    assert fault_proxy.require_fence(request, [entry]) == entry
    request["args"][0]["document_token"] = "3" * 32
    with pytest.raises(RuntimeError, match="exact immutable"):
        fault_proxy.require_fence(request, [entry])


@pytest.mark.parametrize("fault", ["type", "path", "version_type", "extra", "args", "token"])
def test_fault_fence_rejects_malformed_frame_before_observer(
    fault_proxy, monkeypatch, tmp_path, fault
):
    """Malformed native requests never trigger a disk child or a socket send."""
    request, _ = fence_fixture()
    if fault == "extra":
        request["extra"] = True
    elif fault == "args":
        request["args"] = [{}, {}]
    elif fault == "token":
        request["args"][0]["operation_id"] = "not-original-token"
    else:
        key = {"type": "type", "path": "path", "version_type": "bridge_protocol_version"}[fault]
        request[key] = True if fault == "version_type" else "other"
    observer = fault_proxy.LedgerObserver.__new__(fault_proxy.LedgerObserver)
    monkeypatch.setattr(observer, "observe", lambda *a: pytest.fail("Malformed request child"))
    monkeypatch.setattr(fault_proxy, "send_bytes_frame", lambda *a: pytest.fail("Native send"))
    with pytest.raises(RuntimeError, match="withheld"):
        fault_proxy.forward_once(
            observer, None, b"unused", request, tmp_path / "evidence.jsonl", time.monotonic() + 1
        )


@pytest.mark.parametrize("failure", [PermissionError, TimeoutError, ValueError])
def test_fault_observer_failure_withholds_real_socket_and_retains_evidence(
    fault_proxy, monkeypatch, tmp_path, failure
):
    """A failed independent observation cannot reach the native frame sender."""
    request, _ = fence_fixture()
    observer = fault_proxy.LedgerObserver.__new__(fault_proxy.LedgerObserver)

    def fail(*unused):
        raise failure("observer failure")

    monkeypatch.setattr(observer, "observe", fail)
    log = tmp_path / "evidence.jsonl"
    outgoing, peer = socket.socketpair()
    with outgoing, peer:
        peer.setblocking(False)
        with pytest.raises(RuntimeError, match="withheld"):
            fault_proxy.forward_once(
                observer, outgoing, json.dumps(request).encode(), request, log, time.monotonic() + 1
            )
        with pytest.raises(BlockingIOError):
            peer.recv(1)
    events = [json.loads(line) for line in log.read_text().splitlines()]
    assert [event["kind"] for event in events] == ["native_request_withheld"]
    assert events[0]["token"] == "1" * 32 and events[0]["reason"] == failure.__name__


@pytest.mark.parametrize("mode", ["success", "output_limit", "timeout", "inherited_stdout"])
def test_fault_observer_output_is_finite_and_cleans_its_process_group(fault_proxy, mode):
    """Use real isolated processes for output, time, and exited-parent pipe bounds."""
    code = {
        "success": "print('original disk bytes', end='')",
        "output_limit": "import sys; sys.stdout.write('x'*8192)",
        "timeout": "import time; time.sleep(10)",
        "inherited_stdout": (
            "import subprocess,sys; "
            "subprocess.Popen([sys.executable,'-c','import time; time.sleep(10)']); "
            "print('parent completed',flush=True)"
        ),
    }[mode]
    start = time.monotonic()
    if mode == "success":
        assert (
            fault_proxy.observer_output(
                [sys.executable, "-c", code], start + 1, dict(os.environ), limit=1024
            )
            == b"original disk bytes"
        )
    else:
        with pytest.raises((ValueError, TimeoutError)):
            fault_proxy.observer_output(
                [sys.executable, "-c", code], start + 0.2, dict(os.environ), limit=1024
            )
    assert time.monotonic() - start < 2


@pytest.mark.parametrize("source", ["environment", "context"])
@pytest.mark.parametrize(
    "endpoint,accepted",
    [
        ("unix:///var/run/docker.sock", True),
        ("npipe:////./pipe/docker_engine", True),
        ("npipe:////./pipe/dockerDesktopLinuxEngine", True),
        ("npipe:////192.0.2.1/pipe/docker_engine", False),
        ("npipe:////server/pipe/docker_engine", False),
        ("tcp://192.0.2.1:2375", False),
        ("unix://server/docker.sock", False),
    ],
)
def test_fault_observer_local_endpoint_is_checked_before_daemon_calls(
    fault_proxy, source, endpoint, accepted
):
    """Literal environment/context URI check; no endpoint or actual daemon is contacted."""
    observer = fault_proxy.LedgerObserver.__new__(fault_proxy.LedgerObserver)
    observer.environment = {"DOCKER_HOST": endpoint} if source == "environment" else {}
    observer.command = ["docker"]
    calls = []

    class ContextRead:
        """Model only the local CLI's read-only context-file response."""

        @staticmethod
        def _bounded_docker(command, deadline, environment):
            calls.append(command)
            assert command == ["docker", "context", "inspect"]
            return 0, json.dumps([{"Endpoints": {"docker": {"Host": endpoint}}}])

    observer.doctor = ContextRead()
    if accepted:
        observer._local_daemon(time.monotonic() + 1)
        assert calls == [["docker", "context", "inspect"]]
    else:
        with pytest.raises(ValueError, match="local Docker"):
            observer._local_daemon(time.monotonic() + 1)
        assert len(calls) == (0 if source == "environment" else 1)


def test_fault_observer_reads_private_uid1001_ledger_with_verified_existing_image(
    monkeypatch, tmp_path
):
    """Actual Docker UID/mount join; no Live ABI, native execution, or image build claim."""
    if os.environ.get("SUNNY_FAULT_OBSERVER_TESTS") != "1":
        pytest.skip("Enable the existing-image private-ledger witness explicitly")
    if os.name != "posix" or os.getuid() != 1000:
        pytest.skip("This fixture proves the explicit host UID1000/image UID1001 mismatch")
    release = Path(os.environ["SUNNY_RELEASE_PROTOCOL_DIRECTORY"])
    trusted = os.environ["SUNNY_RELEASE_PROTOCOL_MANIFEST_SHA256"]
    image = os.environ["SUNNY_RELEASE_PROTOCOL_IMAGE"]
    spec = importlib.util.spec_from_file_location(
        "qualification_private_ledger_release_helper",
        release / "operator/live_qualification/common.py",
    )
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    proxy = load_fault_proxy(monkeypatch, helper)
    manifest = json.loads((release / "release.json").read_bytes())
    data = tmp_path / "new owned qualification data"
    data.mkdir()
    data.chmod(0o777)  # Only a newly owned fixture bind, allowing default image UID setup.
    production = tmp_path / "production.json"
    production.write_text(
        json.dumps(
            {
                "configuration_schema_version": 1,
                "client": {
                    "transport": {"mode": "tcp", "host": "127.0.0.1", "port": 9001},
                    "workspace": {"path": "/data/workspace.json", "recovery": "none"},
                },
                "native": {"bridge": {"bind_host": "127.0.0.1", "port": 9001}},
            }
        )
    )
    config = {
        "release_directory": str(release),
        "expected_release_manifest_sha256": trusted,
        "configuration_schema_version": manifest["configuration_schema_version"],
        "expected_bridge_source_sha256": manifest["bridge"]["source_sha256"],
        "expected_bridge_protocol_version": manifest["bridge"]["contract"][
            "bridge_protocol_version"
        ],
        "expected_target_snapshot_schema_version": manifest["bridge"]["contract"][
            "target_snapshot_schema_version"
        ],
        "image_local_immutable_id": image,
        "host_mount_directory": str(data),
        "host_workspace": str(data / "workspace.json"),
        "server_mount_prefix": "/data",
        "server_workspace": "/data/workspace.json",
        "mcp_command": [
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--mount",
            f"type=bind,source={production},target=/run/sunny/configuration.json,readonly",
            "--mount",
            f"type=bind,source={data},target=/data",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            image,
        ],
    }
    log = tmp_path / "actual observer evidence.jsonl"
    observer = proxy.LedgerObserver(config, log)  # Full archive/toolkit verification before child.
    observer._local_daemon(time.monotonic() + 10)
    request, entry = fence_fixture()
    namespace = "a" * 32
    document = {
        "format": "sunny-realization-ledger",
        "schema_version": 1,
        "workspace_namespace": namespace,
        "attempts": [entry],
    }
    cleanup_records = []

    def owned_fixture(code, *arguments):
        # Parent-authorized creation/cleanup writes ONLY this new pytest bind.
        # No runtime MCP/native process is launched, and no history ACL changes.
        owned = observer.doctor.OwnedDocker(
            observer.command,
            [
                "--name",
                "sunny-private-ledger-fixture-" + os.urandom(16).hex(),
                "--network=none",
                "--read-only",
                "--mount",
                f"type=bind,source={data},target=/data",
                "--entrypoint=python3",
                image,
                "-I",
                "-S",
                "-c",
                code,
                *arguments,
            ],
            observer.environment,
            time.monotonic() + 10,
        )
        try:
            assert observer.daemon is None or owned.daemon == observer.daemon
            result = proxy.observer_output(
                owned.command, time.monotonic() + 10, observer.environment
            )
        finally:
            cleanup = owned.close(time.monotonic() + 10)
            cleanup_records.append(cleanup)
            owned.directory.cleanup()
        assert cleanup["removed"] is True, cleanup
        return json.loads(result or b"null")

    setup = r"""
import hashlib,json,os,pathlib,shutil,sys
assert os.getuid() == 1001
root = pathlib.Path('/data')
for name in ('history','history-alternate','history-redirect'):
    path = root/name
    if path.is_symlink(): path.unlink()
    elif path.exists(): shutil.rmtree(path)
doc = json.loads(sys.argv[1])
ns = doc['workspace_namespace']
directory = root/'history'/ns
directory.mkdir(parents=True,mode=0o700)
directory.chmod(0o700)
path = directory/'ledger.json'
path.write_text(json.dumps(doc))
path.chmod(0o600)
workspace = root/'workspace.json'
workspace.write_text(json.dumps({'native_realization': {
    'workspace_namespace': ns, 'history_base_directory': '/data/history'}}))
workspace.chmod(0o600)
print(json.dumps({'uid':os.getuid(), 'ledger_mode':path.stat().st_mode&0o777,
                  'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}))
"""
    mutate = r"""
import json,pathlib,shutil,sys
root=pathlib.Path('/data'); workspace=root/'workspace.json'
m=json.loads(workspace.read_text()); native=m['native_realization']; case=sys.argv[1]
path=root/'history'/native['workspace_namespace']/'ledger.json'
if case == 'invalid_namespace': native['workspace_namespace']='not-a-namespace'
elif case == 'replacement_namespace': native['workspace_namespace']='b'*32
elif case == 'outside_base': native['history_base_directory']='/tmp'
elif case == 'redirected_base':
    (root/'history-redirect').symlink_to('/tmp',target_is_directory=True)
    native['history_base_directory']='/data/history-redirect'
elif case == 'redirected_namespace':
    parent=path.parent; parent.rename(parent.with_name('kept'))
    parent.symlink_to('kept',target_is_directory=True)
elif case == 'redirected_ledger':
    path.rename(path.with_name('kept.json')); path.symlink_to('kept.json')
elif case == 'changed_original_path':
    shutil.copytree(root/'history',root/'history-alternate')
    native['history_base_directory']='/data/history-alternate'
elif case == 'ledger_namespace_mismatch':
    d=json.loads(path.read_text()); d['workspace_namespace']='c'*32
    path.write_text(json.dumps(d))
else: raise ValueError(case)
workspace.write_text(json.dumps(m))
"""
    teardown = r"""
import pathlib,shutil
root=pathlib.Path('/data')
for path in root.iterdir():
    if path.is_symlink() or path.is_file(): path.unlink()
    elif path.is_dir(): shutil.rmtree(path)
"""
    try:
        original = owned_fixture(setup, json.dumps(document))
        assert original["uid"] == 1001 and original["ledger_mode"] == 0o600
        assert (data / "workspace.json").stat().st_uid == 1001
        with pytest.raises(PermissionError):
            (data / "workspace.json").read_bytes()
        with pytest.raises(PermissionError):
            (data / "history" / namespace / "ledger.json").read_bytes()
        outgoing, peer = socket.socketpair()
        with outgoing, peer:
            raw = json.dumps(request).encode()
            proxy.forward_once(observer, outgoing, raw, request, log, time.monotonic() + 10)
            assert helper.recv_frame(peer, time.monotonic() + 1) == raw
        assert observer.namespace == namespace
        assert observer.ledger == "/data/history/" + namespace + "/ledger.json"
        for case in (
            "invalid_namespace",
            "replacement_namespace",
            "outside_base",
            "redirected_base",
            "redirected_namespace",
            "redirected_ledger",
            "changed_original_path",
            "ledger_namespace_mismatch",
        ):
            owned_fixture(setup, json.dumps(document))
            owned_fixture(mutate, case)
            outgoing, peer = socket.socketpair()
            with outgoing, peer:
                peer.setblocking(False)
                with pytest.raises(RuntimeError, match="withheld"):
                    proxy.forward_once(
                        observer,
                        outgoing,
                        json.dumps(request).encode(),
                        request,
                        log,
                        time.monotonic() + 10,
                    )
                with pytest.raises(BlockingIOError):
                    peer.recv(1)
        # Re-observe original valid bytes after failures; observation never repairs
        # metadata or mutates the ledger, and it never takes the writer's lock.
        restored = owned_fixture(setup, json.dumps(document))
        assert restored["sha256"] == original["sha256"]
        assert proxy.observe_fence(observer, request, log, time.monotonic() + 10) == entry
        events = [json.loads(line) for line in log.read_text().splitlines()]
        assert sum(event["kind"] == "forward_once" for event in events) == 1
        assert sum(event["kind"] == "native_request_withheld" for event in events) == 8
        actual_cleanup = [e["cleanup"] for e in events if e["kind"] == "ledger_observer_cleanup"]
        assert len(actual_cleanup) == 10 and all(c["removed"] is True for c in actual_cleanup)
        assert all(c["daemon_id"] == observer.daemon for c in actual_cleanup)
    finally:
        owned_fixture(teardown)
    assert not list(data.iterdir())
    assert all(record["removed"] is True for record in cleanup_records)
