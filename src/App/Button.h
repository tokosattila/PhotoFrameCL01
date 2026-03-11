#ifndef BUTTON_H
#define BUTTON_H

#include <App/Global.h>

namespace App {

  using FButtonCallback = FDefaultCallback;

  enum class EButtonEvent : uint8_t {
    Pressed = 1,
    Held
  };

  struct SButtonDebounce {
    uint8_t Pin;
    bool Inverted;
    uint16_t History;
    uint32_t DownTimeMs;
    uint32_t NextHoldMs;
    SButtonDebounce() = default;
  };

  struct SButtonEvent {
    uint8_t Pin;
    EButtonEvent Event;
    SButtonEvent() = default;
  };

  class Button_ {
    DEFINE_TAG("BTN");
    static constexpr uint16_t kDebounceMask = 0b1111000000111111;
    static constexpr uint32_t kDefaultLongMs = 1e3;
    static constexpr uint32_t kRepeatIntervalMs = 200;
    static constexpr uint32_t kQueueLength = 32;
    static constexpr uint32_t kTaskStack = 3 * 1024;
  public:
    static Button_ &Instance();
    void AddPin(uint8_t tPin, const char *tMessage = "", bool tPullUp = true);
    void Start();
    void AddShortPressCallback(EDevicePins tPin, FButtonCallback tCallback);
    void AddLongPressCallback(EDevicePins tPin, FButtonCallback tCallback, uint32_t tLongMs = kDefaultLongMs);
    void AddShortPressCallback(uint8_t tPin, FButtonCallback tCallback);
    void AddLongPressCallback(uint8_t tPin, FButtonCallback tCallback, uint32_t tLongMs = kDefaultLongMs);
    void HandleEvents();
  private:
    Button_();
    Button_(const Button_&) = delete;
    Button_ &operator=(const Button_&) = delete;
    ~Button_();
    struct SCallback {
      FButtonCallback ShortCallback = nullptr;
      FButtonCallback LongCallback = nullptr;
      uint32_t LongThresholdMs = kDefaultLongMs;
      bool BlockShort = false;
    };
    std::vector<SButtonDebounce> mDebounceData;
    std::vector<SCallback> mCallbacks;
    uint64_t mPinMask = 0;
    QueueHandle_t mEventQueue = nullptr;
    TaskHandle_t mTaskHandle = nullptr;
    int GetLogicalIndex(uint8_t tPin) const;
    static void ButtonEventTask(void *tParameter);
    bool ButtonDown(const SButtonDebounce *tDebounce) const;
    bool ButtonUp(const SButtonDebounce *tDebounce) const;
    void UpdateDebounce(SButtonDebounce *tDebounce) const;
    void SendEvent(uint8_t tPin, EButtonEvent tEvent);
  };

}

#endif