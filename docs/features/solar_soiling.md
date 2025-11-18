# Solar Soiling Detection, Monitoring, and Prediction (Off-grid, IoT + Edge AI)

Goal: Detect and anticipate PV performance loss due to dust/sand/humidity to reduce energy losses and schedule cleaning efficiently.

## Method
- Reference PV Cell → irradiance proxy (ADC on node).
- Expected DC power (gateway):
  - P_exp = Irradiance(ref_adc) × Gain(array) × (1 + TempCoef × (Tmodule − 25))
- Actual DC power:
  - P_act = V_panel × I_panel
- Soiling Index:
  - SI = max(0, (P_exp − P_act) / max(P_exp, ε))
- Trend:
  - Online linear regression of SI vs time for daylight samples (P_exp > threshold)
  - Predict days until SI reaches alert threshold

## Alerts
- SI > 10% sustained during daylight → SMS
- Predicted cleaning due within 3 days → SMS
- Pump anomaly or low battery → SMS

## Calibration
1) Install a small reference cell similar tilt/orientation to main array → connect to REF_ADC_PIN with a divider (max ~2.5V at ADC).
2) On a clean sunny window, tune:
   - `REF_ADC_K_W_PER_ADC` (W/m² per ADC count)
   - `EXP_GAIN_W_PER_WM2` (W per W/m² for your array size)
   so that P_exp ≈ P_act near midday.
3) Keep TEMP_COEF_PER_C ≈ −0.004/°C unless module datasheet differs.

## Notes
- If the ref cell is absent, fallback to voltage-based expected power works but is less accurate.
- Log cleaning dates to correlate SI improvements after maintenance.
