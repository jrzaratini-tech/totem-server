#include "core/AudioManager.h"
#include "Config.h"
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

using namespace audio_tools;

// ========== AUDIO PROCESSING CONFIGURATION ==========
// Digital gain boost for maximum volume (1.0 = no boost, 3.0 = 200% boost)
#define MAX_DIGITAL_GAIN 3.0f
#define MIN_DIGITAL_GAIN 0.0f

// Audio metrics logging interval
#define METRICS_LOG_INTERVAL 5000

AudioManager::AudioManager() {
    playing = false;
    downloading = false;
    currentVersion = 0;
    downloadStartMs = 0;
    i2s = nullptr;
    volumeStream = nullptr;
    encoded = nullptr;
    decoder = nullptr;
    copier = nullptr;
    peakSample = 0;
    lastMetricsLog = 0;
    samplesProcessed = 0;
    clippedSamples = 0;
}

// Volume digital global (0-10 do backend -> 0.0-4.0 gain with boost)
static float gDigitalGain = 1.0f; // Base gain (will be multiplied by boost)
static float gVolumeBoost = 4.0f; // Additional boost multiplier for maximum output

// Helper macros para cast de ponteiros
#define I2S_PTR ((I2SStream*)i2s)
#define VOLUME_PTR ((VolumeStream*)volumeStream)
#define ENCODED_PTR ((EncodedAudioStream*)encoded)
#define DECODER_PTR ((MP3DecoderHelix*)decoder)
#define COPIER_PTR ((StreamCopy*)copier)

// ========== CUSTOM AUDIO PROCESSOR ==========
// Stereo-to-mono mixer with digital gain boost and clipping prevention
// Uses BaseConverter for proper AudioTools integration
class StereoToMonoGainConverter : public BaseConverter {
private:
    int16_t *peakSamplePtr;
    unsigned long *samplesProcessedPtr;
    unsigned long *clippedSamplesPtr;
    
public:
    StereoToMonoGainConverter(int16_t *peak, unsigned long *samples, unsigned long *clipped) 
        : peakSamplePtr(peak), samplesProcessedPtr(samples), clippedSamplesPtr(clipped) {}
    
    size_t convert(uint8_t *src, size_t size) override {
        if (!src || size == 0) return 0;
        
        // Process stereo 16-bit samples
        int16_t *samples = (int16_t*)src;
        size_t numSamples = size / 2; // bytes to 16-bit samples
        
        // Ensure even number (stereo pairs)
        if (numSamples % 2 != 0) numSamples--;
        
        for (size_t i = 0; i < numSamples; i += 2) {
            // Read stereo pair
            int16_t left = samples[i];
            int16_t right = samples[i + 1];
            
            // Mix to mono (average)
            int32_t mono = ((int32_t)left + (int32_t)right) / 2;
            
            // Apply digital gain with volume boost
            float totalGain = gDigitalGain * gVolumeBoost;
            mono = (int32_t)(mono * totalGain);
            
            // Clamp to prevent clipping
            if (mono > 32767) {
                mono = 32767;
                (*clippedSamplesPtr)++;
            } else if (mono < -32768) {
                mono = -32768;
                (*clippedSamplesPtr)++;
            }
            
            // Track peak for metrics
            int16_t absSample = abs((int16_t)mono);
            if (absSample > *peakSamplePtr) {
                *peakSamplePtr = absSample;
            }
            
            // Write mono sample to both channels (MAX98357A expects stereo format)
            samples[i] = (int16_t)mono;
            samples[i + 1] = (int16_t)mono;
            
            (*samplesProcessedPtr)++;
        }
        
        return size; // Return original size
    }
};

static StereoToMonoGainConverter *gAudioConverter = nullptr;
static ConverterStream<int16_t> *gConverterStream = nullptr;

