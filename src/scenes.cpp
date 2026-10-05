// scenes.cpp —— 房间(俯视) / 梦境(自动向右) / 空想时间世界
#include "game.hpp"
#include <stdio.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "YUME"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ============================================================
//  房间 (俯视, 固定相机)
// ============================================================
#define RW 23
#define RH 11
static const float RT = 48.0f;
static const char* ROOM[RH] = {
    "WWWWWWWWKKWWWWWWWWWWWWW",
    "W............CC.......W",
    "W.SSS.......DDDD......W",
    "W.SSS.......DDDD......W",
    "W.....................W",
    "W...N.................W",
    "W.BBB.................W",
    "W.BBB.................W",
    "W.BBB.................W",
    "W..........X..M.....TPW",
    "WWWWWWWWWWOOWWWWWWWWWWW"
};

typedef struct { int tx, ty; const char* node; int after; int idx; float r; } EntDef;
static const EntDef EDEF[] = {
    { 13,  1, "room.pc",     AFT_NONE,  0, 118 },
    {  3,  2, "room.shelf",  AFT_NONE,  0, 110 },
    {  3,  7, "room.bed",    AFT_SLEEP, 0, 130 },
    {  8,  0, "room.window", AFT_NONE,  0, 140 },
    { 20,  9, "room.bin",    AFT_NONE,  0,  95 },
    { 21,  9, "room.plant",  AFT_NONE,  0,  95 },
    { 14,  9, "room.phone",  AFT_NONE,  0,  95 },
    { 10, 10, "room.door",   AFT_NONE,  0, 120 },
    {  4,  5, "room.npc",    AFT_NONE,  1, 105 }
};
#define NEDEF ((int)(sizeof(EDEF)/sizeof(EDEF[0])))

typedef struct { float x, y, r; const char* node; int after; int idx; } Ent;
static Ent ent[NEDEF];
static int nEnt = 0;
static float rpx, rpy;
static int   rdir, rstep;
static float rstepT;
static int   morning = 0;
static int   wakePending = 0;

// ---- 昼夜状态: chaos 混沌 / clarity 清醒 (持久化) ----
static float chaos = 0, clarity = 100;
static int   day = 1, stateLoaded = 0;
static int   nightHP = 3, nightMaxHP = 3, nightDeaths = 0, nightDmg = 0;
static float dInv = 0, dHitT = 0, awakenT = 0;
static void stateSave(void) {
    if (!g_dataDir[0]) return;
    char p[512]; snprintf(p, sizeof(p), "%s/yume.sav", g_dataDir);
    FILE* f = fopen(p, "w"); if (!f) return;
    fprintf(f, "day=%d", day); fputc(10, f);
    fprintf(f, "chaos=%.1f", chaos); fputc(10, f);
    fprintf(f, "clarity=%.1f", clarity); fputc(10, f);
    fclose(f);
}
static void stateLoad(void) {
    if (stateLoaded || !g_dataDir[0]) return;
    stateLoaded = 1;
    char p[512]; snprintf(p, sizeof(p), "%s/yume.sav", g_dataDir);
    FILE* f = fopen(p, "r"); if (!f) return;
    char ln[128];
    while (fgets(ln, sizeof(ln), f)) {
        float v;
        if (sscanf(ln, "chaos=%f", &v) == 1) chaos = v;
        else if (sscanf(ln, "clarity=%f", &v) == 1) clarity = v;
        else if (sscanf(ln, "day=%f", &v) == 1) day = (int)v;
    }
    fclose(f);
    if (chaos < 0 || chaos > 100) chaos = 0;
    if (clarity < 0 || clarity > 100) clarity = 100;
    LOGI("state load day=%d chaos=%.0f clarity=%.0f", day, chaos, clarity);
}
static float sDispClr = 100.0f, sDispDel = 0.0f;
static float sBarLastT = -1.0f;
static void drawStateBar(const char* label, float val, float* disp, int delirium) {
    float u = G.ui; if (u <= 0.0f) u = 1.0f;
    float dt = (sBarLastT < 0.0f) ? 0.016f : G.t - sBarLastT;
    sBarLastT = G.t;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.06f) dt = 0.06f;
    float d = val - *disp, mv = 140.0f * dt;
    if (fabsf(d) <= mv) *disp = val; else *disp += (d > 0.0f) ? mv : -mv;
    float t = *disp / 100.0f;
    float lr = delirium ? (0.92f + 0.03f * t) : 0.64f;
    float lg = delirium ? (0.92f - 0.64f * t) : 0.90f;
    float lb = delirium ? (0.90f - 0.64f * t) : 0.62f;
    float bw = 168.0f * u, bh = 40.0f * u;
    float bx = G.sw - bw - 16.0f * u;
    float by = G.sh - 330.0f * u;
    boxRect(bx, by, bw, bh, 0.05f, 0.06f, 0.09f, 0.62f);
    boxRect(bx, by, 3.0f * u, bh, 0.90f, 0.90f, 0.95f, 0.85f);
    txt(label, bx + 10.0f * u, by + 15.0f * u, 0.40f * u, lr, lg, lb, 0.92f);
    int lit = (int)(*disp / 10.0f + 0.5f);
    if (lit < 0) lit = 0;
    if (lit > 10) lit = 10;
    for (int i = 0; i < 10; i++) {
        float s = 12.0f * u;
        float x = bx + 10.0f * u + i * 14.0f * u;
        float y = by + 21.0f * u;
        if (i < lit) boxRect(x, y, s, s, lr, lg, lb, 0.95f);
        else frameRect(x, y, s, s, 1.2f * u, 0.30f);
    }
}

