#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <vector>
#include <string>

#ifdef _WIN32
  #define strcasecmp _stricmp
#else
  #include <strings.h>
#endif

bool SecureStrcmp(const char *tA, const char *tB) {
  if (!tA || !tB) return false;
  size_t tLenA = strlen(tA);
  size_t tLenB = strlen(tB);
  volatile uint8_t tDiff = static_cast<uint8_t>(tLenA ^ tLenB);
  size_t tMinLen = (tLenA < tLenB) ? tLenA : tLenB;
  for (size_t i = 0; i < tMinLen; i++) {
    tDiff |= static_cast<uint8_t>(tA[i] ^ tB[i]);
  }

  return tDiff == 0;
}

uint32_t SafeAtoul(const char *tStr, uint32_t tMinVal, uint32_t tMaxVal, uint32_t tDefaultVal) {
  if (!tStr || *tStr == '\0' || *tStr == ' ' || *tStr == '\t') return tDefaultVal;
  char *tEndPtr = nullptr;
  errno = 0;
  unsigned long tVal = strtoul(tStr, &tEndPtr, 10);
  if (errno == ERANGE || tEndPtr == tStr || *tEndPtr != '\0') return tDefaultVal;
  if (tVal < tMinVal || tVal > tMaxVal) return tDefaultVal;
  return static_cast<uint32_t>(tVal);
}

void ByteToReadableSize(uint64_t tBytes, char *tBuffer, size_t tLength) {
  if (tBytes < 1024ULL) snprintf(tBuffer, tLength, "%llu B", static_cast<unsigned long long>(tBytes));
  else if (tBytes < 1024ULL * 1024ULL) {
    float tSizeKB = static_cast<float>(tBytes) / 1024.0f;
    if (fabs(tSizeKB - (int)tSizeKB) < 0.01f) snprintf(tBuffer, tLength, "%d KB", (int)tSizeKB);
    else snprintf(tBuffer, tLength, "%.2f KB", tSizeKB);
  } else if (tBytes < 1024ULL * 1024ULL * 1024ULL) {
    float tSizeMB = static_cast<float>(tBytes) / (1024.0f * 1024.0f);
    if (fabs(tSizeMB - (int)tSizeMB) < 0.01f) snprintf(tBuffer, tLength, "%d MB", (int)tSizeMB);
    else snprintf(tBuffer, tLength, "%.2f MB", tSizeMB);
  } else {
    float tSizeGB = static_cast<float>(tBytes) / (1024.0f * 1024.0f * 1024.0f);
    if (fabs(tSizeGB - (int)tSizeGB) < 0.01f) snprintf(tBuffer, tLength, "%d GB", (int)tSizeGB);
    else snprintf(tBuffer, tLength, "%.2f GB", tSizeGB);
  }
}

bool WasWokenByPin(uint32_t tWakeCause, uint64_t tWakeStatus, uint8_t tPin) {
  static constexpr uint32_t kExt1WakeupCause = 3;
  if (tWakeCause != kExt1WakeupCause) return false;
  return (tWakeStatus & (1ULL << tPin)) != 0;
}

std::string NormalizeDashboardLanguageCode(const std::string &tLanguage) {
  std::string tNormalized = tLanguage;
  for (char &tChar : tNormalized) tChar = static_cast<char>(tolower(static_cast<unsigned char>(tChar)));
  if (tNormalized == "en" || tNormalized == "hu") return tNormalized;
  return std::string();
}

bool IsDashboardLanguageEnabled(const std::vector<std::string> &tLanguages, const std::string &tLanguage) {
  const std::string tNormalized = NormalizeDashboardLanguageCode(tLanguage);
  if (tNormalized.empty()) return false;
  for (const std::string &tItem : tLanguages) {
    if (NormalizeDashboardLanguageCode(tItem) == tNormalized) return true;
  }

  return false;
}

std::string ResolveDashboardLanguage(const std::vector<std::string> &tLanguages, const std::string &tPreferredLanguage) {
  const std::string tNormalizedPreferredLanguage = NormalizeDashboardLanguageCode(tPreferredLanguage);
  if (!tNormalizedPreferredLanguage.empty() && IsDashboardLanguageEnabled(tLanguages, tNormalizedPreferredLanguage)) return tNormalizedPreferredLanguage;
  if (IsDashboardLanguageEnabled(tLanguages, "en")) return std::string("en");
  for (const std::string &tLanguage : tLanguages) {
    const std::string tNormalizedLanguage = NormalizeDashboardLanguageCode(tLanguage);
    if (!tNormalizedLanguage.empty()) return tNormalizedLanguage;
  }

  return !tNormalizedPreferredLanguage.empty() ? tNormalizedPreferredLanguage : std::string("en");
}

