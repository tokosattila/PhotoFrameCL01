#include <App/WakeScheduler.h>

namespace App {

  WakeScheduler_ &WakeScheduler_::Instance() {
    static WakeScheduler_ tInstance;
    return tInstance;
  }

  WakeScheduler_::WakeScheduler_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  WakeScheduler_::~WakeScheduler_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void WakeScheduler_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void WakeScheduler_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void WakeScheduler_::AddSeconds(SRTCDateTime &tDt, uint32_t tSeconds) {
    unsigned long tEpoch = RTC_::DateTimeToEpoch(tDt);
    tEpoch += tSeconds;
    RTC_::EpochToDateTime(tEpoch, tDt);
  }

  void WakeScheduler_::AlignToHour(SRTCDateTime &tDt, uint8_t tTargetHour) {
    SRTCDateTime tTarget = tDt;
    tTarget.Hour = (uint8_t)(tTargetHour % 24);
    tTarget.Minute = 0;
    tTarget.Second = 0;
    unsigned long tNowEpoch = RTC_::DateTimeToEpoch(tDt);
    unsigned long tTargetEpoch = RTC_::DateTimeToEpoch(tTarget);
    if (tTargetEpoch <= tNowEpoch) tTargetEpoch += (24UL * 60UL * 60UL);
    RTC_::EpochToDateTime(tTargetEpoch, tDt);
  }

  void WakeScheduler_::FillAlarmSpec(const SRTCDateTime &tNext, SAlarmSpec &tOut) {
    tOut.Minute = tNext.Minute;
    tOut.Hour = tNext.Hour;
    tOut.Day = tNext.Day;
    tOut.Weekday = 0;
    tOut.EnableMinute = true;
    tOut.EnableHour = true;
    tOut.EnableDay = true;
    tOut.EnableWeekday = false;
  }

  bool WakeScheduler_::Compute(const STimerConfig &tConfig, const SRTCDateTime &tNow, SWakeSchedule &tOut) {
    Guard tGuard;
    if (tNow.Year < 2000 || tNow.Year > 2099) return false;
    if (tNow.Month < 1 || tNow.Month > 12) return false;
    if (tNow.Day < 1 || tNow.Day > 31) return false;
    if (tNow.Hour > 23 || tNow.Minute > 59 || tNow.Second > 59) return false;
    const uint8_t tHour = (uint8_t)(tConfig.WakeUpHour % 24);
    SRTCDateTime tNext = tNow;
    switch (tConfig.WakeUp) {
      case ETimerWakeUp::Minutes:
        AddSeconds(tNext, 60);
        break;
      case ETimerWakeUp::Hourly:
        AddSeconds(tNext, 60UL * 60UL);
        break;
      case ETimerWakeUp::HalfDay:
        AddSeconds(tNext, 12UL * 60UL * 60UL);
        break;
      case ETimerWakeUp::Daily:
        AlignToHour(tNext, tHour);
        break;
      case ETimerWakeUp::Weekly:
        AlignToHour(tNext, tHour);
        AddSeconds(tNext, 6UL * 24UL * 60UL * 60UL);
        break;
      case ETimerWakeUp::Monthly:
        AlignToHour(tNext, tHour);
        AddSeconds(tNext, 29UL * 24UL * 60UL * 60UL);
        break;
      default:
        AlignToHour(tNext, tHour);
        break;
    }
    unsigned long tNowEpoch = RTC_::DateTimeToEpoch(tNow);
    unsigned long tNextEpoch = RTC_::DateTimeToEpoch(tNext);
    if (tNextEpoch <= tNowEpoch) {
      tNextEpoch = tNowEpoch + kMinDelaySec;
      RTC_::EpochToDateTime(tNextEpoch, tNext);
    }
    unsigned long tDelta = tNextEpoch - tNowEpoch;
    if (tDelta < kMinDelaySec) tDelta = kMinDelaySec;
    if (tDelta > kMaxDelaySec) tDelta = kMaxDelaySec;
    tOut.NextWake = tNext;
    tOut.DelaySeconds = (uint32_t)tDelta;
    FillAlarmSpec(tNext, tOut.Alarm);
    return true;
  }

}
