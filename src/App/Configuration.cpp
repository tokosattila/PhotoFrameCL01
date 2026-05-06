#include <App/Configuration.h>
#include <App/Dashboard/Utils/DashboardUtils.h>

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
      xLOG("Config failed %s → %s", (tReadOnly ? "read" : "write"), mLabel);
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
      xLOG("Config begin failed, retrying in 1s...");
      vTaskDelay(CONFIG_RETRY_DELAY_MS / portTICK_PERIOD_MS);
      if (!Begin(false)) {
        xLOG("Config begin failed after retry, cannot proceed");
        return false;
      }
    }
    bool tConfigExists = mConfig.getBool(kNvsDeviceConfig);
    End();
    if (!tConfigExists) {
      xLOG("Attempting to create default config...");
      if (!CreateConfig()) {
        xLOG("Failed to create default config");
        return false;
      }
    }
    return true;
  }

  SAppConfig Configuration_::GetDefaultConfig() {
    SAppConfig tDefaultConfig {};
    tDefaultConfig.Device.Name = "Photo Frame CL01";
    tDefaultConfig.Device.Version = "v1.0";
    tDefaultConfig.Device.SoundEnabled = true;
    tDefaultConfig.Device.LogManagerEnabled = true;
    tDefaultConfig.Device.NextImgPin = NEXT_IMG_PIN;
    tDefaultConfig.Device.ResetPin = RESET_PIN;
    tDefaultConfig.Device.SettingPin = SETTING_PIN;
    tDefaultConfig.Device.ActLedPin = ACT_LED_PIN;
    tDefaultConfig.Display.Width = DISPLAY_WIDTH;
    tDefaultConfig.Display.Height = DISPLAY_HEIGHT;
    tDefaultConfig.Display.Rotate = DISPLAY_ROTATE;
    tDefaultConfig.Display.JpgBrightness = DISPLAY_JPG_BRIGHTNESS;
    tDefaultConfig.Display.JpgContrast = DISPLAY_JPG_CONTRAST;
    tDefaultConfig.Display.JpgGamma = DISPLAY_JPG_GAMMA;
    tDefaultConfig.Display.JpgSaturation = DISPLAY_JPG_SATURATION;
    tDefaultConfig.Display.JpgRedGain = DISPLAY_JPG_RED_GAIN;
    tDefaultConfig.Display.JpgGreenGain = DISPLAY_JPG_GREEN_GAIN;
    tDefaultConfig.Display.JpgBlueGain = DISPLAY_JPG_BLUE_GAIN;
    tDefaultConfig.Display.ImagesDir = IMAGES_DIR;
    tDefaultConfig.Display.ImageExt = IMAGE_EXT;
    tDefaultConfig.Display.CurrentFile = "";
    tDefaultConfig.Display.ImageUpdatedAt = 0;
    tDefaultConfig.Ntp.Server = "pool.ntp.org";
    tDefaultConfig.Ntp.NtpPort = 123;
    tDefaultConfig.Ntp.GMTOffset = 0;
    tDefaultConfig.Ntp.DaylightOffset = 0;
    tDefaultConfig.Ntp.TimeZoneLabel = "ETC/UTC";
    tDefaultConfig.Ntp.LowPowerSyncEnable = true;
    tDefaultConfig.Ntp.LowPowerSyncIntervalSec = 7 * SECONDS_PER_DAY;
    tDefaultConfig.Ntp.LastSuccessfulSyncEpochUtc = 0;
    tDefaultConfig.Connection.ApModeEnable = true;
    tDefaultConfig.Connection.ApSsid = "PhotoFrameCL01";
    tDefaultConfig.Connection.ApPassword = "123456789";
    tDefaultConfig.Connection.ApIp = "192.168.4.1";
    tDefaultConfig.Connection.ApGateway = "192.168.4.1";
    tDefaultConfig.Connection.ApSubnet = "255.255.255.0";
    tDefaultConfig.Connection.FallbackApSsid = "PhotoFrameCL01-Fallback";
    tDefaultConfig.Connection.FallbackApPassword = "123456789";
    tDefaultConfig.Connection.FallbackApIp = "192.168.10.1";
    tDefaultConfig.Connection.FallbackApGateway = "192.168.10.1";
    tDefaultConfig.Connection.FallbackApSubnet = "255.255.255.0";
    tDefaultConfig.Connection.StaSsid = "SSID";
    tDefaultConfig.Connection.StaPassword = "PASSWORD";
    tDefaultConfig.Connection.StaAutoFallbackApEnable = true;
    tDefaultConfig.Connection.StaConnectMaxRetry = 3;
    tDefaultConfig.Connection.StaRetryDelayMs = 5 * ONE_SECOND_MS;
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
    tDefaultConfig.Storage.DefaultFileSystem = STORAGE_DEFAULT;
    tDefaultConfig.Storage.FallbackEnabled = STORAGE_FALLBACK_ENABLED;
    tDefaultConfig.Dashboard.User = "admin";
    tDefaultConfig.Dashboard.Password = "15e2b0d3c33891ebb0f1ef609ec419420c20e320ce94c65fbc8c3312448eb225";
    tDefaultConfig.Dashboard.Language = "en";
    tDefaultConfig.Dashboard.EnabledLanguages = {"en", "hu"};
    tDefaultConfig.Dashboard.Theme = "light";
    tDefaultConfig.Dashboard.ShowDescription = true;
    tDefaultConfig.Dashboard.DynamicCpuScaling = true;
    tDefaultConfig.Dashboard.TargetWidth = DASHBOARD_IMG_WIDTH;
    tDefaultConfig.Dashboard.TargetHeight = DASHBOARD_IMG_HEIGHT;
    tDefaultConfig.Dashboard.ThumbWidth = DASHBOARD_IMG_THUMB_WIDTH;
    tDefaultConfig.Dashboard.ThumbHeight = DASHBOARD_IMG_THUMB_HEIGHT;
    tDefaultConfig.Dashboard.Rotate = DASHBOARD_IMG_ROTATE;
    tDefaultConfig.Dashboard.ImageExt = DASHBOARD_IMG_EXT;
    return tDefaultConfig;
  }

  bool Configuration_::CreateConfig() {
    SAppConfig tDefaultConfig = GetDefaultConfig();
    if (!SaveAllConfig(tDefaultConfig)) {
      xLOG("Failed to save default config values");
      return false;
    }
    bool tSuccess = false;
    AccessConfig(false, [&]() {
      tSuccess = mConfig.putBool(kNvsDeviceConfig, true);
    });
    if (!tSuccess) {
      xLOG("Config creation failed, NVS write access failure");
      return false;
    }
    xLOG("Default config created successfully");
    return true;
  }

  bool Configuration_::FactoryReset() {
    xLOG("Starting factory reset...");
    Guard tLock;
    if (!Begin(false)) {
      xLOG("Failed to open NVS for factory reset");
      return false;
    }
    if (!mConfig.clear()) {
      xLOG("Failed to clear NVS storage");
      End();
      return false;
    }
    xLOG("NVS storage cleared successfully");
    End();
    if (!CreateConfig()) {
      xLOG("Failed to recreate default configuration");
      return false;
    }
    AccessConfig(false, [&]() {
      mConfig.putBool(kNvsDeviceConfig, true);
    });
    xLOG("Factory reset completed successful");
    return true;
  }

  template<> SDeviceConfig Configuration_::Get<SDeviceConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SDeviceConfig tCfg = tDefaultConfig.Device;
    AccessConfig(true, [&]() {
      tCfg.Name = mConfig.getString(kNvsDeviceAppName, tDefaultConfig.Device.Name);
      tCfg.Version = mConfig.getString(kNvsDeviceVersion, tDefaultConfig.Device.Version);
      tCfg.SoundEnabled = mConfig.getBool(kNvsDeviceSoundEnabled, tDefaultConfig.Device.SoundEnabled);
      tCfg.LogManagerEnabled = mConfig.getBool(kNvsDeviceLogManagerEnabled, tDefaultConfig.Device.LogManagerEnabled);
      tCfg.NextImgPin = NEXT_IMG_PIN;
      tCfg.ResetPin = RESET_PIN;
      tCfg.SettingPin = SETTING_PIN;
      tCfg.ActLedPin = ACT_LED_PIN;
    });
    return tCfg;
  }

  template<> SNTPConfig Configuration_::Get<SNTPConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SNTPConfig tCfg = tDefaultConfig.Ntp;
    AccessConfig(true, [&]() {
      tCfg.Server = mConfig.getString(kNvsTimeServer, tDefaultConfig.Ntp.Server);
      tCfg.NtpPort = mConfig.getUShort(kNvsTimePort, tDefaultConfig.Ntp.NtpPort);
      tCfg.GMTOffset = mConfig.getInt(kNvsTimeGmtOffset, tDefaultConfig.Ntp.GMTOffset);
      tCfg.DaylightOffset = mConfig.getInt(kNvsTimeDaylightOffset, tDefaultConfig.Ntp.DaylightOffset);
      tCfg.TimeZoneLabel = mConfig.getString(kNvsTimeZoneLabel, tDefaultConfig.Ntp.TimeZoneLabel);
      tCfg.LowPowerSyncEnable = mConfig.getBool(kNvsTimeLowPowerSyncEnable, tDefaultConfig.Ntp.LowPowerSyncEnable);
      tCfg.LowPowerSyncIntervalSec = mConfig.getUInt(kNvsTimeLowPowerSyncIntervalSec, tDefaultConfig.Ntp.LowPowerSyncIntervalSec);
      tCfg.LastSuccessfulSyncEpochUtc = mConfig.getUInt(kNvsTimeLastSuccessfulSyncEpochUtc, tDefaultConfig.Ntp.LastSuccessfulSyncEpochUtc);
    });
    if (tCfg.LowPowerSyncIntervalSec != 0 &&
        tCfg.LowPowerSyncIntervalSec != SECONDS_PER_DAY &&
        tCfg.LowPowerSyncIntervalSec != 7UL * SECONDS_PER_DAY &&
        tCfg.LowPowerSyncIntervalSec != 30UL * SECONDS_PER_DAY &&
        tCfg.LowPowerSyncIntervalSec != 90UL * SECONDS_PER_DAY &&
        tCfg.LowPowerSyncIntervalSec != 180UL * SECONDS_PER_DAY) {
      tCfg.LowPowerSyncIntervalSec = tDefaultConfig.Ntp.LowPowerSyncIntervalSec;
    }
    if (tCfg.LowPowerSyncIntervalSec == 0) tCfg.LowPowerSyncEnable = false;
    return tCfg;
  }

  template<> SConnectionConfig Configuration_::Get<SConnectionConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SConnectionConfig tCfg = tDefaultConfig.Connection;
    AccessConfig(true, [&]() {
      tCfg.ApModeEnable = mConfig.getBool(kNvsConApEnable, tDefaultConfig.Connection.ApModeEnable);
      tCfg.ApSsid = mConfig.getString(kNvsConApSsid, tDefaultConfig.Connection.ApSsid);
      tCfg.ApPassword = mConfig.getString(kNvsConApPass, tDefaultConfig.Connection.ApPassword);
      tCfg.ApIp = mConfig.getString(kNvsConApIp, tDefaultConfig.Connection.ApIp);
      tCfg.ApGateway = mConfig.getString(kNvsConApGw, tDefaultConfig.Connection.ApGateway);
      tCfg.ApSubnet = mConfig.getString(kNvsConApSubnet, tDefaultConfig.Connection.ApSubnet);
      tCfg.FallbackApSsid = mConfig.getString(kNvsConFallbackApSsid, tDefaultConfig.Connection.FallbackApSsid);
      tCfg.FallbackApPassword = mConfig.getString(kNvsConFallbackApPass, tDefaultConfig.Connection.FallbackApPassword);
      tCfg.FallbackApIp = mConfig.getString(kNvsConFallbackApIp, tDefaultConfig.Connection.FallbackApIp);
      tCfg.FallbackApGateway = mConfig.getString(kNvsConFallbackApGw, tDefaultConfig.Connection.FallbackApGateway);
      tCfg.FallbackApSubnet = mConfig.getString(kNvsConFallbackApSubnet, tDefaultConfig.Connection.FallbackApSubnet);
      tCfg.StaSsid = mConfig.getString(kNvsConStaSsid, tDefaultConfig.Connection.StaSsid);
      tCfg.StaPassword = mConfig.getString(kNvsConStaPass, tDefaultConfig.Connection.StaPassword);
      tCfg.StaAutoFallbackApEnable = mConfig.getBool(kNvsConStaAutoFallbackApEnable, tDefaultConfig.Connection.StaAutoFallbackApEnable);
      tCfg.StaConnectMaxRetry = mConfig.getUChar(kNvsConStaConnectMaxRetry, tDefaultConfig.Connection.StaConnectMaxRetry);
      tCfg.StaRetryDelayMs = mConfig.getUInt(kNvsConStaRetryDelayMs, tDefaultConfig.Connection.StaRetryDelayMs);
      tCfg.StaIpEnable = mConfig.getBool(kNvsConStaEnable, tDefaultConfig.Connection.StaIpEnable);
      tCfg.StaIp = mConfig.getString(kNvsConStaIp, tDefaultConfig.Connection.StaIp);
      tCfg.StaGateway = mConfig.getString(kNvsConStaGw, tDefaultConfig.Connection.StaGateway);
      tCfg.StaSubnet = mConfig.getString(kNvsConStaSubnet, tDefaultConfig.Connection.StaSubnet);
      tCfg.StaPrimaryDns = mConfig.getString(kNvsConStaDns1, tDefaultConfig.Connection.StaPrimaryDns);
      tCfg.StaSecondaryDns = mConfig.getString(kNvsConStaDns2, tDefaultConfig.Connection.StaSecondaryDns);
      tCfg.MdnsEnable = mConfig.getBool(kNvsConMdnsEnable, tDefaultConfig.Connection.MdnsEnable);
      tCfg.MdnsName = mConfig.getString(kNvsConMdnsName, tDefaultConfig.Connection.MdnsName);
    });
    if (tCfg.StaConnectMaxRetry == 0) tCfg.StaConnectMaxRetry = tDefaultConfig.Connection.StaConnectMaxRetry;
    if (tCfg.StaRetryDelayMs < ONE_SECOND_MS) tCfg.StaRetryDelayMs = tDefaultConfig.Connection.StaRetryDelayMs;
    if (!tCfg.FallbackApSsid.length()) tCfg.FallbackApSsid = tDefaultConfig.Connection.FallbackApSsid;
    if (!tCfg.FallbackApPassword.length()) tCfg.FallbackApPassword = tDefaultConfig.Connection.FallbackApPassword;
    if (!tCfg.FallbackApIp.length()) tCfg.FallbackApIp = tDefaultConfig.Connection.FallbackApIp;
    if (!tCfg.FallbackApGateway.length()) tCfg.FallbackApGateway = tDefaultConfig.Connection.FallbackApGateway;
    if (!tCfg.FallbackApSubnet.length()) tCfg.FallbackApSubnet = tDefaultConfig.Connection.FallbackApSubnet;
    return tCfg;
  }

  template<> SDisplayConfig Configuration_::Get<SDisplayConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SDisplayConfig tCfg = tDefaultConfig.Display;
    AccessConfig(true, [&]() {
      tCfg.Width = tDefaultConfig.Display.Width;
      tCfg.Height = tDefaultConfig.Display.Height;
      uint16_t tRotate = mConfig.getUShort(kNvsDisplayRotate, tDefaultConfig.Display.Rotate);
      switch (tRotate) {
        case 0:
        case 90:
        case 180:
        case 270:
          tCfg.Rotate = tRotate;
          break;
        default:
          tCfg.Rotate = tDefaultConfig.Display.Rotate;
          break;
      }
      tCfg.JpgBrightness = mConfig.getUChar(kNvsDisplayBrightness, tDefaultConfig.Display.JpgBrightness);
      tCfg.JpgContrast = mConfig.getUChar(kNvsDisplayContrast, tDefaultConfig.Display.JpgContrast);
      tCfg.JpgGamma = mConfig.getUChar(kNvsDisplayGamma, tDefaultConfig.Display.JpgGamma);
      tCfg.JpgSaturation = mConfig.getUChar(kNvsDisplaySaturation, tDefaultConfig.Display.JpgSaturation);
      tCfg.JpgRedGain = mConfig.getUChar(kNvsDisplayRedGain, tDefaultConfig.Display.JpgRedGain);
      tCfg.JpgGreenGain = mConfig.getUChar(kNvsDisplayGreenGain, tDefaultConfig.Display.JpgGreenGain);
      tCfg.JpgBlueGain = mConfig.getUChar(kNvsDisplayBlueGain, tDefaultConfig.Display.JpgBlueGain);
      tCfg.ImagesDir = tDefaultConfig.Display.ImagesDir;
      tCfg.ImageExt = tDefaultConfig.Display.ImageExt;
      tCfg.CurrentFile = mConfig.getString(kNvsDisplayFile, tDefaultConfig.Display.CurrentFile);
      tCfg.ImageUpdatedAt = mConfig.getULong(kNvsDisplayImageUpdatedAt, tDefaultConfig.Display.ImageUpdatedAt);
    });
    return tCfg;
  }

  template<> STimerConfig Configuration_::Get<STimerConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    STimerConfig tCfg = tDefaultConfig.Timer;
    AccessConfig(true, [&]() {
      tCfg.WakeUp = static_cast<ETimerWakeUp>(mConfig.getUChar(kNvsTimerWake, static_cast<uint8_t>(tDefaultConfig.Timer.WakeUp)));
      tCfg.WakeUpHour = mConfig.getUChar(kNvsTimerWakeHour, tDefaultConfig.Timer.WakeUpHour);
    });
    return tCfg;
  }

  template<> SStorageConfig Configuration_::Get<SStorageConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SStorageConfig tCfg = tDefaultConfig.Storage;
    AccessConfig(true, [&]() {
      tCfg.DefaultFileSystem = static_cast<EFileSystemType>(mConfig.getUChar(kNvsStgDefaultFs, static_cast<uint8_t>(tDefaultConfig.Storage.DefaultFileSystem)));
      tCfg.FallbackEnabled = mConfig.getBool(kNvsStgFallback, tDefaultConfig.Storage.FallbackEnabled);
    });
    return tCfg;
  }

  template<> SDashboardConfig Configuration_::Get<SDashboardConfig>() {
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    SDashboardConfig tCfg = tDefaultConfig.Dashboard;
    AccessConfig(true, [&]() {
      tCfg.User = mConfig.getString(kNvsDashUser, tDefaultConfig.Dashboard.User);
      tCfg.Password = mConfig.getString(kNvsDashPassword, tDefaultConfig.Dashboard.Password);
      tCfg.Language = DashboardUtils_::NormalizeLanguageCode(mConfig.getString(kNvsDashLanguage, tDefaultConfig.Dashboard.Language));
      if (!tCfg.Language.length()) tCfg.Language = DashboardUtils_::NormalizeLanguageCode(tDefaultConfig.Dashboard.Language);
      tCfg.EnabledLanguages = DashboardUtils_::ParseEnabledLanguages(
        mConfig.getString(kNvsDashEnabledLanguages, DashboardUtils_::JoinEnabledLanguages(tDefaultConfig.Dashboard.EnabledLanguages, tCfg.Language)),
        tCfg.Language);
      tCfg.Theme = mConfig.getString(kNvsDashTheme, tDefaultConfig.Dashboard.Theme);
      tCfg.ShowDescription = mConfig.getBool(kNvsDashShowDescription, tDefaultConfig.Dashboard.ShowDescription);
      tCfg.DynamicCpuScaling = mConfig.getBool(kNvsDashDynamicCpuScaling, tDefaultConfig.Dashboard.DynamicCpuScaling);
    });
    if (!tCfg.Language.length()) tCfg.Language = "en";
    tCfg.Theme.trim();
    tCfg.Theme.toLowerCase();
    if (tCfg.Theme != "dark") tCfg.Theme = "light";
    DashboardUtils_::NormalizeEnabledLanguages(tCfg.EnabledLanguages, tCfg.Language);
    tCfg.Language = DashboardUtils_::ResolveLanguage(tCfg.EnabledLanguages, tCfg.Language);
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
    if (!tSuccess) xLOG("Failed to save image name: %s", (tValue && tValue[0]) ? tValue : "<empty>");
    return tSuccess;
  }

  uint32_t Configuration_::GetImageUpdatedAt() {
    uint32_t tValue = 0;
    AccessConfig(true, [&]() {
      tValue = mConfig.getULong(kNvsDisplayImageUpdatedAt, 0);
    });
    return tValue;
  }

  uint32_t Configuration_::GetBootCount() {
    uint32_t tBootCount = 0;
    AccessConfig(true, [&]() {
      tBootCount = mConfig.getUInt(kNvsDeviceBootCount, 0);
    });
    return tBootCount;
  }

  uint32_t Configuration_::IncrementBootCount() {
    uint32_t tBootCount = 0;
    AccessConfig(false, [&]() {
      tBootCount = mConfig.getUInt(kNvsDeviceBootCount, 0) + 1;
      mConfig.putUInt(kNvsDeviceBootCount, tBootCount);
    });
    return tBootCount;
  }

  bool Configuration_::SaveAllConfig(const SAppConfig &tConfig) {
    bool tSuccess = true;
    const char *tFailedKey = nullptr;
    const SAppConfig tDefaultConfig = GetDefaultConfig();
    const uint8_t tStaConnectMaxRetry = tConfig.Connection.StaConnectMaxRetry > 0 ? tConfig.Connection.StaConnectMaxRetry : tDefaultConfig.Connection.StaConnectMaxRetry;
    const uint32_t tStaRetryDelayMs = tConfig.Connection.StaRetryDelayMs >= ONE_SECOND_MS ? tConfig.Connection.StaRetryDelayMs : tDefaultConfig.Connection.StaRetryDelayMs;
    const String tFallbackApSsid = tConfig.Connection.FallbackApSsid.length() ? tConfig.Connection.FallbackApSsid : tDefaultConfig.Connection.FallbackApSsid;
    const String tFallbackApPassword = tConfig.Connection.FallbackApPassword.length() ? tConfig.Connection.FallbackApPassword : tDefaultConfig.Connection.FallbackApPassword;
    const String tFallbackApIp = tConfig.Connection.FallbackApIp.length() ? tConfig.Connection.FallbackApIp : tDefaultConfig.Connection.FallbackApIp;
    const String tFallbackApGateway = tConfig.Connection.FallbackApGateway.length() ? tConfig.Connection.FallbackApGateway : tDefaultConfig.Connection.FallbackApGateway;
    const String tFallbackApSubnet = tConfig.Connection.FallbackApSubnet.length() ? tConfig.Connection.FallbackApSubnet : tDefaultConfig.Connection.FallbackApSubnet;
    String tDashboardLanguage = DashboardUtils_::NormalizeLanguageCode(tConfig.Dashboard.Language);
    if (!tDashboardLanguage.length()) tDashboardLanguage = DashboardUtils_::NormalizeLanguageCode(tDefaultConfig.Dashboard.Language);
    std::vector<String> tEnabledLanguages = tConfig.Dashboard.EnabledLanguages;
    DashboardUtils_::NormalizeEnabledLanguages(tEnabledLanguages, tDashboardLanguage);
    tDashboardLanguage = DashboardUtils_::ResolveLanguage(tEnabledLanguages, tDashboardLanguage);
    const String tEnabledLanguagesValue = DashboardUtils_::JoinEnabledLanguages(tEnabledLanguages, tDashboardLanguage);
    String tDashboardTheme = tConfig.Dashboard.Theme;
    tDashboardTheme.trim();
    tDashboardTheme.toLowerCase();
    if (tDashboardTheme != "dark") tDashboardTheme = "light";
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
        String tSaved = mConfig.getString(tKey, "");
        if (tSaved != tValue) {
          tSuccess = false;
          tFailedKey = tKey;
        }
      };
      tPutString(kNvsDeviceAppName, tConfig.Device.Name);
      tPutString(kNvsDeviceVersion, tConfig.Device.Version);
      tPutBool(kNvsDeviceSoundEnabled, mConfig.putBool(kNvsDeviceSoundEnabled, tConfig.Device.SoundEnabled));
      tPutBool(kNvsDeviceLogManagerEnabled, mConfig.putBool(kNvsDeviceLogManagerEnabled, tConfig.Device.LogManagerEnabled));
      tPutBool(kNvsDeviceActLedPin, mConfig.putUChar(kNvsDeviceActLedPin, tConfig.Device.ActLedPin));
      tPutBool(kNvsDisplayRotate, mConfig.putUShort(kNvsDisplayRotate, tConfig.Display.Rotate));
      tPutBool(kNvsDisplayBrightness, mConfig.putUChar(kNvsDisplayBrightness, tConfig.Display.JpgBrightness));
      tPutBool(kNvsDisplayContrast, mConfig.putUChar(kNvsDisplayContrast, tConfig.Display.JpgContrast));
      tPutBool(kNvsDisplayGamma, mConfig.putUChar(kNvsDisplayGamma, tConfig.Display.JpgGamma));
      tPutBool(kNvsDisplaySaturation, mConfig.putUChar(kNvsDisplaySaturation, tConfig.Display.JpgSaturation));
      tPutBool(kNvsDisplayRedGain, mConfig.putUChar(kNvsDisplayRedGain, tConfig.Display.JpgRedGain));
      tPutBool(kNvsDisplayGreenGain, mConfig.putUChar(kNvsDisplayGreenGain, tConfig.Display.JpgGreenGain));
      tPutBool(kNvsDisplayBlueGain, mConfig.putUChar(kNvsDisplayBlueGain, tConfig.Display.JpgBlueGain));
      tPutString(kNvsDisplayFile, tConfig.Display.CurrentFile);
      tPutBool(kNvsDisplayImageUpdatedAt, mConfig.putULong(kNvsDisplayImageUpdatedAt, tConfig.Display.ImageUpdatedAt));
      tPutString(kNvsTimeServer, tConfig.Ntp.Server);
      tPutBool(kNvsTimePort, mConfig.putUShort(kNvsTimePort, tConfig.Ntp.NtpPort));
      tPutBool(kNvsTimeGmtOffset, mConfig.putInt(kNvsTimeGmtOffset, tConfig.Ntp.GMTOffset));
      tPutBool(kNvsTimeDaylightOffset, mConfig.putInt(kNvsTimeDaylightOffset, tConfig.Ntp.DaylightOffset));
      tPutString(kNvsTimeZoneLabel, tConfig.Ntp.TimeZoneLabel);
      tPutBool(kNvsTimeLowPowerSyncEnable, mConfig.putBool(kNvsTimeLowPowerSyncEnable, tConfig.Ntp.LowPowerSyncEnable));
      tPutBool(kNvsTimeLowPowerSyncIntervalSec, mConfig.putUInt(kNvsTimeLowPowerSyncIntervalSec, tConfig.Ntp.LowPowerSyncIntervalSec));
      tPutBool(kNvsTimeLastSuccessfulSyncEpochUtc, mConfig.putUInt(kNvsTimeLastSuccessfulSyncEpochUtc, tConfig.Ntp.LastSuccessfulSyncEpochUtc));
      tPutBool(kNvsConApEnable, mConfig.putBool(kNvsConApEnable, tConfig.Connection.ApModeEnable));
      tPutString(kNvsConApSsid, tConfig.Connection.ApSsid);
      tPutString(kNvsConApPass, tConfig.Connection.ApPassword);
      tPutString(kNvsConApIp, tConfig.Connection.ApIp);
      tPutString(kNvsConApGw, tConfig.Connection.ApGateway);
      tPutString(kNvsConApSubnet, tConfig.Connection.ApSubnet);
      tPutString(kNvsConFallbackApSsid, tFallbackApSsid);
      tPutString(kNvsConFallbackApPass, tFallbackApPassword);
      tPutString(kNvsConFallbackApIp, tFallbackApIp);
      tPutString(kNvsConFallbackApGw, tFallbackApGateway);
      tPutString(kNvsConFallbackApSubnet, tFallbackApSubnet);
      tPutString(kNvsConStaSsid, tConfig.Connection.StaSsid);
      tPutString(kNvsConStaPass, tConfig.Connection.StaPassword);
      tPutBool(kNvsConStaAutoFallbackApEnable, mConfig.putBool(kNvsConStaAutoFallbackApEnable, tConfig.Connection.StaAutoFallbackApEnable));
      tPutBool(kNvsConStaConnectMaxRetry, mConfig.putUChar(kNvsConStaConnectMaxRetry, tStaConnectMaxRetry));
      tPutBool(kNvsConStaRetryDelayMs, mConfig.putUInt(kNvsConStaRetryDelayMs, tStaRetryDelayMs));
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
      tPutBool(kNvsStgDefaultFs, mConfig.putUChar(kNvsStgDefaultFs, static_cast<uint8_t>(tConfig.Storage.DefaultFileSystem)));
      tPutBool(kNvsStgFallback, mConfig.putBool(kNvsStgFallback, tConfig.Storage.FallbackEnabled));
      tPutString(kNvsDashUser, tConfig.Dashboard.User);
      tPutString(kNvsDashPassword, tConfig.Dashboard.Password);
      tPutString(kNvsDashLanguage, tDashboardLanguage);
      tPutString(kNvsDashEnabledLanguages, tEnabledLanguagesValue);
      tPutString(kNvsDashTheme, tDashboardTheme);
      tPutBool(kNvsDashShowDescription, mConfig.putBool(kNvsDashShowDescription, tConfig.Dashboard.ShowDescription));
      tPutBool(kNvsDashDynamicCpuScaling, mConfig.putBool(kNvsDashDynamicCpuScaling, tConfig.Dashboard.DynamicCpuScaling));
    });
    if (tSuccess) xLOG("Config save successful");
    else xLOG("Config save failed, key → %s", tFailedKey ? tFailedKey : "unknown");
    return tSuccess;
  }

} 
