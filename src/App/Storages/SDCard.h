#ifndef SDCARD_H
#define SDCARD_H

#include <App/Global.h>

namespace App {

  class SDCard_ {
    DEFINE_TAG("SDC");
    friend class AutoGuard<SDCard_>;
    public:
      using Guard = AutoGuard<SDCard_>;
      static SDCard_ &Instance();
      bool Init(bool tVerbose = false);
      void ReloadConfig();
      bool IsMounted();
      File OpenFile(const char *tPath, const char *tMode = FILE_READ, bool tCreate = false);
      const char *ReadFile(const char *tPath, const char *tMode = FILE_READ);
      bool WriteFile(const char *tPath, const char *tData, bool tVerbose = false);
      bool DeleteFile(const char *tPath);
      bool Exists(const char *tPath);
      bool CreateDir(const char *tPath, bool tVerbose = false);
      bool DeleteDir(const char *tPath);
      static const char *NormalizePath(const char *tPath);
      static const char *GetFileName(const char *tPath);
      const char *ListDir(const char *tPath = "/");
      const char *GetNextFile();
      const char *GetNextFile(const char *tCurrentFilename, const char *tDir = IMAGES_DIR, const char *tExt = ".jpg");
      static std::vector<const char *> GetFilesInDir(const char *tDir, const char *tExt);
      static void InvalidateFileCache();
      void BootstrapVault(bool tVerbose = false);
      bool Format(volatile uint8_t *tProgress = nullptr);
      void PrintListDir(size_t tMaxLines = 15);
      void End();
      size_t GetListPos() { return mListPos; };
      uint64_t TotalBytes();
      uint64_t UsedBytes();
      uint8_t CardType();
      const char *CardTypeName();
    private:
      SDCard_();
      SDCard_(const SDCard_&) = delete;
      SDCard_ &operator=(const SDCard_&) = delete;
      ~SDCard_();
      bool mMounted = false;
      mutable SemaphoreHandle_t mMutex = nullptr;
      SAppConfig mCfg {};
      static constexpr uint8_t kSckPin = SD_SCK_PIN;
      static constexpr uint8_t kMisoPin = SD_MISO_PIN;
      static constexpr uint8_t kMosiPin = SD_MOSI_PIN;
      static constexpr uint8_t kCsPin = SD_CS_PIN;
      static constexpr uint32_t kSpiSpeed = 4000000;
      static constexpr const char *kMountPoint = "/sdc";
      static constexpr uint8_t kMaxFiles = 10;
      static char mReadBuffer[4096];
      static bool mReadValid;
      static char mListBuffer[4096];
      static char mFileBuffer[4096];
      static size_t mListPos;
      static std::vector<const char*> mFileList;
      static size_t mFilesCount;
      static char mFilesLastDir[128];
      static char mFilesLastExt[16];
      size_t CountEntriesRecursive(const char *tDirPath);
      bool WipeDirRecursive(const char *tDirPath, volatile uint8_t *tProgress, size_t tTotalEntries, size_t *tProcessedEntries);
      static void Lock();
      static void Unlock();
      void AppendToBuffer(const char *tData, size_t tLength);
  };

}

#endif
