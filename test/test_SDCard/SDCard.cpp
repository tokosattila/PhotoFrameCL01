/**
 * @file SDCard.cpp
 * @brief Unit tests for SDCard/LittleFS path functions (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// Standalone implementations for testing (extracted from SDCard.cpp/LittleFS.cpp)
// ============================================================================

static char sNormalizedPath[128] = {0};

const char *NormalizePath(const char *tPath) {
  if (!tPath || tPath[0] == '\0') strcpy(sNormalizedPath, "/");
  else if (tPath[0] == '/') strncpy(sNormalizedPath, tPath, sizeof(sNormalizedPath) - 1);
  else snprintf(sNormalizedPath, sizeof(sNormalizedPath), "/%s", tPath);
  sNormalizedPath[sizeof(sNormalizedPath)-1] = '\0';
  return sNormalizedPath;
}

const char *GetFileName(const char *tPath) {
  const char *tName = strrchr(tPath, '/');
  return tName ? tName + 1 : tPath;
}

const char *PrependSlash(const char *tPath, char *tOutBuffer, size_t tBufSize) {
  if (!tPath || !tOutBuffer || tBufSize == 0) return "";
  if (tPath[0] == '/') {
    strncpy(tOutBuffer, tPath, tBufSize - 1);
  } else {
    snprintf(tOutBuffer, tBufSize, "/%s", tPath);
  }
  tOutBuffer[tBufSize - 1] = '\0';
  return tOutBuffer;
}

// ============================================================================
// NormalizePath Tests
// ============================================================================

void test_NormalizePath_empty_string() {
  const char *result = NormalizePath("");
  TEST_ASSERT_EQUAL_STRING("/", result);
}

void test_NormalizePath_null_input() {
  const char *result = NormalizePath(nullptr);
  TEST_ASSERT_EQUAL_STRING("/", result);
}

void test_NormalizePath_already_absolute() {
  const char *result = NormalizePath("/images");
  TEST_ASSERT_EQUAL_STRING("/images", result);
}

void test_NormalizePath_relative_path() {
  const char *result = NormalizePath("images");
  TEST_ASSERT_EQUAL_STRING("/images", result);
}

void test_NormalizePath_deep_path() {
  const char *result = NormalizePath("/images/photos/2024");
  TEST_ASSERT_EQUAL_STRING("/images/photos/2024", result);
}

void test_NormalizePath_relative_deep() {
  const char *result = NormalizePath("images/photos");
  TEST_ASSERT_EQUAL_STRING("/images/photos", result);
}

void test_NormalizePath_root() {
  const char *result = NormalizePath("/");
  TEST_ASSERT_EQUAL_STRING("/", result);
}

void test_NormalizePath_filename() {
  const char *result = NormalizePath("config.ini");
  TEST_ASSERT_EQUAL_STRING("/config.ini", result);
}

void test_NormalizePath_absolute_filename() {
  const char *result = NormalizePath("/config.ini");
  TEST_ASSERT_EQUAL_STRING("/config.ini", result);
}

// ============================================================================
// GetFileName Tests
// ============================================================================

void test_GetFileName_simple_path() {
  const char *result = GetFileName("/images/pic_01.jpg");
  TEST_ASSERT_EQUAL_STRING("pic_01.jpg", result);
}

void test_GetFileName_deep_path() {
  const char *result = GetFileName("/images/2024/vacation/photo.jpg");
  TEST_ASSERT_EQUAL_STRING("photo.jpg", result);
}

void test_GetFileName_root_file() {
  const char *result = GetFileName("/config.ini");
  TEST_ASSERT_EQUAL_STRING("config.ini", result);
}

void test_GetFileName_no_slash() {
  const char *result = GetFileName("filename.txt");
  TEST_ASSERT_EQUAL_STRING("filename.txt", result);
}

void test_GetFileName_directory_trailing_slash() {
  const char *result = GetFileName("/images/");
  TEST_ASSERT_EQUAL_STRING("", result);
}

void test_GetFileName_only_filename() {
  const char *result = GetFileName("test.jpg");
  TEST_ASSERT_EQUAL_STRING("test.jpg", result);
}

void test_GetFileName_multiple_dots() {
  const char *result = GetFileName("/path/file.name.ext");
  TEST_ASSERT_EQUAL_STRING("file.name.ext", result);
}

// ============================================================================
// PrependSlash Tests
// ============================================================================

void test_PrependSlash_relative_path() {
  char buffer[64];
  const char *result = PrependSlash("images", buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("/images", result);
  TEST_ASSERT_EQUAL_STRING("/images", buffer);
}

void test_PrependSlash_already_absolute() {
  char buffer[64];
  const char *result = PrependSlash("/images", buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("/images", result);
}

void test_PrependSlash_filename() {
  char buffer[64];
  const char *result = PrependSlash("config.ini", buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("/config.ini", result);
}

void test_PrependSlash_null_path() {
  char buffer[64];
  const char *result = PrependSlash(nullptr, buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("", result);
}

void test_PrependSlash_null_buffer() {
  const char *result = PrependSlash("images", nullptr, 64);
  TEST_ASSERT_EQUAL_STRING("", result);
}

void test_PrependSlash_zero_buffer() {
  char buffer[64];
  const char *result = PrependSlash("images", buffer, 0);
  TEST_ASSERT_EQUAL_STRING("", result);
}

void test_PrependSlash_small_buffer() {
  char buffer[8];
  const char *result = PrependSlash("images", buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("/images", result);
}

void test_PrependSlash_empty_path() {
  char buffer[64];
  const char *result = PrependSlash("", buffer, sizeof(buffer));
  TEST_ASSERT_EQUAL_STRING("/", result);
}

// ============================================================================
// Extension Checking (helper for tests)
// ============================================================================

bool HasJpgExtension(const char *tFilename) {
  if (!tFilename) return false;
  const char *tExt = strrchr(tFilename, '.');
  if (!tExt) return false;
  return (strcasecmp(tExt, ".jpg") == 0 || strcasecmp(tExt, ".jpeg") == 0);
}

// ============================================================================
// Circular Index Helper (extracted from GetNextFile logic)
// ============================================================================

size_t GetNextIndex(size_t tCurrentIndex, size_t tTotalCount) {
  if (tTotalCount == 0) return 0;
  return (tCurrentIndex + 1) % tTotalCount;
}

int FindFileIndex(const char *tFilename, const char **tFileList, size_t tCount) {
  if (!tFilename || !tFileList || tCount == 0) return -1;
  for (size_t i = 0; i < tCount; ++i) {
    if (tFileList[i] && strcmp(tFilename, tFileList[i]) == 0) {
      return (int)i;
    }
  }
  return -1;
}

// IsFile helper - checks if name has 3-char extension
bool IsFile(const char *tName) {
  if (!tName || tName[0] == '\0') return false;
  const char *tDot = strrchr(tName, '.');
  if (tDot && strlen(tDot + 1) == 3) return true;
  return false;
}

void test_HasJpgExtension_jpg() {
  TEST_ASSERT_TRUE(HasJpgExtension("photo.jpg"));
  TEST_ASSERT_TRUE(HasJpgExtension("photo.JPG"));
  TEST_ASSERT_TRUE(HasJpgExtension("photo.Jpg"));
}

void test_HasJpgExtension_jpeg() {
  TEST_ASSERT_TRUE(HasJpgExtension("photo.jpeg"));
  TEST_ASSERT_TRUE(HasJpgExtension("photo.JPEG"));
}

void test_HasJpgExtension_other() {
  TEST_ASSERT_FALSE(HasJpgExtension("photo.png"));
  TEST_ASSERT_FALSE(HasJpgExtension("photo.gif"));
  TEST_ASSERT_FALSE(HasJpgExtension("photo.bmp"));
}

void test_HasJpgExtension_no_extension() {
  TEST_ASSERT_FALSE(HasJpgExtension("photo"));
  TEST_ASSERT_FALSE(HasJpgExtension(""));
}

void test_HasJpgExtension_null() {
  TEST_ASSERT_FALSE(HasJpgExtension(nullptr));
}

void test_HasJpgExtension_path_with_extension() {
  TEST_ASSERT_TRUE(HasJpgExtension("/images/pic_01.jpg"));
  TEST_ASSERT_FALSE(HasJpgExtension("/images/pic_01.png"));
}

// ============================================================================
// GetNextIndex Tests (circular iteration)
// ============================================================================

void test_GetNextIndex_simple() {
  TEST_ASSERT_EQUAL_UINT(1, GetNextIndex(0, 5));
  TEST_ASSERT_EQUAL_UINT(2, GetNextIndex(1, 5));
  TEST_ASSERT_EQUAL_UINT(3, GetNextIndex(2, 5));
}

void test_GetNextIndex_wrap_around() {
  TEST_ASSERT_EQUAL_UINT(0, GetNextIndex(4, 5));  // 4 -> 0 (wrap)
  TEST_ASSERT_EQUAL_UINT(0, GetNextIndex(2, 3));  // 2 -> 0 (wrap)
}

void test_GetNextIndex_single_element() {
  TEST_ASSERT_EQUAL_UINT(0, GetNextIndex(0, 1));  // Always stays at 0
}

void test_GetNextIndex_empty() {
  TEST_ASSERT_EQUAL_UINT(0, GetNextIndex(0, 0));
}

// ============================================================================
// FindFileIndex Tests
// ============================================================================

void test_FindFileIndex_found() {
  const char *files[] = {"pic_01.jpg", "pic_02.jpg", "pic_03.jpg"};
  TEST_ASSERT_EQUAL_INT(0, FindFileIndex("pic_01.jpg", files, 3));
  TEST_ASSERT_EQUAL_INT(1, FindFileIndex("pic_02.jpg", files, 3));
  TEST_ASSERT_EQUAL_INT(2, FindFileIndex("pic_03.jpg", files, 3));
}

void test_FindFileIndex_not_found() {
  const char *files[] = {"pic_01.jpg", "pic_02.jpg", "pic_03.jpg"};
  TEST_ASSERT_EQUAL_INT(-1, FindFileIndex("pic_04.jpg", files, 3));
  TEST_ASSERT_EQUAL_INT(-1, FindFileIndex("other.png", files, 3));
}

void test_FindFileIndex_null_input() {
  const char *files[] = {"pic_01.jpg"};
  TEST_ASSERT_EQUAL_INT(-1, FindFileIndex(nullptr, files, 1));
  TEST_ASSERT_EQUAL_INT(-1, FindFileIndex("pic_01.jpg", nullptr, 1));
}

void test_FindFileIndex_empty_list() {
  const char *files[] = {};
  TEST_ASSERT_EQUAL_INT(-1, FindFileIndex("pic_01.jpg", files, 0));
}

// ============================================================================
// IsFile Tests
// ============================================================================

void test_IsFile_valid_extensions() {
  TEST_ASSERT_TRUE(IsFile("photo.jpg"));
  TEST_ASSERT_TRUE(IsFile("config.ini"));
  TEST_ASSERT_TRUE(IsFile("data.txt"));
}

void test_IsFile_invalid_extensions() {
  TEST_ASSERT_FALSE(IsFile("photo.jpeg"));  // 4 chars
  TEST_ASSERT_FALSE(IsFile("readme.md"));   // 2 chars
  TEST_ASSERT_FALSE(IsFile("file.a"));      // 1 char
}

void test_IsFile_no_extension() {
  TEST_ASSERT_FALSE(IsFile("README"));
  TEST_ASSERT_FALSE(IsFile("Makefile"));
}

void test_IsFile_empty_null() {
  TEST_ASSERT_FALSE(IsFile(""));
  TEST_ASSERT_FALSE(IsFile(nullptr));
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // NormalizePath tests
  RUN_TEST(test_NormalizePath_empty_string);
  RUN_TEST(test_NormalizePath_null_input);
  RUN_TEST(test_NormalizePath_already_absolute);
  RUN_TEST(test_NormalizePath_relative_path);
  RUN_TEST(test_NormalizePath_deep_path);
  RUN_TEST(test_NormalizePath_relative_deep);
  RUN_TEST(test_NormalizePath_root);
  RUN_TEST(test_NormalizePath_filename);
  RUN_TEST(test_NormalizePath_absolute_filename);
  
  // GetFileName tests
  RUN_TEST(test_GetFileName_simple_path);
  RUN_TEST(test_GetFileName_deep_path);
  RUN_TEST(test_GetFileName_root_file);
  RUN_TEST(test_GetFileName_no_slash);
  RUN_TEST(test_GetFileName_directory_trailing_slash);
  RUN_TEST(test_GetFileName_only_filename);
  RUN_TEST(test_GetFileName_multiple_dots);
  
  // PrependSlash tests
  RUN_TEST(test_PrependSlash_relative_path);
  RUN_TEST(test_PrependSlash_already_absolute);
  RUN_TEST(test_PrependSlash_filename);
  RUN_TEST(test_PrependSlash_null_path);
  RUN_TEST(test_PrependSlash_null_buffer);
  RUN_TEST(test_PrependSlash_zero_buffer);
  RUN_TEST(test_PrependSlash_small_buffer);
  RUN_TEST(test_PrependSlash_empty_path);
  
  // Extension tests
  RUN_TEST(test_HasJpgExtension_jpg);
  RUN_TEST(test_HasJpgExtension_jpeg);
  RUN_TEST(test_HasJpgExtension_other);
  RUN_TEST(test_HasJpgExtension_no_extension);
  RUN_TEST(test_HasJpgExtension_null);
  RUN_TEST(test_HasJpgExtension_path_with_extension);
  
  // GetNextIndex tests (circular iteration)
  RUN_TEST(test_GetNextIndex_simple);
  RUN_TEST(test_GetNextIndex_wrap_around);
  RUN_TEST(test_GetNextIndex_single_element);
  RUN_TEST(test_GetNextIndex_empty);
  
  // FindFileIndex tests
  RUN_TEST(test_FindFileIndex_found);
  RUN_TEST(test_FindFileIndex_not_found);
  RUN_TEST(test_FindFileIndex_null_input);
  RUN_TEST(test_FindFileIndex_empty_list);
  
  // IsFile tests
  RUN_TEST(test_IsFile_valid_extensions);
  RUN_TEST(test_IsFile_invalid_extensions);
  RUN_TEST(test_IsFile_no_extension);
  RUN_TEST(test_IsFile_empty_null);
  
  return UNITY_END();
}
