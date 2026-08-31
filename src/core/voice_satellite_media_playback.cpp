#include "voice_satellite_media_playback.h"
#include "voice_satellite_config.h"

#if (defined(VOICE_SATELLITE_MEDIA_PLAYBACK) && VOICE_SATELLITE_MEDIA_PLAYBACK) || \
    (defined(JARVIS_MEDIA_PLAYBACK) && JARVIS_MEDIA_PLAYBACK)

#include "waveshare_185c_audio.h"
#include <Audio.h>
#include <AudioBoard.h>
#include <WiFi.h>
#include <Wire.h>

#ifndef VOICE_SATELLITE_MEDIA_DEFAULT_VOLUME
#define VOICE_SATELLITE_MEDIA_DEFAULT_VOLUME 65
#endif

#ifndef VOICE_SATELLITE_MEDIA_RESUME_DELAY_MS
#define VOICE_SATELLITE_MEDIA_RESUME_DELAY_MS 700
#endif

namespace {

using namespace audio_driver;

constexpr uint8_t kI2sMclk = 2;
constexpr uint8_t kPaCtrl = 15;
constexpr uint8_t kI2sLrck = 38;
constexpr uint8_t kI2sDout = 47;
constexpr uint8_t kI2sBclk = 48;
constexpr uint8_t kEs8311Address = 0x18;

Audio* player = nullptr;
DriverPins codecPins;
AudioDriverES8311Class codec;

bool codecPinsReady = false;
bool codecReady = false;
bool configured = false;
bool boardSuspended = false;
bool playerI2sReady = false;
bool active = false;
bool paused = false;
bool buffering = false;
bool everPlayed = false;
bool interruptedForVoice = false;
bool resumePending = false;
bool pendingSeek = false;
volatile bool eofPending = false;

uint8_t volumePercent = VOICE_SATELLITE_MEDIA_DEFAULT_VOLUME;
uint32_t savedPositionS = 0;
uint32_t resumeAfterMs = 0;
uint32_t notRunningSinceMs = 0;

String playbackId;
String title;
String artist;
String streamUrl;
String pendingState;

// Board pointer — set via jarvisMediaSetBoard(), used to update the display.
Board* mediaBoard = nullptr;

// Push current playback state to the display overlay.
void notifyDisplay() {
    if (!mediaBoard) return;
    MediaInfo info;
    info.active  = active || buffering;
    info.playing = active && !paused;
    info.title   = title;
    info.artist  = artist;
    mediaBoard->showMedia(info);
}

void queueState(const char* state);

String absoluteCoreUrl(const String& value) {
    if (value.startsWith("http://") || value.startsWith("https://")) {
        return value;
    }

    String base = VOICE_SATELLITE_CORE_TLS ? "https://" : "http://";
    base += VOICE_SATELLITE_CORE_HOST;
    base += ":";
    base += String(VOICE_SATELLITE_CORE_PORT);

    if (!value.startsWith("/")) base += "/";
    base += value;
    return base;
}

const char* currentMediaState() {
    if (buffering) return "buffering";
    if (active) return paused ? "paused" : "playing";
    if (playbackId.length()) return "stopped";
    return "idle";
}

uint8_t sharedBoardVolume() {
    Waveshare185CAudio* boardAudio = Waveshare185CAudio::activeInstance();
    return boardAudio ? boardAudio->volume() : volumePercent;
}

void applyCodecVolume() {
    if (!codecReady) return;
    codec.setVolume(volumePercent);
    codec.setMute(volumePercent == 0);
}

void setSharedVolume(uint8_t percent) {
    volumePercent = static_cast<uint8_t>(constrain(static_cast<int>(percent), 0, 100));

    Waveshare185CAudio* boardAudio = Waveshare185CAudio::activeInstance();
    if (boardAudio) {
        boardAudio->setVolume(volumePercent);
        // setVolume() is the single persistent Satellite volume. While media
        // owns I2S, the board only stores desiredVolume; this media codec is
        // updated immediately below.
        volumePercent = boardAudio->volume();
    }

    applyCodecVolume();
}

bool syncVolumeFromBoard(bool announce) {
    const uint8_t next = sharedBoardVolume();
    if (next == volumePercent) return false;

    volumePercent = next;
    applyCodecVolume();
    Serial.printf("[media] gemeinsame Satellite-Lautstaerke: %u%%\n",
                  static_cast<unsigned>(volumePercent));
    if (announce) queueState(currentMediaState());
    return true;
}

void queueState(const char* state) {
    JsonDocument event;
    event["type"] = "media.state";
    event["state"] = state;

    if (playbackId.length()) event["playback_id"] = playbackId;
    if (title.length()) event["title"] = title;
    event["volume"] = volumePercent;

    pendingState = "";
    serializeJson(event, pendingState);
}

bool takeState(String& out) {
    if (!pendingState.length()) return false;
    out = pendingState;
    pendingState = "";
    return true;
}

void powerAmplifier(bool enabled) {
    pinMode(kPaCtrl, OUTPUT);
    digitalWrite(kPaCtrl, enabled ? HIGH : LOW);
}

bool suspendBoardAudio() {
    if (boardSuspended) return true;

    Waveshare185CAudio* boardAudio = Waveshare185CAudio::activeInstance();
    if (!boardAudio) {
        Serial.println("[media] Waveshare Audio Instanz nicht gefunden.");
        return false;
    }

    if (!boardAudio->suspendForMediaPlayback()) {
        Serial.println("[media] Waveshare I2S konnte nicht freigegeben werden.");
        return false;
    }

    boardSuspended = true;
    return true;
}

void restoreBoardAudio() {
    if (!boardSuspended) return;

    Waveshare185CAudio* boardAudio = Waveshare185CAudio::activeInstance();
    if (boardAudio && !boardAudio->resumeAfterMediaPlayback()) {
        Serial.println("[media] WARNUNG: Waveshare Mikrofon/WakeNet Restore fehlgeschlagen.");
    }

    boardSuspended = false;
}

bool configureCodec() {
    if (!codecPinsReady) {
        codecPins.addI2C(PinFunction::CODEC, Wire, false);
        codec.setI2CAddress(kEs8311Address);
        codecPinsReady = true;
    }

    CodecConfig cfg;
    cfg.input_device = ADC_INPUT_NONE;
    cfg.output_device = DAC_OUTPUT_ALL;
    cfg.i2s.bits = BIT_LENGTH_16BITS;
    cfg.i2s.rate = RATE_44K;
    cfg.i2s.channels = CHANNELS2;
    cfg.i2s.fmt = I2S_NORMAL;
    cfg.i2s.mode = MODE_SLAVE;

    codecReady = codec.begin(cfg, codecPins);
    if (!codecReady) {
        Serial.println("[media] ES8311 Codec-Initialisierung fuer 44.1 kHz fehlgeschlagen.");
        return false;
    }

    codec.setMute(false);
    codec.setVolume(volumePercent);
    powerAmplifier(true);
    return true;
}

void releaseCodec() {
    powerAmplifier(false);
    if (codecReady) {
        codec.setMute(true);
        codec.end();
    }
    codecReady = false;
}

void destroyPlayer(bool callStopSong = true) {
    if (!player) return;

    if (callStopSong) {
        player->stopSong();
    }

    // Audio::~Audio() releases the IDF TX channel. This is why the player is
    // intentionally created per media session instead of as a permanent global.
    delete player;
    player = nullptr;
    playerI2sReady = false;
    delay(4);
}

void releaseMediaHardware(bool stopSong = true) {
    destroyPlayer(stopSong);
    releaseCodec();
    restoreBoardAudio();
}

void clearRuntimeState() {
    active = false;
    paused = false;
    buffering = false;
    everPlayed = false;
    pendingSeek = false;
    resumePending = false;
    notRunningSinceMs = 0;
    eofPending = false;
}

void stopPlayer(bool clearTrack, bool announce) {
    releaseMediaHardware(true);
    clearRuntimeState();
    interruptedForVoice = false;
    savedPositionS = 0;

    if (announce) queueState("stopped");

    if (clearTrack) {
        playbackId = "";
        title  = "";
        artist = "";
        streamUrl = "";
    }
}

bool createPlayer() {
    if (player) destroyPlayer(true);

    // The board has already released its I2SClass at this point, therefore
    // I2S0 can be owned exclusively by ESP32-audioI2S during media playback.
    player = new Audio(I2S_NUM_0);
    if (!player) {
        Serial.println("[media] Audio Player konnte nicht angelegt werden.");
        return false;
    }

    player->setOutputSampleRate(Audio::SR_44100);

    if (!player->setPinout(kI2sBclk, kI2sLrck, kI2sDout, kI2sMclk)) {
        Serial.println("[media] ESP32-audioI2S setPinout fehlgeschlagen.");
        destroyPlayer(false);
        return false;
    }

    playerI2sReady = true;
    player->setVolumeSteps(100);
    player->setVolume(100);
    player->setConnectionTimeout(4000, 6000);
    return true;
}

bool startStream(bool resumeFromSavedPosition) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[media] Wi-Fi unavailable");
        queueState("error");
        return false;
    }

    if (!streamUrl.length()) {
        Serial.println("[media] stream URL missing");
        queueState("error");
        return false;
    }

    // Media, TTS and the local volume buttons all share the board value.
    volumePercent = sharedBoardVolume();

    if (!suspendBoardAudio()) {
        queueState("error");
        return false;
    }

    if (!configureCodec()) {
        restoreBoardAudio();
        queueState("error");
        return false;
    }

    if (!createPlayer()) {
        releaseCodec();
        restoreBoardAudio();
        queueState("error");
        return false;
    }

    buffering = true;
    active = true;
    paused = false;
    everPlayed = false;
    notRunningSinceMs = 0;
    pendingSeek = resumeFromSavedPosition && savedPositionS > 0;
    eofPending = false;

    queueState("buffering");

    Serial.printf(
        "[media] start %s%s\n",
        title.length() ? title.c_str() : "(untitled)",
        resumeFromSavedPosition ? " (resume)" : ""
    );
    Serial.printf("[media] connecting: %s\n", streamUrl.c_str());
    Serial.printf(
        "[media] Wi-Fi RSSI=%d dBm, local IP=%s\n",
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str()
    );

    if (!player->connecttohost(streamUrl.c_str())) {
        Serial.println("[media] connecttohost failed");
        releaseMediaHardware(true);
        clearRuntimeState();
        queueState("error");
        return false;
    }

    return true;
}

