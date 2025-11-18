/*
  ESP32 Gateway: LoRa RX + Local WiFi AP + Web Dashboard + SMS Alerts (SIM800L)
  Adds: Soiling Index computation using Reference PV Cell (protocol v2) and trend-based prediction.
*/
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LoRa.h>
#include "../common/config.h"

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
HardwareSerial gsmSerial(1);
TinyGsm modem(gsmSerial);

WebServer server(80);

// Ring buffer
#define MAX_POINTS 512
struct Point {
  uint32_t ts;
  uint16_t panel_mv, panel_ma, pump_ma, flow_cnt, batt_mv;
  int16_t temp_cC;
  uint16_t ref_adc;   // new
  uint8_t flags;
  float soiling;
  float health;
  float pr;           // performance ratio proxy
  float p_act_w;
  float p_exp_w;
};
Point buf[MAX_POINTS];
volatile int head = 0, countPts = 0;

// Trend estimation (online linear regression y ~ a + b*t)
struct Trend {
  double S_t = 0, S_y = 0, S_tt = 0, S_ty = 0;
  int n = 0;
  uint32_t t0 = 0;
  void reset() { S_t=S_y=S_tt=S_ty=0; n=0; t0=0; }
  void add(uint32_t ts, float y) {
    if (t0==0) t0 = ts;
    double t = (double)(ts - t0) / 3600.0; // hours
    S_t += t; S_y += y; S_tt += t*t; S_ty += t*y; n++;
    if (n > 360) { reset(); } // cap ~3 days @ 12-min cadence
  }
  // slope per day
  double slope_per_day() const {
    if (n < 5) return 0.0;
    double den = (n * S_tt - S_t * S_t);
    if (fabs(den) < 1e-9) return 0.0;
    double b_per_hour = (n * S_ty - S_t * S_y) / den;
    return b_per_hour * 24.0;
  }
} siTrend;

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

// Expected power model
float expected_power_from_ref(uint16_t ref_adc, float tempC) {
  float irr = REF_ADC_K_W_PER_ADC * (float)ref_adc; // W/m2
  float derate = 1.0f + TEMP_COEF_PER_C * (tempC - 25.0f);
  float pexp = irr * EXP_GAIN_W_PER_WM2 * derate;
  return max(0.0f, pexp);
}

float expected_power_from_voltage(uint16_t panel_mv, float tempC) {
  float V = panel_mv / 1000.0f;
  float derate = 1.0f + TEMP_COEF_PER_C * (tempC - 25.0f);
  float I_proxy = max(0.0f, (V - 15.0f)) * 0.5f; // crude proxy
  return V * I_proxy * 0.19f * derate;
}

void sendSMSAlert(const String &msg) {
  static bool ready = false;
  if (!ready) {
    gsmSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
    delay(600);
    modem.restart();
    ready = true;
  }
  modem.sendSMS(ALERT_PHONE, msg);
}

void enqueue(Point p) {
  buf[head] = p;
  head = (head + 1) % MAX_POINTS;
  if (countPts < MAX_POINTS) countPts++;
}

void handleRoot() {
  String html = R"(
    <!doctype html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
    <title>SREMS Gateway</title>
    <style>body{font-family:sans-serif;margin:16px} .kpi{display:inline-block;margin:8px;padding:8px;border:1px solid #ddd}</style>
    </head><body>
    <h2>SREMS-TN Gateway</h2>
    <div class="kpi"><b>Points:</b> <span id="n">0</span></div>
    <div class="kpi"><b>Soiling:</b> <span id="si">-</span></div>
    <div class="kpi"><b>Health:</b> <span id="hs">-</span></div>
    <div class="kpi"><b>Cleaning in:</b> <span id="cd">-</span></div>
    <canvas id="p" width="360" height="120"></canvas>
    <script>
    async function load(){ const r = await fetch('/api/data'); const j = await r.json();
      document.getElementById('n').textContent = j.data.length;
      if(j.data.length){ const last = j.data[j.data.length-1];
        document.getElementById('si').textContent = (last.soiling*100).toFixed(1)+'%';
        document.getElementById('hs').textContent = (last.health*100).toFixed(0)+'%';
        document.getElementById('cd').textContent = (last.pred_days>=0? last.pred_days.toFixed(1)+' days' : 'n/a');
      }
      const ctx = document.getElementById('p').getContext('2d');
      const ys = j.data.map(d=>d.p_act_w);
      ctx.clearRect(0,0,360,120); ctx.beginPath(); ctx.strokeStyle='#06f';
      const max = Math.max(...ys,1), min=0;
      ys.forEach((y,i)=>{ const xp=i*(360/Math.max(1,ys.length-1)); const yp=110-((y-min)/(max-min+1e-6))*100; if(i==0) ctx.moveTo(xp,yp); else ctx.lineTo(xp,yp);}); ctx.stroke();
    }
    setInterval(load, 3000); load();
    </script></body></html>
  )";
  server.send(200, "text/html", html);
}

