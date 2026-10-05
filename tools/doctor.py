"""Finite read-only Sunny stdio/bridge diagnosis and deliberate scoped JSON export."""

from __future__ import annotations

import argparse
import collections
import ctypes
import json
import math
import os
import queue
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from pathlib import Path

MAX_MESSAGE_BYTES = 64 * 1024 * 1024
MAX_EXPORT_BYTES = 1024 * 1024
MAX_RECORDS = 1000
HEX32 = re.compile(r"[0-9a-f]{32}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
PLATFORM = sys.platform
OWNER_LABEL = "com.sunny.doctor.owner"
DEFAULT_CLEANUP_SECONDS = 10.0


def _owned_linux_pids(group: int, parent: int | None = None) -> list[int]:
    """Read only process IDs in this private group or adopted by its isolated supervisor."""
    result = []
    for path in Path("/proc").iterdir():
        if not path.name.isdecimal() or int(path.name) == os.getpid():
            continue
        try:
            fields = (path / "stat").read_text().rsplit(")", 1)[1].split()
            if int(fields[2]) == group or (parent is not None and int(fields[1]) == parent):
                result.append(int(path.name))
        except (OSError, ValueError, IndexError):
            continue
    return result


def _supervise_native(command: list[str]) -> int:
    """Contain and reap Linux wrapper descendants in a separate, private subreaper."""
    # PR_SET_CHILD_SUBREAPER applies only to this supervisor, never the calling
    # Python application. Descendants are adopted here even after a wrapper exits.
    try:
        library = ctypes.CDLL(None, use_errno=True)
        if library.prctl(36, 1, 0, 0, 0) != 0:
            return 125
    except (OSError, AttributeError):
        return 125
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        process = subprocess.Popen(command)  # Inherit only the selected protocol pipes/group.
    except OSError:
        return 125
    while process.poll() is None and not stopping:
        time.sleep(0.02)
    result = process.poll()
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        for identifier in _owned_linux_pids(os.getpgrp(), os.getpid()):
            try:
                os.kill(identifier, signal.SIGKILL)
            except ProcessLookupError:
                pass
        process.poll()
        while True:
            try:
                reaped, _ = os.waitpid(-1, os.WNOHANG)
                if reaped == 0:
                    break
            except ChildProcessError:
                break
        if not _owned_linux_pids(os.getpgrp(), os.getpid()):
            return result if result is not None and result >= 0 else 143
        time.sleep(0.02)
    return 125


def _unsupported_launch():
    return DiagnosticFailure(
        "client_launch",
        "unsupported_cleanup_profile",
        "This launch profile cannot establish exclusive bounded cleanup ownership.",
        "Use direct docker run -i or POSIX native; Windows native and Docker exec are unsupported.",
    )


def _docker_arguments(command: list[str]) -> list[str] | None:
    """Admit only attached direct Docker run syntax whose resource ownership is known."""
    docker = Path(command[0]).name.lower() in ("docker", "docker.exe")
    if not docker:
        if any(Path(value).name.lower() in ("docker", "docker.exe") for value in command[1:]):
            raise _unsupported_launch()
        return None
    offset = 2 if command[1:2] == ["run"] else 3 if command[1:3] == ["container", "run"] else 0
    if not offset:
        raise _unsupported_launch()
    arguments = command[offset:]
    values = {
        "--env",
        "--env-file",
        "--volume",
        "--mount",
        "--network",
        "--add-host",
        "--name",
        "--user",
        "--workdir",
        "--entrypoint",
        "--platform",
        "--pull",
        "--restart",
        "--label",
        "--attach",
        "--stop-timeout",
    }
    booleans = {
        "--rm",
        "--interactive",
        "--tty",
        "--detach",
        "--init",
        "--read-only",
        "--sig-proxy",
        "--no-healthcheck",
    }
    short_values = {"e", "v", "l", "u", "w", "a"}
    interactive, index = False, 0
    while index < len(arguments):
        argument = arguments[index]
        if argument == "--":
            index += 1
            break
        if not argument.startswith("-"):
            break
        name, equal, value = argument.partition("=")
        if name in booleans:
            if equal and value not in ("true", "false"):
                raise _unsupported_launch()
            enabled = not equal or value == "true"
            if enabled and name in ("--tty", "--detach"):
                raise _unsupported_launch()
            if name == "--interactive":
                interactive = enabled
        elif argument in ("-i", "-t", "-d"):
            if argument != "-i":
                raise _unsupported_launch()
            interactive = True
        else:
            short = argument[1:2]
            if name not in values and not (not argument.startswith("--") and short in short_values):
                raise _unsupported_launch()
            if not argument.startswith("--"):
                name = {
                    "e": "--env",
                    "v": "--volume",
                    "l": "--label",
                    "u": "--user",
                    "w": "--workdir",
                    "a": "--attach",
                }[short]
                value = argument[2:]
                equal = bool(value)
            if not equal:
                index += 1
                if index >= len(arguments):
                    raise _unsupported_launch()
                value = arguments[index]
            if (
                (name == "--pull" and value != "never")
                or (name == "--restart" and value != "no")
                or (name == "--label" and value.partition("=")[0] == OWNER_LABEL)
            ):
                raise _unsupported_launch()
        index += 1
    if not interactive or index >= len(arguments):
        raise _unsupported_launch()
    return arguments


def _bounded_docker(command: list[str], deadline: float, environment: dict) -> tuple[int, str]:
    """Run one scoped daemon command with bounded time/output and private stderr."""
    remaining = min(2.0, deadline - time.monotonic())
    if remaining <= 0:
        raise TimeoutError
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        env=environment,
        start_new_session=os.name == "posix",
    )
    output = bytearray()
    oversized = False

    def read():
        nonlocal oversized
        assert process.stdout
        while chunk := process.stdout.read(512):
            if len(output) + len(chunk) <= 4096:
                output.extend(chunk)
            else:
                oversized = True

    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    try:
        process.wait(timeout=remaining)
    except subprocess.TimeoutExpired as exc:
        if os.name == "posix":
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        else:
            process.kill()
        process.wait(timeout=0.2)
        raise TimeoutError from exc
    reader.join(timeout=0.1)
    if reader.is_alive() or oversized:
        raise ValueError("Docker response was not bounded")
    return process.returncode, output.decode("utf-8").strip()


