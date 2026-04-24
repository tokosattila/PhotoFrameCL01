#ifndef CONNECTION_H
#define CONNECTION_H

#include <App/Global.h>

namespace App {

  class Connection_ {
    DEFINE_TAG("CON");
    friend class AutoGuard<Connection_>;
    static constexpr uint32_t kConTaskStack = 12 * 1024;
    public:
      using Guard = AutoGuard<Connection_>;
      static Connection_ &Instance();
      void Init(bool tVerbose = false);
      void ReloadConfig();
      void Start();
      void Stop();
      const char *GetIpAddress();
      bool HasActiveWifiClient() const;
      void Callback(FConnectionCallback tCallback);
      bool TryConnectApSta();
    private:
      Connection_();
      Connection_(const Connection_&) = delete;
      Connection_ &operator=(const Connection_&) = delete;
      ~Connection_();
      static WiFiClass *mWiFi;
      MDNSResponder mMDNS;
      FConnectionCallback mCallback = nullptr;
      mutable SemaphoreHandle_t mMutex = nullptr;
      static TaskHandle_t mTaskHandle;
      static volatile bool sStopWiFiEventTask;
      SAppConfig mCfg {};
      static void Lock();
      static void Unlock();
      static void WiFiEventTask(void *tParameter);
      void SetupAp();
        bool ConnectSta();
        bool TryConnectStaWithRetry();
        void SwitchToFallbackApMode(bool tPersistConfig);
      void StartMdns();
      void PrintConnectionInfo();
      void BootstrapVault();
  };

}

#endif
