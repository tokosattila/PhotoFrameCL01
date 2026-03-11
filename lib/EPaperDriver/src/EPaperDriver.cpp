#include <EPaperDriver.h>

static PAINT Paint;
static bool gPortInitialized = false;

SemaphoreHandle_t EPaperDriver_::sMutex = nullptr;
portMUX_TYPE EPaperDriver_::sMutexCreateMux = portMUX_INITIALIZER_UNLOCKED;

void EPaperDriver_::EnsureMutex() {
  if (sMutex) return;
  taskENTER_CRITICAL(&sMutexCreateMux);
  if (!sMutex) sMutex = xSemaphoreCreateRecursiveMutex();
  taskEXIT_CRITICAL(&sMutexCreateMux);
}

void EPaperDriver_::Lock() {
  EnsureMutex();
  if (sMutex) xSemaphoreTakeRecursive(sMutex, portMAX_DELAY);
}

void EPaperDriver_::Unlock() {
  if (sMutex) xSemaphoreGiveRecursive(sMutex);
}

void EPaperDriver_::PortInit() {
  if (gPortInitialized) return;
  EpaperPortClearBusyTimeout();
  EpaperPortHwInit();
  gPortInitialized = !EpaperPortGetAndClearBusyTimeout();
}

void EPaperDriver_::PortSleep() {
  EpaperPortHwSleep();
}

void EPaperDriver_::PortClear(uint8_t *tImage, uint8_t tColor) {
  EpaperPortHwClear(tImage, tColor);
}

void EPaperDriver_::PortDisplay(uint8_t *tImage) {
  EpaperPortHwDisplay(tImage);
}

void EPaperDriver_::PaintNewImage(uint8_t *tImage, uint16_t tWidth, uint16_t tHeight, uint16_t tRotate, uint16_t tColor) {
  Paint.Image = tImage;
  Paint.WidthMemory = tWidth;
  Paint.HeightMemory = tHeight;
  Paint.Color = tColor;
  Paint.Scale = 2;
  Paint.WidthByte = (tWidth % 8 == 0) ? (tWidth / 8) : (tWidth / 8 + 1);
  Paint.HeightByte = tHeight;
  Paint.Rotate = tRotate;
  Paint.Mirror = MirrorNone;
  if (tRotate == Rotate0 || tRotate == Rotate180) {
    Paint.Width = tWidth;
    Paint.Height = tHeight;
  } else {
    Paint.Width = tHeight;
    Paint.Height = tWidth;
  }
}

void EPaperDriver_::PaintSelectImage(uint8_t *tImage) {
  Paint.Image = tImage;
}

void EPaperDriver_::PaintSetRotate(uint16_t tRotate) {
  if (tRotate == Rotate0 || tRotate == Rotate90 || tRotate == Rotate180 || tRotate == Rotate270) {
    Paint.Rotate = tRotate;
  }
}

void EPaperDriver_::PaintSetMirroring(uint8_t tMirror) {
  if (tMirror == MirrorNone || tMirror == MirrorHorizontal || tMirror == MirrorVertical || tMirror == MirrorOrigin) {
    Paint.Mirror = tMirror;
  }
}

void EPaperDriver_::PaintSetScale(uint8_t tScale) {
  if (tScale == 2) {
    Paint.Scale = tScale;
    Paint.WidthByte = (Paint.WidthMemory % 8 == 0) ? (Paint.WidthMemory / 8) : (Paint.WidthMemory / 8 + 1);
  } else if (tScale == 4) {
    Paint.Scale = tScale;
    Paint.WidthByte = (Paint.WidthMemory % 4 == 0) ? (Paint.WidthMemory / 4) : (Paint.WidthMemory / 4 + 1);
  } else if (tScale == 6 || tScale == 7 || tScale == 16) {
    Paint.Scale = tScale;
    Paint.WidthByte = (Paint.WidthMemory % 2 == 0) ? (Paint.WidthMemory / 2) : (Paint.WidthMemory / 2 + 1);
  }
}