bool playbackIdMatches(const JsonDocument& doc) {
    const String incoming = doc["playback_id"] | "";
    return !incoming.length() || !playbackId.length() || incoming == playbackId;
}

void observeVoiceEvent(const String& type) {
    const bool ttsStart =
        type == "tts_start" ||
        type == "tts.start" ||
        type == "tts.started" ||
        type == "audio_output_start" ||
        type == "audio.output.start" ||
        type == "response.audio.start";

    if (ttsStart) {
        jarvisMediaInterruptForVoice();
        return;
    }

    const bool ttsEnd =
        type == "tts_end" ||
        type == "tts.end" ||
        type == "tts.finished" ||
        type == "audio_output_end" ||
        type == "audio.output.end" ||
        type == "response.audio.done" ||
        type == "reset";

    if (ttsEnd && interruptedForVoice && streamUrl.length()) {
        resumePending = true;
        resumeAfterMs = millis() + VOICE_SATELLITE_MEDIA_RESUME_DELAY_MS;
    }
}

void audioInfo(Audio::msg_t message) {
    if (message.msg && *message.msg) {
        Serial.printf("[audioI2S] %s\n", message.msg);
    }

    if (message.e == Audio::evt_eof) {
        // Do not delete Audio from its own audio task callback.
        eofPending = true;
    }
}

