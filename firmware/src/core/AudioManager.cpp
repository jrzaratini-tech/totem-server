#include "core/AudioManager.h"
#include "Config.h"
#include <WiFiClientSecure.h>

AudioManager::AudioManager() {
    playing = false;
    downloading = false;
    currentVersion = 0;
    downloadStartMs = 0;
}

void AudioManager::begin(const String &totemId, const String &deviceToken) {
    this->totemId = totemId;
    this->deviceToken = deviceToken;
    SPIFFS.begin(true);

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DIN);
    audio.setVolume(DEFAULT_VOLUME);
}

void AudioManager::loop() {
    if (playing) {
        audio.loop();
        if (!audio.isRunning()) {
            playing = false;
        }
    }
}

void AudioManager::play() {
    if (playing) return;
    if (!SPIFFS.exists(AUDIO_FILENAME)) {
        return;
    }

    if (audio.connecttoFS(SPIFFS, AUDIO_FILENAME)) {
        playing = true;
    }
}

void AudioManager::stop() {
    if (!playing) return;
    audio.stopSong();
    playing = false;
}

bool AudioManager::isPlaying() const { return playing; }
bool AudioManager::isDownloading() const { return downloading; }

int AudioManager::getVersion() const { return currentVersion; }
void AudioManager::setVersion(int v) { currentVersion = max(0, v); }

bool AudioManager::downloadFileToTemp(const String &url) {
    if (!url.startsWith("https://")) return false;
    if (String(ROOT_CA_PEM).length() == 0) return false;

    WiFiClientSecure secure;
    secure.setCACert(ROOT_CA_PEM);

    HTTPClient http;
    if (!http.begin(secure, url)) return false;

    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    int len = http.getSize();
    if (len <= 0 || len > (int)MAX_AUDIO_SIZE) {
        http.end();
        return false;
    }

    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_WRITE);
    if (!tmp) {
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[1024];
    int total = 0;

    while (http.connected() && total < len) {
        if (millis() - downloadStartMs > DOWNLOAD_TIMEOUT) {
            tmp.close();
            http.end();
            return false;
        }

        size_t avail = stream->available();
        if (avail) {
            int r = stream->readBytes(buf, (size_t)min(avail, sizeof(buf)));
            if (r > 0) {
                tmp.write(buf, (size_t)r);
                total += r;
            }
        }
        yield();
    }

    tmp.close();
    http.end();

    if (total != len) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        return false;
    }

    return true;
}

bool AudioManager::activateTempAsCurrent() {
    if (!SPIFFS.exists(AUDIO_TEMP_FILENAME)) return false;

    if (SPIFFS.exists(AUDIO_FILENAME)) {
        SPIFFS.remove(AUDIO_FILENAME);
    }

    bool ok = SPIFFS.rename(AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    if (!ok) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
    }
    return ok;
}

bool AudioManager::checkAndDownloadFromServer(String *outError) {
    if (downloading) {
        if (outError) *outError = "download_in_progress";
        return false;
    }

    downloading = true;
    downloadStartMs = millis();

    String apiUrl = String(SERVER_URL) + "/api/audio/" + totemId;

    if (!apiUrl.startsWith("https://")) {
        downloading = false;
        if (outError) *outError = "server_url_not_https";
        return false;
    }
    if (String(ROOT_CA_PEM).length() == 0) {
        downloading = false;
        if (outError) *outError = "root_ca_missing";
        return false;
    }

    WiFiClientSecure secure;
    secure.setCACert(ROOT_CA_PEM);

    HTTPClient http;
    if (!http.begin(secure, apiUrl)) {
        downloading = false;
        if (outError) *outError = "http_begin_failed";
        return false;
    }

    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
    }

    int code = http.GET();
    if (code != 200) {
        http.end();
        downloading = false;
        if (outError) *outError = String("api_http_") + code;
        return false;
    }

    String body = http.getString();
    http.end();

    StaticJsonDocument<768> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        downloading = false;
        if (outError) *outError = "json_invalid";
        return false;
    }

    const char *url = doc["url"] | nullptr;
    String versao = doc["versao"] | "";

    if (!url || String(url).length() == 0) {
        downloading = false;
        // Sem áudio personalizado: mantém atual
        return true;
    }

    // Converte versao para um inteiro simples (hash) para comparação estável
    // Se vier ISO string, usa um hash; se vier número, usa direto.
    int newVersion = 0;
    if (doc["audioVersion"].is<int>()) {
        newVersion = doc["audioVersion"].as<int>();
    } else if (doc["versao"].is<const char*>()) {
        // hash simples
        for (size_t i = 0; i < versao.length(); i++) newVersion = (newVersion * 31) + versao[i];
    }

    if (newVersion != 0 && newVersion == currentVersion) {
        downloading = false;
        return true;
    }

    // Baixa para arquivo temporário e ativa
    if (!downloadFileToTemp(String(url))) {
        downloading = false;
        if (outError) *outError = "download_failed";
        return false;
    }

    if (!activateTempAsCurrent()) {
        downloading = false;
        if (outError) *outError = "activate_failed";
        return false;
    }

    lastUrl = String(url);
    if (newVersion != 0) currentVersion = newVersion;

    downloading = false;
    return true;
}
