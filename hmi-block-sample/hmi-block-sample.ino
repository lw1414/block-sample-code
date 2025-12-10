#include <Adafruit_NeoPixel.h>

#define LED_PIN     14
#define LED_COUNT   10
#define TOUCH_PIN   34    // AT42QT1010 output pin (active HIGH)

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

int colorIndex = 0;
unsigned long lastChange = 0;
bool lastState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  strip.begin();
  strip.show();
}

void loop() {
  bool touchState = digitalRead(TOUCH_PIN);

  // Detect rising edge (touch press)
  if (touchState == HIGH && lastState == LOW) {
    changeColor();
  }

  lastState = touchState;
}

void changeColor() {
  colorIndex = (colorIndex + 1) % 3;   // Cycle 0 → 1 → 2 → 0

  uint32_t color;

  if (colorIndex == 0)      color = strip.Color(55, 0, 0);   // Red
  else if (colorIndex == 1) color = strip.Color(0, 55, 0);   // Green
  else                      color = strip.Color(0, 0, 55);   // Blue

  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();

  Serial.print("Touched! New color index = ");
  Serial.println(colorIndex);
}
