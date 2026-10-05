"""One explicitly selected running-cue host diagnostic, no implicit transport start/stop."""

import argparse
import json
from pathlib import Path

from common import bridge_rpc, record, rpc


def main():
    """Run only the explicitly selected command-line qualification operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--scratch-running-approved", action="store_true")
    parser.add_argument("--beat", type=float, default=8.0)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    if not args.scratch_running_approved or not args.label.startswith("SUNNY_HOST_PROBE_CUE_"):
        raise ValueError(
            "Explicit running scratch approval and unique diagnostic locator prefix required"
        )
    log = Path(config["evidence_directory"]) / "running-cue.jsonl"
    before = rpc(
        config["diagnostic_host"],
        config["diagnostic_port"],
        {"op": "inventory", "track_names": []},
        log,
    )
    if before.get("success") is not True:
        raise RuntimeError("Scratch inventory failed; do not send a running-cue mutation")
    actual = before["result"]
    if actual["set_name"]["value"] != config["scratch_set_name"] or not actual["set_name"][
        "value"
    ].startswith("SUNNY_HOST_QUALIFICATION_"):
        raise RuntimeError("Wrong actual scratch Set")
    if actual["song"]["is_playing"].get("value") is not True:
        raise RuntimeError("Operator must start scratch transport first")
    response = bridge_rpc(
        config,
        {
            "bridge_protocol_version": config["expected_bridge_protocol_version"],
            "type": "call",
            "path": "song",
            "name": "sunny_set_cue",
            "args": [args.beat, args.label],
        },
        log,
    )
    record(log, "running_cue_actual_response", response=response)
    rpc(
        config["diagnostic_host"],
        config["diagnostic_port"],
        {"op": "inventory", "track_names": []},
        log,
    )
    print(
        "Stop transport manually; capture UI/audible/playhead evidence "
        "and existing locator preservation."
    )


if __name__ == "__main__":
    main()