static float rscale(void) { return G.sh / 528.0f; }
static void roomOrigin(float* ox, float* oy) {
    float sc = rscale();
    *ox = (G.sw - RW * RT * sc) * 0.5f;
    *oy = (G.sh - RH * RT * sc) * 0.5f;
}
static char roomTile(int cx, int cy) {
    if (cx < 0 || cx >= RW || cy < 0 || cy >= RH) return 'W';
    return ROOM[cy][cx];
}
static int roomSolid(char c) {
    return c == 'W' || c == 'S' || c == 'D' || c == 'C' || c == 'B' || c == 'T' || c == 'P';
}
static int roomBoxSolid(float x, float y, float w, float h) {
    int x0 = (int)floorf(x / RT), x1 = (int)floorf((x + w - 0.01f) / RT);
    int y0 = (int)floorf(y / RT), y1 = (int)floorf((y + h - 0.01f) / RT);
    for (int cy = y0; cy <= y1; cy++)
        for (int cx = x0; cx <= x1; cx++)
            if (roomSolid(roomTile(cx, cy))) return 1;
    return 0;
}
static void roomMove(float dx, float dy) {
    float bx = rpx - 19.0f, by = rpy - 36.0f;
    const float st = 6.0f;
    float rem = dx;
    while (fabsf(rem) > 0.01f) {
        float d = (rem > 0) ? (rem < st ? rem : st) : (rem > -st ? rem : -st);
        if (!roomBoxSolid(bx + d, by, 38, 36)) { bx += d; rpx += d; }
        rem -= d;
    }
    rem = dy;
    while (fabsf(rem) > 0.01f) {
        float d = (rem > 0) ? (rem < st ? rem : st) : (rem > -st ? rem : -st);
        if (!roomBoxSolid(bx, by + d, 38, 36)) { by += d; rpy += d; }
        rem -= d;
    }
}

void roomEnter(int m) {
    (void)m;
    int wake = wakePending;
    stateLoad();
    morning = wake ? 1 : 0;
    nEnt = 0;
    for (int y = 0; y < RH; y++)
        for (int x = 0; x < RW; x++)
            if (ROOM[y][x] == 'X') { rpx = x * RT + RT * 0.5f; rpy = y * RT + RT * 0.92f; }
    for (int i = 0; i < NEDEF; i++) {
        ent[i].x = EDEF[i].tx * RT + RT * 0.5f;
        ent[i].y = EDEF[i].ty * RT + RT * 0.5f;
        ent[i].r = EDEF[i].r;
        ent[i].node = EDEF[i].node;
        ent[i].after = EDEF[i].after;
        ent[i].idx = EDEF[i].idx;
    }
    nEnt = NEDEF;
    rdir = 0; rstep = 0; rstepT = 0;
    G.showBtns = 1;
    LOGI("room enter morning=%d ents=%d", morning, nEnt);
    if (wake) {
        wakePending = 0;
        if (nightDmg == 0 && nightDeaths == 0) { clarity = 100.0f; chaos -= (chaos > 8.0f ? 8.0f : chaos); }
        else { clarity = 100.0f - chaos; if (clarity < 20.0f) clarity = 20.0f; }
        day++;
        nightDeaths = 0; nightDmg = 0;
        stateSave();
        LOGI("morning day=%d clarity=%.0f chaos=%.0f", day, clarity, chaos);
        dlgStart("room.wake", AFT_NONE);
    }
}

