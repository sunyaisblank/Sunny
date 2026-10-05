"""One controlled lost reply: actual host dispatch once, never mutation replay.

Run only for a separately saved disposable fault Set/workspace. It verifies the
actual product ledger fence immediately before forwarding each tokened mutation.
A matched response is logged in full but deliberately not forwarded. Subsequent
original-token journal queries pass normally. No Live API or permissive host model.
"""

import argparse
import json
import socket
import struct
import threading
from pathlib import Path

from common import record, recv_frame

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


def require_fence(request, entries):
    """Typed parsed immutable payload closure before forwarding any managed mutator."""
    if request.get("name") not in MANAGED_MUTATIONS:
        return None  # Original-token read-only queries intentionally pass without a mutation fence.
    if request.get("type") != "call" or request.get("path") != "song":
        raise RuntimeError("Managed mutator path/type differs; request withheld")
    args = request.get("args")
    if not isinstance(args, list) or len(args) != 1 or not isinstance(args[0], dict):
        raise RuntimeError("Managed mutation requires one closed payload; request withheld")
    token = args[0].get("operation_id")
    if not isinstance(token, str) or not token:
        raise RuntimeError("Managed mutation lacks operation token; native request withheld")
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


def ledger(config):
    """Resolve the original durable namespace ledger through the configured mount."""
    metadata = json.loads(Path(config["host_workspace"]).read_text())["native_realization"]
    base = Path(metadata["history_base_directory"])
    if config.get("server_mount_prefix"):
        base = Path(config["host_mount_directory"]) / base.relative_to(
            config["server_mount_prefix"]
        )
    return base / metadata["workspace_namespace"] / "ledger.json"


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
    config = json.loads(Path(args.config).read_text())
    dropped = False
    stop = threading.Event()
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((args.listen_host, args.listen_port))
        listener.listen(1)
        print("Fault proxy ready: one matched reply only; Ctrl-C stops", flush=True)
        while not stop.is_set():
            incoming, _ = listener.accept()
            with (
                incoming,
                socket.create_connection(
                    (config["bridge_host"], config["bridge_port"]), timeout=35
                ) as outgoing,
            ):
                incoming.settimeout(None)
                outgoing.settimeout(180)
                while True:
                    try:
                        request_bytes = recv_frame(incoming)
                        request = json.loads(request_bytes)
                        payload = request.get("args", [{}])[0] if request.get("args") else {}
                        token = payload.get("operation_id") if isinstance(payload, dict) else None
                        if request.get("name") in MANAGED_MUTATIONS:
                            entries = json.loads(ledger(config).read_text())["attempts"]
                            fenced = require_fence(request, entries)
                            record(
                                args.log,
                                "actual_predispatch_fence",
                                token=token,
                                ledger=str(ledger(config)),
                                entry=fenced,
                            )
                        record(args.log, "forward_once", request=request)
                        outgoing.sendall(struct.pack(">I", len(request_bytes)) + request_bytes)
                        response = recv_frame(outgoing)
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
                        incoming.sendall(struct.pack(">I", len(response)) + response)
                    except (EOFError, ConnectionError):
                        break


if __name__ == "__main__":
    main()
