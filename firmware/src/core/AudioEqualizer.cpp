#include "core/AudioEqualizer.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <math.h>

AudioEqualizer::AudioEqualizer() {
    currentProfile = DEFAULT_AUDIO_PROFILE;
    currentGain = 1.0f;
    currentBoost = 2.5f;
    peakLevel = 0;
    clippingEvents = 0;
}

void AudioEqualizer::begin() {
    Serial.println("[EQ] ========== AUDIO EQUALIZER INITIALIZED ==========");
    setProfile(DEFAULT_AUDIO_PROFILE);
    printProfile();
    Serial.println("[EQ] ===================================================");
}

void AudioEqualizer::setProfile(const AudioProfile &profile) {
    currentProfile = profile;
    currentProfile.lastCalibrated = millis();
    
    setVolume(profile.volume);
    setAmplifierGain(profile.amplifierGain);
    applyESPModelCompensation();
    
    Serial.println("[EQ] Profile updated");
}

AudioProfile AudioEqualizer::getProfile() const {
    return currentProfile;
}

float AudioEqualizer::dbToLinear(float db) {
    return pow(10.0f, db / 20.0f);
}

void AudioEqualizer::applyESPModelCompensation() {
    switch (currentProfile.espModel) {
        case ESP32_AUDIOKIT:
            currentBoost = 2.0f;
            Serial.println("[EQ] ESP32 AudioKit compensation: 2.0x boost");
            break;
        case ESP32_S3_DEVKIT:
            currentBoost = 3.0f;
            Serial.println("[EQ] ESP32-S3 DevKit compensation: 3.0x boost");
            break;
        case ESP32_DEVKIT_V1:
            currentBoost = 2.5f;
            Serial.println("[EQ] ESP32 DevKit V1 compensation: 2.5x boost");
            break;
        case ESP32_GENERIC:
        default:
            currentBoost = 2.2f;
            Serial.println("[EQ] ESP32 Generic compensation: 2.2x boost");
            break;
    }
}

void AudioEqualizer::setVolume(int vol) {
    currentProfile.volume = constrain(vol, MIN_VOLUME, MAX_VOLUME);
    currentGain = applyVolumeCurve(currentProfile.volume);
    
    Serial.printf("[EQ] Volume set to %d/10, gain: %.3f\n", currentProfile.volume, currentGain);
}

void AudioEqualizer::setEqualizer(float bass, float mid, float treble) {
    currentProfile.equalizer.bass = constrain(bass, EQ_MIN_DB, EQ_MAX_DB);
    currentProfile.equalizer.mid = constrain(mid, EQ_MIN_DB, EQ_MAX_DB);
    currentProfile.equalizer.treble = constrain(treble, EQ_MIN_DB, EQ_MAX_DB);
    
    Serial.printf("[EQ] Equalizer: Bass=%.1fdB, Mid=%.1fdB, Treble=%.1fdB\n",
                 currentProfile.equalizer.bass,
                 currentProfile.equalizer.mid,
                 currentProfile.equalizer.treble);
}

void AudioEqualizer::setAmplifierGain(int gainDB) {
    if (gainDB != 3 && gainDB != 9 && gainDB != 15) {
        Serial.printf("[EQ] Invalid gain %d, using 15dB\n", gainDB);
        gainDB = 15;
    }
    
    currentProfile.amplifierGain = gainDB;
    
    pinMode(I2S_GAIN, OUTPUT);
    
    if (gainDB == 3) {
        digitalWrite(I2S_GAIN, GAIN_3DB);
        Serial.println("[EQ] Amplifier gain: 3dB (LOW)");
    } else if (gainDB == 9) {
        pinMode(I2S_GAIN, INPUT);
        Serial.println("[EQ] Amplifier gain: 9dB (FLOAT)");
    } else {
        digitalWrite(I2S_GAIN, GAIN_15DB);
        Serial.println("[EQ] Amplifier gain: 15dB (HIGH)");
    }
    
    delay(10);
}

void AudioEqualizer::setESPModel(ESPModel model) {
    currentProfile.espModel = model;
    applyESPModelCompensation();
}

void AudioEqualizer::setVolumeCurve(VolumeCurve curve) {
    currentProfile.volumeCurve = curve;
    currentGain = applyVolumeCurve(currentProfile.volume);
    
    const char* curveName[] = {"LINEAR", "EXPONENTIAL_SOFT", "EXPONENTIAL_AGGRESSIVE"};
    Serial.printf("[EQ] Volume curve: %s, new gain: %.3f\n", curveName[curve], currentGain);
}

void AudioEqualizer::setClippingProtection(bool enabled) {
    currentProfile.clippingProtection = enabled;
    Serial.printf("[EQ] Clipping protection: %s\n", enabled ? "ENABLED" : "DISABLED");
}

float AudioEqualizer::applyVolumeCurveInternal(int volume, VolumeCurve curve) {
    if (volume == 0) return 0.0f;
    
    float normalized = (float)volume / 10.0f;
    
    switch (curve) {
        case LINEAR:
            return LINEAR_MIN_GAIN + (normalized * (LINEAR_MAX_GAIN - LINEAR_MIN_GAIN));
            
        case EXPONENTIAL_SOFT:
            return EXPONENTIAL_SOFT_BASE + (normalized * normalized * EXPONENTIAL_SOFT_MULT);
            
        case EXPONENTIAL_AGGRESSIVE:
            return EXPONENTIAL_AGG_BASE + (normalized * normalized * EXPONENTIAL_AGG_MULT);
            
        default:
            return normalized;
    }
}

