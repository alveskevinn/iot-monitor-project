// Shared plumbing of the LoRa sensor nodes: radio setup, node id in NVS,
// serial provisioning, battery reading and the downlink config listener.
#pragma once

#include <Arduino.h>

namespace node {

constexpr const char* kNoId = "NO-ID";
constexpr uint32_t kMinIntervalS = 1;
constexpr uint32_t kMaxIntervalS = 3600;

extern String id;
extern uint32_t intervalS;

// Loads the node id, starts Serial and the radio. Halts if the radio is missing.
void begin(const char* nvsNamespace, const char* banner);

// Handles SETID:/GETID on the serial console. Call from loop().
void pollSerial();

// Applies a CFG downlink addressed to this node, if one arrived. Call from loop().
void pollDownlink();

// Sends one uplink frame ("NODE:<id>|..." is prepended).
void send(const String& fields);

int batteryPercent();

}  // namespace node
