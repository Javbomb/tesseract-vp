# Tesseract VP roadmap

*Updated 25 November 2026.*

This roadmap takes Tesseract VP from a tone-mapping box to a full video processor. Phases are listed in the order we work on them. Most of the processing runs in the Cube's video hardware, so features are built to cost little or no CPU while a film plays.

**Key:**
- ✅ **Live**: in the current release.
- 🔶 **Partly**: built or partly live; testing or the rest to come.
- ⬜ **Planned**: designed, not built yet.

---

## Phase 0: Tone mapping (where it started)

| | Feature | Details |
|---|---|---|
| ✅ | **Dynamic tone mapping with look-ahead** | <ul><li>Every frame is measured and gets its own curve.</li><li>Films are measured several frames before they are shown, so scene cuts never flash or pump.</li><li>The full-resolution peak of the frame on screen is checked too, so small highlights are never missed.</li></ul> |
| ✅ | **Faithful to the grade** | <ul><li>Shadows and mid-tones stay 1:1. Only light the screen can't show is brought into range.</li><li>No clipping, no flicker. Highlights whiten gently, without shrinking the gamut.</li></ul> |
| ✅ | **Netflix, Disney+ and YouTube in 4K HDR** | Tone mapped like any other source, with a choice for Dolby Vision titles: our tone mapping, Dolby's own mapping, or Dolby Vision out to the display. |
| ✅ | **Look-ahead for protected video** | <ul><li>DRM frames can't be read by software, so they are measured in the video hardware, ahead of the screen.</li><li>Disney+ 4K: 99.5 % of frames reached the screen with their own curve. Netflix: 95–97 %.</li><li>4K kept and no protection errors on either.</li></ul> |
| ✅ | **HDR10+ films** | Tone mapped from their HDR10 base, like any HDR10 film. |
| ✅ | **Picture controls** | <ul><li>Brightness, highlights and shadows.</li><li>Highlight colour and adaptation speed.</li><li>Scene brightness, a clip guard and more under Advanced.</li></ul> |
| ✅ | **Auto black floor** | Films mastered with a raised black are corrected gently. The floor is learned over the whole title, never shot by shot, so foggy or night scenes stay as graded. |

## Phase 1: Games and output

| | Feature | Details |
|---|---|---|
| ✅ | **HDMI input, HDR10** | A console on the HDMI input is tone mapped live, each frame from itself. |
| ✅ | **Low latency** | <ul><li>About 7 ms from a frame's arrival to the screen in game mode, against 41 ms with look-ahead.</li><li>**Auto low latency (ALLM)** follows the console's flag.</li></ul> |
| ✅ | **Genlock** | <ul><li>The output follows the HDMI input's timing exactly, so no frame is ever dropped or shown twice.</li><li>It locks within about 40 s.</li></ul> |
| ✅ | **EDID builder** | <ul><li>You choose what the source is told: resolutions, refresh rates, HDR formats, peak and black, 4:2:0, bit depth and ALLM.</li><li>It is built from the receiver's EDID, so sound formats and Atmos pass untouched.</li></ul> |
| ✅ | **Output menu** | <ul><li>Colour format, bit depth, resolution, refresh rate and colour space, applied over whatever the app picks.</li><li>A mode the display can't carry is lowered automatically to what HDMI 2.0 supports.</li></ul> |
| ✅ | **Frame-rate matching** | <ul><li>Each film plays at its own rate, read before its first frame (23.976 and 24.000 told apart).</li><li>Streaming apps are included, with the sound kept through the switch.</li><li>At film rates the output goes to 4:4:4 12-bit in the same switch.</li></ul> |
| 🔶 | **Frame-rate matching for protected apps, through any receiver** | <ul><li>A switch now waits until the app's DRM sees the HDMI link protected again, and the picture holds for under 3 s.</li><li>Disney+ switches on every chain, with Atmos kept.</li><li>Netflix switches where the HDMI handshake is quick (straight into a display, or a fast receiver: 23.976, 24 and 25 tested in 4K with Atmos). Through slower receivers it stays at the usual rate by itself, or you can choose **Always try**.</li><li>Next: the right rate set before the film starts, for every receiver.</li></ul> |
| ✅ | **Colour space and gamut** | <ul><li>**BT.2020 out:** menus, SDR and console pictures are converted in without oversaturation.</li><li>**BT.709 out:** films' wider colours are mapped in along constant hue, never clipped.</li><li>**Auto:** each signal is flagged as what it is.</li></ul> |
| ✅ | **Profiles with automatic rules** | Game, HDMI, Film, Video and SDR each load their own preset when the source changes. |
| ✅ | **LLDV** | Dolby Vision from apps and Kodi arrives as low-latency Dolby Vision, and our tone mapping finishes it at 24 fps, zoom included. |
| ✅ | **Dolby Vision, three modes** | <ul><li>**Off (HDR10):** our tone mapping.</li><li>**LLDV:** Dolby's processing, finished by our tone mapping.</li><li>**Dolby Vision:** Dolby's processing end to end, to a Dolby Vision display.</li></ul>Live for streaming apps and Kodi, through an AV receiver too. The HDMI input takes HDR10. |
| ⬜ | **VRR pass-through and glass-to-glass lag measurement** | |

