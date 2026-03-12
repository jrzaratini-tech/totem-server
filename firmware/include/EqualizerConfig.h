#ifndef EQUALIZER_CONFIG_H
#define EQUALIZER_CONFIG_H

#include <Arduino.h>

enum VolumeCurve {
    LINEAR = 0,
    EXPONENTIAL_SOFT = 1,
    EXPONENTIAL_AGGRESSIVE = 2
};

enum ESPModel {
    ESP32_DEVKIT_V1 = 0,
    ESP32_AUDIOKIT = 1,
    ESP32_S3_DEVKIT = 2,
    ESP32_GENERIC = 3
};

struct EqualizerBands {
    float bass;
    float mid;
    float treble;
};

struct AudioProfile {
    int volume;
    EqualizerBands equalizer;
    int amplifierGain;
    ESPModel espModel;
    VolumeCurve volumeCurve;
    bool clippingProtection;
    unsigned long lastCalibrated;
};

const AudioProfile DEFAULT_AUDIO_PROFILE = {
    .volume = 10,
    .equalizer = {
        .bass = 3.0f,      // Graves ultra-controlados (reduzido mais 3dB)
        .mid = 7.0f,       // Médios presentes e claros
        .treble = 8.0f     // Agudos brilhantes e cristalinos
    },
    .amplifierGain = 9,    // MAX98357A com GAIN no GND = 9dB fixo
    .espModel = ESP32_S3_DEVKIT,
    .volumeCurve = EXPONENTIAL_AGGRESSIVE,  // Curva agressiva para máximo volume
    .clippingProtection = true,
    .lastCalibrated = 0
};

#define GAIN_3DB    LOW
#define GAIN_9DB    -1
#define GAIN_15DB   HIGH

#define EQ_MIN_DB   -12.0f
#define EQ_MAX_DB   +12.0f

#define LINEAR_MIN_GAIN         0.0f
#define LINEAR_MAX_GAIN         1.0f
#define EXPONENTIAL_SOFT_BASE   0.2f
#define EXPONENTIAL_SOFT_MULT   1.8f
#define EXPONENTIAL_AGG_BASE    0.03f    // Base mínima para máximo range
#define EXPONENTIAL_AGG_MULT    3.5f     // Multiplicador aumentado para mais volume

#define LIMITER_THRESHOLD       0.85f    // Threshold muito baixo (captura graves cedo)
#define LIMITER_RATIO           6.0f     // Compressão agressiva (controla graves)
#define LIMITER_ATTACK          0.0001f  // Attack instantâneo (captura todos transientes)
#define LIMITER_RELEASE         0.10f    // Release rápido (evita estalos)
#define MAKEUP_GAIN             1.4f     // Ganho de compensação conservador
#define HEADROOM_DB             -1.0f    // Headroom amplo para evitar clipping

#endif
