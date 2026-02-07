/**
 * @file OSCAPI001A.h
 * @brief OSC Address Space Definition
 *
 * Component: OSCAPI001A
 * Domain: IN (Infrastructure) | Category: BR (Bridge)
 *
 * Formal OSC address space mapping from Sunny operations to
 * AbletonOSC-compatible addresses. Defines the contract between
 * the C++ orchestrator and the Python Remote Script.
 */

#pragma once

namespace Sunny::Infrastructure::OscApi {

// =============================================================================
// Outgoing: C++ Orchestrator → Python Remote Script
// =============================================================================

// Song-level
constexpr auto SONG_GET_TEMPO        = "/live/song/get/tempo";
constexpr auto SONG_SET_TEMPO        = "/live/song/set/tempo";
constexpr auto SONG_GET_TIME         = "/live/song/get/current_song_time";
constexpr auto SONG_START_PLAYING    = "/live/song/start_playing";
constexpr auto SONG_STOP_PLAYING     = "/live/song/stop_playing";
constexpr auto SONG_IS_PLAYING       = "/live/song/get/is_playing";

// Track-level (args: track_index, ...)
constexpr auto TRACK_GET_VOLUME      = "/live/track/get/volume";
constexpr auto TRACK_SET_VOLUME      = "/live/track/set/volume";
constexpr auto TRACK_GET_PAN         = "/live/track/get/panning";
constexpr auto TRACK_SET_PAN         = "/live/track/set/panning";
constexpr auto TRACK_GET_MUTE        = "/live/track/get/mute";
constexpr auto TRACK_SET_MUTE        = "/live/track/set/mute";
constexpr auto TRACK_GET_NAME        = "/live/track/get/name";

// Clip slot (args: track_index, slot_index, ...)
constexpr auto CLIP_SLOT_CREATE      = "/live/clip_slot/create_clip";
constexpr auto CLIP_SLOT_DELETE      = "/live/clip_slot/delete_clip";
constexpr auto CLIP_SLOT_HAS_CLIP    = "/live/clip_slot/get/has_clip";

// Clip (args: track_index, slot_index, ...)
constexpr auto CLIP_FIRE             = "/live/clip/fire";
constexpr auto CLIP_STOP             = "/live/clip/stop";
constexpr auto CLIP_SET_NOTES        = "/live/clip/set/notes";
constexpr auto CLIP_GET_NOTES        = "/live/clip/get/notes";
constexpr auto CLIP_ADD_NOTES        = "/live/clip/add/notes";
constexpr auto CLIP_REMOVE_NOTES     = "/live/clip/remove/notes";
constexpr auto CLIP_GET_NAME         = "/live/clip/get/name";
constexpr auto CLIP_SET_NAME         = "/live/clip/set/name";
constexpr auto CLIP_GET_LENGTH       = "/live/clip/get/length";

// Device (args: track_index, device_index, ...)
constexpr auto DEVICE_GET_NAME       = "/live/device/get/name";
constexpr auto DEVICE_GET_PARAMS     = "/live/device/get/parameters/name";

// =============================================================================
// Incoming: Python Remote Script → C++ Orchestrator
// =============================================================================

constexpr auto RESPONSE_PREFIX       = "/sunny/response/";
constexpr auto LISTENER_PREFIX       = "/sunny/listen/";

// Listener callbacks (pushed from Remote Script on state changes)
constexpr auto LISTEN_TEMPO          = "/sunny/listen/tempo";
constexpr auto LISTEN_PLAYING        = "/sunny/listen/is_playing";
constexpr auto LISTEN_TRACK_VOLUME   = "/sunny/listen/track/volume";
constexpr auto LISTEN_CLIP_SLOT      = "/sunny/listen/clip_slot";

// =============================================================================
// Sunny-specific extensions
// =============================================================================

// Theory query results (C++ → Python for display purposes)
constexpr auto SUNNY_SCALE_NOTES     = "/sunny/theory/scale_notes";
constexpr auto SUNNY_CHORD_NOTES     = "/sunny/theory/chord_notes";
constexpr auto SUNNY_HARMONY_FUNC    = "/sunny/theory/harmony_function";

// Status
constexpr auto SUNNY_STATUS          = "/sunny/status";
constexpr auto SUNNY_ERROR           = "/sunny/error";

}  // namespace Sunny::Infrastructure::OscApi