## Phase 2: Picture quality

| | Feature | Details |
|---|---|---|
| 🔶 | **Upscaling 1080p → 4K** | <ul><li>A choice of upscalers: the super-scaler, Bicubic, Lanczos 3, Lanczos 4 and Soft.</li><li>Switched live in the hardware, at no CPU cost.</li><li>Next: pick the best default on a resolution chart.</li></ul> |
| 🔶 | **Chroma upsampling** | 4K films' colour is interpolated up to full resolution instead of repeated, and goes out at 4:4:4 12-bit. |
| 🔶 | **Colour upgrade for 1080p** | <ul><li>1080p films' colour is rebuilt to full 4:4:4 on the Cube's GPU.</li><li>It runs for films up to 30 fps, and steps aside by itself if it can't keep up.</li><li>Built. Testing live.</li></ul> |
| ⬜ | **Downscaling 4K → 1080p** | Anti-aliased Sharp, Balanced and Soft settings. 4K's colour lands as true 4:4:4 1080p. |
| ⬜ | **Contrast and shadow detail** | Local contrast and shadow-separation options for the curve. |
| ⬜ | **Sharpening and detail** | Within what the scaler and post-processing hardware offer. Off by default. |
| ⬜ | **Debanding** | Smooths 8- and 10-bit gradient banding without softening edges. A feasibility study comes first. |

## Phase 3: Every picture fitted to the screen

| | Feature | Details |
|---|---|---|
| ✅ | **Black bar detection** | <ul><li>Every source: apps, Kodi, the HDMI input, protected streams and Dolby Vision, FEL included.</li><li>Films are measured before they are shown, so a change of shape (an IMAX scene) is met on the frame it happens.</li><li>Subtitles in a bar don't fool it. Fast and Precise modes.</li></ul> |
| ✅ | **Aspect menu** | <ul><li>Fill, and picture to the top or bottom.</li><li>Anamorphic lenses (1.25, 1.33 and 1.5x).</li><li>Masking for each edge.</li></ul>It applies to every source and changes on the exact frame the film's shape does. |
| ✅ | **Scope screens** | <ul><li>Scope zoom for 2.35, 2.37 and 2.40 screens.</li><li>Non-linear stretch for 16:9 through an anamorphic lens.</li><li>"Only for pictures wider than…" rules and a manual aspect override.</li></ul> |
| ✅ | **Subtitles kept in view** | Burned-in subtitles in a bar are recognised, and Fill keeps just enough of the bar for them. |
| 🔶 | **Geometry correction** | <ul><li>Warp the picture onto the screen: keystone, curved screens, and pincushion or barrel from a lens.</li><li>Uses a 12-point or finer grid, in the Cube's own warp hardware.</li><li>In development and testing.</li></ul> |
| ⬜ | **Lens memory automation** | Recall the projector's lens memory when the aspect ratio changes. |
| ⬜ | **Convergence correction** | Red, green and blue each shifted by fractions of a pixel, zone by zone across the screen. |

## Phase 4: Control and devices

