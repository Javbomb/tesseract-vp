#!/system/bin/sh
MODDIR=${0%/*}
i=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ $i -lt 150 ]; do sleep 2; i=$((i + 1)); done
rm -f "$MODDIR/boots"
[ -e "$MODDIR/disable" ] && { echo "DISABLED" > "$MODDIR/status.txt"; exit 0; }
sleep 10
pid=$(pidof fireos.hardware.audio@2.0-service)
if [ -n "$pid" ] && grep -q audio.primary.tesseract.so /proc/$pid/maps 2>/dev/null; then
  echo "ACTIVE pid=$pid $(date '+%Y-%m-%d %H:%M')" > "$MODDIR/status.txt"
else
  echo "NOT_LOADED pid=$pid prop=$(getprop ro.hardware.audio.primary)" > "$MODDIR/status.txt"
fi
# Android caches HDMI audio formats at connect; re-read them whenever the receiver's change
[ -x "$MODDIR/tvp_audio_watch" ] && [ -f "$MODDIR/hdmi_refresh.dex" ] && "$MODDIR/tvp_audio_watch" "$MODDIR/hdmi_refresh.dex"
