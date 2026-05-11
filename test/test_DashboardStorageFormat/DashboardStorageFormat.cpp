#include <unity.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace {

  std::string Trim(const std::string &tValue) {
    size_t tStart = 0;
    while (tStart < tValue.size() && std::isspace(static_cast<unsigned char>(tValue[tStart]))) tStart++;
    size_t tEnd = tValue.size();
    while (tEnd > tStart && std::isspace(static_cast<unsigned char>(tValue[tEnd - 1]))) tEnd--;
    return tValue.substr(tStart, tEnd - tStart);
  }

  std::string NormalizeStorageKey(const std::string &tValue) {
    std::string tNormalized = Trim(tValue);
    std::transform(tNormalized.begin(), tNormalized.end(), tNormalized.begin(), [](unsigned char tChar) {
      if (tChar == '-') return '_';
      return static_cast<char>(std::tolower(tChar));
    });
    return tNormalized;
  }

  struct SStorageFormatResult {
    int Status = 0;
    std::string Message;
    std::string Target;
  };

  SStorageFormatResult ResolveStorageFormatRequest(
    bool tAuthorized,
    bool tFormatAlreadyActive,
    bool tHasTarget,
    const std::string &tTargetValue,
    bool tFallbackEnabled,
    bool tSdMounted,
    bool tLittleFsMounted,
    bool tStartTaskOk) {
    SStorageFormatResult tResult;

    if (!tAuthorized) {
      tResult.Status = 401;
      tResult.Message = "unauthorized";
      return tResult;
    }

    if (tFormatAlreadyActive) {
      tResult.Status = 409;
      tResult.Message = "format_storage_busy";
      return tResult;
    }

    if (!tHasTarget) {
      tResult.Status = 400;
      tResult.Message = "format_storage_error";
      return tResult;
    }

    const std::string tStorageKey = NormalizeStorageKey(tTargetValue);
    if (tStorageKey == "littlefs") {
      if (!tFallbackEnabled) {
        tResult.Status = 400;
        tResult.Message = "format_littlefs_requires_fallback";
        return tResult;
      }
      if (!tLittleFsMounted) {
        tResult.Status = 400;
        tResult.Message = "format_storage_unavailable";
        return tResult;
      }
      tResult.Target = "littlefs";
    } else if (tStorageKey == "sd_card") {
      if (!tSdMounted) {
        tResult.Status = 400;
        tResult.Message = "format_storage_unavailable";
        return tResult;
      }
      tResult.Target = "sd_card";
    } else {
      tResult.Status = 400;
      tResult.Message = "format_storage_error";
      return tResult;
    }

    if (!tStartTaskOk) {
      tResult.Status = 500;
      tResult.Message = "format_storage_error";
      return tResult;
    }

    tResult.Status = 200;
    tResult.Message = "format_storage_started";
    return tResult;
  }

} // namespace

void test_StorageFormat_rejects_unauthorized() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(false, false, true, "sd_card", true, true, true, true);
  TEST_ASSERT_EQUAL_INT(401, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("unauthorized", tResult.Message.c_str());
}

void test_StorageFormat_rejects_when_task_already_running() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(true, true, true, "sd_card", true, true, true, true);
  TEST_ASSERT_EQUAL_INT(409, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("format_storage_busy", tResult.Message.c_str());
}

void test_StorageFormat_rejects_littlefs_without_fallback() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(true, false, true, "littlefs", false, true, true, true);
  TEST_ASSERT_EQUAL_INT(400, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("format_littlefs_requires_fallback", tResult.Message.c_str());
}

void test_StorageFormat_rejects_unmounted_sd_card() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(true, false, true, "sd_card", true, false, true, true);
  TEST_ASSERT_EQUAL_INT(400, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("format_storage_unavailable", tResult.Message.c_str());
}

void test_StorageFormat_accepts_littlefs_with_fallback() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(true, false, true, "littlefs", true, true, true, true);
  TEST_ASSERT_EQUAL_INT(200, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("format_storage_started", tResult.Message.c_str());
  TEST_ASSERT_EQUAL_STRING("littlefs", tResult.Target.c_str());
}

void test_StorageFormat_rejects_start_task_failure() {
  const SStorageFormatResult tResult = ResolveStorageFormatRequest(true, false, true, "sd_card", true, true, true, false);
  TEST_ASSERT_EQUAL_INT(500, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("format_storage_error", tResult.Message.c_str());
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_StorageFormat_rejects_unauthorized);
  RUN_TEST(test_StorageFormat_rejects_when_task_already_running);
  RUN_TEST(test_StorageFormat_rejects_littlefs_without_fallback);
  RUN_TEST(test_StorageFormat_rejects_unmounted_sd_card);
  RUN_TEST(test_StorageFormat_accepts_littlefs_with_fallback);
  RUN_TEST(test_StorageFormat_rejects_start_task_failure);
  return UNITY_END();
}
