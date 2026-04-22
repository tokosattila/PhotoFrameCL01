#include <App/Button.h>

namespace App {

  Button_ &Button_::Instance() {
    static Button_ tInstance;
    return tInstance;
  }

  Button_::Button_() {
    mEventQueue = xQueueCreate(kQueueLength, sizeof(SButtonEvent));
  }

  Button_::~Button_() {
    if (mTaskHandle) vTaskDelete(mTaskHandle);
    if (mEventQueue) vQueueDelete(mEventQueue);
  }

  void Button_::AddPin(uint8_t tPin, const char *tMessage, bool tPullUp) {
    if (mTaskHandle) {
      xLOG("Pin cannot be added after Start() has been called.");
      return;
    }
    if (mPinMask & (1ULL << tPin)) {
      xLOG("Already added → %d pin", tPin);
      return;
    }
    mPinMask |= (1ULL << tPin);
    gpio_config_t tConfig = {};
    tConfig.pin_bit_mask = (1ULL << tPin);
    tConfig.mode = GPIO_MODE_INPUT;
    tConfig.pull_up_en = tPullUp ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    tConfig.pull_down_en = tPullUp ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    tConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&tConfig);
    SButtonDebounce tDebounce = {
      .Pin = tPin,
      .Inverted = tPullUp,
      .History = static_cast<uint16_t>(tPullUp ? 0xFFFF : 0x0000),
      .DownTimeMs = 0,
      .NextHoldMs = 0
    };
    mDebounceData.push_back(tDebounce);
    mCallbacks.push_back({});
    xLOG("Button added pin %d %s", tPin, (tMessage ? tMessage : ""));
  }

  void Button_::Start() {
    if (mTaskHandle) return;
    if (mDebounceData.empty()) {
      xLOG("No pins added. Call AddPin() first.");
      return;
    }
    xTaskCreatePinnedToCore(&ButtonEventTask, "ButtonEventTask", kTaskStack, this, 10, &mTaskHandle, 1);
  }

  int Button_::GetLogicalIndex(uint8_t tPin) const {
    for (size_t i = 0; i < mDebounceData.size(); ++i) {
      if (mDebounceData[i].Pin == tPin) return static_cast<int>(i);
    }
    return -1;
  }

  void IRAM_ATTR Button_::ButtonEventTask(void *tParameter) {
    Button_ *tSelf = static_cast<Button_*>(tParameter);
    while (true) {
      uint32_t tNowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
      for (size_t tIndex = 0; tIndex < tSelf->mDebounceData.size(); tIndex++) {
        SButtonDebounce& tDebounce = tSelf->mDebounceData[tIndex];
        auto &tCallback = tSelf->mCallbacks[tIndex]; 
        tSelf->UpdateDebounce(&tDebounce);
        if (tSelf->ButtonDown(&tDebounce) && !tDebounce.DownTimeMs) {
          tDebounce.DownTimeMs = tNowMs;
          tDebounce.NextHoldMs = tNowMs + tCallback.LongThresholdMs;
          tCallback.BlockShort = false;
        } else
        if (tDebounce.DownTimeMs && !tCallback.BlockShort && tNowMs >= tDebounce.NextHoldMs) {
          if (tCallback.LongCallback) tSelf->SendEvent(tDebounce.Pin, EButtonEvent::Held);
          tCallback.BlockShort = true;
        } else 
        if (tDebounce.DownTimeMs && tSelf->ButtonUp(&tDebounce)) {
          uint32_t tHeldMs = tNowMs - tDebounce.DownTimeMs;
          if (!tCallback.BlockShort && tHeldMs < tCallback.LongThresholdMs && tCallback.ShortCallback) tSelf->SendEvent(tDebounce.Pin, EButtonEvent::Pressed);
          tDebounce.DownTimeMs = 0;
          tDebounce.NextHoldMs = 0;
          tCallback.BlockShort = false;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  void Button_::HandleEvents() {
    SButtonEvent tEvent;
    while (xQueueReceive(mEventQueue, &tEvent, 0) == pdTRUE) {
      int tIndex = GetLogicalIndex(tEvent.Pin);
      if (tIndex == -1) continue; 
      if (tEvent.Event == EButtonEvent::Pressed && mCallbacks[tIndex].ShortCallback) mCallbacks[tIndex].ShortCallback();
      if (tEvent.Event == EButtonEvent::Held && mCallbacks[tIndex].LongCallback) mCallbacks[tIndex].LongCallback();
    }
  }

  void Button_::AddShortPressCallback(EDevicePins tPin, FButtonCallback tCallback) {
    AddShortPressCallback(static_cast<uint8_t>(tPin), tCallback);
  }

  void Button_::AddLongPressCallback(EDevicePins tPin, FButtonCallback tCallback, uint32_t tLongMs) {
    AddLongPressCallback(static_cast<uint8_t>(tPin), tCallback, tLongMs);
  }

  void Button_::AddShortPressCallback(uint8_t tPin, FButtonCallback tCallback) {
    int tIndex = GetLogicalIndex(tPin);
    if (tIndex != -1) mCallbacks[tIndex].ShortCallback = tCallback;
    else xLOG("Callback registration failed → %d pin", tPin);
  }

  void Button_::AddLongPressCallback(uint8_t tPin, FButtonCallback tCallback, uint32_t tLongMs) {
    int tIndex = GetLogicalIndex(tPin);
    if (tIndex != -1) {
      mCallbacks[tIndex].LongCallback = tCallback;
      mCallbacks[tIndex].LongThresholdMs = tLongMs ? tLongMs : kDefaultLongMs;
    } else xLOG("Callback registration failed → %d pin", tPin);
  }
  
  bool IRAM_ATTR Button_::ButtonDown(const SButtonDebounce *tDebounce) const {
    return tDebounce->Inverted ? ((tDebounce->History & kDebounceMask) == 0b1111000000000000) : ((tDebounce->History & kDebounceMask) == 0b0000000000111111);
  }

  bool IRAM_ATTR Button_::ButtonUp(const SButtonDebounce *tDebounce) const {
    return tDebounce->Inverted ? ((tDebounce->History & kDebounceMask) == 0b0000000000111111) : ((tDebounce->History & kDebounceMask) == 0b1111000000000000);
  }

  void IRAM_ATTR Button_::UpdateDebounce(SButtonDebounce *tDebounce) const {
    tDebounce->History = (tDebounce->History << 1) | gpio_get_level((gpio_num_t)tDebounce->Pin);
  }

  void Button_::SendEvent(uint8_t tPin, EButtonEvent tEvent) {
    SButtonEvent tEv = { tPin, tEvent };
    xQueueSendToBack(mEventQueue, &tEv, 0);
  }

}