| | Feature | Details |
|---|---|---|
| ✅ | **Control page** | <ul><li>Tone Mapping, Aspect, Input & Output, Calibration and Profiles.</li><li>The live picture, waveform and tone curve stay in view, so every change shows live.</li></ul> |
| ✅ | **On-screen menu and settings page** | Everything on the control page is on the remote too, and a full-screen settings page runs on the Cube itself. |
| ✅ | **Phone app** | <ul><li>Finds every Tesseract VP on the Wi-Fi and opens its control page or on-screen display.</li><li>**One-tap scenes:** Movie night wakes the projector and amp, picks the input and dims the lights; Off puts it all to sleep. From the app, a home-screen widget or a quick-settings tile.</li></ul> |
| ✅ | **HDMI-CEC for every device** | Each device's power, name and input, plus the amp's volume. One press of the remote's power button wakes or sleeps the display and amp with the Cube. |
| ✅ | **Launcher** | A new home screen: the glowing cube with the Cube's name, a clock with your HDMI devices, your own apps on the home row, a Last played strip and one Settings tile for everything. |
| ✅ | **API and webhooks** | Events for Home Assistant or any webhook: source, playing, HDR type, output mode, picture shape. The whole network API is documented. |
| ✅ | **Several Cubes** | Every Tesseract VP on the network, one click away from the control page. |
| 🔶 | **IR devices and room automation** | <ul><li>Learn any IR remote's buttons, and send them through the Cube's IR blaster.</li><li>Tie them to what plays: lights off when a film starts.</li><li>Built. Testing.</li></ul> |

## Phase 5: Installing and updating

| | Feature | Details |
|---|---|---|
| ✅ | **Tesseract VP Setup** | <ul><li>A Windows app that prepares, backs up, unlocks and installs a Cube, with a button for every step.</li><li>Display setup: peak, colour space and receiver.</li><li>Restore, roll back and uninstall.</li></ul> |
| ✅ | **Signed self-updates** | <ul><li>Cubes update themselves from this repository's Stable or Nightly releases.</li><li>Every release is signed and checked before it installs.</li></ul> |
| ✅ | **Safe updates** | <ul><li>One-step roll back.</li><li>A watchdog that puts the previous version back if a new one keeps failing.</li><li>Backup and restore of every setting.</li></ul> |

## Phase 6: Calibration

| | Feature | Details |
|---|---|---|
| ✅ | **Fine calibration controls** | <ul><li>Black level, white level and gamma.</li><li>Red, green and blue gain and offset.</li></ul>Steps are far finer than a display's own, held for every source. |
| ✅ | **Output range** | Limited or Full, with the HDMI flag to match. |
| 🔶 | **Gamut guardrail** | BT.709 and BT.2020 are live. P3, and a display's measured primaries, are next. |
| ✅ | **Built-in test patterns** | Ramps, windows, average-light patches and single-pixel 4K checks, in SDR and HDR, shown tone mapped or untouched. |
| ✅ | **Separate SDR and HDR calibration** | Each has its own calibration set, picked by what is playing. |
| ⬜ | **LUT upload and calibration software** | Upload a 3D LUT, and drive the patterns from common calibration software. |
| ⬜ | **Greyscale (1D) correction** | A separate greyscale and gamma table in front of the 3D LUT. |
| ⬜ | **Dynamic contrast guard** | For displays with dynamic contrast or laser and backlight dimming: leave room in dark scenes so their brightness lift never clips highlights. |

## Phase 7: Beyond

| | Feature | Details |
|---|---|---|
| ⬜ | **Two-projector stacking and split tone mapping** | <ul><li>Two projectors, each fed by its own Cube.</li><li>**Stacked:** both show the whole picture, for twice the light.</li><li>**Split:** one carries the picture and the other only the highlights. They add back to the full picture with colours unchanged.</li></ul> |
| ✅ | **Dolby Vision with full enhancement layer (FEL)** | <ul><li>Full FEL films rebuilt to their 12-bit picture, in a player that supports it (Kodi FEL, VLC FEL, Wholphin FEL).</li><li>Sent as Dolby Vision to the display, or tone mapped by us.</li></ul> |
| 🔶 | **Lossless audio from any app** | <ul><li>TrueHD Atmos and DTS:X Master go to the receiver untouched (tested from Kodi).</li><li>Our own built-in version is in development.</li></ul> |
