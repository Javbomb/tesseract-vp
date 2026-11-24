# Tesseract VP

An HDR video processor for the Fire TV Cube (3rd generation).

This repository holds Tesseract VP's **signed releases**. Installed Cubes update themselves from here.

## Releases

- **stable**: tested releases. A Cube checks for these once a day and tells you when one is ready. It never installs one by itself.
- **nightly**: the newest builds, checked only when you ask.

Every release is signed. A Cube refuses any update whose signature or file checksums don't match, so a tampered download is never installed.

## What it does

- Dynamic tone mapping for HDR10, HDR10+ and Dolby Vision titles, on streaming apps (protected ones included), Kodi and the HDMI input.
- Output and frame-rate control, including matching the film's frame rate.
- HDMI-CEC and IR control, profiles and automations.
- Its own launcher, an on-screen menu and a phone remote.

## Updating a Cube

On the control page (`http://<cube>:8090/`), open **Profiles › Updates**, choose Stable or Nightly, then press **Check now** and **Update**. You can do the same from the Cube's own Settings › Tesseract VP page, or from Tesseract VP Setup's **Manage › Check for updates**.
