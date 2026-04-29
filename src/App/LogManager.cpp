#include <App/LogManager.h>

namespace App {

  LogManager_::LogManager_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  LogManager_::~LogManager_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void LogManager_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void LogManager_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  LogManager_ &LogManager_::Instance() {
    static LogManager_ tInstance;
    return tInstance;
  }

  bool LogManager_::IsEnabledByConfig() const {
    const SDeviceConfig tDeviceConfig = CFG.Get<SDeviceConfig>();
    return tDeviceConfig.LogManagerEnabled;
  }

  void LogManager_::ReloadConfig() {
    Guard tLock;
    const bool tEnable = IsEnabledByConfig();
    if (!tEnable) {
      mEnabled = false;
      mInitialized = false;
      mWriteBufferPos = 0;
      mCurrentFilePath[0] = '\0';
      return;
    }
    mEnabled = true;
    mInitialized = false;
    mCurrentFilePath[0] = '\0';
    mWriteBufferPos = 0;
    if (!IsFileSystemAvailable()) return;
    if (!TryOpenFile()) return;
    mInitialized = true;
  }

  void LogManager_::Init() {
    Guard tLock;
    if (mInitialized) return;
    mEnabled = IsEnabledByConfig();
    if (!mEnabled) return;
    mCurrentFilePath[0] = '\0';
    mWriteBufferPos = 0;
    if (!IsFileSystemAvailable()) {
      xLOG("No filesystem available, logging disabled");
      return;
    }
    if (!TryOpenFile()) {
      xLOG("Log file open failed");
      return;
    }
    mInitialized = true;
  }

  void LogManager_::Flush() {
    Guard tLock;
    if (!mEnabled || !mInitialized || mWriteBufferPos == 0) return;
    FlushBufferToFile();
  }

  void LogManager_::Write(ELogLevel tLevel, const char *tFormat, ...) {
    if (!tFormat) return;
    char tMessage[kLineBufferSize] = "";
    va_list tArgs;
    va_start(tArgs, tFormat);
    vsnprintf(tMessage, sizeof(tMessage) - 1, tFormat, tArgs);
    va_end(tArgs);
    Guard tLock;
    if (!mEnabled || !mInitialized) return;
    char tLine[kLineBufferSize + 48] = "";
    FormatLine(tLine, sizeof(tLine), tLevel, tMessage);
    AppendToBuffer(tLine);
  }

  void LogManager_::Boot(const char *tReason, const char *tMode, const char *tFirmware, uint32_t tBootCount) {
    char tMessage[kLineBufferSize] = "";
    snprintf(tMessage, sizeof(tMessage), "reason=%s, firmware=%s, boot_count=%lu", tReason  ? tReason   : "unknown", tFirmware ? tFirmware : "unknown", (unsigned long)tBootCount);
    Write(ELogLevel::Boot, "%s", tMessage);
    if (tMode && tMode[0]) {
      char tModeMsg[kLineBufferSize] = "";
      snprintf(tModeMsg, sizeof(tModeMsg), "mode=%s, cpu=%uMHz", tMode, (unsigned)ESP.getCpuFreqMHz());
      Write(ELogLevel::Boot, "%s", tModeMsg);
    }
  }

  void LogManager_::Halt(const char *tReason) {
    Write(ELogLevel::Halt, "reason=%s", tReason ? tReason : "unknown");
    Flush();
  }

  void LogManager_::Storage(const char *tName, bool tMounted, uint64_t tTotalBytes, uint64_t tFreeBytes) {
    if (!tMounted) {
      Write(ELogLevel::Storage, "%s mount failed", tName ? tName : "unknown");
      return;
    }
    char tTotal[20] = "";
    char tFree[20] = "";
    UTL.ByteToReadableSize(tTotalBytes, tTotal, sizeof(tTotal));
    UTL.ByteToReadableSize(tFreeBytes,  tFree,  sizeof(tFree));
    Write(ELogLevel::Storage, "%s mounted, total=%s, free=%s", tName ? tName : "unknown", tTotal, tFree);
  }

  void LogManager_::Wifi(bool tConnected, const char *tSsid, const char *tIp, int32_t tRssi) {
    if (!tConnected) {
      Write(ELogLevel::Wifi, "disconnected");
      return;
    }
    Write(ELogLevel::Wifi, "connected, ssid=%s, ip=%s, rssi=%ld", tSsid ? tSsid : "?", tIp   ? tIp   : "?", (long)tRssi);
  }

