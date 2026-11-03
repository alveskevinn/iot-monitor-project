// Heltec WiFi LoRa 32 V2 board wiring and radio settings shared by all images.
#pragma once

// SX1276 (on-board)
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_FREQ 915E6  // AU915/US915 band; use 868E6 in the EU, 433E6 for 433 MHz boards
#define LORA_SF 10
#define LORA_BW 125E3
#define LORA_CR 8
#define LORA_TX_DBM 20

// On-board OLED (SSD1306 128x64) — gateway only
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

// External I2C for sensors (second bus, keeps the OLED bus free).
// Field-tested on V2 nodes, which run with the OLED off.
#define SENSOR_SDA 21
#define SENSOR_SCL 22

// Battery sense: on-board divider to GPIO37
#define VBAT_PIN 37
#define VBAT_DIVIDER 4.90f
