"""Host qualification I/O; no Live imports or mutation retry."""

import hashlib
import ipaddress
import json
import math
import os
import re
import select
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path
from types import ModuleType

# A release inventory remains unchanged when these developer helpers run.
sys.dont_write_bytecode = True

MAX_FRAME = 16 * 1024 * 1024
MAX_MCP_REQUEST = 4 * 1024 * 1024


def remaining(deadline):
    """Use one absolute budget across name resolution, connect, send and receive."""
    value = deadline - time.monotonic()
    if value <= 0:
        raise TimeoutError("Qualification deadline expired; retain original tokens, do not replay")
    return value


def seconds(value, maximum):
    """Reject nonfinite or unsupported caller budgets before starting a child or socket."""
    if type(value) not in (int, float) or not math.isfinite(value) or not 0 < value <= maximum:
        raise ValueError("Qualification budget must be finite and within " + str(maximum))
    return float(value)


def addresses(host, port, deadline):
    """Resolve IPv4 in an owned, killable stdlib child; numeric loopback needs no resolver."""
    if type(host) is not str or not host or len(host) > 253 or any(c.isspace() for c in host):
        raise ValueError("Supply a bounded IPv4 address or DNS name")
    if "\x00" in host or type(port) is not int or not 1 <= port <= 65535:
        raise ValueError("Supply a typed TCP endpoint")
    try:
        literal = ipaddress.IPv4Address(host)
    except ipaddress.AddressValueError:
        literal = None
    if literal is not None:
        return [(str(literal), port)]
    code = (
        "import json,socket,sys; rows=socket.getaddrinfo(sys.argv[1],int(sys.argv[2]),"
        "socket.AF_INET,socket.SOCK_STREAM); "
        "print(json.dumps([row[4][0] for row in rows[:16]]))"
    )
    child = subprocess.Popen(
        [sys.executable, "-I", "-c", code, host, str(port)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    try:
        output, _ = child.communicate(timeout=remaining(deadline))
        if child.returncode != 0 or len(output) > 4096:
            raise ValueError("Qualification name resolution failed")
        values = json.loads(output)
        if type(values) is not list or not 1 <= len(values) <= 16:
            raise ValueError("Qualification name resolution returned no bounded addresses")
        return [(str(ipaddress.IPv4Address(value)), port) for value in values]
    finally:
        if child.poll() is None:
            child.kill()
            child.communicate(timeout=1)


def record(path, kind, **fields):
    """Append one deliberate timestamped qualification observation."""
    row = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "kind": kind, **fields}
    with Path(path).open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(row, ensure_ascii=True, allow_nan=False) + "\n")
    return row


def exact(sock, size, deadline=None):
    """Read precisely one bounded frame part; never replay an uncertain request."""
    data = bytearray()
    while len(data) < size:
        if deadline is not None:
            sock.settimeout(remaining(deadline))
        part = sock.recv(size - len(data))
        if not part:
            raise EOFError("Connection closed; outcome may be uncertain. Do not replay a mutation.")
        data.extend(part)
    return bytes(data)


def recv_frame(sock, deadline=None):
    """Read the literal production framing with its 16 MiB payload limit."""
    size = struct.unpack(">I", exact(sock, 4, deadline))[0]
    if not 0 < size <= MAX_FRAME:
        raise ValueError("Response exceeds the production 16 MiB frame boundary")
    return exact(sock, size, deadline)


def send_frame(sock, value, deadline=None):
    """Send one bounded JSON frame without mutation retry."""
    body = json.dumps(value, allow_nan=False).encode("utf-8")
    send_bytes_frame(sock, body, deadline)


def send_bytes_frame(sock, body, deadline=None):
    """Forward exact observed frame bytes without changing their JSON representation."""
    if not isinstance(body, bytes):
        raise ValueError("Literal frame payload must be bytes")
    if not 0 < len(body) <= MAX_FRAME:
        raise ValueError("Request exceeds the production frame boundary")
    if deadline is not None:
        sock.settimeout(remaining(deadline))
    sock.sendall(struct.pack(">I", len(body)) + body)


