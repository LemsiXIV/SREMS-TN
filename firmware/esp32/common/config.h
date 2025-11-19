#pragma once

// ====== Radio (RFM95 SX1276/78) ======
#define LORA_SS        18
#define LORA_RST       14
#define LORA_DIO0      26
#define LORA_BAND      868E6  // EU868 confirmed

// ====== Node identity ======
#define NODE_ID        0x0001
#define DEVICE_TYPE    0x01
#define PROTO_VERSION  0x02   // v2 includes ref-cell ADC

// ====== WiFi AP (gateway) ======
#define AP_SSID        "SREMS-GW"
#define AP_PASS        "srems1234"

// ====== SMS/GSM (gateway) ======
#define GSM_BAUD       9600
#define GSM_TX         27
#define GSM_RX         25
#define ALERT_PHONE    "+21625301941"  // confirmed

// ====== Sampling (node) ======
#define SAMPLE_PERIOD_SEC   180      // every 3 minutes
#define LORA_TX_RETRY       2
#define DEEP_SLEEP_ENABLED  1

// ====== Thresholds ======
#define SOILING_ALERT            0.10f    // 10% loss vs expected
#define SOILING_PREDICT_DAYS     3        // alert if cleaning due within N days
#define PUMP_FLOW_MIN_PPS        2        // pulses/s minimal when pump on
#define PUMP_CURRENT_MIN_MA      200      // pump considered ON if above
#define BATTERY_LOW_MV           11600    // 12V system low

// ====== Reference PV cell (node ADC) — optional but recommended ======
#define REF_ADC_PIN              34       // ADC1_CH6 on ESP32
#define REF_ADC_MIN_VALID        50       // raw ADC threshold to consider "daylight"

// ====== Expected power model (gateway-side calibration) ======
// Tune these after a clean, sunny calibration window.
#define REF_ADC_K_W_PER_ADC      0.25f    // W/m2 per ADC count (site dependent)
#define EXP_GAIN_W_PER_WM2       1.0f     // W output per W/m2 irradiance (array dependent)
#define TEMP_COEF_PER_C          -0.004f  // derating per °C from 25C
