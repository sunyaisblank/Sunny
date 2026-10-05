"""One explicitly selected running-cue host diagnostic, no implicit transport start/stop."""

import argparse
import json
import math
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from common import (  # noqa: E402 - preserve the release before peer imports.
    Mcp,
    legacy_call,
    prime_native_session,
    record,
    require_primary_context,
    verify_selected_release,
)
from host_runner import diagnostic, ok  # noqa: E402


def main():
    """Run only the explicitly selected command-line qualification operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument("--scratch-running-approved", action="store_true")
    parser.add_argument("--beat", type=float, default=8.0)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    verify_selected_release(config)
    if not args.scratch_running_approved or not args.label.startswith("SUNNY_HOST_PROBE_CUE_"):
        raise ValueError(
            "Explicit running scratch approval and unique diagnostic locator prefix required"
        )
    if not math.isfinite(args.beat) or args.beat < 0 or len(args.label) > 256:
        raise ValueError("Select a finite nonnegative beat and bounded diagnostic cue label")
    log = Path(config["evidence_directory"]) / "running-cue.jsonl"
    actual = diagnostic(
        config,
        log,
        {"op": "inventory", "track_names": []},
    )
    if actual["set_name"]["value"] != config["scratch_set_name"] or not actual["set_name"][
        "value"
    ].startswith("SUNNY_HOST_QUALIFICATION_"):
        raise RuntimeError("Wrong actual scratch Set")
    if actual["song"]["is_playing"].get("value") is not True:
        raise RuntimeError("Operator must start scratch transport first")
    if (
        any(
            actual["song"][field].get("value") is not False
            for field in ("session_record", "record_mode")
        )
        or actual.get("input_monitoring", {}).get("all_off") is not True
        or actual.get("version") != config["host_live_version"]
        or not config.get("host_edition")
        or not config.get("host_os")
    ):
        raise RuntimeError(
            "Disable recording/input monitoring and record exact host metadata first"
        )
    require_primary_context(config, actual)
    with Mcp(config, log) as client:
        prime_native_session(client, config)
        if Path(config["host_workspace"]).exists():
            ok(client, "workspace_open", path=config["server_workspace"])
        ok(client, "workspace_save", path=config["server_workspace"])
        response = legacy_call(
            client,
            config,
            {
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [args.beat, args.label],
            },
        )
        # Keep the full stage-aware receipt, including failed prepare/execute/query
        # provenance. This command is sent once and never compensated or replayed.
        record(log, "running_cue_actual_response", response=response)
    diagnostic(config, log, {"op": "inventory", "track_names": []})
    print(
        "Stop transport manually; capture UI/audible/playhead evidence "
        "and existing locator preservation."
    )


if __name__ == "__main__":
    main()
