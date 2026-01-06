#include <Arduino.h>      // Core Arduino library, provides basic functions like pinMode(), digitalWrite(), delay(), etc.
#include <WiFi.h>         // Library to connect ESP32 to WiFi networks
#include <PubSubClient.h> // MQTT client library to connect to brokers and publish/subscribe messages
#include <NimBLEDevice.h> // BLE library for ESP32 (low memory usage)
#include <Wire.h>         // I2C library to communicate with sensors like AHT20 and BH1750

/* =========================
   WIFI CONFIG
========================= */
// These are your WiFi credentials. Replace with your network's SSID and password.
const char* WIFI_SSID = "PLDTinnov";
const char* WIFI_PASS = "Password12345!";

/* =========================
   MQTT CONFIG
========================= */
// MQTT broker settings: host, port, username, password, and the base topic for publishing.
const char* MQTT_HOST = "innovph.com";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "mqtt";
const char* MQTT_PASS = "ICPHmqtt!";
const char* MQTT_BASE = "BLE/act5/esp01";

/* =========================
   BLE CONFIG
========================= */
// Name of the BLE device that will appear when scanning
#define BLE_DEVICE_NAME "ESP32_SERIAL_BLE"
// UUIDs uniquely identify the BLE service and characteristic
#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"
#define CHARACTERISTIC_UUID "cba1d466-344c-4be3-ab3f-189f80dd7518"

/* =========================
   SENSOR CONFIG
========================= */
// I2C addresses for sensors
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C
#define MQ136_PIN 32    // Analog pin for gas sensor

/* =========================
   THRESHOLDS (initial)
========================= */
// These thresholds determine when the alarm state is triggered
float TEMP_THRESHOLD = 32.0;    // °C
float HUM_THRESHOLD = 70.0;     // %
float LUX_THRESHOLD = 800.0;    // Lux
float GAS_THRESHOLD_PPM = 1000.0; // PPM

/* =========================
   GLOBAL OBJECTS
========================= */
// Create global objects for WiFi/MQTT and BLE
WiFiClient espClient;               // WiFi client for MQTT
PubSubClient mqttClient(espClient); // MQTT client using WiFi

NimBLEServer* pServer = nullptr;       // BLE server pointer
NimBLECharacteristic* pCharacteristic = nullptr; // BLE characteristic pointer

/* =========================
   SENSOR VARIABLES
========================= */
// Variables to store sensor readings
float temperature = 0;
float humidity = 0;
float lux = 0;
float gasPPM = 0;
bool alarmState = false;     // True if any sensor crosses threshold

unsigned long lastPublish = 0; // Timestamp to control publishing interval

/* =========================
   WIFI / MQTT FUNCTIONS
========================= */
// Connect to WiFi
void connectWiFi() {
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// Callback function for receiving MQTT messages
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i]; // Convert bytes to string

  Serial.printf("MQTT Received -> %s : %s\n", topic, msg.c_str());

  // Update thresholds dynamically if a message is received
  if (String(topic).endsWith("threshold/temperature")) TEMP_THRESHOLD = msg.toFloat();
  if (String(topic).endsWith("threshold/humidity")) HUM_THRESHOLD = msg.toFloat();
  if (String(topic).endsWith("threshold/light")) LUX_THRESHOLD = msg.toFloat();
  if (String(topic).endsWith("threshold/gas")) GAS_THRESHOLD_PPM = msg.toFloat();
}

// Connect to MQTT broker
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqttClient.connect("ESP32_BLE_MQTT", MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");

      // Subscribe to threshold topics so the ESP can receive updates
      mqttClient.setCallback(mqttCallback);
      mqttClient.subscribe("BLE/act5/esp01/threshold/temperature");
      mqttClient.subscribe("BLE/act5/esp01/threshold/humidity");
      mqttClient.subscribe("BLE/act5/esp01/threshold/light");
      mqttClient.subscribe("BLE/act5/esp01/threshold/gas");

    } else {
      Serial.print("failed rc=");
      Serial.println(mqttClient.state());
      delay(2000); // Wait before retrying
    }
  }
}

/* =========================
   SENSOR FUNCTIONS
========================= */
// Read temperature and humidity from AHT20
bool readAHT20(float* t, float* h) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC); // Command to start measurement
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(80); // Wait for measurement

  Wire.requestFrom(AHT20_ADDR, 6); // Read 6 bytes
  if (Wire.available() != 6) return false;

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  // Convert raw data to human-readable values
  uint32_t hum_raw = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | ((uint32_t)(d[3] >> 4));
  uint32_t temp_raw = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

  *h = ((float)hum_raw / 1048576.0) * 100.0;      // % humidity
  *t = ((float)temp_raw / 1048576.0) * 200.0 - 50.0; // °C temperature
  return true;
}

// Read light intensity from BH1750
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // Continuous high-res mode
  Wire.endTransmission();
  delay(180);

  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8 | Wire.read();
    return raw / 1.2; // Convert to lux
  }
  return -1;
}

// Read gas concentration from MQ136 (analog sensor)
float readGasPPM() {
  int raw = analogRead(MQ136_PIN); // Read ADC value (0-4095)
  return ((float)raw / 4095.0) * 2000.0; // Convert to PPM
}

