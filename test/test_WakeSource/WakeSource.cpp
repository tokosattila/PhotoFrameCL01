#include <unity.h>
#include <cstdint>
#include <cstring>
#include <string>

static constexpr uint8_t kPinNextImg = 4U;
static constexpr uint8_t kPinSetting = 5U;
static constexpr uint8_t kPinRtcInt = 6U;

enum EWakeCause {
  kCauseUndefined = 0,
  kCauseExt0 = 2,
  kCauseExt1 = 3,
  kCauseTimer = 4,
  kCauseOther = 99
};

static const char *ResolveWakeSourceKey(EWakeCause tCause, uint64_t tExt1Status) {
  switch (tCause) {
    case kCauseTimer: return "wake_src_timer";
    case kCauseExt0: return "wake_src_setting_btn";
    case kCauseExt1: {
      if (tExt1Status & (1ULL << kPinRtcInt)) return "wake_src_rtc_alarm";
      if (tExt1Status & (1ULL << kPinNextImg)) return "wake_src_next_img_btn";
      return "wake_src_other";
    }
    case kCauseUndefined: return "wake_src_power_on";
    default: return "wake_src_other";
  }
}

static bool gBootRtcReady = false;
static void SetBootRtcReady(bool tReady) { gBootRtcReady = tReady; }
static bool WasBootRtcReady() { return gBootRtcReady; }

void setUp() { gBootRtcReady = false; }
void tearDown() {}

void test_WakeSource_Timer_returns_timer_key() {
  TEST_ASSERT_EQUAL_STRING("wake_src_timer", ResolveWakeSourceKey(kCauseTimer, 0));
}

void test_WakeSource_Ext0_returns_setting_button_key() {
  TEST_ASSERT_EQUAL_STRING("wake_src_setting_btn", ResolveWakeSourceKey(kCauseExt0, 0));
}

void test_WakeSource_Ext1_with_rtc_pin_returns_rtc_alarm_key() {
  const uint64_t tStatus = 1ULL << kPinRtcInt;
  TEST_ASSERT_EQUAL_STRING("wake_src_rtc_alarm", ResolveWakeSourceKey(kCauseExt1, tStatus));
}

void test_WakeSource_Ext1_with_next_image_pin_returns_next_img_key() {
  const uint64_t tStatus = 1ULL << kPinNextImg;
  TEST_ASSERT_EQUAL_STRING("wake_src_next_img_btn", ResolveWakeSourceKey(kCauseExt1, tStatus));
}

void test_WakeSource_Ext1_with_rtc_and_button_prefers_rtc() {
  const uint64_t tStatus = (1ULL << kPinRtcInt) | (1ULL << kPinNextImg);
  TEST_ASSERT_EQUAL_STRING("wake_src_rtc_alarm", ResolveWakeSourceKey(kCauseExt1, tStatus));
}

void test_WakeSource_Ext1_with_unrelated_pin_returns_other() {
  const uint64_t tStatus = 1ULL << 10;
  TEST_ASSERT_EQUAL_STRING("wake_src_other", ResolveWakeSourceKey(kCauseExt1, tStatus));
}

void test_WakeSource_Ext1_with_zero_status_returns_other() {
  TEST_ASSERT_EQUAL_STRING("wake_src_other", ResolveWakeSourceKey(kCauseExt1, 0));
}

void test_WakeSource_Undefined_returns_power_on_key() {
  TEST_ASSERT_EQUAL_STRING("wake_src_power_on", ResolveWakeSourceKey(kCauseUndefined, 0));
}

void test_WakeSource_Unknown_cause_returns_other() {
  TEST_ASSERT_EQUAL_STRING("wake_src_other", ResolveWakeSourceKey(kCauseOther, 0));
}

void test_BootRtcReady_defaults_to_false() {
  TEST_ASSERT_FALSE(WasBootRtcReady());
}

void test_BootRtcReady_can_be_set_true() {
  SetBootRtcReady(true);
  TEST_ASSERT_TRUE(WasBootRtcReady());
}

void test_BootRtcReady_can_be_toggled_back_to_false() {
  SetBootRtcReady(true);
  SetBootRtcReady(false);
  TEST_ASSERT_FALSE(WasBootRtcReady());
}

static void AppendWakeupSection(std::string &tJson, const char *tSourceKey, bool tBootRtcReady) {
  tJson += "{\"LabelKey\":\"wakeup_status\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"last_wake_source\",\"ValueKey\":\"";
  tJson += tSourceKey;
  tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"rtc_alarm\",\"ValueKey\":\"";
  tJson += tBootRtcReady ? "rtc_alarm_armed" : "rtc_alarm_unavailable";
  tJson += "\"}]}";
}

void test_WakeupSection_armed_rtc_alarm() {
  std::string tJson;
  AppendWakeupSection(tJson, "wake_src_rtc_alarm", true);
  TEST_ASSERT_EQUAL_STRING(
    "{\"LabelKey\":\"wakeup_status\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"last_wake_source\",\"ValueKey\":\"wake_src_rtc_alarm\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"rtc_alarm\",\"ValueKey\":\"rtc_alarm_armed\"}]}",
    tJson.c_str());
}

void test_WakeupSection_unavailable_rtc_with_timer_fallback() {
  std::string tJson;
  AppendWakeupSection(tJson, "wake_src_timer", false);
  TEST_ASSERT_EQUAL_STRING(
    "{\"LabelKey\":\"wakeup_status\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"last_wake_source\",\"ValueKey\":\"wake_src_timer\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"rtc_alarm\",\"ValueKey\":\"rtc_alarm_unavailable\"}]}",
    tJson.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_WakeSource_Timer_returns_timer_key);
  RUN_TEST(test_WakeSource_Ext0_returns_setting_button_key);
  RUN_TEST(test_WakeSource_Ext1_with_rtc_pin_returns_rtc_alarm_key);
  RUN_TEST(test_WakeSource_Ext1_with_next_image_pin_returns_next_img_key);
  RUN_TEST(test_WakeSource_Ext1_with_rtc_and_button_prefers_rtc);
  RUN_TEST(test_WakeSource_Ext1_with_unrelated_pin_returns_other);
  RUN_TEST(test_WakeSource_Ext1_with_zero_status_returns_other);
  RUN_TEST(test_WakeSource_Undefined_returns_power_on_key);
  RUN_TEST(test_WakeSource_Unknown_cause_returns_other);
  RUN_TEST(test_BootRtcReady_defaults_to_false);
  RUN_TEST(test_BootRtcReady_can_be_set_true);
  RUN_TEST(test_BootRtcReady_can_be_toggled_back_to_false);
  RUN_TEST(test_WakeupSection_armed_rtc_alarm);
  RUN_TEST(test_WakeupSection_unavailable_rtc_with_timer_fallback);
  return UNITY_END();
}
