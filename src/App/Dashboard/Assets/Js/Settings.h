#ifndef DASHBOARD_JS_SETTINGS_H
#define DASHBOARD_JS_SETTINGS_H

#include <App/Global.h>

namespace App {
  namespace DashboardJs {
    const char Settings[] PROGMEM = R"JS(
"use strict";(function(){function Init(){var tApp=window.AppCore||window.AppPlugin;if(tApp)tApp.InitCommonFormUi()}window.AppPageRegistry.Register(["settings"],{Init:Init})})();
)JS";
  }
}

#endif