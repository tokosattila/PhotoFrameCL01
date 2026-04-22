#ifndef DASHBOARD_PARTIALS_MODALS_FIRMWARE_H
#define DASHBOARD_PARTIALS_MODALS_FIRMWARE_H

#include <App/Global.h>

namespace App {
  namespace DashboardFirmwareModals {
    const char Extra[] PROGMEM = R"PAGE(
<div id=firmware-upload-confirm-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=firmware_upload_confirm_title></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p id=firmware-upload-confirm-message data-t=confirm_firmware_upload></p></div><div class=modal-footer><a href=# class=button data-modal-close data-firmware-upload-cancel data-t=cancel></a><a href=# class="button button-primary" data-modal-close data-firmware-upload-confirm data-t=upload></a></div></div></div><div id=firmware-restart-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=restart_photo_frame></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_restart></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=close></a><a href=# class="button button-primary" data-modal-close data-firmware-restart-confirm data-t=restart></a></div></div></div>
)PAGE";
  }
}

#endif
