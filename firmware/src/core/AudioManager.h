#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Audio.h>

class AudioManager {
private:
    Audio audio;

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
    void setVolume(int vol); // 0-21 (ES8388 range)

    bool isPlaying() const;
    bool isDownloading() const;

    int getVersion() const;
    void setVersion(int v);

    // Consulta o backend e baixa áudio se houver versão nova.
    // Retorna true se fez download/aplicou (ou se já estava atualizado com sucesso), false se houve erro.
    bool checkAndDownloadFromServer(String *outError = nullptr);
};

#endif
