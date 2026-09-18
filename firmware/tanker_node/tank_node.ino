/**
 * SYNORA Hackathon - IoT Track
 * Tank Node Firmware (ESP32)
 *
 * Controls multi-zone pump relays, monitors tank water levels, enforces safety timeouts,
 * and handles debouncing to protect physical hardware.
 * Required Libraries: PubSubClient, ArduinoJson (v6/v7)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Define Zone-to-Pin hardware mapping
ZoneRelayMap zoneMappings[] = {
  {"z1", 25, HIGH, false, 0, 0}, // Zone 1 -> GPIO 25
  {"z2", 26, HIGH, false, 0, 0}  // Zone 2 -> GPIO 26
};
const size_t NUM_ZONES = sizeof(zoneMappings) / sizeof(zoneMappings[0]);

// Network Objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Operational States
bool isWaterLevelLow = false;
unsigned long lastLevelCheckTime = 0;

void setupWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void publishRelayStatus(const char* zone_id, bool isValveOn) {
  char topic[64];
  snprintf(topic, sizeof(topic), "farm/tank/%s/relay/status", zone_id);

  StaticJsonDocument<128> doc;
  doc["zone"] = zone_id;
  doc["valve"] = isValveOn;
  doc["ts"] = millis() / 1000;

  char jsonBuffer[128];
  serializeJson(doc, jsonBuffer);
  mqttClient.publish(topic, jsonBuffer);
  Serial.printf("Status confirmation published [%s]: %s\n", topic, jsonBuffer);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.f_str());
    return;
  }

  const char* zone = doc["zone"];
  bool targetState = doc["valve"];
  unsigned long now = millis();

  for (size_t i = 0; i < NUM_ZONES; i++) {
    if (strcmp(zoneMappings[i].zone_id, zone) == 0) {
      
      // --- SAFETY MECHANISM 1: Command Debouncing ---
      // Prevents rapid relay toggling (relay chatter) from duplicate/noisy MQTT messages
      if (now - zoneMappings[i].lastCmdTime < DEBOUNCE_INTERVAL_MS) {
        Serial.printf("[SAFETY] Command ignored for zone %s: Debounce limit (2s active)\n", zone);
        return;
      }
      zoneMappings[i].lastCmdTime = now;

      // --- SAFETY MECHANISM 2: Dry-Run / Low-Water Cutoff ---
      // Refuses to open valves/turn on pumps if tank water is empty
      if (targetState && isWaterLevelLow) {
        Serial.printf("[SAFETY BLOCK] Cannot open valve for %s: Tank water level is LOW!\n", zone);
        publishRelayStatus(zone, false);
        return;
      }

      // Execute Pin Switching
      zoneMappings[i].currentState = targetState;
      digitalWrite(zoneMappings[i].pin, targetState ? zoneMappings[i].activeState : !zoneMappings[i].activeState);
      
      if (targetState) {
        zoneMappings[i].turnOnTime = now; // Start continuous runtime watchdog timer
      }

      Serial.printf("Relay for Zone %s set to: %s\n", zone, targetState ? "ON" : "OFF");
      publishRelayStatus(zone, targetState);
      return;
    }
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting Tank Node to MQTT...");
    if (mqttClient.connect("TankNodeAdmin")) {
      Serial.println(" Connected!");
      
      // Dynamically subscribe to commands for all mapped zones
      for (size_t i = 0; i < NUM_ZONES; i++) {
        char cmdTopic[64];
        snprintf(cmdTopic, sizeof(cmdTopic), "farm/tank/%s/relay/cmd", zoneMappings[i].zone_id);
        mqttClient.subscribe(cmdTopic);
        Serial.printf("Subscribed to: %s\n", cmdTopic);
      }
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 3 seconds...");
      delay(3000);
    }
  }
}

float measureTankLevelPct() {
  // Ultrasonic pulse measurement (HC-SR04)
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1.0; // Return invalid on sensor timeout

  float distanceCm = (duration * 0.0343) / 2.0;

  // Calculate percentage based on depth configuration
  float pct = ((TANK_DEPTH_CM - distanceCm) / (TANK_DEPTH_CM - TANK_FULL_CM)) * 100.0;
  return constrain(pct, 0.0, 100.0);
  
  /* NOTE: To switch to a basic float switch instead:
   * 1. Set pin mode: pinMode(FLOAT_SWITCH_PIN, INPUT_PULLUP);
   * 2. Return digitalRead(FLOAT_SWITCH_PIN) == LOW ? 100.0 : 0.0;
   */
}

void checkAndPublishTankLevel() {
  float currentLevel = measureTankLevelPct();

  if (currentLevel < 0.0) {
    Serial.println("Warning: Ultrasonic sensor timeout!");
    return;
  }

  // Update dry-run protection status state
  isWaterLevelLow = (currentLevel < LOW_LEVEL_THRESHOLD_PCT);

  // If low water condition detected, force shut down all active relays immediately
  if (isWaterLevelLow) {
    for (size_t i = 0; i < NUM_ZONES; i++) {
      if (zoneMappings[i].currentState) {
        zoneMappings[i].currentState = false;
        digitalWrite(zoneMappings[i].pin, !zoneMappings[i].activeState);
        Serial.printf("[SAFETY AUTO-CUTOFF] Tank level critical! Forced relay OFF for zone %s\n", zoneMappings[i].zone_id);
        publishRelayStatus(zoneMappings[i].zone_id, false);
      }
    }
  }

  // Publish payload to farm/tank/level
  StaticJsonDocument<128> doc;
  doc["level_pct"] = (int)currentLevel;
  doc["low"] = isWaterLevelLow;
  doc["ts"] = millis() / 1000;

  char buffer[128];
  serializeJson(doc, buffer);
  mqttClient.publish("farm/tank/level", buffer);
  Serial.printf("Published Tank Level: %s\n", buffer);
}

void enforceMaxRuntimeWatchdog() {
  // --- SAFETY MECHANISM 3: Maximum Continuous Runtime Watchdog ---
  // Turns off relays that stay active longer than MAX_RUNTIME_SECONDS to avoid flooding/overheating.
  unsigned long now = millis();
  for (size_t i = 0; i < NUM_ZONES; i++) {
    if (zoneMappings[i].currentState) {
      if ((now - zoneMappings[i].turnOnTime) >= ((unsigned long)MAX_RUNTIME_SECONDS * 1000)) {
        zoneMappings[i].currentState = false;
        digitalWrite(zoneMappings[i].pin, !zoneMappings[i].activeState);

        Serial.printf("[SAFETY WATCHDOG] Relay %s exceeded max runtime (%d sec). Automatic SHUTOFF triggered!\n", 
                      zoneMappings[i].zone_id, MAX_RUNTIME_SECONDS);

        publishRelayStatus(zoneMappings[i].zone_id, false);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Setup Relay Pins
  for (size_t i = 0; i < NUM_ZONES; i++) {
    pinMode(zoneMappings[i].pin, OUTPUT);
    digitalWrite(zoneMappings[i].pin, !zoneMappings[i].activeState); // Ensure initial OFF state
  }

  // Setup Ultrasonic Sensor Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  mqttClient.loop();

  // Watchdog monitoring active relays
  enforceMaxRuntimeWatchdog();

  // Periodic level publishing
  unsigned long currentMillis = millis();
  if (currentMillis - lastLevelCheckTime >= LEVEL_CHECK_INTERVAL_MS) {
    lastLevelCheckTime = currentMillis;
    checkAndPublishTankLevel();
  }
}