void IRAM_ATTR EPaperDriver_::PaintSetPixel(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor) {
  if (!Paint.Image) return;
  if (tXpoint >= Paint.Width || tYpoint >= Paint.Height) return;
  uint16_t tX = 0;
  uint16_t tY = 0;
  switch (Paint.Rotate) {
    case Rotate0:
      tX = tXpoint;
      tY = tYpoint;
      break;
    case Rotate90:
      tX = Paint.WidthMemory - tYpoint - 1;
      tY = tXpoint;
      break;
    case Rotate180:
      tX = Paint.WidthMemory - tXpoint - 1;
      tY = Paint.HeightMemory - tYpoint - 1;
      break;
    case Rotate270:
      tX = tYpoint;
      tY = Paint.HeightMemory - tXpoint - 1;
      break;
    default:
      return;
  }
  switch (Paint.Mirror) {
    case MirrorNone:
      break;
    case MirrorHorizontal:
      tX = Paint.WidthMemory - tX - 1;
      break;
    case MirrorVertical:
      tY = Paint.HeightMemory - tY - 1;
      break;
    case MirrorOrigin:
      tX = Paint.WidthMemory - tX - 1;
      tY = Paint.HeightMemory - tY - 1;
      break;
    default:
      return;
  }
  if (tX >= Paint.WidthMemory || tY >= Paint.HeightMemory) return;
  if (Paint.Scale == 2) {
    uint32_t tAddress = tX / 8 + tY * Paint.WidthByte;
    uint8_t tReadData = Paint.Image[tAddress];
    if (tColor == MonoBlack) {
      Paint.Image[tAddress] = tReadData & ~(0x80 >> (tX % 8));
    } else {
      Paint.Image[tAddress] = tReadData | (0x80 >> (tX % 8));
    }
  } else if (Paint.Scale == 4) {
    uint32_t tAddress = tX / 4 + tY * Paint.WidthByte;
    uint8_t tReadData = Paint.Image[tAddress];
    uint16_t tClampedColor = tColor % 4;
    tReadData = tReadData & (~(0xC0 >> ((tX % 4) * 2)));
    Paint.Image[tAddress] = tReadData | ((tClampedColor << 6) >> ((tX % 4) * 2));
  } else if (Paint.Scale == 6 || Paint.Scale == 7 || Paint.Scale == 16) {
    uint32_t tAddress = tX / 2 + tY * Paint.WidthByte;
    uint8_t tReadData = Paint.Image[tAddress];
    tReadData = tReadData & (~(0xF0 >> ((tX % 2) * 4)));
    Paint.Image[tAddress] = tReadData | ((tColor << 4) >> ((tX % 2) * 4));
  }
}

void EPaperDriver_::PaintClear(uint16_t tColor) {
  if (!Paint.Image) return;
  if (Paint.Scale == 2) {
    for (uint16_t tY = 0; tY < Paint.HeightByte; tY++) {
      for (uint16_t tX = 0; tX < Paint.WidthByte; tX++) {
        uint32_t tAddress = tX + tY * Paint.WidthByte;
        Paint.Image[tAddress] = tColor;
      }
    }
  } else if (Paint.Scale == 4) {
    for (uint16_t tY = 0; tY < Paint.HeightByte; tY++) {
      for (uint16_t tX = 0; tX < Paint.WidthByte; tX++) {
        uint32_t tAddress = tX + tY * Paint.WidthByte;
        Paint.Image[tAddress] = (tColor << 6) | (tColor << 4) | (tColor << 2) | tColor;
      }
    }
  } else if (Paint.Scale == 6 || Paint.Scale == 7 || Paint.Scale == 16) {
    for (uint16_t tY = 0; tY < Paint.HeightByte; tY++) {
      for (uint16_t tX = 0; tX < Paint.WidthByte; tX++) {
        uint32_t tAddress = tX + tY * Paint.WidthByte;
        Paint.Image[tAddress] = (tColor << 4) | tColor;
      }
    }
  }
}

