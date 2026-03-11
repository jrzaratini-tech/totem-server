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
        .bass = 8.0f,      // Graves potentes e profundos
        .mid = 6.0f,       // Médios presentes e claros
        .treble = 7.0f     // Agudos brilhantes e cristalinos
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
#define EXPONENTIAL_AGG_MULT    3.0f     // Multiplicador máximo para volume alto

#define LIMITER_THRESHOLD       0.95f    // Threshold alto para máximo volume
#define LIMITER_RATIO           6.0f     // Compressão mais agressiva
#define LIMITER_ATTACK          0.001f   // Attack ultra-rápido (previne clipping)
#define LIMITER_RELEASE         0.08f    // Release suave (som natural)
#define MAKEUP_GAIN             1.5f     // Ganho de compensação máximo
#define HEADROOM_DB             -0.1f    // Headroom mínimo = volume máximo

#endif
