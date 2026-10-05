// game.cpp —— 主框架 / 输入 / 对话框系统
#include "game.hpp"
#include <android/log.h>
#include <android/asset_manager.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "YUME"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

Ctx G;
static AAssetManager* s_am = 0;
AAssetManager* gameAM(void) { return s_am; }
static int s_keyDown[B_COUNT];

// ===================== 工具 =====================
void txt(const char* s, float x, float by, float sc, float r, float g, float b, float a) {
    gfxText(G.A.font, s, x, by, sc, r, g, b, a);
}
float txtW(const char* s, float sc) { return gfxTextW(G.A.font, s, sc); }
void boxRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    gfxSolid(x, y, w, h, r, g, b, a);
}
void frameRect(float x, float y, float w, float h, float th, float a) {
    gfxSolid(x, y, w, th, 1, 1, 1, a);
    gfxSolid(x, y + h - th, w, th, 1, 1, 1, a);
    gfxSolid(x, y, th, h, 1, 1, 1, a);
    gfxSolid(x + w - th, y, th, h, 1, 1, 1, a);
}

// ===================== 输入 =====================
void uiLayout(void) {
    float u = G.ui; if (u <= 0.0f) u = 1.0f;
    float aw  = 132.0f * u;
    float amr = 86.0f * u;
    float amb = 150.0f * u;
    for (int i = 0; i < B_COUNT; i++) { G.btn[i].w = 0; G.btn[i].h = 0; }
    if (G.scene == SC_DREAM) {
        G.btn[B_JUMP].x = G.sw - aw - amr; G.btn[B_JUMP].y = G.sh - aw - amb;
        G.btn[B_JUMP].w = aw; G.btn[B_JUMP].h = aw;
    } else {
        G.btn[B_ACT].x = G.sw - aw - amr; G.btn[B_ACT].y = G.sh - aw - amb;
        G.btn[B_ACT].w = aw; G.btn[B_ACT].h = aw;
    }
    for (int i = 0; i < B_COUNT; i++) G.btn[i].down = 0;
}

// ---- 虚拟摇杆 ----
static void stickUpdate(void) {
    float u = G.ui; if (u <= 0.0f) u = 1.0f;
    float R = 152.0f * u;
    G.stR = R;
    if (!G.stickActive) {
        G.stBaseX = 74.0f * u + R;
        G.stBaseY = G.sh - 74.0f * u - R;
        G.stKnobX = G.stBaseX; G.stKnobY = G.stBaseY;
        G.axX = G.axY = 0;
    }
    int idx = -1;
    if (G.stickId >= 0) {
        for (int i = 0; i < G.np; i++) if (G.pid[i] == G.stickId) { idx = i; break; }
        if (idx < 0) { G.stickId = -1; G.stickActive = 0; G.axX = G.axY = 0; return; }
    }
    if (G.stickId < 0) {
        for (int i = 0; i < G.np; i++) {
            if (G.px[i] < G.sw * 0.5f && G.py[i] > G.sh * 0.22f) {
                G.stickId = G.pid[i]; G.stickActive = 1;
                G.stBaseX = G.px[i]; G.stBaseY = G.py[i];
                G.stKnobX = G.stBaseX; G.stKnobY = G.stBaseY;
                idx = i; break;
            }
        }
    }
    if (idx < 0) { G.axX = G.axY = 0; return; }
    float dx = G.px[idx] - G.stBaseX, dy = G.py[idx] - G.stBaseY;
    float len = sqrtf(dx * dx + dy * dy);
    float cl = (len > R) ? (R / len) : 1.0f;
    G.stKnobX = G.stBaseX + dx * cl;
    G.stKnobY = G.stBaseY + dy * cl;
    float nx = (dx * cl) / R, ny = (dy * cl) / R;
    const float DZ = 0.20f;
    float m = sqrtf(nx * nx + ny * ny);
    if (m < DZ) { nx = ny = 0; }
    else {
        float sc = (m - DZ) / (1.0f - DZ) / m;
        nx *= sc; ny *= sc;
        float m2 = sqrtf(nx * nx + ny * ny);
        if (m2 > 1.0f) { nx /= m2; ny /= m2; }
    }
    G.axX = nx; G.axY = ny;
}

