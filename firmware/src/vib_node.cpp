// Vibration node — Heltec LoRa 32 V2 + ADXL345 on I2C (SDA 21, SCL 22, address 0x53).
//
// Overall-level vibration monitoring for motors, pumps and fans. Each interval
// the node samples a burst of 1024 readings at the ADXL345's 3200 Hz output
// rate and computes, on the board, the AC RMS per axis (gravity and offset
// removed) and the peak excursion. Only those metrics go over LoRa: a raw
// waveform would not fit in a LoRa frame.
//
// Uplink: NODE:<id>|RX|RY|RZ:<g rms>|RM:<g rms total>|PK:<g peak>|FS:<Hz>|B:<%>|N:<seq>
//
// Wiring: VCC->3V3, GND->GND, SDA->21, SCL->22, CS->3V3 (selects I2C; floating
// does NOT work), SDO->GND (address 0x53).

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "heltec_v2.h"
#include "node_common.h"

namespace adxl {
constexpr uint8_t ADDR = 0x53;
constexpr uint8_t DEVID = 0x00;  // reads 0xE5
constexpr uint8_t BW_RATE = 0x2C;
constexpr uint8_t POWER_CTL = 0x2D;
constexpr uint8_t DATA_FORMAT = 0x31;
constexpr uint8_t DATAX0 = 0x32;
constexpr float G_PER_LSB = 0.0039f;  // full resolution: 3.9 mg/LSB at any range

void write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t read(uint8_t reg) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

// Reads X/Y/Z (registers 0x32..0x37) in one burst.
bool readXYZ(int16_t& x, int16_t& y, int16_t& z) {
  Wire.beginTransmission(ADDR);
  Wire.write(DATAX0);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  x = (int16_t)(Wire.read() | (Wire.read() << 8));
  y = (int16_t)(Wire.read() | (Wire.read() << 8));
  z = (int16_t)(Wire.read() | (Wire.read() << 8));
  return true;
}

bool begin() {
  if (read(DEVID) != 0xE5) return false;
  write(DATA_FORMAT, 0x0B);  // full resolution, ±16 g
  write(BW_RATE, 0x0F);      // 3200 Hz output data rate
  write(POWER_CTL, 0x08);    // measurement mode
  delay(20);
  return true;
}
}  // namespace adxl

constexpr int SAMPLES = 1024;

struct Vibration {
  float rmsX, rmsY, rmsZ, rmsTotal, peak, sampleRateHz;
};

// One pass, O(1) memory: running sums give the mean (DC) and the AC RMS as
// sqrt(E[x²] - mean²), i.e. the standard deviation of each axis.
static Vibration measure() {
  double sx = 0, sy = 0, sz = 0, qx = 0, qy = 0, qz = 0;
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f, minZ = 1e9f, maxZ = -1e9f;
  int n = 0;
  const uint32_t t0 = micros();
  while (n < SAMPLES) {
    int16_t rx, ry, rz;
    if (!adxl::readXYZ(rx, ry, rz)) continue;
    const float gx = rx * adxl::G_PER_LSB, gy = ry * adxl::G_PER_LSB, gz = rz * adxl::G_PER_LSB;
    sx += gx;
    sy += gy;
    sz += gz;
    qx += (double)gx * gx;
    qy += (double)gy * gy;
    qz += (double)gz * gz;
    minX = fminf(minX, gx);
    maxX = fmaxf(maxX, gx);
    minY = fminf(minY, gy);
    maxY = fmaxf(maxY, gy);
    minZ = fminf(minZ, gz);
    maxZ = fmaxf(maxZ, gz);
    ++n;
  }
  const uint32_t dt = micros() - t0;

  const float mx = sx / n, my = sy / n, mz = sz / n;
  Vibration v;
  v.rmsX = sqrtf(fmaxf(0.0f, (float)(qx / n) - mx * mx));
  v.rmsY = sqrtf(fmaxf(0.0f, (float)(qy / n) - my * my));
  v.rmsZ = sqrtf(fmaxf(0.0f, (float)(qz / n) - mz * mz));
  v.rmsTotal = sqrtf(v.rmsX * v.rmsX + v.rmsY * v.rmsY + v.rmsZ * v.rmsZ);
  v.peak = fmaxf(fmaxf(fmaxf(maxX - mx, mx - minX), fmaxf(maxY - my, my - minY)),
                 fmaxf(maxZ - mz, mz - minZ));
  v.sampleRateHz = dt > 0 ? (float)n * 1e6f / dt : 0;
  return v;
}

static bool adxlOk = false;
static uint32_t seq = 0;
static uint32_t lastSample = 0;

void setup() {
  node::begin("vibnode", "Vibration node");
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Wire.setClock(400000);  // fast-mode I2C: needed to approach 3200 Hz
  adxlOk = adxl::begin();
  Serial.println(adxlOk ? "ADXL345 OK (full-res, ±16 g)"
                        : "[error] ADXL345 not found (check CS->3V3, SDO->GND, SDA/SCL)");
}

void loop() {
  node::pollSerial();
  node::pollDownlink();

  if (millis() - lastSample < node::intervalS * 1000UL) return;
  lastSample = millis();

  if (!adxlOk && !(adxlOk = adxl::begin())) {
    Serial.println("ADXL345 unavailable");
    return;
  }

  const Vibration v = measure();
  const int battery = node::batteryPercent();
  ++seq;
  Serial.printf("RMS x:%.3f y:%.3f z:%.3f total:%.3f peak:%.3f g | %.0f Hz | B:%d%% #%lu\n", v.rmsX,
                v.rmsY, v.rmsZ, v.rmsTotal, v.peak, v.sampleRateHz, battery, (unsigned long)seq);
  node::send("RX:" + String(v.rmsX, 3) + "|RY:" + String(v.rmsY, 3) + "|RZ:" + String(v.rmsZ, 3) +
             "|RM:" + String(v.rmsTotal, 3) + "|PK:" + String(v.peak, 3) +
             "|FS:" + String((int)v.sampleRateHz) + "|B:" + String(battery) + "|N:" + String(seq));
}
