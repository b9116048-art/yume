#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#define TAG "GFX"
#define LOGI(...) LOGF(__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG,__VA_ARGS__)
typedef struct { ANativeActivity*act; ANativeWindow*win; AInputQueue*iq;
  pthread_mutex_t mtx; pthread_cond_t cv; int haveWin,quit; } App;
static App g;

static pthread_mutex_t g_logMtx = PTHREAD_MUTEX_INITIALIZER;
static char g_log[32768]; static int g_logOff = 0;
static void LOGF(const char*fmt,...){
  va_list a1,a2; va_start(a1,fmt); va_copy(a2,a1);
  __android_log_vprint(ANDROID_LOG_INFO,TAG,fmt,a1);
  pthread_mutex_lock(&g_logMtx);
  int n=vsnprintf(g_log+g_logOff,sizeof(g_log)-g_logOff,fmt,a2);
  if(n>0&&g_logOff+n<(int)sizeof(g_log)) g_logOff+=n;
  pthread_mutex_unlock(&g_logMtx);
  va_end(a2); va_end(a1);
}
static void flushReport(void){
  mkdir("/storage/emulated/0/Android/data/dev.mcp.gfx",0770);
  mkdir("/storage/emulated/0/Android/data/dev.mcp.gfx/files",0770);
  FILE*f=fopen("/storage/emulated/0/Android/data/dev.mcp.gfx/files/gfx_report.txt","w");
  if(f){fwrite(g_log,1,g_logOff,f);fclose(f);}
  int fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd>=0){
    struct sockaddr_in sa; memset(&sa,0,sizeof(sa));
    sa.sin_family=AF_INET; sa.sin_port=htons(7777);
    sa.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(connect(fd,(struct sockaddr*)&sa,sizeof(sa))==0){
      send(fd,g_log,g_logOff,0); send(fd,"\n[EOF]\n",7,0);
      __android_log_print(ANDROID_LOG_INFO,TAG,"report sent %d bytes",g_logOff);
    }
    close(fd);
  }
}
static double nowsec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
  return (double)t.tv_sec+(double)t.tv_nsec*1e-9;}
static int dcmp(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;
  return (x<y)?-1:((x>y)?1:0);}
static void report(const char*tag,double*v,int n){
  double s=0; for(int i=0;i<n;i++) s+=v[i];
  double mean=s/n;
  double*c=malloc(sizeof(double)*n); memcpy(c,v,sizeof(double)*n);
  qsort(c,n,sizeof(double),dcmp);
  LOGI("[%s] mean=%.2f fps (%.3f ms) p50=%.3f p95=%.3f p99=%.3f max=%.3f ms",
    tag,1.0/mean,mean*1000,c[n/2]*1000,c[(int)(n*.95)]*1000,c[(int)(n*.99)]*1000,c[n-1]*1000);
  free(c);}
static void cbWinCreated(ANativeActivity*a,ANativeWindow*w){
  pthread_mutex_lock(&g.mtx);
  if(g.win)ANativeWindow_release(g.win);
  ANativeWindow_acquire(w); g.win=w; g.haveWin=1;
  LOGI("window %p %dx%d",(void*)w,ANativeWindow_getWidth(w),ANativeWindow_getHeight(w));
  pthread_cond_broadcast(&g.cv); pthread_mutex_unlock(&g.mtx);}
static void cbWinDestroyed(ANativeActivity*a,ANativeWindow*w){
  pthread_mutex_lock(&g.mtx);
  if(g.win==w){ANativeWindow_release(g.win);g.win=NULL;g.haveWin=0;}
  pthread_mutex_unlock(&g.mtx); LOGI("window gone");}
static void cbIqCreated(ANativeActivity*a,AInputQueue*q){
  pthread_mutex_lock(&g.mtx); g.iq=q; pthread_mutex_unlock(&g.mtx); LOGI("input up");}
static void cbIqDestroyed(ANativeActivity*a,AInputQueue*q){
  pthread_mutex_lock(&g.mtx); if(g.iq==q)g.iq=NULL; pthread_mutex_unlock(&g.mtx);}
