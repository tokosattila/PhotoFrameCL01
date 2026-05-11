#include <unity.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace {

  // ---------------------------------------------------------------------------
  // Mirrors src/App/Dashboard/Assets/Js/Logs.h Pad2 + FormatDayFileName.
  // ---------------------------------------------------------------------------
  std::string Pad2(int tValue) {
    char tBuffer[8] = {};
    snprintf(tBuffer, sizeof(tBuffer), "%02d", tValue);
    return std::string(tBuffer);
  }

  std::string FormatDayFileName(int tYear, int tMonth, int tDay) {
    char tBuffer[24] = {};
    snprintf(tBuffer, sizeof(tBuffer), "%04d%02d%02d.log", tYear, tMonth, tDay);
    return std::string(tBuffer);
  }

  // ---------------------------------------------------------------------------
  // Mirrors LogManager_::BuildFilePath in src/App/LogManager.cpp.
  // ---------------------------------------------------------------------------
  std::string BuildLogFilePath(const char *tRoot, int tYear, int tMonth, int tDay, int tRollIndex) {
    char tBuffer[96] = {};
    if (tRollIndex == 0) snprintf(tBuffer, sizeof(tBuffer), "/%s/%04d/%02d/%02d/%04d%02d%02d.log", tRoot, tYear, tMonth, tDay, tYear, tMonth, tDay);
    else snprintf(tBuffer, sizeof(tBuffer), "/%s/%04d/%02d/%02d/%04d%02d%02d_%u.log", tRoot, tYear, tMonth, tDay, tYear, tMonth, tDay, (unsigned)tRollIndex);
    return std::string(tBuffer);
  }

  // ---------------------------------------------------------------------------
  // Mirrors ReadDayContent date guard.
  // ---------------------------------------------------------------------------
  bool IsValidLogDate(int tYear, int tMonth, int tDay) {
    if (tYear < 2000 || tYear > 2999) return false;
    if (tMonth < 1 || tMonth > 12) return false;
    if (tDay < 1 || tDay > 31) return false;
    return true;
  }

  // ---------------------------------------------------------------------------
  // Mirrors LogManager_::ListAvailableDates sort comparator (descending).
  // ---------------------------------------------------------------------------
  struct SLogDate {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
  };

  void SortLogDatesDescending(std::vector<SLogDate> &tDates) {
    std::sort(tDates.begin(), tDates.end(), [](const SLogDate &tLeft, const SLogDate &tRight) {
      if (tLeft.Year != tRight.Year) return tLeft.Year > tRight.Year;
      if (tLeft.Month != tRight.Month) return tLeft.Month > tRight.Month;
      return tLeft.Day > tRight.Day;
    });
  }

  // ---------------------------------------------------------------------------
  // Mirrors JS RenderContent: strips trailing CR/LF then splits on \r?\n.
  // ---------------------------------------------------------------------------
  std::vector<std::string> SplitLogLines(const std::string &tInput) {
    std::vector<std::string> tResult;
    if (tInput.empty()) return tResult;
    size_t tEnd = tInput.size();
    while (tEnd > 0 && (tInput[tEnd - 1] == '\n' || tInput[tEnd - 1] == '\r')) tEnd--;
    std::string tTrimmed = tInput.substr(0, tEnd);
    if (tTrimmed.empty()) return tResult;
    size_t tStart = 0;
    for (size_t tIndex = 0; tIndex <= tTrimmed.size(); tIndex++) {
      if (tIndex == tTrimmed.size() || tTrimmed[tIndex] == '\n') {
        std::string tLine = tTrimmed.substr(tStart, tIndex - tStart);
        if (!tLine.empty() && tLine.back() == '\r') tLine.pop_back();
        tResult.push_back(tLine);
        tStart = tIndex + 1;
      }
    }
    return tResult;
  }

  // ---------------------------------------------------------------------------
  // Mock filesystem mirroring STG semantics enough to validate DeleteRecursive
  // (post-order delete of files first, then containing directories).
  // ---------------------------------------------------------------------------
  struct MockFs {
    // path -> isDirectory
    std::map<std::string, bool> Entries;
    int FileDeleteCount = 0;
    int DirDeleteCount = 0;

    void AddFile(const std::string &tPath) { Entries[tPath] = false; EnsureParents(tPath); }
    void AddDir(const std::string &tPath) { Entries[tPath] = true; EnsureParents(tPath); }
    void EnsureParents(const std::string &tPath) {
      size_t tPos = tPath.find_last_of('/');
      if (tPos == std::string::npos || tPos == 0) return;
      std::string tParent = tPath.substr(0, tPos);
      if (!tParent.empty()) Entries[tParent] = true;
      EnsureParents(tParent);
    }
    bool Exists(const std::string &tPath) const { return Entries.count(tPath) > 0; }
    bool IsDir(const std::string &tPath) const {
      auto tIt = Entries.find(tPath);
      return tIt != Entries.end() && tIt->second;
    }
    std::vector<std::string> Children(const std::string &tPath) const {
      std::vector<std::string> tResult;
      const std::string tPrefix = tPath + "/";
      for (const auto &tPair : Entries) {
        const std::string &tEntry = tPair.first;
        if (tEntry.size() <= tPrefix.size()) continue;
        if (tEntry.compare(0, tPrefix.size(), tPrefix) != 0) continue;
        if (tEntry.find('/', tPrefix.size()) != std::string::npos) continue;
        tResult.push_back(tEntry);
      }
      return tResult;
    }
    bool DeleteFile(const std::string &tPath) {
      auto tIt = Entries.find(tPath);
      if (tIt == Entries.end() || tIt->second) return false;
      Entries.erase(tIt);
      FileDeleteCount++;
      return true;
    }
    bool DeleteDir(const std::string &tPath) {
      auto tIt = Entries.find(tPath);
      if (tIt == Entries.end() || !tIt->second) return false;
      if (!Children(tPath).empty()) return false;
      Entries.erase(tIt);
      DirDeleteCount++;
      return true;
    }
  };

  bool DeleteRecursive(MockFs &tFs, const std::string &tPath) {
    if (tPath.empty()) return false;
    if (!tFs.Exists(tPath)) return true;
    if (!tFs.IsDir(tPath)) return tFs.DeleteFile(tPath);
    bool tAllOk = true;
    auto tChildren = tFs.Children(tPath);
    for (const auto &tChild : tChildren) {
      if (tFs.IsDir(tChild)) {
        if (!DeleteRecursive(tFs, tChild)) tAllOk = false;
      } else {
        if (!tFs.DeleteFile(tChild)) tAllOk = false;
      }
    }
    if (!tFs.DeleteDir(tPath)) tAllOk = false;
    return tAllOk;
  }

  // ---------------------------------------------------------------------------
  // Mirrors AsyncWebServer prefix match semantics for AsyncCallbackWebHandler:
  // a route registered with URI U matches request URL R if R == U or R starts
  // with (U + "/"). With first-match-wins, more specific routes must be
  // registered BEFORE generic ones.
  // ---------------------------------------------------------------------------
  std::string ResolveRoute(const std::vector<std::string> &tRegisteredRoutes, const std::string &tRequestUrl) {
    for (const auto &tRoute : tRegisteredRoutes) {
      if (tRequestUrl == tRoute) return tRoute;
      if (tRequestUrl.size() > tRoute.size() && tRequestUrl.compare(0, tRoute.size() + 1, tRoute + "/") == 0) return tRoute;
    }
    return "";
  }

  // ---------------------------------------------------------------------------
  // Mirrors LogManager_::LevelToString.
  // ---------------------------------------------------------------------------
  enum class ELogLevel : uint8_t {
    Boot = 0, Halt, Storage, Wifi, Ntp, Rtc, Battery, Image, Sleep, Dashboard, Ota, Warn, Error
  };

  const char *LevelToString(ELogLevel tLevel) {
    switch (tLevel) {
      case ELogLevel::Boot: return "BOOT";
      case ELogLevel::Halt: return "HALT";
      case ELogLevel::Storage: return "STORAGE";
      case ELogLevel::Wifi: return "WIFI";
      case ELogLevel::Ntp: return "NTP";
      case ELogLevel::Rtc: return "RTC";
      case ELogLevel::Battery: return "BATTERY";
      case ELogLevel::Image: return "IMAGE";
      case ELogLevel::Sleep: return "SLEEP";
      case ELogLevel::Dashboard: return "DASH";
      case ELogLevel::Ota: return "FIRMW";
      case ELogLevel::Warn: return "WARN";
      case ELogLevel::Error: return "ERROR";
      default: return "LOG";
    }
  }

  // ---------------------------------------------------------------------------
  // Storage key normalization (matches JSON "Storage":"sd_card"|"littlefs").
  // ---------------------------------------------------------------------------
  std::string ResolveStorageKey(bool tIsSdCard) {
    return tIsSdCard ? "sd_card" : "littlefs";
  }

} // namespace

