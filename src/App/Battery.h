#ifndef BATTERY_H
#define BATTERY_H

#include <App/Global.h>

namespace App {

  enum class EChargeState : uint8_t {
    TrickleCharge = 0,
    PreCharge,
    ConstantCurrent,
    ConstantVoltage,
    Done,
    Stopped
  };

  class Battery_ {
    DEFINE_TAG("BAT");
    friend class AutoGuard<Battery_>;
    public:
      using Guard = AutoGuard<Battery_>;
      static Battery_ &Instance();
      bool Init(bool tVerbose = false);
      void End();
      bool IsAvailable() const { return mAvailable; }
      uint16_t GetVoltage();
      uint8_t GetPercentage();
      bool IsCharging();
      bool IsDischarging();
      bool IsBatteryConnected();
      bool IsLowBattery();
      void PrintInfo();
    private:
      Battery_();
      Battery_(const Battery_&) = delete;
      Battery_ &operator=(const Battery_&) = delete;
      ~Battery_();
      static constexpr uint8_t kSdaPin = AXP_SDA_PIN;
      static constexpr uint8_t kSclPin = AXP_SCL_PIN;
      static constexpr uint8_t kAddress = POWER_OUTPUT_ADDRESS;
      static constexpr uint16_t kLowBatteryVoltageMv = 3300;
      static constexpr uint8_t kLowBatteryPercent = 1;
      mutable SemaphoreHandle_t mMutex = nullptr;
      bool mAvailable = false;
      bool WriteRegister(uint8_t tReg, uint8_t tValue);
      int ReadRegister(uint8_t tReg);
      bool SetRegisterBit(uint8_t tReg, uint8_t tBit);
        bool EnablePowerOutputs(bool tVerbose = false);
        bool EnableBatteryADC(bool tVerbose = false);
      bool TryI2C();
      static void Lock();
      static void Unlock();
  };

}

#endif
