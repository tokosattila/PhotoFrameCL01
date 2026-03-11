/**
 * @file ConfigCommand.cpp
 * @brief Unit tests for ConfigCommand parsing functions (pure C++ logic, no hardware)
 */

#include <unity.h>
#include <cstring>
#include <cstdint>
#include <cstdio>

// ============================================================================
// Standalone implementations for testing (extracted from ConfigCommand.h)
// ============================================================================

enum class EConfigParseResult {
  Success = 0,
  MissingKey,
  KeyTooLong,
  ValueTooLong,
  TooManyArguments
};

struct SConfigParsed {
  char Key[32];
  char Value[128];
  bool HasValue;
};

// Skip whitespace helper
inline const char *SkipWhitespace(const char *tPtr) {
  while (*tPtr == ' ' || *tPtr == '\t') ++tPtr;
  return tPtr;
}

// Skip non-whitespace helper
inline const char *SkipNonWhitespace(const char *tPtr) {
  while (*tPtr != '\0' && *tPtr != ' ' && *tPtr != '\t') ++tPtr;
  return tPtr;
}

// Parse config command arguments: "config <key> [value]"
EConfigParseResult ParseConfigArgs(const char *tArguments, SConfigParsed &tParsed) {
  memset(&tParsed, 0, sizeof(tParsed));
  
  if (!tArguments || tArguments[0] == '\0') {
    return EConfigParseResult::MissingKey;
  }
  
  const char *tPtr = tArguments;
  
  // Skip "config" command name
  tPtr = SkipWhitespace(tPtr);
  tPtr = SkipNonWhitespace(tPtr);
  tPtr = SkipWhitespace(tPtr);
  
  if (*tPtr == '\0') {
    return EConfigParseResult::MissingKey;
  }
  
  // Parse key
  const char *tKeyStart = tPtr;
  tPtr = SkipNonWhitespace(tPtr);
  size_t tKeyLen = tPtr - tKeyStart;
  
  if (tKeyLen >= sizeof(tParsed.Key)) {
    return EConfigParseResult::KeyTooLong;
  }
  
  strncpy(tParsed.Key, tKeyStart, tKeyLen);
  tParsed.Key[tKeyLen] = '\0';
  
  // Check for value
  tPtr = SkipWhitespace(tPtr);
  
  if (*tPtr == '\0') {
    tParsed.HasValue = false;
    return EConfigParseResult::Success;
  }
  
  // Parse value (with quote support)
  const char *tValueStart = tPtr;
  bool tInQuote = false;
  
  if (*tPtr == '"') {
    tInQuote = true;
    ++tPtr;
    tValueStart = tPtr;
  }
  
  while (*tPtr != '\0') {
    if (tInQuote) {
      if (*tPtr == '"') break;
    } else {
      if (*tPtr == ' ' || *tPtr == '\t') break;
    }
    ++tPtr;
  }
  
  size_t tValueLen = tPtr - tValueStart;
  
  if (tValueLen >= sizeof(tParsed.Value)) {
    return EConfigParseResult::ValueTooLong;
  }
  
  strncpy(tParsed.Value, tValueStart, tValueLen);
  tParsed.Value[tValueLen] = '\0';
  tParsed.HasValue = true;
  
  // Skip closing quote
  if (tInQuote && *tPtr == '"') ++tPtr;
  
  // Check for extra arguments
  tPtr = SkipWhitespace(tPtr);
  if (*tPtr != '\0') {
    return EConfigParseResult::TooManyArguments;
  }
  
  return EConfigParseResult::Success;
}

// ============================================================================
// ParseConfigArgs - Get key only tests
// ============================================================================

void test_ParseConfigArgs_get_key_simple() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_FALSE(parsed.HasValue);
}

void test_ParseConfigArgs_get_key_with_dots() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config wifi.ssid", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("wifi.ssid", parsed.Key);
  TEST_ASSERT_FALSE(parsed.HasValue);
}

void test_ParseConfigArgs_get_key_underscore() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config display_brightness", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("display_brightness", parsed.Key);
  TEST_ASSERT_FALSE(parsed.HasValue);
}

// ============================================================================
// ParseConfigArgs - Set key=value tests
// ============================================================================

