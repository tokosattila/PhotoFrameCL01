#include <App/Storages/LittleFS.h>

namespace App {

  char LittleFS_::mReadBuffer[4096] = "";
  bool LittleFS_::mReadValid = false;
  char LittleFS_::mListBuffer[4096] = "";
  char LittleFS_::mFileBuffer[4096] = "";
  size_t LittleFS_::mListPos = 0;
  std::vector<const char*> App::LittleFS_::mFileList;
  size_t LittleFS_::mFilesCount = 0;
  char LittleFS_::mFilesLastDir[128] = "";
  char LittleFS_::mFilesLastExt[16] = "";

  LittleFS_ &LittleFS_::Instance() {
    static LittleFS_ tInstance;
    return tInstance;
  }

  LittleFS_::LittleFS_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  LittleFS_::~LittleFS_() {
    for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
    mFileList.clear();
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
    End();
  }

  void LittleFS_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void LittleFS_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool LittleFS_::Init(bool tVerbose) {
    ReloadConfig();
    Guard tLock;
    bool tOk = LittleFS.begin(false, kMountLabel, kMaxFiles, kPartLabel);
    if (tOk) {
      if (tVerbose) {
        xLOG("LittleFS init successful");
        BootstrapVault(tVerbose);
      }
    } else {
      if (tVerbose) xLOG("LittleFS init failed");
    }
    return tOk;
  }

