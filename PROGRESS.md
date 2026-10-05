# 《ユメ》项目进度档案

> 给下一个对话的我：读完这份文件，你就接上了上下文。
> 生成时间：2026-10-04 16:30 (Asia/Shanghai)
> 上一个对话中的我（无跨会话记忆，可靠的只有这份文件）

---

## 一、项目定位

- 名称：《ユメ》/《夢》/《Utsusemi》
- 类型：2D 单人独立游戏，现实平面 RPG + 梦境横版跳跃双视角
- 主题：社畜、梦境、现实互渗、语言双关、治愈但致郁
- 目标：Steam，预期定价 6 元
- 主角：Utsusemi（空蝉）

### 红线（作者定的，不可越）

**代码里不出现任何一句台词。** 全部文本在 `assets/dialogue.txt`。

我（AI）负责：双视角框架、梦境编译器、文件修改机制、双关分发器、
结局评估器（只做求值）、台词槽位系统。

我永不产出：空蝉的任何台词、NPC 关键台词、结局种类/触发条件/结局文本、
伏笔与呼应、人物关系底层逻辑。

---

## 二、已完成（概念版可运行）

### 构建链（全部在手机上，无电脑/无 Gradle/无 Android Studio/无 root）

```
C++ → clang 交叉编译 aarch64 .so → aapt2 编资源 → d8 产 dex
→ zip 打包 → zipalign 对齐 → apksigner 签名 → 安装 → logcat 读回
```

产物：`~/game/out/yume.apk`，约 427 KB，v2+v3 签名。

### 代码（2350 行）

| 文件 | 行数 | 作用 |
|---|---|---|
| `src/gfx.cpp` | 306 | 批处理渲染 + 中日文点阵字库 + UTF-8 排版 |
| `src/game.cpp` | 476 | 状态机 / 输入 / 对话框系统 / 场景过渡 |
| `src/scenes.cpp` | 466 | 房间 / 梦境 / 空想时间世界 |
| `src/main.cpp` | 308 | NativeActivity 胶水层 + 输入分发 + 主循环 |
| `java/.../Immersive.java` | 29 | 沉浸全屏（反射） |
| `tools/*.py` | 649 | 素材生成器 6 个 |

### 素材（全部程序化生成）

`room.png` 房间图集 16 格 / `human.png` 人物 3款×4向×2帧 /
`blocks.png` 梦境方块 12 种 / `portraits.png` 人像 4 张 /
`dreambg.png` 梦境视差背景 / `voidbg.png` 空想时间星野 /
`ui.png` 摇杆+按钮 / `font.png` 中文点阵字库 295 字 /
`dialogue.txt` 台词容器 / `dream1.txt` 梦境关卡

### 已实现的功能

- 房间：俯视视角，虚拟摇杆移动，9 个可交互目标，头顶提示，上床进梦境
- 对话框：平滑弹出（smoothstep 滑入+淡入）+ 流式逐字输出（30 字/秒）+ 人像
- 梦境：方块主角带双眼，摄像机自动向右推，地形起伏/多层平台/坑/尖刺/敌人
- 空想时间世界：星野背景，与「他」对话后起床
- 输入：虚拟摇杆 + 圆形按钮 + 键盘映射层（WASD/方向键/ZX/空格/Enter/ESC）
- 沉浸全屏：反射调用，系统导航栏已隐藏
- 应用图标：梦境混沌背景 + 白色爱心，5 个密度

---

## 三、已踩过的坑（血泪，别再踩）

| 现象 | 根因 | 修法 |
|---|---|---|
| `dlopen failed: libc++_shared.so not found` | Termux C++ 动态链 libc++ | `-nostdlib++` |
| `INSTALL_FAILED_DEXOPT` | manifest 写 `hasCode=true` 但包里没 `classes.dex` | d8 产 dex 并打进包 |
| `NEEDED` 是 `libEGL.so.1` | 用了 mesa 的 SONAME | 自建桩 `-Wl,-soname,libEGL.so` |
| 字全部叠在一起 | PIL `getlength()` 不接受 `anchor` 参数，异常后步进全退化为常量 22px | 改用 `getlength(ch)` |
| 桌面点进去黑屏 | 窗口重建竞态，指针比较因地址复用而漏判 | 改用**代数计数器** `winGen` |
| `FindClass` 找不到自己的类 | `ANativeActivity_onCreate` 属系统类，走 boot classloader | 从 Activity 取 `getClassLoader().loadClass()` |
| 梦境整层地面消失 | `#` 同时当注释符和地面方块，地面行全部以 `#` 开头被当注释丢弃 | 注释符改成 `;` |
| 系统导航栏压住按钮 | 导航栏是 `[2268,0][2400,1080]` 的竖条，窗口只有 2290 宽 | 沉浸全屏 + 按钮内缩 |
| 输入分发器 5 秒超时杀进程 | 注册了 `onInputQueueCreated` 却从不消费事件 | 每帧排空 `AInputQueue` |
| 出生即死 | 摄像机从 x=0 开始推，角色出生在 x=140 | 相机后退 230px + 2.2 秒起步缓冲 |

---

## 四、环境（已就绪）

### Termux 工具链

