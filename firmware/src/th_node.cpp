// Temperature/humidity node — Heltec LoRa 32 V2 + SHT31 on I2C (SDA 21, SCL 22).
// Continuous mode with a remotely adjustable interval (CFG downlink via gateway).
// Uplink: NODE:<id>|T:<°C>|U:<%RH>|B:<battery %>|N:<seq>

#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <Wire.h>

#include "heltec_v2.h"
#include "node_common.h"

static Adafruit_SHT31 sht(&Wire1);
static bool shtOk = false;
static uint32_t seq = 0;
static uint32_t lastSample = 0;

void setup() {
  node::begin("thnode", "TH node");
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  shtOk = sht.begin(0x44);
  Serial.println(shtOk ? "SHT31 OK" : "[error] SHT31 not found");
}

void loop() {
  node::pollSerial();
  node::pollDownlink();

  if (millis() - lastSample < node::intervalS * 1000UL) return;
  lastSample = millis();

  if (!shtOk && !(shtOk = sht.begin(0x44))) {
    Serial.println("SHT31 unavailable");
    return;
  }
  const float t = sht.readTemperature();
  const float h = sht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("[error] invalid SHT31 reading");
    return;
  }

  const int battery = node::batteryPercent();
  ++seq;
  Serial.printf("T:%.2f U:%.2f B:%d%% #%lu\n", t, h, battery, (unsigned long)seq);
  node::send("T:" + String(t, 2) + "|U:" + String(h, 2) + "|B:" + String(battery) +
             "|N:" + String(seq));
}
