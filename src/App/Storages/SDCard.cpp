#include <App/Storages/SDCard.h>

namespace App {

  char SDCard_::mReadBuffer[4096] = "";
  bool SDCard_::mReadValid = false;
  char SDCard_::mListBuffer[4096] = "";
  char SDCard_::mFileBuffer[4096] = "";
  size_t SDCard_::mListPos = 0;
  std::vector<const char *> App::SDCard_::mFileList;
  size_t SDCard_::mFilesCount = 0;
  char SDCard_::mFilesLastDir[128] = "";
  char SDCard_::mFilesLastExt[16] = "";

  SDCard_ &SDCard_::Instance() {
    static SDCard_ tInstance;
    return tInstance;
  }

  SDCard_::SDCard_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  SDCard_::~SDCard_() {
    for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
    mFileList.clear();
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
    End();
  }

  void SDCard_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void SDCard_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool SDCard_::Init(bool tVerbose) {
    ReloadConfig();
    Guard tLock;
    SPI.begin(kSckPin, kMisoPin, kMosiPin, kCsPin);
    mMounted = SD.begin(kCsPin, SPI, kSpiSpeed, kMountPoint, kMaxFiles);
    if (mMounted) {
      if (tVerbose) {
        xLOG("SDCard init successful");
        xLOG("Type: %s", CardTypeName());
        char tTotalBuffer[16], tUsedBuffer[16];
        UTL.ByteToReadableSize(TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
        UTL.ByteToReadableSize(UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
        xLOG("Size: %s / %s", tUsedBuffer, tTotalBuffer);
        BootstrapVault(tVerbose);
      }
    } else {
      if (tVerbose) xLOG("SDCard init failed, no card inserted?");
    }
    return mMounted;
  }

  void SDCard_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
  }

  bool SDCard_::IsMounted() {
    Guard tLock;
    return mMounted && (SD.cardType() != CARD_NONE);
  }

  void SDCard_::End() {
    Guard tLock;
    SD.end();
    mMounted = false;
  }

  uint8_t SDCard_::CardType() {
    Guard tLock;
    return SD.cardType();
  }

  const char *SDCard_::CardTypeName() {
    uint8_t tType = CardType();
    switch (tType) {
      case CARD_NONE: return "None";
      case CARD_MMC: return "MMC";
      case CARD_SD: return "SD";
      case CARD_SDHC: return "SDHC";
      default: return "Unknown";
    }
  }

  uint64_t SDCard_::TotalBytes() {
    Guard tLock;
    return SD.totalBytes();
  }

  uint64_t SDCard_::UsedBytes() {
    Guard tLock;
    return SD.usedBytes();
  }

  const char *SDCard_::ListDir(const char *tPath) {
    Guard tLock;
    mListPos = 0;
    mListBuffer[0] = '\0';
    if (!mMounted) return "SDCard not mounted\r\n";   
    char tFullPathBuffer[128];
    strncpy(tFullPathBuffer, NormalizePath(tPath), sizeof(tFullPathBuffer) - 1);
    tFullPathBuffer[sizeof(tFullPathBuffer) - 1] = '\0';
    File tRoot = SD.open(tFullPathBuffer);
    if (!tRoot || !tRoot.isDirectory()) return "SDCard invalid root\r\n";    
    auto IsFile = [&](const char *tName) -> bool {
      if (!tName || tName[0] == '\0') return false;
      const char *tDot = strrchr(tName, '.');
      if (tDot && strlen(tDot + 1) >= 1) return true;
      return false;
    };
    auto IsHiddenDirectory = [&](File &tNode, const char *tName) -> bool {
      return tNode && tNode.isDirectory() && tName && tName[0] == '.';
    };
    File tEntry = tRoot.openNextFile();
    while (tEntry) {
      const char *tShort = GetFileName(tEntry.name());
      if (IsHiddenDirectory(tEntry, tShort)) {
        tEntry = tRoot.openNextFile();
        continue;
      }
      if (tEntry.isDirectory() && !IsFile(tShort)) {
        AppendToBuffer(tShort, strlen(tShort));
        AppendToBuffer("/\r\n", 3);
        char tSubBuffer[128];
        snprintf(tSubBuffer, sizeof(tSubBuffer), "/%s", tEntry.name());
        File tSub = SD.open(tSubBuffer);
        File tFile = tSub.openNextFile();
        while (tFile) {
          const char *tName = GetFileName(tFile.name());
          if (IsHiddenDirectory(tFile, tName)) {
            tFile = tSub.openNextFile();
            continue;
          }
          if (IsFile(tName)) {
            char tBuffer[16];
            UTL.ByteToReadableSize(tFile.size(), tBuffer, sizeof(tBuffer));
            AppendToBuffer("  ", 2);
            AppendToBuffer(tName, strlen(tName));
            AppendToBuffer(" [", 2);
            AppendToBuffer(tBuffer, strlen(tBuffer));
            AppendToBuffer("]\r\n", 3);
          }
          tFile = tSub.openNextFile();
        }
        tSub.close();
      }
      tEntry = tRoot.openNextFile();
    }
    tRoot.close();
    File tFileRoot = SD.open(tFullPathBuffer);
    File tFileEntry = tFileRoot.openNextFile();
    while (tFileEntry) {
      const char *tShort = GetFileName(tFileEntry.name());
      if (IsHiddenDirectory(tFileEntry, tShort)) {
        tFileEntry = tFileRoot.openNextFile();
        continue;
      }
      if (IsFile(tShort)) {
        char tBuffer[16];
        UTL.ByteToReadableSize(tFileEntry.size(), tBuffer, sizeof(tBuffer));
        AppendToBuffer(tShort, strlen(tShort));
        AppendToBuffer(" [", 2);
        AppendToBuffer(tBuffer, strlen(tBuffer));
        AppendToBuffer("]\r\n", 3);
      }
      tFileEntry = tFileRoot.openNextFile();
    }
    tFileRoot.close();
    return mListBuffer;
  }

  void SDCard_::AppendToBuffer(const char *tData, size_t tLength) {
    if (mListPos + tLength >= sizeof(mListBuffer) - 1) tLength = sizeof(mListBuffer) - mListPos - 1;
    memcpy(mListBuffer + mListPos, tData, tLength);
    mListPos += tLength;
    mListBuffer[mListPos] = '\0';
  }

  File SDCard_::OpenFile(const char *tPath, const char *tMode, bool tCreate) {
    tPath = NormalizePath(tPath);
    File tOk = SD.open(tPath, tMode, tCreate);
    if (!tOk) xLOG("Cannot open: %s", tPath);
    return tOk;
  }

  const char *SDCard_::ReadFile(const char *tPath, const char *tMode) {
    Guard tLock;
    mReadValid = false;
    mReadBuffer[0] = '\0';
    if (!mMounted) return "";
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    File tFile = SD.open(tNormalizedPath, tMode);
    if (!tFile) return "";
    size_t tLength = tFile.size();
    if (tLength == 0) {
      tFile.close();
      mReadValid = true;
      return mReadBuffer;
    }
    if (tLength >= sizeof(mReadBuffer)) tLength = sizeof(mReadBuffer) - 1;
    size_t tRead = tFile.readBytes(mReadBuffer, tLength);
    tFile.close();
    if (tRead == tLength) {
      mReadBuffer[tRead] = '\0';
      mReadValid = true;
      return mReadBuffer;
    } else {
      mReadBuffer[0] = '\0';
      mReadValid = false;
      return "";
    }
  }

  bool SDCard_::WriteFile(const char *tPath, const char *tData, bool tVerbose) {
    Guard tLock;
    if (!mMounted) {
      if (tVerbose) xLOG("Not mounted");
      return false;
    }
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    const char *tName = GetFileName(tNormalizedPath);
    size_t tLength = strlen(tData);
    File tFile = SD.open(tNormalizedPath, FILE_WRITE, true);
    bool tOk = false;
    if (tFile) {
      tOk = (tFile.write((const uint8_t*)tData, tLength) == tLength);
      if (tOk) {
        tFile.flush();
        if (tVerbose) xLOG("File created → %s", tName);
      } else {
        if (tVerbose) xLOG("Error writing file → %s", tName);
      }
      tFile.close();
    } else {
      if (tVerbose) xLOG("Failed to open for writing → %s", tName);
    }
    return tOk;
  }

  bool SDCard_::DeleteFile(const char *tPath) {
    Guard tLock;
    if (!mMounted) return false;
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    bool tOk = SD.remove(tNormalizedPath);
    if (tOk) xLOG("File deleted → %s", tPath);
    else xLOG("Error deleting file → %s", tPath);
    return tOk;
  }

  bool SDCard_::CreateDir(const char *tPath, bool tVerbose) {
    Guard tLock;
    if (!mMounted) return false;
    bool tExists = Exists(tPath);
    bool tOk = tExists ? true : SD.mkdir(tPath);
    if (tVerbose) {
      if (tOk && !tExists) xLOG("Directory created → %s", tPath);
      else if (tExists) xLOG("Directory already exists → %s", tPath);
      else xLOG("Error creating directory → %s", tPath);
    }
    return tOk;
  }

  bool SDCard_::DeleteDir(const char *tPath) {
    Guard tLock;
    if (!mMounted) return false;
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    bool tOk = SD.rmdir(tNormalizedPath);
    if (tOk) xLOG("Directory deleted → %s", tPath);
    else xLOG("Error deleting directory → %s", tPath);
    return tOk;
  }

  bool SDCard_::Exists(const char *tPath) {
    Guard tLock;
    if (!mMounted) return false;
    return SD.exists(tPath);
  }

  const char *SDCard_::GetFileName(const char *tPath) {
    const char *tName = strrchr(tPath, '/');
    return tName ? tName + 1 : tPath;
  }

  void SDCard_::InvalidateFileCache() {
    for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
    mFileList.clear();
    mFilesCount = 0;
    mFilesLastDir[0] = '\0';
    mFilesLastExt[0] = '\0';
  }

  void SDCard_::BootstrapVault(bool tVerbose) {
    char tIDirName[128] = "";
    UTL.PrependSlash(mCfg.Display.ImagesDir.c_str(), tIDirName, sizeof(tIDirName));
    CreateDir(tIDirName, tVerbose);
    if (tVerbose) PrintListDir();
  }

  void SDCard_::PrintListDir(size_t tMaxLines) {
    xLOG_PL();
    UTL.PrintInfo("SDCARD FILE STRUCTURE", EUtilsInfoType::Header);
    UTL.PrintInfo("", EUtilsInfoType::Line);
    const char *tData = ListDir("/");
    char *tLine = (char*)tData;
    char *tEnd = (char*)tData + mListPos;
    size_t tPrintedLines = 0;
    while (tLine < tEnd) {
      if (tPrintedLines >= tMaxLines) {
        UTL.PrintInfo("...", EUtilsInfoType::Cell);
        break;
      }
      char *tNext = tLine;
      while (tNext < tEnd && *tNext != '\r' && *tNext != '\n') ++tNext;
      char tTemp = *tNext; *tNext = '\0';
      UTL.PrintInfo(tLine, EUtilsInfoType::Cell);
      ++tPrintedLines;
      if (tTemp) *tNext = tTemp;
      tLine = tNext + (tTemp ? (tTemp == '\r' && tNext[1] == '\n' ? 2 : 1) : 0);
    }
    UTL.PrintInfo("", EUtilsInfoType::Footer);
  }

  const char *SDCard_::NormalizePath(const char *tPath) {
    static char sNDir[128] = "";
    if (!tPath || tPath[0] == '\0') strcpy(sNDir, "/");
    else if (tPath[0] == '/') strncpy(sNDir, tPath, sizeof(sNDir) - 1);
    else snprintf(sNDir, sizeof(sNDir), "/%s", tPath);
    sNDir[sizeof(sNDir)-1] = '\0';
    return sNDir;
  }

  std::vector<const char*> SDCard_::GetFilesInDir(const char *tDir, const char *tExt) {
    Guard tLock;
    if (!Instance().mMounted) return {};
    tDir = NormalizePath(tDir);
    if (strcmp(tDir, mFilesLastDir) != 0 || strcmp(tExt, mFilesLastExt) != 0) {
      for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
      mFileList.clear();
      mFilesCount = 0;
      strncpy(mFilesLastDir, tDir, sizeof(mFilesLastDir)-1);
      strncpy(mFilesLastExt, tExt, sizeof(mFilesLastExt)-1);
    }
    if (mFilesCount > 0) return mFileList;
    if (!SD.exists(tDir)) {
      xLOG("Directory not found → %s", tDir);
      return {};
    }
    File tRoot = SD.open(tDir);
    if (!tRoot || !tRoot.isDirectory()) {
      xLOG("Cannot open directory → %s", tDir);
      return {};
    }
    char tSearchExt[16];
    if (tExt[0] == '.') strncpy(tSearchExt, tExt, sizeof(tSearchExt)-1);
    else snprintf(tSearchExt, sizeof(tSearchExt), ".%s", tExt);
    tSearchExt[sizeof(tSearchExt)-1] = '\0';
    File tFile = tRoot.openNextFile();
    size_t tEntryCount = 0;
    while (tFile) {
      tEntryCount++;
      if ((tEntryCount % 16U) == 0U) {
        esp_task_wdt_reset();
        vTaskDelay(1);
      }
      if (!tFile.isDirectory()) {
        const char *tName = tFile.name();
        if (strstr(tName, tSearchExt) || strcmp(tExt, "*") == 0) {
          char *tFull = strdup(tName);
          if (tFull) mFileList.push_back(tFull);
        }
      }
      tFile.close();
      tFile = tRoot.openNextFile();
    }
    tRoot.close();
    mFilesCount = mFileList.size();
    return mFileList;
  }

  const char *SDCard_::GetNextFile() {
    Guard tLock;
    return GetNextFile(mCfg.Display.CurrentFile.c_str(), mCfg.Display.ImagesDir.c_str(), mCfg.Display.ImageExt.c_str());
  }

  const char *SDCard_::GetNextFile(const char *tCurrentFilename, const char *tDir, const char *tExt) {
    Guard tLock;
    tDir = NormalizePath(tDir);
    mFileList = GetFilesInDir(tDir, tExt);
    mFilesCount = mFileList.size(); 
    if (mFilesCount == 0) return nullptr;    
    size_t tNextIndex = 0;
    if (tCurrentFilename && tCurrentFilename[0]) {
      const char *tName = strrchr(tCurrentFilename, '/');
      tName = tName ? tName + 1 : tCurrentFilename;
      char tSearch[128];
      snprintf(tSearch, sizeof(tSearch), "%s", tName);
      for (size_t i = 0; i < mFilesCount; ++i) {
        const char *tEntryName = strrchr(mFileList[i], '/');
        tEntryName = tEntryName ? tEntryName + 1 : mFileList[i];
        if (strcmp(tEntryName, tSearch) == 0) {
          tNextIndex = (i + 1) % mFilesCount;
          break;
        }
      }
    }
    return mFileList[tNextIndex];
  }

}