void handleData() {
  String out = "{\"data\":[";
  for (int i = 0; i < countPts; i++) {
    int idx = (head - countPts + i + MAX_POINTS) % MAX_POINTS;
    const Point &p = buf[idx];
    double slope_day = siTrend.slope_per_day();
    double pred_days = -1.0;
    if (slope_day > 1e-4) {
      double remain = SOILING_ALERT - p.soiling;
      if (remain > 0) pred_days = remain / slope_day;
    }
    out += "{\"ts\":" + String(p.ts) +
           ",\"panel_mv\":" + String(p.panel_mv) +
           ",\"panel_ma\":" + String(p.panel_ma) +
           ",\"pump_ma\":" + String(p.pump_ma) +
           ",\"flow\":" + String(p.flow_cnt) +
           ",\"temp_c\":" + String(p.temp_cC/100.0f,1) +
           ",\"batt_mv\":" + String(p.batt_mv) +
           ",\"ref_adc\":" + String(p.ref_adc) +
           ",\"soiling\":" + String(p.soiling,3) +
           ",\"health\":" + String(p.health,3) +
           ",\"pr\":" + String(p.pr,3) +
           ",\"p_act_w\":" + String(p.p_act_w,1) +
           ",\"p_exp_w\":" + String(p.p_exp_w,1) +
           ",\"pred_days\":" + String(pred_days,2) + "}";
    if (i < countPts-1) out += ",";
  }
  out += "]}";
  server.send(200, "application/json", out);
}

void setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  server.on("/", handleRoot);
  server.on("/api/data", handleData);
  server.begin();
}

void setupRadio() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    while(1){ delay(500); }
  }
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();
}

void setup() {
  Serial.begin(115200);
  setupAP();
  setupRadio();
}