  void LittleFS_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
  }

  bool LittleFS_::IsMounted() {
    Guard tLock;
    File tOk = LittleFS.open("/", "r");
    bool tIsMounted = tOk ? true : false;
    if (tOk) tOk.close();
    return tIsMounted;
  }

  void LittleFS_::End() {
    Guard tLock;
    LittleFS.end();
  }

  const char *LittleFS_::ListDir(const char *tPath) {
    Guard tLock;
    mListPos = 0;
    mListBuffer[0] = '\0';
    char tFullPathBuffer[128];
    strncpy(tFullPathBuffer, NormalizePath(tPath), sizeof(tFullPathBuffer) - 1);
    tFullPathBuffer[sizeof(tFullPathBuffer) - 1] = '\0';
    File tRoot = LittleFS.open(tFullPathBuffer);
    if (!tRoot || !tRoot.isDirectory()) return "Invalid root\r\n";
    auto IsFile = [&](const char *tName) -> bool {
      if (!tName || tName[0] == '\0') return false;
      const char *tDot = strrchr(tName, '.');
      if (tDot && strlen(tDot + 1) == 3) return true;
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
        File tSub = LittleFS.open(tSubBuffer);
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
    File tFileRoot = LittleFS.open(tFullPathBuffer);
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

  void LittleFS_::AppendToBuffer(const char *tData, size_t tLength) {
    if (mListPos + tLength >= sizeof(mListBuffer) - 1) tLength = sizeof(mListBuffer) - mListPos - 1;
    memcpy(mListBuffer + mListPos, tData, tLength);
    mListPos += tLength;
    mListBuffer[mListPos] = '\0';
  }

  File LittleFS_::OpenFile(const char *tPath, const char *tMode, bool tCreate) {
    tPath = NormalizePath(tPath);
    File tOk = LittleFS.open(tPath, tMode, tCreate);
    if (!tOk) xLOG("Cannot open → %s", tPath);
    return tOk;
  }

  const char *LittleFS_::ReadFile(const char *tPath, const char *tMode) {
    Guard tLock;
    mReadValid = false;
    mReadBuffer[0] = '\0';
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    File tFile = LittleFS.open(tNormalizedPath, tMode);
    if (!tFile) {
      return "";
    }
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

  bool LittleFS_::WriteFile(const char *tPath, const char *tData, bool tVerbose) {
    Guard tLock;
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    const char *tName = GetFileName(tNormalizedPath);
    size_t tLength = strlen(tData);
    File tFile = LittleFS.open(tNormalizedPath, FILE_WRITE, true);
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

  bool LittleFS_::DeleteFile(const char *tPath) {
    Guard tLock;
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    bool tOk = LittleFS.remove(tNormalizedPath);
    if (tOk) xLOG("File deleted → %s", tPath);
    else xLOG("Error deleting file → %s", tPath);
    return tOk;
  }

  bool LittleFS_::CreateDir(const char *tPath, bool tVerbose) {
    Guard tLock;
    bool tExists = Exists(tPath);
    bool tOk = tExists ? true : LittleFS.mkdir(tPath);
    if (tVerbose) {
      if (tOk && !tExists) xLOG("Directory created → %s", tPath);
      else if (tExists) xLOG("Directory already exists → %s", tPath);
      else xLOG("Error creating directory → %s", tPath);
    }
    return tOk;
  }

  bool LittleFS_::DeleteDir(const char *tPath) {
    Guard tLock;
    char tNormalizedPath[128];
    strncpy(tNormalizedPath, NormalizePath(tPath), sizeof(tNormalizedPath) - 1);
    tNormalizedPath[sizeof(tNormalizedPath) - 1] = '\0';
    bool tOk = LittleFS.rmdir(tNormalizedPath);
    if (tOk) xLOG("Directory deleted → %s", tPath);
    else xLOG("Error deleting directory → %s", tPath);
    return tOk;
  }

  bool LittleFS_::Exists(const char *tPath) {
    Guard tLock;
    bool tOk = LittleFS.exists(tPath);
    return tOk;
  }

  uint64_t LittleFS_::TotalBytes() {
    Guard tLock;
    uint64_t tValue = LittleFS.totalBytes();
    return tValue;
  }

  uint64_t LittleFS_::UsedBytes() {
    Guard tLock;
    uint64_t tValue = LittleFS.usedBytes();
    return tValue;
  }

  const char *LittleFS_::GetFileName(const char *tPath) {
    const char *tName = strrchr(tPath, '/');
    return tName ? tName + 1 : tPath;
  }

  void LittleFS_::InvalidateFileCache() {
    for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
    mFileList.clear();
    mFilesCount = 0;
    mFilesLastDir[0] = '\0';
    mFilesLastExt[0] = '\0';
  }

  void LittleFS_::BootstrapVault(bool tVerbose) {
    char tIDirName[128] = "";
    UTL.PrependSlash(mCfg.Display.ImagesDir.c_str(), tIDirName, sizeof(tIDirName));
    CreateDir(tIDirName, tVerbose);
    if (tVerbose) PrintListDir();
  }

  bool LittleFS_::Format() {
    Guard tLock;
    LittleFS.end();
    bool tOk = LittleFS.format();
    if (tOk) {
      tOk = LittleFS.begin(false, kMountLabel, kMaxFiles, kPartLabel);
      if (tOk) xLOG("Formatted and remounted");
      else xLOG("Formatted but remount failed");
    } else {
      xLOG("Format failed");
    }
    return tOk;
  }

  void LittleFS_::PrintListDir() {
    xLOG_PL();
    UTL.PrintInfo("LITTLEFS FILE STRUCTURE", EUtilsInfoType::Header);
    UTL.PrintInfo("", EUtilsInfoType::Line);
    const char *tData = ListDir("/");
    char *tLine = (char*)tData;
    char *tEnd = (char*)tData + mListPos;
    while (tLine < tEnd) {
      char *tNext = tLine;
      while (tNext < tEnd && *tNext != '\r' && *tNext != '\n') ++tNext;
      char tTemp = *tNext; *tNext = '\0';
      UTL.PrintInfo(tLine, EUtilsInfoType::Cell);
      if (tTemp) *tNext = tTemp;
      tLine = tNext + (tTemp ? (tTemp == '\r' && tNext[1] == '\n' ? 2 : 1) : 0);
    }
    UTL.PrintInfo("", EUtilsInfoType::Footer);
  }

  const char *LittleFS_::NormalizePath(const char *tPath) {
    static char sNDir[128] = "";
    if (!tPath || tPath[0] == '\0') strcpy(sNDir, "/");
    else if (tPath[0] == '/') strncpy(sNDir, tPath, sizeof(sNDir) - 1);
    else snprintf(sNDir, sizeof(sNDir), "/%s", tPath);
    sNDir[sizeof(sNDir)-1] = '\0';
    return sNDir;
  }

  std::vector<const char*> LittleFS_::GetFilesInDir(const char *tDir, const char *tExt) {
    Guard tLock;
    tDir = NormalizePath(tDir);
    if (strcmp(tDir, mFilesLastDir) != 0 || strcmp(tExt, mFilesLastExt) != 0) {
      for (size_t i = 0; i < mFileList.size(); ++i) free((void*)mFileList[i]);
      mFileList.clear();
      mFilesCount = 0;
      strncpy(mFilesLastDir, tDir, sizeof(mFilesLastDir)-1);
      strncpy(mFilesLastExt, tExt, sizeof(mFilesLastExt)-1);
    }
    if (mFilesCount > 0) {
      std::vector<const char*> tResult = mFileList;
      return tResult;
    }
    if (!LittleFS.exists(tDir)) {
      xLOG("Directory not found → %s", tDir);
      return {};
    }
    File tRoot = LittleFS.open(tDir);
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
    std::vector<const char*> tResult = mFileList;
    return tResult;
  }

  const char *LittleFS_::GetNextFile() {
    Guard tLock;
    const char *tResult = GetNextFile(mCfg.Display.CurrentFile.c_str(), mCfg.Display.ImagesDir.c_str(), mCfg.Display.ImageExt.c_str());
    return tResult;
  }

  const char *LittleFS_::GetNextFile(const char *tCurrentFilename, const char *tDir, const char *tExt) {
    Guard tLock;
    tDir = NormalizePath(tDir);
    mFileList = GetFilesInDir(tDir, tExt);
    mFilesCount = mFileList.size();
    if (mFilesCount == 0) {
      return nullptr;
    }
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
    const char *tResult = mFileList[tNextIndex];
    return tResult;
  }

}
