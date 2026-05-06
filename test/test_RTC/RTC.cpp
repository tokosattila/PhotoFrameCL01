#include <unity.h>
#include <cstdint>

static const unsigned long kSecondsPerMinute = 60;
static const unsigned long kSecondsPerHour = 3600;
static const unsigned long kSecondsPerDay = 86400;
static const unsigned long kMinSyncEpoch = 1735689600UL;
static const unsigned long kMaxSigned32Epoch = 2147483647UL;

struct SRTCDateTime {
  uint8_t Second = 0;
  uint8_t Minute = 0;
  uint8_t Hour = 0;
  uint8_t DayOfWeek = 0;
  uint8_t Day = 1;
  uint8_t Month = 1;
  uint16_t Year = 2026;
};

static uint8_t BcdToDec(uint8_t tBcd) {
  return ((tBcd >> 4) * 10) + (tBcd & 0x0F);
}

static uint8_t DecToBcd(uint8_t tDec) {
  return ((tDec / 10) << 4) | (tDec % 10);
}

static bool IsLeapYear(uint16_t tYear) {
  return (tYear % 4 == 0 && (tYear % 100 != 0 || tYear % 400 == 0));
}

static bool IsDateTimePlausible(const SRTCDateTime &tDateTime) {
  if (tDateTime.Second > 59) return false;
  if (tDateTime.Minute > 59) return false;
  if (tDateTime.Hour > 23) return false;
  if (tDateTime.Month < 1 || tDateTime.Month > 12) return false;
  if (tDateTime.Day < 1 || tDateTime.Day > 31) return false;
  if (tDateTime.DayOfWeek > 6) return false;
  if (tDateTime.Year < 2000 || tDateTime.Year > 2099) return false;
  return true;
}

static unsigned long DateTimeToEpoch(const SRTCDateTime &tDateTime) {
  unsigned long tDays = 0;
  for (uint16_t tYear = 1970; tYear < tDateTime.Year; tYear++) tDays += IsLeapYear(tYear) ? 366 : 365;
  static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (uint8_t tMonth = 1; tMonth < tDateTime.Month; tMonth++) {
    tDays += kDaysInMonth[tMonth - 1];
    if (tMonth == 2 && IsLeapYear(tDateTime.Year)) tDays++;
  }

  tDays += tDateTime.Day - 1;
  return tDays * kSecondsPerDay + tDateTime.Hour * kSecondsPerHour + tDateTime.Minute * kSecondsPerMinute + tDateTime.Second;
}

static void EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDateTime) {
  unsigned long tSeconds = tEpoch % 60;
  tEpoch /= 60;
  unsigned long tMinutes = tEpoch % 60;
  tEpoch /= 60;
  unsigned long tHours = tEpoch % 24;
  tEpoch /= 24;
  tDateTime.Second = static_cast<uint8_t>(tSeconds);
  tDateTime.Minute = static_cast<uint8_t>(tMinutes);
  tDateTime.Hour = static_cast<uint8_t>(tHours);
  unsigned long tDays = tEpoch;
  tDateTime.DayOfWeek = static_cast<uint8_t>((tDays + 4) % 7);
  uint16_t tYear = 1970;
  while (true) {
    const uint16_t tDaysInYear = IsLeapYear(tYear) ? 366 : 365;
    if (tDays < tDaysInYear) break;
    tDays -= tDaysInYear;
    tYear++;
  }

  tDateTime.Year = tYear;
  static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t tMonth = 1;
  while (tMonth <= 12) {
    uint8_t tDaysInMonth = kDaysInMonth[tMonth - 1];
    if (tMonth == 2 && IsLeapYear(tYear)) tDaysInMonth = 29;
    if (tDays < tDaysInMonth) break;
    tDays -= tDaysInMonth;
    tMonth++;
  }

  tDateTime.Month = tMonth;
  tDateTime.Day = static_cast<uint8_t>(tDays + 1);
}

static bool CanSyncEpochToSystem(unsigned long tEpoch) {
  if (tEpoch == 0) return false;
  if (tEpoch > kMaxSigned32Epoch) return false;
  if (tEpoch < kMinSyncEpoch) return false;
  return true;
}

void test_Bcd_roundtrip_0_to_99() {
  for (uint8_t tValue = 0; tValue <= 99; tValue++) TEST_ASSERT_EQUAL_UINT8(tValue, BcdToDec(DecToBcd(tValue)));
}

