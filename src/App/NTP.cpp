#include <App/NTP.h>

namespace App {

  static int32_t ResolveUtcOffsetMinutesFromEpoch(time_t tEpochUtc) {
    struct tm tLocalTime = {};
    struct tm tUtcTime = {};
    localtime_r(&tEpochUtc, &tLocalTime);
    gmtime_r(&tEpochUtc, &tUtcTime);
    tLocalTime.tm_isdst = -1;
    tUtcTime.tm_isdst = 0;
    const time_t tLocalEpoch = mktime(&tLocalTime);
    const time_t tUtcEpochAsLocal = mktime(&tUtcTime);
    return static_cast<int32_t>((tLocalEpoch - tUtcEpochAsLocal) / 60);
  }

  NTP_ &NTP_::Instance() {
    static NTP_ tInstance;
    return tInstance;
  }

  NTP_::NTP_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  NTP_::~NTP_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void NTP_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void NTP_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void NTP_::Init() {
    ReloadConfig();
  }

  void NTP_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SNTPConfig>();
    ApplyTimeZone();
  }

  bool NTP_::IsAvailable() {
    wifi_mode_t tMode = WiFi.getMode();
    if (tMode == WIFI_AP || tMode == WIFI_OFF) {
      xLOG("NTP not available, WiFi in AP mode or OFF");
      return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
      xLOG("NTP not available, WiFi not connected");
      return false;
    }
    return true;
  }

  bool NTP_::Begin() {
    if (!IsAvailable()) {
      mCurrentEpoch = 0;
      mUDPSetup = false;
      return false;
    }
    if (mUDPSetup) return true;
    xLOG("Connecting to NTP → %s", mCfg.Server.c_str());
    if (mUDP.begin(mCfg.NtpPort) == 0) {
      xLOG("Connecting to NTP server failed");
      mUDPSetup = false;
      return false;
    }
    xLOG("Connecting to NTP server successful");
    mUDPSetup = true;
    return true;
  }

  void NTP_::End() {
    mUDP.stop();
    mUDPSetup = false;
  }

  bool NTP_::IsPacketValid(uint8_t *tPacket) {
    if ((tPacket[0] & 0b11000000) == 0b11000000) return false;
    if (((tPacket[0] & 0b00111000) >> 3) < 3) return false;
    if ((tPacket[0] & 0b00000111) != 4) return false;
    if (tPacket[1] < 1 || tPacket[1] > 15) return false;
    for (uint8_t i = 16; i <= 23; i++) if (tPacket[i] != 0) return true;
    return true;
  }

  bool NTP_::UpdateTime() {
    const unsigned long tSystemEpochUtc = static_cast<unsigned long>(time(nullptr));
    if (tSystemEpochUtc >= 1735689600UL) return true;
    if (mCurrentEpoch == 0 || mLastUpdate == 0) return false;
    return (mCurrentEpoch + ((millis() - mLastUpdate) / 1000UL)) >= 1735689600UL;
  }

  bool NTP_::ForceTimeSync() {
    while (mUDP.parsePacket()) mUDP.flush();
    SendNtpRequest();
    uint8_t tTimeoutCounter = 0;
    int tPacketSize = 0;
    do {
      vTaskDelay(pdMS_TO_TICKS(10));
      tPacketSize = mUDP.parsePacket();
      if (tPacketSize) {
        mUDP.read(mPacketBuffer, kNtpPacketSize);
        if (!IsPacketValid(mPacketBuffer)) tPacketSize = 0;
      }
      if (++tTimeoutCounter > 200) {
        xLOG("NTP timeout, no response");
        mCurrentEpoch = 0;
        return false;
      }
    } while (tPacketSize == 0);
    mLastUpdate = millis() - 10UL * (tTimeoutCounter + 1);
    unsigned long tNtpTime = (mPacketBuffer[40] << 24) | (mPacketBuffer[41] << 16) | (mPacketBuffer[42] <<  8) | mPacketBuffer[43];
    mCurrentEpoch = tNtpTime - mSevenZYYears;
    if (mCurrentEpoch < 1704067200UL) {
      xLOG("Invalid epoch received → %lu", mCurrentEpoch);
      mCurrentEpoch = 0;
      return false;
    }
    ApplyTimeZone();
    struct timeval tTv = { 
      .tv_sec = (time_t)(mCurrentEpoch), 
      .tv_usec = 0 
    };
    if (settimeofday(&tTv, nullptr) != 0) return false;
    mCfg.LastSuccessfulSyncEpochUtc = mCurrentEpoch;
    PersistLastSuccessfulSyncEpoch(mCurrentEpoch);
    return true;
  }

  bool NTP_::ApplyTimeZone() {
    const String tTimeZoneSpec = ResolveTimeZoneSpec();
    if (!tTimeZoneSpec.length()) return false;
    if (setenv("TZ", tTimeZoneSpec.c_str(), 1) != 0) return false;
    tzset();
    return true;
  }

  String NTP_::ResolveTimeZoneSpec() const {
    String tTimeZoneLabel = mCfg.TimeZoneLabel;
    tTimeZoneLabel.trim();
    if (tTimeZoneLabel.length() && tTimeZoneLabel.indexOf('/') >= 0) return tTimeZoneLabel;
    const long tOffsetMinutes = (mCfg.GMTOffset + mCfg.DaylightOffset) / 60L;
    const long tAbsOffsetMinutes = labs(tOffsetMinutes);
    const long tHours = tAbsOffsetMinutes / 60L;
    const long tMinutes = tAbsOffsetMinutes % 60L;
    char tBuffer[24] = "UTC0";
    const char tSign = (tOffsetMinutes >= 0) ? '-' : '+';
    if (tMinutes == 0) snprintf(tBuffer, sizeof(tBuffer), "UTC%c%ld", tSign, tHours);
    else snprintf(tBuffer, sizeof(tBuffer), "UTC%c%ld:%02ld", tSign, tHours, tMinutes);
    return String(tBuffer);
  }

  bool NTP_::ShouldSyncNow(unsigned long tCurrentEpochUtc) const {
    if (!mCfg.LowPowerSyncEnable) return false;
    if (mCfg.LastSuccessfulSyncEpochUtc == 0) return true;
    if (tCurrentEpochUtc < mCfg.LastSuccessfulSyncEpochUtc) return true;
    const unsigned long tElapsedSec = tCurrentEpochUtc - mCfg.LastSuccessfulSyncEpochUtc;
    return tElapsedSec >= mCfg.LowPowerSyncIntervalSec;
  }

  bool NTP_::PersistLastSuccessfulSyncEpoch(unsigned long tEpochUtc) {
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (tConfig.Ntp.LastSuccessfulSyncEpochUtc == tEpochUtc) return true;
    tConfig.Ntp.LastSuccessfulSyncEpochUtc = tEpochUtc;
    const bool tSaved = CFG.SaveAllConfig(tConfig);
    if (!tSaved) xLOG("Failed to persist last sync epoch");
    return tSaved;
  }

  void NTP_::SendNtpRequest() {
    memset(mPacketBuffer, 0, kNtpPacketSize);
    mPacketBuffer[0] = 0b11100011;
    mPacketBuffer[2] = 6;
    mPacketBuffer[3] = 0xEC;
    mPacketBuffer[12] = 0x49;
    mPacketBuffer[13] = 0x4E;
    mPacketBuffer[14] = 0x49;
    mPacketBuffer[15] = 0x52;
    mUDP.beginPacket(mCfg.Server.c_str(), mCfg.NtpPort);
    mUDP.write(mPacketBuffer, kNtpPacketSize);
    mUDP.endPacket();
  }

  unsigned long NTP_::GetCurrentEpoch() {
    return GetCurrentEpochUTC();
  }

  unsigned long NTP_::GetCurrentEpochUTC() {
    Guard tLock;
    const unsigned long tSystemEpochUtc = static_cast<unsigned long>(time(nullptr));
    if (tSystemEpochUtc >= 1735689600UL) return tSystemEpochUtc;
    if (!UpdateTime()) return 0;
    return mCurrentEpoch + ((millis() - mLastUpdate) / 1000UL);
  }

  void NTP_::GetTime(char *tBuffer, uint8_t tLength, char tFormat) {
    unsigned long tEpoch = GetCurrentEpoch();
    if (tLength < 9) { 
      tBuffer[0] = '\0';
      return;
    }
    if (tEpoch == 0) {
      tBuffer[0] = '\0';
      return;
    }
    time_t tTime = static_cast<time_t>(tEpoch);
    struct tm tTimeInfo = {};
    localtime_r(&tTime, &tTimeInfo);
    const int tHours = tTimeInfo.tm_hour;
    const int tMinutes = tTimeInfo.tm_min;
    const int tSeconds = tTimeInfo.tm_sec;
    switch (tFormat) {
      case 'h': 
        FormatTwoDigits(tBuffer, tHours); 
        break;
      case 'm': 
        FormatTwoDigits(tBuffer, tMinutes); 
        break;
      case 'i': 
        FormatTwoDigits(tBuffer, tSeconds); 
        break;
      default:
        FormatTwoDigits(tBuffer, tHours);
        tBuffer[2] = ':';
        FormatTwoDigits(tBuffer + 3, tMinutes);
        tBuffer[5] = ':';
        FormatTwoDigits(tBuffer + 6, tSeconds);
        tBuffer[8] = '\0';
    }
  }

  void NTP_::GetDate(char *tBuffer, uint8_t tLength, char tFormat) {
    unsigned long tEpoch = GetCurrentEpoch();
    if (tLength < 11) { tBuffer[0] = '\0'; return; }
    if (tEpoch == 0) { tBuffer[0] = '\0'; return; }
    time_t tTime = static_cast<time_t>(tEpoch);
    struct tm tTimeInfo = {};
    localtime_r(&tTime, &tTimeInfo);
    const int tYear = tTimeInfo.tm_year + 1900;
    const int tMonth = tTimeInfo.tm_mon + 1;
    const int tDay = tTimeInfo.tm_mday;
    switch (tFormat) {
      case 'y': itoa(tYear, tBuffer, 10); break;
      case 'm': itoa(tMonth, tBuffer, 10); break;
      case 'd': itoa(tDay, tBuffer, 10); break;
      default:
        itoa(tYear, tBuffer, 10);
        tBuffer[4] = '.';
        FormatTwoDigits(tBuffer + 5, tMonth);
        tBuffer[7] = '.';
        FormatTwoDigits(tBuffer + 8, tDay);
        tBuffer[10] = '\0';
    }
  }

  void NTP_::FormatTwoDigits(char *tBuffer, int tValue) {
    if (tValue < 0) tValue = 0;
    if (tValue > 99) tValue = 99;
    tBuffer[0] = '0' + tValue / 10;
    tBuffer[1] = '0' + tValue % 10;
  }

  bool NTP_::IsLeapYear(unsigned long tYear) {
    return (tYear % 4 == 0) && (tYear % 100 != 0 || tYear % 400 == 0);
  }

  bool NTP_::IsDST(unsigned long tEpoch) {
    struct tm timeinfo;
    localtime_r((time_t*)&tEpoch, &timeinfo);
    int tYear = timeinfo.tm_year + 1900;
    int tMonth = timeinfo.tm_mon + 1;
    int tDay = timeinfo.tm_mday;
    int tHour = timeinfo.tm_hour;
    int tDow = timeinfo.tm_wday;
    if (tMonth < 3 || tMonth > 10) return false;
    if (tMonth > 3 && tMonth < 10) return true;
    if (tMonth == 3) {
      int tLastSun = 31 - ((tDow + 6) % 7 + 7) % 7;
      if (tDay > tLastSun) return true;
      if (tDay < tLastSun) return false;
      return (tHour >= 3);
    }
  if (tMonth == 10) {
      int tLastSun = 31 - ((tDow + 6) % 7 + 7) % 7;
      if (tDay > tLastSun) return false;
      if (tDay < tLastSun) return true;
      return (tHour < 3);
    }
    return false;
  }

  bool NTP_::SyncSystemTime() {
    if (!mUDPSetup) {
      if (!Begin()) {
        xLOG("System time failed synchronized from NTP");
        return false;
      }
    }
    bool tSuccess = ForceTimeSync();
    if (tSuccess) {
      xLOG("System time synchronized from NTP");
      char tDate[32];
      GetDate(tDate, sizeof(tDate));
      xLOG("Current date: %s", tDate);
      char tTime[9];
      GetTime(tTime, sizeof(tTime));
      xLOG("Current time: %s", tTime);
    } else xLOG("System time failed synchronized from NTP");
    return tSuccess;
  }

  bool NTP_::SyncSystemTimeIfNeeded() {
    unsigned long tCurrentEpochUtc = static_cast<unsigned long>(time(nullptr));
    if (tCurrentEpochUtc < 1735689600UL) tCurrentEpochUtc = static_cast<unsigned long>(RTC.GetEpoch());
    if (!ShouldSyncNow(tCurrentEpochUtc)) {
      xLOG("NTP auto sync skipped, policy not due");
      return true;
    }
    return SyncSystemTime();
  }

  int8_t NTP_::GetGMTOffset() {
    const unsigned long tEpochUtc = GetCurrentEpochUTC();
    if (tEpochUtc >= 1735689600UL) return static_cast<int8_t>(ResolveUtcOffsetMinutesFromEpoch(static_cast<time_t>(tEpochUtc)) / 60);
    const long tOffset = mCfg.GMTOffset + mCfg.DaylightOffset;
    return static_cast<int8_t>(tOffset / SECONDS_PER_HOUR);
  }

  const char *NTP_::GetTimezoneName() {
    return mCfg.TimeZoneLabel.length() ? mCfg.TimeZoneLabel.c_str() : "UTC";
  }

  void NTP_::PrintDateTimeInfo() {
    char tText[UTL.GetPrintInfoWidth() - 4] = "";
    xLOG_PL();
    UTL.PrintInfo("NTP", EUtilsInfoType::Header);
    UTL.PrintInfo("", EUtilsInfoType::Line);
    snprintf(tText, sizeof(tText), "NTP Server: %s", mCfg.Server.c_str());
    UTL.PrintInfo(tText);
    snprintf(tText, sizeof(tText), "NTP Port: %d", mCfg.NtpPort);
    UTL.PrintInfo(tText);
    UTL.PrintInfo("", EUtilsInfoType::Line);
    int8_t tGmt = GetGMTOffset();
    const char *tZone = GetTimezoneName();
    snprintf(tText, sizeof(tText), "NTP time zone: GMT%+d (%s)", tGmt, tZone);
    UTL.PrintInfo(tText);
    UTL.PrintInfo("", EUtilsInfoType::Footer);
  }

}
