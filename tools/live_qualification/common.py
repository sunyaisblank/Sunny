"""Host qualification I/O only; no Live model or production imports."""

import json
import os
import queue
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path

MAX_FRAME = 16 * 1024 * 1024


def record(path, kind, **fields):
    """Append one deliberate timestamped qualification observation."""
    row = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "kind": kind, **fields}
    with Path(path).open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(row, ensure_ascii=True, allow_nan=False) + "\n")
    return row


def exact(sock, size):
    """Read precisely one bounded frame part; never replay an uncertain request."""
    data = bytearray()
    while len(data) < size:
        part = sock.recv(size - len(data))
        if not part:
            raise EOFError("Connection closed; outcome may be uncertain. Do not replay a mutation.")
        data.extend(part)
    return bytes(data)


def recv_frame(sock):
    """Read the literal production framing with its 16 MiB payload limit."""
    size = struct.unpack(">I", exact(sock, 4))[0]
    if not 0 < size <= MAX_FRAME:
        raise ValueError("Response exceeds the production 16 MiB frame boundary")
    return exact(sock, size)


def send_frame(sock, value):
    """Send one bounded JSON frame without mutation retry."""
    body = json.dumps(value, allow_nan=False).encode("utf-8")
    if not 0 < len(body) <= MAX_FRAME:
        raise ValueError("Request exceeds the production frame boundary")
    sock.sendall(struct.pack(">I", len(body)) + body)


def rpc(host, port, request, log, timeout=35.0):
    """Perform one request and retain the actual timed wire response."""
    start = time.monotonic()
    record(log, "tcp_request", host=host, port=port, request=request)
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        send_frame(sock, request)
        body = recv_frame(sock)
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


class Mcp:
    """One process, one request at a time, no retry or synthesized successful evidence."""

    def __init__(self, configuration, log):
        """Initialize the single process or native probe without granting mutation authority."""
        self.log = log
        self.config = configuration
        env = dict(os.environ)
        env.update(
            SUNNY_ABLETON_HOST=configuration["bridge_host"],
            SUNNY_TCP_PORT=str(configuration["bridge_port"]),
            SUNNY_WORKSPACE_PATH=configuration["server_workspace"],
        )
        env.pop("SUNNY_WORKSPACE_RECOVERY", None)
        self.stderr = Path(str(log) + ".stderr").open("a", encoding="utf-8")
        self.process = subprocess.Popen(
            configuration["mcp_command"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self.stderr,
            text=True,
            env=env,
        )
        self.lines = queue.Queue()
        self.next_id = 0

        def read():
            for line in self.process.stdout:
                self.lines.put(line)
            self.lines.put(None)

        threading.Thread(target=read, daemon=True).start()
        self.request(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "SunnyFinalHostProbe", "version": "1"},
            },
        )

    def request(self, method, params):
        """Send one serial MCP request and verify its matching response identity."""
        self.next_id += 1
        request = {"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params}
        record(self.log, "mcp_request", request=request)
        self.process.stdin.write(json.dumps(request, allow_nan=False) + "\n")
        self.process.stdin.flush()
        start = time.monotonic()
        line = self.lines.get(timeout=self.config.get("mcp_timeout", 180))
        if line is None:
            raise EOFError("MCP exited: query original token, never replay automatically")
        response = json.loads(line)
        record(
            self.log,
            "mcp_response",
            response=response,
            elapsed=time.monotonic() - start,
            utf8_bytes=len(line.encode("utf-8")),
        )
        if response.get("id") != self.next_id or "error" in response:
            raise RuntimeError(response)
        return response["result"]

    def call(self, tool, **arguments):
        """Decode one tool result without inventing successful content."""
        result = self.request("tools/call", {"name": tool, "arguments": arguments})
        content = result.get("structuredContent")
        if content is None:
            content = json.loads(result["content"][0]["text"])
        return content

    def close(self):
        """Close MCP input and bound process cleanup after recording its output."""
        self.process.stdin.close()
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.stderr.close()

    def __enter__(self):
        """Return this single-client MCP session."""
        return self

    def __exit__(self, *_):
        """Release the session after either successful or failed qualification."""
        self.close()
