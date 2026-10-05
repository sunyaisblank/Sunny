"""Internal ownership of one TCP peer generation; never accepted from JSON."""

from __future__ import annotations

import contextlib
import select
import socket
import threading
import uuid
from typing import Any

_local = threading.local()


def current_peer() -> PeerControl | None:
    """Return the exact admitted peer captured by this callback."""
    return getattr(_local, "peer", None)


@contextlib.contextmanager
def peer_scope(peer: PeerControl | None) -> Any:
    """Carry ownership from the socket worker into its main-thread callback."""
    previous = current_peer()
    _local.peer = peer
    try:
        yield
    finally:
        _local.peer = previous


class PeerControl:
    """Fresh peer checks plus a finite observer for a single owned connection."""

    def __init__(self, connection: socket.socket) -> None:
        self.generation = uuid.uuid4().hex
        self.revoked = threading.Event()
        self._socket = connection
        self._lock = threading.Lock()
        self._phase = "receiving"
        self._closed = threading.Event()
        self._observer = threading.Thread(
            target=self._observe, name="SunnyPeerObserver", daemon=True
        )
        self._observer.start()

    def phase(self, phase: str) -> None:
        """Serialize observation with the worker's receive/response boundaries."""
        with self._lock:
            self._phase = phase

    def check(self) -> bool:
        """Decline EOF/errors and unsupported pipelining before native entry.

        While native work waits there is no consuming receiver, so select plus
        MSG_PEEK is safe on Windows as well as POSIX without changing timeouts.
        During receiving only the worker diagnoses EOF; responding permits a
        next request arriving after the complete acknowledgment.
        """
        with self._lock:
            if self.revoked.is_set():
                return False
            if self._phase not in {"native", "responding"}:
                return not self._closed.is_set()
            try:
                readable, _, errors = select.select([self._socket], [], [self._socket], 0)
                if errors:
                    self.revoked.set()
                elif readable:
                    data = self._socket.recv(1, socket.MSG_PEEK)
                    if not data or self._phase == "native":
                        self.revoked.set()
            except (OSError, ValueError):
                self.revoked.set()
            return not self.revoked.is_set()

    def _observe(self) -> None:
        while not self._closed.wait(0.05):
            if not self.check():
                return

    def close(self) -> None:
        """Revoke this generation and join its observer without a socket wait."""
        self.revoked.set()
        self._closed.set()
        self._observer.join()


def require_peer(expected: PeerControl | None = None) -> PeerControl:
    """Require the same live admitted generation before authority or effect."""
    peer = current_peer()
    if peer is None or (expected is not None and peer is not expected) or not peer.check():
        raise RuntimeError("Native peer generation is unavailable or revoked")
    return peer


def check_native_peer() -> None:
    """Check effect admission while keeping offline source models usable."""
    peer = current_peer()
    if peer is not None and not peer.check():
        raise RuntimeError("Native peer generation was revoked before effect entry")


def native_call(callback: Any, *args: Any) -> Any:
    """Honor peer revocation in existing authority helpers and offline models."""
    check_native_peer()
    return callback(*args)
