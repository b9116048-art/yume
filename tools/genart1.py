#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 《ユメ》概念版素材生成 —— 黑白灰为主, 仅两个点缀色(红=危险 / 冷蓝=屏幕)
import os, math, random
from PIL import Image, ImageDraw

OUT = "/data/data/com.termux/files/home/game/assets"
os.makedirs(OUT, exist_ok=True)

K0=(0,0,0,255);   K1=(30,30,30,255);  K2=(62,62,62,255);  K3=(100,100,100,255)
K4=(142,142,142,255); K5=(186,186,186,255); K6=(220,220,220,255); K7=(242,242,242,255)
WHT=(252,252,252,255)
RED=(208,64,64,255)      # 危险
COLD=(126,158,196,255)   # 显示屏冷光
SKIN=(214,214,214,255)

def new(w,h,bg=(0,0,0,0)): return Image.new("RGBA",(w,h),bg)
def up(im,k): return im.resize((im.width*k,im.height*k), Image.NEAREST)

# ============================================================
# 1) 房间图集  (逻辑 24x24 -> 48x48, 8列2行 = 16格)
# ============================================================
TS=24; TK=2
tiles=new(TS*8,TS*2); d=ImageDraw.Draw(tiles)
def tcell(i): return ((i%8)*TS,(i//8)*TS)

def t_floor(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K5)
    d.line([x,y,x+TS-1,y],fill=K4); d.line([x,y,x,y+TS-1],fill=K4)
    d.rectangle([x+TS-3,y+TS-3,x+TS-1,y+TS-1],fill=K4)
def t_floor2(x,y):
    t_floor(x,y)
    d.rectangle([x+8,y+8,x+15,y+15],fill=K4)
def t_wall(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K2)
    d.rectangle([x,y,x+TS-1,y+2],fill=K3)
    for i in range(3,TS,6): d.line([x+i,y+3,x+i,y+TS-4],fill=K1)
def t_wallbase(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K3)
    d.rectangle([x,y,x+TS-1,y+4],fill=K4)
    d.line([x,y+TS-4,x+TS-1,y+TS-4],fill=K2)
def t_desk(x,y):
    d.rectangle([x+1,y+7,x+TS-2,y+TS-1],fill=K4)
    d.rectangle([x+1,y+7,x+TS-2,y+9],fill=K5)
    d.rectangle([x+2,y+TS-7,x+5,y+TS-2],fill=K2)
    d.rectangle([x+TS-6,y+TS-7,x+TS-3,y+TS-2],fill=K2)
def t_pc(x,y):
    d.rectangle([x+2,y+1,x+TS-3,y+14],fill=K1)
    d.rectangle([x+4,y+3,x+TS-5,y+12],fill=COLD)
    d.rectangle([x+6,y+5,x+TS-7,y+7],fill=(150,182,216,255))
    d.rectangle([x+TS//2-1,y+14,x+TS//2+1,y+18],fill=K2)
    d.rectangle([x+6,y+18,x+TS-7,y+19],fill=K2)
def t_bedh(x,y):
    d.rectangle([x,y+1,x+TS-1,y+TS-1],fill=K5)
    d.rectangle([x+1,y+2,x+TS-2,y+9],fill=WHT)
    d.line([x,y+1,x+TS-1,y+1],fill=K3)
def t_bedf(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K5)
    d.rectangle([x+1,y+1,x+TS-2,y+10],fill=K6)
    d.line([x,y+TS-1,x+TS-1,y+TS-1],fill=K3)
def t_shelf(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K3)
    d.rectangle([x+1,y+1,x+TS-2,y+TS-2],fill=K2)
    for r in (2,8,14):
        d.rectangle([x+2,y+r+3,x+TS-3,y+r+5],fill=K4)
        for b in range(3,TS-4,4):
            d.rectangle([x+b,y+r,x+b+2,y+r+5],fill=K5 if (b%3) else K6)
def t_door(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K2)
    d.rectangle([x+2,y+1,x+TS-3,y+TS-1],fill=K4)
    d.rectangle([x+3,y+2,x+TS-4,y+TS-3],fill=K3)
    d.rectangle([x+TS-7,y+11,x+TS-5,y+14],fill=K1)
    d.line([x+2,y+1,x+TS-3,y+1],fill=K5)
def t_window(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K3)
    d.rectangle([x+2,y+2,x+TS-3,y+TS-3],fill=K6)
    d.line([x+TS//2,y+2,x+TS//2,y+TS-3],fill=K4)
    d.line([x+2,y+TS//2,x+TS-3,y+TS//2],fill=K4)
    d.polygon([(x+4,y+12),(x+10,y+5),(x+16,y+12)],fill=K5)
def t_plant(x,y):
    d.rectangle([x+7,y+15,x+16,y+TS-2],fill=K4)
    d.rectangle([x+7,y+15,x+16,y+17],fill=K5)
    for (px,py,r) in [(11,11,6),(7,14,4),(16,14,4)]:
        d.ellipse([x+px-r,y+py-r,x+px+r,y+py+r],fill=K3)
    d.ellipse([x+9,y+8,x+15,y+14],fill=K4)
def t_rug(x,y):
    d.rectangle([x,y,x+TS-1,y+TS-1],fill=K4)
    d.rectangle([x+3,y+3,x+TS-4,y+TS-4],fill=K5)
    d.rectangle([x+7,y+7,x+TS-8,y+TS-8],fill=K4)
def t_chair(x,y):
    d.rectangle([x+4,y+4,x+TS-5,y+TS-5],fill=K4)
    d.rectangle([x+4,y+4,x+TS-5,y+7],fill=K5)
    d.rectangle([x+5,y+TS-7,x+7,y+TS-3],fill=K2)
    d.rectangle([x+TS-8,y+TS-7,x+TS-6,y+TS-3],fill=K2)
def t_bin(x,y):
    d.rectangle([x+5,y+6,x+TS-6,y+TS-3],fill=K3)
    d.rectangle([x+4,y+4,x+TS-5,y+7],fill=K4)
    d.line([x+7,y+9,x+7,y+TS-5],fill=K2)
    d.line([x+TS-8,y+9,x+TS-8,y+TS-5],fill=K2)
def t_tv(x,y):
    d.rectangle([x+1,y+3,x+TS-2,y+16],fill=K1)
    d.rectangle([x+3,y+5,x+TS-4,y+14],fill=K2)
    d.rectangle([x+9,y+17,x+14,y+19],fill=K2)

for _i,_f in enumerate([t_floor,t_floor2,t_wall,t_wallbase,t_desk,t_pc,t_bedh,t_bedf,
                        t_shelf,t_door,t_window,t_plant,t_rug,t_chair,t_bin,t_tv]):
    _x,_y=tcell(_i); _f(_x,_y)
up(tiles,TK).save(os.path.join(OUT,"room.png"))

# ============================================================
# 2) 人物 (俯视)  逻辑 24x32 -> 48x64, 8列 x 3行
# ============================================================
HW,HH=24,32
sheet=new(HW*8,HH*3)
sd=ImageDraw.Draw(sheet)

def human(ox,oy,dirn,step,hair,cloth,accent=K2):
    # dirn 0下 1上 2左 3右 ; step 0/1
    sx=ox; sy=oy
    # 阴影
    sd.ellipse([sx+5,sy+HH-5,sx+18,sy+HH-2],fill=(0,0,0,70))
    # 腿
    lx=sx+7 + (1 if step else 0); rx=sx+13 - (1 if step else 0)
    sd.rectangle([lx,sy+22,lx+3,sy+28],fill=K2)
    sd.rectangle([rx,sy+22,rx+3,sy+28],fill=K2)
    # 身体
    sd.rectangle([sx+6,sy+15,sx+17,sy+24],fill=cloth)
    sd.rectangle([sx+6,sy+15,sx+17,sy+17],fill=accent)
    # 手臂
    sd.rectangle([sx+4,sy+17,sx+6,sy+23],fill=cloth)
    sd.rectangle([sx+17,sy+17,sx+19,sy+23],fill=cloth)
    # 头
    sd.rectangle([sx+5,sy+3,sx+18,sy+16],fill=SKIN)
    sd.rectangle([sx+5,sy+3,sx+5,sy+16],fill=K3)
    sd.rectangle([sx+18,sy+3,sx+18,sy+16],fill=K3)
    sd.rectangle([sx+5,sy+15,sx+18,sy+16],fill=K3)
    # 头发
    sd.rectangle([sx+5,sy+2,sx+18,sy+7],fill=hair)
    if dirn==1:
        sd.rectangle([sx+5,sy+2,sx+18,sy+13],fill=hair)
    elif dirn in (2,3):
        sd.rectangle([sx+5,sy+2,sx+18,sy+9],fill=hair)
        if dirn==2: sd.rectangle([sx+5,sy+2,sx+8,sy+15],fill=hair)
        else:       sd.rectangle([sx+15,sy+2,sx+18,sy+15],fill=hair)
    # 眼睛
    if dirn==0:
        sd.rectangle([sx+8,sy+10,sx+9,sy+12],fill=K0); sd.rectangle([sx+14,sy+10,sx+15,sy+12],fill=K0)
    elif dirn==2:
        sd.rectangle([sx+7,sy+10,sx+8,sy+12],fill=K0)
    elif dirn==3:
        sd.rectangle([sx+15,sy+10,sx+16,sy+12],fill=K0)

HAIR=[K1,K1,K0]; CLOTH=[K3,K2,K4]
for v in range(3):
    for dirn in range(4):
        for step in range(2):
            k=(v*4+dirn)*2+step
            human((k%8)*HW,(k//8)*HH,dirn,step,HAIR[v],CLOTH[v],K2 if v!=1 else K4)
up(sheet,2).save(os.path.join(OUT,"human.png"))

# ============================================================
# 3) 梦境方块生物 / 障碍  逻辑 32x32 -> 64x64, 8列 x 2行
# ============================================================
BS=32; BK=2
blk=new(BS*8,BS*2); bd=ImageDraw.Draw(blk)
def bcell(i): return ((i%8)*BS,(i//8)*BS)

def b_square(x,y,fill,outline=K0,eye_fn=None):
    bd.rectangle([x+2,y+2,x+BS-3,y+BS-3],fill=outline)
    bd.rectangle([x+3,y+3,x+BS-4,y+BS-4],fill=fill)
    bd.rectangle([x+3,y+3,x+BS-4,y+6],fill=K7 if fill==WHT else K4)
    if eye_fn: eye_fn(x,y)
def eyes_normal(x,y,col=K0):
    bd.rectangle([x+9,y+12,x+12,y+17],fill=col)
    bd.rectangle([x+19,y+12,x+22,y+17],fill=col)
def eyes_angry(x,y):
    bd.polygon([(x+8,y+11),(x+14,y+15),(x+13,y+18),(x+7,y+14)],fill=RED)
    bd.polygon([(x+23,y+11),(x+17,y+15),(x+18,y+18),(x+24,y+14)],fill=RED)
def eyes_sad(x,y):
    bd.rectangle([x+9,y+14,x+12,y+18],fill=K0)
    bd.rectangle([x+19,y+14,x+22,y+18],fill=K0)
    bd.rectangle([x+8,y+12,x+13,y+13],fill=K2)
    bd.rectangle([x+18,y+12,x+23,y+13],fill=K2)
def eyes_dot(x,y):
    bd.rectangle([x+10,y+13,x+12,y+16],fill=K0)
    bd.rectangle([x+19,y+13,x+21,y+16],fill=K0)

def b_player(x,y): b_square(x,y,WHT,K0,eyes_normal)
def b_enemy(x,y):
    b_square(x,y,K1,RED,eyes_angry)
    for k in range(3,BS-3,7):
        bd.polygon([(x+k,y+2),(x+k+3,y-3),(x+k+6,y+2)],fill=RED)
def b_flyer(x,y):
    bd.polygon([(x-3,y+10),(x+2,y+14),(x+2,y+20),(x-3,y+24)],fill=K2)
    bd.polygon([(x+BS+2,y+10),(x+BS-3,y+14),(x+BS-3,y+20),(x+BS+2,y+24)],fill=K2)
    b_square(x,y,K1,K0,eyes_dot)
def b_cat(x,y):
    bd.polygon([(x+5,y+4),(x+9,y-5),(x+13,y+4)],fill=K0)
    bd.polygon([(x+18,y+4),(x+22,y-5),(x+26,y+4)],fill=K0)
    bd.polygon([(x+7,y+4),(x+9,y-1),(x+11,y+4)],fill=K4)
    bd.polygon([(x+20,y+4),(x+22,y-1),(x+24,y+4)],fill=K4)
    b_square(x,y,WHT,K0,eyes_sad)
def b_rabbit(x,y):
    bd.rectangle([x+7,y-11,x+11,y+4],fill=K0)
    bd.rectangle([x+20,y-11,x+24,y+4],fill=K0)
    bd.rectangle([x+9,y-8,x+10,y+2],fill=K5)
    bd.rectangle([x+22,y-8,x+23,y+2],fill=K5)
    b_square(x,y,WHT,K0,eyes_dot)
def b_tail(x,y):
    bd.polygon([(x+BS-2,y+16),(x+BS+8,y+9),(x+BS+9,y+13),(x+BS-2,y+21)],fill=K0)
    b_square(x,y,WHT,K0,eyes_normal)
def b_spike(x,y):
    for k in (0,11,22):
        bd.polygon([(x+k,y+BS-3),(x+k+5,y+4),(x+k+10,y+BS-3)],fill=RED)
        bd.polygon([(x+k+2,y+BS-4),(x+k+5,y+9),(x+k+8,y+BS-4)],fill=K7)
def b_door(x,y):
    bd.rectangle([x+1,y-2,x+BS-2,y+BS-3],fill=K0)
    bd.rectangle([x+4,y+1,x+BS-5,y+BS-4],fill=WHT)
    bd.rectangle([x+7,y+4,x+BS-8,y+BS-7],fill=K6)
    bd.rectangle([x+13,y+12,x+18,y+20],fill=K0)
    for k in range(0,3):
        bd.rectangle([x-2-k*3,y+k*4,x-1-k*3,y+BS-6-k*4],fill=K4)
def b_ground(x,y):
    bd.rectangle([x,y,x+BS-1,y+BS-1],fill=K2)
    bd.rectangle([x,y,x+BS-1,y+3],fill=K4)
    bd.rectangle([x+4,y+9,x+11,y+14],fill=K1)
    bd.rectangle([x+19,y+18,x+26,y+23],fill=K1)
def b_plat(x,y):
    bd.rectangle([x,y,x+BS-1,y+7],fill=K4)
    bd.rectangle([x,y,x+BS-1,y+2],fill=K6)
    bd.rectangle([x,y+6,x+BS-1,y+7],fill=K2)
def b_star(x,y):
    bd.rectangle([x+14,y+6,x+18,y+26],fill=K6)
    bd.rectangle([x+6,y+14,x+26,y+18],fill=K6)
def b_fog(x,y):
    bd.ellipse([x+2,y+6,x+BS-3,y+BS-7],fill=(255,255,255,26))

for _i,_f in enumerate([b_player,b_enemy,b_flyer,b_cat,b_rabbit,b_tail,
                        b_spike,b_door,b_ground,b_plat,b_star,b_fog]):
    _x,_y=bcell(_i); _f(_x,_y)
up(blk,BK).save(os.path.join(OUT,"blocks.png"))
print("room/human/blocks 完成")
