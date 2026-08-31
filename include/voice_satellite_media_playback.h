#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "board/board.h"

/*
 * Voice Satellite Media Playback v1
 *
 * The implementation is active only for the Waveshare profile when
 * JARVIS_MEDIA_PLAYBACK=1 is injected by platformio.ini.
 */

void jarvisMediaAugmentCapabilities(JsonDocument& doc);

/*
 * Handles inbound Core events. Returns true when the message was completely
 * consumed by the media layer. response_json contains an optional immediate
 * media.state event that should be sent over the existing Voice WebSocket.
 *
 * Voice/TTS events are observed but deliberately return false so the existing
 * Voice implementation continues to process them.
 */
bool jarvisMediaHandleMessage(
    const JsonDocument& doc,
    String& response_json
);

/* Must be called regularly from the existing protocol/client loop. */
void jarvisMediaLoop();

/*
 * Returns one queued asynchronous media.state event, if available.
 * This is used for buffering -> playing, EOF and delayed resume transitions.
 */
bool jarvisMediaPollState(String& state_json);

/*
 * Voice always has priority over music. Call this before a new
 * session.start / microphone recording is started.
 */
void jarvisMediaInterruptForVoice();

/*
 * Register the board so the media layer can update the display overlay
 * whenever playback state changes (play, pause, stop).
 * Call once from setup(), after the board is initialized.
 */
class Board;
void jarvisMediaSetBoard(Board* board);