def rpc(host, port, request, log, timeout=35.0):
    """Perform one request and retain the actual timed wire response."""
    start = time.monotonic()
    deadline = start + seconds(timeout, 120)
    record(log, "tcp_request", host=host, port=port, request=request)
    sock = None
    try:
        failure = None
        for address in addresses(host, port, deadline):
            candidate = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            try:
                candidate.settimeout(remaining(deadline))
                candidate.connect(address)
            except OSError as error:
                candidate.close()
                failure = error
                continue
            sock = candidate
            break
        if sock is None:
            raise failure or ConnectionError("No qualification endpoint connected")
        # Trying another resolved address is permitted only before connection;
        # once any frame bytes may have escaped there is no automatic retry.
        send_frame(sock, request, deadline)
        body = recv_frame(sock, deadline)
    finally:
        if sock is not None:
            sock.close()
    value = json.loads(body)
    record(
        log,
        "tcp_response",
        response=value,
        payload_bytes=len(body),
        elapsed=time.monotonic() - start,
    )
    return value


def bridge_rpc(configuration, request, log, timeout=35.0):
    """Require an exact production envelope and a successful versioned response."""
    version = configuration["expected_bridge_protocol_version"]
    if request.get("bridge_protocol_version") != version:
        raise ValueError("Request protocol differs from the configured production release")
    response = rpc(
        configuration["bridge_host"], configuration["bridge_port"], request, log, timeout
    )
    if response.get("bridge_protocol_version") != version or response.get("success") is not True:
        raise RuntimeError(
            {"response": response, "action": "Preserve evidence; do not repeat a mutation."}
        )
    return response


def approved_origin(configuration):
    """Read the operator's retained original pair; observations cannot replace approval."""
    pair = {
        "bridge_instance": configuration.get("scratch_bridge_instance"),
        "document_token": configuration.get("scratch_document_token"),
    }
    for value in pair.values():
        if (
            not isinstance(value, str)
            or len(value) != 32
            or any(c not in "0123456789abcdef" for c in value)
        ):
            raise ValueError("Record the approved original scratch Bridge/Document tokens")
    return pair


def verify_selected_release(configuration):
    """Verify the immutable full release before native qualification launches or effects."""
    directory = configuration.get("release_directory")
    if type(directory) is not str or not directory:
        raise ValueError("Select the complete verified release before qualification launches")
    root = Path(directory)
    checksum = configuration.get("expected_release_manifest_sha256")
    if (
        not isinstance(checksum, str)
        or len(checksum) != 64
        or any(c not in "0123456789abcdef" for c in checksum)
    ):
        raise ValueError("Supply the separately retained release manifest checksum")
    manifest = root / "release.json"
    if manifest.is_symlink() or not manifest.is_file() or manifest.stat().st_size > 1024 * 1024:
        raise ValueError("Supply a physical bounded paired release manifest")
    data = manifest.read_bytes()
    if hashlib.sha256(data).hexdigest() != checksum:
        raise ValueError("Selected release differs from the separately retained manifest checksum")
    identity = json.loads(data)
    helper = Path(__file__).resolve().parent / "verify_artifacts.py"
    verifier_bytes = None
    for name, path in (("common.py", Path(__file__)), ("verify_artifacts.py", helper)):
        relative = "operator/live_qualification/" + name
        rows = [row for row in identity["files"] if row.get("path") == relative]
        if (
            len(rows) != 1
            or set(rows[0]) != {"path", "bytes", "sha256"}
            or type(rows[0]["bytes"]) is not int
            or not 0 < rows[0]["bytes"] <= 1024 * 1024
            or path.is_symlink()
            or not path.is_file()
            or path.resolve() != (root / relative).resolve()
        ):
            raise ValueError("Run the exact image-owned qualification helper from this release")
        source = path.read_bytes()
        if (
            len(source) != rows[0]["bytes"]
            or hashlib.sha256(source).hexdigest() != rows[0]["sha256"]
        ):
            raise ValueError("Qualification helper differs from the trusted release")
        if name == "verify_artifacts.py":
            verifier_bytes = source
    module = ModuleType("sunny_qualification_verifier")
    module.__file__ = str(helper)
    exec(compile(verifier_bytes, str(helper), "exec"), module.__dict__)
    verified = module.verify_paired_release(root, checksum)
    contract = verified["bridge"]["contract"]
    if (
        type(configuration.get("configuration_schema_version")) is not int
        or configuration.get("configuration_schema_version")
        != verified["configuration_schema_version"]
        or type(configuration["expected_bridge_protocol_version"]) is not int
        or type(configuration["expected_target_snapshot_schema_version"]) is not int
        or configuration["expected_bridge_source_sha256"] != verified["bridge"]["source_sha256"]
        or configuration["expected_bridge_protocol_version"] != contract["bridge_protocol_version"]
        or configuration["expected_target_snapshot_schema_version"]
        != contract["target_snapshot_schema_version"]
        or type(configuration.get("image_local_immutable_id")) is not str
        or configuration.get("image_local_immutable_id")
        not in {
            verified["image"][name]
            for name in (
                "local_immutable_id",
                "oci_index_digest",
                "oci_manifest_digest",
                "oci_config_digest",
            )
        }
        or (
            configuration.get("expected_commit") is not None
            and configuration["expected_commit"] != verified["source_revision"]
        )
    ):
        raise ValueError("External qualification configuration differs from the verified release")
    return verified


