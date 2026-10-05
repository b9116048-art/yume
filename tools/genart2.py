#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 人像 + 背景
import os, random
from PIL import Image, ImageDraw

OUT="/data/data/com.termux/files/home/game/assets"
K0=(0,0,0,255); K1=(30,30,30,255); K2=(62,62,62,255); K3=(100,100,100,255)
K4=(142,142,142,255); K5=(186,186,186,255); K6=(220,220,220,255); K7=(242,242,242,255); WHT=(252,252,252,255)
SKIN=(214,214,214,255); RED=(208,64,64,255)
def new(w,h,bg=(0,0,0,0)): return Image.new("RGBA",(w,h),bg)
def up(im,k): return im.resize((im.width*k,im.height*k),Image.NEAREST)

# ================= 人像 32x32 -> 192x192, 2x2 =================
PS=32; PK=6
port=new(PS*2,PS*2); pd=ImageDraw.Draw(port)

def p_human(x,y,hair,variant):
    pd.rectangle([x,y,x+PS-1,y+PS-1],fill=K2)
    pd.rectangle([x,y,x+PS-1,y+3],fill=K3)
    pd.rectangle([x+2,y+25,x+PS-3,y+PS-1],fill=K1)       # 肩
    pd.rectangle([x+5,y+23,x+26,y+27],fill=K0)
    pd.rectangle([x+13,y+20,x+18,y+25],fill=K3)          # 脖
    pd.rectangle([x+6,y+4,x+25,y+22],fill=SKIN)          # 脸
    pd.rectangle([x+6,y+4,x+7,y+22],fill=K5)
    pd.rectangle([x+24,y+4,x+25,y+22],fill=K5)
    pd.rectangle([x+6,y+22,x+25,y+22],fill=K5)
    if variant==0:
        pd.rectangle([x+5,y+2,x+26,y+9],fill=hair)
        pd.rectangle([x+5,y+2,x+7,y+20],fill=hair)
        pd.rectangle([x+24,y+2,x+26,y+20],fill=hair)
        pd.rectangle([x+9,y+9,x+13,y+10],fill=hair)
    else:
        pd.rectangle([x+5,y+2,x+26,y+8],fill=hair)
        pd.rectangle([x+5,y+2,x+7,y+16],fill=hair)
        pd.rectangle([x+19,y+2,x+26,y+22],fill=hair)
        pd.rectangle([x+14,y+3,x+19,y+12],fill=K0)
    pd.rectangle([x+9,y+12,x+12,y+13],fill=K1)            # 眉
    pd.rectangle([x+19,y+12,x+22,y+13],fill=K1)
    pd.rectangle([x+10,y+14,x+12,y+17],fill=K0)           # 眼
    pd.rectangle([x+19,y+14,x+21,y+17],fill=K0)
    pd.rectangle([x+10,y+18,x+12,y+18],fill=K4)
    pd.rectangle([x+19,y+18,x+21,y+18],fill=K4)
    pd.rectangle([x+14,y+20,x+17,y+20],fill=K3)           # 嘴

def p_square_creature(x,y,kind):
    pd.rectangle([x,y,x+PS-1,y+PS-1],fill=K2)
    if kind==0:
        pd.polygon([(x+6,y+9),(x+10,y+1),(x+14,y+9)],fill=K0)
        pd.polygon([(x+18,y+9),(x+22,y+1),(x+26,y+9)],fill=K0)
        pd.polygon([(x+8,y+9),(x+10,y+4),(x+12,y+9)],fill=K5)
        pd.polygon([(x+20,y+9),(x+22,y+4),(x+24,y+9)],fill=K5)
    else:
        pd.rectangle([x+7,y+1,x+11,y+10],fill=K0)
        pd.rectangle([x+21,y+1,x+25,y+10],fill=K0)
        pd.rectangle([x+9,y+3,x+10,y+8],fill=K5)
        pd.rectangle([x+23,y+3,x+24,y+8],fill=K5)
    pd.rectangle([x+4,y+8,x+27,y+29],fill=K0)
    pd.rectangle([x+5,y+9,x+26,y+28],fill=WHT)
    pd.rectangle([x+5,y+9,x+26,y+12],fill=K7)
    pd.rectangle([x+9,y+16,x+12,y+21],fill=K0)
    pd.rectangle([x+19,y+16,x+22,y+21],fill=K0)
    pd.rectangle([x+8,y+14,x+13,y+15],fill=K3)
    pd.rectangle([x+18,y+14,x+23,y+15],fill=K3)
    pd.rectangle([x+14,y+24,x+17,y+25],fill=K3)

p_human(0,0,K1,0)                 # 同僚
p_human(PS,0,K0,1)                # 他
p_square_creature(0,PS,0)         # 猫方块
p_square_creature(PS,PS,1)        # 兔方块
up(port,PK).save(os.path.join(OUT,"portraits.png"))

# ================= 梦境背景 256x256 (横向可平铺) =================
W=H=256
bg=Image.new("RGB",(W,H),(8,8,10)); bd=ImageDraw.Draw(bg)
random.seed(7)
for _ in range(46):
    cx=random.randint(0,W-1); cy=random.randint(20,H-40)
    w=random.randint(16,60); h=random.randint(8,40)
    g=random.choice([(18,18,22),(26,26,30),(14,14,18),(34,34,38)])
    for k in (-1,0,1):
        bd.rectangle([cx+k*W-w//2,cy-h//2,cx+k*W+w//2,cy+h//2],fill=g)
for _ in range(90):
    cx=random.randint(0,W-1); cy=random.randint(0,H-1)
    g=random.choice([40,52,64,80])
    bd.point((cx,cy),fill=(g,g,g))
for y in (64,132,196):
    bd.line([0,y,W-1,y],fill=(20,20,24))
bg.save(os.path.join(OUT,"dreambg.png"))

# ================= 空想时间世界背景 256x256 =================
v=Image.new("RGB",(W,H),(4,4,6)); vd=ImageDraw.Draw(v)
random.seed(23)
for _ in range(120):
    cx=random.randint(0,W-1); cy=random.randint(0,H-1)
    g=random.choice([60,90,120,160,210])
    vd.point((cx,cy),fill=(g,g,g))
for _ in range(7):
    y=random.randint(10,H-10); x0=random.randint(0,W-90); ln=random.randint(50,90)
    vd.line([x0,y,x0+ln,y],fill=(26,26,30))
    vd.line([x0,y+1,x0+ln,y+1],fill=(16,16,20))
v.save(os.path.join(OUT,"voidbg.png"))

for f in sorted(os.listdir(OUT)):
    p=os.path.join(OUT,f)
    print("%-16s %8d" % (f, os.path.getsize(p)))
