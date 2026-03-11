/**
 * @file test_configuration.cpp
 * @brief Unit tests for Configuration INI parsing (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cstdlib>

// ============================================================================
// Standalone implementations for testing (extracted from Configuration.cpp)
// ============================================================================

void TrimValue(char *tValue) {
  if (!tValue || tValue[0] == '\0') return;
  size_t tLength = strlen(tValue);
  // Trim trailing whitespace
  while (tLength > 0 && (tValue[tLength - 1] == ' ' || tValue[tLength - 1] == '\t' || 
         tValue[tLength - 1] == '\r' || tValue[tLength - 1] == '\n')) {
    tValue[--tLength] = '\0';
  }
  // Trim leading whitespace
  size_t tStart = 0;
  while (tValue[tStart] == ' ' || tValue[tStart] == '\t' || 
         tValue[tStart] == '\r' || tValue[tStart] == '\n') {
    tStart++;
  }
  if (tStart > 0) memmove(tValue, tValue + tStart, tLength - tStart + 1);
}

bool ParseLine(char *tLine, char *tSection, char *tKey, char *tValue) {
  if (tLine == nullptr || tLine[0] == '\0') return false;
  TrimValue(tLine);
  if (tLine[0] == '\0' || tLine[0] == '#' || tLine[0] == ';') return false;
  if (tLine[0] == '[') {
    char *tEnd = strchr(tLine, ']');
    if (tEnd) {
      *tEnd = '\0';
      strncpy(tSection, tLine + 1, 31);
      tSection[31] = '\0';
    }
    return false;
  }
  char *tSeparator = strchr(tLine, '=');
  if (tSeparator) {
    *tSeparator = '\0';
    strncpy(tKey, tLine, 31);
    tKey[31] = '\0';
    strncpy(tValue, tSeparator + 1, 127);
    tValue[127] = '\0';
    TrimValue(tKey);
    TrimValue(tValue);
    return true;
  }
  return false;
}

bool IsPublicConfigKey(const char *tKey) {
  if (!tKey || !tKey[0]) return false;
  static const char *kPublicKeys[] = {
    "appname", "version", "jpg_brightness", "jpg_contrast", "jpg_gamma", "image_file",
    "ntp_server", "ntp_port", "ntp_gmt_offset", "ntp_update",
    "ap_enable", "ap_ssid", "ap_password", "ap_ip", "ap_gateway", "ap_subnet",
    "sta_ssid", "sta_password", "sta_enable", "sta_ip", "sta_gateway", "sta_subnet", "sta_dns1", "sta_dns2",
    "mdns_enable", "mdns_hostname",
    "wake_up", "wake_up_hour",
    "default_file_system", "fallback_enabled"
  };
  for (size_t i = 0; i < sizeof(kPublicKeys) / sizeof(kPublicKeys[0]); i++) {
    if (strcmp(kPublicKeys[i], tKey) == 0) return true;
  }
  return false;
}

// ============================================================================
// TrimValue Tests
// ============================================================================

void test_TrimValue_no_whitespace() {
  char buffer[64] = "hello";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_leading_spaces() {
  char buffer[64] = "   hello";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_trailing_spaces() {
  char buffer[64] = "hello   ";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_both_sides() {
  char buffer[64] = "   hello   ";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_tabs() {
  char buffer[64] = "\t\thello\t\t";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_mixed_whitespace() {
  char buffer[64] = " \t hello \t ";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_newlines() {
  char buffer[64] = "hello\r\n";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("hello", buffer);
}

void test_TrimValue_empty_string() {
  char buffer[64] = "";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("", buffer);
}

void test_TrimValue_only_whitespace() {
  char buffer[64] = "   \t\t  \r\n";
  TrimValue(buffer);
  TEST_ASSERT_EQUAL_STRING("", buffer);
}

void test_TrimValue_null_input() {
  TrimValue(nullptr);  // Should not crash
  TEST_PASS();
}

// ============================================================================
// ParseLine Tests - Comments and Empty Lines
// ============================================================================

void test_ParseLine_empty_line() {
  char line[128] = "";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
}

void test_ParseLine_comment_hash() {
  char line[128] = "# This is a comment";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
}

void test_ParseLine_comment_semicolon() {
  char line[128] = "; This is a comment";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
}

void test_ParseLine_whitespace_only() {
  char line[128] = "   \t\t  ";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
}

// ============================================================================
// ParseLine Tests - Sections
// ============================================================================

void test_ParseLine_section() {
  char line[128] = "[device]";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("device", section);
}

void test_ParseLine_section_with_spaces() {
  char line[128] = "[ap mode]";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("ap mode", section);
}

void test_ParseLine_section_preserves_previous() {
  char section[32] = "old_section";
  char key[32] = "";
  char value[128] = "";
  char line[128] = "[new_section]";
  ParseLine(line, section, key, value);
  TEST_ASSERT_EQUAL_STRING("new_section", section);
}

// ============================================================================
// ParseLine Tests - Key-Value Pairs
// ============================================================================

void test_ParseLine_simple_keyvalue() {
  char line[128] = "appname = Photo Frame";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("appname", key);
  TEST_ASSERT_EQUAL_STRING("Photo Frame", value);
}

void test_ParseLine_keyvalue_no_spaces() {
  char line[128] = "port=23";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("port", key);
  TEST_ASSERT_EQUAL_STRING("23", value);
}

void test_ParseLine_keyvalue_extra_spaces() {
  char line[128] = "  username   =   admin  ";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("username", key);
  TEST_ASSERT_EQUAL_STRING("admin", value);
}

void test_ParseLine_empty_value() {
  char line[128] = "password = ";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("password", key);
  TEST_ASSERT_EQUAL_STRING("", value);
}

void test_ParseLine_value_with_equals() {
  char line[128] = "formula = a=b+c";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("formula", key);
  TEST_ASSERT_EQUAL_STRING("a=b+c", value);  // Value contains everything after first =
}

void test_ParseLine_numeric_value() {
  char line[128] = "brightness = 35";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("brightness", key);
  TEST_ASSERT_EQUAL_STRING("35", value);
}

void test_ParseLine_boolean_true() {
  char line[128] = "enable = true";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("enable", key);
  TEST_ASSERT_EQUAL_STRING("true", value);
}

void test_ParseLine_boolean_false() {
  char line[128] = "enable = false";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("enable", key);
  TEST_ASSERT_EQUAL_STRING("false", value);
}

void test_ParseLine_ip_address() {
  char line[128] = "sta_ip = 192.168.0.100";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("sta_ip", key);
  TEST_ASSERT_EQUAL_STRING("192.168.0.100", value);
}

void test_ParseLine_ssid_with_spaces() {
  char line[128] = "sta_ssid = My WiFi Network";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_TRUE(ParseLine(line, section, key, value));
  TEST_ASSERT_EQUAL_STRING("sta_ssid", key);
  TEST_ASSERT_EQUAL_STRING("My WiFi Network", value);
}

// ============================================================================
// ParseLine Tests - Null and Edge Cases
// ============================================================================

void test_ParseLine_null_input() {
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(nullptr, section, key, value));
}

void test_ParseLine_no_equals_sign() {
  char line[128] = "invalid line without equals";
  char section[32] = "";
  char key[32] = "";
  char value[128] = "";
  TEST_ASSERT_FALSE(ParseLine(line, section, key, value));
}

void test_PublicConfigKeys_image_updated_at_not_exposed() {
  TEST_ASSERT_FALSE(IsPublicConfigKey("image_updated_at"));
  TEST_ASSERT_FALSE(IsPublicConfigKey("dsp.file.upd"));
  TEST_ASSERT_TRUE(IsPublicConfigKey("image_file"));
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // TrimValue tests
  RUN_TEST(test_TrimValue_no_whitespace);
  RUN_TEST(test_TrimValue_leading_spaces);
  RUN_TEST(test_TrimValue_trailing_spaces);
  RUN_TEST(test_TrimValue_both_sides);
  RUN_TEST(test_TrimValue_tabs);
  RUN_TEST(test_TrimValue_mixed_whitespace);
  RUN_TEST(test_TrimValue_newlines);
  RUN_TEST(test_TrimValue_empty_string);
  RUN_TEST(test_TrimValue_only_whitespace);
  RUN_TEST(test_TrimValue_null_input);
  
  // ParseLine - Comments/Empty
  RUN_TEST(test_ParseLine_empty_line);
  RUN_TEST(test_ParseLine_comment_hash);
  RUN_TEST(test_ParseLine_comment_semicolon);
  RUN_TEST(test_ParseLine_whitespace_only);
  
  // ParseLine - Sections
  RUN_TEST(test_ParseLine_section);
  RUN_TEST(test_ParseLine_section_with_spaces);
  RUN_TEST(test_ParseLine_section_preserves_previous);
  
  // ParseLine - Key-Value
  RUN_TEST(test_ParseLine_simple_keyvalue);
  RUN_TEST(test_ParseLine_keyvalue_no_spaces);
  RUN_TEST(test_ParseLine_keyvalue_extra_spaces);
  RUN_TEST(test_ParseLine_empty_value);
  RUN_TEST(test_ParseLine_value_with_equals);
  RUN_TEST(test_ParseLine_numeric_value);
  RUN_TEST(test_ParseLine_boolean_true);
  RUN_TEST(test_ParseLine_boolean_false);
  RUN_TEST(test_ParseLine_ip_address);
  RUN_TEST(test_ParseLine_ssid_with_spaces);
  
  // ParseLine - Edge cases
  RUN_TEST(test_ParseLine_null_input);
  RUN_TEST(test_ParseLine_no_equals_sign);
  RUN_TEST(test_PublicConfigKeys_image_updated_at_not_exposed);
  
  return UNITY_END();
}
