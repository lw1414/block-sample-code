#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// ------------------------
// LCD pins
// ------------------------
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   14
#define TFT_SCLK  18
#define TFT_MOSI  23

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// ------------------------
// Joystick pins 
// ------------------------
#define JOY_X_PIN 32   // horizontal movement
#define JOY_Y_PIN 33   // vertical movement

// ------------------------
// Buttons
// ------------------------
#define BTN1_PIN 22
#define BTN2_PIN 21
#define BTN3_PIN 35
#define BTN4_PIN 27

// ------------------------
// Software pull-up function
// For floating pin BTN3
// ------------------------
bool softwarePullupRead(int pin) {
  int lowCount = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(pin) == LOW) lowCount++;
    delayMicroseconds(300);
  }
  return (lowCount > 7);
}

void setup() {
  Serial.begin(115200);  // Initialize serial communication for printing X/Y

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.begin();

  // Screen base rotation (UI rotated manually)
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  // Initialize button pins all have hardware pullup
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT);

  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
}

// ------------------------
// Draw joystick crosshair
// ------------------------
void drawCrosshair(int joyX, int joyY) {
  int cx = 120;   // center X of LCD
  int cy = 120;   // center Y of LCD

  // Map joystick values to screen movement
  int dx = map(joyY, 4095, 0, -40, 40); // vertical movement inverted
  int dy = map(joyX, 0, 4095, -40, 40); // horizontal movement

  // Clear crosshair area
  tft.fillCircle(cx, cy, 60, GC9A01A_BLACK);

  // Draw crosshair lines
  tft.drawLine(cx - 60, cy, cx + 60, cy, GC9A01A_DARKGREY); // horizontal
  tft.drawLine(cx, cy - 60, cx, cy + 60, GC9A01A_DARKGREY); // vertical

  // Draw joystick position dot
  tft.fillCircle(cx + dx, cy - dy, 6, GC9A01A_RED);
}

// ------------------------
// Draw button labels near center
// ------------------------
void drawQuadrantButtons(bool b1, bool b2, bool b3, bool b4) {
  tft.setTextSize(2);
  int cx = 120;
  int cy = 120;

  // Top-right (B1)
  tft.setCursor(cx + 45, cy - 45);
  tft.setTextColor(b1 ? GC9A01A_GREEN : GC9A01A_WHITE);
  tft.print("B1");

  // Bottom-right (B2)
  tft.setCursor(cx + 45, cy + 25);
  tft.setTextColor(b2 ? GC9A01A_GREEN : GC9A01A_WHITE);
  tft.print("B2");

  // Top-left (B3)
  tft.setCursor(cx - 70, cy - 45);
  tft.setTextColor(b3 ? GC9A01A_GREEN : GC9A01A_WHITE);
  tft.print("B3");

  // Bottom-left (B4)
  tft.setCursor(cx - 70, cy + 25);
  tft.setTextColor(b4 ? GC9A01A_GREEN : GC9A01A_WHITE);
  tft.print("B4");
}

void loop() {
  // ------------------------
  // Read joystick analog values
  // ------------------------
  int joyX = analogRead(JOY_X_PIN);
  int joyY = analogRead(JOY_Y_PIN);

  // ------------------------
  // Read button states
  // ------------------------
  bool b1 = (digitalRead(BTN1_PIN) == LOW);
  bool b2 = (digitalRead(BTN2_PIN) == LOW);
  bool b3 = softwarePullupRead(BTN3_PIN);
  bool b4 = (digitalRead(BTN4_PIN) == LOW);

  // ------------------------
  // Draw joystick crosshair on LCD
  // ------------------------
  drawCrosshair(joyX, joyY);

  // ------------------------
  // Draw buttons on LCD
  // ------------------------
  drawQuadrantButtons(b1, b2, b3, b4);

  // ------------------------
  // SERIAL PRINT (for teaching/debug)
  // Print joystick X and Y to serial monitor
  // ------------------------
  Serial.print("Joystick X: ");
  Serial.print(joyX);
  Serial.print(" | Joystick Y: ");
  Serial.println(joyY);

  delay(60); // refresh rate
}
