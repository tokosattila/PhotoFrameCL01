#include <App/Display.h>

namespace App {

  Display_ &Display_::Instance() {
    static Display_ tInstance;
    return tInstance;
  }

  Display_::Display_() {
    mMutex = xSemaphoreCreateRecursiveMutex();
  }

  Display_::~Display_() {
    if (mFrameBuffer) heap_caps_free(mFrameBuffer);
    if (mMutex) {
      vSemaphoreDelete(mMutex);
      mMutex = nullptr;
    }
  }

  void Display_::Lock() {
    if (Instance().mMutex) xSemaphoreTakeRecursive(Instance().mMutex, portMAX_DELAY);
  }

  void Display_::Unlock() {
    if (Instance().mMutex) xSemaphoreGiveRecursive(Instance().mMutex);
  }

  void Display_::Init() {
    Guard tLock;
    ReloadConfig();
    if (mFrameBuffer) {
      heap_caps_free(mFrameBuffer);
      mFrameBuffer = nullptr;
    }
    EPaperDriver_::Init();
    mFrameBuffer = static_cast<uint8_t*>(heap_caps_calloc(mCfg.Width * mCfg.Height / 2, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!mFrameBuffer) xLOG("Allocation PS memory failed");
    EPaperDriver_::NewImage(mFrameBuffer, static_cast<uint16_t>(mCfg.Width), static_cast<uint16_t>(mCfg.Height), static_cast<uint16_t>(mDisplayRotate), static_cast<uint16_t>(EDisplayColor::White));
    EPaperDriver_::SetScale(6);
    SetFont(&OpenSans13B);
    SetBgColor(EDisplayColor::White);
    SetColor(EDisplayColor::Black);
    EPaperDriver_::ClearImage(static_cast<uint16_t>(mBgColor));
    xLOG("Display init successful");
  }

  void Display_::ReloadConfig() {
    Guard tLock;
    mCfg = CFG.Get<SDisplayConfig>();
  }

  void Display_::DeInit() {
    Guard tLock;
    if (mFrameBuffer) {
      heap_caps_free(mFrameBuffer);
      mFrameBuffer = nullptr;
    }
  };

  int32_t Display_::ClampToRange(int32_t tValue, int32_t tMinimum, int32_t tMaximum) {
    if (tValue < tMinimum) return tMinimum;
    if (tValue > tMaximum) return tMaximum;
    return tValue;
  }

  int32_t Display_::MaxOf(int32_t tLeft, int32_t tRight) {
    return (tLeft > tRight) ? tLeft : tRight;
  }

  int32_t Display_::DivideFloor(int32_t tValue, int32_t tDivisor) {
    if (tDivisor <= 0) return tValue;
    if (tValue >= 0) return tValue / tDivisor;
    return -(((-tValue) + tDivisor - 1) / tDivisor);
  }

  int32_t Display_::DivideCeil(int32_t tValue, int32_t tDivisor) {
    if (tDivisor <= 0) return tValue;
    if (tValue >= 0) return (tValue + tDivisor - 1) / tDivisor;
    return -((-tValue) / tDivisor);
  }

  const GFXglyph* Display_::FindGlyphForCodePoint(const GFXfont *tFont, uint16_t tCodePoint) {
    if (!tFont || !tFont->Intervals || !tFont->Glyphs) return nullptr;
    for (uint16_t tIndex = 0; tIndex < tFont->IntervalCount; tIndex++) {
      const UnicodeInterval &tRange = tFont->Intervals[tIndex];
      if (tCodePoint >= tRange.First && tCodePoint <= tRange.Last) {
        uint16_t tGlyphIndex = tRange.Offset + (tCodePoint - tRange.First);
        return &tFont->Glyphs[tGlyphIndex];
      }
    }
    return nullptr;
  }

  bool Display_::DecodeGlyphBitmapData(const GFXfont *tFont, const GFXglyph *tGlyph, std::vector<uint8_t> &tOutput) {
    if (!tFont || !tGlyph || !tFont->Bitmaps) return false;
    size_t tBytesPerRow = (static_cast<size_t>(tGlyph->Width) + 1) / 2;
    size_t tRawLength = tBytesPerRow * tGlyph->Height;
    if (tRawLength == 0) return false;
    const uint8_t *tCompressedData = tFont->Bitmaps + tGlyph->DataOffset;
    tOutput.assign(tRawLength, 0);
    if (!tFont->Compressed) {
      size_t tCopyLength = std::min(tRawLength, static_cast<size_t>(tGlyph->DataLen));
      memcpy(tOutput.data(), tCompressedData, tCopyLength);
      return true;
    }
    size_t tDestLength = tinfl_decompress_mem_to_mem(
      tOutput.data(),
      tRawLength,
      tCompressedData,
      static_cast<size_t>(tGlyph->DataLen),
      kTinflZlibHeaderFlag
    );
    return !(tDestLength == static_cast<size_t>(-1) || tDestLength != tRawLength);
  }

  uint8_t Display_::ReadGlyphCoverageNibble(const std::vector<uint8_t> &tData, uint16_t tWidth, uint16_t tX, uint16_t tY) {
    size_t tBytesPerRow = (static_cast<size_t>(tWidth) + 1) / 2;
    size_t tByteIndex = static_cast<size_t>(tY) * tBytesPerRow + tX / 2;
    if (tByteIndex >= tData.size()) return 0;
    uint8_t tByte = tData[tByteIndex];
    return (tX % 2 == 0) ? ((tByte >> 4) & 0x0F) : (tByte & 0x0F);
  }

  uint8_t Display_::NormalizeToSpectra6Color(uint8_t tColor) {
    switch (tColor) {
      case EpaperColorBlack:
      case EpaperColorWhite:
      case EpaperColorYellow:
      case EpaperColorRed:
      case EpaperColorBlue:
      case EpaperColorGreen:
        return tColor;
      default:
        return EpaperColorWhite;
    }
  }

  int32_t Display_::GetCanvasWidthUnsafe() const {
    if (mDisplayRotate == EDisplayRotate::Rotate90 || mDisplayRotate == EDisplayRotate::Rotate270) return mCfg.Height;
    return mCfg.Width;
  }

  int32_t Display_::GetCanvasHeightUnsafe() const {
    if (mDisplayRotate == EDisplayRotate::Rotate90 || mDisplayRotate == EDisplayRotate::Rotate270) return mCfg.Width;
    return mCfg.Height;
  }

  int32_t Display_::GetCanvasWidth() const {
    Guard tLock;
    return GetCanvasWidthUnsafe();
  }

  int32_t Display_::GetCanvasHeight() const {
    Guard tLock;
    return GetCanvasHeightUnsafe();
  }

  bool Display_::MapLogicalToPhysical(int32_t tX, int32_t tY, int32_t &tMappedX, int32_t &tMappedY) const {
    switch (mDisplayRotate) {
      case EDisplayRotate::Rotate0:
        tMappedX = tX;
        tMappedY = tY;
        break;
      case EDisplayRotate::Rotate90:
        tMappedX = mCfg.Width - 1 - tY;
        tMappedY = tX;
        break;
      case EDisplayRotate::Rotate180:
        tMappedX = mCfg.Width - 1 - tX;
        tMappedY = mCfg.Height - 1 - tY;
        break;
      case EDisplayRotate::Rotate270:
        tMappedX = tY;
        tMappedY = mCfg.Height - 1 - tX;
        break;
      default:
        return false;
    }
    return tMappedX >= 0 && tMappedX < mCfg.Width && tMappedY >= 0 && tMappedY < mCfg.Height;
  }

  void Display_::SetFrameBufferPixel(int32_t tX, int32_t tY, uint8_t tColor) {
    if (!mFrameBuffer) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    if (tX < 0 || tY < 0 || tX >= tCanvasWidth || tY >= tCanvasHeight) return;
    int32_t tMappedX = 0;
    int32_t tMappedY = 0;
    if (!MapLogicalToPhysical(tX, tY, tMappedX, tMappedY)) return;
    tColor = NormalizeToSpectra6Color(tColor);
    uint32_t tAddress = static_cast<uint32_t>(tMappedX / 2 + tMappedY * (mCfg.Width / 2));
    uint8_t tReadData = mFrameBuffer[tAddress];
    tReadData = static_cast<uint8_t>(tReadData & static_cast<uint8_t>(~(0xF0 >> ((tMappedX % 2) * 4))));
    mFrameBuffer[tAddress] = static_cast<uint8_t>(tReadData | ((tColor << 4) >> ((tMappedX % 2) * 4)));
  }

  void Display_::SetFont(const GFXfont *tFont) {
    mGfxFont = tFont;
  }

  void Display_::SetRotate(EDisplayRotate tRotate) {
    Guard tLock;
    EDisplayRotate tResolvedRotate = static_cast<EDisplayRotate>(0);
    switch (tRotate) {
      case EDisplayRotate::Rotate0:
      case EDisplayRotate::Rotate90:
      case EDisplayRotate::Rotate180:
      case EDisplayRotate::Rotate270:
        tResolvedRotate = tRotate;
        break;
      default:
        tResolvedRotate = static_cast<EDisplayRotate>(0);
        break;
    }
    if (mDisplayRotate == tResolvedRotate) return;
    mDisplayRotate = tResolvedRotate;
    EPaperDriver_::SetRotate(static_cast<uint16_t>(mDisplayRotate));
  }

  void Display_::SetBgColor(EDisplayColor tColor) {
    Guard tLock;
    mBgColor = tColor;
    if (mFrameBuffer) EPaperDriver_::ClearImage(static_cast<uint16_t>(tColor));
  }
  
  void Display_::SetColor(EDisplayColor tColor) {
    mColor = tColor;
  }

  void Display_::On() {
    Guard tLock;
    if (!mFrameBuffer) return;
    EPaperDriver_::Init();
    EPaperDriver_::NewImage(mFrameBuffer, static_cast<uint16_t>(mCfg.Width), static_cast<uint16_t>(mCfg.Height), static_cast<uint16_t>(mDisplayRotate), static_cast<uint16_t>(mBgColor));
    EPaperDriver_::SetScale(6);
  }

  void Display_::Off() {
    Guard tLock;
    if (!mFrameBuffer) return;
    EPaperDriver_::Sleep();
  }

  void Display_::OffAll() {
    Guard tLock;
    if (!mFrameBuffer) return;
    EPaperDriver_::Sleep();
    heap_caps_free(mFrameBuffer);
    mFrameBuffer = nullptr;
  }

  void Display_::FillRect(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, EDisplayColor tColor) {
    Guard tLock;
    if (!mFrameBuffer || tWidth <= 0 || tHeight <= 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    int32_t tStartX = ClampToRange(tX, 0, tCanvasWidth - 1);
    int32_t tStartY = ClampToRange(tY, 0, tCanvasHeight - 1);
    int32_t tEndX = ClampToRange(tX + tWidth - 1, 0, tCanvasWidth - 1);
    int32_t tEndY = ClampToRange(tY + tHeight - 1, 0, tCanvasHeight - 1);
    if (tEndX < tStartX || tEndY < tStartY) return;
    EPaperDriver_::DrawRectangle(static_cast<uint16_t>(tStartX), static_cast<uint16_t>(tStartY), static_cast<uint16_t>(tEndX), static_cast<uint16_t>(tEndY), static_cast<uint16_t>(tColor), DotPixel1x1, DrawFillFull);
  }

  void Display_::DrawRect(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, EDisplayColor tColor) {
    Guard tLock;
    if (!mFrameBuffer || tWidth <= 0 || tHeight <= 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    int32_t tStartX = ClampToRange(tX, 0, tCanvasWidth - 1);
    int32_t tStartY = ClampToRange(tY, 0, tCanvasHeight - 1);
    int32_t tEndX = ClampToRange(tX + tWidth - 1, 0, tCanvasWidth - 1);
    int32_t tEndY = ClampToRange(tY + tHeight - 1, 0, tCanvasHeight - 1);
    if (tEndX < tStartX || tEndY < tStartY) return;
    EPaperDriver_::DrawRectangle(static_cast<uint16_t>(tStartX), static_cast<uint16_t>(tStartY), static_cast<uint16_t>(tEndX), static_cast<uint16_t>(tEndY), static_cast<uint16_t>(tColor), DotPixel1x1, DrawFillEmpty);
  }

  void Display_::DrawCircle(int32_t tXcenter, int32_t tYcenter, int32_t tRadius, EDisplayColor tColor) {
    Guard tLock;
    if (!mFrameBuffer || tRadius < 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    if (tXcenter < 0 || tYcenter < 0 || tXcenter >= tCanvasWidth || tYcenter >= tCanvasHeight) return;
    EPaperDriver_::DrawCircle(static_cast<uint16_t>(tXcenter), static_cast<uint16_t>(tYcenter), static_cast<uint16_t>(tRadius), static_cast<uint16_t>(tColor), DotPixel1x1, DrawFillEmpty);
  }

  void Display_::FillCircle(int32_t tXcenter, int32_t tYcenter, int32_t tRadius, EDisplayColor tColor) {
    Guard tLock;
    if (!mFrameBuffer || tRadius < 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    if (tXcenter < 0 || tYcenter < 0 || tXcenter >= tCanvasWidth || tYcenter >= tCanvasHeight) return;
    EPaperDriver_::DrawCircle(static_cast<uint16_t>(tXcenter), static_cast<uint16_t>(tYcenter), static_cast<uint16_t>(tRadius), static_cast<uint16_t>(tColor), DotPixel1x1, DrawFillFull);
  }

  STextBounds Display_::WriteText(int32_t tX, int32_t tY, const char *tText, EDisplayHAlignment tHAlignment, EDisplayVAlignment tVAlignment, EDisplayColor tBgColor) {
    Guard tLock;
    if (!mFrameBuffer || !tText) return STextBounds {};
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    int32_t tBoundWidth = 0;
    int32_t tBoundHeight = 0;
    int32_t tOriginOffsetX = 0;
    int32_t tOriginOffsetY = 0;
    if (mGfxFont) {
      int32_t tPenXForMeasure = 0;
      int32_t tMinX = INT32_MAX;
      int32_t tMinY = INT32_MAX;
      int32_t tMaxX = INT32_MIN;
      int32_t tMaxY = INT32_MIN;
      for (const char *tPtr = tText; *tPtr; tPtr++) {
        const GFXglyph *tGlyph = FindGlyphForCodePoint(mGfxFont, static_cast<uint8_t>(*tPtr));
        if (!tGlyph) continue;
        int32_t tDrawX = tPenXForMeasure + DivideFloor(tGlyph->XOffset, kGlyphRenderScaleDivisor);
        int32_t tDrawY = -DivideFloor(tGlyph->YOffset, kGlyphRenderScaleDivisor);
        int32_t tDrawWidth = DivideCeil(tGlyph->Width, kGlyphRenderScaleDivisor);
        int32_t tDrawHeight = DivideCeil(tGlyph->Height, kGlyphRenderScaleDivisor);
        if (tDrawWidth > 0 && tDrawHeight > 0) {
          tMinX = std::min(tMinX, tDrawX);
          tMinY = std::min(tMinY, tDrawY);
          tMaxX = std::max(tMaxX, tDrawX + tDrawWidth);
          tMaxY = std::max(tMaxY, tDrawY + tDrawHeight);
        }
        tPenXForMeasure += DivideCeil(tGlyph->XAdvance, kGlyphRenderScaleDivisor);
      }
      if (tMinX != INT32_MAX) {
        tBoundWidth = std::max(0, tMaxX - tMinX);
        tBoundHeight = std::max(0, tMaxY - tMinY);
        tOriginOffsetX = -tMinX;
        tOriginOffsetY = -tMinY;
      }
    } else return STextBounds {};
    int32_t tCursorX = tX;
    int32_t tCursorY = tY;
    if (tX == 0 && tHAlignment == EDisplayHAlignment::Center) tCursorX = tCanvasWidth / 2;
    if (tY == 0 && tVAlignment == EDisplayVAlignment::Center) tCursorY = tCanvasHeight / 2;
    if (tHAlignment == EDisplayHAlignment::Right) tCursorX -= tBoundWidth;
    if (tHAlignment == EDisplayHAlignment::Center) tCursorX -= tBoundWidth / 2;
    if (tVAlignment == EDisplayVAlignment::Center) tCursorY -= tBoundHeight / 2;
    tCursorX = ClampToRange(tCursorX, 0, MaxOf(0, tCanvasWidth - tBoundWidth));
    tCursorY = ClampToRange(tCursorY, 0, MaxOf(0, tCanvasHeight - tBoundHeight));
    if (tBoundWidth > 0 && tBoundHeight > 0) {
      uint8_t tBackground = NormalizeToSpectra6Color(static_cast<uint8_t>(tBgColor));
      for (int32_t tFillY = 0; tFillY < tBoundHeight; tFillY++) {
        int32_t tPy = tCursorY + tFillY;
        if (tPy < 0 || tPy >= tCanvasHeight) continue;
        for (int32_t tFillX = 0; tFillX < tBoundWidth; tFillX++) {
          int32_t tPx = tCursorX + tFillX;
          if (tPx < 0 || tPx >= tCanvasWidth) continue;
          SetFrameBufferPixel(tPx, tPy, tBackground);
        }
      }
    }
    {
      int32_t tPenX = tCursorX + tOriginOffsetX;
      int32_t tBaselineY = tCursorY + tOriginOffsetY;
      for (const char *tPtr = tText; *tPtr; tPtr++) {
        const GFXglyph *tGlyph = FindGlyphForCodePoint(mGfxFont, static_cast<uint8_t>(*tPtr));
        if (!tGlyph) continue;
        std::vector<uint8_t> tGlyphData;
        if (!DecodeGlyphBitmapData(mGfxFont, tGlyph, tGlyphData)) {
          tPenX += DivideCeil(tGlyph->XAdvance, kGlyphRenderScaleDivisor);
          continue;
        }
        int32_t tDrawX = tPenX + DivideFloor(tGlyph->XOffset, kGlyphRenderScaleDivisor);
        int32_t tDrawY = tBaselineY - DivideFloor(tGlyph->YOffset, kGlyphRenderScaleDivisor);
        int32_t tOutWidth = DivideCeil(tGlyph->Width, kGlyphRenderScaleDivisor);
        int32_t tOutHeight = DivideCeil(tGlyph->Height, kGlyphRenderScaleDivisor);
        for (int32_t tOy = 0; tOy < tOutHeight; tOy++) {
          for (int32_t tOx = 0; tOx < tOutWidth; tOx++) {
            int32_t tPx = tDrawX + tOx;
            int32_t tPy = tDrawY + tOy;
            if (tPx < 0 || tPy < 0 || tPx >= tCanvasWidth || tPy >= tCanvasHeight) continue;
            uint32_t tNibbleSum = 0;
            uint32_t tNibbleCount = 0;
            int32_t tStartY = tOy * kGlyphRenderScaleDivisor;
            int32_t tEndY = std::min<int32_t>(tGlyph->Height, tStartY + kGlyphRenderScaleDivisor);
            int32_t tStartX = tOx * kGlyphRenderScaleDivisor;
            int32_t tEndX = std::min<int32_t>(tGlyph->Width, tStartX + kGlyphRenderScaleDivisor);
            for (int32_t tSy = tStartY; tSy < tEndY; tSy++) {
              for (int32_t tSx = tStartX; tSx < tEndX; tSx++) {
                uint8_t tNibble = ReadGlyphCoverageNibble(tGlyphData, tGlyph->Width, static_cast<uint16_t>(tSx), static_cast<uint16_t>(tSy));
                tNibbleSum += tNibble;
                tNibbleCount++;
              }
            }
            if (tNibbleCount == 0) continue;
            uint8_t tCoverage = static_cast<uint8_t>((tNibbleSum + tNibbleCount / 2) / tNibbleCount);
            if (tCoverage >= kGlyphCoverageOnThreshold) SetFrameBufferPixel(tPx, tPy, static_cast<uint8_t>(mColor));
          }
        }
        tPenX += DivideCeil(tGlyph->XAdvance, kGlyphRenderScaleDivisor);
      }
    }
    STextBounds tBounds;
    tBounds.X = tCursorX;
    tBounds.Y = tCursorY;
    tBounds.Width = tBoundWidth;
    tBounds.Height = tBoundHeight;
    return tBounds;
  }

  void Display_::WriteTextCentered(const char *tText, EDisplayColor tBgColor) {
    WriteText(0, 0, tText, EDisplayHAlignment::Center, EDisplayVAlignment::Center, tBgColor);
  }

  SBoxBounds Display_::WriteTextWithBoxCentered(const char *tText, int32_t tXPadding, int32_t tYPadding, EDisplayColor tBoxColor) {
    Guard tLock;
    if (!mFrameBuffer || !tText) return SBoxBounds {};
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    STextBounds tTextRect = WriteText(0, 0, tText, EDisplayHAlignment::Center, EDisplayVAlignment::Center);
    EPaperDriver_::ClearImage(static_cast<uint16_t>(mBgColor));
    constexpr int32_t kTextBoundsSafetyMargin = 2;
    int32_t tSafeXPadding = tXPadding + kTextBoundsSafetyMargin;
    int32_t tSafeYPadding = tYPadding + kTextBoundsSafetyMargin;
    int32_t tBoxX = tTextRect.X - tSafeXPadding;
    int32_t tBoxY = tTextRect.Y - tSafeYPadding;
    int32_t tBoxW = tTextRect.Width + 2 * tSafeXPadding;
    int32_t tBoxH = tTextRect.Height + 2 * tSafeYPadding;
    tBoxX = ClampToRange(tBoxX, 0, tCanvasWidth - 1);
    tBoxY = ClampToRange(tBoxY, 0, tCanvasHeight - 1);
    tBoxW = std::min(tBoxW, tCanvasWidth - tBoxX);
    tBoxH = std::min(tBoxH, tCanvasHeight - tBoxY);
    if (tBoxW <= 0 || tBoxH <= 0) return SBoxBounds {};
    FillRect(tBoxX, tBoxY, tBoxW, tBoxH, tBoxColor);
    WriteText(0, 0, tText, EDisplayHAlignment::Center, EDisplayVAlignment::Center, tBoxColor);
    SBoxBounds tBounds;
    tBounds.XStartPos = tBoxX;
    tBounds.YStartPos = tBoxY;
    tBounds.XEndPos = tBoxX + tBoxW - 1;
    tBounds.YEndPos = tBoxY + tBoxH - 1;
    return tBounds;
  }

  void Display_::PrintImage(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, const uint8_t *tData) {
    Guard tLock;
    if (!mFrameBuffer || !tData || tWidth <= 0 || tHeight <= 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    const int32_t tSourceStride = (tWidth + 1) / 2;
    uint32_t tInvalidColorCount = 0;
    for (int32_t tRow = 0; tRow < tHeight; tRow++) {
      int32_t tTargetY = tY + tRow;
      if (tTargetY < 0 || tTargetY >= tCanvasHeight) continue;
      for (int32_t tColumn = 0; tColumn < tWidth; tColumn++) {
        int32_t tTargetX = tX + tColumn;
        if (tTargetX < 0 || tTargetX >= tCanvasWidth) continue;
        size_t tSourceOffset = static_cast<size_t>(tRow) * static_cast<size_t>(tSourceStride) + static_cast<size_t>(tColumn / 2);
        uint8_t tSourceByte = tData[tSourceOffset];
        uint8_t tSourceColor = ((tColumn & 1) == 0) ? ((tSourceByte >> 4) & 0x0F) : (tSourceByte & 0x0F);
        if (NormalizeToSpectra6Color(tSourceColor) != tSourceColor) tInvalidColorCount++;
        SetFrameBufferPixel(tTargetX, tTargetY, tSourceColor);
      }
    }
    if (tInvalidColorCount > 0) {
      xLOG("Normalized %lu invalid color nibble(s) to Spectra6 palette", static_cast<unsigned long>(tInvalidColorCount));
    }
  }

  int IRAM_ATTR Display_::JpegDrawCallback(JPEGDRAW *tDraw) {
    if (!tDraw || !tDraw->pUser) return 0;
    SJpegDitherCtx *tCtx = static_cast<SJpegDitherCtx*>(tDraw->pUser);
    for (uint16_t tY = 0; tY < tDraw->iHeight; tY++) {
      int32_t tTargetY = tDraw->y + tY;
      if (tTargetY < 0 || tTargetY >= tCtx->CanvasHeight) continue;
      for (uint16_t tX = 0; tX < tDraw->iWidthUsed; tX++) {
        int32_t tTargetX = tDraw->x + tX;
        if (tTargetX < 0 || tTargetX >= tCtx->CanvasWidth) continue;
        const uint32_t *tPixels32 = reinterpret_cast<const uint32_t*>(tDraw->pPixels);
        uint32_t tPixel = tPixels32[tY * tDraw->iWidth + tX];
        int32_t tRelX = tTargetX - tCtx->AreaX;
        int32_t tRelY = tTargetY - tCtx->AreaY;
        if (tRelX < 0 || tRelY < 0 || tRelX >= tCtx->AreaWidth || tRelY >= tCtx->AreaHeight) continue;
        int32_t tBufIdx = (tRelY * tCtx->AreaWidth + tRelX) * 3;
        tCtx->RgbBuf[tBufIdx] = static_cast<uint8_t>(tPixel & 0xFF);
        tCtx->RgbBuf[tBufIdx + 1] = static_cast<uint8_t>((tPixel >> 8) & 0xFF);
        tCtx->RgbBuf[tBufIdx + 2] = static_cast<uint8_t>((tPixel >> 16) & 0xFF);
      }
    }
    return 1;
  }

  void Display_::ApplyFloydSteinberg(const SJpegDitherCtx &tCtx) {
    constexpr uint8_t kPalette888[6][3] = {
      {0, 0, 0},
      {255, 255, 255},
      {255, 255, 0},
      {255, 0, 0},
      {0, 0, 255},
      {0, 255, 0}
    };
    constexpr uint8_t kColorCode[6] = {
      EpaperColorBlack,
      EpaperColorWhite,
      EpaperColorYellow,
      EpaperColorRed,
      EpaperColorBlue,
      EpaperColorGreen
    };
    const int32_t kStartX = tCtx.AreaX;
    const int32_t kStartY = tCtx.AreaY;
    const int32_t kWidth = tCtx.AreaWidth;
    const int32_t kHeight = tCtx.AreaHeight;
    if (kWidth <= 0 || kHeight <= 0) return;
    uint8_t *tRgb = tCtx.RgbBuf;
    const int32_t kDitherBrightness = kBoardJpgBrightnessBase + (static_cast<int32_t>(mCfg.JpgBrightness) - static_cast<int32_t>(DISPLAY_JPG_BRIGHTNESS));
    const int32_t kDitherContrast = std::max<int32_t>(0, std::min<int32_t>(255, kBoardJpgContrastBase + (static_cast<int32_t>(mCfg.JpgContrast) - static_cast<int32_t>(DISPLAY_JPG_CONTRAST))));
    const int32_t kDitherGamma = std::max<int32_t>(1, std::min<int32_t>(255, kBoardJpgGammaBase + (static_cast<int32_t>(mCfg.JpgGamma) - static_cast<int32_t>(DISPLAY_JPG_GAMMA))));
    const int32_t kDitherSaturation = std::max<int32_t>(0, std::min<int32_t>(255, kBoardJpgSaturationBase + (static_cast<int32_t>(mCfg.JpgSaturation) - static_cast<int32_t>(DISPLAY_JPG_SATURATION))));
    const int32_t kDitherRedGain = std::max<int32_t>(0, std::min<int32_t>(200, kBoardJpgRedGainBase + (static_cast<int32_t>(mCfg.JpgRedGain) - static_cast<int32_t>(DISPLAY_JPG_RED_GAIN))));
    const int32_t kDitherGreenGain = std::max<int32_t>(0, std::min<int32_t>(200, kBoardJpgGreenGainBase + (static_cast<int32_t>(mCfg.JpgGreenGain) - static_cast<int32_t>(DISPLAY_JPG_GREEN_GAIN))));
    const int32_t kDitherBlueGain = std::max<int32_t>(0, std::min<int32_t>(200, kBoardJpgBlueGainBase + (static_cast<int32_t>(mCfg.JpgBlueGain) - static_cast<int32_t>(DISPLAY_JPG_BLUE_GAIN))));
    uint8_t tToneLut[256];
    {
      const int32_t kBrightness = kDitherBrightness;
      const int32_t kContrast = kDitherContrast;
      const uint8_t kGamma = static_cast<uint8_t>(kDitherGamma);
      for (int32_t tI = 0; tI < 256; tI++) {
        int32_t tVal = 128 + ((tI - 128) * kContrast) / 100;
        tVal += ((kBrightness - 100) * 255) / 100;
        if (tVal < 0) tVal = 0; else if (tVal > 255) tVal = 255;
        if (kGamma != 100) {
          float tNorm = static_cast<float>(tVal) / 255.0f;
          tNorm = powf(tNorm, 100.0f / static_cast<float>(kGamma));
          tVal = static_cast<int32_t>(tNorm * 255.0f + 0.5f);
          if (tVal < 0) tVal = 0; else if (tVal > 255) tVal = 255;
        }
        tToneLut[tI] = static_cast<uint8_t>(tVal);
      }
    }
    size_t tErrBufSize = static_cast<size_t>(2 * kWidth * 3) * sizeof(int16_t);
    int16_t *tErrBuf = static_cast<int16_t*>(heap_caps_calloc(1, tErrBufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tErrBuf) return;
    for (int32_t tY = 0; tY < kHeight; tY++) {
      int32_t tCurrRow = tY & 1;
      int32_t tNextRow = tCurrRow ^ 1;
      memset(tErrBuf + tNextRow * kWidth * 3, 0, static_cast<size_t>(kWidth * 3) * sizeof(int16_t));
      for (int32_t tX = 0; tX < kWidth; tX++) {
        int32_t tBufIdx = (tY * kWidth + tX) * 3;
        int32_t tErrIdx = (tCurrRow * kWidth + tX) * 3;
        int32_t tR = static_cast<int32_t>(tToneLut[tRgb[tBufIdx]])     + tErrBuf[tErrIdx];
        int32_t tG = static_cast<int32_t>(tToneLut[tRgb[tBufIdx + 1]]) + tErrBuf[tErrIdx + 1];
        int32_t tB = static_cast<int32_t>(tToneLut[tRgb[tBufIdx + 2]]) + tErrBuf[tErrIdx + 2];
        if (tR < 0) tR = 0; else if (tR > 255) tR = 255;
        if (tG < 0) tG = 0; else if (tG > 255) tG = 255;
        if (tB < 0) tB = 0; else if (tB > 255) tB = 255;
        if (kDitherSaturation != 100) {
          int32_t tGray = (tR + tG + tB) / 3;
          tR = tGray + (tR - tGray) * kDitherSaturation / 100;
          tG = tGray + (tG - tGray) * kDitherSaturation / 100;
          tB = tGray + (tB - tGray) * kDitherSaturation / 100;
          if (tR < 0) tR = 0; else if (tR > 255) tR = 255;
          if (tG < 0) tG = 0; else if (tG > 255) tG = 255;
          if (tB < 0) tB = 0; else if (tB > 255) tB = 255;
        }
        if (kDitherRedGain != 100 || kDitherGreenGain != 100 || kDitherBlueGain != 100) {
          tR = (tR * kDitherRedGain) / 100;
          tG = (tG * kDitherGreenGain) / 100;
          tB = (tB * kDitherBlueGain) / 100;
          if (tR < 0) tR = 0; else if (tR > 255) tR = 255;
          if (tG < 0) tG = 0; else if (tG > 255) tG = 255;
          if (tB < 0) tB = 0; else if (tB > 255) tB = 255;
        }
        uint32_t tBestDist = UINT32_MAX;
        uint8_t tBestIdx = 1;
        for (uint8_t tI = 0; tI < 6; tI++) {
          int32_t tDr = tR - static_cast<int32_t>(kPalette888[tI][0]);
          int32_t tDg = tG - static_cast<int32_t>(kPalette888[tI][1]);
          int32_t tDb = tB - static_cast<int32_t>(kPalette888[tI][2]);
          uint32_t tDist = static_cast<uint32_t>(tDr * tDr + tDg * tDg + tDb * tDb);
          if (tDist < tBestDist) { tBestDist = tDist; tBestIdx = tI; }
        }
        SetFrameBufferPixel(kStartX + tX, kStartY + tY, kColorCode[tBestIdx]);
        int16_t tErrR = static_cast<int16_t>(tR - static_cast<int32_t>(kPalette888[tBestIdx][0]));
        int16_t tErrG = static_cast<int16_t>(tG - static_cast<int32_t>(kPalette888[tBestIdx][1]));
        int16_t tErrB = static_cast<int16_t>(tB - static_cast<int32_t>(kPalette888[tBestIdx][2]));
        if (tX + 1 < kWidth) {
          int32_t tIdx = (tCurrRow * kWidth + tX + 1) * 3;
          tErrBuf[tIdx] += (tErrR * 7) >> 4;
          tErrBuf[tIdx + 1] += (tErrG * 7) >> 4;
          tErrBuf[tIdx + 2] += (tErrB * 7) >> 4;
        }
        if (tX > 0) {
          int32_t tIdx = (tNextRow * kWidth + tX - 1) * 3;
          tErrBuf[tIdx] += (tErrR * 3) >> 4;
          tErrBuf[tIdx + 1] += (tErrG * 3) >> 4;
          tErrBuf[tIdx + 2] += (tErrB * 3) >> 4;
        }
        {
          int32_t tIdx = (tNextRow * kWidth + tX) * 3;
          tErrBuf[tIdx] += (tErrR * 5) >> 4;
          tErrBuf[tIdx + 1] += (tErrG * 5) >> 4;
          tErrBuf[tIdx + 2] += (tErrB * 5) >> 4;
        }
        if (tX + 1 < kWidth) {
          int32_t tIdx = (tNextRow * kWidth + tX + 1) * 3;
          tErrBuf[tIdx] += (tErrR * 1) >> 4;
          tErrBuf[tIdx + 1] += (tErrG * 1) >> 4;
          tErrBuf[tIdx + 2] += (tErrB * 1) >> 4;
        }
      }
    }
    heap_caps_free(tErrBuf);
  }

  bool Display_::PrintJpg(int32_t tX, int32_t tY, const char *tFileName) {
    struct STaskData {
      int32_t X;
      int32_t Y;
      char FileName[128];
      char ImagesDir[96];
      SemaphoreHandle_t Done;
      bool Success;
    };
    if (!tFileName || tFileName[0] == '\0') return false;
    {
      Guard tLock;
      if (!mFrameBuffer) return false;
    }
    SemaphoreHandle_t tDoneSemaphore = xSemaphoreCreateBinary();
    if (!tDoneSemaphore) return false;
    STaskData *tTaskData = new STaskData{};
    tTaskData->X = tX;
    tTaskData->Y = tY;
    tTaskData->Done = tDoneSemaphore;
    tTaskData->Success = false;
    {
      Guard tLock;
      snprintf(tTaskData->FileName, sizeof(tTaskData->FileName), "%s", tFileName);
      snprintf(tTaskData->ImagesDir, sizeof(tTaskData->ImagesDir), "%s", mCfg.ImagesDir.c_str());
    }
    BaseType_t tCreateResult = xTaskCreate([](void *tParameter) {
      STaskData *tData = static_cast<STaskData*>(tParameter);
      Display_ &tSelf = Instance();
      char tFullPath[192] = "";
      snprintf(tFullPath, sizeof(tFullPath), "/%s/%s", tData->ImagesDir, tData->FileName);
      File tFile = LittleFS.open(tFullPath, FILE_READ);
      if (!tFile && SDC.IsMounted()) tFile = SD.open(tFullPath, FILE_READ);
      if (tFile) {
        size_t tSize = tFile.size();
        if (tSize == 0 || tSize > static_cast<size_t>(INT32_MAX)) {
          tFile.close();
          xSemaphoreGive(tData->Done);
          vTaskDelete(nullptr);
          return;
        }
        constexpr size_t kJpegReadPadding = 16;
        size_t tAllocSize = tSize + kJpegReadPadding;
        if (tAllocSize < tSize) {
          tFile.close();
          xSemaphoreGive(tData->Done);
          vTaskDelete(nullptr);
          return;
        }
        uint8_t *tBuffer = static_cast<uint8_t*>(heap_caps_malloc(tAllocSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (tBuffer) {
          memset(tBuffer + tSize, 0, kJpegReadPadding);
          size_t tRead = tFile.readBytes(reinterpret_cast<char*>(tBuffer), tSize);
          tFile.close();
          if (tRead == tSize) {
            JPEGDEC tJpeg;
            if (tJpeg.openRAM(tBuffer, static_cast<int>(tSize), &tSelf.JpegDrawCallback)) {
              tJpeg.setPixelType(RGB8888);
              Display_::Lock();
              if (tSelf.mFrameBuffer) {
                int32_t tCanvasWidth = tSelf.GetCanvasWidthUnsafe();
                int32_t tCanvasHeight = tSelf.GetCanvasHeightUnsafe();
                int16_t tDrawX = (tData->X < 0) ? static_cast<int16_t>((tCanvasWidth - tJpeg.getWidth()) / 2) : static_cast<int16_t>(tData->X);
                int16_t tDrawY = (tData->Y < 0) ? static_cast<int16_t>((tCanvasHeight - tJpeg.getHeight()) / 2) : static_cast<int16_t>(tData->Y);
                int32_t tAreaStartX = std::max<int32_t>(0, tDrawX);
                int32_t tAreaStartY = std::max<int32_t>(0, tDrawY);
                int32_t tAreaEndX = std::min<int32_t>(tCanvasWidth, static_cast<int32_t>(tDrawX) + tJpeg.getWidth());
                int32_t tAreaEndY = std::min<int32_t>(tCanvasHeight, static_cast<int32_t>(tDrawY) + tJpeg.getHeight());
                int32_t tAreaWidth = tAreaEndX - tAreaStartX;
                int32_t tAreaHeight = tAreaEndY - tAreaStartY;
                if (tAreaWidth <= 0 || tAreaHeight <= 0) {
                  tData->Success = true;
                } else {
                  size_t tRgbSize = static_cast<size_t>(tAreaWidth * tAreaHeight * 3);
                  uint8_t *tRgbBuf = static_cast<uint8_t*>(heap_caps_malloc(tRgbSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                  if (tRgbBuf) {
                    SJpegDitherCtx tDitherCtx { tRgbBuf, tCanvasWidth, tCanvasHeight, tAreaStartX, tAreaStartY, tAreaWidth, tAreaHeight };
                    tJpeg.setUserPointer(&tDitherCtx);
                    EPaperDriver_::ClearImage(static_cast<uint16_t>(tSelf.mBgColor));
                    tJpeg.decode(tDrawX, tDrawY, 0);
                    tSelf.ApplyFloydSteinberg(tDitherCtx);
                    tData->Success = true;
                    heap_caps_free(tRgbBuf);
                  }
                }
              }
              Display_::Unlock();
              tJpeg.close();
            }
          }
          heap_caps_free(tBuffer);
        } else tFile.close();
      }
      xSemaphoreGive(tData->Done);
      vTaskDelete(nullptr);
    }, "JpgDecode", JPEG_DECODE_TASK_STACK_SIZE, tTaskData, 5, nullptr);
    if (tCreateResult != pdPASS) {
      vSemaphoreDelete(tDoneSemaphore);
      delete tTaskData;
      return false;
    }
    bool tSuccess = false;
    if (xSemaphoreTake(tDoneSemaphore, portMAX_DELAY) == pdTRUE) tSuccess = tTaskData->Success;
    vSemaphoreDelete(tDoneSemaphore);
    delete tTaskData;
    if (tSuccess) Update();
    return tSuccess;
  }
    
  void Display_::Update() {
    struct SDisplayUpdateTaskData {
      uint8_t *FrameBuffer = nullptr;
      SemaphoreHandle_t Done = nullptr;
      bool Success = false;
    };
    uint8_t *tFrameBuffer = nullptr;
    {
      Guard tLock;
      if (!mFrameBuffer) return;
      tFrameBuffer = mFrameBuffer;
    }
    xLOG("Display update start");
    uint32_t tStartMs = millis();
    SemaphoreHandle_t tDoneSemaphore = xSemaphoreCreateBinary();
    if (!tDoneSemaphore) {
      xLOG("Display update failed: semaphore alloc failed");
      return;
    }
    SDisplayUpdateTaskData *tTaskData = new SDisplayUpdateTaskData{};
    if (!tTaskData) {
      vSemaphoreDelete(tDoneSemaphore);
      xLOG("Display update failed: task data alloc failed");
      return;
    }
    tTaskData->FrameBuffer = tFrameBuffer;
    tTaskData->Done = tDoneSemaphore;
    BaseType_t tCreateResult = xTaskCreate([](void *tParameter) {
      SDisplayUpdateTaskData *tData = static_cast<SDisplayUpdateTaskData*>(tParameter);
      if (tData && tData->FrameBuffer) {
        EPaperDriver_::Display(tData->FrameBuffer);
        tData->Success = true;
      }
      if (tData && tData->Done) xSemaphoreGive(tData->Done);
      vTaskDelete(nullptr);
    }, "DisplayUpd", 4096, tTaskData, 5, nullptr);
    if (tCreateResult != pdPASS) {
      vSemaphoreDelete(tDoneSemaphore);
      delete tTaskData;
      xLOG("Display update failed: task create failed");
      return;
    }
    constexpr TickType_t kDisplayUpdateTimeoutTicks = pdMS_TO_TICKS(65000);
    bool tCompleted = (xSemaphoreTake(tDoneSemaphore, kDisplayUpdateTimeoutTicks) == pdTRUE);
    bool tSuccess = tCompleted && tTaskData->Success;
    if (tCompleted) {
      vSemaphoreDelete(tDoneSemaphore);
      delete tTaskData;
    }
    uint32_t tElapsedMs = millis() - tStartMs;
    if (!tCompleted) {
      xLOG("Display update timeout after %lu.%03lus", static_cast<unsigned long>(tElapsedMs / 1000), static_cast<unsigned long>(tElapsedMs % 1000));
      return;
    }
    if (!tSuccess) {
      xLOG("Display update failed after %lu.%03lus", static_cast<unsigned long>(tElapsedMs / 1000), static_cast<unsigned long>(tElapsedMs % 1000));
      return;
    }
    xLOG("Display update end → %lu.%03lus", static_cast<unsigned long>(tElapsedMs / 1000), static_cast<unsigned long>(tElapsedMs % 1000));
  }

  void Display_::ClearArea(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight) {
    Guard tLock;
    if (!mFrameBuffer || tWidth <= 0 || tHeight <= 0) return;
    int32_t tCanvasWidth = GetCanvasWidthUnsafe();
    int32_t tCanvasHeight = GetCanvasHeightUnsafe();
    int32_t tStartX = ClampToRange(tX, 0, tCanvasWidth - 1);
    int32_t tStartY = ClampToRange(tY, 0, tCanvasHeight - 1);
    int32_t tEndX = ClampToRange(tX + tWidth, 0, tCanvasWidth);
    int32_t tEndY = ClampToRange(tY + tHeight, 0, tCanvasHeight);
    if (tEndX <= tStartX || tEndY <= tStartY) return;
    EPaperDriver_::ClearWindows(static_cast<uint16_t>(tStartX), static_cast<uint16_t>(tStartY), static_cast<uint16_t>(tEndX), static_cast<uint16_t>(tEndY), static_cast<uint16_t>(mBgColor));
  }

}

