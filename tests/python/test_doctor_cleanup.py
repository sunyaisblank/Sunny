"""Doctor cleanup: actual Linux descendants, scoped daemon contracts and local Docker."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

import pytest
from test_doctor import _sunny_mcp_binary, doctor, native_boundary  # noqa: F401


@pytest.mark.skipif(not sys.platform.startswith("linux"), reason="Linux descendant reaping witness")
@pytest.mark.parametrize("parent_exits,detached", [(False, False), (True, False), (False, True)])
def test_actual_linux_cleanup_reaps_owned_descendants_even_after_parent_exit(
    parent_exits, detached
):
    """OS PID absence proves wrappers cannot leave an ordinary or adopted child running."""
    program = f"""import json,os,subprocess,sys,time
child=subprocess.Popen([sys.executable,"-c","import time; time.sleep(30)"],
    stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
    start_new_session={detached!r})
request=json.loads(sys.stdin.readline())
print(json.dumps({{"jsonrpc":"2.0","id":request["id"],
    "result":{{"pid":child.pid,"parent":os.getpid()}}}}),flush=True)
if not {parent_exits!r}: time.sleep(30)
"""
    client = doctor.StdioClient([sys.executable, "-u", "-c", program], time.monotonic() + 3)
    identifiers = client.request("ping", {})
    if parent_exits:
        client.process.wait(timeout=2)
    started = time.monotonic()
    cleanup = client.close(2)
    assert cleanup["success"] and cleanup["process_group_empty"], cleanup
    assert cleanup["process_exited"] and time.monotonic() - started < 2.2
    assert client.close(2) is cleanup  # No second cleanup against a reused PID/group.
    for identifier in identifiers.values():
        with pytest.raises(ProcessLookupError):
            os.kill(identifier, 0)


@pytest.mark.parametrize(
    "command",
    [
        ["docker", "exec", "-i", "caller-container", "sunny-mcp"],
        ["docker", "start", "caller-container"],
        ["docker", "--context", "caller-context", "run", "-i", "image"],
        ["docker", "run", "-it", "image"],
        ["docker", "run", "-i", "--detach", "image"],
        ["docker", "run", "-i", "--cidfile", "caller.cid", "image"],
        ["docker", "run", "-i", "--pull=always", "image"],
        ["docker", "run", "-i", "--restart=always", "image"],
        ["docker", "run", "-i", "--label", "com.sunny.doctor.owner=caller", "image"],
        ["docker", "run", "-i", "--label-file", "caller-labels", "image"],
        ["env", "docker", "run", "-i", "image"],
    ],
)
def test_unsupported_cleanup_profiles_decline_before_any_process(command, monkeypatch):
    """Caller containers and conflicting ownership/attach options cannot be launched."""
    monkeypatch.setattr(doctor.subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Spawned"))
    report = doctor.diagnose(command, 1)
    assert report["checks"][-1]["code"] == "unsupported_cleanup_profile"
    assert report["cleanup"] is None and not report["success"]


def test_windows_native_declines_before_spawn_without_claiming_job_ownership(monkeypatch):
    """The unsupported Windows native profile must never claim descendant containment."""
    monkeypatch.setattr(doctor, "PLATFORM", "win32")
    monkeypatch.setattr(doctor.subprocess, "Popen", lambda *args, **kwargs: pytest.fail("Spawned"))
    report = doctor.diagnose(["sunny-mcp.exe"], 1)
    assert report["checks"][-1]["code"] == "unsupported_cleanup_profile"
    assert report["cleanup"] is None and report["doctor"] is None


@pytest.mark.usefixtures("native_boundary")
def test_unconfirmed_cleanup_invalidates_an_actual_healthy_diagnosis(monkeypatch):
    """Installer confirmation must decline incomplete cleanup despite real fresh native reads."""
    original = doctor.StdioClient.close

    def unconfirmed(client, cleanup_seconds):
        result = original(client, cleanup_seconds)
        assert result["success"]  # The test still reaps its actual launched processes.
        return {**result, "success": False}

    monkeypatch.setattr(doctor.StdioClient, "close", unconfirmed)
    report = doctor.diagnose([str(_sunny_mcp_binary())], 5)
    assert report["doctor"]["success"] and report["read_only_ready"]
    assert not report["success"] and not report["cleanup"]["success"]
    assert report["checks"][-1]["layer"] == "cleanup"
    assert report["checks"][-1]["code"] == "owned_cleanup_unconfirmed"


def _owned_docker_model(
    monkeypatch, cidfile="valid", owner_matches=True, daemon_changes=False, removed=True
):
    """Model only daemon replies using independent literal IDs and ownership assertions."""
    identifier, foreign = "a" * 64, "b" * 64
    actions = []
    state = {"removed": False, "owner": None, "info": 0}

    def boundary(command, deadline, environment):
        assert deadline > time.monotonic()
        if command[1] == "info":
            state["info"] += 1
            return 0, "foreign-daemon" if daemon_changes and state["info"] > 1 else "local-daemon"
        assert command[1] == "container"
        operation = command[2]
        actions.append(command[2:])
        if operation == "ls":
            selector = command[command.index("--filter") + 1]
            if selector.startswith("label="):
                assert selector == f"label=com.sunny.doctor.owner={state['owner']}"
                return 0, "" if state["removed"] or not owner_matches else identifier
            assert selector in (f"id={identifier}", f"id={foreign}")
            return 0, "" if state["removed"] else selector[3:]
        if operation == "inspect":
            selected = command[-1]
            owner = state["owner"] if owner_matches else "preexisting-owner"
            return 0, f"{selected} {owner} true"
        assert operation in ("stop", "rm")
        assert command[-1] == identifier, "Pre-existing container selected for mutation"
        if operation == "rm":
            assert "--volumes" not in command
            state["removed"] = removed
        return 0, identifier

    monkeypatch.setattr(doctor, "_bounded_docker", boundary)
    owned = doctor.OwnedDocker(
        ["docker", "run", "-i", "image"], ["-i", "image"], {}, time.monotonic() + 2
    )
    state["owner"] = owned.owner
    assert not owned.cidfile.exists() and owned.cidfile.parent.is_dir()
    assert owned.command.count("--cidfile") == 1
    assert owned.command[owned.command.index("--cidfile") + 1] == str(owned.cidfile)
    assert "--pull=never" in owned.command
    if cidfile == "valid":
        owned.cidfile.write_text(identifier)
    elif cidfile == "foreign":
        owned.cidfile.write_text(foreign)
    elif cidfile == "interrupted":
        owned.cidfile.write_bytes(b"partial\xff")
    elif cidfile == "unreadable":
        original = Path.open

        def restricted(path, *args, **kwargs):
            if path == owned.cidfile:
                raise PermissionError
            return original(path, *args, **kwargs)

        owned.cidfile.write_text(identifier)
        monkeypatch.setattr(Path, "open", restricted)
    return owned, actions


@pytest.mark.parametrize("cidfile", ["valid", "missing", "interrupted", "unreadable", "foreign"])
def test_owned_container_cleanup_recovers_only_through_this_unique_label(cidfile, monkeypatch):
    """Interrupted/unreadable CID files resolve to this launch's exact label-owned ID."""
    owned, actions = _owned_docker_model(monkeypatch, cidfile)
    try:
        result = owned.close(time.monotonic() + 2)
        assert result["removed"] and result["readback"] == "removed"
        assert result["container_ids"] == ["a" * 64]
        assert result["owner_id"] == owned.owner
        assert [action[0] for action in actions].count("stop") == 1
        assert [action[0] for action in actions].count("rm") == 1
        assert result["ownership_source"] == ("cidfile" if cidfile == "valid" else "owner_label")
    finally:
        owned.directory.cleanup()


