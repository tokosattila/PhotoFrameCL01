#ifndef DASHBOARD_JS_WAKE_UP_H
#define DASHBOARD_JS_WAKE_UP_H

#include <App/Global.h>

namespace App {
  namespace DashboardJs {
    const char WakeUp[] PROGMEM = R"JS(
"use strict";(function(){var tUtils=window.AppCoreUtils;function Init(){var tApp=window.AppCore||window.AppPlugin;if(tApp)tApp.InitCommonFormUi();var tForm=document.querySelector("[data-wakeup-form]");if(!tForm)return;var tInterval=tForm.querySelector('select[name="wake-mode"]');var tHour=tForm.querySelector('select[name="wakeup-hour"]');async function RefreshPageData(){if(!tApp||typeof tApp.LoadPageData!=="function")return;if(typeof tApp.InvalidatePageData==="function")tApp.InvalidatePageData();await tApp.LoadPageData(true)}function ApplyPageConfig(){var tData=tApp&&typeof tApp.GetPageSection==="function"?tApp.GetPageSection("Data"):{};var tCfg=tUtils.IsPlainObject(tData)&&tUtils.IsPlainObject(tData.Timer)?tData.Timer:{};if(!tUtils.IsPlainObject(tCfg))return;if(tInterval&&tCfg.WakeUp!=null)tInterval.value=String(tCfg.WakeUp);if(tHour&&tCfg.WakeUpHour!=null)tHour.value=String(tCfg.WakeUpHour);if(tApp)tApp.InitCommonFormUi()}ApplyPageConfig();tForm.addEventListener("submit",async function(e){e.preventDefault();var tBtn=tForm.querySelector('button[type="submit"]');if(tBtn){tBtn.disabled=true;tBtn.classList.add("button-loading")}try{var tBody=new URLSearchParams;if(tInterval)tBody.set("wakeup[interval]",tInterval.value);if(tHour)tBody.set("wakeup[hour]",tHour.value);var tResponse=await tApp.ApiPost("/api/wakeup/save",tBody.toString());if(!tUtils.IsPlainObject(tResponse)||tResponse.ok===false||tResponse.error===true){tApp.ProcessUiMessagePayload(tResponse,{defaultType:"error_message",defaultMessage:"wakeup_save_error",Data:{target:tForm}});return}tApp.ProcessUiMessagePayload(tResponse,{defaultType:"info_message",defaultMessage:"wakeup_save_success",Data:{target:tForm}});await RefreshPageData();ApplyPageConfig()}catch{if(tApp&&tApp.Notification)tApp.Notification(tApp.TranslateMessage("wakeup_save_error"),true,"danger")}finally{if(tBtn){tBtn.disabled=false;tBtn.classList.remove("button-loading")}}});document.addEventListener("lang-applied",function(){if(tApp)tApp.InitCommonFormUi()});document.addEventListener("page-data:updated",function(){ApplyPageConfig()})}window.AppPageRegistry.Register(["wakeup","wake-up"],{Init:Init})})();
)JS";
  }
}

#endif