void roomUpdate(float dt) {
    float vx = inputX(), vy = inputY();
    if (fabsf(vx) < 0.22f) vx = 0;
    { float sp = 0.78f + 0.22f * (clarity / 100.0f); vx *= sp; vy *= sp; }
    if (fabsf(vy) < 0.22f) vy = 0;
    if (vx != 0 && vy != 0) { vx *= 0.7071f; vy *= 0.7071f; }
    if (vx != 0 || vy != 0) {
        roomMove(vx * 205.0f * dt, vy * 205.0f * dt);
        rstepT += dt;
        if (rstepT > 0.17f) { rstepT = 0; rstep ^= 1; }
        if (fabsf(vx) > fabsf(vy)) rdir = (vx > 0) ? 3 : 2;
        else rdir = (vy > 0) ? 0 : 1;
    } else rstep = 0;
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < nEnt; i++) {
        float dx = ent[i].x - rpx, dy = ent[i].y - rpy;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < ent[i].r && d < bd) { bd = d; best = i; }
    }
    if (best >= 0 && btnHit(B_ACT)) {
        LOGI("interact: %s", ent[best].node);
        dlgStart(ent[best].node, ent[best].after);
    }
}

void roomDraw(void) {
    float sc = rscale(), ox, oy;
    roomOrigin(&ox, &oy);
    boxRect(0, 0, G.sw, G.sh, 0.02f, 0.02f, 0.03f, 1);
    for (int y = 0; y < RH; y++)
        for (int x = 0; x < RW; x++) {
            char c = ROOM[y][x];
            int idx = -1;
            if (c == 'W') idx = 2;
            else if (c == 'K') idx = 10;
            else if (c == '.') idx = ((x + y) & 1) ? 1 : 0;
            else if (c == 'D') idx = 4;
            else if (c == 'C') idx = 5;
            else if (c == 'S') idx = 8;
            else if (c == 'O') idx = 9;
            else if (c == 'T') idx = 14;
            else if (c == 'P') idx = 11;
            else if (c == 'M') idx = 13;
            else if (c == 'B') idx = (y < 8) ? 6 : 7;
            if (idx >= 0) {
                float sx = ox + x * RT * sc, sy = oy + y * RT * sc;
                gfxSprite(G.A.room, 48, 48, 8, idx, sx, sy, RT * sc, RT * sc, 1, 1, 1, 1, 0);
            }
        }
    for (int i = 0; i < nEnt; i++)
        if (ent[i].idx > 0) {
            float sx = ox + ent[i].x * sc, sy = oy + ent[i].y * sc;
            boxRect(sx - 22 * sc, sy + 22 * sc, 44 * sc, 10 * sc, 0, 0, 0, 0.30f);
            gfxSprite(G.A.human, 48, 64, 8, (ent[i].idx * 4) * 2, sx - 24 * sc, sy - 40 * sc, 48 * sc, 64 * sc, 1, 1, 1, 1, 0);
        }
    float px = ox + rpx * sc, py = oy + rpy * sc;
    boxRect(px - 20 * sc, py - 10 * sc, 40 * sc, 10 * sc, 0, 0, 0, 0.32f);
    gfxSprite(G.A.human, 48, 64, 8, (rdir * 2) + rstep, px - 24 * sc, py - 62 * sc, 48 * sc, 64 * sc, 1, 1, 1, 1, 0);
    for (int i = 0; i < nEnt; i++) {
        float dx = ent[i].x - rpx, dy = ent[i].y - rpy;
        if (sqrtf(dx * dx + dy * dy) < ent[i].r) {
            float sx = ox + ent[i].x * sc, sy = oy + ent[i].y * sc;
            float bb = 4.0f * sinf(G.t * 4.0f);
            boxRect(sx - 9 * sc, sy - 78 * sc + bb, 18 * sc, 24 * sc, 1, 1, 1, 0.95f);
            boxRect(sx - 4 * sc, sy - 52 * sc + bb, 8 * sc, 8 * sc, 1, 1, 1, 0.95f);
            break;
        }
    }
    if (morning) boxRect(0, 0, G.sw, G.sh, 1, 1, 0.94f, 0.07f);
    drawStateBar("清醒度", clarity, &sDispClr, 0);
}

// ============================================================
//  梦境 (自动向右滚动)
// ============================================================
#define DW 128
#define DH 16
static char dmap[DH][DW + 2];
static int  dW = 0, dH = 0;
static const float DRT = 64.0f;
static const float SCROLL = 118.0f;
static const float DBX = 40.0f, DBY = 44.0f;
static float dpx, dpy, dvx, dvy;
static int   dGround, dFace, dStep;
static float dStepT, dCamX, dCamY, dShake, dGrace;
#define START_CAMX (-230.0f)
#define GRACE_TIME (2.2f)
static int   dSeenCat = 0, dSeenRab = 0;
static float dSpawnX, dSpawnY;
static float ckX[24], ckY[24];
static int   nCk = 0;

static float dscale(void) { return G.sh / 540.0f; }
static float dViewW(void) { return G.sw / dscale(); }

