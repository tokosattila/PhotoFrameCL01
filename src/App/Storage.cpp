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
    mCfg = CFG.Get<SAppConfig>();
    TryInitLittleFS(false);
    TryInitSDCard(false);
    SelectActiveStorage(tVerbose);
    if (tVerbose && mMounted) {
      if (mSDCardAvailable) SDC.BootstrapVault(true);
      if (mLittleFSAvailable) LFS.BootstrapVault(true);
      char tUsedBuffer[16], tTotalBuffer[16];
      UTL.ByteToReadableSize(UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
      UTL.ByteToReadableSize(TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
      xLOG("%s → %s / %s %s", GetActiveName(), tUsedBuffer, tTotalBuffer, mFallbackActive ? "(fallback)" : "");
      if (mSDCardAvailable && mActiveType != EFileSystemType::SDCard) {
        UTL.ByteToReadableSize(SDC.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
        UTL.ByteToReadableSize(SDC.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
        xLOG("SDCard [fallback] → %s / %s", tUsedBuffer, tTotalBuffer);
      }
      if (mLittleFSAvailable && mActiveType != EFileSystemType::LittleFS) {
        UTL.ByteToReadableSize(LFS.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
        UTL.ByteToReadableSize(LFS.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
        xLOG("LittleFS [fallback] → %s / %s", tUsedBuffer, tTotalBuffer);
      }
    }
    return mMounted;
  }

  void Storage_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
    SelectActiveStorage(false);
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

  bool Storage_::IsTypeAvailable(EFileSystemType tType) const {
    if (tType == EFileSystemType::SDCard) return mSDCardAvailable;
    return mLittleFSAvailable;
  }

  bool Storage_::EnsureImagesDir(EFileSystemType tType) {
    char tDirPath[128] = "";
    UTL.PrependSlash(mCfg.Display.ImagesDir.c_str(), tDirPath, sizeof(tDirPath));
    if (tType == EFileSystemType::SDCard && mSDCardAvailable) return SDC.CreateDir(tDirPath);
    if (tType == EFileSystemType::LittleFS && mLittleFSAvailable) return LFS.CreateDir(tDirPath);
    return false;
  }

  bool Storage_::TryFormatAndRecover(EFileSystemType tType) {
    if (tType != EFileSystemType::LittleFS || !mLittleFSAvailable) return false;
    xLOG("Attempting LittleFS format recovery...");
    if (!LFS.Format()) return false;
    return EnsureImagesDir(EFileSystemType::LittleFS);
  }

  void Storage_::PersistStorageSwitch(EFileSystemType tType, bool tVerbose) {
    mCfg.Storage.DefaultFileSystem = tType;
    mCfg.Storage.FallbackEnabled = false;
    CFG.SaveAllConfig(mCfg);
    if (tVerbose) xLOG("Config persisted: default=%s, fallback=off", tType == EFileSystemType::SDCard ? "SDCard" : "LittleFS");
  }

  void Storage_::SelectActiveStorage(bool tVerbose) {
    EFileSystemType tPreferred = mCfg.Storage.DefaultFileSystem;
    EFileSystemType tOther = (tPreferred == EFileSystemType::SDCard) ? EFileSystemType::LittleFS : EFileSystemType::SDCard;
    const char *tPreferredName = (tPreferred == EFileSystemType::SDCard) ? "SDCard" : "LittleFS";
    const char *tOtherName = (tOther == EFileSystemType::SDCard) ? "SDCard" : "LittleFS";
    bool tPreferredOk = IsTypeAvailable(tPreferred);
    bool tOtherOk = IsTypeAvailable(tOther);
    const bool tFallbackEnabled = mCfg.Storage.FallbackEnabled;
    mFallbackActive = false;
    if (tPreferredOk && EnsureImagesDir(tPreferred)) {
      mActiveType = tPreferred;
      mMounted = true;
      if (tVerbose) xLOG("Active storage → %s", tPreferredName);
      return;
    }
    if (tPreferredOk && TryFormatAndRecover(tPreferred)) {
      mActiveType = tPreferred;
      mMounted = true;
      if (tVerbose) xLOG("Active storage → %s (recovered after format)", tPreferredName);
      return;
    }
    if (tFallbackEnabled && tOtherOk && EnsureImagesDir(tOther)) {
      mActiveType = tOther;
      mMounted = true;
      mFallbackActive = true;
      if (tVerbose) xLOG("Active storage → %s (fallback from %s)", tOtherName, tPreferredName);
      return;
    }
    if (tFallbackEnabled && tOtherOk && TryFormatAndRecover(tOther)) {
      mActiveType = tOther;
      mMounted = true;
      mFallbackActive = true;
      if (tVerbose) xLOG("Active storage → %s (fallback recovery from %s)", tOtherName, tPreferredName);
      return;
    }
    mMounted = false;
    if (tVerbose) xLOG("No usable storage available");
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
    if (!mMounted || !tPath || !tContent) return false;
    File tFile = OpenFile(tPath, tAppend ? FILE_APPEND : FILE_WRITE, true);
    if (!tFile) return false;
    const size_t tLength = strlen(tContent);
    const bool tSuccess = (tFile.write(reinterpret_cast<const uint8_t *>(tContent), tLength) == tLength);
    tFile.close();
    return tSuccess;
  }

  bool Storage_::DeleteFile(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.DeleteFile(tPath);
    return LFS.DeleteFile(tPath);
  }

  bool Storage_::CreateDir(const char *tPath, bool tVerbose) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.CreateDir(tPath, tVerbose);
    return LFS.CreateDir(tPath, tVerbose);
  }

  bool Storage_::DeleteDir(const char *tPath) {
    if (!mMounted) return false;
    if (mActiveType == EFileSystemType::SDCard) return SDC.DeleteDir(tPath);
    return LFS.DeleteDir(tPath);
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

  bool Storage_::Format(EFileSystemType tType, volatile uint8_t *tProgress) {
    Guard tLock;
    bool tOk = false;
    if (tType == EFileSystemType::SDCard) {
      tOk = mSDCardAvailable && SDC.Format(tProgress);
      mSDCardAvailable = SDC.IsMounted();
    } else {
      tOk = mLittleFSAvailable && LFS.Format(tProgress);
      mLittleFSAvailable = LFS.IsMounted();
    }
    if (tOk) {
      EnsureImagesDir(tType);
      SelectActiveStorage(false);
      if (tType == mCfg.Storage.DefaultFileSystem) CFG.SaveImageName("");
    }
    return tOk;
  }

}