void NormalizeDashboardEnabledLanguages(std::vector<std::string> &tLanguages, const std::string &tPreferredLanguage) {
  std::vector<std::string> tNormalizedLanguages;
  auto tAppendUnique = [&](const std::string &tLanguage) {
    const std::string tNormalizedLanguage = NormalizeDashboardLanguageCode(tLanguage);
    if (tNormalizedLanguage.empty()) return;
    for (const std::string &tExisting : tNormalizedLanguages) {
      if (tExisting == tNormalizedLanguage) return;
    }
    tNormalizedLanguages.push_back(tNormalizedLanguage);
  };

  for (const std::string &tLanguage : tLanguages) tAppendUnique(tLanguage);
  if (tNormalizedLanguages.empty()) tAppendUnique(ResolveDashboardLanguage(tNormalizedLanguages, tPreferredLanguage));
  if (tNormalizedLanguages.empty()) tAppendUnique("en");
  tLanguages = tNormalizedLanguages;
}

void test_SecureStrcmp_identical_strings() {
  TEST_ASSERT_TRUE(SecureStrcmp("hello", "hello"));
  TEST_ASSERT_TRUE(SecureStrcmp("password123", "password123"));
  TEST_ASSERT_TRUE(SecureStrcmp("", ""));
}

void test_SecureStrcmp_different_strings() {
  TEST_ASSERT_FALSE(SecureStrcmp("hello", "world"));
  TEST_ASSERT_FALSE(SecureStrcmp("password", "password1"));
  TEST_ASSERT_FALSE(SecureStrcmp("abc", "abd"));
}

void test_SecureStrcmp_different_lengths() {
  TEST_ASSERT_FALSE(SecureStrcmp("short", "longer_string"));
  TEST_ASSERT_FALSE(SecureStrcmp("test", "tes"));
  TEST_ASSERT_FALSE(SecureStrcmp("a", "aa"));
}

void test_SecureStrcmp_null_inputs() {
  TEST_ASSERT_FALSE(SecureStrcmp(nullptr, "hello"));
  TEST_ASSERT_FALSE(SecureStrcmp("hello", nullptr));
  TEST_ASSERT_FALSE(SecureStrcmp(nullptr, nullptr));
}

void test_SecureStrcmp_case_sensitive() {
  TEST_ASSERT_FALSE(SecureStrcmp("Hello", "hello"));
  TEST_ASSERT_FALSE(SecureStrcmp("ADMIN", "admin"));
}

void test_SafeAtoul_valid_numbers() {
  TEST_ASSERT_EQUAL_UINT32(100, SafeAtoul("100", 0, 1000, 0));
  TEST_ASSERT_EQUAL_UINT32(0, SafeAtoul("0", 0, 100, 50));
  TEST_ASSERT_EQUAL_UINT32(999, SafeAtoul("999", 0, 1000, 0));
}

void test_SafeAtoul_boundary_values() {
  TEST_ASSERT_EQUAL_UINT32(10, SafeAtoul("10", 10, 100, 50));
  TEST_ASSERT_EQUAL_UINT32(100, SafeAtoul("100", 10, 100, 50));
}

void test_SafeAtoul_out_of_range() {
  TEST_ASSERT_EQUAL_UINT32(50, SafeAtoul("5", 10, 100, 50));
  TEST_ASSERT_EQUAL_UINT32(50, SafeAtoul("200", 10, 100, 50));
}

void test_SafeAtoul_invalid_input() {
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul("", 0, 100, 99));
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul(nullptr, 0, 100, 99));
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul("abc", 0, 100, 99));
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul("12abc", 0, 100, 99));
}

void test_SafeAtoul_whitespace() {
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul(" 50", 0, 100, 99));
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul("50 ", 0, 100, 99));
  TEST_ASSERT_EQUAL_UINT32(99, SafeAtoul("\t50", 0, 100, 99));
}

