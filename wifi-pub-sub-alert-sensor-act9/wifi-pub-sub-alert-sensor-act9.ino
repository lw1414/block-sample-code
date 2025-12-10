#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ---------------------------
// HARD-CODED WIFI CONFIG
// ---------------------------
const char* ssid     = "PLDTinnov";
const char* password = "Password12345!";

// ---------------------------
// MQTT CONFIG
// ---------------------------
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;

const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

// ---------------------------
// Sensor Addresses
// ---------------------------
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ---------------------------
// Hardware Pins
// ---------------------------
#define LED_PIN     14
#define NUM_LEDS    10
#define BUZZER_PIN  13

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------------------
// WiFi + MQTT
// ---------------------------
WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastMQTTReconnect = 0;
unsigned long lastLedUpdate = 0;
unsigned long lastSensorPublish = 0;
unsigned long lastBuzzerToggle = 0;
int spinningIndex = 0;
bool mqttPreviouslyConnected = false;

const unsigned long SENSOR_INTERVAL = 5000; // 5 seconds between sensor publishes
const unsigned long BUZZER_INTERVAL = 500;  // buzzer on/off interval

// ---------------------------
// Thresholds for buzzer alarm
// ---------------------------
float TEMP_THRESHOLD = 30.0;   // °C
float HUM_THRESHOLD  = 70.0;   // %
float LIGHT_THRESHOLD = 800.0; // lux

bool buzzerActive = false;

// Buzzer frequency for passive buzzer
const int BUZZER_FREQ = 1000; // 1 kHz

// LED Colors
uint32_t colorWiFi    = strip.Color(0, 0, 60);   // Blue
uint32_t colorMQTT    = strip.Color(50, 30, 0);  // Yellow
uint32_t colorWiFiOK  = strip.Color(0, 50, 50);  // Cyan
uint32_t colorReady   = strip.Color(0, 60, 0);   // Green
uint32_t colorError   = strip.Color(60, 0, 0);   // Red

// ---------------------------
// Helper: Fill all LEDs
// ---------------------------
void setAll(uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// ---------------------------
// MQTT Callback
// ---------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

// ---------------------------
// MQTT Connect
// ---------------------------
void connectMQTT() {
  if (mqtt.connected()) return;

  Serial.println("Connecting MQTT...");
  if (mqtt.connect("ESP32Client", mqtt_user, mqtt_pass)) {
    Serial.println("MQTT Connected!");
    mqtt.subscribe("WiFi/#");
  } else {
    Serial.print("MQTT Failed rc=");
    Serial.println(mqtt.state());
    setAll(colorError);  // show error immediately
  }
}

// ---------------------------
// Spinning LED effect (millis-based)
// ---------------------------
void spinningRing(uint32_t color, int speed = 100) {
  unsigned long now = millis();
  if (now - lastLedUpdate > speed) {
    lastLedUpdate = now;
    strip.clear();
    strip.setPixelColor(spinningIndex, color);
    strip.show();
    spinningIndex = (spinningIndex + 1) % NUM_LEDS;
  }
}

// ---------------------------
// Sensor Reads
// ---------------------------
float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // Continuous H-Resolution mode
  Wire.endTransmission();
  delay(180); // small unavoidable wait
  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = Wire.read() << 8 | Wire.read();
    return raw / 1.2;
  }
  return -1;
}

bool readAHT20(float *tempC, float *humidity) {
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(80);

  Wire.requestFrom(AHT20_ADDR, 6);
  if (Wire.available() != 6) return false;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  uint32_t hum_raw =
      ((uint32_t)data[1] << 12) |
      ((uint32_t)data[2] << 4) |
      ((uint32_t)(data[3] >> 4));

  uint32_t temp_raw =
      (((uint32_t)data[3] & 0x0F) << 16) |
      ((uint32_t)data[4] << 8) |
      ((uint32_t)data[5]);

  *humidity = ((float)hum_raw / 1048576.0) * 100.0;
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

// ---------------------------
// Setup
// ---------------------------
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22); // I2C SDA, SCL

  // Init AHT20
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  Serial.println("Sensors Initialized");

  // Connect WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
}

// ---------------------------
// Loop
// ---------------------------
void loop() {
  unsigned long now = millis();

  // Handle WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    spinningRing(colorWiFi, 150);
    return; // skip MQTT until WiFi connected
  }

  // WiFi connected
  if (!mqttPreviouslyConnected) {
    setAll(colorWiFiOK);
  }

  // Setup MQTT server if not already
  if (!mqtt.connected()) {
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(mqttCallback);

    if (now - lastMQTTReconnect > 2000) {
      lastMQTTReconnect = now;
      connectMQTT();
    }

    spinningRing(colorMQTT, 100);
    mqttPreviouslyConnected = false;
  } else {
    mqtt.loop();

    if (!mqttPreviouslyConnected) {
      setAll(colorReady); // solid green
      mqttPreviouslyConnected = true;
      Serial.println("MQTT Connected and LEDs updated!");
    }

    // --- Sensor read + MQTT publish every SENSOR_INTERVAL ---
    if (now - lastSensorPublish > SENSOR_INTERVAL) {
      lastSensorPublish = now;

      float temp, hum;
      if (readAHT20(&temp, &hum)) {
        char buf[10];

        dtostrf(temp, 4, 1, buf);
        mqtt.publish("WiFi/act9/id002/temperature", buf);

        dtostrf(hum, 4, 1, buf);
        mqtt.publish("WiFi/act9/id002/humidity", buf);

        Serial.printf("Temp: %.1f °C, Hum: %.1f %%\n", temp, hum);
      }

      float lux = readBH1750();
      if (lux >= 0) {
        char buf[10];
        dtostrf(lux, 5, 1, buf);
        mqtt.publish("WiFi/act9/id002/light", buf);
        Serial.printf("Light: %.1f lux\n", lux);
      }

      // --- Check thresholds for buzzer ---
      buzzerActive = false;
      if (temp >= TEMP_THRESHOLD || hum >= HUM_THRESHOLD || lux >= LIGHT_THRESHOLD) {
        buzzerActive = true;
      }
    }
  }

  // --- Passive buzzer alarm using tone() (millis-based) ---
  if (buzzerActive) {
    if (now - lastBuzzerToggle > BUZZER_INTERVAL) {
      lastBuzzerToggle = now;
      static bool buzzerOn = false;
      buzzerOn = !buzzerOn;
      if (buzzerOn) {
        tone(BUZZER_PIN, BUZZER_FREQ);
      } else {
        noTone(BUZZER_PIN);
      }
    }
  } else {
    noTone(BUZZER_PIN);
  }
}