float AudioEqualizer::applyVolumeCurve(int volume) {
    return applyVolumeCurveInternal(volume, currentProfile.volumeCurve);
}

float AudioEqualizer::getCurrentGain() const {
    return currentGain;
}

float AudioEqualizer::getCurrentBoost() const {
    return currentBoost;
}

float AudioEqualizer::getTotalGain() const {
    return currentGain * currentBoost;
}

void AudioEqualizer::applyEQToSample(int16_t &left, int16_t &right) {
    float bassGain = dbToLinear(currentProfile.equalizer.bass);
    float midGain = dbToLinear(currentProfile.equalizer.mid);
    float trebleGain = dbToLinear(currentProfile.equalizer.treble);
    
    float avgGain = (bassGain + midGain + trebleGain) / 3.0f;
    
    int32_t leftProcessed = (int32_t)(left * avgGain);
    int32_t rightProcessed = (int32_t)(right * avgGain);
    
    if (currentProfile.clippingProtection) {
        leftProcessed = constrain(leftProcessed, -32768, 32767);
        rightProcessed = constrain(rightProcessed, -32768, 32767);
    }
    
    left = (int16_t)leftProcessed;
    right = (int16_t)rightProcessed;
}

bool AudioEqualizer::checkClipping(int16_t sample) {
    int16_t absSample = abs(sample);
    
    if (absSample > peakLevel) {
        peakLevel = absSample;
    }
    
    if (absSample >= 32767) {
        clippingEvents++;
        return true;
    }
    
    return false;
}

void AudioEqualizer::resetMetrics() {
    peakLevel = 0;
    clippingEvents = 0;
}

int16_t AudioEqualizer::getPeakLevel() const {
    return peakLevel;
}

unsigned long AudioEqualizer::getClippingEvents() const {
    return clippingEvents;
}

void AudioEqualizer::printProfile() const {
    Serial.println("[EQ] ========== CURRENT AUDIO PROFILE ==========");
    Serial.printf("[EQ] Volume: %d/10\n", currentProfile.volume);
    Serial.printf("[EQ] Base Gain: %.3f\n", currentGain);
    Serial.printf("[EQ] Boost: %.2fx\n", currentBoost);
    Serial.printf("[EQ] Total Gain: %.3fx (+%.1fdB)\n", getTotalGain(), 20.0f * log10(getTotalGain()));
    Serial.printf("[EQ] Equalizer - Bass: %.1fdB, Mid: %.1fdB, Treble: %.1fdB\n",
                 currentProfile.equalizer.bass,
                 currentProfile.equalizer.mid,
                 currentProfile.equalizer.treble);
    Serial.printf("[EQ] Amplifier Gain: %ddB\n", currentProfile.amplifierGain);
    
    const char* modelNames[] = {"DevKit V1", "AudioKit", "S3 DevKit", "Generic"};
    Serial.printf("[EQ] ESP Model: %s\n", modelNames[currentProfile.espModel]);
    
    const char* curveNames[] = {"Linear", "Exponential Soft", "Exponential Aggressive"};
    Serial.printf("[EQ] Volume Curve: %s\n", curveNames[currentProfile.volumeCurve]);
    
    Serial.printf("[EQ] Clipping Protection: %s\n", currentProfile.clippingProtection ? "ON" : "OFF");
    Serial.printf("[EQ] Last Calibrated: %lu ms ago\n", millis() - currentProfile.lastCalibrated);
    Serial.println("[EQ] ===============================================");
}

String AudioEqualizer::profileToJSON() const {
    StaticJsonDocument<512> doc;
    
    doc["volume"] = currentProfile.volume;
    
    JsonObject eq = doc.createNestedObject("equalizer");
    eq["bass"] = currentProfile.equalizer.bass;
    eq["mid"] = currentProfile.equalizer.mid;
    eq["treble"] = currentProfile.equalizer.treble;
    
    doc["amplifierGain"] = currentProfile.amplifierGain;
    doc["espModel"] = currentProfile.espModel;
    doc["volumeCurve"] = currentProfile.volumeCurve;
    doc["clippingProtection"] = currentProfile.clippingProtection;
    doc["lastCalibrated"] = currentProfile.lastCalibrated;
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool AudioEqualizer::profileFromJSON(const String &json) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("[EQ] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    AudioProfile newProfile = currentProfile;
    
    if (doc.containsKey("volume")) {
        newProfile.volume = doc["volume"];
    }
    
    if (doc.containsKey("equalizer")) {
        JsonObject eq = doc["equalizer"];
        if (eq.containsKey("bass")) newProfile.equalizer.bass = eq["bass"];
        if (eq.containsKey("mid")) newProfile.equalizer.mid = eq["mid"];
        if (eq.containsKey("treble")) newProfile.equalizer.treble = eq["treble"];
    }
    
    if (doc.containsKey("amplifierGain")) {
        newProfile.amplifierGain = doc["amplifierGain"];
    }
    
    if (doc.containsKey("espModel")) {
        newProfile.espModel = (ESPModel)(int)doc["espModel"];
    }
    
    if (doc.containsKey("volumeCurve")) {
        newProfile.volumeCurve = (VolumeCurve)(int)doc["volumeCurve"];
    }
    
    if (doc.containsKey("clippingProtection")) {
        newProfile.clippingProtection = doc["clippingProtection"];
    }
    
    setProfile(newProfile);
    
    Serial.println("[EQ] Profile loaded from JSON");
    printProfile();
    
    return true;
}
