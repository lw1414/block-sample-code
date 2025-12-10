#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// -------------------
// TFT Pins
// -------------------
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   14
#define TFT_SCLK  18
#define TFT_MOSI  23

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// -------------------
// Gauge settings
// -------------------
#define CENTER_X      120
#define CENTER_Y      120
#define OUTER_RADIUS  110
#define INNER_RADIUS  (OUTER_RADIUS - 20)

float lastTemp = -1.0;     // last displayed temperature
float displayTemp = 0.0;   // smoothed temperature
int lastHum  = -1;         // last displayed humidity
int lastTempAngle = 180;
int lastHumAngle  = 0;

// -------------------
// AHT20 I2C Address
// -------------------
#define AHT20_ADDR 0x38

// -------------------
// Setup
// -------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA = 21, SCL = 22

  // --- AHT20 INIT ---
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);  // Soft reset command
  Wire.write(0x08);  // Initial Read AHT20
  Wire.write(0x00);  // Initial Read AHT20
  Wire.endTransmission();
  delay(50);

  // --- TFT Init ---
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  // Draw gauge frame
  tft.drawCircle(CENTER_X, CENTER_Y, OUTER_RADIUS, GC9A01A_WHITE);
  tft.drawCircle(CENTER_X, CENTER_Y, OUTER_RADIUS - 1, GC9A01A_WHITE);
}

// -------------------
// Main Loop
// -------------------
void loop() {
  float temp, hum;

  // Read temperature and humidity from AHT20
  if (!readAHT20(&temp, &hum)) {
    Serial.println("AHT20 read error");
    delay(500);
    return;
  }

  // --- Smooth temperature (reduce flicker) ---
  displayTemp = displayTemp + (temp - displayTemp) * 0.1; // linear interpolation

  // Update display only if temp changes >=0.1°C or humidity changes
  if (abs(displayTemp - lastTemp) >= 0.1 || int(hum) != lastHum) {
    drawGauges(displayTemp, int(hum));
    lastTemp = displayTemp;
    lastHum  = int(hum);
  }

  // Print readings to Serial for debugging
  Serial.print("Temp: "); Serial.print(displayTemp, 1); Serial.print(" °C, ");
  Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");

  delay(200);
}

// -------------------
// Draw Gauges
// -------------------
void drawGauges(float temp, int hum) {
  drawTempArc(temp);
  drawTempText(temp);
  drawHumArc(hum);
  drawHumText(hum);
}

// Draw temperature arc (top half)
void drawTempArc(float temp) {
  int newAngle = map(temp * 10, 0, 1000, 180, 360);
  if (newAngle > lastTempAngle) {
    for (int a = lastTempAngle + 1; a <= newAngle; a++) {
      float p = float(a - 180) / 180.0;
      drawThickArc(a, tft.color565(255, uint8_t(120 + 135 * p), 0));
    }
  } else if (newAngle < lastTempAngle) {
    for (int a = newAngle + 1; a <= lastTempAngle; a++)
      drawThickArc(a, GC9A01A_BLACK);
  }
  lastTempAngle = newAngle;
}

// Draw humidity arc (bottom half)
void drawHumArc(int hum) {
  int newAngle = map(hum, 0, 100, 0, 180);
  if (newAngle > lastHumAngle) {
    for (int a = lastHumAngle + 1; a <= newAngle; a++) {
      float p = float(a) / 180.0;
      drawThickArc(a, tft.color565(0, uint8_t(255 * p), 255));
    }
  } else if (newAngle < lastHumAngle) {
    for (int a = newAngle + 1; a <= lastHumAngle; a++)
      drawThickArc(a, GC9A01A_BLACK);
  }
  lastHumAngle = newAngle;
}

// Draw thick arc pixels
void drawThickArc(int angle, uint16_t color) {
  float rad = angle * 0.0174533; // degrees to radians
  for (int r = INNER_RADIUS; r <= OUTER_RADIUS; r++) {
    int x = CENTER_X + r * cos(rad);
    int y = CENTER_Y + r * sin(rad);
    tft.drawPixel(x, y, color);
  }
}

// -------------------
// Display Text
// -------------------
void clearCircleArea(int cx, int cy, int r, uint16_t color) {
  for (int y = -r; y <= r; y++) {
    for (int x = -r; x <= r; x++) {
      if (x*x + y*y <= r*r) tft.drawPixel(cx + x, cy + y, color);
    }
  }
}

void drawTempText(float temp) {
  clearCircleArea(CENTER_X, CENTER_Y - 40, 50, GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 30, CENTER_Y - 65);
  tft.print("TEMP");
  tft.setTextColor(GC9A01A_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(CENTER_X - 25, CENTER_Y - 40);
  tft.print(temp, 1);  // one decimal point
  tft.print("C");
}

void drawHumText(int hum) {
  clearCircleArea(CENTER_X, CENTER_Y + 40, 50, GC9A01A_BLACK);
  tft.setTextColor(GC9A01A_WHITE);
  tft.setTextSize(2);
  tft.setCursor(CENTER_X - 40, CENTER_Y + 20);
  tft.print("HUMIDITY");
  tft.setTextColor(GC9A01A_CYAN);
  tft.setTextSize(3);
  tft.setCursor(CENTER_X - 25, CENTER_Y + 45);
  tft.print(hum);
  tft.print("%");
}

// -------------------
// Read AHT20 Temperature & Humidity
// -------------------
bool readAHT20(float *tempC, float *humidity) {
  /*
    1. Send measurement command 0xAC to AHT20
    2. Wait ~80ms for measurement
    3. Read 6 bytes:
       [0] status, [1-5] data
    4. Convert raw 20-bit humidity and temperature data to %
       and °C according to datasheet formulas:
       Humidity = hum_raw / 2^20 * 100
       Temp = temp_raw / 2^20 * 200 - 50
  */
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

  *humidity = ((float)hum_raw / 1048576.0) * 100.0; // convert to %
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0; // convert to °C

  return true;
}
