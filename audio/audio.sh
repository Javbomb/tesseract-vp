#!/system/bin/sh
# Lossless audio module (audio/README.md). Root. install <zip> | remove | live | refresh | status | ftvav-on | ftvav-off | ftvav-remove
M=/data/adb/modules/tesseract_audio
FTV=/data/adb/modules/firetv_passthrough
FTVB=/data/adb/cubevp/firetv_av_backup.tar
BB=/data/adb/magisk/busybox
HAL=fireos.hardware.audio@2.0-service
LIB=audio.primary.tesseract.so

hal_pid() { pidof $HAL; }
loaded() { p=$(hal_pid); [ -n "$p" ] && grep -q $LIB /proc/$p/maps 2>/dev/null; }

watch_start() {
	D=$M; [ -f /data/adb/modules_update/tesseract_audio/tvp_audio_watch ] && D=/data/adb/modules_update/tesseract_audio
	kill $(pidof tvp_audio_watch) 2>/dev/null
	[ -x $D/tvp_audio_watch ] && [ -f $D/hdmi_refresh.dex ] && $D/tvp_audio_watch $D/hdmi_refresh.dex
}

playing() {
	dumpsys media_session 2>/dev/null | grep -q 'state=PlaybackState {state=3'
}

ftvav_off() {
	[ -d $FTV ] || return 0
	touch $FTV/disable
	for p in $(ps -A -o PID,ARGS | grep -E "^ *[0-9]+ ([f]rida-inject|busybox sh /data/adb/modules/[f]iretv_passthrough/service.sh|logcat .*[p]ower_screen_state:I boot_progress)" | awk '{print $1}'); do kill $p 2>/dev/null; done
	echo "Fire TV AV turned off (undo: sh audio.sh ftvav-on, then restart)"
}

case "$1" in
install)
	[ -f "$2" ] || { echo "no module zip: $2"; exit 1; }
	[ "$(getprop ro.product.device)" = gazelle ] || { echo "Fire TV Cube 3 only"; exit 1; }
	magisk --install-module "$2" >/dev/null 2>&1 || { echo "Magisk could not install the audio module"; exit 1; }
	rm -f $M/disable $M/remove $M/boots
	ftvav_off
	echo "lossless audio module installed (active after a restart, or: sh audio.sh live)"
	;;
remove)
	[ -d $M ] && touch $M/remove
	resetprop --delete ro.hardware.audio.primary 2>/dev/null
	kill $(pidof tvp_audio_watch) 2>/dev/null
	echo "lossless audio module removed at the next restart (Fire TV's own audio until then is unchanged)"
	;;
