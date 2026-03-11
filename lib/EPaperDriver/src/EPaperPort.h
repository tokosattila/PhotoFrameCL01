#ifndef EPAPER_PORT_H
#define EPAPER_PORT_H

#include <stdint.h>

#define EPAPER_WIDTH 800
#define EPAPER_HEIGHT 480

#define EPD_7IN3E_BLACK 0x0
#define EPD_7IN3E_WHITE 0x1
#define EPD_7IN3E_YELLOW 0x2
#define EPD_7IN3E_RED 0x3
#define EPD_7IN3E_BLUE 0x5
#define EPD_7IN3E_GREEN 0x6

#ifdef __cplusplus
extern "C" {
#endif

void EpaperPortHwInit(void);
void EpaperPortHwClear(uint8_t *tImageBuffer, uint8_t tColorValue);
void EpaperPortHwDisplay(uint8_t *tImageBuffer);
void EpaperPortHwSleep(void);
bool EpaperPortGetAndClearBusyTimeout(void);
void EpaperPortClearBusyTimeout(void);

#ifdef __cplusplus
}
#endif

#endif
