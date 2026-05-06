#ifndef MOCK_WIFI_CLIENT_H
#define MOCK_WIFI_CLIENT_H

#include <cstdio>
#include <cstdarg>
#include <cstring>

class __FlashStringHelper;
#define F(str) (reinterpret_cast<const __FlashStringHelper*>(str))

class MockWiFiClient {
public:
  static constexpr size_t kBufferSize = 4096;
  MockWiFiClient() : mPos(0) {
    memset(mBuffer, 0, kBufferSize);
  }

  void print(const char *str) {
    if (str && mPos < kBufferSize - 1) {
      size_t len = strlen(str);
      if (mPos + len >= kBufferSize) len = kBufferSize - mPos - 1;
      memcpy(mBuffer + mPos, str, len);
      mPos += len;
      mBuffer[mPos] = '\0';
    }
  }

  void print(const __FlashStringHelper *str) {
    print(reinterpret_cast<const char*>(str));
  }

  void println(const char *str = "") {
    print(str);
    print("\r\n");
  }

  size_t write(const char *str) {
    print(str);
    return strlen(str);
  }

  size_t write(uint8_t c) {
    if (mPos < kBufferSize - 1) {
      mBuffer[mPos++] = static_cast<char>(c);
      mBuffer[mPos] = '\0';
    }
    return 1;
  }

  void printf(const char *fmt, ...) {
    if (mPos >= kBufferSize - 1) return;
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(mBuffer + mPos, kBufferSize - mPos, fmt, args);
    va_end(args);
    if (written > 0) {
      mPos += written;
      if (mPos >= kBufferSize) mPos = kBufferSize - 1;
    }
  }

  const char *getOutput() const { return mBuffer; }

  size_t getOutputLength() const { return mPos; }

  void clear() {
    mPos = 0;
    memset(mBuffer, 0, kBufferSize);
  }

  bool contains(const char *substr) const {
    return strstr(mBuffer, substr) != nullptr;
  }

private:
  char mBuffer[kBufferSize];
  size_t mPos;
};

#endif
