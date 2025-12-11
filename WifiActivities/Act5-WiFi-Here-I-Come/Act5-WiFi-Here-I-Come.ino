#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

/* ===================================================
   1. WiFi Credentials
   ===================================================
   These variables store your WiFi SSID and password.
   The ESP32 uses these to connect to your network.
*/
const char* ssid     = "WIFI";
const char* password = "PASS1234";

/* ===================================================
   2. Pin Definitions
   ===================================================
   - LED_PIN: the data pin for the RGB LED strip
   - LED_COUNT: number of LEDs in the strip
   - BUZZER_PIN: passive buzzer pin
   - TFT_CS / TFT_DC / TFT_RST / TFT_SCLK / TFT_MOSI: TFT display SPI pins
   - TFT_RST = -1 means we will use software reset instead of hardware
*/
#define LED_PIN     14
#define LED_COUNT   10
#define BUZZER_PIN  13

#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   -1   // No hardware reset, we will do software reset
#define TFT_SCLK  18
#define TFT_MOSI  23

/* ===================================================
   3. Objects
   ===================================================
   - strip: controls the NeoPixel RGB LED strip
   - tft: controls the 1.28" round GC9A01A TFT display
*/
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

/* ===================================================
   4. LED Colors
   ===================================================
   - colorWiFi: blue, shown when WiFi is connecting
   - colorReady: green, shown when WiFi is connected
*/
uint32_t colorWiFi    = 0x00003C;  // Blue
uint32_t colorReady   = 0x003C00;  // Green

/* ===================================================
   5. State Variables
   ===================================================
   These variables track program state and timing using millis()
   instead of delay() to make the code non-blocking.
*/
unsigned long lastLedUpdate   = 0;  // Last time the LED spinning updated
unsigned long lastTextUpdate  = 0;  // Last time the LCD text was redrawn
unsigned long lastToneUpdate  = 0;  // Last time a tone note was played
int spinningIndex             = 0;  // Current LED index for spinning
bool wifiTonePlayed           = false; // Flag to play Alexa tone only once

/* ===================================================
   6. Function: setAll
   ===================================================
   This function sets all LEDs in the strip to the same color.
   Very useful to show a “solid color” state, e.g., WiFi connected.
*/
void setAll(uint32_t color) {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

/* ===================================================
   7. Function: spinningRing
   ===================================================
   This creates a “rotating LED” effect.
   - color: the color of the spinning LED
   - speed: milliseconds between moving to the next LED
   Uses millis() to avoid blocking the program.
*/
void spinningRing(uint32_t color, int speed = 100) {
  if (millis() - lastLedUpdate > speed) {
    lastLedUpdate = millis();
    strip.clear();                       // Turn off all LEDs
    strip.setPixelColor(spinningIndex, color); // Light up the current LED
    strip.show();
    spinningIndex = (spinningIndex + 1) % LED_COUNT; // Move to next LED
  }
}

/* ===================================================
   8. Alexa Tone (non-blocking)
   ===================================================
   The classic 4-note Alexa tone.  
   We will implement it in a non-blocking style in loop().
*/
const int toneNotes[4] = {1108, 1319, 1760, 1661}; // C#6, E6, A6, G#6
const int toneDurations[4] = {180, 180, 230, 250}; // ms
int currentToneIndex = 0;
bool playingTone = false;
unsigned long toneStartTime = 0;

/* ===================================================
   9. Function: startAlexaTone
   ===================================================
   Start playing the Alexa 4-note tune non-blocking
*/
void startAlexaTone() {
  if (!playingTone) {
    currentToneIndex = 0;
    toneStartTime = millis();
    tone(BUZZER_PIN, toneNotes[currentToneIndex], toneDurations[currentToneIndex]);
    playingTone = true;
  }
}

/* ===================================================
   10. Function: updateAlexaTone
   ===================================================
   Must be called in loop() to progress the notes without blocking
*/
void updateAlexaTone() {
  if (playingTone && millis() - toneStartTime >= toneDurations[currentToneIndex] + 50) { // small gap
    currentToneIndex++;
    if (currentToneIndex < 4) {
      tone(BUZZER_PIN, toneNotes[currentToneIndex], toneDurations[currentToneIndex]);
      toneStartTime = millis();
    } else {
      noTone(BUZZER_PIN); // Stop sound
      playingTone = false;
    }
  }
}

/* ===================================================
   11. Function: printCentered
   ===================================================
   Print a string centered horizontally on the TFT at y position
*/
void printCentered(const char* text, int y) {
  tft.setTextSize(2);
  tft.setTextColor(GC9A01A_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int centerX = (240 - w) / 2;
  tft.setCursor(centerX, y);
  tft.println(text);
}

/* ===================================================
   12. Function: tftSoftwareReset
   ===================================================
   Resets the TFT display using software command.
   Useful when TFT_RST pin is not physically connected.
*/
void tftSoftwareReset() {
  tft.writeCommand(GC9A01A_SWRESET);
  delay(150); // Wait for reset to complete
}

/* ===================================================
   13. Setup
   ===================================================
   Initialize serial, LEDs, TFT, and start WiFi connection.
*/
void setup() {
  Serial.begin(115200);

  // Initialize LED strip
  strip.begin();
  strip.show();

  // Initialize TFT
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tftSoftwareReset();        // Perform software reset
  tft.setRotation(4);        // Rotate 90° left
  tft.fillScreen(GC9A01A_BLACK);

  // Show connecting text
  printCentered("Connecting", 80);
  printCentered("to", 110);
  printCentered("WiFi", 140);

  // Start WiFi connection
  WiFi.begin(ssid, password);
}

/* ===================================================
   14. Loop
   ===================================================
   Non-blocking main loop:
   - Spinning LED animation while connecting
   - TFT text update every 500ms
   - Show connected state and IP
   - Play Alexa tone non-blocking
*/
void loop() {
  unsigned long now = millis();

  // ---------------------------
  // WiFi NOT connected
  // ---------------------------
  if (WiFi.status() != WL_CONNECTED) {
    spinningRing(colorWiFi, 120); // Update LED spinning

    // Redraw TFT text every 500ms to avoid flicker
    if (now - lastTextUpdate > 500) {
      lastTextUpdate = now;
      tft.fillScreen(GC9A01A_BLACK);
      printCentered("Connecting", 80);
      printCentered("to", 110);
      printCentered("WiFi", 140);
    }

    wifiTonePlayed = false; // Reset tone flag
  }
  // ---------------------------
  // WiFi CONNECTED
  // ---------------------------
  else {
    setAll(colorReady); // Solid green LEDs

    // Show connected text and IP only once
    if (!wifiTonePlayed) {
      tft.fillScreen(GC9A01A_BLACK);
      printCentered("Connected!", 90);

      String ip = WiFi.localIP().toString();
      printCentered(ip.c_str(), 130);

      startAlexaTone(); // Start non-blocking Alexa tone
      wifiTonePlayed = true;
    }
  }

  // ---------------------------
  // Update Alexa tone non-blocking
  // ---------------------------
  updateAlexaTone();
}