void AudioManager::begin(const String &totemId, const String &deviceToken) {
    this->totemId = totemId;
    this->deviceToken = deviceToken;
    
    // Log heap inicial
    Serial.printf("[Audio] Heap at start - Free: %d bytes, Min free: %d bytes\n", 
                 ESP.getFreeHeap(), ESP.getMinFreeHeap());
    
    Serial.println("[Audio] Initializing SPIFFS...");
    if (!SPIFFS.begin(true)) {
        Serial.println("[Audio] ERROR: SPIFFS initialization failed!");
    } else {
        Serial.println("[Audio] SPIFFS initialized");
        size_t totalBytes = SPIFFS.totalBytes();
        size_t usedBytes = SPIFFS.usedBytes();
        Serial.printf("[Audio] SPIFFS - Total: %d bytes, Used: %d bytes, Free: %d bytes\n", 
                     totalBytes, usedBytes, totalBytes - usedBytes);
    }

    Serial.println("[Audio] ========== HARDWARE SETUP ==========");
    Serial.println("[Audio] Hardware: ESP32 DevKit V1 + MAX98357A I2S DAC");
    Serial.printf("[Audio] I2S Pins - BCLK=%d, LRC=%d, DOUT=%d, GAIN=%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_GAIN);
    
    // Configure GAIN pin for maximum amplifier gain (15dB)
    Serial.printf("[Audio] Configuring GAIN pin (GPIO %d) for MAX98357A...\n", I2S_GAIN);
    pinMode(I2S_GAIN, OUTPUT);
    digitalWrite(I2S_GAIN, HIGH);  // HIGH = 15dB gain
    delay(10);
    int gainState = digitalRead(I2S_GAIN);
    Serial.printf("[Audio] GAIN pin state: %s (expected: HIGH)\n", gainState == HIGH ? "HIGH" : "LOW");
    Serial.println("[Audio] MAX98357A configured for 15dB gain (maximum)");
    
    // Create I2S stream for MAX98357A with optimized DMA buffers
    i2s = new I2SStream();
    auto i2s_cfg = I2S_PTR->defaultConfig(TX_MODE);
    i2s_cfg.pin_bck = I2S_BCLK;
    i2s_cfg.pin_ws = I2S_LRC;
    i2s_cfg.pin_data = I2S_DOUT;
    i2s_cfg.sample_rate = 44100;
    i2s_cfg.bits_per_sample = 16;
    i2s_cfg.channels = 2;
    i2s_cfg.i2s_format = I2S_STD_FORMAT;
    i2s_cfg.buffer_count = 8;      // Optimized: 8 DMA buffers
    i2s_cfg.buffer_size = 512;     // Optimized: 512 samples per buffer
    I2S_PTR->begin(i2s_cfg);
    Serial.println("[Audio] ✓ I2S initialized (44.1kHz, 16-bit, Stereo, DMA: 8x512)");
    
    // Create audio converter for stereo-to-mono mixing + gain boost
    gAudioConverter = new StereoToMonoGainConverter(&peakSample, &samplesProcessed, &clippedSamples);
    Serial.println("[Audio] ✓ Audio converter created (stereo→mono + gain boost)");
    
    // Create converter stream (applies gain processing)
    gConverterStream = new ConverterStream<int16_t>(*I2S_PTR, *gAudioConverter);
    gConverterStream->begin();
    Serial.println("[Audio] ✓ Converter stream initialized");
    
    // Create MP3 decoder
    decoder = new MP3DecoderHelix();
    Serial.println("[Audio] ✓ MP3 Helix decoder created");
    
    // Create encoded stream (decoder -> converter -> I2S)
    encoded = new EncodedAudioStream(gConverterStream, DECODER_PTR);
    ENCODED_PTR->begin();
    Serial.println("[Audio] ✓ Encoded audio stream initialized");
    
    // Create copier (file -> encoded stream)
    copier = new StreamCopy();
    Serial.println("[Audio] ✓ Stream copier created");
    
    // Set default volume with boost (using exponential curve)
    float normalizedVol = (float)DEFAULT_VOLUME / 10.0f;
    gDigitalGain = 0.3f + (normalizedVol * normalizedVol * 0.9f);
    Serial.printf("[Audio] ✓ Default volume: %d/10 (base: %.3f, boost: %.2fx, total: %.3fx)\n", 
                 DEFAULT_VOLUME, gDigitalGain, gVolumeBoost, gDigitalGain * gVolumeBoost);
    
    Serial.println("[Audio] ========================================");
    Serial.println("[Audio] AUDIO PIPELINE READY:");
    Serial.println("[Audio]   SPIFFS → MP3 Decoder → Converter Stream");
    Serial.println("[Audio]   → Stereo-to-Mono Mix → Digital Gain Boost");
    Serial.println("[Audio]   → Sample Clamp → I2S → MAX98357A (15dB)");
    Serial.printf("[Audio] Digital gain range: %.1fx - %.1fx\n", MIN_DIGITAL_GAIN, MAX_DIGITAL_GAIN);
    Serial.printf("[Audio] Current boost multiplier: %.2fx\n", gVolumeBoost);
    Serial.printf("[Audio] Heap after init - Free: %d bytes, Min free: %d bytes\n", 
                 ESP.getFreeHeap(), ESP.getMinFreeHeap());
    Serial.println("[Audio] ==========================================");
}

void AudioManager::loop() {
    if (playing && audioFile && copier) {
        // Usar StreamCopy para pipeline correto: File -> EncodedStream -> Decoder -> Processor -> I2S
        if (!COPIER_PTR->copy()) {
            // Fim do arquivo ou erro
            playing = false;
            if (audioFile) {
                audioFile.close();
            }
            Serial.println("[Audio] Playback finished");
            
            // Log final metrics
            Serial.printf("[Audio] Final metrics - Samples: %lu, Peak: %d, Clipped: %lu\n",
                         samplesProcessed, peakSample, clippedSamples);
        }
        
        // Log audio metrics periodically
        if (millis() - lastMetricsLog > METRICS_LOG_INTERVAL) {
            float peakPercent = (peakSample / 32767.0f) * 100.0f;
            float clipPercent = samplesProcessed > 0 ? (clippedSamples / (float)samplesProcessed) * 100.0f : 0.0f;
            Serial.printf("[AUDIO] vol=%d/10 gain=%.2fx boost=%.2fx total=%.2fx peak=%d(%.1f%%) clipped=%lu(%.2f%%)\n",
                         (int)(gDigitalGain * 10.0f), gDigitalGain, gVolumeBoost, 
                         gDigitalGain * gVolumeBoost, peakSample, peakPercent, 
                         clippedSamples, clipPercent);
            lastMetricsLog = millis();
        }
    }
}

void AudioManager::play() {
    Serial.println("[Audio] ========== PLAY REQUEST ==========");
    
    if (playing) {
        Serial.println("[Audio] Already playing, ignoring");
        return;
    }
    
    if (!SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] ERROR: Audio file not found in SPIFFS!");
        Serial.printf("[Audio] Looking for: %s\n", AUDIO_FILENAME);
        return;
    }
    
    if (!copier) {
        Serial.println("[Audio] ERROR: Audio streams not initialized!");
        return;
    }
    
    // Reset metrics
    peakSample = 0;
    samplesProcessed = 0;
    clippedSamples = 0;
    lastMetricsLog = millis();
    
    // Abrir arquivo de áudio
    audioFile = SPIFFS.open(AUDIO_FILENAME, FILE_READ);
    if (!audioFile) {
        Serial.println("[Audio] ERROR: Failed to open audio file!");
        return;
    }
    
    Serial.printf("[Audio] Audio file opened - Size: %d bytes (%.2f KB)\n", 
                 audioFile.size(), audioFile.size() / 1024.0);
    
    // Configurar copier para copiar do arquivo para o encoded stream
    COPIER_PTR->begin(*ENCODED_PTR, audioFile);
    
    playing = true;
    Serial.printf("[Audio] Playback started - Volume: %d/10, Gain: %.2fx, Boost: %.2fx\n",
                 (int)(gDigitalGain * 10.0f), gDigitalGain, gVolumeBoost);
    Serial.println("[Audio] ==========================================");
}

