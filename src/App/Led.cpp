#include <App/Led.h>

namespace App {

  Led_::Led_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Led_::~Led_() {
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Led_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Led_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  Led_ &Led_::Instance() {
    static Led_ tInstance;
    return tInstance;
  }

  int Led_::FindPin(uint8_t tPin) const {
    for (size_t i = 0; i < mPins.size(); i++) {
      if (mPins[i].Pin == tPin) return static_cast<int>(i);
    }
    return -1;
  }

  void Led_::SetLevel(const SPinConfig &tCfg, bool tOn) const {
    const uint8_t tLevel = (tOn == tCfg.ActiveHigh) ? HIGH : LOW;
    digitalWrite(tCfg.Pin, tLevel);
  }

  void Led_::AddPin(uint8_t tPin, const char *tMessage, bool tActiveHigh) {
    Guard tLock;
    if (FindPin(tPin) != -1) {
      xLOG("Led already added %d pin", tPin);
      return;
    }
    pinMode(tPin, OUTPUT);
    SPinConfig tCfg;
    tCfg.Pin = tPin;
    tCfg.ActiveHigh = tActiveHigh;
    mPins.push_back(tCfg);
    SetLevel(mPins.back(), false);
    xLOG("Led added pin %d %s", tPin, (tMessage ? tMessage : ""));
  }

  void Led_::On(uint8_t tPin) {
    Guard tLock;
    int tIndex = FindPin(tPin);
    if (tIndex == -1) return;
    SetLevel(mPins[tIndex], true);
  }

  void Led_::Off(uint8_t tPin) {
    Guard tLock;
    int tIndex = FindPin(tPin);
    if (tIndex == -1) return;
    SetLevel(mPins[tIndex], false);
  }

};