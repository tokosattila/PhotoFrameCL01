#include <unity.h>
#include <cstdint>
#include <cstddef>
#include <cmath>

static constexpr uint32_t kSampleRate = 44100;
static constexpr float kPi = 3.14159265358979323846f;
static constexpr uint8_t kDefaultMasterVolumePct = 25;

struct SSound {
  uint16_t FrequencyHz;
  uint16_t DurationMs;
  uint8_t AmplitudePct;
  uint16_t PauseMs;
};

static uint8_t ClampAmplitudePercent(uint8_t tAmplitudePct) {
  return tAmplitudePct > 100 ? 100 : tAmplitudePct;
}

static size_t ComputeToneFrames(uint32_t tSampleRate, uint16_t tDurationMs) {
  return static_cast<size_t>((static_cast<uint32_t>(tSampleRate) * tDurationMs) / 1000UL);
}

static size_t ComputeSequenceDurationMs(const SSound *tSteps, size_t tCount) {
  if (!tSteps || tCount == 0) return 0;
  size_t tTotalMs = 0;
  for (size_t tIndex = 0; tIndex < tCount; tIndex++) tTotalMs += tSteps[tIndex].DurationMs + tSteps[tIndex].PauseMs;
  return tTotalMs;
}

static float SimulatePhaseWrap(uint16_t tFrequencyHz, uint16_t tDurationMs) {
  if (tFrequencyHz == 0 || tDurationMs == 0) return 0.0f;
  const size_t tFrames = ComputeToneFrames(kSampleRate, tDurationMs);
  const float tStep = (2.0f * kPi * static_cast<float>(tFrequencyHz)) / static_cast<float>(kSampleRate);
  float tPhase = 0.0f;
  for (size_t tFrame = 0; tFrame < tFrames; tFrame++) {
    tPhase += tStep;
    if (tPhase >= (2.0f * kPi)) tPhase -= (2.0f * kPi);
  }

  return tPhase;
}

static uint8_t ClampVolumePercent(uint8_t tVolumePct) {
  return tVolumePct > 100 ? 100 : tVolumePct;
}

static uint8_t ApplyMasterVolume(uint8_t tAmplitudePct, uint8_t tMasterVolumePct) {
  uint16_t tScaled = static_cast<uint16_t>((static_cast<uint16_t>(tAmplitudePct) * ClampVolumePercent(tMasterVolumePct)) / 100U);
  if (tScaled > 100U) tScaled = 100U;
  return static_cast<uint8_t>(tScaled);
}

void test_ClampAmplitudePercent_limits_to_100() {
  TEST_ASSERT_EQUAL_UINT8(0, ClampAmplitudePercent(0));
  TEST_ASSERT_EQUAL_UINT8(85, ClampAmplitudePercent(85));
  TEST_ASSERT_EQUAL_UINT8(100, ClampAmplitudePercent(100));
  TEST_ASSERT_EQUAL_UINT8(100, ClampAmplitudePercent(200));
}

void test_ComputeToneFrames_uses_integer_ms_math() {
  TEST_ASSERT_EQUAL_UINT32(44, ComputeToneFrames(44100, 1));
  TEST_ASSERT_EQUAL_UINT32(2205, ComputeToneFrames(44100, 50));
  TEST_ASSERT_EQUAL_UINT32(4410, ComputeToneFrames(44100, 100));
}

void test_ComputeSequenceDurationMs_sums_duration_and_pause() {
  static const SSound kSound[] = {
    {880, 70, 35, 15},
    {1040, 90, 0, 15}
  };

  TEST_ASSERT_EQUAL_UINT32(190, ComputeSequenceDurationMs(kSound, sizeof(kSound) / sizeof(kSound[0])));
}

void test_SimulatePhaseWrap_stays_in_valid_range() {
  const float tPhase = SimulatePhaseWrap(560, 300);
  TEST_ASSERT_TRUE(tPhase >= 0.0f);
  TEST_ASSERT_TRUE(tPhase < (2.0f * kPi));
}

void test_DefaultMasterVolumePct_is_25() {
  TEST_ASSERT_EQUAL_UINT8(25, kDefaultMasterVolumePct);
}

void test_ClampVolumePercent_limits_to_100() {
  TEST_ASSERT_EQUAL_UINT8(0, ClampVolumePercent(0));
  TEST_ASSERT_EQUAL_UINT8(40, ClampVolumePercent(40));
  TEST_ASSERT_EQUAL_UINT8(100, ClampVolumePercent(100));
  TEST_ASSERT_EQUAL_UINT8(100, ClampVolumePercent(255));
}

void test_ApplyMasterVolume_scales_to_40_percent() {
  TEST_ASSERT_EQUAL_UINT8(20, ApplyMasterVolume(50, 40));
  TEST_ASSERT_EQUAL_UINT8(40, ApplyMasterVolume(100, 40));
}

void test_ApplyMasterVolume_clamps_scaled_value_to_100() {
  TEST_ASSERT_EQUAL_UINT8(100, ApplyMasterVolume(250, 100));
  TEST_ASSERT_EQUAL_UINT8(100, ApplyMasterVolume(255, 255));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_ClampAmplitudePercent_limits_to_100);
  RUN_TEST(test_ComputeToneFrames_uses_integer_ms_math);
  RUN_TEST(test_ComputeSequenceDurationMs_sums_duration_and_pause);
  RUN_TEST(test_SimulatePhaseWrap_stays_in_valid_range);
  RUN_TEST(test_DefaultMasterVolumePct_is_25);
  RUN_TEST(test_ClampVolumePercent_limits_to_100);
  RUN_TEST(test_ApplyMasterVolume_scales_to_40_percent);
  RUN_TEST(test_ApplyMasterVolume_clamps_scaled_value_to_100);
  return UNITY_END();
}
