#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <App/Global.h>

namespace App {

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
      bool SetAlarm(const SAlarmSpec &tSpec);
      bool DisableAlarm();
      bool ClearAlarmFlag();
      bool IsAlarmTriggered();
      bool GetAlarm(SAlarmSpec &tSpec);
      void GetTime(char *tBuffer, size_t tSize);
      void GetDate(char *tBuffer, size_t tSize);
      void GetDateTime(char *tBuffer, size_t tSize);
      void PrintInfo();
      static unsigned long DateTimeToEpoch(const SRTCDateTime &tDateTime);
      static void EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDateTime);
    private:
      RTC_();
      RTC_(const RTC_&) = delete;
      RTC_ &operator=(const RTC_&) = delete;
      ~RTC_();
      static constexpr uint8_t kAddress = RTC_ADDRESS;
      static constexpr uint8_t kSdaPin = RTC_SDA_PIN;
      static constexpr uint8_t kSclPin = RTC_SCL_PIN;
      static constexpr uint8_t kIntPin = RTC_INT_PIN;
      static constexpr uint8_t kRegControl2 = 0x01;
      static constexpr uint8_t kRegAlarmSecond = 0x0B;
      static constexpr uint8_t kRegAlarmMinute = 0x0C;
      static constexpr uint8_t kRegAlarmHour = 0x0D;
      static constexpr uint8_t kRegAlarmDay = 0x0E;
      static constexpr uint8_t kRegAlarmWeekday = 0x0F;
      static constexpr uint8_t kBitAie = 0x80;
      static constexpr uint8_t kBitAf = 0x40;
      static constexpr uint8_t kBitAen = 0x80;
      mutable SemaphoreHandle_t mMutex = nullptr;
      bool mAvailable = false;
      static void Lock();
      static void Unlock();
      bool TryI2C();
      bool ReadDateTimeRaw(SRTCDateTime &tDateTime, bool &tOsFlagSet);
      bool ReadDateTime(SRTCDateTime &tDateTime);
      bool WriteDateTime(const SRTCDateTime &tDateTime);
      bool ReadRegister(uint8_t tReg, uint8_t &tValue);
      bool WriteRegister(uint8_t tReg, uint8_t tValue);
      bool WriteAlarmRegisters(const SAlarmSpec &tSpec);
      bool ReadAlarmRegisters(SAlarmSpec &tSpec);
      static bool IsDateTimePlausible(const SRTCDateTime &tDateTime);
      static uint8_t BcdToDec(uint8_t tBcd);
      static uint8_t DecToBcd(uint8_t tDec);
  };

}

#endif
