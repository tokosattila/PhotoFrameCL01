#include <App/Configuration.h>

namespace App {

  Configuration_ &Configuration_::Instance() {
    static Configuration_ tInstance;
    return tInstance;
  }

  Configuration_::Configuration_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Configuration_::~Configuration_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Configuration_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Configuration_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool Configuration_::Begin(bool tReadOnly) {
    const uint8_t kMaxRetry = 3;
    for (uint8_t tIndex = 0; tIndex < kMaxRetry; tIndex++) {
      if (mConfig.begin(mLabel, tReadOnly, mPartLabel)) return true;
      vTaskDelay(NVS_RETRY_DELAY_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  void Configuration_::End() {
    mConfig.end();
  }

  void Configuration_::AccessConfig(bool tReadOnly, std::function<void()> tAction) {
    Guard tLock;
    if (!Begin(tReadOnly)) {
      xLOG("Config → failed %s → %s", (tReadOnly ? "read" : "write"), mLabel);
      return;
    }
    tAction();
    End();
  }

  bool Configuration_::Init() {
  #if !PRODUCTION
    if (!xLOG_S) xLOG_B(BAUDRATE);
    while (!xLOG_S) vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    xLOG_PL();
  #endif  
    Guard tLock;
    if (!Begin(false)) {
      xLOG("Config → begin failed, retrying in 1s...");
      vTaskDelay(CONFIG_RETRY_DELAY_MS / portTICK_PERIOD_MS);
      if (!Begin(false)) {
        xLOG("Config → begin failed after retry, cCannot proceed");
        return false;
      }
    }
    bool tConfigExists = mConfig.getBool(kNvsDeviceConfig);
    End();
    if (!tConfigExists) {
      xLOG("Config → attempting to create default config...");
      if (!CreateConfig()) {
        xLOG("Config → failed to create default config");
        return false;
      }
    }
    return true;
  }

  SAppConfig Configuration_::GetDefaultConfig() {
    SAppConfig tDefaultConfig {};
    tDefaultConfig.Device.Name = "PHOTO FRAME CL01";
    tDefaultConfig.Device.Version = "v1.0";
    tDefaultConfig.Device.NextImgPin = NEXT_IMG_PIN;
    tDefaultConfig.Device.ResetPin = RESET_PIN;
    tDefaultConfig.Device.SettingPin = SETTING_PIN;
    tDefaultConfig.Device.ActLedPin = ACT_LED_PIN;
    tDefaultConfig.Display.Width = DISPLAY_WIDTH;
    tDefaultConfig.Display.Height = DISPLAY_HEIGHT;
    tDefaultConfig.Display.Rotate = DISPLAY_ROTATE_FALLBACK;
    tDefaultConfig.Display.JpgBrightness = Percentage(25);
    tDefaultConfig.Display.JpgContrast = Percentage(75);
    tDefaultConfig.Display.JpgGamma = Percentage(125);
    tDefaultConfig.Display.ImagesDir = IMAGES_DIR;
    tDefaultConfig.Display.ImageExt = IMAGE_EXT;
    tDefaultConfig.Display.CurrentFile = "";
    tDefaultConfig.Display.ImageUpdatedAt = 0;
    tDefaultConfig.Ntp.Server = "ro.pool.ntp.org";
    tDefaultConfig.Ntp.NtpPort = Port(123);
    tDefaultConfig.Ntp.GMTOffset = 2 * 60 * 60;
    tDefaultConfig.Ntp.UpdateInterval = 60 * 1000;
    tDefaultConfig.Connection.ApModeEnable = true;
    tDefaultConfig.Connection.ApSsid = "PhotoFrameCL01";
    tDefaultConfig.Connection.ApPassword = "123456789";
    tDefaultConfig.Connection.ApIp = "192.168.4.1";
    tDefaultConfig.Connection.ApGateway = "192.168.4.1";
    tDefaultConfig.Connection.ApSubnet = "255.255.255.0";
    tDefaultConfig.Connection.StaSsid = "SSID";
    tDefaultConfig.Connection.StaPassword = "PASSWORD";
    tDefaultConfig.Connection.StaIpEnable = false;
    tDefaultConfig.Connection.StaIp = "192.168.0.83";
    tDefaultConfig.Connection.StaGateway = "192.168.0.1";
    tDefaultConfig.Connection.StaSubnet = "255.255.255.0";
    tDefaultConfig.Connection.StaPrimaryDns = "192.168.0.1";
    tDefaultConfig.Connection.StaSecondaryDns = "8.8.8.8";
    tDefaultConfig.Connection.MdnsEnable = false;
    tDefaultConfig.Connection.MdnsName = "photoframecl01";
    tDefaultConfig.Timer.WakeUp = ETimerWakeUp::Daily;
    tDefaultConfig.Timer.WakeUpHour = 6;
    tDefaultConfig.Dashboard.User = "admin";
    tDefaultConfig.Dashboard.Password = "";
    return tDefaultConfig;
  }

  bool Configuration_::CreateConfig() {
    SAppConfig tDefaultConfig = GetDefaultConfig();
    if (!SaveAllConfig(tDefaultConfig)) {
      xLOG("Config → failed to save default config values");
      return false;
    }
    bool tSuccess = false;
    AccessConfig(false, [&]() {
      tSuccess = mConfig.putBool(kNvsDeviceConfig, true);
    });
    if (!tSuccess) {
      xLOG("Config → creation failed, NVS write access failure");
      return false;
    }
    xLOG("Config → default config created successful");
    return true;
  }

  bool Configuration_::FactoryReset() {
    xLOG("Config → starting factory reset...");
    Guard tLock;
    if (!Begin(false)) {
      xLOG("Config → failed to open NVS for factory reset");
      return false;
    }
    if (!mConfig.clear()) {
      xLOG("Config → failed to clear NVS storage");
      End();
      return false;
    }
    xLOG("Config → NVS storage cleared successful");
    End();
    if (!CreateConfig()) {
      xLOG("Config → failed to recreate default configuration");
      return false;
    }
    AccessConfig(false, [&]() {
      mConfig.putBool(kNvsDeviceConfig, true);
    });
    xLOG("Config → factory reset completed successful");
    return true;
  }

  template<> SDeviceConfig Configuration_::Get<SDeviceConfig>() {
    SDeviceConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.Name = mConfig.getString(kNvsDeviceAppName, "PHOTO FRAME CL01");
      tCfg.Version = mConfig.getString(kNvsDeviceVersion, "v1.0");
      tCfg.NextImgPin = NEXT_IMG_PIN;
      tCfg.ResetPin = RESET_PIN;
      tCfg.SettingPin = SETTING_PIN;
      tCfg.ActLedPin = ACT_LED_PIN;
    });
    return tCfg;
  }

  template<> SNTPConfig Configuration_::Get<SNTPConfig>() {
    SNTPConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.Server = mConfig.getString(kNvsTimeServer, "ro.pool.ntp.org");
      tCfg.NtpPort = Port(mConfig.getUShort(kNvsTimePort, 123));
      tCfg.GMTOffset = mConfig.getInt(kNvsTimeGmtOffset, 2 * 60 * 60);
      tCfg.UpdateInterval = mConfig.getUInt(kNvsTimeUpdate, 60 * 1000);
    });
    return tCfg;
  }

  template<> SConnectionConfig Configuration_::Get<SConnectionConfig>() {
    SConnectionConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.ApModeEnable = mConfig.getBool(kNvsConApEnable, false);
      tCfg.ApSsid = mConfig.getString(kNvsConApSsid, "PhotoFrame CL01");
      tCfg.ApPassword = mConfig.getString(kNvsConApPass, "123456789");
      tCfg.ApIp = mConfig.getString(kNvsConApIp, "192.168.4.1");
      tCfg.ApGateway = mConfig.getString(kNvsConApGw, "192.168.4.1");
      tCfg.ApSubnet = mConfig.getString(kNvsConApSubnet, "255.255.255.0");
      tCfg.StaSsid = mConfig.getString(kNvsConStaSsid, "");
      tCfg.StaPassword = mConfig.getString(kNvsConStaPass, "");
      tCfg.StaIpEnable = mConfig.getBool(kNvsConStaEnable, false);
      tCfg.StaIp = mConfig.getString(kNvsConStaIp, "");
      tCfg.StaGateway = mConfig.getString(kNvsConStaGw, "");
      tCfg.StaSubnet = mConfig.getString(kNvsConStaSubnet, "");
      tCfg.StaPrimaryDns = mConfig.getString(kNvsConStaDns1, "");
      tCfg.StaSecondaryDns = mConfig.getString(kNvsConStaDns2, "");
      tCfg.MdnsEnable = mConfig.getBool(kNvsConMdnsEnable, false);
      tCfg.MdnsName = mConfig.getString(kNvsConMdnsName, "photoframecl01");
    });
    return tCfg;
  }

  template<> SDisplayConfig Configuration_::Get<SDisplayConfig>() {
    SDisplayConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.Width = DISPLAY_WIDTH;
      tCfg.Height = DISPLAY_HEIGHT;
      uint16_t tRotate = mConfig.getUShort(kNvsDisplayRotate, DISPLAY_ROTATE_FALLBACK);
      switch (tRotate) {
        case 0:
        case 90:
        case 180:
        case 270:
          tCfg.Rotate = tRotate;
          break;
        default:
          tCfg.Rotate = DISPLAY_ROTATE_FALLBACK;
          break;
      }
      tCfg.JpgBrightness = Percentage(mConfig.getUChar(kNvsDisplayBrightness, 25));
      tCfg.JpgContrast = Percentage(mConfig.getUChar(kNvsDisplayContrast, 75));
      tCfg.JpgGamma = Percentage(mConfig.getUChar(kNvsDisplayGamma, 125));
      tCfg.ImagesDir = IMAGES_DIR;
      tCfg.ImageExt = IMAGE_EXT;
      tCfg.CurrentFile = mConfig.getString(kNvsDisplayFile, "");
      tCfg.ImageUpdatedAt = mConfig.getULong(kNvsDisplayImageUpdatedAt, 0);
    });
    return tCfg;
  }

  template<> STimerConfig Configuration_::Get<STimerConfig>() {
    STimerConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.WakeUp = static_cast<ETimerWakeUp>(mConfig.getUChar(kNvsTimerWake, static_cast<uint8_t>(ETimerWakeUp::Daily)));
      tCfg.WakeUpHour = mConfig.getUChar(kNvsTimerWakeHour, 6);
    });
    return tCfg;
  }

  template<> SStorageConfig Configuration_::Get<SStorageConfig>() {
    SStorageConfig tCfg {};
    tCfg.DefaultFileSystem = DEFAULT_FILE_SYSTEM;
    tCfg.FallbackEnabled = STORAGE_FALLBACK_ENABLED;
    return tCfg;
  }

  template<> SDashboardConfig Configuration_::Get<SDashboardConfig>() {
    SDashboardConfig tCfg {};
    AccessConfig(true, [&]() {
      tCfg.User = mConfig.getString(kNvsDashUser, "admin");
      tCfg.Password = mConfig.getString(kNvsDashPassword, "");
    });
    return tCfg;
  }

  template<> SAppConfig Configuration_::Get<SAppConfig>() {
    SAppConfig tCfg {};
    tCfg.Device = Get<SDeviceConfig>();
    tCfg.Ntp = Get<SNTPConfig>();
    tCfg.Connection = Get<SConnectionConfig>();
    tCfg.Display = Get<SDisplayConfig>();
    tCfg.Timer = Get<STimerConfig>();
    tCfg.Storage = Get<SStorageConfig>();
    tCfg.Dashboard = Get<SDashboardConfig>();
    return tCfg;
  }

  bool Configuration_::SaveImageName(const char *tValue) {
    if (!tValue) return false;
    bool tSuccess = false;
    uint32_t tEpoch = static_cast<uint32_t>(time(nullptr));
    if (tEpoch == 0) tEpoch = RTC.GetEpoch();
    AccessConfig(false, [&]() {
      size_t tBytesWritten = mConfig.putString(kNvsDisplayFile, tValue);
      String tSaved = mConfig.getString(kNvsDisplayFile, "");
      tSuccess = (tBytesWritten > 0) || (tSaved == String(tValue));
      if (tSuccess) tSuccess = mConfig.putULong(kNvsDisplayImageUpdatedAt, tEpoch);
    });
    if (!tSuccess) xLOG("Config → failed to save image name: %s", (tValue && tValue[0]) ? tValue : "<empty>");
    return tSuccess;
  }

  uint32_t Configuration_::GetImageUpdatedAt() {
    uint32_t tValue = 0;
    AccessConfig(true, [&]() {
      tValue = mConfig.getULong(kNvsDisplayImageUpdatedAt, 0);
    });
    return tValue;
  }

  bool Configuration_::SaveAllConfig(const SAppConfig &tConfig) {
    bool tSuccess = true;
    const char *tFailedKey = nullptr;
    AccessConfig(false, [&]() {
      auto tPutBool = [&](const char *tKey, bool tResult) {
        if (!tSuccess) return;
        if (!tResult) {
          tSuccess = false;
          tFailedKey = tKey;
        }
      };
      auto tPutString = [&](const char *tKey, const String &tValue) {
        if (!tSuccess) return;
        size_t tBytesWritten = mConfig.putString(tKey, tValue);
        if (tBytesWritten > 0) return;
        // Empty strings may report 0 bytes written on some cores, verify persisted value.
        String tSaved = mConfig.getString(tKey, "");
        if (tSaved != tValue) {
          tSuccess = false;
          tFailedKey = tKey;
        }
      };

      tPutString(kNvsDeviceAppName, tConfig.Device.Name);
      tPutString(kNvsDeviceVersion, tConfig.Device.Version);
      tPutBool(kNvsDeviceActLedPin, mConfig.putUChar(kNvsDeviceActLedPin, tConfig.Device.ActLedPin));
      tPutBool(kNvsDisplayRotate, mConfig.putUShort(kNvsDisplayRotate, tConfig.Display.Rotate));
      tPutBool(kNvsDisplayBrightness, mConfig.putUChar(kNvsDisplayBrightness, tConfig.Display.JpgBrightness.Get()));
      tPutBool(kNvsDisplayContrast, mConfig.putUChar(kNvsDisplayContrast, tConfig.Display.JpgContrast.Get()));
      tPutBool(kNvsDisplayGamma, mConfig.putUChar(kNvsDisplayGamma, tConfig.Display.JpgGamma.Get()));
      if (tConfig.Display.CurrentFile.length() > 0) tPutString(kNvsDisplayFile, tConfig.Display.CurrentFile);
      tPutBool(kNvsDisplayImageUpdatedAt, mConfig.putULong(kNvsDisplayImageUpdatedAt, tConfig.Display.ImageUpdatedAt));
      tPutString(kNvsTimeServer, tConfig.Ntp.Server);
      tPutBool(kNvsTimePort, mConfig.putUShort(kNvsTimePort, tConfig.Ntp.NtpPort.Get()));
      tPutBool(kNvsTimeGmtOffset, mConfig.putInt(kNvsTimeGmtOffset, tConfig.Ntp.GMTOffset));
      tPutBool(kNvsTimeUpdate, mConfig.putUInt(kNvsTimeUpdate, tConfig.Ntp.UpdateInterval));
      tPutBool(kNvsConApEnable, mConfig.putBool(kNvsConApEnable, tConfig.Connection.ApModeEnable));
      tPutString(kNvsConApSsid, tConfig.Connection.ApSsid);
      tPutString(kNvsConApPass, tConfig.Connection.ApPassword);
      tPutString(kNvsConApIp, tConfig.Connection.ApIp);
      tPutString(kNvsConApGw, tConfig.Connection.ApGateway);
      tPutString(kNvsConApSubnet, tConfig.Connection.ApSubnet);
      tPutString(kNvsConStaSsid, tConfig.Connection.StaSsid);
      tPutString(kNvsConStaPass, tConfig.Connection.StaPassword);
      tPutBool(kNvsConStaEnable, mConfig.putBool(kNvsConStaEnable, tConfig.Connection.StaIpEnable));
      tPutString(kNvsConStaIp, tConfig.Connection.StaIp);
      tPutString(kNvsConStaGw, tConfig.Connection.StaGateway);
      tPutString(kNvsConStaSubnet, tConfig.Connection.StaSubnet);
      tPutString(kNvsConStaDns1, tConfig.Connection.StaPrimaryDns);
      tPutString(kNvsConStaDns2, tConfig.Connection.StaSecondaryDns);
      tPutBool(kNvsConMdnsEnable, mConfig.putBool(kNvsConMdnsEnable, tConfig.Connection.MdnsEnable));
      tPutString(kNvsConMdnsName, tConfig.Connection.MdnsName);
      tPutBool(kNvsTimerWake, mConfig.putUChar(kNvsTimerWake, static_cast<uint8_t>(tConfig.Timer.WakeUp)));
      tPutBool(kNvsTimerWakeHour, mConfig.putUChar(kNvsTimerWakeHour, tConfig.Timer.WakeUpHour));
      tPutString(kNvsDashUser, tConfig.Dashboard.User);
      tPutString(kNvsDashPassword, tConfig.Dashboard.Password);
    });
    if (tSuccess) xLOG("Config → save successful");
    else xLOG("Config → save failed some values may not have been written! (key: %s)", tFailedKey ? tFailedKey : "unknown");
    return tSuccess;
  }

} 