def require_primary_context(configuration, observed):
    """Require the probe and primary surface to observe the same approved native Song."""
    expected = {"schema_version": 1, **approved_origin(configuration)}
    context = observed.get("primary_context")
    if (
        not isinstance(context, dict)
        or set(context) != set(expected)
        or type(context.get("schema_version")) is not int
        or context != expected
        or observed.get("primary_context_error") is not None
    ):
        raise RuntimeError("Scratch probe/primary surface identity differs from original approval")
    return expected


def prime_native_session(client, configuration):
    """Use the actual read-only doctor to arm future admissions for the approved pair."""
    report = client.call("doctor_ableton")
    session = report.get("session")
    expected = {
        "protocol_version": configuration["expected_bridge_protocol_version"],
        "source_sha256": configuration["expected_bridge_source_sha256"],
    }
    if (
        report.get("success") is not True
        or report.get("read_only_ready") is not True
        or not isinstance(session, dict)
        or type(session.get("schema_version")) is not int
        or session != {"schema_version": 1, **approved_origin(configuration)}
        or report.get("observed_bridge") != expected
        or report.get("expected_bridge") != expected
    ):
        raise RuntimeError("Read-only doctor did not establish the originally approved session")
    return report


def legacy_call(client, configuration, command):
    """Send one closed gateway command with original authority; retain its full receipt."""
    if not isinstance(command, dict) or set(command) != {"type", "path", "name", "args"}:
        raise ValueError("A legacy command requires exactly type/path/name/args")
    result = client.call(
        "legacy_ableton_request", command=command, **approved_origin(configuration)
    )
    if result.get("success") is not True:
        raise RuntimeError(
            {
                "result": result,
                "action": "Preserve the full receipt; query original tokens; no replay",
            }
        )
    return result


