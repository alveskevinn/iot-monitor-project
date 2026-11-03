// LoRa -> Wi-Fi/MQTT gateway — Heltec WiFi LoRa 32 V2.
//
// Receives uplinks from any node, decodes them with lora_packet and publishes
// JSON on MQTT. Downlink: {"interval": N} on <base>/<node>/config is relayed to
// the node over LoRa (CFG:<node>|I:N), so sampling can be tuned without reflashing.
//
// Topics (<base> defaults to "lora"):
//   <base>/<node>/telemetry           {"node","rssi","snr","gateway","values":{...}}
//   <base>/<node>/config        (in)  {"interval": 60}
//   <base>/_gateways/<gw>/status      online / offline (retained, LWT)
//
// Hold PRG (GPIO0) during reset to reopen the Wi-Fi/MQTT configuration portal.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LoRa.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <lora_packet.h>

#include "heltec_v2.h"

#define PORTAL_BUTTON 0
#define PORTAL_TIMEOUT_S 180

struct Config {
  char host[64] = "";
  uint16_t port = 1883;
  char user[32] = "";
  char pass[48] = "";
  char base[48] = "lora";
};

static Config cfg;
static char gatewayId[24];
static Preferences prefs;
static WiFiClient net;
static PubSubClient mqtt(net);
static Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST);

static uint32_t rxCount = 0;
static uint32_t lastMqttTry = 0;
static String lastNode = "-";
static int lastRssi = 0;

// --- settings ----------------------------------------------------------------
static void loadConfig() {
  prefs.begin("gateway", true);
  prefs.getString("host", cfg.host, sizeof cfg.host);
  cfg.port = prefs.getUShort("port", cfg.port);
  prefs.getString("user", cfg.user, sizeof cfg.user);
  prefs.getString("pass", cfg.pass, sizeof cfg.pass);
  if (prefs.isKey("base")) prefs.getString("base", cfg.base, sizeof cfg.base);
  prefs.end();
}

static void saveConfig() {
  prefs.begin("gateway", false);
  prefs.putString("host", cfg.host);
  prefs.putUShort("port", cfg.port);
  prefs.putString("user", cfg.user);
  prefs.putString("pass", cfg.pass);
  prefs.putString("base", cfg.base);
  prefs.end();
}

static void runPortal(bool force) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setTitle("LoRa Gateway");
  char port[6];
  snprintf(port, sizeof port, "%u", cfg.port);
  WiFiManagerParameter pHost("host", "MQTT broker host", cfg.host, sizeof cfg.host - 1);
  WiFiManagerParameter pPort("port", "MQTT port", port, 5);
  WiFiManagerParameter pUser("user", "MQTT username (optional)", cfg.user, sizeof cfg.user - 1);
  WiFiManagerParameter pPass("pass", "MQTT password (optional)", cfg.pass, sizeof cfg.pass - 1,
                             "type='password'");
  WiFiManagerParameter pBase("base", "Base topic", cfg.base, sizeof cfg.base - 1);
  for (auto* p : {&pHost, &pPort, &pUser, &pPass, &pBase}) wm.addParameter(p);
  bool saved = false;
  wm.setSaveParamsCallback([&saved]() { saved = true; });
  wm.setSaveConfigCallback([&saved]() { saved = true; });

  const bool ok = force ? wm.startConfigPortal(gatewayId) : wm.autoConnect(gatewayId);
  if (saved) {
    strlcpy(cfg.host, pHost.getValue(), sizeof cfg.host);
    const long p = atol(pPort.getValue());
    if (p > 0 && p < 65536) cfg.port = p;
    strlcpy(cfg.user, pUser.getValue(), sizeof cfg.user);
    strlcpy(cfg.pass, pPass.getValue(), sizeof cfg.pass);
    if (strlen(pBase.getValue())) strlcpy(cfg.base, pBase.getValue(), sizeof cfg.base);
    saveConfig();
  }
  if (!ok) ESP.restart();
}

// --- display -----------------------------------------------------------------
static void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(gatewayId);
  display.printf("WiFi: %s\n", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "offline");
  display.printf("MQTT: %s\n", mqtt.connected() ? "OK" : "OFFLINE");
  display.printf("RX:   %lu\n", (unsigned long)rxCount);
  display.printf("Last: %s\n", lastNode.c_str());
  display.printf("RSSI: %d dBm\n", lastRssi);
  display.display();
}