void EPaperDriver_::PaintClearWindows(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor) {
  for (uint16_t tYpoint = tYstart; tYpoint < tYend; tYpoint++) {
    for (uint16_t tXpoint = tXstart; tXpoint < tXend; tXpoint++) PaintSetPixel(tXpoint, tYpoint, tColor);
  }
}

void EPaperDriver_::PaintDrawPoint(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor, DotPixel tDotPixel, DotStyle tDotStyle) {
  int16_t tXDir = 0;
  int16_t tYDir = 0;
  if (tDotStyle == DotFillAround) {
    for (tXDir = 0; tXDir < 2 * tDotPixel - 1; tXDir++) {
      for (tYDir = 0; tYDir < 2 * tDotPixel - 1; tYDir++) {
        int32_t tPixelX = (int32_t)tXpoint + tXDir - tDotPixel;
        int32_t tPixelY = (int32_t)tYpoint + tYDir - tDotPixel;
        if (tPixelX >= 0 && tPixelY >= 0) PaintSetPixel((uint16_t)tPixelX, (uint16_t)tPixelY, tColor);
      }
    }
  } else {
    for (tXDir = 0; tXDir < tDotPixel; tXDir++) {
      for (tYDir = 0; tYDir < tDotPixel; tYDir++) {
        int32_t tPixelX = (int32_t)tXpoint + tXDir - 1;
        int32_t tPixelY = (int32_t)tYpoint + tYDir - 1;
        if (tPixelX >= 0 && tPixelY >= 0) PaintSetPixel((uint16_t)tPixelX, (uint16_t)tPixelY, tColor);
      }
    }
  }
}

void EPaperDriver_::PaintDrawLine(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, LineStyle tLineStyle) {
  uint16_t tXpoint = tXstart;
  uint16_t tYpoint = tYstart;
  int tDeltaX = ((int)tXend - (int)tXstart >= 0) ? (tXend - tXstart) : (tXstart - tXend);
  int tDeltaY = ((int)tYend - (int)tYstart <= 0) ? (tYend - tYstart) : (tYstart - tYend);
  int tXAddWay = (tXstart < tXend) ? 1 : -1;
  int tYAddWay = (tYstart < tYend) ? 1 : -1;
  int tError = tDeltaX + tDeltaY;
  int tDottedLength = 0;
  for (;;) {
    tDottedLength++;
    if (tLineStyle == LineStyleDotted && tDottedLength % 3 == 0) {
      PaintDrawPoint(tXpoint, tYpoint, ImageBackground, tLineWidth, DotFillAround);
      tDottedLength = 0;
    } else PaintDrawPoint(tXpoint, tYpoint, tColor, tLineWidth, DotFillAround);
    if (2 * tError >= tDeltaY) {
      if (tXpoint == tXend) break;
      tError += tDeltaY;
      tXpoint += tXAddWay;
    }
    if (2 * tError <= tDeltaX) {
      if (tYpoint == tYend) break;
      tError += tDeltaX;
      tYpoint += tYAddWay;
    }
  }
}

void EPaperDriver_::PaintDrawRectangle(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill) {
  if (tDrawFill == DrawFillFull) {
    for (uint16_t tYpoint = tYstart; tYpoint < tYend; tYpoint++) PaintDrawLine(tXstart, tYpoint, tXend, tYpoint, tColor, tLineWidth, LineStyleSolid);
  } else {
    PaintDrawLine(tXstart, tYstart, tXend, tYstart, tColor, tLineWidth, LineStyleSolid);
    PaintDrawLine(tXstart, tYstart, tXstart, tYend, tColor, tLineWidth, LineStyleSolid);
    PaintDrawLine(tXend, tYend, tXend, tYstart, tColor, tLineWidth, LineStyleSolid);
    PaintDrawLine(tXend, tYend, tXstart, tYend, tColor, tLineWidth, LineStyleSolid);
  }
}

