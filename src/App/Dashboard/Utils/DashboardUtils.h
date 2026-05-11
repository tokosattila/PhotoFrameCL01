#ifndef DASHBOARD_UTILS_H
#define DASHBOARD_UTILS_H

#include <App/Global.h>
#include <App/Dashboard/Languages/Registry.h>

namespace App {

  class DashboardUtils_ {
    public:
      static inline String ExtractCookieValue(const String &tCookieHeader, const char *tCookieName) {
        if (!tCookieName || !tCookieName[0] || !tCookieHeader.length()) return "";
        String tPattern = String(tCookieName) + "=";
        int tPatternIndex = tCookieHeader.indexOf(tPattern);
        while (tPatternIndex >= 0) {
          bool tAtCookieStart = (tPatternIndex == 0) || (tCookieHeader[tPatternIndex - 1] == ';') || (tCookieHeader[tPatternIndex - 1] == ' ');
          if (tAtCookieStart) {
            int tValueStart = tPatternIndex + tPattern.length();
            int tValueEnd = tCookieHeader.indexOf(';', tValueStart);
            if (tValueEnd < 0) tValueEnd = tCookieHeader.length();
            String tValue = tCookieHeader.substring(tValueStart, tValueEnd);
            tValue.trim();
            return tValue;
          }
          tPatternIndex = tCookieHeader.indexOf(tPattern, tPatternIndex + 1);
        }
        return "";
      }

      static inline void JsonResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tJson) {
        if (!tRequest) return;
        AsyncWebServerResponse *tResponse = tRequest->beginResponse(tCode, "application/json", tJson ? tJson : "{}");
        tResponse->addHeader("Cache-Control", "no-store,no-cache,must-revalidate,post-check=0,pre-check=0");
        tResponse->addHeader("Pragma", "no-cache");
        tResponse->addHeader("Expires", "0");
        tRequest->send(tResponse);
      }

      static inline void OkResponse(AsyncWebServerRequest *tRequest, const char *tMessage = "ok") {
        char tJson[192] = "";
        snprintf(tJson, sizeof(tJson), "{\"ok\":true,\"error\":false,\"type\":\"info_message\",\"message\":\"%s\"}", tMessage ? tMessage : "ok");
        JsonResponse(tRequest, 200, tJson);
      }

      static inline void ErrorResponse(AsyncWebServerRequest *tRequest, int tCode, const char *tMessage) {
        char tJson[224] = "";
        snprintf(tJson, sizeof(tJson), "{\"ok\":false,\"error\":true,\"type\":\"error_message\",\"message\":\"%s\"}", tMessage ? tMessage : "internal_error");
        JsonResponse(tRequest, tCode, tJson);
      }

      static inline void UnauthorizedResponse(AsyncWebServerRequest *tRequest) {
        char tJson[224] = "";
        snprintf(tJson, sizeof(tJson), "{\"ok\":false,\"error\":true,\"type\":\"error_message\",\"message\":\"%s\"}", "unauthorized");
        JsonResponse(tRequest, 401, tJson);
      }

      static inline void Sha256Hex(const uint8_t *tInput, size_t tLength, char *tHexOutput65) {
        if (!tInput || !tHexOutput65) return;
        uint8_t tDigest[32] = {};
        mbedtls_sha256_context tShaContext;
        mbedtls_sha256_init(&tShaContext);
        if (mbedtls_sha256_starts_ret(&tShaContext, 0) == 0 && mbedtls_sha256_update_ret(&tShaContext, tInput, tLength) == 0 && mbedtls_sha256_finish_ret(&tShaContext, tDigest) == 0) {
          for (uint8_t tIndex = 0; tIndex < 32; tIndex++) snprintf(tHexOutput65 + tIndex * 2, 3, "%02x", tDigest[tIndex]);
        } else tHexOutput65[0] = '\0';
        mbedtls_sha256_free(&tShaContext);
      }

      static inline void RandomToken(char *tTokenOutput65) {
        if (!tTokenOutput65) return;
        for (uint8_t tIndex = 0; tIndex < 32; tIndex++) {
          uint8_t tRandomByte = static_cast<uint8_t>(esp_random() & 0xFF);
          snprintf(tTokenOutput65 + tIndex * 2, 3, "%02x", tRandomByte);
        }
        tTokenOutput65[64] = '\0';
      }

