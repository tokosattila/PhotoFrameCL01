#ifndef DASHBOARD_PARTIALS_LAYOUT_H
#define DASHBOARD_PARTIALS_LAYOUT_H

#include <App/Dashboard/Pages/Registry.h>
#include <App/Dashboard/Partials/Modals.h>

namespace App {

  class DashboardLayout_ {
    public:
      static String BuildPageDocument(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig) {
        String tHtml;
        String tMainHtml = ReplaceSiteTokens(String(tPage.MainHtml ? tPage.MainHtml : ""), tConfig);
        String tExtraHtml = ReplaceSiteTokens(String(tPage.ExtraHtml ? tPage.ExtraHtml : ""), tConfig);
        ApplyPageState(tPage, tConfig, tMainHtml);
        ApplyPageState(tPage, tConfig, tExtraHtml);
        tHtml.reserve(32768);
        tHtml += BuildHead(tPage, tConfig);
        if (tPage.ShowSidebar) tHtml += BuildHeader(tPage, tConfig);
        else tHtml += BuildHeaderAuth(tConfig);
        if (tPage.ShowSidebar) tHtml += BuildSidebar(tPage, tConfig);
        tHtml += tMainHtml;
        tHtml += BuildFooter(tConfig);
        tHtml += "</div>";
        tHtml += ReplaceSiteTokens(DashboardModals_::BuildPageModals(tPage, tConfig, tExtraHtml), tConfig);
        tHtml.replace("\r", "");
        tHtml.replace("\n", "");
        tHtml += "</body></html>";
        return tHtml;
      }
      
    private:
      static constexpr const char *kDocumentationUrl = "https://github.com/tokosattila/PhotoFrameCL01";

      static String EscapeHtml(const String &tValue) {
        String tSafe = tValue;
        tSafe.replace("&", "&amp;");
        tSafe.replace("\"", "&quot;");
        tSafe.replace("'", "&#39;");
        tSafe.replace("<", "&lt;");
        tSafe.replace(">", "&gt;");
        return tSafe;
      }

      static String NormalizeLanguageCode(const String &tLanguage) {
        String tNormalized = tLanguage;
        tNormalized.trim();
        tNormalized.toLowerCase();
        if (tNormalized == "en" || tNormalized == "hu") return tNormalized;
        return String();
      }

      static bool IsLanguageEnabled(const std::vector<String> &tLanguages, const String &tLanguage) {
        const String tNormalized = NormalizeLanguageCode(tLanguage);
        if (!tNormalized.length()) return false;
        for (const String &tItem : tLanguages) {
          if (NormalizeLanguageCode(tItem) == tNormalized) return true;
        }
        return false;
      }

      static String ReplaceSiteTokens(const String &tSource, const SAppConfig &tConfig) {
        String tHtml = tSource;
        const String tProjectName = EscapeHtml(tConfig.Device.Name.length() ? tConfig.Device.Name : String("Photo Frame CL01"));
        const String tProjectVersion = EscapeHtml(tConfig.Device.Version.length() ? tConfig.Device.Version : String("v1.0"));
        tHtml.replace("__PROJECT_NAME__", tProjectName);
        tHtml.replace("__PROJECT_VERSION__", tProjectVersion);
        tHtml.replace("__PROJECT_URL__", String(kDocumentationUrl));
        return tHtml;
      }

      static String PageUrl(const char *tPageKey) {
        const SDashboardPageDefinition *tPage = DashboardPages::FindByKey(tPageKey);
        if (!tPage || !tPage->Path || !tPage->Path[0]) return "/index.html";
        return String(tPage->Path);
      }

