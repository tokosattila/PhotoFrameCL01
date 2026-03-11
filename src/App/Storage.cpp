#include <App/Storage.h>

namespace App {

  Storage_ &Storage_::Instance() {
    static Storage_ tInstance;
    return tInstance;
  }

  Storage_::Storage_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Storage_::~Storage_() {
    End();
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Storage_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Storage_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool Storage_::Init(bool tVerbose) {
    Guard tLock;
    TryInitLittleFS(false);
    TryInitSDCard(false);
    SelectActiveStorage(tVerbose);
    if (tVerbose && mMounted) {
      if (mActiveType == EFileSystemType::SDCard) SDC.BootstrapVault(true);
      else LFS.BootstrapVault(true);
      char tUsedBuffer[16], tTotalBuffer[16];
      UTL.ByteToReadableSize(UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
      UTL.ByteToReadableSize(TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
      xLOG("%s: %s / %s %s", GetActiveName(), tUsedBuffer, tTotalBuffer, mFallbackActive ? "(fallback)" : "");
      if (mSDCardAvailable && mActiveType != EFileSystemType::SDCard) {
        UTL.ByteToReadableSize(SDC.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
        UTL.ByteToReadableSize(SDC.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
        xLOG("SDCard (secondary): %s / %s", tUsedBuffer, tTotalBuffer);
      }
      if (mLittleFSAvailable && mActiveType != EFileSystemType::LittleFS) {
        UTL.ByteToReadableSize(LFS.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
        UTL.ByteToReadableSize(LFS.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
        xLOG("LittleFS (secondary): %s / %s", tUsedBuffer, tTotalBuffer);
      }
    }
    return mMounted;
  }

  bool Storage_::TryInitSDCard(bool tVerbose) {
    if (SDC.Init(tVerbose)) {
      mSDCardAvailable = SDC.IsMounted();
      return mSDCardAvailable;
    }
    mSDCardAvailable = false;
    return false;
  }  

  bool Storage_::TryInitLittleFS(bool tVerbose) {
    if (LFS.Init(tVerbose)) {
      mLittleFSAvailable = LFS.IsMounted();
      return mLittleFSAvailable;
    }
    mLittleFSAvailable = false;
    return false;
  }

  bool Storage_::HasImagesInDir(EFileSystemType tType) {
    SDisplayConfig tCfg = CFG.Get<SDisplayConfig>();
    char tImagesDir[64];
    char tImageExt[16];
    strncpy(tImagesDir, tCfg.ImagesDir.c_str(), sizeof(tImagesDir) - 1);
    tImagesDir[sizeof(tImagesDir) - 1] = '\0';
    strncpy(tImageExt, tCfg.ImageExt.c_str(), sizeof(tImageExt) - 1);
    tImageExt[sizeof(tImageExt) - 1] = '\0';
    if (tType == EFileSystemType::SDCard && mSDCardAvailable) {
      auto tFiles = SDC.GetFilesInDir(tImagesDir, tImageExt);
      return !tFiles.empty();
    }
    if (tType == EFileSystemType::LittleFS && mLittleFSAvailable) {
      auto tFiles = LFS.GetFilesInDir(tImagesDir, tImageExt);
      return !tFiles.empty();
    }
    return false;
  }

  void Storage_::SelectActiveStorage(bool tVerbose) {
    mFallbackActive = false;
    if (DEFAULT_FILE_SYSTEM == EFileSystemType::SDCard) {
      if (mSDCardAvailable) {
        bool tHasImages = HasImagesInDir(EFileSystemType::SDCard);
        if (tHasImages) {
          mActiveType = EFileSystemType::SDCard;
          mMounted = true;
          if (tVerbose) xLOG("Active storage → SDCard");
          return;
        }
        if (STORAGE_FALLBACK_ENABLED && mLittleFSAvailable && HasImagesInDir(EFileSystemType::LittleFS)) {
          mActiveType = EFileSystemType::LittleFS;
          mMounted = true;
          mFallbackActive = true;
          if (tVerbose) xLOG("SDCard → images folder empty, smart fallback to LittleFS");
          return;
        }
        mActiveType = EFileSystemType::SDCard;
        mMounted = true;
        if (mLittleFSAvailable) {
          if (tVerbose) xLOG("Active storage → SDCard (no images on either storage)");
        } else {
          if (tVerbose) xLOG("Active storage → SDCard (images empty, LittleFS not available)");
        }
        return;
      }
      if (STORAGE_FALLBACK_ENABLED && mLittleFSAvailable) {
        mActiveType = EFileSystemType::LittleFS;
        mMounted = true;
        mFallbackActive = true;
        if (tVerbose) xLOG("SDCard → not available, fallback to LittleFS");
        return;
      }
    } else {
      if (mLittleFSAvailable) {
        bool tHasImages = HasImagesInDir(EFileSystemType::LittleFS);
        if (tHasImages) {
          mActiveType = EFileSystemType::LittleFS;
          mMounted = true;
          if (tVerbose) xLOG("Active storage → LittleFS");
          return;
        }
        if (STORAGE_FALLBACK_ENABLED && mSDCardAvailable && HasImagesInDir(EFileSystemType::SDCard)) {
          mActiveType = EFileSystemType::SDCard;
          mMounted = true;
          mFallbackActive = true;
          if (tVerbose) xLOG("LittleFS → images folder empty, smart fallback to SDCard");
          return;
        }
        mActiveType = EFileSystemType::LittleFS;
        mMounted = true;
        if (mSDCardAvailable) {
          if (tVerbose) xLOG("Active storage → LittleFS (no images on either storage)");
        } else {
          if (tVerbose) xLOG("Active storage → LittleFS (images empty, SDCard not available)");
        }
        return;
      }
      if (STORAGE_FALLBACK_ENABLED && mSDCardAvailable) {
        mActiveType = EFileSystemType::SDCard;
        mMounted = true;
        mFallbackActive = true;
        if (tVerbose) xLOG("LittleFS → not available, fallback to SDCard");
        return;
      }
    }
    mMounted = false;
    if (tVerbose) xLOG("No storage available");
  }

  void Storage_::End() {
    Guard tLock;
    if (mSDCardAvailable) {
      SDC.End();
      mSDCardAvailable = false;
    }
    if (mLittleFSAvailable) {
      LFS.End();
      mLittleFSAvailable = false;
    }
    mMounted = false;
  }

  const char *Storage_::GetActiveName() const {
    if (mActiveType == EFileSystemType::SDCard) return "SDCard";
    return "LittleFS";
  }

  File Storage_::OpenFile(const char *tPath, const char *tMode, bool tCreate) {
    if (!mMounted) return File();
    if (mActiveType == EFileSystemType::SDCard) return SDC.OpenFile(tPath, tMode, tCreate);
    return LFS.OpenFile(tPath, tMode, tCreate);
  }

  const char *Storage_::ReadFile(const char *tPath) {
    if (!mMounted) return "";
    if (mActiveType == EFileSystemType::SDCard) return SDC.ReadFile(tPath);
    return LFS.ReadFile(tPath);
  }

  bool Storage_::WriteFile(const char *tPath, const char *tContent, bool tAppend) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.WriteFile(tPath, tContent, tAppend);
    return LFS.WriteFile(tPath, tContent, tAppend);
  }

  bool Storage_::DeleteFile(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.DeleteFile(tPath);
    return LFS.DeleteFile(tPath);
  }

  const char *Storage_::ListDir(const char *tPath) {
    if (!mMounted) return "";
    if (mActiveType == EFileSystemType::SDCard) return SDC.ListDir(tPath);
    return LFS.ListDir(tPath);
  }

  size_t Storage_::GetListPos() const {
    if (!mMounted) return 0;
    if (mActiveType == EFileSystemType::SDCard) return SDC.GetListPos();
    return LFS.GetListPos();
  }

  const char *Storage_::GetNextFile(const char *tCurrentFile) {
    if (!mMounted) return "";
    if (mActiveType == EFileSystemType::SDCard) return SDC.GetNextFile(tCurrentFile);
    return LFS.GetNextFile(tCurrentFile);
  }
  bool Storage_::MakeDir(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.CreateDir(tPath);
    return LFS.CreateDir(tPath);
  }

  bool Storage_::RemoveDir(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.DeleteDir(tPath);
    return LFS.DeleteDir(tPath);
  }

  bool Storage_::Exists(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.Exists(tPath);
    return LFS.Exists(tPath);
  }

  const char *Storage_::NormalizePath(const char *tPath) {
    if (!mMounted) return tPath;
    if (mActiveType == EFileSystemType::SDCard) return SDC.NormalizePath(tPath);
    return LFS.NormalizePath(tPath);
  }

  uint64_t Storage_::TotalBytes() {
    if (!mMounted) return 0;
    if (mActiveType == EFileSystemType::SDCard) return SDC.TotalBytes();
    return LFS.TotalBytes();
  }

  uint64_t Storage_::UsedBytes() {
    if (!mMounted) return 0;
    if (mActiveType == EFileSystemType::SDCard) return SDC.UsedBytes();
    return LFS.UsedBytes();
  }

  const char *Storage_::CatFile(const char *tPath) {
    if (!mMounted) return "";
    if (mActiveType == EFileSystemType::SDCard) return SDC.CatFile(tPath);
    return LFS.CatFile(tPath);
  }

}
