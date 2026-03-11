#ifndef SDCARD_H
#define SDCARD_H

#include <App/Global.h>

namespace App {

  using FSDCardCallback = FDefaultCallback;

  class SDCard_ {
    DEFINE_TAG("SDC");
    friend class AutoGuard<SDCard_>;
    public:
      using Guard = AutoGuard<SDCard_>;
      static SDCard_ &Instance();
      bool Init(bool tVerbose = false);
      void ReloadConfig();
      void Callback(FSDCardCallback tCallback);
      bool IsMounted();
      const char *GetName() const { return "SDCard"; }
      EFileSystemType GetType() const { return EFileSystemType::SDCard; }
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
      const char *CatFile(const char *tPath);
      const char *GetNextFile();
      const char *GetNextFile(const char *tCurrentFilename, const char *tDir = IMAGES_DIR, const char *tExt = ".jpg");
      static std::vector<const char *> GetFilesInDir(const char *tDir, const char *tExt);
      void BootstrapVault(bool tVerbose = false);
      void PrintListDir();
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
      FSDCardCallback mCallback = nullptr;
      SAppConfig mCfg {};
      const uint8_t mSckPin = SD_SCK_PIN;
      const uint8_t mMisoPin = SD_MISO_PIN;
      const uint8_t mMosiPin = SD_MOSI_PIN;
      const uint8_t mCsPin = SD_CS_PIN;
      const uint32_t mSpiSpeed = 4000000;
      const char *mMountPoint = "/sdc";
      const uint8_t mMaxFiles = 10;
      static char mReadBuffer[4096];
      static bool mReadValid;
      static char mListBuffer[4096];
      static char mFileBuffer[4096];
      static size_t mListPos;
      static std::vector<const char*> mFileList;
      static size_t mFilesCount;
      static char mFilesLastDir[128];
      static char mFilesLastExt[16];
      static void Lock();
      static void Unlock();
      void AppendToBuffer(const char *tData, size_t tLength);
  };

}

#endif
