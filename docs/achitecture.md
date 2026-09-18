# SYNORA IoT Irrigation System — Technical Architecture

## 1. System Topology

The system uses a 3-node decentralized edge architecture communicating over MQTT via a local Wi-Fi broker.
## 2. Node Breakdown & Responsibilities

### A. Admin Node (`/firmware/admin_node/`)
- **Hardware:** ESP32-S3
- **Function:** Serves the frontend user dashboard via LittleFS and AsyncWebServer. Acts as the master decision engine analyzing telemetry to dispatch actuation commands.

### B. Sensor Node (`/firmware/sensor_node/`)
- **Hardware:** ESP32 (or ESP8266), Capacitive Soil Moisture Sensor, DHT22 Temperature & Humidity Sensor
- **Publish Interval:** 10 Seconds
- **State Machine:** Connection loop handles automatic Wi-Fi/MQTT reconnection with backoff. Built-in LED visually indicates publication success (blink) vs. disconnect states (solid ON).

### C. Tank Node (`/firmware/tank_node/`)
- **Hardware:** Standard ESP32, HC-SR04 Ultrasonic Distance Sensor, Multi-Channel Relay Module
- **Publish Interval:** 30 Seconds (Tank Fill Level)
- **Subscribed Topics:** `farm/tank/{zone_id}/relay/cmd`
- **Safety Engine:**
  1. **Maximum Continuous Runtime Watchdog:** Automatically turns off any relay running continuously for longer than `MAX_RUNTIME_SECONDS` (default: 10 mins).
  2. **Dry-Run / Low-Level Cutoff:** Blocks relay activation and auto-shuts off active relays if water level falls below `LOW_LEVEL_THRESHOLD_PCT` (default: 20%).
  3. **Command Debouncing:** Ignores duplicate or rapidly fired commands targeting the same zone within 2 seconds (`DEBOUNCE_INTERVAL_MS`).

---

## 3. MQTT Topic & Data Schema

| Topic Structure | Direction | Payload Example | Description |
| :--- | :--- | :--- | :--- |
| `farm/sensor/{zone_id}/data` | Sensor Node ➔ Admin | `{"zone":"z1","soil":34,"temp":28.5,"humidity":60,"ts":1234567890}` | Environmental & soil readings |
| `farm/tank/{zone_id}/relay/cmd` | Admin ➔ Tank Node | `{"zone":"z1","valve":true}` | Command to switch pump relay ON/OFF |
| `farm/tank/{zone_id}/relay/status` | Tank Node ➔ Admin | `{"zone":"z1","valve":true,"ts":1234567890}` | Hardware confirmation of state change |
| `farm/tank/level` | Tank Node ➔ Admin | `{"level_pct":62,"low":false,"ts":1234567890}` | Real-time water tank volume level |

---

## 4. Hardware Safety Mechanisms

- **Relay Chatter Prevention:** Software debouncing limits state transitions.
- **Over-Irrigation Mitigation:** Hard watchdog cuts power after continuous execution limit.
- **Pump Protection:** Physical dry-run protection prevents pump burn-out when water supply is low.
