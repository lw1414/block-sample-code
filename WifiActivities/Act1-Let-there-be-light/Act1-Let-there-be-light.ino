#include <Adafruit_NeoPixel.h>

// ----------------------------------------------------
// HARDWARE SETUP
// ----------------------------------------------------
#define LED_PIN    14      // Pin connected to the NeoPixel strip
#define NUM_LEDS   10      // How many LEDs your strip has

// Create a NeoPixel object named "strip"
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ----------------------------------------------------
// FADE CONFIGURATION
// ----------------------------------------------------
int brightness = 0;         // This will go from 0 → 255 → 0 (fade in/out)
int fadeAmount = 5;         // How much brightness changes per step
int fadeDelay  = 25;        // Delay between steps (ms) → controls fade speed

void setup() {

  // Start the LED strip
  strip.begin();

  // Set the maximum brightness for the strip
  // Even if brightness = 255, this caps the output
  // Helps reduce power draw and glare
  strip.setBrightness(80);     // 0–255 (80 is gentle brightness)

  // Turn off any leftover colors
  strip.show();
}

void loop() {

  // ----------------------------------------------------
  // 1. UPDATE BRIGHTNESS (fade logic)
  // ----------------------------------------------------
  
  // Increase or decrease brightness by fadeAmount
  brightness += fadeAmount;

  // If we hit the darkest (0) or brightest (255),
  // reverse direction of fading
  if (brightness <= 0 || brightness >= 255) {
    fadeAmount = -fadeAmount;   // Flip positive ↔ negative
  }


  // ----------------------------------------------------
  // 2. SELECT YOUR COLOR (UNCOMMENT ONE)
  // ----------------------------------------------------
  // Use brightness for the channel you want to fade.
  // Only ONE should remain uncommented.

  // uint32_t color = strip.Color(brightness, 0, 0);        // Fading RED
  // uint32_t color = strip.Color(0, brightness, 0);        // Fading GREEN
    uint32_t color = strip.Color(0, 0, brightness);        // Fading BLUE
  // uint32_t color = strip.Color(brightness, brightness, 0);   // Fading YELLOW
  // uint32_t color = strip.Color(brightness, 0, brightness);     // FADING PURPLE (default)
  // uint32_t color = strip.Color(0, brightness, brightness);   // Fading CYAN


  // ----------------------------------------------------
  // 3. APPLY THE COLOR TO ALL LEDs
  // ----------------------------------------------------
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);   // Set each LED to the calculated color
  }

  // Send the updated colors to the strip
  strip.show();


  // ----------------------------------------------------
  // 4. WAIT A SHORT TIME (controls fade speed)
  // ----------------------------------------------------
  delay(fadeDelay);
}
