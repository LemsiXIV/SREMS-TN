# SREMS-TN (Off-Grid ESP32 + LoRa + Lightweight AI)

Low-cost, off-grid IoT for remote Tunisian solar installations (agricultural pumping, irrigation, greenhouses). Nodes use ESP32 + LoRa to send compact telemetry to a nearby gateway (ESP32) that:
- hosts a local WiFi AP with a basic dashboard (no Internet required),
- raises alerts via SMS (SIM800L) or LoRa beacons,
- runs lightweight AI (expected power baseline, soiling index, pump-health checks),
- stores a rolling buffer in RAM (optional SD later).

EU868 band, ESP32-DevKitC or ESP32-WROOM-32D + RFM95 (SX1276/78) @ 868 MHz.

## Quick Start
1. Create a branch:
   ```bash
   git checkout -b lemsi_test_branch
   ```
2. Build (PlatformIO):
   ```bash
   pio run -e node_solar_pump
   pio run -e gateway_lora_gsm
   ```
3. Flash:
   ```bash
   pio run -e node_solar_pump -t upload
   pio run -e gateway_lora_gsm -t upload
   ```
4. Connect to AP `SREMS-GW` (pwd `srems1234`), open http://192.168.4.1

## Features (Phase 1)
- LoRa P2P binary protocol v2 (with reference PV-cell ADC) + CRC8
- Real-time Soiling Index: SI = (Expected − Actual) / Expected (temp-derated)
- Trend-based cleaning prediction (days until threshold)
- Pump health (current vs flow pulses), low-battery detection
- SMS alerts to +216 25301941
- Offline dashboard (AP-hosted): Soiling %, Health %, Battery V, Pump state, power sparkline

## Folders
- firmware/esp32/common: shared config
- firmware/esp32/node_solar_pump: node firmware
- firmware/esp32/gateway_lora_gsm: gateway firmware
- ai/edge: tiny baseline helper (optional)
- packets/: protocol spec
- docs/: architecture, soiling method, thresholds
- power/: power budget
- dashboard/web: sample static dashboard (gateway serves its own embedded page)

See docs/architecture/offgrid.md and docs/features/solar_soiling.md for details and calibration steps.
