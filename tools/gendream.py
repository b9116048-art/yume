#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 梦境关卡生成器 v2 —— 地形高度起伏 + 多层平台 + 坑 + 阶梯
# 可达性保证: 跳跃顶点 2.7 格 / 水平跨距 4.7 格
import os, random

OUT = "/data/data/com.termux/files/home/game/assets/dream1.txt"
W, H = 112, 14
SEED = 20261005
random.seed(SEED)

AIR='.'; GND='#'; PLAT='='; SPIKE='^'; WALK='e'; FLY='E'; DOOR='D'; START='P'
FLOOR_TOP = 13          # 地面最底行

# ---------- 1. 地形高度轮廓 (地面顶行 h[x]) ----------
h = [10] * W
x = 1
levels = [9, 10, 11, 8, 10, 11]
li = 0
while x < W:
    run = random.randint(5, 13)
    lv = levels[li % len(levels)]
    li += 1
    for i in range(x, min(W, x + run)): h[i] = lv
    x += run

# 相邻高度差 <= 2, 保证能跳上去
for i in range(1, W):
    if h[i] - h[i-1] >  2: h[i] = h[i-1] + 2
    if h[i] - h[i-1] < -2: h[i] = h[i-1] - 2

# ---------- 2. 坑 (宽 2~3 格) ----------
gaps = []
x = 16
while x < W - 15:
    w = random.choice([2, 3])
    for i in range(x, min(W, x + w)): h[i] = -1
    gaps.append((x, x + w - 1))
    x += w + random.randint(7, 12)

def near_gap(i, m):
    return any(a - m <= i <= b + m for (a, b) in gaps)

# ---------- 3. 铺地面 ----------
g = [[AIR] * W for _ in range(H)]
for i in range(W):
    if h[i] < 0: continue
    for y in range(h[i], H): g[y][i] = GND

# ---------- 4. 阶梯 (让地面高度真正被用上) ----------
for i in range(1, W):
    if h[i] < 0 or h[i-1] < 0: continue
    d = h[i-1] - h[i]
    if d >= 1:
        for k in range(1, d + 1):
            if 0 <= i - k < W and h[i-k] >= 0:
                for y in range(h[i-k], H): g[y][i-k] = GND
                h[i-k] = h[i]

# ---------- 5. 悬空平台 (可叠二层) ----------
plats = []
x = 20; lastend = -99
while x < W - 14:
    if h[x] < 0 or (x > 0 and h[x-1] < 0) or (x+1 < W and h[x+1] < 0):
        x += 1; continue
    ln = random.randint(3, 5)
    row = h[x] - 2
    if row < 5 or x - lastend < 7:
        x += 1; continue
    for i in range(x, min(W, x + ln)): g[row][i] = PLAT
    plats.append((x, min(W, x+ln) - 1, row))
    lastend = x + ln - 1
    # 二层平台
    if random.random() < 0.45 and row - 2 >= 4:
        x2 = x + random.randint(1, 2)
        ln2 = random.randint(2, 3)
        if x2 + ln2 < W:
            for i in range(x2, x2 + ln2): g[row-2][i] = PLAT
            plats.append((x2, x2 + ln2 - 1, row - 2))
            lastend = max(lastend, x2 + ln2 - 1)
    x += ln + random.randint(6, 10)

# 坑上方架桥 (让坑不只靠跳)
for (a, b) in gaps:
    if random.random() < 0.45 and b + 3 < W and a - 2 >= 0:
        if h[a-2] >= 0:
            row = h[a-2] - 2
            if row >= 5:
                for i in range(a - 1, min(W, b + 2)): g[row][i] = PLAT
                plats.append((a-1, min(W, b+2)-1, row))

def on_plat(i, r):
    return any(a <= i <= b and abs(r - rr) <= 1 for (a, b, rr) in plats)

# ---------- 6. 尖刺 ----------
spikes = []; last = -99
for i in range(13, W - 12):
    if h[i] < 0 or h[i] != h[i-1] or h[i] != h[i+1]: continue
    if near_gap(i, 3) or on_plat(i, h[i]) or i - last < 7: continue
    g[h[i]-1][i] = SPIKE; spikes.append(i); last = i

# ---------- 7. 敌人 ----------
walks = []; last = -99
for i in range(18, W - 13):
    if h[i] < 0: continue
    if not all(h[i+k] == h[i] for k in (-1, 0, 1)): continue
    if near_gap(i, 2) or on_plat(i, h[i]) or i - last < 10: continue
    g[h[i]-1][i] = WALK; walks.append(i); last = i

flys = []; last = -99
for i in range(22, W - 13):
    if h[i] < 0 or near_gap(i, 2) or i - last < 11: continue
    r = h[i] - 3
    if r >= 3 and g[r][i] == AIR and g[r-1][i] == AIR:
        g[r][i] = FLY; flys.append(i); last = i

# ---------- 8. 起点 / 终点 ----------
for i in range(0, 5):
    h[i] = 10
    for y in range(H): g[y][i] = AIR
    for y in range(10, H): g[y][i] = GND
g[9][2] = START
for i in range(W - 8, W):
    h[i] = 10
    for y in range(H): g[y][i] = AIR
    for y in range(10, H): g[y][i] = GND
for i in range(W - 8, W):
    if g[9][i] in (SPIKE, WALK, FLY): g[9][i] = AIR
g[9][W-5] = DOOR

# ---------- 9. 存档点（白纸横杠, 悬浮, 碰到即复活点） ----------
SAVE = 'S'
ckpts = []
for target in (14, 34, 62, 88):
    best = None
    for c in range(target - 6, target + 7):
        if c < 6 or c >= W - 9 or any(abs(c - x) < 6 for x, _ in ckpts): continue
        if h[c] < 0 or h[c] != h[c + 1]: continue
        if g[h[c] - 1][c] != AIR: continue
        if any(0 <= c + k < W and g[r][c + k] in (SPIKE, WALK, FLY) for k in range(-2, 3) for r in range(H)): continue
        d = abs(c - target)
        if best is None or d < best[0]: best = (d, c)
    if best:
        c = best[1]; g[h[c] - 1][c] = SAVE; ckpts.append((c, h[c] - 1))
print("  存档点列:", [x for x, _ in ckpts])

with open(OUT, "w", encoding="utf-8") as f:
    f.write("; dream 1  seed=%d  %dx%d\n" % (SEED, W, H))
    for row in g: f.write("".join(row) + "\n")

solid = sum(1 for i in range(W) if h[i] >= 0)
print("dream1.txt  %dx%d  有地面列 %d  地形高度范围 %d..%d" % (
    W, H, solid, min([v for v in h if v >= 0]), max(h)))
print("  坑%d 尖刺%d 平台%d 地面敌%d 飞行敌%d" % (
    len(gaps), len(spikes), len(plats), len(walks), len(flys)))
print("  坑宽 %s   平台层高 %s" % ([b-a+1 for (a,b) in gaps], sorted(set(r for (_,_,r) in plats))))
print("-" * W)
for r in g: print("".join(r))
