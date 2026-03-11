#ifndef NTP_H
#define NTP_H

#include <App/Global.h>

namespace App {

  class NTP_ {
    DEFINE_TAG("NTP");
    friend class AutoGuard<NTP_>;
    public:
      using Guard = AutoGuard<NTP_>;
      static NTP_ &Instance();
      void Init();
      void ReloadConfig(); 
      bool Begin();
      void End();
      bool IsAvailable();
      unsigned long EpochTime();
      void GetTime(char *tOutputBuffer, uint8_t tBufferSize, char tFormat = ' ');
      void GetDate(char *tOutputBuffer, uint8_t tBufferSize, char tFormat = ' ');
      unsigned long GetCurrentEpoch();
      unsigned long GetCurrentEpochUTC();
      String Time(char format = ' ');
      String Date(char format = ' ');
      bool SyncSystemTime();
      void PrintDateTimeInfo();
    private:
      NTP_();
      NTP_(const NTP_&) = delete;
      NTP_ &operator=(const NTP_&) = delete;
      ~NTP_();
      WiFiUDP mUDP;
      mutable SemaphoreHandle_t mMutex;
      SNTPConfig mCfg {};
      static constexpr unsigned long mSevenZYYears = 2208988800UL;
      static const uint8_t kNtpPacketSize = 48;
      bool mUDPSetup = false;
      unsigned long mCurrentEpoch = 0;
      unsigned long mLastUpdate = 0;
      uint8_t mPacketBuffer[kNtpPacketSize];
      static void Lock();
      static void Unlock();
      bool IsPacketValid(uint8_t *tPacket);
      bool UpdateTime();
      bool ForceTimeSync();
      void SendNtpRequest();
      void FormatTwoDigits(char *tOutputBuffer, int tValue);
      bool IsLeapYear(unsigned long tYear);
      bool IsDST(unsigned long tEpoch);
      int8_t GetGMTOffset();
      const char *GetTimezoneName();

  };

}

#endif
