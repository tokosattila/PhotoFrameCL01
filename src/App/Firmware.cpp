#include <App/Firmware.h>

namespace App {

  namespace {
    static constexpr bool kEnableActivateFallback = false;
  }

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
  }

  void Firmware_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Firmware_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void Firmware_::Reset() {
    mActive = false;
    mError = false;
    mFinalEventSent = false;
    mWritten = 0;
    mTotal = 0;
    mLastActivityMs = 0;
    mTargetSubtype = ESP_PARTITION_SUBTYPE_APP_FACTORY;
    memset(mLastMessage, 0, sizeof(mLastMessage));
  }

  void Firmware_::Fail(const char *tLogMessage, const char *tMessage, bool tAbortUpdate) {
    mError = true;
    mLastActivityMs = millis();
    const char *tResolved = (tMessage && tMessage[0]) ? tMessage : "firmware_upload_failed";
    strncpy(mLastMessage, tResolved, sizeof(mLastMessage) - 1);
    mLastMessage[sizeof(mLastMessage) - 1] = '\0';
    if (tLogMessage && tLogMessage[0]) xLOG("%s", tLogMessage);
    if (tAbortUpdate && mActive) Update.abort();
  }

  void Firmware_::Begin(size_t tTotalSize) {
    Reset();
    mActive = true;
    mTotal = tTotalSize;
    mLastActivityMs = millis();
    const esp_partition_t *tTargetPartition = esp_ota_get_next_update_partition(nullptr);
    if (tTargetPartition) {
      mTargetSubtype = tTargetPartition->subtype;
      xLOG("Firmware target partition: %s @ 0x%08x", tTargetPartition->label, static_cast<unsigned>(tTargetPartition->address));
    }
    char tReadableSize[20] = {0};
    UTL.ByteToReadableSize(static_cast<uint64_t>(tTotalSize), tReadableSize, sizeof(tReadableSize));
    xLOG("Firmware upload started, size → %s", tReadableSize);
    const size_t tUpdateSize = UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(tUpdateSize)) {
      xLOG("Firmware begin failed, update error code: %u", static_cast<unsigned>(Update.getError()));
      Fail("Firmware begin failed", "firmware_upload_failed", false);
    }
  }

  void Firmware_::Write(uint8_t *tData, size_t tLength) {
    if (mError || !tData || tLength == 0) return;
    static constexpr size_t kWriteChunkSize = 1024;
    if (mWritten == 0 && tData[0] != 0xE9) {
      xLOG("Firmware image header invalid, magic: 0x%02X", static_cast<unsigned>(tData[0]));
      Fail("Firmware write failed", "firmware_upload_failed", true);
      return;
    }
    size_t tOffset = 0;
    while (tOffset < tLength) {
      const size_t tChunk = std::min(kWriteChunkSize, tLength - tOffset);
      esp_task_wdt_reset();
      if (Update.write(tData + tOffset, tChunk) != tChunk) {
        xLOG("Firmware write failed, update error code: %u", static_cast<unsigned>(Update.getError()));
        Fail("Firmware write failed", "firmware_upload_failed", true);
        return;
      }
      esp_task_wdt_reset();
      tOffset += tChunk;
    }
    mWritten += tLength;
    mLastActivityMs = millis();
  }

  bool Firmware_::TryActivateTargetPartitionFallback() {
    if (mTargetSubtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 || mTargetSubtype > ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
      xLOG("Firmware fallback activate skipped, invalid target subtype: %u", static_cast<unsigned>(mTargetSubtype));
      return false;
    }
    const esp_partition_t *tTargetPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, mTargetSubtype, nullptr);
    if (!tTargetPartition) {
      xLOG("Firmware fallback activate failed, target partition not found for subtype: %u", static_cast<unsigned>(mTargetSubtype));
      return false;
    }
    esp_task_wdt_reset();
    const esp_err_t tErr = esp_ota_set_boot_partition(tTargetPartition);
    if (tErr == ESP_OK) {
      xLOG("Firmware fallback activate succeeded: %s @ 0x%08x", tTargetPartition->label, static_cast<unsigned>(tTargetPartition->address));
      return true;
    }
    xLOG("Firmware fallback activate failed for %s @ 0x%08x, err:0x%08X", tTargetPartition->label, static_cast<unsigned>(tTargetPartition->address), static_cast<unsigned>(tErr));
    return false;
  }

  void Firmware_::Finalize() {
    if (mError) return;
    mLastActivityMs = millis();
    if (mTotal > 0 && mWritten != mTotal) {
      xLOG("Firmware upload payload differs from request size (multipart overhead expected), written: %u, request: %u",
        static_cast<unsigned>(mWritten), static_cast<unsigned>(mTotal));
    }
    esp_task_wdt_reset();
    if (!Update.end(true)) {
      const uint8_t tUpdateError = Update.getError();
      if (tUpdateError == UPDATE_ERROR_ACTIVATE && kEnableActivateFallback && TryActivateTargetPartitionFallback()) {
        xLOG("Firmware upload finished successfully (fallback activate)");
        mFinalEventSent = true;
        mActive = false;
        mLastActivityMs = millis();
        return;
      }
      if (tUpdateError == UPDATE_ERROR_ACTIVATE && !kEnableActivateFallback) {
        xLOG("Firmware activate failed (UPDATE_ERROR_ACTIVATE). Fallback retries are disabled to avoid repeated bootloader_mmap failures");
      }
      xLOG("Firmware finalize failed, written: %u, expected: %u, update error code: %u", static_cast<unsigned>(mWritten), static_cast<unsigned>(mTotal), static_cast<unsigned>(Update.getError()));
      Fail("Firmware finalize failed", "firmware_upload_failed", true);
      return;
    }
    esp_task_wdt_reset();
    xLOG("Firmware upload finished successfully");
    mFinalEventSent = true;
    mActive = false;
    mLastActivityMs = millis();
  }

  void Firmware_::Abort() {
    if (mActive) {
      xLOG("Firmware upload aborted");
      Update.abort();
    }
    Reset();
  }

} 