#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "AudioEqualizer.h"

class AudioManager {
private:
    AudioEqualizer* equalizer;
    // Ponteiros void para objetos da biblioteca (implementação no .cpp)
    void *i2s;
    void *volumeStream;
    void *encoded;
    void *decoder;
    void *copier;
    File audioFile;

    // Audio metrics
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

public:
    AudioManager();

    void begin(const String &totemId, const String &deviceToken);
    void loop();

    void play();
    void stop();
    void setVolume(int vol); // 0-10 (backend range, mapped to digital gain)
    void playTestTone(int durationMs = 3000); // Test sine wave at full scale

    bool isPlaying() const;
    bool isDownloading() const;

    int getVersion() const;
    void setVersion(int v);

    // Consulta o backend e baixa áudio se houver versão nova.
    // Retorna true se fez download/aplicou (ou se já estava atualizado com sucesso), false se houve erro.
    bool checkAndDownloadFromServer(String *outError = nullptr);
    
    // Audio equalizer access
    AudioEqualizer* getEqualizer();
};

#endif