void ensureConfigured() {
    if (configured) return;
    Audio::audio_info_callback = audioInfo;
    configured = true;
}

}  // namespace

void jarvisMediaAugmentCapabilities(JsonDocument& doc) {
    ensureConfigured();

    doc["capabilities"]["media"] = true;

    JsonObject features = doc["features"].to<JsonObject>();
    features["media"] = true;

    JsonObject media = doc["media"].to<JsonObject>();
    media["enabled"] = true;
    volumePercent = sharedBoardVolume();
    media["volume"] = volumePercent;

    JsonArray formats = media["formats"].to<JsonArray>();
    formats.clear();
    formats.add("audio/mpeg");

    JsonArray controls = media["controls"].to<JsonArray>();
    controls.clear();
    controls.add("play");
    controls.add("pause");
    controls.add("resume");
    controls.add("stop");
    controls.add("volume");
}

bool jarvisMediaHandleMessage(
    const JsonDocument& doc,
    String& responseJson
) {
    ensureConfigured();
    responseJson = "";

    const String type = doc["type"] | "";
    if (!type.length()) return false;

    observeVoiceEvent(type);

    if (type == "media.state.accepted") return true;

    if (type == "media.start") {
        stopPlayer(true, false);

        playbackId = String(doc["playback_id"] | "");
        title  = String(doc["item"]["title"]  | "");
        artist = String(doc["item"]["artist"] | "");

        const String rawUrl = doc["stream"]["url"] | "";
        streamUrl = absoluteCoreUrl(rawUrl);

        Serial.printf("[media] raw stream URL: %s\n", rawUrl.c_str());
        Serial.printf(
            "[media] core target: %s:%u TLS=%u\n",
            VOICE_SATELLITE_CORE_HOST,
            static_cast<unsigned>(VOICE_SATELLITE_CORE_PORT),
            static_cast<unsigned>(VOICE_SATELLITE_CORE_TLS)
        );
        Serial.printf("[media] resolved stream URL: %s\n", streamUrl.c_str());

        if (!streamUrl.length()) {
            queueState("error");
            takeState(responseJson);
            return true;
        }

        if (doc["volume"].is<int>()) {
            setSharedVolume(static_cast<uint8_t>(
                constrain(doc["volume"].as<int>(), 0, 100)
            ));
        } else {
            volumePercent = sharedBoardVolume();
        }

        startStream(false);
        notifyDisplay();
        takeState(responseJson);
        return true;
    }

    if (type == "media.pause") {
        if (!playbackIdMatches(doc)) return true;

        if (active && !paused && player && player->pauseResume()) {
            paused = true;
            buffering = false;
            queueState("paused");
            notifyDisplay();
        }
        takeState(responseJson);
        return true;
    }

    if (type == "media.resume") {
        if (!playbackIdMatches(doc)) return true;

        if (active && paused && player && player->pauseResume()) {
            paused = false;
            queueState("playing");
            notifyDisplay();
        } else if (!active && streamUrl.length()) {
            startStream(savedPositionS > 0);
            notifyDisplay();
        }
        takeState(responseJson);
        return true;
    }

    if (type == "media.stop") {
        if (!playbackIdMatches(doc)) return true;

        stopPlayer(false, true);
        notifyDisplay();   // active=false → overlay verschwindet
        takeState(responseJson);
        return true;
    }

    if (type == "media.volume") {
        if (!playbackIdMatches(doc)) return true;

        const int requested = doc["volume"] | static_cast<int>(volumePercent);
        setSharedVolume(static_cast<uint8_t>(constrain(requested, 0, 100)));

        Serial.printf("[media] Lautstaerke vom Core: %u%%\n",
                      static_cast<unsigned>(volumePercent));
        queueState(currentMediaState());
        takeState(responseJson);
        return true;
    }

    return false;
}

