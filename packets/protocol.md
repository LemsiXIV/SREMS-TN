# LoRa Binary Protocol (P2P)

Frequency: 868.1 MHz (EU868), SF7–SF10 (configurable), BW 125kHz, CRC on

## Version 1 (24 bytes) — legacy
- 0:   Version (u8) = 0x01
- 1:   Device Type (u8)
- 2-3: Node ID (u16)
- 4-7: Epoch Seconds (u32)
- 8-9: Panel Voltage (mV, u16)
- 10-11: Panel Current (mA, u16)
- 12-13: Pump Current (mA, u16)
- 14-15: Flow (pulses/window, u16)
- 16-17: Module Temp (°C × 100, i16)
- 18-19: Battery Voltage (mV, u16)
- 20: Flags (u8)
- 21: RSSI placeholder (i8)
- 22: Reserved (u8)
- 23: CRC8 (Dallas/Maxim)

## Version 2 (26 bytes) — adds Reference PV Cell ADC
- 0:   Version (u8) = 0x02
- 1:   Device Type (u8)
- 2-3: Node ID (u16)
- 4-7: Epoch Seconds (u32)
- 8-9: Panel Voltage (u16, mV)
- 10-11: Panel Current (u16, mA)
- 12-13: Pump Current (u16, mA)
- 14-15: Flow (u16, pulses/window)
- 16-17: Module Temp (i16, °C × 100)
- 18-19: Battery Voltage (u16, mV)
- 20: Flags (u8)
- 21: RSSI placeholder (i8)
- 22-23: Ref Cell ADC (u16, 0..4095)
- 24: Reserved2 (u8)
- 25: CRC8 over bytes 0..24

Gateways must parse both v1 and v2; prefer v2 for soiling accuracy.
