#include <unity.h>
#include <cstdint>

static const uint16_t kDebounceMask = 0b1111000000111111;

struct SButtonDebounce {
  uint8_t Pin;
  bool Inverted;
  uint16_t History;
  uint32_t DownTimeMs;
  uint32_t NextHoldMs;
};

bool ButtonDown(const SButtonDebounce *tDebounce) {
  return tDebounce->Inverted
    ? ((tDebounce->History & kDebounceMask) == 0b1111000000000000)
    : ((tDebounce->History & kDebounceMask) == 0b0000000000111111);
}

bool ButtonUp(const SButtonDebounce *tDebounce) {
  return tDebounce->Inverted
    ? ((tDebounce->History & kDebounceMask) == 0b0000000000111111)
    : ((tDebounce->History & kDebounceMask) == 0b1111000000000000);
}

void UpdateDebounce(SButtonDebounce *tDebounce, uint8_t tLevel) {
  tDebounce->History = (tDebounce->History << 1) | (tLevel & 1);
}

void test_ButtonDown_normal_pressed() {
  SButtonDebounce db = {0, false, 0b0000000000111111, 0, 0};
  TEST_ASSERT_TRUE(ButtonDown(&db));
}

void test_ButtonDown_normal_not_pressed() {
  SButtonDebounce db = {0, false, 0b1111111111111111, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

void test_ButtonDown_normal_bouncing() {
  SButtonDebounce db = {0, false, 0b0000000000111110, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

void test_ButtonDown_normal_partial() {
  SButtonDebounce db = {0, false, 0b0000000000011111, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
}

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

void test_ButtonUp_inverted_released() {
  SButtonDebounce db = {0, true, 0b0000000000111111, 0, 0};
  TEST_ASSERT_TRUE(ButtonUp(&db));
}

void test_ButtonUp_inverted_still_pressed() {
  SButtonDebounce db = {0, true, 0b1111000000000000, 0, 0};
  TEST_ASSERT_FALSE(ButtonUp(&db));
}

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
  SButtonDebounce db = {0, false, 0xFFFF, 0, 0};
  for (int i = 0; i < 6; i++) {
    UpdateDebounce(&db, 0);
  }

  TEST_ASSERT_EQUAL_HEX16(0b1111111111000000, db.History);
}

void test_UpdateDebounce_stable_press() {
  SButtonDebounce db = {0, false, 0x0000, 0, 0};
  for (int i = 0; i < 16; i++) {
    UpdateDebounce(&db, 1);
  }

  TEST_ASSERT_EQUAL_HEX16(0xFFFF, db.History);
}

void test_full_press_cycle_normal() {
  SButtonDebounce db = {0, false, 0x0000, 0, 0};
  TEST_ASSERT_FALSE(ButtonDown(&db));
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 0);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  UpdateDebounce(&db, 1);
  TEST_ASSERT_TRUE(ButtonDown(&db));
}

void test_full_release_cycle_normal() {
  SButtonDebounce db = {0, false, 0xFFFF, 0, 0};
  for (int i = 0; i < 4; i++) {
    UpdateDebounce(&db, 1);
  }

  for (int i = 0; i < 10; i++) {
    UpdateDebounce(&db, 0);
  }

  TEST_ASSERT_TRUE(ButtonUp(&db));
}

void setUp(void) {}

void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_ButtonDown_normal_pressed);
  RUN_TEST(test_ButtonDown_normal_not_pressed);
  RUN_TEST(test_ButtonDown_normal_bouncing);
  RUN_TEST(test_ButtonDown_normal_partial);
  RUN_TEST(test_ButtonDown_inverted_pressed);
  RUN_TEST(test_ButtonDown_inverted_not_pressed);
  RUN_TEST(test_ButtonDown_inverted_bouncing);
  RUN_TEST(test_ButtonUp_normal_released);
  RUN_TEST(test_ButtonUp_normal_still_pressed);
  RUN_TEST(test_ButtonUp_normal_bouncing);
  RUN_TEST(test_ButtonUp_inverted_released);
  RUN_TEST(test_ButtonUp_inverted_still_pressed);
  RUN_TEST(test_UpdateDebounce_shift_high);
  RUN_TEST(test_UpdateDebounce_shift_low);
  RUN_TEST(test_UpdateDebounce_transition);
  RUN_TEST(test_UpdateDebounce_stable_press);
  RUN_TEST(test_full_press_cycle_normal);
  RUN_TEST(test_full_release_cycle_normal);
  return UNITY_END();
}
