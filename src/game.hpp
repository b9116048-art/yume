// game.hpp —— 全局状态与接口
#pragma once
#include "gfx.hpp"
#include <android/asset_manager.h>

AAssetManager* gameAM(void);

enum { SC_ROOM = 0, SC_DREAM, SC_VOID };
enum { B_L = 0, B_R, B_U, B_D, B_ACT, B_JUMP, B_COUNT };
enum { AFT_NONE = 0, AFT_SLEEP, AFT_WAKE, AFT_VOID_END, AFT_RESTART_DREAM };

struct Assets {
    Tex room, human, blocks, portraits, dreambg, voidbg, ui;
    CJKFont font;
};
struct BtnState { float x, y, w, h; int down, prev; };

struct Ctx {
    Assets A;
    float sw, sh, ui;
    BtnState btn[B_COUNT];
    int scene;
    float t;
    float px[16], py[16];
    int   pid[16];
    int   np, anyDown, anyPrev, tapX, tapY;
    float axX, axY;
    int   stickId, stickActive;
    float stBaseX, stBaseY, stKnobX, stKnobY, stR;
    float fade;
    int   fadeDir, fadeTo;
    int   showBtns;
};
extern Ctx G;
extern char g_dataDir[256];

void uiLayout(void);
float inputX(void);
float inputY(void);
void uiDrawButtons(void);
int  btnDown(int id);
int  btnHit(int id);
int  anyTap(float* x, float* y);

void  txt(const char* s, float x, float baselineY, float scale,
          float r, float g, float b, float a);
float txtW(const char* s, float scale);
void  boxRect(float x, float y, float w, float h, float r, float g, float b, float a);
void  frameRect(float x, float y, float w, float h, float th, float a);

void dlgLoad(void);
void dlgStart(const char* node, int after);
int  dlgActive(void);
void dlgUpdate(float dt);
void dlgDraw(void);
void dlgAdvance(void);
void dlgBar(float x, float y, float w);

void sceneGo(int id);
void gameSaveSession(void);
int  gameLoadSession(void);

void roomEnter(int morning);
void roomUpdate(float dt);
void roomDraw(void);
void dreamEnter(void);
void dreamUpdate(float dt);
void dreamDraw(void);
void voidEnter(void);
void voidUpdate(float dt);
void voidDraw(void);