live)
	SRC=$M/system/vendor/lib/hw/$LIB
	[ -f /data/adb/modules_update/tesseract_audio/system/vendor/lib/hw/$LIB ] && SRC=/data/adb/modules_update/tesseract_audio/system/vendor/lib/hw/$LIB
	[ -f $SRC ] || { echo "not installed"; exit 1; }
	loaded && cmp -s $SRC /vendor/lib/hw/$LIB && { pidof tvp_audio_watch >/dev/null || watch_start; echo "already active"; exit 0; }
	playing && { echo "something is playing; not restarting the audio service now"; exit 1; }
	ftvav_off
	if ! cmp -s $SRC /vendor/lib/hw/$LIB; then
		NS="/data/adb/magisk/busybox nsenter -t 1 -m --"
		grep -q ' /vendor/lib/hw ' /proc/mounts && $NS umount -l /vendor/lib/hw
		S=/dev/tvp_audio_hw.$$
		mkdir $S && cp -p /vendor/lib/hw/* $S/ && cp $SRC $S/$LIB.new && mv $S/$LIB.new $S/$LIB || { rm -rf $S; echo "copy failed"; exit 1; }
		[ "$(ls $S | wc -l)" -ge "$(ls /vendor/lib/hw | wc -l)" ] && cmp -s $SRC $S/$LIB && [ -f $S/audio.primary.amlogic.so ] || { rm -rf $S; echo "copy incomplete"; exit 1; }
		chmod 644 $S/* && chcon u:object_r:vendor_hal_file:s0 $S && chcon u:object_r:vendor_file:s0 $S/* || { rm -rf $S; echo "label failed"; exit 1; }
		$NS mount --bind $S /vendor/lib/hw || { rm -rf $S; echo "bind mount failed"; exit 1; }
	fi
	resetprop ro.hardware.audio.primary tesseract
	kill -9 $(hal_pid) 2>/dev/null
	i=0; while [ $i -lt 20 ]; do sleep 1; i=$((i + 1)); loaded && break; done
	if loaded; then echo "active (audio service pid $(hal_pid))"; else echo "NOT active: audio service $(hal_pid)"; exit 1; fi
	watch_start
	;;
refresh)
	CLASSPATH=$M/hdmi_refresh.dex app_process /system/bin HdmiRefresh
	r=$?; [ $r = 0 ] && echo "Android re-read the HDMI audio formats" || echo "not refreshed ($r: 2 no HDMI, 3 playing)"
	;;
status)
	p=$(hal_pid)
	echo "module: $([ -d $M ] && cat $M/module.prop | sed -n 's/^version=//p' || echo none) $([ -e $M/disable ] && echo '(disabled)') $(cat $M/status.txt 2>/dev/null)"
	echo "prop: ro.hardware.audio.primary=$(getprop ro.hardware.audio.primary)"
	echo "audio service pid $p: $(loaded && echo 'Tesseract VP wrapper loaded' || echo 'stock HAL only')"
	[ -n "$p" ] && echo "audio service: $(grep -E 'VmRSS' /proc/$p/status | tr -s ' ') $(grep -q frida /proc/$p/maps 2>/dev/null && echo '+ frida agent (Fire TV AV)')"
	echo "format watcher: $(pidof tvp_audio_watch >/dev/null && echo running || echo 'not running')"
	echo "Fire TV AV: $([ -d $FTV ] && { [ -e $FTV/disable ] && echo 'installed, off' || echo 'installed, ON'; } || echo 'not installed')"
	echo "hdmi_audio switch: $(cat /sys/class/switch/hdmi_audio/state 2>/dev/null)"
	echo "sink: $(grep -v CodingType /sys/class/amhdmitx/amhdmitx0/aud_cap 2>/dev/null | cut -d, -f1-3 | tr '\n' ';')"
	for h in /proc/asound/card0/pcm1p/sub0/hw_params; do echo "hdmi pcm: $(tr '\n' ' ' < $h 2>/dev/null)"; done
	logcat -d -s TesseractAudio 2>/dev/null | tail -5
	;;
ftvav-off) ftvav_off ;;
ftvav-on)
	if [ ! -d $FTV ]; then
		[ -f $FTVB ] || { echo "Fire TV AV is not installed and there is no backup ($FTVB)"; exit 1; }
		$BB tar -xf $FTVB -C /data/adb/modules || { echo "restore failed"; exit 1; }
	fi
	rm -f $FTV/disable $FTV/remove
	[ -d $M ] && touch $M/disable
	kill $(pidof tvp_audio_watch) 2>/dev/null
	echo "Fire TV AV on after a restart (ours turned off; undo: install ours again)"
	;;
ftvav-remove)
	[ -d $FTV ] || { echo "Fire TV AV not installed"; exit 0; }
	ftvav_off >/dev/null
	rm -f $FTV/disable
	$BB tar -cf $FTVB -C /data/adb/modules firetv_passthrough || { echo "backup failed; not removing"; exit 1; }
	touch $FTV/disable $FTV/remove
	echo "Fire TV AV removed at the next restart (backup: $FTVB; ftvav-on puts it back)"
	;;
*) echo "usage: sh audio.sh install <zip> | remove | live | status | ftvav-on | ftvav-off | ftvav-remove"; exit 1 ;;
esac
