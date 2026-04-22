#ifndef DASHBOARD_PAGE_INDEX_H
#define DASHBOARD_PAGE_INDEX_H

#include <App/Global.h>
#include <App/Dashboard/Partials/Modals/Index.h>

namespace App {
  namespace DashboardPageIndex {
    const char Main[] PROGMEM = R"PAGE(
<main class=dashboard-main><div class=dashboard-section><div class=dashboard-content><div class=breadcrumbs><a href=/index.html class=breadcrumbs-item><span data-t=home></span></a><span class=breadcrumbs-item><span data-t=gallery></span></span></div></div></div><div class=dashboard-section><div class=dashboard-content><div class="mb-2 card-alert" data-dismiss-session=gallery-info-hint><div class=card-alert-icon><i class="icon icon-lightbulb"></i></div><small class=card-alert-text data-t=gallery_page_info></small><button class=card-alert-close type=button data-card-close></button></div></div></div><div class="dashboard-section dashboard-section-sticky mb-7"><div class="dashboard-content dashboard-content-row"><div class=flex-item-grow id=gallery-tabs><div class="menu display-small-none"></div><div class="display-small button dropdown dropdown-full"><span class=icon-text><span></span><span class=dropdown-arrow></span></span><div class=dropdown-items></div></div></div><div id=pic-queue-save-btn class="button button-primary display-none" data-queue-save-success-message data-t-data-queue-save-success-message=queue_save_success data-queue-save-error-message data-t-data-queue-save-error-message=queue_save_error><i class="icon icon-file-earmark-arrow-up"></i><span data-t=upload></span></div></div></div><div class=dashboard-section><div class=dashboard-content><div id=gallery-sections></div></div></div></main>
)PAGE";
    const char *Extra = DashboardIndexModals::Extra;
  }
}

#endif