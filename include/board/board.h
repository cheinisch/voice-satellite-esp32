#pragma once

#include <Arduino.h>
#include "audio/audio_io.h"

// ---------------------------------------------------------------------------
// Media overlay — used by the display layer to show playback controls.
// Defined here so board.h is the single include needed by the media layer.
// ---------------------------------------------------------------------------
struct MediaInfo {
    String title;
    String artist;
    bool   playing = false;  // true = playing, false = paused/stopped
    bool   active  = false;  // overlay visible at all
};

enum class SatelliteState {
    Booting,
    ConnectingWifi,
    ConnectingCore,
    Ready,
    Listening,
    Processing,
    Speaking,
    Muted,
    Error,
};

struct BoardCapabilities {
    bool microphone = false;
    bool speaker = false;
    bool display = false;
    bool touch = false;
    bool buttons = false;
    bool psram = false;
    bool sdcard = false;
};

class Board {
public:
    virtual ~Board() = default;
    virtual bool begin() = 0;
    virtual void loop() = 0;

    virtual const char* model() const = 0;
    virtual const char* profile() const = 0;
    virtual BoardCapabilities capabilities() const = 0;

    virtual AudioIO& audio() = 0;

    // Liefert true genau einmal pro Betätigung (Touch oder Taste).
    virtual bool consumeVoiceTrigger() = 0;
    // Optional separate UI control. Defaults to unsupported on headless boards.
    virtual bool consumeMuteToggle() { return false; }
    // Optional display branding supplied by the Core. Headless boards ignore it.
    virtual void setDisplayName(const String& name) { (void)name; }

    virtual void setState(SatelliteState state, const String& detail = String()) = 0;
    virtual void showTranscript(const String& text) = 0;
    virtual void showAssistant(const String& text) = 0;

    // Optional media overlay — headless boards provide a no-op default.
    virtual void showMedia(const MediaInfo& info) { (void)info; }
};