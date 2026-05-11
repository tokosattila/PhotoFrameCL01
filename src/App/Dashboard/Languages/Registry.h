#ifndef DASHBOARD_LANGUAGES_REGISTRY_H
#define DASHBOARD_LANGUAGES_REGISTRY_H

#include <App/Global.h>
#include <App/Dashboard/Components/Types.h>
#include <App/Dashboard/Languages/En.h>
#include <App/Dashboard/Languages/Hu.h>

namespace App {
  namespace DashboardLanguages {
    static const SDashboardTextAsset sLanguageAssets[] = {
      { "/lang/en.json", "application/json", kDashboardLangEnglish },
      { "/lang/hu.json", "application/json", kDashboardLangHungarian }
    };
  }
}

#endif