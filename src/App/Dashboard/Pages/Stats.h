#ifndef DASHBOARD_PAGE_STATS_H
#define DASHBOARD_PAGE_STATS_H

#include <App/Global.h>
#include <App/Dashboard/Partials/Modals/Common.h>

namespace App {
  namespace DashboardPageStats {
    const char Main[] PROGMEM = R"PAGE(
<main class=dashboard-main><div class=dashboard-section><div class="dashboard-content dashboard-content-narrow"><div class=breadcrumbs><a href=/index.html class=breadcrumbs-item><span data-t=home></span></a><span class=breadcrumbs-item><span data-t=statistics></span></span></div></div></div><div class=dashboard-section><div class="dashboard-content dashboard-content-narrow"><div class="flex flex-column flex-gap"><div class="mb-2 mt-0 card-alert" data-dismiss-session=stats-info-hint><div class=card-alert-icon><i class="icon icon-lightbulb"></i></div><small class=card-alert-text data-t=stats_page_info></small><button class=card-alert-close type=button data-card-close></button></div><div id=stats-body class="flex flex-column flex-gap"></div></div></div></div></main>
)PAGE";
    const char *Extra = DashboardCommonModals::kSidebarCommonExtraHtml;
  }
}

#endif