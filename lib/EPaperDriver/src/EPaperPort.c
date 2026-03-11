#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "EPaperPort.h"

#define EPD_DC_PIN 8
#define EPD_CS_PIN 9
#define EPD_SCK_PIN 10
#define EPD_MOSI_PIN 11
#define EPD_RST_PIN 12
#define EPD_BUSY_PIN 13

#define EPD_RST_1 gpio_set_level(EPD_RST_PIN, 1)
#define EPD_RST_0 gpio_set_level(EPD_RST_PIN, 0)
#define EPD_CS_1 gpio_set_level(EPD_CS_PIN, 1)
#define EPD_CS_0 gpio_set_level(EPD_CS_PIN, 0)
#define EPD_DC_1 gpio_set_level(EPD_DC_PIN, 1)
#define EPD_DC_0 gpio_set_level(EPD_DC_PIN, 0)
#define EPD_READ_BUSY gpio_get_level(EPD_BUSY_PIN)

#define EPD_BUSY_TIMEOUT_MS 45000

static const char *gTag = "EPD";
static volatile bool gBusyTimeoutOccurred = false;

static spi_device_handle_t gSpi;
static bool gSpiInitialized = false;
static void IRAM_ATTR SpiSendByte(uint8_t tCommandByte);

static void EpaperGpioInit(void) {
  gpio_reset_pin(EPD_DC_PIN);
  gpio_reset_pin(EPD_CS_PIN);
  gpio_reset_pin(EPD_RST_PIN);
  gpio_reset_pin(EPD_BUSY_PIN);
  gpio_config_t tGpioConfiguration = {};
  tGpioConfiguration.intr_type = GPIO_INTR_DISABLE;
  tGpioConfiguration.mode = GPIO_MODE_OUTPUT;
  tGpioConfiguration.pin_bit_mask = ((uint64_t) 0x01 << EPD_RST_PIN) | ((uint64_t) 0x01 << EPD_DC_PIN) | ((uint64_t) 0x01 << EPD_CS_PIN);
  tGpioConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
  tGpioConfiguration.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&tGpioConfiguration));
  tGpioConfiguration.intr_type = GPIO_INTR_DISABLE;
  tGpioConfiguration.mode = GPIO_MODE_INPUT;
  tGpioConfiguration.pin_bit_mask = ((uint64_t) 0x01 << EPD_BUSY_PIN);
  tGpioConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
  tGpioConfiguration.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&tGpioConfiguration));
  EPD_RST_1;
}

static void EpaperReset(void) {
  EPD_RST_1;
  vTaskDelay(pdMS_TO_TICKS(50));
  EPD_RST_0;
  vTaskDelay(pdMS_TO_TICKS(20));
  EPD_RST_1;
  vTaskDelay(pdMS_TO_TICKS(50));
}

