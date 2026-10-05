// gfx.cpp —— 批处理渲染 + 中日文点阵字库
#include "gfx.hpp"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TAG "YUME"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static AAssetManager* s_am = 0;
static GLuint s_prog = 0, s_vbo = 0, s_vao = 0, s_cur = 0;
static GLint  s_uScreen = -1, s_uTex = -1;
static int    s_sw = 0, s_sh = 0, s_calls = 0;
static float* s_v = 0;
static int    s_n = 0, s_cap = 0;
static Tex    s_white = {0, 1, 1};
static unsigned char s_wpx[4] = {255, 255, 255, 255};

static const char* VS =
"#version 300 es\n"
"layout(location=0) in vec2 aPos;\n"
"layout(location=1) in vec2 aUV;\n"
"layout(location=2) in vec4 aCol;\n"
"uniform vec2 uScreen;\n"
"out vec2 vUV; out vec4 vCol;\n"
"void main(){ vUV=aUV; vCol=aCol;\n"
"  vec2 n=vec2(aPos.x/uScreen.x*2.0-1.0, 1.0-aPos.y/uScreen.y*2.0);\n"
"  gl_Position=vec4(n,0.0,1.0); }\n";

static const char* FSH =
"#version 300 es\n"
"precision mediump float;\n"
"in vec2 vUV; in vec4 vCol; uniform sampler2D uTex; out vec4 o;\n"
"void main(){ o = texture(uTex,vUV) * vec4(vCol.rgb*vCol.a, vCol.a); }\n";

static GLuint mksh(GLenum t, const char* s) {
    GLuint o = glCreateShader(t);
    glShaderSource(o, 1, &s, 0);
    glCompileShader(o);
    GLint ok = 0; glGetShaderiv(o, GL_COMPILE_STATUS, &ok);
    if (!ok) { char l[1024]; glGetShaderInfoLog(o, 1024, 0, l); LOGE("shader: %s", l); }
    return o;
}

void gfxInit(AAssetManager* am) {
    s_am = am;
    GLuint v = mksh(GL_VERTEX_SHADER, VS), f = mksh(GL_FRAGMENT_SHADER, FSH);
    s_prog = glCreateProgram();
    glAttachShader(s_prog, v); glAttachShader(s_prog, f);
    glLinkProgram(s_prog);
    GLint ok = 0; glGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    if (!ok) { char l[1024]; glGetProgramInfoLog(s_prog, 1024, 0, l); LOGE("link: %s", l); }
    glDeleteShader(v); glDeleteShader(f);
    s_uScreen = glGetUniformLocation(s_prog, "uScreen");
    s_uTex = glGetUniformLocation(s_prog, "uTex");
    glGenVertexArrays(1, &s_vao); glBindVertexArray(s_vao);
    glGenBuffers(1, &s_vbo); glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    const GLsizei S = 8 * sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, S, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, S, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, S, (void*)(4 * sizeof(float)));
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    s_cap = 16384;
    s_v = (float*)malloc((size_t)s_cap * 8 * sizeof(float));
    LOGI("gfxInit ok GL=%s", (const char*)glGetString(GL_VERSION));
}

static char* readAsset(const char* name, size_t* outLen) {
    AAsset* a = AAssetManager_open(s_am, name, AASSET_MODE_BUFFER);
    if (!a) { LOGE("asset missing: %s", name); return 0; }
    size_t len = (size_t)AAsset_getLength(a);
    char* buf = (char*)malloc(len + 1);
    int got = (int)AAsset_read(a, buf, len);
    AAsset_close(a);
    if (got <= 0) { free(buf); return 0; }
    buf[len] = 0;
    if (outLen) *outLen = len;
    return buf;
}

static void premul(unsigned char* p, int px) {
    for (int i = 0; i < px; i += 4) {
        unsigned a = p[i + 3];
        p[i]     = (unsigned char)(p[i]     * a / 255);
        p[i + 1] = (unsigned char)(p[i + 1] * a / 255);
        p[i + 2] = (unsigned char)(p[i + 2] * a / 255);
    }
}