float inputX(void) {
    float v = G.axX;
    if (s_keyDown[B_L]) v -= 1.0f;
    if (s_keyDown[B_R]) v += 1.0f;
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}
float inputY(void) {
    float v = G.axY;
    if (s_keyDown[B_U]) v -= 1.0f;
    if (s_keyDown[B_D]) v += 1.0f;
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}

int btnDown(int id) { return G.btn[id].down; }
int btnHit(int id)  { return G.btn[id].down && !G.btn[id].prev; }
int anyTap(float* x, float* y) {
    int r = G.anyDown && !G.anyPrev;
    if (r) { if (x) *x = G.tapX; if (y) *y = G.tapY; }
    return r;
}

static int inRect(const BtnState& b, float x, float y) {
    return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
}

void uiDrawButtons(void) {
    if (!G.showBtns) return;
    float R = G.stR;
    if (R > 0.0f && G.A.ui.id) {
        float bs = R * 2.0f;
        gfxSprite(G.A.ui, 128, 128, 2, 0, G.stBaseX - R, G.stBaseY - R, bs, bs, 1, 1, 1, G.stickActive ? 0.95f : 0.42f, 0);
        float kr = R * 0.60f;
        gfxSprite(G.A.ui, 128, 128, 2, 1, G.stKnobX - kr, G.stKnobY - kr, kr * 2.0f, kr * 2.0f, 1, 1, 1, G.stickActive ? 0.98f : 0.50f, 0);
    }
    int id = (G.scene == SC_DREAM) ? B_JUMP : B_ACT;
    BtnState& b = G.btn[id];
    if (b.w > 0.0f && G.A.ui.id) {
        gfxSprite(G.A.ui, 128, 128, 2, b.down ? 3 : 2, b.x, b.y, b.w, b.h, 1, 1, 1, 0.95f, 0);
        const char* lab = (id == B_JUMP) ? "^" : "!";
        float sc = G.ui * 0.95f;
        float w = txtW(lab, sc);
        txt(lab, b.x + (b.w - w) * 0.5f, b.y + b.h * 0.5f + 16 * G.ui, sc, 1, 1, 1, 0.95f);
    }
}

// ===================== 对话数据 =====================
struct DLine { char spk[40]; char por[20]; char txt[300]; };
struct DNode { char key[40]; int first, cnt; };
#define MAXDL 400
#define MAXDN 128
static DLine DL[MAXDL];
static DNode DN[MAXDN];
static int nDL = 0, nDN = 0;

static void trim(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r')) s[--n] = 0;
}

