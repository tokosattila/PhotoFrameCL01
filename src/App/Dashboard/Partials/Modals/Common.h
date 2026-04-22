#ifndef DASHBOARD_PARTIALS_MODALS_COMMON_H
#define DASHBOARD_PARTIALS_MODALS_COMMON_H

namespace App {
  namespace DashboardCommonModals {
    static constexpr const char *kSidebarCommonExtraHtml = "<!--MODAL:restart-modal:BEGIN--><div id=restart-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=restart_photo_frame></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_restart></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=close></a><a href=# class=\"button button-primary\" data-modal-close data-t=restart></a></div></div></div><!--MODAL:restart-modal:END--><!--MODAL:logout-modal:BEGIN--><div id=logout-modal class=modal><div class=modal-dialog><div class=modal-header><span data-t=logout></span><div class=modal-close data-modal-close role=button aria-label data-t-aria-label=close></div></div><div class=modal-body><p data-t=confirm_logout></p></div><div class=modal-footer><a href=# class=button data-modal-close data-t=close></a><a href=# class=\"button button-primary\" data-modal-close data-t=logout></a></div></div></div><!--MODAL:logout-modal:END--><!--MODAL:about-modal:BEGIN--><div id=about-modal class=modal><div class=\"modal-dialog modal-dialog-small\"><div class=modal-body><div class=logo><img class=logo-light src=\"/assets/img/logo-mini.svg\"/><img class=logo-dark src=\"/assets/img/logo-mini-dark.svg\"/></div><hr><p data-t=about_text></p><hr><span class=\"flex flex-between flex-align-center\"><a class=text-link target=_blank rel=\"noopener noreferrer\" href=\"__PROJECT_URL__\">__PROJECT_NAME__</a><a target=_blank rel=\"noopener noreferrer\" class=badge href=\"__PROJECT_URL__\">__PROJECT_VERSION__</a></span></div></div></div><!--MODAL:about-modal:END--><button class=scroll-top-button type=button tooltip data-t-tooltip=scroll_to_top tooltip-options=placement:left><i class=\"icon icon-arrow-up-short scroll-top-button-icon\" aria-hidden=true></i></button>";
    static constexpr const char *kAuthCommonExtraHtml = kSidebarCommonExtraHtml;
  }
}

#endif