void test_ByteToReadableSize_bytes() {
  char buffer[32];
  ByteToReadableSize(0, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("0 B", buffer);
  ByteToReadableSize(512, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("512 B", buffer);
  ByteToReadableSize(1023, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1023 B", buffer);
}

void test_ByteToReadableSize_kilobytes() {
  char buffer[32];
  ByteToReadableSize(1024, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1 KB", buffer);
  ByteToReadableSize(2048, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2 KB", buffer);
  ByteToReadableSize(1536, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1.50 KB", buffer);
}

void test_ByteToReadableSize_megabytes() {
  char buffer[32];
  ByteToReadableSize(1024 * 1024, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1 MB", buffer);
  ByteToReadableSize(2 * 1024 * 1024, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2 MB", buffer);
  ByteToReadableSize(1536 * 1024, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1.50 MB", buffer);
}

void test_ByteToReadableSize_large_values() {
  char buffer[32];
  ByteToReadableSize(16 * 1024 * 1024, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("16 MB", buffer);
}

void test_ByteToReadableSize_gigabytes() {
  char buffer[32];
  ByteToReadableSize(8ULL * 1024ULL * 1024ULL * 1024ULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("8 GB", buffer);
  ByteToReadableSize(7ULL * 1024ULL * 1024ULL * 1024ULL + 512ULL * 1024ULL * 1024ULL, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("7.50 GB", buffer);
}

void test_WasWokenByPin_ext1_and_matching_pin() {
  TEST_ASSERT_TRUE(WasWokenByPin(3, (1ULL << 4), 4));
  TEST_ASSERT_TRUE(WasWokenByPin(3, (1ULL << 0) | (1ULL << 5), 5));
}

void test_WasWokenByPin_ext1_but_other_pin() {
  TEST_ASSERT_FALSE(WasWokenByPin(3, (1ULL << 4), 5));
  TEST_ASSERT_FALSE(WasWokenByPin(3, 0, 4));
}

void test_WasWokenByPin_non_ext1_cause() {
  TEST_ASSERT_FALSE(WasWokenByPin(0, (1ULL << 4), 4));
  TEST_ASSERT_FALSE(WasWokenByPin(4, (1ULL << 5), 5));
}

static const unsigned long SECONDS_PER_MINUTE = 60;
static const unsigned long SECONDS_PER_HOUR = 3600;
static const unsigned long SECONDS_PER_DAY = 86400;

const char *EpochToReadableDuration(unsigned long tEpoch, char *tBuffer, size_t tLength) {
  if (!tBuffer || tLength == 0) return "";
  tBuffer[0] = '\0';
  if (tEpoch == 0) {
    strcpy(tBuffer, "0");
    return tBuffer;
  }

  if (tEpoch < SECONDS_PER_MINUTE) snprintf(tBuffer, tLength, "%lu sec", tEpoch);
  else if (tEpoch < SECONDS_PER_HOUR) snprintf(tBuffer, tLength, "%02lu:%02lu min", tEpoch / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
  else if (tEpoch < SECONDS_PER_DAY) snprintf(tBuffer, tLength, "%lu:%02lu:%02lu hour(s)", tEpoch / SECONDS_PER_HOUR, (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
  else snprintf(tBuffer, tLength, "%lu day(s) %02lu:%02lu:%02lu", tEpoch / SECONDS_PER_DAY, (tEpoch % SECONDS_PER_DAY) / SECONDS_PER_HOUR, (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
  return tBuffer;
}

void test_EpochToReadableDuration_zero() {
  char buffer[64];
  EpochToReadableDuration(0, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("0", buffer);
}

void test_EpochToReadableDuration_seconds() {
  char buffer[64];
  EpochToReadableDuration(1, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1 sec", buffer);
  EpochToReadableDuration(30, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("30 sec", buffer);
  EpochToReadableDuration(59, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("59 sec", buffer);
}

void test_EpochToReadableDuration_minutes() {
  char buffer[64];
  EpochToReadableDuration(60, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("01:00 min", buffer);
  EpochToReadableDuration(90, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("01:30 min", buffer);
  EpochToReadableDuration(3599, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("59:59 min", buffer);
}

void test_EpochToReadableDuration_hours() {
  char buffer[64];
  EpochToReadableDuration(3600, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1:00:00 hour(s)", buffer);
  EpochToReadableDuration(7200, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2:00:00 hour(s)", buffer);
  EpochToReadableDuration(3661, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1:01:01 hour(s)", buffer);
}

void test_EpochToReadableDuration_days() {
  char buffer[64];
  EpochToReadableDuration(86400, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1 day(s) 00:00:00", buffer);
  EpochToReadableDuration(172800, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("2 day(s) 00:00:00", buffer);
  EpochToReadableDuration(90061, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("1 day(s) 01:01:01", buffer);
}

void test_EpochToReadableDuration_null_buffer() {
  const char *result = EpochToReadableDuration(100, nullptr, 64);
  TEST_ASSERT_EQUAL_STRING("", result);
}

void test_EpochToReadableDuration_zero_length() {
  char buffer[64];
  const char *result = EpochToReadableDuration(100, buffer, 0);
  TEST_ASSERT_EQUAL_STRING("", result);
}

uint64_t SecondsUntilHour(uint8_t tTargetHour, uint8_t tNowHour, uint8_t tNowMinute, uint8_t tNowSecond) {
  uint32_t tNowSec = tNowHour * SECONDS_PER_HOUR + tNowMinute * SECONDS_PER_MINUTE + tNowSecond;
  uint32_t tTargetSec = tTargetHour * SECONDS_PER_HOUR;
  if (tTargetSec <= tNowSec) tTargetSec += SECONDS_PER_DAY;
  return tTargetSec - tNowSec;
}

bool HasElapsedMs(uint32_t tStartMs, uint32_t tNowMs, uint32_t tDelayMs) {
  return static_cast<uint32_t>(tNowMs - tStartMs) >= tDelayMs;
}

void test_SecondsUntilHour_target_in_future() {
  TEST_ASSERT_EQUAL_UINT32(6 * 3600, (uint32_t)SecondsUntilHour(10, 4, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(3600, (uint32_t)SecondsUntilHour(15, 14, 0, 0));
}

void test_SecondsUntilHour_target_passed_wraps() {
  TEST_ASSERT_EQUAL_UINT32(20 * 3600, (uint32_t)SecondsUntilHour(6, 10, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(23 * 3600, (uint32_t)SecondsUntilHour(0, 1, 0, 0));
}

void test_SecondsUntilHour_same_hour_wraps() {
  TEST_ASSERT_EQUAL_UINT32(SECONDS_PER_DAY - 1800, (uint32_t)SecondsUntilHour(6, 6, 30, 0));
  TEST_ASSERT_EQUAL_UINT32(SECONDS_PER_DAY, (uint32_t)SecondsUntilHour(6, 6, 0, 0));
}

void test_SecondsUntilHour_midnight_edge() {
  TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)SecondsUntilHour(0, 23, 59, 59));
  TEST_ASSERT_EQUAL_UINT32(SECONDS_PER_DAY, (uint32_t)SecondsUntilHour(0, 0, 0, 0));
}

void test_SecondsUntilHour_hour_23() {
  TEST_ASSERT_EQUAL_UINT32(3600, (uint32_t)SecondsUntilHour(23, 22, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(23 * 3600 + 1800, (uint32_t)SecondsUntilHour(23, 23, 30, 0));
}

void test_SecondsUntilHour_with_minutes_seconds() {
  TEST_ASSERT_EQUAL_UINT32(5 * 3600 + 1800, (uint32_t)SecondsUntilHour(10, 4, 30, 0));
  TEST_ASSERT_EQUAL_UINT32(5 * 3600 + 1800 - 45, (uint32_t)SecondsUntilHour(10, 4, 30, 45));
}

void test_HasElapsedMs_not_elapsed_yet() {
  TEST_ASSERT_FALSE(HasElapsedMs(1000, 1200, 500));
}

void test_HasElapsedMs_elapsed_normally() {
  TEST_ASSERT_TRUE(HasElapsedMs(1000, 1600, 500));
}

void test_HasElapsedMs_handles_wraparound() {
  TEST_ASSERT_TRUE(HasElapsedMs(0xFFFFFF00UL, 0x00000100UL, 300));
  TEST_ASSERT_FALSE(HasElapsedMs(0xFFFFFF00UL, 0x00000020UL, 300));
}

bool GlobMatch(const char *tPattern, const char *tText) {
  while (*tPattern) {
    if (*tPattern == '*') {
      ++tPattern;
      if (!*tPattern) return true;
      while (*tText) {
        if (GlobMatch(tPattern, tText)) return true;
        ++tText;
      }
      return false;
    }
    if (tolower((unsigned char)*tPattern) != tolower((unsigned char)*tText)) return false;
    ++tPattern;
    ++tText;
  }

  return *tText == '\0';
}

void test_GlobMatch_exact() {
  TEST_ASSERT_TRUE(GlobMatch("hello.jpg", "hello.jpg"));
  TEST_ASSERT_FALSE(GlobMatch("hello.jpg", "hello.png"));
}

void test_GlobMatch_star_suffix() {
  TEST_ASSERT_TRUE(GlobMatch("*.jpg", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("*.jpg", "a.jpg"));
  TEST_ASSERT_FALSE(GlobMatch("*.jpg", "photo.png"));
}

void test_GlobMatch_star_prefix() {
  TEST_ASSERT_TRUE(GlobMatch("photo*", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("photo*", "photo123.png"));
  TEST_ASSERT_FALSE(GlobMatch("photo*", "image.jpg"));
}

void test_GlobMatch_star_middle() {
  TEST_ASSERT_TRUE(GlobMatch("p*.jpg", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("p*.jpg", "p.jpg"));
  TEST_ASSERT_FALSE(GlobMatch("p*.png", "photo.jpg"));
}

void test_GlobMatch_star_only() {
  TEST_ASSERT_TRUE(GlobMatch("*", "anything"));
  TEST_ASSERT_TRUE(GlobMatch("*", ""));
}

void test_GlobMatch_case_insensitive() {
  TEST_ASSERT_TRUE(GlobMatch("*.JPG", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("PHOTO*", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("*.Jpg", "IMAGE.jPg"));
}

void test_GlobMatch_no_match() {
  TEST_ASSERT_FALSE(GlobMatch("abc", "abcd"));
  TEST_ASSERT_FALSE(GlobMatch("abcd", "abc"));
  TEST_ASSERT_FALSE(GlobMatch("", "text"));
}

void test_GlobMatch_empty() {
  TEST_ASSERT_TRUE(GlobMatch("", ""));
  TEST_ASSERT_FALSE(GlobMatch("", "x"));
}

void test_GlobMatch_multiple_stars() {
  TEST_ASSERT_TRUE(GlobMatch("*o*", "photo.jpg"));
  TEST_ASSERT_TRUE(GlobMatch("*h*j*", "photo.jpg"));
  TEST_ASSERT_FALSE(GlobMatch("*z*", "photo.jpg"));
}

bool IsSD(const char *tTarget) {
  return strcasecmp(tTarget, "sd") == 0 || strcasecmp(tTarget, "sdcard") == 0;
}

bool IsLFS(const char *tTarget) {
  return strcasecmp(tTarget, "lfs") == 0 || strcasecmp(tTarget, "littlefs") == 0;
}

bool IsValidTarget(const char *tTarget) {
  return IsSD(tTarget) || IsLFS(tTarget);
}

bool IsSameTarget(const char *tA, const char *tB) {
  return (IsSD(tA) && IsSD(tB)) || (IsLFS(tA) && IsLFS(tB));
}

void test_IsSD() {
  TEST_ASSERT_TRUE(IsSD("sd"));
  TEST_ASSERT_TRUE(IsSD("SD"));
  TEST_ASSERT_TRUE(IsSD("sdcard"));
  TEST_ASSERT_TRUE(IsSD("SDCard"));
  TEST_ASSERT_FALSE(IsSD("lfs"));
  TEST_ASSERT_FALSE(IsSD("sd2"));
  TEST_ASSERT_FALSE(IsSD(""));
}

void test_IsLFS() {
  TEST_ASSERT_TRUE(IsLFS("lfs"));
  TEST_ASSERT_TRUE(IsLFS("LFS"));
  TEST_ASSERT_TRUE(IsLFS("littlefs"));
  TEST_ASSERT_TRUE(IsLFS("LittleFS"));
  TEST_ASSERT_FALSE(IsLFS("sd"));
  TEST_ASSERT_FALSE(IsLFS("lfs2"));
  TEST_ASSERT_FALSE(IsLFS(""));
}

void test_IsValidTarget() {
  TEST_ASSERT_TRUE(IsValidTarget("sd"));
  TEST_ASSERT_TRUE(IsValidTarget("lfs"));
  TEST_ASSERT_TRUE(IsValidTarget("sdcard"));
  TEST_ASSERT_TRUE(IsValidTarget("littlefs"));
  TEST_ASSERT_FALSE(IsValidTarget("usb"));
  TEST_ASSERT_FALSE(IsValidTarget(""));
}

void test_IsSameTarget() {
  TEST_ASSERT_TRUE(IsSameTarget("sd", "sdcard"));
  TEST_ASSERT_TRUE(IsSameTarget("lfs", "littlefs"));
  TEST_ASSERT_TRUE(IsSameTarget("SD", "sd"));
  TEST_ASSERT_FALSE(IsSameTarget("sd", "lfs"));
  TEST_ASSERT_FALSE(IsSameTarget("lfs", "sdcard"));
}

void test_NormalizeDashboardEnabledLanguages_keeps_user_disabled_current_language_off() {
  std::vector<std::string> tLanguages = { "en" };
  NormalizeDashboardEnabledLanguages(tLanguages, "hu");
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(tLanguages.size()));
  TEST_ASSERT_TRUE(IsDashboardLanguageEnabled(tLanguages, "en"));
  TEST_ASSERT_FALSE(IsDashboardLanguageEnabled(tLanguages, "hu"));
}

void test_NormalizeDashboardEnabledLanguages_preserves_single_language_mode() {
  std::vector<std::string> tLanguages = { "hu" };
  NormalizeDashboardEnabledLanguages(tLanguages, "hu");
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(tLanguages.size()));
  TEST_ASSERT_TRUE(IsDashboardLanguageEnabled(tLanguages, "hu"));
  TEST_ASSERT_FALSE(IsDashboardLanguageEnabled(tLanguages, "en"));
}

static const char *kImagesDir = "images";

bool SplitPathAndFile(const char *tSpec, char *tDir, size_t tDirSize, char *tFile, size_t tFileSize) {
  if (!tSpec || *tSpec == '\0') return false;
  const char *tEnd = tSpec + strlen(tSpec);
  while (tEnd > tSpec && (*(tEnd - 1) == ' ' || *(tEnd - 1) == '\t')) --tEnd;
  if (tEnd <= tSpec) return false;
  if (*tSpec == '/') {
    const char *tLastSlash = tSpec;
    for (const char *tC = tSpec + 1; tC < tEnd; ++tC) {
      if (*tC == '/') tLastSlash = tC;
    }
    const char *tAfter = tLastSlash + 1;
    size_t tAfterLen = tEnd - tAfter;
    if (tAfterLen == 0) return false;
    size_t tDirLen = tLastSlash - tSpec;
    if (tDirLen == 0) {
      tDir[0] = '/';
      tDir[1] = '\0';
    } else {
      if (tDirLen >= tDirSize) tDirLen = tDirSize - 1;
      strncpy(tDir, tSpec, tDirLen);
      tDir[tDirLen] = '\0';
    }
    if (tAfterLen >= tFileSize) tAfterLen = tFileSize - 1;
    strncpy(tFile, tAfter, tAfterLen);
    tFile[tAfterLen] = '\0';
  } else {
    snprintf(tDir, tDirSize, "/%s", kImagesDir);
    size_t tLen = tEnd - tSpec;
    if (tLen >= tFileSize) tLen = tFileSize - 1;
    strncpy(tFile, tSpec, tLen);
    tFile[tLen] = '\0';
  }

  return true;
}

const char *GetLeafName(const char *tPath) {
  if (!tPath) return nullptr;
  const char *tSlash = strrchr(tPath, '/');
  return tSlash ? tSlash + 1 : tPath;
}

bool EqualsIgnoreCase(const char *tLeft, const char *tRight) {
  if (!tLeft || !tRight) return false;
  while (*tLeft && *tRight) {
    if (tolower((unsigned char)*tLeft) != tolower((unsigned char)*tRight)) return false;
    ++tLeft;
    ++tRight;
  }

  return *tLeft == '\0' && *tRight == '\0';
}

void test_GetLeafName_simple_path() {
  TEST_ASSERT_EQUAL_STRING("photo.jpg", GetLeafName("/images/photo.jpg"));
}

void test_GetLeafName_deep_path() {
  TEST_ASSERT_EQUAL_STRING("file.txt", GetLeafName("/a/b/c/file.txt"));
}

void test_GetLeafName_root_file() {
  TEST_ASSERT_EQUAL_STRING("config.ini", GetLeafName("/config.ini"));
}

void test_GetLeafName_no_slash() {
  TEST_ASSERT_EQUAL_STRING("plain.jpg", GetLeafName("plain.jpg"));
}

void test_GetLeafName_trailing_slash() {
  TEST_ASSERT_EQUAL_STRING("", GetLeafName("/images/"));
}

void test_GetLeafName_null() {
  TEST_ASSERT_NULL(GetLeafName(nullptr));
}

void test_GetLeafName_only_slash() {
  TEST_ASSERT_EQUAL_STRING("", GetLeafName("/"));
}

void test_EqualsIgnoreCase_identical() {
  TEST_ASSERT_TRUE(EqualsIgnoreCase("photo.jpg", "photo.jpg"));
}

void test_EqualsIgnoreCase_mixed_case() {
  TEST_ASSERT_TRUE(EqualsIgnoreCase("Photo.JPG", "photo.jpg"));
  TEST_ASSERT_TRUE(EqualsIgnoreCase("ABC", "abc"));
  TEST_ASSERT_TRUE(EqualsIgnoreCase("001pic.JPG", "001pic.jpg"));
}

void test_EqualsIgnoreCase_different_strings() {
  TEST_ASSERT_FALSE(EqualsIgnoreCase("photo.jpg", "other.jpg"));
  TEST_ASSERT_FALSE(EqualsIgnoreCase("abc", "abcd"));
  TEST_ASSERT_FALSE(EqualsIgnoreCase("abcd", "abc"));
}

void test_EqualsIgnoreCase_empty_strings() {
  TEST_ASSERT_TRUE(EqualsIgnoreCase("", ""));
}

void test_EqualsIgnoreCase_null_inputs() {
  TEST_ASSERT_FALSE(EqualsIgnoreCase(nullptr, "photo.jpg"));
  TEST_ASSERT_FALSE(EqualsIgnoreCase("photo.jpg", nullptr));
  TEST_ASSERT_FALSE(EqualsIgnoreCase(nullptr, nullptr));
}

void test_EqualsIgnoreCase_one_empty() {
  TEST_ASSERT_FALSE(EqualsIgnoreCase("", "photo.jpg"));
  TEST_ASSERT_FALSE(EqualsIgnoreCase("photo.jpg", ""));
}

void test_SplitPathAndFile_absolute_path() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("/images/photo.jpg", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/images", tDir);
  TEST_ASSERT_EQUAL_STRING("photo.jpg", tFile);
}

void test_SplitPathAndFile_root_file() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("/config.ini", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/", tDir);
  TEST_ASSERT_EQUAL_STRING("config.ini", tFile);
}

void test_SplitPathAndFile_deep_path() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("/a/b/c/file.txt", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/a/b/c", tDir);
  TEST_ASSERT_EQUAL_STRING("file.txt", tFile);
}

void test_SplitPathAndFile_relative_defaults_images() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("photo.jpg", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/images", tDir);
  TEST_ASSERT_EQUAL_STRING("photo.jpg", tFile);
}

void test_SplitPathAndFile_trailing_space() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("photo.jpg   ", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/images", tDir);
  TEST_ASSERT_EQUAL_STRING("photo.jpg", tFile);
}

void test_SplitPathAndFile_null_empty() {
  char tDir[64], tFile[64];
  TEST_ASSERT_FALSE(SplitPathAndFile(nullptr, tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_FALSE(SplitPathAndFile("", tDir, sizeof(tDir), tFile, sizeof(tFile)));
}

void test_SplitPathAndFile_dir_only() {
  char tDir[64], tFile[64];
  TEST_ASSERT_FALSE(SplitPathAndFile("/images/", tDir, sizeof(tDir), tFile, sizeof(tFile)));
}

void test_SplitPathAndFile_glob_pattern() {
  char tDir[64], tFile[64];
  TEST_ASSERT_TRUE(SplitPathAndFile("/images/*.jpg", tDir, sizeof(tDir), tFile, sizeof(tFile)));
  TEST_ASSERT_EQUAL_STRING("/images", tDir);
  TEST_ASSERT_EQUAL_STRING("*.jpg", tFile);
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_SecureStrcmp_identical_strings);
  RUN_TEST(test_SecureStrcmp_different_strings);
  RUN_TEST(test_SecureStrcmp_different_lengths);
  RUN_TEST(test_SecureStrcmp_null_inputs);
  RUN_TEST(test_SecureStrcmp_case_sensitive);
  RUN_TEST(test_SafeAtoul_valid_numbers);
  RUN_TEST(test_SafeAtoul_boundary_values);
  RUN_TEST(test_SafeAtoul_out_of_range);
  RUN_TEST(test_SafeAtoul_invalid_input);
  RUN_TEST(test_SafeAtoul_whitespace);
  RUN_TEST(test_ByteToReadableSize_bytes);
  RUN_TEST(test_ByteToReadableSize_kilobytes);
  RUN_TEST(test_ByteToReadableSize_megabytes);
  RUN_TEST(test_ByteToReadableSize_large_values);
  RUN_TEST(test_ByteToReadableSize_gigabytes);
  RUN_TEST(test_WasWokenByPin_ext1_and_matching_pin);
  RUN_TEST(test_WasWokenByPin_ext1_but_other_pin);
  RUN_TEST(test_WasWokenByPin_non_ext1_cause);
  RUN_TEST(test_EpochToReadableDuration_zero);
  RUN_TEST(test_EpochToReadableDuration_seconds);
  RUN_TEST(test_EpochToReadableDuration_minutes);
  RUN_TEST(test_EpochToReadableDuration_hours);
  RUN_TEST(test_EpochToReadableDuration_days);
  RUN_TEST(test_EpochToReadableDuration_null_buffer);
  RUN_TEST(test_EpochToReadableDuration_zero_length);
  RUN_TEST(test_SecondsUntilHour_target_in_future);
  RUN_TEST(test_SecondsUntilHour_target_passed_wraps);
  RUN_TEST(test_SecondsUntilHour_same_hour_wraps);
  RUN_TEST(test_SecondsUntilHour_midnight_edge);
  RUN_TEST(test_SecondsUntilHour_hour_23);
  RUN_TEST(test_SecondsUntilHour_with_minutes_seconds);
  RUN_TEST(test_HasElapsedMs_not_elapsed_yet);
  RUN_TEST(test_HasElapsedMs_elapsed_normally);
  RUN_TEST(test_HasElapsedMs_handles_wraparound);
  RUN_TEST(test_GlobMatch_exact);
  RUN_TEST(test_GlobMatch_star_suffix);
  RUN_TEST(test_GlobMatch_star_prefix);
  RUN_TEST(test_GlobMatch_star_middle);
  RUN_TEST(test_GlobMatch_star_only);
  RUN_TEST(test_GlobMatch_case_insensitive);
  RUN_TEST(test_GlobMatch_no_match);
  RUN_TEST(test_GlobMatch_empty);
  RUN_TEST(test_GlobMatch_multiple_stars);
  RUN_TEST(test_IsSD);
  RUN_TEST(test_IsLFS);
  RUN_TEST(test_IsValidTarget);
  RUN_TEST(test_IsSameTarget);
  RUN_TEST(test_NormalizeDashboardEnabledLanguages_keeps_user_disabled_current_language_off);
  RUN_TEST(test_NormalizeDashboardEnabledLanguages_preserves_single_language_mode);
  RUN_TEST(test_SplitPathAndFile_absolute_path);
  RUN_TEST(test_SplitPathAndFile_root_file);
  RUN_TEST(test_SplitPathAndFile_deep_path);
  RUN_TEST(test_SplitPathAndFile_relative_defaults_images);
  RUN_TEST(test_SplitPathAndFile_trailing_space);
  RUN_TEST(test_SplitPathAndFile_null_empty);
  RUN_TEST(test_SplitPathAndFile_dir_only);
  RUN_TEST(test_SplitPathAndFile_glob_pattern);
  RUN_TEST(test_GetLeafName_simple_path);
  RUN_TEST(test_GetLeafName_deep_path);
  RUN_TEST(test_GetLeafName_root_file);
  RUN_TEST(test_GetLeafName_no_slash);
  RUN_TEST(test_GetLeafName_trailing_slash);
  RUN_TEST(test_GetLeafName_null);
  RUN_TEST(test_GetLeafName_only_slash);
  RUN_TEST(test_EqualsIgnoreCase_identical);
  RUN_TEST(test_EqualsIgnoreCase_mixed_case);
  RUN_TEST(test_EqualsIgnoreCase_different_strings);
  RUN_TEST(test_EqualsIgnoreCase_empty_strings);
  RUN_TEST(test_EqualsIgnoreCase_null_inputs);
  RUN_TEST(test_EqualsIgnoreCase_one_empty);
  return UNITY_END();
}
