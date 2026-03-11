#include <App/RTCTime.h>

namespace App {

  RTCTime_ &RTCTime_::Instance() {
    static RTCTime_ tInstance;
    return tInstance;
  }

  RTCTime_::RTCTime_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  RTCTime_::~RTCTime_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void RTCTime_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void RTCTime_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool RTCTime_::Init(bool tVerbose) {
    Guard tLock;
    mWire.begin(mSdaPin, mSclPin);
    mAvailable = TryI2C();
    if (mAvailable) {
      SRTCDateTime tDateTime {};
      if (!ReadDateTime(tDateTime)) mAvailable = false;
    }
    if (tVerbose) {
      if (mAvailable) {
        xLOG("RTC → found PCF85063");
        PrintInfo();
      } else xLOG("RTC → not detected on I2C bus");
    }
    return mAvailable;
  }

  void RTCTime_::End() {
    Guard tLock;
    mWire.end();
    mAvailable = false;
  }

  bool RTCTime_::TryI2C() {
    mWire.beginTransmission(mAddress);
    return (mWire.endTransmission() == 0);
  }

  bool RTCTime_::GetDateTime(SRTCDateTime &tDateTime) {
    Guard tLock;
    if (!mAvailable) return false;
    return ReadDateTime(tDateTime);
  }

  bool RTCTime_::ReadDateTime(SRTCDateTime &tDateTime) {
    mWire.beginTransmission(mAddress);
    mWire.write(0x04);
    if (mWire.endTransmission() != 0) return false;
    if (mWire.requestFrom(mAddress, (uint8_t)7) < 7) return false;
    uint8_t tSecRaw = mWire.read();
    if (tSecRaw & 0x80) xLOG("RTC → PCF85063 OS flag set clock data may be invalid");
    tDateTime.Second = BcdToDec(tSecRaw & 0x7F);
    tDateTime.Minute = BcdToDec(mWire.read() & 0x7F);
    tDateTime.Hour = BcdToDec(mWire.read() & 0x3F);
    tDateTime.Day = BcdToDec(mWire.read() & 0x3F);
    tDateTime.DayOfWeek = BcdToDec(mWire.read() & 0x07);
    tDateTime.Month = BcdToDec(mWire.read() & 0x1F);
    tDateTime.Year = 2000 + BcdToDec(mWire.read());
    return IsDateTimePlausible(tDateTime);
  }

  bool RTCTime_::SetDateTime(const SRTCDateTime &tDateTime) {
    Guard tLock;
    if (!mAvailable) return false;
    return WriteDateTime(tDateTime);
  }

  bool RTCTime_::WriteDateTime(const SRTCDateTime &tDateTime) {
    mWire.beginTransmission(mAddress);
    mWire.write(0x04);
    mWire.write(DecToBcd(tDateTime.Second) & 0x7F);
    mWire.write(DecToBcd(tDateTime.Minute) & 0x7F);
    mWire.write(DecToBcd(tDateTime.Hour) & 0x3F);
    mWire.write(DecToBcd(tDateTime.Day) & 0x3F);
    mWire.write(DecToBcd(tDateTime.DayOfWeek) & 0x07);
    mWire.write(DecToBcd(tDateTime.Month) & 0x1F);
    uint8_t tYear = static_cast<uint8_t>((tDateTime.Year >= 2000) ? (tDateTime.Year - 2000) : 0);
    mWire.write(DecToBcd(tYear));
    return (mWire.endTransmission() == 0);
  }

  bool RTCTime_::IsDateTimePlausible(const SRTCDateTime &tDateTime) {
    if (tDateTime.Second > 59) return false;
    if (tDateTime.Minute > 59) return false;
    if (tDateTime.Hour > 23) return false;
    if (tDateTime.Month < 1 || tDateTime.Month > 12) return false;
    if (tDateTime.Day < 1 || tDateTime.Day > 31) return false;
    if (tDateTime.DayOfWeek > 6) return false;
    if (tDateTime.Year < 2000 || tDateTime.Year > 2099) return false;
    return true;
  }

  uint8_t RTCTime_::BcdToDec(uint8_t tBcd) {
    return ((tBcd >> 4) * 10) + (tBcd & 0x0F);
  }

  uint8_t RTCTime_::DecToBcd(uint8_t tDec) {
    return ((tDec / 10) << 4) | (tDec % 10);
  }

  unsigned long RTCTime_::DateTimeToEpoch(const SRTCDateTime &tDateTime) {
    unsigned long tDays = 0;
    for (uint16_t y = 1970; y < tDateTime.Year; y++) {
      tDays += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
    }
    static const uint8_t tDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (uint8_t m = 1; m < tDateTime.Month; m++) {
      tDays += tDaysInMonth[m - 1];
      if (m == 2 && (tDateTime.Year % 4 == 0 && (tDateTime.Year % 100 != 0 || tDateTime.Year % 400 == 0))) tDays++;
    }
    tDays += tDateTime.Day - 1;
    return tDays * 86400UL + tDateTime.Hour * 3600UL + tDateTime.Minute * 60UL + tDateTime.Second;
  }