@pytest.mark.parametrize("failure", ["owner", "daemon", "remains", "unknown_creation"])
def test_cleanup_never_claims_success_from_unproven_daemon_or_owner(failure, monkeypatch):
    """A wrong owner, changed daemon, absent identity or failed removal remains incomplete."""
    owned, actions = _owned_docker_model(
        monkeypatch,
        cidfile="missing" if failure == "unknown_creation" else "valid",
        owner_matches=failure not in ("owner", "unknown_creation"),
        daemon_changes=failure == "daemon",
        removed=failure != "remains",
    )
    try:
        result = owned.close(time.monotonic() + 2)
        assert not result["removed"]
        assert result["owner_id"] == owned.owner
        if failure != "remains":
            assert all(action[0] not in ("stop", "rm") for action in actions)
    finally:
        owned.directory.cleanup()


@pytest.fixture
def local_docker():
    """Opt in only to an existing local socket/pipe daemon and an already available image."""
    if os.environ.get("SUNNY_DOCTOR_DOCKER_TESTS") != "1":
        pytest.skip("Set SUNNY_DOCTOR_DOCKER_TESTS=1 for authorized local Docker cleanup witnesses")
    executable = shutil.which("docker")
    if not executable:
        pytest.skip("Docker CLI unavailable")
    environment_host = os.environ.get("DOCKER_HOST")
    if environment_host and not environment_host.startswith(("unix://", "npipe://")):
        pytest.skip("Remote Docker endpoints are outside this local witness")
    context = subprocess.run(
        [executable, "context", "show"], capture_output=True, text=True, timeout=2, check=True
    ).stdout.strip()
    host = subprocess.run(
        [executable, "context", "inspect", context, "--format", "{{.Endpoints.docker.Host}}"],
        capture_output=True,
        text=True,
        timeout=2,
        check=True,
    ).stdout.strip()
    if not host.startswith(("unix://", "npipe://")):
        pytest.skip("Only a local Docker endpoint may run this witness")
    image = "busybox:1.36"
    present = subprocess.run(
        [executable, "image", "inspect", image, "--format", "{{.Id}}"],
        capture_output=True,
        text=True,
        timeout=2,
    )
    if present.returncode:
        pytest.skip("The offline witness image must already exist; no pull or build is performed")
    return executable, image