```
clang 21.1.8 + ndk-sysroot 29-2   → aarch64-linux-android24 真机 .so
cmake 4.2.2 / make / ninja / lld / binutils / elfutils / patchelf
aapt 13.0.0.6 / aapt2 / d8 33.0.1 / zipalign / apksigner 0.9
OpenJDK 21 + keytool / gdb 16.3 / strace / jadx
freetype / harfbuzz / fontconfig / glm / vulkan-headers+tools+layers
glslang / shaderc / spirv-tools / assimp / openal-soft
libpng / libjpeg-turbo / libwebp / openjpeg / freeimage
imagemagick / optipng / pngcrush / pngquant / libimagequant
python3.12 + numpy + pillow / mesa / swiftshader
```

### 网络能力（决定上限）

| 目标 | 状态 |
|---|---|
| `github.com` 主站 | **000 不通** |
| `api.github.com` | **200 通** |
| `codeload.github.com` | **301 通**（可下任何 GitHub 源码） |
| `dl.google.com/android/repository/` | 200 通 |
| `repo1.maven.org` | 200 通 |
| PyPI | 200 通 |
| gitee / tuna 镜像 | 200 通 |

⇒ 拿源码没问题；**但 `git push` 到 github.com 会失败，必须走 api.github.com**。

### 设备实测

| 项 | 值 |
|---|---|
| 机型 | OPPO PEHM00 / Android 12 (API 31) |
| SoC | 骁龙 480 (SM4350)，6×A55@1.80G + 2×A76@2.04G |
| GPU | Adreno 619v1 @650MHz，GLES 3.2 / Vulkan 1.1 |
| 屏幕 | 2400×1080，游戏窗口 2290×1080 |
| 帧率 | 稳定 60 fps（vsync 锁定） |
| 内存 | 7.3 GiB 总，可用约 2.2 GiB（紧张） |

GL 能力：`MAX_SAMPLES=4`、`MAX_TEXTURE_SIZE=16384`、
各向异性过滤 ✓、`EXT_sRGB_write_control` ✓、
`EXT_multisampled_render_to_texture` ✓、`shader_framebuffer_fetch` ✓、
`disjoint_timer_query` ✓。贴图压缩 ASTC/ETC1/ATC，**不支持 S3TC**。

`persist.sys.force_sw_gles=1` 是**虚惊**，实测 `GL_RENDERER = Adreno (TM) 619`，硬件渲染正常。

---

## 五、关键工具与路径

| 用途 | 路径 |
|---|---|
| 项目根 | `~/game/` |
| 一键构建 | `bash ~/game/build.sh` |
| 产物 | `~/game/out/yume.apk` |
| 台词容器 | `~/game/assets/dialogue.txt` |
| 关卡数据 | `~/game/assets/dream1.txt` |
| 素材生成器 | `~/game/tools/*.py` |
| Shizuku shell | `~/rish/rish`（`sh ~/rish/rish -c "命令"`） |
| 共享存储交付 | `/sdcard/Download/yume.apk` |

### 安装（两种）

```bash
# A. Shizuku 可用时（全自动）
cp ~/game/out/yume.apk /sdcard/Download/yume.apk
sh ~/rish/rish -c "cp /sdcard/Download/yume.apk /data/local/tmp/y.apk && pm install -r /data/local/tmp/y.apk"

# B. Shizuku 挂了（需用户手点）
termux-open --content-type application/vnd.android.package-archive /sdcard/Download/yume.apk
```

### 读日志

```bash
sh ~/rish/rish -c "logcat -d -s YUME:V GAME:V"
```

---

## 六、当前阻塞

1. **Shizuku 服务挂了**（`Server is not running`）
   ⇒ 无法自动安装、无法注入按键自测。需用户重启 Shizuku。
2. **GitHub 推送缺凭据**
   ⇒ 需要用户提供仓库地址 + 有 `repo` 权限的 PAT。

---

## 七、待办

- [ ] 梦境地面加载实机复验（`;` 注释符修复后未自测）
- [ ] 摇杆手感调参（半径 152u / 死区 0.20 是估值）
- [ ] 梦境难度密度调参
- [ ] 双关语分发器（同一词在 RPG 模式是对话选项、在横版模式是操作指令）
- [ ] 第四面墙：文件修改机制与状态投射
- [ ] 结局评估器（条件由作者写，代码只做求值）
- [ ] Windows 交叉编译（`llvm-mingw-w64` 已在 Termux 源里）

---

## 八、参考：三角符文移植版（org.hndteam.deltarune）

从它身上学到的（已应用到本项目）：

- **输入漏斗**：`dispatchKeyEvent → Gamepad.handleKeyEvent`，
  物理键盘/手柄/iCade 全走同一条，先翻译成统一虚拟键码再进引擎
- **沉浸全屏**：`setupUiVisibility()` + `ourSetSystemUiVisibility()`（反射）
  + `restoreImmersiveModeRunnable` 反复重刷
- **配置文件**：`assets/options.ini`，其中 `YYiCadeSupport=TRUE` 就是硬件键映射开关
- **它也大量用反射**（`mSetSystemUiVisibility` 是 `Method` 字段），
  因为 NDK 环境拿不到 android.jar ⇒ 我们也可以这么做

已抠出的 APK：`/sdcard/Download/deltarune.apk`（786 MB，GameMaker 引擎）
