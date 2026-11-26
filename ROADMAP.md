# Tesseract VP roadmap

*Updated 27 November 2026: 3D Blu-ray is in development; two-Cube 3D and stacking, profiles per display and 4096x2160 output are planned.*

This roadmap takes Tesseract VP from a tone-mapping box to a full video processor. Phases are listed in the order we work on them. Most of the processing runs in the Cube's video hardware, so features are built to cost little or no CPU while a film plays.

**Key:**
- ✅ **Live**: in the current release.
- 🔶 **Partly**: built or partly live; testing or the rest to come.
- ⬜ **Planned**: designed, not built yet.
- ➖ **Not possible** on this hardware.

---

## Phase 0: Tone mapping (where it started)

| | Feature | Details |
|---|---|---|
| ✅ | **Dynamic tone mapping with look-ahead** | <ul><li>Every frame is measured and gets its own curve.</li><li>Films are measured several frames before they are shown, so scene cuts don't flash or pump.</li><li>The full-resolution peak of the frame on screen is checked too, so small highlights aren't missed.</li></ul> |
| ✅ | **1:1 wherever it can be** | <ul><li>A scene that fits the screen is shown 1:1, as mastered. One that doesn't is compressed to fit, as gently as possible: as much of the picture as it can stays 1:1, and the rest, mid-tones included when the scene is far brighter than the screen, is brought down smoothly.</li><li>Highlights roll off smoothly instead of hard clipping, the curve moves gently between scenes, and highlights whiten gently without shrinking the gamut.</li></ul> |
| ✅ | **Netflix, Disney+ and YouTube in 4K HDR** | Tone mapped like any other source, with a choice for Dolby Vision titles: our tone mapping, Dolby's own mapping, or Dolby Vision out to the display. |
| ✅ | **Look-ahead for protected video** | <ul><li>DRM frames can't be read by software, so they are measured in the video hardware, ahead of the screen.</li><li>Disney+ 4K: 99.5 % of frames reached the screen with their own curve. Netflix: 95–97 %.</li><li>4K kept and no protection errors on either.</li></ul> |
| ✅ | **Subtitles left out of the measurement** | Burned-in white or yellow subtitles at the bottom of the picture, and in the bars, don't count towards the scene's brightness, so they don't dim the film. On by default. |
| ✅ | **HDR10+ films** | Tone mapped from their HDR10 base, like any HDR10 film. |
| ✅ | **SDR or HDR10 out** | The tone-mapped picture goes out as SDR, or as HDR10 aimed at the display's peak. In HDR10, menus and subtitles are converted in at graphics white. |
| ✅ | **Highlights and shadows refined** | <ul><li>Highlights keep their shape right up to the screen's peak.</li><li>Deep shadows stay deep, and bright scenes keep their shadow detail.</li><li>Highlight colour is fitted to the gamut, so saturated skin doesn't band.</li></ul> |
| ✅ | **Picture controls** | <ul><li>Brightness, Punch, Highlights, Highlight colour, Shadows, Depth, White-hot and Clip guard, with the tuned setting in the middle of each.</li><li>Every change ramps in smoothly, with no flicker.</li><li>Scene brightness and adaptation speed.</li><li>Further fine controls in the Detail view.</li></ul> |
| ✅ | **Auto black floor** | Films mastered with a raised black are corrected gently. The floor is learned over the whole title, never shot by shot, so foggy or night scenes stay as graded. |

## Phase 1: Games and output

