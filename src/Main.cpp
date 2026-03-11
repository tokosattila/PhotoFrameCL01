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
      if (psramFound()) heap_caps_malloc_extmem_enable(512);
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
      UTL.DisableBrownout();
      UTL.DisableBT();
      UTL.DisableTouchPad();
      if (RTC.Init() && RTC.IsAvailable()) {
        RTC.SyncToSystem();
        RTC.End();
        char tBuf[24];
        xLOG("Date/Time → %s", UTL.EpochToReadableFormat(time(nullptr), true, tBuf, sizeof(tBuf)));
      } else RTC.End();
      BAT.Init(true);
      const bool tBatteryIsAvailable = BAT.IsAvailable() && BAT.IsBatteryConnected() && BAT.GetPercentage() == 0;
      if (tBatteryIsAvailable) {
        LowBatteryMode();
        return;
      }
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
      FWU.CleanupUpdateDirOnBoot();
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

    void ShowDefaultImage() {
      DSP.PrintImage(0, 0, DefaultImageWidth, DefaultImageHeight, DefaultImage);
      DSP.Update();
    }

    void SaveNextImage(const char *tNextImage) {
      if (!CFG.SaveImageName(tNextImage)) xLOG("Failed to save → next image name");
      else xLOG("Display → next image: %s", tNextImage);
    }

    bool TryDisplayImage(const char *tImage) {
      if (!tImage || *tImage == '\0') return false;
      char tFullPath[128] = "";
      snprintf(tFullPath, sizeof(tFullPath), "/%s/%s", mCfg.Display.ImagesDir.c_str(), tImage);
      if (!STG.Exists(tFullPath)) return false;
      xLOG("Display → trying image: %s", tImage);
      return DSP.PrintJpg(0, 0, tImage);
    }

    void PhotoFrameMode() {
      ReloadConfig();
      UTL.PrintInfo("Device → starts in Photo Frame Mode", EUtilsInfoType::Single);
      STG.Init(true);
      DSP.Init();
      DSP.SetRotate(EDisplayRotate::Rotate0);
      const char *tImage = mCfg.Display.CurrentFile.isEmpty() ? STG.GetNextFile("")  : mCfg.Display.CurrentFile.c_str();
      if (TryDisplayImage(tImage)) SaveNextImage(STG.GetNextFile(tImage));
      else {
        xLOG("Image failed → %s", tImage ? tImage : "(null)");
        const char *tNextImage = STG.GetNextFile(tImage ? tImage : "");
        if (TryDisplayImage(tNextImage)) SaveNextImage(STG.GetNextFile(tNextImage));
        else {
          xLOG("Displaying default image.");
          ShowDefaultImage();
          if (!CFG.SaveImageName("")) xLOG("Failed to clear image name.");
          else xLOG("Image name cleared.");
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
      UTL.SetCPUFrequency(ECPUFrequency::F240MHz);
      ReloadConfig();
      LED.On(mCfg.Device.ActLedPin);
      {
        char tText[45] = "";
        snprintf(tText, sizeof(tText), "Device → starts in Maintenance [%s] Mode", (mCfg.Connection.ApModeEnable ? "AP" : "STA"));
        UTL.PrintInfo(tText, EUtilsInfoType::Single);
      }
      STG.Init(true);
      DSP.Init();
      DSP.SetRotate(EDisplayRotate::Rotate0);
      CON.Init(true);
      vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      xLOG_FLUSH();
      if (!sButtonTaskStarted) {
        BTN.AddPin(mCfg.Device.SettingPin, "[settings button]", false);
        BTN.AddPin(mCfg.Device.ResetPin, "[reset button]", true);
        BTN.Start();
        xTaskCreatePinnedToCore(&ButtonTask, "ButtonTask", BUTTON_TASK_STACK_SIZE, nullptr, 12, nullptr, 1);
        sButtonTaskStarted = true;
      }
      BTN.AddLongPressCallback(mCfg.Device.SettingPin, []() {
        #if !PRODUCTION
          xLOG("Device → rebooting...");
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
          xLOG("Device → factory reset...");
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
        DSP.Update();
      }
      while (!CON.HasActiveWifiClient()) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
      if (tIsApMode) DSP.FillRect(0, 300, mCfg.Display.Width, 50, EDisplayColor::White);
      DSP.SetFont(&OpenSans13B);
      DSP.SetColor(EDisplayColor::Black);
      if (mCfg.Connection.MdnsEnable) {
        snprintf(tTitleBuffer, sizeof(tTitleBuffer), "localhost %s.local", mCfg.Connection.MdnsName.c_str());
        DSP.WriteText(0, tInfoTextY, tTitleBuffer, EDisplayHAlignment::Center);
      }
      DSP.Update();
      UTL.PrintMemoryInfo();
      while (true) vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
    }

    void LowBatteryMode() {
      LED.Off(mCfg.Device.ActLedPin);
      UTL.PrintInfo("Device → starts in Low Battery Mode", EUtilsInfoType::Single);
      DSP.Init();
      DSP.SetRotate(EDisplayRotate::Rotate0);
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
        while (true) vTaskDelay(1e3 / portTICK_PERIOD_MS);
      #endif     
    }
    
    static void ButtonTask(void *tParameter) {
      while (true) {
        BTN.HandleEvents();
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
      }
    }
};

bool Application::sButtonTaskStarted = false;

#define APP Application::Instance()

void setup() {
  APP.Init();
};

void loop() {
  APP.Run();
};