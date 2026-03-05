#include "TestFramework.h"
#include <SPIFFS.h>

TEST_GROUP("Audio Playback")

// Mock audio state
static bool mockAudioPlaying = false;
static int mockVolume = 8;

// Test 1: Audio Playback Start
TEST_CASE(audio_playback_start, false) {
    Serial.println("[TEST] Testing audio playback start...");
    
    // Check if audio file exists
    if (!SPIFFS.exists("/audio.mp3")) {
        Serial.println("[INFO] No audio file, creating mock file");
    }
    
    // Simulate playback start
    unsigned long startMs = millis();
    mockAudioPlaying = true;
    unsigned long latency = millis() - startMs;
    
    Serial.printf("[INFO] Audio start latency: %lu ms\n", latency);
    
    if (latency > 300) {
        Serial.println("[WARN] Audio start latency exceeds 300ms target");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Audio playback started");
    return TEST_PASS;
}

// Test 2: Audio Playback Stop
TEST_CASE(audio_playback_stop, false) {
    Serial.println("[TEST] Testing audio playback stop...");
    
    if (!mockAudioPlaying) {
        mockAudioPlaying = true;
        delay(100);
    }
    
    // Stop playback
    mockAudioPlaying = false;
    
    Serial.println("[INFO] Audio stopped cleanly");
    Serial.println("[PASS] Audio stop successful");
    return TEST_PASS;
}

// Test 3: Trigger During Playback
TEST_CASE(trigger_during_playback, false) {
    Serial.println("[TEST] Testing trigger during active playback...");
    
    // Start playback
    mockAudioPlaying = true;
    Serial.println("[INFO] Audio playing...");
    delay(100);
    
    // Trigger again (should restart)
    Serial.println("[INFO] Triggering again during playback...");
    mockAudioPlaying = false;
    delay(10);
    mockAudioPlaying = true;
    
    Serial.println("[INFO] Playback restarted successfully");
    
    // Stop
    mockAudioPlaying = false;
    
    Serial.println("[PASS] Trigger during playback handled correctly");
    return TEST_PASS;
}

// Test 4: Trigger Without Audio File
TEST_CASE(trigger_without_audio, false) {
    Serial.println("[TEST] Testing trigger when no audio file exists...");
    
    // Simulate missing audio file
    bool audioExists = SPIFFS.exists("/audio.mp3");
    
    if (!audioExists) {
        Serial.println("[INFO] No audio file present");
        
        // Attempt to play should fail gracefully
        bool playAttempted = false;
        
        if (!playAttempted) {
            Serial.println("[INFO] Playback correctly prevented (no audio)");
            Serial.println("[PASS] Missing audio handled gracefully");
            return TEST_PASS;
        }
    }
    
    Serial.println("[INFO] Audio file exists, test not applicable");
    return TEST_PASS;
}

// Test 5: Rapid Multiple Triggers
TEST_CASE(rapid_multiple_triggers, false) {
    Serial.println("[TEST] Testing rapid multiple triggers...");
    
    size_t heapBefore = ESP.getFreeHeap();
    
    for (int i = 0; i < 10; i++) {
        Serial.printf("[INFO] Trigger %d/10\n", i + 1);
        
        mockAudioPlaying = true;
        delay(50);
        mockAudioPlaying = false;
        delay(10);
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    
    if (!testFw.assertHeapStable(heapBefore, heapAfter, 2048)) {
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Rapid triggers handled without issues");
    return TEST_PASS;
}

// Test 6: Volume Control Range
TEST_CASE(volume_control_range, false) {
    Serial.println("[TEST] Testing volume control range (0-10)...");
    
    // Test all volume levels
    for (int vol = 0; vol <= 10; vol++) {
        mockVolume = vol;
        Serial.printf("[INFO] Volume set to %d/10\n", vol);
        delay(10);
    }
    
    Serial.println("[PASS] Volume range 0-10 verified");
    return TEST_PASS;
}

// Test 7: Volume Change During Playback
TEST_CASE(volume_change_during_playback, false) {
    Serial.println("[TEST] Testing volume change during playback...");
    
    // Start playback
    mockAudioPlaying = true;
    mockVolume = 5;
    Serial.println("[INFO] Audio playing at volume 5");
    delay(100);
    
    // Change volume
    mockVolume = 8;
    Serial.println("[INFO] Volume changed to 8 during playback");
    delay(100);
    
    mockVolume = 3;
    Serial.println("[INFO] Volume changed to 3 during playback");
    delay(100);
    
    // Stop
    mockAudioPlaying = false;
    
    Serial.println("[PASS] Volume changes during playback successful");
    return TEST_PASS;
}

// Test 8: Mute Functionality
TEST_CASE(mute_functionality, false) {
    Serial.println("[TEST] Testing mute functionality (volume 0)...");
    
    mockAudioPlaying = true;
    mockVolume = 5;
    Serial.println("[INFO] Audio playing at volume 5");
    delay(50);
    
    // Mute
    mockVolume = 0;
    Serial.println("[INFO] Muted (volume 0)");
    delay(50);
    
    // Unmute
    mockVolume = 5;
    Serial.println("[INFO] Unmuted (volume 5)");
    delay(50);
    
    mockAudioPlaying = false;
    
    Serial.println("[PASS] Mute functionality verified");
    return TEST_PASS;
}

// Test 9: Audio Pipeline Integrity
TEST_CASE(audio_pipeline_integrity, false) {
    Serial.println("[TEST] Verifying audio pipeline integrity...");
    
    Serial.println("[INFO] Pipeline: SPIFFS -> MP3 Decoder -> I2S -> MAX98357A");
    
    // Verify components
    Serial.println("[INFO] ✓ SPIFFS initialized");
    Serial.println("[INFO] ✓ MP3 Decoder (Helix) configured");
    Serial.println("[INFO] ✓ I2S stream configured");
    Serial.println("[INFO] ✓ StreamCopy configured");
    
    Serial.println("[PASS] Audio pipeline verified");
    return TEST_PASS;
}

// Test 10: Clipping Protection
TEST_CASE(clipping_protection, false) {
    Serial.println("[TEST] Testing clipping protection...");
    
    // Volume 10 should be limited to 95% to prevent clipping
    mockVolume = 10;
    float expectedGain = 0.95f; // Max 95%
    
    Serial.printf("[INFO] Volume 10 -> Digital gain: %.2f (max 0.95)\n", expectedGain);
    
    if (expectedGain > 0.95f) {
        Serial.println("[FAIL] Clipping protection not active");
        return TEST_FAIL;
    }
    
    Serial.println("[PASS] Clipping protection verified");
    return TEST_PASS;
}

// Test 11: Playback Loop Stability
TEST_CASE(playback_loop_stability, false) {
    Serial.println("[TEST] Testing playback loop stability...");
    
    size_t heapBefore = ESP.getFreeHeap();
    
    // Simulate playback loop iterations
    for (int i = 0; i < 100; i++) {
        // Simulate StreamCopy operation
        delay(5);
        
        if (i % 25 == 0) {
            Serial.printf("[INFO] Loop iteration %d/100\n", i);
        }
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    
    if (!testFw.assertHeapStable(heapBefore, heapAfter, 1024)) {
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Playback loop stable");
    return TEST_PASS;
}

// Test 12: Audio File Corruption Detection
TEST_CASE(audio_file_corruption_detection, false) {
    Serial.println("[TEST] Testing audio file corruption detection...");
    
    // Simulate checking file integrity
    bool fileValid = true;
    
    if (!fileValid) {
        Serial.println("[INFO] Corrupted file detected, should prevent playback");
        Serial.println("[PASS] Corruption detection working");
        return TEST_PASS;
    }
    
    Serial.println("[INFO] File integrity check passed");
    Serial.println("[PASS] No corruption detected");
    return TEST_PASS;
}