static char dTile(int cx, int cy) {
    if (cy < 0 || cy >= dH || cx < 0 || cx >= dW) return '.';
    return dmap[cy][cx];
}
static int dBoxSolid(float x, float y, float w, float h) {
    int x0 = (int)floorf(x / DRT), x1 = (int)floorf((x + w - 0.01f) / DRT);
    int y0 = (int)floorf(y / DRT), y1 = (int)floorf((y + h - 0.01f) / DRT);
    for (int cy = y0; cy <= y1; cy++)
        for (int cx = x0; cx <= x1; cx++)
            if (dTile(cx, cy) == '#') return 1;
    return 0;
}

void dreamEnter(void) {
    AAsset* a = AAssetManager_open(gameAM(), "dream1.txt", AASSET_MODE_BUFFER);
    if (!a) { LOGE("dream1.txt missing"); return; }
    size_t len = (size_t)AAsset_getLength(a);
    char* buf = (char*)malloc(len + 1);
    AAsset_read(a, buf, len);
    buf[len] = 0;
    AAsset_close(a);
    int y = 0, sx = -1, sy = -1;
    nCk = 0;
    char* p = buf;
    while (*p && y < DH) {
        char* e = strchr(p, '\n');
        size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll && p[0] != ';') {
            size_t m = 0;
            for (size_t j = 0; j < ll && m < DW; j++) {
                char c = p[j];
                if (c == '\r') continue;
                if (c == 'P') { sx = (int)m; sy = y; c = '.'; }
                if (c == 'S' && nCk < 24) { ckX[nCk] = (float)m * DRT + 19.0f; ckY[nCk] = (float)y * DRT + 14.0f; nCk++; c = '.'; }
                dmap[y][m++] = c;
            }
            dmap[y][m] = 0;
            if ((int)m > dW) dW = (int)m;
            y++;
        }
        if (!e) break;
        p = e + 1;
    }
    free(buf);
    dH = y;
    dpx = (sx > 0 ? sx : 2) * DRT + 12;
    dpy = (sy > 0 ? sy : 9) * DRT + DRT - DBY;
    dvx = dvy = 0; dFace = 1; dCamX = START_CAMX; dShake = 0; dGround = 1;
    dGrace = GRACE_TIME;
    dSpawnX = dpx; dSpawnY = dpy;
    nightMaxHP = 3; nightHP = nightMaxHP;
    nightDeaths = 0; nightDmg = 0; dInv = 0; dHitT = 0; awakenT = 0;
    if (!stateLoaded) stateLoad();
    dCamY = (float)dH * DRT - 540.0f;
    if (dCamY < 0) dCamY = 0;
    LOGI("dream enter %dx%d start=(%d,%d)", dW, dH, sx, sy);
}

static void dreamDie(void) {
    LOGI("dream die at x=%.0f", dpx);
    dShake = 0.30f;
    dpx = dSpawnX; dpy = dSpawnY;
    dvx = dvy = 0;
    dCamX = dSpawnX - 320.0f; if (dCamX < START_CAMX) dCamX = START_CAMX;
    dGround = 1;
    dGrace = GRACE_TIME;
    nightDeaths++;
    chaos += 10.0f; if (chaos > 100.0f) chaos = 100.0f;
    nightHP = nightMaxHP;
    if (nightDeaths >= 5 && awakenT <= 0.0f) { awakenT = 2.6f; LOGI("awakening trigger deaths=%d chaos=%.0f", nightDeaths, chaos); }
}

