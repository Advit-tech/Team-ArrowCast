#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Network Configuration ---
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER_IP  "192.168.1.100"
#define MQTT_PORT       1883

// --- Tank Hardware Calibration ---
#define TRIG_PIN        5     // Ultrasonic HC-SR04 Trigger Pin
#define ECHO_PIN        18    // Ultrasonic HC-SR04 Echo Pin
#define TANK_DEPTH_CM   100.0 // Distance from sensor to bottom of empty tank (cm)
#define TANK_FULL_CM    10.0  // Distance from sensor to water when 100% full (cm)
#define LOW_LEVEL_THRESHOLD_PCT 20.0 // Cutoff percentage (Safety mechanism)

// --- Safety & Timing Parameters ---
#define MAX_RUNTIME_SECONDS     600    // 10 minutes max continuous relay operation
#define DEBOUNCE_INTERVAL_MS    2000   // Minimum 2s between switching state changes
#define LEVEL_CHECK_INTERVAL_MS 30000  // Read water level every 30 seconds

// --- Multi-Zone Relay Pin Mapping ---
struct ZoneRelayMap {
  const char* zone_id;
  uint8_t pin;
  bool activeState;        // HIGH or LOW activation
  bool currentState;       // Current ON/OFF tracking state
  unsigned long turnOnTime;// Timestamp when turned ON (Safety timer)
  unsigned long lastCmdTime;// Timestamp of last command received (Debounce)
};

// Map each zone ID to dedicated relay pins on this Tank Node
extern ZoneRelayMap zoneMappings[];
extern const size_t NUM_ZONES;

#endif // CONFIG_H
