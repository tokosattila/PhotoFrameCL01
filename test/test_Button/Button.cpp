/**
 * @file Button.cpp
 * @brief Unit tests for Button debounce logic (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstdint>

// ============================================================================
// Standalone implementations for testing (extracted from Button.cpp)
// ============================================================================

static const uint16_t kDebounceMask = 0b1111000000111111;

struct SButtonDebounce {
  uint8_t Pin;
  bool Inverted;
  uint16_t History;
  uint32_t DownTimeMs;
  uint32_t NextHoldMs;
};

// Button is pressed (debounced)
bool ButtonDown(const SButtonDebounce *tDebounce) {
  return tDebounce->Inverted 
    ? ((tDebounce->History & kDebounceMask) == 0b1111000000000000) 
    : ((tDebounce->History & kDebounceMask) == 0b0000000000111111);
}

// Button is released (debounced)
bool ButtonUp(const SButtonDebounce *tDebounce) {
  return tDebounce->Inverted 
    ? ((tDebounce->History & kDebounceMask) == 0b0000000000111111) 
    : ((tDebounce->History & kDebounceMask) == 0b1111000000000000);
}

// Simulate debounce update (shift left and add new bit)
void UpdateDebounce(SButtonDebounce *tDebounce, uint8_t tLevel) {
  tDebounce->History = (tDebounce->History << 1) | (tLevel & 1);
}

// ============================================================================
// ButtonDown Tests - Normal (not inverted, pull-down)
// ============================================================================

void test_ButtonDown_normal_pressed() {
  SButtonDebounce db = {0, false, 0b0000000000111111, 0, 0};
  TEST_ASSERT_TRUE(ButtonDown(&db));
}

void test_ButtonDown_normal_not_pressed() {
  SButtonDebounce db = {0, false, 0b1111111111111111, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

void test_ButtonDown_normal_bouncing() {
  // Still bouncing - not stable enough
  SButtonDebounce db = {0, false, 0b0000000000111110, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

void test_ButtonDown_normal_partial() {
  SButtonDebounce db = {0, false, 0b0000000000011111, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

// ============================================================================
// ButtonDown Tests - Inverted (pull-up)
// ============================================================================

void test_ButtonDown_inverted_pressed() {
  SButtonDebounce db = {0, true, 0b1111000000000000, 0, 0};
  TEST_ASSERT_TRUE(ButtonDown(&db));
}

void test_ButtonDown_inverted_not_pressed() {
  SButtonDebounce db = {0, true, 0b0000000000000000, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

void test_ButtonDown_inverted_bouncing() {
  SButtonDebounce db = {0, true, 0b1111000000000001, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

// ============================================================================
// ButtonUp Tests - Normal (not inverted)
// ============================================================================

void test_ButtonUp_normal_released() {
  SButtonDebounce db = {0, false, 0b1111000000000000, 0, 0};
  TEST_ASSERT_TRUE(ButtonUp(&db));
}

void test_ButtonUp_normal_still_pressed() {
  SButtonDebounce db = {0, false, 0b0000000000111111, 0, 0};
  TEST_ASSERT_FALSE(ButtonUp(&db));
}

void test_ButtonUp_normal_bouncing() {
  SButtonDebounce db = {0, false, 0b1111000000000001, 0, 0};
  TEST_ASSERT_FALSE(ButtonUp(&db));
}

// ============================================================================
// ButtonUp Tests - Inverted (pull-up)
// ============================================================================

void test_ButtonUp_inverted_released() {
  SButtonDebounce db = {0, true, 0b0000000000111111, 0, 0};
  TEST_ASSERT_TRUE(ButtonUp(&db));
}

void test_ButtonUp_inverted_still_pressed() {
  SButtonDebounce db = {0, true, 0b1111000000000000, 0, 0};
  TEST_ASSERT_FALSE(ButtonUp(&db));
}

// ============================================================================
// UpdateDebounce Tests
// ============================================================================

void test_UpdateDebounce_shift_high() {
  SButtonDebounce db = {0, false, 0b0000000000000000, 0, 0};
  UpdateDebounce(&db, 1);
  TEST_ASSERT_EQUAL_HEX16(0b0000000000000001, db.History);
  
  UpdateDebounce(&db, 1);
  TEST_ASSERT_EQUAL_HEX16(0b0000000000000011, db.History);
}

void test_UpdateDebounce_shift_low() {
  SButtonDebounce db = {0, false, 0b1111111111111111, 0, 0};
  UpdateDebounce(&db, 0);
  TEST_ASSERT_EQUAL_HEX16(0b1111111111111110, db.History);
  
  UpdateDebounce(&db, 0);
  TEST_ASSERT_EQUAL_HEX16(0b1111111111111100, db.History);
}

void test_UpdateDebounce_transition() {
  // Simulate button press transition
  SButtonDebounce db = {0, false, 0xFFFF, 0, 0};  // Button released (all 1s for pull-up sim)
  
  // Press button - send 6 lows
  for (int i = 0; i < 6; i++) {
    UpdateDebounce(&db, 0);
  }
  // Should show transition pattern in lower bits
  TEST_ASSERT_EQUAL_HEX16(0b1111111111000000, db.History);
}

void test_UpdateDebounce_stable_press() {
  // Simulate complete stable press
  SButtonDebounce db = {0, false, 0x0000, 0, 0};
  
  // Fill with 1s (button pressed for pull-down)
  for (int i = 0; i < 16; i++) {
    UpdateDebounce(&db, 1);
  }
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, db.History);
}

// ============================================================================
// Integration Tests - Full Press/Release Cycle
// ============================================================================

void test_full_press_cycle_normal() {
  SButtonDebounce db = {0, false, 0x0000, 0, 0};  // Start released
  
  // Not pressed yet
  TEST_ASSERT_FALSE(ButtonDown(&db));
  
  // Simulate bouncy press
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 0);  // bounce
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  
  // Should now detect press (6 consecutive 1s in lower bits)
  TEST_ASSERT_TRUE(ButtonDown(&db));
}

void test_full_release_cycle_normal() {
  SButtonDebounce db = {0, false, 0xFFFF, 0, 0};  // Start pressed
  
  // Simulate release with bounce
  for (int i = 0; i < 4; i++) {  // 4 high bits needed
    UpdateDebounce(&db, 1);
  }
  for (int i = 0; i < 10; i++) {  // 10 low bits - overwrite lower 6 with 0s
    UpdateDebounce(&db, 0);
  }
  
  // Should detect release
  TEST_ASSERT_TRUE(ButtonUp(&db));
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // ButtonDown - Normal
  RUN_TEST(test_ButtonDown_normal_pressed);
  RUN_TEST(test_ButtonDown_normal_not_pressed);
  RUN_TEST(test_ButtonDown_normal_bouncing);
  RUN_TEST(test_ButtonDown_normal_partial);
  
  // ButtonDown - Inverted
  RUN_TEST(test_ButtonDown_inverted_pressed);
  RUN_TEST(test_ButtonDown_inverted_not_pressed);
  RUN_TEST(test_ButtonDown_inverted_bouncing);
  
  // ButtonUp - Normal
  RUN_TEST(test_ButtonUp_normal_released);
  RUN_TEST(test_ButtonUp_normal_still_pressed);
  RUN_TEST(test_ButtonUp_normal_bouncing);
  
  // ButtonUp - Inverted
  RUN_TEST(test_ButtonUp_inverted_released);
  RUN_TEST(test_ButtonUp_inverted_still_pressed);
  
  // UpdateDebounce
  RUN_TEST(test_UpdateDebounce_shift_high);
  RUN_TEST(test_UpdateDebounce_shift_low);
  RUN_TEST(test_UpdateDebounce_transition);
  RUN_TEST(test_UpdateDebounce_stable_press);
  
  // Integration
  RUN_TEST(test_full_press_cycle_normal);
  RUN_TEST(test_full_release_cycle_normal);
  
  return UNITY_END();
}
