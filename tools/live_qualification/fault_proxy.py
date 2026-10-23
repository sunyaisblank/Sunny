"""One controlled lost reply: actual host dispatch once, never mutation replay.

Run only for a separately saved disposable fault Set/workspace. It verifies the
actual product ledger fence immediately before forwarding each tokened mutation.
A matched response is logged in full but deliberately not forwarded. Subsequent
original-token journal queries pass normally. No Live API or permissive host model.
"""

import argparse
import hashlib
import ipaddress
import json
import os
import re
import signal
import socket
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path, PurePosixPath
from types import ModuleType

sys.dont_write_bytecode = True

from common import (  # noqa: E402 - preserve the release before peer imports.
    addresses,
    qualification_command,
    record,
    recv_frame,
    remaining,
    seconds,
    send_bytes_frame,
    verify_selected_release,
)

HEX32 = re.compile(r"[0-9a-f]{32}\Z")
MAX_OBSERVER_BYTES = 64 * 1024 * 1024 + 65536
OBSERVER_SECONDS = 10.0

# Runs with the selected image's default UID, isolated from networking and with
# only the already-admitted data bind mounted read-only. It returns one matching
# record, not permission to dispatch; require_fence checks the actual wire bytes.
LEDGER_OBSERVER = r"""
import json, pathlib, re, sys
LIMIT = 64 * 1024 * 1024
def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate field")
        result[key] = value
    return result
def load(path, limit):
    path = pathlib.Path(path)
    resolved = path.resolve(strict=True)
    resolved.relative_to(pathlib.Path('/data').resolve(strict=True))
    if not resolved.is_file() or resolved != path:
        raise ValueError("file is missing or redirected")
    with resolved.open('rb') as source:
        data = source.read(limit + 1)
    if len(data) > limit:
        raise ValueError("file limit")
    return json.loads(data, object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
workspace, operation, original = sys.argv[1:]
metadata = load(workspace, 1024 * 1024)['native_realization']
namespace = metadata['workspace_namespace']
base = metadata['history_base_directory']
if (type(namespace) is not str or not re.fullmatch('[0-9a-f]{32}', namespace)
    or (original and namespace != original) or type(base) is not str
    or not base.startswith('/data/') or str(pathlib.PurePosixPath(base)) != base
    or '..' in pathlib.PurePosixPath(base).parts):
    raise ValueError("original namespace/mount mismatch")
path = pathlib.PurePosixPath(base) / namespace / 'ledger.json'
ledger = load(path, LIMIT)
if (ledger.get('format') != 'sunny-realization-ledger'
    or type(ledger.get('schema_version')) is not int
    or ledger['schema_version'] not in (1, 2, 3)
    or ledger.get('workspace_namespace') != namespace
    or not isinstance(ledger.get('attempts'), list)):
    raise ValueError("ledger namespace/schema mismatch")
entries = [entry for entry in ledger['attempts']
           if entry['intent']['attempt_id'] == operation]
reply = dict(workspace_namespace=namespace, ledger=str(path), entries=entries)
data = json.dumps(reply, ensure_ascii=False, separators=(',', ':'), allow_nan=False).encode('utf-8')
if len(data) > LIMIT + 65536:
    raise ValueError("output limit")
sys.stdout.buffer.write(data)
"""

MANAGED_MUTATIONS = frozenset(
    {
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
    }
)


def mutation_token(request):
    """Validate the managed frame before even launching its disk observer."""
    if not isinstance(request, dict):
        raise RuntimeError("Native request is not an object; withheld")
    if (
        set(request) != {"bridge_protocol_version", "type", "path", "name", "args"}
        or type(request.get("bridge_protocol_version")) is not int
        or request.get("type") != "call"
        or request.get("path") != "song"
    ):
        raise RuntimeError("Managed mutator path/type differs; request withheld")
    args = request.get("args")
    if not isinstance(args, list) or len(args) != 1 or not isinstance(args[0], dict):
        raise RuntimeError("Managed mutation requires one closed payload; request withheld")
    token = args[0].get("operation_id")
    if not isinstance(token, str) or not HEX32.fullmatch(token):
        raise RuntimeError("Managed mutation lacks operation token; native request withheld")
    return token


