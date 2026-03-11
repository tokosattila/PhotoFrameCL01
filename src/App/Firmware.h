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
      bool Init();
      bool UpdateAvailable();
      bool VerifySha256(const char *tShaPath = nullptr);
      bool PerformUpdate(Stream *tLogStream = nullptr);
      bool CleanupUpdateDirIfExists(Stream *tLogStream = nullptr);
      bool CleanupUpdateDirOnBoot(Stream *tLogStream = nullptr);
      const char *GetLastError() const;
    private:
      Firmware_();
      ~Firmware_();
      Firmware_(const Firmware_ &) = delete;
      Firmware_ &operator=(const Firmware_ &) = delete;
      mutable SemaphoreHandle_t mMutex = nullptr;
      static constexpr size_t mUpdateBufferDefaultSize = 4096;
      uint8_t *mUpdateBuffer = nullptr;
      size_t mUpdateBufferSize = 0;
      const char *mPath = FIRMWARE_PATH;
      const char *mShaPath = FIRMWARE_SHA_PATH;
      char mLastError[256] = "";
      void SetError(const char *tErrorMessage);
      bool EnsureUpdateBuffer();
      bool CleanupUpdateDir(Stream *tLogStream);
    };

}

#endif