void test_IsDateTimePlausible_accepts_valid() {
  SRTCDateTime tDateTime = {59, 59, 23, 6, 31, 12, 2099};
  TEST_ASSERT_TRUE(IsDateTimePlausible(tDateTime));
}

void test_IsDateTimePlausible_rejects_invalid_ranges() {
  SRTCDateTime tDateTime = {60, 0, 0, 0, 1, 1, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 60, 0, 0, 1, 1, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 24, 0, 1, 1, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 0, 7, 1, 1, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 0, 0, 0, 1, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 0, 0, 1, 13, 2026};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 0, 0, 1, 1, 1999};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
  tDateTime = {0, 0, 0, 0, 1, 1, 2100};
  TEST_ASSERT_FALSE(IsDateTimePlausible(tDateTime));
}

void test_DateTimeToEpoch_known_values() {
  SRTCDateTime tDateTime = {0, 0, 0, 4, 1, 1, 1970};
  TEST_ASSERT_EQUAL_UINT32(0, DateTimeToEpoch(tDateTime));
  tDateTime = {0, 0, 0, 0, 1, 1, 2026};
  TEST_ASSERT_EQUAL_UINT32(1767225600UL, DateTimeToEpoch(tDateTime));
  tDateTime = {45, 30, 12, 0, 1, 1, 2024};
  TEST_ASSERT_EQUAL_UINT32(1704112245UL, DateTimeToEpoch(tDateTime));
}

void test_EpochToDateTime_known_values() {
  SRTCDateTime tDateTime = {};
  EpochToDateTime(0, tDateTime);
  TEST_ASSERT_EQUAL_UINT16(1970, tDateTime.Year);
  TEST_ASSERT_EQUAL_UINT8(1, tDateTime.Month);
  TEST_ASSERT_EQUAL_UINT8(1, tDateTime.Day);
  TEST_ASSERT_EQUAL_UINT8(0, tDateTime.Hour);
  TEST_ASSERT_EQUAL_UINT8(0, tDateTime.Minute);
  TEST_ASSERT_EQUAL_UINT8(0, tDateTime.Second);
  EpochToDateTime(1767225600UL, tDateTime);
  TEST_ASSERT_EQUAL_UINT16(2026, tDateTime.Year);
  TEST_ASSERT_EQUAL_UINT8(1, tDateTime.Month);
  TEST_ASSERT_EQUAL_UINT8(1, tDateTime.Day);
}

void test_Epoch_DateTime_roundtrip() {
  const unsigned long tEpochValues[] = {0UL, 86400UL, 1704067200UL, 1709164800UL, 1767225600UL, 1893456000UL};
  for (size_t tIndex = 0; tIndex < sizeof(tEpochValues) / sizeof(tEpochValues[0]); tIndex++) {
    SRTCDateTime tDateTime = {};
    EpochToDateTime(tEpochValues[tIndex], tDateTime);
    const unsigned long tRoundTrip = DateTimeToEpoch(tDateTime);
    TEST_ASSERT_EQUAL_UINT32(tEpochValues[tIndex], tRoundTrip);
  }
}

void test_CanSyncEpochToSystem_bounds() {
  TEST_ASSERT_FALSE(CanSyncEpochToSystem(0));
  TEST_ASSERT_FALSE(CanSyncEpochToSystem(kMinSyncEpoch - 1));
  TEST_ASSERT_TRUE(CanSyncEpochToSystem(kMinSyncEpoch));
  TEST_ASSERT_TRUE(CanSyncEpochToSystem(kMaxSigned32Epoch));
  TEST_ASSERT_FALSE(CanSyncEpochToSystem(kMaxSigned32Epoch + 1UL));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_Bcd_roundtrip_0_to_99);
  RUN_TEST(test_IsDateTimePlausible_accepts_valid);
  RUN_TEST(test_IsDateTimePlausible_rejects_invalid_ranges);
  RUN_TEST(test_DateTimeToEpoch_known_values);
  RUN_TEST(test_EpochToDateTime_known_values);
  RUN_TEST(test_Epoch_DateTime_roundtrip);
  RUN_TEST(test_CanSyncEpochToSystem_bounds);
  return UNITY_END();
}
