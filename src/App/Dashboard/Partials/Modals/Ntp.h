#ifndef DASHBOARD_PARTIALS_MODALS_NTP_H
#define DASHBOARD_PARTIALS_MODALS_NTP_H

#include <App/Global.h>

namespace App {
  namespace DashboardNtpModals {
    const char Extra[] PROGMEM = R"PAGE(
<div id=ntp-sync-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=sync_from_ntp></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_ntp_sync></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=close></a><button type=button class="button button-primary" data-ntp-sync-confirm><i class="icon icon-arrow-repeat"></i><span data-t=synchronization></span></button></div></div></div>
)PAGE";
  }
}

#endif
