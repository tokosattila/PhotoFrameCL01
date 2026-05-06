#include <App/Connection.h>

namespace App {

  WiFiClass *Connection_::mWiFi = nullptr;
  TaskHandle_t Connection_::mTaskHandle = nullptr;
  volatile bool Connection_::sStopWiFiEventTask = false;
  
  Connection_ &Connection_::Instance() {
    static Connection_ tInstance;
    return tInstance;
  }

  Connection_::Connection_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
    if (!mWiFi) mWiFi = new WiFiClass();
  }

  Connection_::~Connection_() {
    delete mWiFi;
    mWiFi = nullptr;
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Connection_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Connection_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void Connection_::Init(bool tVerbose) {
    ReloadConfig();
    Start();
    if (mCfg.Connection.ApModeEnable) {
      vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      return;
    }
    uint32_t tStart = millis();
    while (true) {
      bool tWifiActive = HasActiveWifiClient();
      if (tWifiActive) {
        BootstrapVault();
        break;
      }
      if (millis() - tStart > WIFI_CONNECT_TIMEOUT_MS) {
        xLOG("Connection timeout, timedate syncronization skipped");
        break;
      }
      vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    }
  }

  void Connection_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
  }

  void Connection_::Start() {
    sStopWiFiEventTask = false;
    if (!mTaskHandle) xTaskCreatePinnedToCore(&WiFiEventTask, "WiFiEventTask", kConTaskStack, this, 1, &mTaskHandle, 1);
  }

  void Connection_::Stop() {
    sStopWiFiEventTask = true;
    if (mTaskHandle) {
      for (uint8_t i = 0; i < 20 && eTaskGetState(mTaskHandle) != eDeleted; ++i) vTaskDelay(DELAY_SHORT_MS / portTICK_PERIOD_MS);
      mTaskHandle = nullptr;
    }
    Guard tLock;
    if (mWiFi) mWiFi->mode(WIFI_OFF);
  }

  const char *Connection_::GetIpAddress() {
    static char sIpBuffer[16] = "0.0.0.0";
    Guard tLock;
    if (mWiFi) {
      if (mWiFi->getMode() & WIFI_AP) {
        IPAddress tIp = mWiFi->softAPIP();
        snprintf(sIpBuffer, sizeof(sIpBuffer), "%d.%d.%d.%d", tIp[0], tIp[1], tIp[2], tIp[3]);
      } else if (mWiFi->getMode() & WIFI_STA && mWiFi->status() == WL_CONNECTED) {
        IPAddress tIp = mWiFi->localIP();
        snprintf(sIpBuffer, sizeof(sIpBuffer), "%d.%d.%d.%d", tIp[0], tIp[1], tIp[2], tIp[3]);
      }
    }
    return sIpBuffer;
  }

  bool Connection_::HasActiveWifiClient() const {
    bool tActive = false;
    Guard tLock;
    if (!mWiFi) return false;
    wifi_mode_t tMode = mWiFi->getMode();
    if (tMode & WIFI_AP) tActive = mWiFi->softAPgetStationNum() > 0;
    else if (tMode & WIFI_STA) tActive = mWiFi->status() == WL_CONNECTED;
    return tActive;
  }

  void Connection_::Callback(FConnectionCallback tCallback) {
    Guard tLock;
    mCallback = tCallback;
  }

  void Connection_::WiFiEventTask(void *tParameter) {
    Connection_ *tSelf = static_cast<Connection_ *>(tParameter);
    bool tLastActive = false;
    bool tMdnsRunning = false;
    bool tStaFailureLogged = false;
    while (!sStopWiFiEventTask) {
      bool tCurrentActive = false;
      if (tSelf->mCfg.Connection.ApModeEnable) {
        tSelf->SetupAp();
        tCurrentActive = (mWiFi->softAPgetStationNum() > 0);
        tStaFailureLogged = false;
      } else {
        if (tSelf->TryConnectStaWithRetry()) tCurrentActive = true;
        else {
          if (!tStaFailureLogged) {
            xLOG("STA failed after retries");
            tStaFailureLogged = true;
          }
          if (tSelf->mCfg.Connection.StaAutoFallbackApEnable) {
            tSelf->SwitchToFallbackApMode(true);
            tCurrentActive = true;
            tStaFailureLogged = false;
          }
        }
        if (tCurrentActive) tStaFailureLogged = false;
      }
      if (tCurrentActive && !tLastActive) {
        if (tSelf->mCfg.Connection.ApModeEnable) {
          xLOG("Client connected to AP, clients → %d", mWiFi->softAPgetStationNum());
        }
        if (tSelf->mCfg.Connection.MdnsEnable && tSelf->mCfg.Connection.MdnsName.length() > 0 && tSelf->mCfg.Connection.StaIpEnable) {
          tSelf->StartMdns();
          tMdnsRunning = true;
        }
        if (tSelf->mCallback) tSelf->mCallback();
      }
      if (!tCurrentActive && tLastActive) {
        if (tSelf->mCfg.Connection.ApModeEnable) {
          xLOG("Client(s) disconnected from AP");
        }
        if (tMdnsRunning) {
          tSelf->mMDNS.end();
          tMdnsRunning = false;
          xLOG("Localhost stopped, connection lost");
        }
      }
      tLastActive = tCurrentActive;
      vTaskDelay(tSelf->mCfg.Connection.ApModeEnable ? DELAY_SHORT_MS / portTICK_PERIOD_MS : DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(nullptr);
  }

  void Connection_::SetupAp() {
    if (mWiFi->getMode() == WIFI_AP && mWiFi->softAPSSID() == mCfg.Connection.ApSsid) return;
    IPAddress tIp, tGateway, tSubnet;
    tIp.fromString(mCfg.Connection.ApIp.c_str());
    tGateway.fromString(mCfg.Connection.ApGateway.c_str());
    tSubnet.fromString(mCfg.Connection.ApSubnet.c_str());
    mWiFi->mode(WIFI_AP);
    mWiFi->persistent(false);
    mWiFi->softAPConfig(tIp, tGateway, tSubnet);
    mWiFi->softAP(mCfg.Connection.ApSsid.c_str(), mCfg.Connection.ApPassword.c_str());
    xLOG("Starting AP Mode → %s", mCfg.Connection.ApSsid.c_str());
    PrintConnectionInfo();
  }

  bool Connection_::ConnectSta() {
    if (mWiFi->status() == WL_CONNECTED) return true;
    mWiFi->mode(WIFI_STA);
    mWiFi->useStaticBuffers(true);
    if (mCfg.Connection.StaIpEnable) {
      IPAddress tIp, tGateway, tSubnet, tDns1, tDns2;
      tIp.fromString(mCfg.Connection.StaIp.c_str());
      tGateway.fromString(mCfg.Connection.StaGateway.c_str());
      tSubnet.fromString(mCfg.Connection.StaSubnet.c_str());
      tDns1.fromString(mCfg.Connection.StaPrimaryDns.c_str());
      tDns2.fromString(mCfg.Connection.StaSecondaryDns.c_str());
      mWiFi->config(tIp, tGateway, tSubnet, tDns1, tDns2);
    }
    mWiFi->hostname(mCfg.Connection.MdnsName);
    xLOG("Connecting to WiFi → %s", mCfg.Connection.StaSsid.c_str());
    mWiFi->begin(mCfg.Connection.StaSsid.c_str(), mCfg.Connection.StaPassword.c_str());
    uint8_t tRetry = 0;
    while (mWiFi->status() != WL_CONNECTED && tRetry++ < WIFI_RETRY_COUNT) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    PrintConnectionInfo();
    return mWiFi->status() == WL_CONNECTED;
  }

  bool Connection_::TryConnectStaWithRetry() {
    if (mWiFi->status() == WL_CONNECTED) return true;
    uint8_t tMaxRetry = mCfg.Connection.StaConnectMaxRetry;
    if (tMaxRetry == 0) tMaxRetry = 1;
    uint32_t tRetryDelayMs = mCfg.Connection.StaRetryDelayMs;
    if (tRetryDelayMs < ONE_SECOND_MS) tRetryDelayMs = ONE_SECOND_MS;
    for (uint8_t tAttempt = 1; tAttempt <= tMaxRetry; tAttempt++) {
      xLOG("STA attempt → %u/%u", tAttempt, tMaxRetry);
      if (ConnectSta()) return true;
      if (tAttempt < tMaxRetry) vTaskDelay(tRetryDelayMs / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool Connection_::TryConnectApSta() {
    if (mWiFi->status() == WL_CONNECTED) return true;
    if (mCfg.Connection.StaSsid.isEmpty()) return false;
    const wifi_mode_t tMode = mWiFi->getMode();
    if (!(tMode & WIFI_MODE_AP)) return false;
    xLOG("Switching to AP+STA for internet access → %s", mCfg.Connection.StaSsid.c_str());
    mWiFi->mode(WIFI_AP_STA);
    mWiFi->useStaticBuffers(true);
    if (mCfg.Connection.StaIpEnable) {
      IPAddress tIp, tGateway, tSubnet, tDns1, tDns2;
      tIp.fromString(mCfg.Connection.StaIp.c_str());
      tGateway.fromString(mCfg.Connection.StaGateway.c_str());
      tSubnet.fromString(mCfg.Connection.StaSubnet.c_str());
      tDns1.fromString(mCfg.Connection.StaPrimaryDns.c_str());
      tDns2.fromString(mCfg.Connection.StaSecondaryDns.c_str());
      mWiFi->config(tIp, tGateway, tSubnet, tDns1, tDns2);
    }
    mWiFi->hostname(mCfg.Connection.MdnsName);
    mWiFi->begin(mCfg.Connection.StaSsid.c_str(), mCfg.Connection.StaPassword.c_str());
    uint8_t tRetry = 0;
    while (mWiFi->status() != WL_CONNECTED && tRetry++ < WIFI_RETRY_COUNT)
      vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    const bool tConnected = mWiFi->status() == WL_CONNECTED;
    xLOG("AP+STA internet %s", tConnected ? "connected" : "failed");
    if (!tConnected) mWiFi->mode(WIFI_MODE_AP);
    return tConnected;
  }

  void Connection_::SwitchToFallbackApMode(bool tPersistConfig) {
    if (!mCfg.Connection.FallbackApSsid.length()) {
      xLOG("Fallback AP skipped, missing fallback SSID");
      return;
    }
    mCfg.Connection.ApModeEnable = true;
    mCfg.Connection.ApSsid = mCfg.Connection.FallbackApSsid;
    mCfg.Connection.ApPassword = mCfg.Connection.FallbackApPassword;
    mCfg.Connection.ApIp = mCfg.Connection.FallbackApIp;
    mCfg.Connection.ApGateway = mCfg.Connection.FallbackApGateway;
    mCfg.Connection.ApSubnet = mCfg.Connection.FallbackApSubnet;
    if (tPersistConfig) {
      SAppConfig tConfig = CFG.Get<SAppConfig>();
      tConfig.Connection.ApModeEnable = true;
      tConfig.Connection.ApSsid = mCfg.Connection.FallbackApSsid;
      tConfig.Connection.ApPassword = mCfg.Connection.FallbackApPassword;
      tConfig.Connection.ApIp = mCfg.Connection.FallbackApIp;
      tConfig.Connection.ApGateway = mCfg.Connection.FallbackApGateway;
      tConfig.Connection.ApSubnet = mCfg.Connection.FallbackApSubnet;
      if (!CFG.SaveAllConfig(tConfig)) xLOG("Failed to persist fallback AP mode");
    }
    xLOG("Switched to fallback AP mode → %s", mCfg.Connection.ApSsid.c_str());
    SetupAp();
  }

  void Connection_::StartMdns() {
    mMDNS.end();
    vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
    uint8_t tRetry = 0;
    while (!(mWiFi->status() == WL_CONNECTED || mCfg.Connection.ApModeEnable) && tRetry++ < 10) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    if (!(mWiFi->status() == WL_CONNECTED || mCfg.Connection.ApModeEnable)) {
      xLOG("Localhost start failed, no active interface");
      return;
    }
    bool tStarted = false;
    for (uint8_t i = 0; i < 3 && !tStarted; ++i) {
      if (i > 0) {
        mMDNS.end();
        vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
      }
      tStarted = mMDNS.begin(mCfg.Connection.MdnsName);
      if (!tStarted) {
        xLOG("Localhost start attempt %d failed", i + 1);
        vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
      }
    }
    if (tStarted && mCfg.Connection.MdnsName.length() > 0 && mCfg.Connection.ApModeEnable) {
      xLOG("Localhost started → %s.local", mCfg.Connection.MdnsName.c_str());
    } else xLOG("Localhost failed to start after 3 attempts");
  }

  void Connection_::PrintConnectionInfo() {
    char tText[UTL.GetPrintInfoWidth() - 4] = "";
    xLOG_PL();
    if (mCfg.Connection.ApModeEnable) {
      UTL.PrintInfo("CONNECTION: AP MODE", EUtilsInfoType::Header);
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "SSID: %s", mCfg.Connection.ApSsid.c_str());
      UTL.PrintInfo(tText);
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "IP: %s", mWiFi->softAPIP().toString().c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "Gateway: %s", mCfg.Connection.ApGateway.c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "Subnet: %s", mWiFi->softAPSubnetMask().toString().c_str());
      UTL.PrintInfo(tText);
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "MAC: %s", mWiFi->macAddress().c_str());
      UTL.PrintInfo(tText);
      UTL.PrintInfo("", EUtilsInfoType::Footer);
      xLOG("Connect to WiFi, AP Mode → %s", mCfg.Connection.ApSsid.c_str());
    } else {
      snprintf(tText, sizeof(tText), "CONNECTION: STA MODE %s", mCfg.Connection.StaIpEnable ? "(STATIC)" : "(DHCP)");
      UTL.PrintInfo(tText, EUtilsInfoType::Header);
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "SSID: %s", mWiFi->SSID().c_str());
      UTL.PrintInfo(tText);
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "IP: %s", mWiFi->localIP().toString().c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "Gateway: %s", mWiFi->gatewayIP().toString().c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "Subnet: %s", mWiFi->subnetMask().toString().c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "DNS 1: %s", mWiFi->dnsIP(0).toString().c_str());
      UTL.PrintInfo(tText);
      snprintf(tText, sizeof(tText), "DNS 2: %s", mWiFi->dnsIP(1).toString().c_str());
      UTL.PrintInfo(tText);      
      UTL.PrintInfo("", EUtilsInfoType::Line);
      snprintf(tText, sizeof(tText), "MAC: %s", mWiFi->macAddress().c_str());
      UTL.PrintInfo(tText);
      UTL.PrintInfo("", EUtilsInfoType::Footer);
    }
  }

  void Connection_::BootstrapVault() {
    Guard tLock;
    NTP.Init();
    NTP.SyncSystemTimeIfNeeded();
    NTP.PrintDateTimeInfo();
    NTP.End();
  }

}