def require_fence(request, entries):
    """Typed parsed immutable payload closure before forwarding any managed mutator."""
    if not isinstance(request, dict):
        raise RuntimeError("Native request is not an object; withheld")
    if request.get("name") not in MANAGED_MUTATIONS:
        return None  # Original-token read-only queries intentionally pass without a mutation fence.
    token = mutation_token(request)
    matches = [entry for entry in entries if entry["intent"]["attempt_id"] == token]
    if len(matches) != 1 or matches[0]["dispatch_state"] != "may_have_sent":
        raise RuntimeError("No exact durable pre-send fence: native request withheld")
    expected = matches[0]["intent"]["prepared"]["request"]
    if json.dumps(expected, sort_keys=True, allow_nan=False) != json.dumps(
        request, sort_keys=True, allow_nan=False
    ):
        raise RuntimeError(
            "Native request differs from exact immutable fenced method/path/args; withheld"
        )
    return matches[0]


def observer_output(command, deadline, environment, limit=MAX_OBSERVER_BYTES):
    """Capture finite observer stdout; kill/reap only this subprocess on failure."""
    remaining(deadline)
    process = subprocess.Popen(
        command,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        env=environment,
        start_new_session=os.name == "posix",
    )
    output = bytearray()
    failed = threading.Event()
    lingering_group = False

    def read():
        try:
            while chunk := process.stdout.read(65536):
                if len(output) + len(chunk) > limit:
                    failed.set()
                    return
                output.extend(chunk)
        except OSError:
            failed.set()

    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    try:
        while process.poll() is None:
            if failed.is_set():
                raise ValueError("Ledger observer output exceeded its bound")
            try:
                process.wait(timeout=min(0.05, remaining(deadline)))
            except subprocess.TimeoutExpired:
                continue
    finally:
        # A CLI wrapper may exit while a child still holds stdout. This group is
        # created exclusively for our launch and is cleaned even after parent exit.
        if os.name == "posix":
            try:
                if process.poll() is not None:
                    os.killpg(process.pid, 0)
                    lingering_group = True
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        elif process.poll() is None:
            process.kill()
        process.wait(timeout=0.5)
        reader.join(timeout=0.5)
        if not reader.is_alive():
            process.stdout.close()
    if reader.is_alive() or failed.is_set() or lingering_group or len(output) > limit:
        raise ValueError("Ledger observer output was not bounded")
    if time.monotonic() >= deadline:
        raise TimeoutError("Ledger observer exceeded its deadline")
    if process.returncode:
        raise RuntimeError("Ledger observer could not read the original private ledger")
    return bytes(output)


