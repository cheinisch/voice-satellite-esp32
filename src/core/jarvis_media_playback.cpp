#include "jarvis_media_playback.h"

#if defined(JARVIS_MEDIA_PLAYBACK) && JARVIS_MEDIA_PLAYBACK

#include <Audio.h>
#include <AudioBoard.h>
#include <WiFi.h>
#include <Wire.h>

#ifndef JARVIS_CORE_HOST
#define JARVIS_CORE_HOST "127.0.0.1"
#endif

#ifndef JARVIS_CORE_PORT
#define JARVIS_CORE_PORT 8081
#endif

#ifndef JARVIS_CORE_TLS
#define JARVIS_CORE_TLS 0
#endif

#ifndef JARVIS_MEDIA_DEFAULT_VOLUME
#define JARVIS_MEDIA_DEFAULT_VOLUME 65
#endif

#ifndef JARVIS_MEDIA_I2S_PORT
#define JARVIS_MEDIA_I2S_PORT 1
#endif

#ifndef JARVIS_MEDIA_RESUME_DELAY_MS
#define JARVIS_MEDIA_RESUME_DELAY_MS 700
#endif

namespace {

using namespace audio_driver;

constexpr uint8_t kI2sMclk = 2;
constexpr uint8_t kI2cScl = 10;
constexpr uint8_t kI2cSda = 11;
constexpr uint8_t kPaCtrl = 15;
constexpr uint8_t kI2sLrck = 38;
constexpr uint8_t kI2sDout = 47;
constexpr uint8_t kI2sBclk = 48;
constexpr uint8_t kEs8311Address = 0x18;

Audio player(JARVIS_MEDIA_I2S_PORT);
DriverDeviceInfo codec_pins;
AudioDriverES8311Class codec(kEs8311Address);

bool codec_pins_ready = false;
bool codec_ready = false;
bool configured = false;
bool active = false;
bool paused = false;
bool buffering = false;
bool ever_played = false;
bool interrupted_for_voice = false;
bool resume_pending = false;
bool pending_seek = false;

uint8_t volume_percent = JARVIS_MEDIA_DEFAULT_VOLUME;
uint32_t saved_position_s = 0;
uint32_t resume_after_ms = 0;
uint32_t not_running_since_ms = 0;

String playback_id;
String title;
String stream_url;
String pending_state;

String absoluteCoreUrl(const String& value) {
    if (value.startsWith("http://") || value.startsWith("https://")) {
        return value;
    }

    String base = JARVIS_CORE_TLS ? "https://" : "http://";
    base += JARVIS_CORE_HOST;
    base += ":";
    base += String(JARVIS_CORE_PORT);

    if (!value.startsWith("/")) {
        base += "/";
    }
    base += value;
    return base;
}

void queueState(const char* state) {
    JsonDocument event;
    event["type"] = "media.state";
    event["state"] = state;

    if (playback_id.length()) {
        event["playback_id"] = playback_id;
    }
    if (title.length()) {
        event["title"] = title;
    }
    event["volume"] = volume_percent;

    pending_state = "";
    serializeJson(event, pending_state);
}

bool takeState(String& out) {
    if (!pending_state.length()) {
        return false;
    }
    out = pending_state;
    pending_state = "";
    return true;
}

void powerAmplifier(bool enabled) {
    pinMode(kPaCtrl, OUTPUT);
    digitalWrite(kPaCtrl, enabled ? HIGH : LOW);
}

bool configureCodec() {
    /*
     * The existing Waveshare board owns and initializes the shared Wire bus.
     * Do not call DriverDeviceInfo::begin() here: that would reinitialize Wire
     * and can break the display/touch/audio devices sharing GPIO10/11.
     *
     * We only give the codec driver the already-running Wire bus and ask the
     * ES8311 driver to apply its codec registers for fixed 44.1 kHz output.
     */
    if (!codec_pins_ready) {
        codec_pins.addI2C(
            PinFunction::CODEC,
            kI2cScl,
            kI2cSda,
            kEs8311Address,
            100000,
            Wire
        );
        codec.setPins(codec_pins);
        codec.setI2CAddress(kEs8311Address);
        codec_pins_ready = true;
    }

    CodecConfig cfg;
    cfg.input_device = ADC_INPUT_NONE;
    cfg.output_device = DAC_OUTPUT_ALL;
    cfg.i2s.bits = BIT_LENGTH_16BITS;
    cfg.i2s.rate = RATE_44K;
    cfg.i2s.fmt = I2S_NORMAL;

    codec_ready = codec.setConfig(cfg);
    if (!codec_ready) {
        Serial.println("[media] ES8311 codec configuration failed");
        return false;
    }

    codec.setMute(false);
    codec.setVolume(volume_percent);
    powerAmplifier(true);
    return true;
}

void releaseCodec() {
    powerAmplifier(false);
    if (codec_ready) {
        codec.setMute(true);
        codec.end();
    }
    codec_ready = false;
}

void stopPlayer(bool clear_track, bool announce) {
    if (active || buffering || paused || interrupted_for_voice) {
        player.stopSong();
    }

    active = false;
    paused = false;
    buffering = false;
    ever_played = false;
    pending_seek = false;
    resume_pending = false;
    interrupted_for_voice = false;
    not_running_since_ms = 0;
    saved_position_s = 0;

    releaseCodec();

    if (announce) {
        queueState("stopped");
    }

    if (clear_track) {
        playback_id = "";
        title = "";
        stream_url = "";
    }
}

bool startStream(bool resume_from_saved_position) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[media] Wi-Fi unavailable");
        queueState("error");
        return false;
    }

    if (!stream_url.length()) {
        Serial.println("[media] stream URL missing");
        queueState("error");
        return false;
    }

    if (!configureCodec()) {
        queueState("error");
        return false;
    }

    /*
     * MP3 source rates are normalized to 44.1 kHz so the ES8311 can stay on
     * one deterministic clock profile while a track is playing.
     */
    player.setOutputSampleRate(Audio::SR_44100);
    player.setPinout(kI2sBclk, kI2sLrck, kI2sDout, kI2sMclk);

    // Keep digital playback at full scale. User/media volume is applied by ES8311.
    player.setVolumeSteps(100);
    player.setVolume(100);
    player.setConnectionTimeout(4000, 6000);

    buffering = true;
    active = true;
    paused = false;
    ever_played = false;
    not_running_since_ms = 0;
    pending_seek = resume_from_saved_position && saved_position_s > 0;

    queueState("buffering");

    Serial.printf(
        "[media] start %s%s\n",
        title.length() ? title.c_str() : "(untitled)",
        resume_from_saved_position ? " (resume)" : ""
    );

    if (!player.connecttohost(stream_url.c_str())) {
        Serial.println("[media] connecttohost failed");
        buffering = false;
        active = false;
        pending_seek = false;
        releaseCodec();
        queueState("error");
        return false;
    }

    return true;
}

