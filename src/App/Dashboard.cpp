#include <App/Dashboard.h>
#include <App/Dashboard/Assets/Registry.h>
#include <App/Dashboard/Languages/Registry.h>
#include <App/Dashboard/Partials/Layout.h>
#include <App/Dashboard/Utils/DashboardUtils.h>

namespace App {

  Dashboard_ &Dashboard_::Instance() {
    static Dashboard_ tInstance;
    return tInstance;
  }

  Dashboard_::Dashboard_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Dashboard_::~Dashboard_() {
    Stop();
    if (mWebSocket) {
      delete mWebSocket;
      mWebSocket = nullptr;
    }
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
    ReloadConfig();
    mOnReboot = tOnReboot;
    mOnReset = tOnReset;
    mRestartPending = false;
    mResetPending = false;
    mLastBroadcast = 0;
    mLastStatusCacheRefreshMs = 0;
    mLastStatsCacheRefreshMs = 0;
    mStatsCacheDirty = true;
    mLastIndexDataCacheRefreshMs = 0;
    mIndexDataCacheDirty = true;
    mCachedIndexGalleryCount = 0;
    mCachedIndexStoragesJson = "\"Storages\":[]";
    mCachedPageStatsDataJson = "\"Statistics\":{}";
    mCachedStatusJson[0] = '\0';
    mCachedStatsJson[0] = '\0';
    strncpy(mCachedUser, mCfg.Dashboard.User.c_str(), sizeof(mCachedUser) - 1);
    mCachedUser[sizeof(mCachedUser) - 1] = '\0';
    strncpy(mCachedPassHash, mCfg.Dashboard.Password.c_str(), sizeof(mCachedPassHash) - 1);
    mCachedPassHash[sizeof(mCachedPassHash) - 1] = '\0';
    if (!mWebSocket) mWebSocket = new AsyncWebSocket("/ws");
    if (mWebSocket) {
      mWebSocket->onEvent([this](AsyncWebSocket *tWebSocket, AsyncWebSocketClient *tClient, AwsEventType tType, void *tEventArg, uint8_t *tData, size_t tLength) {
        OnWebSocketEvent(tWebSocket, tClient, tType, tEventArg, tData, tLength);
      });
      if (!mRoutesRegistered) mServer.addHandler(mWebSocket);
    }
    if (!mRoutesRegistered) {
      RegisterRoutes();
      mRoutesRegistered = true;
    }
    BuildStatusJson(mCachedStatusJson, sizeof(mCachedStatusJson));
    mLastStatusCacheRefreshMs = millis();
  }

