#include "core/AudioEqualizer.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <math.h>

AudioEqualizer::AudioEqualizer() {
    currentProfile = DEFAULT_AUDIO_PROFILE;
    currentGain = 1.0f;
    currentBoost = 1.0f;
    peakLevel = 0;
    clippingEvents = 0;
    limiterEnvelope = 1.0f;
}

void AudioEqualizer::begin() {
    setProfile(DEFAULT_AUDIO_PROFILE);
    Serial.println("[EQ] Professional audio engine initialized");
    printProfile();
}

void AudioEqualizer::setProfile(const AudioProfile &profile) {
    currentProfile = profile;
    currentProfile.lastCalibrated = millis();
    setVolume(profile.volume);
    setAmplifierGain(profile.amplifierGain);
    applyESPModelCompensation();
}

AudioProfile AudioEqualizer::getProfile() const {
    return currentProfile;
}

float AudioEqualizer::dbToLinear(float db) {
    return pow(10.0f, db / 20.0f);
}

void AudioEqualizer::applyESPModelCompensation() {
    currentBoost = 1.0f;
    const char* models[] = {"DevKit V1", "AudioKit", "S3 DevKit", "Generic"};
    Serial.printf("[EQ] Model: %s\n", models[currentProfile.espModel]);
}

void AudioEqualizer::setVolume(int vol) {
    currentProfile.volume = constrain(vol, MIN_VOLUME, MAX_VOLUME);
    currentGain = applyVolumeCurve(currentProfile.volume);
    Serial.printf("[EQ] Volume: %d/10 (%.3f)\n", currentProfile.volume, currentGain);
}

void AudioEqualizer::setEqualizer(float bass, float mid, float treble) {
    currentProfile.equalizer.bass = constrain(bass, EQ_MIN_DB, EQ_MAX_DB);
    currentProfile.equalizer.mid = constrain(mid, EQ_MIN_DB, EQ_MAX_DB);
    currentProfile.equalizer.treble = constrain(treble, EQ_MIN_DB, EQ_MAX_DB);
    Serial.printf("[EQ] Bass=%.1f Mid=%.1f Treble=%.1fdB\n", bass, mid, treble);
}

void AudioEqualizer::setAmplifierGain(int gainDB) {
    currentProfile.amplifierGain = 9;
    Serial.println("[EQ] Amp gain: 9dB (MAX98357A GAIN pin hardwired to GND)");
}

void AudioEqualizer::setESPModel(ESPModel model) {
    currentProfile.espModel = model;
    applyESPModelCompensation();
}

void AudioEqualizer::setVolumeCurve(VolumeCurve curve) {
    currentProfile.volumeCurve = curve;
    currentGain = applyVolumeCurve(currentProfile.volume);
    const char* curves[] = {"Linear", "Exp-Soft", "Exp-Aggressive"};
    Serial.printf("[EQ] Curve: %s (%.3f)\n", curves[curve], currentGain);
}

