# 🧪 ESP32 Firmware Automated Test Framework
## Comprehensive Testing Suite for IoT Interactive Totem

**Version:** 1.0  
**Target:** ESP32 DevKit V1 + MAX98357A  
**Framework:** Arduino + PlatformIO

---

## 📋 Overview

This automated test framework provides comprehensive validation of the ESP32 firmware before production deployment. It includes both **simulated logic tests** and **real hardware tests** to ensure firmware stability, reliability, and performance.

### Test Coverage

- ✅ WiFi connectivity and stability
- ✅ MQTT reliability and reconnection
- ✅ Filesystem safety and integrity
- ✅ Audio playback and pipeline
- ✅ Memory stability and leak detection
- ✅ Long-duration stress tests
- ✅ Power recovery scenarios
- ✅ Network resilience

---

## 🏗️ Framework Architecture

### Components

```
test/
├── TestFramework.h          # Core framework header
├── TestFramework.cpp        # Framework implementation
├── WiFiTests.cpp            # WiFi stability tests
├── MQTTTests.cpp            # MQTT reliability tests
├── FilesystemTests.cpp      # Filesystem safety tests
├── AudioTests.cpp           # Audio playback tests
├── MemoryTests.cpp          # Memory stability tests
├── StressTests.cpp          # Stress & long-duration tests
├── main_test.cpp            # Test runner
└── README-TESTING.md        # This file
```

### Test Modes

1. **SIMULATED MODE** (Default)
   - Runs logic validation without hardware
   - Fast execution
   - Safe for development
   - No external dependencies

2. **HARDWARE MODE**
   - Requires real ESP32 hardware
   - Tests actual WiFi, MQTT, SPIFFS
   - Validates hardware integration
   - Production validation

---

## 🚀 Quick Start

### 1. Configure Test Mode

Edit `test/main_test.cpp`:

```cpp
// Set to true for hardware tests, false for simulated
#define HARDWARE_MODE false

// Enable/disable test groups
#define RUN_WIFI_TESTS true
#define RUN_MQTT_TESTS true
#define RUN_FILESYSTEM_TESTS true
#define RUN_AUDIO_TESTS true
#define RUN_MEMORY_TESTS true
#define RUN_STRESS_TESTS true
```

### 2. Build and Upload

```bash
cd firmware
pio run -t upload
```

### 3. Monitor Results

```bash
pio device monitor
```

### 4. Review Report

The framework generates a comprehensive report including:
- Total tests executed
- Pass/fail rate
- Memory statistics
- Performance metrics
- Failed test details

---

## 📊 Test Groups

### GROUP 1: WiFi Stability Tests (8 tests)

**Purpose:** Validate WiFi connection stability and recovery

| Test | Description | Hardware Only |
|------|-------------|---------------|
| `wifi_connection_stable` | Check WiFi connection status | ✓ |
| `wifi_reconnect_simulation` | Simulate disconnect/reconnect | ✗ |
| `dns_resolution` | Test DNS lookup | ✓ |
| `network_latency` | Check network latency | ✓ |
| `wifi_auto_reconnect_enabled` | Verify auto-reconnect | ✗ |
| `wifi_persistent_enabled` | Check persistent settings | ✗ |
| `multiple_reconnect_cycles` | Test multiple reconnects | ✗ |
| `wifi_signal_monitoring` | Monitor signal strength | ✓ |

**Expected Results:**
- ✅ WiFi auto-reconnect enabled
- ✅ RSSI > -90 dBm (warning if weaker)
- ✅ DNS resolution < 2s
- ✅ Stable heap across reconnects

---

### GROUP 2: MQTT Reliability Tests (10 tests)

**Purpose:** Validate MQTT connection and message handling

| Test | Description | Hardware Only |
|------|-------------|---------------|
| `mqtt_connection_status` | Check MQTT connection | ✗ |
| `mqtt_reconnect_logic` | Test reconnect logic | ✗ |
| `subscription_restoration` | Verify topic resubscription | ✗ |
| `message_processing` | Test message handling | ✗ |
| `duplicate_message_prevention` | Detect duplicates | ✗ |
| `qos_level_verification` | Verify QoS settings | ✗ |
| `keepalive_mechanism` | Test keep-alive | ✗ |
| `large_payload_handling` | Test large payloads | ✗ |
| `topic_validation` | Validate topic format | ✗ |
| `connection_stability_under_load` | Stress test | ✗ |