def qualification_command(configuration, image):
    """Admit one image-owned final launch with explicit configuration and data mounts."""
    command = configuration.get("mcp_command")
    if (
        not isinstance(command, list)
        or not 9 <= len(command) <= 64
        or not all(
            type(value) is str and value and len(value) <= 4096 and "\x00" not in value
            for value in command
        )
        or Path(command[0]).name not in ("docker", "docker.exe")
        or command[1] != "run"
        or not re.fullmatch(r"sha256:[0-9a-f]{64}", image or "")
        or configuration.get("server_mount_prefix") != "/data"
        or type(configuration.get("host_mount_directory")) is not str
        or not configuration["host_mount_directory"]
    ):
        raise ValueError("Final qualification requires the explicit paired Docker launch profile")
    flags, mounts, labels = set(), {}, set()
    index = 2

    def unique(name):
        if name in flags:
            raise ValueError("Duplicate qualification Docker option: " + name)
        flags.add(name)

    while index < len(command):
        argument = command[index]
        if not argument.startswith("-"):
            break
        name, equal, value = argument.partition("=")
        if argument in ("-i", "--interactive", "--rm"):
            unique("--interactive" if argument == "-i" else argument)
            index += 1
            continue
        if name not in {
            "--pull",
            "--mount",
            "--env",
            "-e",
            "--name",
            "--label",
            "--platform",
            "--add-host",
            "--network",
        }:
            raise ValueError("Unsupported final qualification Docker option: " + name)
        if not equal:
            index += 1
            if index >= len(command):
                raise ValueError("Missing qualification Docker option value")
            value = command[index]
        if not value:
            raise ValueError("Empty qualification Docker option value")
        if name == "--mount":
            fields = {}
            for item in value.split(","):
                key, separator, field = item.partition("=")
                if key in fields or key not in {"type", "source", "target", "readonly"}:
                    raise ValueError("Malformed or duplicate qualification mount field")
                if key == "readonly":
                    if separator and field != "true":
                        raise ValueError("Configuration mount must be readonly")
                    fields[key] = True
                else:
                    if not separator or not field:
                        raise ValueError("Missing qualification mount field")
                    fields[key] = field
            target = fields.get("target")
            if target in mounts or fields.get("type") != "bind":
                raise ValueError("Duplicate or unsupported qualification mount")
            if target == "/run/sunny/configuration.json":
                if set(fields) != {"type", "source", "target", "readonly"}:
                    raise ValueError("Production JSON mount must be explicitly readonly")
            elif target == "/data":
                if (
                    set(fields) != {"type", "source", "target"}
                    or fields["source"] != configuration["host_mount_directory"]
                ):
                    raise ValueError(
                        "Data mount differs from the configured writable host directory"
                    )
            else:
                raise ValueError(
                    "Qualification mount cannot replace image-owned executable or operator files"
                )
            mounts[target] = fields
        elif name == "--label":
            key, separator, field = value.partition("=")
            if (
                not separator
                or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.:/-]{0,127}", key)
                or len(field) > 256
                or key in labels
                or len(labels) >= 8
                or key == "com.sunny.doctor.owner"
            ):
                raise ValueError("Invalid or duplicate qualification label")
            labels.add(key)
        else:
            name = "--env" if name == "-e" else name
            unique(name)
            if (
                (name == "--pull" and value != "never")
                or (name == "--env" and value != "SUNNY_CONFIG_PATH=/run/sunny/configuration.json")
                or (name == "--platform" and value != "linux/amd64")
                or (name == "--add-host" and value != "host.docker.internal:host-gateway")
                or (name == "--network" and value not in {"default", "bridge"})
                or (
                    name == "--name" and not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,63}", value)
                )
            ):
                raise ValueError(
                    "Qualification option differs from the supported production profile"
                )
        index += 1
    if (
        index != len(command) - 1
        or command[index] != image
        or not {"--interactive", "--rm", "--pull", "--env"} <= flags
        or set(mounts) != {"/data", "/run/sunny/configuration.json"}
    ):
        raise ValueError(
            "Qualification requires the exact immutable image without a container command override"
        )
    return command


