#ifndef DASHBOARD_PAGE_LOGS_H
#define DASHBOARD_PAGE_LOGS_H

#include <App/Global.h>
#include <App/Dashboard/Partials/Modals/Common.h>

namespace App {
  namespace DashboardPageLogs {
    const char Main[] PROGMEM = R"PAGE(
<main class=dashboard-main><div class=dashboard-section><div class="dashboard-content dashboard-content"><div class=breadcrumbs><a href=/index.html class=breadcrumbs-item><span data-t=home></span></a><span class=breadcrumbs-item><span data-t=logs></span></span></div></div></div><div class=dashboard-section><div class="dashboard-content dashboard-content"><div class="mb-2 mt-0 card-alert" data-dismiss-session=logs-info-hint><div class=card-alert-icon><i class="icon icon-lightbulb"></i></div><small class=card-alert-text data-t=logs_page_info></small><button class=card-alert-close type=button data-card-close></button></div><div class="card mb-2 card-logs-toolbar-card"><div class="card-body card-logs-toolbar-body"><div class=card-logs-toolbar-left><h4 class="card-logs-toolbar-title m-0"><span data-t=logs></span> <span class=card-logs-toolbar-storage data-logs-storage></span></h4></div><div class=card-logs-toolbar-right><button type=button class=button data-logs-download disabled><i class="icon icon-download"></i><span data-t=download_log></span></button><a href=# class="button button-danger" data-modal=#logs-delete-all-modal><i class="icon icon-trash3"></i><span data-t=delete_all_logs></span></a></div></div></div><div class="dashboard-grid-logs mt-2"><div class="card card-logs-tree"><div class="card-body card-logs-tree-body"><div class=card-logs-tree-scroll data-logs-tree><div class="text-info p-2" data-logs-tree-empty data-t=no_logs_for_day></div></div></div></div><div class="card card-logs-content"><div class="card-body card-logs-content-body"><div class=card-logs-title-row><h4 class="card-logs-title" data-logs-title>—</h4></div><div class=card-logs-pre-wrap><pre class=card-logs-pre data-logs-content></pre></div></div></div></div></div></div><div id=logs-delete-all-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=delete_all_logs></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_delete_all_logs></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=cancel></a><a href=# class="button button-danger" data-modal-close data-logs-confirm-delete data-t=delete></a></div></div></div></main>
)PAGE";
    const char *Extra = DashboardCommonModals::kSidebarCommonExtraHtml;
  }
}

#endif

