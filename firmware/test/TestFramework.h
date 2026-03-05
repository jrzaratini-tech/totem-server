#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <Arduino.h>
#include <vector>
#include <functional>

enum TestResult {
    TEST_PASS,
    TEST_FAIL,
    TEST_WARN,
    TEST_SKIP
};

struct TestMetrics {
    unsigned long startMs;
    unsigned long endMs;
    size_t heapBefore;
    size_t heapAfter;
    size_t minHeap;
};

struct TestCase {
    String name;
    String group;
    std::function<TestResult()> testFunc;
    TestResult result;
    String failureReason;
    TestMetrics metrics;
    bool hardwareOnly;
};

class TestFramework {
private:
    std::vector<TestCase> tests;
    int totalTests;
    int passedTests;
    int failedTests;
    int warnTests;
    int skippedTests;
    
    bool hardwareMode;
    unsigned long testStartTime;
    
    void logTestStart(const String &name);
    void logTestEnd(const String &name, TestResult result, unsigned long duration);
    void collectMetrics(TestCase &test);
    
public:
    TestFramework(bool hwMode = false);
    
    void addTest(const String &group, const String &name, std::function<TestResult()> func, bool hwOnly = false);
    void runAllTests();
    void runGroup(const String &group);
    void generateReport();
    
    void setHardwareMode(bool enabled) { hardwareMode = enabled; }
    bool isHardwareMode() const { return hardwareMode; }
    
    // Helper assertion functions
    bool assertTrue(bool condition, const String &msg = "");
    bool assertFalse(bool condition, const String &msg = "");
    bool assertEqual(int expected, int actual, const String &msg = "");
    bool assertNotNull(void* ptr, const String &msg = "");
    bool assertHeapStable(size_t before, size_t after, int toleranceBytes = 1024);
};

// Global test framework instance
extern TestFramework testFw;

// Test macros
#define TEST_GROUP(group) \
    static const char* currentTestGroup = group;

#define TEST_CASE(name, hwOnly) \
    TestResult test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            testFw.addTest(currentTestGroup, #name, test_##name, hwOnly); \
        } \
    }; \
    static TestRegistrar_##name registrar_##name; \
    TestResult test_##name()

#define ASSERT_TRUE(cond) if (!testFw.assertTrue(cond, #cond)) return TEST_FAIL
#define ASSERT_FALSE(cond) if (!testFw.assertFalse(cond, #cond)) return TEST_FAIL
#define ASSERT_EQUAL(exp, act) if (!testFw.assertEqual(exp, act, #exp " == " #act)) return TEST_FAIL
#define ASSERT_NOT_NULL(ptr) if (!testFw.assertNotNull(ptr, #ptr)) return TEST_FAIL

#endif
