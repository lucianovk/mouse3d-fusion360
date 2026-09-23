/*
 * Mouse 3D — true SpaceMouse HID emulator (orbit-only)
 * Board: any Adafruit TinyUSB-capable board (Seeed XIAO nRF52840 Sense,
 * RP2040-Zero + external MPU6050, etc).
 *
 * Unlike orbit_module.ino (which emulates Shift + middle-mouse-drag, a
 * workaround any app understands), this presents itself as an actual
 * 3Dconnexion SpaceMouse Wireless over USB: same Vendor/Product ID and
 * HID report descriptor, reverse-engineered and published by the
 * AndunHH/spacemouse project (github.com/AndunHH/spacemouse — not
 * affiliated with 3Dconnexion). Fusion 360 has NATIVE support for this
 * device, so it drives the camera directly and continuously — no
 * keyboard emulation, no drag start/stop, real simultaneous 6-DOF.
 *
 * REQUIRES: the 3Dconnexion "3DxWare" driver installed on the PC (free,
 * from 3dconnexion.com) — Fusion talks to the SpaceMouse through that
 * driver's API, not by reading raw HID itself. Without 3DxWare installed,
 * this board plugged in will do nothing in Fusion.
 *
 * Scope: orbit only, as agreed. Translation (pan) axes are always sent
 * as zero; only the rotation axes (Rx, Ry, Rz) carry real data. Rz comes
 * from the gyroscope (twisting the board like a dial); Rx/Ry come from
 * tilt, high-pass filtered to mimic the physical SpaceMouse's spring
 * return-to-center — this board doesn't have springs, so "how far you've
 * moved recently" has to be computed instead of felt.
 *
 * This is UNTESTED against real hardware/3DxWare — it's built from
 * community-documented reverse engineering, not an official spec. Expect
 * to need adjustments once you can actually see it (or not) show up as a
 * SpaceMouse on your PC.
 *
 * Arduino IDE setup:
 *   1. Same board package as orbit_module.ino (Seeed nRF52 Boards, or
 *      RP2040 board package if using RP2040-Zero).
 *   2. Tools > USB Stack > TinyUSB.
 *   3. If using an external MPU6050 instead of an onboard IMU, swap
 *      readTiltAngles()/readGyroZ() for that sensor's library — the rest
 *      of this file doesn't care where the numbers come from.
 *   4. Install 3DxWare on the PC before testing.
 */

#include <Adafruit_TinyUSB.h>
#include <LSM6DS3.h>
#include <Wire.h>

LSM6DS3 imu(I2C_MODE, 0x6A);

// ---- 3Dconnexion SpaceMouse Wireless identity ----
// Reverse-engineered by github.com/AndunHH/spacemouse (SpaceMouseWireless.md).
#define SPACEMOUSE_VID 0x256Fu
#define SPACEMOUSE_PID 0xC62Eu

uint8_t const desc_hid_report[] = {
  0x05, 0x01, 0x09, 0x08, 0xA1, 0x01, 0xA1, 0x00, 0x85, 0x01, 0x16, 0xA2,
  0xFE, 0x26, 0x5E, 0x01, 0x36, 0x88, 0xFA, 0x46, 0x78, 0x05, 0x55, 0x0C,
  0x65, 0x11, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x33, 0x09, 0x34,
  0x09, 0x35, 0x75, 0x10, 0x95, 0x06, 0x81, 0x02, 0xC0, 0xA1, 0x02, 0x85,
  0x03, 0x05, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x02, 0x15, 0x00, 0x25,
  0x01, 0x35, 0x00, 0x45, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02, 0x95,
  0x0E, 0x81, 0x03, 0xC0, 0xA1, 0x02, 0x85, 0x04, 0x05, 0x08, 0x09, 0x4B,
  0x15, 0x00, 0x25, 0x01, 0x95, 0x01, 0x75, 0x01, 0x91, 0x02, 0x95, 0x01,
  0x75, 0x07, 0x91, 0x03, 0xC0, 0xA1, 0x02, 0x85, 0x17, 0x15, 0x00, 0x25,
  0x64, 0x55, 0x00, 0x65, 0x00, 0x05, 0x06, 0x09, 0x20, 0x75, 0x08, 0x95,
  0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x01, 0x06, 0x00, 0xFF, 0x09, 0x27,
  0x75, 0x01, 0x95, 0x01, 0x81, 0x02, 0x75, 0x07, 0x81, 0x03, 0xC0, 0x06,
  0x00, 0xFF, 0x09, 0x01, 0xA1, 0x02, 0x15, 0x80, 0x25, 0x7F, 0x75, 0x08,
  0x09, 0x3A, 0xA1, 0x02, 0x85, 0x05, 0x09, 0x20, 0x95, 0x01, 0xB1, 0x02,
  0xC0, 0xA1, 0x02, 0x85, 0x06, 0x09, 0x21, 0x95, 0x01, 0xB1, 0x02, 0xC0,
  0xA1, 0x02, 0x85, 0x07, 0x09, 0x22, 0x95, 0x01, 0xB1, 0x02, 0xC0, 0xA1,
  0x02, 0x85, 0x08, 0x09, 0x23, 0x95, 0x07, 0xB1, 0x02, 0xC0, 0xA1, 0x02,
  0x85, 0x09, 0x09, 0x24, 0x95, 0x07, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85,
  0x0A, 0x09, 0x25, 0x95, 0x07, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x0B,
  0x09, 0x26, 0x95, 0x01, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x13, 0x09,
  0x2E, 0x95, 0x01, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x19, 0x09, 0x31,
  0x95, 0x04, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x1A, 0x09, 0x32, 0x95,
  0x07, 0xB1, 0x02, 0xC0, 0xC0, 0xC0
};