bool playbackIdMatches(const JsonDocument& doc) {
    const String incoming = doc["playback_id"] | "";
    return !incoming.length() || !playback_id.length() || incoming == playback_id;
}

void observeVoiceEvent(const String& type) {
    if (type == "tts.start") {
        jarvisMediaInterruptForVoice();
        return;
    }

    if ((type == "tts.end" || type == "reset") &&
        interrupted_for_voice &&
        stream_url.length()) {
        resume_pending = true;
        resume_after_ms = millis() + JARVIS_MEDIA_RESUME_DELAY_MS;
    }
}

void audioInfo(Audio::msg_t message) {
    if (message.e == Audio::evt_eof && active && !paused && !interrupted_for_voice) {
        active = false;
        buffering = false;
        ever_played = false;
        releaseCodec();
        queueState("stopped");
    }
}

void ensureConfigured() {
    if (configured) {
        return;
    }
    Audio::audio_info_callback = audioInfo;
    configured = true;
}

}  // namespace

void jarvisMediaAugmentCapabilities(JsonDocument& doc) {
    ensureConfigured();

    JsonObject features = doc["features"].to<JsonObject>();
    features["media"] = true;

    JsonObject media = doc["media"].to<JsonObject>();
    media["enabled"] = true;

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
    String& response_json
) {
    ensureConfigured();
    response_json = "";

    const String type = doc["type"] | "";
    if (!type.length()) {
        return false;
    }

    observeVoiceEvent(type);

    if (type == "media.state.accepted") {
        return true;
    }

    if (type == "media.start") {
        stopPlayer(true, false);

        playback_id = String(doc["playback_id"] | "");
        title = String(doc["item"]["title"] | "");

        const String raw_url = doc["stream"]["url"] | "";
        stream_url = absoluteCoreUrl(raw_url);

        if (!stream_url.length()) {
            queueState("error");
            takeState(response_json);
            return true;
        }

        if (doc["volume"].is<int>()) {
            volume_percent = constrain(
                static_cast<int>(doc["volume"].as<int>()),
                0,
                100
            );
        }

        startStream(false);
        takeState(response_json);
        return true;
    }

    if (type == "media.pause") {
        if (!playbackIdMatches(doc)) {
            return true;
        }

        if (active && !paused && player.pauseResume()) {
            paused = true;
            buffering = false;
            queueState("paused");
        }
        takeState(response_json);
        return true;
    }

    if (type == "media.resume") {
        if (!playbackIdMatches(doc)) {
            return true;
        }

        if (active && paused && player.pauseResume()) {
            paused = false;
            queueState("playing");
        } else if (!active && stream_url.length()) {
            startStream(saved_position_s > 0);
        }
        takeState(response_json);
        return true;
    }

    if (type == "media.stop") {
        if (!playbackIdMatches(doc)) {
            return true;
        }

        stopPlayer(false, true);
        takeState(response_json);
        return true;
    }

    if (type == "media.volume") {
        if (!playbackIdMatches(doc)) {
            return true;
        }

        const int requested = doc["volume"] | static_cast<int>(volume_percent);
        volume_percent = static_cast<uint8_t>(constrain(requested, 0, 100));

        if (codec_ready) {
            codec.setVolume(volume_percent);
            codec.setMute(volume_percent == 0);
        }

        queueState(paused ? "paused" : (active ? "playing" : "stopped"));
        takeState(response_json);
        return true;
    }

    return false;
}