**Expected Results:**
- ✅ All 7 topics subscribed after reconnect
- ✅ QoS 0 for all messages
- ✅ Keep-alive: 60 seconds
- ✅ Max payload: 768 bytes
- ✅ Stable heap under 100 messages

---

### GROUP 3: Filesystem Safety Tests (10 tests)

**Purpose:** Validate SPIFFS operations and file safety

| Test | Description | Hardware Only |
|------|-------------|---------------|
| `spiffs_mount_status` | Check SPIFFS mount | ✓ |
| `file_write_read` | Test file I/O | ✓ |
| `audio_file_replacement_safety` | Safe file replacement | ✗ |
| `filesystem_full_handling` | Handle full filesystem | ✗ |
| `partial_download_cleanup` | Cleanup on failure | ✗ |
| `file_handle_leak_detection` | Detect file leaks | ✓ |
| `concurrent_file_access` | Handle concurrent access | ✗ |
| `spiffs_corruption_detection` | Check for corruption | ✓ |
| `large_file_write_performance` | Write performance | ✓ |
| `audio_file_existence_check` | Check audio file | ✓ |

**Expected Results:**
- ✅ SPIFFS free space > 100KB
- ✅ Write speed > 10 KB/s
- ✅ No file handle leaks
- ✅ Safe file replacement (temp → rename)
- ✅ Temp file removed on failure

---

### GROUP 4: Audio Playback Tests (12 tests)

**Purpose:** Validate audio pipeline and playback

| Test | Description | Hardware Only |
|------|-------------|---------------|
| `audio_playback_start` | Test playback start | ✗ |
| `audio_playback_stop` | Test playback stop | ✗ |
| `trigger_during_playback` | Restart during play | ✗ |
| `trigger_without_audio` | Handle missing audio | ✗ |
| `rapid_multiple_triggers` | Rapid trigger stress | ✗ |
| `volume_control_range` | Test volume 0-10 | ✗ |
| `volume_change_during_playback` | Change volume live | ✗ |
| `mute_functionality` | Test mute (vol 0) | ✗ |
| `audio_pipeline_integrity` | Verify pipeline | ✗ |
| `clipping_protection` | Test clipping limit | ✗ |
| `playback_loop_stability` | Loop stability | ✗ |
| `audio_file_corruption_detection` | Detect corruption | ✗ |

**Expected Results:**
- ✅ Audio start latency < 300ms
- ✅ Volume range: 0-10
- ✅ Max gain: 0.95 (clipping protection)
- ✅ Clean restart on trigger
- ✅ Stable heap across 10 rapid triggers

---

### GROUP 5: Memory Stability Tests (10 tests)

**Purpose:** Detect memory leaks and fragmentation

| Test | Description | Hardware Only |
|------|-------------|---------------|
| `initial_heap_check` | Check initial heap | ✓ |
| `heap_fragmentation_check` | Check fragmentation | ✓ |
| `memory_leak_1000_cycles` | 1000 cycle leak test | ✗ |
| `stack_usage_check` | Check stack usage | ✓ |
| `dynamic_allocation_stress` | Allocation stress | ✗ |
| `psram_check` | Check for PSRAM | ✓ |
| `heap_stability_over_time` | 60s stability | ✗ |
| `large_buffer_allocation` | 32KB allocation | ✗ |
| `memory_alignment_check` | Check alignment | ✗ |
| `minimum_heap_threshold` | Verify > 120KB | ✓ |

**Expected Results:**
- ✅ Min free heap > 120KB
- ✅ Fragmentation < 30%
- ✅ No leak after 1000 cycles (< 5KB drift)
- ✅ Stable heap over 60 seconds
- ✅ 32KB buffer allocation succeeds

---

### GROUP 6: Stress & Long Duration Tests (10 tests)

**Purpose:** Validate long-term stability and extreme conditions

