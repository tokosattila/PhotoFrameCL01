#include <App/Sound.h>
#include <driver/i2c.h>

namespace App {

  Sound_ &Sound_::Instance() {
    static Sound_ tInstance;
    return tInstance;
  }

  Sound_::Sound_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Sound_::~Sound_() {
    End();
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Sound_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Sound_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void Sound_::SetVolume(uint8_t tVolumePct) {
    Guard tLock;
    if (tVolumePct > 100U) tVolumePct = 100U;
    mMasterVolumePct = tVolumePct;
  }

  bool Sound_::Init(bool tVerbose) {
    Guard tLock;
    if (mAvailable) return true;
    (void)tVerbose;
    mRuntimeCodecClockFromBclk = kCodecClockFromBclk;
    mRuntimeCodecPaActiveHigh = kCodecPaActiveHigh;
    mRuntimeSampleRate = kSampleRate;
    pinMode(kCodecBoardInitPin, OUTPUT);
    digitalWrite(kCodecBoardInitPin, LOW);
    pinMode(kCodecPaPin, OUTPUT);
    digitalWrite(kCodecPaPin, mRuntimeCodecPaActiveHigh ? LOW : HIGH);
    {
      I2CBusGuard tBusLock;
      Wire.begin(kCodecI2CSdaPin, kCodecI2CSclPin);
      Wire.setClock(100000UL);
    }
    const bool tI2sReady = ConfigureI2s();
    if (tI2sReady) vTaskDelay(300 / portTICK_PERIOD_MS);
    const bool tCodecFound = TryI2C();
    const bool tCodecConfigured = tCodecFound && ConfigureCodec();
    mAvailable = tI2sReady && tCodecFound && tCodecConfigured;
    if (mAvailable) {
      vTaskDelay(kCodecPowerStabilizeMs / portTICK_PERIOD_MS);
      digitalWrite(kCodecPaPin, mRuntimeCodecPaActiveHigh ? HIGH : LOW);
    }
    return mAvailable;
  }

  void Sound_::End() {
    Guard tLock;
    if (mI2sStarted) {
      i2s_zero_dma_buffer(kI2sPort);
      i2s_driver_uninstall(kI2sPort);
      mI2sStarted = false;
    }
    digitalWrite(kCodecPaPin, mRuntimeCodecPaActiveHigh ? LOW : HIGH);
    {
      I2CBusGuard tBusLock;
      Wire.end();
    }
    mAvailable = false;
  }

  bool Sound_::TryI2C() {
    I2CBusGuard tBusLock;
    Wire.beginTransmission(mCodecAddress);
    const uint8_t tStatus = Wire.endTransmission();
    if (tStatus == 0) return true;
    return false;
  }

  bool Sound_::WriteCodecRegister(uint8_t tRegister, uint8_t tValue) {
    constexpr TickType_t kCodecI2CTimeoutTicks = pdMS_TO_TICKS(25);
    const uint8_t tPayload[2] = {tRegister, tValue};
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      const esp_err_t tErr = i2c_master_write_to_device(I2C_NUM_0, mCodecAddress, tPayload, sizeof(tPayload), kCodecI2CTimeoutTicks);
      if (tErr == ESP_OK) return true;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool Sound_::ReadCodecRegister(uint8_t tRegister, uint8_t &tValue) {
    constexpr TickType_t kCodecI2CTimeoutTicks = pdMS_TO_TICKS(25);
    for (uint8_t tTry = 0; tTry < kI2CRetryCount; tTry++) {
      uint8_t tBuffer = 0x00;
      const esp_err_t tErr = i2c_master_write_read_device(I2C_NUM_0, mCodecAddress, &tRegister, 1, &tBuffer, 1, kCodecI2CTimeoutTicks);
      if (tErr == ESP_OK) {
        tValue = tBuffer;
        return true;
      }
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    }
    return false;
  }

  bool Sound_::ConfigureCodec() {
    const uint8_t tClockSourceMask = mRuntimeCodecClockFromBclk ? 0x80 : 0x00;
    WriteCodecRegister(kRegReset, 0x9F);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    WriteCodecRegister(kRegReset, 0x9F);
    vTaskDelay(30 / portTICK_PERIOD_MS);
    const auto tWriteStrict = [&](uint8_t tReg, uint8_t tValue) -> bool {
      if (!WriteCodecRegister(tReg, tValue)) return false;
      vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
      return true;
    };
    const auto tReadStrict = [&](uint8_t tReg, uint8_t &tValue) -> bool {
      if (!ReadCodecRegister(tReg, tValue)) return false;
      return true;
    };
    if (!WriteCodecRegister(kRegPageFa, 0x00)) return false;
    vTaskDelay(DELAY_ULTRA_SHORT_MS / portTICK_PERIOD_MS);
    if (!tWriteStrict(kRegClkManager01, 0x30)) return false;
    if (!tWriteStrict(kRegClkManager02, 0x00)) return false;
    if (!tWriteStrict(kRegClkManager03, 0x10)) return false;
    if (!tWriteStrict(kRegAdc16, 0x24)) return false;
    if (!tWriteStrict(kRegClkManager04, 0x10)) return false;
    if (!tWriteStrict(kRegClkManager05, 0x00)) return false;
    if (!tWriteStrict(kRegSystem0B, 0x00)) return false;
    if (!tWriteStrict(kRegSystem0C, 0x00)) return false;
    if (!tWriteStrict(kRegSystem10, 0x1F)) return false;
    if (!tWriteStrict(kRegSystem11, 0x7F)) return false;
    if (!tWriteStrict(kRegReset, 0x9F)) return false;
    if (!tWriteStrict(kRegReset, 0x80)) return false;
    if (!tWriteStrict(kRegClkManager01, 0x3F)) return false;
    uint8_t tReg01 = 0x00;
    if (!tReadStrict(kRegClkManager01, tReg01)) return false;
    tReg01 = static_cast<uint8_t>((tReg01 & 0x7FU) | tClockSourceMask);
    tReg01 = static_cast<uint8_t>(tReg01 & ~0x40U);
    if (!tWriteStrict(kRegClkManager01, tReg01)) return false;
    constexpr uint8_t kPreDiv = 0x01;
    constexpr uint8_t kPreMulti = 0x01;
    constexpr uint8_t kAdcDiv = 0x01;
    constexpr uint8_t kDacDiv = 0x01;
    constexpr uint8_t kFsMode = 0x00;
    constexpr uint8_t kLrckH = 0x00;
    constexpr uint8_t kLrckL = 0xFF;
    constexpr uint8_t kBclkDiv = 0x04;
    constexpr uint8_t kAdcOsr = 0x10;
    constexpr uint8_t kDacOsr = 0x10;
    uint8_t tReg02 = 0x00;
    if (!tReadStrict(kRegClkManager02, tReg02)) return false;
    uint8_t tPreMulti = 0;
    if (kPreMulti == 2U) tPreMulti = 1U;
    else if (kPreMulti == 4U) tPreMulti = 2U;
    else if (kPreMulti == 8U) tPreMulti = 3U;
    if (mRuntimeCodecClockFromBclk) tPreMulti = 3U;
    tReg02 = static_cast<uint8_t>((tReg02 & 0x07U) | ((kPreDiv - 1U) << 5) | (tPreMulti << 3));
    if (!tWriteStrict(kRegClkManager02, tReg02)) return false;
    uint8_t tReg05 = 0x00;
    if (!tReadStrict(kRegClkManager05, tReg05)) return false;
    tReg05 = static_cast<uint8_t>((tReg05 & 0x00U) | ((kAdcDiv - 1U) << 4) | (kDacDiv - 1U));
    if (!tWriteStrict(kRegClkManager05, tReg05)) return false;
    uint8_t tReg03 = 0x00;
    if (!tReadStrict(kRegClkManager03, tReg03)) return false;
    tReg03 = static_cast<uint8_t>((tReg03 & 0x80U) | (kFsMode << 6) | kAdcOsr);
    if (!tWriteStrict(kRegClkManager03, tReg03)) return false;
    uint8_t tReg04 = 0x00;
    if (!tReadStrict(kRegClkManager04, tReg04)) return false;
    tReg04 = static_cast<uint8_t>((tReg04 & 0x80U) | kDacOsr);
    if (!tWriteStrict(kRegClkManager04, tReg04)) return false;
    uint8_t tReg07 = 0x00;
    if (!tReadStrict(kRegClkManager07, tReg07)) return false;
    tReg07 = static_cast<uint8_t>((tReg07 & 0xC0U) | kLrckH);
    if (!tWriteStrict(kRegClkManager07, tReg07)) return false;
    if (!tWriteStrict(kRegClkManager08, kLrckL)) return false;
    uint8_t tReg06 = 0x00;
    if (!tReadStrict(kRegClkManager06, tReg06)) return false;
    const uint8_t tBclkField = static_cast<uint8_t>((kBclkDiv < 19U) ? (kBclkDiv - 1U) : kBclkDiv);
    tReg06 = static_cast<uint8_t>((tReg06 & 0xE0U) | tBclkField);
    tReg06 = static_cast<uint8_t>(tReg06 & ~0x20U);
    if (!tWriteStrict(kRegClkManager06, tReg06)) return false;
    if (!tWriteStrict(kRegSystem13, 0x10)) return false;
    if (!tWriteStrict(kRegAdc1B, 0x0A)) return false;
    if (!tWriteStrict(kRegAdc1C, 0x6A)) return false;
    uint8_t tDacIface = 0;
    uint8_t tAdcIface = 0;
    if (!tReadStrict(kRegSdIn09, tDacIface)) return false;
    if (!tReadStrict(kRegSdOut0A, tAdcIface)) return false;
    tDacIface = static_cast<uint8_t>((tDacIface & 0xFCU) | 0x0CU);
    tAdcIface = static_cast<uint8_t>((tAdcIface & 0xFCU) | 0x0CU);
    tDacIface = static_cast<uint8_t>(tDacIface & static_cast<uint8_t>(~0x40U));
    tAdcIface = static_cast<uint8_t>(tAdcIface | 0x40U);
    if (!tWriteStrict(kRegSdIn09, tDacIface)) return false;
    if (!tWriteStrict(kRegSdOut0A, tAdcIface)) return false;
    if (!tWriteStrict(kRegAdc17, 0xBF)) return false;
    if (!tWriteStrict(kRegSystem0E, 0x02)) return false;
    if (!tWriteStrict(kRegSystem12, 0x00)) return false;
    if (!tWriteStrict(kRegSystem14, 0x1A)) return false;
    if (!tWriteStrict(kRegSystem0D, 0x01)) return false;
    if (!tWriteStrict(kRegAdc15, 0x00)) return false;
    if (!tWriteStrict(kRegDac37, 0x08)) return false;
    if (!tWriteStrict(kRegDac31, 0x00)) return false;
    if (!tWriteStrict(kRegGpio44, 0x58)) return false;
    if (!tWriteStrict(kRegGp45, 0x00)) return false;
    int tVolumePct = static_cast<int>(mMasterVolumePct);
    if (tVolumePct < 0) tVolumePct = 0;
    if (tVolumePct > 100) tVolumePct = 100;
    int tVolumeReg = 0;
    if (tVolumePct > 0) tVolumeReg = static_cast<int>(255.0f * log10f(9.0f * static_cast<float>(tVolumePct) / 100.0f + 1.0f));
    if (tVolumeReg > 255) tVolumeReg = 255;
    if (!tWriteStrict(kRegDac32, static_cast<uint8_t>(tVolumeReg))) return false;
    if (!tWriteStrict(kRegSdIn09, tDacIface)) return false;
    if (!tWriteStrict(kRegSdOut0A, tAdcIface)) return false;
    if (!tWriteStrict(kRegDac31, 0x00)) return false;
    if (!tWriteStrict(kRegGpio44, 0x58)) return false;
    if (!tWriteStrict(kRegSystem0D, 0x01)) return false;
    if (!WriteCodecRegister(kRegPageFa, 0x00)) return false;
    return true;
  }

  bool Sound_::ConfigureI2s() {
    if (mI2sStarted) return true;
    i2s_config_t tConfig {};
    tConfig.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    tConfig.sample_rate = static_cast<int>(mRuntimeSampleRate);
    tConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    tConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    tConfig.communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
    tConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    tConfig.dma_buf_count = 4;
    tConfig.dma_buf_len = 256;
    tConfig.use_apll = true;
    tConfig.tx_desc_auto_clear = true;
    tConfig.fixed_mclk = static_cast<int>(mRuntimeSampleRate * 256U);
    if (i2s_driver_install(kI2sPort, &tConfig, 0, nullptr) != ESP_OK) {
      xLOG("I2S driver install failed");
      return false;
    }
    i2s_pin_config_t tPins;
    memset(&tPins, 0, sizeof(tPins));
    tPins.mck_io_num = static_cast<int>(kI2sMclkPin);
    tPins.bck_io_num = static_cast<int>(kI2sBclkPin);
    tPins.ws_io_num = static_cast<int>(kI2sWsPin);
    tPins.data_out_num = static_cast<int>(kI2sDoutPin);
    tPins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(kI2sPort, &tPins) != ESP_OK) {
      xLOG("I2S set pin failed");
      i2s_driver_uninstall(kI2sPort);
      return false;
    }
    if (i2s_set_clk(kI2sPort, mRuntimeSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
      xLOG("I2S set clock failed");
      i2s_driver_uninstall(kI2sPort);
      return false;
    }
    if (i2s_start(kI2sPort) != ESP_OK) {
      xLOG("I2S start failed");
      i2s_driver_uninstall(kI2sPort);
      return false;
    }
    i2s_zero_dma_buffer(kI2sPort);
    mI2sStarted = true;
    return true;
  }

  bool Sound_::PlayTone(uint16_t tFrequencyHz, uint16_t tDurationMs, uint8_t tAmplitudePct) {
    if (!mAvailable || tFrequencyHz == 0 || tDurationMs == 0) return false;
    if (tAmplitudePct > 100) tAmplitudePct = 100;
    constexpr size_t kFramesPerChunk = 128;
    int16_t tBuffer[kFramesPerChunk * 2] = {0};
    const size_t tTotalFrames = static_cast<size_t>((mRuntimeSampleRate * tDurationMs) / 1000UL);
    const float tAmplitude = static_cast<float>(tAmplitudePct) / 100.0f;
    const float tStep = (2.0f * PI * static_cast<float>(tFrequencyHz)) / static_cast<float>(mRuntimeSampleRate);
    float tPhase = 0.0f;
    size_t tWrittenFrames = 0;
    while (tWrittenFrames < tTotalFrames) {
      const size_t tFramesThisChunk = std::min(kFramesPerChunk, tTotalFrames - tWrittenFrames);
      for (size_t tFrame = 0; tFrame < tFramesThisChunk; tFrame++) {
        const float tWave = sinf(tPhase) * tAmplitude;
        const int16_t tSample = static_cast<int16_t>(tWave * 32767.0f);
        tBuffer[tFrame * 2] = tSample;
        tBuffer[tFrame * 2 + 1] = tSample;
        tPhase += tStep;
        if (tPhase >= (2.0f * PI)) tPhase -= (2.0f * PI);
      }
      size_t tBytesWritten = 0;
      const size_t tBytes = tFramesThisChunk * sizeof(int16_t) * 2;
      const esp_err_t tWriteError = i2s_write(kI2sPort, tBuffer, tBytes, &tBytesWritten, portMAX_DELAY);
      if (tWriteError != ESP_OK) {
        xLOG("Sound i2s_write failed, err:0x%08X, freq:%u, duration:%u", static_cast<unsigned>(tWriteError), static_cast<unsigned>(tFrequencyHz), static_cast<unsigned>(tDurationMs));
        return false;
      }
      if (tBytesWritten != tBytes) return false;
      tWrittenFrames += tFramesThisChunk;
    }
    return true;
  }

  bool Sound_::PlaySequence(const SSound *tSteps, size_t tCount) {
    if (!tSteps || tCount == 0) return false;
    if (!Init()) {
      xLOG("Sound playback failed, sound init unavailable");
      return false;
    }
    if (i2s_set_clk(kI2sPort, mRuntimeSampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
      xLOG("Sound playback set clock failed, sr:%lu", static_cast<unsigned long>(mRuntimeSampleRate));
      return false;
    }
    digitalWrite(kCodecPaPin, mRuntimeCodecPaActiveHigh ? HIGH : LOW);
    vTaskDelay(60 / portTICK_PERIOD_MS);
    i2s_zero_dma_buffer(kI2sPort);
    for (size_t tIndex = 0; tIndex < tCount; tIndex++) {
      const SSound &tStep = tSteps[tIndex];
      if (tStep.FrequencyHz > 0 && tStep.DurationMs > 0) {
        uint16_t tAmplitudePct = static_cast<uint16_t>(tStep.AmplitudePct);
        tAmplitudePct = static_cast<uint16_t>((tAmplitudePct * mMasterVolumePct) / 100U);
        if (tAmplitudePct > 100U) tAmplitudePct = 100U;
        if (!PlayTone(tStep.FrequencyHz, tStep.DurationMs, tAmplitudePct)) {
          return false;
        }
      }
      if (tStep.PauseMs > 0) vTaskDelay(tStep.PauseMs / portTICK_PERIOD_MS);
    }
    digitalWrite(kCodecPaPin, mRuntimeCodecPaActiveHigh ? HIGH : LOW);
    return true;
  }

  bool Sound_::Play(const SSound *tSteps, size_t tCount) {
    Guard tLock;
    const bool tSoundEnabled = CFG.Get<SDeviceConfig>().SoundEnabled;
    if (!tSoundEnabled) return false;
    return PlaySequence(tSteps, tCount);
  }

}