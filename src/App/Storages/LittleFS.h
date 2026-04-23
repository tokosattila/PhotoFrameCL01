#ifndef LITTLEFS_H
#define LITTLEFS_H

#include <App/Global.h>

namespace App {

  class LittleFS_ {
    DEFINE_TAG("LFS");
    friend class AutoGuard<LittleFS_>;
    public:
      using Guard = AutoGuard<LittleFS_>;
      static LittleFS_ &Instance();
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
      static std::vector<const char*> GetFilesInDir(const char *tDir, const char *tExt);
      static void InvalidateFileCache();
      void BootstrapVault(bool tVerbose = false);
      bool Format();
      void PrintListDir(size_t tMaxLines = 15);
      void End();
      size_t GetListPos() { return mListPos; };
      uint64_t TotalBytes();
      uint64_t UsedBytes();
    private:
      LittleFS_();
      LittleFS_(const LittleFS_&) = delete;
      LittleFS_ &operator=(const LittleFS_&) = delete;
      ~LittleFS_();
      mutable SemaphoreHandle_t mMutex = nullptr;
      SAppConfig mCfg {};
      static constexpr const char *kMountLabel = "/lfs";
      static constexpr const char *kPartLabel = "littlefs";
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
      static void Lock();
      static void Unlock();
      void AppendToBuffer(const char *tData, size_t tLength);
  };

}

#endif