void AudioEqualizer::setClippingProtection(bool enabled) {
    currentProfile.clippingProtection = enabled;
    Serial.printf("[EQ] Limiter: %s\n", enabled ? "ON" : "OFF");
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

float AudioEqualizer::applyLimiter(float sample) {
    float absSample = fabs(sample);
    float targetGain = 1.0f;
    
    if (absSample > LIMITER_THRESHOLD * 32767.0f) {
        float excess = absSample - (LIMITER_THRESHOLD * 32767.0f);
        targetGain = (LIMITER_THRESHOLD * 32767.0f) / (absSample + (excess / LIMITER_RATIO));
    }
    
    float attack = (targetGain < limiterEnvelope) ? LIMITER_ATTACK : LIMITER_RELEASE;
    limiterEnvelope += (targetGain - limiterEnvelope) * attack;
    
    float limitedSample = sample * limiterEnvelope * MAKEUP_GAIN;
    
    limitedSample = constrain(limitedSample, -32767.0f, 32767.0f);
    
    return limitedSample;
}

void AudioEqualizer::applyEQToSample(int16_t &left, int16_t &right) {
    float bassGain = dbToLinear(currentProfile.equalizer.bass);
    float midGain = dbToLinear(currentProfile.equalizer.mid);
    float trebleGain = dbToLinear(currentProfile.equalizer.treble);
    
    float avgEQGain = (bassGain * 0.30f + midGain * 0.40f + trebleGain * 0.30f);
    float totalGain = avgEQGain * currentGain * currentBoost;
    
    float leftF = (float)left * totalGain;
    float rightF = (float)right * totalGain;
    
    if (currentProfile.clippingProtection) {
        leftF = applyLimiter(leftF);
        rightF = applyLimiter(rightF);
    } else {
        leftF = constrain(leftF, -32767.0f, 32767.0f);
        rightF = constrain(rightF, -32767.0f, 32767.0f);
    }
    
    left = (int16_t)leftF;
    right = (int16_t)rightF;
}

bool AudioEqualizer::checkClipping(int16_t sample) {
    int16_t absSample = abs(sample);
    if (absSample > peakLevel) peakLevel = absSample;
    if (absSample >= 32767) {
        clippingEvents++;
        return true;
    }
    return false;
}

void AudioEqualizer::resetMetrics() {
    peakLevel = 0;
    clippingEvents = 0;
    limiterEnvelope = 1.0f;
}

int16_t AudioEqualizer::getPeakLevel() const {
    return peakLevel;
}

unsigned long AudioEqualizer::getClippingEvents() const {
    return clippingEvents;
}

float AudioEqualizer::getLimiterGainReduction() const {
    return (1.0f - limiterEnvelope) * 100.0f;
}

void AudioEqualizer::printProfile() const {
    Serial.println("[EQ] ===== PROFESSIONAL AUDIO PROFILE =====");
    Serial.printf("[EQ] Volume: %d/10 | Gain: %.3f\n", currentProfile.volume, currentGain);
    Serial.printf("[EQ] Total: %.3fx (+%.1fdB)\n", getTotalGain(), 20.0f * log10(getTotalGain()));
    Serial.printf("[EQ] Bass: %.1fdB | Mid: %.1fdB | Treble: %.1fdB\n",
                 currentProfile.equalizer.bass,
                 currentProfile.equalizer.mid,
                 currentProfile.equalizer.treble);
    Serial.printf("[EQ] Amp: %ddB | Limiter: %s\n", 
                 currentProfile.amplifierGain,
                 currentProfile.clippingProtection ? "ON" : "OFF");
    const char* curves[] = {"Linear", "Exp-Soft", "Exp-Aggressive"};
    Serial.printf("[EQ] Curve: %s\n", curves[currentProfile.volumeCurve]);
    Serial.println("[EQ] =========================================");
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
        Serial.printf("[EQ] JSON error: %s\n", error.c_str());
        return false;
    }
    AudioProfile newProfile = currentProfile;
    if (doc.containsKey("volume")) newProfile.volume = doc["volume"];
    if (doc.containsKey("equalizer")) {
        JsonObject eq = doc["equalizer"];
        if (eq.containsKey("bass")) newProfile.equalizer.bass = eq["bass"];
        if (eq.containsKey("mid")) newProfile.equalizer.mid = eq["mid"];
        if (eq.containsKey("treble")) newProfile.equalizer.treble = eq["treble"];
    }
    if (doc.containsKey("amplifierGain")) newProfile.amplifierGain = doc["amplifierGain"];
    if (doc.containsKey("espModel")) newProfile.espModel = (ESPModel)(int)doc["espModel"];
    if (doc.containsKey("volumeCurve")) newProfile.volumeCurve = (VolumeCurve)(int)doc["volumeCurve"];
    if (doc.containsKey("clippingProtection")) newProfile.clippingProtection = doc["clippingProtection"];
    setProfile(newProfile);
    Serial.println("[EQ] Profile loaded");
    printProfile();
    return true;
}

void AudioEqualizer::applyMultibandEQ(float &sample, float freq) {
    float bassGain = 1.0f;
    float midGain = 1.0f;
    float trebleGain = 1.0f;
    
    if (freq < 250.0f) {
        bassGain = dbToLinear(currentProfile.equalizer.bass);
        sample *= bassGain;
    } else if (freq < 4000.0f) {
        midGain = dbToLinear(currentProfile.equalizer.mid);
        sample *= midGain;
    } else {
        trebleGain = dbToLinear(currentProfile.equalizer.treble);
        sample *= trebleGain;
    }
}
