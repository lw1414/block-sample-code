/*****************************************************************************************
 *  FULLY COMMENTED & TEACHING-MODE CODE  
 *  WiFi + MQTT + Sensors + RGB Indicators + Alarm System  
 *
 *  This version is designed to TEACH you how every part works.
 *  You will see:
 *     ✔ Block comments explaining whole sections
 *     ✔ Inline comments explaining specific lines
 *     ✔ LED behavior that changes based on system state
 *     ✔ Buzzer alerts for WiFi, MQTT, and threshold alarms
 *
 *  EVERYTHING uses millis(), NO delay-based blocking (except short beep effects).
 *****************************************************************************************/

#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

/***************************************************************************************
 *                     SECTION 1 — HARD-CODED CONFIGURATION
 ***************************************************************************************/

// ---------------------------
// WiFi Credentials
// ---------------------------
const char* ssid     = "RiveraWIFI";
const char* password = "@Rivera20214";

// ---------------------------
// MQTT Server Login
// ---------------------------
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

// ---------------------------
// Device ID (For MQTT topics)
// ---------------------------
String deviceID = "id002";  // <-- You may change this anytime

// ---------------------------
// I2C Sensor Addresses
// ---------------------------
#define AHT20_ADDR 0x38
#define BH1750_ADDR 0x5C

// ---------------------------
// ESP32 Hardware Pins
// ---------------------------
#define LED_PIN     14           // RGB ring (10 LEDs)
#define NUM_LEDS    10
#define BUZZER_PIN  13           // Passive buzzer

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

/***************************************************************************************
 *              SECTION 2 — GLOBAL VARIABLES AND TIMERS (millis-based)
 ***************************************************************************************/

// WiFi/MQTT clients
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Timing variables for millis() control
unsigned long lastMQTTReconnect = 0;
unsigned long lastLedUpdate     = 0;
unsigned long lastSensorPublish = 0;
unsigned long lastBuzzerToggle  = 0;
unsigned long lastBlinkToggle   = 0;

// Animation index for spinning LED
int spinningIndex = 0;

// State trackers
bool mqttPreviouslyConnected = false;
bool wifiPreviouslyConnected = false;

// Sensor publish every 5 seconds
const unsigned long SENSOR_INTERVAL = 5000;

// Buzzer toggle interval when alarming
const unsigned long BUZZER_INTERVAL = 400;

// LED blink interval when alarming
const unsigned long BLINK_INTERVAL  = 300;

/***************************************************************************************
 *                          SECTION 3 — THRESHOLD SETTINGS
 ***************************************************************************************/
float TEMP_THRESHOLD  = 40.0;   // °C
float HUM_THRESHOLD   = 70.0;   // %
float LIGHT_THRESHOLD = 800.0;  // lux

bool buzzerActive = false;      // This becomes TRUE when alarm conditions met

// Passive buzzer tone frequency
const int BUZZER_FREQ = 1000;

// RGB Color presets
uint32_t colorWiFi   = strip.Color(0, 0, 60);    // Blue during WiFi connecting
uint32_t colorMQTT   = strip.Color(50, 30, 0);   // Amber during MQTT connecting
uint32_t colorWiFiOK = strip.Color(0, 50, 50);   // Cyan when WiFi connected
uint32_t colorReady  = strip.Color(0, 60, 0);    // Solid green (normal mode)
uint32_t colorError  = strip.Color(60, 0, 0);    // Red for error
uint32_t colorAlarm  = strip.Color(60, 20, 0);   // Orange alarm color

/***************************************************************************************
 *                SECTION 4 — HELPER FUNCTION: SET ALL LEDs TO ONE COLOR
 ***************************************************************************************/
void setAll(uint32_t color) {
  // This function sets ALL LEDs to a single color.
  // Very useful when indicating states: green = ready, red = error.
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color); // Inline comment: This sets one pixel at a time
  }
  strip.show(); // Update the LED ring to apply the new colors
}

/***************************************************************************************
 *        SECTION 5 — MQTT CALLBACK (RUNS WHEN A MESSAGE IS RECEIVED)
 ***************************************************************************************/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Whenever MQTT receives a message, this function is triggered.
  // Here we simply print the incoming topic + message to Serial Monitor.
  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);   // Inline: Print each character from the payload
  }
  Serial.println();
}

/***************************************************************************************
 *        SECTION 6 — TRY TO CONNECT TO MQTT SERVER (non-blocking)
 ***************************************************************************************/
void connectMQTT() {
  // This function attempts to connect to MQTT.
  // It does NOT block; it only tries once when called.

  if (mqtt.connected()) return; // If already connected, exit immediately

  Serial.println("Connecting MQTT...");

  if (mqtt.connect("ESP32Client", mqtt_user, mqtt_pass)) {
    // SUCCESS
    Serial.println("MQTT Connected!");

    mqtt.subscribe("WiFi/#"); // Subscribe to wildcard topic

    // Play a short single beep to indicate MQTT success
    tone(BUZZER_PIN, 1500, 150);

  } else {
    // FAILED
    Serial.print("MQTT Failed rc=");
    Serial.println(mqtt.state());

    // Show RED LEDs immediately to indicate a problem
    setAll(colorError);
  }
}

/***************************************************************************************
 *         SECTION 7 — SPINNING LED ANIMATION DURING CONNECTION STATES
 ***************************************************************************************/
