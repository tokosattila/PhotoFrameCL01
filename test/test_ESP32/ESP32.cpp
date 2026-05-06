#include <unity.h>
#include <Arduino.h>
#include <WiFi.h>

void test_chip_model() {
  const char *model = ESP.getChipModel();
  TEST_ASSERT_NOT_NULL(model);
  TEST_ASSERT_TRUE(strstr(model, "ESP32") != nullptr);
}

void test_chip_cores() {
  uint8_t cores = ESP.getChipCores();
  TEST_ASSERT_EQUAL_UINT8(2, cores);
}

void test_cpu_frequency() {
  uint32_t freq = ESP.getCpuFreqMHz();
  TEST_ASSERT_TRUE(freq >= 80 && freq <= 240);
}

void test_psram_available() {
  size_t psram = ESP.getPsramSize();
  TEST_ASSERT_TRUE(psram > 0);
}

void test_flash_size() {
  uint32_t flash = ESP.getFlashChipSize();
  TEST_ASSERT_TRUE(flash >= 4 * 1024 * 1024);
}

void test_heap_available() {
  size_t freeHeap = ESP.getFreeHeap();
  TEST_ASSERT_TRUE(freeHeap > 100000);
}

void test_millis_running() {
  unsigned long start = millis();
  delay(10);
  unsigned long end = millis();
  TEST_ASSERT_TRUE(end > start);
}

void test_gpio_read() {
  pinMode(21, INPUT);
  int value = digitalRead(21);
  TEST_ASSERT_TRUE(value == HIGH || value == LOW);
}

void test_temperature_sensor() {
  float temp = temperatureRead();
  TEST_ASSERT_TRUE(temp > -40.0f && temp < 125.0f);
}

void setUp(void) {}

void tearDown(void) {}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_chip_model);
  RUN_TEST(test_chip_cores);
  RUN_TEST(test_cpu_frequency);
  RUN_TEST(test_psram_available);
  RUN_TEST(test_flash_size);
  RUN_TEST(test_heap_available);
  RUN_TEST(test_millis_running);
  RUN_TEST(test_gpio_read);
  RUN_TEST(test_temperature_sensor);
  UNITY_END();
}

void loop() {
}
