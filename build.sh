#!/data/data/com.termux/files/usr/bin/bash
set -e
R=$HOME/game; P=/data/data/com.termux/files/usr; S=$R/sdk/stubs; B=$R/build; STB=$HOME/_net/stb-master
mkdir -p $B/lib $B/cls $B/dex $R/out
echo "[1/6] generate assets"
python3 $R/tools/gendream.py > /dev/null
python3 $R/tools/genui.py
python3 $R/tools/genfont.py
echo "[2/6] compile native"
clang++ -std=c++17 -shared -fPIC -O2 -nostdlib++ -fno-exceptions -fno-rtti -fvisibility=hidden \
  -Wall -Wno-unused-parameter -Wno-unused-variable \
  -I$R/src -I$STB -L$S -L$P/lib -o $B/lib/libgame.so \
  $R/src/main.cpp $R/src/gfx.cpp $R/src/game.cpp $R/src/scenes.cpp \
  -landroid -llog -lm -lEGL -lGLESv3
echo "[3/6] dex"
javac --release 8 -nowarn -d $B/cls $R/java/dev/mcp/gfx/Boot.java $R/java/dev/mcp/gfx/Immersive.java
d8 --min-api 24 --output $B/dex $B/cls/dev/mcp/gfx/Boot.class $B/cls/dev/mcp/gfx/Immersive.class
echo "[4/6] aapt2 link"
aapt2 compile --dir $R/res -o $B/res.zip
aapt2 link -o $B/base.apk -I /system/framework/framework-res.apk \
  --manifest $R/AndroidManifest.xml --min-sdk-version 24 --target-sdk-version 31 \
  --version-code 9 --version-name 2.4 -A $R/assets -R $B/res.zip
echo "[5/6] pack + align"
rm -rf $B/stage; mkdir -p $B/stage/lib/arm64-v8a
cp $B/lib/libgame.so $B/stage/lib/arm64-v8a/
cp $B/dex/classes.dex $B/stage/
cp $B/base.apk $B/unsigned.apk
(cd $B/stage && zip -0 -X ../unsigned.apk classes.dex lib/arm64-v8a/libgame.so >/dev/null)
zipalign -f -p 4 $B/unsigned.apk $B/aligned.apk
echo "[6/6] sign"
apksigner sign --ks $R/debug.keystore --ks-pass pass:android --key-pass pass:android \
  --ks-key-alias gfx --out $R/out/yume.apk $B/aligned.apk
apksigner verify $R/out/yume.apk
ls -l $R/out/yume.apk
