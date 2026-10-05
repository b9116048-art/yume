#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# UI 素材: 摇杆底盘 / 摇杆头 / 圆形按钮(常态 / 按下)
import os
from PIL import Image, ImageDraw

OUT = "/data/data/com.termux/files/home/game/assets/ui.png"
UI = 64
K  = 2

def new(w, h): return Image.new("RGBA", (w, h), (0, 0, 0, 0))
def up(im, k): return im.resize((im.width * k, im.height * k), Image.NEAREST)

im = new(UI * 2, UI * 2)
d = ImageDraw.Draw(im)
def cell(i): return ((i % 2) * UI, (i // 2) * UI)

# ---- 0 摇杆底盘 ----
x, y = cell(0); cx, cy = x + UI // 2, y + UI // 2
d.ellipse([cx-30, cy-30, cx+30, cy+30], fill=(255,255,255,26))
d.ellipse([cx-30, cy-30, cx+30, cy+30], outline=(255,255,255,115), width=3)
d.ellipse([cx-14, cy-14, cx+14, cy+14], fill=(255,255,255,20))
d.line([cx-30, cy, cx-22, cy], fill=(255,255,255,70), width=2)
d.line([cx+22, cy, cx+30, cy], fill=(255,255,255,70), width=2)
d.line([cx, cy-30, cx, cy-22], fill=(255,255,255,70), width=2)
d.line([cx, cy+22, cx, cy+30], fill=(255,255,255,70), width=2)

# ---- 1 摇杆头 ----
x, y = cell(1); cx, cy = x + UI // 2, y + UI // 2
d.ellipse([cx-19, cy-19, cx+19, cy+19], fill=(233,233,233,240))
d.ellipse([cx-19, cy-19, cx+19, cy+19], outline=(255,255,255,255), width=3)
d.ellipse([cx-12, cy-12, cx+12, cy+12], fill=(196,196,196,240))
d.ellipse([cx-5, cy-5, cx+5, cy+5], fill=(228,228,228,240))

# ---- 2 按钮(常态) ----
x, y = cell(2); cx, cy = x + UI // 2, y + UI // 2
d.ellipse([cx-29, cy-29, cx+29, cy+29], fill=(255,255,255,48))
d.ellipse([cx-29, cy-29, cx+29, cy+29], outline=(255,255,255,175), width=4)

# ---- 3 按钮(按下) ----
x, y = cell(3); cx, cy = x + UI // 2, y + UI // 2
d.ellipse([cx-29, cy-29, cx+29, cy+29], fill=(255,255,255,130))
d.ellipse([cx-29, cy-29, cx+29, cy+29], outline=(255,255,255,245), width=5)
d.ellipse([cx-18, cy-18, cx+18, cy+18], fill=(255,255,255,90))

up(im, K).save(OUT)
print("ui.png %dx%d  %d bytes" % (UI*2*K, UI*2*K, os.path.getsize(OUT)))