  void LogManager_::Ntp(bool tSynced, long tOffsetMs, const char *tServer) {
    if (!tSynced) {
      Write(ELogLevel::Ntp, "sync failed");
      return;
    }
    const float tOffsetSec = static_cast<float>(tOffsetMs) / 1000.0f;
    Write(ELogLevel::Ntp, "synced, offset=%+.1fs, server=%s", tOffsetSec, tServer ? tServer : "?");
  }

  void LogManager_::Rtc(bool tSynced, unsigned long tEpoch) {
    if (!tSynced) {
      Write(ELogLevel::Rtc, "sync failed");
      return;
    }
    Write(ELogLevel::Rtc, "synced, epoch=%lu", tEpoch);
  }

  void LogManager_::Battery(uint8_t tPercent, uint16_t tMilliVolts, const char *tState) {
    Write(ELogLevel::Battery, "%u%% [%.2fV], state=%s", (unsigned)tPercent, static_cast<float>(tMilliVolts) / 1000.0f, tState ? tState : "unknown");
  }

  void LogManager_::Image(const char *tFilename, const char *tStorage) {
    Write(ELogLevel::Image, "displayed=%s, storage=%s", tFilename ? tFilename : "none", tStorage  ? tStorage  : "?");
  }

  void LogManager_::Sleep(uint64_t tWakeupInSeconds) {
    Write(ELogLevel::Sleep, "deep_sleep, wakeup_in=%llus", (unsigned long long)tWakeupInSeconds);
  }

  void LogManager_::Dashboard(const char *tEvent, const char *tDetail) {
    if (tDetail && tDetail[0]) Write(ELogLevel::Dashboard, "%s, %s", tEvent ? tEvent : "event", tDetail);
    else Write(ELogLevel::Dashboard, "%s", tEvent ? tEvent : "event");
  }

  void LogManager_::Ota(const char *tEvent, size_t tWritten, size_t tTotal) {
    if (tTotal > 0) Write(ELogLevel::Ota, "%s, written=%u, total=%u", tEvent ? tEvent : "event", (unsigned)tWritten, (unsigned)tTotal);
    else Write(ELogLevel::Ota, "%s", tEvent ? tEvent : "event");
  }

  void LogManager_::Warn(const char *tFormat, ...) {
    if (!tFormat) return;
    char tMessage[kLineBufferSize] = "";
    va_list tArgs;
    va_start(tArgs, tFormat);
    vsnprintf(tMessage, sizeof(tMessage) - 1, tFormat, tArgs);
    va_end(tArgs);
    Write(ELogLevel::Warn, "%s", tMessage);
  }

  void LogManager_::Error(const char *tFormat, ...) {
    if (!tFormat) return;
    char tMessage[kLineBufferSize] = "";
    va_list tArgs;
    va_start(tArgs, tFormat);
    vsnprintf(tMessage, sizeof(tMessage) - 1, tFormat, tArgs);
    va_end(tArgs);
    Write(ELogLevel::Error, "%s", tMessage);
    Flush();
  }

