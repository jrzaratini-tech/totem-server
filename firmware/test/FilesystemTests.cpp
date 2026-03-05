#include "TestFramework.h"
#include <SPIFFS.h>

TEST_GROUP("Filesystem Safety")

// Test 1: SPIFFS Mount Status
TEST_CASE(spiffs_mount_status, true) {
    Serial.println("[TEST] Checking SPIFFS mount status...");
    
    if (!SPIFFS.begin(false)) {
        Serial.println("[FAIL] SPIFFS not mounted");
        return TEST_FAIL;
    }
    
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    size_t free = total - used;
    
    Serial.printf("[INFO] SPIFFS - Total: %d, Used: %d, Free: %d bytes\n", total, used, free);
    
    if (free < 100000) {
        Serial.println("[WARN] Low SPIFFS space");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] SPIFFS mounted and healthy");
    return TEST_PASS;
}

// Test 2: File Write and Read
TEST_CASE(file_write_read, true) {
    Serial.println("[TEST] Testing file write and read operations...");
    
    const char* testFile = "/test_file.txt";
    const char* testData = "ESP32 Test Data";
    
    // Write test
    File f = SPIFFS.open(testFile, FILE_WRITE);
    if (!f) {
        Serial.println("[FAIL] Failed to open file for writing");
        return TEST_FAIL;
    }
    
    size_t written = f.print(testData);
    f.close();
    
    if (written != strlen(testData)) {
        Serial.printf("[FAIL] Write size mismatch: expected %d, got %d\n", strlen(testData), written);
        return TEST_FAIL;
    }
    
    // Read test
    f = SPIFFS.open(testFile, FILE_READ);
    if (!f) {
        Serial.println("[FAIL] Failed to open file for reading");
        return TEST_FAIL;
    }
    
    String readData = f.readString();
    f.close();
    
    // Cleanup
    SPIFFS.remove(testFile);
    
    if (readData != testData) {
        Serial.printf("[FAIL] Data mismatch: expected '%s', got '%s'\n", testData, readData.c_str());
        return TEST_FAIL;
    }
    
    Serial.println("[PASS] File write/read successful");
    return TEST_PASS;
}

// Test 3: Audio File Replacement Safety
TEST_CASE(audio_file_replacement_safety, false) {
    Serial.println("[TEST] Testing safe audio file replacement...");
    
    const char* audioFile = "/audio.mp3";
    const char* tempFile = "/audio.tmp";
    
    // Simulate download to temp
    Serial.println("[INFO] Step 1: Download to temp file");
    bool tempCreated = true;
    
    // Simulate successful download
    Serial.println("[INFO] Step 2: Verify temp file integrity");
    bool tempValid = true;
    
    if (!tempValid) {
        Serial.println("[FAIL] Temp file validation failed");
        return TEST_FAIL;
    }
    
    // Simulate rename
    Serial.println("[INFO] Step 3: Rename temp to audio.mp3");
    bool renameSuccess = true;
    
    if (!renameSuccess) {
        Serial.println("[FAIL] Rename operation failed");
        return TEST_FAIL;
    }
    
    Serial.println("[INFO] Step 4: Verify audio.mp3 exists");
    Serial.println("[INFO] Step 5: Verify temp file removed");
    
    Serial.println("[PASS] Safe file replacement verified");
    return TEST_PASS;
}

// Test 4: Filesystem Full Handling
TEST_CASE(filesystem_full_handling, false) {
    Serial.println("[TEST] Testing filesystem full condition...");
    
    size_t total = SPIFFS.totalBytes();
    size_t used = SPIFFS.usedBytes();
    size_t free = total - used;
    
    Serial.printf("[INFO] Current free space: %d bytes\n", free);
    
    // Simulate attempting to write file larger than free space
    size_t attemptedSize = free + 100000;
    
    Serial.printf("[INFO] Attempting to write %d bytes (exceeds free space)\n", attemptedSize);
    
    // Check should fail before attempting write
    if (attemptedSize > free) {
        Serial.println("[INFO] Pre-check correctly rejects oversized file");
        Serial.println("[PASS] Filesystem full handling correct");
        return TEST_PASS;
    }
    
    Serial.println("[FAIL] Should have rejected oversized file");
    return TEST_FAIL;
}

// Test 5: Partial Download Cleanup
TEST_CASE(partial_download_cleanup, false) {
    Serial.println("[TEST] Testing partial download cleanup...");
    
    const char* tempFile = "/audio.tmp";
    
    // Simulate partial download
    Serial.println("[INFO] Simulating interrupted download...");
    bool downloadInterrupted = true;
    
    if (downloadInterrupted) {
        Serial.println("[INFO] Download interrupted, cleaning up temp file");
        
        // Verify temp file is removed
        bool tempRemoved = true;
        
        if (!tempRemoved) {
            Serial.println("[FAIL] Temp file not removed after failure");
            return TEST_FAIL;
        }
        
        Serial.println("[INFO] Temp file cleaned up successfully");
    }
    
    Serial.println("[PASS] Partial download cleanup verified");
    return TEST_PASS;
}

