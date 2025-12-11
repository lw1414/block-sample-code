#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// -------------------
// TFT Pins
// -------------------
// TFT_RST = -1 tells the library to perform a software reset instead of using a GPIO pin
#define TFT_CS    5
#define TFT_DC    25
#define TFT_RST   -1   // Software reset
#define TFT_SCLK  18
#define TFT_MOSI  23

// Create TFT object
Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

// -------------------
// Gauge settings
// -------------------
#define CENTER_X      120
#define CENTER_Y      120
#define OUTER_RADIUS  110
#define INNER_RADIUS  (OUTER_RADIUS - 20)

// Variables to keep track of last values to reduce flicker
float lastTemp = -1.0;     
float displayTemp = 0.0;   
int lastHum  = -1;         
int lastTempAngle = 180;
int lastHumAngle  = 0;

// -------------------
// AHT20 I2C Address
// -------------------
// This is the default address of the AHT20 sensor
#define AHT20_ADDR 0x38

// -------------------
// Setup
// -------------------
void setup() {
  Serial.begin(115200);

  // Initialize I2C with SDA=21, SCL=22
  Wire.begin(21, 22); 

  // --- AHT20 INIT (soft reset) ---
  // AHT20 supports a soft reset command (0xBE) which resets the internal registers.
  // This avoids having to power cycle the sensor.
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE);  // soft reset command
  Wire.endTransmission();
  delay(50);          // wait 50ms for reset to complete

  // --- TFT Init (software reset) ---
  // tft.begin() will initialize the display and perform a software reset because TFT_RST = -1
  tft.begin();
  tft.setRotation(0);        // Set rotation if needed (0-3)
  tft.fillScreen(GC9A01A_BLACK);  // Clear screen

  // Draw static gauge frame
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
    Serial.println("AHT20 read error"); // Sensor read failed, skip this loop
    delay(500);
    return;
  }

  // --- Smooth temperature (reduce flicker) ---
  // This is a simple low-pass filter to prevent the gauge from jumping around
  displayTemp = displayTemp + (temp - displayTemp) * 0.1;

  // Update display only if temperature changes >=0.1°C or humidity changes
  if (abs(displayTemp - lastTemp) >= 0.1 || int(hum) != lastHum) {
    drawGauges(displayTemp, int(hum));
    lastTemp = displayTemp;
    lastHum  = int(hum);
  }

  // Print readings to Serial for debugging
  Serial.print("Temp: "); Serial.print(displayTemp, 1); Serial.print(" °C, ");
  Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");

  delay(200); // small delay to reduce I2C traffic
}

// -------------------
// Draw Gauges
// -------------------
void drawGauges(float temp, int hum) {
  drawTempArc(temp);   // Draw temperature arc
  drawTempText(temp);  // Draw temperature value
  drawHumArc(hum);     // Draw humidity arc
  drawHumText(hum);    // Draw humidity value
}

// -------------------
// Draw Temperature Arc
// -------------------
void drawTempArc(float temp) {
  // Map temperature to angles: 0°C -> 180°, 100°C -> 360°
  int newAngle = map(temp * 10, 0, 1000, 180, 360);

  if (newAngle > lastTempAngle) {
    // Draw new arc segment forward
    for (int a = lastTempAngle + 1; a <= newAngle; a++) {
      float p = float(a - 180) / 180.0;
      drawThickArc(a, tft.color565(255, uint8_t(120 + 135 * p), 0));
    }
  } else if (newAngle < lastTempAngle) {
    // Erase arc segment backward
    for (int a = newAngle + 1; a <= lastTempAngle; a++)
      drawThickArc(a, GC9A01A_BLACK);
  }
  lastTempAngle = newAngle;
}

// -------------------
// Draw Humidity Arc
// -------------------
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

// -------------------
// Draw Thick Arc Pixels
// -------------------
void drawThickArc(int angle, uint16_t color) {
  float rad = angle * 0.0174533; // convert degrees to radians
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
  tft.print(temp, 1);
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
    How AHT20 works (simplified for students):
    1. We send a "measurement command" (0xAC) to the sensor via I2C.
    2. The sensor takes ~80ms to measure temperature & humidity internally.
    3. We read 6 bytes back:
       [0] status byte
       [1-3] humidity raw data (20 bits)
       [3-5] temperature raw data (20 bits)
    4. We convert raw 20-bit values to real temperature in °C and humidity in %.
  */

  // Send measurement command
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC);  // trigger measurement
  Wire.write(0x33);  // normal mode
  Wire.write(0x00);  // dummy byte
  Wire.endTransmission();
  delay(80);         // wait for measurement

  // Read 6 bytes from sensor
  Wire.requestFrom(AHT20_ADDR, 6);
  if (Wire.available() != 6) return false; // if less than 6 bytes, fail

  uint8_t data[6];
  for (int i = 0; i < 6; i++) data[i] = Wire.read();

  // Extract humidity (20-bit) from bytes
  uint32_t hum_raw =
      ((uint32_t)data[1] << 12) |
      ((uint32_t)data[2] << 4) |
      ((uint32_t)(data[3] >> 4));

  // Extract temperature (20-bit) from bytes
  uint32_t temp_raw =
      (((uint32_t)data[3] & 0x0F) << 16) |
      ((uint32_t)data[4] << 8) |
      ((uint32_t)data[5]);

  // Convert raw values to human-readable values
  *humidity = ((float)hum_raw / 1048576.0) * 100.0; // 2^20 = 1048576
  *tempC   = ((float)temp_raw / 1048576.0) * 200.0 - 50.0;

  return true;
}
