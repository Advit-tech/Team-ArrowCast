#ifndef CONFIG_H
#define CONFIG_H

// --- Network Configuration ---
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER_IP  "192.168.1.100" // IP address of Admin Node / Broker
#define MQTT_PORT       1883

// --- Node Identification ---
#define DEVICE_ZONE_ID  "z1"

// --- Hardware Pin Definitions (Standard ESP32) ---
#define SOIL_PIN        34    // ADC pin for capacitive moisture sensor
#define DHT_PIN         4     // Digital pin for DHT22 sensor
#define DHT_TYPE        DHT22 
#define STATUS_LED_PIN  2     // Built-in LED on standard ESP32 (on ESP32-S3 change to 13 or RGB)

// --- Intervals ---
#define PUBLISH_INTERVAL_MS 10000 // 10 seconds between data publishes

#endif // CONFIG_H
