#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ---------------------------
// HARD-CODED WIFI CONFIG
// ---------------------------
const char* ssid     = "WIFI";
const char* password = "PASS1234";

// ---------------------------
// MQTT CONFIG
// ---------------------------
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;

const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

// ---------------------------
// Hardware Pins
// ---------------------------
#define LED_PIN     14
#define LED_COUNT   10

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------------------
// WiFi + MQTT
// ---------------------------
WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastMQTTReconnect = 0;
unsigned long lastLedUpdate = 0;
int spinningIndex = 0;
bool mqttPreviouslyConnected = false;

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
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
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
    spinningIndex = (spinningIndex + 1) % LED_COUNT;
  }
}

// ---------------------------
// Setup
// ---------------------------
void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.show();

  // ---------------------------
  // Connect WiFi (non-blocking style)
  // ---------------------------
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
}

// ---------------------------
// Loop
// ---------------------------
void loop() {
  // Handle WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    // Spinning blue while connecting
    spinningRing(colorWiFi, 150);
    return; // skip MQTT until WiFi connected
  }

  // WiFi connected
  if (!mqttPreviouslyConnected) {
    // show cyan when WiFi OK but MQTT not connected
    setAll(colorWiFiOK);
  }

  // Setup MQTT server if not already
  if (!mqtt.connected()) {
    mqtt.setServer(mqtt_server, mqtt_port);
    mqtt.setCallback(mqttCallback);

    // Attempt reconnection every 2 seconds
    if (millis() - lastMQTTReconnect > 2000) {
      lastMQTTReconnect = millis();
      connectMQTT();
    }

    // Spinning yellow while reconnecting MQTT
    spinningRing(colorMQTT, 100);
    mqttPreviouslyConnected = false;
  } else {
    mqtt.loop();

    // MQTT just connected
    if (!mqttPreviouslyConnected) {
      setAll(colorReady); // solid green
      mqttPreviouslyConnected = true;
      Serial.println("MQTT Connected and LEDs updated!");
    }
  }
}
