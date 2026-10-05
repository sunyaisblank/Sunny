"""Read-only primary/probe native identity join, including module reload ambiguity."""

from __future__ import annotations

import builtins
import gc
import threading
from pathlib import Path
from types import ModuleType, SimpleNamespace

import pytest
from Sunny.managed import ManagedRegistry
from Sunny.native_qualification import (
    observe_primary_context,
    register_primary,
    unregister_primary,
)

SLOT = "_sunny_primary_qualification_registrations_v1"


@pytest.fixture(autouse=True)
def isolated_join(monkeypatch):
    """Keep this interpreter-lifetime registry separate from other surface fixtures."""
    monkeypatch.delattr(builtins, SLOT, raising=False)


def registry(song):
    """Only the surface Song accessor is an external boundary."""
    return ManagedRegistry(SimpleNamespace(song=lambda: song))


def test_registration_reads_no_native_properties_and_join_has_original_primary_tokens():
    """Registration is passive; the joined tokens are the actual primary epoch."""
    song = object()
    calls = []
    primary = ManagedRegistry(SimpleNamespace(song=lambda: calls.append("song") or song))
    assert register_primary(primary) is True and calls == []
    joined = observe_primary_context(song)
    assert joined == {
        "schema_version": 1,
        "bridge_instance": primary._bridge_instance,
        "document_token": primary._document_token,
    }
    assert calls == ["song"]
    assert unregister_primary(primary) is True
    with pytest.raises(RuntimeError, match="Exactly one"):
        observe_primary_context(song)


def test_same_named_replacement_and_equality_failure_cannot_join_original_song():
    """A shared name never supplies native identity, and comparison errors stay unavailable."""
    original = SimpleNamespace(name="SUNNY_HOST_QUALIFICATION_LITERAL")

    class DistinctSong:
        name = "SUNNY_HOST_QUALIFICATION_LITERAL"

        def __eq__(self, other):
            return False

    primary = registry(DistinctSong())
    assert register_primary(primary)
    with pytest.raises(RuntimeError, match="same native Song"):
        observe_primary_context(original)

    class BrokenSong:
        def __eq__(self, other):
            raise ValueError("Native equality is unavailable")

    primary._surface.song = lambda: BrokenSong()
    with pytest.raises(RuntimeError, match="native identity is unavailable"):
        observe_primary_context(original)


def test_native_equivalent_proxy_handles_can_join_without_python_identity():
    """Actual native equality can identify separately wrapped handles."""

    class SongHandle:
        def __init__(self, token):
            self.token = token

        def __eq__(self, other):
            return type(other) is SongHandle and self.token == other.token

    primary = registry(SongHandle("original-native-song"))
    assert register_primary(primary)
    assert (
        observe_primary_context(SongHandle("original-native-song"))["document_token"]
        == primary._document_token
    )


def test_wrong_thread_fails_before_reading_live():
    """A socket/background observer cannot read the registry through this helper."""
    calls = []
    primary = ManagedRegistry(SimpleNamespace(song=lambda: calls.append("read") or object()))
    assert register_primary(primary)
    failures = []

    def read():
        try:
            observe_primary_context(object())
        except RuntimeError as error:
            failures.append(str(error))

    child = threading.Thread(target=read)
    child.start()
    child.join(timeout=1)
    assert not child.is_alive() and failures == [
        "Primary qualification requires the Live main thread"
    ]
    assert calls == []


def test_module_reload_cannot_hide_an_active_previous_primary():
    """Both generations share the same weak lifetime state and ambiguity declines."""
    song = object()
    previous, current = registry(song), registry(song)
    assert register_primary(previous)
    path = Path(__file__).resolve().parents[2] / "remote_script/Sunny/native_qualification.py"
    reloaded = ModuleType("reloaded_native_qualification")
    exec(compile(path.read_bytes(), str(path), "exec"), reloaded.__dict__)
    assert reloaded.register_primary(current)
    with pytest.raises(RuntimeError, match="Exactly one"):
        reloaded.observe_primary_context(song)
    assert unregister_primary(previous)
    assert reloaded.observe_primary_context(song)["bridge_instance"] == current._bridge_instance
    del current
    gc.collect()
    with pytest.raises(RuntimeError, match="Exactly one"):
        observe_primary_context(song)


def test_unknown_existing_registration_state_is_preserved():
    """Neither bootstrap nor cleanup replaces state it cannot interpret."""
    foreign = {"version": 99, "registries": []}
    setattr(builtins, SLOT, foreign)
    primary = registry(object())
    assert register_primary(primary) is False and unregister_primary(primary) is False
    assert getattr(builtins, SLOT) is foreign
    with pytest.raises(RuntimeError, match="registration is unavailable"):
        observe_primary_context(object())
