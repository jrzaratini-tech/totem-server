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
        .bass = 0.0f,      // Graves neutros para evitar distorção
        .mid = 2.0f,       // Médios presentes
        .treble = 3.0f     // Agudos cristalinos
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
#define EXPONENTIAL_AGG_BASE    0.05f    // Base mínima para máximo range
#define EXPONENTIAL_AGG_MULT    4.2f     // Multiplicador aumentado para mais volume

#define LIMITER_THRESHOLD       0.95f    // Threshold alto para máximo volume
#define LIMITER_RATIO           4.0f     // Compressão suave (preserva dinâmica)
#define LIMITER_ATTACK          0.001f   // Attack rápido mas não instantâneo
#define LIMITER_RELEASE         0.05f    // Release rápido (evita estalos)
#define MAKEUP_GAIN             1.15f    // Ganho de compensação otimizado
#define HEADROOM_DB             -0.5f    // Headroom mínimo para máximo volume

#endif
