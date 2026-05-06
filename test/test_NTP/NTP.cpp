#include <unity.h>
#include <cstring>
#include <cstdint>
#include <ctime>

void FormatTwoDigits(char *tBuffer, int tValue) {
  if (tValue < 0) tValue = 0;
  if (tValue > 99) tValue = 99;
  tBuffer[0] = '0' + tValue / 10;
  tBuffer[1] = '0' + tValue % 10;
}

bool IsLeapYear(unsigned long tYear) {
  return (tYear % 4 == 0) && (tYear % 100 != 0 || tYear % 400 == 0);
}

bool IsPacketValid(uint8_t *tPacket) {
  if ((tPacket[0] & 0b11000000) == 0b11000000) return false;
  if (((tPacket[0] & 0b00111000) >> 3) < 3) return false;
  if ((tPacket[0] & 0b00000111) != 4) return false;
  if (tPacket[1] < 1 || tPacket[1] > 15) return false;
  for (uint8_t i = 16; i <= 23; i++) if (tPacket[i] != 0) return true;
  return true;
}

static const unsigned long SECONDS_PER_MINUTE = 60;
static const unsigned long SECONDS_PER_HOUR = 3600;
static const unsigned long SECONDS_PER_DAY = 86400;
static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void GetDateFromEpoch(unsigned long tEpoch, int &tYear, int &tMonth, int &tDay) {
  unsigned long tDays = tEpoch / SECONDS_PER_DAY;
  tYear = 1970;
  while (tDays >= (IsLeapYear(tYear) ? 366UL : 365UL)) {
    tDays -= IsLeapYear(tYear) ? 366UL : 365UL;
    tYear++;
  }

  tMonth = 0;
  while (tDays >= (tMonth == 1 && IsLeapYear(tYear) ? 29U : kDaysInMonth[tMonth])) {
    tDays -= (tMonth == 1 && IsLeapYear(tYear) ? 29U : kDaysInMonth[tMonth]);
    tMonth++;
  }

  tDay = tDays + 1;
  tMonth++;
}

void GetTimeFromEpoch(unsigned long tEpoch, int &tHours, int &tMinutes, int &tSeconds) {
  tHours = (tEpoch % SECONDS_PER_DAY) / SECONDS_PER_HOUR;
  tMinutes = (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
  tSeconds = tEpoch % SECONDS_PER_MINUTE;
}

unsigned long ApplyConfiguredOffset(unsigned long tEpoch, long tGmtOffset, long tDaylightOffset) {
  long tLocalEpoch = static_cast<long>(tEpoch) + tGmtOffset + tDaylightOffset;
  return tLocalEpoch > 0 ? static_cast<unsigned long>(tLocalEpoch) : 0UL;
}

void test_FormatTwoDigits_zero() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, 0);
  TEST_ASSERT_EQUAL_STRING("00", buffer);
}

void test_FormatTwoDigits_single_digit() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, 5);
  TEST_ASSERT_EQUAL_STRING("05", buffer);
}

void test_FormatTwoDigits_double_digit() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, 42);
  TEST_ASSERT_EQUAL_STRING("42", buffer);
}

void test_FormatTwoDigits_max() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, 99);
  TEST_ASSERT_EQUAL_STRING("99", buffer);
}

void test_FormatTwoDigits_negative_clamped() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, -5);
  TEST_ASSERT_EQUAL_STRING("00", buffer);
}

void test_FormatTwoDigits_over99_clamped() {
  char buffer[3] = {0};
  FormatTwoDigits(buffer, 150);
  TEST_ASSERT_EQUAL_STRING("99", buffer);
}

void test_IsLeapYear_regular_year() {
  TEST_ASSERT_FALSE(IsLeapYear(2023));
  TEST_ASSERT_FALSE(IsLeapYear(2019));
  TEST_ASSERT_FALSE(IsLeapYear(1999));
}

