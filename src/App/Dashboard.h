#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <App/Global.h>

namespace App {

  struct SSession {
    char Token[65] = {};
    uint32_t CreatedAt = 0;
    bool Active = false;
  };

  struct SImageUploadCtx {
    File Handle;
    bool Ok = true;
    size_t Written = 0;
    char Path[192] = {};
  };

  class Dashboard_ {
    DEFINE_TAG("DSH");
    friend class AutoGuard<Dashboard_>;
  public:
    using Guard = AutoGuard<Dashboard_>;
    static Dashboard_ &Instance();
    void Init(FDefaultCallback tOnReboot = nullptr, FDefaultCallback tOnReset = nullptr);
    void Start();
    void Stop();
    void HandleEvents();
  private:
    Dashboard_();
    Dashboard_(const Dashboard_&) = delete;
    Dashboard_ &operator=(const Dashboard_&) = delete;
    ~Dashboard_();
    static constexpr uint8_t kMaxSessions = 3;
    static constexpr uint32_t kSessionTtlSec = 30 * 60;
    static constexpr uint32_t kBroadcastIntervalMs = 5 * 1000;
    static constexpr size_t kSmallJsonSize = 512;
    static constexpr size_t kConfigJsonSize = 2 * 1024;
    static constexpr size_t kImagesJsonSize = 8 * 1024;
    static constexpr size_t kMaxUploadFileNameLen = 96;
    mutable SemaphoreHandle_t mMutex = nullptr;
    AsyncWebServer mServer {80};
    AsyncWebSocket *mWs = nullptr;
    FDefaultCallback mOnReboot = nullptr;
    FDefaultCallback mOnReset = nullptr;
    uint32_t mLastBroadcast = 0;
    SSession mSessions[kMaxSessions] = {};
    char mCachedUser[64] = {};
    char mCachedPassHash[65] = {};
    bool mRestartPending = false;
    bool mResetPending = false;
    bool mOtaActive = false;
    bool mOtaError = false;
    bool mRoutesRegistered = false;
    bool mServerStarted = false;
    mbedtls_sha256_context mOtaSha256 = {};
    size_t mOtaWritten = 0;
    char mOtaExpectedHash[65] = {};
    static void Lock();
    static void Unlock();
    bool ValidateToken(const char *tToken);
    bool AuthorizeRequest(AsyncWebServerRequest *tRequest);
    const char *CreateSession();
    void DestroySession(const char *tToken);
    void PurgeExpired();
    void RegisterRoutes();
    void OnWebSocketEvent(AsyncWebSocket *tWebSocket, AsyncWebSocketClient *tClient, AwsEventType tType, void *tEventArg, uint8_t *tData, size_t tLength);
    void OnWebSocketMessage(AsyncWebSocketClient *tClient, const char *tMessage, size_t tLength);
    void BroadcastStatus();
    void HandleLogin(AsyncWebServerRequest *tRequest);
    void HandleLogout(AsyncWebServerRequest *tRequest);
    void HandleStatus(AsyncWebServerRequest *tRequest);
    void HandleConfigGet(AsyncWebServerRequest *tRequest);
    void HandleConfigSave(AsyncWebServerRequest *tRequest);
    void HandleImagesList(AsyncWebServerRequest *tRequest);
    void HandleImageDelete(AsyncWebServerRequest *tRequest);
    void HandleImageUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal);
    void HandleImageDone(AsyncWebServerRequest *tRequest);
    void HandleOtaStatus(AsyncWebServerRequest *tRequest);
    void HandleOtaUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal);
    void HandleOtaDone(AsyncWebServerRequest *tRequest);
    void HandleReboot(AsyncWebServerRequest *tRequest);
    void HandleFactoryReset(AsyncWebServerRequest *tRequest);
    static size_t BuildStatusJson(char *tBuffer, size_t tSize);
    static size_t BuildConfigJson(char *tBuffer, size_t tSize, const SAppConfig &tConfig);
    static size_t BuildImagesJson(char *tBuffer, size_t tSize, const char *tDirectoryPath);
    static void JsonResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tJson);
    static void OkResponse(AsyncWebServerRequest *tRequest, const char *tMessage = "ok");
    static void ErrorResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tMessage);
    static void UnauthorizedResponse(AsyncWebServerRequest *tRequest);
    static void Sha256Hex(const uint8_t *tInput, size_t tLength, char *tHexOut65);
    static void RandomToken(char *tOut65);
    static bool IsAlphanumericOrSafe(char tChar);
    static void SanitizeFilename(const char *tInputFileName, char *tOutputFileName, size_t tOutputSize);
  };

}

#endif
