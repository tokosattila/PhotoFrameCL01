#include <unity.h>
#include <cstdint>

// ---------- Mirrors of production types (host-native, no Arduino link) ----------

enum class ETimerWakeUp : uint8_t {
  Minutes = 1,
  Hourly,
  HalfDay,
  Daily,
  Weekly,
  Monthly
};

struct STimerConfig {
  ETimerWakeUp WakeUp = ETimerWakeUp::Daily;
  uint8_t WakeUpHour = 6;
};

struct SRTCDateTime {
  uint8_t Second = 0;
  uint8_t Minute = 0;
  uint8_t Hour = 0;
  uint8_t DayOfWeek = 0;
  uint8_t Day = 1;
  uint8_t Month = 1;
  uint16_t Year = 2026;
};

struct SAlarmSpec {
  uint8_t Minute = 0;
  uint8_t Hour = 0;
  uint8_t Day = 0;
  uint8_t Weekday = 0;
  bool EnableMinute = false;
  bool EnableHour = false;
  bool EnableDay = false;
  bool EnableWeekday = false;
};

struct SWakeSchedule {
  SRTCDateTime NextWake;
  uint32_t DelaySeconds = 0;
  SAlarmSpec Alarm;
};

static constexpr uint32_t kMinDelaySec = 60;
static constexpr uint32_t kMaxDelaySec = 60UL * 60UL * 24UL * 40UL;

// ---------- Mirrors of RTC epoch helpers ----------

static unsigned long DateTimeToEpoch(const SRTCDateTime &tDt) {
  unsigned long tDays = 0;
  for (uint16_t y = 1970; y < tDt.Year; y++) {
    tDays += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  }
  static const uint8_t tDim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (uint8_t m = 1; m < tDt.Month; m++) {
    tDays += tDim[m - 1];
    if (m == 2 && (tDt.Year % 4 == 0 && (tDt.Year % 100 != 0 || tDt.Year % 400 == 0))) tDays++;
  }
  tDays += tDt.Day - 1;
  return tDays * 86400UL + tDt.Hour * 3600UL + tDt.Minute * 60UL + tDt.Second;
}

static void EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDt) {
  unsigned long tSeconds = tEpoch % 60; tEpoch /= 60;
  unsigned long tMinutes = tEpoch % 60; tEpoch /= 60;
  unsigned long tHours = tEpoch % 24; tEpoch /= 24;
  tDt.Second = (uint8_t)tSeconds;
  tDt.Minute = (uint8_t)tMinutes;
  tDt.Hour = (uint8_t)tHours;
  unsigned long tDays = tEpoch;
  tDt.DayOfWeek = (uint8_t)((tDays + 4) % 7);
  uint16_t tYear = 1970;
  while (true) {
    uint16_t tDiy = (tYear % 4 == 0 && (tYear % 100 != 0 || tYear % 400 == 0)) ? 366 : 365;
    if (tDays < tDiy) break;
    tDays -= tDiy;
    tYear++;
  }
  tDt.Year = tYear;
  static const uint8_t tDim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t tMonth = 1;
  while (tMonth <= 12) {
    uint8_t tD = tDim[tMonth - 1];
    if (tMonth == 2 && (tYear % 4 == 0 && (tYear % 100 != 0 || tYear % 400 == 0))) tD = 29;
    if (tDays < tD) break;
    tDays -= tD;
    tMonth++;
  }
  tDt.Month = tMonth;
  tDt.Day = (uint8_t)(tDays + 1);
}

// ---------- Mirror of WakeScheduler logic ----------

static void AddSeconds(SRTCDateTime &tDt, uint32_t tSeconds) {
  unsigned long e = DateTimeToEpoch(tDt) + tSeconds;
  EpochToDateTime(e, tDt);
}

static void AlignToHour(SRTCDateTime &tDt, uint8_t tTargetHour) {
  SRTCDateTime tTarget = tDt;
  tTarget.Hour = (uint8_t)(tTargetHour % 24);
  tTarget.Minute = 0;
  tTarget.Second = 0;
  unsigned long tNow = DateTimeToEpoch(tDt);
  unsigned long tTgt = DateTimeToEpoch(tTarget);
  if (tTgt <= tNow) tTgt += 86400UL;
  EpochToDateTime(tTgt, tDt);
}

