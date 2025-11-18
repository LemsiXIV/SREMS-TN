# Off-grid Architecture (ESP32 + LoRa P2P)

## Topology
- Multiple Sensor Nodes → LoRa P2P → One Local Gateway
- Gateway → Local WiFi AP → HTML dashboard
- SMS alerts via SIM800L; no Internet required

## Data Flow
1. Node wakes on schedule, samples sensors (panel V/I, pump current, flow, module temp, battery, ref PV cell).
2. Node sends compact binary payload (v2, 26 bytes) over LoRa; sleeps.
3. Gateway receives, validates CRC, updates ring buffer, runs analytics:
   - Expected power (prefers reference cell irradiance + temp derating)
   - Soiling Index (expected vs actual)
   - Pump health (current vs flow mismatch)
   - Battery voltage checks
4. Alerts via SMS; dashboard shows KPIs and recent history.

## Why P2P LoRa?
- Remote farms often lack Internet; P2P has no network dependency.
- Long range, low power, simple deployment.

## Edge AI
- Expected power: ref cell ADC → irradiance proxy → temp derating → P_exp
- SI: (P_exp − P_act)/P_exp
- Trend: online linear regression of SI to predict cleaning time
- Forecast (Phase 1 minimal): use SI trend and daily energy persistence; can upgrade later.

## Power Strategy (Node)
- 3-min cadence, deep sleep between transmissions.
- ADC and sensors low duty cycle.
- Optional adaptive duty cycle (slower at night/low battery).