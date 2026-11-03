// Text protocol shared by the LoRa nodes and the gateway.
//
//   uplink    NODE:<id>|<KEY>:<value>|<KEY>:<value>...
//             e.g. NODE:TH-01|T:23.41|U:58.20|B:87|N:1042
//   downlink  CFG:<id>|I:<seconds>
//
// Human-readable on purpose: packets can be inspected with any LoRa sniffer,
// and at SF10 a ~50-byte payload still fits comfortably in one frame.
// Pure C++ (no Arduino) so it is unit-tested on the host.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lp {

constexpr size_t kMaxFields = 12;
constexpr size_t kIdLen = 16;
constexpr size_t kKeyLen = 4;
constexpr size_t kValueLen = 16;

struct Field {
  char key[kKeyLen];
  char value[kValueLen];
};

struct Packet {
  char nodeId[kIdLen];
  Field fields[kMaxFields];
  uint8_t count;
};

// Parses an uplink. Returns false for anything that is not a well-formed
// NODE packet (noise, other networks, truncated frames).
bool parse(const char* text, size_t len, Packet& out);

// Value of `key`, or nullptr.
const char* get(const Packet& p, const char* key);

// Human-friendly name for a protocol key ("T" -> "temperature"); unknown keys
// are returned unchanged.
const char* fieldName(const char* key);

// Builds a downlink. Returns the length written, or 0 if it does not fit.
size_t formatConfig(char* buf, size_t size, const char* nodeId, uint32_t intervalS);

// Parses a downlink addressed to `nodeId`. Returns true and sets `intervalS`.
bool parseConfig(const char* text, size_t len, const char* nodeId, uint32_t& intervalS);

}  // namespace lp
