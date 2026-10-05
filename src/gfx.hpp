// gfx.hpp —— 精灵批处理渲染层 + 中日文字库
#pragma once
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <stdint.h>

struct Tex { GLuint id; int w, h; };

// 点阵字库: 每字一格, 固定基线, 逐字步进
struct CJKFont {
    Tex tex;
    int cellW, cellH, base;
    int n;
    uint32_t* cps;   // 升序 codepoint
    float*    x0;    // 左斜距
    float*    adv;   // 步进
    uint8_t*  col;
    uint8_t*  row;
};

void gfxInit(AAssetManager* am);
Tex  gfxLoad(const char* name, int linear);
Tex  gfxWhite(void);
void gfxFrame(int sw, int sh);

void gfxSprite(const Tex& t, int fw, int fh, int cols, int idx,
               float x, float y, float w, float h,
               float r, float g, float b, float a, int flip = 0);

void gfxBg(const Tex& t, float x, float y, float w, float h,
           float u0, float v0, float u1, float v1,
           float r, float g, float b, float a);

void gfxSolid(float x, float y, float w, float h,
              float r, float g, float b, float a);

void gfxFlush(void);
int  gfxDrawCalls(void);

// ---- 文字 ----
CJKFont gfxLoadFont(const char* png, const char* mapfile);
int   utf8Next(const char* s, int* i, uint32_t* cp);
float gfxGlyphAdv(const CJKFont& f, uint32_t cp, float scale);
void  gfxText(const CJKFont& f, const char* utf8, float x, float baselineY,
              float scale, float r, float g, float b, float a);
float gfxTextW(const CJKFont& f, const char* utf8, float scale);
