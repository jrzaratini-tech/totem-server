#ifndef AUDIO_EQUALIZER_H
#define AUDIO_EQUALIZER_H

#include <Arduino.h>
#include "EqualizerConfig.h"

class AudioEqualizer {
private:
    AudioProfile currentProfile;
    float currentGain;
    float currentBoost;
    int16_t peakLevel;
    unsigned long clippingEvents;
    float limiterEnvelope;
    
    float applyVolumeCurveInternal(int volume, VolumeCurve curve);
    float dbToLinear(float db);
    void applyESPModelCompensation();
    float applyLimiter(float sample);
    
public:
    AudioEqualizer();
    
    void begin();
    void setProfile(const AudioProfile &profile);
    AudioProfile getProfile() const;
    
    void setVolume(int vol);
    void setEqualizer(float bass, float mid, float treble);
    void setAmplifierGain(int gainDB);
    void setESPModel(ESPModel model);
    void setVolumeCurve(VolumeCurve curve);
    void setClippingProtection(bool enabled);
    
    float applyVolumeCurve(int volume);
    float getCurrentGain() const;
    float getCurrentBoost() const;
    float getTotalGain() const;
    
    void applyEQToSample(int16_t &left, int16_t &right);
    bool checkClipping(int16_t sample);
    void applyMultibandEQ(float &sample, float freq);
    
    void resetMetrics();
    int16_t getPeakLevel() const;
    unsigned long getClippingEvents() const;
    float getLimiterGainReduction() const;
    
    void printProfile() const;
    String profileToJSON() const;
    bool profileFromJSON(const String &json);
};

#endif