class OwnedDocker:
    """One run's exclusive cidfile and unique label; cleanup never selects caller containers."""

    def __init__(
        self, command: list[str], arguments: list[str], environment: dict, deadline: float
    ):
        """Inject ownership before launching Docker; the cidfile does not already exist."""
        self.executable, self.environment = command[0], environment
        try:
            self.daemon = self._daemon(deadline)
        except (OSError, TimeoutError, ValueError, UnicodeError, subprocess.TimeoutExpired) as exc:
            raise DiagnosticFailure(
                "client_launch",
                "docker_daemon_unavailable",
                "The selected Docker daemon identity could not be observed within the deadline.",
                "Check the configured local Docker context before starting diagnosis.",
            ) from exc
        self.owner = uuid.uuid4().hex
        self.directory = tempfile.TemporaryDirectory(prefix="sunny-doctor-")
        self.cidfile = Path(self.directory.name) / "container.cid"
        self.command = [
            self.executable,
            "run",
            "--cidfile",
            str(self.cidfile),
            "--label",
            f"{OWNER_LABEL}={self.owner}",
            "--pull=never",
            *arguments,
        ]
        self.identifiers: list[str] = []

    def _daemon(self, deadline):
        code, identity = _bounded_docker(
            [self.executable, "info", "--format", "{{.ID}}"], deadline, self.environment
        )
        if code or not re.fullmatch(r"[A-Za-z0-9:_-]{1,128}", identity):
            raise ValueError("Docker daemon identity unavailable")
        return identity

    def _call(self, arguments, deadline):
        return _bounded_docker(
            [self.executable, "container", *arguments], deadline, self.environment
        )

    def _listing(self, selector, deadline):
        code, output = self._call(
            ["ls", "--all", "--no-trunc", "--filter", selector, "--format", "{{.ID}}"], deadline
        )
        identifiers = output.splitlines() if output else []
        if code or any(not HEX64.fullmatch(identifier) for identifier in identifiers):
            raise ValueError("Docker ownership readback failed")
        return identifiers

    def close(self, deadline: float) -> dict:
        """Resolve this run's identity, stop/remove it, then verify absence on the daemon."""
        result = dict(
            owner_label=OWNER_LABEL,
            owner_id=self.owner,
            container_ids=[],
            ownership_source=None,
            removed=False,
            readback="ownership_unconfirmed",
            daemon_id=self.daemon,
            cidfile_status="missing",
        )
        try:
            if self._daemon(deadline) != self.daemon:
                raise ValueError("Docker daemon identity changed")
            try:
                if self.cidfile.is_file():
                    with self.cidfile.open("rb") as source:
                        raw = source.read(66)
                    identifier = raw.decode("ascii").strip()
                    result["cidfile_status"] = "invalid"
                    if HEX64.fullmatch(identifier) and len(raw) <= 65:
                        self.identifiers = [identifier]
                        result["ownership_source"] = "cidfile"
                        result["cidfile_status"] = "valid"
            except (OSError, UnicodeError):
                result["cidfile_status"] = "unavailable"
            label = f"label={OWNER_LABEL}={self.owner}"
            labelled = self._listing(label, deadline)
            if len(labelled) > 1:
                raise ValueError("Contradictory ownership identity")
            if labelled and labelled != self.identifiers:
                if self.identifiers:
                    result["cidfile_status"] = "conflict"
                self.identifiers = labelled
                result["ownership_source"] = "owner_label"
            result["container_ids"] = self.identifiers[:]
            if not self.identifiers:
                result["readback"] = "creation_unconfirmed"
                return result
            identifier = self.identifiers[0]
            if self._listing(f"id={identifier}", deadline):
                code, observed = self._call(
                    [
                        "inspect",
                        "--format",
                        '{{.Id}} {{index .Config.Labels "' + OWNER_LABEL + '"}} {{.State.Running}}',
                        identifier,
                    ],
                    deadline,
                )
                if code or observed not in (
                    f"{identifier} {self.owner} true",
                    f"{identifier} {self.owner} false",
                ):
                    raise ValueError("Container is not owned by this run")
                if observed.endswith(" true"):
                    self._call(["stop", "--timeout", "1", identifier], deadline)
                self._call(
                    ["rm", "--force", identifier], deadline
                )  # No --volumes: caller data survives.
            if self._daemon(deadline) != self.daemon:
                raise ValueError("Docker daemon identity changed during cleanup")
            if self._listing(f"id={identifier}", deadline) or self._listing(label, deadline):
                result["readback"] = "owned_container_remains"
            else:
                result["removed"], result["readback"] = True, "removed"
        except (OSError, TimeoutError, ValueError, UnicodeError, subprocess.TimeoutExpired):
            result["readback"] = "cleanup_unconfirmed"
            result["container_ids"] = self.identifiers[:]
        return result