void AudioManager::stop() {
    if (!playing) return;
    Serial.println("[Audio] Stopping playback");
    
    // Parar copier primeiro
    if (copier) {
        COPIER_PTR->end();
    }
    
    // Fechar arquivo
    if (audioFile) {
        audioFile.close();
    }
    
    // Limpar buffers I2S
    if (encoded) {
        ENCODED_PTR->end();
        ENCODED_PTR->begin();
    }
    
    playing = false;
}

void AudioManager::setVolume(int vol) {
    int clampedVol = constrain(vol, MIN_VOLUME, MAX_VOLUME);
    
    Serial.println("[Audio] ========================================");
    Serial.printf("[Audio] VOLUME CHANGE REQUEST: %d/10\n", clampedVol);
    
    if (clampedVol == 0) {
        // Mute
        gDigitalGain = 0.0f;
        gVolumeBoost = 1.0f;
        Serial.println("[Audio] Volume: MUTED");
    } else {
        // Use exponential curve for better perceived loudness
        // Volume 1-10 maps to gain 0.3-1.2 (exponential)
        float normalizedVol = (float)clampedVol / 10.0f;
        gDigitalGain = 0.3f + (normalizedVol * normalizedVol * 0.9f); // Exponential: 0.3 to 1.2
        
        // Aggressive boost multiplier for maximum output
        gVolumeBoost = 2.5f;
        
        float totalGain = gDigitalGain * gVolumeBoost;
        Serial.printf("[Audio] Volume: %d/10\n", clampedVol);
        Serial.printf("[Audio] Base gain: %.3f (exponential curve)\n", gDigitalGain);
        Serial.printf("[Audio] Boost multiplier: %.2fx\n", gVolumeBoost);
        Serial.printf("[Audio] Total digital gain: %.3fx\n", totalGain);
        Serial.printf("[Audio] Hardware gain: 15dB (GPIO %d = HIGH)\n", I2S_GAIN);
        Serial.printf("[Audio] Combined output: %.1fdB digital + 15dB hardware\n", 
                     20.0f * log10(totalGain));
    }
    
    Serial.println("[Audio] ========================================");
}