@pytest.mark.parametrize("program", ["sleep 30", "exit 0"])
def test_actual_local_docker_cleanup_readback_leaves_no_owned_container(local_docker, program):
    """Both timeout and normal container exit have actual daemon-side orphan readback."""
    executable, image = local_docker
    started = time.monotonic()
    report = doctor.diagnose(
        [executable, "run", "-i", "--rm", image, "sh", "-c", program], 1.0, cleanup_seconds=8
    )
    cleanup = report["cleanup"]
    assert cleanup and cleanup["success"], report
    assert cleanup["container"]["readback"] == "removed"
    assert cleanup["container"]["container_ids"] and cleanup["process_exited"]
    assert time.monotonic() - started < 9.5
    token = cleanup["container"]["owner_id"]
    actual = subprocess.run(
        [
            executable,
            "container",
            "ls",
            "--all",
            "--no-trunc",
            "--filter",
            f"label=com.sunny.doctor.owner={token}",
            "--format",
            "{{.ID}}",
        ],
        capture_output=True,
        text=True,
        timeout=2,
        check=True,
    )
    assert actual.stdout.strip() == ""
    for identifier in cleanup["container"]["container_ids"]:
        missing = subprocess.run(
            [executable, "container", "inspect", identifier, "--format", "{{.State.Running}}"],
            capture_output=True,
            text=True,
            timeout=2,
        )
        assert missing.returncode != 0


def test_actual_local_docker_exec_target_is_preserved(local_docker):
    """An existing caller container is never launched into or selected for doctor cleanup."""
    executable, image = local_docker
    created = subprocess.run(
        [executable, "run", "--pull=never", "-d", image, "sleep", "30"],
        capture_output=True,
        text=True,
        timeout=3,
        check=True,
    )
    identifier = created.stdout.strip()
    assert doctor.HEX64.fullmatch(identifier)
    try:
        report = doctor.diagnose([executable, "exec", "-i", identifier, "sleep", "10"], 1)
        assert report["checks"][-1]["code"] == "unsupported_cleanup_profile"
        assert report["cleanup"] is None
        observed = subprocess.run(
            [executable, "container", "inspect", identifier, "--format", "{{.State.Running}}"],
            capture_output=True,
            text=True,
            timeout=2,
            check=True,
        )
        assert observed.stdout.strip() == "true"
    finally:
        subprocess.run(
            [executable, "container", "rm", "--force", identifier],
            capture_output=True,
            timeout=3,
            check=True,
        )  # Only this test's own container.


@pytest.mark.parametrize("interrupt_cidfile", [False, True])
def test_actual_local_docker_killed_client_cleanup_does_not_rely_on_signal_proxy(
    local_docker, interrupt_cidfile
):
    """A killed CLI leaves a running container; exact CID/label cleanup must remove it."""
    executable, image = local_docker
    client = doctor.StdioClient(
        [executable, "run", "-i", "--sig-proxy=false", image, "sleep", "30"],
        time.monotonic() + 4,
    )
    owner = client.docker
    try:
        deadline = time.monotonic() + 3
        while not owner.cidfile.is_file() or len(owner.cidfile.read_text().strip()) != 64:
            assert time.monotonic() < deadline
            time.sleep(0.02)
        identifier = owner.cidfile.read_text().strip()
        # The CID is written after create, before start. Observe the daemon-side
        # orphan precondition before killing the attached client.
        while True:
            running = subprocess.run(
                [executable, "container", "inspect", identifier, "--format", "{{.State.Running}}"],
                capture_output=True,
                text=True,
                timeout=2,
                check=True,
            )
            if running.stdout.strip() == "true":
                break
            assert time.monotonic() < deadline
            time.sleep(0.02)
        client.process.kill()  # No forwarded signal: equivalent daemon ownership problem.
        client.process.wait(timeout=2)
        running = subprocess.run(
            [executable, "container", "inspect", identifier, "--format", "{{.State.Running}}"],
            capture_output=True,
            text=True,
            timeout=2,
            check=True,
        )
        assert running.stdout.strip() == "true"
        if interrupt_cidfile:
            owner.cidfile.unlink()
        cleanup = client.close(8)
        assert cleanup["success"] and cleanup["container"]["removed"], cleanup
        assert cleanup["container"]["container_ids"] == [identifier]
        assert cleanup["container"]["ownership_source"] == (
            "owner_label" if interrupt_cidfile else "cidfile"
        )
        absent = subprocess.run(
            [
                executable,
                "container",
                "ls",
                "--all",
                "--filter",
                f"label=com.sunny.doctor.owner={owner.owner}",
                "--format",
                "{{.ID}}",
            ],
            capture_output=True,
            text=True,
            timeout=2,
            check=True,
        )
        assert absent.stdout.strip() == ""
    finally:
        client.close(8)
