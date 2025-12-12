#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ------------------------
// NeoPixel configuration
// ------------------------
#define LED_PIN    14          // Pin connected to NeoPixel strip
#define NUM_LEDS   10          // Number of LEDs in the strip
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ------------------------
// Buzzer pin
// ------------------------
#define BUZZER_PIN 13          // Passive buzzer pin

// ------------------------
// WiFi + MQTT configuration
// ------------------------
const char* ssid     = "RiveraWIFI";
const char* password = "@Rivera20214";
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

// ------------------------
// Device ID (configurable)
// ------------------------
const char* deviceId = "esp01";

// ------------------------
// MQTT client setup
// ------------------------
WiFiClient espClient;
PubSubClient mqtt(espClient);
char buttonTopic[64];       // MQTT topic for the button

// ------------------------
// NeoPixel state
// ------------------------
bool stripOn = false;        // State of the main NeoPixel strip

// ------------------------
// Timing variables (millis-based, non-blocking)
// ------------------------
unsigned long previousFlashMillis = 0;   // Tracks time for status LED flashes
unsigned long previousMQTTMillis = 0;    // Tracks MQTT reconnect attempts
const int flashDuration = 300;           // Duration of RGB status flash in ms
bool flashActive = false;                // Whether a status flash is active

// ------------------------
// Empire tune variables
// ------------------------
int melody[] = { 440, 494, 523, 440 };   // Frequencies in Hz
int duration[] = { 300, 300, 300, 500 }; // Duration per note in ms
int melodyIndex = 0;
bool playingTune = false;

// ------------------------
// Function declarations
// ------------------------
void connectMQTT();
void setStripOnOff(bool on);
void updateRGBStatus();
void playEmpireTuneNonBlocking();

// ------------------------
// Setup function
// ------------------------
void setup() {
  Serial.begin(115200);

  // Initialize NeoPixel strip
  strip.begin();
  strip.show(); // Turn off all LEDs initially

  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  // Start WiFi connection
  WiFi.begin(ssid, password);

  // Configure MQTT
  mqtt.setServer(mqtt_server, mqtt_port);

  // Prepare MQTT topic for button
  snprintf(buttonTopic, sizeof(buttonTopic), "WiFi/act10/%s/button", deviceId);

  // Set MQTT message callback
  mqtt.setCallback([](char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0'; // null-terminate payload
    String msg = String((char*)payload);
    msg.trim();

    Serial.print("MQTT RX ["); Serial.print(topic); Serial.print("] "); Serial.println(msg);

    // Control NeoPixel strip and play tune
    if (msg.equalsIgnoreCase("ON")) {
      setStripOnOff(true);
      playingTune = true;   // Start tune playback in loop
      melodyIndex = 0;
    } else if (msg.equalsIgnoreCase("OFF")) {
      setStripOnOff(false);
    }
  });
}

// ------------------------
// Main loop function
// ------------------------
void loop() {
  unsigned long currentMillis = millis();

  // Handle WiFi + MQTT status flashes (only when not connected fully)
  updateRGBStatus();

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    // Attempt reconnect every 2 seconds
    if (currentMillis - previousMQTTMillis >= 2000) {
      Serial.println("Connecting WiFi...");
      WiFi.begin(ssid, password);
      previousMQTTMillis = currentMillis;
    }
  }

  // Check MQTT connection
  if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
    // Attempt reconnect every 2 seconds
    if (currentMillis - previousMQTTMillis >= 2000) {
      connectMQTT();
      previousMQTTMillis = currentMillis;
    }
  }

  // Process MQTT messages
  mqtt.loop();

  // Play buzzer tune (non-blocking)
  playEmpireTuneNonBlocking();
}

// ------------------------
// Connect to MQTT broker
// ------------------------
void connectMQTT() {
  Serial.println("Connecting MQTT...");
  if (mqtt.connect(deviceId, mqtt_user, mqtt_pass)) {
    Serial.println("MQTT connected!");
    mqtt.subscribe(buttonTopic);
  } else {
    Serial.print("MQTT failed, rc=");
    Serial.println(mqtt.state());
  }
}

// ------------------------
// Turn NeoPixel strip ON/OFF
// ------------------------
void setStripOnOff(bool on) {
  stripOn = on;
  uint32_t color = on ? strip.Color(255, 0, 0) : 0; // Red if ON, off if OFF
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// ------------------------
// Handle RGB status flashes
// ------------------------
void updateRGBStatus() {
  unsigned long currentMillis = millis();

  // Only show flashes if strip is OFF (so ON state retains red)
  if (stripOn) return;

  // Turn off LEDs after flashDuration
  if (flashActive && (currentMillis - previousFlashMillis >= flashDuration)) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0);
    strip.show();
    flashActive = false;
  }

  // Trigger flash only if no flash is active
  if (!flashActive) {
    if (WiFi.status() != WL_CONNECTED) {
      // WiFi connecting → brief blue flash
      for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(0, 0, 255));
      strip.show();
      flashActive = true;
      previousFlashMillis = currentMillis;
    } 
    else if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
      // WiFi connected, MQTT not connected → brief yellow flash
      for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(255, 255, 0));
      strip.show();
      flashActive = true;
      previousFlashMillis = currentMillis;
    } 
    // No green flash when fully connected (status is silent)
  }
}

// ------------------------
// Play buzzer tune non-blocking
// ------------------------
void playEmpireTuneNonBlocking() {
  if (!playingTune) return;

  unsigned long currentMillis = millis();
  static unsigned long noteStartMillis = 0;

  if (melodyIndex < 4) {
    // Start next note if duration has passed
    if (currentMillis - noteStartMillis >= duration[melodyIndex]) {
      tone(BUZZER_PIN, melody[melodyIndex]);
      noteStartMillis = currentMillis;
      melodyIndex++;
    }
  } else {
    noTone(BUZZER_PIN);
    playingTune = false;
  }
}