static void FillAlarmSpec(const SRTCDateTime &tNext, SAlarmSpec &tOut) {
  tOut.Minute = tNext.Minute;
  tOut.Hour = tNext.Hour;
  tOut.Day = tNext.Day;
  tOut.Weekday = 0;
  tOut.EnableMinute = true;
  tOut.EnableHour = true;
  tOut.EnableDay = true;
  tOut.EnableWeekday = false;
}

static bool Compute(const STimerConfig &tCfg, const SRTCDateTime &tNow, SWakeSchedule &tOut) {
  if (tNow.Year < 2000 || tNow.Year > 2099) return false;
  if (tNow.Month < 1 || tNow.Month > 12) return false;
  if (tNow.Day < 1 || tNow.Day > 31) return false;
  if (tNow.Hour > 23 || tNow.Minute > 59 || tNow.Second > 59) return false;
  const uint8_t tHour = (uint8_t)(tCfg.WakeUpHour % 24);
  SRTCDateTime tNext = tNow;
  switch (tCfg.WakeUp) {
    case ETimerWakeUp::Minutes:  AddSeconds(tNext, 60); break;
    case ETimerWakeUp::Hourly:   AddSeconds(tNext, 3600UL); break;
    case ETimerWakeUp::HalfDay:  AddSeconds(tNext, 12UL * 3600UL); break;
    case ETimerWakeUp::Daily:    AlignToHour(tNext, tHour); break;
    case ETimerWakeUp::Weekly:   AlignToHour(tNext, tHour); AddSeconds(tNext, 6UL * 86400UL); break;
    case ETimerWakeUp::Monthly:  AlignToHour(tNext, tHour); AddSeconds(tNext, 29UL * 86400UL); break;
    default: AlignToHour(tNext, tHour); break;
  }
  unsigned long tNowE = DateTimeToEpoch(tNow);
  unsigned long tNextE = DateTimeToEpoch(tNext);
  if (tNextE <= tNowE) {
    tNextE = tNowE + kMinDelaySec;
    EpochToDateTime(tNextE, tNext);
  }
  unsigned long tDelta = tNextE - tNowE;
  if (tDelta < kMinDelaySec) tDelta = kMinDelaySec;
  if (tDelta > kMaxDelaySec) tDelta = kMaxDelaySec;
  tOut.NextWake = tNext;
  tOut.DelaySeconds = (uint32_t)tDelta;
  FillAlarmSpec(tNext, tOut.Alarm);
  return true;
}

// ---------- Helpers ----------

static SRTCDateTime MakeDt(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s = 0) {
  SRTCDateTime dt;
  dt.Year = y; dt.Month = mo; dt.Day = d;
  dt.Hour = h; dt.Minute = mi; dt.Second = s;
  return dt;
}

// ---------- Tests ----------

void setUp(void) {}
void tearDown(void) {}

void test_minutes_adds_60s(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Minutes; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 15, 10, 20, 30);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT32(60, out.DelaySeconds);
  TEST_ASSERT_EQUAL_UINT8(21, out.NextWake.Minute);
  TEST_ASSERT_EQUAL_UINT8(30, out.NextWake.Second);
}