void dlgLoad(void) {
    nDL = 0; nDN = 0;
    AAsset* a = AAssetManager_open(s_am, "dialogue.txt", AASSET_MODE_BUFFER);
    if (!a) { LOGE("dialogue.txt missing"); return; }
    size_t len = (size_t)AAsset_getLength(a);
    char* buf = (char*)malloc(len + 1);
    AAsset_read(a, buf, len);
    buf[len] = 0;
    AAsset_close(a);
    char* p = buf;
    while (*p && nDL < MAXDL) {
        char* e = strchr(p, '\n');
        size_t ll = e ? (size_t)(e - p) : strlen(p);
        char line[512];
        if (ll > 511) ll = 511;
        memcpy(line, p, ll); line[ll] = 0;
        trim(line);
        if (line[0] && line[0] != '#') {
            char* b1 = strchr(line, '|');
            if (b1) {
                char* b2 = strchr(b1 + 1, '|');
                if (b2) {
                    char* b3 = strchr(b2 + 1, '|');
                    if (b3) {
                        *b1 = 0; *b2 = 0; *b3 = 0;
                        char* k = line;      trim(k);
                        char* sp = b1 + 1;   trim(sp);
                        char* po = b2 + 1;   trim(po);
                        char* tx = b3 + 1;   trim(tx);
                        strncpy(DL[nDL].spk, sp, 39); DL[nDL].spk[39] = 0;
                        strncpy(DL[nDL].por, po, 19); DL[nDL].por[19] = 0;
                        strncpy(DL[nDL].txt, tx, 299); DL[nDL].txt[299] = 0;
                        // 节点归并
                        if (nDN > 0 && strcmp(DN[nDN-1].key, k) == 0) DN[nDN-1].cnt++;
                        else if (nDN < MAXDN) {
                            strncpy(DN[nDN].key, k, 39); DN[nDN].key[39] = 0;
                            DN[nDN].first = nDL; DN[nDN].cnt = 1; nDN++;
                        }
                        nDL++;
                    }
                }
            }
        }
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    LOGI("dialogue: %d lines / %d nodes", nDL, nDN);
}

// ===================== 对话运行 =====================
static int   dOn = 0, dIdx = 0, dNode = -1, dAfter = 0;
static float dAnim = 0, dChars = 0;
static uint32_t cps[320]; static int ncp = 0;
static int lstart[24]; static int nline = 1;
static const float CPS_SPEED = 30.0f;

static int portraitIdx(const char* k) {
    if (!k || !k[0] || strcmp(k, "none") == 0) return -1;
    if (strcmp(k, "colleague") == 0) return 0;
    if (strcmp(k, "him") == 0) return 1;
    if (strcmp(k, "cat") == 0) return 2;
    if (strcmp(k, "rabbit") == 0) return 3;
    return -1;
}

static void dlgBox(float* bx, float* by, float* bw, float* bh) {
    float u = G.ui;
    *bw = G.sw - 110 * u;
    *bh = 250 * u;
    *bx = 55 * u;
    *by = G.sh - *bh - 34 * u;
}

static void dlgPrepare(void) {
    if (dNode < 0) return;
    DLine& L = DL[DN[dNode].first + dIdx];
    ncp = 0;
    int i = 0; uint32_t cp;
    while (utf8Next(L.txt, &i, &cp) && ncp < 319) cps[ncp++] = cp;
    float u = G.ui;
    float sc = 0.95f * u;
    float bx, by, bw, bh;
    dlgBox(&bx, &by, &bw, &bh);
    int port = portraitIdx(L.por);
    float padL = 46 * u + (port >= 0 ? 226 * u : 0);
    float maxw = bw - padL - 46 * u;
    nline = 1; lstart[0] = 0;
    float acc = 0;
    for (int k = 0; k < ncp; k++) {
        float a = gfxGlyphAdv(G.A.font, cps[k], sc);
        if (acc + a > maxw && k > lstart[nline-1] && nline < 23) {
            lstart[nline++] = k; acc = a;
        } else acc += a;
    }
    dChars = 0;
}

void dlgStart(const char* node, int after) {
    dNode = -1;
    for (int i = 0; i < nDN; i++) if (strcmp(DN[i].key, node) == 0) { dNode = i; break; }
    if (dNode < 0) { LOGI("!! 对话节点缺失: %s", node); return; }
    dOn = 1; dIdx = 0; dAnim = 0; dAfter = after;
    dlgPrepare();
    LOGI("dlg: %s (%d lines)", node, DN[dNode].cnt);
}

int dlgActive(void) { return dOn; }

void dlgUpdate(float dt) {
    if (!dOn) return;
    if (dAnim < 1.0f) dAnim += dt * 5.5f;
    if (dAnim > 1.0f) dAnim = 1.0f;
    if (dChars < (float)ncp) {
        dChars += CPS_SPEED * dt;
        if (dChars > (float)ncp) dChars = (float)ncp;
    }
}

void dlgAdvance(void) {
    if (!dOn) return;
    if (dAnim < 0.9f) return;
    if (dChars < (float)ncp) { dChars = (float)ncp; return; }
    dIdx++;
    if (dIdx >= DN[dNode].cnt) {
        dOn = 0;
        int a = dAfter; dAfter = AFT_NONE; dNode = -1;
        if (a == AFT_SLEEP) sceneGo(SC_DREAM);
        else if (a == AFT_WAKE) sceneGo(SC_ROOM);
        else if (a == AFT_VOID_END) sceneGo(SC_ROOM);
        return;
    }
    dlgPrepare();
}

static float dBoxY = 0;

void dlgDraw(void) {
    if (!dOn || dNode < 0) return;
    float u = G.ui;
    float bx, by, bw, bh;
    dlgBox(&bx, &by, &bw, &bh);
    float e = dAnim * dAnim * (3.0f - 2.0f * dAnim);   // smoothstep
    float slide = (1.0f - e) * (bh + 60 * u);
    float alpha = e;
    float x = bx, y = by + slide;
    boxRect(x, y, bw, bh, 0, 0, 0, 0.86f * alpha);
    frameRect(x, y, bw, bh, 3.0f * u, 0.55f * alpha);
    DLine& L = DL[DN[dNode].first + dIdx];
    int port = portraitIdx(L.por);
    float padL = 46 * u;
    if (port >= 0) {
        float ps = 200 * u;
        float pxp = x + 26 * u, pyp = y - ps + 46 * u;
        boxRect(pxp - 6 * u, pyp - 6 * u, ps + 12 * u, ps + 12 * u, 0, 0, 0, 0.9f * alpha);
        frameRect(pxp - 6 * u, pyp - 6 * u, ps + 12 * u, ps + 12 * u, 3.0f * u, 0.6f * alpha);
        gfxSprite(G.A.portraits, 192, 192, 2, port, pxp, pyp, ps, ps, 1, 1, 1, alpha, 0);
        padL = 26 * u + ps + 34 * u;
    }
    // 名字标签
    float ns = 0.82f * u;
    if (L.spk[0]) {
        float nw = txtW(L.spk, ns) + 44 * u;
        float nx = x + padL - 12 * u, ny = y - 46 * u;
        boxRect(nx, ny, nw, 46 * u, 0.10f, 0.10f, 0.10f, 0.92f * alpha);
        frameRect(nx, ny, nw, 46 * u, 2.0f * u, 0.5f * alpha);
        txt(L.spk, nx + 22 * u, ny + 32 * u, ns, 1, 1, 1, 0.96f * alpha);
    }
    // 正文 (流式)
    float sc = 0.95f * u;
    float lh = 60 * u * 1.28f;
    float tx = x + padL;
    float ty = y + 74 * u;
    int shown = (int)dChars;
    for (int k = 0; k < ncp && k < shown; k++) {
        int li = nline - 1;
        for (int j = 0; j < nline; j++) if (k >= lstart[j]) li = j;
        float px = tx;
        for (int j = 0; j < k; j++) {
            int lj = nline - 1;
            for (int q = 0; q < nline; q++) if (j >= lstart[q]) lj = q;
            if (lj == li) px += gfxGlyphAdv(G.A.font, cps[j], sc);
        }
        char one[8]; int oi = 0;
        uint32_t cp = cps[k];
        if (cp < 0x80) one[oi++] = (char)cp;
        else if (cp < 0x800) { one[oi++] = (char)(0xC0 | (cp >> 6)); one[oi++] = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { one[oi++] = (char)(0xE0 | (cp >> 12)); one[oi++] = (char)(0x80 | ((cp >> 6) & 0x3F)); one[oi++] = (char)(0x80 | (cp & 0x3F)); }
        else { one[oi++] = (char)(0xF0 | (cp >> 18)); one[oi++] = (char)(0x80 | ((cp >> 12) & 0x3F)); one[oi++] = (char)(0x80 | ((cp >> 6) & 0x3F)); one[oi++] = (char)(0x80 | (cp & 0x3F)); }
        one[oi] = 0;
        txt(one, px, ty + li * lh, sc, 0.96f, 0.96f, 0.96f, 0.97f * alpha);
    }
    // 继续标记
    if (dChars >= (float)ncp && dAnim > 0.95f) {
        float bl = 0.5f + 0.5f * sinf(G.t * 5.0f);
        float mx = x + bw - 40 * u, my = y + bh - 34 * u;
        boxRect(mx, my, 16 * u, 16 * u, 1, 1, 1, 0.25f + 0.6f * bl);
    }
    dBoxY = y;
}

void dlgBar(float x, float y, float w) { (void)x; (void)y; (void)w; }

// ===================== 主循环 =====================
static void switchScene(int id) {
    G.scene = id;
    if (id == SC_ROOM) roomEnter(0);
    else if (id == SC_DREAM) dreamEnter();
    else if (id == SC_VOID) voidEnter();
}

void sceneGo(int id) {
    if (G.fadeDir != 0) return;
    G.fadeDir = 1; G.fadeTo = id;
    LOGI("scene -> %d", id);
}

char g_dataDir[256] = {0};

void gameInit(AAssetManager* am) {
    s_am = am;
    memset(&G, 0, sizeof(G));
    gfxInit(am);
    G.A.room      = gfxLoad("room.png", 0);
    G.A.human     = gfxLoad("human.png", 0);
    G.A.blocks    = gfxLoad("blocks.png", 0);
    G.A.portraits = gfxLoad("portraits.png", 0);
    G.A.dreambg   = gfxLoad("dreambg.png", 1);
    G.A.voidbg    = gfxLoad("voidbg.png", 1);
    G.A.ui        = gfxLoad("ui.png", 1);
    G.A.font      = gfxLoadFont("font.png", "fontmap.txt");
    dlgLoad();
    roomEnter(0);
    LOGI("gameInit done");
}

void gameResize(int sw, int sh) {
    G.sw = (float)sw; G.sh = (float)sh;
    G.ui = (float)sh / 1080.0f;
    LOGI("resize %dx%d ui=%.3f", sw, sh, G.ui);
}

void gamePointers(const float* xy, const int* ids, int n) {
    G.np = n > 16 ? 16 : n;
    for (int i = 0; i < G.np; i++) {
        G.px[i] = xy[i*2]; G.py[i] = xy[i*2+1];
        G.pid[i] = ids ? ids[i] : i;
    }
    G.anyDown = (G.np > 0);
    if (G.anyDown && !G.anyPrev) { G.tapX = (int)G.px[0]; G.tapY = (int)G.py[0]; }
}

void gameUpdate(float dt) {
    G.t += dt;
    G.showBtns = dlgActive() ? 0 : 1;
    uiLayout();
    if (dlgActive()) { G.stickId = -1; G.stickActive = 0; G.axX = G.axY = 0; }
    stickUpdate();
    for (int i = 0; i < B_COUNT; i++) {
        G.btn[i].down = s_keyDown[i];
        if (G.btn[i].w <= 0.0f) continue;
        for (int p = 0; p < G.np; p++)
            if (inRect(G.btn[i], G.px[p], G.py[p])) { G.btn[i].down = 1; break; }
    }

    if (G.fadeDir == 1) {
        G.fade += dt * 3.4f;
        if (G.fade >= 1.0f) { G.fade = 1.0f; G.fadeDir = -1; switchScene(G.fadeTo); }
    } else if (G.fadeDir == -1) {
        G.fade -= dt * 3.4f;
        if (G.fade <= 0.0f) { G.fade = 0.0f; G.fadeDir = 0; }
    }

    if (dlgActive()) {
        dlgUpdate(dt);
        if (anyTap(0, 0)) dlgAdvance();
    } else if (G.fadeDir == 0) {
        if (G.scene == SC_ROOM) roomUpdate(dt);
        else if (G.scene == SC_DREAM) dreamUpdate(dt);
        else voidUpdate(dt);
    }

    for (int i = 0; i < B_COUNT; i++) G.btn[i].prev = G.btn[i].down;
    G.anyPrev = G.anyDown;
}

void gameRender(void) {
    gfxFrame((int)G.sw, (int)G.sh);
    glClearColor(0.03f, 0.03f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (G.fade > 0.0f) { /* 保留 */ }
    if (G.scene == SC_ROOM) roomDraw();
    else if (G.scene == SC_DREAM) dreamDraw();
    else voidDraw();
    uiDrawButtons();
    dlgDraw();
    if (G.fade > 0.0f) boxRect(0, 0, G.sw, G.sh, 0, 0, 0, G.fade);
    gfxFlush();
}

// ---- 输入映射层: 一个动作绑多个物理键(参考 HND 移植版的 Gamepad.handleKeyEvent) ----
struct KeyBind { int key; int btn; };
static const KeyBind KEYBINDS[] = {
    { 21, B_L    }, { 29, B_L    },
    { 22, B_R    }, { 32, B_R    },
    { 19, B_U    }, { 51, B_U    },
    { 20, B_D    }, { 47, B_D    },
    { 23, B_ACT  }, { 54, B_ACT  }, { 66, B_ACT  }, { 33, B_ACT },
    { 62, B_JUMP }, { 52, B_JUMP }, { 99, B_JUMP }
};
#define NKEYBINDS ((int)(sizeof(KEYBINDS)/sizeof(KEYBINDS[0])))
void gameKey(int code, int down) {
    for (int i = 0; i < NKEYBINDS; i++) {
        if (KEYBINDS[i].key == code) {
            s_keyDown[KEYBINDS[i].btn] = down;
            if (down && dlgActive()) dlgAdvance();
            return;
        }
    }
    if (code == 111 && down && dlgActive()) dlgAdvance();   // ESC
}
