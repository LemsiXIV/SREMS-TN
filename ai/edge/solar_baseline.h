#pragma once
// Lightweight baseline: expected PV power from panel voltage + temperature
// Use this on node or gateway for quick soiling estimation.
// Tune coeffs after short calibration per site.

typedef struct {
  float k_v;       // amps per volt proxy (array dependent)
  float eff0;      // nominal efficiency proxy
  float temp_coef; // -0.004 per °C from 25C typical
  float v_offset;  // threshold voltage for meaningful irradiance
} PVBaselineCoeffs;

static const PVBaselineCoeffs DEFAULT_COEFFS = {
  .k_v = 0.5f,
  .eff0 = 0.19f,
  .temp_coef = -0.004f,
  .v_offset = 15.0f
};

inline float pv_expected_power_w(float panel_v, float temp_c, const PVBaselineCoeffs &c = DEFAULT_COEFFS) {
  float derate = 1.0f + c.temp_coef * (temp_c - 25.0f);
  float i_proxy = (panel_v - c.v_offset) * c.k_v;
  if (i_proxy < 0) i_proxy = 0;
  return panel_v * i_proxy * c.eff0 * derate;
}