// Read all sensors and update alarm state
void readSensors() {
  readAHT20(&temperature, &humidity);
  lux = readBH1750();
  gasPPM = readGasPPM();

  // Alarm if any value exceeds threshold
  alarmState =
    (temperature > TEMP_THRESHOLD) || (humidity > HUM_THRESHOLD) || (lux > LUX_THRESHOLD) || (gasPPM > GAS_THRESHOLD_PPM);

  Serial.printf("T=%.2f H=%.2f L=%.1f G=%.1f ALARM=%d\n",
                temperature, humidity, lux, gasPPM, alarmState);
}

/* =========================
   MQTT PUBLISH
========================= */
// Send sensor values and alarm status to MQTT broker
void publishMQTT() {
  char topic[64], payload[16];

  // Temperature
  snprintf(topic, sizeof(topic), "%s/temperature", MQTT_BASE);
  snprintf(payload, sizeof(payload), "%.2f", temperature);
  mqttClient.publish(topic, payload);

  // Humidity
  snprintf(topic, sizeof(topic), "%s/humidity", MQTT_BASE);
  snprintf(payload, sizeof(payload), "%.2f", humidity);
  mqttClient.publish(topic, payload);

  // Light
  snprintf(topic, sizeof(topic), "%s/lux", MQTT_BASE);
  snprintf(payload, sizeof(payload), "%.1f", lux);
  mqttClient.publish(topic, payload);

  // Gas
  snprintf(topic, sizeof(topic), "%s/gas", MQTT_BASE);
  snprintf(payload, sizeof(payload), "%.1f", gasPPM);
  mqttClient.publish(topic, payload);

  // Alarm (1=on,0=off)
  snprintf(topic, sizeof(topic), "%s/alarm", MQTT_BASE);
  mqttClient.publish(topic, alarmState ? "1" : "0");
}

/* =========================
   BLE CALLBACKS
========================= */
// Handle BLE client connections and disconnections
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    Serial.println("BLE client connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    Serial.println("BLE client disconnected");

    // Restart advertising so clients can reconnect
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->stop();
    adv->start();
    Serial.println("BLE advertising restarted");
  }
};

/* =========================
   BLE INIT
========================= */
// Initialize BLE server, service, and characteristic
void initBLE() {
  NimBLEDevice::init(BLE_DEVICE_NAME);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  pCharacteristic->setValue("00000000");  // initial value
  pService->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData data;
  data.setName(BLE_DEVICE_NAME);
  data.addServiceUUID(SERVICE_UUID);
  adv->setAdvertisementData(data);
  adv->start();

  Serial.println("BLE advertising started");
}

/* =========================
   BLE NOTIFY HEX
========================= */
// Send sensor readings over BLE in binary format
void notifyBLE() {
  if (pServer->getConnectedCount() == 0) return; // Only notify if a client is connected

  uint16_t t = (uint16_t)(temperature * 100); // Convert to integer for sending
  uint16_t h = (uint16_t)(humidity * 100);
  uint16_t l = (uint16_t)(lux);
  uint16_t g = (uint16_t)(gasPPM);
  uint8_t a = alarmState ? 1 : 0;

  uint8_t buf[9];
  buf[0] = t >> 8; buf[1] = t & 0xFF;
  buf[2] = h >> 8; buf[3] = h & 0xFF;
  buf[4] = l >> 8; buf[5] = l & 0xFF;
  buf[6] = g >> 8; buf[7] = g & 0xFF;
  buf[8] = a;

  pCharacteristic->setValue(buf, sizeof(buf));
  pCharacteristic->notify();

  Serial.printf("BLE HEX Notify -> T=0x%04X H=0x%04X L=0x%04X G=0x%04X A=0x%02X\n",
                t, h, l, g, a);
}

/* =========================
   SETUP
========================= */
void setup() {
  Serial.begin(115200);       // Start serial monitor
  Wire.begin(21, 22);         // Initialize I2C pins (SDA=21, SCL=22)
  analogReadResolution(12);   // Set ADC resolution (0-4095)
  pinMode(MQ136_PIN, INPUT);  // Gas sensor as input

  connectWiFi();               // Connect to WiFi
  mqttClient.setServer(MQTT_HOST, MQTT_PORT); // Setup MQTT server

  initBLE();                   // Initialize BLE server
}

/* =========================
   LOOP
========================= */
void loop() {
  // Ensure MQTT connection
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop(); // Keep MQTT client alive

  // Read sensors and publish every 3s
  if (millis() - lastPublish > 3000) {
    lastPublish = millis();
    readSensors();
    publishMQTT();
    notifyBLE();
  }

  // ----------------------------
  // BLE Reinitialization Logic
  // ----------------------------
  static unsigned long lastBLECheck = 0;
  if (millis() - lastBLECheck > 2000) { // check every 2s
    lastBLECheck = millis();

    // If server exists but no advertising, restart it
    if (pServer) {
      NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
      if (!adv->isAdvertising()) {
        Serial.println("BLE advertising stopped unexpectedly. Restarting...");
        adv->start();
      }
    } 
    // If server is null, reinitialize BLE completely
    else {
      Serial.println("BLE server lost. Reinitializing...");
      initBLE();
    }
  }
}

/* =========================
   NOTES TO CONVERT HEX BACK
========================
Each value is 2 bytes (MSB first) except alarm:
t  = (buf[0]<<8 | buf[1]) / 100.0  -> temperature °C
h  = (buf[2]<<8 | buf[3]) / 100.0  -> humidity %
l  = (buf[4]<<8 | buf[5])           -> lux
g  = (buf[6]<<8 | buf[7])           -> gas PPM
a  = buf[8]                         -> alarm (0=off,1=on)
*/
