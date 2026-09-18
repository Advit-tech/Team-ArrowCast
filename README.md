# SYNORA — 3-Node Smart IoT Irrigation System

An automated, safety-critical 3-node IoT irrigation network built for the **SYNORA Hackathon (IoT Track)**. The system continuously monitors soil telemetry, evaluates tank capacity, and manages multi-zone pump relays with hardware safety cutoffs.

---

## 🛠 Repository Structure

```text
esp32-irrigation-system/
├── README.md
├── docs/
│   └── architecture.md       # Technical design & safety documentation
└── firmware/
    ├── admin_node/          # Dashboard server & decision engine (ESP32-S3)
    ├── sensor_node/         # Telemetry publishing node (ESP32 / ESP8266)
    └── tank_node/           # Multi-relay & ultrasonic level node (ESP32)
