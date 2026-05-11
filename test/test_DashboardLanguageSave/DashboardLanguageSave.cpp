#include <unity.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace {

  std::string Trim(const std::string &tValue) {
    size_t tStart = 0;
    while (tStart < tValue.size() && std::isspace(static_cast<unsigned char>(tValue[tStart]))) tStart++;
    size_t tEnd = tValue.size();
    while (tEnd > tStart && std::isspace(static_cast<unsigned char>(tValue[tEnd - 1]))) tEnd--;
    return tValue.substr(tStart, tEnd - tStart);
  }

  std::string NormalizeLanguageCode(const std::string &tLanguage) {
    std::string tNormalized = Trim(tLanguage);
    std::transform(tNormalized.begin(), tNormalized.end(), tNormalized.begin(), [](unsigned char tChar) {
      return static_cast<char>(std::tolower(tChar));
    });
    return tNormalized;
  }

  bool IsLanguageEnabled(const std::vector<std::string> &tLanguages, const std::string &tLanguage) {
    const std::string tNormalized = NormalizeLanguageCode(tLanguage);
    if (tNormalized.empty()) return false;
    for (const std::string &tItem : tLanguages) {
      if (NormalizeLanguageCode(tItem) == tNormalized) return true;
    }
    return false;
  }

  bool IsSupportedLanguage(const std::string &tLanguage) {
    const std::string tNormalized = NormalizeLanguageCode(tLanguage);
    return tNormalized == "en" || tNormalized == "hu";
  }

  std::string ResolveLanguage(const std::vector<std::string> &tLanguages, const std::string &tPreferredLanguage) {
    const std::string tPreferred = NormalizeLanguageCode(tPreferredLanguage);
    if (!tPreferred.empty() && IsLanguageEnabled(tLanguages, tPreferred)) return tPreferred;
    if (IsLanguageEnabled(tLanguages, "en")) return "en";
    for (const std::string &tLanguage : tLanguages) {
      const std::string tNormalized = NormalizeLanguageCode(tLanguage);
      if (!tNormalized.empty()) return tNormalized;
    }
    if (IsSupportedLanguage("en")) return "en";
    return tPreferred.empty() ? std::string("en") : tPreferred;
  }

  void NormalizeEnabledLanguages(std::vector<std::string> &tLanguages, const std::string &tPreferredLanguage) {
    std::vector<std::string> tNormalizedLanguages;
    auto tAppendUnique = [&](const std::string &tLanguage) {
      const std::string tNormalized = NormalizeLanguageCode(tLanguage);
      if (tNormalized.empty()) return;
      for (const std::string &tExisting : tNormalizedLanguages) {
        if (tExisting == tNormalized) return;
      }
      tNormalizedLanguages.push_back(tNormalized);
    };

    for (const std::string &tLanguage : tLanguages) tAppendUnique(tLanguage);
    if (IsSupportedLanguage("en")) tAppendUnique("en");
    if (tNormalizedLanguages.empty()) tAppendUnique(ResolveLanguage(tNormalizedLanguages, tPreferredLanguage));
    if (tNormalizedLanguages.empty() && IsSupportedLanguage("en")) tAppendUnique("en");
    tLanguages = tNormalizedLanguages;
  }

  struct SLanguageSaveResult {
    int Status = 0;
    std::string Message;
    std::string Language;
    std::vector<std::string> EnabledLanguages;
  };

  SLanguageSaveResult ApplyLanguageSave(
    const std::string &tCurrentLanguage,
    const std::vector<std::string> &tCurrentEnabled,
    bool tHasDefaultLanguage,
    const std::string &tDefaultLanguage,
    bool tHasEnabledLanguages,
    const std::vector<std::string> &tIncomingEnabled,
    bool tSaveOk) {
    SLanguageSaveResult tResult;
    tResult.Language = tCurrentLanguage;
    tResult.EnabledLanguages = tCurrentEnabled;

    std::string tLanguage = NormalizeLanguageCode(tDefaultLanguage);

    if (!tHasDefaultLanguage && !tHasEnabledLanguages) {
      tResult.Status = 400;
      tResult.Message = "language_save_error";
      return tResult;
    }

    if (tHasDefaultLanguage && tLanguage.empty()) {
      tResult.Status = 400;
      tResult.Message = "language_save_error";
      return tResult;
    }

    if (tHasEnabledLanguages) {
      std::vector<std::string> tEnabled = tIncomingEnabled;
      NormalizeEnabledLanguages(tEnabled, tHasDefaultLanguage ? tLanguage : tCurrentLanguage);
      tResult.EnabledLanguages = tEnabled;
      tResult.Language = ResolveLanguage(tResult.EnabledLanguages, tHasDefaultLanguage ? tLanguage : tCurrentLanguage);
    } else if (tHasDefaultLanguage) {
      tResult.Language = tLanguage;
      if (!IsLanguageEnabled(tResult.EnabledLanguages, tResult.Language)) tResult.Language = "en";
    }

    if (!tSaveOk) {
      tResult.Status = 500;
      tResult.Message = "language_save_error";
      return tResult;
    }

    tResult.Status = 200;
    tResult.Message = "language_save_success";
    return tResult;
  }

} // namespace

