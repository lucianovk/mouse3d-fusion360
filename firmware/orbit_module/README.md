# Mouse 3D — Orbit USB module

Standalone USB device for Fusion 360 orbit, using the Seeed XIAO nRF52840
Sense's onboard IMU. No phone, no Wi-Fi, no PC-side server — plug in and
tilt/twist the board to orbit. Pan and zoom stay on the normal PC mouse.

## Setup

1. Arduino IDE > Boards Manager > install **Seeed nRF52 Boards**, then
   select board **Seeed XIAO nRF52840 Sense**.
2. Tools > **USB Stack > TinyUSB** (not the default "Arduino" stack —
   Mouse/Keyboard HID need this).
3. Library Manager > install **Seeed Arduino LSM6DS3**.
4. In `orbit_module.ino`, leave `USB_DEBUG` set to `1` for the first
   upload.

## First-run calibration

1. Upload with `USB_DEBUG 1`, open the Serial Monitor at 115200 baud.
2. Tilt the board forward/back and left/right, watch `beta`/`gamma`.
   If a direction feels backwards once you're in Fusion, flip the sign
   in `readTiltAngles()` (or in the `dx`/`dy` calculation in `loop()`).
3. Twist the board flat like a dial, confirm `gyroZ` changes sign the way
   you expect it to spin the model; flip the sign on `gyroDx` if not.
4. Set `USB_DEBUG` back to `0`, re-upload. The board now sends real mouse
   + keyboard HID instead of printing.

## Sensitivity

`ABS_SENS` and `GYRO_Z_SENS` are carried over from the phone webapp
version of this project, calibrated there so 90° of physical tilt
produced 90° of orbit in Fusion 360 (see the parent project's
`static/index.html`). Fusion's pixels-per-degree ratio isn't a property
of the phone, so this should transfer — but repeat the same test (tilt
exactly 90°, compare to how far the model actually turned) once the
board is in hand, and scale both constants by the same ratio if it's off.

## Safety note

The board holds Shift + the middle mouse button only while it detects
actual tilt/twist motion, releasing automatically after half a second of
stillness (`IDLE_RELEASE_MS`) — it must never stay held indefinitely,
since that blocks normal use of the PC (typing, clicking, shortcuts).
This exact failure happened once during development of the phone
version; don't remove or weaken this behavior without keeping some
equivalent safeguard in place.
