#include <App/Battery.h>

namespace App {

  namespace {
    constexpr uint8_t kI2CRetryCount = 3;
  }

  Battery_ &Battery_::Instance() {
    static Battery_ tInstance;
    return tInstance;
  }

  Battery_::Battery_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Battery_::~Battery_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Battery_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Battery_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  bool Battery_::Init(bool tVerbose) {
    Guard tLock;
    {
      I2CBusGuard tBusLock;
      Wire.begin(kSdaPin, kSclPin);
    }
    mAvailable = TryI2C();
    if (!mAvailable) {
      if (tVerbose) xLOG("Battery management is not available, no power outputs detected on I2C bus");
      return false;
    }
    if (!EnablePowerOutputs(tVerbose)) {
      if (tVerbose) xLOG("Battery management is not available, failed to enable power outputs");
      mAvailable = false;
      return false;
    }
    if (!EnableBatteryADC(tVerbose)) {
      if (tVerbose) xLOG("Battery management is not available, failed to enable battery ADC");
      mAvailable = false;
      return false;
    }
    if (tVerbose) {
      xLOG("Battery management is available, found AXP2101");
      PrintInfo();
    }
    return mAvailable;
  }

  void Battery_::End() {
    Guard tLock;
    {
      I2CBusGuard tBusLock;
      Wire.end();
    }
    mAvailable = false;
  }

  bool Battery_::TryI2C() {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool Battery_::WriteRegister(uint8_t tReg, uint8_t tValue) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(tReg);
      Wire.write(tValue);
      if (Wire.endTransmission() == 0) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  int Battery_::ReadRegister(uint8_t tReg) {
    I2CBusGuard tBusLock;
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      Wire.beginTransmission(kAddress);
      Wire.write(tReg);
      if (Wire.endTransmission() != 0) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      if (Wire.requestFrom(kAddress, (uint8_t)1) < 1) {
        vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
        continue;
      }
      return Wire.read();
    }
    return -1;
  }

  bool Battery_::SetRegisterBit(uint8_t tReg, uint8_t tBit) {
    int tValue = ReadRegister(tReg);
    if (tValue < 0) return false;
    return WriteRegister(tReg, tValue | (1 << tBit));
  }

  bool Battery_::EnablePowerOutputs(bool tVerbose) {
    if (!WriteRegister(0x82, 0x12)) {
      if (tVerbose) xLOG("AXP init fail write reg 0x82 = 0x12");
      return false;
    }
    if (!SetRegisterBit(0x80, 0)) {
      if (tVerbose) xLOG("AXP init fail set bit 0 on reg 0x80");
      return false;
    }
    if (!WriteRegister(0x94, 0x1C)) {
      if (tVerbose) xLOG("AXP init fail write reg 0x94 = 0x1C");
      return false;
    }
    if (!SetRegisterBit(0x90, 2)) {
      if (tVerbose) xLOG("AXP init fail set bit 2 on reg 0x90");
      return false;
    }
    if (!WriteRegister(0x95, 0x1C)) {
      if (tVerbose) xLOG("AXP init fail write reg 0x95 = 0x1C");
      return false;
    }
    if (!SetRegisterBit(0x90, 3)) {
      if (tVerbose) xLOG("AXP init fail set bit 3 on reg 0x90");
      return false;
    }
    if (!WriteRegister(0x96, 0x1C)) {
      if (tVerbose) xLOG("AXP init fail write reg 0x96 = 0x1C");
      return false;
    }
    if (!SetRegisterBit(0x90, 4)) {
      if (tVerbose) xLOG("AXP init fail set bit 4 on reg 0x90");
      return false;
    }
    if (!WriteRegister(0x97, 0x1C)) {
      if (tVerbose) xLOG("AXP init fail write reg 0x97 = 0x1C");
      return false;
    }
    if (!SetRegisterBit(0x90, 5)) {
      if (tVerbose) xLOG("AXP init fail set bit 5 on reg 0x90");
      return false;
    }
    return true;
  }