void jarvisMediaLoop() {
    ensureConfigured();

    if (resume_pending &&
        static_cast<int32_t>(millis() - resume_after_ms) >= 0) {
        resume_pending = false;
        interrupted_for_voice = false;
        startStream(true);
    }

    if (!active || paused || interrupted_for_voice) {
        return;
    }

    player.loop();

    if (player.isRunning()) {
        not_running_since_ms = 0;

        if (pending_seek && saved_position_s > 0) {
            if (player.setAudioPlayTime(static_cast<uint16_t>(
                    min(saved_position_s, static_cast<uint32_t>(65535))))) {
                pending_seek = false;
            }
        }

        if (buffering || !ever_played) {
            buffering = false;
            ever_played = true;
            queueState("playing");
        }
        return;
    }

    if (!ever_played) {
        return;
    }

    if (not_running_since_ms == 0) {
        not_running_since_ms = millis();
        return;
    }

    if (millis() - not_running_since_ms > 1800) {
        active = false;
        buffering = false;
        ever_played = false;
        releaseCodec();
        queueState("stopped");
    }
}

bool jarvisMediaPollState(String& state_json) {
    return takeState(state_json);
}

void jarvisMediaInterruptForVoice() {
    ensureConfigured();

    if ((!active && !paused) || interrupted_for_voice || !stream_url.length()) {
        return;
    }

    saved_position_s = player.stopSong();
    active = false;
    paused = false;
    buffering = false;
    ever_played = false;
    pending_seek = false;
    interrupted_for_voice = true;
    resume_pending = false;
    not_running_since_ms = 0;

    releaseCodec();
    queueState("paused");

    Serial.printf(
        "[media] interrupted for voice at %lu s\n",
        static_cast<unsigned long>(saved_position_s)
    );
}

#else

void jarvisMediaAugmentCapabilities(JsonDocument&) {}

bool jarvisMediaHandleMessage(
    const JsonDocument&,
    String& response_json
) {
    response_json = "";
    return false;
}

void jarvisMediaLoop() {}

bool jarvisMediaPollState(String& state_json) {
    state_json = "";
    return false;
}

void jarvisMediaInterruptForVoice() {}

#endif