Tex gfxLoad(const char* name, int linear) {
    Tex t; t.id = 0; t.w = 0; t.h = 0;
    size_t len = 0;
    char* buf = readAsset(name, &len);
    if (!buf) return t;
    int w = 0, h = 0, ch = 0;
    unsigned char* px = stbi_load_from_memory((const unsigned char*)buf, (int)len, &w, &h, &ch, 4);
    free(buf);
    if (!px) { LOGE("decode fail: %s", name); return t; }
    premul(px, w * h);
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    t.w = w; t.h = h;
    LOGI("tex %-14s %dx%d %s", name, w, h, linear ? "LIN" : "NEAR");
    return t;
}

Tex gfxWhite(void) {
    if (!s_white.id) {
        glGenTextures(1, &s_white.id);
        glBindTexture(GL_TEXTURE_2D, s_white.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_wpx);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        s_white.w = 1; s_white.h = 1;
    }
    return s_white;
}

void gfxFrame(int sw, int sh) { s_sw = sw; s_sh = sh; s_n = 0; s_calls = 0; }

void gfxFlush(void) {
    if (s_n == 0) return;
    glUseProgram(s_prog);
    glUniform2f(s_uScreen, (float)s_sw, (float)s_sh);
    glUniform1i(s_uTex, 0);
    glActiveTexture(GL_TEXTURE0);
    if (s_cur) glBindTexture(GL_TEXTURE_2D, s_cur);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)s_n * 8 * sizeof(float)), s_v, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, s_n);
    s_calls++;
    s_n = 0;
}

static void useTex(GLuint id) { if (id != s_cur) { gfxFlush(); s_cur = id; } }

static void grow(int add) {
    if (s_n + add <= s_cap) return;
    while (s_n + add > s_cap) s_cap *= 2;
    s_v = (float*)realloc(s_v, (size_t)s_cap * 8 * sizeof(float));
}
static void vpush(float x, float y, float u, float v, float r, float g, float b, float a) {
    float* p = s_v + (size_t)s_n * 8;
    p[0] = x; p[1] = y; p[2] = u; p[3] = v; p[4] = r; p[5] = g; p[6] = b; p[7] = a;
    s_n++;
}
static void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                 float r, float g, float b, float a) {
    grow(6);
    float x1 = x + w, y1 = y + h;
    vpush(x, y, u0, v0, r, g, b, a);
    vpush(x1, y, u1, v0, r, g, b, a);
    vpush(x1, y1, u1, v1, r, g, b, a);
    vpush(x, y, u0, v0, r, g, b, a);
    vpush(x1, y1, u1, v1, r, g, b, a);
    vpush(x, y1, u0, v1, r, g, b, a);
}

void gfxSprite(const Tex& t, int fw, int fh, int cols, int idx,
               float x, float y, float w, float h,
               float r, float g, float b, float a, int flip) {
    if (!t.id) return;
    useTex(t.id);
    int fx = (idx % cols) * fw;
    int fy = (idx / cols) * fh;
    float u0 = ((float)fx + 0.5f) / (float)t.w;
    float u1 = ((float)fx + (float)fw - 0.5f) / (float)t.w;
    float v0 = ((float)fy + 0.5f) / (float)t.h;
    float v1 = ((float)fy + (float)fh - 0.5f) / (float)t.h;
    if (flip) { float tm = u0; u0 = u1; u1 = tm; }
    quad(x, y, w, h, u0, v0, u1, v1, r, g, b, a);
}

void gfxBg(const Tex& t, float x, float y, float w, float h,
           float u0, float v0, float u1, float v1,
           float r, float g, float b, float a) {
    if (!t.id) return;
    useTex(t.id);
    quad(x, y, w, h, u0, v0, u1, v1, r, g, b, a);
}

void gfxSolid(float x, float y, float w, float h, float r, float g, float b, float a) {
    Tex t = gfxWhite();
    useTex(t.id);
    quad(x, y, w, h, 0.5f, 0.5f, 0.5f, 0.5f, r, g, b, a);
}

int gfxDrawCalls(void) { return s_calls; }

// ==================== 文字 ====================
int utf8Next(const char* s, int* i, uint32_t* cp) {
    if (!s) return 0;
    int k = *i;
    unsigned char c = (unsigned char)s[k];
    if (!c) return 0;
    if (c < 0x80) { *cp = c; *i = k + 1; return 1; }
    if ((c & 0xE0) == 0xC0) { *cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[k+1] & 0x3F); *i = k + 2; return 1; }
    if ((c & 0xF0) == 0xE0) { *cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[k+1] & 0x3F) << 6) | (uint32_t)(s[k+2] & 0x3F); *i = k + 3; return 1; }
    if ((c & 0xF8) == 0xF0) { *cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[k+1] & 0x3F) << 12) | ((uint32_t)(s[k+2] & 0x3F) << 6) | (uint32_t)(s[k+3] & 0x3F); *i = k + 4; return 1; }
    *i = k + 1; *cp = '?'; return 1;
}