bool AudioManager::isPlaying() const { return playing; }
bool AudioManager::isDownloading() const { return downloading; }

int AudioManager::getVersion() const { return currentVersion; }
void AudioManager::setVersion(int v) { currentVersion = max(0, v); }

bool AudioManager::downloadFileToTemp(const String &url) {
    Serial.printf("[Audio] downloadFileToTemp() - URL: %s\n", url.c_str());
    Serial.printf("[Audio] Heap before download - Free: %d bytes\n", ESP.getFreeHeap());

    // Limpar arquivo temporário anterior se existir
    if (SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        Serial.println("[Audio] Removing old temp file...");
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
    }

    // SEGURANÇA: Verificar espaço disponível ANTES de deletar áudio atual
    size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    Serial.printf("[Audio] SPIFFS free space: %d bytes\n", freeBytes);
    
    // Só deletar áudio antigo se houver espaço mínimo (1MB)
    if (freeBytes < 1048576 && SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] Low space, removing old audio file...");
        SPIFFS.remove(AUDIO_FILENAME);
        freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
        Serial.printf("[Audio] SPIFFS free space after cleanup: %d bytes\n", freeBytes);
    }

    if (!url.startsWith("https://")) {
        Serial.println("[Audio] ERROR: URL must be HTTPS");
        return false;
    }

    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        Serial.println("[Audio] Using ROOT_CA_PEM for HTTPS");
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        Serial.println("[Audio] Using insecure HTTPS (no certificate validation)");
        secure.setInsecure();
        #else
        Serial.println("[Audio] ERROR: ROOT_CA_PEM required but not configured");
        return false;
        #endif
    }

    HTTPClient http;
    if (!http.begin(secure, url)) {
        Serial.println("[Audio] ERROR: HTTP begin failed");
        return false;
    }

    // Configurar timeout HTTP
    http.setTimeout(30000); // 30 segundos
    
    if (deviceToken.length() > 0) {
        http.addHeader("X-Device-Token", deviceToken);
        Serial.println("[Audio] Added device token to request");
    }

    Serial.println("[Audio] Sending GET request...");
    int code = http.GET();
    Serial.printf("[Audio] Download response code: %d\n", code);
    
    if (code != 200) {
        http.end();
        Serial.printf("[Audio] ERROR: Expected 200, got %d\n", code);
        return false;
    }

    int len = http.getSize();
    Serial.printf("[Audio] File size: %d bytes (%.2f KB)\n", len, len / 1024.0);
    
    if (len <= 0 || len > (int)MAX_AUDIO_SIZE) {
        http.end();
        Serial.printf("[Audio] ERROR: Invalid file size (max: %d bytes)\n", MAX_AUDIO_SIZE);
        return false;
    }
    
    // Verificar se há espaço suficiente no SPIFFS
    size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    if ((size_t)len > freeSpace) {
        http.end();
        Serial.printf("[Audio] ERROR: Insufficient SPIFFS space (need: %d, have: %d)\n", len, freeSpace);
        return false;
    }

    Serial.printf("[Audio] Opening temp file: %s\n", AUDIO_TEMP_FILENAME);
    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_WRITE);
    if (!tmp) {
        http.end();
        Serial.println("[Audio] ERROR: Failed to open temp file for writing");
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    
    // Alocar buffer no heap para evitar stack overflow
    uint8_t *buf = (uint8_t*)malloc(1024);
    if (!buf) {
        tmp.close();
        http.end();
        Serial.println("[Audio] ERROR: Failed to allocate buffer");
        return false;
    }
    
    int total = 0;
    int lastPercent = -1;

    Serial.println("[Audio] Starting download...");
    unsigned long lastWdtReset = millis();
    unsigned long lastProgressLog = millis();
    
    while (http.connected() && total < len) {
        // Reset watchdog a cada 500ms durante download
        if (millis() - lastWdtReset > 500) {
            esp_task_wdt_reset();
            lastWdtReset = millis();
        }
        
        if (millis() - downloadStartMs > DOWNLOAD_TIMEOUT) {
            tmp.close();
            http.end();
            free(buf);
            Serial.println("[Audio] ERROR: Download timeout");
            return false;
        }

        size_t avail = stream->available();
        if (avail) {
            size_t toRead = min(avail, (size_t)1024);
            int r = stream->readBytes(buf, toRead);
            if (r > 0) {
                tmp.write(buf, (size_t)r);
                total += r;
                
                int percent = (total * 100) / len;
                if (percent != lastPercent && percent % 20 == 0) {
                    Serial.printf("[Audio] %d%%\n", percent);
                    lastPercent = percent;
                }
            }
        } else {
            delay(10);  // Pequeno delay se não há dados disponíveis
        }
        yield();
    }

    tmp.close();
    http.end();
    free(buf);

    Serial.printf("[Audio] Download OK - %d bytes\n", total);

    if (total != len) {
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        Serial.println("[Audio] ERROR: Incomplete download, removed temp file");
        return false;
    }

    Serial.println("[Audio] Download successful!");
    return true;
}

