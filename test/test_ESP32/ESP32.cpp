/**
 * @file ESP32.cpp
 * @brief Hardware tests for ESP32-S3 (runs on actual hardware)
 * 
 * Run with: pio test -e esp32_test
 */

#include <unity.h>
#include <Arduino.h>
#include <WiFi.h>

// ============================================================================
// Basic ESP32 Hardware Tests
// ============================================================================

void test_chip_model() {
  const char *model = ESP.getChipModel();
  TEST_ASSERT_NOT_NULL(model);
  TEST_ASSERT_TRUE(strstr(model, "ESP32") != nullptr);
}

void test_chip_cores() {
  uint8_t cores = ESP.getChipCores();
  TEST_ASSERT_EQUAL_UINT8(2, cores);  // ESP32-S3 has 2 cores
}

void test_cpu_frequency() {
  uint32_t freq = ESP.getCpuFreqMHz();
  TEST_ASSERT_TRUE(freq >= 80 && freq <= 240);
}

void test_psram_available() {
  size_t psram = ESP.getPsramSize();
  TEST_ASSERT_TRUE(psram > 0);  // Should have PSRAM
}

void test_flash_size() {
  uint32_t flash = ESP.getFlashChipSize();
  TEST_ASSERT_TRUE(flash >= 4 * 1024 * 1024);  // At least 4MB
}

void test_heap_available() {
  size_t freeHeap = ESP.getFreeHeap();
  TEST_ASSERT_TRUE(freeHeap > 100000);  // At least 100KB free
}

void test_millis_running() {
  unsigned long start = millis();
  delay(10);
  unsigned long end = millis();
  TEST_ASSERT_TRUE(end > start);
}

// ============================================================================
// GPIO Test (Button pin)
// ============================================================================

void test_gpio_read() {
  // GPIO21 is the button pin on LilyGo T5 4.7"
  pinMode(21, INPUT);
  int value = digitalRead(21);
  TEST_ASSERT_TRUE(value == HIGH || value == LOW);
}

// ============================================================================
// Temperature Sensor
// ============================================================================

void test_temperature_sensor() {
  float temp = temperatureRead();
  TEST_ASSERT_TRUE(temp > -40.0f && temp < 125.0f);  // Valid range
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

void setup() {
  delay(2000);  // Wait for serial monitor
  
  UNITY_BEGIN();
  
  // Chip info tests
  RUN_TEST(test_chip_model);
  RUN_TEST(test_chip_cores);
  RUN_TEST(test_cpu_frequency);
  RUN_TEST(test_psram_available);
  RUN_TEST(test_flash_size);
  RUN_TEST(test_heap_available);
  
  // Runtime tests
  RUN_TEST(test_millis_running);
  RUN_TEST(test_gpio_read);
  RUN_TEST(test_temperature_sensor);
  
  UNITY_END();
}

void loop() {
  // Nothing here
}
