/**
 * SYNORA Hackathon - IoT Track
 * Sensor Node Firmware (ESP32)
 *
 * Reads capacitive soil moisture + DHT22 (temp/humidity) and publishes JSON payload to MQTT.
 * Required Libraries: PubSubClient, ArduinoJson (v6/v7), DHT sensor library by Adafruit
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "config.h"

// Hardware and Network Objects
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// State Tracking
unsigned long lastPublishTime = 0;
char mqttTopic[64];

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Status LED: Solid ON while disconnected
  digitalWrite(STATUS_LED_PIN, HIGH);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempt++;
    if (attempt > 40) { // Safety reboot after 20s stuck connection on live demo
      Serial.println("\nWiFi timeout. Restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  // Retries connection with simple backoff to survive disconnects during demo
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(MQTT_BROKER_IP);
    
    String clientId = "SensorNode-";
    clientId += DEVICE_ZONE_ID;

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("... Connected!");
      // Turn off debug LED once fully operational
      digitalWrite(STATUS_LED_PIN, LOW);
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 3 seconds...");
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH); // ON during setup

  dht.begin();
  
  // Format topic string dynamically: farm/sensor/{zone_id}/data
  snprintf(mqttTopic, sizeof(mqttTopic), "farm/sensor/%s/data", DEVICE_ZONE_ID);

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  mqttClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastPublishTime >= PUBLISH_INTERVAL_MS) {
    lastPublishTime = currentMillis;

    // Read Sensors
    float rawMoisture = analogRead(SOIL_PIN);
    // Map raw ADC (0-4095) to estimated percentage (0-100%)
    int soilPercentage = map(rawMoisture, 4095, 1500, 0, 100); 
    soilPercentage = constrain(soilPercentage, 0, 100);

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
      Serial.println("Error reading DHT22 sensor!");
      return;
    }

    // Construct JSON Payload using ArduinoJson
    StaticJsonDocument<256> doc;
    doc["zone"] = DEVICE_ZONE_ID;
    doc["soil"] = soilPercentage;
    doc["temp"] = serialized(String(temp, 1));
    doc["humidity"] = serialized(String(humidity, 1));
    doc["ts"] = millis() / 1000; // System uptime in seconds

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    // Publish to MQTT Broker
    if (mqttClient.publish(mqttTopic, jsonBuffer)) {
      Serial.print("Published payload: ");
      Serial.println(jsonBuffer);

      // Status LED Blink Indicator for visual confirmation
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(100);
      digitalWrite(STATUS_LED_PIN, LOW);
    } else {
      Serial.println("Failed to publish MQTT message.");
    }
  }
}
