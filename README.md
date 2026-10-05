# ユメ / YUME

> 2D 单人独立游戏。融合「现实平面 RPG」与「梦境横版跳跃」的双视角体验。
> 当前阶段：**可运行的概念版（Android）**

---

## 这是什么

一个在**手机上从零构建**的 Android 原生游戏原型。没有电脑、没有 Gradle、没有 Android Studio、没有 root。

完整构建链：

```
C++ 源码 → clang 交叉编译 aarch64 .so → aapt2 编资源 → d8 产 dex
→ zip 打包 → zipalign 对齐 → apksigner 签名 → 安装 → 全量 logcat 读回
```

---

## 概念版流程

```
房间（俯视，虚拟摇杆）
  ├ 9 个可交互目标 → 头顶浮出提示 → 按「!」→ 对话框滑出
  │   电脑 / 书架 / 窗 / 垃圾桶 / 盆栽 / 手机 / 门 / 同僚（带人像）
  └ 上床 → 黑屏 → 梦境
梦境（方块主角，摄像机自动向右推）
  └ 地形起伏 / 多层平台 / 坑 / 尖刺 / 地面敌 / 飞行敌
    中途触发 → 猫方块对话、兔方块对话
    到终点门 → 空想时间世界
空想时间世界（纯黑星野）
  └ 走到右边和「他」说话（带人像）→ 说完 → 起床回房间，房间变亮
```

---

## 目录结构

```
src/            原生 C++（游戏本体）
  gfx.hpp/cpp     批处理渲染 + 中日文点阵字库 + UTF-8 排版
  game.hpp/cpp    状态机 / 输入 / 对话框系统 / 场景过渡
  scenes.cpp      房间(俯视) / 梦境(自动向右) / 空想时间世界
  main.cpp        NativeActivity 胶水层 + 输入分发 + 主循环
java/           Java 壳（仅用于反射调用系统 API）
  Boot.java       占位类（hasCode=true 需要）
  Immersive.java  沉浸全屏（反射，因为 NDK 环境无 android.jar）
tools/          素材生成器（全部程序化）
  genart1.py      房间图集 / 人物 / 梦境方块
  genart2.py      人像 / 背景
  genfont.py      中日文字库（自动扫描 dialogue.txt 烘焙）
  genui.py        摇杆 / 圆形按钮
  genicon.py      应用图标
  gendream.py     梦境关卡生成器（梦境编译器 v0）
assets/         运行时素材（由 tools/ 生成）
  dialogue.txt    ★ 全部台词都在这里，代码里没有任何中文字
  dream1.txt      梦境关卡数据
res/mipmap-*/    应用图标
build.sh        一键构建
```

---

## 构建

```bash
bash build.sh
```

依赖（Termux）：

```
pkg install clang cmake ninja make git openjdk-21 \
            aapt aapt2 d8 apksigner zipalign \
            python python-pillow imagemagick
```

关键编译参数：

```
clang++ -std=c++17 -shared -fPIC -O2 -nostdlib++ -fno-exceptions -fno-rtti
```

`-nostdlib++` 是必须的：Termux 的 C++ 会链上 `libc++_shared.so`，
而 APK 里没有它，`dlopen` 会直接失败。

---

## 红线（设计约束）

**代码里不出现任何一句台词。** 所有文本在 `assets/dialogue.txt`，格式：

```
节点ID | 说话人 | 人像 | 台词
```

- 人像：`none` / `colleague` / `him` / `cat` / `rabbit`
- 连续同 ID 的行组成一段对话，按出现顺序播放
- `#` 开头是注释
- 改完重跑 `build.sh`，字库会自动重新烘焙

> ⚠️ 关卡文件 `dream1.txt` 的注释符是 **`;`** 而不是 `#`。
> 因为 `#` 已经被用作地面方块。两者撞车会导致整层地面被当成注释丢弃。

---

## 已踩过的坑（重要）

| 现象 | 根因 | 修法 |
|---|---|---|
| `dlopen failed: libc++_shared.so not found` | Termux C++ 动态链 libc++ | `-nostdlib++` |
| `INSTALL_FAILED_DEXOPT` | manifest 写 `hasCode=true` 但包里没 `classes.dex` | 用 d8 产 dex 并打进包 |
| 链接后 `NEEDED` 是 `libEGL.so.1` | 用了 mesa 的 SONAME | 自建桩，`-Wl,-soname,libEGL.so` |
| 字全部叠在一起 | PIL `getlength()` 不接受 `anchor` 参数，异常后步进全退化为常量 | 改用 `getlength(ch)` |
| 桌面点进去黑屏 | 窗口重建竞态，指针比较会因地址复用而漏判 | 改用**代数计数器** `winGen` |
| `FindClass` 找不到自己的类 | `ANativeActivity_onCreate` 属系统类，走 boot classloader | 从 Activity 取 `getClassLoader().loadClass()` |
| 梦境整层地面消失 | `#` 同时当注释符和地面方块 | 注释符改成 `;` |
| 系统导航栏压住按钮 | 导航栏是 `[2268,0][2400,1080]` 的竖条 | 沉浸全屏 + 按钮内缩 |
| 输入分发器 5 秒超时杀进程 | 注册了 `onInputQueueCreated` 却从不消费事件 | 每帧排空 `AInputQueue` |

---

## 设备实测数据

| 项 | 值 |
|---|---|
| 机型 | OPPO PEHM00 / Android 12 (API 31) |
| SoC | 骁龙 480 (SM4350)，6×A55@1.80G + 2×A76@2.04G |
| GPU | Adreno 619v1 @650MHz，GLES 3.2 / Vulkan 1.1 |
| 屏幕 | 2400×1080，游戏窗口 2290×1080 |
| 帧率 | 稳定 60 fps（vsync 锁定） |

GL 能力：`MAX_SAMPLES=4`、`MAX_TEXTURE_SIZE=16384`、
各向异性过滤 ✓、`EXT_sRGB_write_control` ✓、
`EXT_multisampled_render_to_texture` ✓、`shader_framebuffer_fetch` ✓、
`disjoint_timer_query` ✓。贴图压缩支持 ASTC/ETC1/ATC，**不支持 S3TC**。

---

## 待办

- [ ] 梦境关卡的地面加载需实机复验（`;` 注释符修复后未自测）
- [ ] 摇杆手感调参（半径 152u / 死区 0.20 是估值）
- [ ] 梦境难度密度调参
- [ ] 双关语分发器（同一词在 RPG 模式是对话选项、在横版模式是操作指令）
- [ ] 第四面墙：文件修改机制与状态投射
- [ ] 结局评估器（条件由作者写，代码只做求值）
- [ ] Windows 交叉编译（`llvm-mingw-w64` 已在 Termux 源里）

---

## 许可

个人独立项目。
