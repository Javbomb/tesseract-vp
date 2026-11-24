# Tesseract VP

**An HDR video processor for the Fire TV Cube (3rd generation).**

Tesseract VP turns a Fire TV Cube into a video processor that sits between your sources and your screen. It tone maps every HDR picture to your display's real brightness, frame by frame. That covers the streaming apps (Netflix and Disney+ included), Kodi and the Cube's HDMI input. It is built for projectors and other displays that can't reach HDR brightness. It also gives you full control of the signal that leaves the box: output format, frame rate, aspect and masking, and calibration.

![The Tesseract VP control page: the signal in and out, the measured peak, colour and clipping, a live waveform, the tone curve and the Picture settings](docs/control-page.jpg)

*The control page, open in any browser on your network.*

This repository holds Tesseract VP's **signed releases** and its [roadmap](ROADMAP.md). Installed Cubes update themselves from here.

---

## What it does

### Dynamic tone mapping
- **Every frame is measured and gets its own curve.** Each picture is fitted to your screen's real peak brightness.
- **Faithful to the grade.** Shadows and mid-tones stay as the film was mastered. Only light your screen can't show is brought into range. The picture never clips, never pumps, and colours keep their hue.
- **Look-ahead.** Films are measured several frames before they are shown, so a scene cut gets the right curve on the frame it happens. Small, bright highlights are caught too.
- **Protected streams included.** Netflix, Disney+ and other DRM video can't be read by software. Tesseract VP measures them in the video hardware instead, ahead of the screen. On a Disney+ 4K film, 99.5 % of frames reached the screen with their own curve, with 4K kept and no protection errors.
- **HDR10, HDR10+ and Dolby Vision titles.** HDR10+ films get the same tone mapping as HDR10. For Dolby Vision titles you choose: Tesseract VP's tone mapping, Dolby's own mapping, or Dolby Vision out to the display.
- **Controls you can tune live:**
  - brightness, highlights and shadows;
  - highlight colour and adaptation speed;
  - an auto black floor for films mastered with raised blacks.

### Games and the HDMI input
- **HDR10 consoles, tone mapped live.** An Xbox on the HDMI input gets a curve made from each of its own frames.
- **Low latency.**
  - About **7 ms** from a frame's arrival to the screen in game mode, against 41 ms without it.
  - **Auto low latency (ALLM)** switches to game mode when the console asks.
- **Genlock.** The output follows the source's timing, so no frame is dropped or shown twice.
- **EDID builder.** You choose what the source is told about your screen: resolutions, refresh rates, HDR formats, peak and black, and bit depth. Your receiver's sound formats and Atmos pass through untouched.

### Output
- **Frame-rate matching.** Each film plays at its own rate, and 23.976 and 24.000 are told apart. Streaming apps are included, with one quick switch as the film starts and the sound kept.
- **Full control of the signal:**
  - colour format (4:4:4, 4:2:2, 4:2:0 or RGB);
  - bit depth and resolution;
  - refresh rate;
  - colour space (BT.2020 or BT.709);
  - Limited or Full range.

  Anything a mode can't carry is lowered automatically to what HDMI 2.0 and your display support.
- **Colour.** Keep your display in BT.2020, and menus, SDR and console pictures are converted in without oversaturation. Or use a BT.709 display, and films' wider colours are mapped in along constant hue instead of being clipped.
- **Upscaling and chroma.**
  - A choice of upscalers for 1080p and smaller pictures.
  - Smooth chroma upsampling for 4K films.

  Both run in the hardware, at no CPU cost.

### Aspect, masking and subtitles
- **Black bar detection in every source:** apps, Kodi, the HDMI input and protected streams. A film that changes shape (an IMAX scene) is followed on the frame it changes.
- **Fit the picture to your screen:**
  - fill;
  - picture to the top or bottom;
  - anamorphic lenses (1.25, 1.33 and 1.5x), with non-linear stretch for 16:9;
  - scope zoom for 2.35, 2.37 and 2.40 screens;
  - masking for each edge.