void jarvisMediaLoop() {
    ensureConfigured();

    // If the hardware/UI buttons changed Waveshare185CAudio::desiredVolume,
    // immediately apply the same value to the Media ES8311 profile and report
    // it back to the Core. This keeps TTS, Media, UI and Core synchronized.
    syncVolumeFromBoard(true);

    if (eofPending) {
        eofPending = false;
        releaseMediaHardware(false);
        clearRuntimeState();
        interruptedForVoice = false;
        savedPositionS = 0;
        queueState("stopped");
        return;
    }

    if (resumePending &&
        static_cast<int32_t>(millis() - resumeAfterMs) >= 0) {
        resumePending = false;
        interruptedForVoice = false;
        startStream(true);
    }

    if (!active || paused || interruptedForVoice || !player) return;

    player->loop();

    if (player->isRunning()) {
        notRunningSinceMs = 0;

        if (pendingSeek && savedPositionS > 0) {
            if (player->setAudioPlayTime(static_cast<uint16_t>(
                    min(savedPositionS, static_cast<uint32_t>(65535))))) {
                pendingSeek = false;
            }
        }

        if (buffering || !everPlayed) {
            buffering = false;
            everPlayed = true;
            queueState("playing");
        }
        return;
    }

    if (!everPlayed) return;

    if (notRunningSinceMs == 0) {
        notRunningSinceMs = millis();
        return;
    }

    if (millis() - notRunningSinceMs > 1800) {
        releaseMediaHardware(false);
        clearRuntimeState();
        queueState("stopped");
    }
}

bool jarvisMediaPollState(String& stateJson) {
    return takeState(stateJson);
}

void jarvisMediaInterruptForVoice() {
    ensureConfigured();

    if ((!active && !paused) ||
        interruptedForVoice ||
        !streamUrl.length() ||
        !player) {
        return;
    }

    savedPositionS = player->stopSong();

    // The Voice path needs the original RX I2S back immediately.
    releaseMediaHardware(false);

    active = false;
    paused = false;
    buffering = false;
    everPlayed = false;
    pendingSeek = false;
    interruptedForVoice = true;
    resumePending = false;
    notRunningSinceMs = 0;
    eofPending = false;

    queueState("paused");

    Serial.printf(
        "[media] interrupted for voice at %lu s\n",
        static_cast<unsigned long>(savedPositionS)
    );
}

void jarvisMediaSetBoard(Board* board) {
    mediaBoard = board;
    Serial.println("[media] Board-Referenz gesetzt (Display-Overlay aktiv).");
}

#else

void jarvisMediaAugmentCapabilities(JsonDocument&) {}

bool jarvisMediaHandleMessage(
    const JsonDocument&,
    String& responseJson
) {
    responseJson = "";
    return false;
}

void jarvisMediaLoop() {}

bool jarvisMediaPollState(String& stateJson) {
    stateJson = "";
    return false;
}

void jarvisMediaInterruptForVoice() {}

void jarvisMediaSetBoard(Board*) {}

#endif