#include <App/Battery.h>

namespace App {

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
    mWire.begin(mSdaPin, mSclPin);
    mAvailable = TryI2C();
    if (!mAvailable) {
      if (tVerbose) xLOG("Battery → no power outputs detected on I2C bus");
      return false;
    }
    if (!EnablePowerOutputs()) {
      if (tVerbose) xLOG("Battery → failed to enable power outputs");
      mAvailable = false;
      return false;
    }
    if (!EnableBatteryADC()) {
      if (tVerbose) xLOG("Battery → failed to enable battery ADC");
      mAvailable = false;
      return false;
    }
    if (tVerbose) {
      xLOG("Battery → found AXP2101");
      PrintInfo();
    }
    return mAvailable;
  }

  void Battery_::End() {
    Guard tLock;
    mWire.end();
    mAvailable = false;
  }

  bool Battery_::TryI2C() {
    mWire.beginTransmission(mAddress);
    return (mWire.endTransmission() == 0);
  }

  bool Battery_::WriteRegister(uint8_t tReg, uint8_t tValue) {
    mWire.beginTransmission(mAddress);
    mWire.write(tReg);
    mWire.write(tValue);
    return (mWire.endTransmission() == 0);
  }

  int Battery_::ReadRegister(uint8_t tReg) {
    mWire.beginTransmission(mAddress);
    mWire.write(tReg);
    if (mWire.endTransmission() != 0) return -1;
    if (mWire.requestFrom(mAddress, (uint8_t)1) < 1) return -1;
    return mWire.read();
  }

  bool Battery_::SetRegisterBit(uint8_t tReg, uint8_t tBit) {
    int tValue = ReadRegister(tReg);
    if (tValue < 0) return false;
    return WriteRegister(tReg, tValue | (1 << tBit));
  }

  bool Battery_::EnablePowerOutputs() {
    if (!WriteRegister(0x82, 0x12)) return false;
    if (!SetRegisterBit(0x80, 0)) return false;
    if (!WriteRegister(0x94, 0x1C)) return false;
    if (!SetRegisterBit(0x90, 2)) return false;
    if (!WriteRegister(0x95, 0x1C)) return false;
    if (!SetRegisterBit(0x90, 3)) return false;
    return true;
  }

  bool Battery_::EnableBatteryADC() {
    return SetRegisterBit(0x30, 0);
  }

  uint16_t Battery_::GetVoltage() {
    Guard tLock;
    if (!mAvailable) return 0;
    int tHigh = ReadRegister(0x34);
    int tLow = ReadRegister(0x35);
    if (tHigh < 0 || tLow < 0) return 0;
    return ((tHigh & 0x1F) << 8) | tLow;
  }

  uint8_t Battery_::GetPercentage() {
    Guard tLock;
    if (!mAvailable) return 0;
    int tValue = ReadRegister(0xA4);
    if (tValue < 0) return 0;
    return (tValue > 100) ? 100 : static_cast<uint8_t>(tValue);
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

  EChargeState Battery_::GetChargeState() {
    Guard tLock;
    if (!mAvailable) return EChargeState::Stopped;
    int tValue = ReadRegister(0x01);
    if (tValue < 0) return EChargeState::Stopped;
    uint8_t tState = tValue & 0x07;
    if (tState <= 5) return static_cast<EChargeState>(tState);
    return EChargeState::Stopped;
  }

  void Battery_::PrintInfo() {
    if (!IsBatteryConnected()) {
      xLOG("Battery → no battery attached");
      return;
    }
    xLOG("Battery → %umV [%u%%] %s", GetVoltage(), GetPercentage(), IsCharging() ? "charging" : (IsDischarging() ? "discharging" : "standby"));
  }

}
