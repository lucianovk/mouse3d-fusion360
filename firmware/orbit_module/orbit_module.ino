/*
 * Mouse 3D — Orbit-only USB module firmware
 * Board: Seeed XIAO nRF52840 Sense (onboard LSM6DS3TR-C IMU)
 *
 * Standalone USB "3D mouse" for Fusion 360 orbit only. Plugs into the PC
 * and emulates Shift + middle-mouse-button drag while the board is being
 * tilted or twisted, using the same interaction model and calibration
 * already validated on the phone webapp version of this project
 * (see ../../server.py and ../../static/index.html):
 *   - absolute tilt angle -> orbit position (1:1-ish, calibrated so 90° of
 *     tilt = 90° of orbit in Fusion 360)
 *   - gyroscope Z (twisting the board like a dial) adds to horizontal orbit
 *   - the drag is only held open while actual motion is detected, and is
 *     released automatically after IDLE_RELEASE_MS of stillness — never
 *     indefinitely. Holding Shift/a mouse button stuck system-wide blocks
 *     normal use of the PC; this happened once already in the phone
 *     version and must not happen here either.
 * Pan and zoom are NOT handled by this module — use the PC's normal mouse
 * (middle-drag / scroll wheel), which Fusion already supports natively.
 *
 * Arduino IDE setup:
 *   1. Boards Manager: install "Seeed nRF52 Boards", select
 *      "Seeed XIAO nRF52840 Sense".
 *   2. Tools > USB Stack > TinyUSB  (required — Mouse/Keyboard HID need
 *      this instead of the default "Arduino" USB stack).
 *   3. Library Manager: install "Seeed Arduino LSM6DS3".
 *   4. Set USB_DEBUG to 1 below for the first upload, open the Serial
 *      Monitor at 115200 baud, and tilt the board to confirm beta/gamma
 *      move the way you expect (flip signs in readTiltAngles if not).
 *      Set it back to 0 before normal use.
 */

#include <Adafruit_TinyUSB.h>
#include <LSM6DS3.h>
#include <Wire.h>

#define USB_DEBUG 1  // 1 = print readings over Serial instead of sending HID

LSM6DS3 imu(I2C_MODE, 0x6A);

// ---- Calibration ----
// Carried over from the phone build: 90 deg of tilt measured 360 deg of
// orbit at ABS_SENS=30, so 30 * (90/360) = 7.5, then +10% per a later
// request = 8.25. GYRO_Z_SENS scaled by the same factor. Fusion's
// pixels-per-degree ratio is a property of Fusion/the viewport, not of
// the phone, so this should carry over — but re-run the same 90 deg test
// once the board is in hand and adjust if it doesn't quite match.
const float ABS_SENS = 8.25f;      // deg of tilt -> pixel-equivalent HID delta
const float GYRO_Z_SENS = 137.5f;  // rad/s (integrated) -> pixel-equivalent horizontal delta

const float MOTION_THRESHOLD_DEG = 1.0f;     // per-sample angle change that counts as "tilting"
const unsigned long IDLE_RELEASE_MS = 500;   // release the drag after this long without motion
const unsigned long LOOP_INTERVAL_MS = 10;   // ~100 Hz sampling

bool dragHeld = false;
bool haveRef = false;
float refBeta = 0, refGamma = 0;
bool haveLast = false;
float lastBeta = 0, lastGamma = 0;
unsigned long lastMotionAt = 0;
unsigned long lastLoopAt = 0;

// Accelerometer-derived tilt, in degrees. Which physical axis is "beta"
// (front-back) vs "gamma" (left-right), and which direction is positive,
// depends on how the board is oriented in your hand/enclosure — this is
// the first thing to check with USB_DEBUG=1.
void readTiltAngles(float &beta, float &gamma) {
  float ax = imu.readFloatAccelX();
  float ay = imu.readFloatAccelY();
  float az = imu.readFloatAccelZ();
  beta = atan2(ay, az) * 180.0f / PI;
  gamma = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
}

void beginDrag() {
  if (dragHeld) return;
#if !USB_DEBUG
  Keyboard.press(KEY_LEFT_SHIFT);
  delay(50); // give Fusion time to register the modifier before the button-down
  Mouse.press(MOUSE_MIDDLE);
#else
  Serial.println("[drag] begin");
#endif
  dragHeld = true;
  haveRef = false;
}

void endDrag() {
  if (!dragHeld) return;
#if !USB_DEBUG
  Mouse.release(MOUSE_MIDDLE);
  delay(30);
  Keyboard.release(KEY_LEFT_SHIFT);
#else
  Serial.println("[drag] end");
#endif
  dragHeld = false;
  haveRef = false;
}

void sendMoveClamped(float dx, float dy) {
  // Mouse.move() takes an int8_t per axis; split large deltas across
  // multiple calls instead of silently clipping them.
  for (int guard = 0; guard < 8 && (fabs(dx) > 127 || fabs(dy) > 127); guard++) {
    float sx = constrain(dx, -127, 127);
    float sy = constrain(dy, -127, 127);
#if USB_DEBUG
    Serial.print("move "); Serial.print(sx); Serial.print(","); Serial.println(sy);
#else
    Mouse.move((int8_t)sx, (int8_t)sy);
#endif
    dx -= sx; dy -= sy;
  }
#if USB_DEBUG
  Serial.print("move "); Serial.print(dx); Serial.print(","); Serial.println(dy);
#else
  Mouse.move((int8_t)dx, (int8_t)dy);
#endif
}

void setup() {
#if USB_DEBUG
  Serial.begin(115200);
  uint32_t waited = 0;
  while (!Serial && waited < 3000) { delay(10); waited += 10; }
#else
  Mouse.begin();
  Keyboard.begin();
#endif
  imu.begin();
  lastLoopAt = millis();
  lastMotionAt = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopAt < LOOP_INTERVAL_MS) return;
  float dt = (now - lastLoopAt) / 1000.0f;
  lastLoopAt = now;

  float beta, gamma;
  readTiltAngles(beta, gamma);
  float gyroZ = imu.readFloatGyroZ() * PI / 180.0f; // deg/s -> rad/s
  float gyroDx = gyroZ * dt * GYRO_Z_SENS;

  // ---- Motion detection drives whether the drag session is open ----
  bool moved = false;
  if (haveLast) {
    if (fabs(beta - lastBeta) > MOTION_THRESHOLD_DEG) moved = true;
    if (fabs(gamma - lastGamma) > MOTION_THRESHOLD_DEG) moved = true;
  }
  if (fabs(gyroDx) > 0.05f) moved = true; // twisting counts as motion too
  lastBeta = beta; lastGamma = gamma; haveLast = true;

  if (moved) {
    lastMotionAt = now;
    if (!dragHeld) beginDrag();
  } else if (dragHeld && (now - lastMotionAt > IDLE_RELEASE_MS)) {
    endDrag();
  }

  if (!dragHeld) return;

  if (!haveRef) { refBeta = beta; refGamma = gamma; haveRef = true; return; }
  float dx = (gamma - refGamma) * ABS_SENS + gyroDx;
  float dy = (beta - refBeta) * ABS_SENS;
  refBeta = beta; refGamma = gamma;

#if USB_DEBUG
  Serial.print("beta="); Serial.print(beta);
  Serial.print(" gamma="); Serial.print(gamma);
  Serial.print(" gyroZ="); Serial.println(gyroZ);
#endif
  sendMoveClamped(dx, dy);
}