class LedgerObserver:
    """Authenticate the launch once; inspect private disk bytes without a writer RPC."""

    def __init__(self, config, log):
        """Verify the complete paired release and exact launch before any listener/child."""
        verified = verify_selected_release(config)
        self.image = config["image_local_immutable_id"]
        self.command = qualification_command(config, self.image)
        self.log, self.namespace, self.ledger, self.daemon = log, None, None, None
        self.environment = dict(os.environ)
        self.mount = config["host_mount_directory"]
        self.workspace = config.get("server_workspace")
        if (
            type(self.workspace) is not str
            or not self.workspace.startswith("/data/")
            or str(PurePosixPath(self.workspace)) != self.workspace
            or ".." in PurePosixPath(self.workspace).parts
            or Path(config.get("host_workspace", "")).absolute()
            != (Path(self.mount) / PurePosixPath(self.workspace).relative_to("/data")).absolute()
        ):
            raise ValueError("Workspace must identify the same admitted /data bind")
        # Capture the cleanup implementation authenticated by the complete release;
        # no unchecked reread/alternate helper is executed after verification.
        root = Path(config["release_directory"])
        manifest_bytes = (root / "release.json").read_bytes()
        if hashlib.sha256(manifest_bytes).hexdigest() != config["expected_release_manifest_sha256"]:
            raise ValueError("Release manifest changed after verification")
        manifest = json.loads(manifest_bytes)
        rows = [row for row in manifest["files"] if row["path"] == "operator/doctor.py"]
        helper = root / "operator/doctor.py"
        if (
            len(rows) != 1
            or helper.is_symlink()
            or not helper.is_file()
            or type(rows[0]["bytes"]) is not int
            or not 0 < rows[0]["bytes"] <= 1024 * 1024
        ):
            raise ValueError("Verified image-owned observer cleanup helper is absent")
        with helper.open("rb") as source:
            captured = source.read(1024 * 1024 + 1)
        if (
            len(captured) != rows[0]["bytes"]
            or hashlib.sha256(captured).hexdigest() != rows[0]["sha256"]
        ):
            raise ValueError("Observer cleanup helper differs from the verified release")
        self.doctor = ModuleType("sunny_fault_observer_cleanup")
        self.doctor.__file__ = str(helper)
        exec(compile(captured, str(helper), "exec"), self.doctor.__dict__)
        self.source_revision = verified["source_revision"]

    def _local_daemon(self, deadline):
        host = self.environment.get("DOCKER_HOST")
        if host is not None and not local_docker_endpoint(host):
            raise ValueError("Fault observer requires a local Docker daemon")
        code, value = self.doctor._bounded_docker(
            [self.command[0], "context", "inspect"], deadline, self.environment
        )
        contexts = json.loads(value) if not code else None
        if (
            not isinstance(contexts, list)
            or len(contexts) != 1
            or not isinstance(contexts[0], dict)
            or not isinstance(contexts[0].get("Endpoints"), dict)
            or not isinstance(contexts[0]["Endpoints"].get("docker"), dict)
            or not local_docker_endpoint(contexts[0]["Endpoints"]["docker"].get("Host"))
        ):
            raise ValueError("Fault observer requires an observed local Docker context")

    def observe(self, operation, deadline):
        """One same-image default-UID read with exact daemon/ownership cleanup."""
        if not isinstance(operation, str) or not HEX32.fullmatch(operation):
            raise RuntimeError("Managed mutation lacks a typed original token; withheld")
        deadline = min(deadline, time.monotonic() + OBSERVER_SECONDS)
        self._local_daemon(deadline)
        owned = self.doctor.OwnedDocker(
            self.command,
            [
                "--name",
                "sunny-fault-ledger-" + uuid.uuid4().hex,
                "--network=none",
                "--read-only",
                "--mount",
                "type=bind,source=" + self.mount + ",target=/data,readonly",
                "--entrypoint=python3",
                self.image,
                "-I",
                "-S",
                "-c",
                LEDGER_OBSERVER,
                self.workspace,
                operation,
                self.namespace or "",
            ],
            self.environment,
            deadline,
        )
        try:
            if self.daemon is not None and owned.daemon != self.daemon:
                raise RuntimeError("Original local Docker daemon changed; native request withheld")
            self.daemon = owned.daemon
            output = observer_output(owned.command, deadline, self.environment)
        finally:
            cleanup = owned.close(time.monotonic() + OBSERVER_SECONDS)
            record(
                self.log,
                "ledger_observer_cleanup",
                token=operation,
                image=self.image,
                source_revision=self.source_revision,
                cleanup=cleanup,
            )
            owned.directory.cleanup()
        if cleanup.get("removed") is not True:
            raise RuntimeError("Ledger observer cleanup is unconfirmed; native request withheld")
        result = json.loads(output)
        if (
            not isinstance(result, dict)
            or set(result) != {"workspace_namespace", "ledger", "entries"}
            or not isinstance(result["workspace_namespace"], str)
            or not HEX32.fullmatch(result["workspace_namespace"])
            or (self.namespace is not None and result["workspace_namespace"] != self.namespace)
            or not isinstance(result["entries"], list)
            or type(result["ledger"]) is not str
            or (self.ledger is not None and result["ledger"] != self.ledger)
        ):
            raise RuntimeError("Ledger observer returned invalid original correlation; withheld")
        self.namespace = result["workspace_namespace"]
        self.ledger = result["ledger"]
        return result


def local_docker_endpoint(value):
    """Admit an absolute local Unix socket or dot-host Windows pipe, never remote UNC."""
    return (
        type(value) is str
        and "\x00" not in value
        and (
            (value.startswith("unix:///") and len(value) > len("unix:///"))
            or re.fullmatch(r"npipe:/{4}\./pipe/[A-Za-z0-9_.-]{1,128}", value) is not None
        )
    )


