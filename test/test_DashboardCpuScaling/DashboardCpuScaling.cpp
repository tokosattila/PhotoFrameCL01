/**
 * @file DashboardCpuScaling.cpp
 * @brief Unit tests for dashboard CPU scaling and page key logic (pure C++ logic).
 */

#include <unity.h>
#include <cstdint>
#include <cstring>
#include <string>

// ============================================================================
// Constants mirroring Dashboard.h / Global.h
// ============================================================================

static constexpr uint32_t kPageHighPerformanceHoldMs = 6 * 1000;
static constexpr uint32_t kMediaHighPerformanceHoldMs = 10 * 1000;
static constexpr uint32_t kOtaHighPerformanceHoldMs = 45 * 1000;
static constexpr uint32_t kMaintenanceInactivityTimeoutMs = 10 * 60 * 1000;

// ============================================================================
// Standalone helpers mirroring production logic
// ============================================================================

static bool IsHighPerformancePageKey(const char *tPageKey) {
  return strcmp(tPageKey, "index") == 0 || strcmp(tPageKey, "firmware") == 0;
}

enum class EHighPerformanceWorkload { Page, Media, Ota };

struct SCpuDemandState {
  uint32_t lastPageMs = 0;
  uint32_t lastMediaMs = 0;
  uint32_t lastOtaMs = 0;
  bool dynamicCpuScalingEnabled = true;
};

static bool EvaluateHighPerfDemand(const SCpuDemandState &tState, uint32_t tNow) {
  const bool tPageDemand = tState.lastPageMs != 0 && static_cast<uint32_t>(tNow - tState.lastPageMs) < kPageHighPerformanceHoldMs;
  const bool tMediaDemand = tState.lastMediaMs != 0 && static_cast<uint32_t>(tNow - tState.lastMediaMs) < kMediaHighPerformanceHoldMs;
  const bool tOtaDemand = tState.lastOtaMs != 0 && static_cast<uint32_t>(tNow - tState.lastOtaMs) < kOtaHighPerformanceHoldMs;
  return tState.dynamicCpuScalingEnabled && (tPageDemand || tMediaDemand || tOtaDemand);
}

static bool InactivityExpired(uint32_t tRef, uint32_t tNow, uint32_t tTimeoutMs) {
  return static_cast<uint32_t>(tNow - tRef) >= tTimeoutMs;
}

// ============================================================================
// IsHighPerformancePageKey tests
// ============================================================================

void test_PageKey_index_is_high_performance() {
  TEST_ASSERT_TRUE(IsHighPerformancePageKey("index"));
}

void test_PageKey_firmware_is_high_performance() {
  TEST_ASSERT_TRUE(IsHighPerformancePageKey("firmware"));
}

void test_PageKey_display_is_not_high_performance() {
  TEST_ASSERT_FALSE(IsHighPerformancePageKey("display"));
}

void test_PageKey_empty_is_not_high_performance() {
  TEST_ASSERT_FALSE(IsHighPerformancePageKey(""));
}

void test_PageKey_config_is_not_high_performance() {
  TEST_ASSERT_FALSE(IsHighPerformancePageKey("config"));
}

// ============================================================================
// EvaluateHighPerfDemand tests
// ============================================================================

void test_Demand_no_activity_yields_low_perf() {
  SCpuDemandState tState {};
  TEST_ASSERT_FALSE(EvaluateHighPerfDemand(tState, 10000));
}

