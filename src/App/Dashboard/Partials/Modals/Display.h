#ifndef DASHBOARD_PARTIALS_MODALS_DISPLAY_H
#define DASHBOARD_PARTIALS_MODALS_DISPLAY_H

#include <App/Global.h>

namespace App {
  namespace DashboardDisplayModals {
    const char Extra[] PROGMEM = R"PAGE(
<div id=display-reset-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=restore_defaults></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_display_restore_defaults></p></div><div class=modal-footer><button type=button class=button data-modal-close data-t=close></button><button type=button class="button button-primary" data-display-reset-confirm data-t=restore></button></div></div></div>
)PAGE";
  }
}

#endif
