/**
 * @file Storage.cpp
 * @brief Unit tests for Storage functions (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

// ============================================================================
// Standalone implementations for testing (extracted from Storage.cpp)
// ============================================================================

enum class EFileSystemType : uint8_t {
  LittleFS = 1,
  SDCard = 2
};

// Simulated storage state for testing
struct MockStorageState {
  bool SDCardMounted = false;
  bool LittleFSMounted = false;
  bool SDCardHasImages = false;
  bool LittleFSHasImages = false;
  EFileSystemType DefaultFS = EFileSystemType::SDCard;
  bool FallbackEnabled = true;
};

// Storage selection logic (extracted from SelectActiveStorage)
struct StorageSelection {
  EFileSystemType ActiveType;
  bool Mounted;
  bool FallbackActive;
};

struct StorageLabelLayout {
  std::string PrimaryStorageKey;
  std::string SecondaryStorageKey;
  std::string PrimaryLabelKey;
  std::string SecondaryLabelKey;
};

StorageSelection SelectActiveStorage(const MockStorageState &state) {
  StorageSelection result = {EFileSystemType::LittleFS, false, false};
  
  if (state.DefaultFS == EFileSystemType::SDCard) {
    // SDCard is primary
    if (state.SDCardMounted) {
      if (state.SDCardHasImages) {
        result.ActiveType = EFileSystemType::SDCard;
        result.Mounted = true;
        result.FallbackActive = false;
        return result;
      }
      // SDCard mounted but no images - try fallback
      if (state.FallbackEnabled && state.LittleFSMounted && state.LittleFSHasImages) {
        result.ActiveType = EFileSystemType::LittleFS;
        result.Mounted = true;
        result.FallbackActive = true;
        return result;
      }
      // No images on either, use primary
      result.ActiveType = EFileSystemType::SDCard;
      result.Mounted = true;
      result.FallbackActive = false;
      return result;
    }
    // SDCard not mounted - fallback if enabled
    if (state.FallbackEnabled && state.LittleFSMounted) {
      result.ActiveType = EFileSystemType::LittleFS;
      result.Mounted = true;
      result.FallbackActive = true;
      return result;
    }
  } else {
    // LittleFS is primary
    if (state.LittleFSMounted) {
      if (state.LittleFSHasImages) {
        result.ActiveType = EFileSystemType::LittleFS;
        result.Mounted = true;
        result.FallbackActive = false;
        return result;
      }
      // LittleFS mounted but no images - try fallback
      if (state.FallbackEnabled && state.SDCardMounted && state.SDCardHasImages) {
        result.ActiveType = EFileSystemType::SDCard;
        result.Mounted = true;
        result.FallbackActive = true;
        return result;
      }
      // No images on either, use primary
      result.ActiveType = EFileSystemType::LittleFS;
      result.Mounted = true;
      result.FallbackActive = false;
      return result;
    }
    // LittleFS not mounted - fallback if enabled
    if (state.FallbackEnabled && state.SDCardMounted) {
      result.ActiveType = EFileSystemType::SDCard;
      result.Mounted = true;
      result.FallbackActive = true;
      return result;
    }
  }
  
  return result; // Not mounted
}

StorageLabelLayout BuildDashboardStorageLabelLayout(EFileSystemType tDefaultFS, bool tFallbackEnabled) {
  StorageLabelLayout result;

  result.PrimaryStorageKey = (tDefaultFS == EFileSystemType::SDCard) ? "sd_card" : "littlefs";
  result.SecondaryStorageKey = (result.PrimaryStorageKey == "sd_card") ? "littlefs" : "sd_card";

  const bool tPrimaryStorageIsSd = result.PrimaryStorageKey == "sd_card";
  const bool tSecondaryStorageIsSd = result.SecondaryStorageKey == "sd_card";

  result.PrimaryLabelKey = tPrimaryStorageIsSd ? "sd_card" : "littlefs";
  if (tFallbackEnabled) result.SecondaryLabelKey = tSecondaryStorageIsSd ? "sd_card_fallback" : "littlefs_fallback";
  else result.SecondaryLabelKey = tSecondaryStorageIsSd ? "sd_card" : "littlefs";

  return result;
}

// ============================================================================
// SDCard Primary - Basic Tests
// ============================================================================

void test_SDCard_primary_with_images() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = true;
  state.SDCardHasImages = true;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_FALSE(result.FallbackActive);
}

void test_SDCard_primary_no_images_fallback_to_littlefs() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = true;
  state.SDCardHasImages = false;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::LittleFS, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_TRUE(result.FallbackActive);
}

void test_SDCard_primary_not_mounted_fallback() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = false;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::LittleFS, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_TRUE(result.FallbackActive);
}

void test_SDCard_primary_no_images_no_fallback_images() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = true;
  state.SDCardHasImages = false;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = false;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  // Should stay on primary even with no images
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_FALSE(result.FallbackActive);
}

void test_SDCard_primary_fallback_disabled() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = true;
  state.SDCardHasImages = false;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  state.FallbackEnabled = false;
  
  StorageSelection result = SelectActiveStorage(state);
  
  // Should stay on primary even though fallback has images
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_FALSE(result.FallbackActive);
}

// ============================================================================
// LittleFS Primary - Basic Tests
// ============================================================================

void test_LittleFS_primary_with_images() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::LittleFS;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  state.SDCardMounted = true;
  state.SDCardHasImages = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::LittleFS, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_FALSE(result.FallbackActive);
}

void test_LittleFS_primary_no_images_fallback_to_sdcard() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::LittleFS;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = false;
  state.SDCardMounted = true;
  state.SDCardHasImages = true;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_TRUE(result.FallbackActive);
}

void test_LittleFS_primary_not_mounted_fallback() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::LittleFS;
  state.LittleFSMounted = false;
  state.SDCardMounted = true;
  state.SDCardHasImages = true;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_TRUE(result.FallbackActive);
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_nothing_mounted() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = false;
  state.LittleFSMounted = false;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_FALSE(result.Mounted);
}

void test_only_fallback_mounted() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = false;
  state.LittleFSMounted = true;
  state.LittleFSHasImages = true;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::LittleFS, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_TRUE(result.FallbackActive);
}

void test_only_primary_mounted_no_images() {
  MockStorageState state;
  state.DefaultFS = EFileSystemType::SDCard;
  state.SDCardMounted = true;
  state.SDCardHasImages = false;
  state.LittleFSMounted = false;
  state.FallbackEnabled = true;
  
  StorageSelection result = SelectActiveStorage(state);
  
  TEST_ASSERT_EQUAL(EFileSystemType::SDCard, result.ActiveType);
  TEST_ASSERT_TRUE(result.Mounted);
  TEST_ASSERT_FALSE(result.FallbackActive);
}

// ============================================================================
// Dashboard Stats label/order mapping tests
// ============================================================================

void test_DashboardLayout_default_sd_fallback_enabled() {
  StorageLabelLayout layout = BuildDashboardStorageLabelLayout(EFileSystemType::SDCard, true);

  TEST_ASSERT_EQUAL_STRING("sd_card", layout.PrimaryStorageKey.c_str());
  TEST_ASSERT_EQUAL_STRING("littlefs", layout.SecondaryStorageKey.c_str());
  TEST_ASSERT_EQUAL_STRING("sd_card", layout.PrimaryLabelKey.c_str());
  TEST_ASSERT_EQUAL_STRING("littlefs_fallback", layout.SecondaryLabelKey.c_str());
}

void test_DashboardLayout_default_littlefs_fallback_enabled() {
  StorageLabelLayout layout = BuildDashboardStorageLabelLayout(EFileSystemType::LittleFS, true);

  TEST_ASSERT_EQUAL_STRING("littlefs", layout.PrimaryStorageKey.c_str());
  TEST_ASSERT_EQUAL_STRING("sd_card", layout.SecondaryStorageKey.c_str());
  TEST_ASSERT_EQUAL_STRING("littlefs", layout.PrimaryLabelKey.c_str());
  TEST_ASSERT_EQUAL_STRING("sd_card_fallback", layout.SecondaryLabelKey.c_str());
}

void test_DashboardLayout_fallback_disabled_has_no_fallback_labels() {
  StorageLabelLayout layoutSd = BuildDashboardStorageLabelLayout(EFileSystemType::SDCard, false);
  StorageLabelLayout layoutLittleFs = BuildDashboardStorageLabelLayout(EFileSystemType::LittleFS, false);

  TEST_ASSERT_EQUAL_STRING("littlefs", layoutSd.SecondaryLabelKey.c_str());
  TEST_ASSERT_EQUAL_STRING("sd_card", layoutLittleFs.SecondaryLabelKey.c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // SDCard Primary tests
  RUN_TEST(test_SDCard_primary_with_images);
  RUN_TEST(test_SDCard_primary_no_images_fallback_to_littlefs);
  RUN_TEST(test_SDCard_primary_not_mounted_fallback);
  RUN_TEST(test_SDCard_primary_no_images_no_fallback_images);
  RUN_TEST(test_SDCard_primary_fallback_disabled);
  
  // LittleFS Primary tests
  RUN_TEST(test_LittleFS_primary_with_images);
  RUN_TEST(test_LittleFS_primary_no_images_fallback_to_sdcard);
  RUN_TEST(test_LittleFS_primary_not_mounted_fallback);
  
  // Edge cases
  RUN_TEST(test_nothing_mounted);
  RUN_TEST(test_only_fallback_mounted);
  RUN_TEST(test_only_primary_mounted_no_images);

  // Dashboard stats label/order mapping
  RUN_TEST(test_DashboardLayout_default_sd_fallback_enabled);
  RUN_TEST(test_DashboardLayout_default_littlefs_fallback_enabled);
  RUN_TEST(test_DashboardLayout_fallback_disabled_has_no_fallback_labels);
  
  return UNITY_END();
}
