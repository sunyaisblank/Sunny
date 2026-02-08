# Max for Live UDP Device Specification

This document describes the Max for Live device that handles UDP/OSC communication for low-latency parameter modulation.

## Overview

The `Sunny_UDP.amxd` device should be placed on the **Master track** to receive UDP messages from the MCP server for real-time parameter modulation.

## Purpose

While the Remote Script handles reliable TCP commands, certain operations require ultra-low latency (<5ms):
- Real-time parameter modulation
- LFO/envelope following
- MIDI CC-style continuous control
- Live performance automation

## UDP Protocol

### Port Configuration
- **Default Port**: 9877
- **Configurable via**: UI number box in the device

### OSC Address Format

```
/track/{track_index}/device/{device_index}/param/{param_index} {value}
```

Where:
- `track_index`: 0-indexed track number
- `device_index`: 0-indexed device position on track
- `param_index`: Parameter index from device parameter list
- `value`: Normalized float 0.0 to 1.0

### Additional Addresses

```
/track/{track_index}/volume {value}     # 0.0-1.0
/track/{track_index}/pan {value}        # -1.0 to 1.0
/track/{track_index}/send/{send} {value} # 0.0-1.0
/transport/tempo {bpm}                   # 20.0-999.0
```

## Max Patch Structure

```
┌─────────────────────────────────────────────────────┐
│                 Sunny_UDP                      │
├─────────────────────────────────────────────────────┤
│                                                     │
│   ┌─────────┐                                       │
│   │ udpreceive │──▶ Port: [9877]                   │
│   │   9877    │                                     │
│   └─────────┘                                       │
│        │                                            │
│        ▼                                            │
│   ┌─────────┐                                       │
│   │ oscroute │                                      │
│   │  /track │                                       │
│   └─────────┘                                       │
│        │                                            │
│        ▼                                            │
│   ┌─────────────┐                                   │
│   │ route by    │                                   │
│   │ track index │                                   │
│   └─────────────┘                                   │
│        │                                            │
│        ▼                                            │
│   ┌────────────────────┐                            │
│   │ live.path          │                            │
│   │ live.remote~       │ ◀── For sample-accurate   │
│   └────────────────────┘     parameter control      │
│                                                     │
└─────────────────────────────────────────────────────┘
```

## Required Max Objects

1. **udpreceive** - Receive UDP packets on specified port
2. **oscroute** - Parse OSC addresses
3. **route** - Route by index values
4. **live.path** - Get Live API objects
5. **live.object** - Get/set Live API properties
6. **live.remote~** - Sample-accurate parameter control

## Implementation Notes

### live.remote~ for Sample-Accurate Control

The `live.remote~` object provides sample-accurate parameter changes, essential for modulation effects. It takes a signal input (0.0-1.0) and applies it directly to a parameter.

```max
[sig~ 0.5]
     │
     ▼
[live.remote~ id $1]  ◀── ID from live.path
```

### Parameter ID Lookup

To get the parameter ID:
```max
[live.path live_set tracks $TRACK devices $DEVICE parameters $PARAM]
     │
     ▼
[live.object]
     │
     ▼
[getid]
     │
     ▼  
[live.remote~ id $1]
```

### Error Handling

The device should:
1. Validate track/device/param indices before applying
2. Clamp values to 0.0-1.0 range
3. Log errors to Max console for debugging
4. Show activity indicator in device UI

## Device UI Layout

```
┌────────────────────────────────────────┐
│  ● Sunny UDP                      │
├────────────────────────────────────────┤
│                                        │
│  Port: [9877   ]  [◉ Active]           │
│                                        │
│  Last Message: /track/0/device/0/...   │
│  Messages: [1234  ] /sec               │
│                                        │
│  [Clear Stats]                         │
│                                        │
└────────────────────────────────────────┘
```

## Installation

1. Save `Sunny_UDP.amxd` to:
   - Windows: `C:\Users\<User>\Documents\Ableton\User Library\Presets\Audio Effects\Max Audio Effect\`
   - macOS: `~/Music/Ableton/User Library/Presets/Audio Effects/Max Audio Effect/`

2. In Ableton Live, drag the device onto the **Master track**

3. Ensure the port matches the MCP server configuration (default: 9877)

## Testing

To test the device without the MCP server:

```python
# test_udp.py
from pythonosc import udp_client

client = udp_client.SimpleUDPClient("127.0.0.1", 9877)

# Modulate first device's first parameter on first track
client.send_message("/track/0/device/0/param/1", 0.5)
```

## Performance Considerations

1. **Rate Limiting**: The device should handle up to 1000 messages/second
2. **Buffering**: Use a small message queue to smooth out bursts
3. **CPU Usage**: `live.remote~` has minimal CPU overhead
4. **Latency**: Target <5ms from UDP receive to parameter change

## Known Limitations

1. Max for Live requires Ableton Live Suite
2. Some third-party plugins may not respond to `live.remote~`
3. Parameter automation recording may not capture all rapid changes
