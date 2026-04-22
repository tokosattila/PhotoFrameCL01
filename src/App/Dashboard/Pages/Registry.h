#ifndef DASHBOARD_PAGES_REGISTRY_H
#define DASHBOARD_PAGES_REGISTRY_H

#include <App/Dashboard/Components/Types.h>
#include <App/Dashboard/Pages/Index.h>
#include <App/Dashboard/Pages/Settings.h>
#include <App/Dashboard/Pages/Stats.h>
#include <App/Dashboard/Pages/Firmware.h>
#include <App/Dashboard/Pages/Display.h>
#include <App/Dashboard/Pages/Network.h>
#include <App/Dashboard/Pages/Mdns.h>
#include <App/Dashboard/Pages/Ntp.h>
#include <App/Dashboard/Pages/DateTime.h>
#include <App/Dashboard/Pages/Language.h>
#include <App/Dashboard/Pages/User.h>
#include <App/Dashboard/Pages/WakeUp.h>
#include <App/Dashboard/Pages/Login.h>

namespace App {
  namespace DashboardPages {
    static const SDashboardPageDefinition sPages[] = {
      { "index", "gallery", "icon-images", "/index.html", "/", "/assets/js/index.js", DashboardPageIndex::Main, DashboardPageIndex::Extra, true, true },
      { "settings", "settings", "icon-gear", "/settings.html", "/settings", "/assets/js/settings.js", DashboardPageSettings::Main, DashboardPageSettings::Extra, true, false },
      { "stats", "statistics", "icon-database", "/stats.html", "/stats", "/assets/js/stats.js", DashboardPageStats::Main, DashboardPageStats::Extra, true, true },
      { "firmware", "firmware", "icon-code-square", "/settings/firmware.html", "/settings/firmware", "/assets/js/firmware.js", DashboardPageFirmware::Main, DashboardPageFirmware::Extra, true, false },
      { "display", "display", "icon-cast", "/settings/display.html", "/settings/display", "/assets/js/display.js", DashboardPageDisplay::Main, DashboardPageDisplay::Extra, true, false },
      { "network", "network", "icon-router", "/settings/network.html", "/settings/network", "/assets/js/network.js", DashboardPageNetwork::Main, DashboardPageNetwork::Extra, true, false },
      { "mdns", "local_name_resolution", "icon-hdd-network", "/settings/mdns.html", "/settings/mdns", "/assets/js/mdns.js", DashboardPageMdns::Main, DashboardPageMdns::Extra, true, false },
      { "ntp", "time_synchronization", "icon-clock-history", "/settings/ntp.html", "/settings/ntp", "/assets/js/ntp.js", DashboardPageNtp::Main, DashboardPageNtp::Extra, true, false },
      { "datetime", "date_and_time", "icon-clock", "/settings/datetime.html", "/settings/datetime", "/assets/js/datetime.js", DashboardPageDateTime::Main, DashboardPageDateTime::Extra, true, false },
      { "language", "language", "icon-translate", "/settings/language.html", "/settings/language", "/assets/js/language.js", DashboardPageLanguage::Main, DashboardPageLanguage::Extra, true, false },
      { "user", "user", "icon-person-square", "/settings/user.html", "/settings/user", "/assets/js/user.js", DashboardPageUser::Main, DashboardPageUser::Extra, true, false },
      { "wakeup", "wake_up", "icon-bell", "/settings/wakeup.html", "/settings/wakeup", "/assets/js/wakeup.js", DashboardPageWakeUp::Main, DashboardPageWakeUp::Extra, true, false },
      { "login", "sign_in", "icon-box-arrow-in-right", "/login.html", "/login", "/assets/js/login.js", DashboardPageLogin::Main, DashboardPageLogin::Extra, false, false }
    };

    inline const SDashboardPageDefinition *FindByKey(const char *tKey) {
      if(!tKey || !tKey[0]) return nullptr;
      for(size_t tIndex = 0; tIndex < sizeof(sPages) / sizeof(sPages[0]); tIndex++) {
        if(strcmp(sPages[tIndex].Key, tKey) == 0) return &sPages[tIndex];
      }
      return nullptr;
    }

    inline const SDashboardPageDefinition *FindByPath(const String &tPath) {
      String tNormalizedPath = tPath;
      if (!tNormalizedPath.length()) tNormalizedPath = "/";
      if (tNormalizedPath.length() > 1 && tNormalizedPath.endsWith("/")) {
        tNormalizedPath.remove(tNormalizedPath.length() - 1);
      }
      for(size_t tIndex = 0; tIndex < sizeof(sPages) / sizeof(sPages[0]); tIndex++) {
        String tPagePath = sPages[tIndex].Path ? String(sPages[tIndex].Path) : String();
        if (tPagePath.length() > 1 && tPagePath.endsWith("/")) tPagePath.remove(tPagePath.length() - 1);
        if(tNormalizedPath.equalsIgnoreCase(tPagePath)) return &sPages[tIndex];
        if(sPages[tIndex].AliasPath && sPages[tIndex].AliasPath[0]) {
          String tAliasPath = String(sPages[tIndex].AliasPath);
          if (tAliasPath.length() > 1 && tAliasPath.endsWith("/")) tAliasPath.remove(tAliasPath.length() - 1);
          if(tNormalizedPath.equalsIgnoreCase(tAliasPath)) return &sPages[tIndex];
        }
      }
      return nullptr;
    }
  }
}

#endif