class Mcp:
    """One process, one request at a time, no retry or synthesized successful evidence."""

    def __init__(self, configuration, log):
        """Initialize the single process or native probe without granting mutation authority."""
        self.log = log
        self.config = configuration
        verify_selected_release(configuration)
        qualification_command(configuration, configuration["image_local_immutable_id"])
        if os.name != "posix":
            raise ValueError(
                "The developer qualification kit requires POSIX Python; "
                "use SunnyDoctor.ps1 for Windows operations"
            )
        if any(
            key in os.environ
            for key in (
                "SUNNY_ABLETON_HOST",
                "SUNNY_TCP_PORT",
                "SUNNY_BIND_HOST",
                "SUNNY_WORKSPACE_PATH",
                "SUNNY_WORKSPACE_RECOVERY",
            )
        ):
            raise ValueError(
                "Qualification requires explicit production JSON "
                "without legacy environment selectors"
            )
        self.timeout = seconds(configuration.get("mcp_timeout", 120), 120)
        self.deadline = time.monotonic() + seconds(
            configuration.get("mcp_session_timeout", 1800), 3600
        )
        self._closed = False
        # Reuse the maintained bounded reader and ownership/cleanup mechanism.
        # This helper launches no SSH connection and changes no trust settings.
        source = Path(__file__).resolve().parent.parent / "doctor.py"
        module = ModuleType("sunny_qualification_stdio")
        module.__file__ = str(source)
        exec(compile(source.read_bytes(), str(source), "exec"), module.__dict__)
        self.client = module.StdioClient(configuration["mcp_command"], self.deadline)
        self.process = self.client.process
        self.writer = BoundedInput(self.process.stdin)
        self.process.stdin = self.writer
        try:
            result = self.request(
                "initialize",
                {
                    "protocolVersion": "2025-11-25",
                    "capabilities": {},
                    "clientInfo": {"name": "SunnyFinalHostProbe", "version": "2"},
                },
            )
            if result.get("protocolVersion") != "2025-11-25":
                raise ValueError("Qualification server did not negotiate the selected MCP version")
            self.writer.deadline = min(self.deadline, time.monotonic() + self.timeout)
            self.writer.write(b'{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
        except BaseException:
            self.close()
            raise

    def request(self, method, params):
        """Send one serial MCP request and verify its matching response identity."""
        identifier = self.client._next_id + 1
        request = {"jsonrpc": "2.0", "id": identifier, "method": method, "params": params}
        record(self.log, "mcp_request", request=request)
        start = time.monotonic()
        self.writer.deadline = min(self.deadline, start + self.timeout)
        self.writer.last_bytes = 0
        try:
            response = self.client.request(method, params, self.writer.deadline)
        except BaseException as error:
            record(
                self.log,
                "mcp_response_unavailable",
                request_id=identifier,
                bytes_written_to_owned_pipe=self.writer.last_bytes,
                action="Retain original native tokens; pipe delivery is not native effect evidence",
            )
            if isinstance(error.__cause__, TimeoutError):
                raise TimeoutError(
                    "Qualification stdin budget expired; preserve original tokens"
                ) from error
            raise
        record(
            self.log,
            "mcp_response",
            request_id=identifier,
            response_result=response,
            elapsed=time.monotonic() - start,
        )
        return response

    def call(self, tool, **arguments):
        """Decode one tool result without inventing successful content."""
        result = self.request("tools/call", {"name": tool, "arguments": arguments})
        content = result.get("structuredContent")
        if content is None:
            content = json.loads(result["content"][0]["text"])
        return content

    def close(self):
        """Close MCP input and bound process cleanup after recording its output."""
        if self._closed:
            return
        self._closed = True
        cleanup = self.client.close()
        record(self.log, "mcp_owned_cleanup", cleanup=cleanup)
        if cleanup.get("success") is not True:
            raise RuntimeError(
                {
                    "cleanup": cleanup,
                    "action": "Preserve the workspace and owned-process evidence; "
                    "cleanup is unconfirmed",
                }
            )

    def __enter__(self):
        """Return this single-client MCP session."""
        return self

    def __exit__(self, *_):
        """Release the session after either successful or failed qualification."""
        self.close()


class BoundedInput:
    """Write one bounded request through an owned POSIX pipe within its absolute budget."""

    def __init__(self, original):
        """Borrow the existing pipe while retaining its original close ownership."""
        self.original = original
        self.deadline = time.monotonic()
        self.last_bytes = 0
        os.set_blocking(original.fileno(), False)

    def write(self, data):
        """Send only once; a partial write followed by timeout remains uncertain."""
        if not 0 < len(data) <= MAX_MCP_REQUEST:
            raise ValueError("Qualification request exceeds Sunny's 4 MiB MCP line boundary")
        fd = self.original.fileno()
        offset = 0
        self.last_bytes = 0
        while offset < len(data):
            wait = min(0.05, remaining(self.deadline))
            if not select.select([], [fd], [], wait)[1]:
                continue
            try:
                count = os.write(fd, data[offset : offset + 65536])
            except BlockingIOError:
                continue
            if count <= 0:
                raise BrokenPipeError("Qualification stdin closed; preserve the original request")
            offset += count
            self.last_bytes = offset
        return offset

    def flush(self):
        """All bytes were written directly; no buffered bytes remain."""

    def close(self):
        """Close the single owned descriptor without waiting on a blocked buffered writer."""
        self.original.close()
