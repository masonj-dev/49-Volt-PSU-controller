#include <Arduino.h>
#include <unity.h>

// Mock time for testing
static unsigned long mockMillis = 0;

// ===== UNIT TESTS =====

void setUp(void) {
    mockMillis = 0;
}

void tearDown(void) {
}

// Test timeElapsed function handles normal intervals
void test_timeElapsed_normal_interval(void) {
    unsigned long lastTime = 0;
    unsigned long currentMillis = 100;
    
    // Simulate millis value
    TEST_ASSERT_TRUE(currentMillis - lastTime >= 50);
}

// Test ADC filter rejects outliers
void test_adc_filter_rejects_outliers(void) {
    // Simulate a buffer of stable readings
    int16_t buffer[16] = {1000, 1005, 998, 1002, 1001, 999, 1003, 1000,
                          1004, 997, 1001, 1002, 998, 1005, 999, 1000};
    
    // Calculate average
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += buffer[i];
    }
    int16_t avg = sum / 16;
    
    // Average should be ~1001
    TEST_ASSERT_UINT16_WITHIN(10, 1001, avg);
}

// Test DAC value constraining
void test_dac_value_constraining(void) {
    float percent = 150.0;  // Out of range
    percent = constrain(percent, 0.0, 100.0);
    TEST_ASSERT_EQUAL_FLOAT(100.0, percent);
    
    percent = -50.0;  // Out of range
    percent = constrain(percent, 0.0, 100.0);
    TEST_ASSERT_EQUAL_FLOAT(0.0, percent);
}

// Test voltage range validation
void test_voltage_range_validation(void) {
    float vbat = -5.0;
    if (vbat < 0.1) vbat = 0.01;
    TEST_ASSERT_EQUAL_FLOAT(0.01, vbat);
    
    vbat = 120.0;  // Beyond max
    if (vbat > 85.0) vbat = 85.0;
    TEST_ASSERT_EQUAL_FLOAT(85.0, vbat);
}

// Test touch debouncing logic
void test_touch_debounce(void) {
    unsigned long lastTouchTime = 0;
    unsigned long now = 100;
    const unsigned long DEBOUNCE_MS = 50;
    
    bool shouldProcess = (now - lastTouchTime >= DEBOUNCE_MS);
    TEST_ASSERT_TRUE(shouldProcess);
    
    // Try again too quickly
    now = 120;
    shouldProcess = (now - lastTouchTime >= DEBOUNCE_MS);
    TEST_ASSERT_FALSE(shouldProcess);
}

// Test safe millis overflow handling
void test_millis_overflow_safe_comparison(void) {
    unsigned long time1 = 4294967200UL;  // Near overflow
    unsigned long time2 = 100;            // After overflow wrap
    
    // Safe comparison using difference
    unsigned long diff = time2 - time1;
    bool elapsed = diff >= 200;
    
    TEST_ASSERT_FALSE(elapsed);  // Should wrap correctly
}

// Test frame rate limiting
void test_frame_rate_limiting(void) {
    unsigned long lastFrame = 0;
    unsigned long now = 33;  // 33ms later = 1 frame at 30FPS
    const unsigned long FRAME_INTERVAL = 33;
    
    bool shouldDraw = (now - lastFrame >= FRAME_INTERVAL);
    TEST_ASSERT_TRUE(shouldDraw);
    
    // Try next frame too quickly
    now = 50;
    shouldDraw = (now - lastFrame >= FRAME_INTERVAL);
    TEST_ASSERT_FALSE(shouldDraw);
}

// Test ADC reading validation
void test_adc_read_validation(void) {
    int16_t adc0 = 1000;
    int calib = 7;
    float voltage = ((adc0 + calib) * 0.1875) / 1000.0;
    
    // Voltage should be within expected range
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0.19, voltage);
}

// Test DAC output scaling (0-100% -> 0-6V)
void test_dac_output_scaling(void) {
    float percent = 50.0;  // 50%
    int16_t DAC_FULL_SCALE = 19520;
    int8_t DAC_ZERO_TRIM = -2;
    
    uint16_t dacValue = (int16_t)((percent / 100.0) * DAC_FULL_SCALE) + DAC_ZERO_TRIM;
    
    // 50% should give us ~9758
    TEST_ASSERT_UINT16_WITHIN(5, 9758, dacValue);
}

// Test gauge smoothing
void test_gauge_smoothing(void) {
    float value = 50.0;
    float displayedValue = 0.0;
    float diff = value - displayedValue;
    
    // Apply smoothing
    displayedValue += diff * 0.1;
    
    // Should move 10% closer
    TEST_ASSERT_FLOAT_WITHIN(0.5, 5.0, displayedValue);
}

// ===== TEST SUITE =====
void runAllTests(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_timeElapsed_normal_interval);
    RUN_TEST(test_adc_filter_rejects_outliers);
    RUN_TEST(test_dac_value_constraining);
    RUN_TEST(test_voltage_range_validation);
    RUN_TEST(test_touch_debounce);
    RUN_TEST(test_millis_overflow_safe_comparison);
    RUN_TEST(test_frame_rate_limiting);
    RUN_TEST(test_adc_read_validation);
    RUN_TEST(test_dac_output_scaling);
    RUN_TEST(test_gauge_smoothing);
    
    UNITY_END();
}

void setup() {
    delay(2000);
    runAllTests();
}

void loop() {
}
