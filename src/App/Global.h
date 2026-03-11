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
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <driver/touch_pad.h>
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
#include <strings.h>
#include <vector>
#include <mbedtls/sha256.h>
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

namespace App {

  #define PRODUCTION false
  
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
    RTCSdaPin = 47U,
    RTCSclPin = 48U,
    AXPSdaPin = 47U,
    AXPSclPin = 48U,
    SDMisoPin = 40U,
    SDMosiPin = 41U,
    SDSckPin = 39U,
    SDCsPin = 38U,
  };

  enum class EFileSystemType : uint8_t {
    LittleFS = 1,
    SDCard
  };

  enum class ETimerWakeUp : uint8_t {
    Seconds = 1,
    Minutes,
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

  class Percentage {
    public:
      constexpr explicit Percentage(uint8_t tValue = 0) : mValue(tValue) {}
      constexpr uint8_t Get() const { return mValue; }
      constexpr operator uint8_t() const { return mValue; }
      bool operator==(const Percentage& tOther) const { return mValue == tOther.mValue; }
      bool operator!=(const Percentage& tOther) const { return mValue != tOther.mValue; }
    private:
      uint8_t mValue;
  };

  class Port {
    public:
      constexpr explicit Port(uint16_t tValue = 0) : mValue(tValue == 0 ? 1 : tValue) {}
      constexpr uint16_t Get() const { return mValue; }
      constexpr operator uint16_t() const { return mValue; }
      bool IsValid() const { return mValue > 0; }
      bool operator==(const Port& tOther) const { return mValue == tOther.mValue; }
      bool operator!=(const Port& tOther) const { return mValue != tOther.mValue; }
    private:
      uint16_t mValue;
  };

  struct SDirEntry {
    char Name[128] = "";
    bool IsDir = false;
    size_t Size = 0;
  };  

  struct SDeviceConfig {
    String Name;
    String Version;
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
    String StaSsid;
    String StaPassword;
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
    Port NtpPort {123};
    unsigned long GMTOffset = 0;
    unsigned long UpdateInterval = 0;
    SNTPConfig() = default;
  };

  struct SDisplayConfig {
    int32_t Width = 0;
    int32_t Height = 0;
    Percentage JpgBrightness {25};
    Percentage JpgContrast {75};
    Percentage JpgGamma {125};
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
    EFileSystemType DefaultFileSystem = EFileSystemType::LittleFS;
    bool FallbackEnabled = true;
    SStorageConfig() = default;
  };

  struct SAppConfig {
    SDeviceConfig Device {};
    SNTPConfig Ntp {};
    SConnectionConfig Connection {};
    SDisplayConfig Display {};
    STimerConfig Timer {};
    SStorageConfig Storage {};
    SAppConfig() = default;
  };

  constexpr unsigned long BAUDRATE = 115200;

  constexpr const char *IMAGES_DIR = "images";

  constexpr const char *FIRMWARE_DIR = "/firmware";
  constexpr const char *FIRMWARE_PATH = "/firmware/firmware.bin";
  constexpr const char *FIRMWARE_SHA_PATH = "/firmware/firmware.sha256";

  constexpr EFileSystemType DEFAULT_FILE_SYSTEM = EFileSystemType::SDCard;
  constexpr bool STORAGE_FALLBACK_ENABLED = true;

  constexpr uint16_t DISPLAY_WIDTH = 800;
  constexpr uint16_t DISPLAY_HEIGHT = 480;
  constexpr const char *IMAGE_EXT = ".jpg";

  constexpr uint8_t NEXT_IMG_PIN = static_cast<uint8_t>(EDevicePins::Btn1);
  constexpr uint8_t RESET_PIN = static_cast<uint8_t>(EDevicePins::Btn2);
  constexpr uint8_t SETTING_PIN = static_cast<uint8_t>(EDevicePins::Btn3);
  constexpr uint8_t ACT_LED_PIN = static_cast<uint8_t>(EDevicePins::ActLedPin);
  
  static constexpr uint8_t RTC_ADDRESS = 0x51;
  constexpr uint8_t RTC_SDA_PIN = static_cast<uint8_t>(EDevicePins::RTCSdaPin);
  constexpr uint8_t RTC_SCL_PIN = static_cast<uint8_t>(EDevicePins::RTCSclPin);

  static constexpr uint8_t POWER_OUTPUT_ADDRESS = 0x34;
  constexpr uint8_t AXP_SDA_PIN = static_cast<uint8_t>(EDevicePins::AXPSdaPin);
  constexpr uint8_t AXP_SCL_PIN = static_cast<uint8_t>(EDevicePins::AXPSclPin);

  constexpr uint8_t SD_MISO_PIN = static_cast<uint8_t>(EDevicePins::SDMisoPin);
  constexpr uint8_t SD_MOSI_PIN = static_cast<uint8_t>(EDevicePins::SDMosiPin);
  constexpr uint8_t SD_SCK_PIN = static_cast<uint8_t>(EDevicePins::SDSckPin);
  constexpr uint8_t SD_CS_PIN = static_cast<uint8_t>(EDevicePins::SDCsPin);
  
  constexpr uint32_t SECONDS_PER_MINUTE = 60;
  constexpr uint32_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
  constexpr uint32_t SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

  constexpr uint32_t KB = 1024;
  constexpr size_t LOOP_TASK_STACK_SIZE = 48 * KB;
  constexpr size_t BUTTON_TASK_STACK_SIZE = 16 * KB;
  constexpr size_t JPEG_DECODE_TASK_STACK_SIZE = 32 * KB;

  constexpr size_t ONE_SECOND_MS = 1000;
  constexpr uint32_t REBOOT_LONG_PRESS_MS = 3 * ONE_SECOND_MS;
  constexpr uint32_t FACTORY_RESET_LONG_PRESS_MS = 30 * ONE_SECOND_MS;

  constexpr uint32_t DELAY_ULTRA_SHORT_MS = ONE_SECOND_MS / 100;
  constexpr uint32_t DELAY_SHORT_MS = ONE_SECOND_MS / 10;
  constexpr uint32_t DELAY_MEDIUM_MS = ONE_SECOND_MS / 5;
  constexpr uint32_t DELAY_HALF_SEC_MS = ONE_SECOND_MS / 2;
  constexpr uint32_t DELAY_ONE_SEC_MS = ONE_SECOND_MS;

  constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30 * ONE_SECOND_MS;
  constexpr uint32_t WIFI_RETRY_COUNT = 20;
  constexpr uint32_t NVS_RETRY_DELAY_MS = ONE_SECOND_MS / 10;
  constexpr uint32_t CONFIG_RETRY_DELAY_MS = ONE_SECOND_MS;

  extern RTC_DATA_ATTR uint32_t gBootCount;

}

#include <App/Utils.h>
#include <App/Configuration.h>
#include <App/Storages/LittleFS.h>
#include <App/Storages/SDCard.h>
#include <App/Storage.h>
#include <App/NTP.h>
#include <App/RTCTime.h>
#include <App/Battery.h>
#include <App/Connection.h>
#include <App/Button.h>
#include <App/Led.h>
#include <App/Display.h>
#include <App/Firmware.h>

#include <Fonts/OpenSans11.h>
#include <Fonts/OpenSans11b.h>
#include <Fonts/OpenSans13.h>
#include <Fonts/OpenSans13b.h>

#include <Images/DefaultImage.h>

#define CFG Configuration_::Instance()
#define UTL Utils_::Instance()
#define LFS LittleFS_::Instance()
#define SDC SDCard_::Instance()
#define STG Storage_::Instance()
#define NTP NTP_::Instance()
#define RTC RTCTime_::Instance()
#define BAT Battery_::Instance()
#define CON Connection_::Instance()
#define BTN Button_::Instance()
#define LED Led_::Instance()
#define DSP Display_::Instance()
#define FWU Firmware_::Instance()

#endif