void EPaperDriver_::PaintDrawCircle(uint16_t tXcenter, uint16_t tYcenter, uint16_t tRadius, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill) {
  if (tRadius == 0) {
    PaintDrawPoint(tXcenter, tYcenter, tColor, tLineWidth, DotFillAround);
    return;
  }
  auto tDrawHorizontalSpan = [&](int32_t tXstart, int32_t tXend, int32_t tY) {
    if (tY < 0 || tXend < 0) return;
    if (tXstart < 0) tXstart = 0;
    PaintDrawLine(static_cast<uint16_t>(tXstart), static_cast<uint16_t>(tY), static_cast<uint16_t>(tXend), static_cast<uint16_t>(tY), tColor, tLineWidth, LineStyleSolid);
  };
  auto tDrawOutlinePoints = [&](int32_t tOffsetX, int32_t tOffsetY) {
    int32_t tPointX = static_cast<int32_t>(tXcenter) + tOffsetX;
    int32_t tPointY = static_cast<int32_t>(tYcenter) + tOffsetY;
    if (tPointX >= 0 && tPointY >= 0) PaintDrawPoint(static_cast<uint16_t>(tPointX), static_cast<uint16_t>(tPointY), tColor, tLineWidth, DotFillAround);
  };
  int32_t tX = 0;
  int32_t tY = static_cast<int32_t>(tRadius);
  int32_t tDecision = 3 - (2 * tY);
  while (tX <= tY) {
    if (tDrawFill == DrawFillFull) {
      tDrawHorizontalSpan(static_cast<int32_t>(tXcenter) - tX, static_cast<int32_t>(tXcenter) + tX, static_cast<int32_t>(tYcenter) - tY);
      tDrawHorizontalSpan(static_cast<int32_t>(tXcenter) - tX, static_cast<int32_t>(tXcenter) + tX, static_cast<int32_t>(tYcenter) + tY);
      tDrawHorizontalSpan(static_cast<int32_t>(tXcenter) - tY, static_cast<int32_t>(tXcenter) + tY, static_cast<int32_t>(tYcenter) - tX);
      tDrawHorizontalSpan(static_cast<int32_t>(tXcenter) - tY, static_cast<int32_t>(tXcenter) + tY, static_cast<int32_t>(tYcenter) + tX);
    } else {
      tDrawOutlinePoints(tX, tY);
      tDrawOutlinePoints(-tX, tY);
      tDrawOutlinePoints(tX, -tY);
      tDrawOutlinePoints(-tX, -tY);
      tDrawOutlinePoints(tY, tX);
      tDrawOutlinePoints(-tY, tX);
      tDrawOutlinePoints(tY, -tX);
      tDrawOutlinePoints(-tY, -tX);
    }
    if (tDecision < 0) tDecision += 4 * tX + 6;
    else {
      tDecision += 4 * (tX - tY) + 10;
      tY--;
    }
    tX++;
  }
}

void EPaperDriver_::PaintDrawBitmap(const unsigned char *tImageBuffer) {
  if (!Paint.Image || !tImageBuffer) return;
  for (uint16_t tY = 0; tY < Paint.HeightByte; tY++) {
    for (uint16_t tX = 0; tX < Paint.WidthByte; tX++) {
      uint32_t tAddress = tX + tY * Paint.WidthByte;
      Paint.Image[tAddress] = tImageBuffer[tAddress];
    }
  }
}

uint8_t EPaperDriver_::MapRgbTo6Color(uint8_t tBlue, uint8_t tGreen, uint8_t tRed) {
  if (tBlue == 0 && tGreen == 0 && tRed == 0) return EpaperColorBlack;
  if (tBlue == 255 && tGreen == 255 && tRed == 255) return EpaperColorWhite;
  if (tBlue == 0 && tGreen == 255 && tRed == 255) return EpaperColorYellow;
  if (tBlue == 0 && tGreen == 0 && tRed == 255) return EpaperColorRed;
  if (tBlue == 255 && tGreen == 0 && tRed == 0) return EpaperColorBlue;
  if (tBlue == 0 && tGreen == 255 && tRed == 0) return EpaperColorGreen;
  return EpaperColorWhite;
}