      static bool IsSettingsPage(const char *tPageKey) {
        if (!tPageKey || !tPageKey[0]) return false;
        return strcmp(tPageKey, "settings") == 0 || strcmp(tPageKey, "display") == 0 || strcmp(tPageKey, "network") == 0 || strcmp(tPageKey, "mdns") == 0 || strcmp(tPageKey, "ntp") == 0 || strcmp(tPageKey, "datetime") == 0 || strcmp(tPageKey, "language") == 0 || strcmp(tPageKey, "user") == 0 || strcmp(tPageKey, "wakeup") == 0 || strcmp(tPageKey, "firmware") == 0;
      }

      static String RenderLogoLink(const char *tClassName, const char *tPageKey = "index") {
        String tHtml;
        tHtml += "<a class=\"";
        tHtml += tClassName ? tClassName : "navbar-item";
        tHtml += "\" href=\"";
        tHtml += PageUrl(tPageKey);
        tHtml += "\"><span class=logo><img class=logo-light src=\"/assets/img/logo.svg\"/><img class=logo-dark src=\"/assets/img/logo-dark.svg\"/></span></a>";
        return tHtml;
      }

      static String RenderThemeToggle() {
        return "<a class=navbar-item href=# data-theme-toggle data-theme-icon-light=icon-sun-fill data-theme-icon-dark=icon-moon-stars-fill data-theme-title-light data-theme-title-dark tooltip data-t-data-theme-title-light=switch_to_light_theme data-t-data-theme-title-dark=switch_to_dark_theme data-t-tooltip=switch_to_dark_theme><i class=\"icon icon-sun-fill\"></i></a>";
      }

