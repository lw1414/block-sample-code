#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// --------------------
// ESP32 pin definitions
// --------------------
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   14
#define TFT_SCLK  18  // hardware SPI clock
#define TFT_MOSI  23  // hardware SPI MOSI
#define TFT_MISO  -1  // not used

// Use hardware SPI constructor
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);  

void setup() {
  Serial.begin(115200);
  Serial.println("GC9A01A Test on ESP32");

  // Initialize SPI pins for ESP32 VSPI (SCLK=18, MOSI=23)
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  // Initialize the display
  tft.begin();
  tft.setRotation(1); // Landscape mode

  // Fill screen and test text
  tft.fillScreen(GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.println("Hello GC9A01!");
}

void loop() {
  // Simple color cycle
  tft.fillScreen(GC9A01A_RED);
  delay(500);
  tft.fillScreen(GC9A01A_GREEN);
  delay(500);
  tft.fillScreen(GC9A01A_BLUE);
  delay(500);
}
