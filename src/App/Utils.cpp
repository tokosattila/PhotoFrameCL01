#include <App/Utils.h>

namespace App {

  Utils_ &Utils_::Instance() {
    static Utils_ tInstance;
    return tInstance;
  }

  Utils_::Utils_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Utils_::~Utils_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Utils_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Utils_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool Utils_::SecureStrcmp(const char *tA, const char *tB) {
    if (!tA || !tB) return false;
    size_t tLenA = strlen(tA);
    size_t tLenB = strlen(tB);
    volatile uint8_t tDiff = static_cast<uint8_t>(tLenA ^ tLenB);
    size_t tMinLen = (tLenA < tLenB) ? tLenA : tLenB;
    for (size_t i = 0; i < tMinLen; i++) {
      tDiff |= static_cast<uint8_t>(tA[i] ^ tB[i]);
    }
    return tDiff == 0;
  }

  const char *Utils_::GetLeafName(const char *tPath) {
    if (!tPath) return "";
    const char *tSlash = strrchr(tPath, '/');
    return tSlash ? (tSlash + 1) : tPath;
  }

  bool Utils_::EqualsIgnoreCase(const char *tLeft, const char *tRight) {
    const char *tL = tLeft ? tLeft : "";
    const char *tR = tRight ? tRight : "";
    while (*tL && *tR) {
      const char tLc = static_cast<char>(tolower(static_cast<unsigned char>(*tL)));
      const char tRc = static_cast<char>(tolower(static_cast<unsigned char>(*tR)));
      if (tLc != tRc) return false;
      ++tL;
      ++tR;
    }
    return *tL == '\0' && *tR == '\0';
  }

  EDisplayRotate Utils_::ResolveDisplayRotate(uint16_t tRotate) {
    switch (tRotate) {
      case 0:
        return EDisplayRotate::Rotate0;
      case 90:
        return EDisplayRotate::Rotate90;
      case 180:
        return EDisplayRotate::Rotate180;
      case 270:
        return EDisplayRotate::Rotate270;
      default:
        return static_cast<EDisplayRotate>(DISPLAY_ROTATE);
    }
  }

  uint32_t Utils_::SafeAtoul(const char *tStr, uint32_t tMinVal, uint32_t tMaxVal, uint32_t tDefaultVal) {
    if (!tStr || *tStr == '\0' || *tStr == ' ' || *tStr == '\t') return tDefaultVal;
    char *tEndPtr = nullptr;
    errno = 0;
    unsigned long tVal = strtoul(tStr, &tEndPtr, 10);
    if (errno == ERANGE || tEndPtr == tStr || *tEndPtr != '\0') return tDefaultVal;
    if (tVal < tMinVal || tVal > tMaxVal) return tDefaultVal;
    return static_cast<uint32_t>(tVal);
  }

  bool Utils_::HasElapsedMs(uint32_t tStartMs, uint32_t tNowMs, uint32_t tDelayMs) {
    return static_cast<uint32_t>(tNowMs - tStartMs) >= tDelayMs;
  }

  void Utils_::Init() {
    ReloadConfig();
  }