void test_ParseConfigArgs_set_simple_value() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid MyNetwork", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("MyNetwork", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_set_numeric_value() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config brightness 75", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("brightness", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("75", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_set_quoted_value_with_spaces() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid \"My WiFi Network\"", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("My WiFi Network", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_set_ip_address() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ntp_server 192.168.1.1", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ntp_server", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("192.168.1.1", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_set_boolean_true() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ap_mode true", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ap_mode", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("true", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_set_boolean_false() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ap_mode false", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ap_mode", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("false", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

// ============================================================================
// ParseConfigArgs - Whitespace handling tests
// ============================================================================

void test_ParseConfigArgs_extra_spaces_before_key() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config    ssid", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
}

void test_ParseConfigArgs_extra_spaces_before_value() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid    MyNetwork", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("MyNetwork", parsed.Value);
}

void test_ParseConfigArgs_tabs_instead_of_spaces() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config\tssid\tMyNetwork", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("MyNetwork", parsed.Value);
}

// ============================================================================
// ParseConfigArgs - Error cases
// ============================================================================

void test_ParseConfigArgs_missing_key_empty() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::MissingKey, result);
}

void test_ParseConfigArgs_missing_key_null() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs(nullptr, parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::MissingKey, result);
}

void test_ParseConfigArgs_missing_key_only_command() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::MissingKey, result);
}

void test_ParseConfigArgs_missing_key_spaces_only() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config   ", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::MissingKey, result);
}

void test_ParseConfigArgs_too_many_arguments() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid value extra", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::TooManyArguments, result);
}

void test_ParseConfigArgs_key_too_long() {
  SConfigParsed parsed;
  // Key longer than 31 chars
  EConfigParseResult result = ParseConfigArgs("config this_is_a_very_long_key_that_exceeds_limit", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::KeyTooLong, result);
}

// ============================================================================
// Edge cases
// ============================================================================

void test_ParseConfigArgs_empty_quoted_value() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config ssid \"\"", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("ssid", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("", parsed.Value);
  TEST_ASSERT_TRUE(parsed.HasValue);
}

void test_ParseConfigArgs_value_with_special_chars() {
  SConfigParsed parsed;
  EConfigParseResult result = ParseConfigArgs("config password \"P@ss!w0rd#123\"", parsed);
  TEST_ASSERT_EQUAL(EConfigParseResult::Success, result);
  TEST_ASSERT_EQUAL_STRING("password", parsed.Key);
  TEST_ASSERT_EQUAL_STRING("P@ss!w0rd#123", parsed.Value);
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  // Get key only
  RUN_TEST(test_ParseConfigArgs_get_key_simple);
  RUN_TEST(test_ParseConfigArgs_get_key_with_dots);
  RUN_TEST(test_ParseConfigArgs_get_key_underscore);
  
  // Set key=value
  RUN_TEST(test_ParseConfigArgs_set_simple_value);
  RUN_TEST(test_ParseConfigArgs_set_numeric_value);
  RUN_TEST(test_ParseConfigArgs_set_quoted_value_with_spaces);
  RUN_TEST(test_ParseConfigArgs_set_ip_address);
  RUN_TEST(test_ParseConfigArgs_set_boolean_true);
  RUN_TEST(test_ParseConfigArgs_set_boolean_false);
  
  // Whitespace handling
  RUN_TEST(test_ParseConfigArgs_extra_spaces_before_key);
  RUN_TEST(test_ParseConfigArgs_extra_spaces_before_value);
  RUN_TEST(test_ParseConfigArgs_tabs_instead_of_spaces);
  
  // Error cases
  RUN_TEST(test_ParseConfigArgs_missing_key_empty);
  RUN_TEST(test_ParseConfigArgs_missing_key_null);
  RUN_TEST(test_ParseConfigArgs_missing_key_only_command);
  RUN_TEST(test_ParseConfigArgs_missing_key_spaces_only);
  RUN_TEST(test_ParseConfigArgs_too_many_arguments);
  RUN_TEST(test_ParseConfigArgs_key_too_long);
  
  // Edge cases
  RUN_TEST(test_ParseConfigArgs_empty_quoted_value);
  RUN_TEST(test_ParseConfigArgs_value_with_special_chars);
  
  return UNITY_END();
}