      static inline bool IsAlphanumericOrSafe(char tChar) {
        if ((tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') || (tChar >= '0' && tChar <= '9')) return true;
        if (tChar == '.' || tChar == '_' || tChar == '-') return true;
        return false;
      }

      static inline void SanitizeFilename(const char *tInputFileName, char *tOutputFileName, size_t tOutputSize) {
        if (!tOutputFileName || tOutputSize == 0) return;
        tOutputFileName[0] = '\0';
        if (!tInputFileName || !tInputFileName[0]) return;
        size_t tWritePosition = 0;
        for (size_t tIndex = 0; tInputFileName[tIndex] != '\0' && tWritePosition + 1 < tOutputSize; tIndex++) {
          char tChar = tInputFileName[tIndex];
          if (tChar == '/' || tChar == '\\' || tChar == ':') continue;
          if (!IsAlphanumericOrSafe(tChar)) continue;
          tOutputFileName[tWritePosition++] = tChar;
        }
        tOutputFileName[tWritePosition] = '\0';
      }

      static inline String GetLanguageCodeFromPath(const String &tPath) {
        int tStart = tPath.lastIndexOf('/');
        if (tStart < 0) return String();
        int tEnd = tPath.lastIndexOf('.');
        if (tEnd <= tStart) return String();
        String tCode = tPath.substring(tStart + 1, tEnd);
        tCode.toLowerCase();
        return tCode;
      }

      static inline std::vector<String> GetAvailableLanguageCodes() {
        std::vector<String> tCodes;
        const size_t tCount = sizeof(DashboardLanguages::sLanguageAssets) / sizeof(DashboardLanguages::sLanguageAssets[0]);
        tCodes.reserve(tCount);
        for (size_t tIndex = 0; tIndex < tCount; tIndex++) {
          const String tCode = GetLanguageCodeFromPath(String(DashboardLanguages::sLanguageAssets[tIndex].Path ? DashboardLanguages::sLanguageAssets[tIndex].Path : ""));
          if (!tCode.length()) continue;
          bool tExists = false;
          for (const String &tExisting : tCodes) {
            if (tExisting == tCode) { tExists = true; break; }
          }
          if (!tExists) tCodes.push_back(tCode);
        }
        return tCodes;
      }

      static inline bool IsSupportedLanguage(const String &tLanguage) {
        String tCode = tLanguage;
        tCode.trim();
        tCode.toLowerCase();
        if (!tCode.length()) return false;
        const std::vector<String> tCodes = GetAvailableLanguageCodes();
        for (const String &tKnown : tCodes) {
          if (tKnown == tCode) return true;
        }
        return false;
      }

      static inline String NormalizeLanguageCode(const String &tLanguage) {
        String tNormalized = tLanguage;
        tNormalized.trim();
        tNormalized.toLowerCase();
        if (IsSupportedLanguage(tNormalized)) return tNormalized;
        return String();
      }

      static inline bool IsLanguageEnabled(const std::vector<String> &tLanguages, const String &tLanguage) {
        const String tNormalized = NormalizeLanguageCode(tLanguage);
        if (!tNormalized.length()) return false;
        for (const String &tItem : tLanguages) {
          if (NormalizeLanguageCode(tItem) == tNormalized) return true;
        }
        return false;
      }

      static inline String ResolveLanguage(const std::vector<String> &tLanguages, const String &tPreferredLanguage = "en") {
        const String tNormalizedPreferredLanguage = NormalizeLanguageCode(tPreferredLanguage);
        if (tNormalizedPreferredLanguage.length() && IsLanguageEnabled(tLanguages, tNormalizedPreferredLanguage)) return tNormalizedPreferredLanguage;
        if (IsLanguageEnabled(tLanguages, "en")) return String("en");
        for (const String &tLanguage : tLanguages) {
          const String tNormalizedLanguage = NormalizeLanguageCode(tLanguage);
          if (tNormalizedLanguage.length()) return tNormalizedLanguage;
        }
        if (tNormalizedPreferredLanguage.length()) return tNormalizedPreferredLanguage;
        if (IsSupportedLanguage("en")) return String("en");
        const std::vector<String> tAvailableLanguages = GetAvailableLanguageCodes();
        for (const String &tLanguage : tAvailableLanguages) {
          const String tNormalizedLanguage = NormalizeLanguageCode(tLanguage);
          if (tNormalizedLanguage.length()) return tNormalizedLanguage;
        }
        return String();
      }

      static inline void NormalizeEnabledLanguages(std::vector<String> &tLanguages, const String &tPreferredLanguage = "en") {
        std::vector<String> tNormalizedLanguages;
        auto tAppendUnique = [&](const String &tLanguage) {
          const String tNormalizedLanguage = NormalizeLanguageCode(tLanguage);
          if (!tNormalizedLanguage.length()) return;
          for (const String &tExisting : tNormalizedLanguages) {
            if (tExisting == tNormalizedLanguage) return;
          }
          tNormalizedLanguages.push_back(tNormalizedLanguage);
        };
        for (const String &tLanguage : tLanguages) tAppendUnique(tLanguage);
        if (IsSupportedLanguage("en")) tAppendUnique("en");
        if (tNormalizedLanguages.empty()) tAppendUnique(ResolveLanguage(tNormalizedLanguages, tPreferredLanguage));
        if (tNormalizedLanguages.empty() && IsSupportedLanguage("en")) tAppendUnique("en");
        tLanguages = tNormalizedLanguages;
      }

      static inline std::vector<String> ParseEnabledLanguages(const String &tValue, const String &tPreferredLanguage = "en") {
        std::vector<String> tLanguages;
        int tStart = 0;
        while (tStart <= static_cast<int>(tValue.length())) {
          const int tSeparator = tValue.indexOf('|', tStart);
          if (tSeparator >= 0) {
            tLanguages.push_back(tValue.substring(tStart, tSeparator));
            tStart = tSeparator + 1;
            continue;
          }
          tLanguages.push_back(tValue.substring(tStart));
          break;
        }
        NormalizeEnabledLanguages(tLanguages, tPreferredLanguage);
        return tLanguages;
      }

      static inline String JoinEnabledLanguages(const std::vector<String> &tLanguages, const String &tPreferredLanguage = "en") {
        std::vector<String> tNormalizedLanguages = tLanguages;
        NormalizeEnabledLanguages(tNormalizedLanguages, tPreferredLanguage);
        String tJoined;
        for (size_t tIndex = 0; tIndex < tNormalizedLanguages.size(); tIndex++) {
          if (tIndex > 0) tJoined += '|';
          tJoined += tNormalizedLanguages[tIndex];
        }
        return tJoined;
      }
  };

}

#endif
