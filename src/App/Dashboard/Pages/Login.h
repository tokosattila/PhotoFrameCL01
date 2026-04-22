#ifndef DASHBOARD_PAGE_LOGIN_H
#define DASHBOARD_PAGE_LOGIN_H

#include <App/Global.h>
#include <App/Dashboard/Partials/Modals/Common.h>

namespace App {
  namespace DashboardPageLogin {
    const char Main[] PROGMEM = R"PAGE(
<main class=centered><div class=centered-main><div class="centered-content login-form"><h2 data-t=sign_in></h2><form data-login-form data-t-data-login-error-message=invalid_login><template data-form-alert-template></template><div class=card><div class=card-form><div class=field data-error data-t-data-error=username_required><input class="input validate" type=text name=name placeholder data-t-placeholder=username pattern=^.+$ required></div><div class="field mt-3" data-error data-t-data-error=password_required><input class="input validate" type=password name=password placeholder data-t-placeholder=password pattern=^.+$ required></div><div class=card-form-buttons><button type=submit class="button button-primary"><i class="icon icon-box-arrow-in-right"></i><span data-t=sign_in></span></button></div></div></div></form></div></div></main>
)PAGE";
    const char *Extra = DashboardCommonModals::kAuthCommonExtraHtml;
  }
}

#endif