Adafruit_USBD_HID usb_hid;

// Report ID 1: translation X,Y,Z + rotation Rx,Ry,Rz, all int16_t,
// logical range roughly -350..+350 per the reverse-engineered spec.
#pragma pack(push, 1)
struct SpaceMouseReport {
  int16_t x, y, z, rx, ry, rz;
};
#pragma pack(pop)

const int16_t AXIS_LIMIT = 350;

// ---- Sensitivity (needs real-hardware tuning, these are starting points) ----
const float TILT_RATE_SENS = 40.0f;  // deg of high-pass-filtered tilt -> axis units
const float GYRO_RZ_SENS = 60.0f;    // deg/s of twist -> axis units
const float RECENTER_TAU_S = 2.0f;   // seconds: how fast the "center" catches up to where you're resting

float baselineBeta = 0, baselineGamma = 0;
bool haveBaseline = false;
unsigned long lastLoopAt = 0;
const unsigned long LOOP_INTERVAL_MS = 8; // ~125 Hz, matches the "62.5 packets/s" spec-ish

void readTiltAngles(float &beta, float &gamma) {
  float ax = imu.readFloatAccelX();
  float ay = imu.readFloatAccelY();
  float az = imu.readFloatAccelZ();
  beta = atan2(ay, az) * 180.0f / PI;
  gamma = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
}

int16_t clampAxis(float v) {
  if (v > AXIS_LIMIT) v = AXIS_LIMIT;
  if (v < -AXIS_LIMIT) v = -AXIS_LIMIT;
  return (int16_t)v;
}

void setup() {
  TinyUSBDevice.setID(SPACEMOUSE_VID, SPACEMOUSE_PID);
  TinyUSBDevice.setManufacturerDescriptor("3Dconnexion");
  TinyUSBDevice.setProductDescriptor("SpaceMouse Wireless");

  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  imu.begin();
  lastLoopAt = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopAt < LOOP_INTERVAL_MS) return;
  float dt = (now - lastLoopAt) / 1000.0f;
  lastLoopAt = now;

  if (!TinyUSBDevice.mounted()) return;

  float beta, gamma;
  readTiltAngles(beta, gamma);
  float gyroZ = imu.readFloatGyroZ(); // deg/s

  if (!haveBaseline) {
    baselineBeta = beta;
    baselineGamma = gamma;
    haveBaseline = true;
    return;
  }

  // Software "spring return to center": the baseline slowly drifts to
  // wherever the board has been resting, so a deliberate tilt registers
  // as a strong signal that decays back toward zero if you just hold the
  // new position — same feel as releasing a physical SpaceMouse cap.
  float alpha = dt / (RECENTER_TAU_S + dt);
  baselineBeta += (beta - baselineBeta) * alpha;
  baselineGamma += (gamma - baselineGamma) * alpha;

  float rateBeta = beta - baselineBeta;
  float rateGamma = gamma - baselineGamma;

  SpaceMouseReport report = {0};
  report.x = 0;
  report.y = 0;
  report.z = 0;
  report.rx = clampAxis(rateBeta * TILT_RATE_SENS);
  report.ry = clampAxis(-rateGamma * TILT_RATE_SENS);
  report.rz = clampAxis(gyroZ * GYRO_RZ_SENS / 100.0f);

  usb_hid.sendReport(1, &report, sizeof(report));
}
