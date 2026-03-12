#include <App/Dashboard.h>

namespace App {

  Dashboard_ &Dashboard_::Instance() {
    static Dashboard_ tInstance;
    return tInstance;
  }

  Dashboard_::Dashboard_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
    mbedtls_sha256_init(&mOtaSha256);
  }

  Dashboard_::~Dashboard_() {
    Stop();
    if (mWs) {
      delete mWs;
      mWs = nullptr;
    }
    mbedtls_sha256_free(&mOtaSha256);
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Dashboard_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Dashboard_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void Dashboard_::Init(FDefaultCallback tOnReboot, FDefaultCallback tOnReset) {
    Guard tLock;
    mOnReboot = tOnReboot;
    mOnReset = tOnReset;
    mRestartPending = false;
    mResetPending = false;
    mLastBroadcast = 0;
    SDashboardConfig tDashboardConfig = CFG.Get<SDashboardConfig>();
    strncpy(mCachedUser, tDashboardConfig.User.c_str(), sizeof(mCachedUser) - 1);
    mCachedUser[sizeof(mCachedUser) - 1] = '\0';
    strncpy(mCachedPassHash, tDashboardConfig.Password.c_str(), sizeof(mCachedPassHash) - 1);
    mCachedPassHash[sizeof(mCachedPassHash) - 1] = '\0';
    if (!mWs) mWs = new AsyncWebSocket("/ws");
    if (mWs) {
      mWs->onEvent([this](AsyncWebSocket *tWebSocket, AsyncWebSocketClient *tClient, AwsEventType tType, void *tEventArg, uint8_t *tData, size_t tLength) {
        OnWebSocketEvent(tWebSocket, tClient, tType, tEventArg, tData, tLength);
      });
      if (!mRoutesRegistered) mServer.addHandler(mWs);
    }
    if (!mRoutesRegistered) {
      RegisterRoutes();
      mRoutesRegistered = true;
    }
  }

  void Dashboard_::Start() {
    Guard tLock;
    if (mServerStarted) return;
    mServer.begin();
    mServerStarted = true;
    SConnectionConfig tConnectionConfig = CFG.Get<SConnectionConfig>();
    const char *tHost = tConnectionConfig.MdnsEnable ? tConnectionConfig.MdnsName.c_str() : CON.GetIpAddress();
    const char *tHostSuffix = tConnectionConfig.MdnsEnable ? ".local/" : "/";
    char tAdminUrl[96] = "";
    snprintf(tAdminUrl, sizeof(tAdminUrl), "http://%s%s", tHost ? tHost : "", tHostSuffix);
    xLOG("Dashboard → server started on port 80");
    xLOG("Dashboard → admin URL: %s", tAdminUrl);
  }

  void Dashboard_::Stop() {
    Guard tLock;
    if (mServerStarted) {
      mServer.end();
      mServerStarted = false;
      xLOG("Dashboard → server stopped");
    }
    if (mWs) mWs->closeAll();
  }

  void Dashboard_::HandleEvents() {
    Guard tLock;
    PurgeExpired();
    if (mWs && millis() - mLastBroadcast >= kBroadcastIntervalMs) {
      BroadcastStatus();
      mLastBroadcast = millis();
    }
    if (mResetPending) {
      mResetPending = false;
      if (mOnReset) mOnReset();
    }
    if (mRestartPending) {
      mRestartPending = false;
      if (mOnReboot) mOnReboot();
    }
  }

  bool Dashboard_::ValidateToken(const char *tToken) {
    if (!tToken || !tToken[0]) return false;
    PurgeExpired();
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (strcmp(mSessions[tIndex].Token, tToken) == 0) return true;
    }
    return false;
  }

  bool Dashboard_::AuthorizeRequest(AsyncWebServerRequest *tRequest) {
    if (!mCachedPassHash[0]) return true;
    if (!tRequest || !tRequest->hasHeader("Authorization")) return false;
    String tAuthorizationHeader = tRequest->header("Authorization");
    if (!tAuthorizationHeader.startsWith("Bearer ")) return false;
    String tToken = tAuthorizationHeader.substring(7);
    return ValidateToken(tToken.c_str());
  }

  const char *Dashboard_::CreateSession() {
    PurgeExpired();
    uint8_t tSlot = kMaxSessions;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) {
        tSlot = tIndex;
        break;
      }
    }
    if (tSlot == kMaxSessions) {
      uint32_t tOldest = UINT32_MAX;
      for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
        if (mSessions[tIndex].CreatedAt <= tOldest) {
          tOldest = mSessions[tIndex].CreatedAt;
          tSlot = tIndex;
        }
      }
    }
    RandomToken(mSessions[tSlot].Token);
    mSessions[tSlot].CreatedAt = static_cast<uint32_t>(time(nullptr));
    mSessions[tSlot].Active = true;
    return mSessions[tSlot].Token;
  }

  void Dashboard_::DestroySession(const char *tToken) {
    if (!tToken || !tToken[0]) return;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (strcmp(mSessions[tIndex].Token, tToken) == 0) {
        memset(mSessions[tIndex].Token, 0, sizeof(mSessions[tIndex].Token));
        mSessions[tIndex].CreatedAt = 0;
        mSessions[tIndex].Active = false;
        return;
      }
    }
  }

  void Dashboard_::PurgeExpired() {
    const uint32_t tNow = static_cast<uint32_t>(time(nullptr));
    if (tNow == 0) return;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (tNow - mSessions[tIndex].CreatedAt > kSessionTtlSec) {
        memset(mSessions[tIndex].Token, 0, sizeof(mSessions[tIndex].Token));
        mSessions[tIndex].CreatedAt = 0;
        mSessions[tIndex].Active = false;
      }
    }
  }

  void Dashboard_::RegisterRoutes() {
    mServer.on("/", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      tRequest->send(LittleFS, "/index.html", "text/html");
    });

    mServer.on("/api/auth/login", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogin(tRequest);
    });

    mServer.on("/api/auth/logout", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogout(tRequest);
    });

    mServer.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleStatus(tRequest);
    });

    mServer.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleConfigGet(tRequest);
    });

    mServer.on("/api/config/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleConfigSave(tRequest);
    });

    mServer.on("/api/images", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleImagesList(tRequest);
    });

    mServer.on(AsyncURIMatcher::prefix("/api/images/"), HTTP_DELETE, [this](AsyncWebServerRequest *tRequest) {
      HandleImageDelete(tRequest);
    });

    mServer.on("/api/images/upload", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleImageDone(tRequest);
    }, [this](AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
      HandleImageUpload(tRequest, tFilename, tIndex, tData, tLength, tFinal);
    });

    mServer.on("/api/ota/status", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleOtaStatus(tRequest);
    });

    mServer.on("/api/ota/upload", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleOtaDone(tRequest);
    }, [this](AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
      HandleOtaUpload(tRequest, tFilename, tIndex, tData, tLength, tFinal);
    });

    mServer.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleReboot(tRequest);
    });

    mServer.on("/api/factory/reset", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleFactoryReset(tRequest);
    });
  }

  void Dashboard_::OnWebSocketEvent(AsyncWebSocket *tWebSocket, AsyncWebSocketClient *tClient, AwsEventType tType, void *tEventArg, uint8_t *tData, size_t tLength) {
    (void)tWebSocket;
    if (tType == WS_EVT_CONNECT) {
      if (!mCachedPassHash[0]) {
        if (tClient) tClient->text("{\"type\":\"ack\",\"ok\":true,\"message\":\"connected\"}");
        return;
      }
      AsyncWebServerRequest *tRequest = static_cast<AsyncWebServerRequest *>(tEventArg);
      if (!AuthorizeRequest(tRequest)) {
        xLOG("Dashboard → websocket unauthorized, closing client");
        if (tClient) tClient->close();
      }
      return;
    }
    if (tType != WS_EVT_DATA || !tData || tLength == 0 || !tClient) return;
    AwsFrameInfo *tFrame = static_cast<AwsFrameInfo *>(tEventArg);
    if (!tFrame || tFrame->opcode != WS_TEXT) return;
    char tMessage[256] = "";
    size_t tCopy = tLength;
    if (tCopy > sizeof(tMessage) - 1) tCopy = sizeof(tMessage) - 1;
    memcpy(tMessage, tData, tCopy);
    tMessage[tCopy] = '\0';
    OnWebSocketMessage(tClient, tMessage, tCopy);
  }

  void Dashboard_::OnWebSocketMessage(AsyncWebSocketClient *tClient, const char *tMessage, size_t tLength) {
    (void)tLength;
    if (!tMessage || !tClient) return;
    if (strstr(tMessage, "\"type\":\"config_save\"")) {
      SAppConfig tConfig = CFG.Get<SAppConfig>();
      if (CFG.SaveAllConfig(tConfig)) {
        xLOG("Dashboard → config saved from websocket");
        tClient->text("{\"type\":\"ack\",\"ok\":true}");
      } else {
        xLOG("Dashboard → config save failed from websocket");
        tClient->text("{\"type\":\"ack\",\"ok\":false,\"error\":\"save failed\"}");
      }
      return;
    }
    if (strstr(tMessage, "\"type\":\"reboot\"")) {
      mRestartPending = true;
      xLOG("Dashboard → reboot requested from websocket");
      tClient->text("{\"type\":\"ack\",\"ok\":true}");
      return;
    }
    tClient->text("{\"type\":\"ack\",\"ok\":false,\"error\":\"unsupported\"}");
  }

  void Dashboard_::BroadcastStatus() {
    if (!mWs) return;
    char tJson[kSmallJsonSize] = "";
    BuildStatusJson(tJson, sizeof(tJson));
    mWs->textAll(tJson);
  }

  void Dashboard_::HandleLogin(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!tRequest) return;
    if (!mCachedPassHash[0]) {
      JsonResponse(tRequest, 200, "{\"ok\":true,\"auth\":\"disabled\"}");
      return;
    }
    if (!tRequest->hasParam("user", true) || !tRequest->hasParam("pass", true)) {
      ErrorResponse(tRequest, 400, "missing credentials");
      return;
    }
    String tUserName = tRequest->getParam("user", true)->value();
    String tPassword = tRequest->getParam("pass", true)->value();
    char tPasswordHash[65] = "";
    Sha256Hex(reinterpret_cast<const uint8_t *>(tPassword.c_str()), tPassword.length(), tPasswordHash);
    if (tUserName != String(mCachedUser) || strcasecmp(tPasswordHash, mCachedPassHash) != 0) {
      xLOG("Dashboard → login failed for user: %s", tUserName.c_str());
      UnauthorizedResponse(tRequest);
      return;
    }
    const char *tToken = CreateSession();
    if (!tToken) {
      ErrorResponse(tRequest, 500, "session failed");
      return;
    }
    char tResponseJson[160] = "";
    snprintf(tResponseJson, sizeof(tResponseJson), "{\"ok\":true,\"token\":\"%s\"}", tToken);
    xLOG("Dashboard → login successful for user: %s", tUserName.c_str());
    JsonResponse(tRequest, 200, tResponseJson);
  }

  void Dashboard_::HandleLogout(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!tRequest) return;
    if (tRequest->hasHeader("Authorization")) {
      String tAuthorizationHeader = tRequest->header("Authorization");
      if (tAuthorizationHeader.startsWith("Bearer ")) {
        String tToken = tAuthorizationHeader.substring(7);
        DestroySession(tToken.c_str());
      }
    }
    xLOG("Dashboard → logout completed");
    OkResponse(tRequest, "logout");
  }

  void Dashboard_::HandleStatus(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    char tJson[kSmallJsonSize] = "";
    BuildStatusJson(tJson, sizeof(tJson));
    JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleConfigGet(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    char tJson[kConfigJsonSize] = "";
    BuildConfigJson(tJson, sizeof(tJson), tConfig);
    JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleConfigSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (CFG.SaveAllConfig(tConfig)) {
      SDashboardConfig tDashboardConfig = CFG.Get<SDashboardConfig>();
      strncpy(mCachedUser, tDashboardConfig.User.c_str(), sizeof(mCachedUser) - 1);
      mCachedUser[sizeof(mCachedUser) - 1] = '\0';
      strncpy(mCachedPassHash, tDashboardConfig.Password.c_str(), sizeof(mCachedPassHash) - 1);
      mCachedPassHash[sizeof(mCachedPassHash) - 1] = '\0';
      xLOG("Dashboard → config saved via HTTP");
      OkResponse(tRequest, "saved");
      return;
    }
    xLOG("Dashboard → config save failed via HTTP");
    ErrorResponse(tRequest, 500, "save failed");
  }

  void Dashboard_::HandleImagesList(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tDirectoryPath[64] = "";
    snprintf(tDirectoryPath, sizeof(tDirectoryPath), "/%s", tDisplayConfig.ImagesDir.c_str());
    char tJson[kImagesJsonSize] = "";
    BuildImagesJson(tJson, sizeof(tJson), tDirectoryPath);
    JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleImageDelete(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    String tRequestUrl = tRequest->url();
    const String tPrefix = "/api/images/";
    if (!tRequestUrl.startsWith(tPrefix) || tRequestUrl.length() <= tPrefix.length()) {
      ErrorResponse(tRequest, 400, "invalid image path");
      return;
    }
    String tImageName = tRequest->urlDecode(tRequestUrl.substring(tPrefix.length()));
    char tSafeFileName[kMaxUploadFileNameLen + 1] = "";
    SanitizeFilename(tImageName.c_str(), tSafeFileName, sizeof(tSafeFileName));
    if (!tSafeFileName[0]) {
      ErrorResponse(tRequest, 400, "invalid image name");
      return;
    }
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tImagePath[192] = "";
    snprintf(tImagePath, sizeof(tImagePath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
    if (!STG.Exists(tImagePath)) {
      ErrorResponse(tRequest, 404, "file not found");
      return;
    }
    if (!STG.DeleteFile(tImagePath)) {
      ErrorResponse(tRequest, 500, "delete failed");
      return;
    }
    xLOG("Dashboard → image deleted: %s", tImagePath);
    OkResponse(tRequest, "deleted");
  }

  void Dashboard_::HandleImageUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
    (void)tRequest;
    Guard tLock;
    if (!tRequest || !AuthorizeRequest(tRequest)) return;
    SImageUploadCtx *tUploadContext = static_cast<SImageUploadCtx *>(tRequest->_tempObject);
    if (!tUploadContext && tIndex == 0) {
      tUploadContext = new SImageUploadCtx();
      tRequest->_tempObject = tUploadContext;
      SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
      char tSafeFileName[kMaxUploadFileNameLen + 1] = "";
      SanitizeFilename(tFilename.c_str(), tSafeFileName, sizeof(tSafeFileName));
      if (!tSafeFileName[0]) {
        tUploadContext->Ok = false;
        return;
      }
      snprintf(tUploadContext->Path, sizeof(tUploadContext->Path), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
      char tDirectoryPath[80] = "";
      snprintf(tDirectoryPath, sizeof(tDirectoryPath), "/%s", tDisplayConfig.ImagesDir.c_str());
      if (!STG.Exists(tDirectoryPath)) STG.MakeDir(tDirectoryPath);
      tUploadContext->Handle = STG.OpenFile(tUploadContext->Path, FILE_WRITE, true);
      if (!tUploadContext->Handle) tUploadContext->Ok = false;
    }
    if (!tUploadContext || !tUploadContext->Ok) return;
    if (tLength > 0) {
      size_t tBytesWritten = tUploadContext->Handle.write(tData, tLength);
      if (tBytesWritten != tLength) tUploadContext->Ok = false;
      else tUploadContext->Written += tBytesWritten;
    }
    if (tFinal && tUploadContext->Handle) {
      tUploadContext->Handle.flush();
      tUploadContext->Handle.close();
    }
  }

  void Dashboard_::HandleImageDone(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    SImageUploadCtx *tUploadContext = static_cast<SImageUploadCtx *>(tRequest->_tempObject);
    if (!tUploadContext || !tUploadContext->Ok || tUploadContext->Written == 0) {
      if (tUploadContext) {
        if (tUploadContext->Handle) tUploadContext->Handle.close();
        delete tUploadContext;
        tRequest->_tempObject = nullptr;
      }
      ErrorResponse(tRequest, 500, "upload failed");
      return;
    }
    delete tUploadContext;
    tRequest->_tempObject = nullptr;
    xLOG("Dashboard → image uploaded successfully");
    OkResponse(tRequest, "uploaded");
  }

  void Dashboard_::HandleOtaStatus(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    char tJson[192] = "";
    snprintf(tJson, sizeof(tJson), "{\"ok\":true,\"version\":\"%s\",\"ota_active\":%s,\"ota_written\":%u}", CFG.Get<SDeviceConfig>().Version.c_str(), mOtaActive ? "true" : "false", (unsigned)mOtaWritten);
    JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleOtaUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
    (void)tFilename;
    Guard tLock;
    if (!tRequest || !AuthorizeRequest(tRequest)) return;
    if (tIndex == 0) {
      mOtaActive = true;
      mOtaError = false;
      mOtaWritten = 0;
      xLOG("Dashboard → OTA upload started");
      memset(mOtaExpectedHash, 0, sizeof(mOtaExpectedHash));
      if (tRequest->hasHeader("X-SHA256")) {
        String tExpectedHashHeader = tRequest->header("X-SHA256");
        tExpectedHashHeader.toLowerCase();
        strncpy(mOtaExpectedHash, tExpectedHashHeader.c_str(), sizeof(mOtaExpectedHash) - 1);
      }
      mbedtls_sha256_free(&mOtaSha256);
      mbedtls_sha256_init(&mOtaSha256);
      if (mbedtls_sha256_starts_ret(&mOtaSha256, 0) != 0) {
        mOtaError = true;
        xLOG("Dashboard → OTA sha256 init failed");
        return;
      }
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        mOtaError = true;
        xLOG("Dashboard → OTA begin failed");
        return;
      }
    }
    if (mOtaError) return;
    if (tLength > 0) {
      if (Update.write(tData, tLength) != tLength) {
        mOtaError = true;
        xLOG("Dashboard → OTA write failed");
        return;
      }
      if (mbedtls_sha256_update_ret(&mOtaSha256, tData, tLength) != 0) {
        mOtaError = true;
        xLOG("Dashboard → OTA sha256 update failed");
        return;
      }
      mOtaWritten += tLength;
      if (mWs) {
        char tProgress[96] = "";
        snprintf(tProgress, sizeof(tProgress), "{\"type\":\"ota_progress\",\"written\":%u}", (unsigned)mOtaWritten);
        mWs->textAll(tProgress);
      }
    }
    if (tFinal) {
      uint8_t tDigest[32] = {};
      char tDigestHex[65] = "";
      if (mbedtls_sha256_finish_ret(&mOtaSha256, tDigest) != 0) {
        mOtaError = true;
      } else {
        for (uint8_t tIndex2 = 0; tIndex2 < 32; tIndex2++) snprintf(tDigestHex + tIndex2 * 2, 3, "%02x", tDigest[tIndex2]);
      }
      if (mOtaExpectedHash[0] && strcasecmp(tDigestHex, mOtaExpectedHash) != 0) mOtaError = true;
      if (!mOtaError && !Update.end(true)) mOtaError = true;
      if (mOtaError) Update.abort();
      if (mOtaError) xLOG("Dashboard → OTA upload failed");
      else xLOG("Dashboard → OTA upload finished successfully");
      mOtaActive = false;
    }
  }

  void Dashboard_::HandleOtaDone(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    if (mOtaError || mOtaActive) {
      ErrorResponse(tRequest, 500, "ota failed");
      return;
    }
    OkResponse(tRequest, "ota ok, reboot required");
  }

  void Dashboard_::HandleReboot(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    mRestartPending = true;
    xLOG("Dashboard → reboot requested via HTTP");
    OkResponse(tRequest, "rebooting");
  }

  void Dashboard_::HandleFactoryReset(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      UnauthorizedResponse(tRequest);
      return;
    }
    mResetPending = true;
    xLOG("Dashboard → factory reset requested via HTTP");
    OkResponse(tRequest, "factory reset");
  }

  size_t Dashboard_::BuildStatusJson(char *tBuffer, size_t tSize) {
    if (!tBuffer || tSize == 0) return 0;
    const uint32_t tUptime = millis() / 1000;
    const uint32_t tHeap = ESP.getFreeHeap();
    const uint32_t tBatteryMilliVolts = BAT.GetVoltage();
    const uint8_t tBatteryPercent = BAT.GetPercentage();
    const int32_t tWifiRssi = WiFi.RSSI();
    const char *tStorage = STG.GetActiveName();
    SDisplayConfig tDisplay = CFG.Get<SDisplayConfig>();
    int tLength = snprintf(tBuffer, tSize,
      "{\"type\":\"status\",\"uptime\":%u,\"heap_free\":%u,\"battery_mv\":%u,\"battery_pct\":%u,\"wifi_rssi\":%d,\"storage\":\"%s\",\"current_image\":\"%s\"}",
      (unsigned)tUptime, (unsigned)tHeap, (unsigned)tBatteryMilliVolts, (unsigned)tBatteryPercent, (int)tWifiRssi, tStorage ? tStorage : "n/a", tDisplay.CurrentFile.c_str());
    if (tLength < 0) {
      tBuffer[0] = '\0';
      return 0;
    }
    return static_cast<size_t>(tLength);
  }

  size_t Dashboard_::BuildConfigJson(char *tBuffer, size_t tSize, const SAppConfig &tConfig) {
    if (!tBuffer || tSize == 0) return 0;
    int tLength = snprintf(tBuffer, tSize,
      "{\"device\":{\"name\":\"%s\",\"version\":\"%s\"},\"display\":{\"rotate\":%u,\"current\":\"%s\"},\"connection\":{\"ap\":%s,\"mdns\":%s,\"mdns_name\":\"%s\"},\"timer\":{\"wake_hour\":%u},\"dashboard\":{\"user\":\"%s\",\"hash_enabled\":%s}}",
      tConfig.Device.Name.c_str(), tConfig.Device.Version.c_str(), (unsigned)tConfig.Display.Rotate,
      tConfig.Display.CurrentFile.c_str(), tConfig.Connection.ApModeEnable ? "true" : "false", tConfig.Connection.MdnsEnable ? "true" : "false",
      tConfig.Connection.MdnsName.c_str(), (unsigned)tConfig.Timer.WakeUpHour,
      tConfig.Dashboard.User.c_str(), tConfig.Dashboard.Password.isEmpty() ? "false" : "true");
    if (tLength < 0) {
      tBuffer[0] = '\0';
      return 0;
    }
    return static_cast<size_t>(tLength);
  }

  size_t Dashboard_::BuildImagesJson(char *tBuffer, size_t tSize, const char *tDirectoryPath) {
    if (!tBuffer || tSize == 0) return 0;
    const char *tDirectoryList = STG.ListDir(tDirectoryPath);
    if (!tDirectoryList) tDirectoryList = "";
    size_t tWritePos = 0;
    tWritePos += snprintf(tBuffer + tWritePos, tSize - tWritePos, "{\"images\":[");
    const char *tCursor = tDirectoryList;
    bool tIsFirstEntry = true;
    while (*tCursor && tWritePos + 8 < tSize) {
      while (*tCursor == '\r' || *tCursor == '\n' || *tCursor == ' ') tCursor++;
      if (!*tCursor) break;
      const char *tLineEnd = tCursor;
      while (*tLineEnd && *tLineEnd != '\r' && *tLineEnd != '\n') tLineEnd++;
      const char *tBracketPos = static_cast<const char *>(memchr(tCursor, '[', tLineEnd - tCursor));
      size_t tNameLength = tBracketPos ? static_cast<size_t>(tBracketPos - tCursor) : static_cast<size_t>(tLineEnd - tCursor);
      while (tNameLength > 0 && (tCursor[tNameLength - 1] == ' ' || tCursor[tNameLength - 1] == '/')) tNameLength--;
      if (tNameLength > 0) {
        if (!tIsFirstEntry) tWritePos += snprintf(tBuffer + tWritePos, tSize - tWritePos, ",");
        tWritePos += snprintf(tBuffer + tWritePos, tSize - tWritePos, "\"%.*s\"", (int)tNameLength, tCursor);
        tIsFirstEntry = false;
      }
      tCursor = tLineEnd;
    }
    tWritePos += snprintf(tBuffer + tWritePos, tSize - tWritePos, "]}");
    return tWritePos;
  }

  void Dashboard_::JsonResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tJson) {
    if (!tRequest) return;
    tRequest->send(tCode, "application/json", tJson ? tJson : "{}");
  }

  void Dashboard_::OkResponse(AsyncWebServerRequest *tRequest, const char *tMessage) {
    char tJson[160] = "";
    snprintf(tJson, sizeof(tJson), "{\"ok\":true,\"message\":\"%s\"}", tMessage ? tMessage : "ok");
    JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::ErrorResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tMessage) {
    char tJson[192] = "";
    snprintf(tJson, sizeof(tJson), "{\"ok\":false,\"error\":\"%s\"}", tMessage ? tMessage : "error");
    JsonResponse(tRequest, tCode, tJson);
  }

  void Dashboard_::UnauthorizedResponse(AsyncWebServerRequest *tRequest) {
    if (tRequest) xLOG("Dashboard → unauthorized request: %s", tRequest->url().c_str());
    ErrorResponse(tRequest, 401, "unauthorized");
  }

  void Dashboard_::Sha256Hex(const uint8_t *tInput, size_t tLength, char *tHexOut65) {
    if (!tInput || !tHexOut65) return;
    uint8_t tDigest[32] = {};
    mbedtls_sha256_context tShaContext;
    mbedtls_sha256_init(&tShaContext);
    if (mbedtls_sha256_starts_ret(&tShaContext, 0) == 0 && mbedtls_sha256_update_ret(&tShaContext, tInput, tLength) == 0 && mbedtls_sha256_finish_ret(&tShaContext, tDigest) == 0) {
      for (uint8_t tIndex = 0; tIndex < 32; tIndex++) snprintf(tHexOut65 + tIndex * 2, 3, "%02x", tDigest[tIndex]);
    } else tHexOut65[0] = '\0';
    mbedtls_sha256_free(&tShaContext);
  }

  void Dashboard_::RandomToken(char *tOut65) {
    if (!tOut65) return;
    for (uint8_t tIndex = 0; tIndex < 32; tIndex++) {
      uint8_t tRandom = static_cast<uint8_t>(esp_random() & 0xFF);
      snprintf(tOut65 + tIndex * 2, 3, "%02x", tRandom);
    }
    tOut65[64] = '\0';
  }

  bool Dashboard_::IsAlphanumericOrSafe(char tChar) {
    if ((tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') || (tChar >= '0' && tChar <= '9')) return true;
    if (tChar == '.' || tChar == '_' || tChar == '-') return true;
    return false;
  }

  void Dashboard_::SanitizeFilename(const char *tInputFileName, char *tOutputFileName, size_t tOutputSize) {
    if (!tOutputFileName || tOutputSize == 0) return;
    tOutputFileName[0] = '\0';
    if (!tInputFileName || !tInputFileName[0]) return;
    size_t tWritePos = 0;
    for (size_t tIndex = 0; tInputFileName[tIndex] != '\0' && tWritePos + 1 < tOutputSize; tIndex++) {
      char tCharacter = tInputFileName[tIndex];
      if (tCharacter == '/' || tCharacter == '\\' || tCharacter == ':') continue;
      if (!IsAlphanumericOrSafe(tCharacter)) continue;
      tOutputFileName[tWritePos++] = tCharacter;
    }
    tOutputFileName[tWritePos] = '\0';
  }

}
