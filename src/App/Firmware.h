#ifndef FIRMWARE_H
#define FIRMWARE_H

#include <App/Global.h>

namespace App {

  class Firmware_ {
    DEFINE_TAG("FWU");
    friend class AutoGuard<Firmware_>;
    public:
      using Guard = AutoGuard<Firmware_>;
      static Firmware_ &Instance();
      static void Lock();
      static void Unlock();
      bool IsActive() const { return mActive; }
      bool HasError() const { return mError; }
      bool IsFinalEventSent() const { return mFinalEventSent; }
      size_t GetWritten() const { return mWritten; }
      size_t GetTotal() const { return mTotal; }
      uint32_t GetLastActivityMs() const { return mLastActivityMs; }
      const char *GetLastMessage() const { return mLastMessage; }
      void Begin(size_t tTotalSize);
      void Write(uint8_t *tData, size_t tLength);
      void Finalize();
      void Abort();
      void Reset();
    private:
      Firmware_();
      ~Firmware_();
      Firmware_(const Firmware_ &) = delete;
      Firmware_ &operator=(const Firmware_ &) = delete;
      mutable SemaphoreHandle_t mMutex = nullptr;
      bool mActive = false;
      bool mError = false;
      bool mFinalEventSent = false;
      size_t mWritten = 0;
      size_t mTotal = 0;
      uint32_t mLastActivityMs = 0;
      esp_partition_subtype_t mTargetSubtype = ESP_PARTITION_SUBTYPE_APP_FACTORY;
      char mLastMessage[64] = {};
      void Fail(const char *tLogMessage, const char *tMessage, bool tAbortUpdate);
      bool TryActivateTargetPartitionFallback();
  };

}

#endif