| | Feature | Details |
|---|---|---|
| ✅ | **HDMI input, HDR10** | A console on the HDMI input is tone mapped live, each frame from itself. |
| ✅ | **Low latency** | <ul><li>About 7 ms from a frame's arrival to the screen in game mode, against 41 ms with look-ahead.</li><li>**Auto low latency (ALLM)** follows the console's flag.</li></ul> |
| ✅ | **Genlock** | <ul><li>The output follows the HDMI input's timing exactly, so frames aren't dropped or repeated once it has locked.</li><li>It locks within about 40 s.</li></ul> |
| ✅ | **EDID builder** | <ul><li>You choose what the source is told: resolutions, refresh rates, HDR formats, peak and black, 4:2:0, bit depth and ALLM.</li><li>It is built from the receiver's EDID, so sound formats and Atmos pass untouched.</li></ul> |
| ✅ | **Output menu** | <ul><li>Colour format, bit depth, resolution, refresh rate and colour space, applied over whatever the app picks.</li><li>A mode the display can't carry is lowered automatically to what HDMI 2.0 supports.</li></ul> |
| ✅ | **Frame-rate matching** | <ul><li>Each film plays at its own rate, read before its first frame (23.976 and 24.000 told apart).</li><li>Streaming apps are included, with the sound kept through the switch.</li><li>At film rates the output goes to 4:4:4 12-bit in the same switch.</li></ul> |
| 🔶 | **Frame-rate matching for protected apps, through more receivers** | <ul><li>A switch now waits until the app's DRM sees the HDMI link protected again, and the picture holds for under 3 s.</li><li>Disney+ switched on every chain we tested, with Atmos kept.</li><li>Netflix switches where the HDMI handshake is quick (straight into a display, or a fast receiver: 23.976, 24 and 25 tested in 4K with Atmos). Through slower receivers it stays at the usual rate by itself, or you can choose **Always try**.</li><li>Next: the right rate set before the film starts, through more receivers.</li></ul> |
| ✅ | **Colour space and gamut** | <ul><li>**BT.2020 out:** menus, SDR and console pictures are converted in so they don't look oversaturated.</li><li>**BT.709 out:** films' wider colours are mapped in along constant hue rather than clipped.</li><li>**Auto:** each signal is flagged as what it is.</li></ul> |
| ✅ | **Profiles with automatic rules** | Game, HDMI, Film, Video and SDR each load their own preset when the source changes. |
| ✅ | **LLDV** | Dolby Vision from apps and Kodi arrives as low-latency Dolby Vision, and our tone mapping finishes it at 24 fps, zoom included. |
| ✅ | **Dolby Vision, three modes** | <ul><li>**Off (HDR10):** our tone mapping.</li><li>**LLDV:** Dolby's processing, finished by our tone mapping.</li><li>**Dolby Vision:** Dolby's processing end to end, to a Dolby Vision display.</li></ul>Live for streaming apps and Kodi, through an AV receiver too. The HDMI input can take LLDV (built, not yet tested with a real Dolby Vision source) or pass full Dolby Vision to the display. |
| 🔶 | **Lag measurement** | <ul><li>The delay Tesseract VP adds is measured live and shown while the HDMI input runs.</li><li>The display's own share needs a light sensor on the screen.</li></ul> |
| ➖ | **VRR pass-through** | The Cube's HDMI output can't carry variable refresh rate. |

## Phase 2: Picture quality

