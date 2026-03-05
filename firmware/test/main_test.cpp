#include <Arduino.h>
#include "TestFramework.h"

// Test mode selection
// Set to true for hardware tests, false for simulated tests
#define HARDWARE_MODE false

// Test group selection
#define RUN_WIFI_TESTS true
#define RUN_MQTT_TESTS true
#define RUN_FILESYSTEM_TESTS true
#define RUN_AUDIO_TESTS true
#define RUN_MEMORY_TESTS true
#define RUN_STRESS_TESTS true

void setup() {
    Serial.begin(115200);
    delay(2000); // Wait for serial monitor
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║     ESP32 FIRMWARE AUTOMATED TEST FRAMEWORK v1.0           ║");
    Serial.println("║              IoT Interactive Totem System                  ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println();
    
    // Configure test framework
    testFw.setHardwareMode(HARDWARE_MODE);
    
    Serial.printf("Test Mode: %s\n", HARDWARE_MODE ? "HARDWARE" : "SIMULATED");
    Serial.println();
    Serial.println("Press any key to start tests...");
    
    // Wait for user input
    while (!Serial.available()) {
        delay(100);
    }
    while (Serial.available()) Serial.read(); // Clear buffer
    
    Serial.println("\n[SYSTEM] Starting test execution...\n");
    delay(1000);
    
    // Run selected test groups
    if (RUN_WIFI_TESTS) {
        Serial.println("\n>>> Running WiFi Stability Tests <<<\n");
        testFw.runGroup("WiFi Stability");
        delay(1000);
    }
    
    if (RUN_MQTT_TESTS) {
        Serial.println("\n>>> Running MQTT Reliability Tests <<<\n");
        testFw.runGroup("MQTT Reliability");
        delay(1000);
    }
    
    if (RUN_FILESYSTEM_TESTS) {
        Serial.println("\n>>> Running Filesystem Safety Tests <<<\n");
        testFw.runGroup("Filesystem Safety");
        delay(1000);
    }
    
    if (RUN_AUDIO_TESTS) {
        Serial.println("\n>>> Running Audio Playback Tests <<<\n");
        testFw.runGroup("Audio Playback");
        delay(1000);
    }
    
    if (RUN_MEMORY_TESTS) {
        Serial.println("\n>>> Running Memory Stability Tests <<<\n");
        testFw.runGroup("Memory Stability");
        delay(1000);
    }
    
    if (RUN_STRESS_TESTS) {
        Serial.println("\n>>> Running Stress & Long Duration Tests <<<\n");
        testFw.runGroup("Stress & Long Duration");
        delay(1000);
    }
    
    // Generate final report
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║              GENERATING FINAL REPORT                       ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println();
    
    testFw.generateReport();
    
    Serial.println("\n[SYSTEM] Test execution complete.");
    Serial.println("[SYSTEM] Review the report above for deployment decision.");
}

void loop() {
    // Test framework runs once in setup()
    // Loop does nothing
    delay(1000);
}