| Test | Description | Duration | Hardware Only |
|------|-------------|----------|---------------|
| `stability_24h_simulation` | 24h simulation | 5 min | ✗ |
| `continuous_playback_stress` | 100 playback cycles | 1 min | ✗ |
| `network_interruption_recovery` | 10 interruptions | 30s | ✗ |
| `concurrent_operations_stress` | 50 concurrent ops | 1 min | ✗ |
| `power_cycle_simulation` | Power loss scenarios | 30s | ✗ |
| `download_interruption_stress` | 10 interrupted downloads | 1 min | ✗ |
| `watchdog_stress_test` | Watchdog compatibility | 30s | ✗ |
| `rapid_state_transitions` | 10 cycles x 8 states | 30s | ✗ |
| `maximum_uptime_simulation` | 30-day simulation | 2 min | ✗ |
| `extreme_load_test` | 200 operations | 1 min | ✗ |

**Expected Results:**
- ✅ No crashes during 24h simulation
- ✅ Heap stable across all tests
- ✅ No watchdog resets
- ✅ Clean recovery from interruptions
- ✅ Stable across state transitions

---

## 📈 Performance Targets

### Latency Targets

| Metric | Target | Measured By |
|--------|--------|-------------|
| Audio Start Latency | < 300ms | `audio_playback_start` |
| MQTT Reconnect | < 3s | `mqtt_reconnect_logic` |
| WiFi Reconnect | < 5s | `wifi_reconnect_simulation` |
| DNS Lookup | < 2s | `dns_resolution` |

### Memory Targets

| Metric | Target | Measured By |
|--------|--------|-------------|
| Minimum Free Heap | > 120KB | `minimum_heap_threshold` |
| Heap Fragmentation | < 30% | `heap_fragmentation_check` |
| Memory Leak (1000 cycles) | < 5KB drift | `memory_leak_1000_cycles` |

### Stability Targets

| Metric | Target | Measured By |
|--------|--------|-------------|
| 24h Uptime | No crashes | `stability_24h_simulation` |
| Continuous Playback | 100 cycles | `continuous_playback_stress` |
| Network Interruptions | 10 recoveries | `network_interruption_recovery` |

---

## 🔍 Interpreting Results

### Test Results

- **✓ PASS** - Test completed successfully
- **✗ FAIL** - Critical failure, firmware not ready
- **⚠ WARN** - Warning, review recommended
- **○ SKIP** - Test skipped (hardware-only in simulated mode)

### Pass Rate Thresholds

| Pass Rate | Status | Action |
|-----------|--------|--------|
| 100% | ✅ Excellent | Deploy to production |
| 95-99% | ⚠️ Good | Review warnings, deploy with caution |
| 90-94% | ⚠️ Fair | Fix failures before deployment |
| < 90% | ❌ Poor | Do not deploy, fix critical issues |

### Memory Health

| Min Free Heap | Status | Action |
|---------------|--------|--------|
| > 150KB | ✅ Excellent | Safe for deployment |
| 120-150KB | ✅ Good | Monitor in production |
| 100-120KB | ⚠️ Warning | Investigate memory usage |
| < 100KB | ❌ Critical | Do not deploy |

---

## 🛠️ Customizing Tests

### Adding New Tests

1. Create test file in `test/` directory
2. Include `TestFramework.h`
3. Define test group:

```cpp
TEST_GROUP("My Test Group")
```

4. Add test cases:

```cpp
TEST_CASE(my_test_name, false) {  // false = simulated, true = hardware only
    Serial.println("[TEST] My test description...");
    
    // Test logic here
    
    if (condition) {
        return TEST_PASS;
    } else {
        return TEST_FAIL;
    }
}
```

5. Use assertions:

```cpp
ASSERT_TRUE(condition);
ASSERT_FALSE(condition);
ASSERT_EQUAL(expected, actual);
ASSERT_NOT_NULL(pointer);
```

### Configuring Test Groups

Edit `test/main_test.cpp`:

```cpp
#define RUN_WIFI_TESTS true      // Enable/disable
#define RUN_MQTT_TESTS true
#define RUN_FILESYSTEM_TESTS true
#define RUN_AUDIO_TESTS true
#define RUN_MEMORY_TESTS true
#define RUN_STRESS_TESTS true
```

---

## 📝 Sample Test Report