static void cbFocus(ANativeActivity*a,int f){LOGI("focus=%d",f);}
static void cbPause(ANativeActivity*a){LOGI("pause");}
static void cbResume(ANativeActivity*a){LOGI("resume");}
static void cbDestroy(ANativeActivity*a){
  pthread_mutex_lock(&g.mtx);g.quit=1;pthread_cond_broadcast(&g.cv);pthread_mutex_unlock(&g.mtx);}
static void cbConfig(ANativeActivity*a){LOGI("config changed");}
static void cbLowMem(ANativeActivity*a){LOGI("low mem");}
static const char*VS=
"#version 300 es\n"
"out vec2 vUv;\n"
"void main(){ vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));\n"
" vUv=p; gl_Position=vec4(p*2.0-1.0,0.0,1.0); }\n";
static const char*FS=
"#version 300 es\n"
"precision highp float;\n"
"in vec2 vUv; out vec4 o;\n"
"uniform vec2 uRes; uniform float uT;\n"
"void main(){ vec2 px=vUv*uRes; vec3 c;\n"
" if(vUv.x<0.34){ c=vec3(mod(floor(px.x)+floor(px.y),2.0)); }\n"
" else if(vUv.x<0.67){ float b=floor(vUv.y*64.0)/63.0; c=vec3(b*0.98+fract(vUv.y*64.0)*0.02); }\n"
" else { vec2 q=vUv-0.5; float a=atan(q.y,q.x), r=length(q);\n"
"   float s=sin((a+uT)*60.0);\n"
"   c=mix(vec3(0.05),vec3(0.95),smoothstep(-0.12,0.12,s))*((r<0.48)?1.0:0.12); }\n"
" float bar=exp(-pow(fract(vUv.y-uT*0.45)-0.5,2.0)*900.0);\n"
" c=mix(c,vec3(1.0,0.25,0.1),bar*0.85); o=vec4(c,1.0); }\n";
typedef struct { EGLDisplay d; EGLContext c; EGLSurface s; int samples; } Gpu;
static GLuint mk(GLenum t,const char*s){
  GLuint o=glCreateShader(t); glShaderSource(o,1,&s,NULL); glCompileShader(o);
  GLint ok=0; glGetShaderiv(o,GL_COMPILE_STATUS,&ok);
  if(!ok){char l[1024];glGetShaderInfoLog(o,1024,NULL,l);LOGE("shader: %s",l);}
  return o;}
static GLuint mkprog(void){
  GLuint v=mk(GL_VERTEX_SHADER,VS),f=mk(GL_FRAGMENT_SHADER,FS);
  GLuint p=glCreateProgram(); glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p);
  GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
  if(!ok){char l[1024];glGetProgramInfoLog(p,1024,NULL,l);LOGE("link: %s",l);}
  glDeleteShader(v); glDeleteShader(f); return p;}
static void dumpext(void){
  GLint n=0; glGetIntegerv(GL_NUM_EXTENSIONS,&n);
  static char b[8000]; int o=0;
  for(GLint i=0;i<n&&o<(int)sizeof(b)-80;i++)
    o+=snprintf(b+o,sizeof(b)-o,"%s ",(const char*)glGetStringi(GL_EXTENSIONS,i));
  LOGI("EXT(%d): %s",n,b);
  static const char*key[]={"GL_EXT_texture_filter_anisotropic","GL_EXT_sRGB_write_control",
    "GL_OES_texture_float_linear","GL_EXT_color_buffer_float","GL_EXT_disjoint_timer_query",
    "GL_EXT_shader_framebuffer_fetch","GL_ARM_shader_framebuffer_fetch","GL_KHR_debug",
    "GL_EXT_texture_compression_s3tc","GL_EXT_texture_compression_astc_ldr",
    "GL_AMD_compressed_ATC_texture","GL_EXT_multisampled_render_to_texture",NULL};
  for(int i=0;key[i];i++){
    int f=0;
    for(GLint j=0;j<n;j++) if(!strcmp((const char*)glGetStringi(GL_EXTENSIONS,j),key[i])){f=1;break;}
    LOGI("  %-42s %s",key[i],f?"YES":"-");}}
