/**
 * @file BootPartitionCommand.cpp
 * @brief Unit tests for bootpart command parsing (pure C++ logic)
 */

#include <unity.h>
#include <cstring>
#include <cctype>

enum class EBootPartArg {
  Status,
  Ota0,
  Ota1,
  Invalid,
  TooManyArgs,
};

static int CiStrcmp(const char *a, const char *b) {
  if (!a) a = "";
  if (!b) b = "";
  while (*a && *b) {
    int ca = std::tolower((unsigned char)*a++);
    int cb = std::tolower((unsigned char)*b++);
    if (ca != cb) return ca - cb;
  }
  return std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
}

static EBootPartArg ParseBootPartArg(const char *input) {
  const char *p = input ? input : "";
  while (*p == ' ' || *p == '\t') ++p;
  while (*p != '\0' && *p != ' ' && *p != '\t') ++p; // skip command
  while (*p == ' ' || *p == '\t') ++p;

  if (*p == '\0') return EBootPartArg::Status;

  char arg[16] = "";
  const char *start = p;
  while (*p != '\0' && *p != ' ' && *p != '\t') ++p;
  size_t len = (size_t)(p - start);
  if (len == 0 || len >= sizeof(arg)) return EBootPartArg::Invalid;
  std::memcpy(arg, start, len);
  arg[len] = '\0';

  while (*p == ' ' || *p == '\t') ++p;
  if (*p != '\0') return EBootPartArg::TooManyArgs;

  if (CiStrcmp(arg, "status") == 0) return EBootPartArg::Status;
  if (CiStrcmp(arg, "ota0") == 0) return EBootPartArg::Ota0;
  if (CiStrcmp(arg, "ota1") == 0) return EBootPartArg::Ota1;
  return EBootPartArg::Invalid;
}

void test_default_status() {
  TEST_ASSERT_EQUAL((int)EBootPartArg::Status, (int)ParseBootPartArg("bootpart"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Status, (int)ParseBootPartArg("bootpart   "));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Status, (int)ParseBootPartArg("  bootpart\t"));
}

void test_explicit_args() {
  TEST_ASSERT_EQUAL((int)EBootPartArg::Status, (int)ParseBootPartArg("bootpart status"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Ota0, (int)ParseBootPartArg("bootpart ota0"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Ota1, (int)ParseBootPartArg("bootpart ota1"));
}

void test_case_insensitive_and_tabs() {
  TEST_ASSERT_EQUAL((int)EBootPartArg::Ota0, (int)ParseBootPartArg("bootpart\tOTA0"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Ota1, (int)ParseBootPartArg("BOOTPART\tOtA1"));
}

void test_invalid_and_too_many() {
  TEST_ASSERT_EQUAL((int)EBootPartArg::Invalid, (int)ParseBootPartArg("bootpart wat"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::TooManyArgs, (int)ParseBootPartArg("bootpart ota0 extra"));
  TEST_ASSERT_EQUAL((int)EBootPartArg::Status, (int)ParseBootPartArg(nullptr));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_default_status);
  RUN_TEST(test_explicit_args);
  RUN_TEST(test_case_insensitive_and_tabs);
  RUN_TEST(test_invalid_and_too_many);
  return UNITY_END();
}