void dreamUpdate(float dt) {
    if (awakenT > 0.0f) {
        awakenT -= dt;
        if (awakenT <= 0.0f) {
            clarity = 100.0f - chaos; if (clarity < 20.0f) clarity = 20.0f;
            day++;
            nightDeaths = 0; nightDmg = 0;
            stateSave();
            wakePending = 1;
            sceneGo(SC_ROOM);
            LOGI("awakening done day=%d clarity=%.0f", day, clarity);
        }
        return;
    }
    const float GRAV = 1900.0f, JUMPV = 880.0f, SPD = 340.0f;
    float want = (dHitT > 0.0f || awakenT > 0.0f) ? 0.0f : inputX();
    if (fabsf(want) < 0.25f) want = 0;
    if (want != 0) dFace = (want > 0) ? 1 : -1;
    float target = want * SPD;
    if (dvx < target) { dvx += 2400 * dt; if (dvx > target) dvx = target; }
    else if (dvx > target) { dvx -= 2400 * dt; if (dvx < target) dvx = target; }
    if (btnHit(B_JUMP) && dGround) { dvy = -JUMPV; dGround = 0; }
    if (!btnDown(B_JUMP) && dvy < -300.0f) dvy = -300.0f;
    dvy += GRAV * dt;
    if (dvy > 1300) dvy = 1300;

    const float st = 4.0f;
    float rem = dvx * dt;
    while (fabsf(rem) > 0.01f) {
        float d = (rem > 0) ? (rem < st ? rem : st) : (rem > -st ? rem : -st);
        if (dBoxSolid(dpx + d, dpy, DBX, DBY)) { dvx = 0; break; }
        dpx += d;
        rem -= d;
    }
    rem = dvy * dt;
    while (fabsf(rem) > 0.01f) {
        float d = (rem > 0) ? (rem < st ? rem : st) : (rem > -st ? rem : -st);
        if (dBoxSolid(dpx, dpy + d, DBX, DBY)) { dvy = 0; break; }
        dpy += d;
        rem -= d;
    }
    dGround = dBoxSolid(dpx, dpy + 2.0f, DBX, DBY);
    if (dInv > 0.0f) dInv -= dt;
    if (dHitT > 0.0f) dHitT -= dt;
    dStepT += dt;
    if (dStepT > 0.12f) { dStepT = 0; dStep ^= 1; }

    int x0 = (int)floorf(dpx / DRT), x1 = (int)floorf((dpx + DBX - 0.01f) / DRT);
    int y0 = (int)floorf(dpy / DRT), y1 = (int)floorf((dpy + DBY - 0.01f) / DRT);
    for (int cy = y0; cy <= y1; cy++)
        for (int cx = x0; cx <= x1; cx++) {
            char c = dTile(cx, cy);
            if (c == '^') { dreamDie(); return; }
            if (c == 'D') { LOGI("dream door reached"); sceneGo(SC_VOID); return; }
        }
    for (int cy = 0; cy < dH; cy++)
        for (int cx = 0; cx < dW; cx++) {
            char c = dmap[cy][cx];
            if (c != 'e' && c != 'E') continue;
            float ex, ey;
            if (c == 'e') { ex = cx * DRT + 12 + sinf(G.t * 1.1f + cx) * 96.0f; ey = cy * DRT + DRT - DBY; }
            else          { ex = cx * DRT + 12; ey = cy * DRT + 4 + sinf(G.t * 2.0f + cx) * 42.0f; }
            if (dInv <= 0.0f && dpx + DBX > ex && dpx < ex + 40 && dpy + DBY > ey && dpy < ey + 44) {
                nightHP--; nightDmg++;
                chaos += 6.0f; if (chaos > 100.0f) chaos = 100.0f;
                dInv = 1.0f; dHitT = 0.25f;
                dvy = -520.0f; dGround = 0;
                dvx = (dpx + DBX * 0.5f < ex + 20.0f) ? -280.0f : 280.0f;
                LOGI("enemy hit hp=%d chaos=%.0f", nightHP, chaos);
                if (nightHP <= 0) { dreamDie(); return; }
            }
        }

    for (int i = 0; i < nCk; i++) {
        if (dpx + DBX > ckX[i] && dpx < ckX[i] + 26.0f && dpy + DBY > ckY[i] && dpy < ckY[i] + 34.0f) {
            if (dSpawnX != ckX[i] || dSpawnY != ckY[i]) { dSpawnX = ckX[i]; dSpawnY = ckY[i]; LOGI("checkpoint set col %.0f", ckX[i] / DRT); }
        }
    }
    if (dGrace > 0.0f) dGrace -= dt;
    else {
        dCamX += SCROLL * dt;
        float maxCam = (float)dW * DRT - dViewW();
        if (maxCam < 0) maxCam = 0;
        if (dCamX > maxCam) dCamX = maxCam;
    }
    if (dGrace <= 0.0f && dpx + DBX < dCamX + 10.0f) {
        float nx = dCamX + 10.0f - DBX;
        if (dBoxSolid(nx, dpy, DBX, DBY)) { dreamDie(); return; }
        dpx = nx;
    }
    if (dpy > (float)dH * DRT + 120) { dreamDie(); return; }
    dCamY = (float)dH * DRT - 540.0f;
    if (dCamY < 0) dCamY = 0;
    if (dShake > 0) dShake -= dt;

    if (!dSeenCat && dpx > 15 * DRT) { dSeenCat = 1; dlgStart("dream.cat", AFT_NONE); }
    if (!dSeenRab && dpx > 43 * DRT) { dSeenRab = 1; dlgStart("dream.rabbit", AFT_NONE); }
}

