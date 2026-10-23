"""Bounded in-memory log of the Remote Script's own records.

Live's Log.txt sits on the Ableton machine, out of reach of a client that
drives Sunny from elsewhere. The Remote Script therefore keeps its recent
records in a fixed-size ring and serves them over the bridge
(sunny_get_remote_log), so an MCP client can read what happened inside Live
without separate log shipping. Records are numbered; a client polls with the
last sequence it has seen and receives only newer records.
"""

from __future__ import annotations

import collections
import json
import logging
import math
import threading
import time
import uuid
from typing import Any

# Enough for a long live session of requests and errors while keeping the
# response within the bridge's frame limit.
REMOTE_LOG_CAPACITY = 1000
# Longest message retained per record; tracebacks are cut rather than dropped.
REMOTE_LOG_MESSAGE_LIMIT = 2000
# Bound metadata as well as messages: logger names can be supplied by callers.
REMOTE_LOG_METADATA_LIMIT = 128
# Includes the complete length-prefixed bridge's JSON payload, not just messages.
REMOTE_LOG_WIRE_LIMIT = 16 * 1024 * 1024


class RemoteLog(logging.Handler):
    """A logging handler that retains the most recent records in order."""

    def __init__(self, capacity: int = REMOTE_LOG_CAPACITY) -> None:
        super().__init__(level=logging.INFO)
        if capacity < 1:
            raise ValueError("capacity must be positive")
        self._records = collections.deque(maxlen=capacity)  # type: collections.deque
        self._next_sequence = 1
        self._stream_id = uuid.uuid4().hex
        self._guard = threading.Lock()

    def emit(self, record: logging.LogRecord) -> None:
        try:
            message = record.getMessage()
            if record.exc_info:
                message = message + "\n" + logging.Formatter().formatException(record.exc_info)
        except Exception:
            message = str(record.msg)
        entry = {
            "time": record.created if math.isfinite(record.created) else time.time(),
            "level": str(record.levelname)[:REMOTE_LOG_METADATA_LIMIT],
            "source": str(record.name)[:REMOTE_LOG_METADATA_LIMIT],
            "message": message[:REMOTE_LOG_MESSAGE_LIMIT],
        }
        with self._guard:
            if self._next_sequence > 2147483647:
                # The wire cursor is int32. Rollover is a visible new stream,
                # never an unusable cursor or an unnoticed reused sequence.
                self._stream_id = uuid.uuid4().hex
                self._next_sequence = 1
                self._records.clear()
            entry["sequence"] = self._next_sequence
            self._next_sequence += 1
            self._records.append(entry)

    def entries_after(self, sequence: int, stream_id: str | None = None) -> dict[str, Any]:
        """Records numbered above ``sequence``, oldest first.

        Keep the legacy sequence-only API. An epoch cursor detects a restart
        even when its sequence is plausible in the new stream. An ahead legacy
        cursor also resets. ``truncated`` reports a retained-history gap;
        ``reset`` reports lost cursor continuity. Pagination advances only to
        the last delivered record, while ``latest_sequence`` is the snapshot
        watermark. A page is a fresh in-memory observation, not Live readiness.
        """
        with self._guard:
            latest = self._next_sequence - 1
            reset = (stream_id is not None and stream_id != self._stream_id) or sequence > latest
            after = 0 if reset else sequence
            records = [dict(entry) for entry in self._records if entry["sequence"] > after]
            oldest = self._records[0]["sequence"] if self._records else self._next_sequence
            result = {
                "entries": [],
                "next_sequence": latest,
                "truncated": oldest > after + 1,
                "stream_id": self._stream_id,
                "reset": reset,
                "observed_at": time.time(),
                "oldest_sequence": oldest,
                "latest_sequence": latest,
                "has_more": False,
            }

        # Match TcpServer's default JSON escaping/separators, including Unicode
        # surrogate pairs. Reserve the largest valid uint32 protocol version,
        # the watermark's digits and the longer Boolean spelling before adding
        # records. This remains conservative as the page cursor changes.
        size = len(
            json.dumps(
                {"success": True, "value": result, "bridge_protocol_version": 0xFFFFFFFF},
                allow_nan=False,
            ).encode("utf-8")
        )
        entries = result["entries"]
        for entry in records:
            entry_size = len(json.dumps(entry, allow_nan=False).encode("utf-8"))
            extra = entry_size + (2 if entries else 0)
            if size + extra > REMOTE_LOG_WIRE_LIMIT:
                break
            entries.append(entry)
            size += extra
        result["next_sequence"] = entries[-1]["sequence"] if entries else after
        result["has_more"] = len(entries) < len(records)
        return result
