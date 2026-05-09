#ifndef GLOBAL_H
#define GLOBAL_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_partition.h>
#include <esp_bt.h>
#include <esp_adc_cal.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <driver/touch_pad.h>
#include <driver/i2s.h>
#include <nvs_flash.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <strings.h>
#include <vector>
#include <mbedtls/sha256.h>
#include <pgmspace.h>
#include <Arduino.h>
#include <USB.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <EPaperDriver.h>
#include <JPEGDEC.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace App {

  #define PRODUCTION true
  
  #define DEFINE_TAG(tTag) static constexpr const char *TAG = tTag

  #if PRODUCTION
    #define xLOG_S
    #define xLOG_FLUSH()
    #define xLOG_B(...)
    #define xLOG_PR(...)
    #define xLOG_PL(...)
    #define xLOG_PR_(...)
    #define xLOG_PL_(...)
    #define xLOG_PF(...)
    #define xLOG(...)
    #define xLOG_E()
  #else
    #define xLOG_S USBSerial
    #define xLOG_FLUSH() USBSerial.flush()
    #define xLOG_B(...) USBSerial.begin(__VA_ARGS__)
    #define xLOG_PR(...) USBSerial.print(__VA_ARGS__)
    #define xLOG_PL(...) USBSerial.println(__VA_ARGS__)
    #define xLOG_PR_(...) USBSerial.print(F(__VA_ARGS__))
    #define xLOG_PL_(...) USBSerial.println(F(__VA_ARGS__))
    #define xLOG_PF(...) USBSerial.printf(__VA_ARGS__)
    #define xLOG(...) do { \
      constexpr const char *tTag = TAG ? TAG : "[APP]"; \
      char tBuffer[256] = ""; \
      snprintf(tBuffer, sizeof(tBuffer), __VA_ARGS__); \
      USBSerial.printf("[%s] %s\n", tTag, tBuffer); \
    } while(0)
    #define xLOG_E() USBSerial.end()
  #endif

  using FDefaultCallback = std::function<void()>;
  using FConnectionCallback = FDefaultCallback;

  enum class ECPUFrequency : uint8_t {
    F80MHz = 80,
    F160MHz = 160,
    F240MHz = 240
  };

  enum class EDevicePins : uint8_t {
    Btn1 = 4U,
    Btn2 = 0U,
    Btn3 = 5U,
    ActLedPin = 42U,
    I2CSdaPin = 47U,
    I2CSclPin = 48U,
    I2SMclkPin = 14U,
    I2SWsPin = 16U,
    I2SBclkPin = 15U,
    I2SDoutPin = 17U,
    CodecPaPin = 7U,
    MisoPin = 40U,
    MosiPin = 41U,
    SckPin = 39U,
    CsPin = 38U
  };

  enum class EFileSystemType : uint8_t {
    LittleFS = 1,
    SDCard
  };

  enum class ETimerWakeUp : uint8_t {
    Minutes = 1,
    Hourly,
    HalfDay,
    Daily,
    Weekly,
    Monthly
  };

  template<typename T>
  class AutoGuard {
    public:
      AutoGuard() { T::Lock(); }
      ~AutoGuard() { T::Unlock(); }
      AutoGuard(const AutoGuard&) = delete;
      AutoGuard& operator=(const AutoGuard&) = delete;
  };

  class I2CBusGuard {
    public:
      I2CBusGuard() {
        SemaphoreHandle_t tMutex = Mutex();
        if (tMutex) xSemaphoreTakeRecursive(tMutex, portMAX_DELAY);
      }
      ~I2CBusGuard() {
        SemaphoreHandle_t tMutex = Mutex();
        if (tMutex) xSemaphoreGiveRecursive(tMutex);
      }
      I2CBusGuard(const I2CBusGuard&) = delete;
      I2CBusGuard& operator=(const I2CBusGuard&) = delete;
    private:
      static SemaphoreHandle_t Mutex() {
        static SemaphoreHandle_t sI2CMutex = xSemaphoreCreateRecursiveMutex();
        return sI2CMutex;
      }
  };

  struct SDirEntry {
    char Name[128] = "";
    bool IsDir = false;
    size_t Size = 0;
  };
  
  struct SSound {
    constexpr SSound(uint16_t tFrequencyHz = 0, uint16_t tDurationMs = 0, uint16_t tPauseMs = 0, uint8_t tAmplitudePct = 0) : FrequencyHz(tFrequencyHz), DurationMs(tDurationMs), PauseMs(tPauseMs), AmplitudePct(tAmplitudePct) {}
    uint16_t FrequencyHz;
    uint16_t DurationMs;
    uint16_t PauseMs;
    uint8_t AmplitudePct;
  };  

  struct SDeviceConfig {
    String Name;
    String Version;
    bool SoundEnabled = true;
    bool LogManagerEnabled = true;
    uint8_t NextImgPin = 0;
    uint8_t ResetPin = 0;
    uint8_t SettingPin = 0;
    uint8_t ActLedPin = 0;
    SDeviceConfig() = default;
  };

  struct SConnectionConfig {
    bool ApModeEnable = false;
    String ApSsid;
    String ApPassword;
    String ApIp;
    String ApGateway;
    String ApSubnet;
    String FallbackApSsid;
    String FallbackApPassword;
    String FallbackApIp;
    String FallbackApGateway;
    String FallbackApSubnet;
    String StaSsid;
    String StaPassword;
    bool StaAutoFallbackApEnable = true;
    uint8_t StaConnectMaxRetry = 3;
    uint32_t StaRetryDelayMs = 5 * 1000;
    bool StaIpEnable = false;
    String StaIp;
    String StaGateway;
    String StaSubnet;
    String StaPrimaryDns;
    String StaSecondaryDns;
    bool MdnsEnable = false;
    String MdnsName;
    SConnectionConfig() = default;
  };

  struct SNTPConfig {
    String Server;
    uint16_t NtpPort = 123;
    long GMTOffset = 0;
    long DaylightOffset = 0;
    String TimeZoneLabel;
    bool LowPowerSyncEnable = true;
    unsigned long LowPowerSyncIntervalSec = 7UL * 24UL * 60UL * 60UL;
    unsigned long LastSuccessfulSyncEpochUtc = 0;
    SNTPConfig() = default;
  };

  constexpr uint8_t DISPLAY_JPG_BRIGHTNESS = 100;
  constexpr uint8_t DISPLAY_JPG_CONTRAST = 100;
  constexpr uint8_t DISPLAY_JPG_GAMMA = 100;
  constexpr uint8_t DISPLAY_JPG_SATURATION = 100;
  constexpr uint8_t DISPLAY_JPG_RED_GAIN = 100;
  constexpr uint8_t DISPLAY_JPG_GREEN_GAIN = 100;
  constexpr uint8_t DISPLAY_JPG_BLUE_GAIN = 100;

  struct SDisplayConfig {
    int32_t Width = 0;
    int32_t Height = 0;
    uint16_t Rotate = 0;
    uint8_t JpgBrightness = DISPLAY_JPG_BRIGHTNESS;
    uint8_t JpgContrast = DISPLAY_JPG_CONTRAST;
    uint8_t JpgGamma = DISPLAY_JPG_GAMMA;
    uint8_t JpgSaturation = DISPLAY_JPG_SATURATION;
    uint8_t JpgRedGain = DISPLAY_JPG_RED_GAIN;
    uint8_t JpgGreenGain = DISPLAY_JPG_GREEN_GAIN;
    uint8_t JpgBlueGain = DISPLAY_JPG_BLUE_GAIN;
    String ImagesDir;
    String ImageExt;
    String CurrentFile;
    unsigned long ImageUpdatedAt = 0;
    SDisplayConfig() = default;
  };

  struct STimerConfig {
    ETimerWakeUp WakeUp;
    uint8_t WakeUpHour = 6;
    STimerConfig() = default;
  };

  struct SStorageConfig {
    EFileSystemType DefaultFileSystem = EFileSystemType::SDCard;
    bool FallbackEnabled = true;
    SStorageConfig() = default;
  };

  struct SDashboardConfig {
    String User;
    String Password;
    String Language;
    std::vector<String> EnabledLanguages;
    String Theme = "light";
    bool ShowDescription = true;
    bool DynamicCpuScaling = true;
    uint16_t TargetWidth = 0;
    uint16_t TargetHeight = 0;
    uint16_t ThumbWidth = 0;
    uint16_t ThumbHeight = 0;
    uint16_t Rotate = 0;
    String ImageExt;
    SDashboardConfig() = default;
  };

  struct SAppConfig {
    SDeviceConfig Device {};
    SNTPConfig Ntp {};
    SConnectionConfig Connection {};
    SDisplayConfig Display {};
    STimerConfig Timer {};
    SStorageConfig Storage {};
    SDashboardConfig Dashboard {};
    SAppConfig() = default;
  };

  constexpr unsigned long BAUDRATE = 115200;

  constexpr const char *IMAGES_DIR = "images";
  constexpr const char *FIRMWARE_FILENAME = "firmware.bin";

  constexpr EFileSystemType STORAGE_DEFAULT = EFileSystemType::SDCard;
  constexpr bool STORAGE_FALLBACK_ENABLED = true;

  constexpr uint16_t DISPLAY_WIDTH = 800;
  constexpr uint16_t DISPLAY_HEIGHT = 480;
  constexpr uint16_t DISPLAY_ROTATE = 180;
  constexpr const char *IMAGE_EXT = ".jpg";

  constexpr uint16_t DASHBOARD_IMG_WIDTH = DISPLAY_WIDTH;
  constexpr uint16_t DASHBOARD_IMG_HEIGHT = DISPLAY_HEIGHT;
  constexpr uint16_t DASHBOARD_IMG_THUMB_WIDTH = 200;
  constexpr uint16_t DASHBOARD_IMG_THUMB_HEIGHT = 200;
  constexpr uint16_t DASHBOARD_IMG_ROTATE = DISPLAY_ROTATE;
  constexpr float DASHBOARD_IMG_JPEG_QUALITY = 0.95f;
  constexpr const char *DASHBOARD_IMG_EXT = IMAGE_EXT;

  constexpr uint8_t NEXT_IMG_PIN = static_cast<uint8_t>(EDevicePins::Btn1);
  constexpr uint8_t RESET_PIN = static_cast<uint8_t>(EDevicePins::Btn2);
  constexpr uint8_t SETTING_PIN = static_cast<uint8_t>(EDevicePins::Btn3);
  constexpr uint8_t ACT_LED_PIN = static_cast<uint8_t>(EDevicePins::ActLedPin);

  static constexpr uint8_t SOUND_CODEC_ADDRESS = 0x18;
  static constexpr uint32_t SOUND_SAMPLE_RATE = 24000;
  constexpr uint8_t SOUND_I2S_MCLK_PIN = static_cast<uint8_t>(EDevicePins::I2SMclkPin);
  constexpr uint8_t SOUND_I2S_WS_PIN = static_cast<uint8_t>(EDevicePins::I2SWsPin);
  constexpr uint8_t SOUND_I2S_BCLK_PIN = static_cast<uint8_t>(EDevicePins::I2SBclkPin);
  constexpr uint8_t SOUND_I2S_DOUT_PIN = static_cast<uint8_t>(EDevicePins::I2SDoutPin);
  constexpr uint8_t SOUND_CODEC_PA_PIN = static_cast<uint8_t>(EDevicePins::CodecPaPin);
  static constexpr uint8_t SOUND_CODEC_BOARD_INIT_PIN = 45U;
  static constexpr bool SOUND_CODEC_PA_ACTIVE_HIGH = true;
  static constexpr bool SOUND_CODEC_CLOCK_FROM_BCLK = false;
  static constexpr uint8_t SOUND_MASTER_VOLUME_PCT = 25;
  constexpr uint8_t SOUND_CODEC_I2C_SDA_PIN = static_cast<uint8_t>(EDevicePins::I2CSdaPin);
  constexpr uint8_t SOUND_CODEC_I2C_SCL_PIN = static_cast<uint8_t>(EDevicePins::I2CSclPin);
  static constexpr i2s_port_t SOUND_I2S_PORT = I2S_NUM_0;
  
  static constexpr uint8_t RTC_ADDRESS = 0x51;
  constexpr uint8_t RTC_SDA_PIN = static_cast<uint8_t>(EDevicePins::I2CSdaPin);
  constexpr uint8_t RTC_SCL_PIN = static_cast<uint8_t>(EDevicePins::I2CSclPin);

  static constexpr uint8_t POWER_OUTPUT_ADDRESS = 0x34;
  constexpr uint8_t AXP_SDA_PIN = static_cast<uint8_t>(EDevicePins::I2CSdaPin);
  constexpr uint8_t AXP_SCL_PIN = static_cast<uint8_t>(EDevicePins::I2CSclPin);

  constexpr uint8_t SD_MISO_PIN = static_cast<uint8_t>(EDevicePins::MisoPin);
  constexpr uint8_t SD_MOSI_PIN = static_cast<uint8_t>(EDevicePins::MosiPin);
  constexpr uint8_t SD_SCK_PIN = static_cast<uint8_t>(EDevicePins::SckPin);
  constexpr uint8_t SD_CS_PIN = static_cast<uint8_t>(EDevicePins::CsPin);
  
  constexpr uint32_t SECONDS_PER_MINUTE = 60;
  constexpr uint32_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
  constexpr uint32_t SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

  constexpr uint32_t KB = 1024;
  constexpr size_t LOOP_TASK_STACK_SIZE = 48 * KB;
  constexpr size_t BUTTON_TASK_STACK_SIZE = 16 * KB;
  constexpr size_t JPEG_DECODE_TASK_STACK_SIZE = 32 * KB;
  constexpr size_t DASHBOARD_TASK_STACK_SIZE = 24 * KB;

  constexpr size_t ONE_SECOND_MS = 1000;
  constexpr uint32_t REBOOT_LONG_PRESS_MS = 3 * ONE_SECOND_MS;
  constexpr uint32_t FACTORY_RESET_LONG_PRESS_MS = 30 * ONE_SECOND_MS;

  constexpr uint32_t DELAY_ULTRA_SHORT_MS = ONE_SECOND_MS / 100;
  constexpr uint32_t DELAY_SHORT_MS = ONE_SECOND_MS / 10;
  constexpr uint32_t DELAY_MEDIUM_MS = ONE_SECOND_MS / 5;
  constexpr uint32_t DELAY_HALF_SEC_MS = ONE_SECOND_MS / 2;
  constexpr uint32_t DELAY_ONE_SEC_MS = ONE_SECOND_MS;

  constexpr uint32_t MAINTENANCE_INACTIVITY_TIMEOUT_MS = 5 * 60 * ONE_SECOND_MS;

  constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30 * ONE_SECOND_MS;
  constexpr uint32_t WIFI_RETRY_COUNT = 20;
  constexpr uint32_t NVS_RETRY_DELAY_MS = ONE_SECOND_MS / 10;
  constexpr uint32_t CONFIG_RETRY_DELAY_MS = ONE_SECOND_MS;

}