void dreamDraw(void) {
    float sc = dscale();
    float shk = (dShake > 0) ? sinf(G.t * 60.0f) * 6.0f * sc : 0;
    if (chaos >= 85.0f) shk += sinf(G.t * 31.0f) * 3.5f * sc;
    else if (chaos >= 60.0f) shk += sinf(G.t * 27.0f) * 1.8f * sc;
    float ts = 540.0f * sc;
    float off = fmodf(dCamX * 0.18f * sc, ts);
    if (off < 0) off += ts;
    for (float x = -off; x < G.sw; x += ts)
        gfxSprite(G.A.dreambg, 256, 256, 1, 0, x, 0, ts, ts, 1, 1, 1, 1, 0);
    int cx0 = (int)floorf(dCamX / DRT) - 1;
    int cx1 = (int)floorf((dCamX + dViewW()) / DRT) + 1;
    for (int cy = 0; cy < dH; cy++)
        for (int cx = cx0; cx <= cx1; cx++) {
            if (cx < 0 || cx >= dW) continue;
            char c = dmap[cy][cx];
            int idx = -1;
            if (c == '#') idx = 8;
            else if (c == '=') idx = 9;
            else if (c == '^') idx = 6;
            else if (c == 'D') idx = 7;
            if (idx < 0) continue;
            float sx = (cx * DRT - dCamX) * sc + shk;
            float sy = (cy * DRT - dCamY) * sc;
            if (idx == 7) boxRect(sx - 46 * sc, sy - 56 * sc, 156 * sc, 156 * sc, 1, 1, 1, 0.09f);
            gfxSprite(G.A.blocks, 64, 64, 8, idx, sx, sy, DRT * sc, DRT * sc, 1, 1, 1, 1, 0);
        }
    for (int i = 0; i < nCk; i++) {
        float bx = (ckX[i] - dCamX) * sc + shk, by = (ckY[i] - dCamY) * sc + sinf(G.t * 2.4f + i * 1.9f) * 5.0f * sc;
        if (bx < -80.0f || bx > G.sw + 80.0f) continue;
        float act = (dSpawnX == ckX[i] && dSpawnY == ckY[i]) ? 1.0f : 0.72f;
        boxRect(bx, by, 26.0f * sc, 34.0f * sc, 0.97f, 0.97f, 0.94f, act);
        boxRect(bx + 5.0f * sc, by + 7.0f * sc, 16.0f * sc, 2.5f * sc, 0.52f, 0.55f, 0.60f, act);
        boxRect(bx + 5.0f * sc, by + 15.0f * sc, 16.0f * sc, 2.5f * sc, 0.52f, 0.55f, 0.60f, act);
        boxRect(bx + 5.0f * sc, by + 23.0f * sc, 10.0f * sc, 2.5f * sc, 0.52f, 0.55f, 0.60f, act);
    }
    for (int cy = 0; cy < dH; cy++)
        for (int cx = 0; cx < dW; cx++) {
            char c = dmap[cy][cx];
            if (c != 'e' && c != 'E') continue;
            float ex, ey;
            if (c == 'e') { ex = cx * DRT + 12 + sinf(G.t * 1.1f + cx) * 96.0f; ey = cy * DRT + DRT - DBY; }
            else          { ex = cx * DRT + 12; ey = cy * DRT + 4 + sinf(G.t * 2.0f + cx) * 42.0f; }
            float sx = (ex - dCamX) * sc + shk, sy = (ey - dCamY) * sc;
            if (c == 'e') boxRect(sx + 4 * sc, sy + 42 * sc, 32 * sc, 6 * sc, 1, 1, 1, 0.12f);
            gfxSprite(G.A.blocks, 64, 64, 8, (c == 'e') ? 1 : 2, sx - 12 * sc, sy - 10 * sc, 64 * sc, 64 * sc, 1, 1, 1, 1, 0);
        }
    {
        float ax = (15 * DRT - dCamX) * sc + shk;
        float ay = (9 * DRT - dCamY) * sc - 10 * sc + sinf(G.t * 1.6f) * 4 * sc;
        gfxSprite(G.A.blocks, 64, 64, 8, 3, ax - 12 * sc, ay - 10 * sc, 64 * sc, 64 * sc, 1, 1, 1, 1, 0);
        float bx2 = (43 * DRT - dCamX) * sc + shk;
        float by2 = (9 * DRT - dCamY) * sc - 12 * sc + sinf(G.t * 1.6f + 2) * 4 * sc;
        gfxSprite(G.A.blocks, 64, 64, 8, 4, bx2 - 12 * sc, by2 - 10 * sc, 64 * sc, 64 * sc, 1, 1, 1, 1, 0);
    }
    float px = (dpx - dCamX) * sc + shk, py = (dpy - dCamY) * sc;
    boxRect(px + 4 * sc, py + 42 * sc, 32 * sc, 6 * sc, 1, 1, 1, 0.12f);
    gfxSprite(G.A.blocks, 64, 64, 8, 0, px - 12 * sc, py - 10 * sc, 64 * sc, 64 * sc, 1, 1, 1, 1, (dFace < 0) ? 1 : 0);
    if (dGrace > 0.0f) {
        float s = 1.7f * G.ui;
        const char* t = "READY";
        float w = txtW(t, s);
        txt(t, (G.sw - w) * 0.5f, G.sh * 0.34f, s, 1, 1, 1, 0.9f);
        float bw = 420.0f * G.ui, bh = 14.0f * G.ui;
        float bx = (G.sw - bw) * 0.5f, by = G.sh * 0.34f + 34.0f * G.ui;
        boxRect(bx, by, bw, bh, 1, 1, 1, 0.20f);
        boxRect(bx, by, bw * (1.0f - dGrace / GRACE_TIME), bh, 1, 1, 1, 0.85f);
    }
    {
        float bw = 168.0f * G.ui, bh = 13.0f * G.ui;
        float bx = G.sw - bw - 24.0f * G.ui, by = 24.0f * G.ui;
        boxRect(bx - 2.0f * G.ui, by - 2.0f * G.ui, bw + 4.0f * G.ui, bh + 4.0f * G.ui, 0, 0, 0, 0.45f);
        float rt = (nightMaxHP > 0) ? (float)nightHP / (float)nightMaxHP : 0.0f;
        if (rt < 0) rt = 0;
        float r2 = rt > 0.4f ? 0.35f : 0.95f, g2 = rt > 0.4f ? 0.85f : 0.30f;
        boxRect(bx, by, bw * rt, bh, r2, g2, 0.30f, 0.92f);
        for (int i = 1; i < nightMaxHP; i++)
            boxRect(bx + bw * (float)i / nightMaxHP - 1.0f * G.ui, by, 2.0f * G.ui, bh, 0, 0, 0, 0.5f);
    }
    drawStateBar("虚幻度", chaos, &sDispDel, 1);
    if (awakenT > 0.0f) {
        float a = (awakenT > 2.1f) ? (2.6f - awakenT) / 0.5f : (awakenT < 1.2f ? awakenT / 1.2f : 1.0f);
        if (a > 1.0f) a = 1.0f;
        boxRect(0, 0, G.sw, G.sh, 1, 1, 1, 0.92f * a);
        const char* t = "AWAKENING";
        float s2 = 1.9f * G.ui;
        float w2 = txtW(t, s2);
        txt(t, (G.sw - w2) * 0.5f, G.sh * 0.5f, s2, 0.08f, 0.08f, 0.10f, a);
    }
    float hs = 0.72f * G.ui;
    txt("DREAM", 24 * G.ui, 46 * G.ui, hs, 1, 1, 1, 0.30f);
}