  bool Battery_::EnableBatteryADC(bool tVerbose) {
    const uint8_t tAdcReg = 0x30;
    int tCurrentValue = ReadRegister(tAdcReg);
    if (tCurrentValue < 0) {
      if (tVerbose) xLOG("AXP adc ctrl read fail reg 0x30, trying direct write 0x01");
      if (!WriteRegister(tAdcReg, 0x01)) {
        if (tVerbose) xLOG("AXP init fail write reg 0x30 = 0x01");
        return false;
      }
      return true;
    }
    if (!WriteRegister(tAdcReg, static_cast<uint8_t>(tCurrentValue | (1 << 0)))) {
      if (tVerbose) xLOG("AXP init fail set bit 0 on reg 0x30");
      return false;
    }
    return true;
  }

  uint16_t Battery_::GetVoltage() {
    Guard tLock;
    if (!mAvailable) return 0;
    int tHigh = ReadRegister(0x34);
    int tLow = ReadRegister(0x35);
    if (tHigh < 0 || tLow < 0) return 0;
    uint16_t tRawVoltage = static_cast<uint16_t>(((tHigh & 0x1F) << 8) | tLow);
    if (tRawVoltage > 5000U) return static_cast<uint16_t>((tRawVoltage + 1U) / 2U);
    return tRawVoltage;
  }

  uint8_t Battery_::GetPercentage() {
    Guard tLock;
    if (!mAvailable) return 0;
    int tValue = ReadRegister(0xA4);
    if (tValue < 0) return 0;
    return (tValue > 100) ? 100 : static_cast<uint8_t>(tValue);
  }

  bool Battery_::IsUsbPowerPresent() {
    Guard tLock;
    if (!mAvailable) return false;
    int tStatus1 = ReadRegister(0x00);
    int tStatus2 = ReadRegister(0x01);
    if (tStatus1 < 0 || tStatus2 < 0) return false;
    const bool tVbusGood = ((tStatus1 >> 5) & 0x01) != 0;
    const bool tVbusInserted = ((tStatus2 >> 3) & 0x01) == 0;
    return tVbusGood && tVbusInserted;
  }

  bool Battery_::IsCharging() {
    Guard tLock;
    if (!mAvailable) return false;
    int tValue = ReadRegister(0x01);
    if (tValue < 0) return false;
    return (tValue >> 5) == 0x01;
  }

  bool Battery_::IsDischarging() {
    Guard tLock;
    if (!mAvailable) return false;
    int tValue = ReadRegister(0x01);
    if (tValue < 0) return false;
    return (tValue >> 5) == 0x02;
  }

  bool Battery_::IsBatteryConnected() {
    Guard tLock;
    if (!mAvailable) return false;
    int tValue = ReadRegister(0x00);
    if (tValue < 0) return false;
    return (tValue >> 3) & 0x01;
  }

  bool Battery_::IsLowBattery() {
    Guard tLock;
    if (!IsBatteryConnected()) return false;
    const uint16_t tVoltageMv = GetVoltage();
    const uint8_t tPercent = GetPercentage();
    return tVoltageMv > 0 && tVoltageMv < kLowBatteryVoltageMv && tPercent <= kLowBatteryPercent;
  }

  String Battery_::GetStatusText() {
    Guard tLock;
    if (!mAvailable || !IsBatteryConnected()) return "no battery";
    char tBuffer[48] = "0% [0.00V]";
    snprintf(tBuffer, sizeof(tBuffer), "%u%% [%.2fV]", static_cast<unsigned>(GetPercentage()), static_cast<float>(GetVoltage()) / 1000.0f);
    String tStatusText = tBuffer;
    if (IsCharging()) tStatusText += " USB charging";
    else if (IsUsbPowerPresent()) tStatusText += " USB power";
    return tStatusText;
  }

  void Battery_::PrintInfo() {
    if (!IsBatteryConnected()) {
      xLOG("No battery attached");
      return;
    }
    xLOG("Battery → %s", GetStatusText().c_str());
  }

}
