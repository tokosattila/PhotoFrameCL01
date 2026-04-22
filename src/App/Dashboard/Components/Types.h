#ifndef DASHBOARD_COMPONENTS_TYPES_H
#define DASHBOARD_COMPONENTS_TYPES_H

#include <App/Global.h>

namespace App {

  struct SDashboardTextAsset {
    const char *Path;
    const char *MimeType;
    const char *Data;
  };

  struct SDashboardBinaryAsset {
    const char *Path;
    const char *MimeType;
    const uint8_t *Data;
    size_t Size;
  };

  struct SDashboardPageDefinition {
    const char *Key;
    const char *LabelKey;
    const char *IconClass;
    const char *Path;
    const char *AliasPath;
    const char *ScriptPath;
    const char *MainHtml;
    const char *ExtraHtml;
    bool ShowSidebar;
    bool ApiPage;
  };

}

#endif