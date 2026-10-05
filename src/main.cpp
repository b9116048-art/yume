// main.cpp —— NativeActivity 胶水层 + 输入分发 + 主循环
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <jni.h>
#include "gfx.hpp"

#define TAG "GAME"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

void gameInit(AAssetManager* am);
void gameSaveSession(void);
int  gameLoadSession(void);
extern char g_dataDir[256];
void gameResize(int sw, int sh);
void gameUpdate(float dt);
void gameRender(void);
void gamePointers(const float* xy, const int* ids, int n);
void gameKey(int code, int down);

struct App {
    ANativeActivity* act;
    ANativeWindow*  win;
    AInputQueue*    iq;
    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    int haveWin, quit, winGen;
};
static App g;
static int s_inited = 0;

// ---- 沉浸全屏: 借 HND 移植版的做法 ----
static jclass    g_immCls = 0;
static jmethodID g_immMid = 0;

// NativeActivity 是系统类, FindClass 会走 boot classloader, 找不到 APK 内的类。
// 必须从 Activity 实例拿它自己的 ClassLoader 再 loadClass。
static jclass loadAppClass(JNIEnv* e, jobject activity, const char* name) {
    if (!e || !activity) return 0;
    jclass actCls = e->GetObjectClass(activity);
    if (!actCls) return 0;
    jmethodID mid = e->GetMethodID(actCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!mid) { e->DeleteLocalRef(actCls); return 0; }
    jobject cl = e->CallObjectMethod(activity, mid);
    e->DeleteLocalRef(actCls);
    if (!cl) return 0;
    jclass clCls = e->GetObjectClass(cl);
    jmethodID lc = e->GetMethodID(clCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    e->DeleteLocalRef(clCls);
    if (!lc) { e->DeleteLocalRef(cl); return 0; }
    jstring nm = e->NewStringUTF(name);
    jclass c = (jclass)e->CallObjectMethod(cl, lc, nm);
    e->DeleteLocalRef(nm);
    e->DeleteLocalRef(cl);
    if (e->ExceptionCheck()) { e->ExceptionClear(); return 0; }
    return c;
}

static void immersiveApply(ANativeActivity* a) {
    if (!a || !a->vm) return;
    JNIEnv* e = 0;
    if (a->vm->GetEnv((void**)&e, JNI_VERSION_1_6) != JNI_OK) {
        if (a->vm->AttachCurrentThread(&e, 0) != JNI_OK) return;
    }
    if (!e) return;
    if (!g_immCls) {
        jclass c = loadAppClass(e, a->clazz, "dev.mcp.gfx.Immersive");
        if (!c) { e->ExceptionClear(); LOGI("immersive: class not found (via app loader)"); return; }
        g_immCls = (jclass)e->NewGlobalRef(c);
        e->DeleteLocalRef(c);
        if (g_immCls) g_immMid = e->GetStaticMethodID(g_immCls, "go", "(Ljava/lang/Object;)V");
    }
    if (g_immCls && g_immMid && a->clazz) {
        e->CallStaticVoidMethod(g_immCls, g_immMid, a->clazz);
        if (e->ExceptionCheck()) e->ExceptionClear();
        LOGI("immersive applied");
    }
}


#define MAXP 16
struct Ptr { int id; float x, y; };
static Ptr s_ptr[MAXP];
static int s_nptr = 0;
static float s_flat[MAXP * 2];
static int   s_ids[MAXP];

static double nowsec(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static void ptrSet(int id, float x, float y) {
    for (int i = 0; i < s_nptr; i++) if (s_ptr[i].id == id) { s_ptr[i].x = x; s_ptr[i].y = y; return; }
    if (s_nptr < MAXP) { s_ptr[s_nptr].id = id; s_ptr[s_nptr].x = x; s_ptr[s_nptr].y = y; s_nptr++; }
}
static void ptrDel(int id) {
    for (int i = 0; i < s_nptr; i++) if (s_ptr[i].id == id) { s_ptr[i] = s_ptr[s_nptr - 1]; s_nptr--; return; }
}

static void handleMotion(AInputEvent* e) {
    int action = AMotionEvent_getAction(e);
    int mask = action & AMOTION_EVENT_ACTION_MASK;
    size_t cnt = AMotionEvent_getPointerCount(e);
    if (mask == AMOTION_EVENT_ACTION_DOWN || mask == AMOTION_EVENT_ACTION_POINTER_DOWN ||
        mask == AMOTION_EVENT_ACTION_MOVE) {
        for (size_t i = 0; i < cnt; i++)
            ptrSet(AMotionEvent_getPointerId(e, i), AMotionEvent_getX(e, i), AMotionEvent_getY(e, i));
    }
    if (mask == AMOTION_EVENT_ACTION_UP || mask == AMOTION_EVENT_ACTION_POINTER_UP) {
        int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        if (idx >= 0 && (size_t)idx < cnt) ptrDel(AMotionEvent_getPointerId(e, idx));
    }
    if (mask == AMOTION_EVENT_ACTION_CANCEL) s_nptr = 0;
}

static void handleKey(AInputEvent* e) {
    int code = AKeyEvent_getKeyCode(e);
    int act = AKeyEvent_getAction(e);
    int down = (act == AKEY_EVENT_ACTION_DOWN) ? 1 : (act == AKEY_EVENT_ACTION_UP ? 0 : -1);
    if (down < 0) return;
    gameKey(code, down);
}

static void pumpInput(void) {
    AInputQueue* q;
    pthread_mutex_lock(&g.mtx); q = g.iq; pthread_mutex_unlock(&g.mtx);
    if (!q) { s_nptr = 0; gamePointers(s_flat, s_ids, 0); return; }
    AInputEvent* ev = 0;
    int guard = 0;
    while (AInputQueue_getEvent(q, &ev) >= 0) {
        int t = AInputEvent_getType(ev);
        if (AInputQueue_preDispatchEvent(q, ev)) { if (++guard > 128) break; continue; }
        if (t == AINPUT_EVENT_TYPE_MOTION) handleMotion(ev);
        else if (t == AINPUT_EVENT_TYPE_KEY) handleKey(ev);
        AInputQueue_finishEvent(q, ev, 0);
        if (++guard > 128) break;
    }
    for (int i = 0; i < s_nptr; i++) { s_flat[i * 2] = s_ptr[i].x; s_flat[i * 2 + 1] = s_ptr[i].y; }
    gamePointers(s_flat, s_ids, s_nptr);
}

// ---------------- EGL ----------------
struct Gpu { EGLDisplay d; EGLContext c; EGLSurface s; EGLConfig cfg; int hasCtx; };
static Gpu s_gpu;

static int gpuUp(Gpu* G, ANativeWindow* w) {
    if (G->hasCtx) {
        G->s = eglCreateWindowSurface(G->d, G->cfg, w, 0);
        if (G->s == EGL_NO_SURFACE) { LOGE("surface reuse 0x%x", eglGetError()); return 0; }
        if (!eglMakeCurrent(G->d, G->s, G->s, G->c)) { LOGE("makeCurrent reuse 0x%x", eglGetError()); return 0; }
        LOGI("EGL context reused");
        return 1;
    }
    G->d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint a = 0, b = 0;
    if (!eglInitialize(G->d, &a, &b)) { LOGE("eglInitialize 0x%x", eglGetError()); return 0; }
    const EGLint ca[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
    EGLint n = 0;
    if (!eglChooseConfig(G->d, ca, &G->cfg, 1, &n) || n < 1) { LOGE("chooseConfig failed"); eglTerminate(G->d); return 0; }
    const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    G->c = eglCreateContext(G->d, G->cfg, EGL_NO_CONTEXT, cx);
    if (G->c == EGL_NO_CONTEXT) { LOGE("ctx 0x%x", eglGetError()); eglTerminate(G->d); return 0; }
    G->s = eglCreateWindowSurface(G->d, G->cfg, w, 0);
    if (G->s == EGL_NO_SURFACE) { LOGE("surface 0x%x", eglGetError()); eglDestroyContext(G->d, G->c); eglTerminate(G->d); return 0; }
    if (!eglMakeCurrent(G->d, G->s, G->s, G->c)) { LOGE("makeCurrent 0x%x", eglGetError()); return 0; }
    eglSwapInterval(G->d, 1);
    G->hasCtx = 1;
    return 1;
}
static void gpuLoseSurface(Gpu* G) {
    eglMakeCurrent(G->d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (G->s != EGL_NO_SURFACE) { eglDestroySurface(G->d, G->s); G->s = EGL_NO_SURFACE; }
}
static void gpuShutdown(Gpu* G) {
    eglMakeCurrent(G->d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (G->s != EGL_NO_SURFACE) eglDestroySurface(G->d, G->s);
    if (G->c != EGL_NO_CONTEXT) eglDestroyContext(G->d, G->c);
    if (G->d != EGL_NO_DISPLAY) eglTerminate(G->d);
    G->hasCtx = 0;
}

static int renderLoop(ANativeWindow* w) {
    /* EGL state now persists in s_gpu across window recreate */
    for (int i = 0; i < 250 && !g.quit; i++) {
        if (ANativeWindow_getWidth(w) > 0 && ANativeWindow_getHeight(w) > 0) break;
        usleep(8 * 1000);
    }
    int W = ANativeWindow_getWidth(w), H = ANativeWindow_getHeight(w);
    if (W <= 0 || H <= 0) { LOGI("window %dx%d not ready, fallback 2400x1080", W, H); W = 2400; H = 1080; }
    if (!gpuUp(&s_gpu, w)) return 1;
    glViewport(0, 0, W, H);
    if (!s_inited) {
        const char* dp = g.act->internalDataPath;
        snprintf(g_dataDir, sizeof(g_dataDir), "%s", dp ? dp : "");
        LOGI("dataDir=%s", g_dataDir);
        gameInit(g.act->assetManager); s_inited = 1;
    }
    gameResize(W, H);
    int myGen = g.winGen;
    LOGI("render loop start %dx%d gen=%d", W, H, myGen);
    double prev = nowsec();
    int frames = 0;
    double acc = 0.0, worst = 0.0;
    int immTick = 0;   // 参考 HND 移植版的 restoreImmersiveModeRunnable: 开局几秒内反复重刷沉浸

    for (;;) {
        pthread_mutex_lock(&g.mtx);
        int lost = (!g.haveWin || g.quit || g.winGen != myGen);
        pthread_mutex_unlock(&g.mtx);
        if (lost) { LOGI("window lost/replaced gen %d->%d", myGen, g.winGen); break; }
        int nw = ANativeWindow_getWidth(w), nh = ANativeWindow_getHeight(w);
        if (++immTick < 900 && (immTick % 30) == 0 && g.act) immersiveApply(g.act);
        if (nw > 0 && nh > 0 && (nw != W || nh != H)) {
            W = nw; H = nh;
            glViewport(0, 0, W, H);
            gameResize(W, H);
            LOGI("window resized %dx%d", W, H);
        }
        pumpInput();
        double t = nowsec();
        float dt = (float)(t - prev);
        prev = t;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
        gameUpdate(dt);
        gameRender();
        eglSwapBuffers(s_gpu.d, s_gpu.s);
        frames++; acc += dt;
        if (dt > worst) worst = dt;
        if (acc >= 5.0) {
            LOGI("PERF %.1f fps frames=%d worst=%.1f ms", frames / acc, frames, worst * 1000.0);
            frames = 0; acc = 0.0; worst = 0.0;
        }
    }
    LOGI("render loop end");
    gpuLoseSurface(&s_gpu);
    return 1;
}

static void* renderMain(void* arg) {
    for (;;) {
        pthread_mutex_lock(&g.mtx);
        while (!g.haveWin && !g.quit) pthread_cond_wait(&g.cv, &g.mtx);
        ANativeWindow* w = g.win;
        if (w) ANativeWindow_acquire(w);
        int q = g.quit;
        pthread_mutex_unlock(&g.mtx);
        if (q) { if (w) ANativeWindow_release(w); break; }
        if (!w) continue;
        int ok = renderLoop(w);
        ANativeWindow_release(w);
        if (!ok) {
            pthread_mutex_lock(&g.mtx);
            if (g.win == w) g.haveWin = 0;
            while (!g.haveWin && !g.quit) pthread_cond_wait(&g.cv, &g.mtx);
            pthread_mutex_unlock(&g.mtx);
        }
    }
    return 0;
}

// ---------------- 生命周期 ----------------
static void cbWinCreated(ANativeActivity* a, ANativeWindow* w) {
    pthread_mutex_lock(&g.mtx);
    if (g.win) ANativeWindow_release(g.win);
    ANativeWindow_acquire(w);
    g.win = w; g.haveWin = 1; g.winGen++;
    pthread_cond_broadcast(&g.cv);
    pthread_mutex_unlock(&g.mtx);
    LOGI("window created %dx%d", ANativeWindow_getWidth(w), ANativeWindow_getHeight(w));
}
static void cbWinDestroyed(ANativeActivity* a, ANativeWindow* w) {
    pthread_mutex_lock(&g.mtx);
    if (g.win == w) { ANativeWindow_release(g.win); g.win = 0; g.haveWin = 0; g.winGen++; }
    pthread_mutex_unlock(&g.mtx);
    LOGI("window destroyed");
}
static void cbIqCreated(ANativeActivity* a, AInputQueue* q) {
    pthread_mutex_lock(&g.mtx); g.iq = q; pthread_mutex_unlock(&g.mtx);
    LOGI("input queue up");
}
static void cbIqDestroyed(ANativeActivity* a, AInputQueue* q) {
    pthread_mutex_lock(&g.mtx); if (g.iq == q) g.iq = 0; pthread_mutex_unlock(&g.mtx);
    s_nptr = 0;
}
static void cbFocus(ANativeActivity* a, int f) { LOGI("focus=%d", f); if (f) immersiveApply(a); }
static void cbPause(ANativeActivity* a) { LOGI("pause"); gameSaveSession(); }
static void cbResume(ANativeActivity* a) { LOGI("resume"); }
static void cbDestroy(ANativeActivity* a) {
    gpuShutdown(&s_gpu);
    pthread_mutex_lock(&g.mtx); g.quit = 1; pthread_cond_broadcast(&g.cv); pthread_mutex_unlock(&g.mtx);
}
static void cbConfig(ANativeActivity* a) { LOGI("config changed"); }
static void cbLowMem(ANativeActivity* a) { LOGI("low memory"); }

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* a, void* sv, size_t sz) {
    (void)sv; (void)sz;
    memset(&g, 0, sizeof(g));
    g.act = a;
    pthread_mutex_init(&g.mtx, 0);
    pthread_cond_init(&g.cv, 0);
    a->callbacks->onNativeWindowCreated = cbWinCreated;
    a->callbacks->onNativeWindowDestroyed = cbWinDestroyed;
    a->callbacks->onInputQueueCreated = cbIqCreated;
    a->callbacks->onInputQueueDestroyed = cbIqDestroyed;
    a->callbacks->onWindowFocusChanged = cbFocus;
    a->callbacks->onPause = cbPause;
    a->callbacks->onResume = cbResume;
    a->callbacks->onDestroy = cbDestroy;
    a->callbacks->onConfigurationChanged = cbConfig;
    a->callbacks->onLowMemory = cbLowMem;
    ANativeActivity_setWindowFlags(a, 0x400 /*FULLSCREEN*/ | 0x80 /*KEEP_SCREEN_ON*/, 0);
    immersiveApply(a);
    pthread_t th;
    pthread_create(&th, 0, renderMain, 0);
    LOGI("onCreate done");
}