bool AudioManager::activateTempAsCurrent() {
    Serial.println("[Audio] activateTempAsCurrent() called");
    
    if (!SPIFFS.exists(AUDIO_TEMP_FILENAME)) {
        Serial.println("[Audio] ERROR: Temp file does not exist");
        return false;
    }

    File tmp = SPIFFS.open(AUDIO_TEMP_FILENAME, FILE_READ);
    if (tmp) {
        Serial.printf("[Audio] Temp file size: %d bytes\n", tmp.size());
        tmp.close();
    }

    if (SPIFFS.exists(AUDIO_FILENAME)) {
        Serial.println("[Audio] Removing old audio file");
        SPIFFS.remove(AUDIO_FILENAME);
    }

    Serial.printf("[Audio] Renaming %s to %s\n", AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    bool ok = SPIFFS.rename(AUDIO_TEMP_FILENAME, AUDIO_FILENAME);
    
    if (!ok) {
        Serial.println("[Audio] ERROR: Failed to rename temp file");
        SPIFFS.remove(AUDIO_TEMP_FILENAME);
        return false;
    }

    Serial.println("[Audio] Audio file activated successfully!");
    return true;
}

bool AudioManager::checkAndDownloadFromServer(String *outError) {
    Serial.println("[Audio] checkAndDownloadFromServer() called");
    
    if (downloading) {
        Serial.println("[Audio] Download already in progress");
        if (outError) *outError = "download_in_progress";
        return false;
    }

    downloading = true;
    downloadStartMs = millis();
    Serial.println("[Audio] Starting download process...");

    String apiUrl = String(SERVER_URL) + "/api/audio/" + totemId;

    if (!apiUrl.startsWith("https://")) {
        downloading = false;
        if (outError) *outError = "server_url_not_https";
        return false;
    }
    WiFiClientSecure secure;
    if (String(ROOT_CA_PEM).length() > 0) {
        secure.setCACert(ROOT_CA_PEM);
    } else {
        #if defined(ALLOW_INSECURE_HTTPS) && (ALLOW_INSECURE_HTTPS == 1)
        secure.setInsecure();
        #else
        downloading = false;
        if (outError) *outError = "root_ca_missing";
        return false;
        #endif
    }

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
    Serial.printf("[Audio] API response code: %d\n", code);
    if (code != 200) {
        http.end();
        downloading = false;
        if (outError) *outError = String("api_http_") + code;
        Serial.printf("[Audio] ERROR: API returned code %d\n", code);
        return false;
    }

    String body = http.getString();
    http.end();
    Serial.printf("[Audio] API response body: %s\n", body.c_str());

    StaticJsonDocument<768> doc;
    auto err = deserializeJson(doc, body);
    if (err) {
        downloading = false;
        if (outError) *outError = "json_invalid";
        Serial.printf("[Audio] ERROR: Invalid JSON - %s\n", err.c_str());
        return false;
    }

    String url = "";
    if (doc.containsKey("url") && !doc["url"].isNull()) {
        url = doc["url"].as<String>();
    }
    String versao = doc["versao"] | "";

    Serial.printf("[Audio] Parsed - URL: %s, Versao: %s\n", 
                  url.c_str(), versao.c_str());

    if (url.length() == 0) {
        downloading = false;
        Serial.println("[Audio] No audio URL provided - keeping current audio");
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

    Serial.printf("[Audio] Version check - Current: %d, New: %d\n", currentVersion, newVersion);

    if (newVersion != 0 && newVersion == currentVersion) {
        downloading = false;
        Serial.println("[Audio] Audio already up to date - skipping download");
        return true;
    }

    // Baixa para arquivo temporário e ativa
    Serial.printf("[Audio] Starting download from: %s\n", url.c_str());
    if (!downloadFileToTemp(url)) {
        downloading = false;
        if (outError) *outError = "download_failed";
        Serial.println("[Audio] ERROR: Download failed");
        return false;
    }

    Serial.println("[Audio] Download completed, activating file...");
    if (!activateTempAsCurrent()) {
        downloading = false;
        if (outError) *outError = "activate_failed";
        Serial.println("[Audio] ERROR: Failed to activate downloaded file");
        return false;
    }

    lastUrl = String(url);
    if (newVersion != 0) currentVersion = newVersion;

    downloading = false;
    Serial.printf("[Audio] SUCCESS: Audio updated to version %d\n", newVersion);
    return true;
}

void AudioManager::playTestTone(int durationMs) {
    Serial.println("[Audio] ========== TEST TONE ==========");
    Serial.printf("[Audio] Generating %dms sine wave at 1kHz, full scale\n", durationMs);
    
    if (!i2s) {
        Serial.println("[Audio] ERROR: I2S not initialized");
        return;
    }
    
    const int sampleRate = 44100;
    const int frequency = 1000; // 1kHz tone
    const int amplitude = 32767; // Full scale
    const int numSamples = (sampleRate * durationMs) / 1000;
    
    Serial.printf("[Audio] Sample rate: %d Hz, Frequency: %d Hz, Samples: %d\n", 
                 sampleRate, frequency, numSamples);
    Serial.printf("[Audio] Current gain: %.2fx, Boost: %.2fx, Total: %.2fx\n",
                 gDigitalGain, gVolumeBoost, gDigitalGain * gVolumeBoost);
    
    int16_t buffer[256];
    int samplesGenerated = 0;
    
    while (samplesGenerated < numSamples) {
        int chunkSize = min(128, (numSamples - samplesGenerated) / 2); // stereo pairs
        
        for (int i = 0; i < chunkSize; i++) {
            float t = (float)(samplesGenerated + i) / sampleRate;
            int16_t sample = (int16_t)(amplitude * sin(2.0 * PI * frequency * t));
            
            // Apply current gain settings
            int32_t gained = (int32_t)(sample * gDigitalGain * gVolumeBoost);
            if (gained > 32767) gained = 32767;
            if (gained < -32768) gained = -32768;
            
            // Stereo output (both channels same)
            buffer[i * 2] = (int16_t)gained;
            buffer[i * 2 + 1] = (int16_t)gained;
        }
        
        I2S_PTR->write((uint8_t*)buffer, chunkSize * 4); // 2 channels * 2 bytes
        samplesGenerated += chunkSize;
        
        if (samplesGenerated % 4410 == 0) {
            Serial.printf("[Audio] Test tone: %d%%\n", (samplesGenerated * 100) / numSamples);
        }
    }
    
    Serial.println("[Audio] Test tone complete");
    Serial.println("[Audio] =======================================");
}