void loop() {
  server.handleClient();

  int pktSize = LoRa.parsePacket();
  if (pktSize >= 24) {
    uint8_t pkt[64];
    int n = LoRa.readBytes(pkt, min(pktSize, 64));
    bool parsed = false;

    // Try v2 (26 bytes)
    if (n >= 26 && crc8(pkt, 25) == pkt[25] && pkt[0] == 0x02) {
      uint32_t ts; memcpy(&ts, &pkt[4], 4);
      uint16_t panel_mv, panel_ma, pump_ma, flow_cnt, batt_mv, ref_adc;
      int16_t temp_cC;
      memcpy(&panel_mv, &pkt[8], 2);
      memcpy(&panel_ma, &pkt[10], 2);
      memcpy(&pump_ma,  &pkt[12], 2);
      memcpy(&flow_cnt, &pkt[14], 2);
      memcpy(&temp_cC,  &pkt[16], 2);
      memcpy(&batt_mv,  &pkt[18], 2);
      memcpy(&ref_adc,  &pkt[22], 2);
      uint8_t flags = pkt[20];

      float p_act_w = (panel_mv/1000.0f) * (panel_ma/1000.0f);
      float tempC = temp_cC / 100.0f;
      float p_exp_w = (ref_adc > REF_ADC_MIN_VALID) ?
        expected_power_from_ref(ref_adc, tempC) :
        expected_power_from_voltage(panel_mv, tempC);
      float si = 0.0f, pr = 0.0f;
      if (p_exp_w > 5.0f) { si = max(0.0f, (p_exp_w - p_act_w) / p_exp_w); pr = p_act_w / p_exp_w; }

      float health = 1.0f;
      health -= min(0.5f, si);
      bool pump_on = pump_ma > PUMP_CURRENT_MIN_MA;
      bool flow_low = (flow_cnt < (uint16_t)(PUMP_FLOW_MIN_PPS * 5)); // 5s window
      if (pump_on && flow_low) health -= 0.4f;
      if (batt_mv < BATTERY_LOW_MV) health -= 0.2f;
      health = max(0.0f, health);

      if (p_exp_w > 50.0f) { siTrend.add(ts, si); }

      static uint32_t lastSmsTs = 0;
      uint32_t now_ms = millis();
      if (si > SOILING_ALERT && (now_ms - lastSmsTs) > 600000) {
        sendSMSAlert(String("ALERT: Soiling ") + String(si*100,1) + "% (ref-based)");
        lastSmsTs = now_ms;
      }
      double slope_day = siTrend.slope_per_day();
      if (slope_day > 1e-4) {
        double remain = SOILING_ALERT - si;
        if (remain > 0) {
          double pred_days = remain / slope_day;
          if (pred_days >= 0 && pred_days <= SOILING_PREDICT_DAYS && (now_ms - lastSmsTs) > 600000) {
            sendSMSAlert(String("NOTICE: Cleaning due in ~") + String(pred_days,1) + " days (soiling trend)");
            lastSmsTs = now_ms;
          }
        }
      }

      Point p{ts, panel_mv, panel_ma, pump_ma, flow_cnt, batt_mv, temp_cC,
              ref_adc, flags, si, health, pr, p_act_w, p_exp_w};
      enqueue(p);
      parsed = true;
    }

    // Fallback v1
    if (!parsed && n >= 24 && crc8(pkt, 23) == pkt[23] && pkt[0] == 0x01) {
      uint32_t ts; memcpy(&ts, &pkt[4], 4);
      uint16_t panel_mv, panel_ma, pump_ma, flow_cnt, batt_mv;
      int16_t temp_cC;
      memcpy(&panel_mv, &pkt[8], 2);
      memcpy(&panel_ma, &pkt[10], 2);
      memcpy(&pump_ma,  &pkt[12], 2);
      memcpy(&flow_cnt, &pkt[14], 2);
      memcpy(&temp_cC,  &pkt[16], 2);
      memcpy(&batt_mv,  &pkt[18], 2);
      uint8_t flags = pkt[20];

      float tempC = temp_cC / 100.0f;
      float p_act_w = (panel_mv/1000.0f) * (panel_ma/1000.0f);
      float p_exp_w = expected_power_from_voltage(panel_mv, tempC);
      float si = 0.0f, pr = 0.0f;
      if (p_exp_w > 5.0f) { si = max(0.0f, (p_exp_w - p_act_w) / p_exp_w); pr = p_act_w / p_exp_w; }

      float health = 1.0f;
      health -= min(0.5f, si);
      bool pump_on = pump_ma > PUMP_CURRENT_MIN_MA;
      bool flow_low = (flow_cnt < (uint16_t)(PUMP_FLOW_MIN_PPS * 5));
      if (pump_on && flow_low) health -= 0.4f;
      if (batt_mv < BATTERY_LOW_MV) health -= 0.2f;
      health = max(0.0f, health);

      if (p_exp_w > 50.0f) siTrend.add(ts, si);

      static uint32_t lastSmsTs = 0;
      uint32_t now_ms = millis();
      if (si > SOILING_ALERT && (now_ms - lastSmsTs) > 600000) {
        sendSMSAlert(String("ALERT: Soiling ") + String(si*100,1) + "%");
        lastSmsTs = now_ms;
      }

      Point p{ts, panel_mv, panel_ma, pump_ma, flow_cnt, batt_mv, temp_cC,
              0, flags, si, health, pr, p_act_w, p_exp_w};
      enqueue(p);
    }
  }
}