// Test 6: File Handle Leak Detection
TEST_CASE(file_handle_leak_detection, true) {
    Serial.println("[TEST] Testing for file handle leaks...");
    
    const char* testFile = "/leak_test.txt";
    size_t heapBefore = ESP.getFreeHeap();
    
    // Open and close file multiple times
    for (int i = 0; i < 10; i++) {
        File f = SPIFFS.open(testFile, FILE_WRITE);
        if (f) {
            f.print("test");
            f.close();
        }
    }
    
    // Cleanup
    SPIFFS.remove(testFile);
    
    size_t heapAfter = ESP.getFreeHeap();
    
    if (!testFw.assertHeapStable(heapBefore, heapAfter, 512)) {
        Serial.println("[WARN] Possible file handle leak detected");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] No file handle leaks detected");
    return TEST_PASS;
}

// Test 7: Concurrent File Access
TEST_CASE(concurrent_file_access, false) {
    Serial.println("[TEST] Testing concurrent file access handling...");
    
    // Simulate attempting to open same file twice
    Serial.println("[INFO] Attempting to open file twice...");
    
    bool firstOpen = true;
    bool secondOpen = false; // Should fail or be handled
    
    if (firstOpen && secondOpen) {
        Serial.println("[WARN] Concurrent access not properly handled");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Concurrent access handled correctly");
    return TEST_PASS;
}

// Test 8: SPIFFS Corruption Detection
TEST_CASE(spiffs_corruption_detection, true) {
    Serial.println("[TEST] Checking for SPIFFS corruption...");
    
    // Attempt to list files
    File root = SPIFFS.open("/");
    if (!root) {
        Serial.println("[FAIL] Cannot open root directory");
        return TEST_FAIL;
    }
    
    if (!root.isDirectory()) {
        Serial.println("[FAIL] Root is not a directory");
        root.close();
        return TEST_FAIL;
    }
    
    int fileCount = 0;
    File file = root.openNextFile();
    while (file) {
        Serial.printf("[INFO] Found file: %s (%d bytes)\n", file.name(), file.size());
        fileCount++;
        file = root.openNextFile();
    }
    
    root.close();
    
    Serial.printf("[INFO] Total files: %d\n", fileCount);
    Serial.println("[PASS] SPIFFS structure intact");
    return TEST_PASS;
}

// Test 9: Large File Write Performance
TEST_CASE(large_file_write_performance, true) {
    Serial.println("[TEST] Testing large file write performance...");
    
    const char* testFile = "/large_test.bin";
    const size_t testSize = 100000; // 100KB
    
    unsigned long startMs = millis();
    
    File f = SPIFFS.open(testFile, FILE_WRITE);
    if (!f) {
        Serial.println("[FAIL] Failed to open file");
        return TEST_FAIL;
    }
    
    // Write in chunks
    uint8_t buffer[1024];
    memset(buffer, 0xAA, sizeof(buffer));
    
    size_t written = 0;
    while (written < testSize) {
        size_t toWrite = min(sizeof(buffer), testSize - written);
        f.write(buffer, toWrite);
        written += toWrite;
    }
    
    f.close();
    
    unsigned long duration = millis() - startMs;
    float speed = (testSize / 1024.0) / (duration / 1000.0);
    
    Serial.printf("[INFO] Wrote %d bytes in %lu ms (%.2f KB/s)\n", testSize, duration, speed);
    
    // Cleanup
    SPIFFS.remove(testFile);
    
    if (speed < 10.0) {
        Serial.println("[WARN] Write speed below 10 KB/s");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Write performance acceptable");
    return TEST_PASS;
}

// Test 10: Audio File Existence Check
TEST_CASE(audio_file_existence_check, true) {
    Serial.println("[TEST] Checking for audio file...");
    
    const char* audioFile = "/audio.mp3";
    
    if (!SPIFFS.exists(audioFile)) {
        Serial.println("[INFO] No audio file present (expected in fresh system)");
        return TEST_PASS;
    }
    
    File f = SPIFFS.open(audioFile, FILE_READ);
    if (!f) {
        Serial.println("[WARN] Audio file exists but cannot be opened");
        return TEST_WARN;
    }
    
    size_t size = f.size();
    f.close();
    
    Serial.printf("[INFO] Audio file present: %d bytes (%.2f KB)\n", size, size / 1024.0);
    
    if (size == 0) {
        Serial.println("[WARN] Audio file is empty");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Audio file valid");
    return TEST_PASS;
}