static void pumpInput(void){
  AInputQueue*q=NULL;
  pthread_mutex_lock(&g.mtx); q=g.iq; pthread_mutex_unlock(&g.mtx);
  if(!q) return;
  AInputEvent*ev=NULL; int n=0;
  while(AInputQueue_getEvent(q,&ev)>=0){
    if(AInputQueue_preDispatchEvent(q,ev)) continue;
    AInputQueue_finishEvent(q,ev,0);
    if(++n>64) break;
  }
}
static void gpuInfo(void){
  LOGI("GL_VENDOR   = %s",(const char*)glGetString(GL_VENDOR));
  LOGI("GL_RENDERER = %s",(const char*)glGetString(GL_RENDERER));
  LOGI("GL_VERSION  = %s",(const char*)glGetString(GL_VERSION));
  LOGI("GLSL        = %s",(const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
  GLint v=0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE,&v);        LOGI("MAX_TEXTURE_SIZE=%d",v);
  glGetIntegerv(GL_MAX_SAMPLES,&v);             LOGI("MAX_SAMPLES=%d",v);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE,&v);   LOGI("MAX_RB=%d",v);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&v); LOGI("MAX_TEX_UNITS=%d",v);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&v);      LOGI("MAX_VERT_ATTRIBS=%d",v);
  glGetIntegerv(GL_MAX_VIEWPORT_DIMS,&v);       LOGI("MAX_VIEWPORT=%d",v);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,&v);  LOGI("MAX_UBO=%d",v);
  glGetIntegerv(GL_MAX_ELEMENTS_INDICES,&v);    LOGI("MAX_ELEM_INDICES=%d",v);}
