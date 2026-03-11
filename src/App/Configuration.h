#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <App/Global.h>

namespace App {

  class Configuration_ {
    DEFINE_TAG("CFG");
    friend class AutoGuard<Configuration_>;
    public:
      using Guard = AutoGuard<Configuration_>;
      static Configuration_ &Instance();
      bool Init();
      template<typename T> T Get();
      bool CreateConfig();
      bool FactoryReset();
      bool SaveImageName(const char *tValue);
      uint32_t GetImageUpdatedAt();
      bool SaveAllConfig(const SAppConfig &tConfig);
    private:
      Configuration_();
      Configuration_(const Configuration_&) = delete;
      Configuration_ &operator=(const Configuration_&) = delete;
      ~Configuration_();
      Preferences mConfig;
      const char *mLabel = "cfg";
      const char *mPartLabel = "nvs";
      mutable SemaphoreHandle_t mMutex;
      static void Lock();
      static void Unlock();
      static constexpr const char *kNvsDeviceConfig = "dvc.cfg";
      static constexpr const char *kNvsDeviceAppName = "dvc.appname";
      static constexpr const char *kNvsDeviceVersion = "dvc.version";
      static constexpr const char *kNvsDeviceActLedPin = "dvc.led.act";
      static constexpr const char *kNvsDisplayBrightness = "dsp.jpg.brght";
      static constexpr const char *kNvsDisplayContrast = "dsp.jpg.cntrst";
      static constexpr const char *kNvsDisplayGamma = "dsp.jpg.gmm";
      static constexpr const char *kNvsDisplayFile = "dsp.file";
      static constexpr const char *kNvsDisplayImageUpdatedAt = "dsp.file.upd";
      static constexpr const char *kNvsTimeServer = "tme.server";
      static constexpr const char *kNvsTimePort = "tme.port";
      static constexpr const char *kNvsTimeGmtOffset = "tme.gmt.offset";
      static constexpr const char *kNvsTimeUpdate = "tme.update";
      static constexpr const char *kNvsConApEnable = "con.ap.en";
      static constexpr const char *kNvsConApSsid = "con.ap.ssid";
      static constexpr const char *kNvsConApPass = "con.ap.pass";
      static constexpr const char *kNvsConApIp = "con.ap.ip";
      static constexpr const char *kNvsConApGw = "con.ap.gw";
      static constexpr const char *kNvsConApSubnet = "con.ap.subnet";
      static constexpr const char *kNvsConStaSsid = "con.sta.ssid";
      static constexpr const char *kNvsConStaPass = "con.sta.pass";
      static constexpr const char *kNvsConStaEnable = "con.sta.en";
      static constexpr const char *kNvsConStaIp = "con.sta.ip";
      static constexpr const char *kNvsConStaGw = "con.sta.gw";
      static constexpr const char *kNvsConStaSubnet = "con.sta.subnet";
      static constexpr const char *kNvsConStaDns1 = "con.sta.dns1";
      static constexpr const char *kNvsConStaDns2 = "con.sta.dns2";
      static constexpr const char *kNvsConMdnsEnable = "con.mdns.en";
      static constexpr const char *kNvsConMdnsName = "con.mdns.name";
      static constexpr const char *kNvsTimerWake = "tmr.wake";
      static constexpr const char *kNvsTimerWakeHour = "tmr.wake.hr";
      static SAppConfig GetDefaultConfig();
      bool Begin(bool tReadOnly);
      void End();
      void AccessConfig(bool tReadOnly, std::function<void()> tAction);
  };

  template<> SAppConfig Configuration_::Get<SAppConfig>();
  template<> SDeviceConfig Configuration_::Get<SDeviceConfig>();
  template<> SConnectionConfig Configuration_::Get<SConnectionConfig>();
  template<> SNTPConfig Configuration_::Get<SNTPConfig>();
  template<> SDisplayConfig Configuration_::Get<SDisplayConfig>();
  template<> STimerConfig Configuration_::Get<STimerConfig>();
  template<> SStorageConfig Configuration_::Get<SStorageConfig>();

};

#endif
