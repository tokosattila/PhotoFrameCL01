#include <unity.h>
#include <string>

enum class EBootTargetSubtype {
  Ota0,
  Ota1
};

enum class EBootTargetSaveResult {
  Success,
  MissingPartition,
  InvalidPartition,
  SetBootFailed
};

static EBootTargetSubtype ResolveBootTargetSubtype(const std::string &tValue) {
  return tValue == "ota_1" ? EBootTargetSubtype::Ota1 : EBootTargetSubtype::Ota0;
}

static const char *ResolveBootTargetErrorMessage(EBootTargetSaveResult tResult) {
  if (tResult == EBootTargetSaveResult::InvalidPartition) return "boot_target_invalid_partition";
  if (tResult == EBootTargetSaveResult::Success) return "boot_target_save_success";
  return "boot_target_save_error";
}

static EBootTargetSaveResult ResolveBootTargetSaveResult(bool tPartitionFound, bool tValidAppDescription, bool tSetBootSucceeded) {
  if (!tPartitionFound) return EBootTargetSaveResult::MissingPartition;
  if (!tValidAppDescription) return EBootTargetSaveResult::InvalidPartition;
  if (!tSetBootSucceeded) return EBootTargetSaveResult::SetBootFailed;
  return EBootTargetSaveResult::Success;
}

void test_BootTargetSubtype_ota_1_selects_ota1() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(EBootTargetSubtype::Ota1), static_cast<int>(ResolveBootTargetSubtype("ota_1")));
}

void test_BootTargetSubtype_defaults_to_ota0() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(EBootTargetSubtype::Ota0), static_cast<int>(ResolveBootTargetSubtype("ota_0")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(EBootTargetSubtype::Ota0), static_cast<int>(ResolveBootTargetSubtype("invalid")));
}

void test_BootTargetSaveResult_rejects_missing_partition() {
  const EBootTargetSaveResult tResult = ResolveBootTargetSaveResult(false, true, true);
  TEST_ASSERT_EQUAL_STRING("boot_target_save_error", ResolveBootTargetErrorMessage(tResult));
}

void test_BootTargetSaveResult_rejects_invalid_app_partition() {
  const EBootTargetSaveResult tResult = ResolveBootTargetSaveResult(true, false, true);
  TEST_ASSERT_EQUAL_STRING("boot_target_invalid_partition", ResolveBootTargetErrorMessage(tResult));
}

void test_BootTargetSaveResult_rejects_set_boot_failure() {
  const EBootTargetSaveResult tResult = ResolveBootTargetSaveResult(true, true, false);
  TEST_ASSERT_EQUAL_STRING("boot_target_save_error", ResolveBootTargetErrorMessage(tResult));
}

void test_BootTargetSaveResult_accepts_valid_partition() {
  const EBootTargetSaveResult tResult = ResolveBootTargetSaveResult(true, true, true);
  TEST_ASSERT_EQUAL_STRING("boot_target_save_success", ResolveBootTargetErrorMessage(tResult));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_BootTargetSubtype_ota_1_selects_ota1);
  RUN_TEST(test_BootTargetSubtype_defaults_to_ota0);
  RUN_TEST(test_BootTargetSaveResult_rejects_missing_partition);
  RUN_TEST(test_BootTargetSaveResult_rejects_invalid_app_partition);
  RUN_TEST(test_BootTargetSaveResult_rejects_set_boot_failure);
  RUN_TEST(test_BootTargetSaveResult_accepts_valid_partition);
  return UNITY_END();
}
