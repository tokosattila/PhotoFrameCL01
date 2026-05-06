#include <unity.h>

static bool ResolveInternetEnable(bool tApModeEnable, const char *tStaSsid) {
  (void)tStaSsid;
  return tApModeEnable ? false : true;
}

void test_InternetEnable_false_when_ap_mode_enabled() {
  TEST_ASSERT_FALSE(ResolveInternetEnable(true, ""));
}

void test_InternetEnable_true_when_sta_mode_enabled() {
  TEST_ASSERT_TRUE(ResolveInternetEnable(false, ""));
}

void test_InternetEnable_stays_false_in_ap_mode_even_with_sta_ssid() {
  TEST_ASSERT_FALSE(ResolveInternetEnable(true, "HomeWifi"));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_InternetEnable_false_when_ap_mode_enabled);
  RUN_TEST(test_InternetEnable_true_when_sta_mode_enabled);
  RUN_TEST(test_InternetEnable_stays_false_in_ap_mode_even_with_sta_ssid);
  return UNITY_END();
}