void test_LanguageSave_rejects_empty_request() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en"}, false, "", false, {}, true);
  TEST_ASSERT_EQUAL_INT(400, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_error", tResult.Message.c_str());
}

void test_LanguageSave_rejects_invalid_default_language() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en"}, true, "   ", false, {}, true);
  TEST_ASSERT_EQUAL_INT(400, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_error", tResult.Message.c_str());
}

void test_LanguageSave_updates_default_only() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en", "hu"}, true, " HU ", false, {}, true);
  TEST_ASSERT_EQUAL_INT(200, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_success", tResult.Message.c_str());
  TEST_ASSERT_EQUAL_STRING("hu", tResult.Language.c_str());
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<unsigned>(tResult.EnabledLanguages.size()));
}

void test_LanguageSave_updates_enabled_languages_and_resolves_default() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en"}, false, "", true, {"fr", "de", "fr"}, true);
  TEST_ASSERT_EQUAL_INT(200, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_success", tResult.Message.c_str());
  TEST_ASSERT_EQUAL_STRING("en", tResult.Language.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, static_cast<unsigned>(tResult.EnabledLanguages.size()));
  TEST_ASSERT_EQUAL_STRING("fr", tResult.EnabledLanguages[0].c_str());
  TEST_ASSERT_EQUAL_STRING("de", tResult.EnabledLanguages[1].c_str());
  TEST_ASSERT_EQUAL_STRING("en", tResult.EnabledLanguages[2].c_str());
}

void test_LanguageSave_default_only_falls_back_when_not_enabled() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en"}, true, "hu", false, {}, true);
  TEST_ASSERT_EQUAL_INT(200, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_success", tResult.Message.c_str());
  TEST_ASSERT_EQUAL_STRING("en", tResult.Language.c_str());
}

void test_LanguageSave_enabled_includes_en_fallback() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en", "hu"}, false, "", true, {"hu"}, true);
  TEST_ASSERT_EQUAL_INT(200, tResult.Status);
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<unsigned>(tResult.EnabledLanguages.size()));
  TEST_ASSERT_EQUAL_STRING("hu", tResult.EnabledLanguages[0].c_str());
  TEST_ASSERT_EQUAL_STRING("en", tResult.EnabledLanguages[1].c_str());
}

void test_LanguageSave_save_failure_returns_server_error() {
  const SLanguageSaveResult tResult = ApplyLanguageSave("en", {"en"}, true, "fr", false, {}, false);
  TEST_ASSERT_EQUAL_INT(500, tResult.Status);
  TEST_ASSERT_EQUAL_STRING("language_save_error", tResult.Message.c_str());
}

void setUp() {}
void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_LanguageSave_rejects_empty_request);
  RUN_TEST(test_LanguageSave_rejects_invalid_default_language);
  RUN_TEST(test_LanguageSave_updates_default_only);
  RUN_TEST(test_LanguageSave_updates_enabled_languages_and_resolves_default);
  RUN_TEST(test_LanguageSave_default_only_falls_back_when_not_enabled);
  RUN_TEST(test_LanguageSave_enabled_includes_en_fallback);
  RUN_TEST(test_LanguageSave_save_failure_returns_server_error);
  return UNITY_END();
}
