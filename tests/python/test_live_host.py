"""Final live check against a running Ableton Live (opt-in).

Skipped unless SUNNY_LIVE_HOST names the machine running Live with the Sunny
Remote Script. It deploys two tracks into the open Set, so run it on an empty
or scratch Set. SUNNY_TCP_PORT selects the port (default 9001), and
SUNNY_MCP_COMMAND may replace the local binary, for example:

    export SUNNY_LIVE_HOST=192.168.1.20
    export SUNNY_MCP_COMMAND="docker run -i --rm -e SUNNY_ABLETON_HOST -e SUNNY_TCP_PORT sunny-mcp"
    pytest tests/python/test_live_host.py -s

The printed observations, including the Remote Script's own log, are the
record that settles the open assumptions in issue #22.
"""

from __future__ import annotations

import json
import os
import shlex

import pytest
from live_scenario import run_live_smoke
from test_live_end_to_end import _McpClient, _sunny_mcp_binary

LIVE_HOST = os.environ.get("SUNNY_LIVE_HOST")


@pytest.mark.skipif(not LIVE_HOST, reason="set SUNNY_LIVE_HOST to run against a real Live")
def test_project_deploys_to_a_real_live_set() -> None:
    """Deploy the smoke project to the configured Live and print what was observed."""
    command = os.environ.get("SUNNY_MCP_COMMAND")
    client = _McpClient(
        None if command else _sunny_mcp_binary(),
        int(os.environ.get("SUNNY_TCP_PORT", "9001")),
        host=LIVE_HOST or "",
        command=shlex.split(command) if command else None,
    )
    try:
        observed = run_live_smoke(client)
    finally:
        client.close()
    print(json.dumps(observed, indent=2, default=str))
