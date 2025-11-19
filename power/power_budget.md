# Power Budget (Typical)

Node (ESP32 + INA219 + DS18B20 + LoRa):
- Active (measure + TX ~5 s): ~130 mA peak, ~0.18 mAh per cycle
- Deep sleep (175 s @ 150 µA): ~0.007 mAh per cycle
- 3-min interval → ~3.8 mAh/h → ~91 mAh/day

Gateway (ESP32 + LoRa RX + WiFi AP + SIM800L idle):
- ~150 mA average; SMS bursts up to 2A for <2 s
- Use a robust 4V rail with large capacitors for SIM800L

Optimizations:
- Reduce AP TX power, duty-cycle WiFi
- Longer intervals at night
- Lower SF when link margin is good
