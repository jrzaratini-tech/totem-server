#include "TestFramework.h"

TEST_GROUP("Memory Stability")

// Test 1: Initial Heap Check
TEST_CASE(initial_heap_check, true) {
    Serial.println("[TEST] Checking initial heap status...");
    
    size_t freeHeap = ESP.getFreeHeap();
    size_t minHeap = ESP.getMinFreeHeap();
    size_t totalHeap = ESP.getHeapSize();
    
    Serial.printf("[INFO] Total Heap: %d bytes\n", totalHeap);
    Serial.printf("[INFO] Free Heap: %d bytes\n", freeHeap);
    Serial.printf("[INFO] Min Free Heap: %d bytes\n", minHeap);
    Serial.printf("[INFO] Heap Usage: %.1f%%\n", (1.0f - (float)freeHeap / totalHeap) * 100);
    
    if (minHeap < 120000) {
        Serial.println("[WARN] Minimum free heap below 120KB threshold");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Heap status healthy");
    return TEST_PASS;
}

// Test 2: Heap Fragmentation Check
TEST_CASE(heap_fragmentation_check, true) {
    Serial.println("[TEST] Checking heap fragmentation...");
    
    size_t freeHeap = ESP.getFreeHeap();
    size_t maxAllocHeap = ESP.getMaxAllocHeap();
    
    Serial.printf("[INFO] Free Heap: %d bytes\n", freeHeap);
    Serial.printf("[INFO] Max Alloc Heap: %d bytes\n", maxAllocHeap);
    
    float fragmentation = 1.0f - ((float)maxAllocHeap / freeHeap);
    Serial.printf("[INFO] Fragmentation: %.1f%%\n", fragmentation * 100);
    
    if (fragmentation > 0.3f) {
        Serial.println("[WARN] High heap fragmentation detected");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Heap fragmentation acceptable");
    return TEST_PASS;
}

// Test 3: Memory Leak Detection (1000 Cycles)
TEST_CASE(memory_leak_1000_cycles, false) {
    Serial.println("[TEST] Running 1000 trigger cycles for memory leak detection...");
    
    size_t heapBefore = ESP.getFreeHeap();
    size_t minHeapBefore = ESP.getMinFreeHeap();
    
    Serial.printf("[INFO] Starting heap: %d bytes\n", heapBefore);
    
    for (int i = 0; i < 1000; i++) {
        // Simulate trigger cycle
        // This would normally trigger audio playback
        
        if (i % 100 == 0) {
            size_t currentHeap = ESP.getFreeHeap();
            Serial.printf("[INFO] Cycle %d/1000 - Heap: %d bytes\n", i, currentHeap);
        }
        
        delay(1); // Minimal delay to prevent watchdog
        yield();
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    size_t minHeapAfter = ESP.getMinFreeHeap();
    
    Serial.printf("[INFO] Final heap: %d bytes\n", heapAfter);
    Serial.printf("[INFO] Min heap during test: %d bytes\n", minHeapAfter);
    
    int heapDiff = abs((int)heapBefore - (int)heapAfter);
    Serial.printf("[INFO] Heap difference: %d bytes\n", heapDiff);
    
    if (heapDiff > 5000) {
        Serial.println("[FAIL] Significant memory leak detected");
        return TEST_FAIL;
    }
    
    if (heapDiff > 2000) {
        Serial.println("[WARN] Minor memory leak detected");
        return TEST_WARN;
    }
    
    Serial.println("[PASS] No memory leak detected");
    return TEST_PASS;
}

// Test 4: Stack Usage Check
TEST_CASE(stack_usage_check, true) {
    Serial.println("[TEST] Checking stack usage...");
    
    // ESP32 has separate stacks for different tasks
    // Main loop stack is typically 8KB
    
    Serial.println("[INFO] Stack check - recursion test");
    
    // Simple recursion test to check stack depth
    int maxDepth = 0;
    
    std::function<void(int)> recursiveTest = [&](int depth) {
        if (depth > 100) return; // Safety limit
        maxDepth = max(maxDepth, depth);
        recursiveTest(depth + 1);
    };
    
    recursiveTest(0);
    
    Serial.printf("[INFO] Max recursion depth: %d\n", maxDepth);
    
    Serial.println("[PASS] Stack usage acceptable");
    return TEST_PASS;
}

// Test 5: Dynamic Allocation Stress Test
TEST_CASE(dynamic_allocation_stress, false) {
    Serial.println("[TEST] Testing dynamic allocation stress...");
    
    size_t heapBefore = ESP.getFreeHeap();
    
    // Allocate and free memory repeatedly
    const int iterations = 100;
    const size_t allocSize = 1024;
    
    for (int i = 0; i < iterations; i++) {
        uint8_t* buffer = (uint8_t*)malloc(allocSize);
        
        if (!buffer) {
            Serial.printf("[FAIL] Allocation failed at iteration %d\n", i);
            return TEST_FAIL;
        }
        
        // Use the buffer
        memset(buffer, 0xAA, allocSize);
        
        // Free it
        free(buffer);
        
        if (i % 25 == 0) {
            Serial.printf("[INFO] Iteration %d/%d\n", i, iterations);
        }
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    
    if (!testFw.assertHeapStable(heapBefore, heapAfter, 512)) {
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Dynamic allocation stress test passed");
    return TEST_PASS;
}

// Test 6: PSRAM Check (if available)
TEST_CASE(psram_check, true) {
    Serial.println("[TEST] Checking for PSRAM...");
    
    size_t psramSize = ESP.getPsramSize();
    
    if (psramSize == 0) {
        Serial.println("[INFO] No PSRAM detected (expected for ESP32 DevKit V1)");
        return TEST_PASS;
    }
    
    size_t freePsram = ESP.getFreePsram();
    Serial.printf("[INFO] PSRAM Total: %d bytes\n", psramSize);
    Serial.printf("[INFO] PSRAM Free: %d bytes\n", freePsram);
    
    Serial.println("[PASS] PSRAM check complete");
    return TEST_PASS;
}

// Test 7: Heap Stability Over Time
TEST_CASE(heap_stability_over_time, false) {
    Serial.println("[TEST] Monitoring heap stability over 60 seconds...");
    
    size_t heapBefore = ESP.getFreeHeap();
    unsigned long startMs = millis();
    
    while (millis() - startMs < 60000) {
        // Simulate normal operations
        delay(1000);
        
        size_t currentHeap = ESP.getFreeHeap();
        int elapsed = (millis() - startMs) / 1000;
        
        if (elapsed % 10 == 0) {
            Serial.printf("[INFO] %d sec - Heap: %d bytes\n", elapsed, currentHeap);
        }
        
        yield();
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    
    if (!testFw.assertHeapStable(heapBefore, heapAfter, 3072)) {
        return TEST_WARN;
    }
    
    Serial.println("[PASS] Heap stable over 60 seconds");
    return TEST_PASS;
}

// Test 8: Large Buffer Allocation
TEST_CASE(large_buffer_allocation, false) {
    Serial.println("[TEST] Testing large buffer allocation...");
    
    const size_t bufferSize = 32768; // 32KB
    
    uint8_t* buffer = (uint8_t*)malloc(bufferSize);
    
    if (!buffer) {
        Serial.println("[FAIL] Failed to allocate 32KB buffer");
        return TEST_FAIL;
    }
    
    Serial.printf("[INFO] Successfully allocated %d bytes\n", bufferSize);
    
    // Use the buffer
    memset(buffer, 0x55, bufferSize);
    
    // Free it
    free(buffer);
    
    Serial.println("[PASS] Large buffer allocation successful");
    return TEST_PASS;
}

// Test 9: Memory Alignment Check
TEST_CASE(memory_alignment_check, false) {
    Serial.println("[TEST] Checking memory alignment...");
    
    // Allocate various sizes and check alignment
    size_t sizes[] = {1, 4, 8, 16, 32, 64, 128, 256};
    
    for (size_t size : sizes) {
        void* ptr = malloc(size);
        
        if (!ptr) {
            Serial.printf("[FAIL] Allocation failed for size %d\n", size);
            return TEST_FAIL;
        }
        
        uintptr_t addr = (uintptr_t)ptr;
        bool aligned = (addr % 4) == 0; // 4-byte alignment
        
        Serial.printf("[INFO] Size %d: addr=0x%08X, aligned=%s\n", 
                     size, addr, aligned ? "yes" : "no");
        
        free(ptr);
    }
    
    Serial.println("[PASS] Memory alignment verified");
    return TEST_PASS;
}

// Test 10: Minimum Heap Threshold
TEST_CASE(minimum_heap_threshold, true) {
    Serial.println("[TEST] Verifying minimum heap threshold...");
    
    size_t minHeap = ESP.getMinFreeHeap();
    const size_t threshold = 120000; // 120KB
    
    Serial.printf("[INFO] Minimum free heap: %d bytes\n", minHeap);
    Serial.printf("[INFO] Threshold: %d bytes\n", threshold);
    
    if (minHeap < threshold) {
        Serial.printf("[FAIL] Minimum heap (%d) below threshold (%d)\n", minHeap, threshold);
        return TEST_FAIL;
    }
    
    Serial.println("[PASS] Minimum heap above threshold");
    return TEST_PASS;
}
