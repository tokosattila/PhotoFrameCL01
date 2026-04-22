#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <App/Global.h>

namespace App {

  struct SRTCDateTime {
    uint8_t Second = 0;
    uint8_t Minute = 0;
    uint8_t Hour = 0;
    uint8_t DayOfWeek = 0;
    uint8_t Day = 1;
    uint8_t Month = 1;
    uint16_t Year = 2026;
  };

  class RTC_ {
    DEFINE_TAG("RTC");
    friend class AutoGuard<RTC_>;
    public:
      using Guard = AutoGuard<RTC_>;
      static RTC_ &Instance();
      bool Init(bool tVerbose = false);
      void End();
      bool IsAvailable() const { return mAvailable; }
      bool GetDateTime(SRTCDateTime &tDateTime);
      bool SetDateTime(const SRTCDateTime &tDateTime);
      unsigned long GetEpoch();
      bool SetFromEpoch(unsigned long tEpoch);
      bool SyncFromNTP();
      bool SyncToSystem();
      bool SyncFromSystem();
      void GetTime(char *tBuffer, size_t tSize);
      void GetDate(char *tBuffer, size_t tSize);
      void GetDateTime(char *tBuffer, size_t tSize);
      void PrintInfo();
    private:
      RTC_();
      RTC_(const RTC_&) = delete;
      RTC_ &operator=(const RTC_&) = delete;
      ~RTC_();
      static constexpr uint8_t kAddress = RTC_ADDRESS;
      static constexpr uint8_t kSdaPin = RTC_SDA_PIN;
      static constexpr uint8_t kSclPin = RTC_SCL_PIN;
      mutable SemaphoreHandle_t mMutex = nullptr;
      bool mAvailable = false;
      static void Lock();
      static void Unlock();
      bool TryI2C();
      bool ReadDateTimeRaw(SRTCDateTime &tDateTime, bool &tOsFlagSet);
      bool ReadDateTime(SRTCDateTime &tDateTime);
      bool WriteDateTime(const SRTCDateTime &tDateTime);
      static bool IsDateTimePlausible(const SRTCDateTime &tDateTime);
      static uint8_t BcdToDec(uint8_t tBcd);
      static uint8_t DecToBcd(uint8_t tDec);
      static unsigned long DateTimeToEpoch(const SRTCDateTime &tDateTime);
      static void EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDateTime);
  };

}

#endif