// --- MQTT --------------------------------------------------------------------
static String statusTopic() {
  return String(cfg.base) + "/_gateways/" + gatewayId + "/status";
}

static void onMqtt(char* topic, byte* payload, unsigned int len) {
  // <base>/<node>/config
  const String t(topic);
  const String prefix = String(cfg.base) + "/";
  if (!t.startsWith(prefix) || !t.endsWith("/config")) return;
  const String nodeId = t.substring(prefix.length(), t.length() - 7);
  if (nodeId.isEmpty() || nodeId.length() >= lp::kIdLen) return;

  JsonDocument doc;
  if (deserializeJson(doc, payload, len) || !doc["interval"].is<uint32_t>()) return;
  char frame[40];
  const size_t n = lp::formatConfig(frame, sizeof frame, nodeId.c_str(), doc["interval"]);
  if (n == 0) return;

  // Nodes listen between samples; repeat a few times to beat collisions.
  for (int i = 0; i < 3; ++i) {
    LoRa.beginPacket();
    LoRa.write(reinterpret_cast<const uint8_t*>(frame), n);
    LoRa.endPacket();
    delay(150);
  }
  Serial.printf("[downlink] %s\n", frame);  // next parsePacket() returns to RX
}

static void ensureMqtt() {
  if (mqtt.connected() || strlen(cfg.host) == 0) return;
  if (millis() - lastMqttTry < 5000) return;
  lastMqttTry = millis();
  mqtt.setServer(cfg.host, cfg.port);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);
  const String status = statusTopic();
  const bool ok =
      mqtt.connect(gatewayId, strlen(cfg.user) ? cfg.user : nullptr,
                   strlen(cfg.pass) ? cfg.pass : nullptr, status.c_str(), 1, true, "offline");
  if (!ok) return;
  mqtt.publish(status.c_str(), "online", true);
  mqtt.subscribe((String(cfg.base) + "/+/config").c_str(), 1);
  drawStatus();
}

// --- LoRa uplink -------------------------------------------------------------
static void handleUplink() {
  char buf[128];
  int n = 0;
  while (LoRa.available() && n < (int)sizeof buf - 1) buf[n++] = (char)LoRa.read();
  buf[n] = '\0';

  lp::Packet pkt;
  if (!lp::parse(buf, n, pkt)) return;  // noise or a foreign network

  ++rxCount;
  lastNode = pkt.nodeId;
  lastRssi = LoRa.packetRssi();

  JsonDocument doc;
  doc["node"] = pkt.nodeId;
  doc["gateway"] = gatewayId;
  doc["rssi"] = lastRssi;
  doc["snr"] = LoRa.packetSnr();
  JsonObject values = doc["values"].to<JsonObject>();
  for (uint8_t i = 0; i < pkt.count; ++i) {
    char* end;
    const double v = strtod(pkt.fields[i].value, &end);
    const char* name = lp::fieldName(pkt.fields[i].key);
    if (*end == '\0') {
      values[name] = v;
    } else {
      values[name] = pkt.fields[i].value;  // keep non-numeric values as strings
    }
  }
  String out;
  serializeJson(doc, out);
  Serial.println(out);
  if (mqtt.connected()) {
    mqtt.publish((String(cfg.base) + "/" + pkt.nodeId + "/telemetry").c_str(), out.c_str());
  }
  drawStatus();
}

void setup() {
  Serial.begin(115200);
  pinMode(PORTAL_BUTTON, INPUT_PULLUP);
  snprintf(gatewayId, sizeof gatewayId, "lora-gw-%06llx", ESP.getEfuseMac() & 0xFFFFFFULL);

  // OLED
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  display.clearDisplay();
  display.display();

  loadConfig();
  drawStatus();
  runPortal(digitalRead(PORTAL_BUTTON) == LOW || strlen(cfg.host) == 0);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("[error] LoRa radio not found");
    while (true) delay(1000);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_DBM);
  LoRa.enableCrc();

  ensureMqtt();
  drawStatus();
}

void loop() {
  if (!WiFi.isConnected()) {
    WiFi.reconnect();
    delay(500);
  }
  ensureMqtt();
  mqtt.loop();

  if (LoRa.parsePacket() > 0) handleUplink();
}
