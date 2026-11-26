#!/system/bin/sh
MODDIR=${0%/*}
[ -e "$MODDIR/disable" ] && exit 0
[ -f "$MODDIR/system/vendor/lib/hw/audio.primary.tesseract.so" ] || exit 0
[ -f /vendor/lib/hw/audio.primary.amlogic.so ] || exit 0
n=$(cat "$MODDIR/boots" 2>/dev/null || echo 0)
if [ "$n" -ge 3 ]; then echo "GUARD: 3 unfinished boots; stock HAL" > "$MODDIR/status.txt"; exit 0; fi
echo $((n + 1)) > "$MODDIR/boots"
resetprop ro.hardware.audio.primary tesseract