// ---- 会话存档: 切后台/退出时序列化, 进程重启时恢复 ----
void gameSaveSession(void) {
    if (!g_dataDir[0]) return;
    char p[512]; snprintf(p, sizeof(p), "%s/yume_session.sav", g_dataDir);
    FILE* f = fopen(p, "w"); if (!f) return;
    fprintf(f, "scene=%d", G.scene); fputc(10, f);
    fprintf(f, "rpx=%.1f rpy=%.1f", rpx, rpy); fputc(10, f);
    fprintf(f, "dpx=%.1f dpy=%.1f dCamX=%.1f", dpx, dpy, dCamX); fputc(10, f);
    fprintf(f, "dSpawnX=%.1f dSpawnY=%.1f", dSpawnX, dSpawnY); fputc(10, f);
    fprintf(f, "nightHP=%d nightDeaths=%d nightDmg=%d", nightHP, nightDeaths, nightDmg); fputc(10, f);
    fprintf(f, "dSeenCat=%d dSeenRab=%d", dSeenCat, dSeenRab); fputc(10, f);
    fclose(f);
    stateSave();
    LOGI("session saved scene=%d", G.scene);
}

int gameLoadSession(void) {
    if (!g_dataDir[0]) return 0;
    char p[512]; snprintf(p, sizeof(p), "%s/yume_session.sav", g_dataDir);
    FILE* f = fopen(p, "r"); if (!f) return 0;
    int sc = -1; float vrpx=0, vrpy=0, vdpx=0, vdpy=0, vdcx=0, vdsx=0, vdsy=0;
    int vhp=3, vnd=0, vnmm=0, vsc=0, vsr=0;
    char ln[256];
    while (fgets(ln, sizeof(ln), f)) {
        sscanf(ln, "scene=%d", &sc);
        sscanf(ln, "rpx=%f rpy=%f", &vrpx, &vrpy);
        sscanf(ln, "dpx=%f dpy=%f dCamX=%f", &vdpx, &vdpy, &vdcx);
        sscanf(ln, "dSpawnX=%f dSpawnY=%f", &vdsx, &vdsy);
        sscanf(ln, "nightHP=%d nightDeaths=%d nightDmg=%d", &vhp, &vnd, &vnmm);
        sscanf(ln, "dSeenCat=%d dSeenRab=%d", &vsc, &vsr);
    }
    fclose(f);
    stateLoad();
    if (sc < 0) return 0;
    roomEnter(0);
    if (sc == SC_DREAM) {
        dreamEnter();
        dpx = vdpx; dpy = vdpy; dCamX = vdcx;
        dSpawnX = vdsx; dSpawnY = vdsy;
        nightHP = vhp; nightDeaths = vnd; nightDmg = vnmm;
        dSeenCat = vsc; dSeenRab = vsr;
        dGrace = 1.0f;
        G.scene = SC_DREAM;
    } else if (sc == SC_ROOM) {
        rpx = vrpx; rpy = vrpy;
    }
    LOGI("session restored scene=%d", G.scene);
    return 1;
}

