#include "lora_packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace lp {

static bool copyBounded(char* dst, size_t size, const char* src, size_t n) {
  if (n == 0 || n >= size) return false;
  memcpy(dst, src, n);
  dst[n] = '\0';
  return true;
}

bool parse(const char* text, size_t len, Packet& out) {
  out.count = 0;
  if (len < 6 || strncmp(text, "NODE:", 5) != 0) return false;

  const char* end = text + len;
  const char* p = text + 5;
  const char* bar = static_cast<const char*>(memchr(p, '|', end - p));
  const char* idEnd = bar ? bar : end;
  if (!copyBounded(out.nodeId, kIdLen, p, idEnd - p)) return false;

  p = idEnd;
  while (p < end) {
    ++p;  // skip '|'
    const char* next = static_cast<const char*>(memchr(p, '|', end - p));
    const char* segEnd = next ? next : end;
    const char* colon = static_cast<const char*>(memchr(p, ':', segEnd - p));
    if (!colon || out.count == kMaxFields) return false;
    Field& f = out.fields[out.count];
    if (!copyBounded(f.key, kKeyLen, p, colon - p)) return false;
    if (!copyBounded(f.value, kValueLen, colon + 1, segEnd - colon - 1)) return false;
    ++out.count;
    p = segEnd;
  }
  return true;
}

const char* get(const Packet& p, const char* key) {
  for (uint8_t i = 0; i < p.count; ++i) {
    if (strcmp(p.fields[i].key, key) == 0) return p.fields[i].value;
  }
  return nullptr;
}

const char* fieldName(const char* key) {
  static const struct {
    const char* key;
    const char* name;
  } names[] = {
      {"T", "temperature"},  {"U", "humidity"},        {"B", "battery_pct"},  {"N", "seq"},
      {"RX", "vib_rms_x_g"}, {"RY", "vib_rms_y_g"},    {"RZ", "vib_rms_z_g"}, {"RM", "vib_rms_g"},
      {"PK", "vib_peak_g"},  {"FS", "sample_rate_hz"},
  };
  for (const auto& n : names) {
    if (strcmp(n.key, key) == 0) return n.name;
  }
  return key;
}

size_t formatConfig(char* buf, size_t size, const char* nodeId, uint32_t intervalS) {
  const int n = snprintf(buf, size, "CFG:%s|I:%lu", nodeId, static_cast<unsigned long>(intervalS));
  return (n > 0 && static_cast<size_t>(n) < size) ? static_cast<size_t>(n) : 0;
}

bool parseConfig(const char* text, size_t len, const char* nodeId, uint32_t& intervalS) {
  char expected[kIdLen + 8];
  const int head = snprintf(expected, sizeof expected, "CFG:%s|I:", nodeId);
  if (head <= 0 || len <= static_cast<size_t>(head)) return false;
  if (strncmp(text, expected, head) != 0) return false;

  char digits[12];
  const size_t n = len - head;
  if (!copyBounded(digits, sizeof digits, text + head, n)) return false;
  for (size_t i = 0; i < n; ++i) {
    if (digits[i] < '0' || digits[i] > '9') return false;
  }
  intervalS = static_cast<uint32_t>(strtoul(digits, nullptr, 10));
  return true;
}

}  // namespace lp