bool EPaperDriver_::ReadFully(File &tFile, uint8_t *tBuffer, size_t tSize) {
  size_t tTotalRead = 0;
  while (tTotalRead < tSize) {
    size_t tReadNow = tFile.read(tBuffer + tTotalRead, tSize - tTotalRead);
    if (tReadNow == 0) return false;
    tTotalRead += tReadNow;
  }
  return true;
}

uint8_t EPaperDriver_::GuiReadBmpRgb6Color(const char *tPath, uint16_t tXstart, uint16_t tYstart) {
  if (!tPath) return 0;
  File tFile = SD.open(tPath, FILE_READ);
  if (!tFile) tFile = LittleFS.open(tPath, FILE_READ);
  if (!tFile)  return 0;
  BMPFILEHEADER tBmpFileHeader = {};
  BMPINFOHEADER tBmpInfoHeader = {};
  if (!ReadFully(tFile, reinterpret_cast<uint8_t*>(&tBmpFileHeader), sizeof(BMPFILEHEADER)) || !ReadFully(tFile, reinterpret_cast<uint8_t*>(&tBmpInfoHeader), sizeof(BMPINFOHEADER))) {
    tFile.close();
    return 0;
  }
  if (tBmpFileHeader.bType != 0x4D42 || tBmpInfoHeader.biBitCount != 24 || tBmpInfoHeader.biWidth == 0 || tBmpInfoHeader.biHeight == 0) {
    tFile.close();
    return 0;
  }
  uint16_t tWidth = (uint16_t)tBmpInfoHeader.biWidth;
  uint16_t tHeight = (uint16_t)tBmpInfoHeader.biHeight;
  size_t tRowSize = ((size_t)tWidth * 3 + 3) & ~((size_t)3);
  uint8_t *tRowBuffer = static_cast<uint8_t*>(heap_caps_malloc(tRowSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  uint8_t *tImageBuffer = static_cast<uint8_t*>(heap_caps_malloc((size_t)tWidth * tHeight, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!tRowBuffer || !tImageBuffer) {
    if (tRowBuffer) heap_caps_free(tRowBuffer);
    if (tImageBuffer) heap_caps_free(tImageBuffer);
    tFile.close();
    return 0;
  }
  if (!tFile.seek(tBmpFileHeader.bOffset)) {
    heap_caps_free(tRowBuffer);
    heap_caps_free(tImageBuffer);
    tFile.close();
    return 0;
  }
  for (uint16_t tRowIndex = 0; tRowIndex < tHeight; tRowIndex++) {
    if (!ReadFully(tFile, tRowBuffer, tRowSize)) {
      heap_caps_free(tRowBuffer);
      heap_caps_free(tImageBuffer);
      tFile.close();
      return 0;
    }
    for (uint16_t tColumnIndex = 0; tColumnIndex < tWidth; tColumnIndex++) {
      size_t tOffset = (size_t)tColumnIndex * 3;
      uint8_t tBlue = tRowBuffer[tOffset + 0];
      uint8_t tGreen = tRowBuffer[tOffset + 1];
      uint8_t tRed = tRowBuffer[tOffset + 2];
      uint8_t tColor = MapRgbTo6Color(tBlue, tGreen, tRed);
      tImageBuffer[(size_t)(tHeight - 1 - tRowIndex) * tWidth + tColumnIndex] = tColor;
    }
  }
  tFile.close();
  for (uint16_t tY = 0; tY < tHeight; tY++) {
    uint16_t tPaintY = tYstart + tY;
    if (tPaintY >= Paint.Height) break;
    for (uint16_t tX = 0; tX < tWidth; tX++) {
      uint16_t tPaintX = tXstart + tX;
      if (tPaintX >= Paint.Width) break;
      PaintSetPixel(tPaintX, tPaintY, tImageBuffer[(size_t)tY * tWidth + tX]);
    }
  }
  heap_caps_free(tRowBuffer);
  heap_caps_free(tImageBuffer);
  return 1;
}

void EPaperDriver_::Init() {
  LockGuard tLock;
  PortInit();
}

void EPaperDriver_::Sleep() {
  LockGuard tLock;
  PortSleep();
}

void EPaperDriver_::Clear(uint8_t *tImage, uint8_t tColor) {
  LockGuard tLock;
  PortClear(tImage, tColor);
}

void EPaperDriver_::Display(uint8_t *tImage) {
  LockGuard tLock;
  EpaperPortClearBusyTimeout();
  PortDisplay(tImage);
  if (EpaperPortGetAndClearBusyTimeout()) {
    log_w("Display timeout detected, reinitializing e-paper port and retrying once");
    gPortInitialized = false;
    PortInit();
    if (!gPortInitialized) {
      log_e("E-paper port reinit failed after display timeout");
      return;
    }
    EpaperPortClearBusyTimeout();
    PortDisplay(tImage);
    if (EpaperPortGetAndClearBusyTimeout()) {
      log_e("Display retry failed due to BUSY timeout");
    }
  }
}

void EPaperDriver_::NewImage(uint8_t *tImage, uint16_t tWidth, uint16_t tHeight, uint16_t tRotate, uint16_t tColor) {
  LockGuard tLock;
  PaintNewImage(tImage, tWidth, tHeight, tRotate, tColor);
}

void EPaperDriver_::SelectImage(uint8_t *tImage) {
  LockGuard tLock;
  PaintSelectImage(tImage);
}

void EPaperDriver_::SetRotate(uint16_t tRotate) {
  LockGuard tLock;
  PaintSetRotate(tRotate);
}

void EPaperDriver_::SetMirroring(uint8_t tMirror) {
  LockGuard tLock;
  PaintSetMirroring(tMirror);
}

void EPaperDriver_::SetScale(uint8_t tScale) {
  LockGuard tLock;
  PaintSetScale(tScale);
}

void IRAM_ATTR EPaperDriver_::SetPixel(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor) {
  LockGuard tLock;
  PaintSetPixel(tXpoint, tYpoint, tColor);
}

void EPaperDriver_::ClearImage(uint16_t tColor) {
  LockGuard tLock;
  PaintClear(tColor);
}

void EPaperDriver_::ClearWindows(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor) {
  LockGuard tLock;
  PaintClearWindows(tXstart, tYstart, tXend, tYend, tColor);
}

void EPaperDriver_::DrawPoint(uint16_t tXpoint, uint16_t tYpoint, uint16_t tColor, DotPixel tDotPixel, DotStyle tDotStyle) {
  LockGuard tLock;
  PaintDrawPoint(tXpoint, tYpoint, tColor, tDotPixel, tDotStyle);
}

void EPaperDriver_::DrawLine(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, LineStyle tLineStyle) {
  LockGuard tLock;
  PaintDrawLine(tXstart, tYstart, tXend, tYend, tColor, tLineWidth, tLineStyle);
}

void EPaperDriver_::DrawRectangle(uint16_t tXstart, uint16_t tYstart, uint16_t tXend, uint16_t tYend, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill) {
  LockGuard tLock;
  PaintDrawRectangle(tXstart, tYstart, tXend, tYend, tColor, tLineWidth, tDrawFill);
}

void EPaperDriver_::DrawCircle(uint16_t tXcenter, uint16_t tYcenter, uint16_t tRadius, uint16_t tColor, DotPixel tLineWidth, DrawFill tDrawFill) {
  LockGuard tLock;
  PaintDrawCircle(tXcenter, tYcenter, tRadius, tColor, tLineWidth, tDrawFill);
}

void EPaperDriver_::DrawBitmap(const unsigned char *tImageBuffer) {
  LockGuard tLock;
  PaintDrawBitmap(tImageBuffer);
}

uint8_t EPaperDriver_::ReadBmpRgb6Color(const char *tPath, uint16_t tXstart, uint16_t tYstart) {
  LockGuard tLock;
  return GuiReadBmpRgb6Color(tPath, tXstart, tYstart);
}