#ifndef DASHBOARD_ASSETS_REGISTRY_H
#define DASHBOARD_ASSETS_REGISTRY_H

#include <App/Global.h>

#include <App/Dashboard/Components/Types.h>
#include <App/Dashboard/Assets/Css/Css.h>
#include <App/Dashboard/Assets/Js/App.h>
#include <App/Dashboard/Assets/Js/DateTime.h>
#include <App/Dashboard/Assets/Js/Display.h>
#include <App/Dashboard/Assets/Js/Firmware.h>
#include <App/Dashboard/Assets/Js/Index.h>
#include <App/Dashboard/Assets/Js/Language.h>
#include <App/Dashboard/Assets/Js/Login.h>
#include <App/Dashboard/Assets/Js/Mdns.h>
#include <App/Dashboard/Assets/Js/Network.h>
#include <App/Dashboard/Assets/Js/Ntp.h>
#include <App/Dashboard/Assets/Js/Settings.h>
#include <App/Dashboard/Assets/Js/Stats.h>
#include <App/Dashboard/Assets/Js/User.h>
#include <App/Dashboard/Assets/Js/WakeUp.h>
#include <App/Dashboard/Assets/Images/Favicon.h>
#include <App/Dashboard/Assets/Images/Logo.h>
#include <App/Dashboard/Assets/Images/LogoDev.h>
#include <App/Dashboard/Assets/Images/LogoMini.h>
#include <App/Dashboard/Assets/Fonts/Font.h>
#include <App/Dashboard/Assets/Fonts/FontItalic.h>
#include <App/Dashboard/Assets/Fonts/Icons.h>

namespace App {
  namespace DashboardAssets {
    static const SDashboardTextAsset sTextAssets[] = {
      { "/assets/css/app.css", "text/css", DashboardCss::Css },
      { "/assets/js/app.js", "application/javascript", DashboardJs::App },
      { "/assets/js/datetime.js", "application/javascript", DashboardJs::DateTime },
      { "/assets/js/display.js", "application/javascript", DashboardJs::Display },
      { "/assets/js/firmware.js", "application/javascript", DashboardJs::Firmware },
      { "/assets/js/index.js", "application/javascript", DashboardJs::Index },
      { "/assets/js/language.js", "application/javascript", DashboardJs::Language },
      { "/assets/js/login.js", "application/javascript", DashboardJs::Login },
      { "/assets/js/mdns.js", "application/javascript", DashboardJs::Mdns },
      { "/assets/js/network.js", "application/javascript", DashboardJs::Network },
      { "/assets/js/ntp.js", "application/javascript", DashboardJs::Ntp },
      { "/assets/js/settings.js", "application/javascript", DashboardJs::Settings },
      { "/assets/js/stats.js", "application/javascript", DashboardJs::Stats },
      { "/assets/js/user.js", "application/javascript", DashboardJs::User },
      { "/assets/js/wakeup.js", "application/javascript", DashboardJs::WakeUp },
      { "/assets/js/wake-up.js", "application/javascript", DashboardJs::WakeUp },
      { "/favicon.svg", "image/svg+xml", DashboardImages::Favicon },
      { "/favicon.ico", "image/svg+xml", DashboardImages::Favicon },
      { "/assets/img/logo.svg", "image/svg+xml", DashboardImages::Logo },
      { "/assets/img/logo-dark.svg", "image/svg+xml", DashboardImages::Logo },
      { "/assets/img/logo-dev.svg", "image/svg+xml", DashboardImages::LogoDev },
      { "/assets/img/logo-dev-dark.svg", "image/svg+xml", DashboardImages::LogoDev },
      { "/assets/img/logo-mini.svg", "image/svg+xml", DashboardImages::LogoMini },
      { "/assets/img/logo-mini-dark.svg", "image/svg+xml", DashboardImages::LogoMini }
    };

    static const SDashboardBinaryAsset sBinaryAssets[] = {
      { "/assets/fonts/font.woff2", "font/woff2", DashboardFonts::Font, DashboardFonts::FontSize },
      { "/assets/fonts/font-italic.woff2", "font/woff2", DashboardFonts::FontItalic, DashboardFonts::FontItalicSize },
      { "/assets/fonts/icons.woff2", "font/woff2", DashboardFonts::Icons, DashboardFonts::IconsSize }
    };

    inline const SDashboardTextAsset *FindTextAsset(const String &tPath) {
      for(size_t tIndex = 0; tIndex < sizeof(sTextAssets) / sizeof(sTextAssets[0]); tIndex++) {
        if(tPath.equalsIgnoreCase(sTextAssets[tIndex].Path)) return &sTextAssets[tIndex];
      }
      return nullptr;
    }

    inline const SDashboardBinaryAsset *FindBinaryAsset(const String &tPath) {
      for(size_t tIndex = 0; tIndex < sizeof(sBinaryAssets) / sizeof(sBinaryAssets[0]); tIndex++) {
        if(tPath.equalsIgnoreCase(sBinaryAssets[tIndex].Path)) return &sBinaryAssets[tIndex];
      }
      return nullptr;
    }
  }
}

#endif