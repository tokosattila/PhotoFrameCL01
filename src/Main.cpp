#include <App/Global.h>

namespace App {
  RTC_DATA_ATTR uint32_t gBootCount = 0;
  #if !PRODUCTION
    RTC_NOINIT_ATTR uint32_t gMaintenanceBootRequest = 0;
    constexpr uint32_t kMaintenanceBootMagic = 0x4D41494E;
  #endif
}

using namespace App;

SET_LOOP_TASK_STACK_SIZE(LOOP_TASK_STACK_SIZE);

class Application {

  DEFINE_TAG("APP");
  friend class AutoGuard<Application>;
  static bool sButtonTaskStarted;
  static bool sDashboardTaskStarted;

  public:

    using Guard = AutoGuard<Application>;

    static Application &Instance() {
      static Application tInstance;
      return tInstance;
    };

    void Init() {     
      #if !PRODUCTION
        xLOG_B(BAUDRATE);
        unsigned long tStart = millis();
        while (!xLOG_S && (millis() - tStart) < 5e3) vTaskDelay(10 / portTICK_PERIOD_MS);
        vTaskDelay(3e3 / portTICK_PERIOD_MS);
        xLOG_PL();
        xLOG_FLUSH();
        {
          Guard tLock;
          UTL.PrintPartitionInfo();
        }
      #endif
      if (psramFound()) heap_caps_malloc_extmem_enable(1024);
      if (!mMutex) mMutex = xSemaphoreCreateRecursiveMutex();
      gBootCount++;
      if(!CFG.Init()) return;
      ReloadConfig();
      UTL.Init();
      UTL.SetCPUFrequency(ECPUFrequency::F160MHz);
      #if !PRODUCTION
        { 
          Guard tLock; 
          UTL.PrintDeviceInfo(); 
        }
      #endif
      UTL.DisableBT();
      if (RTC.Init(true) && RTC.IsAvailable()) {
        RTC.SyncToSystem();
        RTC.End();
      } else RTC.End();
      BAT.Init(true);
      const bool tBatteryAvailable = BAT.IsAvailable();
      const bool tBatteryConnected = tBatteryAvailable && BAT.IsBatteryConnected();
      if (!tBatteryConnected) UTL.DisableBrownout();
      SND.Init(true);
      SND.SetVolume(80);
      #if PRODUCTION
        const bool tLowBatteryDetected = BAT.IsLowBattery();
        if (tLowBatteryDetected) {
          LowBatteryMode();
          return;
        }
      #endif
      LED.AddPin(mCfg.Device.ActLedPin, "[act led]");
      LED.Off(mCfg.Device.ActLedPin);
      #if !PRODUCTION
        BTN.AddPin(mCfg.Device.NextImgPin, "[next image button]", true);
        BTN.AddShortPressCallback(mCfg.Device.NextImgPin, []() {
          esp_restart();
        });
        BTN.AddPin(mCfg.Device.ResetPin, "[reset button]", true);
        BTN.AddPin(mCfg.Device.SettingPin, "[settings button]", false);
        BTN.AddShortPressCallback(mCfg.Device.SettingPin, []() {
          gMaintenanceBootRequest = kMaintenanceBootMagic;
          esp_restart();
        });
        if (!sButtonTaskStarted) {
          BTN.Start();
          xTaskCreatePinnedToCore(&ButtonTask, "ButtonTask", BUTTON_TASK_STACK_SIZE, nullptr, 12, nullptr, 1);
          sButtonTaskStarted = true;
        }
      #endif  
      const uint8_t tSettingPin = static_cast<uint8_t>(mCfg.Device.SettingPin);
      #if PRODUCTION
        if (UTL.WasWokenByPin(tSettingPin)) MaintenanceMode();
        else PhotoFrameMode();
      #else
        const bool tMaintenanceRequested = (gMaintenanceBootRequest == kMaintenanceBootMagic);
        gMaintenanceBootRequest = 0;
        if (tMaintenanceRequested || UTL.WasWokenByPin(tSettingPin)) MaintenanceMode();
        else PhotoFrameMode();
      #endif
    }

