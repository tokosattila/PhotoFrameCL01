#ifndef SOUND_H
#define SOUND_H

#include <App/Global.h>

namespace App {

  class Sound_ {
    DEFINE_TAG("SND");
    friend class AutoGuard<Sound_>;
    public:
      using Guard = AutoGuard<Sound_>;
      static Sound_ &Instance();
      bool Init(bool tVerbose = false);
      void End();
      void SetVolume(uint8_t tVolumePct);
      bool IsAvailable() const { return mAvailable; }
      template <size_t N>
      bool Play(const SSound (&tSteps)[N]) { return Play(tSteps, N); }
    private:
      Sound_();
      Sound_(const Sound_&) = delete;
      Sound_ &operator=(const Sound_&) = delete;
      ~Sound_();
      mutable SemaphoreHandle_t mMutex = nullptr;
      static void Lock();
      static void Unlock();
      static constexpr uint32_t kSampleRate = SOUND_SAMPLE_RATE;
      static constexpr uint8_t kAddress = SOUND_CODEC_ADDRESS;
      static constexpr uint8_t kI2sMclkPin = SOUND_I2S_MCLK_PIN;
      static constexpr uint8_t kI2sWsPin = SOUND_I2S_WS_PIN;
      static constexpr uint8_t kI2sBclkPin = SOUND_I2S_BCLK_PIN;
      static constexpr uint8_t kI2sDoutPin = SOUND_I2S_DOUT_PIN;
      static constexpr uint8_t kCodecI2CSdaPin = SOUND_CODEC_I2C_SDA_PIN;
      static constexpr uint8_t kCodecI2CSclPin = SOUND_CODEC_I2C_SCL_PIN;
      static constexpr uint8_t kCodecPaPin = SOUND_CODEC_PA_PIN;
      static constexpr uint8_t kCodecBoardInitPin = SOUND_CODEC_BOARD_INIT_PIN;
      static constexpr bool kCodecPaActiveHigh = SOUND_CODEC_PA_ACTIVE_HIGH;
      static constexpr bool kCodecClockFromBclk = SOUND_CODEC_CLOCK_FROM_BCLK;
      static constexpr uint8_t kDefaultMasterVolumePct = SOUND_MASTER_VOLUME_PCT;
      static constexpr i2s_port_t kI2sPort = SOUND_I2S_PORT;
      static constexpr uint8_t kI2CRetryCount = 3;
      static constexpr uint32_t kCodecPowerStabilizeMs = 20;
      static constexpr uint8_t kRegReset = 0x00;
      static constexpr uint8_t kRegClkManager01 = 0x01;
      static constexpr uint8_t kRegClkManager02 = 0x02;
      static constexpr uint8_t kRegClkManager03 = 0x03;
      static constexpr uint8_t kRegClkManager04 = 0x04;
      static constexpr uint8_t kRegClkManager05 = 0x05;
      static constexpr uint8_t kRegClkManager06 = 0x06;
      static constexpr uint8_t kRegClkManager07 = 0x07;
      static constexpr uint8_t kRegClkManager08 = 0x08;
      static constexpr uint8_t kRegSdIn09 = 0x09;
      static constexpr uint8_t kRegSdOut0A = 0x0A;
      static constexpr uint8_t kRegSystem0B = 0x0B;
      static constexpr uint8_t kRegSystem0C = 0x0C;
      static constexpr uint8_t kRegSystem0D = 0x0D;
      static constexpr uint8_t kRegSystem0E = 0x0E;
      static constexpr uint8_t kRegSystem10 = 0x10;
      static constexpr uint8_t kRegSystem11 = 0x11;
      static constexpr uint8_t kRegSystem12 = 0x12;
      static constexpr uint8_t kRegSystem13 = 0x13;
      static constexpr uint8_t kRegSystem14 = 0x14;
      static constexpr uint8_t kRegAdc15 = 0x15;
      static constexpr uint8_t kRegAdc16 = 0x16;
      static constexpr uint8_t kRegAdc17 = 0x17;
      static constexpr uint8_t kRegAdc1B = 0x1B;
      static constexpr uint8_t kRegAdc1C = 0x1C;
      static constexpr uint8_t kRegDac31 = 0x31;
      static constexpr uint8_t kRegDac32 = 0x32;
      static constexpr uint8_t kRegDac37 = 0x37;
      static constexpr uint8_t kRegGpio44 = 0x44;
      static constexpr uint8_t kRegGp45 = 0x45;
      static constexpr uint8_t kRegPageFa = 0xFA;
      uint8_t mCodecAddress = kAddress;
      bool mRuntimeCodecPaActiveHigh = kCodecPaActiveHigh;
      bool mRuntimeCodecClockFromBclk = kCodecClockFromBclk;
      uint32_t mRuntimeSampleRate = kSampleRate;
      uint8_t mMasterVolumePct = kDefaultMasterVolumePct;
      bool mAvailable = false;
      bool mI2sStarted = false;
      bool TryI2C();
      bool ReadCodecRegister(uint8_t tRegister, uint8_t &tValue);
      bool WriteCodecRegister(uint8_t tRegister, uint8_t tValue);
      bool ConfigureCodec();
      bool Play(const SSound *tSteps, size_t tCount);
      bool ConfigureI2s();
      bool PlaySequence(const SSound *tSteps, size_t tCount);
      bool PlayTone(uint16_t tFrequencyHz, uint16_t tDurationMs, uint8_t tAmplitudePct);
  };

}

#endif