// ============================================================
//  空想时间世界
// ============================================================
static float vpx, vpy, vCamX, vStepT;
static int   vDir, vStep, vState, vTold;
static const float VGROUND = 700.0f;

static float vscale(void) { return G.sh / 540.0f; }

void voidEnter(void) {
    vpx = 160; vpy = VGROUND - DBY; vDir = 1; vState = 0; vTold = 0; vCamX = 0; vStep = 0; vStepT = 0;
    LOGI("void enter");
}

void voidUpdate(float dt) {
    if (vState == 1 && !dlgActive()) {
        if (!vTold) { vTold = 1; dlgStart("void.end", AFT_WAKE); }
        else if (!dlgActive()) { wakePending = 1; sceneGo(SC_ROOM); }
        return;
    }
    float want = inputX();
    if (fabsf(want) < 0.25f) want = 0;
    if (want != 0) {
        vDir = (want > 0) ? 1 : -1;
        vpx += want * 190.0f * dt;
        vStepT += dt;
        if (vStepT > 0.18f) { vStepT = 0; vStep ^= 1; }
    } else vStep = 0;
    if (vpx < 80) vpx = 80;
    if (vpx > 1600) vpx = 1600;
    const float npcX = 1120.0f;
    if (fabsf(vpx - npcX) < 110.0f && btnHit(B_ACT)) {
        LOGI("void talk");
        vState = 1;
        dlgStart("void.meet", AFT_NONE);
    }
    float sc = vscale();
    float tx = vpx - (G.sw / sc) * 0.35f;
    if (tx < 0) tx = 0;
    vCamX += (tx - vCamX) * (1.0f - expf(-6.0f * dt));
}

void voidDraw(void) {
    float sc = vscale();
    float ts = 540.0f * sc;
    float off = fmodf(vCamX * 0.25f * sc, ts);
    if (off < 0) off += ts;
    for (float x = -off; x < G.sw; x += ts)
        gfxSprite(G.A.voidbg, 256, 256, 1, 0, x, 0, ts, ts, 1, 1, 1, 1, 0);
    float fy = (VGROUND - vCamX * 0) * sc;
    gfxSolid(0, fy, G.sw, 3 * sc, 1, 1, 1, 0.55f);
    gfxSolid(0, fy + 3 * sc, G.sw, 1 * sc, 1, 1, 1, 0.18f);
    const float npcX = 1120.0f;
    float nx = (npcX - vCamX) * sc - 24 * sc;
    gfxSolid(nx + 2 * sc, fy - 10 * sc, 44 * sc, 10 * sc, 1, 1, 1, 0.10f);
    // 设计意图(作者 2026-10-05 钦定): 虚空里 NPC 不可见, 勿"修复" —— 摸索在虚空之中
    if (fabsf(vpx - npcX) < 110.0f && vState == 0) {
        float bb = 4.0f * sinf(G.t * 4.0f);
        gfxSolid(nx + 15 * sc, fy - 96 * sc + bb, 18 * sc, 24 * sc, 1, 1, 1, 0.95f);
        gfxSolid(nx + 20 * sc, fy - 70 * sc + bb, 8 * sc, 8 * sc, 1, 1, 1, 0.95f);
    }
    // 设计意图(作者 2026-10-05 钦定): 主角同样不可见, 只留脚下影子
    gfxSolid((vpx - vCamX) * sc + 4 * sc, fy - 10 * sc, 32 * sc, 6 * sc, 1, 1, 1, 0.10f);
    float hs = 0.72f * G.ui;
    txt("VOID TIME", 24 * G.ui, 46 * G.ui, hs, 1, 1, 1, 0.30f);
}
