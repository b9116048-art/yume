#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 中文点阵字库生成器 —— 自动扫描 dialogue.txt + 固定 UI 字, 只烘焙真正用到的字
# 产出: font.png (图集) + fontmap.txt (codepoint -> 格子 / 左斜距 / 步进)
import os, math
from PIL import Image, ImageDraw, ImageFont

ROOT="/data/data/com.termux/files/home/game"
ASSETS=os.path.join(ROOT,"assets")
TTF="/system/fonts/NotoSansCJK-Regular.ttc"
SIZE=44; CELL_W=56; CELL_H=60; BASE=46; COLS=16

# 固定 UI 字（不依赖对话文本的）
UI="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" \
   " .,:;!?'\"()[]{}<>+-*/=_|%~@#&$^" \
   "现实梦境空想时间场景对话继续醒来回到房间空蝉同僚他是猫兔梦境引擎测试版" \
   "梦现实“”「」【】—…·清醒度虚幻惊"

chars=set()
for c in range(32,127): chars.add(chr(c))
for c in UI: chars.add(c)
dlg=os.path.join(ASSETS,"dialogue.txt")
if os.path.exists(dlg):
    for ln in open(dlg,encoding="utf-8"):
        ln=ln.rstrip("\n\r")
        if not ln.strip() or ln.lstrip().startswith("#"): continue
        for c in ln: chars.add(c)
cps=sorted(ord(c) for c in chars if ord(c)>=32)

font=ImageFont.truetype(TTF,SIZE)
rows=max(1,math.ceil(len(cps)/COLS))
atlas=Image.new("RGBA",(COLS*CELL_W, rows*CELL_H),(0,0,0,0))
dr=ImageDraw.Draw(atlas)
meta=[]
for i,cp in enumerate(cps):
    col=i%COLS; row=i//COLS
    ox=col*CELL_W; oy=row*CELL_H
    ch=chr(cp)
    try:
        bb = font.getbbox(ch, anchor="ls")
        try:
            adv = float(font.getlength(ch))
        except Exception:
            adv = 0.0
        if adv <= 0:
            adv = float(SIZE) * 0.5
    except Exception:
        bb=(0,-SIZE,SIZE*0.5,0); adv=SIZE*0.5
    x0,y0,x1,y1=bb
    if x1<=x0 or y1<=y0:
        meta.append((cp,col,row,0,adv if adv>0 else SIZE*0.5)); continue
    if x0<0: x0=0
    if y0<-(BASE-1): y0=-(BASE-1)
    if y1>CELL_H-BASE: y1=CELL_H-BASE
    dr.text((ox+1-x0, oy+BASE), ch, font=font, fill=(255,255,255,255), anchor="ls")
    meta.append((cp,col,row,x0,adv if adv>0 else SIZE*0.5))

atlas.save(os.path.join(ASSETS,"font.png"))
with open(os.path.join(ASSETS,"fontmap.txt"),"w",encoding="utf-8") as f:
    f.write("# hex col row x0 adv   cell=%dx%d base=%d size=%d cols=%d\n"%(CELL_W,CELL_H,BASE,SIZE,COLS))
    for (cp,col,row,x0,adv) in meta:
        f.write("%x %d %d %d %.2f\n"%(cp,col,row,x0,adv))

n_cjk=sum(1 for cp in cps if cp>=0x2E80)
print("字库: %d 字 (其中中日文 %d)  图集 %dx%d  行=%d"%(len(cps),n_cjk,atlas.width,atlas.height,rows))
print("font.png %d bytes / fontmap.txt %d bytes"%(os.path.getsize(os.path.join(ASSETS,'font.png')),os.path.getsize(os.path.join(ASSETS,'fontmap.txt'))))
