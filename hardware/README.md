# Hardware

All three images run on the **Heltec WiFi LoRa 32 V2** (ESP32 + SX1276 + 0.96" OLED), 915 MHz. For the EU 868 MHz band or 433 MHz boards, change `LORA_FREQ` in [`firmware/include/heltec_v2.h`](../firmware/include/heltec_v2.h).

## Bill of materials (per node)

| Qty | Part | Notes |
|---|---|---|
| 1 | Heltec WiFi LoRa 32 V2 + 915 MHz antenna | never transmit without an antenna |
| 1 | SHT31 breakout (TH node) **or** ADXL345 breakout (vibration node) | |
| 1 | 1S LiPo / 18650 cell + JST 1.25 lead | the board has a built-in charger |
| 1 | IP65 enclosure, cable gland | |
| 1 | M4 stud / 2-part epoxy (vibration node) | rigid mounting matters more than the sensor |

Gateway: the same board plus a 5 V USB supply. It needs Wi-Fi coverage, and no sensor.

## Wiring

### TH node: SHT31 (I2C, address 0x44)

| SHT31 | Heltec V2 |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

### Vibration node: ADXL345 (I2C, address 0x53)

| ADXL345 | Heltec V2 | |
|---|---|---|
| VCC | 3V3 | |
| GND | GND | |
| SDA | GPIO21 | |
| SCL | GPIO22 | |
| CS | **3V3** | selects I2C. A floating CS does **not** work |
| SDO | GND | sets the address to 0x53 |

### Reserved on the Heltec V2 (do not reuse)

| GPIO | Used by |
|---|---|
| 5, 19, 27, 18, 14, 26 | SX1276 (SCK, MISO, MOSI, SS, RST, DIO0) |
| 4, 15, 16 | OLED (SDA, SCL, RST) |
| 37 | battery divider (ADC) |
| 0 | PRG button (portal on the gateway) |

## Radio settings

| Parameter | Value | Why |
|---|---|---|
| Frequency | 915 MHz | AU915/US915; Brazil allows 902–907.5 and 915–928 MHz |
| Spreading factor | SF10 | ~1–2 km in an industrial plant, ~1 s air time for a 60-byte frame |
| Bandwidth | 125 kHz | |
| Coding rate | 4/8 | extra robustness against motor/VFD noise |
| TX power | 20 dBm | check local limits |
| CRC | on | corrupted frames are dropped by the radio |

## Mounting the vibration node

- Mount it on the **bearing housing**, as close to the load zone as possible. Never on a fan cover or a sheet-metal guard.
- Use a stud or epoxy. Magnets and double-sided tape damp high frequencies and give you a different (lower) RMS.
- Orientation matters: keep the same axis radial or axial across machines, so trends can be compared.
- At the 3200 Hz output rate, the usable band is roughly 10–1000 Hz, which is right for overall-level severity in the ISO 10816 / 20816 style. Bearing defect frequencies need an accelerometer with a higher bandwidth.
