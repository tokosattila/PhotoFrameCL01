#ifndef DISPLAY_H
#define DISPLAY_H

#include <App/Global.h>

extern "C" size_t tinfl_decompress_mem_to_mem(void *tOutputBuffer, size_t tOutputBufferLength, const void *tSourceBuffer, size_t tSourceBufferLength, int tFlags);

namespace App {

  enum class EDisplayColor : uint8_t {
    Black = EpaperColorBlack,
    Gray = EpaperColorBlack,
    White = EpaperColorWhite,
    LightGray = EpaperColorWhite,
    Yellow = EpaperColorYellow,
    Red = EpaperColorRed,
    Blue = EpaperColorBlue,
    Green = EpaperColorGreen
  };

  struct STextBounds {
    int32_t X = 0;
    int32_t Y = 0;
    int32_t Width = 0;
    int32_t Height = 0;
  };

  struct SBoxBounds {
    int32_t XStartPos = 0;
    int32_t YStartPos = 0;
    int32_t XEndPos = 0;
    int32_t YEndPos = 0;
  };

  enum class EDisplayHAlignment : uint8_t {
    Left = 1,
    Right,
    Center
  };

  enum class EDisplayVAlignment : uint8_t {
    Auto = 1,
    Center
  };

  enum class EDisplayRotate : uint16_t {
    Rotate0 = 0,
    Rotate90 = 90,
    Rotate180 = 180,
    Rotate270 = 270
  };

  class Display_ {
    DEFINE_TAG("DSP");
    friend class AutoGuard<Display_>;
    public:
      using Guard = AutoGuard<Display_>;
      static Display_ &Instance();
      static void Lock();
      static void Unlock();
      void Init();
      void ReloadConfig();
      void DeInit();
      void SetFont(const GFXfont *tFont);
      void SetRotate(EDisplayRotate tRotate = static_cast<EDisplayRotate>(0));
      int32_t GetCanvasWidth() const;
      int32_t GetCanvasHeight() const;
      void SetBgColor(EDisplayColor tColor);
      void SetColor(EDisplayColor tColor);
      void On();
      void Off();
      void OffAll();
      void FillRect(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, EDisplayColor tColor);
      void DrawRect(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, EDisplayColor tColor);
      void DrawCircle(int32_t tXcenter, int32_t tYcenter, int32_t tRadius, EDisplayColor tColor);
      void FillCircle(int32_t tXcenter, int32_t tYcenter, int32_t tRadius, EDisplayColor tColor);
      STextBounds WriteText(int32_t tX, int32_t tY, const char *tText, EDisplayHAlignment tHAlignment = EDisplayHAlignment::Left, EDisplayVAlignment tVAlignment = EDisplayVAlignment::Auto, EDisplayColor tBgColor = EDisplayColor::White);
      void WriteTextCentered(const char *tText, EDisplayColor tBgColor = EDisplayColor::White);
      SBoxBounds WriteTextWithBoxCentered(const char *tText, int32_t tXPadding = 12, int32_t tYPadding = 8, EDisplayColor tBoxColor = EDisplayColor::Black);
      void PrintImage(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight, const uint8_t *tData);
      bool PrintJpg(int32_t tX, int32_t tY, const char *tFileName);
      void Update();
      void ClearArea(int32_t tX, int32_t tY, int32_t tWidth, int32_t tHeight);
    private:
      Display_();
      Display_(const Display_&) = delete;
      Display_ &operator=(const Display_&) = delete;
      ~Display_();
      static constexpr int kTinflZlibHeaderFlag = 1;
      static constexpr int32_t kGlyphRenderScaleDivisor = 2;
      static constexpr uint8_t kGlyphCoverageOnThreshold = 7;
      mutable SemaphoreHandle_t mMutex = nullptr;
      SDisplayConfig mCfg {};
      EDisplayRotate mDisplayRotate = static_cast<EDisplayRotate>(0);
      EDisplayColor mBgColor = EDisplayColor::White;
      EDisplayColor mColor = EDisplayColor::Black;
      const GFXfont *mGfxFont = nullptr;
      uint8_t *mFrameBuffer = nullptr;
      uint8_t mJpgToneLut5[32] = {};
      uint8_t mJpgToneLut6[64] = {};
      uint8_t mJpgToneLutBrightness = 0xFF;
      uint8_t mJpgToneLutContrast = 0xFF;
      uint8_t mJpgToneLutGamma = 0xFF;
      bool mJpgToneLutReady = false;      
      static int32_t ClampToRange(int32_t tValue, int32_t tMinimum, int32_t tMaximum);
      static int32_t MaxOf(int32_t tLeft, int32_t tRight);
      static int32_t DivideFloor(int32_t tValue, int32_t tDivisor);
      static int32_t DivideCeil(int32_t tValue, int32_t tDivisor);
      static const GFXglyph *FindGlyphForCodePoint(const GFXfont *tFont, uint16_t tCodePoint);
      static bool DecodeGlyphBitmapData(const GFXfont *tFont, const GFXglyph *tGlyph, std::vector<uint8_t> &tOutput);
      static uint8_t ReadGlyphCoverageNibble(const std::vector<uint8_t> &tData, uint16_t tWidth, uint16_t tX, uint16_t tY);
      static STextBounds MeasureGfxTextBounds(const GFXfont *tFont, const char *tText);
      static uint8_t NormalizeToSpectra6Color(uint8_t tColor);
      static int JpegDrawCallback(JPEGDRAW *tDraw);
      void EnsureJpgToneLut();
      int32_t GetCanvasWidthUnsafe() const;
      int32_t GetCanvasHeightUnsafe() const;
      bool MapLogicalToPhysical(int32_t tX, int32_t tY, int32_t &tMappedX, int32_t &tMappedY) const;
      void SetFrameBufferPixel(int32_t tX, int32_t tY, uint8_t tColor);
  };

}

#endif
