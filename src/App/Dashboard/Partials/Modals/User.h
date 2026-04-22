#ifndef DASHBOARD_PARTIALS_MODALS_USER_H
#define DASHBOARD_PARTIALS_MODALS_USER_H

#include <App/Global.h>

namespace App {
  namespace DashboardUserModals {
    const char Extra[] PROGMEM = R"PAGE(
<div id=user-restore-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=restore_settings></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_restore_settings></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=close></a><a href=# class="button button-primary" data-modal-close data-user-restore-confirm data-t=restore></a></div></div></div>
)PAGE";
  }
}

#endif
