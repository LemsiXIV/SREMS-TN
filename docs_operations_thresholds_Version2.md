# Thresholds and Alerts

Defaults (tune in firmware/esp32/common/config.h):
- Soiling Alert: `SOILING_ALERT = 0.10` (10%) sustained during daylight
- Cleaning Prediction Window: `SOILING_PREDICT_DAYS = 3`
- Pump Fault: pump current > 200 mA but flow pulses < 2 pps over 5 s
- Battery Low: < 11.6 V (12V system)

Alert Channels:
- SMS (SIM800L): critical alerts (soiling, predicted cleaning soon, pump fault, low battery)
- Dashboard banner (local AP): all alerts