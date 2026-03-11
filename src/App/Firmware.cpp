#include <App/Firmware.h>

namespace App {

  Firmware_ &Firmware_::Instance() {
    static Firmware_ tInstance;
    return tInstance;
  }

  Firmware_::Firmware_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Firmware_::~Firmware_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
    if (mUpdateBuffer) {
      heap_caps_free(mUpdateBuffer);
      mUpdateBuffer = nullptr;
      mUpdateBufferSize = 0;
    }
  }

  void Firmware_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Firmware_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool Firmware_::Init() {
    Guard tGuard;
    return EnsureUpdateBuffer();
  }

  void Firmware_::SetError(const char *errorMessage) {
    if (!errorMessage) errorMessage = "";
    snprintf(mLastError, sizeof(mLastError), "%s", errorMessage);
  }

  const char *Firmware_::GetLastError() const {
    Guard tGuard;
    return mLastError[0] ? mLastError : nullptr;
  }

  bool Firmware_::UpdateAvailable() {
    Guard tGuard;
    Storage_::Guard tStorageGuard;
    if (!STG.IsMounted()) {
      SetError("Storage not mounted");
      return false;
    }
    const char *tFirmwarePath = STG.NormalizePath(mPath);
    File tFile = STG.OpenFile(tFirmwarePath, FILE_READ);
    if (!tFile) return false;
    bool tOk = (tFile.size() > 0);
    tFile.close();
    return tOk;
  }

  bool Firmware_::VerifySha256(const char *tShaPath) {
    Guard tGuard;
    if (!EnsureUpdateBuffer()) return false;
    Storage_::Guard tStorageGuard;
    if (!STG.IsMounted()) {
      SetError("Storage not mounted");
      return false;
    }
    const char *tShaPathResolved = tShaPath ? tShaPath : mShaPath;
    char tFirmwarePath[128] = "";
    char tShaFilePath[128] = "";
    strncpy(tFirmwarePath, STG.NormalizePath(mPath), sizeof(tFirmwarePath) - 1);
    strncpy(tShaFilePath, STG.NormalizePath(tShaPathResolved), sizeof(tShaFilePath) - 1);
    File tFirmwareFile = STG.OpenFile(tFirmwarePath, FILE_READ);
    if (!tFirmwareFile) {
      SetError("Firmware file not found");
      return false;
    }
    File tShaFile = STG.OpenFile(tShaFilePath, FILE_READ);
    if (!tShaFile) {
      tFirmwareFile.close();
      SetError("Sha256 file not found");
      return false;
    }
    char tExpectedHex[65] = {0};
    size_t tExpectedPos = 0;
    while (tShaFile.available() && tExpectedPos < 64) {
      int tCharacter = tShaFile.read();
      if (tCharacter == -1) break;
      if (isxdigit((unsigned char)tCharacter)) {
        tExpectedHex[tExpectedPos++] = (char)tolower((unsigned char)tCharacter);
      }
    }
    tExpectedHex[tExpectedPos] = '\0';
    tShaFile.close();
    if (tExpectedPos != 64) {
      tFirmwareFile.close();
      SetError("Sha256 file invalid");
      return false;
    }
    mbedtls_sha256_context tContext;
    mbedtls_sha256_init(&tContext);
    int tShaResult = mbedtls_sha256_starts_ret(&tContext, 0);
    if (tShaResult != 0) {
      tFirmwareFile.close();
      mbedtls_sha256_free(&tContext);
      char tError[64];
      snprintf(tError, sizeof(tError), "Sha256 init failed (%d)", tShaResult);
      SetError(tError);
      return false;
    }
    uint8_t *tBuffer = mUpdateBuffer;
    size_t tBufferSize = mUpdateBufferSize;
    while (tFirmwareFile.available()) {
      size_t tRead = tFirmwareFile.read(tBuffer, tBufferSize);
      if (tRead == 0) {
        tFirmwareFile.close();
        mbedtls_sha256_free(&tContext);
        SetError("Firmware read failed");
        return false;
      }
      tShaResult = mbedtls_sha256_update_ret(&tContext, tBuffer, tRead);
      if (tShaResult != 0) {
        tFirmwareFile.close();
        mbedtls_sha256_free(&tContext);
        char tError[64];
        snprintf(tError, sizeof(tError), "Sha256 update failed (%d)", tShaResult);
        SetError(tError);
        return false;
      }
    }
    uint8_t tDigest[32];
    tShaResult = mbedtls_sha256_finish_ret(&tContext, tDigest);
    if (tShaResult != 0) {
      tFirmwareFile.close();
      mbedtls_sha256_free(&tContext);
      char tError[64];
      snprintf(tError, sizeof(tError), "Sha256 finish failed (%d)", tShaResult);
      SetError(tError);
      return false;
    }
    mbedtls_sha256_free(&tContext);
    tFirmwareFile.close();
    char tHex[65];
    for (int tIndex = 0; tIndex < 32; ++tIndex) sprintf(tHex + tIndex * 2, "%02x", tDigest[tIndex]);
    tHex[64] = '\0';
    if (strcasecmp(tHex, tExpectedHex) == 0) return true;
    SetError("Sha256 mismatch");
    return false;
  }

  bool Firmware_::PerformUpdate(Stream *tLog) {
    Guard tGuard;
    if (!EnsureUpdateBuffer()) return false;
    Storage_::Guard tStorageGuard;
    if (!STG.IsMounted()) {
      SetError("Storage not mounted");
      return false;
    }
    const esp_partition_t *tNextPartition = esp_ota_get_next_update_partition(nullptr);
    if (!tNextPartition) {
      SetError("Ota partition not found (requires ota_0/ota_1 partition table)");
      return false;
    }
    if (tLog) {
      const esp_partition_t *tRunning = esp_ota_get_running_partition();
      const esp_partition_t *tBoot = esp_ota_get_boot_partition();
      if (tRunning) tLog->printf("\r\n  Running partition: %s @ 0x%08x, size = 0x%x\r\n", tRunning->label, (unsigned)tRunning->address, (unsigned)tRunning->size);
      if (tBoot) tLog->printf("  Boot partition: %s @ 0x%08x\r\n\r\n", tBoot->label, (unsigned)tBoot->address);
      tLog->printf("  Update target: %s @ 0x%08x, size = 0x%x\r\n\r\n", tNextPartition->label, (unsigned)tNextPartition->address, (unsigned)tNextPartition->size);
    }
    const char *tFirmwarePath = STG.NormalizePath(mPath);
    File tFirmwareFile = STG.OpenFile(tFirmwarePath, FILE_READ);
    if (!tFirmwareFile) {
      SetError("Firmware file not found");
      return false;
    }
    size_t tSize = tFirmwareFile.size();
    if (tSize == 0) {
      tFirmwareFile.close();
      SetError("Firmware file empty");
      return false;
    }
    if (tLog) {
      tLog->printf("  Starting update, size = %u\r\n\r\n", (unsigned)tSize);
    }
    esp_ota_handle_t tUpdateHandle = 0;
    esp_err_t tErr = esp_ota_begin(tNextPartition, tSize, &tUpdateHandle);
    if (tErr != ESP_OK) {
      tFirmwareFile.close();
      char tError[128];
      snprintf(tError, sizeof(tError), "  Ota begin failed (error = %d)", (int)tErr);
      SetError(tError);
      return false;
    }
    uint8_t *tBuffer = mUpdateBuffer;
    size_t tBufferSize = mUpdateBufferSize;
    size_t tWritten = 0;
    while (tFirmwareFile.available()) {
      size_t tRead = tFirmwareFile.read(tBuffer, tBufferSize);
      if (tRead == 0) {
        tFirmwareFile.close();
        esp_ota_abort(tUpdateHandle);
        SetError("Firmware read failed");
        return false;
      }
      tErr = esp_ota_write(tUpdateHandle, (const void *)tBuffer, tRead);
      if (tErr != ESP_OK) {
        tFirmwareFile.close();
        esp_ota_abort(tUpdateHandle);
        char tError[128];
        snprintf(tError, sizeof(tError), "Ota write failed (error = %d)", (int)tErr);
        SetError(tError);
        return false;
      }
      tWritten += tRead;
      if (tLog && tSize > 0) {
        int tProgressPercent = (int)((tWritten * 100) / tSize);
        tLog->print("  Progress: ");
        tLog->print(tProgressPercent);
        tLog->println(" %");
      }
    }
    tErr = esp_ota_end(tUpdateHandle);
    if (tErr != ESP_OK) {
      tFirmwareFile.close();
      char tError[128];
      snprintf(tError, sizeof(tError), "Ota end failed (error = %d)", (int)tErr);
      SetError(tError);
      return false;
    }
    tErr = esp_ota_set_boot_partition(tNextPartition);
    if (tErr != ESP_OK) {
      tFirmwareFile.close();
      char tError[128];
      snprintf(tError, sizeof(tError), "Set boot partition failed (error = %d)", (int)tErr);
      SetError(tError);
      return false;
    }
    if (tLog) {
      const esp_partition_t *tBoot = esp_ota_get_boot_partition();
      if (tBoot) {
        tLog->printf("\r\n  New boot partition: %s @ 0x%08x\r\n", tBoot->label, (unsigned)tBoot->address);
      }
    }
    tFirmwareFile.close();
    CleanupUpdateDirIfExists(tLog);
    return true;
  }

  bool Firmware_::CleanupUpdateDirIfExists(Stream *tLog) {
    Guard tGuard;
    Storage_::Guard tStorageGuard;
    if (!STG.IsMounted()) {
      SetError("Storage not mounted");
      return false;
    }
    char tUpdateDir[128] = "";
    strncpy(tUpdateDir, STG.NormalizePath(FIRMWARE_DIR), sizeof(tUpdateDir) - 1);
    if (!STG.Exists(tUpdateDir)) return true;
    return CleanupUpdateDir(tLog);
  }

  bool Firmware_::CleanupUpdateDirOnBoot(Stream *tLog) {
    Guard tGuard;
    STG.Init(false);
    bool tOk = CleanupUpdateDirIfExists(tLog);
    STG.End();
    return tOk;
  }

  bool Firmware_::CleanupUpdateDir(Stream *tLog) {
    Storage_::Guard tStorageGuard;
    if (!STG.IsMounted()) {
      SetError("Storage not mounted");
      return false;
    }
    char tUpdateDir[128] = "";
    strncpy(tUpdateDir, STG.NormalizePath(FIRMWARE_DIR), sizeof(tUpdateDir) - 1);
    if (!STG.Exists(tUpdateDir)) return true;
    bool tAllOk = true;
    File tDir = STG.OpenFile(tUpdateDir, FILE_READ);
    if (!tDir || !tDir.isDirectory()) {
      if (tLog) tLog->println("  Warning: failed to open /firmware directory");
      SetError("Failed to open /firmware directory");
      return false;
    }
    struct SEntry {
      char path[128];
      bool isDir;
    };
    SEntry tEntries[16];
    size_t tEntryCount = 0;
    File tEntry = tDir.openNextFile();
    while (tEntry && tEntryCount < (sizeof(tEntries) / sizeof(tEntries[0]))) {
      bool tIsDir = tEntry.isDirectory();
      char tEntryName[128] = "";
      const char *tNamePtr = tEntry.name();
      if (tNamePtr && tNamePtr[0] != '\0') strncpy(tEntryName, tNamePtr, sizeof(tEntryName) - 1);
      tEntryName[sizeof(tEntryName) - 1] = '\0';
      tEntry.close();
      char tEntryPath[128] = "";
      if (tEntryName[0] == '/') strncpy(tEntryPath, tEntryName, sizeof(tEntryPath) - 1);
      else if (strchr(tEntryName, '/')) snprintf(tEntryPath, sizeof(tEntryPath), "/%s", tEntryName);
      else if (tEntryName[0] != '\0') snprintf(tEntryPath, sizeof(tEntryPath), "%s/%s", tUpdateDir, tEntryName);
      tEntryPath[sizeof(tEntryPath) - 1] = '\0';
      strncpy(tEntries[tEntryCount].path, tEntryPath, sizeof(tEntries[tEntryCount].path) - 1);
      tEntries[tEntryCount].path[sizeof(tEntries[tEntryCount].path) - 1] = '\0';
      tEntries[tEntryCount].isDir = tIsDir;
      tEntryCount++;
      tEntry = tDir.openNextFile();
    }
    if (tEntry) {
      tAllOk = false;
      if (tLog) tLog->println("  Warning: /firmware has too many entries to clean in one pass");
    }
    tDir.close();
    for (size_t i = 0; i < tEntryCount; ++i) {
      const char *tPath = tEntries[i].path;
      if (!tPath || tPath[0] == '\0') {
        tAllOk = false;
        if (tLog) tLog->println("  Warning: failed to delete: <unknown>");
        continue;
      }
      bool tDeleted = false;
      if (tEntries[i].isDir) {
        tDeleted = STG.RemoveDir(tPath);
      } else {
        tDeleted = STG.DeleteFile(tPath);
      }
      if (!tDeleted) {
        tAllOk = false;
        if (tLog) {
          tLog->print("  Warning: failed to delete: ");
          tLog->println(tPath);
        }
      }
    }
    if (!STG.RemoveDir(tUpdateDir)) {
      tAllOk = false;
      if (tLog) tLog->println("  Warning: failed to remove /firmware directory (not empty?)");
    }
    if (!tAllOk) SetError("Failed to remove /firmware directory");
    return tAllOk;
  }

  bool Firmware_::EnsureUpdateBuffer() {
    if (mUpdateBuffer && mUpdateBufferSize > 0) return true;
    uint8_t *tUpdateBuffer = static_cast<uint8_t *>(heap_caps_malloc(mUpdateBufferDefaultSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tUpdateBuffer) {
      SetError("Update buffer allocation failed");
      return false;
    }
    mUpdateBuffer = tUpdateBuffer;
    mUpdateBufferSize = mUpdateBufferDefaultSize;
    return true;
  }

} 