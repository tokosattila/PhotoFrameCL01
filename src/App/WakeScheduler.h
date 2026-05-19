#ifndef WAKE_SCHEDULER_H
#define WAKE_SCHEDULER_H

#include <App/Global.h>

namespace App {

  class WakeScheduler_ {
    DEFINE_TAG("WSC");
    friend class AutoGuard<WakeScheduler_>;
    public:
      using Guard = AutoGuard<WakeScheduler_>;
      static WakeScheduler_ &Instance();
      bool Compute(const STimerConfig &tConfig, const SRTCDateTime &tNow, SWakeSchedule &tOut);
      static constexpr uint32_t kMinDelaySec = 60;
      static constexpr uint32_t kMaxDelaySec = 60UL * 60UL * 24UL * 40UL;
    private:
      WakeScheduler_();
      WakeScheduler_(const WakeScheduler_&) = delete;
      WakeScheduler_ &operator=(const WakeScheduler_&) = delete;
      ~WakeScheduler_();
      mutable SemaphoreHandle_t mMutex;
      static void Lock();
      static void Unlock();
      static void AddSeconds(SRTCDateTime &tDt, uint32_t tSeconds);
      static void AlignToHour(SRTCDateTime &tDt, uint8_t tTargetHour);
      static void FillAlarmSpec(const SRTCDateTime &tNext, SAlarmSpec &tOut);
  };

}

#endif
