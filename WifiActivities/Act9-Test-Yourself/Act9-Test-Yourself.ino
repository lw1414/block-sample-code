/* Combined Sketch with Super Mario Easter Egg
   - NeoPixel (Adafruit_NeoPixel) on LED_PIN = 14, NUM_LEDS = 10
   - Joystick -> brightness (0..100) -> mapped to NeoPixel brightness (0..255)
   - Buttons -> 12 combos -> map to hue (0..360) -> NeoPixel color persists after release
   - Publish dimmer to WiFi/act9/esp01/dimmer (0..100)
   - Publish hue to    WiFi/act9/esp01/hue    (0..360)
   - WiFi double-beep + blink when connected
   - MQTT single-beep + blink when connected
   - Easter Egg: Super Mario tune when all 4 buttons pressed
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ------------------------
// LCD pins
// ------------------------
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   14
#define TFT_SCLK  18
#define TFT_MOSI  23
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST); // Create TFT object

// ------------------------
// Joystick pins
// ------------------------
#define JOY_X_PIN 32
#define JOY_Y_PIN 33

// ------------------------
// Button pins
// ------------------------
#define BTN1_PIN 22
#define BTN2_PIN 21
#define BTN3_PIN 35
#define BTN4_PIN 27

// ------------------------
// NeoPixel (RGB) configuration
// ------------------------
#define LED_PIN    14
#define NUM_LEDS   10
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ------------------------
// Buzzer pin
// ------------------------
#define BUZZER_PIN 13

// ------------------------
// WiFi + MQTT configuration
// ------------------------
const char* ssid     = "RiveraWIFI";
const char* password = "@Rivera20214";
const char* mqtt_server = "innovph.com";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqtt";
const char* mqtt_pass   = "ICPHmqtt!";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ------------------------
// MQTT topics
// ------------------------
const char* deviceBaseTopic = "WiFi/act9/esp01";

// ------------------------
// Timing / throttles
// ------------------------
unsigned long lastMQTTReconnect = 0;
unsigned long lastPublishTime   = 0;
const unsigned long PUBLISH_INTERVAL = 200UL; // ms

// ------------------------
// Last published values to prevent flooding
// ------------------------
int lastPublishedDimmer = -1; // force first publish
int lastPublishedHue    = -1;

// ------------------------
// UI state
// ------------------------
int currentHue = 0;           // 0..360, persists across button releases
int currentBrightness = 128;  // 0..255 (NeoPixel scale)
bool wifiPreviouslyConnected = false;
bool mqttPreviouslyConnected = false;

// ------------------------
// Combo masks (12 combos)
// ------------------------
const uint8_t comboMasks[12] = {
  0b0001, // B1
  0b0010, // B2
  0b0100, // B3
  0b1000, // B4
  0b0011, // B1+B2
  0b0101, // B1+B3
  0b1001, // B1+B4
  0b0110, // B2+B3
  0b1010, // B2+B4
  0b1100, // B3+B4
  0b0111, // B1+B2+B3
  0b1110  // B2+B3+B4
};
const int NUM_COMBOS = 12;

// ------------------------
// Helper prototypes
// ------------------------
bool softwarePullupRead(int pin);
void drawCrosshair(int joyX, int joyY);
void drawQuadrantButtons(bool b1, bool b2, bool b3, bool b4);
void connectMQTT();
bool publishIntTopic(const char* topic, int value);
int maskToComboIndex(uint8_t mask);
int comboIndexToHue(int idx);
uint32_t HSVtoRGBpacked(int h, int s, int v);
void setStripHueBrightness(int hue, int brightness255);
void notifyWiFiConnected();
void notifyMQTTConnected();

// ------------------------
// Setup function
// ------------------------
void setup() {
  Serial.begin(115200); // Serial for debugging

  // Initialize TFT
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);

  // Configure button pins
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT); // floating; use softwarePullupRead
  pinMode(BTN4_PIN, INPUT_PULLUP);

  // Configure buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN); // ensure buzzer off initially

  // Initialize NeoPixel
  strip.begin();
  strip.show(); // turn all LEDs off initially

  // Start WiFi
  WiFi.begin(ssid, password);

  // Setup MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
}

// ------------------------
// Main loop
// ------------------------
void loop() {
  unsigned long now = millis(); // current time

  // ------------------------
  // WiFi connection management
  // ------------------------
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiPreviouslyConnected) {
      wifiPreviouslyConnected = true;
      Serial.println("WiFi CONNECTED");
      notifyWiFiConnected(); // feedback via buzzer + LEDs
    }
  } else {
    wifiPreviouslyConnected = false; // will trigger notification on next connect
  }

  // ------------------------
  // MQTT connection management
  // ------------------------
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      if (now - lastMQTTReconnect > 2000) {
        lastMQTTReconnect = now;
        connectMQTT(); // attempt to reconnect
      }
    } else {
      mqtt.loop(); // keep MQTT alive
      if (!mqttPreviouslyConnected) {
        mqttPreviouslyConnected = true;
        Serial.println("MQTT CONNECTED");
        notifyMQTTConnected(); // single beep + LED feedback
      }
    }
  } else {
    mqttPreviouslyConnected = false; // reset flag
  }

  // ------------------------
  // Read joystick analog values (0..4095)
  // ------------------------
  int joyX = analogRead(JOY_X_PIN);
  int joyY = analogRead(JOY_Y_PIN);

  // Map each axis to 0..100%
  int xPct = map(joyX, 0, 4095, 0, 100);
  int yPct = map(joyY, 0, 4095, 0, 100);

  // Compute dimmer as average of X & Y
  int dimmer = (xPct + yPct) / 2;

  // Map dimmer (0..100) to brightness (0..255)
  int brightness255 = map(dimmer, 0, 100, 0, 255);
  if (brightness255 != currentBrightness) {
    currentBrightness = brightness255;
    setStripHueBrightness(currentHue, currentBrightness);
  }

  // ------------------------
  // Read buttons (pressed == true)
  // ------------------------
  bool b1 = (digitalRead(BTN1_PIN) == LOW);
  bool b2 = (digitalRead(BTN2_PIN) == LOW);
  bool b3 = softwarePullupRead(BTN3_PIN);
  bool b4 = (digitalRead(BTN4_PIN) == LOW);

  // ------------------------
  // Super Mario Easter Egg
  // ------------------------
  // If all four buttons are pressed simultaneously, play Super Mario tune
  if (b1 && b2 && b3 && b4) {
    int marioMelody[]   = { 659, 659, 0, 659, 0, 523, 659, 0, 784 }; // frequencies (Hz)
    int marioDurations[]= { 150, 150, 150, 150, 150, 150, 150, 150, 300 }; // ms

    for (int i = 0; i < 9; i++) {
      if (marioMelody[i] == 0) {
        noTone(BUZZER_PIN); // pause
      } else {
        tone(BUZZER_PIN, marioMelody[i], marioDurations[i]);
      }
      delay(marioDurations[i] * 1.3); // short gap between notes
    }
    noTone(BUZZER_PIN); // stop buzzer after tune
  }

  // ------------------------
  // Draw UI
  // ------------------------
  drawCrosshair(joyX, joyY);
  drawQuadrantButtons(b1, b2, b3, b4);

  // ------------------------
  // Button combos -> hue
  // ------------------------
  uint8_t mask = (b1 ? 0x1 : 0) | (b2 ? 0x2 : 0) | (b3 ? 0x4 : 0) | (b4 ? 0x8 : 0);
  int comboIdx = maskToComboIndex(mask);
  int hueForThisCombo = -1;
  if (comboIdx >= 0) {
    hueForThisCombo = comboIndexToHue(comboIdx);
  }

  // Apply color immediately if combo valid
  if (hueForThisCombo >= 0 && hueForThisCombo != currentHue) {
    currentHue = hueForThisCombo;
    setStripHueBrightness(currentHue, currentBrightness);
  }

  // ------------------------
  // Publish dimmer & hue (throttled)
  // ------------------------
  if (WiFi.status() == WL_CONNECTED && mqtt.connected() && (now - lastPublishTime >= PUBLISH_INTERVAL)) {
    lastPublishTime = now;

    // Publish dimmer if changed
    if (abs(dimmer - lastPublishedDimmer) >= 1) {
      char topicDim[64];
      snprintf(topicDim, sizeof(topicDim), "%s/dimmer", deviceBaseTopic);
      if (publishIntTopic(topicDim, dimmer)) lastPublishedDimmer = dimmer;
    }

    // Publish hue if changed
    if (currentHue >= 0 && currentHue != lastPublishedHue) {
      char topicHue[64];
      snprintf(topicHue, sizeof(topicHue), "%s/hue", deviceBaseTopic);
      if (publishIntTopic(topicHue, currentHue)) lastPublishedHue = currentHue;
    }
  }

  // Small UI refresh delay
  delay(60);
}

// ------------------------
// Helper functions
// ------------------------

// Software pull-up read for floating button
bool softwarePullupRead(int pin) {
  int lowCount = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(pin) == LOW) lowCount++;
    delayMicroseconds(300);
  }
  return (lowCount > 7);
}

// Draw joystick crosshair
void drawCrosshair(int joyX, int joyY) {
  int cx = 120;
  int cy = 120;
  int dx = map(joyY, 4095, 0, -40, 40);
  int dy = map(joyX, 0, 4095, -40, 40);

  tft.fillCircle(cx, cy, 60, GC9A01A_BLACK); // clear area
  tft.drawLine(cx - 60, cy, cx + 60, cy, GC9A01A_DARKGREY);
  tft.drawLine(cx, cy - 60, cx, cy + 60, GC9A01A_DARKGREY);
  tft.fillCircle(cx + dx, cy - dy, 6, GC9A01A_RED);
}

// Draw button quadrant labels
void drawQuadrantButtons(bool b1, bool b2, bool b3, bool b4) {
  tft.setTextSize(2);
  int cx = 120;
  int cy = 120;
  tft.setCursor(cx + 45, cy - 45); tft.setTextColor(b1 ? GC9A01A_GREEN : GC9A01A_WHITE); tft.print("B1");
  tft.setCursor(cx + 45, cy + 25); tft.setTextColor(b2 ? GC9A01A_GREEN : GC9A01A_WHITE); tft.print("B2");
  tft.setCursor(cx - 70, cy - 45); tft.setTextColor(b3 ? GC9A01A_GREEN : GC9A01A_WHITE); tft.print("B3");
  tft.setCursor(cx - 70, cy + 25); tft.setTextColor(b4 ? GC9A01A_GREEN : GC9A01A_WHITE); tft.print("B4");
}

// MQTT callback (just log)
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT RX ["); Serial.print(topic); Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();
}

// Attempt MQTT connection
void connectMQTT() {
  if (mqtt.connected()) return;
  Serial.println("Connecting MQTT...");
  if (mqtt.connect("ESP32Client", mqtt_user, mqtt_pass)) {
    Serial.println("MQTT Connected!");
    mqtt.setCallback(mqttCallback);
    mqtt.subscribe("WiFi/#");
  } else {
    Serial.print("MQTT fail rc="); Serial.println(mqtt.state());
  }
}

// Publish integer topic
bool publishIntTopic(const char* topic, int value) {
  char buf[12]; itoa(value, buf, 10);
  return mqtt.publish(topic, buf);
}

// Convert button mask to combo index
int maskToComboIndex(uint8_t mask) {
  for (int i = 0; i < NUM_COMBOS; ++i) if (comboMasks[i] == mask) return i;
  return -1;
}

// Map combo index to hue
int comboIndexToHue(int idx) {
  if (idx < 0 || idx >= NUM_COMBOS) return -1;
  return idx * (360 / NUM_COMBOS);
}

// Convert HSV to packed RGB for NeoPixel
uint32_t HSVtoRGBpacked(int h, int s_percent, int v) {
  if (h < 0) h = 0; if (h > 360) h = 360;
  if (s_percent < 0) s_percent = 0; if (s_percent > 100) s_percent = 100;
  int s = (s_percent * 255) / 100;

  if (s == 0) return strip.Color(v, v, v); // gray

  float hf = (float)h / 60.0f;
  int i = floor(hf);
  float f = hf - i;
  int p = (int)((v * (255 - s)) / 255.0f);
  int q = (int)((v * (255 - (s * f))) / 255.0f);
  int t = (int)((v * (255 - (s * (1.0f - f)))) / 255.0f);
  uint8_t r, g, b;
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = v; g = v; b = v; break;
  }
  return strip.Color(r, g, b);
}

// Set NeoPixel color using hue and brightness
void setStripHueBrightness(int hue, int brightness255) {
  uint32_t packed = HSVtoRGBpacked(hue, 100, brightness255);
  for (int i = 0; i < NUM_LEDS; ++i) strip.setPixelColor(i, packed);
  strip.show();
}

// WiFi connected notification (double beep + cyan flash)
void notifyWiFiConnected() {
  tone(BUZZER_PIN, 1200, 120);
  delay(150);
  tone(BUZZER_PIN, 1500, 120);

  uint32_t flash = strip.Color(0, 150, 150);
  for (int i = 0; i < NUM_LEDS; ++i) strip.setPixelColor(i, flash);
  strip.show();
  delay(160);
  setStripHueBrightness(currentHue, currentBrightness);
}

// MQTT connected notification (single beep + green flash)
void notifyMQTTConnected() {
  tone(BUZZER_PIN, 1500, 150);
  uint32_t flash = strip.Color(0, 180, 0);
  for (int i = 0; i < NUM_LEDS; ++i) strip.setPixelColor(i, flash);
  strip.show();
  delay(120);
  setStripHueBrightness(currentHue, currentBrightness);
}