  const char *LogManager_::LevelToString(ELogLevel tLevel) const {
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

  bool LogManager_::FormatTimestamp(char *tBuffer, size_t tSize) const {
    time_t tNow = time(nullptr);
    if (tNow < 1000000000L) {
      const uint32_t tUptimeSec = millis() / 1000;
      snprintf(tBuffer, tSize, "[NO_TIME +%02lu:%02lu:%02lu]", (unsigned long)(tUptimeSec / 3600), (unsigned long)((tUptimeSec % 3600) / 60), (unsigned long)(tUptimeSec % 60));
      return false;
    }
    struct tm tTm = {};
    localtime_r(&tNow, &tTm);
    snprintf(tBuffer, tSize, "[%04d-%02d-%02d %02d:%02d:%02d]", tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday, tTm.tm_hour, tTm.tm_min, tTm.tm_sec);
    return true;
  }

  void LogManager_::FormatLine(char *tOut, size_t tOutSize, ELogLevel tLevel, const char *tMessage) const {
    char tTimestamp[32] = "";
    FormatTimestamp(tTimestamp, sizeof(tTimestamp));
    snprintf(tOut, tOutSize, "%s %s -> %s\n", tTimestamp, LevelToString(tLevel), tMessage ? tMessage : "");
  }

  bool LogManager_::IsFileSystemAvailable() const {
    return STG.IsMounted();
  }

  bool LogManager_::EnsureDirectory(const char *tPath) {
    if (!tPath || !tPath[0]) return false;
    if (STG.Exists(tPath)) return true;
    char tCurrentDirectoryPath[64] = "";
    const char *tCursor = tPath;
    if (*tCursor == '/') tCursor++;
    while (*tCursor) {
      const char *tSeparator = strchr(tCursor, '/');
      size_t tSegmentLength = tSeparator ? static_cast<size_t>(tSeparator - tCursor) : strlen(tCursor);
      if (tSegmentLength == 0) break;
      size_t tCurrentLength = strlen(tCurrentDirectoryPath);
      if (tCurrentLength + 1 + tSegmentLength + 1 > sizeof(tCurrentDirectoryPath)) return false;
      tCurrentDirectoryPath[tCurrentLength] = '/';
      memcpy(tCurrentDirectoryPath + tCurrentLength + 1, tCursor, tSegmentLength);
      tCurrentDirectoryPath[tCurrentLength + 1 + tSegmentLength] = '\0';
      if (!STG.Exists(tCurrentDirectoryPath) && !STG.CreateDir(tCurrentDirectoryPath, false)) return false;
      if (!tSeparator) break;
      tCursor = tSeparator + 1;
    }
    return true;
  }

  bool LogManager_::BuildFilePath(char *tPath, size_t tSize, uint8_t tRollIndex) {
    time_t tNow = time(nullptr);
    struct tm tTm = {};
    if (tNow >= 1000000000L) localtime_r(&tNow, &tTm);
    else memset(&tTm, 0, sizeof(tTm));
    if (tRollIndex == 0) snprintf(tPath, tSize, "%s/%04d/%02d/%02d/%04d%02d%02d.log", kLogsRoot, tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday, tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday);
    else snprintf(tPath, tSize, "%s/%04d/%02d/%02d/%04d%02d%02d_%u.log", kLogsRoot, tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday, tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday, (unsigned)tRollIndex);
    return true;
  }

  bool LogManager_::TryOpenFile() {
    for (uint8_t tRollIndex = 0; tRollIndex < kMaxDailyFileRollIndex; tRollIndex++) {
      char tFilePath[64] = "";
      if (!BuildFilePath(tFilePath, sizeof(tFilePath), tRollIndex)) continue;
      char tDirectoryPath[64] = "";
      strncpy(tDirectoryPath, tFilePath, sizeof(tDirectoryPath) - 1);
      char *tLastSeparator = strrchr(tDirectoryPath, '/');
      if (tLastSeparator) *tLastSeparator = '\0';
      if (!EnsureDirectory(tDirectoryPath)) continue;
      if (STG.Exists(tFilePath)) {
        File tFile = STG.OpenFile(tFilePath, FILE_READ, false);
        size_t tExistingFileSize = tFile ? static_cast<size_t>(tFile.size()) : 0;
        if (tFile) tFile.close();
        if (tExistingFileSize >= kMaxDayFileSizeBytes) continue;
      }
      strncpy(mCurrentFilePath, tFilePath, sizeof(mCurrentFilePath) - 1);
      mCurrentFilePath[sizeof(mCurrentFilePath) - 1] = '\0';
      return true;
    }
    return false;
  }

  void LogManager_::AppendToBuffer(const char *tLine) {
    if (!tLine) return;
    const size_t tLen = strlen(tLine);
    if (mWriteBufferPos + tLen >= kWriteBufferSize) FlushBufferToFile();
    if (tLen < kWriteBufferSize) {
      memcpy(mWriteBuffer + mWriteBufferPos, tLine, tLen);
      mWriteBufferPos += tLen;
    }
  }

  void LogManager_::FlushBufferToFile() {
    if (mWriteBufferPos == 0 || !mCurrentFilePath[0]) return;
    char tCurrentExpected[64] = "";
    BuildFilePath(tCurrentExpected, sizeof(tCurrentExpected));
    if (strcmp(tCurrentExpected, mCurrentFilePath) != 0) {
      mCurrentFilePath[0] = '\0';
      if (!TryOpenFile()) {
        mWriteBufferPos = 0;
        return;
      }
    }
    File tFile = STG.OpenFile(mCurrentFilePath, FILE_APPEND, true);
    if (!tFile) {
      xLOG("Log file open for append failed: %s", mCurrentFilePath);
      mWriteBufferPos = 0;
      return;
    }
    tFile.write(reinterpret_cast<const uint8_t *>(mWriteBuffer), mWriteBufferPos);
    tFile.close();
    mWriteBufferPos = 0;
  }

}
