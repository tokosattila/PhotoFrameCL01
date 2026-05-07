#include <unity.h>
#include <cstdint>
#include <string>

static bool IsDefaultStorageAvailable(bool tSdAvailable, bool tLfsAvailable, const std::string &tDefaultKey) {
  if (tDefaultKey == "sd_card") return tSdAvailable;
  return tLfsAvailable;
}

static std::string ResolveFallbackField(const std::string &tStorageKey, const std::string &tDefaultKey, bool tDefaultAvailable) {
  const bool tIsEffectivePrimary = (tStorageKey == tDefaultKey) || !tDefaultAvailable;
  return tIsEffectivePrimary ? "false" : "true";
}

static bool ResolveDefaultField(const std::string &tStorageKey, const std::string &tDefaultKey, bool tDefaultAvailable, const std::string &tFileName, const std::string &tCurrentFile) {
  const bool tIsEffectivePrimary = (tStorageKey == tDefaultKey) || !tDefaultAvailable;
  return tIsEffectivePrimary && !tCurrentFile.empty() && (tFileName == tCurrentFile);
}

void test_Fallback_sd_is_default_sd_available_sd_is_not_fallback() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "sd_card");
  TEST_ASSERT_EQUAL_STRING("false", ResolveFallbackField("sd_card", "sd_card", tAvail).c_str());
}

void test_Fallback_sd_is_default_sd_available_lfs_is_fallback() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "sd_card");
  TEST_ASSERT_EQUAL_STRING("true", ResolveFallbackField("littlefs", "sd_card", tAvail).c_str());
}

void test_Fallback_sd_is_default_sd_absent_lfs_is_not_fallback() {
  bool tAvail = IsDefaultStorageAvailable(false, true, "sd_card");
  TEST_ASSERT_EQUAL_STRING("false", ResolveFallbackField("littlefs", "sd_card", tAvail).c_str());
}

void test_Fallback_lfs_is_default_lfs_available_lfs_is_not_fallback() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "littlefs");
  TEST_ASSERT_EQUAL_STRING("false", ResolveFallbackField("littlefs", "littlefs", tAvail).c_str());
}

void test_Fallback_lfs_is_default_lfs_available_sd_is_fallback() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "littlefs");
  TEST_ASSERT_EQUAL_STRING("true", ResolveFallbackField("sd_card", "littlefs", tAvail).c_str());
}

void test_Fallback_lfs_is_default_lfs_absent_sd_is_not_fallback() {
  bool tAvail = IsDefaultStorageAvailable(true, false, "littlefs");
  TEST_ASSERT_EQUAL_STRING("false", ResolveFallbackField("sd_card", "littlefs", tAvail).c_str());
}

void test_Default_sd_primary_matching_current_file_is_default() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "sd_card");
  TEST_ASSERT_TRUE(ResolveDefaultField("sd_card", "sd_card", tAvail, "img001.jpg", "img001.jpg"));
}

void test_Default_sd_primary_non_matching_file_is_not_default() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "sd_card");
  TEST_ASSERT_FALSE(ResolveDefaultField("sd_card", "sd_card", tAvail, "img002.jpg", "img001.jpg"));
}

void test_Default_lfs_fallback_file_cannot_be_default() {
  bool tAvail = IsDefaultStorageAvailable(true, true, "sd_card");
  TEST_ASSERT_FALSE(ResolveDefaultField("littlefs", "sd_card", tAvail, "img001.jpg", "img001.jpg"));
}

void test_Default_sd_absent_lfs_matches_current_file_is_default() {
  bool tAvail = IsDefaultStorageAvailable(false, true, "sd_card");
  TEST_ASSERT_TRUE(ResolveDefaultField("littlefs", "sd_card", tAvail, "img001.jpg", "img001.jpg"));
}

void test_Default_sd_absent_lfs_non_matching_file_is_not_default() {
  bool tAvail = IsDefaultStorageAvailable(false, true, "sd_card");
  TEST_ASSERT_FALSE(ResolveDefaultField("littlefs", "sd_card", tAvail, "img002.jpg", "img001.jpg"));
}

void test_Default_sd_absent_lfs_empty_current_file_is_not_default() {
  bool tAvail = IsDefaultStorageAvailable(false, true, "sd_card");
  TEST_ASSERT_FALSE(ResolveDefaultField("littlefs", "sd_card", tAvail, "img001.jpg", ""));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_Fallback_sd_is_default_sd_available_sd_is_not_fallback);
  RUN_TEST(test_Fallback_sd_is_default_sd_available_lfs_is_fallback);
  RUN_TEST(test_Fallback_sd_is_default_sd_absent_lfs_is_not_fallback);
  RUN_TEST(test_Fallback_lfs_is_default_lfs_available_lfs_is_not_fallback);
  RUN_TEST(test_Fallback_lfs_is_default_lfs_available_sd_is_fallback);
  RUN_TEST(test_Fallback_lfs_is_default_lfs_absent_sd_is_not_fallback);
  RUN_TEST(test_Default_sd_primary_matching_current_file_is_default);
  RUN_TEST(test_Default_sd_primary_non_matching_file_is_not_default);
  RUN_TEST(test_Default_lfs_fallback_file_cannot_be_default);
  RUN_TEST(test_Default_sd_absent_lfs_matches_current_file_is_default);
  RUN_TEST(test_Default_sd_absent_lfs_non_matching_file_is_not_default);
  RUN_TEST(test_Default_sd_absent_lfs_empty_current_file_is_not_default);
  return UNITY_END();
}
