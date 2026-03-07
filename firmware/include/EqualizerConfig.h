#ifndef EQUALIZER_CONFIG_H
#define EQUALIZER_CONFIG_H

#include <Arduino.h>

// ========== AUDIO EQUALIZER CONFIGURATION ==========

// Volume curve types
enum VolumeCurve {
    LINEAR = 0,
    EXPONENTIAL_SOFT = 1,
    EXPONENTIAL_AGGRESSIVE = 2
};

// ESP32 model presets for audio compensation
enum ESPModel {
    ESP32_DEVKIT_V1 = 0,
    ESP32_AUDIOKIT = 1,
    ESP32_S3_DEVKIT = 2,
    ESP32_GENERIC = 3
};

// Equalizer band structure
struct EqualizerBands {
    float bass;      // -12.0 to +12.0 dB (60Hz)
    float mid;       // -12.0 to +12.0 dB (1kHz)
    float treble;    // -12.0 to +12.0 dB (10kHz)
};

// Audio profile configuration
struct AudioProfile {
    int volume;                          // 0-10
    EqualizerBands equalizer;            // EQ bands
    int amplifierGain;                   // 3, 9, or 15 dB
    ESPModel espModel;                   // ESP model compensation
    VolumeCurve volumeCurve;             // Volume curve type
    bool clippingProtection;             // Enable/disable clipping protection
    unsigned long lastCalibrated;        // Timestamp of last calibration
};

// Default audio profile
const AudioProfile DEFAULT_AUDIO_PROFILE = {
    .volume = 8,
    .equalizer = {
        .bass = 0.0f,
        .mid = 0.0f,
        .treble = 0.0f
    },
    .amplifierGain = 15,
    .espModel = ESP32_DEVKIT_V1,
    .volumeCurve = EXPONENTIAL_SOFT,
    .clippingProtection = true,
    .lastCalibrated = 0
};

// Amplifier gain pin states
#define GAIN_3DB    LOW
#define GAIN_9DB    -1      // Float (high impedance)
#define GAIN_15DB   HIGH

// EQ limits
#define EQ_MIN_DB   -12.0f
#define EQ_MAX_DB   +12.0f

// Volume curve parameters
#define LINEAR_MIN_GAIN         0.0f
#define LINEAR_MAX_GAIN         1.0f
#define EXPONENTIAL_SOFT_BASE   0.3f
#define EXPONENTIAL_SOFT_MULT   1.3f
#define EXPONENTIAL_AGG_BASE    0.1f
#define EXPONENTIAL_AGG_MULT    2.0f

#endif