  void RTCTime_::EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDateTime) {
    unsigned long tSeconds = tEpoch % 60;
    tEpoch /= 60;
    unsigned long tMinutes = tEpoch % 60;
    tEpoch /= 60;
    unsigned long tHours = tEpoch % 24;
    tEpoch /= 24;
    tDateTime.Second = tSeconds;
    tDateTime.Minute = tMinutes;
    tDateTime.Hour = tHours;
    unsigned long tDays = tEpoch;
    tDateTime.DayOfWeek = ((tDays + 4) % 7);
    uint16_t tYear = 1970;
    while (true) {
      uint16_t tDaysInYear = (tYear % 4 == 0 && (tYear % 100 != 0 || tYear % 400 == 0)) ? 366 : 365;
      if (tDays < tDaysInYear) break;
      tDays -= tDaysInYear;
      tYear++;
    }
    tDateTime.Year = tYear;
    static const uint8_t tDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t tMonth = 1;
    while (tMonth <= 12) {
      uint8_t tDim = tDaysInMonth[tMonth - 1];
      if (tMonth == 2 && (tYear % 4 == 0 && (tYear % 100 != 0 || tYear % 400 == 0))) tDim = 29;
      if (tDays < tDim) break;
      tDays -= tDim;
      tMonth++;
    }
    tDateTime.Month = tMonth;
    tDateTime.Day = tDays + 1;
  }

  unsigned long RTCTime_::GetEpoch() {
    Guard tLock;
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) return 0;
    return DateTimeToEpoch(tDateTime);
  }

  bool RTCTime_::SetFromEpoch(unsigned long tEpoch) {
    Guard tLock;
    SRTCDateTime tDateTime;
    EpochToDateTime(tEpoch, tDateTime);
    return SetDateTime(tDateTime);
  }

  bool RTCTime_::SyncFromNTP() {
    Guard tLock;
    unsigned long tEpoch = NTP.GetCurrentEpochUTC();
    if (tEpoch == 0) {
      xLOG("RTC → NTP time not available");
      return false;
    }
    bool tOk = SetFromEpoch(tEpoch);
    if (tOk) xLOG("RTC → synced from NTP");
    return tOk;
  }

  bool RTCTime_::SyncToSystem() {
    Guard tLock;
    unsigned long tEpoch = GetEpoch();
    if (tEpoch == 0) {
      xLOG("RTC → epoch is 0 cannot sync");
      return false;
    }
    if (tEpoch > 2147483647UL) {
      xLOG("RTC → epoch %lu exceeds 32-bit signed max (Y2038 problem) cannot sync", tEpoch);
      return false;
    }
    if (tEpoch < 1735689600UL) {
      xLOG("RTC → epoch %lu is before 2026, is invalid", tEpoch);
      return false;
    }
    struct timeval tTv;
    tTv.tv_sec = (time_t)tEpoch;
    tTv.tv_usec = 0;
    if (settimeofday(&tTv, nullptr) != 0) {
      xLOG("RTC → settimeofday failed");
      return false;
    }
    xLOG("RTC → system time synced from RTC");
    return true;
  }

  bool RTCTime_::SyncFromSystem() {
    Guard tLock;
    struct timeval tTv;
    gettimeofday(&tTv, nullptr);
    if (tTv.tv_sec < 1735689600) return false;
    bool tOk = SetFromEpoch(tTv.tv_sec);
    if (tOk) xLOG("RTC → synced from system time");
    return tOk;
  }

  void RTCTime_::GetTime(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "--:--:--");
      return;
    }
    snprintf(tBuffer, tSize, "%02d:%02d:%02d", tDateTime.Hour, tDateTime.Minute, tDateTime.Second);
  }

  void RTCTime_::GetDate(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "----.--.--");
      return;
    }
    snprintf(tBuffer, tSize, "%04d.%02d.%02d", tDateTime.Year, tDateTime.Month, tDateTime.Day);
  }

  void RTCTime_::GetDateTime(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "----.--.-- --:--:--");
      return;
    }
    snprintf(tBuffer, tSize, "%04d.%02d.%02d %02d:%02d:%02d", tDateTime.Year, tDateTime.Month, tDateTime.Day, tDateTime.Hour, tDateTime.Minute, tDateTime.Second);
  }

  void RTCTime_::PrintInfo() {
    SRTCDateTime tDt;
    if (GetDateTime(tDt)) {
      xLOG("RTC → DateTime %04d.%02d.%02d %02d:%02d:%02d", tDt.Year, tDt.Month, tDt.Day, tDt.Hour, tDt.Minute, tDt.Second);
    } else xLOG("RTC → DateTime read failed");
  }

}
