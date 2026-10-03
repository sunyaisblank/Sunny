"""Contract tests for the Remote Script's bounded diagnostic log."""

from __future__ import annotations

import logging

import pytest
from Sunny.diagnostics import RemoteLog
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler


def _logger(remote_log: RemoteLog, name: str) -> logging.Logger:
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    logger.propagate = False
    logger.addHandler(remote_log)
    return logger


def test_records_are_numbered_and_returned_after_a_sequence() -> None:
    """Each record gets the next sequence; a caller receives only newer ones."""
    remote_log = RemoteLog(capacity=10)
    logger = _logger(remote_log, "sunny.test.numbered")
    for index in range(3):
        logger.info("record %d", index)

    everything = remote_log.entries_after(0)
    assert [entry["message"] for entry in everything["entries"]] == [
        "record 0",
        "record 1",
        "record 2",
    ]
    assert [entry["sequence"] for entry in everything["entries"]] == [1, 2, 3]
    assert everything["next_sequence"] == 3 and everything["truncated"] is False
    assert [entry["message"] for entry in remote_log.entries_after(2)["entries"]] == ["record 2"]
    assert remote_log.entries_after(3)["entries"] == []


def test_the_ring_discards_the_oldest_and_says_so() -> None:
    """A full ring drops the oldest records and reports that unseen ones were lost."""
    remote_log = RemoteLog(capacity=2)
    logger = _logger(remote_log, "sunny.test.ring")
    for index in range(5):
        logger.warning("record %d", index)

    retained = remote_log.entries_after(0)
    assert [entry["sequence"] for entry in retained["entries"]] == [4, 5]
    assert retained["truncated"] is True
    # A caller that has already seen record 3 has lost nothing.
    assert remote_log.entries_after(3)["truncated"] is False


def test_exceptions_keep_their_traceback_and_long_messages_are_cut() -> None:
    """Tracebacks are retained; oversized messages are cut, not dropped."""
    remote_log = RemoteLog(capacity=4)
    logger = _logger(remote_log, "sunny.test.exceptions")
    try:
        raise ValueError("boom")
    except ValueError:
        logger.error("failed", exc_info=True)
    logger.info("x" * 5000)
    first, second = remote_log.entries_after(0)["entries"]
    assert first["level"] == "ERROR" and "ValueError: boom" in first["message"]
    assert len(second["message"]) == 2000


@pytest.mark.parametrize("args", [[], [-1], [1.0], [True], ["1"], [0, 1]])
def test_the_bridge_refuses_malformed_log_requests(args: list) -> None:
    """Only exactly one non-negative integer sequence is accepted."""
    handler = LomHandler(object(), RemoteLog())
    response = handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_get_remote_log",
            "args": args,
        }
    )
    assert response["success"] is False


def test_the_log_is_read_without_touching_live() -> None:
    """Serving the log resolves no Live object."""
    remote_log = RemoteLog()
    _logger(remote_log, "sunny.test.direct").info("hello")

    class NoLiveSurface:
        def song(self):  # pragma: no cover - reaching this is the failure
            raise AssertionError("reading the log must not resolve a Live object")

    response = LomHandler(NoLiveSurface(), remote_log).handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_get_remote_log",
            "args": [0],
        }
    )
    assert response["success"] is True
    assert [entry["message"] for entry in response["value"]["entries"]] == ["hello"]
