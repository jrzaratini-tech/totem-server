#include "TestFramework.h"

TestFramework testFw;

TestFramework::TestFramework(bool hwMode) {
    hardwareMode = hwMode;
    totalTests = 0;
    passedTests = 0;
    failedTests = 0;
    warnTests = 0;
    skippedTests = 0;
    testStartTime = 0;
}

void TestFramework::addTest(const String &group, const String &name, std::function<TestResult()> func, bool hwOnly) {
    TestCase test;
    test.name = name;
    test.group = group;
    test.testFunc = func;
    test.result = TEST_SKIP;
    test.hardwareOnly = hwOnly;
    tests.push_back(test);
}

void TestFramework::logTestStart(const String &name) {
    Serial.println();
    Serial.println("========================================");
    Serial.printf("[TEST] Running: %s\n", name.c_str());
    Serial.println("========================================");
}

void TestFramework::logTestEnd(const String &name, TestResult result, unsigned long duration) {
    const char* resultStr = "";
    switch (result) {
        case TEST_PASS: resultStr = "✓ PASS"; break;
        case TEST_FAIL: resultStr = "✗ FAIL"; break;
        case TEST_WARN: resultStr = "⚠ WARN"; break;
        case TEST_SKIP: resultStr = "○ SKIP"; break;
    }
    
    Serial.printf("[TEST] %s: %s (%lu ms)\n", name.c_str(), resultStr, duration);
    Serial.println("========================================");
}

void TestFramework::collectMetrics(TestCase &test) {
    test.metrics.endMs = millis();
    test.metrics.heapAfter = ESP.getFreeHeap();
    test.metrics.minHeap = ESP.getMinFreeHeap();
}

void TestFramework::runAllTests() {
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║          ESP32 FIRMWARE TEST FRAMEWORK v1.0                ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.printf("\nMode: %s\n", hardwareMode ? "HARDWARE" : "SIMULATED");
    Serial.printf("Total tests registered: %d\n\n", tests.size());
    
    testStartTime = millis();
    totalTests = 0;
    passedTests = 0;
    failedTests = 0;
    warnTests = 0;
    skippedTests = 0;
    
    for (auto &test : tests) {
        // Skip hardware-only tests in simulated mode
        if (test.hardwareOnly && !hardwareMode) {
            test.result = TEST_SKIP;
            skippedTests++;
            Serial.printf("[SKIP] %s (hardware only)\n", test.name.c_str());
            continue;
        }
        
        totalTests++;
        logTestStart(test.name);
        
        test.metrics.startMs = millis();
        test.metrics.heapBefore = ESP.getFreeHeap();
        
        // Run test
        test.result = test.testFunc();
        
        collectMetrics(test);
        
        unsigned long duration = test.metrics.endMs - test.metrics.startMs;
        logTestEnd(test.name, test.result, duration);
        
        switch (test.result) {
            case TEST_PASS: passedTests++; break;
            case TEST_FAIL: failedTests++; break;
            case TEST_WARN: warnTests++; break;
            default: break;
        }
        
        // Small delay between tests
        delay(100);
    }
    
    generateReport();
}

void TestFramework::runGroup(const String &group) {
    Serial.printf("\n[TEST] Running test group: %s\n\n", group.c_str());
    
    int groupTests = 0;
    int groupPassed = 0;
    int groupFailed = 0;
    
    for (auto &test : tests) {
        if (test.group != group) continue;
        
        if (test.hardwareOnly && !hardwareMode) {
            Serial.printf("[SKIP] %s (hardware only)\n", test.name.c_str());
            continue;
        }
        
        groupTests++;
        logTestStart(test.name);
        
        test.metrics.startMs = millis();
        test.metrics.heapBefore = ESP.getFreeHeap();
        
        test.result = test.testFunc();
        
        collectMetrics(test);
        
        unsigned long duration = test.metrics.endMs - test.metrics.startMs;
        logTestEnd(test.name, test.result, duration);
        
        if (test.result == TEST_PASS) groupPassed++;
        else if (test.result == TEST_FAIL) groupFailed++;
        
        delay(100);
    }
    
    Serial.printf("\n[GROUP] %s: %d/%d passed\n", group.c_str(), groupPassed, groupTests);
}