def observe_fence(observer, request, log, deadline):
    """Observation failures retain evidence and never reach the native socket send."""
    operation = None
    try:
        operation = mutation_token(request)
        observed = observer.observe(operation, deadline)
        fenced = require_fence(request, observed["entries"])
        record(
            log,
            "actual_predispatch_fence",
            token=operation,
            ledger=observed["ledger"],
            workspace_namespace=observed["workspace_namespace"],
            entry=fenced,
        )
    except Exception as exc:
        record(log, "native_request_withheld", token=operation, reason=type(exc).__name__)
        raise RuntimeError(
            "Original private ledger fence was unavailable; native request withheld"
        ) from exc
    return fenced


def forward_once(observer, outgoing, request_bytes, request, log, deadline):
    """Dispatch once only after private disk evidence is available and exact."""
    if request.get("name") in MANAGED_MUTATIONS:
        observe_fence(observer, request, log, deadline)
    record(log, "forward_once", request=request)
    send_bytes_frame(outgoing, request_bytes, deadline)


def main():
    """Run only the explicitly selected command-line qualification operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--listen-host", default="127.0.0.1")
    parser.add_argument("--listen-port", type=int, default=9003)
    parser.add_argument(
        "--drop-method",
        required=True,
        choices=[
            "sunny_managed_create_clip",
            "sunny_managed_update_clip_geometry",
            "sunny_managed_revise_note_population",
            "sunny_managed_insert_device",
            "sunny_managed_update_device_parameters",
            "sunny_managed_update_device_modes",
            "sunny_managed_author_envelope",
            "sunny_managed_replace_envelope",
            "sunny_managed_update_static_mixer",
            "sunny_managed_apply_routing",
            "sunny_managed_apply_song_settings",
        ],
    )
    parser.add_argument("--log", required=True)
    args = parser.parse_args()
    try:
        listener_address = ipaddress.IPv4Address(args.listen_host)
    except ipaddress.AddressValueError:
        parser.error("Fault proxy requires a numeric IPv4 loopback and port 1..65535")
    if not listener_address.is_loopback or not 1 <= args.listen_port <= 65535:
        parser.error("Fault proxy requires a numeric IPv4 loopback and port 1..65535")
    config = json.loads(Path(args.config).read_text())
    observer = LedgerObserver(config, args.log)
    session_deadline = time.monotonic() + seconds(config.get("mcp_session_timeout", 1800), 3600)
    frame_budget = seconds(config.get("mcp_timeout", 120), 120)
    dropped = False
    stop = threading.Event()
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((str(listener_address), args.listen_port))
        listener.listen(1)
        print("Fault proxy ready: one matched reply only; Ctrl-C stops", flush=True)
        while not stop.is_set() and time.monotonic() < session_deadline:
            listener.settimeout(min(0.25, remaining(session_deadline)))
            try:
                incoming, _ = listener.accept()
            except TimeoutError:
                continue
            deadline = min(session_deadline, time.monotonic() + frame_budget)
            endpoint = addresses(config["bridge_host"], config["bridge_port"], deadline)[0]
            with (
                incoming,
                socket.socket(socket.AF_INET, socket.SOCK_STREAM) as outgoing,
            ):
                outgoing.settimeout(remaining(deadline))
                outgoing.connect(endpoint)
                while True:
                    try:
                        deadline = min(session_deadline, time.monotonic() + frame_budget)
                        request_bytes = recv_frame(incoming, deadline)
                        request = json.loads(request_bytes)
                        if not isinstance(request, dict):
                            raise RuntimeError("Native request is not an object; withheld")
                        payload = request.get("args", [{}])[0] if request.get("args") else {}
                        token = payload.get("operation_id") if isinstance(payload, dict) else None
                        forward_once(observer, outgoing, request_bytes, request, args.log, deadline)
                        response = recv_frame(outgoing, deadline)
                        record(
                            args.log,
                            "actual_host_response",
                            token=token,
                            response=json.loads(response),
                        )
                        if request.get("name") == args.drop_method and not dropped:
                            dropped = True
                            record(
                                args.log,
                                "reply_deliberately_dropped",
                                original_token=token,
                                request=request,
                            )
                            incoming.shutdown(socket.SHUT_RDWR)
                            break
                        send_bytes_frame(incoming, response, deadline)
                    except (EOFError, ConnectionError, TimeoutError, RuntimeError, ValueError):
                        break


if __name__ == "__main__":
    main()