class DiagnosticFailure(Exception):
    """A precise layer failure with an operator action and bounded safe message."""

    def __init__(self, layer: str, code: str, message: str, next_step: str):
        """Retain only fixed, scoped diagnostic strings."""
        self.check = dict(
            layer=layer, status="fail", code=code, message=message, next_step=next_step
        )
        super().__init__(message)


class StdioClient:
    """One exclusively owned subprocess; protocol stdout and bounded private stderr."""

    def __init__(self, command: list[str], deadline: float):
        """Launch the explicitly supplied argv and own its protocol pipes."""
        self.deadline = deadline
        self._next_id = 0
        self._lines: queue.Queue = queue.Queue(maxsize=2)
        self._stopped = threading.Event()
        self.stderr_bytes = 0
        self._stderr = collections.deque(maxlen=16)
        self._cleanup = None
        self.environment = dict(os.environ)
        self.docker = None
        self.supervised = False
        arguments = _docker_arguments(command)
        if arguments is None and PLATFORM == "win32":
            raise _unsupported_launch()
        if shutil.which(command[0], path=self.environment.get("PATH")) is None:
            raise DiagnosticFailure(
                "client_launch",
                "launch_failed",
                "The configured executable is missing or cannot be executed.",
                "Check the executable path and local launch permissions.",
            )
        try:
            if arguments is not None:
                self.docker = OwnedDocker(command, arguments, self.environment, deadline)
                command = self.docker.command
            elif PLATFORM.startswith("linux"):
                self.supervised = True
                command = [
                    sys.executable,
                    "-u",
                    str(Path(__file__).resolve()),
                    "--_supervise-native",
                    *command,
                ]
            self.process = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=self.environment,
                start_new_session=os.name == "posix",
            )
        except OSError as exc:
            if self.docker:
                try:
                    self.docker.directory.cleanup()
                except OSError:
                    pass
            raise DiagnosticFailure(
                "client_launch",
                "launch_failed",
                "The configured command could not be started.",
                "Check the executable, local permissions and Docker availability if using Docker.",
            ) from exc
        self._readers = [
            threading.Thread(target=read, daemon=True)
            for read in (self._read_stdout, self._read_stderr)
        ]
        for reader in self._readers:
            reader.start()

    def _enqueue(self, value):
        while not self._stopped.is_set():
            try:
                self._lines.put(value, timeout=0.1)
                return
            except queue.Full:
                pass

    def _read_stdout(self):
        assert self.process.stdout is not None
        while not self._stopped.is_set():
            line = self.process.stdout.readline(MAX_MESSAGE_BYTES + 1)
            if not line:
                self._enqueue(None)
                return
            self._enqueue(line)
            if len(line) > MAX_MESSAGE_BYTES:
                return

    def _read_stderr(self):
        assert self.process.stderr is not None
        while chunk := self.process.stderr.read(1024):
            self.stderr_bytes += len(chunk)
            self._stderr.append(chunk)  # Private, bounded; raw stderr is never exported.

    def request(self, method: str, parameters: dict, deadline: float | None = None) -> dict:
        """Exchange one literal JSON-RPC line within the remaining overall deadline."""
        deadline = min(self.deadline, self.deadline if deadline is None else deadline)
        if time.monotonic() >= deadline:
            raise DiagnosticFailure(
                "stdio",
                "response_timeout",
                "The diagnosis deadline expired before another request.",
                "Choose a sufficient bounded deadline and repeat the read-only diagnosis.",
            )
        self._next_id += 1
        identifier = self._next_id
        request = dict(jsonrpc="2.0", id=identifier, method=method, params=parameters)
        assert self.process.stdin is not None
        try:
            self.process.stdin.write(json.dumps(request).encode("utf-8") + b"\n")
            self.process.stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            raise DiagnosticFailure(
                "stdio",
                "server_exited",
                "The launched process stopped accepting MCP input.",
                "Check exit status and launch configuration; image existence is insufficient.",
            ) from exc
        remaining = deadline - time.monotonic()
        try:
            line = self._lines.get(timeout=max(0.0, remaining))
        except queue.Empty as exc:
            raise DiagnosticFailure(
                "stdio",
                "response_timeout",
                "No complete MCP response arrived within the diagnosis deadline.",
                "Check container startup, other clients, script loading and Live responsiveness.",
            ) from exc
        if line is None:
            raise DiagnosticFailure(
                "stdio",
                "server_exited",
                "The launched process closed protocol stdout.",
                "Check launch prerequisites, container exit status and local startup logs.",
            )
        try:
            if len(line) > MAX_MESSAGE_BYTES or not line.endswith(b"\n"):
                raise ValueError("message limit")
            response = json.loads(line.decode("utf-8"))
            if (
                not isinstance(response, dict)
                or response.get("jsonrpc") != "2.0"
                or type(response.get("id")) is not int
                or response.get("id") != identifier
                or "error" in response
                or not isinstance(response.get("result"), dict)
            ):
                raise ValueError("envelope")
        except (UnicodeError, ValueError) as exc:
            raise DiagnosticFailure(
                "stdio",
                "protocol_invalid",
                "Stdout did not contain the expected correlated MCP response.",
                "Use Sunny's stdio command without a TTY; stdout must contain protocol JSON only.",
            ) from exc
        return response["result"]

    def call(self, tool: str, arguments: dict, deadline: float | None = None) -> dict:
        """Call only an explicitly selected read-only diagnostic tool."""
        if tool not in ("doctor_ableton", "get_ableton_remote_log"):
            raise ValueError("Only read-only diagnostic tools are supported")
        result = self.request("tools/call", dict(name=tool, arguments=arguments), deadline)
        value = result.get("structuredContent")
        if value is None:
            try:
                value = json.loads(result["content"][0]["text"])
            except (KeyError, IndexError, TypeError, ValueError) as exc:
                raise DiagnosticFailure(
                    "mcp_tools",
                    "tool_response_invalid",
                    "The diagnostic tool returned no object result.",
                    "Check that this command launches the matching Sunny server.",
                ) from exc
        if not isinstance(value, dict):
            raise DiagnosticFailure(
                "mcp_tools",
                "tool_response_invalid",
                "The diagnostic result was not an object.",
                "Check the paired Sunny release and protocol logs.",
            )
        if (
            type(result.get("isError")) is not bool
            or type(value.get("success")) is not bool
            or result["isError"] == value["success"]
        ):
            raise DiagnosticFailure(
                "mcp_tools",
                "tool_response_invalid",
                "The diagnostic tool returned contradictory status fields.",
                "Check the paired Sunny release and tool result contract.",
            )
        return value

    def _group_empty(self):
        if os.name != "posix":
            return self.process.poll() is not None
        try:
            os.killpg(self.process.pid, 0)
        except ProcessLookupError:
            return True
        except PermissionError:
            return False
        return False

    def _end_process(self, deadline):
        """Stop the complete owned group, even when the original protocol parent exited."""
        try:
            self.process.wait(timeout=min(0.1, max(0, deadline - time.monotonic())))
        except subprocess.TimeoutExpired:
            pass
        if os.name == "posix":
            try:
                os.killpg(self.process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        elif self.process.poll() is None:
            self.process.terminate()  # Windows Docker CLI only; daemon cleanup is explicit.
        grace = min(deadline, time.monotonic() + 1.2)
        while time.monotonic() < grace:
            self.process.poll()
            if self._group_empty():
                return True
            time.sleep(min(0.02, max(0, grace - time.monotonic())))
        if os.name == "posix":
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        elif self.process.poll() is None:
            self.process.kill()
        while time.monotonic() < deadline:
            self.process.poll()
            if self._group_empty():
                return True
            time.sleep(min(0.02, max(0, deadline - time.monotonic())))
        return self._group_empty()

    def close(self, cleanup_seconds=DEFAULT_CLEANUP_SECONDS):
        """Stop/reap owned processes and remove this run's container within a separate budget."""
        if self._cleanup is not None:
            return self._cleanup
        started = time.monotonic()
        deadline = started + cleanup_seconds
        result = dict(
            success=False,
            budget_seconds=cleanup_seconds,
            elapsed_seconds=0.0,
            process_scope="owned_posix_session"
            if os.name == "posix"
            else "direct_windows_docker_cli",
            process_exited=False,
            process_group_empty=None,
            container=None,
        )
        self._stopped.set()
        if self.process.stdin:
            try:
                self.process.stdin.close()
            except OSError:
                pass
        if self.docker:
            result["container"] = self.docker.close(deadline - min(1.5, cleanup_seconds / 2))
        try:
            empty = self._end_process(deadline - 0.2)
        except OSError:
            empty = False
        if self.docker and not result["container"]["removed"]:
            # A cidfile write or daemon create may have been interrupted before
            # the CLI ended. Only this launch's unique label may reconcile it.
            result["container"] = self.docker.close(deadline - 0.2)
        result["process_exited"] = self.process.poll() is not None
        result["process_group_empty"] = empty if os.name == "posix" else None
        result["success"] = (
            empty
            and result["process_exited"]
            and not (self.supervised and self.process.returncode == 125)
            and (not self.docker or result["container"]["removed"])
        )
        for reader in self._readers:
            reader.join(timeout=min(0.1, max(0, deadline - time.monotonic())))
        if any(reader.is_alive() for reader in self._readers):
            result["success"] = False
        if self.docker:
            try:
                self.docker.directory.cleanup()
            except OSError:
                result["success"] = False
        result["elapsed_seconds"] = time.monotonic() - started
        self._cleanup = result
        return result


def _integer(value, minimum=0):
    return type(value) is int and minimum <= value <= 2147483647


def _timestamp(value):
    if type(value) not in (float, int):
        return False
    try:
        return math.isfinite(value) and value >= 0
    except OverflowError:
        return False


def validate_doctor(doctor: dict, request_id: str):
    """Reject extra/unbounded fields rather than exporting an arbitrary tool payload."""
    required = {
        "schema_version",
        "request_id",
        "success",
        "read_only_ready",
        "observed_at",
        "expected_bridge",
        "observed_bridge",
        "session",
        "native_state",
        "capabilities",
        "checks",
        "unverified",
    }
    if (
        set(doctor) not in (required, required | {"connection_failure"})
        or type(doctor["schema_version"]) is not int
        or doctor["schema_version"] != 1
        or doctor["request_id"] != request_id
        or not _timestamp(doctor["observed_at"])
        or type(doctor["success"]) is not bool
        or doctor["read_only_ready"] is not doctor["success"]
        or not isinstance(doctor["checks"], list)
        or not 1 <= len(doctor["checks"]) <= 5
        or not isinstance(doctor["unverified"], list)
        or len(doctor["unverified"]) != 5
        or any(not isinstance(value, str) or len(value) > 512 for value in doctor["unverified"])
        or (
            "connection_failure" in doctor
            and (
                not isinstance(doctor["connection_failure"], str)
                or len(doctor["connection_failure"]) > 512
            )
        )
    ):
        raise ValueError("Doctor schema")
    for key in ("expected_bridge", "observed_bridge"):
        value = doctor[key]
        if value is None and key == "observed_bridge":
            continue
        if (
            not isinstance(value, dict)
            or set(value) != {"protocol_version", "source_sha256"}
            or type(value["protocol_version"]) is not int
            or not 1 <= value["protocol_version"] <= 4294967295
            or not isinstance(value["source_sha256"], str)
            or not HEX64.fullmatch(value["source_sha256"])
        ):
            raise ValueError("Doctor bridge identity")
    for check in doctor["checks"]:
        if (
            not isinstance(check, dict)
            or set(check) != {"layer", "status", "code", "message", "next_step"}
            or any(not isinstance(value, str) or len(value) > 512 for value in check.values())
            or check["status"] not in ("pass", "fail")
        ):
            raise ValueError("Doctor checks")
    session = doctor["session"]
    if session is not None and (
        not isinstance(session, dict)
        or set(session) != {"schema_version", "bridge_instance", "document_token"}
        or type(session["schema_version"]) is not int
        or session["schema_version"] != 1
        or any(
            not isinstance(session[key], str) or not HEX32.fullmatch(session[key])
            for key in ("bridge_instance", "document_token")
        )
    ):
        raise ValueError("Doctor session")
    state = doctor["native_state"]
    if state is not None and (
        not isinstance(state, dict)
        or set(state) != {"is_playing", "session_record", "record_mode"}
        or any(type(value) is not bool for value in state.values())
    ):
        raise ValueError("Doctor state")
    capabilities = doctor["capabilities"]
    if capabilities is not None:
        if (
            not isinstance(capabilities, dict)
            or set(capabilities)
            != {"basis", "reported", "live_version", "managed_authoring_version_eligible"}
            or capabilities["basis"] != "version_floor_claims_not_host_qualification"
            or type(capabilities["managed_authoring_version_eligible"]) is not bool
        ):
            raise ValueError("Doctor capabilities")
        version, reported = capabilities["live_version"], capabilities["reported"]
        if (
            not isinstance(version, dict)
            or set(version) != {"major", "minor", "bugfix"}
            or any(type(value) is not int or not 0 <= value <= 65535 for value in version.values())
            or not isinstance(reported, dict)
            or set(reported)
            != {
                "clip_add_new_notes",
                "track_insert_device_native",
                "automation_envelope_authoring",
                "group_track_creation",
                "arbitrary_browser_loading",
                "structural_snapshot",
                "max_for_live",
            }
            or any(
                value not in ("available", "unavailable", "unknown") for value in reported.values()
            )
        ):
            raise ValueError("Doctor version/capability claims")
    failed = any(check["status"] == "fail" for check in doctor["checks"])
    if failed == doctor["success"] or (
        doctor["success"]
        and (
            doctor["observed_bridge"] != doctor["expected_bridge"]
            or any(doctor[key] is None for key in ("session", "native_state", "capabilities"))
            or {check["layer"] for check in doctor["checks"]}
            != {"bridge_connection", "release_pairing", "native_session", "native_readiness"}
        )
    ):
        raise ValueError("Doctor evidence/status contradiction")


def validate_page(page: dict, sequence: int, stream: str | None):
    """Check native pagination against the caller's cursor, including reset and gaps."""
    required = {
        "success",
        "entries",
        "next_sequence",
        "latest_sequence",
        "oldest_sequence",
        "stream_id",
        "reset",
        "truncated",
        "observed_at",
        "has_more",
    }
    if (
        set(page) != required
        or page["success"] is not True
        or not isinstance(page["entries"], list)
        or len(page["entries"]) > MAX_RECORDS
        or not isinstance(page["stream_id"], str)
        or not HEX32.fullmatch(page["stream_id"])
        or not _timestamp(page["observed_at"])
        or any(type(page[key]) is not bool for key in ("reset", "truncated", "has_more"))
        or any(not _integer(page[key]) for key in ("next_sequence", "latest_sequence"))
        or type(page["oldest_sequence"]) is not int
        or not 1 <= page["oldest_sequence"] <= page["latest_sequence"] + 1
    ):
        raise ValueError("Malformed or legacy log page; current cursor evidence is unavailable")
    reset = (stream is not None and stream != page["stream_id"]) or sequence > page[
        "latest_sequence"
    ]
    after = 0 if reset else sequence
    if page["reset"] != reset or page["truncated"] != (page["oldest_sequence"] > after + 1):
        raise ValueError("Contradictory restart or gap evidence")
    expected = max(after + 1, page["oldest_sequence"])
    for record in page["entries"]:
        if (
            not isinstance(record, dict)
            or set(record) != {"sequence", "time", "level", "source", "message"}
            or not _integer(record["sequence"], 1)
            or record["sequence"] != expected
            or expected > page["latest_sequence"]
            or not _timestamp(record["time"])
            or any(not isinstance(record[key], str) for key in ("level", "source", "message"))
            or len(record["message"]) > 2000
            or any(len(record[key]) > 128 for key in ("source", "level"))
        ):
            raise ValueError("Malformed or unordered log record")
        expected += 1
    next_sequence = expected - 1 if page["entries"] else after
    if (
        page["next_sequence"] != next_sequence
        or page["has_more"] != (next_sequence < page["latest_sequence"])
        or (not page["entries"] and page["has_more"])
    ):
        raise ValueError("Log cursor cannot make the claimed progress")


def redact(text: str) -> str:
    """Redact known credential assignments, authorization tokens and URL credentials."""
    text = re.sub(
        r'(?i)("(?:password|passwd|token|secret|api[_-]?key|authorization)"\s*:\s*)'
        r'"(?:[^"\\]|\\.)*"',
        r'\1"[REDACTED]"',
        text,
    )
    text = re.sub(r"(?i)(?:https?|ssh)://[^\s/@:]+:[^\s/@]+@", "[REDACTED]@", text)
    text = re.sub(r"(?i)\b(?:bearer|basic)\s+[^\s,;]+", "[REDACTED]", text)
    text = re.sub(
        r"(?i)\b(?:password|passwd|token|secret|api[_-]?key|authorization)\s*[:=]\s*[^\s,;]+",
        "[REDACTED]",
        text,
    )
    text = re.sub(r"\b(?:sk-[A-Za-z0-9_-]{8,}|gh[pousr]_[A-Za-z0-9]{8,})\b", "[REDACTED]", text)
    return text


def poll_logs(client, end: float, interval: float, include_messages: bool) -> dict:
    """Observe a finite stream with cursors/backoff; preserve every loss indication."""
    result = dict(
        requested=True,
        stale=True,
        incomplete=False,
        resets=0,
        gaps=0,
        requests=0,
        records_seen=0,
        records=[],
        cursor=None,
        last_received_at=None,
        last_observed_at=None,
        failures=[],
        omitted_records=0,
    )
    sequence, stream, delay, more = 0, None, interval, False
    retained = collections.deque(maxlen=MAX_RECORDS)
    while time.monotonic() < end and result["requests"] < 1000:
        result["requests"] += 1
        arguments = dict(after_sequence=sequence)
        if stream is not None:
            arguments["stream_id"] = stream
        try:
            page = client.call("get_ableton_remote_log", arguments, end)
            validate_page(page, sequence, stream)
            result["stale"] = False
            result["last_received_at"] = time.time()
            result["last_observed_at"] = page["observed_at"]
            result["resets"] += int(page["reset"])
            result["gaps"] += int(page["truncated"])
            result["incomplete"] |= page["reset"] or page["truncated"]
            result["records_seen"] += len(page["entries"])
            for entry in page["entries"]:
                row = {
                    key: redact(entry[key]) if isinstance(entry[key], str) else entry[key]
                    for key in ("sequence", "time", "level", "source")
                }
                row["stream_id"] = page["stream_id"]
                if include_messages:
                    row["message"] = redact(entry["message"])
                retained.append(row)
            sequence, stream = page["next_sequence"], page["stream_id"]
            result["cursor"] = dict(after_sequence=sequence, stream_id=stream)
            delay = interval
            more = page["has_more"]
            if more:
                continue
        except DiagnosticFailure as exc:
            result["failures"].append(exc.check)
            result["stale"] = result["incomplete"] = True
            break  # Correlation is uncertain after a protocol timeout; never overlap requests.
        except ValueError:
            result["failures"] = [
                {
                    "layer": "native_logs",
                    "status": "fail",
                    "code": "log_page_unavailable",
                    "message": "A current validated log page is unavailable.",
                    "next_step": "Check bridge pairing; legacy pages lack freshness evidence.",
                }
            ]
            result["stale"] = result["incomplete"] = True
            delay = min(delay * 2, 5.0)
        time.sleep(min(delay, max(0.0, end - time.monotonic())))
    result["records"] = list(retained)
    result["omitted_records"] = max(0, result["records_seen"] - len(retained))
    result["incomplete"] |= (
        result["omitted_records"] > 0 or result["requests"] >= 1000 or more or result["stale"]
    )
    return result


def diagnose(
    command: list[str],
    timeout: float,
    poll_seconds=0.0,
    interval=0.25,
    include_messages=False,
    cleanup_seconds=DEFAULT_CLEANUP_SECONDS,
) -> dict:
    """Run the actual stdio lifecycle, then minimal native diagnosis and optional polling."""
    report = dict(
        schema_version=1,
        request_id=uuid.uuid4().hex,
        observed_at=time.time(),
        success=False,
        read_only_ready=False,
        checks=[],
        doctor=None,
        logs=None,
        cleanup=None,
        launcher="docker" if Path(command[0]).name in ("docker", "docker.exe") else "process",
        unknowns=[
            "Independent native host-log retrieval is pending approved setup.",
            "Audio, interactive sessions, licensed devices and musical mutations remain unknown.",
        ],
    )
    deadline = time.monotonic() + timeout
    client = None
    try:
        client = StdioClient(command, deadline)
        report["checks"].append(
            dict(
                layer="client_launch",
                status="pass",
                code="process_spawned",
                message="The selected subprocess started; application readiness is separate.",
                next_step="",
            )
        )
        initialized = client.request(
            "initialize",
            dict(
                protocolVersion="2025-11-25",
                capabilities={},
                clientInfo=dict(name="sunny-doctor", version="1"),
            ),
        )
        if initialized.get("protocolVersion") != "2025-11-25":
            raise DiagnosticFailure(
                "stdio",
                "protocol_version_mismatch",
                "MCP version negotiation failed.",
                "Launch the supported Sunny release with newline-delimited stdio.",
            )
        assert client.process.stdin is not None
        try:
            client.process.stdin.write(b'{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
            client.process.stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            raise DiagnosticFailure(
                "stdio",
                "server_exited",
                "The process exited during MCP initialization.",
                "Check local/container startup logs and launch prerequisites.",
            ) from exc
        if client.request("ping", {}) != {}:
            raise DiagnosticFailure(
                "stdio",
                "ping_invalid",
                "The protocol ping returned an unexpected payload.",
                "Check that stdout belongs to the Sunny MCP server.",
            )
        report["checks"].append(
            dict(
                layer="stdio",
                status="pass",
                code="protocol_exchange_passed",
                message="Correlated initialize and ping responses arrived over stdio.",
                next_step="",
            )
        )
        inventory = client.request("tools/list", {}).get("tools")
        if (
            not isinstance(inventory, list)
            or any(
                not isinstance(tool, dict) or not isinstance(tool.get("name"), str)
                for tool in inventory
            )
            or len({tool["name"] for tool in inventory}) != len(inventory)
            or not {"doctor_ableton", "get_ableton_remote_log"}
            <= {tool["name"] for tool in inventory}
        ):
            raise DiagnosticFailure(
                "mcp_tools",
                "diagnostic_tools_missing",
                "The required diagnostic tools are absent or malformed.",
                "Select a Sunny release with the doctor and paired log cursor contract.",
            )
        report["tool_count"] = len(inventory)
        report["checks"].append(
            dict(
                layer="mcp_tools",
                status="pass",
                code="diagnostic_tools_available",
                message="The launched server advertises both read-only diagnostic tools.",
                next_step="",
            )
        )
        doctor = client.call("doctor_ableton", dict(request_id=report["request_id"]))
        try:
            validate_doctor(doctor, report["request_id"])
        except ValueError as exc:
            raise DiagnosticFailure(
                "native_doctor",
                "doctor_response_invalid",
                "Native diagnosis returned an invalid correlated result.",
                "Verify the selected server and diagnostic tool contract.",
            ) from exc
        report["doctor"] = doctor
        report["success"] = doctor["success"]
        report["read_only_ready"] = doctor["read_only_ready"]
        if poll_seconds:
            report["logs"] = poll_logs(
                client, min(deadline, time.monotonic() + poll_seconds), interval, include_messages
            )
            if report["logs"]["stale"] or report["logs"]["incomplete"]:
                report["success"] = False
    except DiagnosticFailure as exc:
        report["checks"].append(exc.check)
    finally:
        if client is not None:
            report["cleanup"] = client.close(cleanup_seconds)
            if not report["cleanup"]["success"]:
                report["success"] = False
                report["checks"].append(
                    dict(
                        layer="cleanup",
                        status="fail",
                        code="owned_cleanup_unconfirmed",
                        message="Cleanup of every supported owned resource could not be confirmed.",
                        next_step="Use its reported owner label/ID for scoped recovery.",
                    )
                )
            report["process_exit_code"] = client.process.returncode
            report["stderr_bytes"] = client.stderr_bytes
        report["observed_at"] = time.time()
    return report


def export_report(path: Path, report: dict):
    """Write an exclusive deliberate export, redacted and capped at one MiB."""

    def safe(value):
        if isinstance(value, str):
            return redact(value)
        if isinstance(value, dict):
            return {key: safe(child) for key, child in value.items()}
        if isinstance(value, list):
            return [safe(child) for child in value]
        return value

    value = safe(report)
    body = json.dumps(value, ensure_ascii=True, allow_nan=False, indent=2).encode("utf-8")
    if len(body) + 1 > MAX_EXPORT_BYTES and value.get("logs"):
        # Preserve summary/cursors/loss facts; mark deliberately omitted detail.
        logs = value["logs"]
        logs["export_records_omitted"] = len(logs["records"])
        logs["records"] = []
        logs["incomplete"] = True
        value["success"] = False
        body = json.dumps(value, ensure_ascii=True, allow_nan=False, indent=2).encode("utf-8")
    if len(body) + 1 > MAX_EXPORT_BYTES:
        raise ValueError("Diagnostic export exceeds one MiB; nothing was written")
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "wb") as output:
        output.write(body + b"\n")


def main(argv=None):
    """Require an explicit launch command and bounded read-only diagnostic duration."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument(
        "--cleanup-timeout",
        type=float,
        default=DEFAULT_CLEANUP_SECONDS,
        help="Separate cleanup budget [2,30] seconds after diagnosis ends.",
    )
    parser.add_argument("--poll-seconds", type=float, default=0.0)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument(
        "--include-log-messages",
        action="store_true",
        help="Include redacted native messages; project content may still be present.",
    )
    parser.add_argument(
        "--export", type=Path, help="Deliberate JSON export; refuses to overwrite an existing file."
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="After --, the native server or docker run -i command; no shell or TTY.",
    )
    arguments = parser.parse_args(argv)
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if (
        not command
        or not math.isfinite(arguments.timeout)
        or not 0 < arguments.timeout <= 120
        or not math.isfinite(arguments.poll_seconds)
        or not 0 <= arguments.poll_seconds <= 60
        or not math.isfinite(arguments.interval)
        or not 0.05 <= arguments.interval <= 5
        or not math.isfinite(arguments.cleanup_timeout)
        or not 2 <= arguments.cleanup_timeout <= 30
    ):
        parser.error(
            "Supply a command, timeout (0,120], cleanup [2,30], polling [0,60], "
            "and interval [0.05,5]"
        )
    report = diagnose(
        command,
        arguments.timeout,
        arguments.poll_seconds,
        arguments.interval,
        arguments.include_log_messages,
        arguments.cleanup_timeout,
    )
    if arguments.export:
        try:
            export_report(arguments.export, report)
        except (OSError, ValueError) as exc:
            parser.error(str(exc))
    print(json.dumps(report, ensure_ascii=True, allow_nan=False))
    return 0 if report["success"] else 1


if __name__ == "__main__":
    if sys.argv[1:2] == ["--_supervise-native"] and PLATFORM.startswith("linux"):
        sys.exit(_supervise_native(sys.argv[2:]))
    sys.exit(main())