#include <App/Utils.h>
#include <App/Configuration.h>
#include <App/Led.h>
#include <App/Button.h>
#include <App/Storages/LittleFS.h>
#include <App/Storages/SDCard.h>
#include <App/Storage.h>
#include <App/NTP.h>
#include <App/RTC.h>
#include <App/Battery.h>
#include <App/Connection.h>
#include <App/Firmware.h>
#include <App/Sound.h>
#include <App/Sounds/LowBatterySound.h>
#include <App/Sounds/MaintenanceSound.h>
#include <App/Display.h>
#include <App/Fonts/OpenSans11.h>
#include <App/Fonts/OpenSans11b.h>
#include <App/Fonts/OpenSans13.h>
#include <App/Fonts/OpenSans13b.h>
#include <App/Images/DefaultImage.h>
#include <App/Dashboard.h>
#include <App/LogManager.h>

#define CFG Configuration_::Instance()
#define UTL Utils_::Instance()
#define BTN Button_::Instance()
#define LED Led_::Instance()
#define LFS LittleFS_::Instance()
#define SDC SDCard_::Instance()
#define STG Storage_::Instance()
#define NTP NTP_::Instance()
#define RTC RTC_::Instance()
#define BAT Battery_::Instance()
#define SND Sound_::Instance()
#define CON Connection_::Instance()
#define DSP Display_::Instance()
#define FWU Firmware_::Instance()
#define DSH Dashboard_::Instance()
#define LGM LogManager_::Instance()

#endif