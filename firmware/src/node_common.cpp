#include "node_common.h"

#include <LoRa.h>
#include <Preferences.h>
#include <SPI.h>
#include <lora_packet.h>

#include "heltec_v2.h"

namespace node {

String id = kNoId;
uint32_t intervalS = 5;

static Preferences prefs;
static const char* nvsNs = "node";

void begin(const char* nvsNamespace, const char* banner) {
  nvsNs = nvsNamespace;
  Serial.begin(115200);
  delay(100);

  prefs.begin(nvsNs, true);
  id = prefs.getString("node_id", kNoId);
  prefs.end();

  Serial.printf("\n=== %s [%s] ===\n", banner, id.c_str());
  if (id == kNoId) Serial.println("[warn] node id not provisioned, send SETID:<id>");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[error] LoRa radio not found");
    while (true) delay(1000);
  }
  LoRa.setTxPower(LORA_TX_DBM);
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.enableCrc();
  Serial.printf("LoRa OK: %.0f MHz, SF%d, %d dBm\n", LORA_FREQ / 1e6, LORA_SF, LORA_TX_DBM);
}

void pollSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.startsWith("SETID:")) {
    String next = line.substring(6);
    next.trim();
    if (next.isEmpty() || next.length() >= lp::kIdLen || next.indexOf('|') >= 0) {
      Serial.println("[provision] invalid id (1-15 chars, no '|')");
      return;
    }
    prefs.begin(nvsNs, false);
    prefs.putString("node_id", next);
    prefs.end();
    id = next;
    Serial.printf("[provision] node id = %s\n", id.c_str());
  } else if (line == "GETID") {
    Serial.printf("[provision] node id = %s, interval = %lus\n", id.c_str(),
                  (unsigned long)intervalS);
  }
}

void pollDownlink() {
  const int size = LoRa.parsePacket();
  if (size == 0) return;
  char buf[48];
  int n = 0;
  while (LoRa.available() && n < (int)sizeof buf - 1) buf[n++] = (char)LoRa.read();
  buf[n] = '\0';

  uint32_t next;
  if (!lp::parseConfig(buf, n, id.c_str(), next)) return;
  if (next >= kMinIntervalS && next <= kMaxIntervalS && next != intervalS) {
    Serial.printf("[config] interval %lus -> %lus\n", (unsigned long)intervalS,
                  (unsigned long)next);
    intervalS = next;
  }
}

void send(const String& fields) {
  const String frame = "NODE:" + id + "|" + fields;
  LoRa.beginPacket();
  LoRa.print(frame);
  LoRa.endPacket();  // parsePacket() in pollDownlink() puts the radio back in RX
}

int batteryPercent() {
  analogSetAttenuation(ADC_11db);
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(VBAT_PIN);
  const float v = (sum / 16.0f / 4095.0f) * 3.3f * VBAT_DIVIDER;
  if (v >= 4.2f) return 100;
  if (v <= 3.0f) return 0;
  return (int)((v - 3.0f) / 1.2f * 100.0f);
}

}  // namespace node