// ===========================================================================
// Filename / path tests
// ===========================================================================

void test_FormatDayFileName_pads_month_and_day() {
  TEST_ASSERT_EQUAL_STRING("20260101.log", FormatDayFileName(2026, 1, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("20261231.log", FormatDayFileName(2026, 12, 31).c_str());
  TEST_ASSERT_EQUAL_STRING("20260509.log", FormatDayFileName(2026, 5, 9).c_str());
}

void test_BuildLogFilePath_first_roll_omits_index() {
  std::string tPath = BuildLogFilePath("logs", 2026, 5, 11, 0);
  TEST_ASSERT_EQUAL_STRING("/logs/2026/05/11/20260511.log", tPath.c_str());
}

void test_BuildLogFilePath_rolled_files_get_underscore_index() {
  std::string tPath = BuildLogFilePath("logs", 2026, 5, 11, 3);
  TEST_ASSERT_EQUAL_STRING("/logs/2026/05/11/20260511_3.log", tPath.c_str());
}

// ===========================================================================
// Date validation tests
// ===========================================================================

void test_IsValidLogDate_accepts_realistic_value() {
  TEST_ASSERT_TRUE(IsValidLogDate(2026, 5, 11));
  TEST_ASSERT_TRUE(IsValidLogDate(2000, 1, 1));
  TEST_ASSERT_TRUE(IsValidLogDate(2999, 12, 31));
}

void test_IsValidLogDate_rejects_out_of_range() {
  TEST_ASSERT_FALSE(IsValidLogDate(1999, 5, 11));
  TEST_ASSERT_FALSE(IsValidLogDate(3000, 5, 11));
  TEST_ASSERT_FALSE(IsValidLogDate(2026, 0, 11));
  TEST_ASSERT_FALSE(IsValidLogDate(2026, 13, 11));
  TEST_ASSERT_FALSE(IsValidLogDate(2026, 5, 0));
  TEST_ASSERT_FALSE(IsValidLogDate(2026, 5, 32));
}

// ===========================================================================
// Sort tests
// ===========================================================================

void test_SortLogDates_orders_newest_first() {
  std::vector<SLogDate> tDates = {
    {2025, 12, 31},
    {2026,  1,  5},
    {2026,  1,  1},
    {2026,  5, 11},
    {2024,  6, 15}
  };
  SortLogDatesDescending(tDates);
  TEST_ASSERT_EQUAL_INT(2026, tDates[0].Year);
  TEST_ASSERT_EQUAL_INT(5, tDates[0].Month);
  TEST_ASSERT_EQUAL_INT(11, tDates[0].Day);
  TEST_ASSERT_EQUAL_INT(2026, tDates[1].Year);
  TEST_ASSERT_EQUAL_INT(1, tDates[1].Month);
  TEST_ASSERT_EQUAL_INT(5, tDates[1].Day);
  TEST_ASSERT_EQUAL_INT(2026, tDates[2].Year);
  TEST_ASSERT_EQUAL_INT(1, tDates[2].Month);
  TEST_ASSERT_EQUAL_INT(1, tDates[2].Day);
  TEST_ASSERT_EQUAL_INT(2025, tDates[3].Year);
  TEST_ASSERT_EQUAL_INT(2024, tDates[4].Year);
}

// ===========================================================================
// Trailing-newline trim (mirrors JS RenderContent)
// ===========================================================================

void test_SplitLogLines_strips_single_trailing_newline() {
  auto tLines = SplitLogLines("alpha\nbeta\ngamma\n");
  TEST_ASSERT_EQUAL_INT(3, (int)tLines.size());
  TEST_ASSERT_EQUAL_STRING("alpha", tLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("beta", tLines[1].c_str());
  TEST_ASSERT_EQUAL_STRING("gamma", tLines[2].c_str());
}

void test_SplitLogLines_strips_repeated_trailing_newlines_and_carriage_returns() {
  auto tLines = SplitLogLines("alpha\r\nbeta\r\n\r\n\r\n");
  TEST_ASSERT_EQUAL_INT(2, (int)tLines.size());
  TEST_ASSERT_EQUAL_STRING("alpha", tLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("beta", tLines[1].c_str());
}

void test_SplitLogLines_keeps_empty_input_empty() {
  auto tLines = SplitLogLines("");
  TEST_ASSERT_EQUAL_INT(0, (int)tLines.size());
}

void test_SplitLogLines_keeps_internal_blank_lines() {
  auto tLines = SplitLogLines("alpha\n\ngamma\n");
  TEST_ASSERT_EQUAL_INT(3, (int)tLines.size());
  TEST_ASSERT_EQUAL_STRING("alpha", tLines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("", tLines[1].c_str());
  TEST_ASSERT_EQUAL_STRING("gamma", tLines[2].c_str());
}

// ===========================================================================
// Recursive delete on mock filesystem
// ===========================================================================

void test_DeleteRecursive_removes_files_and_dirs_post_order() {
  MockFs tFs;
  tFs.AddFile("/logs/2026/05/11/20260511.log");
  tFs.AddFile("/logs/2026/05/11/20260511_1.log");
  tFs.AddFile("/logs/2026/05/12/20260512.log");
  tFs.AddFile("/logs/2025/12/31/20251231.log");

  bool tOk = DeleteRecursive(tFs, "/logs");
  TEST_ASSERT_TRUE(tOk);
  TEST_ASSERT_FALSE(tFs.Exists("/logs"));
  TEST_ASSERT_FALSE(tFs.Exists("/logs/2026"));
  TEST_ASSERT_FALSE(tFs.Exists("/logs/2026/05/11/20260511.log"));
  TEST_ASSERT_EQUAL_INT(4, tFs.FileDeleteCount);
  TEST_ASSERT_TRUE(tFs.DirDeleteCount >= 4);
}

void test_DeleteRecursive_returns_true_for_missing_path() {
  MockFs tFs;
  TEST_ASSERT_TRUE(DeleteRecursive(tFs, "/logs"));
}

void test_DeleteRecursive_handles_single_file() {
  MockFs tFs;
  tFs.AddFile("/logs/2026/05/11/20260511.log");
  TEST_ASSERT_TRUE(DeleteRecursive(tFs, "/logs/2026/05/11/20260511.log"));
  TEST_ASSERT_FALSE(tFs.Exists("/logs/2026/05/11/20260511.log"));
  TEST_ASSERT_TRUE(tFs.Exists("/logs/2026/05/11"));
}

// ===========================================================================
// Route ordering — regression for /api/logs/day vs /api/logs prefix bug
// ===========================================================================

void test_ResolveRoute_specific_must_be_registered_first() {
  std::vector<std::string> tRoutes = {
    "/api/logs/day",
    "/api/logs/download",
    "/api/logs/delete-all",
    "/api/logs"
  };
  TEST_ASSERT_EQUAL_STRING("/api/logs/day", ResolveRoute(tRoutes, "/api/logs/day").c_str());
  TEST_ASSERT_EQUAL_STRING("/api/logs/download", ResolveRoute(tRoutes, "/api/logs/download").c_str());
  TEST_ASSERT_EQUAL_STRING("/api/logs/delete-all", ResolveRoute(tRoutes, "/api/logs/delete-all").c_str());
  TEST_ASSERT_EQUAL_STRING("/api/logs", ResolveRoute(tRoutes, "/api/logs").c_str());
}

void test_ResolveRoute_generic_first_swallows_specific_routes() {
  // This is the BUG: when /api/logs is registered first, prefix-matching causes
  // /api/logs/day to incorrectly resolve to /api/logs (returning JSON list).
  std::vector<std::string> tRoutes = {
    "/api/logs",
    "/api/logs/day",
    "/api/logs/download",
    "/api/logs/delete-all"
  };
  TEST_ASSERT_EQUAL_STRING("/api/logs", ResolveRoute(tRoutes, "/api/logs/day").c_str());
}

// ===========================================================================
// Level-to-string mapping
// ===========================================================================

void test_LevelToString_known_levels() {
  TEST_ASSERT_EQUAL_STRING("BOOT", LevelToString(ELogLevel::Boot));
  TEST_ASSERT_EQUAL_STRING("HALT", LevelToString(ELogLevel::Halt));
  TEST_ASSERT_EQUAL_STRING("STORAGE", LevelToString(ELogLevel::Storage));
  TEST_ASSERT_EQUAL_STRING("WIFI", LevelToString(ELogLevel::Wifi));
  TEST_ASSERT_EQUAL_STRING("NTP", LevelToString(ELogLevel::Ntp));
  TEST_ASSERT_EQUAL_STRING("RTC", LevelToString(ELogLevel::Rtc));
  TEST_ASSERT_EQUAL_STRING("BATTERY", LevelToString(ELogLevel::Battery));
  TEST_ASSERT_EQUAL_STRING("IMAGE", LevelToString(ELogLevel::Image));
  TEST_ASSERT_EQUAL_STRING("SLEEP", LevelToString(ELogLevel::Sleep));
  TEST_ASSERT_EQUAL_STRING("DASH", LevelToString(ELogLevel::Dashboard));
  TEST_ASSERT_EQUAL_STRING("FIRMW", LevelToString(ELogLevel::Ota));
  TEST_ASSERT_EQUAL_STRING("WARN", LevelToString(ELogLevel::Warn));
  TEST_ASSERT_EQUAL_STRING("ERROR", LevelToString(ELogLevel::Error));
}

// ===========================================================================
// Storage key for /api/logs JSON
// ===========================================================================

void test_ResolveStorageKey_returns_sd_card_when_sd_active() {
  TEST_ASSERT_EQUAL_STRING("sd_card", ResolveStorageKey(true).c_str());
}

void test_ResolveStorageKey_returns_littlefs_when_sd_inactive() {
  TEST_ASSERT_EQUAL_STRING("littlefs", ResolveStorageKey(false).c_str());
}

// ===========================================================================
// Unity entry points
// ===========================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_FormatDayFileName_pads_month_and_day);
  RUN_TEST(test_BuildLogFilePath_first_roll_omits_index);
  RUN_TEST(test_BuildLogFilePath_rolled_files_get_underscore_index);
  RUN_TEST(test_IsValidLogDate_accepts_realistic_value);
  RUN_TEST(test_IsValidLogDate_rejects_out_of_range);
  RUN_TEST(test_SortLogDates_orders_newest_first);
  RUN_TEST(test_SplitLogLines_strips_single_trailing_newline);
  RUN_TEST(test_SplitLogLines_strips_repeated_trailing_newlines_and_carriage_returns);
  RUN_TEST(test_SplitLogLines_keeps_empty_input_empty);
  RUN_TEST(test_SplitLogLines_keeps_internal_blank_lines);
  RUN_TEST(test_DeleteRecursive_removes_files_and_dirs_post_order);
  RUN_TEST(test_DeleteRecursive_returns_true_for_missing_path);
  RUN_TEST(test_DeleteRecursive_handles_single_file);
  RUN_TEST(test_ResolveRoute_specific_must_be_registered_first);
  RUN_TEST(test_ResolveRoute_generic_first_swallows_specific_routes);
  RUN_TEST(test_LevelToString_known_levels);
  RUN_TEST(test_ResolveStorageKey_returns_sd_card_when_sd_active);
  RUN_TEST(test_ResolveStorageKey_returns_littlefs_when_sd_inactive);
  return UNITY_END();
}
