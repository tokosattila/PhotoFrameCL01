#include <App/NTP.h>

namespace App {

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
    Begin();
  }

  void NTP_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SNTPConfig>(); 
  }

  bool NTP_::IsAvailable() {
    wifi_mode_t tMode = WiFi.getMode();
    if (tMode == WIFI_AP || tMode == WIFI_OFF) {
      xLOG("NTP → not available, WiFi in AP mode or OFF");
      return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
      xLOG("NTP → not available, WiFi not connected");
      return false;
    }
    return true;
  }

  bool NTP_::Begin() {
    if (!IsAvailable()) {
      mCurrentEpoch = 0;
      return false;
    }
    xLOG("Connecting to NTP → %s", mCfg.Server.c_str());
    if (mUDP.begin(mCfg.NtpPort) == 0) {
      xLOG("Connecting to NTP server failed");
      mUDPSetup = false;
      return false;
    }
    xLOG("Connecting to NTP server successful");
    mUDPSetup = true;
    return ForceTimeSync();
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
    if (!IsAvailable()) return false;
    unsigned long tCurrentMillis = millis();
    if (tCurrentMillis - mLastUpdate >= mCfg.UpdateInterval || mLastUpdate == 0) {
      if (!mUDPSetup && !Begin()) return false;
      return ForceTimeSync();
    }
    return true;
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
        xLOG("NTP → timeout, no response");
        mCurrentEpoch = 0;
        return false;
      }
    } while (tPacketSize == 0);
    mLastUpdate = millis() - 10UL * (tTimeoutCounter + 1);
    unsigned long tNtpTime = (mPacketBuffer[40] << 24) | (mPacketBuffer[41] << 16) | (mPacketBuffer[42] <<  8) | mPacketBuffer[43];
    mCurrentEpoch = tNtpTime - mSevenZYYears;
    if (mCurrentEpoch < 1704067200UL) {
      xLOG("NTP → invalid epoch received: %lu", mCurrentEpoch);
      mCurrentEpoch = 0;
      return false;
    }
    setenv("TZ", "GMT", 1); 
    tzset();
    struct timeval tTv = { 
      .tv_sec = (time_t)(mCurrentEpoch), 
      .tv_usec = 0 
    };
    if (settimeofday(&tTv, nullptr) != 0) return false;
    return true;
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
    Guard tLock;
    unsigned long tBase = GetCurrentEpochUTC();
    if (tBase == 0) return 0;
    unsigned long tOffset = mCfg.GMTOffset;
    bool tDst = IsDST(tBase + tOffset);
    if (tDst) tOffset += SECONDS_PER_HOUR;
    return tBase + tOffset;
  }

  unsigned long NTP_::GetCurrentEpochUTC() {
    Guard tLock;
    if (!UpdateTime()) return 0;
    if (mCurrentEpoch == 0) return 0;
    return mCurrentEpoch + ((millis() - mLastUpdate) / 1000UL);
  }

  unsigned long NTP_::EpochTime() {
    return GetCurrentEpoch();
  }

  void NTP_::GetTime(char *tBuffer, uint8_t tLength, char tFormat) {
    unsigned long tEpoch = GetCurrentEpoch();
    int tHours   = (tEpoch % SECONDS_PER_DAY) / SECONDS_PER_HOUR;
    int tMinutes = (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
    int tSeconds = tEpoch % SECONDS_PER_MINUTE;
    if (tLength < 9) { 
      tBuffer[0] = '\0';
      return;
    }
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
    UpdateTime();
    unsigned long tEpoch = GetCurrentEpoch();
    unsigned long tDays = tEpoch / SECONDS_PER_DAY;
    unsigned long tYear = 1970;
    while (tDays >= (IsLeapYear(tYear) ? 366UL : 365UL)) {
      tDays -= IsLeapYear(tYear) ? 366UL : 365UL;
      tYear++;
    }
    uint8_t tMonth = 0;
    static const uint8_t kDaysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    while (tDays >= (tMonth == 1 && IsLeapYear(tYear) ? 29U : kDaysInMonth[tMonth])) {
      tDays -= (tMonth == 1 && IsLeapYear(tYear) ? 29U : kDaysInMonth[tMonth]);
      tMonth++;
    }
    unsigned long tDay = tDays + 1;
    if (tLength < 11) { tBuffer[0] = '\0'; return; }
    switch (tFormat) {
      case 'y': itoa(tYear, tBuffer, 10); break;
      case 'm': itoa(tMonth + 1, tBuffer, 10); break;
      case 'd': itoa(tDay, tBuffer, 10); break;
      default:
        itoa(tYear, tBuffer, 10);
        tBuffer[4] = '.';
        FormatTwoDigits(tBuffer + 5, tMonth + 1);
        tBuffer[7] = '.';
        FormatTwoDigits(tBuffer + 8, tDay);
        tBuffer[10] = '\0';
    }
  }

  String NTP_::Time(char tFormat) {
    char tTimeBuffer[9];
    GetTime(tTimeBuffer, sizeof(tTimeBuffer), tFormat);
    return String(tTimeBuffer); 
  }

  String NTP_::Date(char tFormat) {
    char tDateBuffer[11];
    GetDate(tDateBuffer, sizeof(tDateBuffer), tFormat);
    return String(tDateBuffer); 
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
        xLOG("NTP → system time failed synchronized");
        return false;
      }
    }
    bool tSuccess = ForceTimeSync();
    if (tSuccess) {
      xLOG("NTP → system time synchronized");
      char tDate[32];
      GetDate(tDate, sizeof(tDate));
      xLOG("NTP → current date: %s", tDate);
      char tTime[9];
      GetTime(tTime, sizeof(tTime));
      xLOG("NTP → current time: %s", tTime);
    } else xLOG("NTP → system time failed synchronized");
    return tSuccess;
  }

  int8_t NTP_::GetGMTOffset() {
    UpdateTime();
    struct tm tLocal;
    localtime_r((time_t*)&mCurrentEpoch, &tLocal);
    #ifdef __TM_GMTOFF
      if (tLocal.__TM_GMTOFF != 0) {
        return (int8_t)(tLocal.__TM_GMTOFF / SECONDS_PER_HOUR);
      }
    #endif
    unsigned long tOffset = mCfg.GMTOffset;
    bool tIsDst = IsDST(mCurrentEpoch + mCfg.GMTOffset);
    if (tIsDst) tOffset += SECONDS_PER_HOUR;
    return (int8_t)(tOffset / SECONDS_PER_HOUR);
  }

  const char *NTP_::GetTimezoneName() {
    struct tm tLocal;
    localtime_r((time_t*)&mCurrentEpoch, &tLocal);
    #ifdef __TM_ZONE
      if (tLocal.__TM_ZONE) return tLocal.__TM_ZONE;
    #endif
    bool tIsDst = IsDST(mCurrentEpoch + mCfg.GMTOffset);
    return tIsDst ? "EEST" : "EET";
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
