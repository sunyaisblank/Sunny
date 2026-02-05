"""Sunny - Remote Script for Ableton Live.

Component: RSMT001A
Domain: RS (RemoteScript) | Category: MT (Main)

This Remote Script creates a socket server within Ableton Live
that receives and executes commands from the MCP server.

Installation:
1. Copy this folder to your Ableton User Library's Remote Scripts folder:
   - Windows: C:\\Users\\<User>\\Documents\\Ableton\\User Library\\Remote Scripts\\Sunny
   - macOS: ~/Music/Ableton/User Library/Remote Scripts/Sunny

2. In Ableton Live, go to Preferences → Link/Tempo/MIDI
3. Under Control Surface, select 'Sunny'
4. Set Input/Output to None (socket-based, not MIDI)

The script will start a TCP server on port 9876 (configurable).

Bounds:
    - track_index: int ∈ [0, ∞)
    - clip_slot: int ∈ [0, ∞)
    - bar: int ∈ [1, ∞)
    - value: float ∈ [0.0, 1.0] for normalized params

Error Codes:
    - 1400: Command execution failed
    - 1401: Track not found
    - 1402: Clip not found
    - 1403: Device not found
"""

from __future__ import annotations

import json
import socket
import threading
import traceback
from functools import partial

# Queue import (Python 2/3 compatible)
try:
    import Queue as queue  # Python 2
except ImportError:
    import queue  # Python 3

# Ableton Live API imports (only available inside Ableton)
try:
    from _Framework.ControlSurface import ControlSurface
    ABLETON_AVAILABLE = True
except ImportError:
    ABLETON_AVAILABLE = False
    # Mock for development outside Ableton
    class ControlSurface:
        def __init__(self, c_instance):
            self._c_instance = c_instance
        def log_message(self, msg):
            print(f"[Sunny] {msg}")
        def schedule_message(self, delay, callback):
            callback()


# Configuration
TCP_HOST = "127.0.0.1"
TCP_PORT = 9001  # Match transport.py default
BUFFER_SIZE = 65536


