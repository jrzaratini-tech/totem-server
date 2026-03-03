#include "core/ConfigManager.h"
#include "utils/Helpers.h"

ConfigManager::ConfigManager() {
    cfg.mode = BREATH;
    cfg.color = 0xFF3366;
    cfg.speed = 50;
    cfg.duration = EFEITO_TEMPO_PADRAO;
    cfg.maxBrightness = DEFAULT_BRIGHTNESS;
    audioVersion = 0;
}

void ConfigManager::begin() {
    load();
}

void ConfigManager::load() {
    if (!prefs.begin("cfg", true)) {
        prefs.begin("cfg", false);
    }
    cfg.mode = (EffectMode)prefs.getUChar("mode", (uint8_t)BREATH);
    cfg.color = prefs.getUInt("color", 0xFF3366);
    cfg.speed = prefs.getInt("speed", 50);
    cfg.duration = prefs.getInt("dur", EFEITO_TEMPO_PADRAO);
    cfg.maxBrightness = prefs.getInt("bri", DEFAULT_BRIGHTNESS);
    audioVersion = prefs.getInt("av", 0);
    prefs.end();
}

void ConfigManager::save() {
    prefs.begin("cfg", false);
    prefs.putUChar("mode", (uint8_t)cfg.mode);
    prefs.putUInt("color", cfg.color);
    prefs.putInt("speed", cfg.speed);
    prefs.putInt("dur", cfg.duration);
    prefs.putInt("bri", cfg.maxBrightness);
    prefs.putInt("av", audioVersion);
    prefs.end();
}

uint32_t ConfigManager::hashPayload(const String &json) const {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < json.length(); i++) {
        h ^= (uint8_t)json[i];
        h *= 16777619u;
    }
    return h;
}

bool ConfigManager::isDuplicateConfigUpdate(const String &json) {
    const uint32_t cur = hashPayload(json);
    if (!prefs.begin("cfg", true)) {
        prefs.begin("cfg", false);
    }
    const uint32_t last = prefs.getUInt("lcrc", 0);
    prefs.end();
    return (last != 0) && (last == cur);
}

void ConfigManager::rememberConfigUpdate(const String &json) {
    const uint32_t cur = hashPayload(json);
    prefs.begin("cfg", false);
    prefs.putUInt("lcrc", cur);
    prefs.end();
}

bool ConfigManager::updateFromJson(const String &json) {
    StaticJsonDocument<512> doc;
    auto err = deserializeJson(doc, json);
    if (err) {
        return false;
    }

    if (doc.containsKey("mode")) {
        String m = doc["mode"].as<String>();
        m.toUpperCase();
        if (m == "SOLID") cfg.mode = SOLID;
        else if (m == "RAINBOW") cfg.mode = RAINBOW;
        else if (m == "BLINK") cfg.mode = BLINK;
        else if (m == "BREATH") cfg.mode = BREATH;
        else if (m == "RUNNING") cfg.mode = RUNNING;
        else if (m == "HEART" || m == "HEARTBEAT") cfg.mode = HEART;
    }

    if (doc.containsKey("color")) {
        cfg.color = parseHexColor(doc["color"].as<String>(), cfg.color);
    }

    if (doc.containsKey("speed")) cfg.speed = doc["speed"].as<int>();
    if (doc.containsKey("duration")) cfg.duration = doc["duration"].as<int>();
    if (doc.containsKey("maxBrightness")) cfg.maxBrightness = doc["maxBrightness"].as<int>();

    cfg.speed = max(1, cfg.speed);
    cfg.duration = max(1, cfg.duration);
    cfg.maxBrightness = constrain(cfg.maxBrightness, 0, MAX_BRIGHTNESS);

    save();
    return true;
}

ConfigData ConfigManager::getEffectConfig() const { return cfg; }

int ConfigManager::getAudioVersion() const { return audioVersion; }

void ConfigManager::setAudioVersion(int v) {
    audioVersion = max(0, v);
    save();
}

uint32_t ConfigManager::getColor() const { return cfg.color; }

void ConfigManager::cycleColor() {
    static uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFFFFFF, 0xFF3366};
    static int idx = 0;
    idx = (idx + 1) % (int)(sizeof(colors) / sizeof(colors[0]));
    cfg.color = colors[idx];
    save();
}

int ConfigManager::getBrightness() const { return cfg.maxBrightness; }

void ConfigManager::setBrightness(int b) {
    cfg.maxBrightness = constrain(b, 0, MAX_BRIGHTNESS);
    save();
}