void test_IsLeapYear_divisible_by_4() {
  TEST_ASSERT_TRUE(IsLeapYear(2024));
  TEST_ASSERT_TRUE(IsLeapYear(2020));
  TEST_ASSERT_TRUE(IsLeapYear(2016));
}

void test_IsLeapYear_century_not_leap() {
  TEST_ASSERT_FALSE(IsLeapYear(1900));
  TEST_ASSERT_FALSE(IsLeapYear(2100));
  TEST_ASSERT_FALSE(IsLeapYear(2200));
}

void test_IsLeapYear_400_year_leap() {
  TEST_ASSERT_TRUE(IsLeapYear(2000));
  TEST_ASSERT_TRUE(IsLeapYear(1600));
  TEST_ASSERT_TRUE(IsLeapYear(2400));
}

void test_IsLeapYear_edge_cases() {
  TEST_ASSERT_FALSE(IsLeapYear(1970));
  TEST_ASSERT_TRUE(IsLeapYear(1972));
}

void test_IsPacketValid_valid_packet() {
  uint8_t packet[48] = {0};
  packet[0] = 0x24;
  packet[1] = 3;
  packet[16] = 0x01;
  TEST_ASSERT_TRUE(IsPacketValid(packet));
}

void test_IsPacketValid_invalid_li() {
  uint8_t packet[48] = {0};
  packet[0] = 0xE4;
  packet[1] = 3;
  packet[16] = 0x01;
  TEST_ASSERT_FALSE(IsPacketValid(packet));
}

void test_IsPacketValid_invalid_version() {
  uint8_t packet[48] = {0};
  packet[0] = 0x14;
  packet[1] = 3;
  packet[16] = 0x01;
  TEST_ASSERT_FALSE(IsPacketValid(packet));
}

void test_IsPacketValid_invalid_mode() {
  uint8_t packet[48] = {0};
  packet[0] = 0x23;
  packet[1] = 3;
  packet[16] = 0x01;
  TEST_ASSERT_FALSE(IsPacketValid(packet));
}

void test_IsPacketValid_invalid_stratum_zero() {
  uint8_t packet[48] = {0};
  packet[0] = 0x24;
  packet[1] = 0;
  packet[16] = 0x01;
  TEST_ASSERT_FALSE(IsPacketValid(packet));
}

void test_IsPacketValid_invalid_stratum_high() {
  uint8_t packet[48] = {0};
  packet[0] = 0x24;
  packet[1] = 16;
  packet[16] = 0x01;
  TEST_ASSERT_FALSE(IsPacketValid(packet));
}

void test_IsPacketValid_all_zeros_timestamp() {
  uint8_t packet[48] = {0};
  packet[0] = 0x24;
  packet[1] = 3;
  TEST_ASSERT_TRUE(IsPacketValid(packet));
}

void test_GetDateFromEpoch_unix_epoch() {
  int year, month, day;
  GetDateFromEpoch(0, year, month, day);
  TEST_ASSERT_EQUAL_INT(1970, year);
  TEST_ASSERT_EQUAL_INT(1, month);
  TEST_ASSERT_EQUAL_INT(1, day);
}

void test_GetDateFromEpoch_known_date() {
  int year, month, day;
  GetDateFromEpoch(1704240000, year, month, day);
  TEST_ASSERT_EQUAL_INT(2024, year);
  TEST_ASSERT_EQUAL_INT(1, month);
  TEST_ASSERT_EQUAL_INT(3, day);
}

void test_GetDateFromEpoch_leap_year_feb29() {
  int year, month, day;
  GetDateFromEpoch(1709208000, year, month, day);
  TEST_ASSERT_EQUAL_INT(2024, year);
  TEST_ASSERT_EQUAL_INT(2, month);
  TEST_ASSERT_EQUAL_INT(29, day);
}

void test_GetDateFromEpoch_year_end() {
  int year, month, day;
  GetDateFromEpoch(1704067199, year, month, day);
  TEST_ASSERT_EQUAL_INT(2023, year);
  TEST_ASSERT_EQUAL_INT(12, month);
  TEST_ASSERT_EQUAL_INT(31, day);
}

