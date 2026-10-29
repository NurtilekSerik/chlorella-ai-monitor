/*
  Chlorella AI Monitor — ESP32 sender
  Hardware example:
    ESP32 + SCD40/SCD41 (CO2, temperature, humidity) over I2C
  Libraries (Arduino Library Manager):
    - Sensirion I2C SCD4x
    - ArduinoJson
  IMPORTANT:
    - Replace SUPABASE_URL and SUPABASE_ANON_KEY.
    - Use the anon/public key only, never service_role.
    - This example sends one row every 10 seconds.
    - reactor_on is a manual experiment label, not a physical relay command.
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SensirionI2CScd4x.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SUPABASE_URL = "https://YOUR_PROJECT.supabase.co";
const char* SUPABASE_ANON_KEY = "YOUR_SUPABASE_ANON_KEY";

SensirionI2CScd4x scd4x;
const int REACTOR_SWITCH_PIN = 27; // optional physical switch to GND; INPUT_PULLUP
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL_MS = 10000;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(REACTOR_SWITCH_PIN, INPUT_PULLUP);
  connectWiFi();
  scd4x.begin(Wire);
  uint16_t error = scd4x.stopPeriodicMeasurement();
  delay(500);
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("SCD4x start error: "); Serial.println(error);
  } else {
    Serial.println("SCD4x periodic measurement started");
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (millis() - lastSend < SEND_INTERVAL_MS) { delay(100); return; }
  lastSend = millis();

  uint16_t co2 = 0;
  float temperature = 0, humidity = 0;
  uint16_t error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error || co2 == 0) {
    Serial.println("Sensor not ready / read error; no data sent");
    return;
  }

  // Optional physical switch: switch connected between GPIO27 and GND.
  // If you don't have this switch, replace with a fixed experiment state.
  bool reactorOn = (digitalRead(REACTOR_SWITCH_PIN) == LOW);

  StaticJsonDocument<256> doc;
  doc["co2"] = co2;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["reactor_on"] = reactorOn;
  String body;
  serializeJson(doc, body);

  HTTPClient http;
  String endpoint = String(SUPABASE_URL) + "/rest/v1/readings";
  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Prefer", "return=minimal");
  int code = http.POST(body);
  Serial.printf("POST result: %d; payload: %s\n", code, body.c_str());
  if (code < 200 || code >= 300) Serial.println(http.getString());
  http.end();
}
