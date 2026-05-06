#ifndef DASHBOARD_PAGE_DATE_TIME_H
#define DASHBOARD_PAGE_DATE_TIME_H

#include <App/Global.h>
#include <App/Dashboard/Partials/Modals/Common.h>

namespace App {
  namespace DashboardPageDateTime {
    const char Main[] PROGMEM = R"PAGE(
<main class=dashboard-main>
  <style>
    [data-datetime-form] .select:has(select[disabled]) {
      color: hsl(var(--tks-text-muted));
      background-color: hsl(var(--tks-input-disabled-background));
      border-color: hsl(var(--tks-input-disabled-border));
      outline: transparent solid 4px;
      pointer-events: none;
    }
  </style>
  <div class=dashboard-section>
    <div class="dashboard-content dashboard-content-narrow">
      <div class=breadcrumbs>
        <a href=/index.html class=breadcrumbs-item><span data-t=home></span></a>
        <a href=/settings.html class=breadcrumbs-item><span data-t=settings></span></a>
        <span class=breadcrumbs-item><span data-t=date_and_time></span></span>
      </div>
    </div>
  </div>
  <div class=dashboard-section>
    <div class="dashboard-content dashboard-content-narrow">
      <div class="flex flex-column flex-gap-medium">
        <div class="group flex-item-grow mb-3">
          <a href=/settings.html class="group-item button button-icon"><i class="icon icon-grid"></i></a>
          <div class="flex-item-grow flex dropdown">
            <div class="group-item flex-item-grow select" data-t=date_and_time></div>
            <div class=dropdown-items>
              <a href=/settings/display.html class=dropdown-link data-t=display_eink></a>
              <a href=/settings/network.html class=dropdown-link data-t=network_ap_sta></a>
              <a href=/settings/mdns.html class=dropdown-link data-t=mdns_local_name></a>
              <a href=/settings/ntp.html class=dropdown-link data-t=ntp_sync></a>
              <a href=/settings/datetime.html class="dropdown-link active" data-t=rtc_datetime></a>
              <a href=/settings/wakeup.html class=dropdown-link data-t=rtc_wakeup></a>
              <a href=/settings/language.html class=dropdown-link data-t=language></a>
              <a href=/settings/user.html class=dropdown-link data-t=user></a>
              <a href=/settings/firmware.html class=dropdown-link data-t=firmware></a>
            </div>
          </div>
        </div>
        <div class="mb-2 mt-0 card-alert" data-dismiss-session=datetime-info-hint>
          <div class=card-alert-icon><i class="icon icon-lightbulb"></i></div>
          <small class=card-alert-text data-t=datetime_page_info></small>
          <button class=card-alert-close type=button data-card-close></button>
        </div>
        <form class="flex flex-column flex-gap-medium" data-datetime-form data-datetime-save-success-message data-datetime-save-error-message data-t-data-datetime-save-success-message=datetime_save_success data-t-data-datetime-save-error-message=datetime_save_error>
          <template data-form-alert-template></template>
          <div class="card card-active">
            <div class="card-body card-statistics">
              <div class=card-statistics-left><i class="icon icon-caret-right"></i> <span data-t=device_datetime></span></div>
              <div class=card-statistics-right data-device-datetime-value>N/A</div>
            </div>
          </div>
          <div class="field mt-0" data-datetime-browser-toggle-wrap>
            <div class="toggle toggle-large mt-0">
              <input value=false id=field-datetime-use-browser-timezone name=datetime-use-browser-timezone type=checkbox spellcheck=false data-datetime-use-browser-timezone>
              <label for=field-datetime-use-browser-timezone>
                <i class=icon></i>
                <span class=toggle-label-text data-datetime-browser-toggle-label data-t=browser_datetime_disabled></span>
              </label>
            </div>
          </div>
          <div class=card data-datetime-browser-box>
            <div class=card-body>
              <div class="field mt-0">
                <div><label class=field-label for=field-browser-datetime><span data-t=browser_datetime></span></label></div>
                <input class=input id=field-browser-datetime name=browser-datetime type=text maxlength=32 readonly disabled data-browser-datetime-value>
              </div>
            </div>
          </div>
          <div class=card>
            <div class=card-body>
              <div class="field-grid-2 mt-0">
                <div class="field mt-0" data-error data-t-data-error=invalid_time_zone>
                  <div><label class=field-label for=field-datetime-timezone><span data-t=time_zone></span></label></div>
                  <div class=select>
                    <span></span>
                    <select id=field-datetime-timezone name=datetime-timezone data-datetime-timezone-name></select>
                  </div>
                </div>
                <div class="field mt-0" data-error data-t-data-error=invalid_datetime_value>
                  <div><label class=field-label for=field-datetime-utc-offset-minutes data-t=utc_offset_minutes></label></div>
                  <div class=select>
                    <span></span>
                    <select id=field-datetime-utc-offset-minutes name=datetime-utc-offset-minutes data-datetime-offset-minutes></select>
                  </div>
                </div>
              </div>
              <div class="field mt-3" data-error data-t-data-error=invalid_datetime_value>
                <div><label class=field-label for=field-datetime><span data-t=set_datetime></span></label></div>
                <input type=datetime-local class="input validate" id=field-datetime name=datetime step=1>
              </div>
            </div>
            <div class="card-footer card-footer-equal-2">
              <div></div>
              <button type=submit class="button button-primary"><i class="icon icon-save"></i><span data-t=save></span></button>
            </div>
          </div>
        </form>
      </div>
    </div>
  </div>
</main>
)PAGE";
    const char *Extra = DashboardCommonModals::kSidebarCommonExtraHtml;
  }
}

#endif