static int findGlyph(const CJKFont& f, uint32_t cp) {
    int lo = 0, hi = f.n - 1;
    while (lo <= hi) {
        int m = (lo + hi) >> 1;
        if (f.cps[m] == cp) return m;
        if (f.cps[m] < cp) lo = m + 1; else hi = m - 1;
    }
    return -1;
}

CJKFont gfxLoadFont(const char* png, const char* mapfile) {
    CJKFont f;
    memset(&f, 0, sizeof(f));
    f.cellW = 56; f.cellH = 60; f.base = 46;
    f.tex = gfxLoad(png, 0);
    size_t len = 0;
    char* txt = readAsset(mapfile, &len);
    if (!txt) return f;
    int cap = 640, n = 0;
    f.cps = (uint32_t*)malloc(cap * 4);
    f.x0  = (float*)malloc(cap * 4);
    f.adv = (float*)malloc(cap * 4);
    f.col = (uint8_t*)malloc(cap);
    f.row = (uint8_t*)malloc(cap);
    char* p = txt;
    while (*p) {
        if (*p == '#') {
            const char* cw = strstr(p, "cell=");
            const char* bs = strstr(p, "base=");
            if (cw) sscanf(cw, "cell=%dx%d", &f.cellW, &f.cellH);
            if (bs) sscanf(bs, "base=%d", &f.base);
            while (*p && *p != '\n') p++;
            continue;
        }
        if (*p == '\n' || *p == '\r' || *p == ' ') { p++; continue; }
        unsigned cp = 0; int c = 0, r = 0; float dx = 0, a = 0;
        if (sscanf(p, "%x %d %d %f %f", &cp, &c, &r, &dx, &a) == 5) {
            if (n >= cap) {
                cap *= 2;
                f.cps = (uint32_t*)realloc(f.cps, cap * 4);
                f.x0  = (float*)realloc(f.x0, cap * 4);
                f.adv = (float*)realloc(f.adv, cap * 4);
                f.col = (uint8_t*)realloc(f.col, cap);
                f.row = (uint8_t*)realloc(f.row, cap);
            }
            f.cps[n] = cp; f.col[n] = (uint8_t)c; f.row[n] = (uint8_t)r;
            f.x0[n] = dx; f.adv[n] = a; n++;
        }
        while (*p && *p != '\n') p++;
    }
    free(txt);
    f.n = n;
    LOGI("font %s  %d glyphs  cell=%dx%d base=%d  tex=%dx%d",
         png, n, f.cellW, f.cellH, f.base, f.tex.w, f.tex.h);
    return f;
}

float gfxGlyphAdv(const CJKFont& f, uint32_t cp, float s) {
    int i = findGlyph(f, cp);
    if (i < 0) return (float)f.cellW * 0.45f * s;
    return f.adv[i] * s;
}

void gfxText(const CJKFont& f, const char* str, float x, float baselineY, float s,
             float r, float g, float b, float a) {
    if (!str || !f.tex.id) return;
    int cols = f.tex.w / f.cellW;
    if (cols < 1) cols = 1;
    float pen = x;
    int i = 0; uint32_t cp;
    while (utf8Next(str, &i, &cp)) {
        int gi = findGlyph(f, cp);
        if (gi >= 0) {
            int idx = f.row[gi] * cols + f.col[gi];
            gfxSprite(f.tex, f.cellW, f.cellH, cols, idx,
                      pen + f.x0[gi] * s - s, baselineY - (float)f.base * s,
                      (float)f.cellW * s, (float)f.cellH * s, r, g, b, a, 0);
        }
        pen += gfxGlyphAdv(f, cp, s);
    }
}

float gfxTextW(const CJKFont& f, const char* str, float s) {
    if (!str) return 0;
    float pen = 0;
    int i = 0; uint32_t cp;
    while (utf8Next(str, &i, &cp)) pen += gfxGlyphAdv(f, cp, s);
    return pen;
}
