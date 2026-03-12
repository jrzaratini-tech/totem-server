#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "AudioWrapper.h"
#include "AudioEqualizer.h"

class AudioManager {
private:
    AudioEqualizer* equalizer;
    Audio audio;
    int16_t peakSample;
    unsigned long lastMetricsLog;
    unsigned long samplesProcessed;
    unsigned long clippedSamples;
    String totemId;
    String deviceToken;
    bool playing;
    bool downloading;
    String lastUrl;
    int currentVersion;
    unsigned long downloadStartMs;
    bool downloadFileToTemp(const String &url);
    bool activateTempAsCurrent();
    static void audio_info(const char *info);
    static void audio_eof_mp3(const char *info);

public:
    AudioManager();
    void begin(const String &totemId, const String &deviceToken);
    void loop();
    void play();
    void stop();
    void setVolume(int vol);
    void playTestTone(int durationMs = 3000);
    bool isPlaying() const;
    bool isDownloading() const;
    int getVersion() const;
    void setVersion(int v);
    bool checkAndDownloadFromServer(String *outError = nullptr);
    AudioEqualizer* getEqualizer();
};

#endif
