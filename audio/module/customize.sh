[ "$(getprop ro.product.device)" = gazelle ] || abort "! Fire TV Cube 3 (gazelle) only"
[ -f /vendor/lib/hw/audio.primary.amlogic.so ] || abort "! stock audio HAL not found"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/system/vendor/lib/hw/audio.primary.tesseract.so" 0 0 0644 u:object_r:vendor_file:s0
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/tvp_audio_watch" 0 0 0755
ui_print "- Tesseract VP lossless audio installed; active after a restart"
