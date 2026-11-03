#include <lora_packet.h>
#include <string.h>
#include <unity.h>

using namespace lp;

void setUp() {}
void tearDown() {}

static bool parseStr(const char* s, Packet& p) {
  return parse(s, strlen(s), p);
}

static void test_parses_th_packet() {
  Packet p;
  TEST_ASSERT_TRUE(parseStr("NODE:TH-01|T:23.41|U:58.20|B:87|N:1042", p));
  TEST_ASSERT_EQUAL_STRING("TH-01", p.nodeId);
  TEST_ASSERT_EQUAL(4, p.count);
  TEST_ASSERT_EQUAL_STRING("23.41", get(p, "T"));
  TEST_ASSERT_EQUAL_STRING("1042", get(p, "N"));
  TEST_ASSERT_NULL(get(p, "PK"));
}

static void test_parses_vibration_packet() {
  Packet p;
  TEST_ASSERT_TRUE(
      parseStr("NODE:VB-07|RX:0.012|RY:0.009|RZ:0.031|RM:0.034|PK:0.118|FS:2710|B:64|N:9", p));
  TEST_ASSERT_EQUAL(8, p.count);
  TEST_ASSERT_EQUAL_STRING("0.118", get(p, "PK"));
}

static void test_accepts_id_only_packet() {
  Packet p;
  TEST_ASSERT_TRUE(parseStr("NODE:X", p));
  TEST_ASSERT_EQUAL(0, p.count);
}

static void test_rejects_noise_and_malformed() {
  Packet p;
  TEST_ASSERT_FALSE(parseStr("", p));
  TEST_ASSERT_FALSE(parseStr("hello world", p));
  TEST_ASSERT_FALSE(parseStr("NODE:", p));
  TEST_ASSERT_FALSE(parseStr("NODE:|T:1", p));  // empty id
  TEST_ASSERT_FALSE(parseStr("NODE:A|T", p));   // no colon
  TEST_ASSERT_FALSE(parseStr("NODE:A|:1", p));  // empty key
  TEST_ASSERT_FALSE(parseStr("NODE:A|T:", p));  // empty value
  TEST_ASSERT_FALSE(parseStr("NODE:AN_ID_THAT_IS_TOO_LONG|T:1", p));
  TEST_ASSERT_FALSE(parseStr("NODE:A|TEMP:1", p));  // key too long
}

static void test_rejects_too_many_fields() {
  Packet p;
  TEST_ASSERT_FALSE(parseStr("NODE:A|a:1|b:1|c:1|d:1|e:1|f:1|g:1|h:1|i:1|j:1|k:1|l:1|m:1", p));
}

static void test_respects_explicit_length() {
  Packet p;
  const char frame[] = "NODE:A|T:1.5GARBAGE";
  TEST_ASSERT_TRUE(parse(frame, 12, p));  // "NODE:A|T:1.5"
  TEST_ASSERT_EQUAL_STRING("1.5", get(p, "T"));
}

static void test_field_names() {
  TEST_ASSERT_EQUAL_STRING("temperature", fieldName("T"));
  TEST_ASSERT_EQUAL_STRING("vib_peak_g", fieldName("PK"));
  TEST_ASSERT_EQUAL_STRING("ZZ", fieldName("ZZ"));
}

static void test_config_roundtrip() {
  char buf[32];
  const size_t n = formatConfig(buf, sizeof buf, "TH-01", 60);
  TEST_ASSERT_EQUAL_STRING("CFG:TH-01|I:60", buf);
  uint32_t interval = 0;
  TEST_ASSERT_TRUE(parseConfig(buf, n, "TH-01", interval));
  TEST_ASSERT_EQUAL_UINT32(60, interval);
}

static void test_config_for_other_node_or_garbage_is_ignored() {
  uint32_t interval = 0;
  TEST_ASSERT_FALSE(parseConfig("CFG:TH-02|I:60", 14, "TH-01", interval));
  TEST_ASSERT_FALSE(parseConfig("CFG:TH-011|I:60", 15, "TH-01", interval));  // prefix clash
  TEST_ASSERT_FALSE(parseConfig("CFG:TH-01|I:6x", 14, "TH-01", interval));
  TEST_ASSERT_FALSE(parseConfig("CFG:TH-01|I:", 12, "TH-01", interval));
}

static void test_format_config_overflow() {
  char tiny[8];
  TEST_ASSERT_EQUAL(0, formatConfig(tiny, sizeof tiny, "TH-01", 60));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_th_packet);
  RUN_TEST(test_parses_vibration_packet);
  RUN_TEST(test_accepts_id_only_packet);
  RUN_TEST(test_rejects_noise_and_malformed);
  RUN_TEST(test_rejects_too_many_fields);
  RUN_TEST(test_respects_explicit_length);
  RUN_TEST(test_field_names);
  RUN_TEST(test_config_roundtrip);
  RUN_TEST(test_config_for_other_node_or_garbage_is_ignored);
  RUN_TEST(test_format_config_overflow);
  return UNITY_END();
}