static int gpuTry(Gpu*G,ANativeWindow*w,int want){
  G->d=eglGetDisplay(EGL_DEFAULT_DISPLAY); EGLint ea=0,eb=0;
  if(!eglInitialize(G->d,&ea,&eb)){LOGE("eglInit 0x%x",eglGetError());return 0;}
  const EGLint ca[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
    EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
    EGL_DEPTH_SIZE,want?16:0,EGL_SAMPLE_BUFFERS,want?1:0,EGL_SAMPLES,want,EGL_NONE};
  EGLConfig cfg; EGLint n=0;
  if(!eglChooseConfig(G->d,ca,&cfg,1,&n)||n<1){eglTerminate(G->d);return 0;}
  EGLint s=0,dep=0,r=0,g2=0,b=0,al=0;
  eglGetConfigAttrib(G->d,cfg,EGL_SAMPLES,&s);   eglGetConfigAttrib(G->d,cfg,EGL_DEPTH_SIZE,&dep);
  eglGetConfigAttrib(G->d,cfg,EGL_RED_SIZE,&r);  eglGetConfigAttrib(G->d,cfg,EGL_GREEN_SIZE,&g2);
  eglGetConfigAttrib(G->d,cfg,EGL_BLUE_SIZE,&b); eglGetConfigAttrib(G->d,cfg,EGL_ALPHA_SIZE,&al);
  LOGI("EGL vendor=%s ver=%s",eglQueryString(G->d,EGL_VENDOR),eglQueryString(G->d,EGL_VERSION));
  LOGI("EGL cfg samples=%d RGBA=%d%d%d%d depth=%d",s,r,g2,b,al,dep);
  const EGLint cxa[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  G->c=eglCreateContext(G->d,cfg,EGL_NO_CONTEXT,cxa);
  if(G->c==EGL_NO_CONTEXT){LOGE("ctx 0x%x",eglGetError());eglTerminate(G->d);return 0;}
  G->s=eglCreateWindowSurface(G->d,cfg,w,NULL);
  if(G->s==EGL_NO_SURFACE){LOGE("surf 0x%x",eglGetError());return 0;}
  if(!eglMakeCurrent(G->d,G->s,G->s,G->c)){LOGE("cur 0x%x",eglGetError());return 0;}
  eglSwapInterval(G->d,1); G->samples=s; return 1;}
static void gpuDown(Gpu*G){
  eglMakeCurrent(G->d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
  eglDestroySurface(G->d,G->s); eglDestroyContext(G->d,G->c); eglTerminate(G->d);}
static void phase(ANativeWindow*w,int want,const char*tag,int frames,int verbose){
  Gpu G; memset(&G,0,sizeof(G));
  if(!gpuTry(&G,w,want)){
    if(want){LOGI("samples=%d no, fallback 0",want); if(!gpuTry(&G,w,0))return;}
    else return;}
  if(verbose){gpuInfo(); dumpext();}
  GLuint p=mkprog();
  GLint uR=glGetUniformLocation(p,"uRes"), uT=glGetUniformLocation(p,"uT");
  int W=ANativeWindow_getWidth(w),H=ANativeWindow_getHeight(w);
  LOGI("window %dx%d msaa=%d",W,H,G.samples);
  glViewport(0,0,W,H); glUseProgram(p); glUniform2f(uR,(float)W,(float)H);
  if(frames<=0){gpuDown(&G);return;}
  double*ft=malloc(sizeof(double)*frames);
  double t0=nowsec(),prev=t0;
  for(int i=0;i<frames;i++){
    pumpInput();
    if(!g.haveWin){LOGI("window lost in %s @%d",tag,i);frames=i;break;}
    double t=nowsec();
    glUniform1f(uT,(float)(t-t0));
    glClear(GL_COLOR_BUFFER_BIT); glDrawArrays(GL_TRIANGLES,0,3);
    eglSwapBuffers(G.d,G.s);
    double n2=nowsec(); ft[i]=n2-prev; prev=n2;}
  if(frames>0) report(tag,ft,frames); free(ft);
  gpuDown(&G);}
static void*renderMain(void*arg){
  pthread_mutex_lock(&g.mtx);
  while(!g.haveWin&&!g.quit) pthread_cond_wait(&g.cv,&g.mtx);
  pthread_mutex_unlock(&g.mtx);
  if(g.quit) return NULL;
  LOGI("===== GFX PROBE START =====");
  for(int k=0;k<3;k++){
    pthread_mutex_lock(&g.mtx);
    ANativeWindow*w=g.win; if(w)ANativeWindow_acquire(w);
    pthread_mutex_unlock(&g.mtx);
    if(!w){LOGI("no window at k=%d",k);break;}
    if(k==1) phase(w,4,"MSAA 4x",300,0);
    else     phase(w,0,(k==0)?"MSAA 0x":"MSAA 0x retest",300,(k==0));
    ANativeWindow_release(w);
    flushReport();
    if(g.quit) break;
  }
  flushReport();
  LOGI("===== MEASURE DONE =====");
  return NULL;}
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity*a,void*sv,size_t sz){
  (void)sv;(void)sz;
  memset(&g,0,sizeof(g)); g.act=a;
  pthread_mutex_init(&g.mtx,NULL); pthread_cond_init(&g.cv,NULL);
  a->callbacks->onNativeWindowCreated=cbWinCreated;
  a->callbacks->onNativeWindowDestroyed=cbWinDestroyed;
  a->callbacks->onInputQueueCreated=cbIqCreated;
  a->callbacks->onInputQueueDestroyed=cbIqDestroyed;
  a->callbacks->onWindowFocusChanged=cbFocus;
  a->callbacks->onPause=cbPause;
  a->callbacks->onResume=cbResume;
  a->callbacks->onDestroy=cbDestroy;
  a->callbacks->onConfigurationChanged=cbConfig;
  a->callbacks->onLowMemory=cbLowMem;
  pthread_t th; pthread_create(&th,NULL,renderMain,NULL);
  LOGI("onCreate done");}