      static String RenderLanguageDropdown(const SAppConfig &tConfig) {
        const bool tEnglishEnabled = IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "en");
        const bool tHungarianEnabled = IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "hu");
        const uint8_t tEnabledCount = static_cast<uint8_t>((tEnglishEnabled ? 1 : 0) + (tHungarianEnabled ? 1 : 0));
        String tHtml = "<div id=languages class=\"dropdown dropdown-right";
        if (tEnabledCount <= 1) tHtml += " display-none";
        tHtml += "\" data-language-dropdown=1><div class=navbar-item tooltip data-t-tooltip=language><i class=\"icon icon-translate\"></i></div><div class=dropdown-items>";
        if (tEnglishEnabled) tHtml += "<a class=dropdown-link data-language=en href=# data-t=en></a>";
        if (tHungarianEnabled) tHtml += "<a class=dropdown-link data-language=hu href=# data-t=hu></a>";
        tHtml += "</div></div>";
        return tHtml;
      }

      static String RenderNavItem(const char *tPageKey, const char *tCurrentPageKey, const char *tIconClass, const char *tLabelKey, const char *tBadge = nullptr) {
        const bool tActive = (strcmp(tPageKey, "settings") == 0) ? IsSettingsPage(tCurrentPageKey) : strcmp(tPageKey, tCurrentPageKey ? tCurrentPageKey : "") == 0 || (strcmp(tPageKey, "index") == 0 && strcmp(tCurrentPageKey ? tCurrentPageKey : "", "gallery") == 0);
        String tHtml;
        tHtml += "<div class=\"nav-item";
        if (tActive) tHtml += " active";
        tHtml += "\" data-page-id=\"";
        tHtml += tPageKey ? tPageKey : "";
        tHtml += "\"><a class=nav-link href=\"";
        tHtml += PageUrl(tPageKey);
        tHtml += "\"><i class=\"icon ";
        tHtml += tIconClass ? tIconClass : "icon-circle";
        tHtml += "\"></i><span class=nav-item-text data-t=";
        tHtml += tLabelKey ? tLabelKey : "";
        tHtml += "></span>";
        if (tBadge && tBadge[0]) {
          tHtml += "<span class=badge>";
          tHtml += tBadge;
          tHtml += "</span>";
        }
        tHtml += "</a></div>";
        return tHtml;
      }

      static String BuildHead(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig) {
        String tProjectName = EscapeHtml(tConfig.Device.Name.length() ? tConfig.Device.Name : String("Photo Frame CL01"));
        const uint16_t tDashboardRotate = tConfig.Dashboard.Rotate;
        String tOrientation = (tDashboardRotate == 90 || tDashboardRotate == 270) ? "portrait" : "landscape";
        String tTheme = tConfig.Dashboard.Theme;
        tTheme.trim();
        tTheme.toLowerCase();
        if (tTheme != "dark") tTheme = "light";
        String tHtml;
        tHtml.reserve(2048);
        tHtml += "<!doctype html><html lang=";
        tHtml += EscapeHtml(tConfig.Dashboard.Language.length() ? tConfig.Dashboard.Language : String("en"));
        tHtml += " class=";
        tHtml += tTheme;
        tHtml += " style=\"color-scheme:";
        tHtml += tTheme;
        tHtml += "\"><head><meta charset=utf-8><meta http-equiv=X-UA-Compatible content=\"IE=edge\"><meta name=viewport content=\"width=device-width,initial-scale=1.0,minimum-scale=1.0,maximum-scale=1.0,user-scalable=no,viewport-fit=cover\"><meta http-equiv=Cache-Control content=\"no-store,no-cache,must-revalidate,post-check=0,pre-check=0\"><title data-t=page_title></title><style>.modal,.dropdown-items,.tooltip,.dashboard-sidebar-backdrop,.scroll-top-button,.device-unavailable-overlay{display:none}.loading-overlay{position:fixed;inset:0;z-index:2147483647;display:flex;align-items:center;justify-content:center;gap:.55rem;opacity:1;pointer-events:auto;cursor:progress;background:rgba(15,23,42,.08)}.loading-overlay:before{content:"";position:fixed;inset:0 calc(100% - var(--progress,0%)) auto 0;height:2px;pointer-events:none;opacity:.98;background-color:hsl(var(--tks-border-active));transition:inset .3s}.loading-overlay .spinner{position:relative;display:inline-block;width:.725rem;height:.725rem;margin:0}.loading-overlay .spinner:before{content:\"\";position:absolute;inset:0;border-radius:999px;border:1.6px solid currentColor;border-left-color:transparent;animation:.8s linear infinite spinner}.loading-overlay-label{font-size:.725rem;letter-spacing:.15rem;text-transform:uppercase;line-height:1}.boot-loading .dashboard{visibility:hidden!important}.boot-loading .loading-overlay{display:flex!important;opacity:1!important;pointer-events:auto!important}@keyframes spinner{0%{transform:rotate(0)}to{transform:rotate(360deg)}}</style><script>(function(){try{var d='";
        tHtml += tTheme;
        tHtml += "';var t=String(localStorage.getItem('theme')||d).toLowerCase();if(t!=='dark'&&t!=='light')t=d;document.documentElement.classList.remove('light','dark');document.documentElement.classList.add(t);document.documentElement.style.colorScheme=t;}catch(e){document.documentElement.classList.remove('dark','light');document.documentElement.classList.add('";
        tHtml += tTheme;
        tHtml += "');document.documentElement.style.colorScheme='";
        tHtml += tTheme;
        tHtml += "';}})();</script><link href=\"/assets/css/app.css\" rel=stylesheet><script src=\"/assets/js/app.js\" defer></script>";
        if (tPage.ScriptPath && tPage.ScriptPath[0]) {
          tHtml += "<script src=\"";
          tHtml += tPage.ScriptPath;
          tHtml += "\" defer></script>";
        }
        tHtml += "<link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\"></head><body class=\"dashboard-page";
        if (tPage.Key && tPage.Key[0]) {
          tHtml += " ";
          tHtml += tPage.Key;
        }
        tHtml += tPage.ShowSidebar ? (strcmp(tPage.Key ? tPage.Key : "", "index") == 0 ? " layout-regular" : " layout-narrow") : " layout-auth";
        tHtml += " root root-loading boot-loading";
        tHtml += "\" data-page-key=\"";
        tHtml += tPage.Key ? tPage.Key : "index";
        tHtml += "\" data-dashboard-user=\"";
        tHtml += EscapeHtml(tConfig.Dashboard.User.length() ? tConfig.Dashboard.User : String("admin"));
        tHtml += "\" data-app-name=\"";
        tHtml += tProjectName;
        tHtml += "\" data-page-label=\"";
        tHtml += tPage.LabelKey ? tPage.LabelKey : "";
        tHtml += "\"";
        if (tPage.ApiPage) tHtml += " data-api-page";
        tHtml += " data-site-default-theme=\"";
        tHtml += tTheme;
        tHtml += "\" data-site-show-description=";
        tHtml += tConfig.Dashboard.ShowDescription ? "true" : "false";
        tHtml += " data-site-default-orientation=\"";
        tHtml += tOrientation;
        tHtml += "\"><div class=\"loading-overlay is-visible\" data-loading-overlay=1 aria-hidden=false><span class=spinner aria-hidden=true></span><span class=loading-overlay-label data-t=loading_page>LOADING PAGE</span></div><div class=dashboard>";
        return tHtml;
      }

      static String BuildHeader(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig) {
        const bool tIsIndexPage = strcmp(tPage.Key ? tPage.Key : "", "index") == 0;
        String tHtml;
        tHtml.reserve(4096);
        tHtml += "<header class=dashboard-header role=banner><div class=\"dashboard-navbar dashboard-navbar-left\"><div class=navbar>";
        tHtml += RenderLogoLink("dashboard-navbar-item", "index");
        tHtml += "</div></div><div class=\"dashboard-navbar dashboard-navbar-right\"><div class=navbar><div class=\"navbar-item no-cursor display-medium-none\"><i class=\"icon ";
        tHtml += tPage.IconClass ? tPage.IconClass : "icon-circle";
        tHtml += " menu-icon\"></i>";
        if (tIsIndexPage) tHtml += "<span></span><span class=badge></span>";
        else {
          tHtml += "<span data-t=";
          tHtml += tPage.LabelKey ? tPage.LabelKey : "";
          tHtml += "></span>";
        }
        tHtml += "</div>";
        tHtml += RenderLogoLink("navbar-item display-medium navbar-mobile-logo", "index");
        tHtml += "<div class=navbar-group>";
        tHtml += RenderThemeToggle();
        tHtml += RenderLanguageDropdown(tConfig);
        tHtml += "<div class=\"dropdown dropdown-right display-medium-none\"><div class=navbar-item tooltip data-t-tooltip=menu><i class=\"icon icon-three-dots-vertical\"></i></div><div class=dropdown-items><a class=\"dropdown-link dropdown-divider\" href=\"";
        tHtml += PageUrl("user");
        tHtml += "\"><i class=\"icon icon-person-square\"></i><span data-t=user></span></a><div class=dropdown-link data-modal=#about-modal><i class=\"icon icon-card-heading\"></i><span data-t=about></span></div><a class=\"dropdown-link dropdown-divider\" target=_blank rel=\"noopener noreferrer\" href=\"";
        tHtml += kDocumentationUrl;
        tHtml += "\"><i class=\"icon icon-book\"></i><span data-t=documentation></span></a><div class=\"dropdown-link dropdown-divider\" data-modal=#restart-modal><i class=\"icon icon-arrow-repeat\"></i><span data-t=restart></span></div><span class=dropdown-label data-t=logged_in></span><div class=dropdown-link data-modal=#logout-modal><i class=\"icon icon-box-arrow-left\"></i><span data-t=logout></span></div></div></div><div class=\"navbar-item display-medium\" data-sidebar-toggle-aria-label data-t-data-sidebar-toggle-aria-label=open_menu><i class=\"icon icon-list\"></i></div></div></div></div></header>";
        return tHtml;
      }

      static String BuildHeaderAuth(const SAppConfig &tConfig) {
        (void)tConfig;
        String tHtml;
        tHtml.reserve(1024);
        tHtml += "<header class=dashboard-header role=banner><div class=\"dashboard-navbar dashboard-navbar-left\"><div class=navbar>";
        tHtml += RenderLogoLink("dashboard-navbar-item", "index");
        tHtml += "</div></div><div class=\"dashboard-navbar dashboard-navbar-right\"><div class=navbar><div class=\"navbar-item display-medium-none\"></div>";
        tHtml += RenderLogoLink("navbar-item display-medium navbar-mobile-logo", "index");
        tHtml += "<div class=navbar-group>";
        tHtml += RenderThemeToggle();
        tHtml += RenderLanguageDropdown(tConfig);
        tHtml += "</div></div></div></header>";
        return tHtml;
      }

      static String BuildSidebar(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig) {
        (void)tConfig;
        String tHtml;
        tHtml.reserve(4096);
        tHtml += "<div class=dashboard-sidebar-backdrop></div><aside class=dashboard-sidebar><div class=\"dashboard-sidebar-navbar display-medium\"><div class=navbar>";
        tHtml += RenderLogoLink("navbar-item", "index");
        tHtml += "</div></div><div class=nav>";
        tHtml += RenderNavItem("index", tPage.Key, "icon-images", "gallery", "0");
        tHtml += RenderNavItem("settings", tPage.Key, "icon-gear", "settings");
        tHtml += "</div><div class=\"nav nav-tree display-medium\"><div class=nav-label data-t=device_info></div>";
        tHtml += RenderNavItem("user", tPage.Key, "icon-person-square", "user");
        tHtml += "<div class=nav-item><div class=nav-link data-modal=#about-modal><i class=\"icon icon-card-heading\"></i><span class=nav-item-text data-t=about></span></div></div><div class=nav-item><a class=nav-link target=_blank rel=\"noopener noreferrer\" href=\"";
        tHtml += kDocumentationUrl;
        tHtml += "\"><i class=\"icon icon-book\"></i><span class=nav-item-text data-t=documentation></span></a></div></div><div class=\"nav nav-tree\"><div class=nav-label data-t=system_tools></div>";
        tHtml += RenderNavItem("stats", tPage.Key, "icon-database", "statistics");
        tHtml += RenderNavItem("firmware", tPage.Key, "icon-code-square", "firmware");
        tHtml += "</div><div class=\"nav nav-tree display-medium\"><div class=nav-item><div class=nav-link data-modal=#restart-modal><i class=\"icon icon-arrow-repeat\"></i><span class=nav-item-text data-t=restart></span></div></div></div><div class=\"nav nav-tree display-medium\"><div class=nav-item><div class=nav-link data-modal=#logout-modal><i class=\"icon icon-box-arrow-left\"></i><span class=nav-item-text data-t=logout></span></div></div></div></aside>";
        return tHtml;
      }

      static String BuildFooter(const SAppConfig &tConfig) {
        String tHtml;
        tHtml.reserve(1024);
        tHtml += "<footer><div class=dashboard-footer-left><div class=text-info data-device-info-display=1></div></div><div class=dashboard-footer-right><div data-modal=#about-modal class=\"cursor-pointer flex flex-gap display-medium-none\"><span class=\"badge badge-muted\">";
        tHtml += EscapeHtml(tConfig.Device.Name.length() ? tConfig.Device.Name : String("Photo Frame CL01"));
        tHtml += " — ";
        tHtml += EscapeHtml(tConfig.Device.Version.length() ? tConfig.Device.Version : String("v1.0"));
        tHtml += "</span></div><div class=\"dashboard-footer-meta\"><div class=\"dev-logo cursor-pointer\" data-modal=#about-modal role=button><img class=logo-light src=\"/assets/img/logo-dev.svg\"/><img class=logo-dark src=\"/assets/img/logo-dev-dark.svg\"/></div></div></div></footer>";
        return tHtml;
      }

      static bool ReplaceToken(String &tHtml, const char *tToken, const String &tValue) {
        if (!tToken || !tToken[0]) return false;
        const String tNeedle = String(tToken);
        if (tHtml.indexOf(tNeedle) < 0) return false;
        tHtml.replace(tNeedle, tValue);
        return true;
      }

      static void SetInputValueByToken(String &tHtml, const char *tToken, const String &tValue) {
        ReplaceToken(tHtml, tToken, EscapeHtml(tValue));
      }

      static String BuildSelectTokenValuePart(const String &tValue) {
        String tPart;
        tPart.reserve(tValue.length() * 2 + 4);
        for (size_t tIndex = 0; tIndex < tValue.length(); tIndex++) {
          const char tChar = tValue[tIndex];
          if ((tChar >= 'a' && tChar <= 'z') || (tChar >= 'A' && tChar <= 'Z') || (tChar >= '0' && tChar <= '9'))  tPart += tChar;
          else if (tChar == '-') tPart += 'N';
          else tPart += '_';
        }
        return tPart;
      }

      static bool ReplaceSelectedTokenByPrefix(String &tHtml, const char *tTokenPrefix, const String &tValue) {
        if (!tTokenPrefix || !tTokenPrefix[0]) return false;
        String tResolvedPrefix = String(tTokenPrefix);
        if (tResolvedPrefix.endsWith("__")) tResolvedPrefix.remove(tResolvedPrefix.length() - 2);
        tResolvedPrefix += "_";
        const size_t tPrefixLength = tResolvedPrefix.length();
        if (tHtml.indexOf(tResolvedPrefix) < 0) return false;
        String tSelectedToken;
        tSelectedToken.reserve(tPrefixLength + (tValue.length() * 2) + 4);
        tSelectedToken += tResolvedPrefix;
        tSelectedToken += BuildSelectTokenValuePart(tValue);
        tSelectedToken += "__";
        const bool tSelectedTokenFound = tHtml.indexOf(tSelectedToken) >= 0;
        if (tSelectedTokenFound) tHtml.replace(tSelectedToken, "selected");
        int tSearchPos = 0;
        while (true) {
          const int tStart = tHtml.indexOf(tResolvedPrefix, tSearchPos);
          if (tStart < 0) break;
          const int tEnd = tHtml.indexOf("__", tStart + static_cast<int>(tPrefixLength));
          if (tEnd < 0) break;
          tHtml.remove(tStart, (tEnd - tStart) + 2);
          tSearchPos = tStart;
        }
        return true;
      }

      static void SetSelectedByToken(String &tHtml, const char *tTokenPrefix, const String &tValue) {
        ReplaceSelectedTokenByPrefix(tHtml, tTokenPrefix, tValue);
      }

      static void SetCheckedByToken(String &tHtml, const char *tToken, bool tChecked) {
        ReplaceToken(tHtml, tToken, tChecked ? "tValue=true checked" : "tValue=false");
      }

      static void SetRadioCheckedByTokens(String &tHtml, const char *tCheckedToken, const char *tUncheckedToken, const String &tSelectedValue, const String &tCheckedValue) {
        if (tCheckedToken && tCheckedToken[0]) ReplaceToken(tHtml, tCheckedToken, tSelectedValue == tCheckedValue ? "checked" : "");
        if (tUncheckedToken && tUncheckedToken[0]) ReplaceToken(tHtml, tUncheckedToken, tSelectedValue == tCheckedValue ? "" : "checked");
      }

      static void SetCurrentTimeText(String &tHtml) {
        time_t tNow = time(nullptr);
        if (tNow <= 0) return;
        struct tm tTimeInfo = {};
        localtime_r(&tNow, &tTimeInfo);
        char tBuffer[32] = "";
        strftime(tBuffer, sizeof(tBuffer), "%Y-%m-%d %H:%M:%S", &tTimeInfo);
        tHtml.replace(">N/A</div>", String(">") + tBuffer + "</div>");
      }

      static void ApplyDisplayState(const SAppConfig &tConfig, String &tHtml) {
        SetInputValueByToken(tHtml, "__V_DSP_BRT__", String(tConfig.Display.JpgBrightness));
        SetInputValueByToken(tHtml, "__V_DSP_CON__", String(tConfig.Display.JpgContrast));
        SetInputValueByToken(tHtml, "__V_DSP_GAM__", String(tConfig.Display.JpgGamma));
        SetInputValueByToken(tHtml, "__V_DSP_SAT__", String(tConfig.Display.JpgSaturation));
        SetInputValueByToken(tHtml, "__V_DSP_RED__", String(tConfig.Display.JpgRedGain));
        SetInputValueByToken(tHtml, "__V_DSP_GRN__", String(tConfig.Display.JpgGreenGain));
        SetInputValueByToken(tHtml, "__V_DSP_BLU__", String(tConfig.Display.JpgBlueGain));
        SetSelectedByToken(tHtml, "__S_DSP_ROT__", String(tConfig.Display.Rotate));
      }

      static void ApplyNetworkState(const SAppConfig &tConfig, String &tHtml) {
        SetCheckedByToken(tHtml, "__C_NET_MODE__", !tConfig.Connection.ApModeEnable);
        SetCheckedByToken(tHtml, "__C_NET_STA_STATIC_IP__", tConfig.Connection.StaIpEnable);
        SetInputValueByToken(tHtml, "__V_NET_AP_SSID__", tConfig.Connection.ApSsid);
        SetInputValueByToken(tHtml, "__V_NET_AP_PASS__", tConfig.Connection.ApPassword);
        SetInputValueByToken(tHtml, "__V_NET_AP_IP__", tConfig.Connection.ApIp);
        SetInputValueByToken(tHtml, "__V_NET_AP_GW__", tConfig.Connection.ApGateway);
        SetInputValueByToken(tHtml, "__V_NET_AP_SUB__", tConfig.Connection.ApSubnet);
        SetInputValueByToken(tHtml, "__V_NET_STA_SSID__", tConfig.Connection.StaSsid);
        SetInputValueByToken(tHtml, "__V_NET_STA_PASS__", tConfig.Connection.StaPassword);
        SetInputValueByToken(tHtml, "__V_NET_STA_IP__", tConfig.Connection.StaIp);
        SetInputValueByToken(tHtml, "__V_NET_STA_GW__", tConfig.Connection.StaGateway);
        SetInputValueByToken(tHtml, "__V_NET_STA_SUB__", tConfig.Connection.StaSubnet);
        SetInputValueByToken(tHtml, "__V_NET_STA_DNS1__", tConfig.Connection.StaPrimaryDns);
        SetInputValueByToken(tHtml, "__V_NET_STA_DNS2__", tConfig.Connection.StaSecondaryDns);
      }

      static void ApplyMdnsState(const SAppConfig &tConfig, String &tHtml) {
        SetCheckedByToken(tHtml, "__C_MDNS_ENABLED__", tConfig.Connection.MdnsEnable);
        SetInputValueByToken(tHtml, "__V_MDNS_NAME__", tConfig.Connection.MdnsName);
      }

      static void ApplyNtpState(const SAppConfig &tConfig, String &tHtml) {
        SetInputValueByToken(tHtml, "__V_NTP_SRV__", tConfig.Ntp.Server);
        SetSelectedByToken(tHtml, "__S_NTP_GMT__", String(static_cast<long>(tConfig.Ntp.GMTOffset)));
        SetSelectedByToken(tHtml, "__S_NTP_DST__", String(static_cast<long>(tConfig.Ntp.DaylightOffset)));
      }

      static void ApplyDateTimeState(const SAppConfig &tConfig, String &tHtml) {
        (void)tConfig;
        SetCurrentTimeText(tHtml);
      }

      static void ApplyLanguageState(const SAppConfig &tConfig, String &tHtml) {
        String tLanguage = tConfig.Dashboard.Language.length() ? tConfig.Dashboard.Language : String("en");
        tLanguage.toLowerCase();
        String tLanguageTokenValue = tLanguage;
        tLanguageTokenValue.toUpperCase();
        const bool tEnglishEnabled = IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "en");
        const bool tHungarianEnabled = IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "hu");
        String tLanguageOptions;
        tLanguageOptions.reserve(96);
        if (tEnglishEnabled) tLanguageOptions += "<option value=en __S_LANG_EN__ data-t=en></option>";
        if (tHungarianEnabled) tLanguageOptions += "<option value=hu __S_LANG_HU__ data-t=hu></option>";
        ReplaceToken(tHtml, "__LANG_OPTIONS__", tLanguageOptions);
        SetSelectedByToken(tHtml, "__S_LANG__", tLanguageTokenValue);
      }

      static void ApplyUserState(const SAppConfig &tConfig, String &tHtml) {
        SetInputValueByToken(tHtml, "__V_USR_NAME__", tConfig.Dashboard.User.length() ? tConfig.Dashboard.User : String("admin"));
        SetInputValueByToken(tHtml, "__V_USR_PW_CUR__", String(""));
        SetInputValueByToken(tHtml, "__V_USR_PW_NEW__", String(""));
        SetInputValueByToken(tHtml, "__V_USR_PW_CONF__", String(""));
        SetCheckedByToken(tHtml, "__C_USR_THEME__", tConfig.Dashboard.Theme == "dark");
        SetCheckedByToken(tHtml, "__C_USR_DESC__", tConfig.Dashboard.ShowDescription);
        SetCheckedByToken(tHtml, "__C_USR_FALLBACK__", tConfig.Storage.FallbackEnabled);
        SetCheckedByToken(tHtml, "__C_USR_LOGMGR__", tConfig.Device.LogManagerEnabled);
        SetCheckedByToken(tHtml, "__C_USR_LANG_EN__", IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "en"));
        SetCheckedByToken(tHtml, "__C_USR_LANG_HU__", IsLanguageEnabled(tConfig.Dashboard.EnabledLanguages, "hu"));
        SetRadioCheckedByTokens(tHtml, "__C_USR_STORAGE_SD__", "__C_USR_STORAGE_LFS__", tConfig.Storage.DefaultFileSystem == EFileSystemType::SDCard ? String("sd-card") : String("littlefs"), "sd-card");
      }

      static void ApplyWakeupState(const SAppConfig &tConfig, String &tHtml) {
        SetSelectedByToken(tHtml, "__S_WAKE_MODE__", String(static_cast<uint8_t>(tConfig.Timer.WakeUp)));
        SetSelectedByToken(tHtml, "__S_WAKE_HOUR__", String(tConfig.Timer.WakeUpHour));
      }

      typedef void (*TPageStateCallback)(const SAppConfig &, String &);

      struct SPageStateApplier {
        const char *Key;
        TPageStateCallback Callback;
      };

      static void ApplyPageState(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig, String &tHtml) {
        if (!tHtml.length()) return;
        static const SPageStateApplier kStateAppliers[] = {
          {"display", ApplyDisplayState},
          {"network", ApplyNetworkState},
          {"mdns", ApplyMdnsState},
          {"ntp", ApplyNtpState},
          {"datetime", ApplyDateTimeState},
          {"language", ApplyLanguageState},
          {"user", ApplyUserState},
          {"wakeup", ApplyWakeupState},
        };
        const char *tPageKey = tPage.Key ? tPage.Key : "";
        for (size_t tIndex = 0; tIndex < (sizeof(kStateAppliers) / sizeof(kStateAppliers[0])); tIndex++) {
          if (strcmp(tPageKey, kStateAppliers[tIndex].Key) != 0) continue;
          kStateAppliers[tIndex].Callback(tConfig, tHtml);
          break;
        }
      }
  };

}

#endif