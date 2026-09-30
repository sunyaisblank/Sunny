#!/usr/bin/env python3
"""Drive one retained Sunny plan/apply attempt against a named Ableton Live peer."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import queue
import re
import subprocess
import threading
from datetime import UTC, datetime
from pathlib import Path
from typing import TextIO

MCP_PROTOCOL_VERSION = "2025-11-25"
REMOTE_SCRIPT_SOURCE_FILES = (
    "__init__.py",
    "bridge_contract.json",
    "handler.py",
    "server.py",
    "surface.py",
)


def _write_json(path: Path, value: object) -> None:
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def _stable_bytes(path: Path, subject: str) -> bytes:
    before = path.lstat()
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"{subject} must be a direct non-symlink regular file: {path}")
    value = path.read_bytes()
    after = path.lstat()
    if (
        before.st_dev,
        before.st_ino,
        before.st_size,
        before.st_mtime_ns,
    ) != (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
    ):
        raise ValueError(f"{subject} changed while it was being read: {path}")
    return value


def _file_fact(path: Path, subject: str) -> dict[str, object]:
    value = _stable_bytes(path, subject)
    return {
        "path": str(path.resolve(strict=True)),
        "size": len(value),
        "sha256": hashlib.sha256(value).hexdigest(),
    }


def _remote_script_fact(root: Path) -> dict[str, object]:
    if root.is_symlink():
        raise ValueError(f"Remote Script root must not be a symlink: {root}")
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f"Remote Script root is not a directory: {root}")
    for name in REMOTE_SCRIPT_SOURCE_FILES:
        _stable_bytes(root / name, f"Remote Script {name}")
    files: list[dict[str, object]] = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Remote Script tree contains a symlink: {path}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError(f"Remote Script tree contains a non-regular entry: {path}")
        fact = _file_fact(path, f"Remote Script file {path.relative_to(root)}")
        fact["relative_path"] = path.relative_to(root).as_posix()
        fact.pop("path")
        files.append(fact)
    if not files:
        raise ValueError("Remote Script tree is empty")
    canonical = json.dumps(files, sort_keys=True, separators=(",", ":")).encode()
    return {
        "root": str(root),
        "required_source_files": list(REMOTE_SCRIPT_SOURCE_FILES),
        "files": files,
        "tree_sha256": hashlib.sha256(canonical).hexdigest(),
    }


def _retain_file_snapshot(source: Path, destination: Path, subject: str) -> dict[str, object]:
    value = _stable_bytes(source, subject)
    temporary = destination.with_name(f".{destination.name}.tmp-{os.getpid()}")
    temporary.write_bytes(value)
    temporary.replace(destination)
    return {
        "source": str(source.resolve(strict=True)),
        "retained_path": str(destination),
        "size": len(value),
        "sha256": hashlib.sha256(value).hexdigest(),
    }


def _printable(value: str, name: str) -> str:
    if (
        not value
        or len(value) > 512
        or any(ord(character) < 0x20 or ord(character) == 0x7F for character in value)
    ):
        raise argparse.ArgumentTypeError(f"{name} must be one non-empty printable string")
    return value


class McpPeer:
    """Minimal newline-delimited MCP client with retained request/response events."""

    def __init__(
        self,
        process: subprocess.Popen[str],
        transcript: dict[str, object],
        transcript_path: Path,
        timeout: float,
    ) -> None:
        """Start a background stdout reader and bind one retained transcript."""
        if process.stdin is None or process.stdout is None:
            raise ValueError("MCP peer pipes were not created")
        self.process = process
        self.input: TextIO = process.stdin
        self.output: TextIO = process.stdout
        self.transcript = transcript
        self.transcript_path = transcript_path
        self.timeout = timeout
        self.next_id = 1
        self.lines: queue.Queue[str | BaseException] = queue.Queue()
        self.reader = threading.Thread(target=self._read_lines, daemon=True)
        self.reader.start()

    def _read_lines(self) -> None:
        try:
            for line in self.output:
                if line.strip():
                    self.lines.put(line)
        except BaseException as error:  # Preserve an unexpected pipe failure for the main thread.
            self.lines.put(error)

    def _retain(self) -> None:
        _write_json(self.transcript_path, self.transcript)

    def request(self, method: str, params: dict[str, object]) -> dict[str, object]:
        """Send one identified request, retain both envelopes, and return its response."""
        request_id = self.next_id
        self.next_id += 1
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": request_id,
        }
        events = self.transcript["events"]
        if not isinstance(events, list):
            raise ValueError("internal transcript event store is invalid")
        event: dict[str, object] = {"request": request, "response": None}
        events.append(event)
        self._retain()
        self.input.write(json.dumps(request, separators=(",", ":")) + "\n")
        self.input.flush()
        try:
            received = self.lines.get(timeout=self.timeout)
        except queue.Empty as error:
            raise TimeoutError(f"timed out waiting for MCP response to {method}") from error
        if isinstance(received, BaseException):
            raise RuntimeError(f"MCP stdout reader failed: {received}") from received
        try:
            response = json.loads(received)
        except json.JSONDecodeError as error:
            raise ValueError(f"MCP emitted non-JSON stdout: {received.rstrip()}") from error
        if not isinstance(response, dict) or response.get("id") != request_id:
            raise ValueError(f"MCP response identity mismatch for {method}: {response}")
        event["response"] = response
        self._retain()
        if "error" in response:
            raise RuntimeError(f"MCP protocol error for {method}: {response['error']}")
        return response

    def notify(self, method: str, params: dict[str, object] | None = None) -> None:
        """Send and retain one notification that intentionally has no response."""
        request: dict[str, object] = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            request["params"] = params
        events = self.transcript["events"]
        if not isinstance(events, list):
            raise ValueError("internal transcript event store is invalid")
        events.append({"notification": request})
        self._retain()
        self.input.write(json.dumps(request, separators=(",", ":")) + "\n")
        self.input.flush()

    def tool(self, name: str, arguments: dict[str, object]) -> dict[str, object]:
        """Call one Sunny MCP tool and decode its single JSON text result."""
        response = self.request("tools/call", {"name": name, "arguments": arguments})
        result = response.get("result")
        if not isinstance(result, dict) or result.get("isError") is not False:
            raise RuntimeError(f"tool {name} did not return a successful MCP result: {result}")
        content = result.get("content")
        if (
            not isinstance(content, list)
            or len(content) != 1
            or not isinstance(content[0], dict)
            or content[0].get("type") != "text"
            or not isinstance(content[0].get("text"), str)
        ):
            raise ValueError(f"tool {name} returned an unexpected content envelope")
        try:
            value = json.loads(content[0]["text"])
        except json.JSONDecodeError as error:
            raise ValueError(f"tool {name} returned non-JSON text") from error
        if not isinstance(value, dict):
            raise ValueError(f"tool {name} result is not a JSON object")
        events = self.transcript["events"]
        if isinstance(events, list):
            events[-1]["decoded_tool_result"] = value
            self._retain()
        if "error" in value or value.get("success") is False:
            raise RuntimeError(f"tool {name} reported failure: {value}")
        return value


def _validation_context(arguments: argparse.Namespace) -> dict[str, object]:
    context: dict[str, object] = {
        "live_edition": arguments.live_edition,
        "operating_system": arguments.operating_system,
        "architecture": arguments.architecture,
        "remote_script_revision": arguments.remote_script_revision,
        "cleanup_steps": arguments.cleanup_step,
    }
    max_values = (arguments.max_version, arguments.max_for_live_version, arguments.license_state)
    if any(max_values) and not all(max_values):
        raise ValueError(
            "--max-version, --max-for-live-version, and --license-state are all-or-none"
        )
    if all(max_values):
        context["max"] = {
            "max_version": arguments.max_version,
            "max_for_live_version": arguments.max_for_live_version,
            "license_state": arguments.license_state,
        }
    return context


def _run(arguments: argparse.Namespace) -> Path:
    server_fact = _file_fact(arguments.server, "Sunny MCP server")
    server = arguments.server.resolve(strict=True)
    if not os.access(server, os.X_OK):
        raise ValueError(f"Sunny MCP server is not executable: {server}")
    if not re.fullmatch(r"[0-9a-f]{7,64}", arguments.remote_script_revision):
        raise ValueError("Remote Script revision must be 7-64 lowercase hexadecimal characters")
    if arguments.apply and not arguments.cleanup_step:
        raise ValueError("--apply requires at least one explicit --cleanup-step")
    context = _validation_context(arguments)
    remote_script_fact = _remote_script_fact(arguments.remote_script_root)
    _stable_bytes(arguments.live_log, "Ableton Live log")

    output_root = arguments.output_root.resolve()
    if output_root.exists():
        raise ValueError(f"refusing to overwrite an existing Live run: {output_root}")
    output_root.mkdir(parents=True)
    live_log_before = _retain_file_snapshot(
        arguments.live_log,
        output_root / "ableton-live-log-before.txt",
        "Ableton Live log",
    )
    transcript_path = output_root / "json-rpc-transcript.json"
    stderr_path = output_root / "sunny-mcp-stderr.txt"
    transcript: dict[str, object] = {
        "schema_version": 1,
        "run_name": "sunny-ableton-named-host-validation",
        "mode": "guarded_apply" if arguments.apply else "plan_only",
        "started_at_utc": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "server": str(server),
        "server_file": server_fact,
        "installed_remote_script": remote_script_fact,
        "live_log_before": live_log_before,
        "live_log_after": None,
        "peer": {"host": arguments.host, "port": arguments.port},
        "operator_environment": context,
        "fixture": {
            "score_title": "Sunny Named Host Validation",
            "part_name": "Sunny Validation Analog",
            "note": {
                "pitch": {"letter": "C", "accidental": 0, "octave": 4},
                "duration": {"n": 1, "d": 8},
                "velocity": 72,
            },
            "native_device": "Analog",
            "parameter": {
                "ir_path": "source.filter.cutoff",
                "name": "Filter Freq",
                "value_property": "value",
            },
        },
        "events": [],
        "validation_record_path": None,
        "finished_at_utc": None,
        "status": "started",
    }
    _write_json(transcript_path, transcript)

    environment = os.environ.copy()
    environment["SUNNY_ABLETON_HOST"] = arguments.host
    environment["SUNNY_TCP_PORT"] = str(arguments.port)
    with stderr_path.open("w", encoding="utf-8") as error_log:
        process = subprocess.Popen(
            [str(server)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=error_log,
            text=True,
            encoding="utf-8",
            bufsize=1,
            env=environment,
        )
        peer = McpPeer(process, transcript, transcript_path, arguments.timeout)
        try:
            peer.request("initialize", {"protocolVersion": MCP_PROTOCOL_VERSION})
            peer.notify("notifications/initialized")

            score = peer.tool(
                "score_create",
                {
                    "title": "Sunny Named Host Validation",
                    "total_bars": 1,
                    "parts": [{"name": "Sunny Validation Analog", "instrument_type": 0}],
                },
            )
            part_ids = score.get("part_ids")
            if not isinstance(part_ids, list) or len(part_ids) != 1:
                raise ValueError("score_create did not return exactly one Part ID")
            score_id = score.get("score_id")
            part_id = part_ids[0]
            formal_plan = peer.tool(
                "score_set_formal_plan",
                {
                    "score_id": score_id,
                    "sections": [{"label": "A", "start_bar": 1, "end_bar": 1}],
                },
            )
            if formal_plan.get("ok") is not True:
                raise ValueError("score_set_formal_plan did not confirm its atomic mutation")
            inserted_note = peer.tool(
                "score_insert_note",
                {
                    "score_id": score_id,
                    "part_id": part_id,
                    "bar": 1,
                    "pitch": {"letter": "C", "accidental": 0, "octave": 4},
                    "duration": {"n": 1, "d": 8},
                    "velocity": 72,
                },
            )
            if inserted_note.get("ok") is not True:
                raise ValueError("score_insert_note did not confirm its atomic mutation")
            profile = peer.tool(
                "create_timbre_profile",
                {"part_id": part_id, "name": "Sunny Validation Analog"},
            )
            profile_id = profile.get("profile_id")
            peer.tool(
                "map_timbre_parameter",
                {
                    "profile_id": profile_id,
                    "ir_path": "source.filter.cutoff",
                    "device_index": 0,
                    "parameter_name": "Filter Freq",
                    "source_min": 20.0,
                    "source_max": 20000.0,
                    "target_min": 0.0,
                    "target_max": 1.0,
                    "value_property": "value",
                    "device_name": "Analog",
                },
            )
            mix = peer.tool("create_mix_graph", {"part_ids": [part_id]})
            mix_id = mix.get("graph_id")
            peer.tool(
                "set_channel_level",
                {"graph_id": mix_id, "channel_id": 1, "level_db": -12.0},
            )
            project = {
                "score_id": score_id,
                "timbre_profile_ids": [profile_id],
                "mix_graph_id": mix_id,
            }
            project_validation = peer.tool("project_validate", project)
            if project_validation.get("valid") is not True:
                raise ValueError("the deterministic project fixture did not pass native validation")
            plan = peer.tool("project_plan_to_ableton", project)
            if plan.get("one_shot") is not True:
                raise ValueError("the Live dry-run did not return a guarded one-shot plan")
            _write_json(output_root / "dry-run-plan.json", plan)

            if arguments.apply:
                applied = peer.tool(
                    "project_apply_ableton_plan",
                    {"plan_id": plan.get("plan_id"), "validation_context": context},
                )
                record = applied.get("validation_record")
                if not isinstance(record, dict):
                    raise ValueError("guarded apply did not return a non-null validation record")
                record_path = output_root / "ableton-validation-record.json"
                _write_json(record_path, record)
                transcript["validation_record_path"] = str(record_path)
                transcript["status"] = "guarded_apply_returned_validation_record"
            else:
                transcript["status"] = "plan_retained_without_mutation"
        except BaseException as error:
            transcript["status"] = "failed"
            transcript["failure"] = f"{type(error).__name__}: {error}"
            raise
        finally:
            transcript["finished_at_utc"] = datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
            _write_json(transcript_path, transcript)
            try:
                peer.input.close()
            except OSError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    try:
        transcript["live_log_after"] = _retain_file_snapshot(
            arguments.live_log,
            output_root / "ableton-live-log-after.txt",
            "Ableton Live log",
        )
    except (OSError, ValueError) as error:
        transcript["status"] = "failed_to_retain_final_live_log"
        transcript["failure"] = f"{type(error).__name__}: {error}"
        _write_json(transcript_path, transcript)
        raise
    _write_json(transcript_path, transcript)
    return output_root


def main() -> None:
    """Run the named-Live handoff in plan-only or explicitly authorized apply mode."""
    parser = argparse.ArgumentParser(
        description=(
            "Retain a deterministic Sunny JSON-RPC transcript and dry-run plan; mutate Live only "
            "when --apply is explicitly supplied."
        )
    )
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--remote-script-root", type=Path, required=True)
    parser.add_argument("--live-log", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--live-edition", required=True)
    parser.add_argument("--operating-system", required=True)
    parser.add_argument("--architecture", required=True)
    parser.add_argument("--remote-script-revision", required=True)
    parser.add_argument("--max-version")
    parser.add_argument("--max-for-live-version")
    parser.add_argument("--license-state")
    parser.add_argument("--cleanup-step", action="append", default=[])
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()
    for attribute in (
        "host",
        "live_edition",
        "operating_system",
        "architecture",
        "remote_script_revision",
    ):
        setattr(arguments, attribute, _printable(getattr(arguments, attribute), attribute))
    for attribute in ("max_version", "max_for_live_version", "license_state"):
        value = getattr(arguments, attribute)
        if value is not None:
            setattr(arguments, attribute, _printable(value, attribute))
    arguments.cleanup_step = [_printable(value, "cleanup_step") for value in arguments.cleanup_step]
    if not 1 <= arguments.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if arguments.timeout <= 0:
        parser.error("--timeout must be positive")
    try:
        output_root = _run(arguments)
    except (OSError, RuntimeError, TimeoutError, UnicodeError, ValueError) as error:
        parser.exit(2, f"run-named-live-validation: {error}\n")
    print(f"retained named Live run: {output_root}")
    if not arguments.apply:
        print("Plan-only mode: Ableton Live was not mutated.")


if __name__ == "__main__":
    main()
