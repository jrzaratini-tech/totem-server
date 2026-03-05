#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== IDENTIFICAÇÃO DO TOTEM ==========
#define TOTEM_ID                "printpixel"
#define DEFAULT_TOTEM_ID        TOTEM_ID
#define FORCE_TOTEM_ID          TOTEM_ID
#define FIRMWARE_VERSION        "4.1.0"
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

// Watchdog
#define WDT_TIMEOUT_SECONDS     8

// Debug/segurança: evita loop de reboot por chamadas de ESP.restart() em callbacks
#define DISABLE_AUTO_RESTART    1

// Failsafe
#define FAILSAFE_BRIGHTNESS     30

// ========== HARDWARE - LEDS ==========
#define NUM_LEDS_MAIN           220
#define NUM_LEDS_HEART          15
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define LED_MAIN_PIN            19
#define LED_HEART_PIN           18
#define MAX_BRIGHTNESS          180
#define DEFAULT_BRIGHTNESS      120

// ========== HARDWARE - BOTÕES ==========
// NOTA: Pinos ajustados para não conflitar com I2S/I2C do Audio-Kit
#define PIN_BTN_COR             15
#define PIN_BTN_MAIS            4
#define PIN_BTN_MENOS           16
#define PIN_BTN_CORACAO         5
#define PIN_BTN_RESET_WIFI      22
#define DEBOUNCE_DELAY          50
#define LONG_PRESS_TIME         5000
#define MIN_CLICK_INTERVAL      150

// ========== HARDWARE - ÁUDIO I2S ==========
// ESP32-Audio-Kit A1S com ES8388
// Pinos I2S corretos para ESP32-Audio-Kit V2.2 A1S
#define I2S_BCLK                27
#define I2S_LRC                 25
#define I2S_DOUT                26
#define I2S_DIN                 35
// Pinos I2C para ES8388 codec
#define I2C_SDA                 33
#define I2C_SCL                 32
#define PA_ENABLE_PIN           21  // Power Amplifier Enable
// Volume padrão (0-21, mapeado de 0-10 do servidor)
#define DEFAULT_VOLUME          15
#define MIN_VOLUME              0
#define MAX_VOLUME              21

// ========== HARDWARE - SD CARD (opcional) ==========
#define SD_CS                   5
#define SD_MOSI                 23
#define SD_MISO                 19
#define SD_SCK                  18

// ========== COMPORTAMENTO ==========
#define EFEITO_TEMPO_PADRAO     30
#define STATUS_INTERVAL         60000
#define DOWNLOAD_TIMEOUT        300000
#define OTA_TIMEOUT             300000
#define MAX_AUDIO_SIZE          (8 * 1024 * 1024)

// ========== TOPICS MQTT ==========
// Os tópicos são montados em runtime com o TOTEM_ID provisionado.

// ========== ARQUIVOS ==========
#define AUDIO_FILENAME          "/audio.mp3"
#define AUDIO_TEMP_FILENAME     "/audio.tmp"

enum EffectMode {
    SOLID = 0,
    RAINBOW = 1,
    BLINK = 2,
    BREATH = 3,
    RUNNING = 4,
    HEART = 5
};

#endif