void test_Demand_page_within_hold_window_yields_high_perf() {
  SCpuDemandState tState {};
  tState.lastPageMs = 5000;
  const uint32_t tNow = 5000 + kPageHighPerformanceHoldMs - 1;
  TEST_ASSERT_TRUE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_page_exactly_at_hold_expiry_yields_low_perf() {
  SCpuDemandState tState {};
  tState.lastPageMs = 5000;
  const uint32_t tNow = 5000 + kPageHighPerformanceHoldMs;
  TEST_ASSERT_FALSE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_media_within_hold_window_yields_high_perf() {
  SCpuDemandState tState {};
  tState.lastMediaMs = 1000;
  const uint32_t tNow = 1000 + kMediaHighPerformanceHoldMs - 1;
  TEST_ASSERT_TRUE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_media_expired_yields_low_perf() {
  SCpuDemandState tState {};
  tState.lastMediaMs = 1000;
  const uint32_t tNow = 1000 + kMediaHighPerformanceHoldMs;
  TEST_ASSERT_FALSE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_ota_within_hold_window_yields_high_perf() {
  SCpuDemandState tState {};
  tState.lastOtaMs = 2000;
  const uint32_t tNow = 2000 + kOtaHighPerformanceHoldMs - 1;
  TEST_ASSERT_TRUE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_ota_expired_yields_low_perf() {
  SCpuDemandState tState {};
  tState.lastOtaMs = 2000;
  const uint32_t tNow = 2000 + kOtaHighPerformanceHoldMs;
  TEST_ASSERT_FALSE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_any_active_window_yields_high_perf() {
  SCpuDemandState tState {};
  // page expired, media active
  tState.lastPageMs = 0;
  tState.lastMediaMs = 3000;
  const uint32_t tNow = 3000 + kMediaHighPerformanceHoldMs - 1;
  TEST_ASSERT_TRUE(EvaluateHighPerfDemand(tState, tNow));
}

void test_Demand_disabled_dynamic_scaling_always_low_perf() {
  SCpuDemandState tState {};
  tState.dynamicCpuScalingEnabled = false;
  tState.lastPageMs = 5000;
  tState.lastMediaMs = 5000;
  tState.lastOtaMs = 5000;
  const uint32_t tNow = 5001;
  TEST_ASSERT_FALSE(EvaluateHighPerfDemand(tState, tNow));
}

// ============================================================================
// Maintenance inactivity timeout tests
// ============================================================================

void test_InactivityTimeout_not_expired_before_timeout() {
  const uint32_t tRef = 1000;
  const uint32_t tNow = tRef + kMaintenanceInactivityTimeoutMs - 1;
  TEST_ASSERT_FALSE(InactivityExpired(tRef, tNow, kMaintenanceInactivityTimeoutMs));
}

void test_InactivityTimeout_expired_at_timeout() {
  const uint32_t tRef = 1000;
  const uint32_t tNow = tRef + kMaintenanceInactivityTimeoutMs;
  TEST_ASSERT_TRUE(InactivityExpired(tRef, tNow, kMaintenanceInactivityTimeoutMs));
}

void test_InactivityTimeout_value_is_10_minutes() {
  TEST_ASSERT_EQUAL_UINT32(600000, kMaintenanceInactivityTimeoutMs);
}

// ============================================================================
// Entry point
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();

  RUN_TEST(test_PageKey_index_is_high_performance);
  RUN_TEST(test_PageKey_firmware_is_high_performance);
  RUN_TEST(test_PageKey_display_is_not_high_performance);
  RUN_TEST(test_PageKey_empty_is_not_high_performance);
  RUN_TEST(test_PageKey_config_is_not_high_performance);

  RUN_TEST(test_Demand_no_activity_yields_low_perf);
  RUN_TEST(test_Demand_page_within_hold_window_yields_high_perf);
  RUN_TEST(test_Demand_page_exactly_at_hold_expiry_yields_low_perf);
  RUN_TEST(test_Demand_media_within_hold_window_yields_high_perf);
  RUN_TEST(test_Demand_media_expired_yields_low_perf);
  RUN_TEST(test_Demand_ota_within_hold_window_yields_high_perf);
  RUN_TEST(test_Demand_ota_expired_yields_low_perf);
  RUN_TEST(test_Demand_any_active_window_yields_high_perf);
  RUN_TEST(test_Demand_disabled_dynamic_scaling_always_low_perf);

  RUN_TEST(test_InactivityTimeout_not_expired_before_timeout);
  RUN_TEST(test_InactivityTimeout_expired_at_timeout);
  RUN_TEST(test_InactivityTimeout_value_is_10_minutes);

  return UNITY_END();
}