| | Feature | Details |
|---|---|---|
| ✅ | **Upscaling 1080p → 4K** | The Cube's scaler takes 1080p and smaller pictures to 4K. Lanczos 4 is the default; Bicubic, Lanczos 3, Soft and Fire OS's own scaler are the other choices. No CPU cost. |
| ✅ | **Chroma upsampling** | For 4K, HDR and Dolby Vision films the hardware brings 4:2:0 colour to full resolution: Smooth interpolates between colour samples (the default), or Fire OS's own. No CPU cost. |
| 🔶 | **Custom 4K upscaler for Kodi** | <ul><li>Tesseract VP's own GPU upscaler takes Kodi's 1080p SDR films to 4K. Each new pixel stays within the range of the source pixels around it, so it adds no ringing. Edge steps add edge anti-aliasing and steepening, and the colour goes from 4:2:0 straight to 4K. It fits as many steps as each frame rate allows; everything else keeps the hardware scaler.</li><li>Measured on a 1080p24 film: about 33 ms of GPU time a frame with every step on, and every frame shown in 4K in the test runs.</li><li>In Nightly builds, off by default until it has been seen on a projector.</li><li>Next: the HDMI input, and 1440-wide anamorphic films.</li></ul> |
| ✅ | **Kodi colour upgrade** | Kodi's 8-bit SDR films up to 1080p: the GPU rebuilds the colour to full 4:4:4 at the film's own size before the 4K scaler. Off by default; about a quarter of one CPU core while a film plays. |
| ✅ | **Downscaling** | Sharp, Balanced (Fire OS's own) or Soft. It acts on uncompressed sources such as the HDMI input; 4K HEVC films come down to 1080p through the decoder's own half-size copy. |
| ✅ | **Contrast and shadow detail** | **Depth** adds local contrast and shadow separation to the curve. |
| ✅ | **Sharpening and debanding for Kodi** | With the Kodi colour upgrade: light, medium or strong sharpening and low or high debanding, off by default. |
| ⬜ | **Sharpening and debanding for other sources** | Beyond Kodi, once the hardware can run them at full quality. |

## Phase 3: Every picture fitted to the screen

| | Feature | Details |
|---|---|---|
| ✅ | **Black bar detection** | <ul><li>Every source: apps, Kodi, the HDMI input, protected streams and Dolby Vision, FEL included.</li><li>Films are measured before they are shown, so a change of shape (an IMAX scene) is met on the frame it happens.</li><li>Subtitles in a bar don't fool it. Fast and Precise modes.</li></ul> |
| ✅ | **Aspect menu** | <ul><li>Fill, and picture to the top or bottom.</li><li>Anamorphic lenses (1.25, 1.33 and 1.5x).</li><li>Masking for each edge.</li></ul>It applies to every source and changes on the exact frame the film's shape does. |
| ✅ | **Scope screens** | <ul><li>Scope zoom for 2.35, 2.37 and 2.40 screens.</li><li>Non-linear stretch for 16:9 through an anamorphic lens.</li><li>"Only for pictures wider than…" rules and a manual aspect override.</li></ul> |
| ✅ | **Subtitles kept in view** | Burned-in subtitles in a bar are recognised, and Fill keeps just enough of the bar for them. |
| ✅ | **Lens memory automation** | The projector's own lens memories are recalled when the film's shape changes, on projectors Tesseract VP can control over the network. Recall by hand from the remote too. |

## Phase 4: Control and devices

| | Feature | Details |
|---|---|---|
| ✅ | **Control page** | <ul><li>Tone Mapping, Aspect, Input & Output, Calibration and Profiles.</li><li>The live picture, waveform and tone curve stay in view, so every change shows live.</li><li>The page's picture shows the menus and subtitles on screen too.</li></ul> |
| ✅ | **On-screen menu and settings page** | Everything on the control page is on the remote too, and a full-screen settings page runs on the Cube itself. |
| ✅ | **Phone app** | <ul><li>Finds every Tesseract VP on the Wi-Fi and opens its control page or on-screen display.</li><li>**One-tap scenes:** Movie night wakes the projector and amp, picks the input and dims the lights; Off puts it all to sleep. From the app, a home-screen widget or a quick-settings tile.</li></ul> |
| ✅ | **HDMI-CEC for every device** | <ul><li>The HDMI chain in signal order, with each device's power, name and input, plus the amp's volume.</li><li>One press of the remote's power button wakes or sleeps the display and amp with the Cube.</li><li>Nothing wakes by itself: the display and amp turn on for a remote press or a scene, never because the Cube woke on its own.</li></ul> |
| ✅ | **Launcher** | A new home screen: the glowing cube with the Cube's name, a clock with your HDMI devices, your own apps on the home row, a Last played strip and one Settings tile for everything. |
| ✅ | **API and webhooks** | Events for Home Assistant or any webhook: source, playing, HDR type, output mode, picture shape. The whole network API is documented. |
| ✅ | **Several Cubes** | Every Tesseract VP on the network, one click away from the control page. |
| ✅ | **IR devices and room automation** | <ul><li>Learn any IR remote's buttons, and send them through the Cube's IR blaster.</li><li>Tie them to what plays: lights off when a film starts.</li></ul> |
| ✅ | **Problem alerts** | Optional, off by default: the phone app tells you quietly, once, when an update didn't install, a version was rolled back, the Cube restarted unexpectedly or stopped answering. |

## Phase 5: Installing and updating

| | Feature | Details |
|---|---|---|
| ✅ | **Tesseract VP Setup** | <ul><li>A Windows app that prepares, backs up, unlocks and installs a Cube, with a button for every step.</li><li>Display setup: peak, colour space and receiver.</li><li>Restore, roll back and uninstall.</li></ul> |
| ✅ | **Windows installer** | <ul><li>One download, `Tesseract-VP-Setup.exe`: installs for your account with no administrator prompt, with a Start-menu shortcut and an uninstaller.</li><li>Google's platform-tools are downloaded on first use and checked, so there is nothing else to fetch.</li><li>Backups kept in Documents.</li></ul> |
| ✅ | **Signed self-updates** | <ul><li>Cubes update themselves from this repository's Stable or Nightly releases.</li><li>Every release is signed and checked before it installs.</li></ul> |
| ✅ | **Safe updates** | <ul><li>One-step roll back.</li><li>A watchdog that puts the previous version back if a new one keeps failing.</li><li>Backup and restore of every setting.</li></ul> |

## Phase 6: Calibration

| | Feature | Details |
|---|---|---|
| ✅ | **Fine calibration controls** | <ul><li>Black level, white level and gamma.</li><li>Red, green and blue gain and offset.</li></ul>Steps are finer than most displays' own, held for every source. |
| ✅ | **Output range** | Limited or Full, with the HDMI flag to match. |
| ✅ | **Gamut guardrail** | Colours are held inside BT.709, DCI-P3, BT.2020 or your display's own measured primaries. |
| ✅ | **Built-in test patterns** | Ramps, windows, average-light patches and single-pixel 4K checks, in SDR and HDR, shown tone mapped or untouched. |
| ✅ | **Separate SDR and HDR calibration** | Each has its own calibration set, picked by what is playing. |
| ✅ | **3D LUT and greyscale correction** | Each calibration set takes its own 3D LUT and a separate greyscale and gamma table in front of it. |
| 🔶 | **Calibration software (in testing)** | The Cube speaks the network pattern protocols that calibration software uses (Resolve, PGenerator and UPGCI), for Calman, HCFR, DisplayCAL and ColourSpace. Checked with protocol test tools; not yet confirmed with each program. |
| ⬜ | **Dynamic contrast guard** | For displays with dynamic contrast or laser and backlight dimming: leave room in dark scenes so their brightness lift doesn't clip highlights. |

## Phase 7: Beyond

| | Feature | Details |
|---|---|---|
| 🔶 | **3D Blu-ray** | <ul><li>3D films (MVC) played from Kodi, including Blu-ray .iso files, sent as HDMI 3D frame packing at 1080p24, the full-resolution 3D format.</li><li>Side-by-side and top-and-bottom 3D too.</li><li>Supported projectors are switched into 3D automatically.</li><li>In development: the Cube's 3D decoder and the projector's 3D input are confirmed; the frame-packed output and the 3D player are being built.</li></ul> |
| ⬜ | **Two Cubes, two projectors** | <ul><li>Two projectors, each fed by its own Cube, with the Cubes genlocked so every frame reaches both screens together.</li><li>**3D:** one projector per eye.</li><li>**Stacked:** both show the whole picture, for twice the light, with matched tone mapping.</li><li>**Split:** one carries the picture and the other only the highlights. They add back to the full picture with colours unchanged.</li><li>Alignment tools to line the two pictures up exactly.</li></ul> |
| ⬜ | **Profiles per display** | A Cube used with more than one display recognises the connected one and loads its own profile: peak, tone mapping and calibration. |
| ⬜ | **4096x2160 output** | For projectors with 17:9 panels. |
| ⬜ | **Display roll-off model for HDR10 out** | <ul><li>An optional BT.2390 model of a display that rolls off HDR10 by itself, undone before the signal leaves, so the light still tracks PQ to the peak.</li><li>Off by default, checked with the PQ check pattern.</li></ul> |
| ✅ | **Dolby Vision with full enhancement layer (FEL)** | <ul><li>Full FEL films rebuilt to their 12-bit picture by signde's Kodi FEL, built on djnice's FEL work.</li><li>Sent as Dolby Vision to the display, or tone mapped by us.</li></ul> |
| ✅ | **Lossless audio, built in** | <ul><li>Dolby TrueHD Atmos, DTS-HD Master Audio and DTS:X go to the receiver untouched, bitstreamed, and Dolby Atmos from Netflix and Disney+ arrives as Atmos.</li><li>Built into Tesseract VP: nothing else to install, and it stays up to date with it.</li><li>The signal card shows the format going out.</li></ul> |
