#ifndef UTILS_H
#define UTILS_H

#include <App/Global.h>

namespace App {

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
      void PrintWakeupReason();
      void PrintDeviceInfo();
      void PrintInfo(const char *tText, EUtilsInfoType tType = EUtilsInfoType::Cell, uint8_t tWidth = 0);
      void PrintMemoryInfo();
      void PrintDateTime();
      void SetPrintInfoWidth(uint8_t tWidth) { mPrintInfoWidth = tWidth; }
      uint8_t GetPrintInfoWidth() const { return mPrintInfoWidth; }
      const char *PrependSlash(const char *tPath, char *tOutBuffer, size_t tBufSize);
      void SleepLowBattery();
      void SleepAndWakeup();
      uint64_t SecondsUntilHour(uint8_t tTargetHour);
      static bool WasWokenByPin(uint8_t tPin);
      static bool SecureStrcmp(const char *tA, const char *tB);
      static uint32_t SafeAtoul(const char *tStr, uint32_t tMinVal, uint32_t tMaxVal, uint32_t tDefaultVal);
      static bool IsSD(const char *tTarget);
      static bool IsLFS(const char *tTarget);
      static bool IsValidTarget(const char *tTarget);
      static bool IsSameTarget(const char *tA, const char *tB);
      static bool GlobMatch(const char *tPattern, const char *tText);
      static bool SplitPathAndFile(const char *tSpec, char *tDir, size_t tDirSize, char *tFile, size_t tFileSize);
      static void CollectMatchingFiles(const char *tDir, const char *tPattern, bool tIsSD, std::vector<String> &tFiles);
      static void ResolveFileSpec(const char *tDir, const char *tSpec, bool tIsSD, std::vector<String> &tFiles);
    private:
      Utils_();
      Utils_(const Utils_&) = delete;
      Utils_ &operator=(const Utils_&) = delete;
      ~Utils_(); 
      mutable SemaphoreHandle_t mMutex;
      SAppConfig mCfg {};
      uint8_t mPrintInfoWidth = 44;
      uint16_t mVref = 1100;
      static void Lock();
      static void Unlock();
      void PrintChipInfo();
      void PrintFlashInfo();
      void PrintNvsUsageInfo();
      void PrintRamInfo();
      void PrintDRamUsageInfo();
      void PrintIRamUsageInfo();
      void PrintPSRamInfo();
      void PrintPSRamUsageInfo();
      void PrintSketchInfo();
      void PrintFileSystemInfo();
      void PrintResourceInfo();
      void PrintRadioInfo();
  };

}

#endif
