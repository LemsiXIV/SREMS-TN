/*
  ESP32 Solar-Pump Node (LoRa P2P) — Protocol v2 with Reference PV Cell
  Hardware: ESP32-DevKitC or ESP32-WROOM-32D + RFM95 (SX1276/78) @ EU868
*/
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include "config.h"

#include <Adafruit_INA219.h>
Adafruit_INA219 ina_panel(0x40);
Adafruit_INA219 ina_pump(0x41);

#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Flow meter
#define FLOW_PIN 5
volatile uint32_t flow_pulses = 0;
void IRAM_ATTR flowISR() { flow_pulses++; }

// CRC8 Dallas/Maxim
uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  while (len--) {
    uint8_t inbyte = *data++;
    for (uint8_t i = 8; i; --i) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

// Battery voltage (via ADC divider) optional
#define VBAT_PIN 35
#define VBAT_R1  30000.0
#define VBAT_R2  7500.0
float readBatteryMV() {
  uint16_t raw = analogRead(VBAT_PIN);
  // NOTE: ADC scaling depends on attenuation; calibrate for your board
  float v = (raw / 4095.0f) * 1100.0f; // mV at ADC (assuming ~1.1V ref)
  float mv = v * ((VBAT_R1 + VBAT_R2) / VBAT_R2);
  return mv;
}

uint16_t readRefCellADC() {
  return (uint16_t)analogRead(REF_ADC_PIN);
}

void setupRadio() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    while (1) { delay(500); }
  }
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  ina_panel.begin();
  ina_pump.begin();

  sensors.begin();
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, RISING);

  analogSetWidth(12); // 0..4095

  setupRadio();
}

void packAndSend(uint16_t panel_mv, uint16_t panel_ma, uint16_t pump_ma,
                 uint16_t flow_count, int16_t temp_cC, uint16_t batt_mv,
                 uint16_t ref_adc) {
  // Protocol v2: 26 bytes
  uint8_t pkt[26] = {0};
  pkt[0] = PROTO_VERSION;      // 0x02
  pkt[1] = DEVICE_TYPE;
  pkt[2] = NODE_ID & 0xFF; pkt[3] = (NODE_ID >> 8) & 0xFF;

  uint32_t epoch = (uint32_t)(millis()/1000 + 1731888000UL); // fallback epoch
  memcpy(&pkt[4], &epoch, 4);

  memcpy(&pkt[8],  &panel_mv, 2);
  memcpy(&pkt[10], &panel_ma, 2);
  memcpy(&pkt[12], &pump_ma, 2);
  memcpy(&pkt[14], &flow_count, 2);
  memcpy(&pkt[16], &temp_cC, 2);
  memcpy(&pkt[18], &batt_mv, 2);

  uint8_t flags = 0;
  if (pump_ma > PUMP_CURRENT_MIN_MA) flags |= 0x01;
  pkt[20] = flags;

  pkt[21] = 0; // RSSI placeholder
  memcpy(&pkt[22], &ref_adc, 2);
  pkt[24] = 0; // reserved2
  pkt[25] = crc8(pkt, 25);

  for (int retry = 0; retry <= LORA_TX_RETRY; retry++) {
    LoRa.beginPacket();
    LoRa.write(pkt, sizeof(pkt));
    LoRa.endPacket();
    delay(50);
  }
}

void loop() {
  // Measurement window
  flow_pulses = 0;
  const uint32_t window_ms = 5000;
  float panel_v = 0, panel_a = 0, pump_a = 0;
  for (int i=0; i<10; i++) {
    panel_v += ina_panel.getBusVoltage_V() * 1000.0f; // mV
    panel_a += ina_panel.getCurrent_mA();
    pump_a  += ina_pump.getCurrent_mA();
    delay(window_ms/10);
  }
  panel_v /= 10.0f;
  panel_a /= 10.0f;
  pump_a  /= 10.0f;

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC < -50 || tempC > 125) tempC = 35.0f;

  uint16_t panel_mv = (uint16_t) constrain(panel_v, 0, 65535);
  uint16_t panel_ma = (uint16_t) constrain(panel_a, 0, 65535);
  uint16_t pump_ma  = (uint16_t) constrain(pump_a, 0, 65535);
  uint16_t flow_cnt = (uint16_t) flow_pulses;
  int16_t temp_cC   = (int16_t) (tempC * 100);
  uint16_t batt_mv  = (uint16_t) readBatteryMV();
  uint16_t ref_adc  = readRefCellADC();

  packAndSend(panel_mv, panel_ma, pump_ma, flow_cnt, temp_cC, batt_mv, ref_adc);

#if DEEP_SLEEP_ENABLED
  esp_sleep_enable_timer_wakeup((uint64_t)SAMPLE_PERIOD_SEC * 1000000ULL);
  esp_deep_sleep_start();
#else
  delay(SAMPLE_PERIOD_SEC * 1000);
#endif
}