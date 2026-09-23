# Mouse 3D — Phone-as-3D-Mouse for Fusion 360

**[⬇ Download Mouse3D.exe](https://github.com/lucianovk/mouse3d-fusion360/releases/latest)**
— a self-contained Windows app, no Python install needed.

Turn an Android phone into a wireless 3D navigation controller for
Autodesk Fusion 360 — no dedicated hardware required. A small system-tray
app on your PC serves a page to your phone's browser; the phone reads its
touchscreen and motion sensors (IMU) and streams orbit/pan/zoom gestures
back over your local Wi-Fi.

Two ways to control the camera, at the same time:

- **Touch**: 1 finger drags to orbit, 2 fingers pan, pinch to zoom.
- **IMU** (optional, toggle on/off): tilt the phone to orbit, or twist it
  flat like a dial — while a finger is on the screen. Two tilt modes:
  **Absolute** (the model tracks the phone's tilt 1:1) and **Relative**
  (orbit speed is proportional to how far you're tilted from where you
  started touching, like a joystick).

## How it works

```
 Phone browser  --(HTTPS + WebSocket, same Wi-Fi)-->  PC tray app
 (touch + IMU)                                        (FastAPI/uvicorn)
                                                              |
                                                    pyautogui replays
                                                    Shift+middle-drag /
                                                    scroll into Fusion 360
```

The PC app has no Fusion 360 plugin or API integration — it just
simulates the same mouse+keyboard input you'd use by hand (Fusion's
default navigation shortcuts: Shift + middle-mouse-drag to orbit,
middle-mouse-drag to pan, scroll wheel to zoom). That makes it work with
the stock Fusion 360 install, at the cost of not supporting true
simultaneous 6-DOF like a real 3Dconnexion SpaceMouse would. See
[`firmware/`](firmware/) for an experimental path toward that.

## Quick start

1. **Get the app** — either download the packaged `Mouse3D.exe` (see
   [Building](#building-the-standalone-exe) to build it yourself; no
   pre-built releases are published here) or run `server.py` from source
   (see [Running from source](#running-from-source)).
2. **Give your phone a way to reach the PC.** The simplest option on a
   locked-down/corporate Wi-Fi: turn on Windows' built-in **Mobile
   Hotspot** (Settings > Network & Internet > Mobile hotspot) and connect
   your phone to it. The PC's hotspot IP is always `192.168.137.1`.
3. **Launch the app.** It appears as an icon in the system tray near the
   clock. Click it to open a window with a QR code and the connection URL.
4. **Scan the QR code** with your phone (same Wi-Fi/hotspot). Your
   browser will warn about the self-signed certificate — this is
   expected for a local-only server; proceed anyway.
5. Open Fusion 360, keep your mouse over the 3D viewport, and try it:
   drag with 1 finger to orbit, 2 fingers to pan, pinch to zoom. Toggle
   the **IMU** button on to also control orbit by tilting/twisting the
   phone.

## Calibration

Fusion 360 doesn't expose a numeric navigation-sensitivity setting or an
API to query its pixels-per-degree ratio, so the IMU sensitivity
constants in `static/index.html` (`ABS_SENS`, `REL_SENS`, `GYRO_Z_SENS`)
were calibrated empirically: tilt the phone exactly 90° and compare to
how far the model actually rotated, then scale the constants by that
ratio. A live sensitivity slider (0.25x–3x) is also built into the page
if the baseline doesn't feel right on your setup.

## Running from source

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python server.py
```

Requires Python 3.10+ on Windows. `pyautogui` is what actually drives the
mouse/keyboard; it doesn't work headless and needs an interactive desktop
session.

## Building the standalone .exe

```bash
pip install pyinstaller
pyinstaller --onefile --windowed --name "Mouse3D" --icon icon.ico --add-data "static;static" server.py
```

The result (`dist/Mouse3D.exe`) is fully self-contained: it generates its
own self-signed TLS certificate and QR code next to the .exe on first
run, no separate install step needed on the target machine.

## Windows Mobile Hotspot note

Windows can disable the hotspot automatically after a period with no
connected devices. If the phone can't reach `192.168.137.1` anymore,
re-open Settings > Network & Internet > Mobile hotspot and turn it back
on.

## Firmware (experimental, hardware-based alternative)

[`firmware/`](firmware/) contains two Arduino sketches for turning a
small USB microcontroller board (e.g. Seeed XIAO nRF52840 Sense) into a
standalone orbit controller, bypassing the phone entirely:

- [`orbit_module/`](firmware/orbit_module/) — same Shift+middle-drag
  emulation as the phone app, driven by the board's onboard IMU. No
  driver needed.
- [`spacemouse_emulator/`](firmware/spacemouse_emulator/) — presents the
  board as an actual 3Dconnexion SpaceMouse Wireless over USB (reverse-
  engineered VID/PID and HID report format), so Fusion's native 3D-mouse
  support drives the camera directly. Requires 3Dconnexion's 3DxWare
  driver installed on the PC. **Untested against real hardware** — built
  from community reverse-engineering, not an official spec.

## Limitations

- Windows only (uses `pyautogui`'s Windows input backend, Windows Mobile
  Hotspot, and Windows-specific tray/packaging).
- Not a real 3D-mouse integration — Fusion sees ordinary mouse/keyboard
  input, so true simultaneous 6-DOF isn't possible (see `firmware/` for
  the closer-to-real alternative).
- The IMU path needs a Chromium-based mobile browser with the Generic
  Sensor API (or, as a fallback, legacy `DeviceOrientationEvent`).

## License

MIT — see [LICENSE](LICENSE).
