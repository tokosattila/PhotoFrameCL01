#include <unity.h>
#include <cstdint>

// =============================================================================
// Percentage wrapper class (from Global.h)
// =============================================================================

class Percentage {
public:
    constexpr explicit Percentage(uint8_t tValue = 0) : mValue(tValue) {}
    constexpr uint8_t Get() const { return mValue; }
    constexpr operator uint8_t() const { return mValue; }
    bool operator==(const Percentage& tOther) const { return mValue == tOther.mValue; }
    bool operator!=(const Percentage& tOther) const { return mValue != tOther.mValue; }
private:
    uint8_t mValue;
};

// =============================================================================
// Port wrapper class (from Global.h)
// =============================================================================

class Port {
public:
    constexpr explicit Port(uint16_t tValue = 0) : mValue(tValue == 0 ? 1 : tValue) {}
    constexpr uint16_t Get() const { return mValue; }
    constexpr operator uint16_t() const { return mValue; }
    bool IsValid() const { return mValue > 0; }
    bool operator==(const Port& tOther) const { return mValue == tOther.mValue; }
    bool operator!=(const Port& tOther) const { return mValue != tOther.mValue; }
private:
    uint16_t mValue;
};

// =============================================================================
// Percentage Tests
// =============================================================================

void test_Percentage_default_constructor() {
    Percentage p;
    TEST_ASSERT_EQUAL_UINT8(0, p.Get());
}

void test_Percentage_explicit_constructor() {
    Percentage p(50);
    TEST_ASSERT_EQUAL_UINT8(50, p.Get());
}

void test_Percentage_max_value() {
    Percentage p(255);
    TEST_ASSERT_EQUAL_UINT8(255, p.Get());
}

void test_Percentage_implicit_conversion() {
    Percentage p(75);
    uint8_t value = p;
    TEST_ASSERT_EQUAL_UINT8(75, value);
}

void test_Percentage_equality() {
    Percentage p1(30);
    Percentage p2(30);
    Percentage p3(40);
    
    TEST_ASSERT_TRUE(p1 == p2);
    TEST_ASSERT_FALSE(p1 == p3);
}

void test_Percentage_inequality() {
    Percentage p1(30);
    Percentage p2(30);
    Percentage p3(40);
    
    TEST_ASSERT_FALSE(p1 != p2);
    TEST_ASSERT_TRUE(p1 != p3);
}

void test_Percentage_common_values() {
    Percentage brightness(35);
    Percentage contrast(70);
    Percentage gamma(135);
    
    TEST_ASSERT_EQUAL_UINT8(35, brightness.Get());
    TEST_ASSERT_EQUAL_UINT8(70, contrast.Get());
    TEST_ASSERT_EQUAL_UINT8(135, gamma.Get());
}

// =============================================================================
// Port Tests
// =============================================================================

void test_Port_default_constructor() {
    Port p;
    TEST_ASSERT_EQUAL_UINT16(1, p.Get());  // 0 becomes 1
}

void test_Port_zero_becomes_one() {
    Port p(0);
    TEST_ASSERT_EQUAL_UINT16(1, p.Get());
}

void test_Port_explicit_constructor() {
    Port p(8080);
    TEST_ASSERT_EQUAL_UINT16(8080, p.Get());
}

void test_Port_common_ports() {
    Port http(80);
    Port https(443);
    Port ntp(123);
    
    TEST_ASSERT_EQUAL_UINT16(80, http.Get());
    TEST_ASSERT_EQUAL_UINT16(443, https.Get());
    TEST_ASSERT_EQUAL_UINT16(123, ntp.Get());
}

void test_Port_max_value() {
    Port p(65535);
    TEST_ASSERT_EQUAL_UINT16(65535, p.Get());
}

void test_Port_implicit_conversion() {
    Port p(3000);
    uint16_t value = p;
    TEST_ASSERT_EQUAL_UINT16(3000, value);
}

void test_Port_IsValid_always_true() {
    // Port is always valid because 0 becomes 1 in constructor
    Port p1(0);
    Port p2(1);
    Port p3(8080);
    
    TEST_ASSERT_TRUE(p1.IsValid());
    TEST_ASSERT_TRUE(p2.IsValid());
    TEST_ASSERT_TRUE(p3.IsValid());
}

void test_Port_equality() {
    Port p1(80);
    Port p2(80);
    Port p3(443);
    
    TEST_ASSERT_TRUE(p1 == p2);
    TEST_ASSERT_FALSE(p1 == p3);
}

void test_Port_inequality() {
    Port p1(80);
    Port p2(80);
    Port p3(443);
    
    TEST_ASSERT_FALSE(p1 != p2);
    TEST_ASSERT_TRUE(p1 != p3);
}

// =============================================================================
// Combined usage tests
// =============================================================================

void test_Percentage_in_config_scenario() {
    // Simulate display config usage
    struct DisplayConfig {
        Percentage JpgBrightness;
        Percentage JpgContrast;
        Percentage JpgGamma;
    };
    
    DisplayConfig cfg;
    cfg.JpgBrightness = Percentage(30);
    cfg.JpgContrast = Percentage(35);
    cfg.JpgGamma = Percentage(135);
    
    TEST_ASSERT_EQUAL_UINT8(30, cfg.JpgBrightness.Get());
    TEST_ASSERT_EQUAL_UINT8(35, cfg.JpgContrast.Get());
    TEST_ASSERT_EQUAL_UINT8(135, cfg.JpgGamma.Get());
}

void test_Port_in_config_scenario() {
    // Simulate network config usage
    struct NetworkConfig {
        Port NtpPort;
    };
    
    NetworkConfig cfg;
    cfg.NtpPort = Port(123);
    
    TEST_ASSERT_EQUAL_UINT16(123, cfg.NtpPort.Get());
}

// =============================================================================
// Main
// =============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Percentage tests
    RUN_TEST(test_Percentage_default_constructor);
    RUN_TEST(test_Percentage_explicit_constructor);
    RUN_TEST(test_Percentage_max_value);
    RUN_TEST(test_Percentage_implicit_conversion);
    RUN_TEST(test_Percentage_equality);
    RUN_TEST(test_Percentage_inequality);
    RUN_TEST(test_Percentage_common_values);
    
    // Port tests
    RUN_TEST(test_Port_default_constructor);
    RUN_TEST(test_Port_zero_becomes_one);
    RUN_TEST(test_Port_explicit_constructor);
    RUN_TEST(test_Port_common_ports);
    RUN_TEST(test_Port_max_value);
    RUN_TEST(test_Port_implicit_conversion);
    RUN_TEST(test_Port_IsValid_always_true);
    RUN_TEST(test_Port_equality);
    RUN_TEST(test_Port_inequality);
    
    // Combined usage tests
    RUN_TEST(test_Percentage_in_config_scenario);
    RUN_TEST(test_Port_in_config_scenario);
    
    return UNITY_END();
}
