#include <App/RTC.h>

namespace App {

  namespace {
    constexpr uint8_t kI2CRetryCount = 3;
  }

  RTC_ &RTC_::Instance() {
    static RTC_ tInstance;
    return tInstance;
  }

  RTC_::RTC_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  RTC_::~RTC_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void RTC_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void RTC_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool RTC_::Init(bool tVerbose) {
    Guard tLock;
    {
      I2CBusGuard tBusLock;
      Wire.begin(kSdaPin, kSclPin);
    }
    mAvailable = TryI2C();
    if (tVerbose) {
      if (mAvailable) xLOG("RTC is available, found PCF85063");
      else xLOG("RTC is not available, PCF85063 not detected on I2C bus");
    }
    if (mAvailable && tVerbose) PrintInfo();
    return mAvailable;
  }

  void RTC_::End() {
    Guard tLock;
    {
      I2CBusGuard tBusLock;
      Wire.end();
    }
    mAvailable = false;
  }

  bool RTC_::TryI2C() {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool RTC_::GetDateTime(SRTCDateTime &tDateTime) {
    Guard tLock;
    if (!mAvailable) return false;
    return ReadDateTime(tDateTime);
  }

  bool RTC_::ReadDateTimeRaw(SRTCDateTime &tDateTime, bool &tOsFlagSet) {
    I2CBusGuard tBusLock;
    tOsFlagSet = false;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(0x04);
      if (Wire.endTransmission() != 0) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      if (Wire.requestFrom(kAddress, (uint8_t)7) < 7) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      uint8_t tSecRaw = Wire.read();
      tOsFlagSet = ((tSecRaw & 0x80) != 0);
      tDateTime.Second = BcdToDec(tSecRaw & 0x7F);
      tDateTime.Minute = BcdToDec(Wire.read() & 0x7F);
      tDateTime.Hour = BcdToDec(Wire.read() & 0x3F);
      tDateTime.Day = BcdToDec(Wire.read() & 0x3F);
      tDateTime.DayOfWeek = BcdToDec(Wire.read() & 0x07);
      tDateTime.Month = BcdToDec(Wire.read() & 0x1F);
      tDateTime.Year = 2000 + BcdToDec(Wire.read());
      return true;
    }
    return false;
  }

  bool RTC_::ReadDateTime(SRTCDateTime &tDateTime) {
    bool tOsFlagSet = false;
    if (!ReadDateTimeRaw(tDateTime, tOsFlagSet)) return false;
    if (tOsFlagSet) {
      xLOG("PCF85063 OS flag set, RTC time is invalid");
      return false;
    }
    if (!IsDateTimePlausible(tDateTime)) return false;
    return true;
  }

  bool RTC_::SetDateTime(const SRTCDateTime &tDateTime) {
    Guard tLock;
    if (!mAvailable) return false;
    return WriteDateTime(tDateTime);
  }

