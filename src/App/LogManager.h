#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <App/Global.h>

namespace App {

  struct SLogDate {
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
  };

  enum class ELogLevel : uint8_t {
    Boot = 0,
    Halt,
    Storage,
    Wifi,
    Ntp,
    Rtc,
    Battery,
    Image,
    Sleep,
    Dashboard,
    Ota,
    Warn,
    Error
  };

  class LogManager_ {
    DEFINE_TAG("LOG");
    friend class AutoGuard<LogManager_>;
    public:
      using Guard = AutoGuard<LogManager_>;
      static LogManager_ &Instance();
      void Init();
      void ReloadConfig();
      void Flush();
      void Write(ELogLevel tLevel, const char *tFormat, ...);
      void Boot(const char *tReason, const char *tMode, const char *tFirmware, uint32_t tBootCount);
      void Halt(const char *tReason);
      void Storage(const char *tName, bool tMounted, uint64_t tTotalBytes, uint64_t tFreeBytes);
      void Wifi(bool tConnected, const char *tSsid = nullptr, const char *tIp = nullptr, int32_t tRssi = 0);
      void Ntp(bool tSynced, long tOffsetMs = 0, const char *tServer = nullptr);
      void Rtc(bool tSynced, unsigned long tEpoch = 0);
      void Battery(uint8_t tPercent, uint16_t tMilliVolts, const char *tState);
      void Image(const char *tFilename, const char *tStorage);
      void Sleep(uint64_t tWakeupInSeconds);
      void Dashboard(const char *tEvent, const char *tDetail = nullptr);
      void Ota(const char *tEvent, size_t tWritten = 0, size_t tTotal = 0);
      void Warn(const char *tFormat, ...);
      void Error(const char *tFormat, ...);
      bool ListAvailableDates(std::vector<SLogDate> &tOut);
      bool ReadDayContent(uint16_t tYear, uint8_t tMonth, uint8_t tDay, String &tOut, size_t tMaxBytes = 256 * 1024);
      bool DeleteAllLogs();
    private:
      LogManager_();
      LogManager_(const LogManager_&) = delete;
      LogManager_ &operator=(const LogManager_&) = delete;
      ~LogManager_();
      static void Lock();
      static void Unlock();
      mutable SemaphoreHandle_t mMutex = nullptr;
      static constexpr const char *kLogsRoot = LOGS_DIR;
      static constexpr size_t kLineBufferSize = 192;
      static constexpr size_t kWriteBufferSize = 4 * 1024;
      static constexpr size_t kMaxDayFileSizeBytes = 512 * 1024;
      static constexpr uint8_t kMaxDailyFileRollIndex = 10;
      bool mInitialized = false;
      bool mEnabled = true;
      char mCurrentFilePath[64] = {};
      char mWriteBuffer[kWriteBufferSize] = {};
      size_t mWriteBufferPos = 0;
      bool IsEnabledByConfig() const;
      bool TryOpenFile();
      bool BuildFilePath(char *tPath, size_t tSize, uint8_t tRollIndex = 0);
      bool EnsureDirectory(const char *tPath);
      void AppendToBuffer(const char *tLine);
      void FlushBufferToFile();
      bool IsFileSystemAvailable() const;
      bool DeleteRecursive(const char *tPath);
      const char *LevelToString(ELogLevel tLevel) const;
      bool FormatTimestamp(char *tBuffer, size_t tSize) const;
      void FormatLine(char *tOut, size_t tOutSize, ELogLevel tLevel, const char *tMessage) const;
  };

}

#endif