class Sunny(ControlSurface):
    """Remote Script for MCP server communication.
    
    Component: RSMT001A
    
    Provides socket-based control over Ableton Live, enabling
    AI agents to manipulate the DAW through the MCP protocol.
    
    Bounds:
        - TCP_PORT: int ∈ [1024, 65535]
        - BUFFER_SIZE: int ∈ [4096, 1048576]
    
    Error Codes:
        - 1400: Command execution failed
        - 1401: Invalid track index
        - 1402: Invalid clip slot
    """
    
    def __init__(self, c_instance):
        """Initialize the Remote Script.
        
        Args:
            c_instance: Ableton's control surface instance
        """
        super().__init__(c_instance)
        
        self._server_socket = None
        self._server_thread = None
        self._running = False
        
        # Response queue for main thread -> socket thread communication
        self._pending_responses = {}
        self._response_lock = threading.Lock()
        self._request_counter = 0
        
        # Command handlers
        self._handlers = {
            # Session
            "get_session_info": self._get_session_info,
            "set_tempo": self._set_tempo,
            "set_time_signature": self._set_time_signature,
            "start_playback": self._start_playback,
            "stop_playback": self._stop_playback,
            
            # Tracks
            "create_midi_track": self._create_midi_track,
            "create_audio_track": self._create_audio_track,
            "delete_track": self._delete_track,
            "get_track_info": self._get_track_info,
            "set_track_name": self._set_track_name,
            "set_track_volume": self._set_track_volume,
            "set_track_pan": self._set_track_pan,
            "set_track_mute": self._set_track_mute,
            "set_track_solo": self._set_track_solo,
            
            # Clips
            "create_clip": self._create_clip,
            "add_notes_to_clip": self._add_notes_to_clip,
            "get_clip_notes": self._get_clip_notes,
            "get_clip_info": self._get_clip_info,
            "set_clip_name": self._set_clip_name,
            "fire_clip": self._fire_clip,
            "stop_clip": self._stop_clip,
            
            # Devices
            "load_device": self._load_device,
            "get_device_parameters": self._get_device_parameters,
            "set_device_parameter": self._set_device_parameter,
            
            # Automation
            "get_clip_automation": self._get_clip_automation,
            "set_clip_automation": self._set_clip_automation,
            "clear_clip_automation": self._clear_clip_automation,
            
            # Arrangement
            "place_clip_in_arrangement": self._place_clip_in_arrangement,
            "create_locator": self._create_locator,
            "get_locators": self._get_locators,
            "jump_to_locator": self._jump_to_locator,
            "set_loop_region": self._set_loop_region,
            "jump_to_bar": self._jump_to_bar,
            "duplicate_clip_in_arrangement": self._duplicate_clip_in_arrangement,
            
            # Browser
            "get_browser_tree": self._get_browser_tree,
            "get_browser_items_at_path": self._get_browser_items_at_path,
            "load_browser_item": self._load_browser_item,
            "discover_plugin_presets": self._discover_plugin_presets,
        }
        
        self._start_server()
        self.log_message("Sunny Remote Script initialized")
    
    def disconnect(self):
        """Called when the script is unloaded."""
        self._stop_server()
        super().disconnect()
    
    # =========================================================================
    # Server Management
    # =========================================================================
    
    def _start_server(self):
        """Start the TCP server in a background thread."""
        try:
            self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._server_socket.bind((TCP_HOST, TCP_PORT))
            self._server_socket.listen(1)
            self._server_socket.settimeout(1.0)
            
            self._running = True
            self._server_thread = threading.Thread(target=self._server_loop, daemon=True)
            self._server_thread.start()
            
            self.log_message(f"Server listening on {TCP_HOST}:{TCP_PORT}")
        except Exception as e:
            self.log_message(f"Failed to start server: {e}")
    
    def _stop_server(self):
        """Stop the TCP server."""
        self._running = False
        
        if self._server_socket:
            try:
                self._server_socket.close()
            except Exception:
                pass
            self._server_socket = None
        
        self.log_message("Server stopped")
    
    def _server_loop(self):
        """Main server loop - accepts and handles connections."""
        while self._running:
            try:
                client_socket, addr = self._server_socket.accept()
                self.log_message(f"Client connected from {addr}")
                
                try:
                    self._handle_client(client_socket)
                finally:
                    client_socket.close()
                    
            except socket.timeout:
                continue
            except Exception as e:
                if self._running:
                    self.log_message(f"Server error: {e}")
    
    def _handle_client(self, client_socket):
        """Handle communication with a connected client.
        
        Uses Queue-based synchronization for reliable main-thread execution,
        following the proven pattern from ableton-mcp-extended.
        
        Args:
            client_socket: Connected client socket
        """
        # No timeout - block until data arrives (prevents premature closure)
        client_socket.settimeout(None)
        buffer = ""
        
        self.log_message("Client handler started")
        
        while self._running:
            try:
                # Receive data
                data = client_socket.recv(BUFFER_SIZE)
                
                if not data:
                    self.log_message("Client disconnected (no data)")
                    break
                
                # Accumulate data in buffer (handles partial JSON)
                try:
                    buffer += data.decode("utf-8")
                except UnicodeDecodeError as e:
                    self.log_message(f"Unicode decode error: {e}")
                    buffer = ""  # Reset on decode error
                    continue
                
                # Try to parse complete JSON from buffer
                try:
                    request = json.loads(buffer)
                    buffer = ""  # Clear buffer on successful parse
                except (ValueError, json.JSONDecodeError):
                    # Incomplete JSON, continue receiving
                    continue
                
                # Process command (support both 'type' and 'command' keys for compatibility)
                command = request.get("type") or request.get("command")
                params = request.get("params", {})
                request_id = request.get("id")
                
                self.log_message(f"Received command: {command}")
                
                if command in self._handlers:
                    # Use Queue for thread-safe communication (more reliable than Event)
                    response_queue = queue.Queue()
                    
                    # Capture command in closure to avoid late binding issues
                    def make_handler(cmd, prms):
                        def execute_on_main_thread():
                            try:
                                result = self._handlers[cmd](prms)
                                response_queue.put({"status": "success", "result": result})
                            except Exception as e:
                                self.log_message(f"Handler error: {e}\n{traceback.format_exc()}")
                                response_queue.put({"status": "error", "message": str(e)})
                        return execute_on_main_thread
                    
                    # Schedule on main thread
                    try:
                        self.schedule_message(0, make_handler(command, params))
                    except AssertionError:
                        # Already on main thread, execute directly
                        make_handler(command, params)()
                    
                    # Wait for response with timeout
                    try:
                        response = response_queue.get(timeout=30.0)
                        if response.get("status") == "error":
                            self._send_error(client_socket, response.get("message", "Unknown error"), request_id)
                        else:
                            self._send_response(client_socket, response.get("result"), request_id)
                    except queue.Empty:
                        self.log_message(f"Timeout waiting for {command}")
                        self._send_error(client_socket, "Command execution timeout", request_id)
                else:
                    self._send_error(client_socket, f"Unknown command: {command}", request_id)
                    
            except socket.timeout:
                # This shouldn't happen with settimeout(None), but handle gracefully
                continue
            except Exception as e:
                self.log_message(f"Client error: {e}\n{traceback.format_exc()}")
                break
        
        self.log_message("Client handler stopped")
    
    def _send_response(self, client_socket, result, request_id=None):
        """Send success response to client.

        Args:
            client_socket: Client socket
            result: Response data
            request_id: Optional request ID for correlation
        """
        response = {
            "status": "success",
            "result": result,
        }
        if request_id is not None:
            response["id"] = request_id
        try:
            # Add newline delimiter for line-based protocol
            client_socket.sendall((json.dumps(response) + "\n").encode("utf-8"))
        except Exception as e:
            self.log_message(f"Send error: {e}")

    def _send_error(self, client_socket, message, request_id=None):
        """Send error response to client.

        Args:
            client_socket: Client socket
            message: Error message
            request_id: Optional request ID for correlation
        """
        response = {
            "status": "error",
            "message": message,
        }
        if request_id is not None:
            response["id"] = request_id
        try:
            # Add newline delimiter for line-based protocol
            client_socket.sendall((json.dumps(response) + "\n").encode("utf-8"))
        except Exception as e:
            self.log_message(f"Send error: {e}")
    
    # =========================================================================
    # Session Handlers
    # =========================================================================
    
    def _get_session_info(self, params):
        """Get information about the current session."""
        song = self._c_instance.song()
        
        return {
            "tempo": song.tempo,
            "time_signature": {
                "numerator": song.signature_numerator,
                "denominator": song.signature_denominator,
            },
            "is_playing": song.is_playing,
            "track_count": {
                "midi": len([t for t in song.tracks if t.has_midi_input]),
                "audio": len([t for t in song.tracks if t.has_audio_input]),
                "return": len(song.return_tracks),
                "master": 1,
            },
            "scene_count": len(song.scenes),
        }
    
    def _set_tempo(self, params):
        """Set the session tempo."""
        bpm = params.get("bpm", 120.0)
        
        song = self._c_instance.song()
        song.tempo = max(20.0, min(999.0, float(bpm)))
        
        return {"tempo": song.tempo}
    
    def _set_time_signature(self, params):
        """Set the time signature."""
        numerator = params.get("numerator", 4)
        denominator = params.get("denominator", 4)
        
        song = self._c_instance.song()
        song.signature_numerator = numerator
        song.signature_denominator = denominator
        
        return {
            "numerator": song.signature_numerator,
            "denominator": song.signature_denominator,
        }
    
    def _start_playback(self, params):
        """Start playback."""
        song = self._c_instance.song()
        song.start_playing()
        return {"is_playing": True}
    
    def _stop_playback(self, params):
        """Stop playback."""
        song = self._c_instance.song()
        song.stop_playing()
        return {"is_playing": False}
    
    # =========================================================================
    # Track Handlers
    # =========================================================================
    
    def _create_midi_track(self, params):
        """Create a new MIDI track."""
        index = params.get("index", -1)
        name = params.get("name")
        
        song = self._c_instance.song()
        song.create_midi_track(index)
        
        # Get the created track
        if index == -1:
            track = song.tracks[-1]
            actual_index = len(song.tracks) - 1
        else:
            track = song.tracks[index]
            actual_index = index
        
        if name:
            track.name = name
        
        return {
            "index": actual_index,
            "name": track.name,
            "type": "midi",
        }
    
    def _create_audio_track(self, params):
        """Create a new audio track."""
        index = params.get("index", -1)
        name = params.get("name")
        
        song = self._c_instance.song()
        song.create_audio_track(index)
        
        if index == -1:
            track = song.tracks[-1]
            actual_index = len(song.tracks) - 1
        else:
            track = song.tracks[index]
            actual_index = index
        
        if name:
            track.name = name
        
        return {
            "index": actual_index,
            "name": track.name,
            "type": "audio",
        }
    
    def _delete_track(self, params):
        """Delete a track."""
        track_index = params.get("track_index")
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        name = track.name
        
        song.delete_track(track_index)
        
        return {
            "deleted_index": track_index,
            "deleted_name": name,
        }
    
    def _get_track_info(self, params):
        """Get detailed track information."""
        track_index = params.get("track_index")
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        
        return {
            "index": track_index,
            "name": track.name,
            "color": track.color,
            "is_midi": track.has_midi_input,
            "is_audio": track.has_audio_input,
            "volume": track.mixer_device.volume.value,
            "pan": track.mixer_device.panning.value,
            "mute": track.mute,
            "solo": track.solo,
            "arm": track.arm if hasattr(track, 'arm') else False,
            "device_count": len(track.devices),
            "clip_slot_count": len(track.clip_slots),
        }
    
    def _set_track_name(self, params):
        """Set track name."""
        track_index = params.get("track_index")
        name = params.get("name")
        
        if track_index is None or name is None:
            raise ValueError("track_index and name required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        track.name = name
        
        return {"name": track.name}
    
    def _set_track_volume(self, params):
        """Set track volume."""
        track_index = params.get("track_index")
        volume = params.get("volume", 0.85)  # Default ~0dB
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        track.mixer_device.volume.value = max(0.0, min(1.0, float(volume)))
        
        return {"volume": track.mixer_device.volume.value}
    
    def _set_track_pan(self, params):
        """Set track pan."""
        track_index = params.get("track_index")
        pan = params.get("pan", 0.0)  # Center
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        track.mixer_device.panning.value = max(-1.0, min(1.0, float(pan)))
        
        return {"pan": track.mixer_device.panning.value}
    
    def _set_track_mute(self, params):
        """Mute or unmute a track."""
        track_index = params.get("track_index")
        mute = params.get("mute", True)
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        track.mute = bool(mute)
        
        return {"muted": track.mute}
    
    def _set_track_solo(self, params):
        """Solo or unsolo a track."""
        track_index = params.get("track_index")
        solo = params.get("solo", True)
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        track.solo = bool(solo)
        
        return {"soloed": track.solo}
    
    # =========================================================================
    # Clip Handlers
    # =========================================================================
    
    def _create_clip(self, params):
        """Create a new MIDI clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        length = params.get("length", 4.0)
        name = params.get("name")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        slot = track.clip_slots[clip_slot]
        
        slot.create_clip(float(length))
        
        if name and slot.clip:
            slot.clip.name = name
        
        return {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "length": length,
            "name": slot.clip.name if slot.clip else None,
        }
    
    def _add_notes_to_clip(self, params):
        """Add MIDI notes to a clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        notes = params.get("notes", [])
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            raise ValueError("No clip in slot")
        
        # Convert to Ableton note format
        clip.select_all_notes()
        
        for note in notes:
            pitch = note.get("pitch", 60)
            start = note.get("start_time", 0.0)
            duration = note.get("duration", 0.5)
            velocity = note.get("velocity", 100)
            mute = note.get("mute", False)
            
            clip.set_notes(
                ((pitch, start, duration, velocity, mute),)
            )
        
        clip.deselect_all_notes()
        
        return {
            "notes_added": len(notes),
        }
    
    def _get_clip_notes(self, params):
        """Get notes from a clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            return {"notes": []}
        
        clip.select_all_notes()
        notes_data = clip.get_selected_notes()
        clip.deselect_all_notes()
        
        notes = [
            {
                "pitch": n[0],
                "start_time": n[1],
                "duration": n[2],
                "velocity": n[3],
                "mute": n[4],
            }
            for n in notes_data
        ]
        
        return {"notes": notes}
    
    def _set_clip_name(self, params):
        """Set clip name."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        name = params.get("name")
        
        if track_index is None or clip_slot is None or name is None:
            raise ValueError("track_index, clip_slot, and name required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            raise ValueError("No clip in slot")
        
        clip.name = name
        
        return {"name": clip.name}
    
    def _fire_clip(self, params):
        """Fire (play) a clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        slot = track.clip_slots[clip_slot]
        
        slot.fire()
        
        return {"fired": True}
    
    def _stop_clip(self, params):
        """Stop a playing clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        slot = track.clip_slots[clip_slot]
        
        slot.stop()
        
        return {"stopped": True}
    
    def _get_clip_info(self, params):
        """Get detailed information about a clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        slot = track.clip_slots[clip_slot]
        
        if not slot.has_clip:
            return {"has_clip": False}
        
        clip = slot.clip
        return {
            "has_clip": True,
            "name": clip.name,
            "length": clip.length,
            "start_time": clip.start_time if hasattr(clip, 'start_time') else 0,
            "end_time": clip.end_time if hasattr(clip, 'end_time') else clip.length,
            "loop_start": clip.loop_start if hasattr(clip, 'loop_start') else 0,
            "loop_end": clip.loop_end if hasattr(clip, 'loop_end') else clip.length,
            "is_midi_clip": clip.is_midi_clip,
            "is_audio_clip": clip.is_audio_clip,
            "is_playing": clip.is_playing,
            "is_recording": clip.is_recording,
            "color": clip.color if hasattr(clip, 'color') else None,
        }
    
    # =========================================================================
    # Device Handlers
    # =========================================================================
    
    def _load_device(self, params):
        """Load a device from the browser. Redirects to load_browser_item."""
        # This is a legacy handler - redirect to the proper browser item loader
        return self._load_browser_item({
            "track_index": params.get("track_index"),
            "item_uri": params.get("uri"),
        })
    
    def _get_device_parameters(self, params):
        """Get all parameters for a device."""
        track_index = params.get("track_index")
        device_index = params.get("device_index", 0)
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        
        if device_index >= len(track.devices):
            raise ValueError(f"Device index {device_index} out of range")
        
        device = track.devices[device_index]
        
        parameters = []
        for i, param in enumerate(device.parameters):
            parameters.append({
                "index": i,
                "name": param.name,
                "value": param.value,
                "min": param.min,
                "max": param.max,
                "is_quantized": param.is_quantized,
            })
        
        return {
            "device_name": device.name,
            "parameters": parameters,
        }
    
    def _set_device_parameter(self, params):
        """Set a device parameter value."""
        track_index = params.get("track_index")
        device_index = params.get("device_index", 0)
        param_index = params.get("param_index")
        value = params.get("value")
        
        if track_index is None or param_index is None or value is None:
            raise ValueError("track_index, param_index, and value required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        device = track.devices[device_index]
        param = device.parameters[param_index]
        
        # Clamp to valid range
        value = max(param.min, min(param.max, float(value)))
        param.value = value
        
        return {
            "param_name": param.name,
            "value": param.value,
        }
    
    # =========================================================================
    # Automation Handlers
    # =========================================================================
    
    def _get_clip_automation(self, params):
        """Get automation breakpoints for a clip parameter."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        parameter_path = params.get("parameter_path")
        
        if None in (track_index, clip_slot, parameter_path):
            raise ValueError("track_index, clip_slot, and parameter_path required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            return {"error": "No clip in slot", "has_envelope": False}
        
        # Parse parameter path (e.g., "devices/0/parameters/1")
        parts = parameter_path.split("/")
        if len(parts) >= 4 and parts[0] == "devices":
            device_idx = int(parts[1])
            param_idx = int(parts[3])
            
            if device_idx >= len(track.devices):
                return {"error": "Device index out of range", "has_envelope": False}
            
            device = track.devices[device_idx]
            
            if param_idx >= len(device.parameters):
                return {"error": "Parameter index out of range", "has_envelope": False}
            
            param = device.parameters[param_idx]
            
            try:
                envelope = clip.automation_envelope(param)
                if envelope:
                    return {
                        "has_envelope": True,
                        "parameter": param.name,
                        "device": device.name,
                        "value_at_start": envelope.value_at_time(0.0) if hasattr(envelope, 'value_at_time') else None,
                    }
            except Exception as e:
                self.log_message("Error getting automation envelope: {}".format(str(e)))
        
        return {"has_envelope": False}
    
    def _set_clip_automation(self, params):
        """Set automation breakpoints for a clip parameter."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        parameter_path = params.get("parameter_path")
        breakpoints = params.get("breakpoints", [])
        
        if None in (track_index, clip_slot, parameter_path):
            raise ValueError("track_index, clip_slot, and parameter_path required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            raise ValueError("No clip in slot")
        
        # Parse parameter path and get the parameter
        parts = parameter_path.split("/")
        if len(parts) >= 4 and parts[0] == "devices":
            device_idx = int(parts[1])
            param_idx = int(parts[3])
            device = track.devices[device_idx]
            param = device.parameters[param_idx]
            
            try:
                envelope = clip.automation_envelope(param)
                if envelope:
                    # Clear existing automation
                    if hasattr(envelope, 'clear_all_automation'):
                        envelope.clear_all_automation()
                    
                    # Insert new breakpoints
                    for bp in breakpoints:
                        time = float(bp.get("time", 0.0))
                        value = float(bp.get("value", 0.5))
                        # Convert normalized value (0-1) to parameter range
                        actual_value = param.min + (param.max - param.min) * value
                        if hasattr(envelope, 'insert_step'):
                            envelope.insert_step(time, 0.0, actual_value)
                    
                    return {"set": len(breakpoints), "parameter": param.name}
            except Exception as e:
                self.log_message("Error setting automation: {}".format(str(e)))
                return {"error": str(e)}
        
        return {"error": "Could not set automation - invalid parameter path"}
    
    def _clear_clip_automation(self, params):
        """Clear automation from a clip."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        parameter_path = params.get("parameter_path")
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        clip = track.clip_slots[clip_slot].clip
        
        if not clip:
            return {"error": "No clip in slot"}
        
        try:
            if parameter_path:
                # Clear specific parameter automation
                parts = parameter_path.split("/")
                if len(parts) >= 4 and parts[0] == "devices":
                    device = track.devices[int(parts[1])]
                    param = device.parameters[int(parts[3])]
                    envelope = clip.automation_envelope(param)
                    if envelope and hasattr(envelope, 'clear_all_automation'):
                        envelope.clear_all_automation()
            else:
                # Clear all automation
                if hasattr(clip, 'clear_all_envelopes'):
                    clip.clear_all_envelopes()
            
            return {"cleared": True}
        except Exception as e:
            self.log_message("Error clearing automation: {}".format(str(e)))
            return {"error": str(e)}
    
    # =========================================================================
    # Arrangement Handlers
    # =========================================================================
    
    def _place_clip_in_arrangement(self, params):
        """Place a clip from session view into arrangement."""
        track_index = params.get("track_index")
        clip_slot = params.get("clip_slot")
        bar = params.get("bar", 1)
        beat = params.get("beat", 1.0)
        
        if track_index is None or clip_slot is None:
            raise ValueError("track_index and clip_slot required")
        
        song = self._c_instance.song()
        track = song.tracks[track_index]
        slot = track.clip_slots[clip_slot]
        
        if not slot.has_clip:
            raise ValueError("No clip in slot")
        
        # Calculate beat position (bars are 1-indexed)
        time_sig_num = song.signature_numerator
        beat_position = (bar - 1) * time_sig_num + (beat - 1)
        
        try:
            # Use duplicate_clip_to_arrangement if available
            if hasattr(slot, 'duplicate_clip_to_arrangement'):
                slot.duplicate_clip_to_arrangement(beat_position)
            else:
                # Fallback: fire the clip and rely on arrangement recording
                self.log_message("duplicate_clip_to_arrangement not available")
                return {"error": "duplicate_clip_to_arrangement not available in this Live version"}
            
            return {
                "placed": True,
                "bar": bar,
                "beat": beat,
                "position_beats": beat_position,
            }
        except Exception as e:
            self.log_message("Error placing clip: {}".format(str(e)))
            return {"error": str(e)}
    
    def _create_locator(self, params):
        """Create a locator at a specific position."""
        name = params.get("name")
        bar = params.get("bar", 1)
        beat = params.get("beat", 1.0)
        
        if not name:
            raise ValueError("name required")
        
        song = self._c_instance.song()
        time_sig_num = song.signature_numerator
        beat_position = (bar - 1) * time_sig_num + (beat - 1)
        
        try:
            # Create cue point
            if hasattr(song, 'set_or_delete_cue'):
                locator = song.set_or_delete_cue(beat_position)
                if locator and hasattr(locator, 'name'):
                    locator.name = name
            
            return {"created": True, "name": name, "bar": bar, "beat": beat}
        except Exception as e:
            self.log_message("Error creating locator: {}".format(str(e)))
            return {"error": str(e)}
    
    def _get_locators(self, params):
        """Get all locators in arrangement."""
        song = self._c_instance.song()
        locators = []
        
        try:
            if hasattr(song, 'cue_points'):
                for cue in song.cue_points:
                    locators.append({
                        "name": cue.name if hasattr(cue, 'name') else "",
                        "time": cue.time if hasattr(cue, 'time') else 0,
                    })
        except Exception as e:
            self.log_message("Error getting locators: {}".format(str(e)))
        
        return {"locators": locators}
    
    def _jump_to_locator(self, params):
        """Jump to a named locator."""
        name = params.get("name")
        
        if not name:
            raise ValueError("name required")
        
        song = self._c_instance.song()
        
        try:
            if hasattr(song, 'cue_points'):
                for cue in song.cue_points:
                    if hasattr(cue, 'name') and cue.name == name:
                        song.current_song_time = cue.time
                        return {"jumped": True, "name": name, "time": cue.time}
        except Exception as e:
            self.log_message("Error jumping to locator: {}".format(str(e)))
            return {"error": str(e)}
        
        return {"jumped": False, "error": "Locator '{}' not found".format(name)}
    
    def _set_loop_region(self, params):
        """Set the loop region in arrangement."""
        start_bar = params.get("start_bar", 1)
        end_bar = params.get("end_bar", 5)
        start_beat = params.get("start_beat", 1.0)
        end_beat = params.get("end_beat", 1.0)
        
        song = self._c_instance.song()
        time_sig = song.signature_numerator
        
        start_pos = (start_bar - 1) * time_sig + (start_beat - 1)
        end_pos = (end_bar - 1) * time_sig + (end_beat - 1)
        
        try:
            song.loop_start = start_pos
            song.loop_length = max(1, end_pos - start_pos)
            song.loop = True
            
            return {
                "loop_start": song.loop_start,
                "loop_length": song.loop_length,
                "loop_enabled": song.loop,
            }
        except Exception as e:
            self.log_message("Error setting loop: {}".format(str(e)))
            return {"error": str(e)}
    
    def _jump_to_bar(self, params):
        """Jump to a specific bar position."""
        bar = params.get("bar", 1)
        beat = params.get("beat", 1.0)
        
        song = self._c_instance.song()
        time_sig = song.signature_numerator
        position = (bar - 1) * time_sig + (beat - 1)
        
        try:
            song.current_song_time = position
            return {"position": position, "bar": bar, "beat": beat}
        except Exception as e:
            self.log_message("Error jumping to bar: {}".format(str(e)))
            return {"error": str(e)}
    
    def _duplicate_clip_in_arrangement(self, params):
        """Duplicate a section of arrangement."""
        track_index = params.get("track_index")
        source_bar = params.get("source_bar", 1)
        target_bar = params.get("target_bar", 5)
        length_bars = params.get("length_bars", 4)
        
        if track_index is None:
            raise ValueError("track_index required")
        
        song = self._c_instance.song()
        time_sig = song.signature_numerator
        
        source_start = (source_bar - 1) * time_sig
        source_length = length_bars * time_sig
        target_start = (target_bar - 1) * time_sig
        
        try:
            # Try to use arrangement duplication API
            if hasattr(song, 'duplicate_scene_to_arrangement'):
                song.duplicate_scene_to_arrangement(source_start, source_length, target_start)
            else:
                return {"error": "Arrangement duplication not available in this Live version"}
            
            return {
                "duplicated": True,
                "source_bar": source_bar,
                "target_bar": target_bar,
                "length_bars": length_bars,
            }
        except Exception as e:
            self.log_message("Error duplicating in arrangement: {}".format(str(e)))
            return {"error": str(e)}
    
    # =========================================================================
    # Browser Handlers
    # =========================================================================
    
    def _get_browser_tree(self, params):
        """Get a simplified tree of browser categories.
        
        Args:
            params: Dict with optional 'category_type' ('all', 'instruments', 
                   'sounds', 'drums', 'audio_effects', 'midi_effects')
        
        Returns:
            Dictionary with browser tree structure
        """
        category_type = params.get("category_type", "all")
        
        app = self.application()
        if not app:
            raise RuntimeError("Could not access Live application")
        
        if not hasattr(app, 'browser') or app.browser is None:
            raise RuntimeError("Browser is not available")
        
        browser = app.browser
        browser_attrs = [attr for attr in dir(browser) if not attr.startswith('_')]
        
        result = {
            "type": category_type,
            "categories": [],
            "available_categories": browser_attrs,
        }
        
        def process_item(item, depth=0, max_depth=2):
            """Process a browser item and its children (limited depth)."""
            if not item:
                return None
            
            item_info = {
                "name": getattr(item, 'name', 'Unknown'),
                "is_folder": hasattr(item, 'children') and bool(item.children),
                "is_device": getattr(item, 'is_device', False),
                "is_loadable": getattr(item, 'is_loadable', False),
                "uri": getattr(item, 'uri', None),
                "children": [],
            }
            
            # Add children up to max_depth
            if depth < max_depth and hasattr(item, 'children'):
                for child in item.children:
                    child_info = process_item(child, depth + 1, max_depth)
                    if child_info:
                        item_info["children"].append(child_info)
            
            return item_info
        
        # Extended category map - covers ALL Ableton browser categories
        category_map = {
            # Core categories
            "instruments": ("instruments", "Instruments"),
            "sounds": ("sounds", "Sounds"),
            "drums": ("drums", "Drums"),
            "audio_effects": ("audio_effects", "Audio Effects"),
            "midi_effects": ("midi_effects", "MIDI Effects"),
            # Max for Live
            "max_for_live": ("max_for_live", "Max for Live"),
            # Plug-ins (VST/AU)
            "plugins": ("plugins", "Plug-ins"),
            "plug_ins": ("plug_ins", "Plug-ins"),
            # Clips and Samples
            "clips": ("clips", "Clips"),
            "samples": ("samples", "Samples"),
            # Grooves and Tunings
            "grooves": ("grooves", "Grooves"),
            "tunings": ("tunings", "Tunings"),
            # Templates
            "templates": ("templates", "Templates"),
            # Modulators
            "modulators": ("modulators", "Modulators"),
            # User Library
            "user_library": ("user_library", "User Library"),
            # Packs
            "packs": ("packs", "Packs"),
            # Current Project
            "current_project": ("current_project", "Current Project"),
        }
        
        if category_type == "all":
            # Use dynamic discovery - process all known categories plus any others
            categories_to_process = list(category_map.keys())
        else:
            # Process specific category
            categories_to_process = [category_type] if category_type in category_map else [category_type]
        
        # Process known categories
        for cat_key in categories_to_process:
            if cat_key in category_map:
                attr_name, display_name = category_map[cat_key]
            else:
                attr_name, display_name = cat_key, cat_key.replace('_', ' ').title()
            
            if hasattr(browser, attr_name):
                try:
                    category_item = getattr(browser, attr_name)
                    # Check if it's a valid browser item (has children or name)
                    if hasattr(category_item, 'children') or hasattr(category_item, 'name'):
                        category_info = process_item(category_item, 0, 2)
                        if category_info:
                            category_info["name"] = display_name
                            result["categories"].append(category_info)
                except Exception as e:
                    self.log_message(f"Error processing {attr_name}: {e}")
        
        # Also discover any additional browser attributes not in our map (for category_type="all")
        if category_type == "all":
            processed_attrs = set(category_map.keys())
            for attr in browser_attrs:
                if attr not in processed_attrs and not attr.startswith(('load', 'filter', 'hotswap')):
                    try:
                        item = getattr(browser, attr)
                        if hasattr(item, 'children') or hasattr(item, 'name'):
                            category_info = process_item(item, 0, 2)
                            if category_info:
                                category_info["name"] = attr.replace('_', ' ').title()
                                result["categories"].append(category_info)
                    except Exception:
                        pass  # Skip non-browsable attributes
        
        self.log_message(f"Browser tree: {len(result['categories'])} categories")
        return result
    
    def _get_browser_items_at_path(self, params):
        """Get browser items at a specific path.
        
        Args:
            params: Dict with 'path' in format "category/folder/subfolder"
        
        Returns:
            Dictionary with items at the specified path
        """
        path = params.get("path", "")
        
        if not path:
            raise ValueError("Path is required")
        
        app = self.application()
        if not app:
            raise RuntimeError("Could not access Live application")
        
        if not hasattr(app, 'browser') or app.browser is None:
            raise RuntimeError("Browser is not available")
        
        browser = app.browser
        path_parts = path.split("/")
        
        if not path_parts:
            raise ValueError("Invalid path")
        
        # Extended category map - same as _get_browser_tree
        root_category = path_parts[0].lower().replace(' ', '_')
        category_map = {
            "instruments": "instruments",
            "sounds": "sounds",
            "drums": "drums",
            "audio_effects": "audio_effects",
            "midi_effects": "midi_effects",
            "max_for_live": "max_for_live",
            "plugins": "plugins",
            "plug_ins": "plug_ins",
            "clips": "clips",
            "samples": "samples",
            "grooves": "grooves",
            "tunings": "tunings",
            "templates": "templates",
            "modulators": "modulators",
            "user_library": "user_library",
            "packs": "packs",
            "current_project": "current_project",
        }
        
        # Try category map first, then try direct attribute access
        attr_name = category_map.get(root_category, root_category)
        if not hasattr(browser, attr_name):
            # Get all available browser categories for error message
            browser_attrs = [attr for attr in dir(browser) if not attr.startswith('_')]
            available = [a for a in browser_attrs if hasattr(getattr(browser, a, None), 'children') or hasattr(getattr(browser, a, None), 'name')]
            return {
                "path": path,
                "error": f"Unknown category: {root_category}",
                "available_categories": available,
                "items": [],
            }
        
        current_item = getattr(browser, attr_name)
        
        # Navigate through the path
        for i in range(1, len(path_parts)):
            part = path_parts[i]
            if not part:
                continue
            
            if not hasattr(current_item, 'children'):
                return {
                    "path": path,
                    "error": f"Item at '{'/'.join(path_parts[:i])}' has no children",
                    "items": [],
                }
            
            found = False
            for child in current_item.children:
                if hasattr(child, 'name') and child.name.lower() == part.lower():
                    current_item = child
                    found = True
                    break
            
            if not found:
                return {
                    "path": path,
                    "error": f"Path part '{part}' not found",
                    "items": [],
                }
        
        # Get items at current path
        items = []
        if hasattr(current_item, 'children'):
            for child in current_item.children:
                items.append({
                    "name": getattr(child, 'name', 'Unknown'),
                    "is_folder": hasattr(child, 'children') and bool(child.children),
                    "is_device": getattr(child, 'is_device', False),
                    "is_loadable": getattr(child, 'is_loadable', False),
                    "uri": getattr(child, 'uri', None),
                })
        
        self.log_message(f"Browser path '{path}': {len(items)} items")
        return {
            "path": path,
            "name": getattr(current_item, 'name', 'Unknown'),
            "uri": getattr(current_item, 'uri', None),
            "is_folder": hasattr(current_item, 'children') and bool(current_item.children),
            "is_loadable": getattr(current_item, 'is_loadable', False),
            "items": items,
        }
    
    def _load_browser_item(self, params):
        """Load a browser item onto a track by its URI.
        
        Args:
            params: Dict with 'track_index' and 'item_uri'
        
        Returns:
            Dictionary with load result
        """
        track_index = params.get("track_index")
        item_uri = params.get("item_uri")
        
        if track_index is None:
            raise ValueError("track_index is required")
        if not item_uri:
            raise ValueError("item_uri is required")
        
        song = self._c_instance.song()
        
        if track_index < 0 or track_index >= len(song.tracks):
            raise IndexError(f"Track index {track_index} out of range")
        
        track = song.tracks[track_index]
        
        app = self.application()
        if not app:
            raise RuntimeError("Could not access Live application")
        
        # Find the browser item by URI
        item = self._find_browser_item_by_uri(app.browser, item_uri)
        
        if not item:
            raise ValueError(f"Browser item with URI '{item_uri}' not found")
        
        # Get devices before loading
        devices_before = [d.name for d in track.devices]
        
        # Select the track and load the item
        song.view.selected_track = track
        app.browser.load_item(item)
        
        # Get devices after loading
        devices_after = [d.name for d in track.devices]
        new_devices = [d for d in devices_after if d not in devices_before]
        
        self.log_message(f"Loaded '{getattr(item, 'name', 'item')}' on track {track_index}")
        return {
            "loaded": True,
            "item_name": getattr(item, 'name', 'Unknown'),
            "track_index": track_index,
            "track_name": track.name,
            "uri": item_uri,
            "devices_after": devices_after,
            "new_devices": new_devices,
        }
    
    def _find_browser_item_by_uri(self, browser_or_item, uri, max_depth=10, current_depth=0):
        """Recursively find a browser item by its URI.
        
        Args:
            browser_or_item: Browser or browser item to search
            uri: URI to find
            max_depth: Maximum recursion depth
            current_depth: Current recursion depth
        
        Returns:
            Browser item if found, None otherwise
        """
        # Check if this is the item we're looking for
        if hasattr(browser_or_item, 'uri') and browser_or_item.uri == uri:
            return browser_or_item
        
        # Stop if max depth reached
        if current_depth >= max_depth:
            return None
        
        # Check root categories if this is the browser
        if hasattr(browser_or_item, 'instruments'):
            # Core categories
            categories = [
                browser_or_item.instruments,
                browser_or_item.sounds,
                browser_or_item.drums,
                browser_or_item.audio_effects,
                browser_or_item.midi_effects,
            ]
            # Extended categories for VST/AU/Max for Live support
            extended_attrs = ['plugins', 'plug_ins', 'max_for_live', 'packs', 'user_library']
            for attr in extended_attrs:
                if hasattr(browser_or_item, attr):
                    cat = getattr(browser_or_item, attr, None)
                    if cat is not None:
                        categories.append(cat)
            
            for category in categories:
                item = self._find_browser_item_by_uri(category, uri, max_depth, current_depth + 1)
                if item:
                    return item
            return None
        
        # Check children
        if hasattr(browser_or_item, 'children') and browser_or_item.children:
            for child in browser_or_item.children:
                item = self._find_browser_item_by_uri(child, uri, max_depth, current_depth + 1)
                if item:
                    return item
        
        return None
    
    def _discover_plugin_presets(self, params):
        """Discover VST/AU/NKS plugin presets in the browser.
        
        Scans the plugins category to find all available plugin presets,
        including NKS-compatible instruments from Native Instruments,
        Spectrasonics, Arturia, and other manufacturers.
        
        Args:
            params: Dict with optional:
                - manufacturer_filter: Filter by manufacturer name (partial match)
                - max_depth: How deep to scan (default: 4)
                - include_folders: Include folder structure (default: False)
        
        Returns:
            Dictionary with discovered plugins organized by manufacturer
        """
        manufacturer_filter = params.get("manufacturer_filter", "").lower()
        max_depth = params.get("max_depth", 4)
        include_folders = params.get("include_folders", False)
        
        app = self.application()
        if not app:
            raise RuntimeError("Could not access Live application")
        
        if not hasattr(app, 'browser') or app.browser is None:
            raise RuntimeError("Browser is not available")
        
        browser = app.browser
        
        # Collect results organized by manufacturer
        results = {
            "manufacturers": {},
            "total_presets": 0,
            "total_plugins": 0,
            "scan_depth": max_depth,
        }
        
        # Known NKS-compatible manufacturers to highlight
        nks_manufacturers = [
            "native instruments", "ni", "kontakt", "massive", "fm8", "reaktor",
            "spectrasonics", "omnisphere", "keyscape", "trilian",
            "arturia", "u-he", "output", "spitfire", "orchestral tools",
            "heavyocity", "8dio", "soundiron", "cinesamples", "audio imperia",
            "ujam", "toontrack", "xln audio", "fabfilter", "izotope",
            "waves", "softube", "plugin alliance", "slate digital",
            "lennar digital", "sylenth", "serum", "xfer", "vital",
        ]
        
        def is_nks_likely(name):
            """Check if manufacturer is likely NKS-compatible."""
            name_lower = name.lower()
            return any(nks in name_lower for nks in nks_manufacturers)
        
        def scan_item(item, path="", depth=0, manufacturer=None):
            """Recursively scan browser items."""
            if depth > max_depth:
                return
            
            if not item:
                return
            
            item_name = getattr(item, 'name', 'Unknown')
            is_folder = hasattr(item, 'children') and bool(item.children)
            is_loadable = getattr(item, 'is_loadable', False)
            uri = getattr(item, 'uri', None)
            
            current_path = "{}/{}".format(path, item_name) if path else item_name
            
            # At depth 1, items are typically manufacturers
            if depth == 1:
                manufacturer = item_name
                
                # Apply manufacturer filter if specified
                if manufacturer_filter and manufacturer_filter not in manufacturer.lower():
                    return
                
                if manufacturer not in results["manufacturers"]:
                    results["manufacturers"][manufacturer] = {
                        "name": manufacturer,
                        "is_nks_likely": is_nks_likely(manufacturer),
                        "plugins": [],
                        "presets": [],
                        "preset_count": 0,
                    }
            
            # Record loadable items (presets/plugins)
            if is_loadable and uri and manufacturer:
                preset_info = {
                    "name": item_name,
                    "path": current_path,
                    "uri": uri,
                    "is_device": getattr(item, 'is_device', False),
                }
                
                # Determine if it's a plugin or a preset
                if getattr(item, 'is_device', False):
                    results["manufacturers"][manufacturer]["plugins"].append(preset_info)
                    results["total_plugins"] += 1
                else:
                    results["manufacturers"][manufacturer]["presets"].append(preset_info)
                    results["manufacturers"][manufacturer]["preset_count"] += 1
                    results["total_presets"] += 1
            
            # Recurse into children
            if is_folder and hasattr(item, 'children'):
                for child in item.children:
                    scan_item(child, current_path, depth + 1, manufacturer)
        
        # Try to find the plugins category
        plugin_categories = []
        for attr in ['plugins', 'plug_ins']:
            if hasattr(browser, attr):
                cat = getattr(browser, attr, None)
                if cat is not None:
                    plugin_categories.append((attr, cat))
        
        if not plugin_categories:
            return {
                "error": "No plugins category found in browser",
                "available_attrs": [a for a in dir(browser) if not a.startswith('_')],
            }
        
        # Scan each plugin category
        for cat_name, category in plugin_categories:
            if hasattr(category, 'children'):
                for child in category.children:
                    scan_item(child, cat_name, depth=1)
        
        # Sort manufacturers by preset count
        sorted_manufacturers = sorted(
            results["manufacturers"].values(),
            key=lambda m: m["preset_count"],
            reverse=True
        )
        
        # Build summary
        nks_count = sum(1 for m in sorted_manufacturers if m["is_nks_likely"])
        
        return {
            "summary": {
                "total_manufacturers": len(results["manufacturers"]),
                "total_plugins": results["total_plugins"],
                "total_presets": results["total_presets"],
                "nks_compatible_manufacturers": nks_count,
                "filter_applied": manufacturer_filter if manufacturer_filter else None,
            },
            "manufacturers": sorted_manufacturers,
        }


# Entry point for Ableton
def create_instance(c_instance):
    """Factory function called by Ableton to create the control surface.
    
    Args:
        c_instance: Ableton's control surface instance
    
    Returns:
        Sunny instance
    """
    return Sunny(c_instance)