  void Utils_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
  }

  void Utils_::SetCPUFrequency(ECPUFrequency tFrequency) {
    Guard tLock;
    setCpuFrequencyMhz(static_cast<uint32_t>(tFrequency));
  }
  
  void Utils_::DisableBT() {
    Guard tLock;
    esp_bt_controller_status_t tStatus = esp_bt_controller_get_status();
    if (tStatus != ESP_BT_CONTROLLER_STATUS_IDLE) {
      if (tStatus == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        if (esp_bt_controller_disable() != ESP_OK) {
          while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) vTaskDelay(1);
        }
      }
      if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) esp_bt_controller_deinit();
      esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
    }
    xLOG("Bluetooth disabled");
  }

  void Utils_::DisableBrownout() {
    Guard tLock;
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    xLOG("Brownout detector disabled");
  }

  void Utils_::ByteToReadableSize(uint64_t tBytes, char *tBuffer, size_t tLength) {
    if (tBytes < 1024ULL) {
      snprintf(tBuffer, tLength, "%llu B", static_cast<unsigned long long>(tBytes));
    } else if (tBytes < 1024ULL * 1024ULL) {
      float tSizeKB = static_cast<float>(tBytes) / 1024.0f;
      if (fabs(tSizeKB - static_cast<int>(tSizeKB)) < 0.01f) snprintf(tBuffer, tLength, "%d KB", static_cast<int>(tSizeKB));
      else snprintf(tBuffer, tLength, "%.2f KB", tSizeKB);
    } else if (tBytes < 1024ULL * 1024ULL * 1024ULL) {
      float tSizeMB = static_cast<float>(tBytes) / (1024.0f * 1024.0f);
      if (fabs(tSizeMB - static_cast<int>(tSizeMB)) < 0.01f) snprintf(tBuffer, tLength, "%d MB", static_cast<int>(tSizeMB));
      else snprintf(tBuffer, tLength, "%.2f MB", tSizeMB);
    } else {
      float tSizeGB = static_cast<float>(tBytes) / (1024.0f * 1024.0f * 1024.0f);
      if (fabs(tSizeGB - static_cast<int>(tSizeGB)) < 0.01f) snprintf(tBuffer, tLength, "%d GB", static_cast<int>(tSizeGB));
      else snprintf(tBuffer, tLength, "%.2f GB", tSizeGB);
    }
  }

  const char *Utils_::EpochToReadableFormat(unsigned long tEpoch, bool tAsDateTime, char *tBuffer, size_t tLength) {
    if (!tBuffer || tLength == 0) return "";
    tBuffer[0] = '\0';
    if (tEpoch == 0) {
      strcpy(tBuffer, "0");
      return tBuffer;
    }
    if (tAsDateTime) {
      time_t tTime = static_cast<time_t>(tEpoch);
      struct tm tTm = {};
      localtime_r(&tTime, &tTm);
      snprintf(tBuffer, tLength, "%04d.%02d.%02d %02d:%02d:%02d", tTm.tm_year + 1900, tTm.tm_mon + 1, tTm.tm_mday, tTm.tm_hour, tTm.tm_min, tTm.tm_sec);
      return tBuffer;
    }
    if (tEpoch < SECONDS_PER_MINUTE) snprintf(tBuffer, tLength, "%lu sec", tEpoch);
    else if (tEpoch < SECONDS_PER_HOUR) snprintf(tBuffer, tLength, "%02lu:%02lu min", tEpoch / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
    else if (tEpoch < SECONDS_PER_DAY) snprintf(tBuffer, tLength, "%lu:%02lu:%02lu hour(s)", tEpoch / SECONDS_PER_HOUR, (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
    else snprintf(tBuffer, tLength, "%lu day(s) %02lu:%02lu:%02lu", tEpoch / SECONDS_PER_DAY, (tEpoch % SECONDS_PER_DAY) / SECONDS_PER_HOUR, (tEpoch % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE, tEpoch % SECONDS_PER_MINUTE);
    return tBuffer;
  }
 
  void Utils_::PrintPartitionInfo() {
    char tText[mPrintInfoWidth - 4] = "";
    const esp_partition_t *tRunning = esp_ota_get_running_partition();
    const esp_partition_t *tBoot = esp_ota_get_boot_partition();
    PrintInfo("PARTITION INFO", EUtilsInfoType::Header);
    PrintInfo("", EUtilsInfoType::Line);
    if (tRunning) {
      snprintf(tText, sizeof(tText), "Running partition: %s @ 0x%08x", tRunning->label, (unsigned)tRunning->address);
      PrintInfo(tText);
    }
    if (tBoot) {
      snprintf(tText, sizeof(tText), "Boot partition: %s @ 0x%08x", tBoot->label, (unsigned)tBoot->address);
      PrintInfo(tText);
    }
    PrintInfo("", EUtilsInfoType::Footer);
  }

  void Utils_::PrintBootInfo() {
    const uint32_t tBootCount = CFG.GetBootCount();
    if (tBootCount <= 1) xLOG("First boot [%lu]\n", (unsigned long)tBootCount);
    else {
      xLOG("Boot no. → %lu", (unsigned long)tBootCount);
      PrintWakeupReason();
      xLOG_PL();
    }
  }

  void Utils_::PrintDeviceInfo() {
    Guard tLock;
    char tText[mPrintInfoWidth - 4] = "";
    PrintBootInfo();
    snprintf(tText, sizeof(tText), "%s %s", mCfg.Device.Name.c_str(), mCfg.Device.Version.c_str());
    PrintInfo(tText, EUtilsInfoType::Header);
    PrintChipInfo();
    PrintRamInfo();
    PrintFlashInfo();
    PrintResourceInfo();
    PrintRadioInfo();
    PrintInfo("", EUtilsInfoType::Footer);
  }

  void Utils_::PrintInfo(const char *tText, EUtilsInfoType tType, uint8_t tWidth) {
    if (tWidth == 0) tWidth = mPrintInfoWidth;
    tWidth -= 2;
    if (tType == EUtilsInfoType::Header || tType == EUtilsInfoType::Single) {
      if (tType == EUtilsInfoType::Single) xLOG_PR_("\n┌─");
      else xLOG_PR_("┌─");
      for (uint8_t i = 0; i < tWidth; i++) xLOG_PR_("─");
      xLOG_PL_("─┐");
    } else 
    if (tType == EUtilsInfoType::Title) {
      xLOG_PR_("├─");
      for (uint8_t i = 0; i < tWidth; i++) xLOG_PR_("─");
      xLOG_PL_("─┤");
    }
    if (tType == EUtilsInfoType::Line) {
      xLOG_PR_("├─");
      for (uint8_t i = 0; i < tWidth; i++) xLOG_PR_("─");
      xLOG_PL_("─┤");
    }
    uint8_t tTextLen = 0;
    for (const char *tPtr = tText; *tPtr; ++tPtr) {
      if ((*tPtr & 0xC0) != 0x80) ++tTextLen;
    }
    if (tTextLen > 0) {
      xLOG_PR_("│ ");
      uint8_t tRightPadding = (tTextLen < tWidth) ? (tWidth - tTextLen) : 0;
      if (tType == EUtilsInfoType::Header || tType == EUtilsInfoType::Title || tType == EUtilsInfoType::Single) {
        xLOG_PR(tText);
        if (tRightPadding > 1) {
          xLOG_PR(" ");
          tRightPadding -= 1;
        }
        if (tRightPadding != 0) {
          for (uint8_t i = 0; i < tRightPadding; i++) xLOG_PR_("░");
        }
      } else {
        xLOG_PR(tText);
        if (tRightPadding != 0) {
          for (uint8_t i = 0; i < tRightPadding; i++) xLOG_PR(" ");
        }
      }
      xLOG_PL_(" │");
    }
    if (tType == EUtilsInfoType::Title) {
      xLOG_PR_("├─");
      for (uint8_t i = 0; i < tWidth; i++) xLOG_PR_("─");
      xLOG_PL_("─┤");
    } else if (tType == EUtilsInfoType::Footer || tType == EUtilsInfoType::Single) {
      xLOG_PR_("└─");
      for (uint8_t i = 0; i < tWidth; i++) xLOG_PR_("─");
      xLOG_PL_("─┘\n");
    }
  }

  void Utils_::PrintChipInfo() {
    char tText[mPrintInfoWidth - 4] = "";
    esp_chip_info_t tChip;
    esp_chip_info(&tChip);
    PrintInfo("CHIP INFO", EUtilsInfoType::Title);
    snprintf(tText, sizeof(tText), "Type: %s, rev. %d.%d", ESP.getChipModel(), tChip.revision / 100, tChip.revision % 100);
    PrintInfo(tText);
    snprintf(tText, sizeof(tText), "Cores: %d", tChip.cores);
    PrintInfo(tText);
    snprintf(tText, sizeof(tText), "Frequency: %d MHz", ESP.getCpuFreqMHz());
    PrintInfo(tText);
    snprintf(tText, sizeof(tText), "Temperature: %.1f °C", temperatureRead());
    PrintInfo(tText, EUtilsInfoType::Cell, mPrintInfoWidth);
  }

  void Utils_::PrintFlashInfo() {
    char tText[19] = "";
    char tFlasChipSize[16];
    PrintInfo("FLASH INFO",EUtilsInfoType::Title);
    snprintf(tText, sizeof(tText), "Frequency: %d MHz", ESP.getFlashChipSpeed() / 1000000);
    PrintInfo(tText);
    ByteToReadableSize(ESP.getFlashChipSize(), tFlasChipSize, sizeof(tFlasChipSize));
    snprintf(tText, sizeof(tText), "Size: %s", tFlasChipSize);
    PrintInfo(tText);    
  }

  void Utils_::PrintNvsUsageInfo() {
    nvs_stats_t tStats{};
    if (nvs_get_stats(nullptr, &tStats) != ESP_OK) {
      PrintInfo("NVS: NVS failed");
      return;
    }
    size_t tUsedCfg = 0;
    nvs_iterator_t tIt = nvs_entry_find("nvs", "cfg", NVS_TYPE_ANY);
    while (tIt != nullptr) {
      ++tUsedCfg;
      tIt = nvs_entry_next(tIt);
    }
    nvs_release_iterator(tIt);
    char tText[128] = "";
    snprintf(tText, sizeof(tText), "NVS: %zu / %zu entries", tUsedCfg, tStats.total_entries);
    PrintInfo(tText);
  }

  void Utils_::PrintRamInfo() {
    char tText[17] = "";
    uint32_t tTotalDram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t tTotalIram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_EXEC);
    uint32_t tTotalRam = tTotalDram + tTotalIram;
    char tTotalDramBuffer[17] = "";
    char tTotalIramBuffer[17] = "";
    char tTotalRamBuffer[17] = "";   
    PrintInfo("RAM INFO", EUtilsInfoType::Title);
    ByteToReadableSize(tTotalDram, tTotalDramBuffer, sizeof(tTotalDramBuffer));
    snprintf(tText, sizeof(tText), "DRAM: %s", tTotalDramBuffer);
    PrintInfo(tText);
    ByteToReadableSize(tTotalIram, tTotalIramBuffer, sizeof(tTotalIramBuffer));
    snprintf(tText, sizeof(tText), "IRAM: %s", tTotalIramBuffer);
    PrintInfo(tText);
    ByteToReadableSize(tTotalRam, tTotalRamBuffer, sizeof(tTotalRamBuffer));
    snprintf(tText, sizeof(tText), "Total: %s", tTotalRamBuffer);
    PrintInfo(tText);
  }

  void Utils_::PrintDRamUsageInfo() {
    char tText[28] = "";
    uint32_t tTotalDram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t tFreeDram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint32_t tUsedDram = tTotalDram - tFreeDram;
    char tUsedDramBuffer[16];
    char tTotalDramBuffer[16];
    ByteToReadableSize(tUsedDram, tUsedDramBuffer, sizeof(tUsedDramBuffer));
    ByteToReadableSize(tTotalDram, tTotalDramBuffer, sizeof(tTotalDramBuffer));
    snprintf(tText, sizeof(tText), "DRAM: %s / %s", tUsedDramBuffer, tTotalDramBuffer);
    PrintInfo(tText);
  }

  void Utils_::PrintIRamUsageInfo() {
    char tText[28] = "";
    uint32_t tTotalIram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_EXEC);
    uint32_t tFreeIram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_EXEC);
    uint32_t tUsedIram = tTotalIram - tFreeIram;
    char tUsedIramBuffer[16];
    char tTotalIramBuffer[16];
    ByteToReadableSize(tUsedIram, tUsedIramBuffer, sizeof(tUsedIramBuffer));
    ByteToReadableSize(tTotalIram, tTotalIramBuffer, sizeof(tTotalIramBuffer));
    snprintf(tText, sizeof(tText), "IRAM: %s / %s", tUsedIramBuffer, tTotalIramBuffer);
    PrintInfo(tText);
  }

  void Utils_::PrintPSRamUsageInfo() {
    char tText[27] = "";
    uint32_t tTotalPsram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t tFreePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t tUsedPsram = tTotalPsram - tFreePsram;
    char tUsedPsramBuffer[16];
    char tTotalPsramBuffer[16];
    ByteToReadableSize(tUsedPsram, tUsedPsramBuffer, sizeof(tUsedPsramBuffer));
    ByteToReadableSize(tTotalPsram, tTotalPsramBuffer, sizeof(tTotalPsramBuffer));
    snprintf(tText, sizeof(tText), "PSRAM: %s / %s", tUsedPsramBuffer, tTotalPsramBuffer);
    PrintInfo(tText);
  }

  void Utils_::PrintMemoryInfo() {
    Guard tLock;
    char tText[mPrintInfoWidth - 4] = "";
    char tFreeHeap[16] = "";
    xLOG_PL();
    PrintInfo("MEMORY INFO", EUtilsInfoType::Header);
    PrintInfo("", EUtilsInfoType::Line);
    ByteToReadableSize(ESP.getFreeHeap(), tFreeHeap, sizeof(tFreeHeap));
    snprintf(tText, sizeof(tText), "Free heap: %s", tFreeHeap);
    PrintInfo(tText);
    PrintInfo("", EUtilsInfoType::Footer);
  }

  void Utils_::PrintSketchInfo() {
    char tText[30] = "";
    uint32_t tSketchSize = ESP.getSketchSize();
    const esp_partition_t *tRunning = esp_ota_get_running_partition();
    if (!tRunning) {
      PrintInfo("Sketch: unknown");
      return;
    }
    char tSketchSizeBuffer[16] = "";
    char tSketchTotalSizeBuffer[16] = "";
    ByteToReadableSize(tSketchSize, tSketchSizeBuffer, sizeof(tSketchSizeBuffer));
    ByteToReadableSize(tRunning->size, tSketchTotalSizeBuffer, sizeof(tSketchTotalSizeBuffer));
    snprintf(tText, sizeof(tText), "Sketch: %s / %s", tSketchSizeBuffer, tSketchTotalSizeBuffer);
    PrintInfo(tText);
  }

  void Utils_::PrintFileSystemInfo() {
    char tText[48] = "";
    char tUsedBuffer[16] = "";
    char tTotalBuffer[16] = "";
    if (LFS.Init(false)) {
      ByteToReadableSize(LFS.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
      ByteToReadableSize(LFS.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
      snprintf(tText, sizeof(tText), "LittleFS: %s / %s", tUsedBuffer, tTotalBuffer);
      PrintInfo(tText);
      LFS.End();
    }
    if (SDC.Init(false)) {
      ByteToReadableSize(SDC.UsedBytes(), tUsedBuffer, sizeof(tUsedBuffer));
      ByteToReadableSize(SDC.TotalBytes(), tTotalBuffer, sizeof(tTotalBuffer));
      snprintf(tText, sizeof(tText), "SDCard (%s): %s / %s", SDC.CardTypeName(), tUsedBuffer, tTotalBuffer);
      PrintInfo(tText);
      SDC.End();
    }
  }

  void Utils_::PrintResourceInfo() {
    PrintInfo("RESOURCE INFO", EUtilsInfoType::Title);
    PrintDRamUsageInfo();
    PrintIRamUsageInfo();
    PrintPSRamUsageInfo();
    PrintInfo("", EUtilsInfoType::Line);
    PrintNvsUsageInfo();
    PrintSketchInfo();
    PrintFileSystemInfo();
  }

  void Utils_::PrintRadioInfo() {
    char tText[16] = "";
    esp_chip_info_t tChip;
    esp_chip_info(&tChip);
    PrintInfo("COM INFO", EUtilsInfoType::Title);
    snprintf(tText, sizeof(tText), "WIFI: %s", (tChip.features & CHIP_FEATURE_WIFI_BGN) ? "yes" : "no");
    PrintInfo(tText);
    snprintf(tText, sizeof(tText), "BT: %s", (tChip.features & CHIP_FEATURE_BT) ? "yes" : "no");
    PrintInfo(tText);
  }

  const char *Utils_::PrependSlash(const char *tPath, char *tOutBuffer, size_t tBufSize) {
    if (!tPath || !tOutBuffer || tBufSize < 2) return tPath;
    size_t tLen = 0;
    while (tPath[tLen] && tLen < tBufSize-2) tLen++;
    if (tLen > 0 && tPath[0] != '/' && tPath[0] != '\\') {
      tOutBuffer[0] = '/';
      memcpy(tOutBuffer + 1, tPath, tLen);
      tOutBuffer[tLen + 1] = 0;
    } else {
      strncpy(tOutBuffer, tPath, tBufSize-1);
      tOutBuffer[tBufSize-1] = 0;
    }
    return tOutBuffer; 
  }

  void Utils_::PrintWakeupReason() {
    static const char *tReasons[] = { "RESTART", "ALL", "EXT0 (RTC_IO)", "EXT1 (RTC_CNTL)", "TIMER", "TOUCHPAD", "ULP", "GPIO", "UART", "WIFI", "COCPU", "COCPU_TRAP_TRIG", "BT" };
    esp_sleep_wakeup_cause_t tCause = esp_sleep_get_wakeup_cause();
    const char *tReasonStr = "UNKNOWN";
    if (tCause < sizeof(tReasons) / sizeof(tReasons[0])) tReasonStr = tReasons[tCause];
    xLOG("Wake-up system → %s [%d]", tReasonStr, (unsigned long)tCause);
  }

  const char *Utils_::ResolveBootReason() {
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON: return "POWER_ON";
      case ESP_RST_EXT: return "EXT_PIN";
      case ESP_RST_SW: return "SOFTWARE";
      case ESP_RST_PANIC: return "PANIC";
      case ESP_RST_INT_WDT: return "INT_WDT";
      case ESP_RST_TASK_WDT: return "TASK_WDT";
      case ESP_RST_WDT: return "WDT";
      case ESP_RST_DEEPSLEEP: {
        switch (esp_sleep_get_wakeup_cause()) {
          case ESP_SLEEP_WAKEUP_TIMER: return "TIMER_WAKEUP";
          case ESP_SLEEP_WAKEUP_EXT0: return "EXT0_WAKEUP";
          case ESP_SLEEP_WAKEUP_EXT1: return "EXT1_WAKEUP";
          case ESP_SLEEP_WAKEUP_GPIO: return "GPIO_WAKEUP";
          case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCH_WAKEUP";
          case ESP_SLEEP_WAKEUP_ULP: return "ULP_WAKEUP";
          default: return "DEEPSLEEP_WAKEUP";
        }
      }
      case ESP_RST_BROWNOUT: return "BROWNOUT";
      case ESP_RST_SDIO: return "SDIO";
      default: return "UNKNOWN";
    }
  }

  bool Utils_::WasWokenByPin(uint8_t tPin) {
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return false;
    const uint64_t tStatus = esp_sleep_get_ext1_wakeup_status();
    return (tStatus & (1ULL << tPin)) != 0;
  }

  uint64_t Utils_::SecondsUntilHour(uint8_t tTargetHour) {
    time_t tEpochUtc = static_cast<time_t>(time(nullptr));
    if (static_cast<unsigned long>(tEpochUtc) < 1735689600UL) tEpochUtc = static_cast<time_t>(RTC.GetEpoch());
    if (static_cast<unsigned long>(tEpochUtc) < 1735689600UL) return SECONDS_PER_DAY;
    struct tm tTargetTime = {};
    localtime_r(&tEpochUtc, &tTargetTime);
    tTargetTime.tm_hour = tTargetHour % 24;
    tTargetTime.tm_min = 0;
    tTargetTime.tm_sec = 0;
    time_t tTargetEpochUtc = mktime(&tTargetTime);
    if (tTargetEpochUtc <= tEpochUtc) {
      tTargetTime.tm_mday += 1;
      tTargetEpochUtc = mktime(&tTargetTime);
    }
    if (tTargetEpochUtc <= tEpochUtc) return SECONDS_PER_DAY;
    return static_cast<uint64_t>(tTargetEpochUtc - tEpochUtc);
  }

  void Utils_::SleepAndWakeup() {
    constexpr uint64_t tSecToUs = 1000000ULL;
    uint64_t tDelaySec = 0;
    uint8_t tHour = mCfg.Timer.WakeUpHour % 24;
    switch (mCfg.Timer.WakeUp) {
      case ETimerWakeUp::Seconds:
        tDelaySec = 10;
        break;
      case ETimerWakeUp::Minutes:
        tDelaySec = SECONDS_PER_MINUTE;
        break;
      case ETimerWakeUp::Hourly:
        tDelaySec = SECONDS_PER_HOUR;
        break;
      case ETimerWakeUp::HalfDay:
        tDelaySec = 12 * SECONDS_PER_HOUR;
        break;
      case ETimerWakeUp::Daily:
        tDelaySec = SecondsUntilHour(tHour);
        break;
      case ETimerWakeUp::Weekly:
        tDelaySec = SecondsUntilHour(tHour) + 6 * SECONDS_PER_DAY;
        break;
      case ETimerWakeUp::Monthly:
        tDelaySec = SecondsUntilHour(tHour) + 29 * SECONDS_PER_DAY;
        break;
      default:
        tDelaySec = SecondsUntilHour(tHour);
        break;
    }
    const char *tUnit = "sec";
    uint64_t tDisplay = tDelaySec;
    if (tDisplay >= 7 * SECONDS_PER_DAY) { 
      tDisplay /= SECONDS_PER_DAY; 
      tUnit = "day";   
    } else 
    if (tDisplay >= SECONDS_PER_DAY) { 
      tDisplay /= SECONDS_PER_DAY; 
      tUnit = "day"; 
    } else 
    if (tDisplay >= SECONDS_PER_HOUR) { 
      tDisplay /= SECONDS_PER_HOUR;  
      tUnit = "hour"; 
    } else 
    if (tDisplay >= SECONDS_PER_MINUTE) { 
      tDisplay /= SECONDS_PER_MINUTE;
      tUnit = "min";
    }
    xLOG("Going to deep sleep...");
    xLOG("Wake-up → hour%02u:00", tHour);
    xLOG("Next wake-up → %llu %s\n\n", tDisplay, tUnit);
    const uint8_t tWakePin = static_cast<uint8_t>(mCfg.Device.SettingPin);
    const uint8_t tNextImgPin = static_cast<uint8_t>(mCfg.Device.NextImgPin);
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(tNextImgPin));
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(tNextImgPin));
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(tNextImgPin), 0);
    const uint64_t tWakeMask = (1ULL << tWakePin);
    rtc_gpio_pullup_dis(static_cast<gpio_num_t>(tWakePin));
    rtc_gpio_pulldown_en(static_cast<gpio_num_t>(tWakePin));
    esp_sleep_enable_ext1_wakeup(tWakeMask, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup(tDelaySec * tSecToUs);
    esp_deep_sleep_start();
  }  

  void Utils_::SleepLowBattery() {
    xLOG("Low battery entering deep sleep..\n\n");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_deep_sleep_start();
  }

}