    void Run() {
      vTaskDelay(portMAX_DELAY);
    }
    
  private:
    Application() = default;
    SemaphoreHandle_t mMutex = nullptr;
    SAppConfig mCfg {};

    static void Lock() {
      if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
    }

    static void Unlock() {
      if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
    }

    void ReloadConfig() {
      Guard tLock;
      mCfg = CFG.Get<SAppConfig>();
    }

    static EDisplayRotate ResolveDisplayRotate(uint16_t tRotate) {
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

    void ShowDefaultImage() {
      DSP.PrintImage(0, 0, DefaultImageWidth, DefaultImageHeight, DefaultImage);
      DSP.Update();
    }

    void SaveNextImage(const char *tNextImage) {
      if (!CFG.SaveImageName(tNextImage)) xLOG("Failed to save next image name");
      else xLOG("Next image → %s", tNextImage);
    }

    bool TryDisplayImage(const char *tImage) {
      if (!tImage || *tImage == '\0') return false;
      char tFullPath[128] = "";
      snprintf(tFullPath, sizeof(tFullPath), "/%s/%s", mCfg.Display.ImagesDir.c_str(), tImage);
      if (!STG.Exists(tFullPath)) return false;
      xLOG("Trying image → %s", tImage);
      return DSP.PrintJpg(0, 0, tImage);
    }

    void PhotoFrameMode() {
      ReloadConfig();
      UTL.PrintInfo("Device starts in Photo Frame Mode", EUtilsInfoType::Single);
      STG.Init(true);
      DSP.Init();
      DSP.SetRotate(ResolveDisplayRotate(mCfg.Display.Rotate));
      const char *tImage = mCfg.Display.CurrentFile.isEmpty() ? STG.GetNextFile("")  : mCfg.Display.CurrentFile.c_str();
      if (TryDisplayImage(tImage)) SaveNextImage(STG.GetNextFile(tImage));
      else {
        xLOG("Image failed → %s", tImage ? tImage : "(null)");
        const char *tNextImage = STG.GetNextFile(tImage ? tImage : "");
        if (TryDisplayImage(tNextImage)) SaveNextImage(STG.GetNextFile(tNextImage));
        else {
          xLOG("Displaying default image");
          ShowDefaultImage();
          if (!CFG.SaveImageName("")) xLOG("Failed to clear image name");
          else xLOG("Image name cleared");
        }
      }
      UTL.PrintMemoryInfo();
      DSP.OffAll();
      STG.End();
      #if PRODUCTION
        UTL.SleepAndWakeup();
        __builtin_unreachable();
      #else
        while (true) vTaskDelay(1e3 / portTICK_PERIOD_MS);
      #endif
    }

    void MaintenanceMode() {
      ReloadConfig();
      const bool tBatteryConnected = BAT.IsAvailable() && BAT.IsBatteryConnected();
      UTL.SetCPUFrequency(tBatteryConnected ? ECPUFrequency::F240MHz : ECPUFrequency::F240MHz);
      LED.On(mCfg.Device.ActLedPin);
      {
        char tText[45] = "";
        snprintf(tText, sizeof(tText), "Device starts in Maintenance [%s] Mode", (mCfg.Connection.ApModeEnable ? "AP" : "STA"));
        UTL.PrintInfo(tText, EUtilsInfoType::Single);
      }
      STG.Init(true);
      DSP.Init();
      DSP.SetRotate(ResolveDisplayRotate(mCfg.Display.Rotate));
      if (!sButtonTaskStarted) {
        BTN.AddPin(mCfg.Device.ResetPin, "[reset button]", true);
        BTN.AddPin(mCfg.Device.SettingPin, "[settings button]", false);
        BTN.Start();
        xTaskCreatePinnedToCore(&ButtonTask, "ButtonTask", BUTTON_TASK_STACK_SIZE, nullptr, 12, nullptr, 1);
        sButtonTaskStarted = true;
      }
      BTN.AddLongPressCallback(mCfg.Device.SettingPin, []() {
        #if !PRODUCTION
          xLOG("Device rebooting...");
          vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
        #endif
        { 
          Guard tLock;
          DSP.OffAll();
          STG.End();
          CON.Stop();
        }
        esp_restart();
      }, REBOOT_LONG_PRESS_MS);
      BTN.AddLongPressCallback(mCfg.Device.ResetPin, []() {
        #if !PRODUCTION
          xLOG("Device preparing restore factory settings...");
          vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
        #endif
        { 
          Guard tLock;
          CFG.FactoryReset();
          DSP.OffAll();
          STG.End();
          CON.Stop();
        }
        esp_restart();
      }, FACTORY_RESET_LONG_PRESS_MS);
      vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
      char tTitleBuffer[64];
      const bool tIsApMode = mCfg.Connection.ApModeEnable;
      DSP.SetFont(&OpenSans13B);
      DSP.SetColor(EDisplayColor::White);
      snprintf(tTitleBuffer, sizeof(tTitleBuffer), "%s / MAINTENANCE [%s MODE]", mCfg.Device.Name.c_str(), (tIsApMode ? "AP" : "STA"));
      const SBoxBounds tTitleBoxBounds = DSP.WriteTextWithBoxCentered(tTitleBuffer, 12, 8, EDisplayColor::Red);
      const int32_t tInfoTextY = tTitleBoxBounds.YEndPos + 15;
      DSP.SetColor(EDisplayColor::Black);
      if (tIsApMode) { 
        DSP.SetFont(&OpenSans13B);
        snprintf(tTitleBuffer, sizeof(tTitleBuffer), "Connect to Wifi AP Mode to SSID: %s", mCfg.Connection.ApSsid.c_str());
        DSP.WriteText(0, tInfoTextY, tTitleBuffer, EDisplayHAlignment::Center);
      }
      DSP.Update();
      xLOG_FLUSH();
      CON.Init(true);
      vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      DSH.Init([]() {
        DSH.Stop();
        DSP.OffAll();
        STG.End();
        CON.Stop();
        vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
        esp_restart();
      }, []() {
        DSH.Stop();
        DSP.OffAll();
        STG.End();
        CON.Stop();
        CFG.FactoryReset();
        vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
        esp_restart();
      });
      DSH.Start();
      if (!sDashboardTaskStarted) {
        xTaskCreatePinnedToCore(&DashboardTask, "DashboardTask", DASHBOARD_TASK_STACK_SIZE, nullptr, 11, nullptr, 1);
        sDashboardTaskStarted = true;
      }
      bool tMaintenanceSoundPlayed = false;
      const uint32_t tWifiTimeoutMs = 5 * ONE_SECOND_MS;
      const uint32_t tWifiStartMs = millis();
      while (!CON.HasActiveWifiClient() && (millis() - tWifiStartMs) < tWifiTimeoutMs) {
        vTaskDelay(DELAY_MEDIUM_MS / portTICK_PERIOD_MS);
      }
      if (!CON.HasActiveWifiClient()) xLOG("WiFi client connect timeout");
      vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
      if (!tMaintenanceSoundPlayed) {
        tMaintenanceSoundPlayed = SND.Play(kMaintenanceSound);
        xLOG("Sound playback → %s", tMaintenanceSoundPlayed ? "Ok" : "Failed");
      }
      if (tIsApMode) DSP.FillRect(0, tInfoTextY, mCfg.Display.Width, 50, EDisplayColor::White);
      DSP.SetFont(&OpenSans13B);
      DSP.SetColor(EDisplayColor::Black);
      const char *tHost = mCfg.Connection.MdnsEnable ? mCfg.Connection.MdnsName.c_str() : CON.GetIpAddress();
      const char *tHostSuffix = mCfg.Connection.MdnsEnable ? ".local/" : "/";
      snprintf(tTitleBuffer, sizeof(tTitleBuffer), "Admin URL: http://%s%s", tHost ? tHost : "", tHostSuffix);
      DSP.WriteText(0, tInfoTextY, tTitleBuffer, EDisplayHAlignment::Center);
      DSP.Update();
      UTL.PrintMemoryInfo();
      while (true) vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
    }

    void LowBatteryMode() {
      const bool tLowBatterySoundPlayed = SND.Play(kLowBatterySound);
      xLOG("Low battery sound playback → %s", tLowBatterySoundPlayed ? "Ok" : "Failed");
      LED.Off(mCfg.Device.ActLedPin);
      UTL.PrintInfo("Device starts in Low Battery Mode", EUtilsInfoType::Single);
      DSP.Init();
      DSP.SetRotate(ResolveDisplayRotate(mCfg.Display.Rotate));
      char tBuffer[32];
      const int32_t tCanvasWidth = DSP.GetCanvasWidth();
      const int32_t tCanvasHeight = DSP.GetCanvasHeight();
      const int32_t tWidth = 160;
      const int32_t tHeight = 60;
      const int32_t tLayers = 2;
      const int32_t tLevelPadding = 5;
      const int32_t tStartX = (tCanvasWidth - tWidth) / 2;
      const int32_t tStartY = (tCanvasHeight - tHeight) / 2;
      const int32_t tButtonWidth = tWidth / 10 + tLayers;
      const int32_t tButtonHeight = tHeight / 2;
      const int32_t tBodyWidth = tWidth - tButtonWidth;
      const int32_t tBodyHeight = tHeight;     
      for (int32_t tLayer = 0; tLayer < tLayers; tLayer++) {
        int32_t tOffset = tLayer;
        int32_t tBtnX = tStartX + tOffset;
        int32_t tBtnY = tStartY + tOffset + (tHeight - tButtonHeight) / 2;
        int32_t tBtnW = tButtonWidth - 2 * tLayer + tLayers;
        int32_t tBtnH = tButtonHeight - 2 * tLayer;
        DSP.DrawRect(tBtnX, tBtnY, tBtnW, tBtnH, EDisplayColor::Black);
        int32_t tBodyX = tStartX + tOffset + tButtonWidth;
        int32_t tBodyY = tStartY + tOffset;
        int32_t tBodyW = tBodyWidth - 2 * tLayer;
        int32_t tBodyH = tBodyHeight - 2 * tLayer;
        DSP.DrawRect(tBodyX, tBodyY, tBodyW, tBodyH, EDisplayColor::Black);
      }
      int32_t tLevelW = (tBodyWidth - 2 * tLayers - 6 * tLevelPadding) / 5;
      int32_t tLevelH = tBodyHeight - 2 * tLayers - 2 * tLevelPadding;
      int32_t tLevelX = tStartX +  tButtonWidth + tLevelPadding + (tLevelW + tLevelPadding) * 4;
      int32_t tLevelY = tStartY + tLayers + tLevelPadding;
      DSP.FillRect(tLevelX, tLevelY, tLevelW, tLevelH, EDisplayColor::Red);
      DSP.SetFont(&OpenSans13B);
      DSP.SetColor(EDisplayColor::Black);
      snprintf(tBuffer, sizeof(tBuffer), "low battery: %umV [%u%%]", BAT.GetVoltage(), BAT.GetPercentage());
      DSP.WriteText(0, tStartY + tHeight + 15, tBuffer, EDisplayHAlignment::Center, EDisplayVAlignment::Auto, EDisplayColor::White);
      DSP.Update();
      DSP.OffAll();
      UTL.PrintMemoryInfo();
      #if PRODUCTION
        UTL.SleepLowBattery();
        __builtin_unreachable();
      #else
        while (true) vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      #endif     
    }
    
    static void ButtonTask(void *tParameter) {
      while (true) {
        BTN.HandleEvents();
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
      }
    }

    static void DashboardTask(void *tParameter) {
      while (true) {
        DSH.HandleEvents();
        vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      }
    }
};

bool Application::sButtonTaskStarted = false;
bool Application::sDashboardTaskStarted = false;

#define APP Application::Instance()

void setup() {
  APP.Init();
};

void loop() {
  APP.Run();
};