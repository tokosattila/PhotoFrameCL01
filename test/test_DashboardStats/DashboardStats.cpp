#include <unity.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

static void ByteToReadableSize(uint64_t tBytes, char *tBuffer, size_t tLength) {
  if (tBytes < 1024ULL) snprintf(tBuffer, tLength, "%llu B", static_cast<unsigned long long>(tBytes));
  else if (tBytes < 1024ULL * 1024ULL) {
    float tSizeKB = static_cast<float>(tBytes) / 1024.0f;
    if (fabs(tSizeKB - static_cast<int>(tSizeKB)) < 0.01f) snprintf(tBuffer, tLength, "%d KB", static_cast<int>(tSizeKB));
    else snprintf(tBuffer, tLength, "%.2f KB", tSizeKB);
  } else if (tBytes < 1024ULL * 1024ULL * 1024ULL) {
    float tSizeMB = static_cast<float>(tBytes) / (1024.0f * 1024.0f);
    if (fabs(tSizeMB - static_cast<int>(tSizeMB)) < 0.01f) snprintf(tBuffer, tLength, "%d MB", static_cast<int>(tSizeMB));
    else snprintf(tBuffer, tLength, "%.2f MB", tSizeMB);
  } else {
    float tSizeGB = static_cast<float>(tBytes) / (1024.0f * 1024.0f * 1024.0f);
    if (fabs(tSizeGB - static_cast<int>(tSizeGB)) < 0.01f) snprintf(tBuffer, tLength, "%d GB", static_cast<int>(tSizeGB));
    else snprintf(tBuffer, tLength, "%.2f GB", tSizeGB);
  }
}

static uint32_t ResolveSketchTotalBytes(uint32_t tSketchUsedBytes, uint32_t tFreeSketchBytes, uint32_t tRunningPartitionSizeBytes) {
  uint32_t tSketchTotalBytes = tSketchUsedBytes + tFreeSketchBytes;
  if (tRunningPartitionSizeBytes > 0) tSketchTotalBytes = tRunningPartitionSizeBytes;
  return tSketchTotalBytes;
}

static std::string BuildProgramCodeText(uint32_t tSketchUsedBytes, uint32_t tFreeSketchBytes, uint32_t tRunningPartitionSizeBytes) {
  const uint32_t tSketchTotalBytes = ResolveSketchTotalBytes(tSketchUsedBytes, tFreeSketchBytes, tRunningPartitionSizeBytes);
  char tProgramCodeText[24] = "0 B";
  char tProgramCodeTotalText[24] = "0 B";
  ByteToReadableSize(tSketchUsedBytes, tProgramCodeText, sizeof(tProgramCodeText));
  ByteToReadableSize(tSketchTotalBytes, tProgramCodeTotalText, sizeof(tProgramCodeTotalText));
  return std::string(tProgramCodeText) + " / " + std::string(tProgramCodeTotalText);
}

static std::string BuildDisplayRotationText(uint16_t tRotation) {
  char tDisplayRotationText[16] = "0°";
  snprintf(tDisplayRotationText, sizeof(tDisplayRotationText), "%u°", static_cast<unsigned>(tRotation));
  return std::string(tDisplayRotationText);
}

void test_ProgramCode_uses_running_partition_size_when_available() {
  const uint32_t tUsed = 2569011;
  const uint32_t tFreeSketch = 3145728;
  const uint32_t tRunningPartition = 3145728;
  std::string tResult = BuildProgramCodeText(tUsed, tFreeSketch, tRunningPartition);
  TEST_ASSERT_EQUAL_STRING("2.45 MB / 3 MB", tResult.c_str());
}

void test_ProgramCode_fallbacks_to_used_plus_free_when_partition_unknown() {
  const uint32_t tUsed = 1024 * 1024;
  const uint32_t tFreeSketch = 2 * 1024 * 1024;
  const uint32_t tRunningPartition = 0;
  std::string tResult = BuildProgramCodeText(tUsed, tFreeSketch, tRunningPartition);
  TEST_ASSERT_EQUAL_STRING("1 MB / 3 MB", tResult.c_str());
}

void test_ProgramCode_small_values_are_formatted_consistently() {
  const uint32_t tUsed = 1536;
  const uint32_t tFreeSketch = 512;
  const uint32_t tRunningPartition = 2048;
  std::string tResult = BuildProgramCodeText(tUsed, tFreeSketch, tRunningPartition);
  TEST_ASSERT_EQUAL_STRING("1.50 KB / 2 KB", tResult.c_str());
}

void test_DisplayRotation_formats_degrees() {
  std::string tResult = BuildDisplayRotationText(180);
  TEST_ASSERT_EQUAL_STRING("180°", tResult.c_str());
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_ProgramCode_uses_running_partition_size_when_available);
  RUN_TEST(test_ProgramCode_fallbacks_to_used_plus_free_when_partition_unknown);
  RUN_TEST(test_ProgramCode_small_values_are_formatted_consistently);
  RUN_TEST(test_DisplayRotation_formats_degrees);
  return UNITY_END();
}
