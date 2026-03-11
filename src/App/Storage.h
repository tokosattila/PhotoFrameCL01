#ifndef STORAGE_H
#define STORAGE_H

#include <App/Global.h>

namespace App {
  class Storage_ {
    DEFINE_TAG("STG");
    friend class AutoGuard<Storage_>;
    public:
      using Guard = AutoGuard<Storage_>;
      static Storage_ &Instance();
      static void Lock();
      static void Unlock();
      bool Init(bool tVerbose = true);
      void End();
      bool IsMounted() const { return mMounted; }
      bool IsSDCard() const { return mActiveType == EFileSystemType::SDCard; }
      bool IsLittleFS() const { return mActiveType == EFileSystemType::LittleFS; }
      EFileSystemType GetActiveType() const { return mActiveType; }
      const char *GetActiveName() const;
      bool HasFallback() const { return mFallbackActive; }
      File OpenFile(const char *tPath, const char *tMode = FILE_READ, bool tCreate = false);
      const char *ReadFile(const char *tPath);
      bool WriteFile(const char *tPath, const char *tContent, bool tAppend = false);
      bool DeleteFile(const char *tPath);
      const char *ListDir(const char *tPath);
      size_t GetListPos() const;
      const char *GetNextFile(const char *tCurrentFile);
      bool MakeDir(const char *tPath);
      bool RemoveDir(const char *tPath);
      bool Exists(const char *tPath);
      const char *NormalizePath(const char *tPath);
      uint64_t TotalBytes();
      uint64_t UsedBytes();
      uint64_t FreeBytes() { return TotalBytes() - UsedBytes(); }
      const char *CatFile(const char *tPath);
    private:
      Storage_();
      ~Storage_();
      Storage_(const Storage_ &) = delete;
      SemaphoreHandle_t mMutex = nullptr;
      EFileSystemType mActiveType = EFileSystemType::LittleFS;
      bool mMounted = false;
      bool mFallbackActive = false;
      bool mSDCardAvailable = false;
      bool mLittleFSAvailable = false;      
      Storage_ &operator=(const Storage_ &) = delete;
      bool TryInitSDCard(bool tVerbose = true);
      bool TryInitLittleFS(bool tVerbose = true);
      bool HasImagesInDir(EFileSystemType tType);
      void SelectActiveStorage(bool tVerbose = true);
  };

}

#endif 
