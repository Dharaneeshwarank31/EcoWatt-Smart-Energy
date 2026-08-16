/*
 * EcoWatt AI Dashboard — ESP8266 Sensor Node
 * Libraries needed:
 *   - EmonLib  (Tools > Manage Libraries > search "EmonLib")
 *   - PubSubClient (for MQTT)
 *   - ArduinoJson
 *
 * Wiring: CT Sensor → A0 (via burden resistor)
 * Upload at: 115200 baud
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EmonLib.h>
#include <ArduinoJson.h>

// ── CONFIG ──
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER = "192.168.1.100"; // Your broker IP
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "ecowatt/sensor/power";

// ── PINS ──
#define CT_PIN A0
#define LED_PIN 2

EnergyMonitor emon;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // CT Sensor calibration (111.1 for 100A/50mA CT)
  emon.current(CT_PIN, 111.1);

  // WiFi connect
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  digitalWrite(LED_PIN, LOW);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT connecting...");
    if (mqtt.connect("ecowatt-sensor")) {
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc="); Serial.println(mqtt.state());
      delay(3000);
    }
  }
}

void loop() {
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();

  // Read current (1480 samples)
  double Irms  = emon.calcIrms(1480);
  double volt  = 230.0;  // Assumed nominal voltage
  double power = Irms * volt;
  double kwh   = power / 1000.0;
  double pf    = 0.95;   // Assumed power factor

  // Build JSON payload
  StaticJsonDocument<128> doc;
  doc["kwh"]     = round(kwh * 1000.0) / 1000.0;
  doc["voltage"] = volt;
  doc["current"] = round(Irms * 100.0) / 100.0;
  doc["pf"]      = pf;

  char payload[128];
  serializeJson(doc, payload);

  // Publish to MQTT
  mqtt.publish(MQTT_TOPIC, payload);

  // Also send to Serial (for USB Serial mode in dashboard)
  Serial.println(payload);

  digitalWrite(LED_PIN, HIGH); delay(50);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
}
