"""Contract tests for the Remote Script's bounded diagnostic log."""

from __future__ import annotations

import json
import logging
import time

import pytest
from Sunny.diagnostics import RemoteLog
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.server import _versioned_response


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


@pytest.mark.parametrize(
    "args",
    [
        [],
        [-1],
        [1.0],
        [True],
        ["1"],
        [0, 1],
        [0, None],
        [0, "A" * 32],
        [0, "a" * 31],
        [0, "g" * 32],
        [0, "a" * 32, 0],
        [2147483648],
    ],
)
def test_the_bridge_refuses_malformed_log_requests(args: list) -> None:
    """Only a bounded integer and optional lowercase stream epoch are accepted."""
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


def test_epoch_cursor_detects_a_restart_even_when_the_old_sequence_is_plausible() -> None:
    """A changed stream must reveal new low-numbered errors to an old cursor."""
    old = RemoteLog()
    old.emit(logging.LogRecord("sunny.test.old", logging.INFO, "", 1, "old", (), None))
    cursor = old.entries_after(0)
    restarted = RemoteLog()
    for message in ("new error", "second", "third"):
        restarted.emit(logging.LogRecord("sunny.test.new", logging.ERROR, "", 1, message, (), None))
    page = restarted.entries_after(cursor["next_sequence"], cursor["stream_id"])
    assert page["reset"] is True and page["truncated"] is False
    assert page["stream_id"] != cursor["stream_id"]
    assert [entry["sequence"] for entry in page["entries"]] == [1, 2, 3]
    assert page["entries"][0]["message"] == "new error"
    assert page["next_sequence"] == page["latest_sequence"] == 3
    assert page["has_more"] is False


def test_legacy_ahead_cursor_resets_and_ring_loss_is_independent() -> None:
    """Restart indication and dropped-record indication express separate losses."""
    remote_log = RemoteLog(capacity=2)
    for index in range(4):
        remote_log.emit(
            logging.LogRecord("sunny.test.ahead", logging.INFO, "", 1, str(index), (), None)
        )
    page = remote_log.entries_after(99)
    assert page["reset"] is True and page["truncated"] is True
    assert page["oldest_sequence"] == 3
    assert [entry["sequence"] for entry in page["entries"]] == [3, 4]
    empty = RemoteLog().entries_after(99)
    assert empty["reset"] is True and empty["entries"] == []
    assert empty["next_sequence"] == empty["latest_sequence"] == 0
    assert empty["oldest_sequence"] == 1 and empty["truncated"] is False


def test_empty_poll_is_fresh_and_epoch_read_touches_no_live_object() -> None:
    """An empty page records its fresh observation without reading native state."""

    class NoLiveSurface:
        def song(self):
            raise AssertionError("An epoch log cursor must not resolve Live")

    remote_log = RemoteLog()
    cursor = remote_log.entries_after(0)
    before = time.time()
    response = LomHandler(NoLiveSurface(), remote_log).handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_get_remote_log",
            "args": [0, cursor["stream_id"]],
        }
    )
    after = time.time()
    assert response["success"] is True
    page = response["value"]
    assert before <= page["observed_at"] <= after
    assert page["reset"] is False and page["has_more"] is False
    assert page["entries"] == [] and page["next_sequence"] == 0


def test_unicode_pages_fit_the_literal_wire_limit_and_deliver_every_sequence_once() -> None:
    """The real wire escaping must retain ordered progress within 16 MiB pages."""
    remote_log = RemoteLog()
    # 1,000 records of 2,000 astral characters produce at least 24,000,000
    # JSON bytes with the actual server's ASCII escaping, exceeding 16 MiB.
    for _ in range(1000):
        remote_log.emit(
            logging.LogRecord("sunny." + "😀" * 10000, logging.INFO, "", 1, "😀" * 2000, (), None)
        )
    sequence = 0
    stream = None
    delivered = []
    page_count = 0
    while True:
        page = remote_log.entries_after(sequence, stream)
        body = json.dumps(_versioned_response({"success": True, "value": page})).encode("utf-8")
        assert len(body) <= 16777216
        assert page["entries"] and page["truncated"] is False and page["reset"] is False
        assert page["latest_sequence"] == 1000
        assert len(page["entries"][0]["source"]) == 128
        delivered.extend(entry["sequence"] for entry in page["entries"])
        sequence, stream = page["next_sequence"], page["stream_id"]
        page_count += 1
        if not page["has_more"]:
            break
        assert page_count < 4  # The chosen finite input needs two bounded pages.
    assert page_count == 2
    assert delivered == list(range(1, 1001))
    assert sequence == 1000


def test_sequence_rollover_starts_an_explicit_new_epoch() -> None:
    """The maximum accepted cursor remains usable through a visible rollover."""
    remote_log = RemoteLog()
    remote_log._next_sequence = 2147483647
    remote_log.emit(logging.LogRecord("sunny.test.rollover", logging.INFO, "", 1, "last", (), None))
    cursor = remote_log.entries_after(2147483646)
    assert cursor["next_sequence"] == 2147483647
    remote_log.emit(logging.LogRecord("sunny.test.rollover", logging.INFO, "", 1, "new", (), None))
    page = remote_log.entries_after(cursor["next_sequence"], cursor["stream_id"])
    assert page["reset"] is True
    assert page["stream_id"] != cursor["stream_id"]
    assert [entry["sequence"] for entry in page["entries"]] == [1]
    assert page["entries"][0]["message"] == "new"
