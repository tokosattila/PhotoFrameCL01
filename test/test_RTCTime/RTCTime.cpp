/**
 * @file RTCTime.cpp
 * @brief Unit tests for RTC time functions (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <ctime>

// ============================================================================
// Standalone implementations for testing (extracted from RTCTime.cpp)
// ============================================================================

static const unsigned long SECONDS_PER_MINUTE = 60;
static const unsigned long SECONDS_PER_HOUR = 3600;
static const unsigned long SECONDS_PER_DAY = 86400;
static const unsigned long MIN_VALID_EPOCH = 1735689600UL; // 2025-01-01 00:00:00 UTC

struct SRTCDateTime {
  uint16_t Year;
  uint8_t Month;
  uint8_t Day;
  uint8_t Hour;
  uint8_t Minute;
  uint8_t Second;
  uint8_t DayOfWeek;
};

uint8_t BcdToDec(uint8_t tBcd) {
  return ((tBcd >> 4) * 10) + (tBcd & 0x0F);
}

uint8_t DecToBcd(uint8_t tDec) {
  return ((tDec / 10) << 4) | (tDec % 10);
}

bool IsLeapYear(uint16_t tYear) {
  return (tYear % 4 == 0) && (tYear % 100 != 0 || tYear % 400 == 0);
}

uint8_t GetDaysInMonth(uint8_t tMonth, uint16_t tYear) {
  static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (tMonth < 1 || tMonth > 12) return 0;
  if (tMonth == 2 && IsLeapYear(tYear)) return 29;
  return kDaysInMonth[tMonth - 1];
}

unsigned long DateTimeToEpoch(const SRTCDateTime &tDateTime) {
  unsigned long tEpoch = 0;
  for (uint16_t y = 1970; y < tDateTime.Year; y++) {
    tEpoch += IsLeapYear(y) ? 366UL * SECONDS_PER_DAY : 365UL * SECONDS_PER_DAY;
  }
  for (uint8_t m = 1; m < tDateTime.Month; m++) {
    tEpoch += GetDaysInMonth(m, tDateTime.Year) * SECONDS_PER_DAY;
  }
  tEpoch += (tDateTime.Day - 1) * SECONDS_PER_DAY;
  tEpoch += tDateTime.Hour * SECONDS_PER_HOUR;
  tEpoch += tDateTime.Minute * SECONDS_PER_MINUTE;
  tEpoch += tDateTime.Second;
  return tEpoch;
}

bool IsValidDateTime(const SRTCDateTime &tDateTime) {
  if (tDateTime.Year < 2026 || tDateTime.Year > 2099) return false;
  if (tDateTime.Month < 1 || tDateTime.Month > 12) return false;
  uint8_t tMaxDay = GetDaysInMonth(tDateTime.Month, tDateTime.Year);
  if (tDateTime.Day < 1 || tDateTime.Day > tMaxDay) return false;
  if (tDateTime.Hour > 23) return false;
  if (tDateTime.Minute > 59) return false;
  if (tDateTime.Second > 59) return false;
  return true;
}

bool IsValidEpoch(unsigned long tEpoch) {
  return tEpoch >= MIN_VALID_EPOCH;
}

void FormatDate(const SRTCDateTime &tDateTime, char *tBuffer, size_t tLength) {
  snprintf(tBuffer, tLength, "%04d.%02d.%02d", tDateTime.Year, tDateTime.Month, tDateTime.Day);
}

void FormatTime(const SRTCDateTime &tDateTime, char *tBuffer, size_t tLength) {
  snprintf(tBuffer, tLength, "%02d:%02d:%02d", tDateTime.Hour, tDateTime.Minute, tDateTime.Second);
}

// ============================================================================
// BCD Conversion Tests
// ============================================================================

void test_BcdToDec_zero() {
  TEST_ASSERT_EQUAL_UINT8(0, BcdToDec(0x00));
}

void test_BcdToDec_single_digit() {
  TEST_ASSERT_EQUAL_UINT8(5, BcdToDec(0x05));
  TEST_ASSERT_EQUAL_UINT8(9, BcdToDec(0x09));
}

void test_BcdToDec_double_digit() {
  TEST_ASSERT_EQUAL_UINT8(10, BcdToDec(0x10));
  TEST_ASSERT_EQUAL_UINT8(25, BcdToDec(0x25));
  TEST_ASSERT_EQUAL_UINT8(59, BcdToDec(0x59));
  TEST_ASSERT_EQUAL_UINT8(99, BcdToDec(0x99));
}

void test_DecToBcd_zero() {
  TEST_ASSERT_EQUAL_UINT8(0x00, DecToBcd(0));
}

void test_DecToBcd_single_digit() {
  TEST_ASSERT_EQUAL_UINT8(0x05, DecToBcd(5));
  TEST_ASSERT_EQUAL_UINT8(0x09, DecToBcd(9));
}

void test_DecToBcd_double_digit() {
  TEST_ASSERT_EQUAL_UINT8(0x10, DecToBcd(10));
  TEST_ASSERT_EQUAL_UINT8(0x25, DecToBcd(25));
  TEST_ASSERT_EQUAL_UINT8(0x59, DecToBcd(59));
  TEST_ASSERT_EQUAL_UINT8(0x99, DecToBcd(99));
}

void test_BcdDec_roundtrip() {
  for (uint8_t i = 0; i <= 99; i++) {
    TEST_ASSERT_EQUAL_UINT8(i, BcdToDec(DecToBcd(i)));
  }
}

// ============================================================================
// GetDaysInMonth Tests
// ============================================================================

void test_GetDaysInMonth_january() {
  TEST_ASSERT_EQUAL_UINT8(31, GetDaysInMonth(1, 2024));
  TEST_ASSERT_EQUAL_UINT8(31, GetDaysInMonth(1, 2025));
}

void test_GetDaysInMonth_february_normal() {
  TEST_ASSERT_EQUAL_UINT8(28, GetDaysInMonth(2, 2023));
  TEST_ASSERT_EQUAL_UINT8(28, GetDaysInMonth(2, 2025));
}

void test_GetDaysInMonth_february_leap() {
  TEST_ASSERT_EQUAL_UINT8(29, GetDaysInMonth(2, 2024));
  TEST_ASSERT_EQUAL_UINT8(29, GetDaysInMonth(2, 2028));
}

void test_GetDaysInMonth_april() {
  TEST_ASSERT_EQUAL_UINT8(30, GetDaysInMonth(4, 2024));
}

void test_GetDaysInMonth_december() {
  TEST_ASSERT_EQUAL_UINT8(31, GetDaysInMonth(12, 2024));
}

void test_GetDaysInMonth_invalid() {
  TEST_ASSERT_EQUAL_UINT8(0, GetDaysInMonth(0, 2024));
  TEST_ASSERT_EQUAL_UINT8(0, GetDaysInMonth(13, 2024));
}

// ============================================================================
// DateTimeToEpoch Tests
// ============================================================================

void test_DateTimeToEpoch_unix_epoch() {
  SRTCDateTime dt = {1970, 1, 1, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(0, DateTimeToEpoch(dt));
}

void test_DateTimeToEpoch_known_date() {
  // 2024-01-01 00:00:00 UTC = 1704067200
  SRTCDateTime dt = {2024, 1, 1, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(1704067200, DateTimeToEpoch(dt));
}

void test_DateTimeToEpoch_with_time() {
  // 2024-01-01 12:30:45 UTC = 1704067200 + 12*3600 + 30*60 + 45 = 1704112245
  SRTCDateTime dt = {2024, 1, 1, 12, 30, 45, 0};
  TEST_ASSERT_EQUAL_UINT32(1704112245, DateTimeToEpoch(dt));
}

void test_DateTimeToEpoch_leap_year() {
  // 2024-02-29 00:00:00 UTC = 1709164800
  SRTCDateTime dt = {2024, 2, 29, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(1709164800, DateTimeToEpoch(dt));
}

void test_DateTimeToEpoch_2026() {
  // 2026-01-01 00:00:00 UTC = 1767225600
  SRTCDateTime dt = {2026, 1, 1, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(1767225600, DateTimeToEpoch(dt));
}

// ============================================================================
// IsValidDateTime Tests
// ============================================================================

void test_IsValidDateTime_valid() {
  SRTCDateTime dt = {2026, 6, 15, 12, 30, 45, 0};
  TEST_ASSERT_TRUE(IsValidDateTime(dt));
}

void test_IsValidDateTime_year_too_low() {
  SRTCDateTime dt = {2020, 6, 15, 12, 30, 45, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

void test_IsValidDateTime_year_too_high() {
  SRTCDateTime dt = {2100, 6, 15, 12, 30, 45, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

void test_IsValidDateTime_invalid_month() {
  SRTCDateTime dt1 = {2026, 0, 15, 12, 30, 45, 0};
  SRTCDateTime dt2 = {2026, 13, 15, 12, 30, 45, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt1));
  TEST_ASSERT_FALSE(IsValidDateTime(dt2));
}

void test_IsValidDateTime_invalid_day() {
  SRTCDateTime dt1 = {2026, 6, 0, 12, 30, 45, 0};
  SRTCDateTime dt2 = {2026, 6, 31, 12, 30, 45, 0}; // June has 30 days
  TEST_ASSERT_FALSE(IsValidDateTime(dt1));
  TEST_ASSERT_FALSE(IsValidDateTime(dt2));
}

void test_IsValidDateTime_feb29_leap_year() {
  SRTCDateTime dt = {2028, 2, 29, 0, 0, 0, 0};
  TEST_ASSERT_TRUE(IsValidDateTime(dt));
}

void test_IsValidDateTime_feb29_non_leap() {
  SRTCDateTime dt = {2027, 2, 29, 0, 0, 0, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

void test_IsValidDateTime_invalid_hour() {
  SRTCDateTime dt = {2026, 6, 15, 24, 0, 0, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

void test_IsValidDateTime_invalid_minute() {
  SRTCDateTime dt = {2026, 6, 15, 12, 60, 0, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

void test_IsValidDateTime_invalid_second() {
  SRTCDateTime dt = {2026, 6, 15, 12, 30, 60, 0};
  TEST_ASSERT_FALSE(IsValidDateTime(dt));
}

// ============================================================================
// IsValidEpoch Tests
// ============================================================================

void test_IsValidEpoch_valid() {
  TEST_ASSERT_TRUE(IsValidEpoch(1767225600)); // 2026-01-01
  TEST_ASSERT_TRUE(IsValidEpoch(MIN_VALID_EPOCH));
}

void test_IsValidEpoch_too_old() {
  TEST_ASSERT_FALSE(IsValidEpoch(0));
  TEST_ASSERT_FALSE(IsValidEpoch(1704067200)); // 2024-01-01
  TEST_ASSERT_FALSE(IsValidEpoch(MIN_VALID_EPOCH - 1));
}

// ============================================================================
// Format Tests
// ============================================================================

void test_FormatDate() {
  SRTCDateTime dt = {2026, 1, 12, 15, 30, 45, 0};
  char buffer[16];
  FormatDate(dt, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2026.01.12", buffer);
}

void test_FormatTime() {
  SRTCDateTime dt = {2026, 1, 12, 15, 30, 45, 0};
  char buffer[16];
  FormatTime(dt, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("15:30:45", buffer);
}

void test_FormatDate_single_digits() {
  SRTCDateTime dt = {2026, 3, 5, 8, 5, 3, 0};
  char buffer[16];
  FormatDate(dt, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2026.03.05", buffer);
}

void test_FormatTime_midnight() {
  SRTCDateTime dt = {2026, 1, 1, 0, 0, 0, 0};
  char buffer[16];
  FormatTime(dt, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("00:00:00", buffer);
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // BCD Conversion tests
  RUN_TEST(test_BcdToDec_zero);
  RUN_TEST(test_BcdToDec_single_digit);
  RUN_TEST(test_BcdToDec_double_digit);
  RUN_TEST(test_DecToBcd_zero);
  RUN_TEST(test_DecToBcd_single_digit);
  RUN_TEST(test_DecToBcd_double_digit);
  RUN_TEST(test_BcdDec_roundtrip);
  
  // GetDaysInMonth tests
  RUN_TEST(test_GetDaysInMonth_january);
  RUN_TEST(test_GetDaysInMonth_february_normal);
  RUN_TEST(test_GetDaysInMonth_february_leap);
  RUN_TEST(test_GetDaysInMonth_april);
  RUN_TEST(test_GetDaysInMonth_december);
  RUN_TEST(test_GetDaysInMonth_invalid);
  
  // DateTimeToEpoch tests
  RUN_TEST(test_DateTimeToEpoch_unix_epoch);
  RUN_TEST(test_DateTimeToEpoch_known_date);
  RUN_TEST(test_DateTimeToEpoch_with_time);
  RUN_TEST(test_DateTimeToEpoch_leap_year);
  RUN_TEST(test_DateTimeToEpoch_2026);
  
  // IsValidDateTime tests
  RUN_TEST(test_IsValidDateTime_valid);
  RUN_TEST(test_IsValidDateTime_year_too_low);
  RUN_TEST(test_IsValidDateTime_year_too_high);
  RUN_TEST(test_IsValidDateTime_invalid_month);
  RUN_TEST(test_IsValidDateTime_invalid_day);
  RUN_TEST(test_IsValidDateTime_feb29_leap_year);
  RUN_TEST(test_IsValidDateTime_feb29_non_leap);
  RUN_TEST(test_IsValidDateTime_invalid_hour);
  RUN_TEST(test_IsValidDateTime_invalid_minute);
  RUN_TEST(test_IsValidDateTime_invalid_second);
  
  // IsValidEpoch tests
  RUN_TEST(test_IsValidEpoch_valid);
  RUN_TEST(test_IsValidEpoch_too_old);
  
  // Format tests
  RUN_TEST(test_FormatDate);
  RUN_TEST(test_FormatTime);
  RUN_TEST(test_FormatDate_single_digits);
  RUN_TEST(test_FormatTime_midnight);
  
  return UNITY_END();
}
