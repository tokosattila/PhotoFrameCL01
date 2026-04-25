#ifndef DASHBOARD_JS_DISPLAY_H
#define DASHBOARD_JS_DISPLAY_H

#include <App/Global.h>

namespace App {
  namespace DashboardJs {
    const char Display[] PROGMEM = R"JS(
"use strict";
(function(){
  var tUtils = window.AppCoreUtils;

  function Clamp(tValue, tMin, tMax) {
    return Math.min(tMax, Math.max(tMin, tValue));
  }

  function Init() {
    var tApp = window.AppCore || window.AppPlugin;
    if (tApp) tApp.InitCommonFormUi();

    var tForm = document.querySelector("[data-display-form]");
    if (!tForm) return;

    var tBri = document.getElementById("field-display-brightness");
    var tCon = document.getElementById("field-display-contrast");
    var tGam = document.getElementById("field-display-gamma");
    var tSat = document.getElementById("field-display-saturation");
    var tRed = document.getElementById("field-display-red-gain");
    var tGrn = document.getElementById("field-display-green-gain");
    var tBlu = document.getElementById("field-display-blue-gain");
    var tRot = document.getElementById("field-display-rotation");
    var tReset = tForm.querySelector("[data-display-range-reset]");
    var tResetModal = document.getElementById("display-reset-modal");
    var tResetConfirm = document.querySelector("[data-display-reset-confirm]");

    function IsRgbRange(tRangeInput) {
      return tRangeInput === tRed || tRangeInput === tGrn || tRangeInput === tBlu;
    }

    function ToUiOffset(tRawValue) {
      return Clamp(Math.round(Number(tRawValue || 0)) - 100, -100, 100);
    }

    function ToRawOffset(tUiValue) {
      return String(Clamp(Math.round(Number(tUiValue || 0)) + 100, 0, 200));
    }

    function InitOffsetRange(tRangeInput) {
      if (!tRangeInput) return;
      tRangeInput.value = String(ToUiOffset(tRangeInput.value));
      tRangeInput.min = "-100";
      tRangeInput.max = "100";
      tRangeInput.setAttribute("data-default", "0");
    }

    function FormatValueLabel(tRangeInput, tValue) {
      if (tRangeInput === tGam) return (tValue / 100).toFixed(2);
      var tRounded = Math.round(tValue);
      if (IsRgbRange(tRangeInput) && tRounded > 0) return "+" + String(tRounded);
      return String(tRounded);
    }

    function UpdateRange(tRangeInput) {
      if (!tRangeInput) return;
      var tMin = Number(tRangeInput.min || 0);
      var tMax = Number(tRangeInput.max || 100);
      var tValue = Clamp(Number(tRangeInput.value || tMin), tMin, tMax);
      var tSpan = tMax - tMin;
      var tPercent = tSpan > 0 ? (tValue - tMin) / tSpan * 100 : 0;
      tRangeInput.style.setProperty("--tks-range-progress", tPercent.toFixed(2) + "%");
      var tWrap = tRangeInput.closest ? tRangeInput.closest(".range-inline") : tRangeInput.parentElement;
      var tLabel = tWrap ? tWrap.querySelector("[data-range-value],.range-value") : null;
      if (tLabel) tLabel.textContent = FormatValueLabel(tRangeInput, tValue);
    }

    function SyncRanges() {
      [tBri, tCon, tGam, tSat, tRed, tGrn, tBlu].forEach(UpdateRange);
    }

    function Capture() {
      return {
        brightness: tBri ? tBri.value : "0",
        contrast: tCon ? tCon.value : "0",
        gamma: tGam ? tGam.value : "100",
        saturation: tSat ? tSat.value : "0",
        redGain: tRed ? tRed.value : "0",
        greenGain: tGrn ? tGrn.value : "0",
        blueGain: tBlu ? tBlu.value : "0",
        rotation: tRot ? tRot.value : "0"
      };
    }

    function ReadDefault(tField, tFallback) {
      return tField ? String(tField.getAttribute("data-default") || tFallback) : String(tFallback);
    }

    function Defaults() {
      return {
        brightness: ReadDefault(tBri, "0"),
        contrast: ReadDefault(tCon, "0"),
        gamma: ReadDefault(tGam, "100"),
        saturation: ReadDefault(tSat, "0"),
        redGain: ReadDefault(tRed, "0"),
        greenGain: ReadDefault(tGrn, "0"),
        blueGain: ReadDefault(tBlu, "0"),
        rotation: ReadDefault(tRot, "0")
      };
    }

    function RestoreDefaults() {
      var tDefaults = Defaults();
      if (tBri) tBri.value = tDefaults.brightness;
      if (tCon) tCon.value = tDefaults.contrast;
      if (tGam) tGam.value = tDefaults.gamma;
      if (tSat) tSat.value = tDefaults.saturation;
      if (tRed) tRed.value = tDefaults.redGain;
      if (tGrn) tGrn.value = tDefaults.greenGain;
      if (tBlu) tBlu.value = tDefaults.blueGain;
      if (tRot) tRot.value = tDefaults.rotation;
      SyncRanges();
      if (tApp) tApp.InitCommonFormUi();
      UpdateResetBtn();
    }

    function UpdateResetBtn() {
      if (!tReset) return;
      var tState = Capture();
      var tDefaults = Defaults();
      tReset.disabled = tState.brightness === tDefaults.brightness &&
        tState.contrast === tDefaults.contrast &&
        tState.gamma === tDefaults.gamma &&
        tState.saturation === tDefaults.saturation &&
        tState.redGain === tDefaults.redGain &&
        tState.greenGain === tDefaults.greenGain &&
        tState.blueGain === tDefaults.blueGain &&
        tState.rotation === tDefaults.rotation;
    }

    async function RefreshPageData() {
      if (!tApp || typeof tApp.LoadPageData !== "function") return;
      if (typeof tApp.InvalidatePageData === "function") tApp.InvalidatePageData();
      await tApp.LoadPageData(true);
    }

    function ApplyPageConfig() {
      var tData = tApp && typeof tApp.GetPageSection === "function" ? tApp.GetPageSection("Data") : {};
      var tCfg = tUtils.IsPlainObject(tData) && tUtils.IsPlainObject(tData.Display) ? tData.Display : {};
      if (!tUtils.IsPlainObject(tCfg)) return;
      if (tBri && tCfg.Brightness != null) tBri.value = String(ToUiOffset(tCfg.Brightness));
      if (tCon && tCfg.Contrast != null) tCon.value = String(ToUiOffset(tCfg.Contrast));
      if (tGam && tCfg.Gamma != null) tGam.value = String(tCfg.Gamma);
      if (tSat && tCfg.Saturation != null) tSat.value = String(ToUiOffset(tCfg.Saturation));
      if (tRed && tCfg.RedGain != null) tRed.value = String(ToUiOffset(tCfg.RedGain));
      if (tGrn && tCfg.GreenGain != null) tGrn.value = String(ToUiOffset(tCfg.GreenGain));
      if (tBlu && tCfg.BlueGain != null) tBlu.value = String(ToUiOffset(tCfg.BlueGain));
      if (tRot && tCfg.Rotation != null) tRot.value = String(tCfg.Rotation);
      SyncRanges();
      if (tApp) tApp.InitCommonFormUi();
      UpdateResetBtn();
    }

    [tBri, tCon, tSat, tRed, tGrn, tBlu].forEach(InitOffsetRange);

    [tBri, tCon, tGam, tSat, tRed, tGrn, tBlu].forEach(function(tRangeInput) {
      if (!tRangeInput) return;
      var tRangeHandler = function() {
        UpdateRange(tRangeInput);
        UpdateResetBtn();
      };
      tRangeInput.addEventListener("input", tRangeHandler);
      tRangeInput.addEventListener("change", tRangeHandler);
    });

    if (tRot) {
      var tRotationHandler = function() {
        UpdateResetBtn();
      };
      tRot.addEventListener("input", tRotationHandler);
      tRot.addEventListener("change", tRotationHandler);
    }

    if (tReset) {
      tReset.addEventListener("click", function(e) {
        e.preventDefault();
        if (tApp && tApp.mModal && typeof tApp.mModal.open === "function" && tResetModal) {
          tApp.mModal.open(tResetModal);
          return;
        }
        RestoreDefaults();
      });
    }

    if (tResetConfirm) {
      tResetConfirm.addEventListener("click", function(e) {
        e.preventDefault();
        RestoreDefaults();
        if (tApp && tApp.mModal && typeof tApp.mModal.close === "function" && tResetModal) tApp.mModal.close(tResetModal);
      });
    }

    ApplyPageConfig();

    tForm.addEventListener("submit", async function(e) {
      e.preventDefault();
      if (typeof tForm.reportValidity === "function" && !tForm.reportValidity()) return;

      var tState = Capture();
      var tBody = new URLSearchParams();
      tBody.set("Display[Brightness]", ToRawOffset(tState.brightness));
      tBody.set("Display[Contrast]", ToRawOffset(tState.contrast));
      tBody.set("Display[Gamma]", tState.gamma);
      tBody.set("Display[Saturation]", ToRawOffset(tState.saturation));
      tBody.set("Display[RedGain]", ToRawOffset(tState.redGain));
      tBody.set("Display[GreenGain]", ToRawOffset(tState.greenGain));
      tBody.set("Display[BlueGain]", ToRawOffset(tState.blueGain));
      tBody.set("Display[Rotation]", tState.rotation);

      var tBtn = tForm.querySelector('button[type="submit"]');
      if (tBtn) {
        tBtn.disabled = true;
        tBtn.classList.add("button-loading");
      }

      try {
        var tResponse = await tApp.ApiPost("/api/display/save", tBody.toString());
        if (!tUtils.IsPlainObject(tResponse) || tResponse.ok === false || tResponse.error === true) {
          tApp.ProcessUiMessagePayload(tResponse, {
            defaultType: "error_message",
            defaultMessage: "display_save_error",
            Data: { target: tForm }
          });
          return;
        }
        tApp.ProcessUiMessagePayload(tResponse, {
          defaultType: "info_message",
          defaultMessage: "display_save_success",
          Data: { target: tForm }
        });
        await RefreshPageData();
        ApplyPageConfig();
      } catch {
        if (tApp && tApp.Notification) tApp.Notification(tApp.TranslateMessage("display_save_error"), true, "danger");
      } finally {
        if (tBtn) {
          tBtn.disabled = false;
          tBtn.classList.remove("button-loading");
        }
      }
    });

    document.addEventListener("lang-applied", function() {
      SyncRanges();
      if (tApp) tApp.InitCommonFormUi();
    });

    document.addEventListener("page-data:updated", function() {
      ApplyPageConfig();
    });
  }

  window.AppPageRegistry.Register(["display"], { Init: Init });
})();
)JS";
  }
}

#endif