  bool RTC_::WriteDateTime(const SRTCDateTime &tDateTime) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(0x04);
      Wire.write(DecToBcd(tDateTime.Second) & 0x7F);
      Wire.write(DecToBcd(tDateTime.Minute) & 0x7F);
      Wire.write(DecToBcd(tDateTime.Hour) & 0x3F);
      Wire.write(DecToBcd(tDateTime.Day) & 0x3F);
      Wire.write(DecToBcd(tDateTime.DayOfWeek) & 0x07);
      Wire.write(DecToBcd(tDateTime.Month) & 0x1F);
      uint8_t tYear = static_cast<uint8_t>((tDateTime.Year >= 2000) ? (tDateTime.Year - 2000) : 0);
      Wire.write(DecToBcd(tYear));
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool RTC_::ReadRegister(uint8_t tReg, uint8_t &tValue) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(tReg);
      if (Wire.endTransmission() != 0) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      if (Wire.requestFrom(kAddress, (uint8_t)1) < 1) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      tValue = Wire.read();
      return true;
    }
    return false;
  }

  bool RTC_::WriteRegister(uint8_t tReg, uint8_t tValue) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(tReg);
      Wire.write(tValue);
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool RTC_::WriteAlarmRegisters(const SAlarmSpec &tSpec) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(kRegAlarmSecond);
      Wire.write(kBitAen);
      uint8_t tMinReg = DecToBcd(tSpec.Minute) & 0x7F;
      if (!tSpec.EnableMinute) tMinReg |= kBitAen;
      Wire.write(tMinReg);
      uint8_t tHourReg = DecToBcd(tSpec.Hour) & 0x3F;
      if (!tSpec.EnableHour) tHourReg |= kBitAen;
      Wire.write(tHourReg);
      uint8_t tDayReg = DecToBcd(tSpec.Day) & 0x3F;
      if (!tSpec.EnableDay) tDayReg |= kBitAen;
      Wire.write(tDayReg);
      uint8_t tWdReg = DecToBcd(tSpec.Weekday) & 0x07;
      if (!tSpec.EnableWeekday) tWdReg |= kBitAen;
      Wire.write(tWdReg);
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool RTC_::ReadAlarmRegisters(SAlarmSpec &tSpec) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(kRegAlarmSecond);
      if (Wire.endTransmission() != 0) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      if (Wire.requestFrom(kAddress, (uint8_t)5) < 5) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      Wire.read();
      uint8_t tMinRaw = Wire.read();
      uint8_t tHourRaw = Wire.read();
      uint8_t tDayRaw = Wire.read();
      uint8_t tWdRaw = Wire.read();
      tSpec.EnableMinute = ((tMinRaw & kBitAen) == 0);
      tSpec.EnableHour = ((tHourRaw & kBitAen) == 0);
      tSpec.EnableDay = ((tDayRaw & kBitAen) == 0);
      tSpec.EnableWeekday = ((tWdRaw & kBitAen) == 0);
      tSpec.Minute = BcdToDec(tMinRaw & 0x7F);
      tSpec.Hour = BcdToDec(tHourRaw & 0x3F);
      tSpec.Day = BcdToDec(tDayRaw & 0x3F);
      tSpec.Weekday = BcdToDec(tWdRaw & 0x07);
      return true;
    }
    return false;
  }

  bool RTC_::SetAlarm(const SAlarmSpec &tSpec) {
    Guard tLock;
    if (!mAvailable) return false;
    uint8_t tCtrl2 = 0;
    if (!ReadRegister(kRegControl2, tCtrl2)) {
      xLOG("RTC SetAlarm: Control_2 read failed");
      return false;
    }
    uint8_t tCtrl2Cleared = (uint8_t)((tCtrl2 & ~(kBitAie | kBitAf)));
    if (!WriteRegister(kRegControl2, tCtrl2Cleared)) {
      xLOG("RTC SetAlarm: Control_2 disable write failed");
      return false;
    }
    if (!WriteAlarmRegisters(tSpec)) {
      xLOG("RTC SetAlarm: alarm registers write failed");
      return false;
    }
    uint8_t tCtrl2Enable = (uint8_t)((tCtrl2Cleared | kBitAie) & ~kBitAf);
    if (!WriteRegister(kRegControl2, tCtrl2Enable)) {
      xLOG("RTC SetAlarm: Control_2 enable write failed");
      return false;
    }
    return true;
  }

  bool RTC_::DisableAlarm() {
    Guard tLock;
    if (!mAvailable) return false;
    uint8_t tCtrl2 = 0;
    if (!ReadRegister(kRegControl2, tCtrl2)) return false;
    uint8_t tCtrl2New = (uint8_t)(tCtrl2 & ~(kBitAie | kBitAf));
    if (!WriteRegister(kRegControl2, tCtrl2New)) return false;
    SAlarmSpec tNone;
    tNone.EnableMinute = false;
    tNone.EnableHour = false;
    tNone.EnableDay = false;
    tNone.EnableWeekday = false;
    return WriteAlarmRegisters(tNone);
  }

  bool RTC_::ClearAlarmFlag() {
    Guard tLock;
    if (!mAvailable) return false;
    uint8_t tCtrl2 = 0;
    if (!ReadRegister(kRegControl2, tCtrl2)) return false;
    uint8_t tCtrl2New = (uint8_t)(tCtrl2 & ~kBitAf);
    return WriteRegister(kRegControl2, tCtrl2New);
  }

  bool RTC_::IsAlarmTriggered() {
    Guard tLock;
    if (!mAvailable) return false;
    uint8_t tCtrl2 = 0;
    if (!ReadRegister(kRegControl2, tCtrl2)) return false;
    return ((tCtrl2 & kBitAf) != 0);
  }

  bool RTC_::GetAlarm(SAlarmSpec &tSpec) {
    Guard tLock;
    if (!mAvailable) return false;
    return ReadAlarmRegisters(tSpec);
  }

  bool RTC_::IsDateTimePlausible(const SRTCDateTime &tDateTime) {
    if (tDateTime.Second > 59) return false;
    if (tDateTime.Minute > 59) return false;
    if (tDateTime.Hour > 23) return false;
    if (tDateTime.Month < 1 || tDateTime.Month > 12) return false;
    if (tDateTime.Day < 1 || tDateTime.Day > 31) return false;
    if (tDateTime.DayOfWeek > 6) return false;
    if (tDateTime.Year < 2000 || tDateTime.Year > 2099) return false;
    return true;
  }

  uint8_t RTC_::BcdToDec(uint8_t tBcd) {
    return ((tBcd >> 4) * 10) + (tBcd & 0x0F);
  }

  uint8_t RTC_::DecToBcd(uint8_t tDec) {
    return ((tDec / 10) << 4) | (tDec % 10);
  }

  unsigned long RTC_::DateTimeToEpoch(const SRTCDateTime &tDateTime) {
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

  void RTC_::EpochToDateTime(unsigned long tEpoch, SRTCDateTime &tDateTime) {
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

  unsigned long RTC_::GetEpoch() {
    Guard tLock;
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) return 0;
    return DateTimeToEpoch(tDateTime);
  }

  bool RTC_::SetFromEpoch(unsigned long tEpoch) {
    Guard tLock;
    SRTCDateTime tDateTime;
    EpochToDateTime(tEpoch, tDateTime);
    return SetDateTime(tDateTime);
  }

  bool RTC_::SyncFromNTP() {
    Guard tLock;
    unsigned long tEpoch = NTP.GetCurrentEpochUTC();
    if (tEpoch == 0) {
      xLOG("NTP time is not available");
      return false;
    }
    bool tOk = SetFromEpoch(tEpoch);
    if (tOk) xLOG("DateTime synced from NTP");
    return tOk;
  }

  bool RTC_::SyncToSystem() {
    Guard tLock;
    if (!mAvailable) {
      xLOG("RTC is not available, cannot sync");
      return false;
    }
    SRTCDateTime tDateTime;
    bool tOsFlagSet = false;
    if (!ReadDateTimeRaw(tDateTime, tOsFlagSet)) {
      xLOG("RTC read failed, cannot sync");
      return false;
    }
    if (tOsFlagSet) {
      xLOG("RTC time is invalid (OS flag set), falling back to Unix epoch");
      struct timeval tFallbackTv = {};
      tFallbackTv.tv_sec = 0;
      tFallbackTv.tv_usec = 0;
      if (settimeofday(&tFallbackTv, nullptr) != 0) {
        xLOG("RTC fallback settimeofday failed");
        return false;
      }
      return true;
    }
    if (!IsDateTimePlausible(tDateTime)) {
      xLOG("RTC time is not plausible, falling back to Unix epoch");
      struct timeval tFallbackTv = {};
      tFallbackTv.tv_sec = 0;
      tFallbackTv.tv_usec = 0;
      if (settimeofday(&tFallbackTv, nullptr) != 0) {
        xLOG("RTC fallback settimeofday failed");
        return false;
      }
      return true;
    }
    unsigned long tEpoch = DateTimeToEpoch(tDateTime);
    if (tEpoch > 2147483647UL) {
      xLOG("Epoch %lu exceeds 32-bit signed max (Y2038 problem), cannot sync", tEpoch);
      return false;
    }
    struct timeval tTv;
    tTv.tv_sec = (time_t)tEpoch;
    tTv.tv_usec = 0;
    if (settimeofday(&tTv, nullptr) != 0) {
      xLOG("RTC settimeofday failed");
      return false;
    }
    xLOG("System time synced from RTC");
    return true;
  }

  bool RTC_::SyncFromSystem() {
    Guard tLock;
    struct timeval tTv;
    gettimeofday(&tTv, nullptr);
    if (tTv.tv_sec < 0) return false;
    bool tOk = SetFromEpoch(tTv.tv_sec);
    if (tOk) xLOG("RTC synced from system time");
    return tOk;
  }

  void RTC_::GetTime(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "--:--:--");
      return;
    }
    snprintf(tBuffer, tSize, "%02d:%02d:%02d", tDateTime.Hour, tDateTime.Minute, tDateTime.Second);
  }

  void RTC_::GetDate(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "----.--.--");
      return;
    }
    snprintf(tBuffer, tSize, "%04d.%02d.%02d", tDateTime.Year, tDateTime.Month, tDateTime.Day);
  }

  void RTC_::GetDateTime(char *tBuffer, size_t tSize) {
    SRTCDateTime tDateTime;
    if (!GetDateTime(tDateTime)) {
      snprintf(tBuffer, tSize, "----.--.-- --:--:--");
      return;
    }
    snprintf(tBuffer, tSize, "%04d.%02d.%02d %02d:%02d:%02d", tDateTime.Year, tDateTime.Month, tDateTime.Day, tDateTime.Hour, tDateTime.Minute, tDateTime.Second);
  }

  void RTC_::PrintInfo() {
    Guard tLock;
    SRTCDateTime tDt;
    bool tOsFlagSet = false;
    if (!mAvailable) {
      xLOG("DateTime read failed");
      return;
    }
    if (!ReadDateTimeRaw(tDt, tOsFlagSet)) {
      xLOG("DateTime read failed");
      return;
    }
    if (tOsFlagSet) {
      xLOG("PCF85063 OS flag set, RTC time is invalid");
      EpochToDateTime(0UL, tDt);
    } else if (!IsDateTimePlausible(tDt)) {
      xLOG("RTC time is not plausible");
      EpochToDateTime(0UL, tDt);
    }
    xLOG("DateTime → %04d.%02d.%02d %02d:%02d:%02d", tDt.Year, tDt.Month, tDt.Day, tDt.Hour, tDt.Minute, tDt.Second);
  }

}
