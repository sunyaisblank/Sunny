"""Sunny - Project Snapshot System.

Component: SRSN001A
Domain: SR (Server) | Category: SN (Snapshot)

Provides automatic backup before destructive operations
to enable Creative Undo functionality.

Bounds:
    - max_snapshots: int ∈ [1, 500]
    - snapshot_id: str (8-char UUID prefix)

Error Codes:
    - 1150: Snapshot creation failed
    - 1151: Snapshot not found
    - 1152: Restore failed

State Schema:
    Snapshots capture:
    - Session settings (tempo, time signature, loop settings)
    - Track list (names, types, indices, colors)
    - Track mixer settings (volume, pan, mute, solo, sends)
    - Device chains (device names, enabled states, parameter values)
    - Clip metadata (names, lengths, colors, loop settings)
    - Note data for MIDI clips
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import uuid
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Any, Protocol, runtime_checkable

logger = logging.getLogger("sunny.snapshot")


# =============================================================================
# State Capture Types
# =============================================================================


@dataclass
class SessionState:
    """Captured session-level state."""

    tempo: float = 120.0
    time_signature_numerator: int = 4
    time_signature_denominator: int = 4
    loop_start: float = 0.0
    loop_length: float = 4.0
    is_loop_on: bool = False


@dataclass
class MixerState:
    """Captured mixer settings for a track."""

    volume: float = 0.85  # 0.0 to 1.0
    pan: float = 0.0  # -1.0 to 1.0
    muted: bool = False
    solo: bool = False
    sends: list[float] = field(default_factory=list)


@dataclass
class DeviceState:
    """Captured device state."""

    name: str = ""
    class_name: str = ""
    enabled: bool = True
    parameters: dict[str, float] = field(default_factory=dict)


@dataclass
class ClipState:
    """Captured clip state."""

    slot_index: int = 0
    name: str = ""
    color: int = 0
    length: float = 4.0
    loop_start: float = 0.0
    loop_end: float = 4.0
    is_looping: bool = True
    notes: list[dict[str, Any]] = field(default_factory=list)


@dataclass
class TrackState:
    """Captured track state."""

    index: int = 0
    name: str = ""
    track_type: str = "midi"  # midi, audio, return, master
    color: int = 0
    armed: bool = False
    mixer: MixerState = field(default_factory=MixerState)
    devices: list[DeviceState] = field(default_factory=list)
    clips: list[ClipState] = field(default_factory=list)


@dataclass
class ProjectState:
    """Complete captured project state."""

    session: SessionState = field(default_factory=SessionState)
    tracks: list[TrackState] = field(default_factory=list)
    return_tracks: list[TrackState] = field(default_factory=list)
    master_track: TrackState | None = None


# =============================================================================
# Transport Protocol for Dependency Injection
# =============================================================================


@runtime_checkable
class AbletonTransport(Protocol):
    """Protocol for Ableton communication."""

    async def send_command(self, command: str, params: dict | None = None) -> dict:
        """Send command to Ableton and return response."""
        ...

    @property
    def is_connected(self) -> bool:
        """Check if connected to Ableton."""
        ...


class SnapshotManager:
    """Manages project snapshots for safe destructive operations.

    Component: SRSN001A

    Creates timestamped snapshots before any operation that could
    result in data loss, enabling easy recovery.

    Bounds:
        - max_snapshots: int ∈ [1, 500]
        - snapshot_id: str (8-char UUID prefix)

    Error Codes:
        - 1150: Snapshot creation failed
        - 1151: Snapshot not found
        - 1152: Restore failed
    """

    def __init__(
        self,
        snapshot_dir: Path | None = None,
        max_snapshots: int | None = None,
        transport: AbletonTransport | None = None,
    ):
        """Initialize snapshot manager.

        Args:
            snapshot_dir: Directory for snapshot storage
            max_snapshots: Maximum number of snapshots to retain
            transport: Optional Ableton transport for state capture/restore
        """
        # Get configuration from environment
        default_dir = Path.home() / ".sunny" / "snapshots"
        self.snapshot_dir = snapshot_dir or Path(
            os.getenv("SUNNY_SNAPSHOT_DIR", str(default_dir))
        )
        self.max_snapshots = max_snapshots or int(
            os.getenv("SUNNY_MAX_SNAPSHOTS", "50")
        )
        self._transport = transport

        # Ensure directory exists
        self.snapshot_dir.mkdir(parents=True, exist_ok=True)

        # In-memory index of snapshots
        self._index: list[dict[str, Any]] = []
        self._load_index()

    def set_transport(self, transport: AbletonTransport) -> None:
        """Set the Ableton transport for state capture/restore.

        Args:
            transport: Ableton communication transport.
        """
        self._transport = transport
    
    def _load_index(self):
        """Load snapshot index from disk."""
        index_file = self.snapshot_dir / "index.json"
        if index_file.exists():
            try:
                with open(index_file, "r") as f:
                    self._index = json.load(f)
            except (json.JSONDecodeError, IOError) as e:
                logger.warning(f"Could not load snapshot index: {e}")
                self._index = []
    
    def _save_index(self):
        """Save snapshot index to disk."""
        index_file = self.snapshot_dir / "index.json"
        try:
            with open(index_file, "w") as f:
                json.dump(self._index, f, indent=2)
        except IOError as e:
            logger.error(f"Could not save snapshot index: {e}")
    
    async def create_snapshot(self, reason: str) -> str:
        """Create a new project snapshot.

        Args:
            reason: Description of why the snapshot was created

        Returns:
            Unique snapshot ID
        """
        snapshot_id = str(uuid.uuid4())[:8]
        timestamp = datetime.now().isoformat()

        snapshot_entry = {
            "id": snapshot_id,
            "timestamp": timestamp,
            "reason": reason,
            "file": f"{snapshot_id}.snapshot",
        }

        # Capture actual project state if transport is available
        if self._transport and self._transport.is_connected:
            state = await self._capture_state()
            state_dict = self._state_to_dict(state)
        else:
            # Offline mode - empty state
            state_dict = {}
            logger.debug("Creating snapshot in offline mode (no state captured)")

        snapshot_file = self.snapshot_dir / f"{snapshot_id}.snapshot"
        snapshot_data = {
            "id": snapshot_id,
            "timestamp": timestamp,
            "reason": reason,
            "state": state_dict,
        }

        try:
            with open(snapshot_file, "w") as f:
                json.dump(snapshot_data, f, indent=2)
        except IOError as e:
            logger.error(f"Could not save snapshot: {e}")
            raise RuntimeError(f"Failed to create snapshot: {e}")

        # Add to index
        self._index.append(snapshot_entry)

        # Enforce retention policy
        await self._enforce_retention()

        # Save index
        self._save_index()

        logger.info(f"Created snapshot {snapshot_id}: {reason}")
        return snapshot_id

    async def _capture_state(self) -> ProjectState:
        """Capture complete project state from Ableton.

        Returns:
            ProjectState containing all captured data.
        """
        state = ProjectState()

        # Capture session settings
        try:
            session_info = await self._transport.send_command("get_session_info")
            state.session = SessionState(
                tempo=session_info.get("tempo", 120.0),
                time_signature_numerator=session_info.get("time_signature", {}).get("numerator", 4),
                time_signature_denominator=session_info.get("time_signature", {}).get("denominator", 4),
                loop_start=session_info.get("loop_start", 0.0),
                loop_length=session_info.get("loop_length", 4.0),
                is_loop_on=session_info.get("is_loop_on", False),
            )
        except Exception as e:
            logger.warning(f"Could not capture session info: {e}")

        # Capture track list
        try:
            track_count = await self._transport.send_command("get_track_count")
            midi_count = track_count.get("midi", 0)
            audio_count = track_count.get("audio", 0)
            return_count = track_count.get("return", 0)

            # Capture MIDI and audio tracks
            for i in range(midi_count + audio_count):
                track_state = await self._capture_track_state(i)
                if track_state:
                    state.tracks.append(track_state)

            # Capture return tracks
            for i in range(return_count):
                track_state = await self._capture_return_track_state(i)
                if track_state:
                    state.return_tracks.append(track_state)

            # Capture master track
            state.master_track = await self._capture_master_track_state()

        except Exception as e:
            logger.warning(f"Could not capture track state: {e}")

        return state

    async def _capture_track_state(self, track_index: int) -> TrackState | None:
        """Capture state for a single track.

        Args:
            track_index: Index of the track to capture.

        Returns:
            TrackState or None if capture failed.
        """
        try:
            track_info = await self._transport.send_command(
                "get_track_info", {"track_index": track_index}
            )

            track_state = TrackState(
                index=track_index,
                name=track_info.get("name", f"Track {track_index + 1}"),
                track_type=track_info.get("type", "midi"),
                color=track_info.get("color", 0),
                armed=track_info.get("armed", False),
            )

            # Capture mixer settings
            track_state.mixer = MixerState(
                volume=track_info.get("volume", 0.85),
                pan=track_info.get("pan", 0.0),
                muted=track_info.get("muted", False),
                solo=track_info.get("solo", False),
                sends=track_info.get("sends", []),
            )

            # Capture devices
            devices_info = await self._transport.send_command(
                "get_device_list", {"track_index": track_index}
            )
            for device_info in devices_info.get("devices", []):
                device_state = DeviceState(
                    name=device_info.get("name", ""),
                    class_name=device_info.get("class_name", ""),
                    enabled=device_info.get("enabled", True),
                    parameters=device_info.get("parameters", {}),
                )
                track_state.devices.append(device_state)

            # Capture clips (for arrangement view, up to 128 slots)
            clips_info = await self._transport.send_command(
                "get_clip_slots", {"track_index": track_index}
            )
            for clip_info in clips_info.get("clips", []):
                if clip_info.get("has_clip", False):
                    clip_state = ClipState(
                        slot_index=clip_info.get("slot_index", 0),
                        name=clip_info.get("name", ""),
                        color=clip_info.get("color", 0),
                        length=clip_info.get("length", 4.0),
                        loop_start=clip_info.get("loop_start", 0.0),
                        loop_end=clip_info.get("loop_end", 4.0),
                        is_looping=clip_info.get("is_looping", True),
                        notes=clip_info.get("notes", []),
                    )
                    track_state.clips.append(clip_state)

            return track_state

        except Exception as e:
            logger.warning(f"Could not capture track {track_index}: {e}")
            return None

    async def _capture_return_track_state(self, return_index: int) -> TrackState | None:
        """Capture state for a return track."""
        try:
            track_info = await self._transport.send_command(
                "get_return_track_info", {"return_index": return_index}
            )

            return TrackState(
                index=return_index,
                name=track_info.get("name", f"Return {chr(65 + return_index)}"),
                track_type="return",
                color=track_info.get("color", 0),
                mixer=MixerState(
                    volume=track_info.get("volume", 0.85),
                    pan=track_info.get("pan", 0.0),
                    muted=track_info.get("muted", False),
                    solo=track_info.get("solo", False),
                ),
            )
        except Exception as e:
            logger.warning(f"Could not capture return track {return_index}: {e}")
            return None

    async def _capture_master_track_state(self) -> TrackState | None:
        """Capture master track state."""
        try:
            track_info = await self._transport.send_command("get_master_track_info")

            return TrackState(
                index=0,
                name="Master",
                track_type="master",
                mixer=MixerState(
                    volume=track_info.get("volume", 0.85),
                    pan=track_info.get("pan", 0.0),
                ),
            )
        except Exception as e:
            logger.warning(f"Could not capture master track: {e}")
            return None

    def _state_to_dict(self, state: ProjectState) -> dict[str, Any]:
        """Convert ProjectState to serializable dictionary."""
        return {
            "session": asdict(state.session),
            "tracks": [self._track_to_dict(t) for t in state.tracks],
            "return_tracks": [self._track_to_dict(t) for t in state.return_tracks],
            "master_track": self._track_to_dict(state.master_track) if state.master_track else None,
        }

    def _track_to_dict(self, track: TrackState) -> dict[str, Any]:
        """Convert TrackState to serializable dictionary."""
        return {
            "index": track.index,
            "name": track.name,
            "track_type": track.track_type,
            "color": track.color,
            "armed": track.armed,
            "mixer": asdict(track.mixer),
            "devices": [asdict(d) for d in track.devices],
            "clips": [asdict(c) for c in track.clips],
        }
    
    async def list_snapshots(self) -> list[dict[str, Any]]:
        """List all available snapshots.
        
        Returns:
            List of snapshot metadata dictionaries
        """
        return [
            {
                "id": s["id"],
                "timestamp": s["timestamp"],
                "reason": s["reason"],
            }
            for s in sorted(self._index, key=lambda x: x["timestamp"], reverse=True)
        ]
    
    async def get_snapshot(self, snapshot_id: str) -> dict[str, Any] | None:
        """Get snapshot metadata by ID.
        
        Args:
            snapshot_id: Snapshot ID to look up
        
        Returns:
            Snapshot metadata or None if not found
        """
        for snapshot in self._index:
            if snapshot["id"] == snapshot_id:
                return snapshot
        return None
    
    async def restore_snapshot(self, snapshot_id: str):
        """Restore project to a snapshot state.

        Args:
            snapshot_id: ID of the snapshot to restore

        Raises:
            ValueError: If snapshot not found
            RuntimeError: If restore operation fails
        """
        snapshot = await self.get_snapshot(snapshot_id)
        if not snapshot:
            raise ValueError(f"Snapshot {snapshot_id} not found")

        snapshot_file = self.snapshot_dir / snapshot["file"]
        if not snapshot_file.exists():
            raise ValueError(f"Snapshot file missing for {snapshot_id}")

        try:
            with open(snapshot_file, "r") as f:
                snapshot_data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            raise RuntimeError(f"Could not read snapshot: {e}")

        state_dict = snapshot_data.get("state", {})

        # Check if we have state to restore
        if not state_dict:
            logger.warning(f"Snapshot {snapshot_id} has no state data (offline mode snapshot)")
            return

        # Check if transport is available
        if not self._transport or not self._transport.is_connected:
            raise RuntimeError("Cannot restore: not connected to Ableton")

        # Restore state
        await self._restore_state(state_dict)

        logger.info(f"Restored snapshot {snapshot_id}")

    async def _restore_state(self, state_dict: dict[str, Any]) -> None:
        """Restore project state from dictionary.

        Args:
            state_dict: Serialized project state.
        """
        # Restore session settings first
        session = state_dict.get("session", {})
        await self._restore_session(session)

        # Restore tracks in order
        # Note: This is a best-effort restoration. We restore settings
        # but don't recreate deleted tracks or clips.
        tracks = state_dict.get("tracks", [])
        for track_data in tracks:
            await self._restore_track(track_data)

        # Restore return tracks
        return_tracks = state_dict.get("return_tracks", [])
        for track_data in return_tracks:
            await self._restore_return_track(track_data)

        # Restore master track
        master = state_dict.get("master_track")
        if master:
            await self._restore_master_track(master)

    async def _restore_session(self, session: dict[str, Any]) -> None:
        """Restore session-level settings."""
        try:
            # Restore tempo
            if "tempo" in session:
                await self._transport.send_command(
                    "set_tempo", {"tempo": session["tempo"]}
                )

            # Restore time signature
            if "time_signature_numerator" in session:
                await self._transport.send_command(
                    "set_time_signature",
                    {
                        "numerator": session["time_signature_numerator"],
                        "denominator": session.get("time_signature_denominator", 4),
                    },
                )

            # Restore loop settings
            if "loop_start" in session:
                await self._transport.send_command(
                    "set_loop",
                    {
                        "start": session["loop_start"],
                        "length": session.get("loop_length", 4.0),
                        "enabled": session.get("is_loop_on", False),
                    },
                )
        except Exception as e:
            logger.warning(f"Could not fully restore session settings: {e}")

    async def _restore_track(self, track_data: dict[str, Any]) -> None:
        """Restore settings for a single track."""
        track_index = track_data.get("index", 0)

        try:
            # Check if track exists
            track_info = await self._transport.send_command(
                "get_track_info", {"track_index": track_index}
            )
            if not track_info:
                logger.warning(f"Track {track_index} not found, skipping")
                return

            # Restore track name
            if "name" in track_data:
                await self._transport.send_command(
                    "set_track_name",
                    {"track_index": track_index, "name": track_data["name"]},
                )

            # Restore mixer settings
            mixer = track_data.get("mixer", {})
            await self._restore_mixer(track_index, mixer)

            # Restore device settings
            devices = track_data.get("devices", [])
            for i, device_data in enumerate(devices):
                await self._restore_device(track_index, i, device_data)

            # Restore clip contents
            clips = track_data.get("clips", [])
            for clip_data in clips:
                await self._restore_clip(track_index, clip_data)

        except Exception as e:
            logger.warning(f"Could not fully restore track {track_index}: {e}")

    async def _restore_mixer(self, track_index: int, mixer: dict[str, Any]) -> None:
        """Restore mixer settings for a track."""
        try:
            if "volume" in mixer:
                await self._transport.send_command(
                    "set_track_volume",
                    {"track_index": track_index, "volume": mixer["volume"]},
                )

            if "pan" in mixer:
                await self._transport.send_command(
                    "set_track_pan",
                    {"track_index": track_index, "pan": mixer["pan"]},
                )

            if "muted" in mixer:
                await self._transport.send_command(
                    "set_track_mute",
                    {"track_index": track_index, "muted": mixer["muted"]},
                )

            if "solo" in mixer:
                await self._transport.send_command(
                    "set_track_solo",
                    {"track_index": track_index, "solo": mixer["solo"]},
                )

            # Restore send levels
            sends = mixer.get("sends", [])
            for i, level in enumerate(sends):
                await self._transport.send_command(
                    "set_send_level",
                    {"track_index": track_index, "send_index": i, "level": level},
                )
        except Exception as e:
            logger.warning(f"Could not restore mixer for track {track_index}: {e}")

    async def _restore_device(
        self, track_index: int, device_index: int, device_data: dict[str, Any]
    ) -> None:
        """Restore device settings."""
        try:
            # Restore enabled state
            if "enabled" in device_data:
                await self._transport.send_command(
                    "enable_device",
                    {
                        "track_index": track_index,
                        "device_index": device_index,
                        "enabled": device_data["enabled"],
                    },
                )

            # Restore parameters
            params = device_data.get("parameters", {})
            for param_name, value in params.items():
                await self._transport.send_command(
                    "set_device_parameter_by_name",
                    {
                        "track_index": track_index,
                        "device_index": device_index,
                        "param_name": param_name,
                        "value": value,
                    },
                )
        except Exception as e:
            logger.warning(
                f"Could not restore device {device_index} on track {track_index}: {e}"
            )

    async def _restore_clip(self, track_index: int, clip_data: dict[str, Any]) -> None:
        """Restore clip contents."""
        slot_index = clip_data.get("slot_index", 0)

        try:
            # Restore clip name
            if "name" in clip_data:
                await self._transport.send_command(
                    "set_clip_name",
                    {
                        "track_index": track_index,
                        "slot_index": slot_index,
                        "name": clip_data["name"],
                    },
                )

            # Restore loop settings
            await self._transport.send_command(
                "set_clip_loop",
                {
                    "track_index": track_index,
                    "slot_index": slot_index,
                    "loop_start": clip_data.get("loop_start", 0.0),
                    "loop_end": clip_data.get("loop_end", 4.0),
                    "is_looping": clip_data.get("is_looping", True),
                },
            )

            # Restore notes if this is a MIDI clip
            notes = clip_data.get("notes", [])
            if notes:
                await self._transport.send_command(
                    "replace_clip_notes",
                    {
                        "track_index": track_index,
                        "slot_index": slot_index,
                        "notes": notes,
                    },
                )
        except Exception as e:
            logger.warning(
                f"Could not restore clip at slot {slot_index} on track {track_index}: {e}"
            )

    async def _restore_return_track(self, track_data: dict[str, Any]) -> None:
        """Restore return track settings."""
        return_index = track_data.get("index", 0)

        try:
            mixer = track_data.get("mixer", {})

            if "volume" in mixer:
                await self._transport.send_command(
                    "set_return_volume",
                    {"return_index": return_index, "volume": mixer["volume"]},
                )

            if "pan" in mixer:
                await self._transport.send_command(
                    "set_return_pan",
                    {"return_index": return_index, "pan": mixer["pan"]},
                )

            if "muted" in mixer:
                await self._transport.send_command(
                    "set_return_mute",
                    {"return_index": return_index, "muted": mixer["muted"]},
                )
        except Exception as e:
            logger.warning(f"Could not restore return track {return_index}: {e}")

    async def _restore_master_track(self, track_data: dict[str, Any]) -> None:
        """Restore master track settings."""
        try:
            mixer = track_data.get("mixer", {})

            if "volume" in mixer:
                await self._transport.send_command(
                    "set_master_volume", {"volume": mixer["volume"]}
                )

            if "pan" in mixer:
                await self._transport.send_command(
                    "set_master_pan", {"pan": mixer["pan"]}
                )
        except Exception as e:
            logger.warning(f"Could not restore master track: {e}")
    
    async def delete_snapshot(self, snapshot_id: str):
        """Delete a snapshot.
        
        Args:
            snapshot_id: ID of the snapshot to delete
        """
        snapshot = await self.get_snapshot(snapshot_id)
        if not snapshot:
            return
        
        # Remove file
        snapshot_file = self.snapshot_dir / snapshot["file"]
        if snapshot_file.exists():
            try:
                snapshot_file.unlink()
            except IOError as e:
                logger.warning(f"Could not delete snapshot file: {e}")
        
        # Remove from index
        self._index = [s for s in self._index if s["id"] != snapshot_id]
        self._save_index()
        
        logger.info(f"Deleted snapshot {snapshot_id}")
    
    async def _enforce_retention(self):
        """Enforce snapshot retention policy.
        
        Removes oldest snapshots if count exceeds maximum.
        """
        while len(self._index) > self.max_snapshots:
            # Sort by timestamp, remove oldest
            sorted_snapshots = sorted(self._index, key=lambda x: x["timestamp"])
            oldest = sorted_snapshots[0]
            await self.delete_snapshot(oldest["id"])
