#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 应用图标: 梦境混沌背景 + 白色爱心
import os, random
from PIL import Image, ImageDraw

ROOT = "/data/data/com.termux/files/home/game"
S = 1024
random.seed(20261006)

im = Image.new("RGB", (S, S), (9, 9, 12))
d = ImageDraw.Draw(im)

# ---- 不规则混沌矩形 (和 dreambg.png 同源风格) ----
for _ in range(110):
    w = random.randint(int(S * 0.05), int(S * 0.24))
    h = random.randint(int(S * 0.04), int(S * 0.17))
    x = random.randint(-w // 3, S - w * 2 // 3)
    y = random.randint(-h // 3, S - h * 2 // 3)
    g = random.choice([(15,15,19),(21,21,26),(11,11,15),(27,27,33),(19,19,24),(24,24,29)])
    d.rectangle([x, y, x + w, y + h], fill=g)
for _ in range(260):
    x = random.randint(0, S - 1); y = random.randint(0, S - 1)
    g = random.choice([46,66,92,126,168])
    d.point((x, y), fill=(g, g, g))

# ---- 白色爱心 (先用蒙版画, 再降采样取得平滑边缘) ----
mask = Image.new("L", (S, S), 0)
m = ImageDraw.Draw(mask)
cx, cy = S // 2, int(S * 0.58)
r = int(S * 0.185)
m.ellipse([cx - r, cy - r, cx, cy + 2], fill=255)
m.ellipse([cx, cy - r, cx + r, cy + 2], fill=255)
m.polygon([(cx - r + 2, cy - r * 0.42), (cx + r - 2, cy - r * 0.42), (cx, cy + r * 1.12)], fill=255)

# ---- 合成 + 降采样输出各密度 ----
DENS = [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]
for name, px in DENS:
    bg = im.resize((px, px), Image.LANCZOS)
    mk = mask.resize((px, px), Image.LANCZOS)
    bg.paste((250, 250, 252), (0, 0), mk)
    out = os.path.join(ROOT, "res", "mipmap-" + name)
    os.makedirs(out, exist_ok=True)
    bg.save(os.path.join(out, "ic_launcher.png"))
    print("  mipmap-%-8s %3dpx" % (name, px))
print("图标完成")
