#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== IDENTIFICAÇÃO DO TOTEM ==========
#define TOTEM_ID                "printpixel"
#define DEFAULT_TOTEM_ID        TOTEM_ID
#define FORCE_TOTEM_ID          TOTEM_ID
#define FIRMWARE_VERSION        "4.2.1"
#define SERVER_URL              "https://totem-server.onrender.com"

// ========== CONFIGURAÇÕES DE REDE ==========
#define MQTT_SERVER             "broker.hivemq.com"
#define MQTT_PORT               1883
#define MQTT_USER               ""
#define MQTT_PASSWORD           ""
#define WIFI_TIMEOUT            20000
#define WIFI_RECONNECT_INTERVAL 30000

// Segurança/Token por dispositivo (opcional)
#define DEVICE_TOKEN            ""

// HTTPS (CA raiz para validar TLS). Se vazio, o HTTPS pode falhar.
// Recomenda-se inserir o CA correto do domínio/bucket.
#define ROOT_CA_PEM             ""

// Se ROOT_CA_PEM estiver vazio, permite HTTPS sem validação de certificado (insecure).
// Útil para testes e ambientes onde não foi provisionado o CA.
#define ALLOW_INSECURE_HTTPS    1

// Watchdog - 90 segundos para suportar áudios de até 60 segundos sem interrupção
#define WDT_TIMEOUT_SECONDS     90

// Debug/segurança: evita loop de reboot por chamadas de ESP.restart() em callbacks
#define DISABLE_AUTO_RESTART    1

// Failsafe
#define FAILSAFE_BRIGHTNESS     30

#define NUM_LEDS_MAIN           200
#define NUM_LEDS_HEART          9
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define LED_MAIN_PIN            8
#define LED_HEART_PIN           9
#define MAX_BRIGHTNESS          180
#define DEFAULT_BRIGHTNESS      120

#define PIN_BTN_TRIGGER         10
#define PIN_BTN_RESET_WIFI      11
#define PIN_BTN_HEARTBEAT       3
#define DEBOUNCE_DELAY          50
#define LONG_PRESS_TIME         5000
#define MIN_CLICK_INTERVAL      150

#define PIN_BTN_COR             10
#define PIN_BTN_MAIS            12
#define PIN_BTN_MENOS           13
#define PIN_BTN_CORACAO         10
#define PIN_BTN_CORACAO_ALIAS   PIN_BTN_TRIGGER

#define I2S_BCLK                6
#define I2S_LRC                 7
#define I2S_DOUT                5

#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_BITS_PER_SAMPLE   16
#define AUDIO_CHANNELS          2

#define I2S_DMA_BUFFER_COUNT    16
#define I2S_DMA_BUFFER_SIZE     1024
#define AUDIO_PREALLOC_SIZE     4096

#define DEFAULT_VOLUME          10
#define MIN_VOLUME              0
#define MAX_VOLUME              10
#define AUDIO_DIAGNOSTICS       1
#define AUDIO_TEST_TONE_FREQ    1000
#define AUDIO_TEST_TONE_DUR     5

#define SD_CS                   14
#define SD_MOSI                 15
#define SD_MISO                 16
#define SD_SCK                  17

// ========== COMPORTAMENTO ==========
#define EFEITO_TEMPO_PADRAO     30
#define STATUS_INTERVAL         60000
#define DOWNLOAD_TIMEOUT        120000
#define OTA_TIMEOUT             300000
#define MAX_AUDIO_SIZE          (5 * 1024 * 1024)
#define DOWNLOAD_BUFFER_SIZE    4096


// ========== ARQUIVOS ==========
#define AUDIO_FILENAME          "/audio.mp3"
#define AUDIO_TEMP_FILENAME     "/audio.tmp"

enum EffectMode {
    SOLID = 0,
    RAINBOW = 1,
    BLINK = 2,
    BREATH = 3,
    RUNNING = 4,
    HEART = 5,
    METEOR = 6,
    PIKSEL = 7,
    BOUNCE = 8,
    SPARKLE = 9
};

#endif
