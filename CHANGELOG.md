# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-11-03

### Added
- `firmware/`: PlatformIO project with three images (TH node with SHT31, vibration node with ADXL345, LoRa → MQTT gateway with OLED and captive portal).
- `lora_packet` library: text uplink/downlink protocol with host unit tests.
- On-node vibration metrics: AC RMS per axis, total RMS, peak and the measured sample rate.
- Remote sampling interval over MQTT → LoRa downlink.
- `hardware/`: BOM, wiring, reserved pins, radio settings and mounting guide.
- CI: lint, native tests and builds for all three images.

### Changed
- The interactive build guide (3D viewer, wiring, BOM) moved to `web/`.
