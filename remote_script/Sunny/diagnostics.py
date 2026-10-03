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
import logging
import threading
from typing import Any

# Enough for a long live session of requests and errors while keeping the
# response within the bridge's frame limit.
REMOTE_LOG_CAPACITY = 1000
# Longest message retained per record; tracebacks are cut rather than dropped.
REMOTE_LOG_MESSAGE_LIMIT = 2000


class RemoteLog(logging.Handler):
    """A logging handler that retains the most recent records in order."""

    def __init__(self, capacity: int = REMOTE_LOG_CAPACITY) -> None:
        super().__init__(level=logging.INFO)
        if capacity < 1:
            raise ValueError("capacity must be positive")
        self._records = collections.deque(maxlen=capacity)  # type: collections.deque
        self._next_sequence = 1
        self._guard = threading.Lock()

    def emit(self, record: logging.LogRecord) -> None:
        try:
            message = record.getMessage()
            if record.exc_info:
                message = message + "\n" + logging.Formatter().formatException(record.exc_info)
        except Exception:
            message = str(record.msg)
        entry = {
            "time": record.created,
            "level": record.levelname,
            "source": record.name,
            "message": message[:REMOTE_LOG_MESSAGE_LIMIT],
        }
        with self._guard:
            entry["sequence"] = self._next_sequence
            self._next_sequence += 1
            self._records.append(entry)

    def entries_after(self, sequence: int) -> dict[str, Any]:
        """Records numbered above ``sequence``, oldest first.

        ``truncated`` is true when records the caller had not yet seen were
        already discarded by the ring, so the caller knows the history is
        incomplete rather than assuming nothing happened.
        """
        with self._guard:
            entries = [dict(entry) for entry in self._records if entry["sequence"] > sequence]
            oldest = self._records[0]["sequence"] if self._records else self._next_sequence
            return {
                "entries": entries,
                "next_sequence": self._next_sequence - 1,
                "truncated": oldest > sequence + 1,
            }
