#ifndef DASHBOARD_PARTIALS_MODALS_H
#define DASHBOARD_PARTIALS_MODALS_H

#include <App/Dashboard/Components/Types.h>
#include <App/Dashboard/Partials/Modals/Common.h>

namespace App {

  class DashboardModals_ {
    public:
      static constexpr const char *kSidebarCommonExtraHtml = DashboardCommonModals::kSidebarCommonExtraHtml;
      static constexpr const char *kAuthCommonExtraHtml = DashboardCommonModals::kAuthCommonExtraHtml;
      
      static String BuildPageModals(const SDashboardPageDefinition &tPage, const SAppConfig &tConfig, const String &tExtraHtml) {
        (void)tConfig;
        String tHtml;
        tHtml.reserve(8192);
        if (tPage.ShowSidebar) tHtml += kSidebarCommonExtraHtml;
        else tHtml += kAuthCommonExtraHtml;
        String tNormalizedExtraHtml = NormalizeExtraHtml(tExtraHtml);
        if (tNormalizedExtraHtml.length()) tHtml += tNormalizedExtraHtml;
        return RemoveModalMarkers(tHtml);
      }

    private:
      static bool HasModalById(const String &tSource, const char *tModalId) {
        if (!tModalId || !tModalId[0]) return false;
        const String tStartMarker = BuildModalMarker(tModalId, "BEGIN");
        if (tSource.indexOf(tStartMarker) >= 0) return true;
        const String tMarker = String("<div id=") + tModalId + " class=modal";
        return tSource.indexOf(tMarker) >= 0;
      }

      static bool HasScrollTopButton(const String &tSource) {
        return tSource.indexOf("<button class=scroll-top-button") >= 0;
      }

      static String RemoveModalById(const String &tSource, const char *tModalId) {
        if (!tModalId || !tModalId[0]) return tSource;
        String tHtml = tSource;
        const String tStartMarker = BuildModalMarker(tModalId, "BEGIN");
        const String tEndMarker = BuildModalMarker(tModalId, "END");
        int tMarkerStart = tHtml.indexOf(tStartMarker);
        while (tMarkerStart >= 0) {
          const int tMarkerEnd = tHtml.indexOf(tEndMarker, tMarkerStart + static_cast<int>(tStartMarker.length()));
          if (tMarkerEnd < 0) {
            tHtml.remove(tMarkerStart, static_cast<int>(tStartMarker.length()));
            tMarkerStart = tHtml.indexOf(tStartMarker);
            continue;
          }
          const int tRemoveLength = (tMarkerEnd - tMarkerStart) + static_cast<int>(tEndMarker.length());
          tHtml.remove(tMarkerStart, tRemoveLength);
          tMarkerStart = tHtml.indexOf(tStartMarker);
        }
        const String tMarker = String("<div id=") + tModalId + " class=modal";
        int tStart = tHtml.indexOf(tMarker);
        while (tStart >= 0) {
          int tCursor = tStart;
          int tDepth = 0;
          bool tFoundRoot = false;
          while (tCursor >= 0 && tCursor < tHtml.length()) {
            const int tOpen = tHtml.indexOf("<div", tCursor);
            const int tClose = tHtml.indexOf("</div>", tCursor);
            if (tOpen >= 0 && (tOpen < tClose || tClose < 0)) {
              tDepth++;
              tFoundRoot = true;
              tCursor = tOpen + 4;
              continue;
            }
            if (tClose >= 0) {
              if (tFoundRoot) tDepth--;
              tCursor = tClose + 6;
              if (tFoundRoot && tDepth <= 0) {
                tHtml.remove(tStart, tCursor - tStart);
                break;
              }
              continue;
            }
            break;
          }
          tStart = tHtml.indexOf(tMarker);
        }
        return tHtml;
      }

      static String RemoveScrollTopButton(const String &tSource) {
        String tHtml = tSource;
        const String tMarker = "<button class=scroll-top-button";
        int tStart = tHtml.indexOf(tMarker);
        while (tStart >= 0) {
          int tEnd = tHtml.indexOf("</button>", tStart);
          if (tEnd < 0) break;
          tEnd += 9;
          tHtml.remove(tStart, tEnd - tStart);
          tStart = tHtml.indexOf(tMarker);
        }
        return tHtml;
      }

      static String NormalizeExtraHtml(const String &tSource) {
        String tHtml = tSource;
        tHtml = RemoveModalById(tHtml, "restart-modal");
        tHtml = RemoveModalById(tHtml, "logout-modal");
        tHtml = RemoveModalById(tHtml, "about-modal");
        tHtml = RemoveScrollTopButton(tHtml);
        tHtml.trim();
        return tHtml;
      }

      static String BuildModalMarker(const char *tModalId, const char *tBoundary) {
        String tMarker;
        tMarker.reserve(32 + (tModalId ? strlen(tModalId) : 0));
        tMarker += "<!--MODAL:";
        tMarker += tModalId ? tModalId : "";
        tMarker += ":";
        tMarker += tBoundary ? tBoundary : "";
        tMarker += "-->";
        return tMarker;
      }

      static String RemoveModalMarkers(const String &tSource) {
        String tHtml = tSource;
        const String tMarkerPrefix = "<!--MODAL:";
        int tStart = tHtml.indexOf(tMarkerPrefix);
        while (tStart >= 0) {
          const int tEnd = tHtml.indexOf("-->", tStart + static_cast<int>(tMarkerPrefix.length()));
          if (tEnd < 0) break;
          tHtml.remove(tStart, (tEnd - tStart) + 3);
          tStart = tHtml.indexOf(tMarkerPrefix);
        }
        return tHtml;
      }
  };

}

#endif