static void EpaperSpiInit(void) {
  if (gSpiInitialized && gSpi) return;
  gpio_reset_pin(EPD_SCK_PIN);
  gpio_reset_pin(EPD_MOSI_PIN);
  esp_err_t tResult;
  spi_bus_config_t tSpiBusConfiguration = {};
  tSpiBusConfiguration.miso_io_num = -1;
  tSpiBusConfiguration.mosi_io_num = EPD_MOSI_PIN;
  tSpiBusConfiguration.sclk_io_num = EPD_SCK_PIN;
  tSpiBusConfiguration.quadwp_io_num = -1;
  tSpiBusConfiguration.quadhd_io_num = -1;
  tSpiBusConfiguration.max_transfer_sz = EPAPER_WIDTH * EPAPER_HEIGHT;
  spi_device_interface_config_t tSpiDeviceConfiguration = {};
  tSpiDeviceConfiguration.spics_io_num = -1;
  tSpiDeviceConfiguration.clock_speed_hz = 10 * 1000 * 1000;
  tSpiDeviceConfiguration.mode = 0;
  tSpiDeviceConfiguration.queue_size = 7;
  tSpiDeviceConfiguration.flags = SPI_DEVICE_HALFDUPLEX;
  tResult = spi_bus_initialize(SPI3_HOST, &tSpiBusConfiguration, SPI_DMA_CH_AUTO);
  if (tResult != ESP_OK && tResult != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(tResult);
  if (gSpiInitialized && gSpi) return;
  tResult = spi_bus_add_device(SPI3_HOST, &tSpiDeviceConfiguration, &gSpi);
  ESP_ERROR_CHECK(tResult);
  gSpiInitialized = true;
}

static bool EpaperReadBusyHigh(const char *tStage) {
  TickType_t tStartTicks = xTaskGetTickCount();
  while (1) {
    if (EPD_READ_BUSY) return true;
    if (pdTICKS_TO_MS(xTaskGetTickCount() - tStartTicks) > EPD_BUSY_TIMEOUT_MS) {
      gBusyTimeoutOccurred = true;
      ESP_LOGE(gTag, "BUSY timeout at %s (pin %d level %d)", tStage, EPD_BUSY_PIN, (int)EPD_READ_BUSY);
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool EpaperPortGetAndClearBusyTimeout(void) {
  bool tOccurred = gBusyTimeoutOccurred;
  gBusyTimeoutOccurred = false;
  return tOccurred;
}

void EpaperPortClearBusyTimeout(void) {
  gBusyTimeoutOccurred = false;
}

static void IRAM_ATTR EpaperSendCommand(uint8_t tCommandRegister) {
  EPD_DC_0;
  EPD_CS_0;
  SpiSendByte(tCommandRegister);
  EPD_CS_1;
}

static void IRAM_ATTR EpaperSendData(uint8_t tDataByte) {
  EPD_DC_1;
  EPD_CS_0;
  SpiSendByte(tDataByte);
  EPD_CS_1;
}

static void EpaperSendBuffer(uint8_t *tDataBuffer, int tDataLength) {
  EPD_DC_1;
  EPD_CS_0;
  esp_err_t tResult;
  spi_transaction_t tTransaction;
  memset(&tTransaction, 0, sizeof(tTransaction));
  int tChunkCount = tDataLength / 5000;
  int tRemainingLength = tDataLength % 5000;
  uint8_t *tBufferPointer = tDataBuffer;
  while (tChunkCount) {
    tTransaction.length = 8 * 5000;
    tTransaction.tx_buffer = tBufferPointer;
    tResult = spi_device_polling_transmit(gSpi, &tTransaction);
    assert(tResult == ESP_OK);
    tChunkCount--;
    tBufferPointer += 5000;
  }
  tTransaction.length = 8 * tRemainingLength;
  tTransaction.tx_buffer = tBufferPointer;
  tResult = spi_device_polling_transmit(gSpi, &tTransaction);
  assert(tResult == ESP_OK);
  EPD_CS_1;
}

static void EpaperTurnOnDisplay(void) {
  EpaperSendCommand(0x04);
  if (!EpaperReadBusyHigh("turn-on 0x04")) {
    ESP_LOGW(gTag, "Retry power-on sequence after BUSY timeout");
    EpaperReset();
    EpaperSendCommand(0x04);
    if (!EpaperReadBusyHigh("turn-on 0x04 retry")) return;
  }
  EpaperSendCommand(0x06);
  EpaperSendData(0x6F);
  EpaperSendData(0x1F);
  EpaperSendData(0x17);
  EpaperSendData(0x49);
  EpaperSendCommand(0x12);
  EpaperSendData(0x00);
  if (!EpaperReadBusyHigh("turn-on 0x12")) return;
  EpaperSendCommand(0x02);
  EpaperSendData(0X00);
  (void)EpaperReadBusyHigh("turn-on 0x02");
}

void EpaperPortHwInit(void) {
  EpaperSpiInit();
  EpaperGpioInit();
  EpaperReset();
  if (!EpaperReadBusyHigh("init after reset")) return;
  vTaskDelay(pdMS_TO_TICKS(50));
  EpaperSendCommand(0xAA);
  EpaperSendData(0x49);
  EpaperSendData(0x55);
  EpaperSendData(0x20);
  EpaperSendData(0x08);
  EpaperSendData(0x09);
  EpaperSendData(0x18);
  EpaperSendCommand(0x01);
  EpaperSendData(0x3F);
  EpaperSendCommand(0x00);
  EpaperSendData(0x5F);
  EpaperSendData(0x69);
  EpaperSendCommand(0x03);
  EpaperSendData(0x00);
  EpaperSendData(0x54);
  EpaperSendData(0x00);
  EpaperSendData(0x44);
  EpaperSendCommand(0x05);
  EpaperSendData(0x40);
  EpaperSendData(0x1F);
  EpaperSendData(0x1F);
  EpaperSendData(0x2C);
  EpaperSendCommand(0x06);
  EpaperSendData(0x6F);
  EpaperSendData(0x1F);
  EpaperSendData(0x17);
  EpaperSendData(0x49);
  EpaperSendCommand(0x08);
  EpaperSendData(0x6F);
  EpaperSendData(0x1F);
  EpaperSendData(0x1F);
  EpaperSendData(0x22);
  EpaperSendCommand(0x30);
  EpaperSendData(0x03);
  EpaperSendCommand(0x50);
  EpaperSendData(0x3F);
  EpaperSendCommand(0x60);
  EpaperSendData(0x02);
  EpaperSendData(0x00);
  EpaperSendCommand(0x61);
  EpaperSendData(0x03);
  EpaperSendData(0x20);
  EpaperSendData(0x01);
  EpaperSendData(0xE0);
  EpaperSendCommand(0x84);
  EpaperSendData(0x01);
  EpaperSendCommand(0xE3);
  EpaperSendData(0x2F);
  EpaperSendCommand(0x04);
  (void)EpaperReadBusyHigh("init final 0x04");
}

static void IRAM_ATTR SpiSendByte(uint8_t tCommandByte) {
  esp_err_t tResult;
  spi_transaction_t tTransaction;
  memset(&tTransaction, 0, sizeof(tTransaction));
  tTransaction.length = 8;
  tTransaction.tx_buffer = &tCommandByte;
  tResult = spi_device_polling_transmit(gSpi, &tTransaction);
  assert(tResult == ESP_OK);
}

void EpaperPortHwClear(uint8_t *tImageBuffer, uint8_t tColorValue) {
  uint16_t tImageWidthBytes;
  uint16_t tImageHeight;
  tImageWidthBytes = (EPAPER_WIDTH % 2 == 0) ? (EPAPER_WIDTH / 2) : (EPAPER_WIDTH / 2 + 1);
  tImageHeight = EPAPER_HEIGHT;
  EpaperSendCommand(0x10);
  for (int tPixelIndex = 0; tPixelIndex < tImageHeight * tImageWidthBytes; tPixelIndex++) tImageBuffer[tPixelIndex] = (tColorValue << 4) | tColorValue;
  EpaperSendBuffer(tImageBuffer, tImageHeight * tImageWidthBytes);
  EpaperTurnOnDisplay();
}

void EpaperPortHwDisplay(uint8_t *tImageBuffer) {
  uint16_t tImageWidthBytes;
  uint16_t tImageHeight;
  tImageWidthBytes = (EPAPER_WIDTH % 2 == 0) ? (EPAPER_WIDTH / 2) : (EPAPER_WIDTH / 2 + 1);
  tImageHeight = EPAPER_HEIGHT;
  EpaperSendCommand(0x10);
  EpaperSendBuffer(tImageBuffer, tImageHeight * tImageWidthBytes);
  EpaperTurnOnDisplay();
}

void EpaperPortHwSleep(void) {
  EpaperSendCommand(0x07);
  EpaperSendData(0xA5);
  vTaskDelay(pdMS_TO_TICKS(200));
}