void TestFramework::generateReport() {
    unsigned long totalDuration = millis() - testStartTime;
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════════════════════════╗");
    Serial.println("║                    TEST REPORT                             ║");
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println();
    
    Serial.printf("Total Duration: %.2f seconds\n", totalDuration / 1000.0);
    Serial.printf("Total Tests:    %d\n", totalTests);
    Serial.printf("✓ Passed:       %d\n", passedTests);
    Serial.printf("✗ Failed:       %d\n", failedTests);
    Serial.printf("⚠ Warnings:     %d\n", warnTests);
    Serial.printf("○ Skipped:      %d\n", skippedTests);
    
    float passRate = totalTests > 0 ? (passedTests * 100.0f / totalTests) : 0;
    Serial.printf("\nPass Rate: %.1f%%\n", passRate);
    
    // Memory stats
    Serial.println("\n--- Memory Statistics ---");
    size_t minHeap = ESP.getMinFreeHeap();
    size_t currentHeap = ESP.getFreeHeap();
    Serial.printf("Current Free Heap: %d bytes\n", currentHeap);
    Serial.printf("Minimum Free Heap: %d bytes\n", minHeap);
    
    if (minHeap < 120000) {
        Serial.println("⚠ WARNING: Low minimum heap detected!");
    }
    
    // Failed tests detail
    if (failedTests > 0) {
        Serial.println("\n--- Failed Tests ---");
        for (const auto &test : tests) {
            if (test.result == TEST_FAIL) {
                Serial.printf("✗ %s: %s\n", test.name.c_str(), 
                             test.failureReason.length() > 0 ? test.failureReason.c_str() : "No reason provided");
            }
        }
    }
    
    // Performance metrics
    Serial.println("\n--- Performance Metrics ---");
    unsigned long totalTestTime = 0;
    unsigned long maxTestTime = 0;
    String slowestTest = "";
    
    for (const auto &test : tests) {
        if (test.result == TEST_SKIP) continue;
        unsigned long duration = test.metrics.endMs - test.metrics.startMs;
        totalTestTime += duration;
        if (duration > maxTestTime) {
            maxTestTime = duration;
            slowestTest = test.name;
        }
    }
    
    if (totalTests > 0) {
        Serial.printf("Average Test Time: %lu ms\n", totalTestTime / totalTests);
        Serial.printf("Slowest Test: %s (%lu ms)\n", slowestTest.c_str(), maxTestTime);
    }
    
    Serial.println("\n╔════════════════════════════════════════════════════════════╗");
    if (failedTests == 0 && passedTests > 0) {
        Serial.println("║              ✓ ALL TESTS PASSED                            ║");
        Serial.println("║           FIRMWARE READY FOR DEPLOYMENT                    ║");
    } else if (failedTests > 0) {
        Serial.println("║              ✗ TESTS FAILED                                ║");
        Serial.println("║           FIRMWARE NOT READY FOR DEPLOYMENT                ║");
    }
    Serial.println("╚════════════════════════════════════════════════════════════╝");
    Serial.println();
}

bool TestFramework::assertTrue(bool condition, const String &msg) {
    if (!condition) {
        Serial.printf("[ASSERT] FAILED: %s\n", msg.c_str());
        return false;
    }
    return true;
}

bool TestFramework::assertFalse(bool condition, const String &msg) {
    if (condition) {
        Serial.printf("[ASSERT] FAILED: %s (expected false)\n", msg.c_str());
        return false;
    }
    return true;
}

bool TestFramework::assertEqual(int expected, int actual, const String &msg) {
    if (expected != actual) {
        Serial.printf("[ASSERT] FAILED: %s (expected: %d, actual: %d)\n", 
                     msg.c_str(), expected, actual);
        return false;
    }
    return true;
}

bool TestFramework::assertNotNull(void* ptr, const String &msg) {
    if (ptr == nullptr) {
        Serial.printf("[ASSERT] FAILED: %s (null pointer)\n", msg.c_str());
        return false;
    }
    return true;
}

bool TestFramework::assertHeapStable(size_t before, size_t after, int toleranceBytes) {
    int diff = abs((int)before - (int)after);
    if (diff > toleranceBytes) {
        Serial.printf("[ASSERT] FAILED: Heap not stable (before: %d, after: %d, diff: %d)\n", 
                     before, after, diff);
        return false;
    }
    return true;
}
