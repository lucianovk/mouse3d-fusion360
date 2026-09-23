# Mouse 3D — true SpaceMouse emulator (orbit-only)

Presents the board as an actual 3Dconnexion SpaceMouse Wireless over USB
(same VID/PID and HID report format), so Fusion 360's *native* 3D-mouse
support drives the camera directly — continuous rotation, no keyboard
emulation, no drag start/stop like `../orbit_module`.

Only the rotation axes are used (Rx/Ry/Rz); translation (pan) is always
zero, per the "orbit only" scope agreed for this device.

## This is experimental

The VID/PID and HID report descriptor come from
[AndunHH/spacemouse](https://github.com/AndunHH/spacemouse)'s reverse
engineering of the real device — not an official 3Dconnexion spec, and
not affiliated with them. There's no way to verify this works without
the actual board and driver installed, so treat the sensitivity constants
and even the basic "does Fusion see it at all" question as unverified
until tested.

## Requirements

1. Install **3DxWare** on the PC first (free, from 3dconnexion.com).
   Fusion talks to the SpaceMouse through that driver's API, not by
   reading raw HID directly — without it installed, this board doing
   nothing in Fusion doesn't necessarily mean the firmware is broken.
2. Same Arduino IDE setup as `../orbit_module` (board package + Tools >
   USB Stack > TinyUSB).
3. If the IMU isn't the onboard LSM6DS3TR-C (e.g. you went with the
   RP2040-Zero + external MPU6050 route), swap `readTiltAngles()` and
   the `imu.readFloatGyroZ()` call for that sensor's library — nothing
   else in the file needs to change.

## What to check once it's flashed

1. Does 3DxWare's own device list/tray icon show a SpaceMouse connected
   at all? If not, the VID/PID or descriptor registration needs
   debugging before anything else matters.
2. Does Fusion's status bar 3D-mouse icon light up?
3. Tilt the board — does the model actually rotate, and in the expected
   direction? Flip signs on `report.rx`/`report.ry`/`report.rz` as needed
   (same idea as the axis calibration in `../orbit_module/README.md`).
4. `RECENTER_TAU_S` controls how quickly the "center" catches up to a
   held tilt (how fast the spring-return feel decays) — raise it if
   rotation dies out too quickly while you're still tilted, lower it if
   it never stops even when you think you've let go.

## Fallback

If 3DxWare doesn't recognize this device, or Fusion doesn't pick it up
through it, `../orbit_module` (Shift + middle-mouse-drag emulation) is
the proven-simpler fallback — it needs no driver and matches what
already worked in the phone webapp version of this project.
