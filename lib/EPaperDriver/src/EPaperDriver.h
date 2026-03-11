#ifndef EPAPERDRIVER_H
#define EPAPERDRIVER_H

#include <cstring>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include "EPaperPort.h"

constexpr uint8_t EpaperColorBlack = 0x0;
constexpr uint8_t EpaperColorWhite = 0x1;
constexpr uint8_t EpaperColorYellow = 0x2;
constexpr uint8_t EpaperColorRed = 0x3;
constexpr uint8_t EpaperColorBlue = 0x5;
constexpr uint8_t EpaperColorGreen = 0x6;

constexpr uint16_t EpaperPanelWidth = 800;
constexpr uint16_t EpaperPanelHeight = 480;

constexpr uint16_t Rotate0 = 0;
constexpr uint16_t Rotate90 = 90;
constexpr uint16_t Rotate180 = 180;
constexpr uint16_t Rotate270 = 270;

constexpr uint8_t MonoWhite = 0xFF;
constexpr uint8_t MonoBlack = 0x00;
constexpr uint8_t MonoRed = MonoBlack;

constexpr uint8_t ImageBackground = MonoWhite;
constexpr uint8_t FontForeground = MonoBlack;
constexpr uint8_t FontBackground = MonoWhite;

struct SUnicodeInterval {
  uint16_t First;
  uint16_t Last;
  uint16_t Offset;
};

struct SGfxGlyph {
  uint16_t Width;
  uint16_t Height;
  uint16_t XAdvance;
  int16_t XOffset;
  int16_t YOffset;
  uint16_t DataLen;
  uint32_t DataOffset;
};

struct SGfxFont {
  uint8_t *Bitmaps;
  SGfxGlyph *Glyphs;
  SUnicodeInterval *Intervals;
  uint16_t IntervalCount;
  uint8_t Compressed;
  uint16_t YAdvance;
  int16_t Ascender;
  int16_t Descender;
};

using UnicodeInterval = SUnicodeInterval;
using GFXglyph = SGfxGlyph;
using GFXfont = SGfxFont;

enum MirrorImage : uint8_t {
  MirrorNone = 0x00,
  MirrorHorizontal = 0x01,
  MirrorVertical = 0x02,
  MirrorOrigin = 0x03
};

enum DotPixel : uint8_t {
  DotPixel1x1 = 1,
  DotPixel2x2,
  DotPixel3x3,
  DotPixel4x4,
  DotPixel5x5,
  DotPixel6x6,
  DotPixel7x7,
  DotPixel8x8
};

enum DotStyle : uint8_t {
  DotFillAround = 1,
  DotFillRightUp
};

enum LineStyle : uint8_t {
  LineStyleSolid = 0,
  LineStyleDotted
};

enum DrawFill : uint8_t {
  DrawFillEmpty = 0,
  DrawFillFull
};

typedef struct {
  uint8_t *Image;
  uint16_t Width;
  uint16_t Height;
  uint16_t WidthMemory;
  uint16_t HeightMemory;
  uint16_t Color;
  uint16_t Rotate;
  uint16_t Mirror;
  uint16_t WidthByte;
  uint16_t HeightByte;
  uint16_t Scale;
} PAINT;

#pragma pack(push, 1)

typedef struct BMP_FILE_HEADER {
  uint16_t bType;
  uint32_t bSize;
  uint16_t bReserved1;
  uint16_t bReserved2;
  uint32_t bOffset;
} BMPFILEHEADER;

typedef struct BMP_INFO {
  uint32_t biInfoSize;
  uint32_t biWidth;
  uint32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t bimpImageSize;
  uint32_t biXPelsPerMeter;
  uint32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} BMPINFOHEADER;

#pragma pack(pop)

class EPaperDriver_ {
  public:
    static void Init();
    static void Sleep();
    static void Clear(uint8_t *tImage, uint8_t tColor);
    static void Display(uint8_t *tImage);
    static void NewImage(uint8_t *tImage, uint16_t tWidth, uint16_t tHeight, uint16_t tRotate, uint16_t tColor);
    static void SelectImage(uint8_t *tImage);
    static void SetRotate(uint16_t tRotate);
    static void SetMirroring(uint8_t tMirror);
    static void SetScale(uint8_t tScale);
    static void IRAM_ATTR SetPixel(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor);
    static void ClearImage(uint16_t tColor);
    static void ClearWindows(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor);
    static void DrawPoint(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor, DotPixel tDotPixel, DotStyle tDotStyle);
    static void DrawLine(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, LineStyle tLineStyle);
    static void DrawRectangle(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill);
    static void DrawCircle(uint16_t tXcenter, uint16_t tYcenter, uint16_t tRadius, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill);
    static void DrawBitmap(const unsigned char *tImageBuffer);
    static uint8_t ReadBmpRgb6Color(const char *tPath, uint16_t tXstart, uint16_t tYstart);
  private:
    class LockGuard {
      public:
        LockGuard() { EPaperDriver_::Lock(); }
        ~LockGuard() { EPaperDriver_::Unlock(); }
    };
    static SemaphoreHandle_t sMutex;
    static portMUX_TYPE sMutexCreateMux;
    static void EnsureMutex();
    static void Lock();
    static void Unlock();
    static void PortInit();
    static void PortSleep();
    static void PortClear(uint8_t *tImage, uint8_t tColor);
    static void PortDisplay(uint8_t *tImage);
    static void PaintNewImage(uint8_t *tImage, uint16_t tWidth, uint16_t tHeight, uint16_t tRotate, uint16_t tColor);
    static void PaintSelectImage(uint8_t *tImage);
    static void PaintSetRotate(uint16_t tRotate);
    static void PaintSetMirroring(uint8_t tMirror);
    static void PaintSetScale(uint8_t tScale);
    static void IRAM_ATTR PaintSetPixel(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor);
    static void PaintClear(uint16_t tColor);
    static void PaintClearWindows(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor);
    static void PaintDrawPoint(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor, DotPixel tDotPixel, DotStyle tDotStyle);
    static void PaintDrawLine(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, LineStyle tLineStyle);
    static void PaintDrawRectangle(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill);
    static void PaintDrawCircle(uint16_t tXcenter, uint16_t tYcenter, uint16_t tRadius, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill);
    static void PaintDrawBitmap(const unsigned char *tImageBuffer);
    static uint8_t MapRgbTo6Color(uint8_t tBlue, uint8_t tGreen, uint8_t tRed);
    static bool ReadFully(File &tFile, uint8_t *tBuffer, size_t tSize);
    static uint8_t GuiReadBmpRgb6Color(const char *tPath, uint16_t tXstart, uint16_t tYstart);
};

#endif