- **Subtitles kept in view.** Burned-in subtitles in a bar are recognised, and Fill keeps just enough of that bar to show them.

### Calibration
- **Fine controls in the output**, held for every source and the menus:
  - black level, white level and gamma;
  - red, green and blue gain and offset.

  Their steps are far finer than a display's own controls.

### Control
- **Control page.** Every setting is one line, with the live picture, waveform and tone curve kept in view, so you see each change as you make it.
- **Other ways in:**
  - an on-screen menu on the Fire TV remote;
  - a full-screen settings page on the Cube itself;
  - a phone app that finds every Tesseract VP on your Wi-Fi.
- **Profiles with automatic rules.** Game, HDMI, Film, Video and SDR each load their own preset when the source changes.
- **HDMI-CEC for every device.** Power, input and the amp's volume. One press of the remote's power button wakes or sleeps the projector or TV and the amp with the Cube.
- **IR** (in testing). Learn any IR remote's buttons and tie them to what plays: lights off when a film starts, back on when it stops.
- **API and webhooks.** Events for Home Assistant or anything else as the source, HDR type, output mode or picture shape changes.
- **Its own launcher**, with your own apps and one place for all the settings.
- **Several Cubes.** Switch between every Tesseract VP on your network from the control page.

### Safe to run
- **Signed releases.** A Cube refuses any update whose signature or checksums don't match.
- **Roll back** any update in one step.
- **Watchdog.** If a new version keeps failing, the watchdog puts the previous one back.
- **Backup and restore** of every setting, preset and profile.
- **Light on the CPU.** Most of the processing runs in the Cube's video hardware, so it uses next to no CPU while a film plays.

---

## Requirements

- A **Fire TV Cube, 3rd generation** (model AFTGAZL), **unlocked and rooted** (Magisk). Tesseract VP Setup can run the community unlock for you, or install straight onto a Cube that is already rooted.
  - The unlock only works on firmware up to the version it was made for, so prepare a new Cube before it ever updates.
  - The unlock itself is a third-party tool and is not included in Tesseract VP.
- A Windows PC for **Tesseract VP Setup**. On a Mac or Linux, the installer is one command.
- [Android platform-tools](https://developer.android.com/tools/releases/platform-tools) (`adb`), and ADB debugging turned on on the Cube.

## Installing

1. Download `tesseract-vp-<date>.zip` from the [latest release](https://github.com/Javbomb/tesseract-vp/releases) and unzip it.
2. Double-click **Tesseract VP Setup**, press **Find Cubes** (or type the Cube's address), and follow the large first button. It backs up the Cube, unlocks it if needed and installs, showing each step as it runs.
3. Open the control page at `http://<cube>:8090/`.

The package's own `README.md` describes every step, including doing it by hand.

## Releases

- **stable**: tested releases. A Cube checks for these once a day and tells you when one is ready. It never installs one by itself.
- **nightly**: the newest builds, checked only when you ask.

## Updating a Cube

On the control page, open **Profiles › Updates**, choose Stable or Nightly, then press **Check now** and **Update**. You can do the same:

- from the Cube's own **Settings › Tesseract VP** page;
- from Tesseract VP Setup's **Manage › Check for updates**.

## Roadmap

**Live now:** everything above.

**Next:**
- Picture quality: sharpening, downscaling, contrast and shadow detail.
- A colour upgrade to full 4:4:4 for 1080p films.
- LLDV and Dolby Vision on the HDMI input.
- Geometry correction (keystone, curved screens, lens distortion) in the Cube's own hardware.
- Lens memory with the projector.

**Then:**
- Debanding.
- Convergence correction.
- LUT upload and meter calibration.
- Two-projector stacking.

The full plan, item by item, is in **[ROADMAP.md](ROADMAP.md)**.

---

Tesseract VP is an independent project. It is not affiliated with or endorsed by Amazon or Dolby. Unlocking a Fire TV Cube is at your own risk and may void its warranty.
