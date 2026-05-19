#ifndef UTILS_H
#define UTILS_H

#include <App/Global.h>

namespace App {

  enum class EDisplayRotate : uint16_t;

  enum class EUtilsInfoType : uint8_t { 
    Header = 1, 
    Title, 
    Cell,
    Line,
    Footer,
    Single
  };

  class Utils_ {
    DEFINE_TAG("CFG");
    friend class AutoGuard<Utils_>;
    public:
      using Guard = AutoGuard<Utils_>;
      static Utils_ &Instance();
      void Init();
      void ReloadConfig();
      static void SetCPUFrequency(ECPUFrequency tFrequency = ECPUFrequency::F240MHz);
      static void DisableBT();
      static void DisableBrownout();
      static void ByteToReadableSize(uint64_t tBytes, char *tBuffer, size_t tLength);
      const char *EpochToReadableFormat(unsigned long tEpoch, bool tAsDateTime, char *tBuffer, size_t tLength);
      void PrintPartitionInfo();
      void PrintBootInfo();
      static const char *ResolveBootReason();
      void PrintWakeupReason();
      void PrintDeviceInfo();
      void PrintInfo(const char *tText, EUtilsInfoType tType = EUtilsInfoType::Cell, uint8_t tWidth = 0);
      void PrintMemoryInfo();
      void SetPrintInfoWidth(uint8_t tWidth) { mPrintInfoWidth = tWidth; }
      uint8_t GetPrintInfoWidth() const { return mPrintInfoWidth; }
      const char *PrependSlash(const char *tPath, char *tOutBuffer, size_t tBufSize);
      void SleepLowBattery();
      void SleepAndWakeup();
      uint64_t SecondsUntilHour(uint8_t tTargetHour);
      static bool WasWokenByPin(uint8_t tPin);
      static bool WasWokenByRtcAlarm();
      static const char *GetLastWakeSourceKey();
      static void SetBootRtcReady(bool tReady);
      static bool WasBootRtcReady();
      static bool SecureStrcmp(const char *tA, const char *tB);
      static const char *GetLeafName(const char *tPath);
      static bool EqualsIgnoreCase(const char *tLeft, const char *tRight);
      static EDisplayRotate ResolveDisplayRotate(uint16_t tRotate);
      static uint32_t SafeAtoul(const char *tStr, uint32_t tMinVal, uint32_t tMaxVal, uint32_t tDefaultVal);
      static bool HasElapsedMs(uint32_t tStartMs, uint32_t tNowMs, uint32_t tDelayMs);
      static String NormalizeLanguageCode(const String &tLanguage);
      static bool IsLanguageEnabled(const std::vector<String> &tLanguages, const String &tLanguage);
      static String ResolveLanguage(const std::vector<String> &tLanguages, const String &tPreferredLanguage = "en");
      static void NormalizeEnabledLanguages(std::vector<String> &tLanguages, const String &tPreferredLanguage = "en");
      static std::vector<String> ParseEnabledLanguages(const String &tValue, const String &tPreferredLanguage = "en");
      static String JoinEnabledLanguages(const std::vector<String> &tLanguages, const String &tPreferredLanguage = "en");
    private:
      Utils_();
      Utils_(const Utils_&) = delete;
      Utils_ &operator=(const Utils_&) = delete;
      ~Utils_(); 
      mutable SemaphoreHandle_t mMutex;
      SAppConfig mCfg {};
      uint8_t mPrintInfoWidth = 44;
      static void Lock();
      static void Unlock();
      void PrintChipInfo();
      void PrintFlashInfo();
      void PrintNvsUsageInfo();
      void PrintRamInfo();
      void PrintDRamUsageInfo();
      void PrintIRamUsageInfo();
      void PrintPSRamUsageInfo();
      void PrintSketchInfo();
      void PrintFileSystemInfo();
      void PrintResourceInfo();
      void PrintRadioInfo();
  };

}

#endif
