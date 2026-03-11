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
        xLOG("Connection → timeout, timedate syncronization skipped");
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
    while (!sStopWiFiEventTask) {
      bool tCurrentActive = false;
      if (tSelf->mCfg.Connection.ApModeEnable) {
        tSelf->SetupAp();
        tCurrentActive = (mWiFi->softAPgetStationNum() > 0);
      } else {
        uint32_t tStart = millis();
        tSelf->ConnectSta();
        while (mWiFi->status() != WL_CONNECTED && (millis() - tStart) < WIFI_CONNECT_TIMEOUT_MS) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
        if (mWiFi->status() == WL_CONNECTED) tCurrentActive = true;
        else {
          xLOG("Connection → STA failed, fallback to AP mode");
          tSelf->SetupAp();
          tCurrentActive = true;
        }
      }
      if (tCurrentActive && !tLastActive) {
        if (tSelf->mCfg.Connection.ApModeEnable) {
          xLOG("Connection → client connected to AP, clients: %d", mWiFi->softAPgetStationNum());
        }
        if (tSelf->mCfg.Connection.MdnsEnable) {
          tSelf->StartMdns();
          tMdnsRunning = true;
        }
        if (tSelf->mCallback) tSelf->mCallback();
      }
      if (!tCurrentActive && tLastActive) {
        if (tSelf->mCfg.Connection.ApModeEnable) {
          xLOG("Connection → all clients disconnected from AP");
        }
        if (tMdnsRunning) {
          tSelf->mMDNS.end();
          tMdnsRunning = false;
          xLOG("Connection → localhost stopped, connection lost");
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
    xLOG("Connection → starting AP Mode: %s", mCfg.Connection.ApSsid.c_str());
    PrintConnectionInfo();
  }

  void Connection_::ConnectSta() {
    if (mWiFi->status() == WL_CONNECTED || (mWiFi->getMode() == WIFI_STA && mWiFi->status() == WL_IDLE_STATUS)) return;
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
    xLOG("Connection → connecting to WiFi: %s", mCfg.Connection.StaSsid.c_str());
    mWiFi->begin(mCfg.Connection.StaSsid.c_str(), mCfg.Connection.StaPassword.c_str());
    uint8_t tRetry = 0;
    while (mWiFi->status() != WL_CONNECTED && tRetry++ < WIFI_RETRY_COUNT) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    PrintConnectionInfo();
  }

  void Connection_::StartMdns() {
    mMDNS.end();
    vTaskDelay(DELAY_ONE_SEC_MS / portTICK_PERIOD_MS);
    uint8_t tRetry = 0;
    while (!(mWiFi->status() == WL_CONNECTED || mCfg.Connection.ApModeEnable) && tRetry++ < 10) vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
    if (!(mWiFi->status() == WL_CONNECTED || mCfg.Connection.ApModeEnable)) {
      xLOG("Connection → localhost start failed, no active interface");
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
        xLOG("Connection → localhost start attempt %d failed", i + 1);
        vTaskDelay(DELAY_HALF_SEC_MS / portTICK_PERIOD_MS);
      }
    }
    if (tStarted) {
      xLOG("Connection → localhost started: %s.local", mCfg.Connection.MdnsName.c_str());
    } else xLOG("Connection → localhost failed to start after 3 attempts");
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
      xLOG("Connect to WiFi AP: %s", mCfg.Connection.ApSsid.c_str());
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
    NTP.SyncSystemTime();
    NTP.PrintDateTimeInfo();
    NTP.End();
  }

}
