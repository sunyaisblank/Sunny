"""Existing authority families also check the actual peer before each effect."""

from __future__ import annotations

import socket

from Sunny.native_control import PeerControl, peer_scope
from test_managed_geometry import geometry_target as geometry_fixture
from test_managed_geometry import request as geometry_request
from test_managed_realization import call, intent
from test_managed_realization import target as target_fixture


def test_managed_track_creation_disconnect_prevents_name_and_clip_writes(monkeypatch):
    """A created Track remains journaled partial, without extra setters or cleanup replay."""
    target = target_fixture.__wrapped__(monkeypatch)
    left, right = socket.socketpair()
    peer = PeerControl(left)
    peer.phase("native")
    song = target.live.song
    original = song.create_midi_track
    entered = []

    def create(index):
        entered.append(index)
        original(index)
        right.close()

    monkeypatch.setattr(song, "create_midi_track", create)
    try:
        with peer_scope(peer):
            request = intent(target)
            journal = call(target, "sunny_managed_create_clip", request)
            assert journal["outcome"] == "indeterminate" and journal["native_mutation_started"]
            assert len(song.tracks) == 2
            assert song.tracks[1].name == "2-MIDI"
            assert song.tracks[1].clip_slots[0].has_clip is False
            assert call(target, "sunny_managed_create_clip", request) == journal
        assert entered == [-1]
    finally:
        peer.close()
        left.close()
        right.close()


def test_managed_geometry_disconnect_after_first_field_preserves_literal_phases(monkeypatch):
    """The original Clip has one setter; the remaining fields never enter."""
    target = target_fixture.__wrapped__(monkeypatch)
    target = geometry_fixture.__wrapped__(target, monkeypatch)
    left, right = socket.socketpair()
    peer = PeerControl(left)
    peer.phase("native")
    target.actions["end_marker"] = lambda clip: right.close()
    try:
        with peer_scope(peer):
            request = geometry_request(target, 8.0, 3, 8)
            journal = call(target, "sunny_managed_update_clip_geometry", request)
            assert journal["outcome"] == "indeterminate" and journal["native_mutation_started"]
            assert target.calls == [("end_marker", 8.0)]
            assert target.clip.signature_numerator == 4
            assert target.clip.signature_denominator == 4
            assert call(target, "sunny_managed_update_clip_geometry", request) == journal
            operation = target.registry._operations[request["operation_id"]]
            assert operation["progress"]["started_properties"] == ["end_marker"]
            assert operation["progress"]["returned_properties"] == ["end_marker"]
    finally:
        peer.close()
        left.close()
        right.close()
