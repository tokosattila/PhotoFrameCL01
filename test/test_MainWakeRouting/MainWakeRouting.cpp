#include <unity.h>
#include <cstdint>

enum class EBootRoute : uint8_t {
  PhotoFrame = 1,
  Maintenance
};

EBootRoute ResolveBootRouteProduction(bool tWokenBySettingPin) {
  if (tWokenBySettingPin) return EBootRoute::Maintenance;
  return EBootRoute::PhotoFrame;
}

EBootRoute ResolveBootRouteDev(bool tMaintenanceRequested, bool tWokenBySettingPin) {
  if (tMaintenanceRequested) return EBootRoute::Maintenance;
  if (tWokenBySettingPin) return EBootRoute::Maintenance;
  return EBootRoute::PhotoFrame;
}

void test_ResolveBootRouteProduction_setting_pin_enters_maintenance() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::Maintenance), static_cast<uint8_t>(ResolveBootRouteProduction(true)));
}

void test_ResolveBootRouteProduction_default_photoframe() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::PhotoFrame), static_cast<uint8_t>(ResolveBootRouteProduction(false)));
}

void test_ResolveBootRouteDev_marker_enters_maintenance() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::Maintenance), static_cast<uint8_t>(ResolveBootRouteDev(true, false)));
}

void test_ResolveBootRouteDev_setting_pin_enters_maintenance() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::Maintenance), static_cast<uint8_t>(ResolveBootRouteDev(false, true)));
}

void test_ResolveBootRouteDev_both_true_still_maintenance() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::Maintenance), static_cast<uint8_t>(ResolveBootRouteDev(true, true)));
}

void test_ResolveBootRouteDev_default_photoframe() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(EBootRoute::PhotoFrame), static_cast<uint8_t>(ResolveBootRouteDev(false, false)));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_ResolveBootRouteProduction_setting_pin_enters_maintenance);
  RUN_TEST(test_ResolveBootRouteProduction_default_photoframe);
  RUN_TEST(test_ResolveBootRouteDev_marker_enters_maintenance);
  RUN_TEST(test_ResolveBootRouteDev_setting_pin_enters_maintenance);
  RUN_TEST(test_ResolveBootRouteDev_both_true_still_maintenance);
  RUN_TEST(test_ResolveBootRouteDev_default_photoframe);
  return UNITY_END();
}
