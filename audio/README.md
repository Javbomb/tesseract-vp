# Tesseract VP lossless audio

TrueHD Atmos (as MAT), DTS-HD MA and DTS:X bitstream from the Fire TV Cube 3 to an AV receiver, from any app that outputs it, on Fire OS 7 (PS7718).

## How it works

`audio.primary.tesseract.so` is a small wrapper around the stock Amlogic primary audio HAL. The Magisk module `tesseract_audio` adds it to `/vendor/lib/hw` and sets `ro.hardware.audio.primary=tesseract` before the audio service starts. libhardware then loads the wrapper. The wrapper dlopens the untouched stock `audio.primary.amlogic.so` and swaps a few function pointers on the structs that HAL hands back.

- No stock file is changed, and no code is patched.
- Nothing runs between calls: no thread, no polling, no Frida, no ptrace.
- Video and DRM are not touched.

| Piece | What the stock Cube does | What the wrapper does |
|---|---|---|
| **Bypass** | Fire OS sends `hdmi_format=5` (Best Available) on every output open and close. The HAL then decodes or re-encodes lossless audio. | While the sink's EDID lists any bitstream format, `5` is rewritten to `6` (bypass) in `adev->set_parameters`. PCM (`0`) and Dolby Digital (`4`) are left as the user chose them. A PCM-only TV keeps `5`, so DD+ is still decoded for it. |
| **Voice DSP reference** | The far-field mic's echo reference is pointed at the HDMI TDM bus and stays there through clock changes, close and sleep. The voice DSP's capture then fails (`pcm_read -16` on card1), and vizzini, the audio HAL and logd spin at several cores. | The HAL's and voice library's own imports of `pcm_open`, `pcm_write`, `pcm_close` and `asp_command` go through the wrapper. Only those libraries' GOT entries are changed. The reference sits on the TDM-A dummy bus (`0/48000/8`) except while an HDMI output is actually writing. |
| **Raw DTS-HD** | Apps that hand Android raw `ENCODING_DTS_HD` (Plex, Jellyfin, most ExoPlayer apps, Kodi's "RAW" device) get a 48 kHz carrier, which only fits the DTS core. | The stream is opened on the stock HAL as IEC 61937 at 192 kHz 7.1, the same path Kodi's own IEC packer uses. The wrapper packs each access unit into an IEC 61937-5 type IV (0x11) HBR burst. Positions are scaled back to the content rate. This happens only when the sink lists DTS-HD 8 ch at 192 kHz. |
| **What is playing** | Nothing says which format leaves the Cube; amhdmitx calls any HBR stream "MAT". | `sniff.c` reads the headers of the first bytes of each bitstream output and of the HDMI device after each open: TrueHD Atmos (4 substreams with the 16-channel presentation), DD+ Atmos (E-AC-3 `extension_type_a`), DTS:X (the XLL X word). Results go to `vendor.media.audio.tvp.out` (the app's stream) and `vendor.media.audio.tvp.wire` (what the receiver gets), which Tesseract VP's control page shows. Nothing is stored; at most 4 MB is looked at per open. |

Kodi and VLC in IEC mode already pack their own bursts (`ENCODING_IEC61937`, 192 kHz). For them only the bypass and the DSP fix matter. Raw TrueHD stays on the stock HAL's MAT encoder.

Switches (read once per audio-service start): `persist.vendor.tvp.audio` takes a comma list of `nobypass`, `nodsp` and `nodtshd`. Leave it empty for all three on.

## Build, install, undo

- **Build:** `bash build.sh` (Android NDK, ARM32, API 28). It writes `out/tesseract-audio.zip`.
- **Install:** Tesseract VP Setup installs it, active after a restart, or starts it at once with its **Lossless audio passthrough** button, restarting the audio service once (about 2 s of silence) when nothing is playing.
- **On the Cube:** `sh /data/adb/cubevp/audio.sh install|live|status|remove|ftvav-on|ftvav-off`.
- **Undo:** run Remove, then restart. The property and mount go away with the restart, and nothing outside the module persists.
- **Crash guard:** three boots in a row that don't finish leave the stock HAL in place.
- **Fire TV AV:** not needed by anything. Installing this module turns off signde's Fire TV AV audio module (`disable` file), because both would steer the same things. **Remove and turn Fire TV AV back on** reverses that.

## Checking it

- `logcat -s TesseractAudio` shows:
  - the wrapped device;
  - each bypass rewrite (first three, then every 256th);
  - DSP reference moves;
  - `dtshd:` stream lines, with byte, burst and error counts at close.
- `cat /proc/asound/card0/pcm1p/sub0/hw_params` shows HDMI at `rate: 192000`, `channels: 8` during TrueHD or DTS-HD MA.

Credit: signde's Fire TV AV (MIT) found the `hdmi_format` 5→6 retention and the DSP reference lifecycle. The code here is separate, written from IEC 61937-5 and the Android HAL headers.

## Licence

MIT (see `LICENSE`). Tesseract VP's audio module is open source so anyone can read, check and reuse it. The rest of Tesseract VP, apart from its GPL kernel modules, isn't covered by this licence.