```
╔════════════════════════════════════════════════════════════╗
║                    TEST REPORT                             ║
╚════════════════════════════════════════════════════════════╝

Total Duration: 12.45 seconds
Total Tests:    60
✓ Passed:       58
✗ Failed:       0
⚠ Warnings:     2
○ Skipped:      8

Pass Rate: 96.7%

--- Memory Statistics ---
Current Free Heap: 245000 bytes
Minimum Free Heap: 230000 bytes

--- Performance Metrics ---
Average Test Time: 207 ms
Slowest Test: stability_24h_simulation (5000 ms)

╔════════════════════════════════════════════════════════════╗
║              ✓ ALL TESTS PASSED                            ║
║           FIRMWARE READY FOR DEPLOYMENT                    ║
╚════════════════════════════════════════════════════════════╝
```

---

## 🚨 Troubleshooting

### Common Issues

**Issue:** Tests fail in hardware mode but pass in simulated mode
- **Cause:** WiFi not connected or MQTT broker unreachable
- **Solution:** Ensure WiFi credentials configured and broker accessible

**Issue:** Memory tests show warnings
- **Cause:** Heap fragmentation or memory leak
- **Solution:** Review dynamic allocations, check for unclosed file handles

**Issue:** Audio tests fail
- **Cause:** No audio file present or SPIFFS not mounted
- **Solution:** Upload test audio file or run filesystem tests first

**Issue:** Watchdog resets during tests
- **Cause:** Test blocking too long without yield()
- **Solution:** Add yield() calls in long-running tests

---

## 📚 Best Practices

### Before Production Deployment

1. ✅ Run **full test suite** in simulated mode
2. ✅ Run **full test suite** in hardware mode
3. ✅ Verify **pass rate > 95%**
4. ✅ Check **min heap > 120KB**
5. ✅ Review all **warnings**
6. ✅ Run **24h stability test** on actual hardware
7. ✅ Document any **skipped tests** and why

### During Development

- Run **simulated tests** frequently
- Run **hardware tests** before commits
- Add **new tests** for new features
- Update **test documentation**

### Continuous Integration

- Automate **simulated tests** in CI/CD
- Schedule **nightly hardware tests**
- Track **pass rate trends**
- Alert on **memory regressions**

---

## 🔗 Integration with Firmware

### Test vs Production Code

The test framework is **separate** from production firmware:

```
firmware/
├── src/              # Production firmware
│   ├── main.cpp
│   └── core/
└── test/             # Test framework (separate)
    ├── main_test.cpp
    └── *Tests.cpp
```

### Running Tests

**Option 1:** Dedicated test build
```bash
# Build and upload test firmware
pio run -e test -t upload
```

**Option 2:** Manual switch
- Comment out production `main.cpp`
- Uncomment test `main_test.cpp`
- Build and upload

---

## 📊 Test Metrics Summary

| Category | Tests | Hardware Only | Avg Duration |
|----------|-------|---------------|--------------|
| WiFi Stability | 8 | 4 | 150ms |
| MQTT Reliability | 10 | 0 | 100ms |
| Filesystem Safety | 10 | 6 | 200ms |
| Audio Playback | 12 | 0 | 80ms |
| Memory Stability | 10 | 5 | 5000ms |
| Stress & Long Duration | 10 | 0 | 30000ms |
| **TOTAL** | **60** | **15** | **~12min** |

---

## ✅ Deployment Checklist

- [ ] All tests pass (100% or > 95%)
- [ ] No critical failures
- [ ] Warnings reviewed and documented
- [ ] Min heap > 120KB
- [ ] Audio start latency < 300ms
- [ ] No memory leaks detected
- [ ] 24h stability test passed
- [ ] Hardware tests completed
- [ ] Test report saved
- [ ] Firmware version tagged

---

## 📞 Support

For issues or questions about the test framework:

1. Review this documentation
2. Check test output logs
3. Verify hardware connections (hardware mode)
4. Review firmware audit report
5. Check SPIFFS space and WiFi connection

---

**Framework Version:** 1.0  
**Last Updated:** March 5, 2026  
**Maintainer:** Embedded Systems Test Engineer  
**Status:** Production Ready ✅