void test_GetTimeFromEpoch_midnight() {
  int hours, minutes, seconds;
  GetTimeFromEpoch(0, hours, minutes, seconds);
  TEST_ASSERT_EQUAL_INT(0, hours);
  TEST_ASSERT_EQUAL_INT(0, minutes);
  TEST_ASSERT_EQUAL_INT(0, seconds);
}

void test_GetTimeFromEpoch_noon() {
  int hours, minutes, seconds;
  GetTimeFromEpoch(43200, hours, minutes, seconds);
  TEST_ASSERT_EQUAL_INT(12, hours);
  TEST_ASSERT_EQUAL_INT(0, minutes);
  TEST_ASSERT_EQUAL_INT(0, seconds);
}

void test_GetTimeFromEpoch_specific_time() {
  int hours, minutes, seconds;
  GetTimeFromEpoch(55845, hours, minutes, seconds);
  TEST_ASSERT_EQUAL_INT(15, hours);
  TEST_ASSERT_EQUAL_INT(30, minutes);
  TEST_ASSERT_EQUAL_INT(45, seconds);
}

void test_GetTimeFromEpoch_end_of_day() {
  int hours, minutes, seconds;
  GetTimeFromEpoch(86399, hours, minutes, seconds);
  TEST_ASSERT_EQUAL_INT(23, hours);
  TEST_ASSERT_EQUAL_INT(59, minutes);
  TEST_ASSERT_EQUAL_INT(59, seconds);
}

void test_ApplyConfiguredOffset_applies_gmt_and_dst() {
  TEST_ASSERT_EQUAL_UINT32(1714543200UL, ApplyConfiguredOffset(1714528800UL, 7200, 7200));
}

void test_ApplyConfiguredOffset_supports_negative_offsets() {
  TEST_ASSERT_EQUAL_UINT32(1714510800UL, ApplyConfiguredOffset(1714528800UL, -18000, 0));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_FormatTwoDigits_zero);
  RUN_TEST(test_FormatTwoDigits_single_digit);
  RUN_TEST(test_FormatTwoDigits_double_digit);
  RUN_TEST(test_FormatTwoDigits_max);
  RUN_TEST(test_FormatTwoDigits_negative_clamped);
  RUN_TEST(test_FormatTwoDigits_over99_clamped);
  RUN_TEST(test_IsLeapYear_regular_year);
  RUN_TEST(test_IsLeapYear_divisible_by_4);
  RUN_TEST(test_IsLeapYear_century_not_leap);
  RUN_TEST(test_IsLeapYear_400_year_leap);
  RUN_TEST(test_IsLeapYear_edge_cases);
  RUN_TEST(test_IsPacketValid_valid_packet);
  RUN_TEST(test_IsPacketValid_invalid_li);
  RUN_TEST(test_IsPacketValid_invalid_version);
  RUN_TEST(test_IsPacketValid_invalid_mode);
  RUN_TEST(test_IsPacketValid_invalid_stratum_zero);
  RUN_TEST(test_IsPacketValid_invalid_stratum_high);
  RUN_TEST(test_IsPacketValid_all_zeros_timestamp);
  RUN_TEST(test_GetDateFromEpoch_unix_epoch);
  RUN_TEST(test_GetDateFromEpoch_known_date);
  RUN_TEST(test_GetDateFromEpoch_leap_year_feb29);
  RUN_TEST(test_GetDateFromEpoch_year_end);
  RUN_TEST(test_GetTimeFromEpoch_midnight);
  RUN_TEST(test_GetTimeFromEpoch_noon);
  RUN_TEST(test_GetTimeFromEpoch_specific_time);
  RUN_TEST(test_GetTimeFromEpoch_end_of_day);
  RUN_TEST(test_ApplyConfiguredOffset_applies_gmt_and_dst);
  RUN_TEST(test_ApplyConfiguredOffset_supports_negative_offsets);
  return UNITY_END();
}