void spinningRing(uint32_t color, int speed = 100) {
  // This function creates a "spinning" LED animation.
  // Useful for showing "Connecting..." without blocking code.

  unsigned long now = millis();

  // Only update if the timer reached the speed limit
  if (now - lastLedUpdate > speed) {
    lastLedUpdate = now;

    strip.clear(); // Turn all LEDs OFF first

    strip.setPixelColor(spinningIndex, color); // Light one LED in the ring
    strip.show();

    spinningIndex = (spinningIndex + 1) % NUM_LEDS; // Move to next LED
  }
}

/***************************************************************************************
 *                SECTION 8 — SENSOR FUNCTIONS FOR AHT20 + BH1750
 ***************************************************************************************/
float readBH1750() {
  // This function asks BH1750 (light sensor) to do a measurement
  // Then reads 2 bytes of data.

  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10); // High-resolution mode
  Wire.endTransmission();

  delay(180);       // Sensor processing time

  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2;   // Convert to lux
  }
  return -1;  // Return -1 if reading failed
}

bool readAHT20(float *tempC, float *humidity) {
  // This function reads temperature/humidity from AHT20.
  // It returns TRUE if successful.

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

  *humidity = (hum_raw / 1048576.0) * 100.0;
  *tempC   = (temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}

/***************************************************************************************
 *                     SECTION 9 — SETUP FUNCTION (RUNS ONCE)
 ***************************************************************************************/
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show();

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  Wire.begin(21, 22);  // Set SDA/SCL pins

  // AHT20 Initialization Sequence
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);
  Wire.write(0x08);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(50);

  Serial.println("Sensors Initialized");

  // Start WiFi Connection
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
}

/***************************************************************************************
 *                     SECTION 10 — MAIN LOOP (RUNS FOREVER)
 ***************************************************************************************/
void loop() {

  unsigned long now = millis();

  /***********************************************
   *         1. HANDLE WIFI CONNECTION STATE
   ***********************************************/
  if (WiFi.status() != WL_CONNECTED) {

    // Show spinning blue animation
    spinningRing(colorWiFi, 150);

    // Stay here until WiFi connects
    return;
  }

  // If WiFi just connected (first time)
  if (!wifiPreviouslyConnected) {
    setAll(colorWiFiOK); // Cyan indicator

    // Double beep effect (WiFi connected)
    tone(BUZZER_PIN, 1200, 120);
    delay(150);
    tone(BUZZER_PIN, 1500, 120);

    wifiPreviouslyConnected = true;
  }

  /***********************************************
   *          2. HANDLE MQTT CONNECTION STATE
   ***********************************************/
  if (!mqtt.connected()) {

    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(mqttCallback);

    // Try reconnect every 2 seconds
    if (now - lastMQTTReconnect > 2000) {
      lastMQTTReconnect = now;
      connectMQTT();
    }

    // Show spinning amber animation
    spinningRing(colorMQTT, 100);
    mqttPreviouslyConnected = false;
    return;
  }

  // Maintain MQTT connection
  mqtt.loop();

  // First time MQTT becomes connected
  if (!mqttPreviouslyConnected) {
    setAll(colorReady); // Turn LEDs GREEN
    mqttPreviouslyConnected = true;
  }

  /***********************************************
   *        3. SENSOR READ + MQTT PUBLISH
   ***********************************************/
  if (now - lastSensorPublish > SENSOR_INTERVAL) {

    lastSensorPublish = now;

    float temp, hum;

    if (readAHT20(&temp, &hum)) {
      char buf[10];

      dtostrf(temp, 4, 1, buf);
      mqtt.publish(("WiFi/act9/" + deviceID + "/temperature").c_str(), buf);

      dtostrf(hum, 4, 1, buf);
      mqtt.publish(("WiFi/act9/" + deviceID + "/humidity").c_str(), buf);

      Serial.printf("Temp: %.1f °C, Hum: %.1f %%\n", temp, hum);
    }

    float lux = readBH1750();
    if (lux >= 0) {
      char buf[10];
      dtostrf(lux, 5, 1, buf);
      mqtt.publish(("WiFi/act9/" + deviceID + "/light").c_str(), buf);
      Serial.printf("Light: %.1f lux\n", lux);
    }

    // Check if thresholds exceeded
    buzzerActive = (temp >= TEMP_THRESHOLD ||
                    hum >= HUM_THRESHOLD ||
                    lux >= LIGHT_THRESHOLD);
  }

  /***********************************************
   *           4. ALARM MODE HANDLING
   ***********************************************/
  if (buzzerActive) {

    // -----------------------
    // (A) Buzzer ON/OFF toggling
    // -----------------------
    if (now - lastBuzzerToggle > BUZZER_INTERVAL) {
      lastBuzzerToggle = now;

      static bool on = false;
      on = !on;

      if (on) tone(BUZZER_PIN, BUZZER_FREQ);
      else    noTone(BUZZER_PIN);
    }

    // -----------------------
    // (B) LED Blinking Orange
    // -----------------------
    if (now - lastBlinkToggle > BLINK_INTERVAL) {
      lastBlinkToggle = now;

      static bool lit = false;
      lit = !lit;

      if (lit) setAll(colorAlarm);
      else     setAll(strip.Color(0,0,0));
    }
  }

  /***********************************************
   *        5. IF ALARM CLEARED → RETURN TO GREEN
   ***********************************************/
  else {
    noTone(BUZZER_PIN);     // Make sure buzzer is fully OFF
    setAll(colorReady);     // Reset LEDs to solid green (NORMAL MODE)
  }
}