void test_hourly_adds_3600s(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Hourly;
  SRTCDateTime now = MakeDt(2026, 3, 15, 10, 20, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT32(3600, out.DelaySeconds);
  TEST_ASSERT_EQUAL_UINT8(11, out.NextWake.Hour);
  TEST_ASSERT_EQUAL_UINT8(20, out.NextWake.Minute);
}

void test_halfday_adds_12h(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::HalfDay;
  SRTCDateTime now = MakeDt(2026, 3, 15, 10, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT32(12UL * 3600UL, out.DelaySeconds);
  TEST_ASSERT_EQUAL_UINT8(22, out.NextWake.Hour);
  TEST_ASSERT_EQUAL_UINT8(15, out.NextWake.Day);
}

void test_daily_before_target_today(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 15, 4, 30, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT8(15, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(6, out.NextWake.Hour);
  TEST_ASSERT_EQUAL_UINT8(0, out.NextWake.Minute);
}

void test_daily_after_target_tomorrow(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 15, 10, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT8(16, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(6, out.NextWake.Hour);
}

void test_daily_month_rollover(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 31, 10, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT8(1, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(4, out.NextWake.Month);
}

void test_daily_year_rollover(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 12, 31, 10, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT16(2027, out.NextWake.Year);
  TEST_ASSERT_EQUAL_UINT8(1, out.NextWake.Month);
  TEST_ASSERT_EQUAL_UINT8(1, out.NextWake.Day);
}

void test_weekly_advances_seven_days_to_target_hour(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Weekly; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 15, 4, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  // align: today 6:00 → +6 days = 21st 6:00
  TEST_ASSERT_EQUAL_UINT8(21, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(3, out.NextWake.Month);
  TEST_ASSERT_EQUAL_UINT8(6, out.NextWake.Hour);
}

void test_monthly_advances_about_a_month(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Monthly; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 1, 1, 4, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  // align: 1st 6:00 + 29 days = 30th 6:00
  TEST_ASSERT_EQUAL_UINT8(30, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(1, out.NextWake.Month);
}

void test_alarm_spec_fields_match_next_wake(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 3, 15, 4, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT8(out.NextWake.Minute, out.Alarm.Minute);
  TEST_ASSERT_EQUAL_UINT8(out.NextWake.Hour, out.Alarm.Hour);
  TEST_ASSERT_EQUAL_UINT8(out.NextWake.Day, out.Alarm.Day);
  TEST_ASSERT_TRUE(out.Alarm.EnableMinute);
  TEST_ASSERT_TRUE(out.Alarm.EnableHour);
  TEST_ASSERT_TRUE(out.Alarm.EnableDay);
  TEST_ASSERT_FALSE(out.Alarm.EnableWeekday);
  TEST_ASSERT_EQUAL_UINT8(0, out.Alarm.Weekday);
}

void test_delay_min_clamp(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Minutes;
  SRTCDateTime now = MakeDt(2026, 3, 15, 10, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_TRUE(out.DelaySeconds >= kMinDelaySec);
}

void test_delay_max_clamp(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Monthly; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2026, 1, 1, 0, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_TRUE(out.DelaySeconds <= kMaxDelaySec);
}

void test_rejects_invalid_datetime(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily;
  SRTCDateTime bad = MakeDt(1999, 1, 1, 0, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_FALSE(Compute(c, bad, out));
  bad = MakeDt(2026, 13, 1, 0, 0, 0);
  TEST_ASSERT_FALSE(Compute(c, bad, out));
}

void test_hour_wakeup_hour_modulo(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Daily; c.WakeUpHour = 30; // 30 % 24 = 6
  SRTCDateTime now = MakeDt(2026, 3, 15, 4, 0, 0);
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  TEST_ASSERT_EQUAL_UINT8(6, out.NextWake.Hour);
}

void test_weekly_leap_feb(void) {
  STimerConfig c; c.WakeUp = ETimerWakeUp::Weekly; c.WakeUpHour = 6;
  SRTCDateTime now = MakeDt(2028, 2, 25, 4, 0, 0); // 2028 leap
  SWakeSchedule out;
  TEST_ASSERT_TRUE(Compute(c, now, out));
  // align: 25th 6:00 + 6 days = March 2nd 6:00 (Feb has 29 in 2028)
  TEST_ASSERT_EQUAL_UINT8(2, out.NextWake.Day);
  TEST_ASSERT_EQUAL_UINT8(3, out.NextWake.Month);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_minutes_adds_60s);
  RUN_TEST(test_hourly_adds_3600s);
  RUN_TEST(test_halfday_adds_12h);
  RUN_TEST(test_daily_before_target_today);
  RUN_TEST(test_daily_after_target_tomorrow);
  RUN_TEST(test_daily_month_rollover);
  RUN_TEST(test_daily_year_rollover);
  RUN_TEST(test_weekly_advances_seven_days_to_target_hour);
  RUN_TEST(test_monthly_advances_about_a_month);
  RUN_TEST(test_alarm_spec_fields_match_next_wake);
  RUN_TEST(test_delay_min_clamp);
  RUN_TEST(test_delay_max_clamp);
  RUN_TEST(test_rejects_invalid_datetime);
  RUN_TEST(test_hour_wakeup_hour_modulo);
  RUN_TEST(test_weekly_leap_feb);
  return UNITY_END();
}
