#ifndef LED_H
#define LED_H

#include <App/Global.h>

namespace App {

  class Led_ {
    DEFINE_TAG("LED");
    friend class AutoGuard<Led_>;
    public:
      using Guard = AutoGuard<Led_>;
      static Led_ &Instance();
      void AddPin(uint8_t tPin, const char *tMessage = "", bool tActiveHigh = false);
      void On(uint8_t tPin);
      void Off(uint8_t tPin);
    private:
      Led_();
      ~Led_();
      Led_(const Led_&) = delete;
      Led_ &operator=(const Led_&) = delete;    
      struct SPinConfig {
        uint8_t Pin = 0;
        bool ActiveHigh = true;
      };
      mutable SemaphoreHandle_t mMutex = nullptr;
      static void Lock();
      static void Unlock();
      std::vector<SPinConfig> mPins;
      int FindPin(uint8_t tPin) const;
      void SetLevel(const SPinConfig &tCfg, bool tOn) const;
  };

};

#endif