  void Dashboard_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SAppConfig>();
    mStatsCacheDirty = true;
    mIndexDataCacheDirty = true;
  }

  void Dashboard_::Start() {
    bool tNeedIndexCacheRefresh = false;
    bool tNeedStatsCacheRefresh = false;
    {
      Guard tLock;
      if (mServerStarted) return;
      ReloadConfig();
      mServer.begin();
      mServerStarted = true;
      mIndexDataCacheDirty = true;
      mStatsCacheDirty = true;
      tNeedIndexCacheRefresh = true;
      tNeedStatsCacheRefresh = true;
      xLOG("Webserver started on port → 80");
      if (!mCfg.Connection.ApModeEnable) {
        const char *tHostName = "photoframecl01";
        char tAdminAddress[96] = "";
        snprintf(tAdminAddress, sizeof(tAdminAddress), "http://%s.local", tHostName);
        xLOG("Dashboard admin URL → %s", tAdminAddress);
      } else  xLOG("Dashboard admin URL → http://%s/", CON.GetIpAddress());
    }
    if (tNeedIndexCacheRefresh || tNeedStatsCacheRefresh) RefreshDataCachesIfNeeded(true);
  }

  void Dashboard_::Stop() {
    Guard tLock;
    if (mServerStarted) {
      mServer.end();
      mServerStarted = false;
      xLOG("Webserver stopped");
    }
    if (mWebSocket) mWebSocket->closeAll();
  }  

  String Dashboard_::EscapeJsonText(const String &tValue) {
    String tSafe;
    tSafe.reserve(tValue.length() + 16);
    for (size_t tIndex = 0; tIndex < tValue.length(); tIndex++) {
      const char tChar = tValue[tIndex];
      if (tChar == '\\') tSafe += "\\\\";
      else if (tChar == '"') tSafe += "\\\"";
      else if (tChar == '\n') tSafe += "\\n";
      else if (tChar == '\r') tSafe += "\\r";
      else if (tChar == '\t') tSafe += "\\t";
      else tSafe += tChar;
    }
    return tSafe;
  }

  bool Dashboard_::ParseBoolValue(const String &tValue) {
    String tNormalized = tValue;
    tNormalized.trim();
    tNormalized.toLowerCase();
    return tNormalized == "1" || tNormalized == "true" || tNormalized == "on" || tNormalized == "yes";
  }

  bool Dashboard_::TryGetRequestValue(AsyncWebServerRequest *tRequest, const char *tKey, String &tValue) {
    if (!tRequest || !tKey || !tKey[0]) return false;
    if (tRequest->hasParam(tKey, true)) {
      tValue = tRequest->getParam(tKey, true)->value();
      return true;
    }
    if (tRequest->hasParam(tKey)) {
      tValue = tRequest->getParam(tKey)->value();
      return true;
    }
    return false;
  }

  String Dashboard_::EncodeUrlComponent(const String &tValue) {
    static const char *kHex = "0123456789ABCDEF";
    String tEncoded;
    tEncoded.reserve(tValue.length() * 3);
    for (size_t tIndex = 0; tIndex < tValue.length(); tIndex++) {
      const uint8_t tChar = static_cast<uint8_t>(tValue[tIndex]);
      const bool tUnreserved = (tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') || (tChar >= '0' && tChar <= '9') || tChar == '-' || tChar == '_' || tChar == '.' || tChar == '~';
      if (tUnreserved) {
        tEncoded += static_cast<char>(tChar);
        continue;
      }
      tEncoded += '%';
      tEncoded += kHex[(tChar >> 4) & 0x0F];
      tEncoded += kHex[tChar & 0x0F];
    }
    return tEncoded;
  }

  bool Dashboard_::IsDarkLogoAssetPath(const char *tPath) {
    if (!tPath || !tPath[0]) return false;
    return strcmp(tPath, "/assets/img/logo-dark.svg") == 0 || strcmp(tPath, "/assets/img/logo-dev-dark.svg") == 0 || strcmp(tPath, "/assets/img/logo-mini-dark.svg") == 0;
  }

  String Dashboard_::CopyProgmemText(const char *tData) {
    String tText;
    if (!tData) return tText;
    const size_t tLength = strlen_P(tData);
    tText.reserve(tLength + 1);
    for (size_t tIndex = 0; tIndex < tLength; tIndex++) {
      tText += static_cast<char>(pgm_read_byte(tData + tIndex));
    }
    return tText;
  }

  String Dashboard_::BuildDarkLogoSvg(const char *tLightSvgData) {
    String tSvg = CopyProgmemText(tLightSvgData);
    if (!tSvg.length()) return tSvg;
    tSvg.replace("#222530", "#ffffff");
    return tSvg;
  }

  String Dashboard_::ResolveSafeRedirectTarget(AsyncWebServerRequest *tRequest, const String &tFallback) {
    String tRedirectTarget;
    if (!TryGetRequestValue(tRequest, "redirect", tRedirectTarget)) return tFallback;
    tRedirectTarget.trim();
    if (!tRedirectTarget.length()) return tFallback;
    if (tRedirectTarget.indexOf('\r') >= 0 || tRedirectTarget.indexOf('\n') >= 0) return tFallback;
    if (tRedirectTarget.startsWith("http://") || tRedirectTarget.startsWith("https://") || tRedirectTarget.startsWith("//") || tRedirectTarget.startsWith("\\")) return tFallback;
    if (!tRedirectTarget.startsWith("/")) tRedirectTarget = String("/") + tRedirectTarget;
    const int tHashPos = tRedirectTarget.indexOf('#');
    if (tHashPos >= 0) tRedirectTarget = tRedirectTarget.substring(0, tHashPos);
    if (!tRedirectTarget.length() || tRedirectTarget == "/") return String("/index.html");
    String tPathOnly = tRedirectTarget;
    const int tQueryPos = tPathOnly.indexOf('?');
    if (tQueryPos >= 0) tPathOnly = tPathOnly.substring(0, tQueryPos);
    if (!tPathOnly.length() || tPathOnly == "/") return String("/index.html");
    if (tPathOnly.equalsIgnoreCase("/login") || tPathOnly.equalsIgnoreCase("/login.html")) return String("/index.html");
    if (tPathOnly.startsWith("/api/") || tPathOnly.startsWith("/assets/") || tPathOnly.startsWith("/lang/")) return tFallback;
    const SDashboardPageDefinition *tPage = DashboardPages::FindByPath(tPathOnly);
    if (tPage && tPage->ShowSidebar) return tRedirectTarget;
    if (tPathOnly.equalsIgnoreCase("/index.html")) return String("/index.html");
    return tFallback;
  }

  String Dashboard_::BuildLoginRedirectUrl(AsyncWebServerRequest *tRequest, const String &tRequestedPath) {
    String tTargetPath = tRequestedPath;
    if (!tTargetPath.length() && tRequest) tTargetPath = tRequest->url();
    tTargetPath.trim();
    if (!tTargetPath.length() || tTargetPath == "/") tTargetPath = "/index.html";
    if (!tTargetPath.startsWith("/")) tTargetPath = String("/") + tTargetPath;
    return String("/login.html?redirect=") + EncodeUrlComponent(tTargetPath);
  }

  int Dashboard_::ResolveErrorStatusCode(int tRequestedCode) {
    if (tRequestedCode == 403 || tRequestedCode == 404 || tRequestedCode == 500) return tRequestedCode;
    return 404;
  }

  int Dashboard_::ResolveErrorStatusCodeFromRequest(AsyncWebServerRequest *tRequest) {
    if (!tRequest) return 404;
    String tCodeValue;
    if (TryGetRequestValue(tRequest, "code", tCodeValue) || TryGetRequestValue(tRequest, "status", tCodeValue)) {
      tCodeValue.trim();
      if (tCodeValue.length()) return ResolveErrorStatusCode(tCodeValue.toInt());
    }
    return 404;
  }

  const char *Dashboard_::ResolveErrorTitleKey(int tCode) {
    if (tCode == 403) return "error_403_title";
    if (tCode == 500) return "error_500_title";
    return "error_404_title";
  }

  const char *Dashboard_::ResolveErrorMessageKey(int tCode) {
    if (tCode == 403) return "error_403_text";
    if (tCode == 500) return "error_500_text";
    return "error_404_text";
  }

  String Dashboard_::BuildErrorPageMainHtml(int tCode, bool tAuthorized) {
    const char *tTitleKey = ResolveErrorTitleKey(tCode);
    const char *tMessageKey = ResolveErrorMessageKey(tCode);
    const char *tBackLink = tAuthorized ? "/index.html" : "/login.html";
    const char *tBackLabelKey = tAuthorized ? "back_to_dashboard" : "back_to_login_page";
    String tMainHtml;
    tMainHtml.reserve(1024);
    tMainHtml += "<main class=centered><div class=centered-main><div class=\"centered-content login-form\">";
    tMainHtml += "<div class=card><div class=card-body><h1 style=\"font-size:1.8rem\" class=\"mt-2 mb-0\"><i class=\"icon icon-emoji-expressionless\"></i> ";
    tMainHtml += String(tCode);
    tMainHtml += " <span data-t=error_page></span></h1><h4 class=mb-0 data-t=";
    tMainHtml += tTitleKey;
    tMainHtml += "></h4><p class=\"mt-2 mb-3\" data-t=";
    tMainHtml += tMessageKey;
    tMainHtml += "></p><div class=card-form-buttons><a class=\"button button-primary\" href=\"";
    tMainHtml += tBackLink;
    tMainHtml += "\"><i class=\"icon icon-box-arrow-left\"></i><span data-t=";
    tMainHtml += tBackLabelKey;
    tMainHtml += "></span></a></div></div></div></div></div></main>";
    return tMainHtml;
  }

  bool Dashboard_::TryExtractBearerToken(AsyncWebServerRequest *tRequest, char *tToken, size_t tTokenSize) {
    if (!tToken || tTokenSize == 0) return false;
    tToken[0] = '\0';
    if (!tRequest) return false;
    auto tCopyToken = [&](const String &tAuthorizationValue) {
      if (!tAuthorizationValue.startsWith("Bearer ")) return false;
      String tResolvedToken = tAuthorizationValue.substring(7);
      tResolvedToken.trim();
      if (!tResolvedToken.length()) return false;
      strncpy(tToken, tResolvedToken.c_str(), tTokenSize - 1);
      tToken[tTokenSize - 1] = '\0';
      return true;
    };
    if (tRequest->hasHeader("Authorization") && tCopyToken(tRequest->header("Authorization"))) return true;
    return false;
  }

  Dashboard_::SWebSocketClientContext *Dashboard_::GetWebSocketClientContext(AsyncWebSocketClient *tClient, bool tCreate) {
    if (!tClient) return nullptr;
    SWebSocketClientContext *tContext = static_cast<SWebSocketClientContext *>(tClient->_tempObject);
    if (!tContext && tCreate) {
      tContext = new SWebSocketClientContext();
      if (tContext) tClient->_tempObject = tContext;
    }
    return tContext;
  }

  void Dashboard_::ReleaseWebSocketClientContext(AsyncWebSocketClient *tClient) {
    if (!tClient) return;
    SWebSocketClientContext *tContext = static_cast<SWebSocketClientContext *>(tClient->_tempObject);
    if (tContext) delete tContext;
    tClient->_tempObject = nullptr;
  }

  std::vector<String> Dashboard_::GetRequestValues(AsyncWebServerRequest *tRequest, const char *tKey) {
    std::vector<String> tValues;
    if (!tRequest || !tKey || !tKey[0]) return tValues;
    const size_t tParamCount = tRequest->params();
    for (size_t tIndex = 0; tIndex < tParamCount; tIndex++) {
      auto tParameter = tRequest->getParam(tIndex);
      if (!tParameter) continue;
      if (tParameter->name() == tKey) tValues.push_back(tParameter->value());
    }
    return tValues;
  }

  String Dashboard_::ResolvePageKeyFromRequest(AsyncWebServerRequest *tRequest) {
    String tPageKey;
    if (TryGetRequestValue(tRequest, "page", tPageKey)) {
      tPageKey.trim();
      tPageKey.toLowerCase();
      if (DashboardPages::FindByKey(tPageKey.c_str())) return tPageKey;
    }
    if (tRequest && tRequest->hasHeader("Referer")) {
      String tReferer = tRequest->header("Referer");
      const int tProtocolPos = tReferer.indexOf("://");
      if (tProtocolPos >= 0) {
        const int tPathPos = tReferer.indexOf('/', tProtocolPos + 3);
        tReferer = (tPathPos >= 0) ? tReferer.substring(tPathPos) : String("/");
      }
      const int tQueryPos = tReferer.indexOf('?');
      if (tQueryPos >= 0) tReferer = tReferer.substring(0, tQueryPos);
      const SDashboardPageDefinition *tPage = DashboardPages::FindByPath(tReferer);
      if (tPage && tPage->Key) return String(tPage->Key);
    }
    return String("index");
  }

  const char *Dashboard_::ResolveContractPageName(const String &tPageKey) {
    if (tPageKey == "index") return "gallery";
    if (tPageKey == "stats") return "statistics";
    return tPageKey.c_str();
  }

  String Dashboard_::NormalizeTimeZoneLabel(const String &tTimeZoneLabel) {
    String tNormalized = tTimeZoneLabel;
    tNormalized.trim();
    if (!tNormalized.length()) return String("");
    if (tNormalized.length() > 64) tNormalized = tNormalized.substring(0, 64);
    String tSafe;
    tSafe.reserve(tNormalized.length());
    for (size_t tIndex = 0; tIndex < tNormalized.length(); tIndex++) {
      const char tChar = tNormalized[tIndex];
      const bool tAlphaNum = (tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') || (tChar >= '0' && tChar <= '9');
      if (tAlphaNum || tChar == '_' || tChar == '-' || tChar == '/' || tChar == '+' || tChar == ':' || tChar == ' ') tSafe += tChar;
    }
    tSafe.trim();
    return tSafe;
  }

  String Dashboard_::FormatUtcOffsetLabel(int32_t tOffsetMinutes) {
    const int32_t tHours = tOffsetMinutes / 60;
    const int32_t tMinutes = abs(tOffsetMinutes % 60);
    char tBuffer[24] = "UTC+00:00";
    snprintf(tBuffer, sizeof(tBuffer), "UTC%c%02ld:%02ld", tOffsetMinutes >= 0 ? '+' : '-', static_cast<long>(abs(tHours)), static_cast<long>(tMinutes));
    return String(tBuffer);
  }

  int32_t Dashboard_::ResolveUtcOffsetMinutes() {
    const SNTPConfig tNtpConfig = CFG.Get<SNTPConfig>();
    return static_cast<int32_t>((tNtpConfig.GMTOffset + tNtpConfig.DaylightOffset) / 60);
  }

  String Dashboard_::ResolveTimeZoneLabel() {
    const SNTPConfig tNtpConfig = CFG.Get<SNTPConfig>();
    const String tSafeLabel = NormalizeTimeZoneLabel(tNtpConfig.TimeZoneLabel);
    if (tSafeLabel.length()) return tSafeLabel;
    return FormatUtcOffsetLabel(ResolveUtcOffsetMinutes());
  }

  const char *Dashboard_::ResolveBatteryStateKey() {
    if (!BAT.IsAvailable() || !BAT.IsBatteryConnected()) return "no_battery";
    if (BAT.GetPercentage() <= 5) return "empty_battery";
    return "normal_battery";
  }

  String Dashboard_::NormalizeStorageKey(const String &tRequestedStorage) {
    String tStorageKey = tRequestedStorage;
    tStorageKey.trim();
    tStorageKey.toLowerCase();
    tStorageKey.replace("-", "_");
    if (tStorageKey.indexOf("sd") >= 0) return String("sd_card");
    if (tStorageKey.indexOf("little") >= 0 || tStorageKey.indexOf("lfs") >= 0) return String("littlefs");
    return STG.IsSDCard() ? String("sd_card") : String("littlefs");
  }

  String Dashboard_::NormalizeDashboardImageExtension(const String &tExtension) {
    String tNormalizedExtension = tExtension.length() ? tExtension : String(DASHBOARD_IMG_EXT);
    tNormalizedExtension.trim();
    tNormalizedExtension.toLowerCase();
    if (!tNormalizedExtension.length()) tNormalizedExtension = String(DASHBOARD_IMG_EXT);
    if (!tNormalizedExtension.startsWith(".")) tNormalizedExtension = String(".") + tNormalizedExtension;
    if (tNormalizedExtension.length() > 10 || tNormalizedExtension.indexOf('/') >= 0 || tNormalizedExtension.indexOf('\\') >= 0) tNormalizedExtension = String(DASHBOARD_IMG_EXT);
    return tNormalizedExtension;
  }

  bool Dashboard_::IsStrictUploadFileName(const char *tFileName) {
    if (!tFileName) return false;
    const String tExpectedSuffix = String("pic") + NormalizeDashboardImageExtension(String(DASHBOARD_IMG_EXT));
    if (strlen(tFileName) != static_cast<size_t>(tExpectedSuffix.length() + 3)) return false;
    if (tFileName[0] < '0' || tFileName[0] > '9') return false;
    if (tFileName[1] < '0' || tFileName[1] > '9') return false;
    if (tFileName[2] < '0' || tFileName[2] > '9') return false;
    return strcasecmp(tFileName + 3, tExpectedSuffix.c_str()) == 0;
  }

  bool Dashboard_::ResolveNextUploadFileName(const String &tStorageKey, const char *tDirectoryPath, char *tOutputFileName, size_t tOutputFileNameSize) {
    if (!tOutputFileName || tOutputFileNameSize == 0) return false;
    tOutputFileName[0] = '\0';
    if (!tDirectoryPath || !tDirectoryPath[0]) return false;
    const String tImageExtension = NormalizeDashboardImageExtension(String(DASHBOARD_IMG_EXT));
    bool tUsedIndices[1000] = {};
    std::vector<const char *> tFiles = GetStorageFilesByKey(tStorageKey, tDirectoryPath, tImageExtension.c_str());
    for (size_t tFileIndex = 0; tFileIndex < tFiles.size(); tFileIndex++) {
      const char *tFileName = tFiles[tFileIndex];
      if (!IsStrictUploadFileName(tFileName)) continue;
      const uint16_t tIndexValue = static_cast<uint16_t>((tFileName[0] - '0') * 100 + (tFileName[1] - '0') * 10 + (tFileName[2] - '0'));
      if (tIndexValue >= 1 && tIndexValue <= 999) tUsedIndices[tIndexValue] = true;
    }
    for (uint16_t tIndexValue = 1; tIndexValue <= 999; tIndexValue++) {
      if (tUsedIndices[tIndexValue]) continue;
      snprintf(tOutputFileName, tOutputFileNameSize, "%03upic%s", static_cast<unsigned>(tIndexValue), tImageExtension.c_str());
      return true;
    }
    return false;
  }

  void Dashboard_::NormalizeUploadFileNameToStorageExt(char *tFileName, size_t tFileNameSize) {
    if (!tFileName || tFileNameSize == 0) return;
    String tNormalizedFileName = String(tFileName);
    tNormalizedFileName.trim();
    tNormalizedFileName.toLowerCase();
    const int tExtensionPos = tNormalizedFileName.lastIndexOf('.');
    if (tExtensionPos > 0) tNormalizedFileName = tNormalizedFileName.substring(0, tExtensionPos);
    tNormalizedFileName.trim();
    while (tNormalizedFileName.length() > 0 && (tNormalizedFileName.endsWith(".") || tNormalizedFileName.endsWith("_") || tNormalizedFileName.endsWith("-"))) {
      tNormalizedFileName.remove(tNormalizedFileName.length() - 1);
    }
    if (!tNormalizedFileName.length()) tNormalizedFileName = "image";
    tNormalizedFileName += NormalizeDashboardImageExtension(String(DASHBOARD_IMG_EXT));
    strncpy(tFileName, tNormalizedFileName.c_str(), tFileNameSize - 1);
    tFileName[tFileNameSize - 1] = '\0';
  }

  const char *Dashboard_::ResolveImageUploadErrorMessage(bool tIsThumb) {
    return tIsThumb ? "editor_thumb_save_error" : "editor_main_save_error";
  }

  bool Dashboard_::IsStorageAvailableByKey(const String &tStorageKey) {
    return tStorageKey == "sd_card" ? SDC.IsMounted() : LFS.IsMounted();
  }

  String Dashboard_::GetDefaultStorageKey(const SStorageConfig &tStorageConfig) {
    return tStorageConfig.DefaultFileSystem == EFileSystemType::SDCard ? String("sd_card") : String("littlefs");
  }

  bool Dashboard_::IsStorageKeyAllowed(const String &tStorageKey, const SStorageConfig &tStorageConfig) {
    if (tStorageKey == GetDefaultStorageKey(tStorageConfig)) return true;
    return tStorageConfig.FallbackEnabled;
  }

  File Dashboard_::OpenStorageFileByKey(const String &tStorageKey, const char *tPath, const char *tMode, bool tCreate) {
    return tStorageKey == "sd_card" ? SDC.OpenFile(tPath, tMode, tCreate) : LFS.OpenFile(tPath, tMode, tCreate);
  }

  bool Dashboard_::StorageFileExistsByKey(const String &tStorageKey, const char *tPath) {
    return tStorageKey == "sd_card" ? SDC.Exists(tPath) : LFS.Exists(tPath);
  }

  bool Dashboard_::DeleteStorageFileByKey(const String &tStorageKey, const char *tPath) {
    return tStorageKey == "sd_card" ? SDC.DeleteFile(tPath) : LFS.DeleteFile(tPath);
  }

  bool Dashboard_::RenameStorageFileByKey(const String &tStorageKey, const char *tFromPath, const char *tToPath) {
    return tStorageKey == "sd_card" ? SD.rename(tFromPath, tToPath) : LittleFS.rename(tFromPath, tToPath);
  }

  bool Dashboard_::MakeStorageDirByKey(const String &tStorageKey, const char *tPath) {
    return tStorageKey == "sd_card" ? SDC.CreateDir(tPath) : LFS.CreateDir(tPath);
  }

  std::vector<const char *> Dashboard_::GetStorageFilesByKey(const String &tStorageKey, const char *tDirectoryPath, const char *tExtension) {
    return tStorageKey == "sd_card" ? SDC.GetFilesInDir(tDirectoryPath, tExtension) : LFS.GetFilesInDir(tDirectoryPath, tExtension);
  }

  void Dashboard_::InvalidateFileCacheByKey(const String &tStorageKey) {
    if (tStorageKey == "sd_card") SDC.InvalidateFileCache();
    else LFS.InvalidateFileCache();
  }

  String Dashboard_::GetStorageLeafFileName(const char *tPathOrFileName) {
    if (!tPathOrFileName || !tPathOrFileName[0]) return String();
    const char *tLeaf = strrchr(tPathOrFileName, '/');
    return String(tLeaf ? tLeaf + 1 : tPathOrFileName);
  }

  String Dashboard_::ResolveNextStorageFileName(const String &tStorageKey, const char *tDirectoryPath, const char *tExtension, const char *tCurrentFileName) {
    std::vector<const char *> tFiles = GetStorageFilesByKey(tStorageKey, tDirectoryPath, tExtension);
    if (tFiles.empty()) return String();
    const String tCurrentName = GetStorageLeafFileName(tCurrentFileName);
    size_t tNextIndex = 0;
    if (tCurrentName.length()) {
      for (size_t tFileIndex = 0; tFileIndex < tFiles.size(); tFileIndex++) {
        if (GetStorageLeafFileName(tFiles[tFileIndex]) == tCurrentName) {
          tNextIndex = (tFileIndex + 1) % tFiles.size();
          break;
        }
      }
    }
    return GetStorageLeafFileName(tFiles[tNextIndex]);
  }

  bool Dashboard_::ResolveStorageFileReadableSize(const String &tStorageKey, const char *tDirectoryPath, const char *tFileName, char *tOutputBuffer, size_t tOutputBufferSize) {
    if (!tOutputBuffer || tOutputBufferSize == 0) return false;
    strncpy(tOutputBuffer, "N/A", tOutputBufferSize - 1);
    tOutputBuffer[tOutputBufferSize - 1] = '\0';
    if (!tFileName || !tFileName[0]) return false;
    String tPath;
    if (tFileName[0] == '/') tPath = String(tFileName);
    else {
      tPath = String("/");
      tPath += (tDirectoryPath && tDirectoryPath[0]) ? tDirectoryPath : "";
      if (!tPath.endsWith("/")) tPath += "/";
      tPath += tFileName;
    }
    File tFile = OpenStorageFileByKey(tStorageKey, tPath.c_str(), FILE_READ, false);
    if (!tFile) return false;
    const uint64_t tFileSize = tFile.size();
    tFile.close();
    UTL.ByteToReadableSize(tFileSize, tOutputBuffer, tOutputBufferSize);
    return true;
  }

  bool Dashboard_::CopyStorageFileByKey(const String &tSourceStorageKey, const char *tSourcePath, const String &tTargetStorageKey, const char *tTargetPath) {
    File tSourceFile = OpenStorageFileByKey(tSourceStorageKey, tSourcePath, FILE_READ, false);
    if (!tSourceFile) return false;
    File tTargetFile = OpenStorageFileByKey(tTargetStorageKey, tTargetPath, FILE_WRITE, true);
    if (!tTargetFile) {
      tSourceFile.close();
      return false;
    }
    uint8_t tBuffer[1024] = {};
    while (tSourceFile.available()) {
      const size_t tReadLength = tSourceFile.read(tBuffer, sizeof(tBuffer));
      if (tReadLength == 0) break;
      if (tTargetFile.write(tBuffer, tReadLength) != tReadLength) {
        tSourceFile.close();
        tTargetFile.close();
        return false;
      }
    }
    tSourceFile.close();
    tTargetFile.flush();
    tTargetFile.close();
    return true;
  }

  void Dashboard_::AppendIndexDataJson(String &tJson, const SAppConfig &tConfig, const String &tStorageDefaultKey, const String &tImagesDirectory, const String &tImageExtension, uint64_t tSdUsedBytes, uint64_t tSdTotalBytes, uint64_t tLfsUsedBytes, uint64_t tLfsTotalBytes) {
    bool tFirstStorage = true;
    tJson += "\"Storages\":[";
    for (uint8_t tStorageIndex = 0; tStorageIndex < 2; tStorageIndex++) {
      const String tStorageKey = tStorageIndex == 0 ? String("sd_card") : String("littlefs");
      const bool tIsFallbackStorage = tStorageKey != tStorageDefaultKey;
      if (!IsStorageAvailableByKey(tStorageKey) || (tIsFallbackStorage && !tConfig.Storage.FallbackEnabled)) continue;
      if (!tFirstStorage) tJson += ",";
      tFirstStorage = false;
      char tUsedText[24] = "0 B";
      char tTotalText[24] = "0 B";
      const uint64_t tUsedBytes = tStorageKey == "sd_card" ? tSdUsedBytes : tLfsUsedBytes;
      const uint64_t tTotalBytes = tStorageKey == "sd_card" ? tSdTotalBytes : tLfsTotalBytes;
      UTL.ByteToReadableSize(tUsedBytes, tUsedText, sizeof(tUsedText));
      UTL.ByteToReadableSize(tTotalBytes, tTotalText, sizeof(tTotalText));
      tJson += "{\"Name\":\"";
      tJson += tStorageKey;
      tJson += "\",\"Fallback\":\"";
      tJson += (tStorageKey == tStorageDefaultKey) ? "false" : "true";
      tJson += "\",\"TotalSize\":\"";
      tJson += tTotalText;
      tJson += "\",\"UsedSize\":\"";
      tJson += tUsedText;
      tJson += "\",\"Images\":[";
      std::vector<const char *> tFiles = GetStorageFilesByKey(tStorageKey, tImagesDirectory.c_str(), tImageExtension.c_str());
      bool tFirstImage = true;
      for (size_t tFileIndex = 0; tFileIndex < tFiles.size(); tFileIndex++) {
        const char *tFileName = tFiles[tFileIndex];
        if (!tFileName || !tFileName[0]) continue;
        char tReadableSize[24] = "N/A";
        ResolveStorageFileReadableSize(tStorageKey, tImagesDirectory.c_str(), tFileName, tReadableSize, sizeof(tReadableSize));
        if (!tFirstImage) tJson += ",";
        tFirstImage = false;
        tJson += "{\"Image\":\"";
        tJson += EscapeJsonText(String(tFileName));
        tJson += "\",\"Size\":\"";
        tJson += EscapeJsonText(String(tReadableSize));
        tJson += "\",\"Default\":";
        tJson += (tStorageKey == tStorageDefaultKey && tConfig.Display.CurrentFile == String(tFileName)) ? "true" : "false";
        tJson += ",\"Preview\":\"/api/images/thumbs/";
        tJson += EscapeJsonText(String(tFileName));
        tJson += "?storage=";
        tJson += tStorageKey;
        tJson += "\"}";
      }
      tJson += "]}";
    }
    tJson += "]";
  }

  void Dashboard_::AppendStatsDataJson(String &tJson, const SAppConfig &tConfig, uint64_t tSdUsedBytes, uint64_t tSdTotalBytes, uint64_t tLfsUsedBytes, uint64_t tLfsTotalBytes) {
    const uint32_t tUptime = millis() / 1000;
    const String tOrientationKey = (tConfig.Display.Rotate == 90 || tConfig.Display.Rotate == 270) ? String("portrait") : String("landscape");
    esp_chip_info_t tChipInfo = {};
    esp_chip_info(&tChipInfo);
    const uint32_t tRevisionMajor = static_cast<uint32_t>(tChipInfo.revision / 100);
    const uint32_t tRevisionMinor = static_cast<uint32_t>(tChipInfo.revision % 100);
    char tTemperatureText[24] = "0.0 °C";
    snprintf(tTemperatureText, sizeof(tTemperatureText), "%.1f °C", temperatureRead());
    const uint32_t tTotalDram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t tFreeDram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const uint32_t tUsedDram = tTotalDram >= tFreeDram ? (tTotalDram - tFreeDram) : 0;
    const uint32_t tTotalIram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_EXEC);
    const uint32_t tFreeIram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_EXEC);
    const uint32_t tUsedIram = tTotalIram >= tFreeIram ? (tTotalIram - tFreeIram) : 0;
    const uint32_t tTotalPsram = ESP.getPsramSize();
    const uint32_t tFreePsram = ESP.getFreePsram();
    const uint32_t tUsedPsram = tTotalPsram >= tFreePsram ? (tTotalPsram - tFreePsram) : 0;
    char tDramUsedText[24] = "0 B";
    char tDramTotalText[24] = "0 B";
    char tIramUsedText[24] = "0 B";
    char tIramTotalText[24] = "0 B";
    char tPsramUsedText[24] = "0 B";
    char tPsramTotalText[24] = "0 B";
    char tHeapFreeText[24] = "0 B";
    UTL.ByteToReadableSize(tUsedDram, tDramUsedText, sizeof(tDramUsedText));
    UTL.ByteToReadableSize(tTotalDram, tDramTotalText, sizeof(tDramTotalText));
    UTL.ByteToReadableSize(tUsedIram, tIramUsedText, sizeof(tIramUsedText));
    UTL.ByteToReadableSize(tTotalIram, tIramTotalText, sizeof(tIramTotalText));
    UTL.ByteToReadableSize(tUsedPsram, tPsramUsedText, sizeof(tPsramUsedText));
    UTL.ByteToReadableSize(tTotalPsram, tPsramTotalText, sizeof(tPsramTotalText));
    UTL.ByteToReadableSize(ESP.getFreeHeap(), tHeapFreeText, sizeof(tHeapFreeText));
    nvs_stats_t tNvsStats = {};
    nvs_get_stats("nvs", &tNvsStats);
    char tFlashSizeText[24] = "0 B";
    char tProgramCodeText[24] = "0 B";
    char tProgramCodeTotalText[24] = "0 B";
    char tLittleFsUsedText[24] = "0 B";
    char tLittleFsTotalText[24] = "0 B";
    char tSdCardUsedText[24] = "0 B";
    char tSdCardTotalText[24] = "0 B";
    const uint32_t tSketchUsedBytes = ESP.getSketchSize();
    const esp_partition_t *tRunningPartition = esp_ota_get_running_partition();
    uint32_t tSketchTotalBytes = tSketchUsedBytes + ESP.getFreeSketchSpace();
    if (tRunningPartition && tRunningPartition->size > 0) tSketchTotalBytes = static_cast<uint32_t>(tRunningPartition->size);
    UTL.ByteToReadableSize(ESP.getFlashChipSize(), tFlashSizeText, sizeof(tFlashSizeText));
    UTL.ByteToReadableSize(tSketchUsedBytes, tProgramCodeText, sizeof(tProgramCodeText));
    UTL.ByteToReadableSize(tSketchTotalBytes, tProgramCodeTotalText, sizeof(tProgramCodeTotalText));
    if (tLfsTotalBytes > 0) {
      UTL.ByteToReadableSize(tLfsUsedBytes, tLittleFsUsedText, sizeof(tLittleFsUsedText));
      UTL.ByteToReadableSize(tLfsTotalBytes, tLittleFsTotalText, sizeof(tLittleFsTotalText));
    }
    const bool tLittleFsMounted = tLfsTotalBytes > 0;
    const bool tSdCardMounted = tSdTotalBytes > 0;
    if (tSdCardMounted) {
      UTL.ByteToReadableSize(tSdUsedBytes, tSdCardUsedText, sizeof(tSdCardUsedText));
      UTL.ByteToReadableSize(tSdTotalBytes, tSdCardTotalText, sizeof(tSdCardTotalText));
    }
    const String tDefaultStorageKey = GetDefaultStorageKey(tConfig.Storage);
    const String tPrimaryStorageKey = tDefaultStorageKey;
    const String tSecondaryStorageKey = tDefaultStorageKey == "sd_card" ? String("littlefs") : String("sd_card");
    const bool tPrimaryStorageIsSd = tPrimaryStorageKey == "sd_card";
    const bool tSecondaryStorageIsSd = tSecondaryStorageKey == "sd_card";
    const bool tPrimaryStorageMounted = tPrimaryStorageIsSd ? tSdCardMounted : tLittleFsMounted;
    const bool tSecondaryStorageMounted = tSecondaryStorageIsSd ? tSdCardMounted : tLittleFsMounted;
    const String tPrimaryStorageLabelKey = (tConfig.Storage.FallbackEnabled && tPrimaryStorageKey != tDefaultStorageKey) ? (tPrimaryStorageIsSd ? String("sd_card_fallback") : String("littlefs_fallback")) : (tPrimaryStorageIsSd ? String("sd_card") : String("littlefs"));
    const String tSecondaryStorageLabelKey = (tConfig.Storage.FallbackEnabled && tSecondaryStorageKey != tDefaultStorageKey) ? (tSecondaryStorageIsSd ? String("sd_card_fallback") : String("littlefs_fallback")) : (tSecondaryStorageIsSd ? String("sd_card") : String("littlefs"));
    const char *tPrimaryStorageUsedText = tPrimaryStorageIsSd ? tSdCardUsedText : tLittleFsUsedText;
    const char *tPrimaryStorageTotalText = tPrimaryStorageIsSd ? tSdCardTotalText : tLittleFsTotalText;
    const char *tSecondaryStorageUsedText = tSecondaryStorageIsSd ? tSdCardUsedText : tLittleFsUsedText;
    const char *tSecondaryStorageTotalText = tSecondaryStorageIsSd ? tSdCardTotalText : tLittleFsTotalText;
    const wifi_mode_t tWifiMode = WiFi.getMode();
    const bool tWifiEnabled = (tWifiMode != WIFI_OFF);
    const bool tWifiRssiValid = (tWifiMode == WIFI_STA || tWifiMode == WIFI_AP_STA) && WiFi.status() == WL_CONNECTED;
    char tWifiSignalText[24] = "N/A";
    if (tWifiRssiValid) snprintf(tWifiSignalText, sizeof(tWifiSignalText), "%d dBm", static_cast<int>(WiFi.RSSI()));
    const char *tWifiModeKey = "not_available";
    if (tWifiMode == WIFI_AP || tWifiMode == WIFI_AP_STA) tWifiModeKey = "ap_mode";
    else if (tWifiMode == WIFI_STA) tWifiModeKey = "sta_mode";
    const esp_bt_controller_status_t tBluetoothStatus = esp_bt_controller_get_status();
    const bool tBluetoothEnabled = (tBluetoothStatus == ESP_BT_CONTROLLER_STATUS_ENABLED);
    const bool tBatteryConnected = BAT.IsAvailable() && BAT.IsBatteryConnected();
    char tBatteryText[32] = "0% [0.00V]";
    snprintf(tBatteryText, sizeof(tBatteryText), "%u%% [%.2fV]", static_cast<unsigned>(BAT.GetPercentage()), static_cast<float>(BAT.GetVoltage()) / 1000.0f);
    tJson += "\"Statistics\":{\"UptimeSeconds\":";
    tJson += String(tUptime);
    tJson += ",\"Sections\":[";
    tJson += "{\"ShowLabel\":false,\"CardClass\":\"card-active\",\"Rows\":[{\"Icon\":\"icon-caret-right-fill\",\"LabelKey\":\"application\",\"Value\":\"";
    tJson += EscapeJsonText(tConfig.Device.Name + String(" ") + tConfig.Device.Version);
    tJson += "\"}]},";
    tJson += "{\"LabelKey\":\"hardware\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"hardware\",\"Value\":\"";
    tJson += EscapeJsonText(String(ESP.getChipModel()) + String(" rev. ") + String(tRevisionMajor) + String(".") + String(tRevisionMinor));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"frequency\",\"Value\":\"";
    tJson += String(ESP.getCpuFreqMHz());
    tJson += " MHz\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"temperature\",\"Value\":\"";
    tJson += EscapeJsonText(String(tTemperatureText));
    tJson += "\"}]},";
    tJson += "{\"LabelKey\":\"memory\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"dram\",\"Value\":\"";
    tJson += EscapeJsonText(String(tDramUsedText) + String(" / ") + String(tDramTotalText));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"iram\",\"Value\":\"";
    tJson += EscapeJsonText(String(tIramUsedText) + String(" / ") + String(tIramTotalText));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"psram\",\"Value\":\"";
    tJson += EscapeJsonText(String(tPsramUsedText) + String(" / ") + String(tPsramTotalText));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"free_heap\",\"Value\":\"";
    tJson += EscapeJsonText(String(tHeapFreeText));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"nvs\",\"Value\":\"";
    tJson += EscapeJsonText(String(static_cast<unsigned>(tNvsStats.used_entries)) + String(" / ") + String(static_cast<unsigned>(tNvsStats.total_entries)));
    tJson += "\",\"ValueSuffixKey\":\"entries\"}]},";
    tJson += "{\"LabelKey\":\"display\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"display_type\",\"ValueKey\":\"spectra6_eink\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"orientation\",\"ValueKey\":\"";
    tJson += tOrientationKey;
    tJson += "\"}]},";
    tJson += "{\"LabelKey\":\"flash\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"flash_size\",\"Value\":\"";
    tJson += EscapeJsonText(String(tFlashSizeText));
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"program_code\",\"Value\":\"";
    tJson += EscapeJsonText(String(tProgramCodeText) + String(" / ") + String(tProgramCodeTotalText));
    tJson += "\"}]},";
    tJson += "{\"LabelKey\":\"storage\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"";
    tJson += tPrimaryStorageLabelKey;
    tJson += "\",";
    if (tPrimaryStorageMounted) {
      tJson += "\"Value\":\"";
      tJson += EscapeJsonText(String(tPrimaryStorageUsedText) + String(" / ") + String(tPrimaryStorageTotalText));
      tJson += "\"";
    } else tJson += tPrimaryStorageIsSd ? "\"ValueKey\":\"no_sd_card\"" : "\"ValueKey\":\"not_available\"";
    tJson += "},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"";
    tJson += tSecondaryStorageLabelKey;
    tJson += "\",";
    if (tSecondaryStorageMounted) {
      tJson += "\"Value\":\"";
      tJson += EscapeJsonText(String(tSecondaryStorageUsedText) + String(" / ") + String(tSecondaryStorageTotalText));
      tJson += "\"";
    } else tJson += tSecondaryStorageIsSd ? "\"ValueKey\":\"no_sd_card\"" : "\"ValueKey\":\"not_available\"";
    tJson += "}]},";
    tJson += "{\"LabelKey\":\"connection\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"wifi\",\"ValueKey\":\"";
    tJson += tWifiEnabled ? "yes" : "no";
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"wifi_mode\",\"ValueKey\":\"";
    tJson += tWifiModeKey;
    tJson += "\"},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"wifi_signal\",";
    if (tWifiRssiValid) {
      tJson += "\"Value\":\"";
      tJson += EscapeJsonText(String(tWifiSignalText));
      tJson += "\"";
    } else tJson += "\"ValueKey\":\"not_available\"";
    tJson += "},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"bluetooth\",\"ValueKey\":\"";
    tJson += tBluetoothEnabled ? "enabled_state" : "not_enabled_state";
    tJson += "\"}]},";
    tJson += "{\"LabelKey\":\"power\",\"Rows\":[{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"battery\",";
    if (tBatteryConnected) {
      tJson += "\"Value\":\"";
      tJson += EscapeJsonText(String(tBatteryText));
      tJson += "\"";
    } else tJson += "\"ValueKey\":\"no_battery\"";
    tJson += "},{\"Icon\":\"icon-caret-right\",\"LabelKey\":\"uptime\",\"ValueId\":\"stats-uptime-value\",\"UptimeSeconds\":";
    tJson += String(tUptime);
    tJson += ",\"Value\":\"";
    tJson += String(tUptime);
    tJson += " s\"}]}]}";
  }

  void Dashboard_::AppendOtherPageDataJson(String &tJson, const String &tPageKey, const SAppConfig &tConfig, const String &tDashboardLanguage, const String &tStorageDefaultKey, unsigned long tTimestamp, int32_t tUtcOffsetMinutes, const String &tTimeZone) {
    if (tPageKey == "display") {
      tJson += "\"Display\":{\"Brightness\":";
      tJson += String(tConfig.Display.JpgBrightness);
      tJson += ",\"Contrast\":";
      tJson += String(tConfig.Display.JpgContrast);
      tJson += ",\"Gamma\":";
      tJson += String(tConfig.Display.JpgGamma);
      tJson += ",\"Rotation\":";
      tJson += String(tConfig.Display.Rotate);
      tJson += "}";
    } else if (tPageKey == "network") {
      tJson += "\"Connection\":{\"Mode\":\"";
      tJson += tConfig.Connection.ApModeEnable ? "AP" : "STA";
      tJson += "\",\"Ap\":{\"Enabled\":";
      tJson += tConfig.Connection.ApModeEnable ? "true" : "false";
      tJson += ",\"Ssid\":\"";
      tJson += EscapeJsonText(tConfig.Connection.ApSsid);
      tJson += "\",\"Password\":\"";
      tJson += EscapeJsonText(tConfig.Connection.ApPassword);
      tJson += "\",\"IpAddress\":\"";
      tJson += EscapeJsonText(tConfig.Connection.ApIp);
      tJson += "\",\"Gateway\":\"";
      tJson += EscapeJsonText(tConfig.Connection.ApGateway);
      tJson += "\",\"SubnetMask\":\"";
      tJson += EscapeJsonText(tConfig.Connection.ApSubnet);
      tJson += "\"},\"Sta\":{\"Enabled\":";
      tJson += tConfig.Connection.ApModeEnable ? "false" : "true";
      tJson += ",\"Ssid\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaSsid);
      tJson += "\",\"Password\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaPassword);
      tJson += "\",\"StaticIpEnabled\":";
      tJson += tConfig.Connection.StaIpEnable ? "true" : "false";
      tJson += ",\"IpAddress\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaIp);
      tJson += "\",\"Gateway\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaGateway);
      tJson += "\",\"SubnetMask\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaSubnet);
      tJson += "\",\"PrimaryDns\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaPrimaryDns);
      tJson += "\",\"SecondaryDns\":\"";
      tJson += EscapeJsonText(tConfig.Connection.StaSecondaryDns);
      tJson += "\"}}";
    } else if (tPageKey == "mdns") {
      tJson += "\"Mdns\":{\"Enabled\":";
      tJson += tConfig.Connection.MdnsEnable ? "true" : "false";
      tJson += ",\"HostName\":\"";
      tJson += EscapeJsonText(tConfig.Connection.MdnsName);
      tJson += "\"}";
    } else if (tPageKey == "ntp") {
      tJson += "\"Ntp\":{\"Server\":\"";
      tJson += EscapeJsonText(tConfig.Ntp.Server);
      tJson += "\",\"GmtOffsetSec\":";
      tJson += String(static_cast<long>(tConfig.Ntp.GMTOffset));
      tJson += ",\"DaylightOffsetSec\":";
      tJson += String(static_cast<long>(tConfig.Ntp.DaylightOffset));
      tJson += ",\"Port\":";
      tJson += String(static_cast<unsigned>(tConfig.Ntp.NtpPort));
      tJson += ",\"UpdateInterval\":";
      tJson += String(tConfig.Ntp.LowPowerSyncIntervalSec * 1000UL);
      tJson += "}";
    } else if (tPageKey == "datetime") {
      tJson += "\"DateTime\":{\"TimeStamp\":";
      tJson += String(tTimestamp);
      tJson += ",\"UtcOffsetMinutes\":";
      tJson += String(tUtcOffsetMinutes);
      tJson += ",\"TimeZone\":\"";
      tJson += EscapeJsonText(tTimeZone);
      tJson += "\"}";
    } else if (tPageKey == "wakeup") {
      tJson += "\"Timer\":{\"WakeUp\":";
      tJson += String(static_cast<uint8_t>(tConfig.Timer.WakeUp));
      tJson += ",\"WakeUpHour\":";
      tJson += String(tConfig.Timer.WakeUpHour);
      tJson += "}";
    } else if (tPageKey == "language" || tPageKey == "user") {
      std::vector<String> tDashboardEnabledLanguages = tConfig.Dashboard.EnabledLanguages;
      DashboardUtils_::NormalizeEnabledLanguages(tDashboardEnabledLanguages, tDashboardLanguage);
      tJson += "\"Dashboard\":{";
      if (tPageKey == "user") {
        tJson += "\"User\":\"";
        tJson += EscapeJsonText(tConfig.Dashboard.User.length() ? tConfig.Dashboard.User : String("admin"));
        tJson += "\",\"HasPassword\":";
        tJson += tConfig.Dashboard.Password.isEmpty() ? "false" : "true";
        tJson += ",";
      }
      tJson += "\"Language\":\"";
      tJson += EscapeJsonText(tDashboardLanguage);
      tJson += "\",\"EnabledLanguages\":[";
      for (size_t tIndex = 0; tIndex < tDashboardEnabledLanguages.size(); tIndex++) {
        if (tIndex > 0) tJson += ",";
        tJson += "\"";
        tJson += EscapeJsonText(tDashboardEnabledLanguages[tIndex]);
        tJson += "\"";
      }
      if (tPageKey == "user") {
        tJson += "],\"Theme\":\"";
        tJson += EscapeJsonText(tConfig.Dashboard.Theme);
        tJson += "\",\"ShowDescription\":";
        tJson += tConfig.Dashboard.ShowDescription ? "true" : "false";
        tJson += ",\"SoundEnabled\":";
        tJson += tConfig.Device.SoundEnabled ? "true" : "false";
        tJson += "},\"Storage\":{\"FallbackEnabled\":";
        tJson += tConfig.Storage.FallbackEnabled ? "true" : "false";
        tJson += ",\"Default\":\"";
        tJson += EscapeJsonText(tStorageDefaultKey);
        tJson += "\"}";
      } else tJson += "]}";
    }
  }

  String Dashboard_::BuildErrorHtml(int tCode, bool tAuthorized) {
    SAppConfig tConfig = {};
    {
      Guard tLock;
      tConfig = Instance().mCfg;
    }
    const String tMainHtml = BuildErrorPageMainHtml(tCode, tAuthorized);
    const SDashboardPageDefinition tErrorPage = {
      "error",
      "error_page",
      "icon-exclamation-triangle",
      "/error.html",
      "/error",
      nullptr,
      tMainHtml.c_str(),
      nullptr,
      false,
      false
    };
    return DashboardLayout_::BuildPageDocument(tErrorPage, tConfig);
  }

  void Dashboard_::HandleEvents() {
    AsyncWebSocket *tWebSocket = nullptr;
    bool tBroadcastStatus = false;
    bool tRefreshStatusCache = false;
    FDefaultCallback tResetCallback = nullptr;
    FDefaultCallback tRebootCallback = nullptr;
    {
      Guard tLock;
      PurgeExpired();
      tWebSocket = mWebSocket;
      const uint32_t tNow = millis();
      if (static_cast<uint32_t>(tNow - mLastStatusCacheRefreshMs) >= kStatusCacheRefreshIntervalMs) {
        tRefreshStatusCache = true;
        mLastStatusCacheRefreshMs = tNow;
      }
      if (tWebSocket && static_cast<uint32_t>(tNow - mLastBroadcast) >= kBroadcastIntervalMs) {
        tBroadcastStatus = true;
        mLastBroadcast = tNow;
      }
      if (mResetPending && (mResetPendingSince == 0 || static_cast<uint32_t>(tNow - mResetPendingSince) >= kRestartActionDelayMs)) {
        mResetPending = false;
        mResetPendingSince = 0;
        tResetCallback = mOnReset;
      }
      if (mRestartPending && (mRestartPendingSince == 0 || static_cast<uint32_t>(tNow - mRestartPendingSince) >= kRestartActionDelayMs)) {
        mRestartPending = false;
        mRestartPendingSince = 0;
        tRebootCallback = mOnReboot;
      }
    }
    if (tRefreshStatusCache) {
      char tStatusJson[kSmallJsonSize] = "{}";
      BuildStatusJson(tStatusJson, sizeof(tStatusJson));
      Guard tLock;
      strncpy(mCachedStatusJson, tStatusJson, sizeof(mCachedStatusJson) - 1);
      mCachedStatusJson[sizeof(mCachedStatusJson) - 1] = '\0';
    }
    if (tWebSocket) tWebSocket->cleanupClients();
    RefreshDataCachesIfNeeded(false);
    if (tBroadcastStatus) BroadcastStatus();
    if (tResetCallback) tResetCallback();
    if (tRebootCallback) tRebootCallback();
  }

  void Dashboard_::RefreshDataCachesIfNeeded(bool tForce) {
    RefreshStatisticsCacheIfNeeded(tForce);
    RefreshGalleryCacheIfNeeded(tForce);
  }

  void Dashboard_::MarkGalleryCacheDirty() {
    Guard tLock;
    mIndexDataCacheDirty = true;
    mStatsCacheDirty = true;
  }

  void Dashboard_::RefreshGalleryCacheIfNeeded(bool tForce) {
    SAppConfig tConfig = {};
    bool tNeedRefresh = false;
    {
      Guard tLock;
      const uint32_t tNow = millis();
      const bool tCacheEmpty = mCachedIndexStoragesJson.length() == 0;
      const bool tRefreshByInterval = static_cast<uint32_t>(tNow - mLastIndexDataCacheRefreshMs) >= kIndexDataCacheRefreshIntervalMs;
      tNeedRefresh = tForce || mIndexDataCacheDirty || tCacheEmpty || tRefreshByInterval;
      if (!tNeedRefresh) return;
      mIndexDataCacheDirty = false;
      mLastIndexDataCacheRefreshMs = tNow;
      tConfig = mCfg;
    }
    const String tStorageDefaultKey = GetDefaultStorageKey(tConfig.Storage);
    const String tImagesDirectory = tConfig.Display.ImagesDir.length() ? tConfig.Display.ImagesDir : String(IMAGES_DIR);
    const String tImageExtension = tConfig.Display.ImageExt.length() ? tConfig.Display.ImageExt : String(IMAGE_EXT);
    const uint64_t tSdTotalBytes = SDC.IsMounted() ? SDC.TotalBytes() : 0;
    const uint64_t tSdUsedBytes = SDC.IsMounted() ? SDC.UsedBytes() : 0;
    const uint64_t tLfsTotalBytes = LFS.IsMounted() ? LFS.TotalBytes() : 0;
    const uint64_t tLfsUsedBytes = LFS.IsMounted() ? LFS.UsedBytes() : 0;
    uint32_t tGalleryCount = 0;
    if (IsStorageAvailableByKey(tStorageDefaultKey)) {
      tGalleryCount = static_cast<uint32_t>(GetStorageFilesByKey(tStorageDefaultKey, tImagesDirectory.c_str(), tImageExtension.c_str()).size());
    }
    String tStoragesJson;
    tStoragesJson.reserve(12288);
    AppendIndexDataJson(tStoragesJson, tConfig, tStorageDefaultKey, tImagesDirectory, tImageExtension, tSdUsedBytes, tSdTotalBytes, tLfsUsedBytes, tLfsTotalBytes);
    Guard tLock;
    mCachedIndexStoragesJson = tStoragesJson.length() ? tStoragesJson : String("\"Storages\":[]");
    mCachedIndexGalleryCount = tGalleryCount;
    mCachedSdTotalBytes = tSdTotalBytes;
    mCachedSdUsedBytes = tSdUsedBytes;
    mCachedLfsTotalBytes = tLfsTotalBytes;
    mCachedLfsUsedBytes = tLfsUsedBytes;
    mLastStorageStatsCacheMs = millis();
  }

  void Dashboard_::RefreshStatisticsCacheIfNeeded(bool tForce) {
    SAppConfig tConfig = {};
    uint64_t tSdTotalBytes = 0;
    uint64_t tSdUsedBytes = 0;
    uint64_t tLfsTotalBytes = 0;
    uint64_t tLfsUsedBytes = 0;
    {
      Guard tLock;
      const uint32_t tNow = millis();
      const bool tRefreshByInterval = static_cast<uint32_t>(tNow - mLastStatsCacheRefreshMs) >= kStatsCacheRefreshIntervalMs;
      if (!tForce && !mStatsCacheDirty && !tRefreshByInterval) return;
      mStatsCacheDirty = false;
      mLastStatsCacheRefreshMs = tNow;
      tConfig = mCfg;
      tSdTotalBytes = mCachedSdTotalBytes;
      tSdUsedBytes = mCachedSdUsedBytes;
      tLfsTotalBytes = mCachedLfsTotalBytes;
      tLfsUsedBytes = mCachedLfsUsedBytes;
    }
    char tStatsJson[kStatsJsonSize] = "{}";
    BuildStatsJson(tStatsJson, sizeof(tStatsJson));
    String tPageStatsJson;
    tPageStatsJson.reserve(8192);
    AppendStatsDataJson(tPageStatsJson, tConfig, tSdUsedBytes, tSdTotalBytes, tLfsUsedBytes, tLfsTotalBytes);
    Guard tLock;
    strncpy(mCachedStatsJson, tStatsJson, sizeof(mCachedStatsJson) - 1);
    mCachedStatsJson[sizeof(mCachedStatsJson) - 1] = '\0';
    mCachedPageStatsDataJson = tPageStatsJson.length() ? tPageStatsJson : String("\"Statistics\":{}");
  }

  bool Dashboard_::ValidateToken(const char *tToken) {
    Guard tLock;
    if (!tToken || !tToken[0]) return false;
    PurgeExpired();
    const uint32_t tNow = millis();
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (strcmp(mSessions[tIndex].Token, tToken) == 0) {
        mSessions[tIndex].LastSeenMs = tNow;
        return true;
      }
    }
    return false;
  }

  bool Dashboard_::AuthorizeRequest(AsyncWebServerRequest *tRequest) {
    if (!tRequest) return false;
    if (tRequest->hasHeader("Authorization")) {
      String tAuthorizationHeader = tRequest->header("Authorization");
      if (tAuthorizationHeader.startsWith("Bearer ")) {
        String tToken = tAuthorizationHeader.substring(7);
        if (ValidateToken(tToken.c_str())) return true;
      }
    }
    if (tRequest->hasHeader("Cookie")) {
      const String tCookieToken = DashboardUtils_::ExtractCookieValue(tRequest->header("Cookie"), kSessionCookieName);
      if (tCookieToken.length() && ValidateToken(tCookieToken.c_str())) return true;
    }
    return false;
  }

  const char *Dashboard_::CreateSession() {
    Guard tLock;
    PurgeExpired();
    uint8_t tSessionSlot = kMaxSessions;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) {
        tSessionSlot = tIndex;
        break;
      }
    }
    if (tSessionSlot == kMaxSessions) {
      uint32_t tOldestTick = UINT32_MAX;
      for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
        if (mSessions[tIndex].LastSeenMs <= tOldestTick) {
          tOldestTick = mSessions[tIndex].LastSeenMs;
          tSessionSlot = tIndex;
        }
      }
    }
    DashboardUtils_::RandomToken(mSessions[tSessionSlot].Token);
    mSessions[tSessionSlot].LastSeenMs = millis();
    mSessions[tSessionSlot].Active = true;
    return mSessions[tSessionSlot].Token;
  }

  void Dashboard_::DestroySession(const char *tToken) {
    Guard tLock;
    if (!tToken || !tToken[0]) return;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (strcmp(mSessions[tIndex].Token, tToken) == 0) {
        memset(mSessions[tIndex].Token, 0, sizeof(mSessions[tIndex].Token));
        mSessions[tIndex].LastSeenMs = 0;
        mSessions[tIndex].Active = false;
        return;
      }
    }
  }

  void Dashboard_::PurgeExpired() {
    Guard tLock;
    const uint32_t tNow = millis();
    const uint32_t tSessionTtlMs = kSessionTtlSec * 1000UL;
    for (uint8_t tIndex = 0; tIndex < kMaxSessions; tIndex++) {
      if (!mSessions[tIndex].Active) continue;
      if (Utils_::HasElapsedMs(mSessions[tIndex].LastSeenMs, tNow, tSessionTtlMs)) {
        memset(mSessions[tIndex].Token, 0, sizeof(mSessions[tIndex].Token));
        mSessions[tIndex].LastSeenMs = 0;
        mSessions[tIndex].Active = false;
      }
    }
  }

  void Dashboard_::RegisterRoutes() {
    const SDashboardPageDefinition *tLoginPage = DashboardPages::FindByKey("login");
    auto tRegisterPageRoute = [this](const char *tPath, ArRequestHandlerFunction tHandler) {
      if (!tPath || !tPath[0]) return;
      mServer.on(AsyncURIMatcher::exact(String(tPath)), HTTP_GET, tHandler);
      String tPathWithSlash = String(tPath);
      if (tPathWithSlash.length() > 1 && !tPathWithSlash.endsWith("/")) {
        tPathWithSlash += "/";
        mServer.on(AsyncURIMatcher::exact(tPathWithSlash), HTTP_GET, tHandler);
      }
    };
    auto tServePage = [this](AsyncWebServerRequest *tRequest, const SDashboardPageDefinition *tPage) {
      if (!tRequest || !tPage) return;
      if (tPage->ShowSidebar && !AuthorizeRequest(tRequest)) {
        const String tPagePath = (tPage->Path && tPage->Path[0]) ? String(tPage->Path) : tRequest->url();
        const String tLoginUrl = BuildLoginRedirectUrl(tRequest, tPagePath);
        tRequest->redirect(tLoginUrl.c_str());
        return;
      }
      if (!tPage->ShowSidebar && AuthorizeRequest(tRequest)) {
        const String tRedirectTarget = ResolveSafeRedirectTarget(tRequest, String("/index.html"));
        tRequest->redirect(tRedirectTarget.c_str());
        return;
      }
      SAppConfig tConfigSnapshot = {};
      {
        Guard tLock;
        tConfigSnapshot = mCfg;
      }
      tRequest->send(200, "text/html;charset=utf-8", DashboardLayout_::BuildPageDocument(*tPage, tConfigSnapshot));
    };
    for (size_t tIndex = 0; tIndex < sizeof(DashboardPages::sPages) / sizeof(DashboardPages::sPages[0]); tIndex++) {
      const SDashboardPageDefinition *tPage = &DashboardPages::sPages[tIndex];
      if (!tPage || !tPage->Key || strcmp(tPage->Key, "login") == 0) continue;
      auto tHandler = [tServePage, tPage](AsyncWebServerRequest *tRequest) {
        tServePage(tRequest, tPage);
      };
      if (tPage->Path && tPage->Path[0]) tRegisterPageRoute(tPage->Path, tHandler);
      if (tPage->AliasPath && tPage->AliasPath[0]) tRegisterPageRoute(tPage->AliasPath, tHandler);
    }
    for (size_t tIndex = 0; tIndex < sizeof(DashboardAssets::sTextAssets) / sizeof(DashboardAssets::sTextAssets[0]); tIndex++) {
      const SDashboardTextAsset *tAsset = &DashboardAssets::sTextAssets[tIndex];
      if (!tAsset || !tAsset->Path || !tAsset->Path[0]) continue;
      mServer.on(tAsset->Path, HTTP_GET, [tAsset](AsyncWebServerRequest *tRequest) {
        if (IsDarkLogoAssetPath(tAsset->Path)) {
          const String tDarkSvg = BuildDarkLogoSvg(tAsset->Data);
          tRequest->send(200, tAsset->MimeType, tDarkSvg);
          return;
        }
        AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, tAsset->MimeType, reinterpret_cast<const uint8_t *>(tAsset->Data), strlen_P(tAsset->Data));
        tResponse->addHeader("Cache-Control", "no-store,no-cache,must-revalidate,post-check=0,pre-check=0");
        tResponse->addHeader("Pragma", "no-cache");
        tResponse->addHeader("Expires", "0");
        tRequest->send(tResponse);
      });
    }
    for (size_t tIndex = 0; tIndex < sizeof(DashboardAssets::sBinaryAssets) / sizeof(DashboardAssets::sBinaryAssets[0]); tIndex++) {
      const SDashboardBinaryAsset *tAsset = &DashboardAssets::sBinaryAssets[tIndex];
      if (!tAsset || !tAsset->Path || !tAsset->Path[0]) continue;
      mServer.on(tAsset->Path, HTTP_GET, [tAsset](AsyncWebServerRequest *tRequest) {
        AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, tAsset->MimeType, reinterpret_cast<const uint8_t *>(tAsset->Data), tAsset->Size);
        tResponse->addHeader("Cache-Control", "no-store,no-cache,must-revalidate,post-check=0,pre-check=0");
        tResponse->addHeader("Pragma", "no-cache");
        tResponse->addHeader("Expires", "0");
        tRequest->send(tResponse);
      });
    }
    for (size_t tIndex = 0; tIndex < sizeof(DashboardLanguages::sLanguageAssets) / sizeof(DashboardLanguages::sLanguageAssets[0]); tIndex++) {
      const SDashboardTextAsset *tAsset = &DashboardLanguages::sLanguageAssets[tIndex];
      if (!tAsset || !tAsset->Path || !tAsset->Path[0]) continue;
      mServer.on(tAsset->Path, HTTP_GET, [tAsset](AsyncWebServerRequest *tRequest) {
        AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, tAsset->MimeType, reinterpret_cast<const uint8_t *>(tAsset->Data), strlen_P(tAsset->Data));
        tResponse->addHeader("Cache-Control", "no-store,no-cache,must-revalidate,post-check=0,pre-check=0");
        tResponse->addHeader("Pragma", "no-cache");
        tResponse->addHeader("Expires", "0");
        tRequest->send(tResponse);
      });
    }
    mServer.on("/", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      tRequest->redirect(AuthorizeRequest(tRequest) ? "/index.html" : "/login.html");
    });
    mServer.on("/login", HTTP_GET, [tServePage, tLoginPage](AsyncWebServerRequest *tRequest) {
      tServePage(tRequest, tLoginPage);
    });
    mServer.on("/login.html", HTTP_GET, [tServePage, tLoginPage](AsyncWebServerRequest *tRequest) {
      tServePage(tRequest, tLoginPage);
    });   
    mServer.on("/login", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogin(tRequest);
    });
    mServer.on("/login.html", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogin(tRequest);
    });
    mServer.on("/error", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      const bool tAuthorized = AuthorizeRequest(tRequest);
      const int tCode = ResolveErrorStatusCodeFromRequest(tRequest);
      tRequest->send(tCode, "text/html;charset=utf-8", BuildErrorHtml(tCode, tAuthorized));
    });
    mServer.on("/error.html", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      const bool tAuthorized = AuthorizeRequest(tRequest);
      const int tCode = ResolveErrorStatusCodeFromRequest(tRequest);
      tRequest->send(tCode, "text/html;charset=utf-8", BuildErrorHtml(tCode, tAuthorized));
    });
    mServer.on("/api/page", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandlePageData(tRequest);
    });
    mServer.on("/api/login", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogin(tRequest);
    });
    mServer.on("/api/logout", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLogout(tRequest);
    });
    mServer.on(AsyncURIMatcher::prefix("/api/images/thumbs/"), HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleImageThumb(tRequest);
    });
    mServer.on(AsyncURIMatcher::prefix("/api/images/thumb/"), HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleImageThumb(tRequest);
    });
    mServer.on(AsyncURIMatcher::prefix("/api/images/"), HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleImageFile(tRequest);
    });
    mServer.on("/api/images", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleImagesList(tRequest);
    });
    mServer.on("/api/images/upload", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleImageDone(tRequest);
    }, [this](AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
      HandleImageUpload(tRequest, tFilename, tIndex, tData, tLength, tFinal);
    });
    mServer.on("/api/images/copy", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleImageCopy(tRequest);
    });
    mServer.on("/api/images/delete", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      String *tBody = static_cast<String *>(tRequest ? tRequest->_tempObject : nullptr);
      const String tJsonBody = tBody ? *tBody : String();
      HandleImageDelete(tRequest, tJsonBody);
      if (tBody) {
        delete tBody;
        tRequest->_tempObject = nullptr;
      }
    }, nullptr, [this](AsyncWebServerRequest *tRequest, uint8_t *tData, size_t tLength, size_t tIndex, size_t tTotal) {
      (void)this;
      if (!tRequest || !tData || tLength == 0) return;
      String *tBody = static_cast<String *>(tRequest->_tempObject);
      if (!tBody && tIndex == 0) {
        tBody = new String();
        tBody->reserve(tTotal);
        tRequest->_tempObject = tBody;
      }
      if (tBody) tBody->concat(reinterpret_cast<const char *>(tData), tLength);
    });
    mServer.on("/api/images/swap", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleImageRename(tRequest);
    });
    mServer.on("/api/images/default", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleImageSetCurrent(tRequest);
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
    mServer.on("/api/display/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleDisplaySave(tRequest);
    });
    mServer.on("/api/network/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleNetworkSave(tRequest);
    });
    mServer.on("/api/mdns/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleMdnsSave(tRequest);
    });
    mServer.on("/api/ntp/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleNtpSave(tRequest, false);
    });
    mServer.on("/api/ntp/sync", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleNtpSave(tRequest, true);
    });
    mServer.on("/api/datetime/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleDateTimeSave(tRequest);
    });
    mServer.on("/api/language/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleLanguageSave(tRequest);
    });
    mServer.on("/api/user/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleUserSave(tRequest);
    });
    mServer.on("/api/user/restore", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleUserRestore(tRequest);
    });
    mServer.on("/api/wakeup/save", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleWakeUpSave(tRequest);
    });
    mServer.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleStats(tRequest);
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
    mServer.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleReboot(tRequest);
    });
    mServer.on("/api/factory/reset", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleFactoryReset(tRequest);
    });
    mServer.on("/api/rtc/sync", HTTP_POST, [this](AsyncWebServerRequest *tRequest) {
      HandleRtcSync(tRequest);
    });
    mServer.on("/api/rtc/now", HTTP_GET, [this](AsyncWebServerRequest *tRequest) {
      HandleRtcNow(tRequest);
    });
    mServer.onNotFound([this](AsyncWebServerRequest *tRequest) {
      if (!tRequest) return;
      const String tUrl = tRequest->url();
      const bool tAuthorized = AuthorizeRequest(tRequest);
      if (tUrl.startsWith("/api/")) {
        if (!tAuthorized) {
          DashboardUtils_::UnauthorizedResponse(tRequest);
          return;
        }
        DashboardUtils_::ErrorResponse(tRequest, 404, "not_found");
        return;
      }
      (void)tUrl;
      tRequest->send(404, "text/html;charset=utf-8", BuildErrorHtml(404, tAuthorized));
    });
  }

  void Dashboard_::OnWebSocketEvent(AsyncWebSocket *tWebSocket, AsyncWebSocketClient *tClient, AwsEventType tType, void *tEventArg, uint8_t *tData, size_t tLength) {
    (void)tWebSocket;
    if (tType == WS_EVT_DISCONNECT) {
      ReleaseWebSocketClientContext(tClient);
      return;
    }
    if (tType == WS_EVT_CONNECT) {
      AsyncWebServerRequest *tRequest = static_cast<AsyncWebServerRequest *>(tEventArg);
      char tToken[65] = "";
      bool tAuthorized = TryExtractBearerToken(tRequest, tToken, sizeof(tToken)) && ValidateToken(tToken);
      if (!tAuthorized && tRequest && tRequest->hasHeader("Cookie")) {
        const String tCookieToken = DashboardUtils_::ExtractCookieValue(tRequest->header("Cookie"), kSessionCookieName);
        if (tCookieToken.length() && ValidateToken(tCookieToken.c_str())) {
          tAuthorized = true;
          strncpy(tToken, tCookieToken.c_str(), sizeof(tToken) - 1);
          tToken[sizeof(tToken) - 1] = '\0';
        }
      }
      SWebSocketClientContext *tContext = GetWebSocketClientContext(tClient, true);
      if (tContext) {
        strncpy(tContext->Token, tAuthorized ? tToken : "", sizeof(tContext->Token) - 1);
        tContext->Token[sizeof(tContext->Token) - 1] = '\0';
      }
      char tStatusJson[kSmallJsonSize] = "";
      {
        Guard tLock;
        strncpy(tStatusJson, mCachedStatusJson, sizeof(tStatusJson) - 1);
        tStatusJson[sizeof(tStatusJson) - 1] = '\0';
      }
      if (!tStatusJson[0]) strncpy(tStatusJson, "{}", sizeof(tStatusJson) - 1);
      if (tClient) tClient->text(tStatusJson);
      if (tAuthorized) xLOG("Websocket authorized client connected");
      else xLOG("Websocket connected without session, status only");
      return;
    }
    if (tType != WS_EVT_DATA || !tData || tLength == 0 || !tClient) return;
    AwsFrameInfo *tFrame = static_cast<AwsFrameInfo *>(tEventArg);
    if (!tFrame || tFrame->opcode != WS_TEXT) return;
    char tMessage[256] = "";
    size_t tCopyLength = tLength;
    if (tCopyLength > sizeof(tMessage) - 1) tCopyLength = sizeof(tMessage) - 1;
    memcpy(tMessage, tData, tCopyLength);
    tMessage[tCopyLength] = '\0';
    OnWebSocketMessage(tClient, tMessage, tCopyLength);
  }

  void Dashboard_::OnWebSocketMessage(AsyncWebSocketClient *tClient, const char *tMessage, size_t tLength) {
    (void)tLength;
    if (!tMessage || !tClient) return;
    if (strstr(tMessage, "\"type\":\"status\"")) {
      char tStatusJson[kSmallJsonSize] = "";
      {
        Guard tLock;
        strncpy(tStatusJson, mCachedStatusJson, sizeof(tStatusJson) - 1);
        tStatusJson[sizeof(tStatusJson) - 1] = '\0';
      }
      if (!tStatusJson[0]) strncpy(tStatusJson, "{}", sizeof(tStatusJson) - 1);
      tClient->text(tStatusJson);
      return;
    }
    if (strstr(tMessage, "\"type\":\"ping\"")) {
      tClient->text("{\"type\":\"ack\",\"ok\":true}");
      return;
    }
    SWebSocketClientContext *tContext = GetWebSocketClientContext(tClient, false);
    const bool tAuthorized = tContext && tContext->Token[0] && ValidateToken(tContext->Token);
    if (!tAuthorized) {
      tClient->text("{\"type\":\"ack\",\"ok\":false,\"error\":\"unauthorized\"}");
      return;
    }
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
      if (tContext && tContext->Token[0]) DestroySession(tContext->Token);
      mRestartPending = true;
      mRestartPendingSince = millis();
      xLOG("Dashboard → reboot requested from websocket");
      tClient->text("{\"type\":\"ack\",\"ok\":true}");
      return;
    }
    tClient->text("{\"type\":\"ack\",\"ok\":false,\"error\":\"unsupported\"}");
  }

  void Dashboard_::BroadcastOtaEvent(const char *tType, const char *tMessage, unsigned tProgress, const char *tState, const char *tModal) {
    if (!mWebSocket) return;
    const char *tResolvedType = (tType && tType[0]) ? tType : "ota_error";
    const char *tResolvedState = (tState && tState[0]) ? tState : "error";
    const bool tHasMessage = tMessage && tMessage[0];
    const bool tHasModal = tModal && tModal[0];
    char tJson[384] = "";
    if (tHasMessage && tHasModal) snprintf(tJson, sizeof(tJson), "{\"type\":\"%s\",\"message\":\"%s\",\"data\":{\"progress\":%u,\"written\":%u,\"total\":%u,\"file\":\"%s\",\"state\":\"%s\",\"modal\":\"%s\",\"display\":\"notification\"}}", tResolvedType, tMessage, tProgress, (unsigned)FWU.GetWritten(), (unsigned)FWU.GetTotal(), FIRMWARE_FILENAME, tResolvedState, tModal);
    else if (tHasMessage) snprintf(tJson, sizeof(tJson), "{\"type\":\"%s\",\"message\":\"%s\",\"data\":{\"progress\":%u,\"written\":%u,\"total\":%u,\"file\":\"%s\",\"state\":\"%s\",\"display\":\"notification\"}}", tResolvedType, tMessage, tProgress, (unsigned)FWU.GetWritten(), (unsigned)FWU.GetTotal(), FIRMWARE_FILENAME, tResolvedState);
    else snprintf(tJson, sizeof(tJson), "{\"type\":\"%s\",\"data\":{\"progress\":%u,\"written\":%u,\"total\":%u,\"file\":\"%s\",\"state\":\"%s\"}}", tResolvedType, tProgress, (unsigned)FWU.GetWritten(), (unsigned)FWU.GetTotal(), FIRMWARE_FILENAME, tResolvedState);
    mWebSocket->textAll(tJson);
  }

  void Dashboard_::BroadcastStatus() {
    if (!mWebSocket) return;
    char tJson[kSmallJsonSize] = "";
    {
      Guard tLock;
      strncpy(tJson, mCachedStatusJson, sizeof(tJson) - 1);
      tJson[sizeof(tJson) - 1] = '\0';
    }
    if (!tJson[0]) strncpy(tJson, "{}", sizeof(tJson) - 1);
    mWebSocket->textAll(tJson);
  }

  void Dashboard_::HandleLogin(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!tRequest) return;
    const String tRedirectUrl = ResolveSafeRedirectTarget(tRequest, String("/index.html"));
    if (!mCachedPassHash[0]) {
      String tResponseJson = "{\"ok\":true,\"error\":false,\"RedirectUrl\":\"";
      tResponseJson += EscapeJsonText(tRedirectUrl);
      tResponseJson += "\"}";
      DashboardUtils_::JsonResponse(tRequest, 200, tResponseJson.c_str());
      return;
    }
    String tUserName;
    String tPassword;
    if (!TryGetRequestValue(tRequest, "name", tUserName) || !TryGetRequestValue(tRequest, "password", tPassword)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "invalid_login");
      return;
    }
    char tPasswordHash[65] = "";
    DashboardUtils_::Sha256Hex(reinterpret_cast<const uint8_t *>(tPassword.c_str()), tPassword.length(), tPasswordHash);
    if (tUserName != String(mCachedUser) || strcasecmp(tPasswordHash, mCachedPassHash) != 0) {
      xLOG("Login failed for user → %s", tUserName.c_str());
      DashboardUtils_::ErrorResponse(tRequest, 401, "invalid_login");
      return;
    }
    const char *tToken = CreateSession();
    if (!tToken) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "internal_error");
      return;
    }
    String tResponseJson = "{\"ok\":true,\"error\":false,\"token\":\"";
    tResponseJson += tToken;
    tResponseJson += "\",\"RedirectUrl\":\"";
    tResponseJson += EscapeJsonText(tRedirectUrl);
    tResponseJson += "\"}";
    xLOG("Login successful for user → %s", tUserName.c_str());
    AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, "application/json", tResponseJson);
    tResponse->addHeader("Cache-Control", "no-store");
    char tSetCookieHeader[192] = "";
    snprintf(tSetCookieHeader, sizeof(tSetCookieHeader), "%s=%s;HttpOnly;SameSite=Lax;Path=/;Max-Age=%u", kSessionCookieName, tToken, static_cast<unsigned>(kSessionTtlSec));
    tResponse->addHeader("Set-Cookie", tSetCookieHeader);
    tRequest->send(tResponse);
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
    if (tRequest->hasHeader("Cookie")) {
      const String tCookieToken = DashboardUtils_::ExtractCookieValue(tRequest->header("Cookie"), kSessionCookieName);
      if (tCookieToken.length()) DestroySession(tCookieToken.c_str());
    }
    xLOG("Logout completed");
    AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, "application/json", "{\"ok\":true,\"error\":false,\"message\":\"logout\",\"RedirectUrl\":\"/login.html\"}");
    tResponse->addHeader("Cache-Control", "no-store");
    tResponse->addHeader("Set-Cookie", String(kSessionCookieName)+"=;HttpOnly;SameSite=Lax;Path=/;Max-Age=0");
    tRequest->send(tResponse);
  }

  void Dashboard_::HandleStatus(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    char tJson[kSmallJsonSize] = "";
    strncpy(tJson, mCachedStatusJson, sizeof(tJson) - 1);
    tJson[sizeof(tJson) - 1] = '\0';
    if (!tJson[0]) strncpy(tJson, "{}", sizeof(tJson) - 1);
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandlePageData(AsyncWebServerRequest *tRequest) {
    const String tPageKey = ResolvePageKeyFromRequest(tRequest);
    const bool tIncludeStatsData = (tPageKey == "stats");
    const bool tIncludeGalleryData = (tPageKey == "index");
    const bool tIncludeGalleryConfig = (tPageKey == "index");
    bool tNeedGalleryCacheRefresh = false;
    SAppConfig tConfig = {};
    uint64_t tSdTotalBytes = 0;
    uint64_t tSdUsedBytes = 0;
    uint64_t tLfsTotalBytes = 0;
    uint64_t tLfsUsedBytes = 0;
    uint32_t tGalleryCount = 0;
    String tCachedIndexStoragesJson = "\"Storages\":[]";
    String tCachedPageStatsDataJson = "\"Statistics\":{}";
    {
      Guard tLock;
      if (!AuthorizeRequest(tRequest)) {
        DashboardUtils_::UnauthorizedResponse(tRequest);
        return;
      }
      tNeedGalleryCacheRefresh = mIndexDataCacheDirty || mCachedIndexStoragesJson.length() == 0;
    }
    if (tNeedGalleryCacheRefresh) RefreshGalleryCacheIfNeeded(true);
    {
      Guard tLock;
      tSdTotalBytes = mCachedSdTotalBytes;
      tSdUsedBytes = mCachedSdUsedBytes;
      tLfsTotalBytes = mCachedLfsTotalBytes;
      tLfsUsedBytes = mCachedLfsUsedBytes;
      tGalleryCount = mCachedIndexGalleryCount;
      if (tIncludeStatsData && mCachedPageStatsDataJson.length()) tCachedPageStatsDataJson = mCachedPageStatsDataJson;
      if (tIncludeGalleryData) {
        if (mCachedIndexStoragesJson.length()) tCachedIndexStoragesJson = mCachedIndexStoragesJson;
      }
    }
    tConfig = CFG.Get<SAppConfig>();
    const String tStorageDefaultKey = GetDefaultStorageKey(tConfig.Storage);
    String tJson;
    tJson.reserve(16384);
    const unsigned long tTimestamp = static_cast<unsigned long>(time(nullptr));
    const int32_t tUtcOffsetMinutes = ResolveUtcOffsetMinutes();
    const String tTimeZone = ResolveTimeZoneLabel();
    const uint16_t tDashboardTargetWidth = tConfig.Dashboard.TargetWidth ? tConfig.Dashboard.TargetWidth : DASHBOARD_IMG_WIDTH;
    const uint16_t tDashboardTargetHeight = tConfig.Dashboard.TargetHeight ? tConfig.Dashboard.TargetHeight : DASHBOARD_IMG_HEIGHT;
    const uint16_t tDashboardThumbWidth = tConfig.Dashboard.ThumbWidth ? tConfig.Dashboard.ThumbWidth : DASHBOARD_IMG_THUMB_WIDTH;
    const uint16_t tDashboardThumbHeight = tConfig.Dashboard.ThumbHeight ? tConfig.Dashboard.ThumbHeight : DASHBOARD_IMG_THUMB_HEIGHT;
    const uint16_t tDashboardRotate = tConfig.Dashboard.Rotate;
    String tDashboardLanguage = DashboardUtils_::NormalizeLanguageCode(tConfig.Dashboard.Language);
    if (!tDashboardLanguage.length()) tDashboardLanguage = "en";
    const String tDashboardImageExt = NormalizeDashboardImageExtension(tConfig.Dashboard.ImageExt);
    const char *tBatteryStateKey = "no_battery";
    const uint8_t tBatteryPercent = 0;
    const char *tBatteryVoltsText = "0.00";
    tJson += "{\"Page\":{\"Name\":\"";
    tJson += ResolveContractPageName(tPageKey);
    tJson += "\",\"HintEnable\":";
    tJson += tConfig.Dashboard.ShowDescription ? "true" : "false";
    tJson += ",\"DateTime\":";
    tJson += String(tTimestamp);
    tJson += ",\"GalleryCount\":";
    tJson += String(tGalleryCount);
    tJson += ",\"Battery\":{\"State\":\"";
    tJson += tBatteryStateKey;
    tJson += "\",\"Percent\":";
    tJson += String(tBatteryPercent);
    tJson += ",\"Volts\":";
    tJson += tBatteryVoltsText;
    tJson += "}},\"Data\":{";
    if (tIncludeStatsData) tJson += tCachedPageStatsDataJson;
    else if (tIncludeGalleryData) tJson += tCachedIndexStoragesJson;
    AppendOtherPageDataJson(tJson, tPageKey, tConfig, tDashboardLanguage, tStorageDefaultKey, tTimestamp, tUtcOffsetMinutes, tTimeZone);
    tJson += "},\"Config\":";
    if (tIncludeGalleryConfig) {
      tJson += "{\"Editor\":{\"TargetWidth\":";
      tJson += String(tDashboardTargetWidth);
      tJson += ",\"TargetHeight\":";
      tJson += String(tDashboardTargetHeight);
      tJson += ",\"ThumbWidth\":";
      tJson += String(tDashboardThumbWidth);
      tJson += ",\"ThumbHeight\":";
      tJson += String(tDashboardThumbHeight);
      tJson += ",\"Rotate\":";
      tJson += String(tDashboardRotate);
      tJson += ",\"ImageExt\":\"";
      tJson += EscapeJsonText(tDashboardImageExt);
      tJson += "\"},\"Storage\":{\"JpegQuality\":";
      tJson += String(DASHBOARD_IMG_JPEG_QUALITY, 2);
      tJson += "},\"Paths\":{\"ImagesPath\":\"/api/images/\",\"ThumbsPath\":\"/api/images/thumbs/\"},\"ApiEndpoints\":{\"Upload\":\"/api/images/upload\",\"UploadThumb\":\"/api/images/upload?type=thumb\",\"Swap\":\"/api/images/swap\",\"SetDefault\":\"/api/images/default\",\"Copy\":\"/api/images/copy\",\"Delete\":\"/api/images/delete\"}}";
    } else tJson += "{}";
    tJson += "}";
    DashboardUtils_::JsonResponse(tRequest, 200, tJson.c_str());
  }

  void Dashboard_::HandleConfigGet(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    char *tJson = static_cast<char *>(ps_malloc(kConfigJsonSize));
    if (!tJson) { DashboardUtils_::ErrorResponse(tRequest, 500, "oom"); return; }
    tJson[0] = '\0';
    BuildConfigJson(tJson, kConfigJsonSize, tConfig);
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
    free(tJson);
  }

  void Dashboard_::HandleConfigSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    ParseConfigSave(tRequest, tConfig);
    if (CFG.SaveAllConfig(tConfig)) {
      ReloadConfig();
      STG.ReloadConfig();
      strncpy(mCachedUser, mCfg.Dashboard.User.c_str(), sizeof(mCachedUser) - 1);
      mCachedUser[sizeof(mCachedUser) - 1] = '\0';
      strncpy(mCachedPassHash, mCfg.Dashboard.Password.c_str(), sizeof(mCachedPassHash) - 1);
      mCachedPassHash[sizeof(mCachedPassHash) - 1] = '\0';
      xLOG("Config saved via HTTP");
      DashboardUtils_::OkResponse(tRequest, "saved");
      return;
    }
    xLOG("Config save failed via HTTP");
    DashboardUtils_::ErrorResponse(tRequest, 500, "save_failed");
  }

  void Dashboard_::HandleDisplaySave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tValue;
    if (TryGetRequestValue(tRequest, "Display[Brightness]", tValue)) tConfig.Display.JpgBrightness = static_cast<uint8_t>(constrain(tValue.toInt(), 0, 100));
    if (TryGetRequestValue(tRequest, "Display[Contrast]", tValue)) tConfig.Display.JpgContrast = static_cast<uint8_t>(constrain(tValue.toInt(), 0, 100));
    if (TryGetRequestValue(tRequest, "Display[Gamma]", tValue)) tConfig.Display.JpgGamma = static_cast<uint8_t>(constrain(tValue.toInt(), 1, 255));
    if (TryGetRequestValue(tRequest, "Display[Rotation]", tValue)) tConfig.Display.Rotate = static_cast<uint16_t>(tValue.toInt());
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "display_save_error");
      return;
    }
    ReloadConfig();
    DSP.ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "display_save_success");
  }

  void Dashboard_::HandleNetworkSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tValue;
    String tMode = tConfig.Connection.ApModeEnable ? String("AP") : String("STA");
    if (TryGetRequestValue(tRequest, "Connection[Mode]", tValue)) {
      tMode = tValue;
      tMode.trim();
      tMode.toUpperCase();
    } else if (TryGetRequestValue(tRequest, "Ap[Enabled]", tValue)) tMode = ParseBoolValue(tValue) ? String("AP") : String("STA");
    tConfig.Connection.ApModeEnable = (tMode != "STA");
    if (TryGetRequestValue(tRequest, "Ap[Ssid]", tValue)) tConfig.Connection.ApSsid = tValue;
    if (TryGetRequestValue(tRequest, "Ap[Password]", tValue)) tConfig.Connection.ApPassword = tValue;
    if (TryGetRequestValue(tRequest, "Ap[IpAddress]", tValue)) tConfig.Connection.ApIp = tValue;
    if (TryGetRequestValue(tRequest, "Ap[Gateway]", tValue)) tConfig.Connection.ApGateway = tValue;
    if (TryGetRequestValue(tRequest, "Ap[SubnetMask]", tValue)) tConfig.Connection.ApSubnet = tValue;
    if (TryGetRequestValue(tRequest, "Sta[Ssid]", tValue)) tConfig.Connection.StaSsid = tValue;
    if (TryGetRequestValue(tRequest, "Sta[Password]", tValue)) tConfig.Connection.StaPassword = tValue;
    if (TryGetRequestValue(tRequest, "Sta[StaticIpEnabled]", tValue)) tConfig.Connection.StaIpEnable = ParseBoolValue(tValue);
    if (TryGetRequestValue(tRequest, "Sta[IpAddress]", tValue)) tConfig.Connection.StaIp = tValue;
    if (TryGetRequestValue(tRequest, "Sta[Gateway]", tValue)) tConfig.Connection.StaGateway = tValue;
    if (TryGetRequestValue(tRequest, "Sta[SubnetMask]", tValue)) tConfig.Connection.StaSubnet = tValue;
    if (TryGetRequestValue(tRequest, "Sta[PrimaryDns]", tValue)) tConfig.Connection.StaPrimaryDns = tValue;
    if (TryGetRequestValue(tRequest, "Sta[SecondaryDns]", tValue)) tConfig.Connection.StaSecondaryDns = tValue;
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "network_save_error");
      return;
    }
    ReloadConfig();
    CON.ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "network_save_success");
  }

  void Dashboard_::HandleMdnsSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tValue;
    if (TryGetRequestValue(tRequest, "Mdns[Enabled]", tValue)) tConfig.Connection.MdnsEnable = ParseBoolValue(tValue);
    if (TryGetRequestValue(tRequest, "Mdns[HostName]", tValue)) tConfig.Connection.MdnsName = tValue;
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "mdns_save_error");
      return;
    }
    ReloadConfig();
    CON.ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "mdns_save_success");
  }

  void Dashboard_::HandleNtpSave(AsyncWebServerRequest *tRequest, bool tSyncNow) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tValue;
    if (TryGetRequestValue(tRequest, "Ntp[Server]", tValue)) tConfig.Ntp.Server = tValue;
    long tGmtOffsetSec = tConfig.Ntp.GMTOffset;
    long tDstOffsetSec = tConfig.Ntp.DaylightOffset;
    if (TryGetRequestValue(tRequest, "Ntp[GmtOffsetSec]", tValue)) tGmtOffsetSec = strtol(tValue.c_str(), nullptr, 10);
    if (TryGetRequestValue(tRequest, "Ntp[DaylightOffsetSec]", tValue)) tDstOffsetSec = strtol(tValue.c_str(), nullptr, 10);
    tConfig.Ntp.GMTOffset = tGmtOffsetSec;
    tConfig.Ntp.DaylightOffset = tDstOffsetSec;
    const int32_t tUtcOffsetMinutes = static_cast<int32_t>((tGmtOffsetSec + tDstOffsetSec) / 60);
    tConfig.Ntp.TimeZoneLabel = (tUtcOffsetMinutes == 0) ? String("ETC/UTC") : FormatUtcOffsetLabel(tUtcOffsetMinutes);
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, tSyncNow ? "ntp_sync_error" : "ntp_save_error");
      return;
    }
    ReloadConfig();
    UTL.ReloadConfig();
    NTP.ReloadConfig();
    bool tSyncOk = true;
    if (tSyncNow) {
      NTP.Init();
      tSyncOk = NTP.SyncSystemTime();
      NTP.End();
      if (tSyncOk) RTC.SyncFromSystem();
    }
    if (!tSyncOk) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "ntp_sync_error");
      return;
    }
    DashboardUtils_::OkResponse(tRequest, tSyncNow ? "ntp_sync_success" : "ntp_save_success");
  }

  void Dashboard_::HandleDateTimeSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    String tValue;
    if (!TryGetRequestValue(tRequest, "DateTime[TimeStamp]", tValue)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "invalid_datetime_value");
      return;
    }
    const unsigned long tEpoch = strtoul(tValue.c_str(), nullptr, 10);
    if (tEpoch < 1735689600UL) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "invalid_datetime_value");
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    int32_t tUtcOffsetMinutes = static_cast<int32_t>((tConfig.Ntp.GMTOffset + tConfig.Ntp.DaylightOffset) / 60);
    if (TryGetRequestValue(tRequest, "DateTime[UtcOffsetMinutes]", tValue)) {
      const long tRawOffsetMinutes = strtol(tValue.c_str(), nullptr, 10);
      if (tRawOffsetMinutes < -720 || tRawOffsetMinutes > 840) {
        DashboardUtils_::ErrorResponse(tRequest, 400, "invalid_datetime_value");
        return;
      }
      tUtcOffsetMinutes = static_cast<int32_t>(tRawOffsetMinutes);
    }
    String tTimeZoneLabel = String("");
    if (TryGetRequestValue(tRequest, "DateTime[TimeZone]", tValue)) tTimeZoneLabel = NormalizeTimeZoneLabel(tValue);
    if (!tTimeZoneLabel.length()) tTimeZoneLabel = FormatUtcOffsetLabel(tUtcOffsetMinutes);

    tConfig.Ntp.GMTOffset = static_cast<long>(tUtcOffsetMinutes) * 60L;
    tConfig.Ntp.DaylightOffset = 0;
    tConfig.Ntp.TimeZoneLabel = tTimeZoneLabel;
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "datetime_save_error");
      return;
    }

    struct timeval tTimeValue = {};
    tTimeValue.tv_sec = static_cast<time_t>(tEpoch);
    tTimeValue.tv_usec = 0;
    const bool tSystemTimeOk = (settimeofday(&tTimeValue, nullptr) == 0);
    if (!tSystemTimeOk) {
      xLOG("DateTime save failed: system time set failed");
      DashboardUtils_::ErrorResponse(tRequest, 500, "datetime_save_error");
      return;
    }
    bool tRtcTimeOk = true;
    if (RTC.Init(false) && RTC.IsAvailable()) tRtcTimeOk = RTC.SyncFromSystem();
    if (!tRtcTimeOk) xLOG("DateTime save warning: RTC sync from system failed");
    ReloadConfig();
    UTL.ReloadConfig();
    NTP.ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "datetime_save_success");
  }

  void Dashboard_::HandleLanguageSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tLanguage;
    if (!TryGetRequestValue(tRequest, "language[default]", tLanguage)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "language_save_error");
      return;
    }
    tLanguage.trim();
    tLanguage.toLowerCase();
    if (tLanguage != "en" && tLanguage != "hu") {
      DashboardUtils_::ErrorResponse(tRequest, 400, "language_save_error");
      return;
    }
    tConfig.Dashboard.Language = tLanguage;
    DashboardUtils_::NormalizeEnabledLanguages(tConfig.Dashboard.EnabledLanguages, tConfig.Dashboard.Language);
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "language_save_error");
      return;
    }
    ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "language_save_success");
  }

  void Dashboard_::HandleUserSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tCurrentPassword;
    String tNewPassword;
    String tConfirmPassword;
    String tValue;
    TryGetRequestValue(tRequest, "user[password][current]", tCurrentPassword);
    TryGetRequestValue(tRequest, "user[password][new]", tNewPassword);
    TryGetRequestValue(tRequest, "user[password][confirm]", tConfirmPassword);
    const bool tPasswordChangeRequested = tNewPassword.length() || tConfirmPassword.length();
    if (tPasswordChangeRequested) {
      if (!tCurrentPassword.length()) {
        DashboardUtils_::ErrorResponse(tRequest, 400, "current_password_required");
        return;
      }
      if (mCachedPassHash[0]) {
        char tCurrentHash[65] = "";
        DashboardUtils_::Sha256Hex(reinterpret_cast<const uint8_t *>(tCurrentPassword.c_str()), tCurrentPassword.length(), tCurrentHash);
        if (strcasecmp(tCurrentHash, mCachedPassHash) != 0) {
          DashboardUtils_::ErrorResponse(tRequest, 401, "invalid_password");
          return;
        }
      }
      if (!tNewPassword.length() || tNewPassword != tConfirmPassword) {
        DashboardUtils_::ErrorResponse(tRequest, 400, "passwords_must_match");
        return;
      }
      if (mCachedPassHash[0]) {
        char tNewHash[65] = "";
        DashboardUtils_::Sha256Hex(reinterpret_cast<const uint8_t *>(tNewPassword.c_str()), tNewPassword.length(), tNewHash);
        if (strcasecmp(tNewHash, mCachedPassHash) == 0) {
          DashboardUtils_::ErrorResponse(tRequest, 400, "new_password_must_differ");
          return;
        }
      }
    }
    if (TryGetRequestValue(tRequest, "user[username]", tValue)) {
      tValue.trim();
      if (tValue.length()) tConfig.Dashboard.User = tValue;
    }
    if (TryGetRequestValue(tRequest, "user[preferences][default_theme]", tValue)) {
      tValue.trim();
      tValue.toLowerCase();
      tConfig.Dashboard.Theme = tValue == "dark" ? String("dark") : String("light");
    }
    if (TryGetRequestValue(tRequest, "user[preferences][show_description]", tValue)) tConfig.Dashboard.ShowDescription = ParseBoolValue(tValue);
    if (TryGetRequestValue(tRequest, "user[preferences][sound_enabled]", tValue)) tConfig.Device.SoundEnabled = ParseBoolValue(tValue);
    if (tNewPassword.length()) {
      char tPasswordHash[65] = "";
      DashboardUtils_::Sha256Hex(reinterpret_cast<const uint8_t *>(tNewPassword.c_str()), tNewPassword.length(), tPasswordHash);
      tConfig.Dashboard.Password = String(tPasswordHash);
    }
    if (TryGetRequestValue(tRequest, "user[storage][fallback_enabled]", tValue)) tConfig.Storage.FallbackEnabled = ParseBoolValue(tValue);
    if (TryGetRequestValue(tRequest, "user[storage][default]", tValue)) tConfig.Storage.DefaultFileSystem = NormalizeStorageKey(tValue) == "sd_card" ? EFileSystemType::SDCard : EFileSystemType::LittleFS;
    std::vector<String> tEnabledLanguages = GetRequestValues(tRequest, "user[languages][enabled][]");
    if (tEnabledLanguages.empty()) tEnabledLanguages = GetRequestValues(tRequest, "enabled_languages[]");
    if (tEnabledLanguages.empty()) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "language_one_required");
      return;
    }
    DashboardUtils_::NormalizeEnabledLanguages(tEnabledLanguages, tConfig.Dashboard.Language);
    tConfig.Dashboard.EnabledLanguages = tEnabledLanguages;
    tConfig.Dashboard.Language = DashboardUtils_::ResolveLanguage(tConfig.Dashboard.EnabledLanguages, tConfig.Dashboard.Language);
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "user_save_error");
      return;
    }
    ReloadConfig();
    STG.ReloadConfig();
    strncpy(mCachedUser, mCfg.Dashboard.User.c_str(), sizeof(mCachedUser) - 1);
    mCachedUser[sizeof(mCachedUser) - 1] = '\0';
    strncpy(mCachedPassHash, mCfg.Dashboard.Password.c_str(), sizeof(mCachedPassHash) - 1);
    mCachedPassHash[sizeof(mCachedPassHash) - 1] = '\0';
    if (tPasswordChangeRequested) {
      for (uint8_t tSessionIndex = 0; tSessionIndex < kMaxSessions; tSessionIndex++) {
        memset(mSessions[tSessionIndex].Token, 0, sizeof(mSessions[tSessionIndex].Token));
        mSessions[tSessionIndex].LastSeenMs = 0;
        mSessions[tSessionIndex].Active = false;
      }
      xLOG("Password changed, all sessions invalidated");
      AsyncWebServerResponse *tResponse = tRequest->beginResponse(200, "application/json", "{\"ok\":true,\"error\":false,\"message\":\"user_save_success\",\"password_changed\":true,\"RedirectUrl\":\"/login.html\"}");
      tResponse->addHeader("Cache-Control", "no-store");
      tResponse->addHeader("Set-Cookie", String(kSessionCookieName) + "=;HttpOnly;SameSite=Lax;Path=/;Max-Age=0");
      tRequest->send(tResponse);
      return;
    }
    DashboardUtils_::OkResponse(tRequest, "user_save_success");
  }

  void Dashboard_::HandleUserRestore(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    char tToken[65] = "";
    if (TryExtractBearerToken(tRequest, tToken, sizeof(tToken)) && tToken[0]) DestroySession(tToken);
    mResetPending = true;
    mResetPendingSince = millis();
    mRestartPending = true;
    mRestartPendingSince = millis();
    DashboardUtils_::OkResponse(tRequest, "restore_restart_pending");
  }

  void Dashboard_::HandleWakeUpSave(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    String tValue;
    if (TryGetRequestValue(tRequest, "wakeup[interval]", tValue)) tConfig.Timer.WakeUp = static_cast<ETimerWakeUp>(tValue.toInt());
    if (TryGetRequestValue(tRequest, "wakeup[hour]", tValue)) tConfig.Timer.WakeUpHour = static_cast<uint8_t>(constrain(tValue.toInt(), 0, 23));
    if (!CFG.SaveAllConfig(tConfig)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "wakeup_save_error");
      return;
    }
    ReloadConfig();
    DashboardUtils_::OkResponse(tRequest, "wakeup_save_success");
  }

  void Dashboard_::ParseConfigSave(AsyncWebServerRequest *tRequest, SAppConfig &tConfig) {
    if (!tRequest) return;
    auto tGetParameter = [&](const char *tKey) -> String {
      if (tRequest->hasParam(tKey, true)) return tRequest->getParam(tKey, true)->value();
      return String();
    };
    auto tHasParameter = [&](const char *tKey) -> bool {
      return tRequest->hasParam(tKey, true);
    };
    if (tHasParameter("display.rotate")) tConfig.Display.Rotate = static_cast<uint16_t>(tGetParameter("display.rotate").toInt());
    if (tHasParameter("display.brightness")) tConfig.Display.JpgBrightness = static_cast<uint8_t>(tGetParameter("display.brightness").toInt());
    if (tHasParameter("display.contrast")) tConfig.Display.JpgContrast = static_cast<uint8_t>(tGetParameter("display.contrast").toInt());
    if (tHasParameter("display.gamma")) tConfig.Display.JpgGamma = static_cast<uint8_t>(tGetParameter("display.gamma").toInt());
    if (tHasParameter("connection.ap_enable")) tConfig.Connection.ApModeEnable = tGetParameter("connection.ap_enable") == "1";
    if (tHasParameter("connection.ap_ssid")) tConfig.Connection.ApSsid = tGetParameter("connection.ap_ssid");
    if (tHasParameter("connection.ap_password")) tConfig.Connection.ApPassword = tGetParameter("connection.ap_password");
    if (tHasParameter("connection.ap_ip")) tConfig.Connection.ApIp = tGetParameter("connection.ap_ip");
    if (tHasParameter("connection.ap_gateway")) tConfig.Connection.ApGateway = tGetParameter("connection.ap_gateway");
    if (tHasParameter("connection.ap_subnet")) tConfig.Connection.ApSubnet = tGetParameter("connection.ap_subnet");
    if (tHasParameter("connection.sta_ssid")) tConfig.Connection.StaSsid = tGetParameter("connection.sta_ssid");
    if (tHasParameter("connection.sta_password")) tConfig.Connection.StaPassword = tGetParameter("connection.sta_password");
    if (tHasParameter("connection.sta_ip_enable")) tConfig.Connection.StaIpEnable = tGetParameter("connection.sta_ip_enable") == "1";
    if (tHasParameter("connection.sta_ip")) tConfig.Connection.StaIp = tGetParameter("connection.sta_ip");
    if (tHasParameter("connection.sta_gateway")) tConfig.Connection.StaGateway = tGetParameter("connection.sta_gateway");
    if (tHasParameter("connection.sta_subnet")) tConfig.Connection.StaSubnet = tGetParameter("connection.sta_subnet");
    if (tHasParameter("connection.sta_dns1")) tConfig.Connection.StaPrimaryDns = tGetParameter("connection.sta_dns1");
    if (tHasParameter("connection.sta_dns2")) tConfig.Connection.StaSecondaryDns = tGetParameter("connection.sta_dns2");
    if (tHasParameter("connection.mdns_enable")) tConfig.Connection.MdnsEnable = tGetParameter("connection.mdns_enable") == "1";
    if (tHasParameter("connection.mdns_name")) tConfig.Connection.MdnsName = tGetParameter("connection.mdns_name");
    if (tHasParameter("ntp.server")) tConfig.Ntp.Server = tGetParameter("ntp.server");
    if (tHasParameter("ntp.port")) tConfig.Ntp.NtpPort = static_cast<uint16_t>(tGetParameter("ntp.port").toInt());
    if (tHasParameter("ntp.gmt_offset")) tConfig.Ntp.GMTOffset = static_cast<long>(tGetParameter("ntp.gmt_offset").toInt());
    if (tHasParameter("ntp.daylight_offset")) tConfig.Ntp.DaylightOffset = static_cast<long>(tGetParameter("ntp.daylight_offset").toInt());
    if (tHasParameter("ntp.update_interval")) {
      unsigned long tIntervalMs = static_cast<unsigned long>(tGetParameter("ntp.update_interval").toInt());
      unsigned long tIntervalSec = tIntervalMs / 1000UL;
      tConfig.Ntp.LowPowerSyncIntervalSec = (tIntervalSec >= SECONDS_PER_DAY) ? tIntervalSec : SECONDS_PER_DAY;
    }
    if (tHasParameter("timer.wake_up")) tConfig.Timer.WakeUp = static_cast<ETimerWakeUp>(tGetParameter("timer.wake_up").toInt());
    if (tHasParameter("timer.wake_hour")) tConfig.Timer.WakeUpHour = static_cast<uint8_t>(tGetParameter("timer.wake_hour").toInt());
    if (tHasParameter("dashboard.user")) tConfig.Dashboard.User = tGetParameter("dashboard.user");
    if (tHasParameter("dashboard.language")) {
      String tLanguage = tGetParameter("dashboard.language");
      tLanguage.trim();
      tLanguage.toLowerCase();
      bool tIsValidLanguage = (tLanguage.length() >= 2 && tLanguage.length() <= 12);
      for (size_t tIndex = 0; tIsValidLanguage && tIndex < tLanguage.length(); tIndex++) {
        char tChar = tLanguage[tIndex];
        bool tIsLowerAlpha = (tChar >= 'a' && tChar <= 'z');
        bool tIsDigit = (tChar >= '0' && tChar <= '9');
        bool tIsAllowedPunctuation = (tChar == '-' || tChar == '_');
        if (!tIsLowerAlpha && !tIsDigit && !tIsAllowedPunctuation) tIsValidLanguage = false;
      }
      if (tIsValidLanguage) tConfig.Dashboard.Language = tLanguage;
    }
    if (tHasParameter("dashboard.enabled_languages")) {
      tConfig.Dashboard.EnabledLanguages = DashboardUtils_::ParseEnabledLanguages(tGetParameter("dashboard.enabled_languages"), tConfig.Dashboard.Language);
    } else {
      std::vector<String> tEnabledLanguages = GetRequestValues(tRequest, "dashboard.enabled_languages[]");
      if (!tEnabledLanguages.empty()) {
        DashboardUtils_::NormalizeEnabledLanguages(tEnabledLanguages, tConfig.Dashboard.Language);
        tConfig.Dashboard.EnabledLanguages = tEnabledLanguages;
      }
    }
    if (tHasParameter("dashboard.password")) {
      String tNewPassword = tGetParameter("dashboard.password");
      if (tNewPassword.length() > 0) {
        char tPasswordHash[65] = "";
        DashboardUtils_::Sha256Hex(reinterpret_cast<const uint8_t *>(tNewPassword.c_str()), tNewPassword.length(), tPasswordHash);
        tConfig.Dashboard.Password = String(tPasswordHash);
      }
    }
  }

  void Dashboard_::HandleImagesList(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    const String tRequestUrl = tRequest ? tRequest->url() : String();
    if (tRequestUrl.startsWith("/api/images/thumbs/") || tRequestUrl.startsWith("/api/images/thumb/")) {
      HandleImageThumb(tRequest);
      return;
    }
    if (tRequestUrl.startsWith("/api/images/") && tRequestUrl.length() > strlen("/api/images/")) {
      HandleImageFile(tRequest);
      return;
    }
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tDirectoryPath[64] = "";
    snprintf(tDirectoryPath, sizeof(tDirectoryPath), "/%s", tDisplayConfig.ImagesDir.c_str());
    char *tJson = static_cast<char *>(ps_malloc(kImagesJsonSize));
    if (!tJson) { DashboardUtils_::ErrorResponse(tRequest, 500, "oom"); return; }
    tJson[0] = '\0';
    BuildImagesJson(tJson, kImagesJsonSize, tDirectoryPath);
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
    free(tJson);
  }

  void Dashboard_::HandleImageDelete(AsyncWebServerRequest *tRequest, const String &tBody) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    String tStorageValue;
    TryGetRequestValue(tRequest, "storage", tStorageValue);
    if (!tStorageValue.length() && tBody.length()) {
      const int tStorageKeyPos = tBody.indexOf("\"storage\"");
      if (tStorageKeyPos >= 0) {
        const int tValueStart = tBody.indexOf('"', tBody.indexOf(':', tStorageKeyPos) + 1);
        const int tValueEnd = tValueStart >= 0 ? tBody.indexOf('"', tValueStart + 1) : -1;
        if (tValueStart >= 0 && tValueEnd > tValueStart) tStorageValue = tBody.substring(tValueStart + 1, tValueEnd);
      }
    }
    const String tStorageKey = NormalizeStorageKey(tStorageValue);
    std::vector<String> tFiles;
    if (tBody.length()) {
      const int tFilesKeyPos = tBody.indexOf("\"files\"");
      const int tArrayStart = tFilesKeyPos >= 0 ? tBody.indexOf('[', tFilesKeyPos) : -1;
      const int tArrayEnd = tArrayStart >= 0 ? tBody.indexOf(']', tArrayStart) : -1;
      int tCursor = tArrayStart;
      while (tCursor >= 0 && tCursor < tArrayEnd) {
        const int tValueStart = tBody.indexOf('"', tCursor + 1);
        const int tValueEnd = tValueStart >= 0 ? tBody.indexOf('"', tValueStart + 1) : -1;
        if (tValueStart < 0 || tValueEnd < 0 || tValueEnd > tArrayEnd) break;
        tFiles.push_back(tBody.substring(tValueStart + 1, tValueEnd));
        tCursor = tValueEnd + 1;
      }
    }
    if (tFiles.empty()) {
      String tSingleFile;
      if (TryGetRequestValue(tRequest, "name", tSingleFile) && tSingleFile.length()) tFiles.push_back(tSingleFile);
    }
    if (tFiles.empty()) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "delete_error");
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (!IsStorageKeyAllowed(tStorageKey, tConfig.Storage)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "delete_error");
      return;
    }
    const String tDefaultStorageKey = GetDefaultStorageKey(tConfig.Storage);
    const String tCurrentFileName = GetStorageLeafFileName(tConfig.Display.CurrentFile.c_str());
    if (tStorageKey == tDefaultStorageKey && tCurrentFileName.length()) {
      for (size_t tFileIndex = 0; tFileIndex < tFiles.size(); tFileIndex++) {
        char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
        DashboardUtils_::SanitizeFilename(tFiles[tFileIndex].c_str(), tSafeFileName, sizeof(tSafeFileName));
        if (!tSafeFileName[0]) continue;
        if (tCurrentFileName == String(tSafeFileName)) {
          DashboardUtils_::ErrorResponse(tRequest, 409, "delete_default_locked");
          return;
        }
      }
    }
    bool tDeleted = false;
    bool tRefreshCurrentImage = false;
    for (size_t tFileIndex = 0; tFileIndex < tFiles.size(); tFileIndex++) {
      char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
      DashboardUtils_::SanitizeFilename(tFiles[tFileIndex].c_str(), tSafeFileName, sizeof(tSafeFileName));
      if (!tSafeFileName[0]) continue;
      char tImagePath[192] = "";
      char tThumbPath[192] = "";
      snprintf(tImagePath, sizeof(tImagePath), "/%s/%s", tConfig.Display.ImagesDir.c_str(), tSafeFileName);
      snprintf(tThumbPath, sizeof(tThumbPath), "/%s/.thumbs/%s", tConfig.Display.ImagesDir.c_str(), tSafeFileName);
      const bool tMatchesCurrentImage = tStorageKey == tDefaultStorageKey && tCurrentFileName.length() && tCurrentFileName == String(tSafeFileName);
      if (StorageFileExistsByKey(tStorageKey, tImagePath) && DeleteStorageFileByKey(tStorageKey, tImagePath)) {
        tDeleted = true;
        if (tMatchesCurrentImage) tRefreshCurrentImage = true;
      }
      if (StorageFileExistsByKey(tStorageKey, tThumbPath)) DeleteStorageFileByKey(tStorageKey, tThumbPath);
    }
    if (!tDeleted) {
      DashboardUtils_::ErrorResponse(tRequest, 404, "delete_error");
      return;
    }
    if (tCurrentFileName.length()) {
      char tCurrentImagePath[192] = "";
      snprintf(tCurrentImagePath, sizeof(tCurrentImagePath), "/%s/%s", tConfig.Display.ImagesDir.c_str(), tCurrentFileName.c_str());
      if (!StorageFileExistsByKey(tDefaultStorageKey, tCurrentImagePath)) tRefreshCurrentImage = true;
    }
    if (tRefreshCurrentImage) {
      char tImagesDirectory[80] = "";
      snprintf(tImagesDirectory, sizeof(tImagesDirectory), "/%s", tConfig.Display.ImagesDir.c_str());
      const String tNextImageFileName = ResolveNextStorageFileName(tDefaultStorageKey, tImagesDirectory, tConfig.Display.ImageExt.c_str(), tCurrentFileName.c_str());
      if (!CFG.SaveImageName(tNextImageFileName.c_str())) xLOG("Dashboard → failed to refresh current image after delete");
      else xLOG("Dashboard → current image refreshed after delete: %s", tNextImageFileName.length() ? tNextImageFileName.c_str() : "<empty>");
    }
    InvalidateFileCacheByKey(tStorageKey);
    MarkGalleryCacheDirty();
    ReloadConfig();
    STG.ReloadConfig();
    for (size_t i = 0; i < tFiles.size(); i++) xLOG("Dashboard → image deleted: %s [%s]", tFiles[i].c_str(), tStorageKey.c_str());
    if (mWebSocket) mWebSocket->textAll("{\"type\":\"gallery_delete\",\"message\":\"delete_success\"}");
    DashboardUtils_::OkResponse(tRequest, "delete_success");
  }

  void Dashboard_::HandleImageCopy(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    String tFileName;
    String tSourceStorageValue;
    String tTargetStorageValue;
    if (!TryGetRequestValue(tRequest, "name", tFileName) || !TryGetRequestValue(tRequest, "from", tSourceStorageValue) || !TryGetRequestValue(tRequest, "to", tTargetStorageValue)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "copy_error");
      return;
    }
    const String tSourceStorageKey = NormalizeStorageKey(tSourceStorageValue);
    const String tTargetStorageKey = NormalizeStorageKey(tTargetStorageValue);
    char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
    char tTargetFileName[kMaxUploadFileNameLength + 1] = "";
    DashboardUtils_::SanitizeFilename(tFileName.c_str(), tSafeFileName, sizeof(tSafeFileName));
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (!tSafeFileName[0] || !IsStorageAvailableByKey(tSourceStorageKey) || !IsStorageAvailableByKey(tTargetStorageKey) || !IsStorageKeyAllowed(tSourceStorageKey, tConfig.Storage) || !IsStorageKeyAllowed(tTargetStorageKey, tConfig.Storage)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "copy_error");
      return;
    }
    const String tDefaultStorageKey = GetDefaultStorageKey(tConfig.Storage);
    const String tCurrentFileName = GetStorageLeafFileName(tConfig.Display.CurrentFile.c_str());
    if (tSourceStorageKey == tDefaultStorageKey && tCurrentFileName.length() && tCurrentFileName == String(tSafeFileName)) {
      DashboardUtils_::ErrorResponse(tRequest, 409, "copy_default_locked");
      return;
    }
    strncpy(tTargetFileName, tSafeFileName, sizeof(tTargetFileName) - 1);
    tTargetFileName[sizeof(tTargetFileName) - 1] = '\0';
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tSourcePath[192] = "";
    char tTargetPath[192] = "";
    char tSourceThumbPath[192] = "";
    char tTargetThumbPath[192] = "";
    char tTargetBaseDirectory[80] = "";
    char tTargetThumbDirectory[80] = "";
    snprintf(tSourcePath, sizeof(tSourcePath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
    snprintf(tTargetPath, sizeof(tTargetPath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tTargetFileName);
    snprintf(tSourceThumbPath, sizeof(tSourceThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
    snprintf(tTargetThumbPath, sizeof(tTargetThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tTargetFileName);
    snprintf(tTargetBaseDirectory, sizeof(tTargetBaseDirectory), "/%s", tDisplayConfig.ImagesDir.c_str());
    snprintf(tTargetThumbDirectory, sizeof(tTargetThumbDirectory), "/%s/.thumbs", tDisplayConfig.ImagesDir.c_str());
    if (!StorageFileExistsByKey(tSourceStorageKey, tSourcePath)) {
      DashboardUtils_::ErrorResponse(tRequest, 404, "copy_error");
      return;
    }
    if (!StorageFileExistsByKey(tTargetStorageKey, tTargetBaseDirectory)) MakeStorageDirByKey(tTargetStorageKey, tTargetBaseDirectory);
    if (!StorageFileExistsByKey(tTargetStorageKey, tTargetThumbDirectory)) MakeStorageDirByKey(tTargetStorageKey, tTargetThumbDirectory);
    const bool tCrossStorageCopy = tSourceStorageKey != tTargetStorageKey;
    if (tCrossStorageCopy || StorageFileExistsByKey(tTargetStorageKey, tTargetPath)) {
      if (!ResolveNextUploadFileName(tTargetStorageKey, tDisplayConfig.ImagesDir.c_str(), tTargetFileName, sizeof(tTargetFileName))) {
        DashboardUtils_::ErrorResponse(tRequest, 500, "copy_error");
        return;
      }
      snprintf(tTargetPath, sizeof(tTargetPath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tTargetFileName);
      snprintf(tTargetThumbPath, sizeof(tTargetThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tTargetFileName);
    }
    if (!CopyStorageFileByKey(tSourceStorageKey, tSourcePath, tTargetStorageKey, tTargetPath)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "copy_error");
      return;
    }
    if (StorageFileExistsByKey(tSourceStorageKey, tSourceThumbPath)) CopyStorageFileByKey(tSourceStorageKey, tSourceThumbPath, tTargetStorageKey, tTargetThumbPath);
    InvalidateFileCacheByKey(tTargetStorageKey);
    MarkGalleryCacheDirty();
    xLOG("Image copied: %s -> %s [%s → %s]", tSafeFileName, tTargetFileName, tSourceStorageKey.c_str(), tTargetStorageKey.c_str());
    if (mWebSocket) mWebSocket->textAll("{\"type\":\"gallery_copy\",\"message\":\"copy_success\"}");
    DashboardUtils_::OkResponse(tRequest, "copy_success");
  }

  void Dashboard_::HandleImageUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
    Guard tLock;
    if (!tRequest || !AuthorizeRequest(tRequest)) return;
    SImageUploadContext *tUploadContext = static_cast<SImageUploadContext *>(tRequest->_tempObject);
    auto tFailUpload = [&](const char *tMessage) {
      if (!tUploadContext) return;
      tUploadContext->Ok = false;
      if (tMessage && tMessage[0]) {
        strncpy(tUploadContext->Message, tMessage, sizeof(tUploadContext->Message) - 1);
        tUploadContext->Message[sizeof(tUploadContext->Message) - 1] = '\0';
      }
      if (tUploadContext->Handle) {
        tUploadContext->Handle.flush();
        tUploadContext->Handle.close();
      }
      if (tUploadContext->StorageKey[0] && tUploadContext->Path[0]) DeleteStorageFileByKey(String(tUploadContext->StorageKey), tUploadContext->Path);
      if (mWebSocket) {
        char tEvent[320] = "";
        snprintf(tEvent, sizeof(tEvent), "{\"type\":\"gallery_upload_error\",\"kind\":\"%s\",\"file\":\"%s\",\"storage\":\"%s\",\"message\":\"%s\"}",
          tUploadContext->IsThumb ? "thumb" : "main", tUploadContext->FileName, tUploadContext->StorageKey, tUploadContext->Message);
        mWebSocket->textAll(tEvent);
      }
    };
    if (!tUploadContext && tIndex == 0) {
      tUploadContext = new SImageUploadContext();
      if (!tUploadContext) return;
      tRequest->_tempObject = tUploadContext;
      SAppConfig tConfig = CFG.Get<SAppConfig>();
      SDisplayConfig tDisplayConfig = tConfig.Display;
      String tStorageValue;
      String tLinkedValue;
      String tOverwriteValue;
      TryGetRequestValue(tRequest, "storage", tStorageValue);
      TryGetRequestValue(tRequest, "linked", tLinkedValue);
      TryGetRequestValue(tRequest, "overwrite", tOverwriteValue);
      const String tStorageKey = NormalizeStorageKey(tStorageValue);
      const bool tIsThumbnail = tRequest->hasParam("type") && tRequest->getParam("type")->value() == "thumb";
      const bool tOverwrite = ParseBoolValue(tOverwriteValue);
      char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
      DashboardUtils_::SanitizeFilename(tFilename.c_str(), tSafeFileName, sizeof(tSafeFileName));
      NormalizeUploadFileNameToStorageExt(tSafeFileName, sizeof(tSafeFileName));
      tUploadContext->IsThumb = tIsThumbnail;
      tUploadContext->IsLinkedUpload = ParseBoolValue(tLinkedValue);
      tUploadContext->Expected = tRequest->contentLength();
      strncpy(tUploadContext->StorageKey, tStorageKey.c_str(), sizeof(tUploadContext->StorageKey) - 1);
      tUploadContext->StorageKey[sizeof(tUploadContext->StorageKey) - 1] = '\0';
      strncpy(tUploadContext->FileName, tSafeFileName, sizeof(tUploadContext->FileName) - 1);
      tUploadContext->FileName[sizeof(tUploadContext->FileName) - 1] = '\0';
      strncpy(tUploadContext->Message, ResolveImageUploadErrorMessage(tIsThumbnail), sizeof(tUploadContext->Message) - 1);
      tUploadContext->Message[sizeof(tUploadContext->Message) - 1] = '\0';
      if (!tSafeFileName[0] || !IsStorageAvailableByKey(tStorageKey) || !IsStorageKeyAllowed(tStorageKey, tConfig.Storage)) {
        tFailUpload(tUploadContext->Message);
        return;
      }
      if (!tIsThumbnail && tOverwrite) {
        const String tDefaultStorageKey = GetDefaultStorageKey(tConfig.Storage);
        if (tStorageKey == tDefaultStorageKey && tConfig.Display.CurrentFile.length() && tConfig.Display.CurrentFile.equalsIgnoreCase(String(tSafeFileName))) {
          tFailUpload("edit_default_locked");
          return;
        }
      }
      if (!tOverwrite && !IsStrictUploadFileName(tSafeFileName)) {
        if (!ResolveNextUploadFileName(tStorageKey, tDisplayConfig.ImagesDir.c_str(), tSafeFileName, sizeof(tSafeFileName))) {
          tFailUpload("queue_save_error");
          return;
        }
        strncpy(tUploadContext->FileName, tSafeFileName, sizeof(tUploadContext->FileName) - 1);
        tUploadContext->FileName[sizeof(tUploadContext->FileName) - 1] = '\0';
      } else if (!tIsThumbnail && !tOverwrite) {
        char tExistingImagePath[192] = "";
        snprintf(tExistingImagePath, sizeof(tExistingImagePath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
        if (StorageFileExistsByKey(tStorageKey, tExistingImagePath)) {
          if (!ResolveNextUploadFileName(tStorageKey, tDisplayConfig.ImagesDir.c_str(), tSafeFileName, sizeof(tSafeFileName))) {
            tFailUpload("queue_save_error");
            return;
          }
          strncpy(tUploadContext->FileName, tSafeFileName, sizeof(tUploadContext->FileName) - 1);
          tUploadContext->FileName[sizeof(tUploadContext->FileName) - 1] = '\0';
        }
      }
      if (tIsThumbnail) {
        snprintf(tUploadContext->Path, sizeof(tUploadContext->Path), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
        char tThumbDirectory[80] = "";
        snprintf(tThumbDirectory, sizeof(tThumbDirectory), "/%s/.thumbs", tDisplayConfig.ImagesDir.c_str());
        if (!StorageFileExistsByKey(tStorageKey, tThumbDirectory)) {
          char tBaseDirectory[80] = "";
          snprintf(tBaseDirectory, sizeof(tBaseDirectory), "/%s", tDisplayConfig.ImagesDir.c_str());
          if (!StorageFileExistsByKey(tStorageKey, tBaseDirectory)) MakeStorageDirByKey(tStorageKey, tBaseDirectory);
          MakeStorageDirByKey(tStorageKey, tThumbDirectory);
        }
      } else {
        snprintf(tUploadContext->Path, sizeof(tUploadContext->Path), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
        char tDirectoryPath[80] = "";
        snprintf(tDirectoryPath, sizeof(tDirectoryPath), "/%s", tDisplayConfig.ImagesDir.c_str());
        if (!StorageFileExistsByKey(tStorageKey, tDirectoryPath)) MakeStorageDirByKey(tStorageKey, tDirectoryPath);
      }
      if (tOverwrite && StorageFileExistsByKey(tStorageKey, tUploadContext->Path)) {
        if (!DeleteStorageFileByKey(tStorageKey, tUploadContext->Path)) {
          tFailUpload(tUploadContext->Message);
          return;
        }
      }
      tUploadContext->Handle = OpenStorageFileByKey(tStorageKey, tUploadContext->Path, FILE_WRITE, true);
      if (!tUploadContext->Handle) {
        tFailUpload(tUploadContext->Message);
        return;
      }
    }
    if (!tUploadContext || !tUploadContext->Ok) return;
    if (tLength > 0) {
      const size_t tBytesWritten = tUploadContext->Handle.write(tData, tLength);
      if (tBytesWritten != tLength) {
        tFailUpload(tUploadContext->Message);
        return;
      }
      tUploadContext->Written += tBytesWritten;
      if (mWebSocket) {
        const unsigned tProgressPercent = tUploadContext->Expected > 0 ? static_cast<unsigned>((tUploadContext->Written * 100U) / tUploadContext->Expected) : 0U;
        char tEvent[320] = "";
        snprintf(tEvent, sizeof(tEvent), "{\"type\":\"gallery_upload_progress\",\"kind\":\"%s\",\"file\":\"%s\",\"storage\":\"%s\",\"progress\":%u}",
          tUploadContext->IsThumb ? "thumb" : "main", tUploadContext->FileName, tUploadContext->StorageKey, tProgressPercent);
        mWebSocket->textAll(tEvent);
      }
    }
    if (tFinal && tUploadContext->Handle) {
      tUploadContext->Handle.flush();
      tUploadContext->Handle.close();
    }
  }

  void Dashboard_::HandleImageDone(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    SImageUploadContext *tUploadContext = static_cast<SImageUploadContext *>(tRequest->_tempObject);
    if (!tUploadContext || !tUploadContext->Ok || tUploadContext->Written == 0) {
      const char *tMessage = (tUploadContext && tUploadContext->Message[0]) ? tUploadContext->Message : "queue_save_error";
      if (tUploadContext) {
        if (tUploadContext->Handle) tUploadContext->Handle.close();
        if (tUploadContext->StorageKey[0] && tUploadContext->Path[0]) DeleteStorageFileByKey(String(tUploadContext->StorageKey), tUploadContext->Path);
        delete tUploadContext;
        tRequest->_tempObject = nullptr;
      }
      DashboardUtils_::ErrorResponse(tRequest, 500, tMessage);
      return;
    }
    char tUploadedPath[192] = "";
    char tUploadedFileName[97] = "";
    char tUploadedStorageKey[24] = "";
    strncpy(tUploadedPath, tUploadContext->Path, sizeof(tUploadedPath) - 1);
    strncpy(tUploadedFileName, tUploadContext->FileName, sizeof(tUploadedFileName) - 1);
    strncpy(tUploadedStorageKey, tUploadContext->StorageKey, sizeof(tUploadedStorageKey) - 1);
    const bool tWasThumb = tUploadContext->IsThumb;
    const bool tWasLinkedUpload = tUploadContext->IsLinkedUpload;
    delete tUploadContext;
    tRequest->_tempObject = nullptr;
    SDC.InvalidateFileCache();
    LFS.InvalidateFileCache();
    MarkGalleryCacheDirty();
    xLOG("Image %s uploaded: %s [%s]", tWasThumb ? "thumbnail" : "image", tUploadedPath, tUploadedStorageKey[0] ? tUploadedStorageKey : "n/a");
    if (mWebSocket) {
      char tEvent[320] = "";
      snprintf(tEvent, sizeof(tEvent), "{\"type\":\"gallery_upload_complete\",\"kind\":\"%s\",\"file\":\"%s\",\"storage\":\"%s\"}",
        tWasThumb ? "thumb" : "main", tUploadedFileName, tUploadedStorageKey);
      mWebSocket->textAll(tEvent);
      if (tWasThumb || !tWasLinkedUpload) mWebSocket->textAll("{\"type\":\"gallery_refresh\"}");
    }
    DashboardUtils_::OkResponse(tRequest, tWasThumb ? "queue_save_success" : "ok");
  }

  void Dashboard_::HandleOtaStatus(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    Firmware_::Guard tFwLock;
    char tJson[192] = "";
    snprintf(tJson, sizeof(tJson), "{\"ok\":true,\"version\":\"%s\",\"ota_active\":%s,\"ota_written\":%u}", CFG.Get<SDeviceConfig>().Version.c_str(), FWU.IsActive() ? "true" : "false", (unsigned)FWU.GetWritten());
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleOtaUpload(AsyncWebServerRequest *tRequest, const String &tFilename, size_t tIndex, uint8_t *tData, size_t tLength, bool tFinal) {
    (void)tFilename;
    if (!tRequest) return;
    if (tIndex == 0) {
      Guard tLock;
      if (!AuthorizeRequest(tRequest)) return;
      mLastOtaProgressBroadcastMs = 0;
      mLastOtaProgressPercent = 0xFF;
    }
    Firmware_::Guard tFwLock;
    if (tIndex == 0) FWU.Begin(tRequest->contentLength());
    if (FWU.HasError()) {
      if (!FWU.IsFinalEventSent()) {
        const unsigned tPct = FWU.GetTotal() > 0 ? static_cast<unsigned>((FWU.GetWritten() * 100U) / FWU.GetTotal()) : 0U;
        BroadcastOtaEvent("ota_error", FWU.GetLastMessage(), tPct, "error");
      }
      return;
    }
    if (tLength > 0) {
      FWU.Write(tData, tLength);
      if (FWU.HasError()) {
        const unsigned tPct = FWU.GetTotal() > 0 ? static_cast<unsigned>((FWU.GetWritten() * 100U) / FWU.GetTotal()) : 0U;
        BroadcastOtaEvent("ota_error", FWU.GetLastMessage(), tPct, "error");
        return;
      }
      const unsigned tProgressPercent = FWU.GetTotal() > 0 ? static_cast<unsigned>((FWU.GetWritten() * 100U) / FWU.GetTotal()) : 0U;
      const uint32_t tNow = millis();
      const bool tFirst = (mLastOtaProgressPercent == 0xFF);
      const bool tPercentChanged = (mLastOtaProgressPercent != static_cast<uint8_t>(tProgressPercent));
      const bool tIntervalElapsed = (tNow - mLastOtaProgressBroadcastMs) >= kOtaProgressBroadcastIntervalMs;
      if ((tFirst && tPercentChanged) || (tPercentChanged && tIntervalElapsed)) {
        BroadcastOtaEvent("ota_progress", nullptr, tProgressPercent, "uploading");
        mLastOtaProgressPercent = static_cast<uint8_t>(tProgressPercent);
        mLastOtaProgressBroadcastMs = tNow;
      }
    }
    if (tFinal) {
      FWU.Finalize();
      if (FWU.HasError()) {
        const unsigned tPct = FWU.GetTotal() > 0 ? static_cast<unsigned>((FWU.GetWritten() * 100U) / FWU.GetTotal()) : 0U;
        BroadcastOtaEvent("ota_error", FWU.GetLastMessage(), tPct, "error");
      } else BroadcastOtaEvent("ota_complete", "firmware_upload_success", 100U, "done", "#firmware-restart-modal");
    }
  }

  void Dashboard_::HandleOtaDone(AsyncWebServerRequest *tRequest) {
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    Firmware_::Guard tFwLock;
    if (FWU.HasError()) {
      char tMessage[64] = "";
      const char *tLastMsg = FWU.GetLastMessage();
      strncpy(tMessage, (tLastMsg && tLastMsg[0]) ? tLastMsg : "firmware_upload_failed", sizeof(tMessage) - 1);
      tMessage[sizeof(tMessage) - 1] = '\0';
      if (!FWU.IsFinalEventSent()) BroadcastOtaEvent("ota_error", tMessage, 0U, "error");
      FWU.Reset();
      DashboardUtils_::ErrorResponse(tRequest, 500, tMessage);
      return;
    }
    if (FWU.IsActive()) {
      const uint32_t tNow = millis();
      const uint32_t tLastActivityMs = FWU.GetLastActivityMs();
      const uint32_t tElapsedMs = tLastActivityMs > 0 ? (tNow - tLastActivityMs) : 0;
      if (tLastActivityMs > 0 && tElapsedMs >= kOtaFinalizeTimeoutMs) {
        const unsigned tPct = FWU.GetTotal() > 0 ? static_cast<unsigned>((FWU.GetWritten() * 100U) / FWU.GetTotal()) : 0U;
        xLOG("Firmware finalize timeout, no final chunk callback for %u ms", static_cast<unsigned>(tElapsedMs));
        if (!FWU.IsFinalEventSent()) BroadcastOtaEvent("ota_error", "firmware_upload_interrupted", tPct, "error");
        FWU.Abort();
        DashboardUtils_::ErrorResponse(tRequest, 504, "firmware_upload_interrupted");
        return;
      }
      xLOG("Firmware upload completion callback arrived before finalize; waiting for final chunk callback");
      DashboardUtils_::OkResponse(tRequest, "firmware_upload_processing");
      return;
    }
    FWU.Reset();
    DashboardUtils_::OkResponse(tRequest, "firmware_upload_success");
  }

  void Dashboard_::HandleReboot(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    char tToken[65] = "";
    if (TryExtractBearerToken(tRequest, tToken, sizeof(tToken)) && tToken[0]) DestroySession(tToken);
    mRestartPending = true;
    mRestartPendingSince = millis();
    xLOG("Reboot requested via HTTP");
    DashboardUtils_::OkResponse(tRequest, "restart_device");
  }

  void Dashboard_::HandleFactoryReset(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    char tToken[65] = "";
    if (TryExtractBearerToken(tRequest, tToken, sizeof(tToken)) && tToken[0]) DestroySession(tToken);
    mResetPending = true;
    mResetPendingSince = millis();
    xLOG("Factory reset requested via HTTP");
    DashboardUtils_::OkResponse(tRequest, "factory reset");
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
    const unsigned long tTimestamp = static_cast<unsigned long>(time(nullptr));
    const int32_t tUtcOffsetMinutes = ResolveUtcOffsetMinutes();
    const String tTimeZone = ResolveTimeZoneLabel();
    char tVoltBuffer[16] = "0.00";
    snprintf(tVoltBuffer, sizeof(tVoltBuffer), "%.2f", static_cast<float>(tBatteryMilliVolts) / 1000.0f);
    int tLength = snprintf(tBuffer, tSize, "{\"ok\":true,\"error\":false,\"type\":\"status\",\"Data\":{\"DateTime\":{\"TimeStamp\":%lu,\"UtcOffsetMinutes\":%ld,\"TimeZone\":\"%s\"},\"Battery\":{\"State\":\"%s\",\"Percent\":%u,\"Volts\":%s}},\"Uptime\":%u,\"HeapFree\":%u,\"WifiRssi\":%d,\"Storage\":\"%s\",\"CurrentImage\":\"%s\"}", tTimestamp, static_cast<long>(tUtcOffsetMinutes), tTimeZone.c_str(), ResolveBatteryStateKey(), (unsigned)tBatteryPercent, tVoltBuffer, (unsigned)tUptime, (unsigned)tHeap, (int)tWifiRssi, tStorage ? tStorage : "n/a", tDisplay.CurrentFile.c_str());
    if (tLength < 0) {
      tBuffer[0] = '\0';
      return 0;
    }
    return static_cast<size_t>(tLength);
  }

  size_t Dashboard_::BuildConfigJson(char *tBuffer, size_t tSize, const SAppConfig &tConfig) {
    if (!tBuffer || tSize == 0) return 0;
    size_t tPosition = 0;
    auto tAppend = [&](const char *tFormat, ...) {
      if (tPosition >= tSize) return;
      va_list tArguments;
      va_start(tArguments, tFormat);
      int tWritten = vsnprintf(tBuffer + tPosition, tSize - tPosition, tFormat, tArguments);
      va_end(tArguments);
      if (tWritten > 0) tPosition += static_cast<size_t>(tWritten);
    };
    tAppend("{\"device\":{\"name\":\"%s\",\"version\":\"%s\"},", tConfig.Device.Name.c_str(), tConfig.Device.Version.c_str());
    tAppend("\"display\":{\"rotate\":%u,\"brightness\":%u,\"contrast\":%u,\"gamma\":%u,\"current\":\"%s\"},", (unsigned)tConfig.Display.Rotate, (unsigned)tConfig.Display.JpgBrightness, (unsigned)tConfig.Display.JpgContrast, (unsigned)tConfig.Display.JpgGamma, tConfig.Display.CurrentFile.c_str());
    tAppend("\"connection\":{\"ap_enable\":%s,\"ap_ssid\":\"%s\",\"ap_ip\":\"%s\",\"ap_gateway\":\"%s\",\"ap_subnet\":\"%s\",", tConfig.Connection.ApModeEnable ? "true" : "false", tConfig.Connection.ApSsid.c_str(), tConfig.Connection.ApIp.c_str(), tConfig.Connection.ApGateway.c_str(), tConfig.Connection.ApSubnet.c_str());
    tAppend("\"sta_ssid\":\"%s\",\"sta_ip_enable\":%s,\"sta_ip\":\"%s\",\"sta_gateway\":\"%s\",\"sta_subnet\":\"%s\",", tConfig.Connection.StaSsid.c_str(), tConfig.Connection.StaIpEnable ? "true" : "false", tConfig.Connection.StaIp.c_str(), tConfig.Connection.StaGateway.c_str(), tConfig.Connection.StaSubnet.c_str());
    tAppend("\"sta_dns1\":\"%s\",\"sta_dns2\":\"%s\",\"mdns_enable\":%s,\"mdns_name\":\"%s\"},", tConfig.Connection.StaPrimaryDns.c_str(), tConfig.Connection.StaSecondaryDns.c_str(), tConfig.Connection.MdnsEnable ? "true" : "false", tConfig.Connection.MdnsName.c_str());
    tAppend("\"ntp\":{\"server\":\"%s\",\"port\":%u,\"gmt_offset\":%ld,\"daylight_offset\":%ld,\"update_interval\":%lu},", tConfig.Ntp.Server.c_str(), (unsigned)tConfig.Ntp.NtpPort, static_cast<long>(tConfig.Ntp.GMTOffset), static_cast<long>(tConfig.Ntp.DaylightOffset), tConfig.Ntp.LowPowerSyncIntervalSec * 1000UL);
    tAppend("\"timer\":{\"wake_up\":%u,\"wake_hour\":%u},", (unsigned)static_cast<uint8_t>(tConfig.Timer.WakeUp), (unsigned)tConfig.Timer.WakeUpHour);
    String tDashboardLanguage = DashboardUtils_::NormalizeLanguageCode(tConfig.Dashboard.Language);
    if (!tDashboardLanguage.length()) tDashboardLanguage = "en";
    const uint16_t tDashboardTargetWidth = tConfig.Dashboard.TargetWidth ? tConfig.Dashboard.TargetWidth : DASHBOARD_IMG_WIDTH;
    const uint16_t tDashboardTargetHeight = tConfig.Dashboard.TargetHeight ? tConfig.Dashboard.TargetHeight : DASHBOARD_IMG_HEIGHT;
    const uint16_t tDashboardThumbWidth = tConfig.Dashboard.ThumbWidth ? tConfig.Dashboard.ThumbWidth : DASHBOARD_IMG_THUMB_WIDTH;
    const uint16_t tDashboardThumbHeight = tConfig.Dashboard.ThumbHeight ? tConfig.Dashboard.ThumbHeight : DASHBOARD_IMG_THUMB_HEIGHT;
    const uint16_t tDashboardRotate = tConfig.Dashboard.Rotate;
    const String tDashboardImageExt = NormalizeDashboardImageExtension(tConfig.Dashboard.ImageExt);
    std::vector<String> tEnabledLanguages = tConfig.Dashboard.EnabledLanguages;
    DashboardUtils_::NormalizeEnabledLanguages(tEnabledLanguages, tDashboardLanguage);
    tAppend("\"dashboard\":{\"user\":\"%s\",\"has_password\":%s,\"language\":\"%s\",\"enabled_languages\":[", tConfig.Dashboard.User.c_str(), tConfig.Dashboard.Password.isEmpty() ? "false" : "true", tDashboardLanguage.c_str());
    for (size_t tIndex = 0; tIndex < tEnabledLanguages.size(); tIndex++) {
      if (tIndex > 0) tAppend(",");
      tAppend("\"%s\"", EscapeJsonText(tEnabledLanguages[tIndex]).c_str());
    }
    tAppend("],\"editor\":{\"target_width\":%u,\"target_height\":%u,\"thumb_width\":%u,\"thumb_height\":%u,\"rotate\":%u,\"image_ext\":\"%s\"}}}", (unsigned)tDashboardTargetWidth, (unsigned)tDashboardTargetHeight, (unsigned)tDashboardThumbWidth, (unsigned)tDashboardThumbHeight, (unsigned)tDashboardRotate, tDashboardImageExt.c_str());
    return tPosition;
  }

  size_t Dashboard_::BuildImagesJson(char *tBuffer, size_t tSize, const char *tDirectoryPath) {
    if (!tBuffer || tSize == 0) return 0;
    const char *tDirectoryList = STG.ListDir(tDirectoryPath);
    if (!tDirectoryList) tDirectoryList = "";
    size_t tWritePosition = 0;
    tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, "{\"images\":[");
    const char *tCursor = tDirectoryList;
    bool tIsFirstImageEntry = true;
    while (*tCursor && tWritePosition + 8 < tSize) {
      while (*tCursor == '\r' || *tCursor == '\n' || *tCursor == ' ') tCursor++;
      if (!*tCursor) break;
      const char *tLineEnd = tCursor;
      while (*tLineEnd && *tLineEnd != '\r' && *tLineEnd != '\n') tLineEnd++;
      const char *tBracketPosition = static_cast<const char *>(memchr(tCursor, '[', tLineEnd - tCursor));
      bool tIsDirectory = false;
      if (tBracketPosition && (tLineEnd - tBracketPosition) >= 5) {
        if (strncmp(tBracketPosition, "[DIR]", 5) == 0) tIsDirectory = true;
      }
      size_t tNameLength = tBracketPosition ? static_cast<size_t>(tBracketPosition - tCursor) : static_cast<size_t>(tLineEnd - tCursor);
      while (tNameLength > 0 && (tCursor[tNameLength - 1] == ' ' || tCursor[tNameLength - 1] == '/')) tNameLength--;
      if (!tIsDirectory && tNameLength > 0) {
        if (!(tNameLength == 7 && strncmp(tCursor, ".thumbs", 7) == 0)) {
          if (!tIsFirstImageEntry) tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, ",");
          tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, "\"%.*s\"", (int)tNameLength, tCursor);
          tIsFirstImageEntry = false;
        }
      }
      tCursor = tLineEnd;
    }
    tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, "],\"sizes\":{");
    tCursor = tDirectoryList;
    bool tIsFirstSizeEntry = true;
    while (*tCursor && tWritePosition + 16 < tSize) {
      while (*tCursor == '\r' || *tCursor == '\n' || *tCursor == ' ') tCursor++;
      if (!*tCursor) break;
      const char *tLineEnd = tCursor;
      while (*tLineEnd && *tLineEnd != '\r' && *tLineEnd != '\n') tLineEnd++;
      const char *tBracketPosition = static_cast<const char *>(memchr(tCursor, '[', tLineEnd - tCursor));
      bool tIsDirectory = false;
      if (tBracketPosition && (tLineEnd - tBracketPosition) >= 5) {
        if (strncmp(tBracketPosition, "[DIR]", 5) == 0) tIsDirectory = true;
      }
      size_t tNameLength = tBracketPosition ? static_cast<size_t>(tBracketPosition - tCursor) : static_cast<size_t>(tLineEnd - tCursor);
      while (tNameLength > 0 && (tCursor[tNameLength - 1] == ' ' || tCursor[tNameLength - 1] == '/')) tNameLength--;
      if (!tIsDirectory && tNameLength > 0) {
        if (!(tNameLength == 7 && strncmp(tCursor, ".thumbs", 7) == 0)) {
          char tFileName[kMaxUploadFileNameLength + 1] = "";
          size_t tCopyLength = tNameLength;
          if (tCopyLength > sizeof(tFileName) - 1) tCopyLength = sizeof(tFileName) - 1;
          memcpy(tFileName, tCursor, tCopyLength);
          tFileName[tCopyLength] = '\0';
          char tFilePath[192] = "";
          snprintf(tFilePath, sizeof(tFilePath), "%s/%s", tDirectoryPath, tFileName);
          File tFileHandle = STG.OpenFile(tFilePath, FILE_READ, false);
          size_t tFileSize = tFileHandle ? static_cast<size_t>(tFileHandle.size()) : 0;
          if (tFileHandle) tFileHandle.close();
          if (!tIsFirstSizeEntry) tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, ",");
          tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, "\"%s\":%u", tFileName, (unsigned)tFileSize);
          tIsFirstSizeEntry = false;
        }
      }
      tCursor = tLineEnd;
    }
    tWritePosition += snprintf(tBuffer + tWritePosition, tSize - tWritePosition, "}}");
    return tWritePosition;
  }

  void Dashboard_::HandleStats(AsyncWebServerRequest *tRequest) {
    char tJson[kStatsJsonSize] = "{}";
    {
      Guard tLock;
      if (!AuthorizeRequest(tRequest)) {
        DashboardUtils_::UnauthorizedResponse(tRequest);
        return;
      }
      strncpy(tJson, mCachedStatsJson, sizeof(tJson) - 1);
      tJson[sizeof(tJson) - 1] = '\0';
    }
    if (!tJson[0]) strncpy(tJson, "{}", sizeof(tJson) - 1);
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
  }

  void Dashboard_::HandleImageRename(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    if (!tRequest->hasParam("from", true) || !tRequest->hasParam("to", true)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "swap_error");
      return;
    }
    String tStorageValue;
    TryGetRequestValue(tRequest, "storage", tStorageValue);
    const String tStorageKey = NormalizeStorageKey(tStorageValue);
    String tSilentValue;
    TryGetRequestValue(tRequest, "silent", tSilentValue);
    tSilentValue.trim();
    tSilentValue.toLowerCase();
    const bool tSilent = tSilentValue == "1" || tSilentValue == "true" || tSilentValue == "yes" || tSilentValue == "on";
    String tSourceFileName = tRequest->getParam("from", true)->value();
    String tTargetFileName = tRequest->getParam("to", true)->value();
    char tSafeSourceName[kMaxUploadFileNameLength + 1] = "";
    char tSafeTargetName[kMaxUploadFileNameLength + 1] = "";
    DashboardUtils_::SanitizeFilename(tSourceFileName.c_str(), tSafeSourceName, sizeof(tSafeSourceName));
    DashboardUtils_::SanitizeFilename(tTargetFileName.c_str(), tSafeTargetName, sizeof(tSafeTargetName));
    if (!tSafeSourceName[0] || !tSafeTargetName[0] || strcmp(tSafeSourceName, tSafeTargetName) == 0 || !IsStorageAvailableByKey(tStorageKey)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "swap_error");
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (!IsStorageKeyAllowed(tStorageKey, tConfig.Storage)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "swap_error");
      return;
    }
    const String tDefaultStorageKey = GetDefaultStorageKey(tConfig.Storage);
    const String tCurrentFileName = GetStorageLeafFileName(tConfig.Display.CurrentFile.c_str());
    if (tStorageKey == tDefaultStorageKey && tCurrentFileName.length()) {
      if (tCurrentFileName == String(tSafeSourceName)) {
        DashboardUtils_::ErrorResponse(tRequest, 409, "swap_source_default_locked");
        return;
      }
      if (tCurrentFileName == String(tSafeTargetName)) {
        DashboardUtils_::ErrorResponse(tRequest, 409, "swap_target_default_locked");
        return;
      }
    }
    SDisplayConfig tDisplayConfig = tConfig.Display;
    char tSourcePath[192] = "";
    char tTargetPath[192] = "";
    char tSourceThumbPath[192] = "";
    char tTargetThumbPath[192] = "";
    snprintf(tSourcePath, sizeof(tSourcePath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeSourceName);
    snprintf(tTargetPath, sizeof(tTargetPath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeTargetName);
    snprintf(tSourceThumbPath, sizeof(tSourceThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tSafeSourceName);
    snprintf(tTargetThumbPath, sizeof(tTargetThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tSafeTargetName);
    if (!StorageFileExistsByKey(tStorageKey, tSourcePath) || !StorageFileExistsByKey(tStorageKey, tTargetPath)) {
      DashboardUtils_::ErrorResponse(tRequest, 404, "swap_error");
      return;
    }
    const bool tSourceThumbExists = StorageFileExistsByKey(tStorageKey, tSourceThumbPath);
    const bool tTargetThumbExists = StorageFileExistsByKey(tStorageKey, tTargetThumbPath);
    char tTempName[kMaxUploadFileNameLength + 1] = "";
    const char *tExtPos = strrchr(tSafeSourceName, '.');
    const char *tTempExt = tExtPos ? tExtPos : "";
    for (uint8_t tAttempt = 0; tAttempt < 8; tAttempt++) {
      snprintf(tTempName, sizeof(tTempName), "__swap_tmp_%lu_%u%s", static_cast<unsigned long>(millis()), static_cast<unsigned>(tAttempt), tTempExt);
      char tTempImagePathCandidate[192] = "";
      snprintf(tTempImagePathCandidate, sizeof(tTempImagePathCandidate), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tTempName);
      if (!StorageFileExistsByKey(tStorageKey, tTempImagePathCandidate)) break;
      tTempName[0] = '\0';
    }
    if (!tTempName[0]) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "swap_error");
      return;
    }
    char tTempPath[192] = "";
    char tTempThumbPath[192] = "";
    snprintf(tTempPath, sizeof(tTempPath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tTempName);
    snprintf(tTempThumbPath, sizeof(tTempThumbPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tTempName);
    auto tRenamePair = [&](const char *tFromImagePath, const char *tToImagePath, const char *tFromThumbPath, const char *tToThumbPath, bool tHasThumb) {
      if (!RenameStorageFileByKey(tStorageKey, tFromImagePath, tToImagePath)) return false;
      if (tHasThumb && !RenameStorageFileByKey(tStorageKey, tFromThumbPath, tToThumbPath)) {
        RenameStorageFileByKey(tStorageKey, tToImagePath, tFromImagePath);
        return false;
      }
      return true;
    };
    if (!tRenamePair(tSourcePath, tTempPath, tSourceThumbPath, tTempThumbPath, tSourceThumbExists)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "swap_error");
      return;
    }
    if (!tRenamePair(tTargetPath, tSourcePath, tTargetThumbPath, tSourceThumbPath, tTargetThumbExists)) {
      tRenamePair(tTempPath, tSourcePath, tTempThumbPath, tSourceThumbPath, tSourceThumbExists);
      DashboardUtils_::ErrorResponse(tRequest, 500, "swap_error");
      return;
    }
    if (!tRenamePair(tTempPath, tTargetPath, tTempThumbPath, tTargetThumbPath, tSourceThumbExists)) {
      tRenamePair(tSourcePath, tTargetPath, tSourceThumbPath, tTargetThumbPath, tTargetThumbExists);
      tRenamePair(tTempPath, tSourcePath, tTempThumbPath, tSourceThumbPath, tSourceThumbExists);
      DashboardUtils_::ErrorResponse(tRequest, 500, "swap_error");
      return;
    }
    InvalidateFileCacheByKey(tStorageKey);
    MarkGalleryCacheDirty();
    if (!tSilent && mWebSocket) mWebSocket->textAll("{\"type\":\"gallery_swap\",\"message\":\"swap_success\"}");
    xLOG("Image swapped: %s ↔ %s [%s]", tSafeSourceName, tSafeTargetName, tStorageKey.c_str());
    DashboardUtils_::OkResponse(tRequest, "swap_success");
  }

  void Dashboard_::HandleImageSetCurrent(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    if (!tRequest->hasParam("name", true)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "set_default_error");
      return;
    }
    String tStorageValue;
    TryGetRequestValue(tRequest, "storage", tStorageValue);
    const String tStorageKey = NormalizeStorageKey(tStorageValue);
    String tName = tRequest->getParam("name", true)->value();
    char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
    DashboardUtils_::SanitizeFilename(tName.c_str(), tSafeFileName, sizeof(tSafeFileName));
    if (!tSafeFileName[0]) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "set_default_error");
      return;
    }
    SAppConfig tConfig = CFG.Get<SAppConfig>();
    if (!IsStorageKeyAllowed(tStorageKey, tConfig.Storage)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "set_default_error");
      return;
    }
    tConfig.Storage.DefaultFileSystem = tStorageKey == "sd_card" ? EFileSystemType::SDCard : EFileSystemType::LittleFS;
    if (!CFG.SaveAllConfig(tConfig) || !CFG.SaveImageName(tSafeFileName)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "set_default_error");
      return;
    }
    ReloadConfig();
    STG.ReloadConfig();
    MarkGalleryCacheDirty();
    if (mWebSocket) mWebSocket->textAll("{\"type\":\"gallery_set_default\",\"message\":\"set_default_success\"}");
    xLOG("Image set as current → %s", tSafeFileName);
    DashboardUtils_::OkResponse(tRequest, "set_default_success");
  }

  void Dashboard_::HandleImageFile(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    String tStorageValue;
    TryGetRequestValue(tRequest, "storage", tStorageValue);
    const String tStorageKey = NormalizeStorageKey(tStorageValue);
    String tRequestUrl = tRequest->url();
    const String tPrefix = "/api/images/";
    if (!tRequestUrl.startsWith(tPrefix) || tRequestUrl.length() <= tPrefix.length()) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "not_found");
      return;
    }
    int tQueryPos = tRequestUrl.indexOf('?');
    if (tQueryPos >= 0) tRequestUrl = tRequestUrl.substring(0, tQueryPos);
    String tFileName = tRequest->urlDecode(tRequestUrl.substring(tPrefix.length()));
    char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
    DashboardUtils_::SanitizeFilename(tFileName.c_str(), tSafeFileName, sizeof(tSafeFileName));
    if (!tSafeFileName[0]) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "not_found");
      return;
    }
    const SStorageConfig tStorageConfig = CFG.Get<SStorageConfig>();
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tPath[192] = "";
    snprintf(tPath, sizeof(tPath), "/%s/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
    auto tSendFromStorage = [&](const String &tCandidateStorageKey) -> bool {
      if (!IsStorageKeyAllowed(tCandidateStorageKey, tStorageConfig)) return false;
      if (!IsStorageAvailableByKey(tCandidateStorageKey) || !StorageFileExistsByKey(tCandidateStorageKey, tPath)) return false;
      if (tCandidateStorageKey == "sd_card") tRequest->send(SD, String(tPath), "image/jpeg");
      else tRequest->send(LittleFS, String(tPath), "image/jpeg");
      return true;
    };
    if (tStorageValue.length()) {
      if (!tSendFromStorage(tStorageKey)) DashboardUtils_::ErrorResponse(tRequest, 404, "not_found");
      return;
    }
    if (tSendFromStorage(tStorageKey)) return;
    if (tStorageConfig.FallbackEnabled) {
      const String tFallbackStorageKey = tStorageKey == "sd_card" ? String("littlefs") : String("sd_card");
      if (tSendFromStorage(tFallbackStorageKey)) return;
    }
    DashboardUtils_::ErrorResponse(tRequest, 404, "not_found");
  }

  void Dashboard_::HandleImageThumb(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    String tStorageValue;
    TryGetRequestValue(tRequest, "storage", tStorageValue);
    const String tStorageKey = NormalizeStorageKey(tStorageValue);
    String tRequestUrl = tRequest->url();
    const String tPrefix = tRequestUrl.startsWith("/api/images/thumbs/") ? String("/api/images/thumbs/") : String("/api/images/thumb/");
    if (!tRequestUrl.startsWith(tPrefix) || tRequestUrl.length() <= tPrefix.length()) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "not_found");
      return;
    }
    int tQueryPos = tRequestUrl.indexOf('?');
    if (tQueryPos >= 0) tRequestUrl = tRequestUrl.substring(0, tQueryPos);
    String tFileName = tRequest->urlDecode(tRequestUrl.substring(tPrefix.length()));
    char tSafeFileName[kMaxUploadFileNameLength + 1] = "";
    DashboardUtils_::SanitizeFilename(tFileName.c_str(), tSafeFileName, sizeof(tSafeFileName));
    if (!tSafeFileName[0]) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "not_found");
      return;
    }
    const SStorageConfig tStorageConfig = CFG.Get<SStorageConfig>();
    SDisplayConfig tDisplayConfig = CFG.Get<SDisplayConfig>();
    char tPath[192] = "";
    snprintf(tPath, sizeof(tPath), "/%s/.thumbs/%s", tDisplayConfig.ImagesDir.c_str(), tSafeFileName);
    auto tSendFromStorage = [&](const String &tCandidateStorageKey) -> bool {
      if (!IsStorageKeyAllowed(tCandidateStorageKey, tStorageConfig)) return false;
      if (!IsStorageAvailableByKey(tCandidateStorageKey) || !StorageFileExistsByKey(tCandidateStorageKey, tPath)) return false;
      if (tCandidateStorageKey == "sd_card") tRequest->send(SD, String(tPath), "image/jpeg");
      else tRequest->send(LittleFS, String(tPath), "image/jpeg");
      return true;
    };
    if (tStorageValue.length()) {
      if (!tSendFromStorage(tStorageKey)) DashboardUtils_::ErrorResponse(tRequest, 404, "not_found");
      return;
    }
    if (tSendFromStorage(tStorageKey)) return;
    if (tStorageConfig.FallbackEnabled) {
      const String tFallbackStorageKey = tStorageKey == "sd_card" ? String("littlefs") : String("sd_card");
      if (tSendFromStorage(tFallbackStorageKey)) return;
    }
    DashboardUtils_::ErrorResponse(tRequest, 404, "not_found");
  }

  void Dashboard_::HandleRtcSync(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    if (!tRequest->hasParam("epoch", true)) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "missing epoch");
      return;
    }
    unsigned long tEpoch = strtoul(tRequest->getParam("epoch", true)->value().c_str(), nullptr, 10);
    if (tEpoch < 1000000000UL) {
      DashboardUtils_::ErrorResponse(tRequest, 400, "invalid epoch");
      return;
    }
    if (!RTC.SetFromEpoch(tEpoch)) {
      DashboardUtils_::ErrorResponse(tRequest, 500, "rtc sync failed");
      return;
    }
    xLOG("Dashboard → RTC synced from browser: %lu", tEpoch);
    DashboardUtils_::OkResponse(tRequest, "rtc synced");
  }

  void Dashboard_::HandleRtcNow(AsyncWebServerRequest *tRequest) {
    Guard tLock;
    if (!AuthorizeRequest(tRequest)) {
      DashboardUtils_::UnauthorizedResponse(tRequest);
      return;
    }
    const unsigned long tRtcEpoch = RTC.GetEpoch();
    const unsigned long tSystemEpoch = static_cast<unsigned long>(time(nullptr));
    char tJson[192] = "";
    snprintf(tJson, sizeof(tJson), "{\"ok\":true,\"rtc_epoch\":%lu,\"system_epoch\":%lu}", tRtcEpoch, tSystemEpoch);
    DashboardUtils_::JsonResponse(tRequest, 200, tJson);
  }

  size_t Dashboard_::BuildStatsJson(char *tBuffer, size_t tSize) {
    if (!tBuffer || tSize == 0) return 0;
    size_t tPosition = 0;
    auto tAppend = [&](const char *tFormat, ...) {
      if (tPosition >= tSize) return;
      va_list tArguments;
      va_start(tArguments, tFormat);
      int tWritten = vsnprintf(tBuffer + tPosition, tSize - tPosition, tFormat, tArguments);
      va_end(tArguments);
      if (tWritten > 0) tPosition += static_cast<size_t>(tWritten);
    };
    SDeviceConfig tDevice = CFG.Get<SDeviceConfig>();
    tAppend("{\"app_name\":\"%s\",\"version\":\"%s\",", tDevice.Name.c_str(), tDevice.Version.c_str());
    tAppend("\"chip\":\"%s\",\"revision\":%u,\"frequency\":%u,", ESP.getChipModel(), (unsigned)ESP.getChipRevision(), (unsigned)ESP.getCpuFreqMHz());
    tAppend("\"temperature\":%.1f,", temperatureRead());
    tAppend("\"dram_total\":%u,\"dram_free\":%u,", (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    tAppend("\"iram_total\":%u,\"iram_free\":%u,", (unsigned)heap_caps_get_total_size(MALLOC_CAP_EXEC), (unsigned)heap_caps_get_free_size(MALLOC_CAP_EXEC));
    tAppend("\"psram_total\":%u,\"psram_free\":%u,\"heap_free\":%u,", (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
    nvs_stats_t tNvsStats = {};
    nvs_get_stats("nvs", &tNvsStats);
    tAppend("\"nvs_used\":%u,\"nvs_total\":%u,", (unsigned)tNvsStats.used_entries, (unsigned)tNvsStats.total_entries);
    tAppend("\"flash_size\":%u,\"sketch_used\":%u,\"sketch_total\":%u,", (unsigned)ESP.getFlashChipSize(), (unsigned)ESP.getSketchSize(), (unsigned)(ESP.getSketchSize() + ESP.getFreeSketchSpace()));
    tAppend("\"littlefs_used\":%llu,\"littlefs_total\":%llu,", (unsigned long long)mCachedLfsUsedBytes, (unsigned long long)mCachedLfsTotalBytes);
    if (SDC.IsMounted()) tAppend("\"sdcard_used\":%llu,\"sdcard_total\":%llu,\"sdcard_type\":\"%s\",", (unsigned long long)mCachedSdUsedBytes, (unsigned long long)mCachedSdTotalBytes, SDC.CardTypeName());
    else tAppend("\"sdcard_used\":0,\"sdcard_total\":0,");
    const wifi_mode_t tWifiMode = WiFi.getMode();
    tAppend("\"wifi\":%s,", (tWifiMode != WIFI_OFF) ? "true" : "false");
    if (tWifiMode == WIFI_AP || tWifiMode == WIFI_AP_STA) tAppend("\"wifi_mode\":\"AP\",");
    else if (tWifiMode == WIFI_STA) tAppend("\"wifi_mode\":\"STA\",");
    else tAppend("\"wifi_mode\":\"OFF\",");
    const bool tRssiValid = (tWifiMode == WIFI_STA || tWifiMode == WIFI_AP_STA);
    const int tWifiRssi = tRssiValid ? (int)WiFi.RSSI() : 0;
    tAppend("\"wifi_rssi\":%d,\"wifi_rssi_valid\":%s,\"bt\":false,", tWifiRssi, tRssiValid ? "true" : "false");
    const bool tBatteryAvailable = BAT.IsAvailable();
    const bool tBatteryConnected = tBatteryAvailable ? BAT.IsBatteryConnected() : false;
    tAppend("\"battery_available\":%s,\"battery_connected\":%s,\"battery_mv\":%u,\"battery_pct\":%u,",
      tBatteryAvailable ? "true" : "false", tBatteryConnected ? "true" : "false", (unsigned)BAT.GetVoltage(), (unsigned)BAT.GetPercentage());
    tAppend("\"uptime\":%u}", (unsigned)(millis() / 1000));
    return tPosition;
  }

}
