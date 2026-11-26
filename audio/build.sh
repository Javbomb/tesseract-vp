#!/usr/bin/env bash
# Builds audio.primary.tesseract.so (ARM32, API 28) and the Magisk module zip.
# Git Bash on Windows or Linux with the Android NDK. Output: audio/out/
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
NDK="${ANDROID_NDK_HOME:-}"
if [ -z "$NDK" ]; then
  for d in "$LOCALAPPDATA/Android/Sdk/ndk"/* "$HOME/Android/Sdk/ndk"/* /c/Users/*/AppData/Local/Android/Sdk/ndk/*; do
    [ -d "$d/toolchains" ] && NDK="$d"
  done
fi
[ -n "$NDK" ] || { echo "Android NDK not found (set ANDROID_NDK_HOME)"; exit 1; }
BIN=$(ls -d "$NDK"/toolchains/llvm/prebuilt/*/bin | head -1)
CC="$BIN/armv7a-linux-androideabi28-clang"
[ -x "$CC" ] || CC="$CC.cmd"
OUT="$HERE/out"
rm -rf "$OUT"; mkdir -p "$OUT"
"$CC" -shared -fPIC -O2 -Wall -Wextra -Wno-unused-parameter -fvisibility=hidden \
  -Wl,-soname,audio.primary.tesseract.so -Wl,--no-undefined -Wl,-z,relro -Wl,-z,now \
  -o "$OUT/audio.primary.tesseract.so" "$HERE/tvp_audio.c" "$HERE/iec_dtshd.c" "$HERE/sniff.c" -llog -ldl
"$BIN/llvm-strip" --strip-unneeded "$OUT/audio.primary.tesseract.so"
"$CC" -O2 -Wall -Wextra -fPIE -pie -o "$OUT/tvp_audio_watch" "$HERE/tvp_audio_watch.c" -llog
"$BIN/llvm-strip" "$OUT/tvp_audio_watch"

SDK=$(dirname "$(dirname "$NDK")")
JAVAC=$(command -v javac || ls "/c/Program Files/Android/Android Studio/jbr/bin/javac.exe" 2>/dev/null || true)
D8=$(ls "$SDK"/build-tools/*/d8* 2>/dev/null | grep -E 'd8(\.bat)?$' | tail -1)
JAR=$(ls "$SDK"/platforms/android-*/android.jar | tail -1)
[ -n "$JAVAC" ] && [ -n "$D8" ] || { echo "javac or d8 not found"; exit 1; }
export JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$JAVAC")")}"
mkdir -p "$OUT/java"
"$JAVAC" -source 8 -target 8 -nowarn -Xlint:-options -cp "$JAR" -d "$OUT/java" "$HERE/HdmiRefresh.java"
( cd "$OUT/java" && "$D8" --min-api 28 --lib "$JAR" --output . HdmiRefresh.class )

M="$OUT/module"
mkdir -p "$M/system/vendor/lib/hw"
cp "$HERE"/module/* "$M/"
cp "$OUT/audio.primary.tesseract.so" "$M/system/vendor/lib/hw/"
cp "$OUT/tvp_audio_watch" "$M/"
cp "$OUT/java/classes.dex" "$M/hdmi_refresh.dex"
( cd "$M" && sha256sum system/vendor/lib/hw/audio.primary.tesseract.so > payload.sha256 )
# zip with forward-slash paths and LF scripts (Magisk unzips it on the Cube)
( cd "$M" && python -I -c "
import os, zipfile, sys
z = zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED)
for root, dirs, files in os.walk('.'):
    for f in sorted(files):
        path = os.path.join(root, f)
        z.write(path, os.path.relpath(path, '.').replace(os.sep, '/'))
z.close()" "$OUT/tesseract-audio.zip" )
ls -la "$OUT"
